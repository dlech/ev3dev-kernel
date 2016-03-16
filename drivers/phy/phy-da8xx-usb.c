/*
 * phy-da8xx-usb - TI DaVinci DA8XX USB PHY driver
 *
 * Copyright (C) 2016 David Lechner <david@lechnology.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/clk.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/module.h>
#include <linux/phy/phy.h>
#include <linux/platform_device.h>
#include <linux/usb/gadget.h>
#include <linux/usb/musb.h>
#include <linux/usb/otg.h>

/* DA8xx CFGCHIP2 (USB PHY Control) register bits */
#define PHYCLKGD		(1 << 17)
#define VBUSSENSE		(1 << 16)
#define RESET			(1 << 15)
#define OTGMODE_MASK		(3 << 13)
#define NO_OVERRIDE		(0 << 13)
#define FORCE_HOST		(1 << 13)
#define FORCE_DEVICE		(2 << 13)
#define FORCE_HOST_VBUS_LOW	(3 << 13)
#define USB1PHYCLKMUX		(1 << 12)
#define USB2PHYCLKMUX		(1 << 11)
#define PHYPWRDN		(1 << 10)
#define OTGPWRDN		(1 << 9)
#define DATPOL			(1 << 8)
#define USB1SUSPENDM		(1 << 7)
#define PHY_PLLON		(1 << 6)
#define SESENDEN		(1 << 5)
#define VBDTCTEN		(1 << 4)
#define REFFREQ_MASK		(0xf << 0)
#define REFFREQ_12MHZ		(1 << 0)
#define REFFREQ_24MHZ		(2 << 0)
#define REFFREQ_48MHZ		(3 << 0)
#define REFFREQ_19_2MHZ		(4 << 0)
#define REFFREQ_38_4MHZ		(5 << 0)
#define REFFREQ_13MHZ		(6 << 0)
#define REFFREQ_26MHZ		(7 << 0)
#define REFFREQ_20MHZ		(8 << 0)
#define REFFREQ_40MHZ		(9 << 0)

struct da8xx_usbphy {
	struct phy_provider	*phy_provider;
	struct phy		*usb11_phy;
	struct phy		*usb20_phy;
	struct clk		*usb11_clk;
	struct clk		*usb20_clk;
	void __iomem		*phy_ctrl;
};

static inline u32 da8xx_usbphy_readl(void __iomem *base)
{
	return readl(base);
}

static inline void da8xx_usbphy_writel(void __iomem *base, u32 value)
{
	writel(value, base);
}

static int da8xx_usb11_phy_init(struct phy *phy)
{
	struct da8xx_usbphy *d_phy = phy_get_drvdata(phy);
	int ret;
	u32 val;

	ret = clk_prepare_enable(d_phy->usb11_clk);
	if (ret)
		return ret;

	val = da8xx_usbphy_readl(d_phy->phy_ctrl);
	val |= USB1SUSPENDM;
	da8xx_usbphy_writel(d_phy->phy_ctrl, val);

	return 0;
}

static int da8xx_usb11_phy_shutdown(struct phy *phy)
{
	struct da8xx_usbphy *d_phy = phy_get_drvdata(phy);
	u32 val;

	val = da8xx_usbphy_readl(d_phy->phy_ctrl);
	val &= ~USB1SUSPENDM;
	da8xx_usbphy_writel(d_phy->phy_ctrl, val);

	clk_disable_unprepare(d_phy->usb11_clk);

	return 0;
}

static const struct phy_ops da8xx_usb11_phy_ops = {
	.power_on	= da8xx_usb11_phy_init,
	.power_off	= da8xx_usb11_phy_shutdown,
	.owner		= THIS_MODULE,
};

static int da8xx_usb20_phy_init(struct phy *phy)
{
	struct da8xx_usbphy *d_phy = phy_get_drvdata(phy);
	int ret;
	u32 val;

	ret = clk_prepare_enable(d_phy->usb20_clk);
	if (ret)
		return ret;

	val = da8xx_usbphy_readl(d_phy->phy_ctrl);
	val &= ~OTGPWRDN;
	da8xx_usbphy_writel(d_phy->phy_ctrl, val);

	return 0;
}

static int da8xx_usb20_phy_shutdown(struct phy *phy)
{
	struct da8xx_usbphy *d_phy = phy_get_drvdata(phy);
	u32 val;

	val = da8xx_usbphy_readl(d_phy->phy_ctrl);
	val |= OTGPWRDN;
	da8xx_usbphy_writel(d_phy->phy_ctrl, val);

	clk_disable_unprepare(d_phy->usb20_clk);

	return 0;
}

static const struct phy_ops da8xx_usb20_phy_ops = {
	.power_on	= da8xx_usb20_phy_init,
	.power_off	= da8xx_usb20_phy_shutdown,
	.owner		= THIS_MODULE,
};

