/*
 * PSC clock driver for TI DaVinci based devices
 *
 * Copyright (C) 2016 David Lechner <david@lechnology.com>
 *
 * Based on clock driver for Keystone 2 based devices
 * Copyright (C) 2013 Texas Instruments.
 *	Murali Karicheri <m-karicheri2@ti.com>
 *	Santosh Shilimkar <santosh.shilimkar@ti.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/clk-provider.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/of_address.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/module.h>

/* PSC register offsets */
#define PTCMD			0x120
#define PTSTAT			0x128
#define PDSTAT			0x200
#define PDCTL			0x300
#define MDSTAT			0x800
#define MDCTL			0xa00

/* PSC module states */
#define PSC_STATE_SWRSTDISABLE	0
#define PSC_STATE_SYNCRST	1
#define PSC_STATE_DISABLE	2
#define PSC_STATE_ENABLE	3

#define MDSTAT_STATE_MASK	0x3f
#define MDSTAT_MCKOUT		BIT(12)
#define PDSTAT_STATE_MASK	0x1f
#define MDCTL_FORCE		BIT(31)
#define MDCTL_LRESET		BIT(8)
#define PDCTL_NEXT		BIT(0)

/* Maximum timeout to bail out state transition for module */
#define STATE_TRANS_MAX_COUNT	0xffff

#define LPSC_MAX_COUNT 32

struct clk_lpsc;

/**
 * struct clk_psc_data - PSC data
 * @cells: Contanins clock data structs for each cell
 * @clks: Array of clk pointers
 * @base: Base address for a PSC control
 */
struct clk_psc_data {
	struct clk_onecell_data cells;
	struct clk *clks[LPSC_MAX_COUNT];
	void __iomem *base;
};

/**
 * struct clk_lpsc - LPSC clock structure
 * @hw: clk_hw for the LPSC
 * @psc_data: PSC driver specific data
 * @lock: Spinlock used by the driver
 * @number: PSC module domain id number
 * @power_domain: PSC power domain id number
 */
struct clk_lpsc {
	struct clk_hw hw;
	struct clk_psc_data *psc_data;
	spinlock_t *lock;
	u32 number;
	u32 power_domain;
};

static DEFINE_SPINLOCK(psc_lock);

#define to_clk_lpsc(_hw) container_of((_hw), struct clk_lpsc, hw)

static void psc_config(void __iomem *base, u32 next_state, u32 md, u32 pd)
{
	u32 ptcmd, pdstat, pdctl, mdstat, mdctl, ptstat;
	u32 count = STATE_TRANS_MAX_COUNT;

	mdctl = readl(base + MDCTL + 4 * md);
	mdctl &= ~MDSTAT_STATE_MASK;
	mdctl |= next_state;
	/* For disable, we always put the module in local reset */
	if (next_state == PSC_STATE_DISABLE)
		mdctl &= ~MDCTL_LRESET;
	writel(mdctl, base + MDCTL + 4 * md);

	pdstat = readl(base + PDSTAT + 4 * pd);
	if (!(pdstat & PDSTAT_STATE_MASK)) {
		pdctl = readl(base + PDCTL + 4 * pd);
		pdctl |= PDCTL_NEXT;
		writel(pdctl, base + PDCTL + 4 * pd);
	}

	ptcmd = 1 << pd;
	writel(ptcmd, base + PTCMD);
	do {
		ptstat = readl(base + PTSTAT);
	} while (((ptstat >> pd) & 1) && count--);

	count = STATE_TRANS_MAX_COUNT;
	do {
		mdstat = readl(base + MDSTAT + 4 * md);
	} while (!((mdstat & MDSTAT_STATE_MASK) == next_state) && count--);
}

static int davinci_psc_clk_enable(struct clk_hw *hw)
{
	struct clk_lpsc *lpsc = to_clk_lpsc(hw);
	struct clk_psc_data *data = lpsc->psc_data;
	unsigned long flags = 0;
printk("%s: %s\n", __func__, __clk_get_name(hw->clk));
	if (lpsc->lock)
		spin_lock_irqsave(lpsc->lock, flags);

	psc_config(data->base, PSC_STATE_ENABLE, lpsc->number,
		   lpsc->power_domain);

	if (lpsc->lock)
		spin_unlock_irqrestore(lpsc->lock, flags);

	return 0;
}

static void davinci_psc_clk_disable(struct clk_hw *hw)
{
	struct clk_lpsc *lpsc = to_clk_lpsc(hw);
	struct clk_psc_data *data = lpsc->psc_data;
	unsigned long flags = 0;
printk("%s: %s\n", __func__, __clk_get_name(hw->clk));
	if (lpsc->lock)
		spin_lock_irqsave(lpsc->lock, flags);

	psc_config(data->base, PSC_STATE_DISABLE, lpsc->number,
		   lpsc->power_domain);

	if (lpsc->lock)
		spin_unlock_irqrestore(lpsc->lock, flags);
}

static int davinci_psc_clk_is_enabled(struct clk_hw *hw)
{
	struct clk_lpsc *lpsc = to_clk_lpsc(hw);
	struct clk_psc_data *data = lpsc->psc_data;
	u32 mdstat = readl(data->base + MDSTAT + 4 * lpsc->number);

	return (mdstat & MDSTAT_MCKOUT) ? 1 : 0;
}

