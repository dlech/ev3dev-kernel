// SPDX-License-Identifier: GPL-2.0
/*
 * PSC clock descriptions for TI DaVinci DM646x
 *
 * Copyright (C) 2017 David Lechner <david@lechnology.com>
 */

#include <linux/clk-provider.h>
#include <linux/clkdev.h>
#include <linux/init.h>
#include <linux/types.h>

#include "psc.h"

static const struct davinci_psc_clk_info dm646x_psc_info[] __initconst = {
	LPSC(0, 0, arm, pll1_sysclk2, LPSC_ALWAYS_ENABLED),
	/* REVISIT how to disable? */
	LPSC(1, 0, dsp, pll1_sysclk1, LPSC_ALWAYS_ENABLED),
	LPSC(4, 0, edma_cc, pll1_sysclk2, LPSC_ALWAYS_ENABLED),
	LPSC(5, 0, edma_tc0, pll1_sysclk2, LPSC_ALWAYS_ENABLED),
	LPSC(6, 0, edma_tc1, pll1_sysclk2, LPSC_ALWAYS_ENABLED),
	LPSC(7, 0, edma_tc2, pll1_sysclk2, LPSC_ALWAYS_ENABLED),
	LPSC(8, 0, edma_tc3, pll1_sysclk2, LPSC_ALWAYS_ENABLED),
	LPSC(10, 0, ide, pll1_sysclk4, 0),
	LPSC(14, 0, emac, pll1_sysclk3, 0),
	LPSC(16, 0, vpif0, ref_clk, LPSC_ALWAYS_ENABLED),
	LPSC(17, 0, vpif1, ref_clk, LPSC_ALWAYS_ENABLED),
	LPSC(21, 0, aemif, pll1_sysclk3, LPSC_ALWAYS_ENABLED),
	LPSC(22, 0, mcasp0, pll1_sysclk3, 0),
	LPSC(23, 0, mcasp1, pll1_sysclk3, 0),
	LPSC(26, 0, uart0, aux_clkin, 0),
	LPSC(27, 0, uart1, aux_clkin, 0),
	LPSC(28, 0, uart2, aux_clkin, 0),
	/* REVIST: disabling hangs system */
	LPSC(29, 0, pwm0, pll1_sysclk3, LPSC_ALWAYS_ENABLED),
	/* REVIST: disabling hangs system */
	LPSC(30, 0, pwm1, pll1_sysclk3, LPSC_ALWAYS_ENABLED),
	LPSC(31, 0, i2c, pll1_sysclk3, 0),
	LPSC(33, 0, gpio, pll1_sysclk3, 0),
	LPSC(34, 0, timer0, pll1_sysclk3, 0),
	LPSC(35, 0, timer1, pll1_sysclk3, 0),
	{ }
};

void __init dm646x_psc_clk_init(void __iomem *psc)
{
	struct clk_onecell_data *clk_data;

	clk_data = davinci_psc_register_clocks(psc, dm646x_psc_info, 41);
	if (!clk_data)
		return;

	clk_register_clkdev(clk_data->clks[0], "arm", NULL);
	clk_register_clkdev(clk_data->clks[10], NULL, "palm_bk3710");
	clk_register_clkdev(clk_data->clks[14], NULL, "davinci_emac.1");
	clk_register_clkdev(clk_data->clks[14], "fck", "davinci_mdio.0");
	clk_register_clkdev(clk_data->clks[21], "aemif", NULL);
	clk_register_clkdev(clk_data->clks[22], NULL, "davinci-mcasp.0");
	clk_register_clkdev(clk_data->clks[23], NULL, "davinci-mcasp.1");
	clk_register_clkdev(clk_data->clks[26], NULL, "serial8250.0");
	clk_register_clkdev(clk_data->clks[27], NULL, "serial8250.1");
	clk_register_clkdev(clk_data->clks[28], NULL, "serial8250.2");
	clk_register_clkdev(clk_data->clks[31], NULL, "i2c_davinci.1");
	clk_register_clkdev(clk_data->clks[33], "gpio", NULL);
	clk_register_clkdev(clk_data->clks[34], "timer0", NULL);

	clk_free_onecell_data(clk_data);
}
