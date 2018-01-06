// SPDX-License-Identifier: GPL-2.0
/*
 * PLL clock descriptions for TI DM365
 *
 * Copyright (C) 2017 David Lechner <david@lechnology.com>
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/types.h>

#include "pll.h"

static const char * const dm365_pll_obsclk_parent_names[] = {
	"ref_clk",
};

static u32 dm365_pll_obsclk_table[] = {
	0x10,
};

void __init dm365_pll_clk_init(void __iomem *pll1, void __iomem *pll2)
{
	davinci_pll_clk_register("pll1", "ref_clk", pll1);
	davinci_pll_aux_clk_register("pll1_aux_clk", "ref_clk", pll1);
	davinci_pll_bpdiv_clk_register("pll1_sysclkbp", "ref_clk", pll1);
	davinci_pll_obs_clk_register("clkout0", dm365_pll_obsclk_parent_names,
			ARRAY_SIZE(dm365_pll_obsclk_parent_names), pll1,
			dm365_pll_obsclk_table);
	davinci_pll_div_clk_register("pll1_sysclk1", "pll1", pll1, 1);
	davinci_pll_div_clk_register("pll1_sysclk2", "pll1", pll1, 2);
	davinci_pll_div_clk_register("pll1_sysclk3", "pll1", pll1, 3);
	davinci_pll_div_clk_register("pll1_sysclk4", "pll1", pll1, 4);
	davinci_pll_div_clk_register("pll1_sysclk5", "pll1", pll1, 5);
	davinci_pll_div_clk_register("pll1_sysclk6", "pll1", pll1, 6);
	davinci_pll_div_clk_register("pll1_sysclk7", "pll1", pll1, 7);
	davinci_pll_div_clk_register("pll1_sysclk8", "pll1", pll1, 8);
	davinci_pll_div_clk_register("pll1_sysclk9", "pll1", pll1, 9);

	davinci_pll_clk_register("pll2", "ref_clk", pll2);
	davinci_pll_aux_clk_register("clkout1", "ref_clk", pll2);
	davinci_pll_obs_clk_register("clkout1", dm365_pll_obsclk_parent_names,
			ARRAY_SIZE(dm365_pll_obsclk_parent_names), pll2,
			dm365_pll_obsclk_table);
	davinci_pll_div_clk_register("pll2_sysclk1", "pll2", pll2, 1);
	davinci_pll_div_clk_register("pll2_sysclk2", "pll2", pll2, 2);
	davinci_pll_div_clk_register("pll2_sysclk3", "pll2", pll2, 3);
	davinci_pll_div_clk_register("pll2_sysclk4", "pll2", pll2, 4);
	davinci_pll_div_clk_register("pll2_sysclk5", "pll2", pll2, 5);
	davinci_pll_div_clk_register("pll2_sysclk6", "pll2", pll2, 6);
	davinci_pll_div_clk_register("pll2_sysclk7", "pll2", pll2, 7);
	davinci_pll_div_clk_register("pll2_sysclk8", "pll2", pll2, 8);
	davinci_pll_div_clk_register("pll2_sysclk9", "pll2", pll2, 9);
}
