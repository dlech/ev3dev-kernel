// SPDX-License-Identifier: GPL-2.0
/*
 * Clock driver for DA8xx/AM17xx/AM18xx/OMAP-L13x CFGCHIP
 *
 * Copyright (C) 2017 David Lechner <david@lechnology.com>
 */

#define pr_fmt(fmt) "%s: " fmt "\n", __func__

#include <linux/clk-provider.h>
#include <linux/init.h>
#include <linux/mfd/da8xx-cfgchip.h>
#include <linux/mfd/syscon.h>
#include <linux/of.h>
#include <linux/regmap.h>
#include <linux/slab.h>

#ifdef CONFIG_OF
struct da8xx_cfgchip_gate_clk_info {
	const char *name;
	u32 cfgchip;
	u32 bit;
};

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

static struct clk * __init
da8xx_cfgchip_gate_clk_register(const struct da8xx_cfgchip_gate_clk_info *info,
				const char *parent_name,
				struct regmap *regmap)
{
	struct da8xx_cfgchip_gate_clk *gate;
	struct clk_init_data init;

	gate = kzalloc(sizeof(*gate), GFP_KERNEL);
	if (!gate)
		return ERR_PTR(-ENOMEM);

	init.name = info->name;
	init.ops = &da8xx_cfgchip_gate_clk_ops;
	init.parent_names = parent_name ? &parent_name : NULL;
	init.num_parents = parent_name ? 1 : 0;
	init.flags = 0;

	gate->hw.init = &init;
	gate->regmap = regmap;
	gate->reg = info->cfgchip;
	gate->mask = info->bit;

	return clk_register(NULL, &gate->hw);
}

static void __init
of_da8xx_cfgchip_gate_clk_init(struct device_node *np,
			       const struct da8xx_cfgchip_gate_clk_info *info)
{
	const char *parent_name;
	struct regmap *regmap;
	struct clk *clk;

	parent_name = of_clk_get_parent_name(np, 0);

	regmap = syscon_node_to_regmap(of_get_parent(np));
	if (IS_ERR(regmap)) {
		pr_err("no regmap for syscon parent of %s (%lu)", np->full_name,
		       PTR_ERR(regmap));
		return;
	}

	clk = da8xx_cfgchip_gate_clk_register(info, parent_name, regmap);
	if (IS_ERR(clk)) {
		pr_err("failed to register %s (%lu)", np->full_name,
		       PTR_ERR(clk));
		return;
	}

	of_clk_add_provider(np, of_clk_src_simple_get, clk);
}

static const struct da8xx_cfgchip_gate_clk_info da8xx_tbclksync_info __initconst = {
	.name = "ehrpwm_tbclk",
	.cfgchip = CFGCHIP(1),
	.bit = CFGCHIP1_TBCLKSYNC,
};

static void __init da8xx_tbclksync_init(struct device_node *np)
{
	of_da8xx_cfgchip_gate_clk_init(np, &da8xx_tbclksync_info);
}
CLK_OF_DECLARE(da8xx_tbclksync, "ti,da830-tbclksync", da8xx_tbclksync_init);

static const struct da8xx_cfgchip_gate_clk_info da8xx_div4p5ena_info __initconst = {
	.name = "div4.5",
	.cfgchip = CFGCHIP(1),
	.bit = CFGCHIP3_DIV45PENA,
};

static void __init da8xx_div4p5ena_init(struct device_node *np)
{
	of_da8xx_cfgchip_gate_clk_init(np, &da8xx_div4p5ena_info);
}
CLK_OF_DECLARE(da8xx_div4p5ena, "ti,da830-div4p5ena", da8xx_div4p5ena_init);

struct da8xx_cfgchip_mux_clk_info {
	const char *name;
	const char *parent0;
	const char *parent1;
	u32 cfgchip;
	u32 bit;
};

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

