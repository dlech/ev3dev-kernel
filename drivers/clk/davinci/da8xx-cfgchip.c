// SPDX-License-Identifier: GPL-2.0
/*
 * Clock driver for DA8xx/AM17xx/AM18xx/OMAP-L13x CFGCHIP
 *
 * Copyright (C) 2018 David Lechner <david@lechnology.com>
 */

#define pr_fmt(fmt) "%s: " fmt "\n", __func__

#include <linux/clk-provider.h>
#include <linux/init.h>
#include <linux/mfd/da8xx-cfgchip.h>
#include <linux/mfd/syscon.h>
#include <linux/of.h>
#include <linux/regmap.h>
#include <linux/slab.h>

#define DA8XX_GATE_CLOCK_IS_DIV4P5	BIT(1)

struct da8xx_cfgchip_gate_clk_info {
	const char *name;
	u32 cfgchip;
	u32 bit;
	u32 flags;
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

static unsigned long da8xx_cfgchip_div4p5_recalc_rate(struct clk_hw *hw,
						      unsigned long parent_rate)
{
	/* this clock divides by 4.5 */
	return parent_rate * 2 / 9;
}

static const struct clk_ops da8xx_cfgchip_gate_clk_ops = {
	.enable		= da8xx_cfgchip_gate_clk_enable,
	.disable	= da8xx_cfgchip_gate_clk_disable,
	.is_enabled	= da8xx_cfgchip_gate_clk_is_enabled,
};

static const struct clk_ops da8xx_cfgchip_div4p5_clk_ops = {
	.enable		= da8xx_cfgchip_gate_clk_enable,
	.disable	= da8xx_cfgchip_gate_clk_disable,
	.is_enabled	= da8xx_cfgchip_gate_clk_is_enabled,
	.recalc_rate	= da8xx_cfgchip_div4p5_recalc_rate,
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
	if (info->flags & DA8XX_GATE_CLOCK_IS_DIV4P5)
		init.ops = &da8xx_cfgchip_div4p5_clk_ops;
	else
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

static const struct da8xx_cfgchip_gate_clk_info da8xx_tbclksync_info __initconst = {
	.name = "ehrpwm_tbclk",
	.cfgchip = CFGCHIP(1),
	.bit = CFGCHIP1_TBCLKSYNC,
};

struct clk * __init da8xx_cfgchip_register_tbclk(struct regmap *regmap)
{
	return da8xx_cfgchip_gate_clk_register(&da8xx_tbclksync_info, "ehrpwm",
					       regmap);
}

static const struct da8xx_cfgchip_gate_clk_info da8xx_div4p5ena_info __initconst = {
	.name = "div4.5",
	.cfgchip = CFGCHIP(3),
	.bit = CFGCHIP3_DIV45PENA,
	.flags = DA8XX_GATE_CLOCK_IS_DIV4P5,
};

struct clk * __init da8xx_cfgchip_register_div4p5(struct regmap *regmap)
{
	return da8xx_cfgchip_gate_clk_register(&da8xx_div4p5ena_info,
					       "pll0_pllout", regmap);
}

#ifdef CONFIG_OF
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

static void __init da8xx_tbclksync_init(struct device_node *np)
{
	of_da8xx_cfgchip_gate_clk_init(np, &da8xx_tbclksync_info);
}
CLK_OF_DECLARE(da8xx_tbclksync, "ti,da830-tbclksync", da8xx_tbclksync_init);

static void __init da8xx_div4p5ena_init(struct device_node *np)
{
	of_da8xx_cfgchip_gate_clk_init(np, &da8xx_div4p5ena_info);
}
CLK_OF_DECLARE(da8xx_div4p5ena, "ti,da830-div4p5ena", da8xx_div4p5ena_init);
#endif

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

static struct clk * __init
da8xx_cfgchip_mux_clk_register(const struct da8xx_cfgchip_mux_clk_info *info,
			       struct regmap *regmap)
{
	const char * const parent_names[] = { info->parent0, info->parent1 };
	struct da8xx_cfgchip_mux_clk *mux;
	struct clk_init_data init;

	mux = kzalloc(sizeof(*mux), GFP_KERNEL);
	if (!mux)
		return ERR_PTR(-ENOMEM);

	init.name = info->name;
	init.ops = &da8xx_cfgchip_mux_clk_ops;
	init.parent_names = parent_names;
	init.num_parents = 2;
	init.flags = 0;

	mux->hw.init = &init;
	mux->regmap = regmap;
	mux->reg = info->cfgchip;
	mux->mask = info->bit;

	return clk_register(NULL, &mux->hw);
}

static const struct da8xx_cfgchip_mux_clk_info da850_async1_info __initconst = {
	.name = "async1",
	.parent0 = "pll0_sysclk3",
	.parent1 = "div4.5",
	.cfgchip = CFGCHIP(3),
	.bit = CFGCHIP3_EMA_CLKSRC,
};

struct clk * __init da8xx_cfgchip_register_async1(struct regmap *cfgchip)
{
	return da8xx_cfgchip_mux_clk_register(&da850_async1_info, cfgchip);
}

static const struct da8xx_cfgchip_mux_clk_info da850_async3_info __initconst = {
	.name = "async3",
	.parent0 = "pll0_sysclk2",
	.parent1 = "pll1_sysclk2",
	.cfgchip = CFGCHIP(3),
	.bit = CFGCHIP3_ASYNC3_CLKSRC,
};

struct clk * __init da8xx_cfgchip_register_async3(struct regmap *cfgchip)
{
	return da8xx_cfgchip_mux_clk_register(&da850_async3_info, cfgchip);
}

#ifdef CONFIG_OF
static void __init
of_da8xx_cfgchip_init_mux_clock(struct device_node *np,
				const struct da8xx_cfgchip_mux_clk_info *info)
{
	struct regmap *regmap;
	struct clk *clk;

	regmap = syscon_node_to_regmap(of_get_parent(np));
	if (IS_ERR(regmap)) {
		pr_err("no regmap for syscon parent of %s (%ld)", np->full_name,
		       PTR_ERR(regmap));
		return;
	}

	clk = da8xx_cfgchip_mux_clk_register(info, regmap);
	if (IS_ERR(clk)) {
		pr_err("Failed to register %s (%ld)", np->full_name,
		       PTR_ERR(clk));
		return;
	}

	of_clk_add_provider(np, of_clk_src_simple_get, clk);
}

static void __init da850_async1_init(struct device_node *np)
{
	of_da8xx_cfgchip_init_mux_clock(np, &da850_async1_info);
}
CLK_OF_DECLARE(da850_async1, "ti,da850-async1-clksrc", da850_async1_init);

static void __init da850_async3_init(struct device_node *np)
{
	of_da8xx_cfgchip_init_mux_clock(np, &da850_async3_info);
}
CLK_OF_DECLARE(da850_async3, "ti,da850-async3-clksrc", da850_async3_init);
#endif
