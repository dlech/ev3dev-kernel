/*
 * PSC power domain driver for TI chips
 *
 * Copyright (C) 2016 David Lechner <david@lechnology.com>
 *
 * Based on clock driver for Keystone 2 based devices
 * Copyright (C) 2013 Texas Instruments.
 *  Murali Karicheri <m-karicheri2@ti.com>
 *  Santosh Shilimkar <santosh.shilimkar@ti.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/err.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/pm_domain.h>
#include <linux/pm_runtime.h>
#include <linux/slab.h>

/* PSC register offsets */
#define PTCMD           0x120
#define PTSTAT          0x128
#define PDSTAT          0x200
#define PDCTL           0x300
#define MDSTAT          0x800
#define MDCTL           0xa00

/* PSC module states */
#define PSC_STATE_SWRSTDISABLE  0
#define PSC_STATE_SYNCRST   1
#define PSC_STATE_DISABLE   2
#define PSC_STATE_ENABLE    3

#define MDSTAT_STATE_MASK   0x3f
#define MDSTAT_MCKOUT       BIT(12)
#define PDSTAT_STATE_MASK   0x1f
#define MDCTL_FORCE     BIT(31)
#define MDCTL_LRESET        BIT(8)
#define PDCTL_NEXT      BIT(0)

/* Maximum timeout to bail out state transition for module */
#define STATE_TRANS_MAX_COUNT   0xffff

struct lpsc_data;

struct psc_data {
	struct lpsc_data *lpscs;
	void __iomem *base;
};

struct lpsc_data {
	struct generic_pm_domain gen;
	struct psc_data *psc;
	u32 index;
	u32 power_domain;
};

#define to_lpsc(gen) container_of(gen, struct lpsc_data, gen)

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

static int psc_power_on(struct generic_pm_domain *gen)
{
	struct lpsc_data *lpsc = to_lpsc(gen);
	struct psc_data *psc = lpsc->psc;
printk("%s: %s\n", __func__, lpsc->gen.name);
	psc_config(psc->base, PSC_STATE_ENABLE, lpsc->index,
		   lpsc->power_domain);

	return 0;
}

static int psc_power_off(struct generic_pm_domain *gen)
{
	struct lpsc_data *lpsc = to_lpsc(gen);
	struct psc_data *psc = lpsc->psc;
printk("%s: %s\n", __func__, lpsc->gen.name);
	psc_config(psc->base, PSC_STATE_DISABLE, lpsc->index,
		   lpsc->power_domain);

	return 0;
}

static int psc_is_on(struct generic_pm_domain *gen)
{
	struct lpsc_data *lpsc = to_lpsc(gen);
	struct psc_data *psc = lpsc->psc;
	u32 mdstat;

	mdstat = readl(psc->base + MDSTAT + 4 * lpsc->index);

	return (mdstat & MDSTAT_MCKOUT) ? 1 : 0;
}

static int psc_attach_dev(struct generic_pm_domain *genpd, struct device *dev)
{
	dev_info(dev, "attached pm: %s\n", genpd->name);

	return 0;
}

static __init int psc_init(void)
{
	struct device_node *np;

	for_each_compatible_node(np, NULL, "ti,da830-psc") {
		struct device_node *child = NULL;
		struct psc_data *psc;
		int num, i;

		psc = kzalloc(sizeof(*psc), GFP_KERNEL);
		if (!psc)
			return -ENOMEM;

		num = of_get_available_child_count(np);

		psc->lpscs = kzalloc(sizeof(*psc->lpscs) * num, GFP_KERNEL);
		if (!psc->lpscs) {
			kfree(psc);
			return -ENOMEM;
		}

		psc->base = of_iomap(np, 0);
		if (IS_ERR(psc->base)) {
			pr_err("%s: iomap failed: %ld\n", __func__,
			       PTR_ERR(psc->base));
			kfree(psc->lpscs);
			kfree(psc);
			continue;
		}

		for (i = 0; i < num; i++) {
			struct lpsc_data *lpsc = &psc->lpscs[i];

			child = of_get_next_available_child(np, child);

			lpsc->gen.name = child->name;
			lpsc->gen.power_on = psc_power_on;
			lpsc->gen.power_off = psc_power_off;
			lpsc->gen.attach_dev = psc_attach_dev;
			lpsc->psc = psc;
			of_property_read_u32_index(child, "reg", 0,
						   &lpsc->index);
			of_property_read_u32_index(child, "reg", 1,
						   &lpsc->power_domain);
printk("%s %s\n", lpsc->gen.name, psc_is_on(&lpsc->gen) ? "on" : "off");
			pm_genpd_init(&lpsc->gen, NULL, true);
			of_genpd_add_provider_simple(child, &lpsc->gen);
		}
	}

	pr_info("%s: done\n", __func__);

	return 0;
}

postcore_initcall(psc_init);
