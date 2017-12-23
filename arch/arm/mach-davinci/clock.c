/*
 * Clock and PLL control for DaVinci devices
 *
 * Copyright (C) 2006-2007 Texas Instruments.
 * Copyright (C) 2008-2009 Deep Root Systems, LLC
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/mutex.h>
#include <linux/io.h>
#include <linux/delay.h>

#include <mach/hardware.h>

#include <mach/clock.h>
#include "psc.h"
#include <mach/cputype.h>
#include "clock.h"

static LIST_HEAD(clocks);
static DEFINE_MUTEX(clocks_mutex);
static DEFINE_SPINLOCK(clockfw_lock);

static int davinci_clk_reset(struct davinci_clk *clk, bool reset)
{
	unsigned long flags;

	if (clk == NULL || IS_ERR(clk))
		return -EINVAL;

	spin_lock_irqsave(&clockfw_lock, flags);
	if (clk->flags & CLK_PSC)
		davinci_psc_reset(clk->gpsc, clk->lpsc, reset);
	spin_unlock_irqrestore(&clockfw_lock, flags);

	return 0;
}

int davinci_clk_reset_assert(struct clk *clk)
{
	struct davinci_clk *dclk = to_davinci_clk(__clk_get_hw(clk));

	if (IS_ERR_OR_NULL(dclk) || !dclk->reset)
		return -EINVAL;

	return dclk->reset(dclk, true);
}
EXPORT_SYMBOL(davinci_clk_reset_assert);

int davinci_clk_reset_deassert(struct clk *clk)
{
	struct davinci_clk *dclk = to_davinci_clk(__clk_get_hw(clk));

	if (IS_ERR_OR_NULL(dclk) || !dclk->reset)
		return -EINVAL;

	return dclk->reset(dclk, false);
}
EXPORT_SYMBOL(davinci_clk_reset_deassert);

static int _clk_enable(struct clk_hw *hw)
{
	struct davinci_clk *clk = to_davinci_clk(hw);

	if (!clk)
		return 0;
	else if (IS_ERR(clk))
		return -EINVAL;

	if (clk->usecount++ == 0) {
		if (clk->flags & CLK_PSC)
			davinci_psc_config(clk->domain, clk->gpsc, clk->lpsc,
					   true, clk->flags);
		else if (clk->clk_enable)
			clk->clk_enable(clk);
	}

	return 0;
}

static void _clk_disable(struct clk_hw *hw)
{
	struct davinci_clk *clk = to_davinci_clk(hw);

	if (clk == NULL || IS_ERR(clk))
		return;

	if (WARN_ON(clk->usecount == 0))
		return;
	if (--clk->usecount == 0) {
		if (!(clk->flags & CLK_PLL) && (clk->flags & CLK_PSC))
			davinci_psc_config(clk->domain, clk->gpsc, clk->lpsc,
					   false, clk->flags);
		else if (clk->clk_disable)
			clk->clk_disable(clk);
	}
}

static unsigned long _clk_recalc_rate(struct clk_hw *hw,
				      unsigned long parent_rate)
{
	struct davinci_clk *clk = to_davinci_clk(hw);

	if (clk == NULL || IS_ERR(clk))
		return 0;

	if (clk->recalc)
		return clk->recalc(clk);

	return clk->rate;
}

static long _clk_round_rate(struct clk_hw *hw, unsigned long rate,
			    unsigned long *parent_rate)
{
	struct davinci_clk *clk = to_davinci_clk(hw);

	if (clk == NULL || IS_ERR(clk))
		return 0;

	if (clk->round_rate)
		return clk->round_rate(clk, rate);

	return clk->rate;
}

static int _clk_set_rate(struct clk_hw *hw, unsigned long rate,
			 unsigned long parent_rate)
{
	struct davinci_clk *clk = to_davinci_clk(hw);
	int ret = -EINVAL;

	if (!clk)
		return 0;
	else if (IS_ERR(clk))
		return -EINVAL;

	if (clk->set_rate)
		ret = clk->set_rate(clk, rate);

	return ret;
}

static const struct clk_ops davinci_clk_ops = {
	.enable		= _clk_enable,
	.disable	= _clk_disable,
	.recalc_rate	= _clk_recalc_rate,
	.round_rate	= _clk_round_rate,
	.set_rate	= _clk_set_rate,
};

struct clk *davinci_clk_register(struct davinci_clk *clk)
{
	struct clk_init_data init = {};
	struct clk *ret;

	if (clk == NULL || IS_ERR(clk))
		return -EINVAL;

	if (WARN(clk->parent && !clk->parent->rate,
			"CLK: %s parent %s has no rate!\n",
			clk->name, clk->parent->name))
		return -EINVAL;

	mutex_lock(&clocks_mutex);
	list_add_tail(&clk->node, &clocks);
	if (clk->parent) {
		if (clk->set_parent) {
			ret = clk->set_parent(clk, clk->parent);

			if (ret) {
				mutex_unlock(&clocks_mutex);
				return ret;
			}
		}
	}
	mutex_unlock(&clocks_mutex);

	init.name = clk->name;
	init.ops = &davinci_clk_ops;
	if (clk->parent) {
		init.parent_names = &clk->parent->name;
		init.num_parents = 1;
	}
	clk->hw.init = &init;

	ret = clk_register(NULL, &clk->hw);
	if (WARN(IS_ERR(ret), "Failed to register clock '%s'\n", clk->name))
		return ret;

	/* If rate is already set, use it */
	if (clk->rate)
		return ret;

	/* Else, see if there is a way to calculate it */
	if (clk->recalc)
		clk->rate = clk->recalc(clk);

	/* Otherwise, default to parent rate */
	else if (clk->parent)
		clk->rate = clk->parent->rate;

	return ret;
}

