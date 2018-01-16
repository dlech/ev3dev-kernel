// SPDX-License-Identifier: GPL-2.0
/*
 * da8xx-usb-phy-clk - TI DaVinci DA8xx USB PHY clocks driver
 *
 * Copyright (C) 2017 David Lechner <david@lechnology.com>
 *
 * This driver exposes the USB PHY clocks on DA8xx/AM18xx/OMAP-L13x SoCs.
 * The clocks consist of two muxes and a PLL. The USB 2.0 PHY mux and PLL are
 * combined into a single clock in Linux. The USB 1.0 PHY clock just consists
 * of a mux. These clocks are controlled through CFGCHIP2, which is accessed
 * as a syscon regmap since it is shared with other devices.
 */

#define pr_fmt(fmt) "%s: " fmt "\n", __func__

#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/mfd/da8xx-cfgchip.h>
#include <linux/mfd/syscon.h>
#include <linux/of.h>
#include <linux/regmap.h>
#include <linux/slab.h>

/* --- USB 2.0 PHY clock --- */

struct da8xx_usb0_phy_clk {
	struct clk_hw hw;
	struct clk *clk;
	struct regmap *regmap;
};

enum da8xx_usb0_phy_clk_parent {
	DA8XX_USB0_PHY_CLK_PARENT_USB_REFCLKIN,
	DA8XX_USB0_PHY_CLK_PARENT_PLL0_AUXCLK,
};

#define to_da8xx_usb0_phy_clk(_hw) \
	container_of((_hw), struct da8xx_usb0_phy_clk, hw)

static int da8xx_usb0_phy_clk_prepare(struct clk_hw *hw)
{
	struct da8xx_usb0_phy_clk *clk = to_da8xx_usb0_phy_clk(hw);

	/* The USB 2.0 PSC clock is only needed temporarily during the USB 2.0
	 * PHY clock enable, but since clk_prepare() can't be called in an
	 * atomic context (i.e. in clk_enable()), we have to prepare it here.
	 */
	return clk_prepare(clk->clk);
}

static void da8xx_usb0_phy_clk_unprepare(struct clk_hw *hw)
{
	struct da8xx_usb0_phy_clk *clk = to_da8xx_usb0_phy_clk(hw);

	clk_unprepare(clk->clk);
}

static int da8xx_usb0_phy_clk_enable(struct clk_hw *hw)
{
	struct da8xx_usb0_phy_clk *clk = to_da8xx_usb0_phy_clk(hw);
	unsigned int mask, val;
	int ret;

	/* Locking the USB 2.O PLL requires that the USB 2.O PSC is enabled
	 * temporaily. It can be turned back off once the PLL is locked.
	 */
	clk_enable(clk->clk);

	/* Turn on the USB 2.0 PHY, but just the PLL, and not OTG. The USB 1.1
	 * PHY may use the USB 2.0 PLL clock without USB 2.0 OTG being used.
	 */
	mask = CFGCHIP2_RESET | CFGCHIP2_PHYPWRDN | CFGCHIP2_PHY_PLLON;
	val = CFGCHIP2_PHY_PLLON;

	regmap_write_bits(clk->regmap, CFGCHIP(2), mask, val);
	ret = regmap_read_poll_timeout(clk->regmap, CFGCHIP(2), val,
				       val & CFGCHIP2_PHYCLKGD, 0, 500000);

	clk_disable(clk->clk);

	return ret;
}

static void da8xx_usb0_phy_clk_disable(struct clk_hw *hw)
{
	struct da8xx_usb0_phy_clk *clk = to_da8xx_usb0_phy_clk(hw);
	unsigned int val;

	val = CFGCHIP2_PHYPWRDN;
	regmap_write_bits(clk->regmap, CFGCHIP(2), val, val);
}

static int da8xx_usb0_phy_clk_is_enabled(struct clk_hw *hw)
{
	struct da8xx_usb0_phy_clk *clk = to_da8xx_usb0_phy_clk(hw);
	unsigned int val;

	regmap_read(clk->regmap, CFGCHIP(2), &val);

	return !!(val & CFGCHIP2_PHYCLKGD);
}

