// SPDX-License-Identifier: GPL-2.0
/*
 * PLL clock descriptions for TI DM355
 *
 * Copyright (C) 2017 David Lechner <david@lechnology.com>
 */

#include <linux/bitops.h>
#include <linux/init.h>
#include <linux/types.h>

#include "pll.h"

static const struct davinci_pll_clk_info dm355_pll1_info __initconst = {
	.name = "pll1",
	.pllm_mask = GENMASK(7, 0),
	.pllm_min = 92,
	.pllm_max = 184,
	.flags = PLL_HAS_OSCIN | PLL_HAS_PREDIV | PLL_HAS_POSTDIV,
};

static const struct davinci_pll_sysclk_info dm355_pll1_sysclk_info[] __initconst = {
	SYSCLK(1, pll1_sysclk1, pll1, 5, SYSCLK_FIXED_DIV),
	SYSCLK(2, pll1_sysclk2, pll1, 5, SYSCLK_FIXED_DIV),
	SYSCLK(3, pll1_sysclk3, pll1, 5, 0),
	SYSCLK(4, pll1_sysclk4, pll1, 5, 0),
	{ }
};

static const struct davinci_pll_clk_info dm355_pll2_info __initconst = {
	.name = "pll2",
	.pllm_mask = GENMASK(7, 0),
	.pllm_min = 92,
	.pllm_max = 184,
	.flags = PLL_HAS_PREDIV | PLL_HAS_POSTDIV,
};

static const struct davinci_pll_sysclk_info dm355_pll2_sysclk_info[] __initconst = {
	SYSCLK(1, pll2_sysclk1, pll2, 5, SYSCLK_FIXED_DIV),
	{ }
};

void __init dm355_pll_clk_init(void __iomem *pll1, void __iomem *pll2)
{
	const struct davinci_pll_sysclk_info *info;

	davinci_pll_clk_register(&dm355_pll1_info, "ref_clk", pll1);
	for (info = dm355_pll1_sysclk_info; info->name; info++)
		davinci_pll_sysclk_register(info, pll1);
	davinci_pll_auxclk_register("pll1_auxclk", pll1);
	davinci_pll_sysclkbp_clk_register("pll1_sysclkbp", pll1);

	davinci_pll_clk_register(&dm355_pll2_info, "oscin", pll2);
	for (info = dm355_pll2_sysclk_info; info->name; info++)
		davinci_pll_sysclk_register(info, pll2);
	davinci_pll_sysclkbp_clk_register("pll2_sysclkbp", pll2);
}
