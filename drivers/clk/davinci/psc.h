// SPDX-License-Identifier: GPL-2.0
/*
 * Clock driver for TI Davinci PSC controllers
 *
 * Copyright (C) 2017 David Lechner <david@lechnology.com>
 */

#ifndef __CLK_DAVINCI_PSC_H__
#define __CLK_DAVINCI_PSC_H__

#include <linux/types.h>

/* PSC quirk flags */
#define LPSC_ALWAYS_ENABLED	BIT(1) /* never disable this clock */
#define LPSC_FORCE		BIT(2) /* requires MDCTL FORCE bit */
#define LPSC_LOCAL_RESET	BIT(3) /* acts as reset provider */

struct clk_onecell_data;

struct davinci_psc_clk_info {
	const char *name;
	const char *parent;
	u32 lpsc;
	u32 pd;
	unsigned long flags;
	bool has_reset;
};

#define LPSC(l, d, n, p, f)	\
{				\
	.name	= #n,		\
	.parent	= #p,		\
	.lpsc	= (l),		\
	.pd	= (d),		\
	.flags	= (f),		\
}

struct clk_onecell_data *
davinci_psc_register_clocks(void __iomem *base,
			    const struct davinci_psc_clk_info *info,
			    u8 num_clks);

#ifdef CONFIG_OF
void of_davinci_psc_clk_init(struct device_node *node,
			     const struct davinci_psc_clk_info *info,
			     u8 num_clks);
#endif

#endif /* __CLK_DAVINCI_PSC_H__ */
