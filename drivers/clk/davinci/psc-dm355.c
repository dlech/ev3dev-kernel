// SPDX-License-Identifier: GPL-2.0
/*
 * PSC clock descriptions for TI DaVinci DM355
 *
 * Copyright (C) 2017 David Lechner <david@lechnology.com>
 */

#include <linux/clk-provider.h>
#include <linux/clkdev.h>
#include <linux/init.h>
#include <linux/types.h>

#include "psc.h"

static const struct davinci_psc_clk_info dm355_psc_info[] __initconst = {
	LPSC(0, 0, vpss_master, pll1_sysclk4, 0),
	LPSC(1, 0, vpss_slave, pll1_sysclk4, 0),
	LPSC(5, 0, timer3, pll1_auxclk, 0),
	LPSC(6, 0, spi1, pll1_sysclk2, 0),
	LPSC(7, 0, mmcsd1, pll1_sysclk2, 0),
	LPSC(8, 0, asp1, pll1_sysclk2, 0),
	LPSC(9, 0, usb, pll1_sysclk2, 0),
	LPSC(10, 0, pwm3, pll1_auxclk, 0),
	LPSC(11, 0, spi2, pll1_sysclk2, 0),
	LPSC(12, 0, rto, pll1_auxclk, 0),
	LPSC(14, 0, aemif, pll1_sysclk2, 0),
	LPSC(15, 0, mmcsd0, pll1_sysclk2, 0),
	LPSC(17, 0, asp0, pll1_sysclk2, 0),
	LPSC(18, 0, i2c, pll1_auxclk, 0),
	LPSC(19, 0, uart0, pll1_auxclk, 0),
	LPSC(20, 0, uart1, pll1_auxclk, 0),
	LPSC(21, 0, uart2, pll1_sysclk2, 0),
	LPSC(22, 0, spi0, pll1_sysclk2, 0),
	LPSC(23, 0, pwm0, pll1_auxclk, 0),
	LPSC(24, 0, pwm1, pll1_auxclk, 0),
	LPSC(25, 0, pwm2, pll1_auxclk, 0),
	LPSC(26, 0, gpio, pll1_sysclk2, 0),
	LPSC(27, 0, timer0, pll1_auxclk, 0),
	LPSC(28, 0, timer1, pll1_auxclk, 0),
	/* REVISIT: why can't this be disabled? */
	LPSC(29, 0, timer2, pll1_auxclk, LPSC_ALWAYS_ENABLED),
	LPSC(31, 0, arm, pll1_sysclk1, LPSC_ALWAYS_ENABLED),
	LPSC(40, 0, mjcp, pll1_sysclk1, 0),
	LPSC(41, 0, vpss_dac, pll1_sysclk3, 0),
	{ }
};

void __init dm355_psc_clk_init(void __iomem *psc)
{
	struct clk_onecell_data *clk_data;

	clk_data = davinci_psc_register_clocks(psc, dm355_psc_info, 42);
	if (!clk_data)
		return;

	clk_register_clkdev(clk_data->clks[0], "master", "vpss");
	clk_register_clkdev(clk_data->clks[1], "slave", "vpss");
	clk_register_clkdev(clk_data->clks[6], NULL, "spi_davinci.1");
	clk_register_clkdev(clk_data->clks[7], NULL, "dm6441-mmc.1");
	clk_register_clkdev(clk_data->clks[8], NULL, "davinci-mcbsp.1");
	clk_register_clkdev(clk_data->clks[9], "usb", NULL);
	clk_register_clkdev(clk_data->clks[11], NULL, "spi_davinci.2");
	clk_register_clkdev(clk_data->clks[14], "aemif", NULL);
	clk_register_clkdev(clk_data->clks[15], NULL, "dm6441-mmc.0");
	clk_register_clkdev(clk_data->clks[17], NULL, "davinci-mcbsp.0");
	clk_register_clkdev(clk_data->clks[18], NULL, "i2c_davinci.1");
	clk_register_clkdev(clk_data->clks[19], NULL, "serial8250.0");
	clk_register_clkdev(clk_data->clks[20], NULL, "serial8250.1");
	clk_register_clkdev(clk_data->clks[21], NULL, "serial8250.2");
	clk_register_clkdev(clk_data->clks[22], NULL, "spi_davinci.0");
	clk_register_clkdev(clk_data->clks[26], "gpio", NULL);
	clk_register_clkdev(clk_data->clks[27], "timer0", NULL);
	clk_register_clkdev(clk_data->clks[29], NULL, "davinci-wdt");
	clk_register_clkdev(clk_data->clks[31], "arm", NULL);
	clk_register_clkdev(clk_data->clks[41], "vpss_dac", NULL);

	clk_free_onecell_data(clk_data);
}
