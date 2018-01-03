// SPDX-License-Identifier: GPL-2.0
/*
 * Clock driver for DA8xx/AM17xx/AM18xx/OMAP-L13x CFGCHIP
 *
 * Copyright (C) 2017 David Lechner <david@lechnology.com>
 */

#include <linux/clk-provider.h>
#include <linux/mfd/da8xx-cfgchip.h>
#include <linux/mfd/syscon.h>
#include <linux/of.h>
#include <linux/regmap.h>
#include <linux/slab.h>

#ifdef CONFIG_OF
struct da8xx_cfgchip_gate_clk {
	struct clk_hw hw;
	struct regmap *regmap;
	u32 reg;
	u32 mask;
};

#define to_da8xx_cfgchip_gate_clk(_hw) \
	container_of((_hw), struct da8xx_cfgchip_gate_clk, hw)

static int da8xx_cfgchip_gate_clk_enable(struct clk_hw *hw)
{
	struct da8xx_cfgchip_gate_clk *clk = to_da8xx_cfgchip_gate_clk(hw);

	return regmap_write_bits(clk->regmap, clk->reg, clk->mask, clk->mask);
}

static void da8xx_cfgchip_gate_clk_disable(struct clk_hw *hw)
{
	struct da8xx_cfgchip_gate_clk *clk = to_da8xx_cfgchip_gate_clk(hw);

	regmap_write_bits(clk->regmap, clk->reg, clk->mask, 0);
}

static int da8xx_cfgchip_gate_clk_is_enabled(struct clk_hw *hw)
{
	struct da8xx_cfgchip_gate_clk *clk = to_da8xx_cfgchip_gate_clk(hw);
	unsigned int val;

	regmap_read(clk->regmap, clk->reg, &val);

	return !!(val & clk->mask);
}

static const struct clk_ops da8xx_cfgchip_gate_clk_ops = {
	.enable		= da8xx_cfgchip_gate_clk_enable,
	.disable	= da8xx_cfgchip_gate_clk_disable,
	.is_enabled	= da8xx_cfgchip_gate_clk_is_enabled,
};

static void da8xx_cfgchip_gate_clk_init(struct device_node *np)
{
	struct da8xx_cfgchip_gate_clk *clk;
	struct clk_init_data init;
	const char *name = np->name;
	const char *parent_name;
	struct regmap *regmap;
	u32 reg, bit;
	int ret;

	of_property_read_string(np, "clock-output-names", &name);
	parent_name = of_clk_get_parent_name(np, 0);
	of_property_read_u32(np, "reg", &reg);
	of_property_read_u32(np, "bit", &bit);
	
	regmap = syscon_node_to_regmap(of_get_parent(np));
	if (IS_ERR(regmap)) {
		pr_err("%s: no regmap for syscon parent of %s (%ld)\n",
		       __func__, name, PTR_ERR(regmap));
		return;
	}

	clk = kzalloc(sizeof(*clk), GFP_KERNEL);
	if (!clk) {
		pr_err("%s: out of memory\n", __func__);
		return;
	}

	init.name = name;
	init.ops = &da8xx_cfgchip_gate_clk_ops;
	init.parent_names = parent_name ? &parent_name : NULL;
	init.num_parents = parent_name ? 1 : 0;

	clk->hw.init = &init;
	clk->regmap = regmap;
	clk->reg = CFGCHIP(reg);
	clk->mask = BIT(bit);

	ret = clk_hw_register(NULL, &clk->hw);
	if (ret) {
		pr_err("%s: failed to register %s (%d)\n", __func__, name, ret);
		return;
	}

	of_clk_add_hw_provider(np, of_clk_hw_simple_get, &clk->hw);
}

CLK_OF_DECLARE(da8xx_cfgchip_gate_clk, "ti,da830-cfgchip-gate-clock",
	       da8xx_cfgchip_gate_clk_init);
#endif
