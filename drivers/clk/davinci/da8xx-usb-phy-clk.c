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

#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/mfd/da8xx-cfgchip.h>
#include <linux/mfd/syscon.h>
#include <linux/of.h>
#include <linux/mfd/da8xx-cfgchip.h>
#include <linux/mfd/syscon.h>
#include <linux/module.h>
#include <linux/platform_data/davinci_clk.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>

/**
 * da8xx_usb_phy_clk
 * @usb0_hw: The USB 2.0 PHY clock (mux + PLL)
 * @usb1_hw: The USB 1.1 PHY clock (mux)
 * @usb0_clk: The USB 2.0 subsystem PSC clock
 * @regmap: The CFGCHIP syscon regmap
 */
struct da8xx_usb_phy_clk {
	struct clk_hw usb0_hw;
	struct clk_hw usb1_hw;
	struct clk *usb0_clk;
	struct regmap *regmap;
};

/* The USB 2.0 PHY can use either USB_REFCLKIN or AUXCLK */
enum usb0_phy_clk_parent {
	USB20_PHY_CLK_PARENT_USB_REFCLKIN,
	USB20_PHY_CLK_PARENT_PLL0_AUX,
};

/* The USB 1.1 PHY can use either the PLL output from the USB 2.0 PHY or
 * USB_REFCLKIN
 */
enum usb1_phy_clk_parent {
	USB1_PHY_CLK_PARENT_USB_REFCLKIN,
	USB1_PHY_CLK_PARENT_USB0_PHY_PLL,
};

/* --- USB 2.0 PHY clock --- */

static int usb0_phy_clk_prepare(struct clk_hw *hw)
{
	struct da8xx_usb_phy_clk *clk =
			container_of(hw, struct da8xx_usb_phy_clk, usb0_hw);

	/* The USB 2.0 PSC clock is only needed temporarily during the USB 2.0
	 * PHY clock enable, but since clk_prepare() can't be called in an
	 * atomic context (i.e. in clk_enable()), we have to prepare it here.
	 */
	return clk_prepare(clk->usb0_clk);
}

static void usb0_phy_clk_unprepare(struct clk_hw *hw)
{
	struct da8xx_usb_phy_clk *clk =
			container_of(hw, struct da8xx_usb_phy_clk, usb0_hw);

	clk_unprepare(clk->usb0_clk);
}

static int usb0_phy_clk_enable(struct clk_hw *hw)
{
	struct da8xx_usb_phy_clk *clk =
			container_of(hw, struct da8xx_usb_phy_clk, usb0_hw);
	unsigned int mask, val;
	int ret;

	/* Locking the USB 2.O PLL requires that the USB 2.O PSC is enabled
	 * temporaily. It can be turned back off once the PLL is locked.
	 */
	clk_enable(clk->usb0_clk);

	/* Turn on the USB 2.0 PHY, but just the PLL, and not OTG. The USB 1.1
	 * PHY may use the USB 2.0 PLL clock without USB 2.0 OTG being used.
	 */
	mask = CFGCHIP2_RESET | CFGCHIP2_PHYPWRDN | CFGCHIP2_PHY_PLLON;
	val = CFGCHIP2_PHY_PLLON;

	regmap_write_bits(clk->regmap, CFGCHIP(2), mask, val);
	ret = regmap_read_poll_timeout(clk->regmap, CFGCHIP(2), val,
				       val & CFGCHIP2_PHYCLKGD, 0, 500000);

	clk_disable(clk->usb0_clk);

	return ret;
}

static void usb0_phy_clk_disable(struct clk_hw *hw)
{
	struct da8xx_usb_phy_clk *clk =
			container_of(hw, struct da8xx_usb_phy_clk, usb0_hw);
	unsigned int val;

	val = CFGCHIP2_PHYPWRDN;
	regmap_write_bits(clk->regmap, CFGCHIP(2), val, val);
}

