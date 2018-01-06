// SPDX-License-Identifier: GPL-2.0
/*
 * PLL clock descriptions for TI DA830/OMAP-L137/AM17XX
 *
 * Copyright (C) 2017 David Lechner <david@lechnology.com>
 */

#include <linux/init.h>
#include <linux/types.h>

#include "pll.h"

void __init da830_pll_clk_init(void __iomem *pll)
{
	davinci_pll_clk_register("pll0", "ref_clk", pll);
	davinci_pll_aux_clk_register("pll0_aux_clk", "ref_clk", pll);
	davinci_pll_div_clk_register("pll0_sysclk2", "pll0", pll, 2);
	davinci_pll_div_clk_register("pll0_sysclk3", "pll0", pll, 3);
	davinci_pll_div_clk_register("pll0_sysclk4", "pll0", pll, 4);
	davinci_pll_div_clk_register("pll0_sysclk5", "pll0", pll, 5);
	davinci_pll_div_clk_register("pll0_sysclk6", "pll0", pll, 6);
	davinci_pll_div_clk_register("pll0_sysclk7", "pll0", pll, 7);
}
