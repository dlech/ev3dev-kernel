// SPDX-License-Identifier: GPL-2.0
/*
 * PLL clock descriptions for TI DM646X
 *
 * Copyright (C) 2018 David Lechner <david@lechnology.com>
 */

#include <linux/clk-provider.h>
#include <linux/clkdev.h>
#include <linux/init.h>
#include <linux/types.h>

#include "pll.h"

static const struct davinci_pll_clk_info dm646x_pll1_info __initconst = {
	.name = "pll1",
	.pllm_mask = GENMASK(4, 0),
	.pllm_min = 14,
	.pllm_max = 32,
	.flags = PLL_HAS_CLKMODE,
};

static const struct davinci_pll_sysclk_info dm646x_pll1_sysclk_info[] __initconst = {
	SYSCLK(1, pll1_sysclk1, pll1_pllen, 4, SYSCLK_FIXED_DIV),
	SYSCLK(2, pll1_sysclk2, pll1_pllen, 4, SYSCLK_FIXED_DIV),
	SYSCLK(3, pll1_sysclk3, pll1_pllen, 4, SYSCLK_FIXED_DIV),
	SYSCLK(4, pll1_sysclk4, pll1_pllen, 4, 0),
	SYSCLK(5, pll1_sysclk5, pll1_pllen, 4, 0),
	SYSCLK(6, pll1_sysclk6, pll1_pllen, 4, 0),
	SYSCLK(8, pll1_sysclk8, pll1_pllen, 4, 0),
	SYSCLK(9, pll1_sysclk9, pll1_pllen, 4, 0),
	{ }
};

static const struct davinci_pll_clk_info dm646x_pll2_info __initconst = {
	.name = "pll2",
	.pllm_mask = GENMASK(4, 0),
	.pllm_min = 14,
	.pllm_max = 32,
	.flags = 0,
};

static const struct davinci_pll_sysclk_info dm646x_pll2_sysclk_info[] __initconst = {
	SYSCLK(1, pll2_sysclk1, pll2_pllen, 4, 0),
	{ }
};

int __init dm646x_pll1_init(struct device *dev, void __iomem *base)
{
	const struct davinci_pll_sysclk_info *info;

	davinci_pll_clk_register(dev, &dm646x_pll1_info, "ref_clk", base);

	for (info = dm646x_pll1_sysclk_info; info->name; info++)
		davinci_pll_sysclk_register(dev, info, base);

	davinci_pll_sysclkbp_clk_register(dev, "pll1_sysclkbp", base);

	davinci_pll_auxclk_register(dev, "pll1_auxclk", base);

	return 0;
}

int __init dm646x_pll2_init(struct device *dev, void __iomem *base)
{
	const struct davinci_pll_sysclk_info *info;
	struct clk *clk;

	davinci_pll_clk_register(dev, &dm646x_pll2_info, "oscin", base);

	for (info = dm646x_pll2_sysclk_info; info->name; info++)
		davinci_pll_sysclk_register(dev, info, base);

	clk = clk_register_fixed_factor(dev, "timer2", "pll1_sysclk3", 0, 1, 1);
	clk_register_clkdev(clk, NULL, "davinci-wdt");

	return 0;
}
