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
	struct genpd_onecell_data xlate;
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

static int psc_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	struct psc_data *psc;
	struct resource *res;
	int ret, num, i;

	if (!np) {
		dev_err(dev, "requires of node\n");
		return -EINVAL;
	}

	ret = of_property_read_u32(np, "num-domains", &num);
	if (ret < 0) {
		dev_err(dev, "missing num-domains of node\n");
		return -EINVAL;
	}

	psc = devm_kzalloc(dev, sizeof(*psc), GFP_KERNEL);
	if (!psc)
		return -ENOMEM;

	psc->xlate.domains = devm_kzalloc(dev, sizeof(*psc->xlate.domains)
					    * num, GFP_KERNEL);
	if (!psc->xlate.domains)
		return -ENOMEM;

	psc->lpscs = devm_kzalloc(dev, sizeof(*psc->lpscs) * num,
				    GFP_KERNEL);
	if (!psc->lpscs)
		return -ENOMEM;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	psc->base = devm_ioremap_resource(dev, res);
	if (IS_ERR(psc->base))
		return PTR_ERR(psc->base);

	psc->xlate.num_domains = num;

	for (i = 0; i < num; i++) {
		struct lpsc_data *lpsc = &psc->lpscs[i];

		of_property_read_string_index(np, "domain-names", i,
					      &lpsc->gen.name);
		lpsc->gen.power_on = psc_power_on;
		lpsc->gen.power_off = psc_power_off;
		lpsc->psc = psc;
		lpsc->index = i;

		pm_genpd_init(&lpsc->gen, NULL, true);
printk("%s %s\n", lpsc->gen.name, psc_is_on(&lpsc->gen) ? "on" : "off");
		psc->xlate.domains[i] = &lpsc->gen;
	}

	of_genpd_add_provider_onecell(np, &psc->xlate);
	platform_set_drvdata(pdev, psc);

	pm_runtime_get_sync(dev);

	dev_info(dev, "TI PSC\n");

	return 0;
}

static const struct of_device_id psc_of_match[] = {
	{ .compatible = "ti,da830-psc", },
	{},
};
MODULE_DEVICE_TABLE(of, psc_of_match);

static struct platform_driver psc_driver = {
	.driver	= {
		.name = "ti-psc",
		.of_match_table = psc_of_match,
	},
	.probe	= psc_probe,
};
builtin_platform_driver(psc_driver);

MODULE_AUTHOR("David Lechner <david@lechnology.com>");
MODULE_DESCRIPTION("TI PSC power domain driver");
MODULE_LICENSE("GPL v2");
