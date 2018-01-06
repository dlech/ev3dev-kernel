// SPDX-License-Identifier: GPL-2.0
/*
 * PLL clock descriptions for TI DA850/OMAP-L138/AM18XX
 *
 * Copyright (C) 2017 David Lechner <david@lechnology.com>
 */

#include <linux/clk-provider.h>
#include <linux/clkdev.h>
#include <linux/init.h>
#include <linux/of.h>
#include <linux/types.h>

#include "pll.h"

/*
 * NB: Technically, the clocks flagged as DIVCLK_FIXED_DIV are "fixed ratio",
 * meaning that we could change the divider as long as we keep the correct
 * ratio between all of the clocks, but we don't support that because there is
 * currently not a need for it.
 */

static const struct davinci_pll_divclk_info
da850_pll0_divclk_info[] __initconst = {
	DIVCLK(1, pll0_sysclk1, pll0, DIVCLK_FIXED_DIV),
	DIVCLK(2, pll0_sysclk2, pll0, DIVCLK_FIXED_DIV),
	DIVCLK(3, pll0_sysclk3, pll0, 0),
	DIVCLK(4, pll0_sysclk4, pll0, DIVCLK_FIXED_DIV),
	DIVCLK(5, pll0_sysclk5, pll0, 0),
	DIVCLK(6, pll0_sysclk6, pll0, DIVCLK_ARM_RATE | DIVCLK_FIXED_DIV),
	DIVCLK(7, pll0_sysclk7, pll0, 0),
	{ }
};

static const struct davinci_pll_divclk_info
da850_pll1_divclk_info[] __initconst = {
	DIVCLK(1, pll1_sysclk1, pll1, DIVCLK_ALWAYS_ENABLED),
	DIVCLK(2, pll1_sysclk2, pll1, 0),
	DIVCLK(3, pll1_sysclk3, pll1, 0),
	{ }
};

void __init da850_pll_clk_init(void __iomem *pll0, void __iomem *pll1)
{
	const struct davinci_pll_divclk_info *info;

	davinci_pll_clk_register("pll0", "ref_clk", pll0);
	davinci_pll_aux_clk_register("pll0_aux_clk", "ref_clk", pll0);
	for (info = da850_pll0_divclk_info; info->name; info++)
		davinci_pll_divclk_register(info, pll0);

	davinci_pll_clk_register("pll1", "ref_clk", pll1);
	for (info = da850_pll1_divclk_info; info->name; info++)
		davinci_pll_divclk_register(info, pll1);
}

#ifdef CONFIG_OF
static void __init of_da850_pll0_auxclk_init(struct device_node *node)
{
	of_davinci_pll_init(node, "pll0", da850_pll0_divclk_info, 7);
}
CLK_OF_DECLARE(da850_pll0_auxclk, "ti,da850-pll0", of_da850_pll0_auxclk_init);

static void __init of_da850_pll1_auxclk_init(struct device_node *node)
{
	of_davinci_pll_init(node, "pll1", da850_pll1_divclk_info, 3);
}
CLK_OF_DECLARE(da850_pll1_auxclk, "ti,da850-pll1", of_da850_pll1_auxclk_init);
#endif
