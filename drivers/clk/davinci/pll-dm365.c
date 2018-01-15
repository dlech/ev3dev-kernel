// SPDX-License-Identifier: GPL-2.0
/*
 * PLL clock descriptions for TI DM365
 *
 * Copyright (C) 2017 David Lechner <david@lechnology.com>
 */

#include <linux/bitops.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/types.h>

#include "pll.h"

static const struct davinci_pll_clk_info dm365_pll1_info __initconst = {
	.name = "pll1",
	.pllm_mask = GENMASK(9, 0),
	.pllm_min = 1,
	.pllm_max = 1023,
	.flags = PLL_HAS_PREDIV | PLL_HAS_POSTDIV | PLL_PLLM_2X,
};

static const struct davinci_pll_sysclk_info dm365_pll1_sysclk_info[] __initconst = {
	SYSCLK(1, pll1_sysclk1, pll1_pllen, 0),
	SYSCLK(2, pll1_sysclk2, pll1_pllen, 0),
	SYSCLK(3, pll1_sysclk3, pll1_pllen, 0),
	SYSCLK(4, pll1_sysclk4, pll1_pllen, 0),
	SYSCLK(5, pll1_sysclk5, pll1_pllen, 0),
	SYSCLK(6, pll1_sysclk6, pll1_pllen, 0),
	SYSCLK(7, pll1_sysclk7, pll1_pllen, 0),
	SYSCLK(8, pll1_sysclk8, pll1_pllen, 0),
	SYSCLK(9, pll1_sysclk9, pll1_pllen, 0),
	{ }
};

static const struct davinci_pll_clk_info dm365_pll2_info __initconst = {
	.name = "pll2",
	.pllm_mask = GENMASK(9, 0),
	.pllm_min = 1,
	.pllm_max = 1023,
	.flags = PLL_HAS_PREDIV | PLL_HAS_POSTDIV | PLL_PLLM_2X,
};

static const struct davinci_pll_sysclk_info dm365_pll2_sysclk_info[] __initconst = {
	SYSCLK(1, pll2_sysclk1, pll2_pllen, 0),
	SYSCLK(2, pll2_sysclk2, pll2_pllen, 0),
	SYSCLK(3, pll2_sysclk3, pll2_pllen, 0),
	SYSCLK(4, pll2_sysclk4, pll2_pllen, 0),
	SYSCLK(5, pll2_sysclk5, pll2_pllen, 0),
	{ }
};

/* both OBSCLKs are the same, so this info is shared */
static const char * const dm365_pll_obsclk_parent_names[] __initconst = {
	"ref_clk",
};

static u32 dm365_pll_obsclk_table[] = {
	0x10,
};

void __init dm365_pll_clk_init(void __iomem *pll1, void __iomem *pll2)
{
	const struct davinci_pll_sysclk_info *info;

	davinci_pll_clk_register(&dm365_pll1_info, "ref_clk", pll1);
	davinci_pll_auxclk_register("pll1_aux_clk", "ref_clk", pll1);
	davinci_pll_bpdiv_clk_register("pll1_sysclkbp", "ref_clk", pll1);
	davinci_pll_obsclk_register("clkout0", dm365_pll_obsclk_parent_names,
				    ARRAY_SIZE(dm365_pll_obsclk_parent_names),
				    pll1, dm365_pll_obsclk_table);
	for (info = dm365_pll1_sysclk_info; info->name; info++)
		davinci_pll_sysclk_register(info, pll1);

	davinci_pll_clk_register(&dm365_pll2_info, "ref_clk", pll2);
	davinci_pll_auxclk_register("clkout1", "ref_clk", pll2);
	davinci_pll_obsclk_register("clkout1", dm365_pll_obsclk_parent_names,
				    ARRAY_SIZE(dm365_pll_obsclk_parent_names),
				    pll2, dm365_pll_obsclk_table);
	for (info = dm365_pll2_sysclk_info; info->name; info++)
		davinci_pll_sysclk_register(info, pll2);
}
