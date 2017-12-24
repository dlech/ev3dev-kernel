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

struct clk *davinci_psc_clk_register(const char *name,
				     const char *parent_name,
				     void __iomem *base,
				     u32 lpsc, u32 pd);

#endif /* __LINUX_CLK_DAVINCI_H__ */
