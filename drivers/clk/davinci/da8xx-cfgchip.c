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

static void da8xx_cfgchip_gate_clk_init(struct device_node *np, u32 reg,
					u32 mask)
{
	struct da8xx_cfgchip_gate_clk *clk;
	struct clk_init_data init;
	const char *name = np->name;
	const char *parent_name;
	struct regmap *regmap;
	int ret;

	of_property_read_string(np, "clock-output-names", &name);
	parent_name = of_clk_get_parent_name(np, 0);
	
	regmap = syscon_node_to_regmap(of_get_parent(np));
	if (IS_ERR(regmap)) {
		pr_err("%s: no regmap for syscon parent of %s (%ld)\n",
		       __func__, np->full_name, PTR_ERR(regmap));
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
	init.flags = 0;

	clk->hw.init = &init;
	clk->regmap = regmap;
	clk->reg = reg;
	clk->mask = mask;

	ret = clk_hw_register(NULL, &clk->hw);
	if (ret) {
		pr_err("%s: failed to register %s (%d)\n", __func__,
		       np->full_name, ret);
		return;
	}

	of_clk_add_hw_provider(np, of_clk_hw_simple_get, &clk->hw);
}

static void da8xx_tbclk_init(struct device_node *np)
{
	da8xx_cfgchip_gate_clk_init(np, CFGCHIP(1), CFGCHIP1_TBCLKSYNC);
}
CLK_OF_DECLARE(da8xx_tbclk, "ti,da830-tbclk", da8xx_tbclk_init);

struct da8xx_cfgchip_mux_clk {
	struct clk_hw hw;
	struct regmap *regmap;
	u32 reg;
	u32 mask;
};

#define to_da8xx_cfgchip_mux_clk(_hw) \
	container_of((_hw), struct da8xx_cfgchip_mux_clk, hw)

static int da8xx_cfgchip_mux_clk_set_parent(struct clk_hw *hw, u8 index)
{
	struct da8xx_cfgchip_mux_clk *clk = to_da8xx_cfgchip_mux_clk(hw);
	unsigned int val = index ? clk->mask : 0;

	return regmap_write_bits(clk->regmap, clk->reg, clk->mask, val);
}

static u8 da8xx_cfgchip_mux_clk_get_parent(struct clk_hw *hw)
{
	struct da8xx_cfgchip_mux_clk *clk = to_da8xx_cfgchip_mux_clk(hw);
	unsigned int val;

	regmap_read(clk->regmap, clk->reg, &val);

	return (val & clk->mask) ? 1 : 0;
}

static const struct clk_ops da8xx_cfgchip_mux_clk_ops = {
	.set_parent	= da8xx_cfgchip_mux_clk_set_parent,
	.get_parent	= da8xx_cfgchip_mux_clk_get_parent,
};

static void da8xx_cfgchip_mux_clk_init(struct device_node *np, u32 reg,
				       u32 mask)
{
	struct da8xx_cfgchip_mux_clk *clk;
	struct clk_init_data init;
	const char *name = np->name;
	const char *parent_names[2];
	struct regmap *regmap;
	int ret;

	if (of_clk_get_parent_count(np) != 2) {
		pr_err("%s: %s requires two parent clocks\n",
		       __func__, np->full_name);
		return;
	}

	of_property_read_string(np, "clock-output-names", &name);
	parent_names[0] = of_clk_get_parent_name(np, 0);
	parent_names[1] = of_clk_get_parent_name(np, 1);
	
	regmap = syscon_node_to_regmap(of_get_parent(np));
	if (IS_ERR(regmap)) {
		pr_err("%s: no regmap for syscon parent of %s (%ld)\n",
		       __func__, np->full_name, PTR_ERR(regmap));
		return;
	}

	clk = kzalloc(sizeof(*clk), GFP_KERNEL);
	if (!clk) {
		pr_err("%s: out of memory\n", __func__);
		return;
	}

	init.name = name;
	init.ops = &da8xx_cfgchip_mux_clk_ops;
	init.parent_names = parent_names;
	init.num_parents = 2;
	init.flags = 0;

	clk->hw.init = &init;
	clk->regmap = regmap;
	clk->reg = reg;
	clk->mask = mask;

	ret = clk_hw_register(NULL, &clk->hw);
	if (ret) {
		pr_err("%s: failed to register %s (%d)\n", __func__,
		       np->full_name, ret);
		return;
	}

	of_clk_add_hw_provider(np, of_clk_hw_simple_get, &clk->hw);
}

static void da8xx_aysnc3_init(struct device_node *np)
{
	da8xx_cfgchip_mux_clk_init(np, CFGCHIP(3), CFGCHIP3_ASYNC3_CLKSRC);
}
CLK_OF_DECLARE(da8xx_aysnc3, "ti,da850-async3-clock", da8xx_aysnc3_init);
#endif
