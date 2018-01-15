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

#define PLL_HAS_PREDIV			BIT(0) /* has prediv before PLL */
#define PLL_PREDIV_ALWAYS_ENABLED	BIT(1) /* don't disable */
#define PLL_PREDIV_FIXED_DIV		BIT(2) /* fixed divider value */
#define PLL_HAS_POSTDIV			BIT(3) /* has postdiv after PLL */
#define PLL_POSTDIV_ALWAYS_ENABLED	BIT(4) /* don't disable */
#define PLL_POSTDIV_FIXED_DIV		BIT(5) /* fixed divider value */
#define PLL_HAS_EXTCLKSRC		BIT(6)
#define PLL_PLLM_2X			BIT(7)

/** davinci_pll_clk_info - controller-specific PLL info
 * @name: The name of the PLL
 * @pllm_mask: Bitmask for PLLM[PLLM] value
 * @pllm_min: Minimum allowable value for PLLM[PLLM]
 * @pllm_max: Maximum allowable value for PLLM[PLLM]
 * @pllout_min_rate: Minimum allowable rate for PLLOUT
 * @pllout_max_rate: Maximum allowable rate for PLLOUT
 * @flags: Bitmap of PLL_* flags.
 */
struct davinci_pll_clk_info {
	const char *name;
	u32 pllm_mask;
	u32 pllm_min;
	u32 pllm_max;
	unsigned long pllout_min_rate;
	unsigned long pllout_max_rate;
	u32 flags;
};

#define SYSCLK_ARM_RATE		BIT(0) /* Controls ARM rate */
#define SYSCLK_FIXED_DIV	BIT(1) /* Fixed divider */
#define SYSCLK_ALWAYS_ENABLED	BIT(2) /* Or bad things happen */

struct davinci_pll_sysclk_info {
	const char *name;
	const char *parent_name;
	u32 id;
	u32 flags;
};

#define SYSCLK(i, n, p, f)	\
{				\
	.name		= #n,	\
	.parent_name	= #p,	\
	.id		= (i),	\
	.flags		= (f),	\
}

struct clk;

struct clk *davinci_pll_clk_register(const struct davinci_pll_clk_info *info,
				     const char *parent_name,
				     void __iomem *base);
struct clk *davinci_pll_aux_clk_register(const char *name,
					 const char *parent_name,
					 void __iomem *base);
struct clk *davinci_pll_bpdiv_clk_register(const char *name,
					   const char *parent_name,
					   void __iomem *base);
struct clk *davinci_pll_obsclk_register(const char *name,
					const char * const *parent_names,
					u8 num_parents,
					void __iomem *base,
					u32 *table);
struct clk *
davinci_pll_sysclk_register(const struct davinci_pll_sysclk_info *info,
			    void __iomem *base);

#ifdef CONFIG_OF
struct device_node;

void of_davinci_pll_init(struct device_node *node,
			 const struct davinci_pll_clk_info *info,
			 const struct davinci_pll_sysclk_info *div_info,
			 u8 max_sysclk_id);
#endif

#endif /* __CLK_DAVINCI_PLL_H___ */
