// SPDX-License-Identifier: GPL-2.0
/*
 * PLL clock driver for TI Davinci SoCs
 *
 * Copyright (C) 2017 David Lechner <david@lechnology.com>
 *
 * Based on drivers/clk/keystone/pll.c
 * Copyright (C) 2013 Texas Instruments Inc.
 *	Murali Karicheri <m-karicheri2@ti.com>
 *	Santosh Shilimkar <santosh.shilimkar@ti.com>
 *
 * And on arch/arm/mach-davinci/clock.c
 * Copyright (C) 2006-2007 Texas Instruments.
 * Copyright (C) 2008-2009 Deep Root Systems, LLC
 */

#define pr_fmt(fmt) "%s: " fmt "\n", __func__

#include <linux/clk-provider.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/of_address.h>
#include <linux/of.h>
#include <linux/slab.h>
#include <linux/types.h>

#include "pll.h"

#define MAX_NAME_SIZE 20

#define REVID		0x000
#define PLLCTL		0x100
#define OCSEL		0x104
#define PLLSECCTL	0x108
#define PLLM		0x110
#define PREDIV		0x114
#define PLLDIV1		0x118
#define PLLDIV2		0x11c
#define PLLDIV3		0x120
#define OSCDIV		0x124
#define POSTDIV		0x128
#define BPDIV		0x12c
#define PLLCMD		0x138
#define PLLSTAT		0x13c
#define ALNCTL		0x140
#define DCHANGE		0x144
#define CKEN		0x148
#define CKSTAT		0x14c
#define SYSTAT		0x150
#define PLLDIV4		0x160
#define PLLDIV5		0x164
#define PLLDIV6		0x168
#define PLLDIV7		0x16c
#define PLLDIV8		0x170
#define PLLDIV9		0x174

#define PLLCTL_PLLEN	BIT(0)
#define PLLCTL_PLLPWRDN	BIT(1)
#define PLLCTL_PLLRST	BIT(3)
#define PLLCTL_PLLDIS	BIT(4)
#define PLLCTL_PLLENSRC	BIT(5)
#define PLLCTL_CLKMODE	BIT(8)

#define PLLM_MASK		0x1f
#define PREDIV_RATIO_MASK	0x1f
#define PREDIV_PREDEN		BIT(15)
#define PLLDIV_RATIO_WIDTH	5
#define PLLDIV_ENABLE_SHIFT	15
#define OSCDIV_RATIO_WIDTH	5
#define POSTDIV_RATIO_MASK	0x1f
#define POSTDIV_POSTDEN		BIT(15)
#define BPDIV_RATIO_SHIFT	0
#define BPDIV_RATIO_WIDTH	5
#define CKEN_OBSCLK_SHIFT	1
#define CKEN_AUXEN_SHIFT	0

/*
 * OMAP-L138 system reference guide recommends a wait for 4 OSCIN/CLKIN
 * cycles to ensure that the PLLC has switched to bypass mode. Delay of 1us
 * ensures we are good for all > 4MHz OSCIN/CLKIN inputs. Typically the input
 * is ~25MHz. Units are micro seconds.
 */
#define PLL_BYPASS_TIME		1
/* From OMAP-L138 datasheet table 6-4. Units are micro seconds */

#define PLL_RESET_TIME		1

/*
 * From OMAP-L138 datasheet table 6-4; assuming prediv = 1, sqrt(pllm) = 4
 * Units are micro seconds.
 */
#define PLL_LOCK_TIME		20


/**
 * struct davinci_pll_pllout_clk - Main PLL clock
 * @hw: clk_hw for the pll
 * @base: Base memory address
 */
struct davinci_pll_pllout_clk {
	struct clk_hw hw;
	void __iomem *base;
	u32 pllm_min;
	u32 pllm_max;
	u32 pllm_mask;
};

#define to_davinci_pll_pllout_clk(_hw) \
	container_of((_hw), struct davinci_pll_pllout_clk, hw)

