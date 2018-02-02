// SPDX-License-Identifier: GPL-2.0
/*
 * PLL clock descriptions for TI DM365
 *
 * Copyright (C) 2018 David Lechner <david@lechnology.com>
 */

#include <linux/bitops.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/types.h>

#include "pll.h"

#define OCSEL_OCSRC_ENABLE	0

static const struct davinci_pll_clk_info dm365_pll1_info __initconst = {
	.name = "pll1",
	.pllm_mask = GENMASK(9, 0),
	.pllm_min = 1,
	.pllm_max = 1023,
	.flags = PLL_HAS_CLKMODE | PLL_HAS_PREDIV | PLL_HAS_POSTDIV |
		 PLL_POSTDIV_ALWAYS_ENABLED | PLL_PLLM_2X,
};

static const struct davinci_pll_sysclk_info dm365_pll1_sysclk_info[] __initconst = {
	SYSCLK(1, pll1_sysclk1, pll1_pllen, 5, SYSCLK_ALWAYS_ENABLED),
	SYSCLK(2, pll1_sysclk2, pll1_pllen, 5, SYSCLK_ALWAYS_ENABLED),
	SYSCLK(3, pll1_sysclk3, pll1_pllen, 5, SYSCLK_ALWAYS_ENABLED),
	SYSCLK(4, pll1_sysclk4, pll1_pllen, 5, SYSCLK_ALWAYS_ENABLED),
	SYSCLK(5, pll1_sysclk5, pll1_pllen, 5, SYSCLK_ALWAYS_ENABLED),
	SYSCLK(6, pll1_sysclk6, pll1_pllen, 5, SYSCLK_ALWAYS_ENABLED),
	SYSCLK(7, pll1_sysclk7, pll1_pllen, 5, SYSCLK_ALWAYS_ENABLED),
	SYSCLK(8, pll1_sysclk8, pll1_pllen, 5, SYSCLK_ALWAYS_ENABLED),
	SYSCLK(9, pll1_sysclk9, pll1_pllen, 5, SYSCLK_ALWAYS_ENABLED),
	{ }
};

/*
 * This is a bit of a hack to make OCSEL[OCSRC] on DM365 look like OCSEL[OCSRC]
 * on DA850. On DM365, OCSEL[OCSRC] is just an enable/disable bit instead of a
 * multiplexer. By modeling it as a single parent mux clock, the clock code will
 * still do the right thing in this case.
 */
static const char * const dm365_pll_obsclk_parent_names[] __initconst = {
	"oscin",
};

static u32 dm365_pll_obsclk_table[] = {
	OCSEL_OCSRC_ENABLE,
};

static const struct davinci_pll_obsclk_info dm365_pll1_obsclk_info __initconst = {
	.name = "pll1_obsclk",
	.parent_names = dm365_pll_obsclk_parent_names,
	.num_parents = ARRAY_SIZE(dm365_pll_obsclk_parent_names),
	.table = dm365_pll_obsclk_table,
	.ocsrc_mask = BIT(4),
};

static const struct davinci_pll_clk_info dm365_pll2_info __initconst = {
	.name = "pll2",
	.pllm_mask = GENMASK(9, 0),
	.pllm_min = 1,
	.pllm_max = 1023,
	.flags = PLL_HAS_PREDIV | PLL_HAS_POSTDIV | PLL_POSTDIV_ALWAYS_ENABLED |
		 PLL_PLLM_2X,
};

static const struct davinci_pll_sysclk_info dm365_pll2_sysclk_info[] __initconst = {
	SYSCLK(1, pll2_sysclk1, pll2_pllen, 5, SYSCLK_ALWAYS_ENABLED),
	SYSCLK(2, pll2_sysclk2, pll2_pllen, 5, SYSCLK_ALWAYS_ENABLED),
	SYSCLK(3, pll2_sysclk3, pll2_pllen, 5, SYSCLK_ALWAYS_ENABLED),
	SYSCLK(4, pll2_sysclk4, pll2_pllen, 5, SYSCLK_ALWAYS_ENABLED),
	SYSCLK(5, pll2_sysclk5, pll2_pllen, 5, SYSCLK_ALWAYS_ENABLED),
	{ }
};

static const struct davinci_pll_obsclk_info dm365_pll2_obsclk_info __initconst = {
	.name = "pll2_obsclk",
	.parent_names = dm365_pll_obsclk_parent_names,
	.num_parents = ARRAY_SIZE(dm365_pll_obsclk_parent_names),
	.table = dm365_pll_obsclk_table,
	.ocsrc_mask = BIT(4),
};

void __init dm365_pll_clk_init(void __iomem *pll1, void __iomem *pll2)
{
	const struct davinci_pll_sysclk_info *info;

	davinci_pll_clk_register(&dm365_pll1_info, "ref_clk", pll1);

	davinci_pll_auxclk_register("pll1_auxclk", pll1);

	davinci_pll_sysclkbp_clk_register("pll1_sysclkbp", pll1);

	davinci_pll_obsclk_register(&dm365_pll1_obsclk_info, pll1);

	for (info = dm365_pll1_sysclk_info; info->name; info++)
		davinci_pll_sysclk_register(info, pll1);

	davinci_pll_clk_register(&dm365_pll2_info, "oscin", pll2);

	davinci_pll_auxclk_register("pll2_auxclk", pll2);

	davinci_pll_obsclk_register(&dm365_pll2_obsclk_info, pll2);

	for (info = dm365_pll2_sysclk_info; info->name; info++)
		davinci_pll_sysclk_register(info, pll2);
}
