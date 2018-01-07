// SPDX-License-Identifier: GPL-2.0
/*
 * PLL clock descriptions for TI DM644X
 *
 * Copyright (C) 2018 David Lechner <david@lechnology.com>
 */

#include <linux/bitops.h>
#include <linux/init.h>
#include <linux/types.h>

#include "pll.h"

static const struct davinci_pll_clk_info dm644x_pll1_info __initconst = {
	.name = "pll1",
	.pllm_mask = GENMASK(4, 0),
	.pllm_min = 1,
	.pllm_max = 32,
	.pllout_min_rate = 400000000,
	.pllout_max_rate = 600000000, /* 810MHz @ 1.3V, -810 only */
	.flags = PLL_HAS_OSCIN | PLL_HAS_POSTDIV,
};

static const struct davinci_pll_sysclk_info dm644x_pll1_sysclk_info[] __initconst = {
	SYSCLK(1, pll1_sysclk1, pll1_pllen, 4, SYSCLK_FIXED_DIV),
	SYSCLK(2, pll1_sysclk2, pll1_pllen, 4, SYSCLK_FIXED_DIV),
	SYSCLK(3, pll1_sysclk3, pll1_pllen, 4, SYSCLK_FIXED_DIV),
	SYSCLK(5, pll1_sysclk5, pll1_pllen, 4, SYSCLK_FIXED_DIV),
	{ }
};

static const struct davinci_pll_clk_info dm644x_pll2_info __initconst = {
	.name = "pll2",
	.pllm_mask = GENMASK(4, 0),
	.pllm_min = 1,
	.pllm_max = 32,
	.pllout_min_rate = 400000000,
	.pllout_max_rate = 900000000,
	.flags = PLL_HAS_POSTDIV | PLL_POSTDIV_FIXED_DIV,
};

static const struct davinci_pll_sysclk_info dm644x_pll2_sysclk_info[] __initconst = {
	SYSCLK(1, pll2_sysclk1, pll2_pllen, 4, 0),
	SYSCLK(2, pll2_sysclk2, pll2_pllen, 4, 0),
	{ }
};

void __init dm644x_pll_clk_init(void __iomem *pll1, void __iomem *pll2)
{
	const struct davinci_pll_sysclk_info *info;

	davinci_pll_clk_register(&dm644x_pll1_info, "ref_clk", pll1);

	for (info = dm644x_pll1_sysclk_info; info->name; info++)
		davinci_pll_sysclk_register(info, pll1);

	davinci_pll_auxclk_register("pll1_auxclk", pll1);

	davinci_pll_sysclkbp_clk_register("pll1_sysclkbp", pll1);

	davinci_pll_clk_register(&dm644x_pll2_info, "oscin", pll2);

	for (info = dm644x_pll2_sysclk_info; info->name; info++)
		davinci_pll_sysclk_register(info, pll2);

	davinci_pll_sysclkbp_clk_register("pll2_sysclkbp", pll2);
}
