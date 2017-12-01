// SPDX-License-Identifier: GPL-2.0
/*
 * DA8xx USB
 */
#include <linux/clk-provider.h>
#include <linux/clk.h>
#include <linux/clkdev.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/init.h>
#include <linux/mfd/da8xx-cfgchip.h>
#include <linux/phy/phy.h>
#include <linux/platform_data/davinci_clk.h>
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
	.clock		= "usb20",
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

/**
 * da8xx_register_usb_refclkin - register USB_REFCLKIN clock
 *
 * @rate: The clock rate in Hz
 *
 * This clock is only needed if the board provides an external USB_REFCLKIN
 * signal, in which case it will be used as the parent of usb20_phy_clk and/or
 * usb11_phy_clk.
 */
int __init da8xx_register_usb_refclkin(unsigned long rate)
{
	struct clk *clk;

	clk = clk_register_fixed_rate(NULL, "usb_refclkin", NULL, 0, rate);
	if (IS_ERR(clk))
		return PTR_ERR(clk);

	return clk_register_clkdev(clk, "usb_refclkin", NULL);
}

static struct platform_device da8xx_phy_clocks_device = {
	.name		= "da8xx-cfgchip-clk",
	.id		= -1,
};

int __init da8xx_register_usb_phy_clocks(struct da8xx_cfgchip_clk_data *pdata)
{
	da8xx_phy_clocks_device.dev.platform_data = pdata;
	return platform_device_register(&da8xx_phy_clocks_device);
}