static unsigned long da8xx_usb0_phy_clk_recalc_rate(struct clk_hw *hw,
						    unsigned long parent_rate)
{
	struct da8xx_usb0_phy_clk *clk = to_da8xx_usb0_phy_clk(hw);
	unsigned int mask, val;

	/* The parent clock rate must be one of the following */
	mask = CFGCHIP2_REFFREQ_MASK;
	switch (parent_rate) {
	case 12000000:
		val = CFGCHIP2_REFFREQ_12MHZ;
		break;
	case 13000000:
		val = CFGCHIP2_REFFREQ_13MHZ;
		break;
	case 19200000:
		val = CFGCHIP2_REFFREQ_19_2MHZ;
		break;
	case 20000000:
		val = CFGCHIP2_REFFREQ_20MHZ;
		break;
	case 24000000:
		val = CFGCHIP2_REFFREQ_24MHZ;
		break;
	case 26000000:
		val = CFGCHIP2_REFFREQ_26MHZ;
		break;
	case 38400000:
		val = CFGCHIP2_REFFREQ_38_4MHZ;
		break;
	case 40000000:
		val = CFGCHIP2_REFFREQ_40MHZ;
		break;
	case 48000000:
		val = CFGCHIP2_REFFREQ_48MHZ;
		break;
	default:
		return 0;
	}

	regmap_write_bits(clk->regmap, CFGCHIP(2), mask, val);

	/* USB 2.0 PLL always supplies 48MHz */
	return 48000000;
}

static long da8xx_usb0_phy_clk_round_rate(struct clk_hw *hw, unsigned long rate,
					  unsigned long *parent_rate)
{
	return 48000000;
}

static int da8xx_usb0_phy_clk_set_parent(struct clk_hw *hw, u8 index)
{
	struct da8xx_usb0_phy_clk *clk = to_da8xx_usb0_phy_clk(hw);
	unsigned int mask, val;

	/* Set the mux depending on the parent clock. */
	mask = CFGCHIP2_USB2PHYCLKMUX;
	switch (index) {
	case DA8XX_USB0_PHY_CLK_PARENT_USB_REFCLKIN:
		val = 0;
		break;
	case DA8XX_USB0_PHY_CLK_PARENT_PLL0_AUXCLK:
		val = CFGCHIP2_USB2PHYCLKMUX;
		break;
	default:
		return -EINVAL;
	}

	regmap_write_bits(clk->regmap, CFGCHIP(2), mask, val);

	return 0;
}

static u8 da8xx_usb0_phy_clk_get_parent(struct clk_hw *hw)
{
	struct da8xx_usb0_phy_clk *clk = to_da8xx_usb0_phy_clk(hw);
	unsigned int val;

	regmap_read(clk->regmap, CFGCHIP(2), &val);

	if (val & CFGCHIP2_USB2PHYCLKMUX)
		return DA8XX_USB0_PHY_CLK_PARENT_PLL0_AUXCLK;

	return DA8XX_USB0_PHY_CLK_PARENT_USB_REFCLKIN;
}

static const struct clk_ops da8xx_usb0_phy_clk_ops = {
	.prepare	= da8xx_usb0_phy_clk_prepare,
	.unprepare	= da8xx_usb0_phy_clk_unprepare,
	.enable		= da8xx_usb0_phy_clk_enable,
	.disable	= da8xx_usb0_phy_clk_disable,
	.is_enabled	= da8xx_usb0_phy_clk_is_enabled,
	.recalc_rate	= da8xx_usb0_phy_clk_recalc_rate,
	.round_rate	= da8xx_usb0_phy_clk_round_rate,
	.set_parent	= da8xx_usb0_phy_clk_set_parent,
	.get_parent	= da8xx_usb0_phy_clk_get_parent,
};

/**
 * da8xx_usb0_phy_clk_register - Register a new USB 2.0 PHY clock
 * @name: The clock name
 * @parent0: The name of the USB_REFCLKIN clock
 * @parent1: The name of the PLL0 AUXCLK
 * @fck_clk: The USB 2.0 PSC clock
 * @regmap: The CFGCHIP regmap
 */
struct clk *da8xx_usb0_phy_clk_register(const char *name,
					const char *parent0,
					const char *parent1,
					struct clk *fck_clk,
					struct regmap *regmap)
{
	const char * const parent_names[] = {
		[DA8XX_USB0_PHY_CLK_PARENT_USB_REFCLKIN] = parent0,
		[DA8XX_USB0_PHY_CLK_PARENT_PLL0_AUXCLK] = parent1,
	};
	struct da8xx_usb0_phy_clk *clk;
	struct clk_init_data init;

	clk = kzalloc(sizeof(*clk), GFP_KERNEL);
	if (!clk)
		return ERR_PTR(-ENOMEM);

	init.name = name;
	init.ops = &da8xx_usb0_phy_clk_ops;
	init.parent_names = parent_names;
	init.num_parents = 2;

	clk->hw.init = &init;
	clk->clk = fck_clk;
	clk->regmap = regmap;

	return clk_register(NULL, &clk->hw);
}

/* --- USB 1.1 PHY clock --- */

struct da8xx_usb1_phy_clk {
	struct clk_hw hw;
	struct regmap *regmap;
};

enum usb1_phy_clk_parent {
	DA8XX_USB1_PHY_CLK_PARENT_USB0_PHY_PLL,
	DA8XX_USB1_PHY_CLK_PARENT_USB_REFCLKIN,
};