static unsigned long davinci_pll_clk_recalc(struct clk_hw *hw,
					    unsigned long parent_rate)
{
	struct davinci_pll_pllout_clk *pll = to_davinci_pll_pllout_clk(hw);
	unsigned long rate = parent_rate;
	u32 prediv, mult, postdiv;

	prediv = readl(pll->base + PREDIV) & PREDIV_RATIO_MASK;
	mult = readl(pll->base + PLLM) & PLLM_MASK;
	postdiv = readl(pll->base + POSTDIV) & POSTDIV_RATIO_MASK;

	rate /= prediv + 1;
	rate *= mult + 1;
	rate /= postdiv + 1;

	return rate;
}

static unsigned long davinci_pll_pllout_recalc_rate(struct clk_hw *hw,
						    unsigned long parent_rate)
{
	struct davinci_pll_pllout_clk *pll = to_davinci_pll_pllout_clk(hw);
	unsigned long rate = parent_rate;
	u32 mult;

	mult = readl(pll->base + PLLM) & pll->pllm_mask;
	rate *= mult + 1;

	return rate;
}

/**
 * da850_pll_get_best_rate - Calculate PLL output closest to a given rate
 * @rate: The target rate
 * @parent_rate: The PLL input clock rate
 * @prediv: Pointer to hold the prediv value (optional)
 * @mult: Pointer to hold the multiplier value (optional)
 * @postdiv: Pointer to hold the postdiv value (optional)
 *
 * This function is based on the OMAP-L138/AM18XX specs.
 *
 * Returns: The closest rate less than or equal to @rate that the PLL can
 * generate. @prediv, @mult and @postdiv will contain the values required to
 * generate that rate.
 */
static long da850_pll_get_best_rate(u32 rate, u32 parent_rate, u32 *prediv,
				    u32 *mult, u32 *postdiv)
{
	u32 pllout, r, d1, m, d2;
	u32 best_rate = 0;
	u32 best_prediv = 0;
	u32 best_mult = 0;
	u32 best_postdiv = 0;

	/*
	 * Technically, pre and post dividers can be 1 to 32, inclusive, but
	 * in practice, we never need greater than 3, so we are using that
	 * as the limit to reduce iterations.
	 */
	for (d2 = 1; d2 <= 3; d2++) {
		for (d1 = 1; d1 <= 3; d1++) {
			/*
			 * Calculate maximum useable multiplier given current
			 * divider values to reduce iterations. PLLOUT must be
			 * 600MHz max per datasheet.
			 */
			m = min(rate * d2, 600000000U) / (parent_rate / d1);
			/* PLLM must be between 4 and 32, inclusive */
			for (m = min(32U, m); m >= 4; m--) {
				pllout = parent_rate / d1 * m;

				/* PLLOUT must be 300MHz min per datasheet */
				if (pllout < 300000000)
					break;

				r = pllout / d2;

				if (r > rate)
					continue;

				if (r < best_rate)
					break;

				/* lower multiplier uses less power */
				if (r > best_rate ||
				    (r == best_rate && m < best_mult)) {
					best_rate = r;
					best_prediv = d1;
					best_mult = m;
					best_postdiv = d2;
				}
			}
		}
	}

	if (prediv)
		*prediv = best_prediv;
	if (mult)
		*mult = best_mult;
	if (postdiv)
		*postdiv = best_postdiv;

	return best_rate;
}

static long da850_pll_round_rate(struct clk_hw *hw, unsigned long rate,
				 unsigned long *parent_rate)
{
	return da850_pll_get_best_rate(rate, *parent_rate, NULL, NULL, NULL);
}

/**
 * __da850_pll_set_rate - set the output rate of a given PLL.
 *
 * Note: Currently tested to work with OMAP-L138 only.
 *
 * @pll: pll whose rate needs to be changed.
 * @prediv: The pre divider value. Passing 0 disables the pre-divider.
 * @pllm: The multiplier value. Passing 0 leads to multiply-by-one.
 * @postdiv: The post divider value. Passing 0 disables the post-divider.
 */