static unsigned long clk_sysclk_recalc(struct davinci_clk *clk)
{
	u32 v, plldiv;
	struct pll_data *pll;
	unsigned long rate = clk->rate;

	/* If this is the PLL base clock, no more calculations needed */
	if (clk->pll_data)
		return rate;

	if (WARN_ON(!clk->parent))
		return rate;

	rate = clk->parent->rate;

	/* Otherwise, the parent must be a PLL */
	if (WARN_ON(!clk->parent->pll_data))
		return rate;

	pll = clk->parent->pll_data;

	/* If pre-PLL, source clock is before the multiplier and divider(s) */
	if (clk->flags & PRE_PLL)
		rate = pll->input_rate;

	if (!clk->div_reg)
		return rate;

	v = __raw_readl(pll->base + clk->div_reg);
	if (v & PLLDIV_EN) {
		plldiv = (v & pll->div_ratio_mask) + 1;
		if (plldiv)
			rate /= plldiv;
	}

	return rate;
}

int davinci_set_sysclk_rate(struct davinci_clk *clk, unsigned long rate)
{
	unsigned v;
	struct pll_data *pll;
	unsigned long input;
	unsigned ratio = 0;

	/* If this is the PLL base clock, wrong function to call */
	if (clk->pll_data)
		return -EINVAL;

	/* There must be a parent... */
	if (WARN_ON(!clk->parent))
		return -EINVAL;

	/* ... the parent must be a PLL... */
	if (WARN_ON(!clk->parent->pll_data))
		return -EINVAL;

	/* ... and this clock must have a divider. */
	if (WARN_ON(!clk->div_reg))
		return -EINVAL;

	pll = clk->parent->pll_data;

	input = clk->parent->rate;

	/* If pre-PLL, source clock is before the multiplier and divider(s) */
	if (clk->flags & PRE_PLL)
		input = pll->input_rate;

	if (input > rate) {
		/*
		 * Can afford to provide an output little higher than requested
		 * only if maximum rate supported by hardware on this sysclk
		 * is known.
		 */
		if (clk->maxrate) {
			ratio = DIV_ROUND_CLOSEST(input, rate);
			if (input / ratio > clk->maxrate)
				ratio = 0;
		}

		if (ratio == 0)
			ratio = DIV_ROUND_UP(input, rate);

		ratio--;
	}

	if (ratio > pll->div_ratio_mask)
		return -EINVAL;

	do {
		v = __raw_readl(pll->base + PLLSTAT);
	} while (v & PLLSTAT_GOSTAT);

	v = __raw_readl(pll->base + clk->div_reg);
	v &= ~pll->div_ratio_mask;
	v |= ratio | PLLDIV_EN;
	__raw_writel(v, pll->base + clk->div_reg);

	v = __raw_readl(pll->base + PLLCMD);
	v |= PLLCMD_GOSET;
	__raw_writel(v, pll->base + PLLCMD);

	do {
		v = __raw_readl(pll->base + PLLSTAT);
	} while (v & PLLSTAT_GOSTAT);

	return 0;
}

