// SPDX-License-Identifier: GPL-2.0
/*
 * PLL clock descriptions for TI DM646X
 *
 * Copyright (C) 2017 David Lechner <david@lechnology.com>
 */

#include <linux/init.h>
#include <linux/types.h>

#include "pll.h"

static const struct davinci_pll_clk_info dm646x_pll1_info __initconst = {
	.name = "pll1",
	.pllm_mask = GENMASK(4, 0),
	.pllm_min = 14,
	.pllm_max = 32,
	.flags = 0,
};

static const struct davinci_pll_divclk_info dm646x_pll1_divclk_info[] __initconst = {
	DIVCLK(1, pll1_sysclk1, pll1_pllen, DIVCLK_FIXED_DIV),
	DIVCLK(2, pll1_sysclk2, pll1_pllen, DIVCLK_FIXED_DIV),
	DIVCLK(3, pll1_sysclk3, pll1_pllen, DIVCLK_FIXED_DIV),
	DIVCLK(4, pll1_sysclk4, pll1_pllen, 0),
	DIVCLK(5, pll1_sysclk5, pll1_pllen, 0),
	DIVCLK(6, pll1_sysclk6, pll1_pllen, 0),
	DIVCLK(7, pll1_sysclk7, pll1_pllen, 0),
	DIVCLK(8, pll1_sysclk8, pll1_pllen, 0),
	DIVCLK(9, pll1_sysclk9, pll1_pllen, 0),
	{ }
};

static const struct davinci_pll_clk_info dm646x_pll2_info __initconst = {
	.name = "pll2",
	.pllm_mask = GENMASK(4, 0),
	.pllm_min = 14,
	.pllm_max = 32,
	.flags = 0,
};

static const struct davinci_pll_divclk_info dm646x_pll2_divclk_info[] __initconst = {
	DIVCLK(1, pll2_sysclk1, pll2_pllen, 0),
	{ }
};

void __init dm646x_pll_clk_init(void __iomem *pll1, void __iomem *pll2)
{
	const struct davinci_pll_divclk_info *info;

	davinci_pll_clk_register(&dm646x_pll1_info, "ref_clk", pll1);
	for (info = dm646x_pll1_divclk_info; info->name; info++)
		davinci_pll_divclk_register(info, pll1);
	davinci_pll_bpdiv_clk_register("pll1_sysclkbp", "ref_clk", pll1);
	davinci_pll_aux_clk_register("pll1_aux_clk", "ref_clk", pll1);

	davinci_pll_clk_register(&dm646x_pll2_info, "ref_clk", pll2);
	for (info = dm646x_pll2_divclk_info; info->name; info++)
		davinci_pll_divclk_register(info, pll2);
}
