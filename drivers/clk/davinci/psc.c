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
 * @flags: clock flags
 */
struct clk *davinci_psc_clk_register(const char *name,
				     const char *parent_name,
				     void __iomem *base,
				     u32 lpsc, u32 pd, u32 flags)
{
	struct clk_init_data init;
	struct davinci_psc_clk *psc;
	struct clk *clk;

	psc = kzalloc(sizeof(*psc), GFP_KERNEL);
	if (!psc)
		return ERR_PTR(-ENOMEM);

	init.name = name;
	init.ops = &davinci_psc_clk_ops;
	init.parent_names = (parent_name ? &parent_name : NULL);
	init.num_parents = (parent_name ? 1 : 0);
	init.flags = flags;

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

#ifdef CONFIG_OF
struct davinci_psc_clk_info {
	const char *name;
	const char *parent;
	u32 lpsc;
	u32 pd;
	unsigned long flags;
	bool has_reset;
};

#define DAVINCI_PSC_ALWAYS_ENABLED	BIT(1) /* never disable this clock */
#define DAVINCI_PSC_LOCAL_RESET		BIT(2) /* acts as reset provider */

#define PSC(n, p, l, d, f)	\
{				\
	.name	= #n,		\
	.parent	= #p,		\
	.lpsc	= (l),		\
	.pd	= (d),		\
	.flags	= (f),		\
}

static const struct davinci_psc_clk_info da850_psc0_info[] = {
	PSC(tpcc0, pll0_sysclk2, 0, 0, DAVINCI_PSC_ALWAYS_ENABLED),
	PSC(tptc0, pll0_sysclk2, 1, 0, DAVINCI_PSC_ALWAYS_ENABLED),
	PSC(tptc1, pll0_sysclk2, 2, 0, DAVINCI_PSC_ALWAYS_ENABLED),
	PSC(aemif, pll0_sysclk3, 3, 0, 0),
	PSC(spi0, pll0_sysclk2, 4, 0, 0),
	PSC(mmcsd0, pll0_sysclk2, 5, 0, 0),
	PSC(aintc, pll0_sysclk4, 6, 0, DAVINCI_PSC_ALWAYS_ENABLED),
	PSC(arm_rom, pll0_sysclk2, 7, 0, DAVINCI_PSC_ALWAYS_ENABLED),
	PSC(uart0, pll0_sysclk2, 9, 0, 0),
	PSC(pruss, pll0_sysclk2, 13, 0, 0),
	PSC(arm, pll0_sysclk6, 14, 0, DAVINCI_PSC_ALWAYS_ENABLED),
	PSC(dsp, pll0_sysclk1, 15, 1, DAVINCI_PSC_LOCAL_RESET),
	{ }
};

static const struct davinci_psc_clk_info da850_psc1_info[] = {
	PSC(tpcc1, pll0_sysclk2, 0, 0, DAVINCI_PSC_ALWAYS_ENABLED),
	PSC(usb0, pll0_sysclk2, 1, 0, 0),
	PSC(usb1, pll0_sysclk4, 2, 0, 0),
	PSC(gpio, pll0_sysclk4, 3, 0, 0),
	PSC(emac, pll0_sysclk4, 5, 0, 0),
	PSC(emif3, pll0_sysclk5, 6, 0, DAVINCI_PSC_ALWAYS_ENABLED),
	PSC(mcasp0, async3, 7, 0, 0),
	PSC(sata, pll0_sysclk2, 8, 0, 0),
	PSC(vpif, pll0_sysclk2, 9, 0, 0),
	PSC(spi1, async3, 10, 0, 0),
	PSC(i2c1, pll0_sysclk4, 11, 0, 0),
	PSC(uart1, async3, 12, 0, 0),
	PSC(uart2, async3, 13, 0, 0),
	PSC(mcbsp0, async3, 14, 0, 0),
	PSC(mcbsp1, async3, 15, 0, 0),
	PSC(lcdc, pll0_sysclk2, 16, 0, 0),
	PSC(ehrpwm, async3, 17, 0, 0),
	PSC(mmcsd1, pll0_sysclk2, 18, 0, 0),
	PSC(ecap, async3, 20, 0, 0),
	PSC(tptc2, pll0_sysclk2, 21, 0, DAVINCI_PSC_ALWAYS_ENABLED),
	{ }
};

static void of_davinci_psc_clk_init(struct device_node *node,
				    const struct davinci_psc_clk_info *info,
				    u8 num_clks)
{
	struct clk_onecell_data *clk_data;
	void __iomem *base;

	base = of_iomap(node, 0);
	if (!base) {
		pr_err("%s: ioremap failed\n", __func__);
		return;
	}

	clk_data = clk_alloc_onecell_data(num_clks);
	if (!clk_data) {
		pr_err("%s: Out of memory\n", __func__);
		return;
	}

	for (; info->name; info++) {
		struct clk *clk;
		u32 clk_flags = 0;

		if (info->flags & DAVINCI_PSC_ALWAYS_ENABLED)
			clk_flags |= CLK_IS_CRITICAL;

		clk = davinci_psc_clk_register(info->name, info->parent, base,
					info->lpsc, info->pd, clk_flags);
		if (IS_ERR(clk)) {
			pr_warn("%s: Failed to register %s:%s (%ld)\n", __func__,
				node->full_name, info->name, PTR_ERR(clk));
			continue;
		}

		clk_data->clks[info->lpsc] = clk;
	}

	of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);
}

static void of_da850_psc0_clk_init(struct device_node *node)
{
	of_davinci_psc_clk_init(node, da850_psc0_info, 16);
}
CLK_OF_DECLARE(da850_psc0_clk, "ti,da850-psc0", of_da850_psc0_clk_init);

static void of_da850_psc1_clk_init(struct device_node *node)
{
	of_davinci_psc_clk_init(node, da850_psc1_info, 32);
}
CLK_OF_DECLARE(da850_psc1_clk, "ti,da850-psc1", of_da850_psc1_clk_init);
#endif
