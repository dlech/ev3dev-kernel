/*
 * TI DaVinci reboot driver
 *
 * Copyright (C) 2014 Texas Instruments Incorporated. http://www.ti.com/
 *
 * Author: Ivan Khoronzhuk <ivan.khoronzhuk@ti.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/io.h>
#include <linux/module.h>
#include <linux/notifier.h>
#include <linux/reboot.h>
#include <linux/regmap.h>
#include <linux/mfd/syscon.h>
#include <linux/of_platform.h>

#define RSCTRL_RG			0xe8
#define RSCTRL_KEY_MASK			0x0000ffff
#define RSCTRL_RESET_MASK		BIT(16)
#define RSCTRL_KEY			0x5a69

static struct regmap *pllctrl_regs;

/**
 * rsctrl_enable_rspll_write - enable access to RSCTRL, RSCFG
 * To be able to access to RSCTRL, RSCFG registers
 * we have to write a key before
 */
static inline int rsctrl_enable_rspll_write(void)
{
	return regmap_update_bits(pllctrl_regs, RSCTRL_RG,
				  RSCTRL_KEY_MASK, RSCTRL_KEY);
}

static int rsctrl_restart_handler(struct notifier_block *this,
				  unsigned long mode, void *cmd)
{
	/* enable write access to RSTCTRL */
	rsctrl_enable_rspll_write();

	/* reset the SOC */
	regmap_update_bits(pllctrl_regs, RSCTRL_RG, RSCTRL_RESET_MASK, 0);

	return NOTIFY_DONE;
}

static struct notifier_block rsctrl_restart_nb = {
	.notifier_call = rsctrl_restart_handler,
	.priority = 128,
};

static const struct of_device_id rsctrl_of_match[] = {
	{.compatible = "ti,davinci-reset", },
	{},
};

static int rsctrl_probe(struct platform_device *pdev)
{
	int i;
	int ret;
	u32 val;
	unsigned int rg;
	u32 rsmux_offset;
	struct regmap *devctrl_regs;
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;

	if (!np)
		return -ENODEV;

	/* get regmaps */
	pllctrl_regs = syscon_regmap_lookup_by_phandle(np, "ti,syscon-pll");
	if (IS_ERR(pllctrl_regs))
		return PTR_ERR(pllctrl_regs);

	ret = rsctrl_enable_rspll_write();
	if (ret)
		return ret;

	ret = register_restart_handler(&rsctrl_restart_nb);
	if (ret)
		dev_err(dev, "cannot register restart handler (err=%d)\n", ret);

	return ret;
}

static struct platform_driver rsctrl_driver = {
	.probe = rsctrl_probe,
	.driver = {
		.name = KBUILD_MODNAME,
		.of_match_table = rsctrl_of_match,
	},
};
module_platform_driver(rsctrl_driver);

MODULE_AUTHOR("Ivan Khoronzhuk <ivan.khoronzhuk@ti.com>");
MODULE_DESCRIPTION("Texas Instruments DaVinci reset driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:" KBUILD_MODNAME);
