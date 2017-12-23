// SPDX-License-Identifier: GPL-2.0
/*
 * da8xx-cfgchip-clk - TI DaVinci DA8xx CFGCHIP clocks driver
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
#include <linux/clkdev.h>
#include <linux/clk-provider.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/mfd/da8xx-cfgchip.h>
#include <linux/mfd/syscon.h>
#include <linux/module.h>
#include <linux/platform_data/davinci_clk.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>

/**
 * da8xx_cfgchip_clk
 * @usb0_hw: The USB 2.0 PHY clock (mux + PLL)
 * @usb1_hw: The USB 1.1 PHY clock (mux)
 * @usb0_clk: The USB 2.0 subsystem PSC clock
 * @regmap: The CFGCHIP syscon regmap
 */
struct da8xx_cfgchip_clk {
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
	struct da8xx_cfgchip_clk *clk =
			container_of(hw, struct da8xx_cfgchip_clk, usb0_hw);

	/* The USB 2.0 PSC clock is only needed temporarily during the USB 2.0
	 * PHY clock enable, but since clk_prepare() can't be called in an
	 * atomic context (i.e. in clk_enable()), we have to prepare it here.
	 */
	return clk_prepare(clk->usb0_clk);
}

static void usb0_phy_clk_unprepare(struct clk_hw *hw)
{
	struct da8xx_cfgchip_clk *clk =
			container_of(hw, struct da8xx_cfgchip_clk, usb0_hw);

	clk_unprepare(clk->usb0_clk);
}

static int usb0_phy_clk_enable(struct clk_hw *hw)
{
	struct da8xx_cfgchip_clk *clk =
			container_of(hw, struct da8xx_cfgchip_clk, usb0_hw);
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
	struct da8xx_cfgchip_clk *clk =
			container_of(hw, struct da8xx_cfgchip_clk, usb0_hw);
	unsigned int val;

	val = CFGCHIP2_PHYPWRDN;
	regmap_write_bits(clk->regmap, CFGCHIP(2), val, val);
}

static unsigned long usb0_phy_clk_recalc_rate(struct clk_hw *hw,
					      unsigned long parent_rate)
{
	struct da8xx_cfgchip_clk *clk =
			container_of(hw, struct da8xx_cfgchip_clk, usb0_hw);
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
	struct da8xx_cfgchip_clk *clk =
			container_of(hw, struct da8xx_cfgchip_clk, usb0_hw);
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
	struct da8xx_cfgchip_clk *clk =
			container_of(hw, struct da8xx_cfgchip_clk, usb0_hw);
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
	struct da8xx_cfgchip_clk *clk =
			container_of(hw, struct da8xx_cfgchip_clk, usb1_hw);
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
	struct da8xx_cfgchip_clk *clk =
			container_of(hw, struct da8xx_cfgchip_clk, usb1_hw);
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

static struct clk *da8xx_cfgchip_clk_src_get(struct of_phandle_args *clkspec,
					     void *data)
{
	struct da8xx_cfgchip_clk *clk = data;

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

/* --- platform driver --- */

static int da8xx_cfgchip_clk_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct da8xx_cfgchip_clk_data *pdata = dev->platform_data;
	struct device_node *node = dev->of_node;
	struct da8xx_cfgchip_clk *phy_clk;
	const char *parent_name;
	struct clk *parent;
	int ret;

	phy_clk = devm_kzalloc(dev, sizeof(*phy_clk), GFP_KERNEL);
	if (!phy_clk)
		return -ENOMEM;

	platform_set_drvdata(pdev, phy_clk);

	/* try device tree, then fall back to platform device */
	phy_clk->regmap = syscon_regmap_lookup_by_compatible("ti,da830-cfgchip");
	if (IS_ERR(phy_clk->regmap))
		phy_clk->regmap = syscon_regmap_lookup_by_pdevname("syscon");
	if (IS_ERR(phy_clk->regmap)) {
		dev_err(dev, "Failed to get syscon\n");
		return PTR_ERR(phy_clk->regmap);
	}

	/* USB 2.0 subsystem PSC clock - needed to lock PLL */
	phy_clk->usb0_clk = clk_get(dev, "usb20");
	if (IS_ERR(phy_clk->usb0_clk)) {
		dev_err(dev, "Failed to get usb20 clock\n");
		return PTR_ERR(phy_clk->usb0_clk);
	}

	phy_clk->usb0_hw.init = &usb0_phy_clk_init_data;
	ret = devm_clk_hw_register(dev, &phy_clk->usb0_hw);
	if (ret) {
		dev_err(dev, "Failed to register usb0_phy_clk");
		return ret;
	}

	phy_clk->usb1_hw.init = &usb1_phy_clk_init_data;
	ret = devm_clk_hw_register(dev, &phy_clk->usb1_hw);
	if (ret) {
		dev_err(dev, "Failed to register usb1_phy_clk");
		return ret;
	}

	parent_name = pdata->usb0_use_refclkin ? "usb_refclkin" : "pll0_aux";
	parent = devm_clk_get(dev, parent_name);
	if (IS_ERR(parent)) {
		dev_err(dev, "Failed to get usb0 parent clock %s\n",
			parent_name);
		return PTR_ERR(parent);
	}

	ret = clk_set_parent(phy_clk->usb0_hw.clk, parent);
	if (ret) {
		dev_err(dev, "Failed to set usb0 parent clock to %s\n",
			parent_name);
		return ret;
	}

	clk_hw_register_clkdev(&phy_clk->usb0_hw, NULL, "da8xx-cfgchip-clk");

	parent_name = pdata->usb1_use_refclkin ? "usb_refclkin" : "usb0_phy_clk";
	parent = devm_clk_get(dev, parent_name);
	if (IS_ERR(parent)) {
		dev_err(dev, "Failed to get usb1 parent clock %s\n",
			parent_name);
		return PTR_ERR(parent);
	}

	ret = clk_set_parent(phy_clk->usb1_hw.clk, parent);
	if (ret) {
		dev_err(dev, "Failed to set usb1 parent clock to %s\n",
			parent_name);
		return ret;
	}

	if (node) {
		of_clk_add_provider(node, da8xx_cfgchip_clk_src_get, phy_clk);
	} else {
		clk_hw_register_clkdev(&phy_clk->usb0_hw, "usb20_phy",
				       "da8xx-usb-phy");
		clk_hw_register_clkdev(&phy_clk->usb1_hw, "usb11_phy",
				       "da8xx-usb-phy");
	}

	return 0;
}

static const struct of_device_id da8xx_cfgchip_clk_ids[] = {
	{ .compatible = "ti,da830-cfgchip-clk" },
	{ }
};
MODULE_DEVICE_TABLE(of, da8xx_cfgchip_clk_ids);

static struct platform_driver da8xx_cfgchip_clk_driver = {
	.probe = da8xx_cfgchip_clk_probe,
	.driver = {
		.name = "da8xx-cfgchip-clk",
		.of_match_table = da8xx_cfgchip_clk_ids,
	},
};
module_platform_driver(da8xx_cfgchip_clk_driver);

MODULE_ALIAS("platform:da8xx-cfgchip-clk");
MODULE_AUTHOR("David Lechner <david@lechnology.com>");
MODULE_DESCRIPTION("TI DA8xx CFGCHIP clock driver");
MODULE_LICENSE("GPL v2");
