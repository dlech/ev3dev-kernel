// SPDX-License-Identifier: GPL-2.0
/*
 * PLL clock descriptions for TI DM355
 *
 * Copyright (C) 2018 David Lechner <david@lechnology.com>
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
	.flags = PLL_HAS_CLKMODE | PLL_HAS_PREDIV | PLL_PREDIV_ALWAYS_ENABLED |
		 PLL_PREDIV_FIXED8 | PLL_HAS_POSTDIV |
		 PLL_POSTDIV_ALWAYS_ENABLED | PLL_POSTDIV_FIXED_DIV,
};

static const struct davinci_pll_sysclk_info dm355_pll1_sysclk_info[] __initconst = {
	SYSCLK(1, pll1_sysclk1, pll1, 5, SYSCLK_FIXED_DIV | SYSCLK_ALWAYS_ENABLED),
	SYSCLK(2, pll1_sysclk2, pll1, 5, SYSCLK_FIXED_DIV | SYSCLK_ALWAYS_ENABLED),
	SYSCLK(3, pll1_sysclk3, pll1, 5, SYSCLK_ALWAYS_ENABLED),
	SYSCLK(4, pll1_sysclk4, pll1, 5, SYSCLK_ALWAYS_ENABLED),
	{ }
};

static const struct davinci_pll_clk_info dm355_pll2_info __initconst = {
	.name = "pll2",
	.pllm_mask = GENMASK(7, 0),
	.pllm_min = 92,
	.pllm_max = 184,
	.flags = PLL_HAS_PREDIV | PLL_PREDIV_ALWAYS_ENABLED | PLL_HAS_POSTDIV |
		 PLL_POSTDIV_ALWAYS_ENABLED | PLL_POSTDIV_FIXED_DIV,
};

static const struct davinci_pll_sysclk_info dm355_pll2_sysclk_info[] __initconst = {
	SYSCLK(1, pll2_sysclk1, pll2, 5, SYSCLK_FIXED_DIV),
	SYSCLK(2, pll2_sysclk2, pll2, 5, SYSCLK_FIXED_DIV | SYSCLK_ALWAYS_ENABLED),
	{ }
};

int __init dm355_pll1_init(struct device *dev, void __iomem *base)
{
	const struct davinci_pll_sysclk_info *info;

	davinci_pll_clk_register(dev, &dm355_pll1_info, "ref_clk", base);

	for (info = dm355_pll1_sysclk_info; info->name; info++)
		davinci_pll_sysclk_register(dev, info, base);

	davinci_pll_auxclk_register(dev, "pll1_auxclk", base);

	davinci_pll_sysclkbp_clk_register(dev, "pll1_sysclkbp", base);

	return 0;
}

int __init dm355_pll2_init(struct device *dev, void __iomem *base)
{
	const struct davinci_pll_sysclk_info *info;

	davinci_pll_clk_register(dev, &dm355_pll2_info, "oscin", base);

	for (info = dm355_pll2_sysclk_info; info->name; info++)
		davinci_pll_sysclk_register(dev, info, base);

	davinci_pll_sysclkbp_clk_register(dev, "pll2_sysclkbp", base);

	return 0;
}
