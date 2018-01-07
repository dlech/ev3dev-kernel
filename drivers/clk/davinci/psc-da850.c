// SPDX-License-Identifier: GPL-2.0
/*
 * PSC clock descriptions for TI DA850/OMAP-L138/AM18XX
 *
 * Copyright (C) 2017 David Lechner <david@lechnology.com>
 */

#include <linux/clk-provider.h>
#include <linux/clkdev.h>
#include <linux/init.h>
#include <linux/of.h>
#include <linux/types.h>

#include "psc.h"

static const struct davinci_psc_clk_info da850_psc0_info[] __initconst = {
	LPSC(0, 0, tpcc0, pll0_sysclk2, LPSC_ALWAYS_ENABLED),
	LPSC(1, 0, tptc0, pll0_sysclk2, LPSC_ALWAYS_ENABLED),
	LPSC(2, 0, tptc1, pll0_sysclk2, LPSC_ALWAYS_ENABLED),
	LPSC(3, 0, aemif, pll0_sysclk3, 0),
	LPSC(4, 0, spi0, pll0_sysclk2, 0),
	LPSC(5, 0, mmcsd0, pll0_sysclk2, 0),
	LPSC(6, 0, aintc, pll0_sysclk4, LPSC_ALWAYS_ENABLED),
	LPSC(7, 0, arm_rom, pll0_sysclk2, LPSC_ALWAYS_ENABLED),
	LPSC(9, 0, uart0, pll0_sysclk2, 0),
	LPSC(13, 0, pruss, pll0_sysclk2, 0),
	LPSC(14, 0, arm, pll0_sysclk6, LPSC_ALWAYS_ENABLED),
	LPSC(15, 1, dsp, pll0_sysclk1, LPSC_FORCE | LPSC_LOCAL_RESET),
	{ }
};

static const struct davinci_psc_clk_info da850_psc1_info[] __initconst = {
	LPSC(0, 0, tpcc1, pll0_sysclk2, LPSC_ALWAYS_ENABLED),
	LPSC(1, 0, usb0, pll0_sysclk2, 0),
	LPSC(2, 0, usb1, pll0_sysclk4, 0),
	LPSC(3, 0, gpio, pll0_sysclk4, 0),
	LPSC(5, 0, emac, pll0_sysclk4, 0),
	LPSC(6, 0, emif3, pll0_sysclk5, LPSC_ALWAYS_ENABLED),
	LPSC(7, 0, mcasp0, async3, 0),
	LPSC(8, 0, sata, pll0_sysclk2, LPSC_FORCE),
	LPSC(9, 0, vpif, pll0_sysclk2, 0),
	LPSC(10, 0, spi1, async3, 0),
	LPSC(11, 0, i2c1, pll0_sysclk4, 0),
	LPSC(12, 0, uart1, async3, 0),
	LPSC(13, 0, uart2, async3, 0),
	LPSC(14, 0, mcbsp0, async3, 0),
	LPSC(15, 0, mcbsp1, async3, 0),
	LPSC(16, 0, lcdc, pll0_sysclk2, 0),
	LPSC(17, 0, ehrpwm, async3, 0),
	LPSC(18, 0, mmcsd1, pll0_sysclk2, 0),
	LPSC(20, 0, ecap, async3, 0),
	LPSC(21, 0, tptc2, pll0_sysclk2, LPSC_ALWAYS_ENABLED),
	{ }
};

void __init da850_psc_clk_init(void __iomem *psc0, void __iomem *psc1)
{
	struct clk_onecell_data *clk_data;

	clk_data = davinci_psc_register_clocks(psc0, da850_psc0_info, 16);
	if (!clk_data)
		return;

	clk_register_clkdev(clk_data->clks[3], NULL, "ti-aemif");
	clk_register_clkdev(clk_data->clks[3], "aemif", "davinci-nand.0");
	clk_register_clkdev(clk_data->clks[4], NULL, "spi_davinci.0");
	clk_register_clkdev(clk_data->clks[5], NULL, "da830-mmc.0");
	clk_register_clkdev(clk_data->clks[9], NULL, "serial8250.0");
	clk_register_clkdev(clk_data->clks[14], "arm", NULL);
	clk_register_clkdev(clk_data->clks[15], NULL, "davinci-rproc.0");

	clk_free_onecell_data(clk_data);

	clk_data = davinci_psc_register_clocks(psc1, da850_psc1_info, 32);
	if (!clk_data)
		return;

	clk_register_clkdev(clk_data->clks[1], "usb20_psc_clk", NULL);
	clk_register_clkdev(clk_data->clks[1], NULL, "musb-da8xx");
	clk_register_clkdev(clk_data->clks[1], NULL, "cppi41-dmaengine");
	clk_register_clkdev(clk_data->clks[2], NULL, "ohci-da8xx");
	clk_register_clkdev(clk_data->clks[3], "gpio", NULL);
	clk_register_clkdev(clk_data->clks[5], NULL, "davinci_emac.1");
	clk_register_clkdev(clk_data->clks[5], "fck", "davinci_mdio.0");
	clk_register_clkdev(clk_data->clks[7], NULL, "davinci-mcasp.0");
	clk_register_clkdev(clk_data->clks[8], "fck", "ahci_da850");
	clk_register_clkdev(clk_data->clks[9], NULL, "vpif");
	clk_register_clkdev(clk_data->clks[10], NULL, "spi_davinci.1");
	clk_register_clkdev(clk_data->clks[11], NULL, "i2c_davinci.2");
	clk_register_clkdev(clk_data->clks[12], NULL, "serial8250.1");
	clk_register_clkdev(clk_data->clks[13], NULL, "serial8250.2");
	clk_register_clkdev(clk_data->clks[14], NULL, "davinci-mcbsp.0");
	clk_register_clkdev(clk_data->clks[15], NULL, "davinci-mcbsp.1");
	clk_register_clkdev(clk_data->clks[16], "fck", "da8xx_lcdc.0");
	clk_register_clkdev(clk_data->clks[17], "fck", "ehrpwm.0");
	clk_register_clkdev(clk_data->clks[17], "fck", "ehrpwm.1");
	clk_register_clkdev(clk_data->clks[18], NULL, "da830-mmc.1");
	clk_register_clkdev(clk_data->clks[20], "fck", "ecap.0");
	clk_register_clkdev(clk_data->clks[20], "fck", "ecap.1");
	clk_register_clkdev(clk_data->clks[20], "fck", "ecap.2");

	clk_free_onecell_data(clk_data);
}

#ifdef CONFIG_OF
static void __init of_da850_psc0_clk_init(struct device_node *node)
{
	of_davinci_psc_clk_init(node, da850_psc0_info, 16);
}
CLK_OF_DECLARE(da850_psc0_clk, "ti,da850-psc0", of_da850_psc0_clk_init);

static void __init of_da850_psc1_clk_init(struct device_node *node)
{
	of_davinci_psc_clk_init(node, da850_psc1_info, 32);
}
CLK_OF_DECLARE(da850_psc1_clk, "ti,da850-psc1", of_da850_psc1_clk_init);
#endif