static unsigned long clk_leafclk_recalc(struct davinci_clk *clk)
{
	if (WARN_ON(!clk->parent))
		return clk->rate;

	return clk->parent->rate;
}

int davinci_simple_set_rate(struct davinci_clk *clk, unsigned long rate)
{
	clk->rate = rate;
	return 0;
}

static unsigned long clk_pllclk_recalc(struct davinci_clk *clk)
{
	u32 ctrl, mult = 1, prediv = 1, postdiv = 1;
	u8 bypass;
	struct pll_data *pll = clk->pll_data;
	unsigned long rate = clk->rate;

	ctrl = __raw_readl(pll->base + PLLCTL);
	rate = pll->input_rate = clk->parent->rate;

	if (ctrl & PLLCTL_PLLEN) {
		bypass = 0;
		mult = __raw_readl(pll->base + PLLM);
		if (cpu_is_davinci_dm365())
			mult = 2 * (mult & PLLM_PLLM_MASK);
		else
			mult = (mult & PLLM_PLLM_MASK) + 1;
	} else
		bypass = 1;

	if (pll->flags & PLL_HAS_PREDIV) {
		prediv = __raw_readl(pll->base + PREDIV);
		if (prediv & PLLDIV_EN)
			prediv = (prediv & pll->div_ratio_mask) + 1;
		else
			prediv = 1;
	}

	/* pre-divider is fixed, but (some?) chips won't report that */
	if (cpu_is_davinci_dm355() && pll->num == 1)
		prediv = 8;

	if (pll->flags & PLL_HAS_POSTDIV) {
		postdiv = __raw_readl(pll->base + POSTDIV);
		if (postdiv & PLLDIV_EN)
			postdiv = (postdiv & pll->div_ratio_mask) + 1;
		else
			postdiv = 1;
	}

	if (!bypass) {
		rate /= prediv;
		rate *= mult;
		rate /= postdiv;
	}

	pr_debug("PLL%d: input = %lu MHz [ ",
		 pll->num, clk->parent->rate / 1000000);
	if (bypass)
		pr_debug("bypass ");
	if (prediv > 1)
		pr_debug("/ %d ", prediv);
	if (mult > 1)
		pr_debug("* %d ", mult);
	if (postdiv > 1)
		pr_debug("/ %d ", postdiv);
	pr_debug("] --> %lu MHz output.\n", rate / 1000000);

	return rate;
}

/**
 * davinci_set_pllrate - set the output rate of a given PLL.
 *
 * Note: Currently tested to work with OMAP-L138 only.
 *
 * @pll: pll whose rate needs to be changed.
 * @prediv: The pre divider value. Passing 0 disables the pre-divider.
 * @pllm: The multiplier value. Passing 0 leads to multiply-by-one.
 * @postdiv: The post divider value. Passing 0 disables the post-divider.
 */
