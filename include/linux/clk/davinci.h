// SPDX-License-Identifier: GPL-2.0
/*
 * TI Davinci clocks
 *
 * Copyright (C) 2017 David Lechner <david@lechnology.com>
 */
#ifndef __LINUX_CLK_DAVINCI_H__
#define __LINUX_CLK_DAVINCI_H__

#include <linux/types.h>

void da830_pll_clk_init(void __iomem *pll);
void da850_pll_clk_init(void __iomem *pll0, void __iomem *pll1);
void dm355_pll_clk_init(void __iomem *pll1, void __iomem *pll2);
void dm365_pll_clk_init(void __iomem *pll1, void __iomem *pll2);
void dm644x_pll_clk_init(void __iomem *pll1, void __iomem *pll2);
void dm646x_pll_clk_init(void __iomem *pll1, void __iomem *pll2);

void da830_psc_clk_init(void __iomem *psc0, void __iomem *psc1);
void da850_psc_clk_init(void __iomem *psc0, void __iomem *psc1);

#endif /* __LINUX_CLK_DAVINCI_H__ */
