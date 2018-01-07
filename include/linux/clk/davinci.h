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

#endif /* __LINUX_CLK_DAVINCI_H__ */