static void __da850_pll_set_rate(struct davinci_pll_pllout_clk *pll, u32 prediv,
			 	 u32 mult, u32 postdiv)
{
	u32 ctrl, locktime;

	/*
	 * PLL lock time required per OMAP-L138 datasheet is
	 * (2000 * prediv)/sqrt(pllm) OSCIN cycles. We approximate sqrt(pllm)
	 * as 4 and OSCIN cycle as 25 MHz.
	 */
	if (prediv) {
		locktime = ((2000 * prediv) / 100);
		prediv = (prediv - 1) | PREDIV_PREDEN;
	} else {
		locktime = PLL_LOCK_TIME;
	}
	if (postdiv)
		postdiv = (postdiv - 1) | POSTDIV_POSTDEN;
	if (mult)
		mult = mult - 1;

	ctrl = readl(pll->base + PLLCTL);

	/* Switch the PLL to bypass mode */
	ctrl &= ~(PLLCTL_PLLENSRC | PLLCTL_PLLEN);
	writel(ctrl, pll->base + PLLCTL);

	udelay(PLL_BYPASS_TIME);

	/* Reset and enable PLL */
	ctrl &= ~(PLLCTL_PLLRST | PLLCTL_PLLDIS);
	writel(ctrl, pll->base + PLLCTL);

	writel(prediv, pll->base + PREDIV);
	writel(mult, pll->base + PLLM);
	writel(postdiv, pll->base + POSTDIV);

	udelay(PLL_RESET_TIME);

	/* Bring PLL out of reset */
	ctrl |= PLLCTL_PLLRST;
	writel(ctrl, pll->base + PLLCTL);

	udelay(locktime);

	/* Remove PLL from bypass mode */
	ctrl |= PLLCTL_PLLEN;
	writel(ctrl, pll->base + PLLCTL);
}

static int da850_pll_set_rate(struct clk_hw *hw, unsigned long rate,
			      unsigned long parent_rate)
{
	struct davinci_pll_pllout_clk *pll = to_davinci_pll_pllout_clk(hw);
	u32 prediv, mult, postdiv;

	da850_pll_get_best_rate(rate, parent_rate, &prediv, &mult, &postdiv);
	__da850_pll_set_rate(pll, prediv, mult, postdiv);

	return 0;
}

#ifdef CONFIG_DEBUG_FS
#include <linux/debugfs.h>

#define DEBUG_REG(n)	\
{			\
	.name	= #n,	\
	.offset	= n,	\
}

static const struct debugfs_reg32 davinci_pll_regs[] = {
	DEBUG_REG(REVID),
	DEBUG_REG(PLLCTL),
	DEBUG_REG(OCSEL),
	DEBUG_REG(PLLSECCTL),
	DEBUG_REG(PLLM),
	DEBUG_REG(PREDIV),
	DEBUG_REG(PLLDIV1),
	DEBUG_REG(PLLDIV2),
	DEBUG_REG(PLLDIV3),
	DEBUG_REG(OSCDIV),
	DEBUG_REG(POSTDIV),
	DEBUG_REG(BPDIV),
	DEBUG_REG(PLLCMD),
	DEBUG_REG(PLLSTAT),
	DEBUG_REG(ALNCTL),
	DEBUG_REG(DCHANGE),
	DEBUG_REG(CKEN),
	DEBUG_REG(CKSTAT),
	DEBUG_REG(SYSTAT),
	DEBUG_REG(PLLDIV4),
	DEBUG_REG(PLLDIV5),
	DEBUG_REG(PLLDIV6),
	DEBUG_REG(PLLDIV7),
	DEBUG_REG(PLLDIV8),
	DEBUG_REG(PLLDIV9),
};

static int davinci_pll_debug_init(struct clk_hw *hw, struct dentry *dentry)
{
	struct davinci_pll_pllout_clk *pll = to_davinci_pll_pllout_clk(hw);
	struct debugfs_regset32 *regset;
	struct dentry *d;

	regset = kzalloc(sizeof(regset), GFP_KERNEL);
	if (!regset)
		return -ENOMEM;

	regset->regs = davinci_pll_regs;
	regset->nregs = ARRAY_SIZE(davinci_pll_regs);
	regset->base = pll->base;

	d = debugfs_create_regset32("registers", 0400, dentry, regset);
	if (IS_ERR(d)) {
		kfree(regset);
		return PTR_ERR(d);
	}

	return 0;
}
#else
#define davinci_pll_debug_init NULL
#endif

