// SPDX-License-Identifier: GPL-2.0
/*
 * TI Davinci clocks
 *
 * Copyright (C) 2017 David Lechner <david@lechnology.com>
 */
#ifndef __LINUX_CLK_DAVINCI_H__
#define __LINUX_CLK_DAVINCI_H__

#include <linux/types.h>

struct clk;
struct regmap;

void da830_pll_clk_init(void __iomem *pll);
void da850_pll_clk_init(void __iomem *pll0, void __iomem *pll1);
void dm355_pll_clk_init(void __iomem *pll1, void __iomem *pll2);
void dm365_pll_clk_init(void __iomem *pll1, void __iomem *pll2);
void dm644x_pll_clk_init(void __iomem *pll1, void __iomem *pll2);
void dm646x_pll_clk_init(void __iomem *pll1, void __iomem *pll2);

void da830_psc_clk_init(void __iomem *psc0, void __iomem *psc1);
void da850_psc_clk_init(void __iomem *psc0, void __iomem *psc1);
void dm355_psc_clk_init(void __iomem *psc);
void dm365_psc_clk_init(void __iomem *psc);
void dm644x_psc_clk_init(void __iomem *psc);
void dm646x_psc_clk_init(void __iomem *psc);

struct clk *da8xx_cfgchip_register_tbclk(struct regmap *regmap);
struct clk *da8xx_cfgchip_register_div4p5(struct regmap *regmap);
struct clk *da8xx_cfgchip_register_async1(struct regmap *regmap);
struct clk *da8xx_cfgchip_register_async3(struct regmap *regmap);
struct clk *da8xx_cfgchip_register_usb0_clk48(struct regmap *regmap,
					      struct clk *usb0_psc_clk);
struct clk *da8xx_cfgchip_register_usb1_clk48(struct regmap *regmap);

#endif /* __LINUX_CLK_DAVINCI_H__ */