int davinci_set_pllrate(struct pll_data *pll, unsigned int prediv,
					unsigned int mult, unsigned int postdiv)
{
	u32 ctrl;
	unsigned int locktime;
	unsigned long flags;

	if (pll->base == NULL)
		return -EINVAL;

	/*
	 *  PLL lock time required per OMAP-L138 datasheet is
	 * (2000 * prediv)/sqrt(pllm) OSCIN cycles. We approximate sqrt(pllm)
	 * as 4 and OSCIN cycle as 25 MHz.
	 */
	if (prediv) {
		locktime = ((2000 * prediv) / 100);
		prediv = (prediv - 1) | PLLDIV_EN;
	} else {
		locktime = PLL_LOCK_TIME;
	}
	if (postdiv)
		postdiv = (postdiv - 1) | PLLDIV_EN;
	if (mult)
		mult = mult - 1;

	/* Protect against simultaneous calls to PLL setting seqeunce */
	spin_lock_irqsave(&clockfw_lock, flags);

	ctrl = __raw_readl(pll->base + PLLCTL);

	/* Switch the PLL to bypass mode */
	ctrl &= ~(PLLCTL_PLLENSRC | PLLCTL_PLLEN);
	__raw_writel(ctrl, pll->base + PLLCTL);

	udelay(PLL_BYPASS_TIME);

	/* Reset and enable PLL */
	ctrl &= ~(PLLCTL_PLLRST | PLLCTL_PLLDIS);
	__raw_writel(ctrl, pll->base + PLLCTL);

	if (pll->flags & PLL_HAS_PREDIV)
		__raw_writel(prediv, pll->base + PREDIV);

	__raw_writel(mult, pll->base + PLLM);

	if (pll->flags & PLL_HAS_POSTDIV)
		__raw_writel(postdiv, pll->base + POSTDIV);

	udelay(PLL_RESET_TIME);

	/* Bring PLL out of reset */
	ctrl |= PLLCTL_PLLRST;
	__raw_writel(ctrl, pll->base + PLLCTL);

	udelay(locktime);

	/* Remove PLL from bypass mode */
	ctrl |= PLLCTL_PLLEN;
	__raw_writel(ctrl, pll->base + PLLCTL);

	spin_unlock_irqrestore(&clockfw_lock, flags);

	return 0;
}

struct clk * __init davinci_clk_init(struct davinci_clk *clk, const char *con_id,
				     const char *dev_id)
{
	struct clk *ret;

	if (!clk->recalc) {

		/* Check if clock is a PLL */
		if (clk->pll_data)
			clk->recalc = clk_pllclk_recalc;

		/* Else, if it is a PLL-derived clock */
		else if (clk->flags & CLK_PLL)
			clk->recalc = clk_sysclk_recalc;

		/* Otherwise, it is a leaf clock (PSC clock) */
		else if (clk->parent)
			clk->recalc = clk_leafclk_recalc;
	}

	if (clk->pll_data) {
		struct pll_data *pll = clk->pll_data;

		if (!pll->div_ratio_mask)
			pll->div_ratio_mask = PLLDIV_RATIO_MASK;

		if (pll->phys_base && !pll->base) {
			pll->base = ioremap(pll->phys_base, SZ_4K);
			WARN_ON(!pll->base);
		}
	}

	if (clk->recalc)
		clk->rate = clk->recalc(clk);

	if (clk->lpsc)
		clk->flags |= CLK_PSC;

	if (clk->flags & PSC_LRST)
		clk->reset = davinci_clk_reset;

	ret = davinci_clk_register(clk);
	if (IS_ERR(ret))
		return ret;

	clk_register_clkdev(ret, con_id, dev_id);

	/* Turn on clocks that Linux doesn't otherwise manage */
	if (clk->flags & ALWAYS_ENABLED)
		clk_prepare_enable(ret);

	return ret;
}
