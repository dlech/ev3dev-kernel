// SPDX-License-Identifier: GPL-2.0
/*
 * PLL clock descriptions for TI DM355
 *
 * Copyright (C) 2017 David Lechner <david@lechnology.com>
 */

#include <linux/init.h>
#include <linux/types.h>

#include "pll.h"

void __init dm355_pll_clk_init(void __iomem *pll1, void __iomem *pll2)
{
	davinci_pll_clk_register("pll1", "ref_clk", pll1);
	davinci_pll_div_clk_register("pll1_sysclk1", "pll1", pll1, 1);
	davinci_pll_div_clk_register("pll1_sysclk2", "pll1", pll1, 2);
	davinci_pll_div_clk_register("pll1_sysclk3", "pll1", pll1, 3);
	davinci_pll_div_clk_register("pll1_sysclk4", "pll1", pll1, 4);
	davinci_pll_aux_clk_register("pll1_aux_clk", "ref_clk", pll1);
	davinci_pll_bpdiv_clk_register("pll1_sysclkbp", "ref_clk", pll1);

	davinci_pll_clk_register("pll2", "ref_clk", pll2);
	davinci_pll_div_clk_register("pll2_sysclk1", "pll2", pll2, 1);
	davinci_pll_bpdiv_clk_register("pll2_sysclkbp", "ref_clk", pll2);
}