int da8xx_usb20_phy_set_mode(struct phy *phy, enum musb_mode mode)
{
	struct da8xx_usbphy *d_phy = phy_get_drvdata(phy);
	u32 val;

	val = da8xx_usbphy_readl(d_phy->phy_ctrl);

	val &= ~OTGMODE_MASK;
	switch (mode) {
	case MUSB_HOST:		/* Force VBUS valid, ID = 0 */
		val |= FORCE_HOST;
		break;
	case MUSB_PERIPHERAL:	/* Force VBUS valid, ID = 1 */
		val |= FORCE_DEVICE;
		break;
	case MUSB_OTG:		/* Don't override the VBUS/ID comparators */
		val |= NO_OVERRIDE;
		break;
	default:
		return -EINVAL;
	}

	da8xx_usbphy_writel(d_phy->phy_ctrl, val);

	return 0;
}
EXPORT_SYMBOL_GPL(da8xx_usb20_phy_set_mode);

static struct phy *da8xx_usbphy_of_xlate(struct device *dev,
					 struct of_phandle_args *args)
{
	struct da8xx_usbphy *d_phy = dev_get_drvdata(dev);

	if (!d_phy)
		return ERR_PTR(-ENODEV);

	switch (args->args[0]) {
	case 1:
		return d_phy->usb11_phy;
	case 2:
		return d_phy->usb20_phy;
	default:
		return ERR_PTR(-EINVAL);
	}
}

static int da8xx_usbphy_probe(struct platform_device *pdev)
{
	struct device		*dev = &pdev->dev;
	struct device_node	*node = dev->of_node;
	struct da8xx_usbphy	*d_phy;
	struct resource		*res;

	d_phy = devm_kzalloc(dev, sizeof(*d_phy), GFP_KERNEL);
	if (!d_phy)
		return -ENOMEM;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	d_phy->phy_ctrl = devm_ioremap_resource(dev, res);
	if (IS_ERR(d_phy->phy_ctrl)) {
		dev_err(dev, "Failed to map resource.\n");
		return PTR_ERR(d_phy->phy_ctrl);
	}

	d_phy->usb11_clk = devm_clk_get(dev, "usb11_phy");
	if (IS_ERR(d_phy->usb11_clk)) {
		dev_err(dev, "Failed to get usb11_phy clock.\n");
		return PTR_ERR(d_phy->usb11_clk);
	}

	d_phy->usb20_clk = devm_clk_get(dev, "usb20_phy");
	if (IS_ERR(d_phy->usb20_clk)) {
		dev_err(dev, "Failed to get usb20_phy clock.\n");
		return PTR_ERR(d_phy->usb20_clk);
	}

	d_phy->usb11_phy = devm_phy_create(dev, node, &da8xx_usb11_phy_ops);
	if (IS_ERR(d_phy->usb11_phy)) {
		dev_err(dev, "Failed to create usb11 phy.\n");
		return PTR_ERR(d_phy->usb11_phy);
	}

	d_phy->usb20_phy = devm_phy_create(dev, node, &da8xx_usb20_phy_ops);
	if (IS_ERR(d_phy->usb20_phy)) {
		dev_err(dev, "Failed to create usb20 phy.\n");
		return PTR_ERR(d_phy->usb20_phy);
	}

	platform_set_drvdata(pdev, d_phy);
	phy_set_drvdata(d_phy->usb11_phy, d_phy);
	phy_set_drvdata(d_phy->usb20_phy, d_phy);

	if (node) {
		d_phy->phy_provider = devm_of_phy_provider_register(dev,
							da8xx_usbphy_of_xlate);
		if (IS_ERR(d_phy->phy_provider)) {
			dev_err(dev, "Failed to create phy provider.\n");
			return PTR_ERR(d_phy->phy_provider);
		}
	} else {
		int ret;

		ret = phy_create_lookup(d_phy->usb11_phy, "usbphy", "ohci.0");
		if (ret)
			dev_warn(dev, "Failed to create usb11 phy lookup .\n");
		ret = phy_create_lookup(d_phy->usb20_phy, "usbphy", "musb-da8xx");
		if (ret)
			dev_warn(dev, "Failed to create usb20 phy lookup .\n");
	}

	return 0;
}

static int da8xx_usbphy_remove(struct platform_device *pdev)
{
	struct da8xx_usbphy *d_phy = platform_get_drvdata(pdev);

	if (!pdev->dev.of_node) {
		phy_remove_lookup(d_phy->usb20_phy, "usbphy", "musb-da8xx");
		phy_remove_lookup(d_phy->usb11_phy, "usbphy", "ohci.0");
	}

	return 0;
}

static const struct of_device_id da8xx_usbphy_ids[] = {
	{ .compatible = "ti,da830-usbphy" },
	{ }
};
MODULE_DEVICE_TABLE(of, da8xx_usbphy_ids);

static struct platform_driver da8xx_usbphy_driver = {
	.probe	= da8xx_usbphy_probe,
	.remove	= da8xx_usbphy_remove,
	.driver	= {
		.name	= "da8xx-usbphy",
		.of_match_table = da8xx_usbphy_ids,
	},
};

module_platform_driver(da8xx_usbphy_driver);

MODULE_ALIAS("platform:da8xx-usbphy");
MODULE_AUTHOR("David Lechner <david@lechnology.com>");
MODULE_DESCRIPTION("TI DA8XX USB PHY driver");
MODULE_LICENSE("GPL v2");