static const struct clk_ops davinci_psc_clk_psc_ops = {
	.enable = davinci_psc_clk_enable,
	.disable = davinci_psc_clk_disable,
	.is_enabled = davinci_psc_clk_is_enabled,
};

/**
 * davinci_psc_clk_register - register lpsc clock
 * @name: name of this clock
 * @number: module id number
 * @power_domain: power domain id number
 * @parent_name: name of clock's parent
 * @psc_data: platform data to configure this clock
 * @lock: spinlock used by this clock
 */
static struct clk *davinci_psc_clk_register(const char *name,
					    u32 number,
					    u32 power_domain,
					    const char *parent_name,
					    struct clk_psc_data *psc_data,
					    spinlock_t *lock)
{
	struct clk_init_data init;
	struct clk_lpsc *lpsc;
	struct clk *clk;

	lpsc = kzalloc(sizeof(*lpsc), GFP_KERNEL);
	if (!lpsc)
		return ERR_PTR(-ENOMEM);

	init.name = name;
	init.ops = &davinci_psc_clk_psc_ops;
	init.flags = 0;
	init.parent_names = (parent_name ? &parent_name : NULL);
	init.num_parents = (parent_name ? 1 : 0);

	lpsc->hw.init = &init;
	lpsc->psc_data = psc_data;
	lpsc->lock = lock;
	lpsc->number = number;
	lpsc->power_domain = power_domain;

	clk = clk_register(NULL, &lpsc->hw);
	if (IS_ERR(clk))
		kfree(lpsc);

	return clk;
}

/**
 * of_davinci_psc_clk_init - initialize PSC through DT
 * @node: device tree node for this clock
 */
static void __init of_davinci_psc_clk_init(struct device_node *node)
{
	struct clk_psc_data *data;
	int count, i;

	data = kzalloc(sizeof(*data), GFP_KERNEL);
	if (!data) {
		pr_err("%s: Out of memory\n", __func__);
		return;
	}

	data->cells.clks = data->clks;
	data->cells.clk_num = LPSC_MAX_COUNT;

	count = of_property_count_u32_elems(node, "clock-indices");
	if (count <= 0) {
		pr_err("%s: Missing clock-indices\n", __func__);
		goto err;
	}
	if (count >= LPSC_MAX_COUNT) {
		pr_err("%s: Too many clock-indices\n", __func__);
		goto err;
	}

	data->base = of_iomap(node, 0);
	if (!data->base) {
		pr_err("%s: ioremap failed\n", __func__);
		goto err;
	}

	for (i = 0; i < count; i++) {
		struct clk *clk;
		const char *name;
		const char *parent;
		u32 number, power_domain;

		of_property_read_string_index(node, "clock-output-names", i,
					      &name);
		of_property_read_u32_index(node, "clock-indices", i, &number);
		of_property_read_u32_index(node, "ti,power-domain", i,
					   &power_domain);
		parent = of_clk_get_parent_name(node, i);
		if (!parent) {
			pr_err("%s: Parent clock not found for '%s'\n",
			       __func__, name);
			continue;
		}
printk("clk: %s, lspc: %u, pd: %u, parent: %s\n", name, number, power_domain, parent);
		clk = davinci_psc_clk_register(name, number, power_domain,
					       parent, data, &psc_lock);
		if (IS_ERR(clk)) {
			pr_err("%s: Failed to register clock '%s'\n", __func__,
			       name);
			continue;
		}

		data->cells.clks[number] = clk;
	}

	of_clk_add_provider(node, of_clk_src_onecell_get, &data->cells);

	return;

err:
	kfree(data);
}
CLK_OF_DECLARE(davinci_psc_clk, "ti,davinci-psc", of_davinci_psc_clk_init);
#if 0
static int davinci_psc_clk_probe(struct platform_device *pdev)
{
	struct davinci_psc_clk_platform_data *pdata = pdev->dev.platform_data;
	struct clk_psc_data *data;
	struct resource *r;
	int i;

	if (!pdata)
		return -ENODEV;

	data = devm_kzalloc(&pdev->dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	r = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	data->base = devm_ioremap_resource(&pdev->dev, r);
	if (IS_ERR(data->base))
		return PTR_ERR(data->base);

	for (i = 0; i < pdata->num_clk; i++) {
		struct davinci_psc_clk_info *info = &pdata->clk_info[i];
		struct clk *clk;
		struct clk_lookup *lookup;

		clk = davinci_psc_clk_register(info->name, info->number,
					       info->power_domain, info->parent,
					       data, &psc_lock);
		if (IS_ERR(clk)) {
			dev_err(&pdev->dev, "failed to register clock '%s'\n",
				info->name);
			continue;
		}

		lookup = clkdev_create(clk, info->con_id, "%s", info->dev_id);
		if (IS_ERR(lookup)) {
			dev_err(&pdev->dev, "failed to create lookup for '%s'\n",
				info->name);
			continue;
		}

		clkdev_add(lookup);
	}
}

static struct platform_driver davinci_psc_clk_driver = {
	.driver = {
		.name = "davinci-psc",
	},
	.probe = davinci_psc_clk_probe,
};
module_platform_driver(davinci_psc_clk_driver);

MODULE_AUTHOR("David Lechner <david@lechnology.com>");
MODULE_DESCRIPTION("TI DaVinci PSC clocks");
MODULE_LICENSE("GPL v2");
#endif