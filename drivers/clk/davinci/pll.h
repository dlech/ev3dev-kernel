// SPDX-License-Identifier: GPL-2.0
/*
 * Clock driver for TI Davinci PSC controllers
 *
 * Copyright (C) 2017 David Lechner <david@lechnology.com>
 */

#ifndef __CLK_DAVINCI_PLL_H___
#define __CLK_DAVINCI_PLL_H___

#include <linux/bitops.h>
#include <linux/types.h>

#define DIVCLK_ARM_RATE		BIT(0) /* Controls ARM rate */
#define DIVCLK_FIXED_DIV	BIT(1) /* Fixed divider */
#define DIVCLK_ALWAYS_ENABLED	BIT(2) /* Or bad things happen */

struct davinci_pll_divclk_info {
	const char *name;
	const char *parent_name;
	u32 id;
	u32 flags;
};

#define DIVCLK(i, n, p, f)	\
{				\
	.name		= #n,	\
	.parent_name	= #p,	\
	.id		= (i),	\
	.flags		= (f),	\
}

struct clk;

struct clk *davinci_pll_clk_register(const char *name,
				     const char *parent_name,
				     void __iomem *base,
				     bool is_da850);
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
struct clk *
davinci_pll_divclk_register(const struct davinci_pll_divclk_info *info,
			    void __iomem *base);

#ifdef CONFIG_OF
struct device_node;

void of_davinci_pll_init(struct device_node *node, const char *name,
			 const struct davinci_pll_divclk_info *info,
			 u8 max_divclk_id);
#endif

#endif /* __CLK_DAVINCI_PLL_H___ */
