// SPDX-License-Identifier: GPL-2.0
/*
 * PSC clock descriptions for TI DA830/OMAP-L137/AM17XX
 *
 * Copyright (C) 2017 David Lechner <david@lechnology.com>
 */

#include <linux/clk-provider.h>
#include <linux/clkdev.h>
#include <linux/init.h>
#include <linux/types.h>

#include "psc.h"

static const struct davinci_psc_clk_info da830_psc0_info[] __initconst = {
	LPSC(0, 0, tpcc, pll0_sysclk2, LPSC_ALWAYS_ENABLED),
	LPSC(1, 0, tptc0, pll0_sysclk2, LPSC_ALWAYS_ENABLED),
	LPSC(2, 0, tptc1, pll0_sysclk2, LPSC_ALWAYS_ENABLED),
	LPSC(3, 0, aemif, pll0_sysclk3, LPSC_ALWAYS_ENABLED),
	LPSC(4, 0, spi0, pll0_sysclk2, 0),
	LPSC(5, 0, mmcsd, pll0_sysclk2, 0),
	LPSC(6, 0, aintc, pll0_sysclk4, LPSC_ALWAYS_ENABLED),
	LPSC(7, 0, arm_rom, pll0_sysclk2, LPSC_ALWAYS_ENABLED),
	LPSC(8, 0, secu_mgr, pll0_sysclk4, LPSC_ALWAYS_ENABLED),
	LPSC(9, 0, uart0, pll0_sysclk2, 0),
	LPSC(10, 0, scr0_ss, pll0_sysclk2, LPSC_ALWAYS_ENABLED),
	LPSC(11, 0, scr1_ss, pll0_sysclk2, LPSC_ALWAYS_ENABLED),
	LPSC(12, 0, scr2_ss, pll0_sysclk2, LPSC_ALWAYS_ENABLED),
	LPSC(13, 0, dmax, pll0_sysclk2, LPSC_ALWAYS_ENABLED),
	LPSC(14, 0, arm, pll0_sysclk6, LPSC_ALWAYS_ENABLED),
	{ }
};

static const struct davinci_psc_clk_info da830_psc1_info[] __initconst = {
	LPSC(1, 0, usb0, pll0_sysclk2, 0),
	LPSC(2, 0, usb1, pll0_sysclk4, 0),
	LPSC(3, 0, gpio, pll0_sysclk4, 0),
	LPSC(5, 0, emac, pll0_sysclk4, 0),
	LPSC(6, 0, emif3, pll0_sysclk5, LPSC_ALWAYS_ENABLED),
	LPSC(7, 0, mcasp0, pll0_sysclk2, 0),
	LPSC(8, 0, mcasp1, pll0_sysclk2, 0),
	LPSC(9, 0, mcasp2, pll0_sysclk2, 0),
	LPSC(10, 0, spi1, pll0_sysclk2, 0),
	LPSC(11, 0, i2c1, pll0_sysclk4, 0),
	LPSC(12, 0, uart1, pll0_sysclk2, 0),
	LPSC(13, 0, uart2, pll0_sysclk2, 0),
	LPSC(16, 0, lcdc, pll0_sysclk2, 0),
	LPSC(17, 0, pwm, pll0_sysclk2, 0),
	LPSC(20, 0, ecap, pll0_sysclk2, 0),
	LPSC(21, 0, eqep, pll0_sysclk2, 0),
	{ }
};

void __init da830_psc_clk_init(void __iomem *psc0, void __iomem *psc1)
{
	struct clk_onecell_data *clk_data;

	clk_data = davinci_psc_register_clocks(psc0, da830_psc0_info, 16);
	if (!clk_data)
		return;

	clk_register_clkdev(clk_data->clks[4], NULL, "spi_davinci.0");
	clk_register_clkdev(clk_data->clks[5], NULL, "da830-mmc.0");
	clk_register_clkdev(clk_data->clks[9], NULL, "serial8250.0");
	clk_register_clkdev(clk_data->clks[14], "arm", NULL);

	clk_free_onecell_data(clk_data);

	clk_data = davinci_psc_register_clocks(psc1, da830_psc1_info, 32);
	if (!clk_data)
		return;

	clk_register_clkdev(clk_data->clks[1], NULL, "musb-da8xx");
	clk_register_clkdev(clk_data->clks[1], NULL, "cppi41-dmaengine");
	clk_register_clkdev(clk_data->clks[2], NULL, "ohci-da8xx");
	clk_register_clkdev(clk_data->clks[3], "gpio", NULL);
	clk_register_clkdev(clk_data->clks[5], NULL, "davinci_emac.1");
	clk_register_clkdev(clk_data->clks[5], "fck", "davinci_mdio.0");
	clk_register_clkdev(clk_data->clks[7], NULL, "davinci-mcasp.0");
	clk_register_clkdev(clk_data->clks[8], NULL, "davinci-mcasp.1");
	clk_register_clkdev(clk_data->clks[9], NULL, "davinci-mcasp.2");
	clk_register_clkdev(clk_data->clks[10], NULL, "spi_davinci.1");
	clk_register_clkdev(clk_data->clks[11], NULL, "i2c_davinci.2");
	clk_register_clkdev(clk_data->clks[12], NULL, "serial8250.1");
	clk_register_clkdev(clk_data->clks[13], NULL, "serial8250.2");
	clk_register_clkdev(clk_data->clks[16], "fck", "da8xx_lcdc.0");
	clk_register_clkdev(clk_data->clks[17], "fck", "ehrpwm.0");
	clk_register_clkdev(clk_data->clks[17], "fck", "ehrpwm.1");
	clk_register_clkdev(clk_data->clks[20], "fck", "ecap.0");
	clk_register_clkdev(clk_data->clks[20], "fck", "ecap.1");
	clk_register_clkdev(clk_data->clks[20], "fck", "ecap.2");
	clk_register_clkdev(clk_data->clks[21], NULL, "eqep.0");
	clk_register_clkdev(clk_data->clks[21], NULL, "eqep.1");

	clk_free_onecell_data(clk_data);
}
