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

#define PLL_HAS_OSCIN			BIT(0) /* register OSCIN clock */
#define PLL_HAS_PREDIV			BIT(1) /* has prediv before PLL */
#define PLL_PREDIV_ALWAYS_ENABLED	BIT(2) /* don't clear DEN bit */
#define PLL_PREDIV_FIXED_DIV		BIT(3) /* fixed divider value */
#define PLL_HAS_POSTDIV			BIT(4) /* has postdiv after PLL */
#define PLL_POSTDIV_ALWAYS_ENABLED	BIT(5) /* don't clear DEN bit */
#define PLL_POSTDIV_FIXED_DIV		BIT(6) /* fixed divider value */
#define PLL_HAS_EXTCLKSRC		BIT(7) /* has selectable bypass */
#define PLL_PLLM_2X			BIT(8)

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
	u32 ratio_width;
	u32 flags;
};

#define SYSCLK(i, n, p, w, f)	\
{				\
	.name		= #n,	\
	.parent_name	= #p,	\
	.id		= (i),	\
	.ratio_width	= (w),	\
	.flags		= (f),	\
}

struct davinci_pll_obsclk_info {
	const char *name;
	const char * const * parent_names;
	u8 num_parents;
	u32 *table;
	u32 ocsrc_mask;
};

struct clk;

struct clk *davinci_pll_clk_register(const struct davinci_pll_clk_info *info,
				     const char *parent_name,
				     void __iomem *base);
struct clk *davinci_pll_auxclk_register(const char *name,
					void __iomem *base);
struct clk *davinci_pll_sysclkbp_clk_register(const char *name,
					      void __iomem *base);
struct clk *
davinci_pll_obsclk_register(const struct davinci_pll_obsclk_info *info,
			    void __iomem *base);
struct clk *
davinci_pll_sysclk_register(const struct davinci_pll_sysclk_info *info,
			    void __iomem *base);

#ifdef CONFIG_OF
struct device_node;

void of_davinci_pll_init(struct device_node *node,
			 const struct davinci_pll_clk_info *info,
			 const struct davinci_pll_obsclk_info *obsclk_info,
			 const struct davinci_pll_sysclk_info *div_info,
			 u8 max_sysclk_id);
#endif

#endif /* __CLK_DAVINCI_PLL_H___ */
