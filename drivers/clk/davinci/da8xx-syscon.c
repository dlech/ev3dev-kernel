/*
 * CFGCHIP syscon clock driver for TI DA8xx/OMAP-L1x/AM180x devices
 *
 * Copyright (C) 2016 David Lechner <david@lechnology.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/mfd/da8xx-cfgchip.h>
#include <linux/mfd/syscon.h>
#include <linux/of.h>
#include <linux/regmap.h>
#include <linux/slab.h>

enum syscon_clk_type {
	SYSCON_CLK_TYPE_GATE,
	SYSCON_CLK_TYPE_MUX,
	SYSCON_CLK_TYPE_USB0,
};

struct syscon_clk_data {
	struct clk_hw hw;
	struct regmap *regmap;
	u32 reg;
	u32 bitmask;
};

#define to_syscon_clk(_hw) container_of((_hw), struct syscon_clk_data, hw)
#define IS_MUX(t) ((t) == SYSCON_CLK_TYPE_MUX || (t) == SYSCON_CLK_TYPE_USB0)

static int da8xx_syscon_clk_gate_enable(struct clk_hw *hw)
{
	struct syscon_clk_data *data = to_syscon_clk(hw);

	regmap_write_bits(data->regmap, data->reg, data->bitmask, data->bitmask);

	return 0;
}

static void da8xx_syscon_clk_gate_disable(struct clk_hw *hw)
{
	struct syscon_clk_data *data = to_syscon_clk(hw);

	regmap_write_bits(data->regmap, data->reg, data->bitmask, 0);
}

static int da8xx_syscon_clk_gate_is_enabled(struct clk_hw *hw)
{
	struct syscon_clk_data *data = to_syscon_clk(hw);
	u32 val;

	regmap_read(data->regmap, data->reg, &val);

	return val & data->bitmask;
}

static const struct clk_ops da8xx_syscon_clk_gate_ops = {
	.enable		= da8xx_syscon_clk_gate_enable,
	.disable	= da8xx_syscon_clk_gate_disable,
	.is_enabled	= da8xx_syscon_clk_gate_is_enabled,
};

static u8 da8xx_syscon_clk_mux_get_parent(struct clk_hw *hw)
{
	struct syscon_clk_data *data = to_syscon_clk(hw);
	u32 val;

	regmap_read(data->regmap, data->reg, &val);

	return (val & data->bitmask) ? 1 : 0;
}

static int da8xx_syscon_clk_mux_set_parent(struct clk_hw *hw, u8 index)
{
	struct syscon_clk_data *data = to_syscon_clk(hw);

	regmap_write_bits(data->regmap, data->reg, data->bitmask,
			  index ? data->bitmask : 0);

	return 0;
}

static const struct clk_ops da8xx_syscon_clk_mux_ops = {
	.get_parent	= da8xx_syscon_clk_mux_get_parent,
	.set_parent	= da8xx_syscon_clk_mux_set_parent,
};

static int da8xx_usb0_phy_clk_enable(struct clk_hw *hw)
{
	struct syscon_clk_data *data = to_syscon_clk(hw);
	u32 val;
	u32 timeout = 500000; /* 500 msec */

	/*
	 * Turn on the USB 2.0 PHY, but just the PLL, and not OTG. The USB 1.1
	 * host may use the PLL clock without USB 2.0 OTG being used.
	 */
	regmap_write_bits(data->regmap, data->reg,
		CFGCHIP2_RESET | CFGCHIP2_PHYPWRDN | CFGCHIP2_PHY_PLLON,
		CFGCHIP2_PHY_PLLON);

	while (--timeout) {
		regmap_read(data->regmap, data->reg, &val);
		if (val & CFGCHIP2_PHYCLKGD)
			return 0;
		udelay(1);
	}

	return -ETIMEDOUT;
}

static void da8xx_usb0_phy_clk_disable(struct clk_hw *hw)
{
	struct syscon_clk_data *data = to_syscon_clk(hw);

	regmap_write_bits(data->regmap, data->reg, CFGCHIP2_PHYPWRDN,
			  CFGCHIP2_PHYPWRDN);
}

