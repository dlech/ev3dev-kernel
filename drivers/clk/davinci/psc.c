// SPDX-License-Identifier: GPL-2.0
/*
 * Clock driver for DA8xx/AM17xx/AM18xx/OMAP-L13x PSC controllers
 *
 * Copyright (C) 2017 David Lechner <david@lechnology.com>
 *
 * Based on: drivers/clk/keystone/gate.c
 * Copyright (C) 2013 Texas Instruments.
 *	Murali Karicheri <m-karicheri2@ti.com>
 *	Santosh Shilimkar <santosh.shilimkar@ti.com>
 *
 * And: arch/arm/mach-davinci/psc.c
 * Copyright (C) 2006 Texas Instruments.
 */

#include <linux/clk-provider.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/of_address.h>
#include <linux/of.h>
#include <linux/slab.h>

/* PSC register offsets */
#define EPCPR			0x070
#define PTCMD			0x120
#define PTSTAT			0x128
#define PDSTAT			0x200
#define PDCTL			0x300
#define MDSTAT			0x800
#define MDCTL			0xa00

/* PSC module states */
enum davinci_psc_state {
	PSC_STATE_SWRSTDISABLE	= 0,
	PSC_STATE_SYNCRST	= 1,
	PSC_STATE_DISABLE	= 2,
	PSC_STATE_ENABLE	= 3,
};

#define MDSTAT_STATE_MASK	0x3f
#define MDSTAT_MCKOUT		BIT(12)
#define PDSTAT_STATE_MASK	0x1f
#define MDCTL_FORCE		BIT(31)
#define MDCTL_LRESET		BIT(8)
#define PDCTL_EPCGOOD		BIT(8)
#define PDCTL_NEXT		BIT(0)

/**
 * struct davinci_psc_clk - PSC clock structure
 * @hw: clk_hw for the psc
 * @psc_data: PSC driver specific data
 * @lpsc: Local PSC number (module id)
 * @pd: Power domain
 */
struct davinci_psc_clk {
	struct clk_hw hw;
	void __iomem *base;
	u32 lpsc;
	u32 pd;
};

#define to_davinci_psc_clk(_hw) container_of(_hw, struct davinci_psc_clk, hw)

static void psc_config(struct davinci_psc_clk *psc,
		       enum davinci_psc_state next_state)
{
	u32 epcpr, ptcmd, pdstat, pdctl, mdstat, mdctl, ptstat;

	mdctl = readl(psc->base + MDCTL + 4 * psc->lpsc);
	mdctl &= ~MDSTAT_STATE_MASK;
	mdctl |= next_state;
	/* TODO: old davinci clocks for da850 set MDCTL_FORCE bit for sata and
	 * dsp here. Is this really needed?
	 */
	writel(mdctl, psc->base + MDCTL + 4 * psc->lpsc);

	pdstat = readl(psc->base + PDSTAT + 4 * psc->pd);
	if ((pdstat & PDSTAT_STATE_MASK) == 0) {
		pdctl = readl(psc->base + PDSTAT + 4 * psc->pd);
		pdctl |= PDCTL_NEXT;
		writel(pdctl, psc->base + PDSTAT + 4 * psc->pd);

		ptcmd = BIT(psc->pd);
		writel(ptcmd, psc->base + PTCMD);

		do {
			epcpr = __raw_readl(psc->base + EPCPR);
		} while (!(epcpr & BIT(psc->pd)));

		pdctl = __raw_readl(psc->base + PDCTL + 4 * psc->pd);
		pdctl |= PDCTL_EPCGOOD;
		__raw_writel(pdctl, psc->base + PDCTL + 4 * psc->pd);
	} else {
		ptcmd = BIT(psc->pd);
		writel(ptcmd, psc->base + PTCMD);
	}

	do {
		ptstat = readl(psc->base + PTSTAT);
	} while (ptstat & BIT(psc->pd));

	do {
		mdstat = readl(psc->base + MDSTAT + 4 * psc->lpsc);
	} while (!((mdstat & MDSTAT_STATE_MASK) == next_state));
}

static int davinci_psc_clk_enable(struct clk_hw *hw)
{
	struct davinci_psc_clk *psc = to_davinci_psc_clk(hw);

	psc_config(psc, PSC_STATE_ENABLE);

	return 0;
}

static void davinci_psc_clk_disable(struct clk_hw *hw)
{
	struct davinci_psc_clk *psc = to_davinci_psc_clk(hw);

	psc_config(psc, PSC_STATE_DISABLE);
}

