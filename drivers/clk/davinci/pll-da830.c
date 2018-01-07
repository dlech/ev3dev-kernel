// SPDX-License-Identifier: GPL-2.0
/*
 * PLL clock descriptions for TI DA830/OMAP-L137/AM17XX
 *
 * Copyright (C) 2017 David Lechner <david@lechnology.com>
 */

#include <linux/init.h>
#include <linux/types.h>

#include "pll.h"

/*
 * NB: Technically, the clocks flagged as DIVCLK_FIXED_DIV are "fixed ratio",
 * meaning that we could change the divider as long as we keep the correct
 * ratio between all of the clocks, but we don't support that because there is
 * currently not a need for it.
 */

static const struct davinci_pll_divclk_info da830_pll_divclk_info[] __initconst = {
	DIVCLK(2, pll0_sysclk2, pll0, DIVCLK_FIXED_DIV),
	DIVCLK(3, pll0_sysclk3, pll0, 0),
	DIVCLK(4, pll0_sysclk4, pll0, DIVCLK_FIXED_DIV),
	DIVCLK(5, pll0_sysclk5, pll0, 0),
	DIVCLK(6, pll0_sysclk6, pll0, DIVCLK_FIXED_DIV),
	DIVCLK(7, pll0_sysclk7, pll0, 0),
	{ }
};

void __init da830_pll_clk_init(void __iomem *pll)
{
	const struct davinci_pll_divclk_info *info;

	davinci_pll_clk_register("pll0", "ref_clk", pll);
	davinci_pll_aux_clk_register("pll0_aux_clk", "ref_clk", pll);
	for (info = da830_pll_divclk_info; info->name; info++)
		davinci_pll_divclk_register(info, pll);
}
