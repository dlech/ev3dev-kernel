// SPDX-License-Identifier: GPL-2.0
/*
 * Clock driver for TI Davinci PSC controllers
 *
 * Copyright (C) 2017 David Lechner <david@lechnology.com>
 */

#ifndef __CLK_DAVINCI_PLL_H___
#define __CLK_DAVINCI_PLL_H___

#include <linux/types.h>

struct clk;

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

#ifdef CONFIG_OF
struct device_node;

void of_davinci_pll_init(struct device_node *node, const char *name,
			 u8 num_sysclk);
#endif

#endif /* __CLK_DAVINCI_PLL_H___ */
