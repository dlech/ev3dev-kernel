// SPDX-License-Identifier: GPL-2.0
/*
 * DA8xx USB
 */
#include <linux/clk-provider.h>
#include <linux/clk.h>
#include <linux/clk/davinci.h>
#include <linux/clkdev.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/init.h>
#include <linux/mfd/syscon.h>
#include <linux/phy/phy.h>
#include <linux/platform_data/usb-davinci.h>
#include <linux/platform_device.h>
#include <linux/usb/musb.h>

#include <mach/clock.h>
#include <mach/common.h>
#include <mach/cputype.h>
#include <mach/da8xx.h>
#include <mach/irqs.h>

#include "clock.h"

#define DA8XX_USB0_BASE		0x01e00000
#define DA8XX_USB1_BASE		0x01e25000

static struct platform_device da8xx_usb_phy = {
	.name		= "da8xx-usb-phy",
	.id		= -1,
	.dev		= {
		/*
		 * Setting init_name so that clock lookup will work in
		 * da8xx_register_usb11_phy_clk() even if this device is not
		 * registered yet.
		 */
		.init_name	= "da8xx-usb-phy",
	},
};

int __init da8xx_register_usb_phy(void)
{
	return platform_device_register(&da8xx_usb_phy);
}

static struct musb_hdrc_config musb_config = {
	.multipoint	= true,
	.num_eps	= 5,
	.ram_bits	= 10,
};

static struct musb_hdrc_platform_data usb_data = {
	/* OTG requires a Mini-AB connector */
	.mode           = MUSB_OTG,
	.config		= &musb_config,
};

static struct resource da8xx_usb20_resources[] = {
	{
		.start		= DA8XX_USB0_BASE,
		.end		= DA8XX_USB0_BASE + SZ_64K - 1,
		.flags		= IORESOURCE_MEM,
	},
	{
		.start		= IRQ_DA8XX_USB_INT,
		.flags		= IORESOURCE_IRQ,
		.name		= "mc",
	},
};

static u64 usb_dmamask = DMA_BIT_MASK(32);

static struct platform_device da8xx_usb20_dev = {
	.name		= "musb-da8xx",
	.id             = -1,
	.dev = {
		/*
		 * Setting init_name so that clock lookup will work in
		 * usb20_phy_clk_enable() even if this device is not registered.
		 */
		.init_name		= "musb-da8xx",
		.platform_data		= &usb_data,
		.dma_mask		= &usb_dmamask,
		.coherent_dma_mask      = DMA_BIT_MASK(32),
	},
	.resource	= da8xx_usb20_resources,
	.num_resources	= ARRAY_SIZE(da8xx_usb20_resources),
};

int __init da8xx_register_usb20(unsigned int mA, unsigned int potpgt)
{
	usb_data.power	= mA > 510 ? 255 : mA / 2;
	usb_data.potpgt = (potpgt + 1) / 2;

	return platform_device_register(&da8xx_usb20_dev);
}

static struct resource da8xx_usb11_resources[] = {
	[0] = {
		.start	= DA8XX_USB1_BASE,
		.end	= DA8XX_USB1_BASE + SZ_4K - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= IRQ_DA8XX_IRQN,
		.end	= IRQ_DA8XX_IRQN,
		.flags	= IORESOURCE_IRQ,
	},
};

static u64 da8xx_usb11_dma_mask = DMA_BIT_MASK(32);

static struct platform_device da8xx_usb11_device = {
	.name		= "ohci-da8xx",
	.id		= -1,
	.dev = {
		.dma_mask		= &da8xx_usb11_dma_mask,
		.coherent_dma_mask	= DMA_BIT_MASK(32),
	},
	.num_resources	= ARRAY_SIZE(da8xx_usb11_resources),
	.resource	= da8xx_usb11_resources,
};

int __init da8xx_register_usb11(struct da8xx_ohci_root_hub *pdata)
{
	da8xx_usb11_device.dev.platform_data = pdata;
	return platform_device_register(&da8xx_usb11_device);
}

int __init da8xx_register_usb20_phy_clock(void)
{
	struct regmap *cfgchip;
	struct clk *fck_clk, *clk;

	cfgchip = syscon_regmap_lookup_by_compatible("ti,da830-cfgchip");
	if (IS_ERR(cfgchip))
		cfgchip = syscon_regmap_lookup_by_pdevname("syscon");
	if (IS_ERR(cfgchip))
		return PTR_ERR(cfgchip);

	fck_clk = clk_get_sys("musb-da8xx", NULL);
	if (IS_ERR(fck_clk))
		return PTR_ERR(fck_clk);

	clk = da8xx_usb0_phy_clk_register("usb0_clk48", "usb_refclkin",
					  "pll0_auxclk", fck_clk, cfgchip);
	if (IS_ERR(clk)) {
		clk_put(fck_clk);
		return PTR_ERR(clk);
	}

	return clk_register_clkdev(clk, "usb20_phy", "da8xx-usb-phy");
}

int __init da8xx_register_usb11_phy_clock(void)
{
	struct regmap *cfgchip;
	struct clk *clk;

	cfgchip = syscon_regmap_lookup_by_compatible("ti,da830-cfgchip");
	if (IS_ERR(cfgchip))
		cfgchip = syscon_regmap_lookup_by_pdevname("syscon");
	if (IS_ERR(cfgchip))
		return PTR_ERR(cfgchip);

	clk = da8xx_usb1_phy_clk_register("usb1_clk48", "usb0_clk48",
					  "usb_refclkin", cfgchip);
	if (IS_ERR(clk))
		return PTR_ERR(clk);

	return clk_register_clkdev(clk, "usb11_phy", "da8xx-usb-phy");
}
