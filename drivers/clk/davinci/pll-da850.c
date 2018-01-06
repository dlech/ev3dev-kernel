// SPDX-License-Identifier: GPL-2.0
/*
 * PLL clock descriptions for TI DA850/OMAP-L138/AM18XX
 *
 * Copyright (C) 2017 David Lechner <david@lechnology.com>
 */

#include <linux/clk-provider.h>
#include <linux/clkdev.h>
#include <linux/init.h>
#include <linux/of.h>
#include <linux/types.h>

#include "pll.h"

void __init da850_pll_clk_init(void __iomem *pll0, void __iomem *pll1)
{
	struct clk *clk;

	davinci_pll_clk_register("pll0", "ref_clk", pll0);
	davinci_pll_aux_clk_register("pll0_aux_clk", "ref_clk", pll0);
	davinci_pll_div_clk_register("pll0_sysclk1", "pll0", pll0, 1);
	davinci_pll_div_clk_register("pll0_sysclk2", "pll0", pll0, 2);
	davinci_pll_div_clk_register("pll0_sysclk3", "pll0", pll0, 3);
	davinci_pll_div_clk_register("pll0_sysclk4", "pll0", pll0, 4);
	davinci_pll_div_clk_register("pll0_sysclk5", "pll0", pll0, 5);
	davinci_pll_div_clk_register("pll0_sysclk6", "pll0", pll0, 6);
	davinci_pll_div_clk_register("pll0_sysclk7", "pll0", pll0, 7);

	davinci_pll_clk_register("pll1", "ref_clk", pll1);
	davinci_pll_aux_clk_register("pll1_aux_clk", "ref_clk", pll1);
	clk = davinci_pll_div_clk_register("pll1_sysclk2", "pll1", pll1, 2);
	clk_register_clkdev(clk, "pll1_sysclk2", NULL);
	davinci_pll_div_clk_register("pll1_sysclk3", "pll1", pll1, 3);
}

#ifdef CONFIG_OF
static void of_da850_pll0_auxclk_init(struct device_node *node)
{
	of_davinci_pll_init(node, "pll0", 7);
}
CLK_OF_DECLARE(da850_pll0_auxclk, "ti,da850-pll0", of_da850_pll0_auxclk_init);

static void of_da850_pll1_auxclk_init(struct device_node *node)
{
	of_davinci_pll_init(node, "pll1", 3);
}
CLK_OF_DECLARE(da850_pll1_auxclk, "ti,da850-pll1", of_da850_pll1_auxclk_init);
#endif