static int davinci_psc_clk_is_enabled(struct clk_hw *hw)
{
	struct davinci_psc_clk *psc = to_davinci_psc_clk(hw);
	u32 mdstat;

	mdstat = readl(psc->base + MDSTAT + 4 * psc->lpsc);

	return (mdstat & MDSTAT_MCKOUT) ? 1 : 0;
}

static const struct clk_ops davinci_psc_clk_ops = {
	.enable		= davinci_psc_clk_enable,
	.disable	= davinci_psc_clk_disable,
	.is_enabled	= davinci_psc_clk_is_enabled,
};

/**
 * davinci_psc_clk_register - register psc clock
 * @dev: device that is registering this clock
 * @name: name of this clock
 * @parent_name: name of clock's parent
 * @base: memory mapped register for the PSC
 * @lpsc: local PSC number
 * @pd: power domain
 */
struct clk *davinci_psc_clk_register(const char *name,
				     const char *parent_name,
				     void __iomem *base,
				     u32 lpsc, u32 pd)
{
	struct clk_init_data init;
	struct davinci_psc_clk *psc;
	struct clk *clk;

	psc = kzalloc(sizeof(*psc), GFP_KERNEL);
	if (!psc)
		return ERR_PTR(-ENOMEM);

	init.name = name;
	init.ops = &davinci_psc_clk_ops;
	init.flags = 0;
	init.parent_names = (parent_name ? &parent_name : NULL);
	init.num_parents = (parent_name ? 1 : 0);

	psc->base = base;
	psc->hw.init = &init;
	psc->lpsc = lpsc;
	psc->pd = pd;

	clk = clk_register(NULL, &psc->hw);
	if (IS_ERR(clk))
		kfree(psc);

	return clk;
}

/* FIXME: This needs to be converted to a reset controller. But, the reset
 * framework is currently device tree only.
 */

DEFINE_SPINLOCK(davinci_psc_reset_lock);

static int davinci_psc_clk_reset(struct davinci_psc_clk *psc, bool reset)
{
	unsigned long flags;
	u32 mdctl;

	if (IS_ERR_OR_NULL(psc))
		return -EINVAL;

	spin_lock_irqsave(&davinci_psc_reset_lock, flags);
	mdctl = readl(psc->base + MDCTL + 4 * psc->lpsc);
	if (reset)
		mdctl &= ~MDCTL_LRESET;
	else
		mdctl |= MDCTL_LRESET;
	writel(mdctl, psc->base + MDCTL + 4 * psc->lpsc);
	spin_unlock_irqrestore(&davinci_psc_reset_lock, flags);

	return 0;
}

int davinci_clk_reset_assert(struct clk *clk)
{
	struct davinci_psc_clk *psc = to_davinci_psc_clk(__clk_get_hw(clk));

	return davinci_psc_clk_reset(psc, true);
}
EXPORT_SYMBOL(davinci_clk_reset_assert);

int davinci_clk_reset_deassert(struct clk *clk)
{
	struct davinci_psc_clk *psc = to_davinci_psc_clk(__clk_get_hw(clk));

	return davinci_psc_clk_reset(psc, false);
}
EXPORT_SYMBOL(davinci_clk_reset_deassert);

/**
 * of_davinci_psc_clk_init - initialize psc clock through DT
 * @node: device tree node for this clock
 */
static void of_davinci_psc_clk_init(struct device_node *node)
{
	void __iomem *base;
	struct device_node *child;

	base = of_iomap(node, 0);
	if (!base) {
		pr_err("%s: ioremap failed\n", __func__);
		return;
	}

	for_each_available_child_of_node(node, child) {
		const char *clk_name, *parent_name;
		struct clk *clk;
		u32 lpsc, pd;

		of_property_read_string(child, "clock-output-names", &clk_name);
		of_property_read_u32(child, "local-domain", &lpsc);
		of_property_read_u32(child, "power-domain", &pd);

		parent_name = of_clk_get_parent_name(child, 0);
		if (!parent_name) {
			pr_warn("%s: Parent clock for %s not found\n",
				__func__, clk_name);
			continue;
		}

		clk = davinci_psc_clk_register(clk_name, parent_name, base,
					       lpsc, pd);
		if (IS_ERR(clk)) {
			pr_warn("%s: Failed to register clock %s\n",
				__func__, clk_name);
			continue;
		}
		of_clk_add_provider(child, of_clk_src_simple_get, clk);
	}
}

CLK_OF_DECLARE(davinci_psc_clk, "ti,davinci-psc-clock", of_davinci_psc_clk_init);