static const struct clk_ops davinci_pll_pllout_ops = {
	.recalc_rate	= davinci_pll_pllout_recalc_rate,
	.debug_init	= davinci_pll_debug_init,
};

static const struct clk_ops davinci_pll_clk_ops = {
	.recalc_rate	= davinci_pll_clk_recalc,
	.debug_init	= davinci_pll_debug_init,
};

static const struct clk_ops da850_pll_clk_ops = {
	.recalc_rate	= davinci_pll_clk_recalc,
	.round_rate	= da850_pll_round_rate,
	.set_rate	= da850_pll_set_rate,
	.debug_init	= davinci_pll_debug_init,
};

static const struct clk_ops davinci_pll_pllen_ops = {
};

static struct clk *davinci_pll_div_register(const char *name,
					    const char *parent_name,
					    void __iomem *reg)
{
	const char * const *parent_names = parent_name ? &parent_name : NULL;
	int num_parents = parent_name ? 1 : 0;
	struct clk_gate *gate;
	struct clk_divider *divider;
	struct clk *clk;

	gate = kzalloc(sizeof(*gate), GFP_KERNEL);
	if (!gate)
		return ERR_PTR(-ENOMEM);

	gate->reg = reg;
	gate->bit_idx = PLLDIV_ENABLE_SHIFT;

	divider = kzalloc(sizeof(*divider), GFP_KERNEL);
	if (!divider) {
		kfree(gate);
		return ERR_PTR(-ENOMEM);
	}

	divider->reg = reg;
	divider->width = PLLDIV_RATIO_WIDTH;
	divider->flags = CLK_DIVIDER_READ_ONLY;

	clk = clk_register_composite(NULL, name, parent_names, num_parents,
				     NULL, NULL,
				     &divider->hw, &clk_divider_ro_ops,
				     &gate->hw, &clk_gate_ops, 0);
	if (IS_ERR(clk)) {
		kfree(divider);
		kfree(gate);
	}

	return clk;
}

/**
 * davinci_pll_clk_register - Register a PLL clock
 * @info: The device-specific clock info
 * @parent_name: The parent clock name
 * @base: The PLL's memory region
 *
 * This creates a series of clocks that represent the PLL.
 *
 *     OSCIN > [PREDIV >] PLLOUT > [POSTDIV >] PLLEN
 *
 * - OSCIN is the parent clock.
 * - PREDIV and POSTDIV are optional
 * - PLLOUT is the PLL output
 * - PLLEN is the bypass multiplexer
 *
 * Returns: The PLLOUT clock or a negative error code.
 */
struct clk *davinci_pll_clk_register(const struct davinci_pll_clk_info *info,
				     const char *parent_name,
				     void __iomem *base)
{
	char prediv_name[MAX_NAME_SIZE];
	char pllout_name[MAX_NAME_SIZE];
	char postdiv_name[MAX_NAME_SIZE];
	char pllen_name[MAX_NAME_SIZE];
	struct clk_init_data init;
	struct davinci_pll_pllout_clk *pllout, *pllen;
	struct clk *pllout_clk, *clk;

	if (info->flags & PLL_HAS_PREDIV) {
		snprintf(prediv_name, MAX_NAME_SIZE, "%s_prediv", info->name);
		clk = davinci_pll_div_register(prediv_name, parent_name,
					       base + PREDIV);
		if (IS_ERR(clk))
			return clk;
		parent_name = prediv_name;
	}

	pllout = kzalloc(sizeof(*pllout), GFP_KERNEL);
	if (!pllout)
		return ERR_PTR(-ENOMEM);

	snprintf(pllout_name, MAX_NAME_SIZE, "%s_pllout", info->name);

	init.name = pllout_name;
	init.ops = &davinci_pll_pllout_ops;
	init.parent_names = &parent_name;
	init.num_parents = 1;
	init.flags = 0;

	if (info->flags & PLL_HAS_PREDIV)
		inti.flags |= CLK_SET_RATE_PARENT;

	pllout->hw.init = &init;
	pllout->base = base;
	pllout->pllm_mask = info->pllm_mask;
	pllout->pllm_min = info->pllm_min;
	pllout->pllm_max = info->pllm_max;

	pllout_clk = clk_register(NULL, &pllout->hw);
	if (IS_ERR(pllout_clk)) {
		kfree(pllout);
		return pllout_clk;
	}

	parent_name = pllout_name;

	if (info->flags & PLL_HAS_POSTDIV) {
		snprintf(postdiv_name, MAX_NAME_SIZE, "%s_postdiv",info->name);
		clk = davinci_pll_div_register(postdiv_name, parent_name,
					       base + POSTDIV);
		if (IS_ERR(clk))
			return clk;
		parent_name = postdiv_name;
	}

	pllen = kzalloc(sizeof(*pllout), GFP_KERNEL);
	if (!pllen)
		return ERR_PTR(-ENOMEM);

	snprintf(pllen_name, MAX_NAME_SIZE, "%s_pllen", info->name);

	init.name = pllen_name;
	init.ops = &davinci_pll_pllen_ops;
	init.parent_names = &parent_name;
	init.num_parents = 1; /* FIXME: two parents */
	init.flags = CLK_SET_RATE_PARENT;

	pllen->hw.init = &init;
	pllen->base = base;

	clk = clk_register(NULL, &pllen->hw);
	if (IS_ERR(clk)) {
		kfree(pllen);
		return clk;
	}

	return pllout_clk;
}