#define to_da8xx_usb1_phy_clk(_hw) \
	container_of((_hw), struct da8xx_usb1_phy_clk, hw)

static int da8xx_usb1_phy_clk_set_parent(struct clk_hw *hw, u8 index)
{
	struct da8xx_usb1_phy_clk *clk = to_da8xx_usb1_phy_clk(hw);
	unsigned int mask, val;

	/* Set the USB 1.1 PHY clock mux based on the parent clock. */
	mask = CFGCHIP2_USB1PHYCLKMUX;
	switch (index) {
	case DA8XX_USB1_PHY_CLK_PARENT_USB_REFCLKIN:
		val = CFGCHIP2_USB1PHYCLKMUX;
		break;
	case DA8XX_USB1_PHY_CLK_PARENT_USB0_PHY_PLL:
		val = 0;
		break;
	default:
		return -EINVAL;
	}

	regmap_write_bits(clk->regmap, CFGCHIP(2), mask, val);

	return 0;
}

static u8 da8xx_usb1_phy_clk_get_parent(struct clk_hw *hw)
{
	struct da8xx_usb1_phy_clk *clk = to_da8xx_usb1_phy_clk(hw);
	unsigned int val;

	regmap_read(clk->regmap, CFGCHIP(2), &val);

	if (val & CFGCHIP2_USB1PHYCLKMUX)
		return DA8XX_USB1_PHY_CLK_PARENT_USB_REFCLKIN;

	return DA8XX_USB1_PHY_CLK_PARENT_USB0_PHY_PLL;
}

static const struct clk_ops da8xx_usb1_phy_clk_ops = {
	.set_parent	= da8xx_usb1_phy_clk_set_parent,
	.get_parent	= da8xx_usb1_phy_clk_get_parent,
};

/**
 * da8xx_usb1_phy_clk_register - Register a new USB 1.1 PHY clock
 * @name: The clock name
 * @parent0: The name of the USB 2.0 PHY clock
 * @parent1: The name of the USB_REFCLKIN clock
 * @regmap: The CFGCHIP regmap
 */
struct clk *da8xx_usb1_phy_clk_register(const char *name,
					const char *parent0,
					const char *parent1,
					struct regmap *regmap)
{
	const char * const parent_names[] = {
		[DA8XX_USB1_PHY_CLK_PARENT_USB0_PHY_PLL] = parent0,
		[DA8XX_USB1_PHY_CLK_PARENT_USB_REFCLKIN] = parent1,
	};
	struct da8xx_usb1_phy_clk *clk;
	struct clk_init_data init;

	clk = kzalloc(sizeof(*clk), GFP_KERNEL);
	if (!clk)
		return ERR_PTR(-ENOMEM);

	init.name = name;
	init.ops = &da8xx_usb1_phy_clk_ops;
	init.parent_names = parent_names;
	init.num_parents = 2;

	clk->hw.init = &init;
	clk->regmap = regmap;

	return clk_register(NULL, &clk->hw);
}

#ifdef CONFIG_OF
static void da8xx_usb_phy_clk_init(struct device_node *np)
{
	struct clk_onecell_data *clk_data;
	struct regmap *regmap;
	struct clk *fck_clk, *clk;

	regmap = syscon_node_to_regmap(of_get_parent(np));
	if (IS_ERR(regmap)) {
		pr_err("No regmap for syscon parent (%ld)", PTR_ERR(regmap));
		return;
	}

	fck_clk = of_clk_get_by_name(np, "fck");
	if (IS_ERR(fck_clk)) {
		pr_err("Missing fck clock (%ld)", PTR_ERR(fck_clk));
		return;
	}

	clk_data = clk_alloc_onecell_data(2);
	if (!clk_data) {
		clk_put(fck_clk);
		return;
	}

	clk = da8xx_usb0_phy_clk_register("usb0_clk48", "usb_refclkin",
					  "pll0_auxclk", fck_clk, regmap);
	if (IS_ERR(clk)) {
		pr_warn("Failed to register usb0_clk48 (%ld)", PTR_ERR(clk));
		clk_put(fck_clk);
	} else {
		clk_data->clks[0] = clk;
	}

	clk = da8xx_usb1_phy_clk_register("usb1_clk48", "usb0_clk48",
					  "usb_refclkin", regmap);
	if (IS_ERR(clk))
		pr_warn("Failed to register usb1_clk48 (%ld)", PTR_ERR(clk));
	else
		clk_data->clks[1] = clk;

	of_clk_add_provider(np, of_clk_src_onecell_get, clk_data);
}

CLK_OF_DECLARE(da8xx_usb_phy_clk, "ti,da830-usb-phy-clocks",
	       da8xx_usb_phy_clk_init);
#endif