static void da8xx_cfgchip_mux_clk_init(struct device_node *np,
				const struct da8xx_cfgchip_mux_clk_info *info)
{
	struct da8xx_cfgchip_mux_clk *clk;
	struct clk_init_data init;
	const char *parent_names[2];
	struct regmap *regmap;
	int ret;

	ret = of_property_match_string(np, "clock-names", info->parent0);
	parent_names[0] = of_clk_get_parent_name(np, ret);
	if (!parent_names[0]) {
		pr_err("missing %s clock", info->parent0);
		return;
	}

	ret = of_property_match_string(np, "clock-names", info->parent1);
	parent_names[1] = of_clk_get_parent_name(np, ret);
	if (!parent_names[1]) {
		pr_err("missing %s clock", info->parent1);
		return;
	}

	regmap = syscon_node_to_regmap(of_get_parent(np));
	if (IS_ERR(regmap)) {
		pr_err("no regmap for syscon parent of %s (%ld)", np->full_name,
		       PTR_ERR(regmap));
		return;
	}

	clk = kzalloc(sizeof(*clk), GFP_KERNEL);
	if (!clk)
		return;

	init.name = info->name;
	init.ops = &da8xx_cfgchip_mux_clk_ops;
	init.parent_names = parent_names;
	init.num_parents = 2;
	init.flags = 0;

	clk->hw.init = &init;
	clk->regmap = regmap;
	clk->reg = info->cfgchip;
	clk->mask = info->bit;

	ret = clk_hw_register(NULL, &clk->hw);
	if (ret) {
		pr_err("failed to register %s (%d)", np->full_name, ret);
		return;
	}

	of_clk_add_hw_provider(np, of_clk_hw_simple_get, &clk->hw);
}

static const struct da8xx_cfgchip_mux_clk_info da8xx_upp_tx_clksrc_info __initconst = {
	.name = "upp_tx_clksrc",
	.parent0 = "async3",
	.parent1 = "upp_2xtxclk",
	.cfgchip = CFGCHIP(3),
	.bit = CFGCHIP3_UPP_TX_CLKSRC,
};

static void __init da8xx_upp_tx_clksrc_init(struct device_node *np)
{
	da8xx_cfgchip_mux_clk_init(np, &da8xx_upp_tx_clksrc_info);
}
CLK_OF_DECLARE(da8xx_upp_tx_clksrc, "ti,da850-upp-tx-clksrc",
	       da8xx_upp_tx_clksrc_init);

static const struct da8xx_cfgchip_mux_clk_info da8xx_async3_clksrc_info __initconst = {
	.name = "async3",
	.parent0 = "pll0_sysclk2",
	.parent1 = "pll1_sysclk2",
	.cfgchip = CFGCHIP(3),
	.bit = CFGCHIP3_ASYNC3_CLKSRC,
};

static void __init da8xx_async3_init(struct device_node *np)
{
	da8xx_cfgchip_mux_clk_init(np, &da8xx_async3_clksrc_info);
}
CLK_OF_DECLARE(da8xx_async3_clksrc, "ti,da850-async3-clksrc", da8xx_async3_init);

static const struct da8xx_cfgchip_mux_clk_info da8xx_ema_clksrc_info __initconst = {
	.name = "ema_clksrc",
	.parent0 = "pll0_sysclk3",
	.parent1 = "div4.5",
	.cfgchip = CFGCHIP(3),
	.bit = CFGCHIP3_EMA_CLKSRC,
};

static void __init da8xx_ema_clksrc_init(struct device_node *np)
{
	da8xx_cfgchip_mux_clk_init(np, &da8xx_ema_clksrc_info);
}
CLK_OF_DECLARE(da8xx_ema_clksrc, "ti,da830-ema-clksrc", da8xx_ema_clksrc_init);

static const struct da8xx_cfgchip_mux_clk_info da8xx_emb_clksrc_info __initconst = {
	.name = "emb_clksrc",
	.parent0 = "pll0_sysclk5",
	.parent1 = "div4.5",
	.cfgchip = CFGCHIP(3),
	.bit = CFGCHIP3_EMB_CLKSRC,
};

static void __init da8xx_emb_clksrc_init(struct device_node *np)
{
	da8xx_cfgchip_mux_clk_init(np, &da8xx_emb_clksrc_info);
}
CLK_OF_DECLARE(da8xx_emb_clksrc, "ti,da830-emb-clksrc", da8xx_emb_clksrc_init);
#endif