struct davinci_pll_aux_clk {
	struct clk_hw hw;
	struct davinci_pll_pllout_clk *pll;
};

/**
 * davinci_pll_aux_clk_register - Register bypass clock (AUXCLK)
 * @name: The clock name
 * @parent_name: The parent clock name (usually "ref_clk" since this bypasses
 *               the PLL)
 * @base: The PLL memory region
 */
struct clk *davinci_pll_aux_clk_register(const char *name,
					 const char *parent_name,
					 void __iomem *base)
{
	return clk_register_gate(NULL, name, parent_name, 0, base + CKEN,
				 CKEN_AUXEN_SHIFT, 0, NULL);
}

/**
 * davinci_pll_bpdiv_clk_register - Register bypass divider clock (SYSCLKBP)
 * @name: The clock name
 * @parent_name: The parent clock name (usually "ref_clk" since this bypasses
 *               the PLL)
 * @base: The PLL memory region
 */
struct clk *davinci_pll_bpdiv_clk_register(const char *name,
					   const char *parent_name,
					   void __iomem *base)
{
	return clk_register_divider(NULL, name, parent_name, 0, base + BPDIV,
				    BPDIV_RATIO_SHIFT, BPDIV_RATIO_WIDTH,
				    CLK_DIVIDER_READ_ONLY, NULL);
}

/**
 * davinci_pll_obs_clk_register - Register oscillator divider clock (OBSCLK)
 * @name: The clock name
 * @parent_names: The parent clock names
 * @num_parents: The number of paren clocks
 * @base: The PLL memory region
 * @table: A table of values cooresponding to the parent clocks (see OCSEL
 *         register in SRM for values)
 */
struct clk *davinci_pll_obs_clk_register(const char *name,
					 const char * const *parent_names,
					 u8 num_parents,
					 void __iomem *base,
					 u32 *table)
{
	struct clk_mux *mux;
	struct clk_gate *gate;
	struct clk_divider *divider;
	struct clk *clk;

	mux = kzalloc(sizeof(*mux), GFP_KERNEL);
	if (!mux)
		return ERR_PTR(-ENOMEM);

	mux->reg = base + OCSEL;
	mux->table = table;

	gate = kzalloc(sizeof(*gate), GFP_KERNEL);
	if (!gate) {
		kfree(mux);
		return ERR_PTR(-ENOMEM);
	}

	gate->reg = base + CKEN;
	gate->bit_idx = CKEN_OBSCLK_SHIFT;

	divider = kzalloc(sizeof(*divider), GFP_KERNEL);
	if (!divider) {
		kfree(gate);
		kfree(mux);
		return ERR_PTR(-ENOMEM);
	}

	divider->reg = base + OSCDIV;
	divider->width = OSCDIV_RATIO_WIDTH;

	clk = clk_register_composite(NULL, name, parent_names, num_parents,
				     &mux->hw, &clk_mux_ops,
				     &divider->hw, &clk_divider_ops,
				     &gate->hw, &clk_gate_ops, 0);
	if (IS_ERR(clk)) {
		kfree(divider);
		kfree(gate);
		kfree(mux);
	}

	return clk;
}