static unsigned long da8xx_usb0_phy_clk_recalc_rate(struct clk_hw *hw,
						    unsigned long parent_rate)
{
	struct syscon_clk_data *data = to_syscon_clk(hw);
	u32 val;

	/* can only handle certain parent clock rates */
	switch (parent_rate) {
	case 12000000:
		val = CFGCHIP2_REFFREQ_12MHZ;
		break;
	case 13000000:
		val = CFGCHIP2_REFFREQ_13MHZ;
		break;
	case 19200000:
		val = CFGCHIP2_REFFREQ_19_2MHZ;
		break;
	case 20000000:
		val = CFGCHIP2_REFFREQ_20MHZ;
		break;
	case 24000000:
		val = CFGCHIP2_REFFREQ_24MHZ;
		break;
	case 26000000:
		val = CFGCHIP2_REFFREQ_26MHZ;
		break;
	case 38400000:
		val = CFGCHIP2_REFFREQ_38_4MHZ;
		break;
	case 40000000:
		val = CFGCHIP2_REFFREQ_40MHZ;
		break;
	case 48000000:
		val = CFGCHIP2_REFFREQ_48MHZ;
		break;
	default:
		pr_err("%s: Bad parent clock rate on USB 2.0 PHY clock.\n",
		       __func__);
		return 0;
	}

	regmap_write_bits(data->regmap, data->reg, CFGCHIP2_REFFREQ_MASK, val);

	/* The USB PHY has a PLL that always generates 48MHz */
	return 48000000;
}

static const struct clk_ops da8xx_usb0_phy_clk_ops = {
	.enable		= da8xx_usb0_phy_clk_enable,
	.disable	= da8xx_usb0_phy_clk_disable,
	.recalc_rate	= da8xx_usb0_phy_clk_recalc_rate,
	.get_parent	= da8xx_syscon_clk_mux_get_parent,
	.set_parent	= da8xx_syscon_clk_mux_set_parent,
};

static void __init of_da8xx_syscon_clk_init(struct device_node *node,
					    enum syscon_clk_type type)
{
	struct clk_init_data init;
	struct syscon_clk_data *data;
	const char *parent_names[2];
	struct clk *clk;
	u32 parent_count, value;

	parent_count = of_clk_get_parent_count(node);
	if (IS_MUX(type) && parent_count != 2) {
		pr_err("%s: Requires exactly 2 parent clocks\n", __func__);
		return;
	} else if (!IS_MUX(type) && parent_count != 1) {
		pr_err("%s: Requires exactly 1 parent clock\n", __func__);
		return;
	}

	data = kzalloc(sizeof(*data), GFP_KERNEL);
	if (!data) {
		pr_err("%s: No memory\n", __func__);
		return;
	}

	data->regmap = syscon_regmap_lookup_by_compatible("ti,da830-cfgchip");
	if (IS_ERR(data->regmap)) {
		pr_err("%s: Could not get syscon node (%ld)\n", __func__,
		       PTR_ERR(data->regmap));
		kfree(data);
		return;
	}

	of_property_read_u32(node, "ti,cfgchip", &value);
	data->reg = CFGCHIP(value);
	of_property_read_u32(node, "bit-shift", &value);
	data->bitmask = 1 << value;

	of_property_read_string(node, "clock-output-names", &init.name);

	of_clk_parent_fill(node, parent_names, parent_count);
	init.parent_names = parent_names;
	init.num_parents = parent_count;

	switch (type) {
	case SYSCON_CLK_TYPE_GATE:
		init.ops = &da8xx_syscon_clk_gate_ops;
		break;
	case SYSCON_CLK_TYPE_MUX:
		init.ops = &da8xx_syscon_clk_mux_ops;
		break;
	case SYSCON_CLK_TYPE_USB0:
		init.ops = &da8xx_usb0_phy_clk_ops;
		break;
	}

	data->hw.init = &init;

	clk = clk_register(NULL, &data->hw);
	if (IS_ERR(clk)) {
		pr_err("%s: failed to register clock\n", __func__);
		kfree(data);
		return;
	}

	of_clk_add_provider(node, of_clk_src_simple_get, clk);
}

static void __init of_da8xx_syscon_clk_gate_init(struct device_node *node)
{
	of_da8xx_syscon_clk_init(node, SYSCON_CLK_TYPE_GATE);
}
CLK_OF_DECLARE(da8xx_syscon_clk_gate, "ti,da830-cfgchip-clk-gate",
	       of_da8xx_syscon_clk_gate_init);


static void __init of_da8xx_syscon_clk_mux_init(struct device_node *node)
{
	of_da8xx_syscon_clk_init(node, SYSCON_CLK_TYPE_MUX);
}
CLK_OF_DECLARE(da8xx_syscon_clk_mux, "ti,da830-cfgchip-clk-mux",
	       of_da8xx_syscon_clk_mux_init);

static void __init of_da8xx_syscon_clk_usb0_init(struct device_node *node)
{
	of_da8xx_syscon_clk_init(node, SYSCON_CLK_TYPE_USB0);
}
CLK_OF_DECLARE(da8xx_syscon_clk_usb0, "ti,da830-cfgchip-clk-usb0",
	       of_da8xx_syscon_clk_usb0_init);
