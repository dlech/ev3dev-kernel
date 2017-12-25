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
struct clk *davinci_psc_clk_register(const char *name,
				     const char *parent_name,
				     void __iomem *base,
				     u32 lpsc, u32 pd);

/* convience macros for board declaration files */
#define EXT_CLK(n, r) clk_register_fixed_rate(NULL, (n), NULL, 0, (r))
#define FIX_CLK(n, p) clk_register_fixed_factor(NULL, (n), (p), 0, 1, 1)
#define PLL_CLK davinci_pll_clk_register
#define PLL_DIV_CLK davinci_pll_div_clk_register
#define PLL_AUX_CLK davinci_pll_aux_clk_register
#define PLL_BP_CLK davinci_pll_bpdiv_clk_register
#define PLL_OBS_CLK davinci_pll_obs_clk_register
#define PSC_CLK davinci_psc_clk_register

#endif /* __LINUX_CLK_DAVINCI_H__ */