struct clk *
davinci_pll_divclk_register(const struct davinci_pll_divclk_info *info,
			    void __iomem *base)
{
	const struct clk_ops *divider_ops = &clk_divider_ops;
	struct clk_gate *gate;
	struct clk_divider *divider;
	struct clk *clk;
	u32 reg;
	u32 flags = 0;

	/* PLLDIVn registers are not entirely consecutive */
	if (info->id < 4)
		reg = PLLDIV1 + 4 * (info->id - 1);
	else
		reg = PLLDIV4 + 4 * (info->id - 4);

	gate = kzalloc(sizeof(*gate), GFP_KERNEL);
	if (!gate)
		return ERR_PTR(-ENOMEM);

	gate->reg = base + reg;
	gate->bit_idx = PLLDIV_ENABLE_SHIFT;

	divider = kzalloc(sizeof(*divider), GFP_KERNEL);
	if (!divider) {
		kfree(gate);
		return ERR_PTR(-ENOMEM);
	}

	divider->reg = base + reg;
	divider->width = PLLDIV_RATIO_WIDTH;
	divider->flags = 0;

	if (info->flags & DIVCLK_FIXED_DIV) {
		divider->flags |= CLK_DIVIDER_READ_ONLY;
		divider_ops = &clk_divider_ro_ops;
	}

	/* Only the ARM clock can change the parent PLL rate */
	if (info->flags & DIVCLK_ARM_RATE)
		flags |= CLK_SET_RATE_PARENT;

	if (info->flags & DIVCLK_ALWAYS_ENABLED)
		flags |= CLK_IS_CRITICAL;

	clk = clk_register_composite(NULL, info->name, &info->parent_name, 1,
				     NULL, NULL, &divider->hw, divider_ops,
				     &gate->hw, &clk_gate_ops, flags);
	if (IS_ERR(clk)) {
		kfree(divider);
		kfree(gate);
	}

	return clk;
}

#ifdef CONFIG_OF

void of_davinci_pll_init(struct device_node *node,
			 const struct davinci_pll_clk_info *info,
			 const struct davinci_pll_divclk_info *div_info,
			 u8 max_divclk_id)
{
	struct device_node *child;
	const char *parent_name;
	void __iomem *base;
	struct clk *clk;

	base = of_iomap(node, 0);
	if (!base) {
		pr_err("ioremap failed");
		return;
	}

	parent_name = of_clk_get_parent_name(node, 0);

	clk = davinci_pll_clk_register(info, parent_name, base);
	if (IS_ERR(clk)) {
		pr_err("failed to register %s (%ld)", info->name, PTR_ERR(clk));
		return;
	}

	of_clk_add_provider(node, of_clk_src_simple_get, clk);

	child = of_get_child_by_name(node, "sysclk");
	if (child && of_device_is_available(child)) {
		struct clk_onecell_data *clk_data;

		clk_data = clk_alloc_onecell_data(max_divclk_id + 1);
		if (!clk_data)
			return;

		for (; div_info->name; div_info++) {
			clk = davinci_pll_divclk_register(div_info, base);
			if (IS_ERR(clk))
				pr_warn("failed to register %s (%ld)",
					div_info->name, PTR_ERR(clk));
			else
				clk_data->clks[div_info->id] = clk;
		}
		of_clk_add_provider(child, of_clk_src_onecell_get, clk_data);
	}
	of_node_put(child);

	child = of_get_child_by_name(node, "auxclk");
	if (child && of_device_is_available(child)) {
		char child_name[MAX_NAME_SIZE];

		snprintf(child_name, MAX_NAME_SIZE, "%s_aux_clk", info->name);

		clk = davinci_pll_aux_clk_register(child_name, parent_name, base);
		if (IS_ERR(clk))
			pr_warn("failed to register %s (%ld)", child_name,
				PTR_ERR(clk));
		else
			of_clk_add_provider(child, of_clk_src_simple_get, clk);
	}
	of_node_put(child);
}
#endif