static unsigned long usb0_phy_clk_recalc_rate(struct clk_hw *hw,
					      unsigned long parent_rate)
{
	struct da8xx_usb_phy_clk *clk =
			container_of(hw, struct da8xx_usb_phy_clk, usb0_hw);
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

static long usb0_phy_clk_round_rate(struct clk_hw *hw, unsigned long rate,
				    unsigned long *parent_rate)
{
	return 48000000;
}

static int usb0_phy_clk_set_parent(struct clk_hw *hw, u8 index)
{
	struct da8xx_usb_phy_clk *clk =
			container_of(hw, struct da8xx_usb_phy_clk, usb0_hw);
	unsigned int mask, val;


	/* Set the mux depending on the parent clock. */
	mask = CFGCHIP2_USB2PHYCLKMUX;
	switch (index) {
	case USB20_PHY_CLK_PARENT_USB_REFCLKIN:
		val = 0;
		break;
	case USB20_PHY_CLK_PARENT_PLL0_AUX:
		val = CFGCHIP2_USB2PHYCLKMUX;
		break;
	default:
		return -EINVAL;
	}

	regmap_write_bits(clk->regmap, CFGCHIP(2), mask, val);

	return 0;
}

static u8 usb0_phy_clk_get_parent(struct clk_hw *hw)
{
	struct da8xx_usb_phy_clk *clk =
			container_of(hw, struct da8xx_usb_phy_clk, usb0_hw);
	unsigned int val;

	regmap_read(clk->regmap, CFGCHIP(2), &val);

	if (val & CFGCHIP2_USB2PHYCLKMUX)
		return USB20_PHY_CLK_PARENT_PLL0_AUX;

	return USB20_PHY_CLK_PARENT_USB_REFCLKIN;
}

static const struct clk_ops usb0_phy_clk_ops = {
	.prepare	= usb0_phy_clk_prepare,
	.unprepare	= usb0_phy_clk_unprepare,
	.enable		= usb0_phy_clk_enable,
	.disable	= usb0_phy_clk_disable,
	.recalc_rate	= usb0_phy_clk_recalc_rate,
	.round_rate	= usb0_phy_clk_round_rate,
	.set_parent	= usb0_phy_clk_set_parent,
	.get_parent	= usb0_phy_clk_get_parent,
};

static const char * const usb0_phy_clk_parent_names[] = {
	[USB20_PHY_CLK_PARENT_USB_REFCLKIN]	= "usb_refclkin",
	[USB20_PHY_CLK_PARENT_PLL0_AUX]		= "pll0_aux_clk",
};

static const struct clk_init_data usb0_phy_clk_init_data = {
	.name		= "usb0_phy_clk",
	.ops		= &usb0_phy_clk_ops,
	.parent_names	= usb0_phy_clk_parent_names,
	.num_parents	= ARRAY_SIZE(usb0_phy_clk_parent_names),
};

/* --- USB 1.1 PHY clock --- */

static int usb1_phy_clk_set_parent(struct clk_hw *hw, u8 index)
{
	struct da8xx_usb_phy_clk *clk =
			container_of(hw, struct da8xx_usb_phy_clk, usb1_hw);
	unsigned int mask, val;

	/* Set the USB 1.1 PHY clock mux based on the parent clock. */
	mask = CFGCHIP2_USB1PHYCLKMUX;
	switch (index) {
	case USB1_PHY_CLK_PARENT_USB_REFCLKIN:
		val = CFGCHIP2_USB1PHYCLKMUX;
		break;
	case USB1_PHY_CLK_PARENT_USB0_PHY_PLL:
		val = 0;
		break;
	default:
		return -EINVAL;
	}

	regmap_write_bits(clk->regmap, CFGCHIP(2), mask, val);

	return 0;
}

static u8 usb1_phy_clk_get_parent(struct clk_hw *hw)
{
	struct da8xx_usb_phy_clk *clk =
			container_of(hw, struct da8xx_usb_phy_clk, usb1_hw);
	unsigned int val;

	regmap_read(clk->regmap, CFGCHIP(2), &val);

	if (val & CFGCHIP2_USB1PHYCLKMUX)
		return USB1_PHY_CLK_PARENT_USB_REFCLKIN;

	return USB1_PHY_CLK_PARENT_USB0_PHY_PLL;
}

static const struct clk_ops usb1_phy_clk_ops = {
	.set_parent	= usb1_phy_clk_set_parent,
	.get_parent	= usb1_phy_clk_get_parent,
};

static const char * const usb1_phy_clk_parent_names[] = {
	[USB1_PHY_CLK_PARENT_USB_REFCLKIN]	= "usb_refclkin",
	[USB1_PHY_CLK_PARENT_USB0_PHY_PLL]	= "usb0_phy_clk",
};

static struct clk_init_data usb1_phy_clk_init_data = {
	.name		= "usb1_phy_clk",
	.ops		= &usb1_phy_clk_ops,
	.parent_names	= usb1_phy_clk_parent_names,
	.num_parents	= ARRAY_SIZE(usb1_phy_clk_parent_names),
};

static struct clk *da8xx_usb_phy_clk_src_get(struct of_phandle_args *clkspec,
					     void *data)
{
	struct da8xx_usb_phy_clk *clk = data;

	if (clkspec->args_count != 1)
		return ERR_PTR(-EINVAL);

	switch (clkspec->args[0]) {
	case 0:
		return clk->usb0_hw.clk;
	case 1:
		return clk->usb1_hw.clk;
	default:
		return ERR_PTR(-EINVAL);
	}
}

#ifdef CONFIG_OF
static void da8xx_usb0_phy_clk_init(struct device_node *np)
{
	struct of_phandle_args clkspec;
	const char *name = np->name;
	const char *parent0, *parent1;
	struct regmap *regmap;
	struct clk *usb0_psc_clk, *clk;
	int ret;

	of_property_read_string(np, "clock-output-names", &name);
	parent0 = of_clk_get_parent_name(np, 0);
	parent1 = of_clk_get_parent_name(np, 1);
	
	regmap = syscon_node_to_regmap(of_get_parent(np));
	if (IS_ERR(regmap)) {
		pr_err("%s: No regmap for syscon parent of %s (%ld)\n",
		       __func__, np->full_name, PTR_ERR(regmap));
		return;
	}

	ret = of_parse_phandle_with_args(np, "usb0-psc-clocks", "#clock-cells",
					 0, &clkspec);
	if (ret < 0) {
		pr_err("%s: Could not get usb0-psc-clocks property for %s (%d)\n",
		       __func__, np->full_name, ret);
		return;
	}

	usb0_psc_clk = of_clk_get_from_provider(&clkspec);
	if (IS_ERR(usb0_psc_clk)) {
		pr_err("%s: Could not get usb0-psc-clocks for %s (%ld)\n",
		       __func__, np->full_name, PTR_ERR(usb0_psc_clk));
		return;
	}

	clk = da8xx_usb0_phy_clk_register(name, parent0, parent1, usb0_psc_clk,
					  regmap);
	if (IS_ERR(clk)) {
		pr_err("%s: Failed to register clock %s (%ld)\n",
		       __func__, np->full_name, PTR_ERR(clk));
		clk_put(usb0_psc_clk);
		return;
	}

	of_clk_add_provider(np, of_clk_src_simple_get, clk);
}

CLK_OF_DECLARE(da8xx_usb0_phy_clk, "ti,da830-usb0-phy-clock",
	       da8xx_usb0_phy_clk_init);

static void da8xx_usb1_phy_clk_init(struct device_node *np)
{
	const char *name = np->name;
	const char *parent0, *parent1;
	struct regmap *regmap;
	struct clk *clk;

	of_property_read_string(np, "clock-output-names", &name);
	parent0 = of_clk_get_parent_name(np, 0);
	parent1 = of_clk_get_parent_name(np, 1);
	
	regmap = syscon_node_to_regmap(of_get_parent(np));
	if (IS_ERR(regmap)) {
		pr_err("%s: No regmap for syscon parent of %s (%ld)\n",
		       __func__, np->full_name, PTR_ERR(regmap));
		return;
	}

	clk = da8xx_usb1_phy_clk_register(name, parent0, parent1, regmap);
	if (IS_ERR(clk)) {
		pr_err("%s: Failed to register clock %s (%ld)\n",
		       __func__, np->full_name, PTR_ERR(clk));
		return;
	}

	of_clk_add_provider(np, of_clk_src_simple_get, clk);
}

CLK_OF_DECLARE(da8xx_usb1_phy_clk, "ti,da830-usb1-phy-clock",
	       da8xx_usb1_phy_clk_init);
#endif
