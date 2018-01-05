// SPDX-License-Identifier: GPL-2.0
/*
 * TI Davinci clocks
 *
 * Copyright (C) 2017 David Lechner <david@lechnology.com>
 */
#ifndef __LINUX_CLK_DAVINCI_H__
#define __LINUX_CLK_DAVINCI_H__

#include <linux/clk-provider.h>
#include <linux/types.h>

void da830_psc_clk_init(void __iomem *psc0, void __iomem *psc1);
void da850_psc_clk_init(void __iomem *psc0, void __iomem *psc1);
void dm355_psc_clk_init(void __iomem *psc);
void dm365_psc_clk_init(void __iomem *psc);
void dm644x_psc_clk_init(void __iomem *psc);
void dm646x_psc_clk_init(void __iomem *psc);

struct regmap;

struct clk *davinci_pll_clk_register(const char *name,
				     const char *parent_name,
				     void __iomem *base);
struct clk *davinci_pll_aux_clk_register(const char *name,
					 const char *parent_name,
					 void __iomem *base);
struct clk *davinci_pll_bpdiv_clk_register(const char *name,
					   const char *parent_name,
					   void __iomem *base);
struct clk *davinci_pll_obs_clk_register(const char *name,
					 const char * const *parent_names,
					 u8 num_parents,
					 void __iomem *base,
					 u32 *table);
struct clk *davinci_pll_div_clk_register(const char *name,
					 const char *parent_name,
					 void __iomem *base,
					 u32 id);
struct clk* da8xx_usb0_phy_clk_register(const char *name,
					const char *parent0,
					const char *parent1,
					struct clk *usb0_psc_clk,
					struct regmap *regmap);
struct clk* da8xx_usb1_phy_clk_register(const char *name,
					const char *parent0,
					const char *parent1,
					struct regmap *regmap);

/* convience macros for board declaration files */
#define PLL_CLK davinci_pll_clk_register
#define PLL_DIV_CLK davinci_pll_div_clk_register
#define PLL_AUX_CLK davinci_pll_aux_clk_register
#define PLL_BP_CLK davinci_pll_bpdiv_clk_register
#define PLL_OBS_CLK davinci_pll_obs_clk_register

#endif /* __LINUX_CLK_DAVINCI_H__ */
