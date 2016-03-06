/*
 * Clock driver for Keystone 2 based devices
 *
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

/**
 * struct clk_psc_data - PSC data
 * @base: Base address for a PSC control
 * @power_domain: PSC power domain id number
 * @module_domain: PSC module (clock) domain id number (LPSC)
 */
struct clk_psc_data {
	void __iomem *base;
	u32 power_domain;
	u32 module_domain;
};

/**
 * struct clk_psc - PSC clock structure
 * @hw: clk_hw for the psc
 * @psc_data: PSC driver specific data
 * @lock: Spinlock used by the driver
 */
struct clk_psc {
	struct clk_hw hw;
	struct clk_psc_data *psc_data;
	spinlock_t *lock;
};

static DEFINE_SPINLOCK(psc_lock);

#define to_clk_psc(_hw) container_of(_hw, struct clk_psc, hw)

static void psc_config(void __iomem *base, u32 next_state, u32 pd, u32 md)
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

static int keystone_clk_is_enabled(struct clk_hw *hw)
{
	struct clk_psc *psc = to_clk_psc(hw);
	struct clk_psc_data *data = psc->psc_data;
	u32 mdstat = readl(data->base + MDSTAT + 4 * data->module_domain);

	return (mdstat & MDSTAT_MCKOUT) ? 1 : 0;
}

static int keystone_clk_enable(struct clk_hw *hw)
{
	struct clk_psc *psc = to_clk_psc(hw);
	struct clk_psc_data *data = psc->psc_data;
	unsigned long flags = 0;

	if (psc->lock)
		spin_lock_irqsave(psc->lock, flags);

	psc_config(data->base, PSC_STATE_ENABLE, data->power_domain,
							data->module_domain);

	if (psc->lock)
		spin_unlock_irqrestore(psc->lock, flags);

	return 0;
}

static void keystone_clk_disable(struct clk_hw *hw)
{
	struct clk_psc *psc = to_clk_psc(hw);
	struct clk_psc_data *data = psc->psc_data;
	unsigned long flags = 0;

	if (psc->lock)
		spin_lock_irqsave(psc->lock, flags);

	psc_config(data->base, PSC_STATE_DISABLE, data->power_domain,
							data->module_domain);

	if (psc->lock)
		spin_unlock_irqrestore(psc->lock, flags);
}

static const struct clk_ops clk_psc_ops = {
	.enable = keystone_clk_enable,
	.disable = keystone_clk_disable,
	.is_enabled = keystone_clk_is_enabled,
};

/**
 * clk_register_psc - register psc clock
 * @dev: device that is registering this clock
 * @name: name of this clock
 * @parent_name: name of clock's parent
 * @psc_data: platform data to configure this clock
 * @lock: spinlock used by this clock
 */
static struct clk *clk_register_psc(struct device *dev,
			const char *name,
			const char *parent_name,
			struct clk_psc_data *psc_data,
			spinlock_t *lock)
{
	struct clk_init_data init;
	struct clk_psc *psc;
	struct clk *clk;

	psc = kzalloc(sizeof(*psc), GFP_KERNEL);
	if (!psc)
		return ERR_PTR(-ENOMEM);

	init.name = name;
	init.ops = &clk_psc_ops;
	init.flags = 0;
	init.parent_names = (parent_name ? &parent_name : NULL);
	init.num_parents = (parent_name ? 1 : 0);

	psc->psc_data = psc_data;
	psc->lock = lock;
	psc->hw.init = &init;

	clk = clk_register(NULL, &psc->hw);
	if (IS_ERR(clk))
		kfree(psc);

	return clk;
}

/**
 * of_psc_clk_init - initialize psc clock through DT
 * @node: device tree node for this clock
 * @lock: spinlock used by this clock
 */
static void __init of_psc_clk_init(struct device_node *node, spinlock_t *lock)
{
	const char *clk_name = node->name;
	const char *parent_name;
	struct clk_psc_data *data;
	struct clk *clk;

	data = kzalloc(sizeof(*data), GFP_KERNEL);
	if (!data) {
		pr_err("%s: Out of memory\n", __func__);
		return;
	}

	data->base = of_iomap(node, 0);
	if (!data->base) {
		pr_err("%s: ioremap failed\n", __func__);
		goto out;
	}

	of_property_read_u32(node, "power-domain", &data->power_domain);
	of_property_read_u32(node, "module-domain", &data->module_domain);
	of_property_read_string(node, "clock-output-names", &clk_name);
	parent_name = of_clk_get_parent_name(node, 0);
	if (!parent_name) {
		pr_err("%s: Parent clock not found\n", __func__);
		goto unmap_base;
	}

	clk = clk_register_psc(NULL, clk_name, parent_name, data, lock);
	if (!IS_ERR(clk)) {
		of_clk_add_provider(node, of_clk_src_simple_get, clk);
		return;
	}

	pr_err("%s: error registering clk %s\n", __func__, node->name);

unmap_base:
	iounmap(data->base);
out:
	kfree(data);
	return;
}

/**
 * of_keystone_psc_clk_init - initialize psc clock through DT
 * @node: device tree node for this clock
 */
static void __init of_keystone_psc_clk_init(struct device_node *node)
{
	of_psc_clk_init(node, &psc_lock);
}
CLK_OF_DECLARE(keystone_gate_clk, "ti,keystone,psc-clock",
					of_keystone_psc_clk_init);
