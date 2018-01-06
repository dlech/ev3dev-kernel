// SPDX-License-Identifier: GPL-2.0
/*
 * PLL clock driver for TI Davinci SoCs
 *
 * Copyright (C) 2017 David Lechner <david@lechnology.com>
 *
 * Based on drivers/clk/keystone/pll.c
 * Copyright (C) 2013 Texas Instruments Inc.
 *	Murali Karicheri <m-karicheri2@ti.com>
 *	Santosh Shilimkar <santosh.shilimkar@ti.com>
 */

#include <linux/clk-provider.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/of_address.h>
#include <linux/of.h>
#include <linux/slab.h>

#define REVID		0x000
#define PLLCTL		0x100
#define OCSEL		0x104
#define PLLSECCTL	0x108
#define PLLM		0x110
#define PREDIV		0x114
#define PLLDIV1		0x118
#define PLLDIV2		0x11c
#define PLLDIV3		0x120
#define OSCDIV		0x124
#define POSTDIV		0x128
#define BPDIV		0x12c
#define PLLCMD		0x138
#define PLLSTAT		0x13c
#define ALNCTL		0x140
#define DCHANGE		0x144
#define CKEN		0x148
#define CKSTAT		0x14c
#define SYSTAT		0x150
#define PLLDIV4		0x160
#define PLLDIV5		0x164
#define PLLDIV6		0x168
#define PLLDIV7		0x16c
#define PLLDIV8		0x170
#define PLLDIV9		0x174

#define PLLM_MASK		0x1f
#define PREDIV_RATIO_MASK	0x1f
#define PLLDIV_RATIO_WIDTH	5
#define PLLDIV_ENABLE_SHIFT	15
#define OSCDIV_RATIO_WIDTH	5
#define POSTDIV_RATIO_MASK	0x1f
#define BPDIV_RATIO_SHIFT	0
#define BPDIV_RATIO_WIDTH	5
#define CKEN_OBSCLK_SHIFT	1
#define CKEN_AUXEN_SHIFT	0

/**
 * struct davinci_pll_clk - Main PLL clock
 * @hw: clk_hw for the pll
 * @base: Base memory address
 * @parent_rate: Saved parent rate used by some child clocks
 */
struct davinci_pll_clk {
	struct clk_hw hw;
	void __iomem *base;
};

#define to_davinci_pll_clk(_hw) container_of((_hw), struct davinci_pll_clk, hw)

static unsigned long davinci_pll_clk_recalc(struct clk_hw *hw,
					    unsigned long parent_rate)
{
	struct davinci_pll_clk *pll = to_davinci_pll_clk(hw);
	unsigned long rate = parent_rate;
	u32 prediv, mult, postdiv;

	prediv = readl(pll->base + PREDIV) & PREDIV_RATIO_MASK;
	mult = readl(pll->base + PLLM) & PLLM_MASK;
	postdiv = readl(pll->base + POSTDIV) & POSTDIV_RATIO_MASK;

	rate /= prediv + 1;
	rate *= mult + 1;
	rate /= postdiv + 1;

	return rate;
}

#ifdef CONFIG_DEBUG_FS
#include <linux/debugfs.h>

#define DEBUG_REG(n)	\
{			\
	.name	= #n,	\
	.offset	= n,	\
}

static const struct debugfs_reg32 davinci_pll_regs[] = {
	DEBUG_REG(REVID),
	DEBUG_REG(PLLCTL),
	DEBUG_REG(OCSEL),
	DEBUG_REG(PLLSECCTL),
	DEBUG_REG(PLLM),
	DEBUG_REG(PREDIV),
	DEBUG_REG(PLLDIV1),
	DEBUG_REG(PLLDIV2),
	DEBUG_REG(PLLDIV3),
	DEBUG_REG(OSCDIV),
	DEBUG_REG(POSTDIV),
	DEBUG_REG(BPDIV),
	DEBUG_REG(PLLCMD),
	DEBUG_REG(PLLSTAT),
	DEBUG_REG(ALNCTL),
	DEBUG_REG(DCHANGE),
	DEBUG_REG(CKEN),
	DEBUG_REG(CKSTAT),
	DEBUG_REG(SYSTAT),
	DEBUG_REG(PLLDIV4),
	DEBUG_REG(PLLDIV5),
	DEBUG_REG(PLLDIV6),
	DEBUG_REG(PLLDIV7),
	DEBUG_REG(PLLDIV8),
	DEBUG_REG(PLLDIV9),
};

static int davinci_pll_debug_init(struct clk_hw *hw, struct dentry *dentry)
{
	struct davinci_pll_clk *pll = to_davinci_pll_clk(hw);
	struct debugfs_regset32 *regset;
	struct dentry *d;

	regset = kzalloc(sizeof(regset), GFP_KERNEL);
	if (!regset)
		return -ENOMEM;

	regset->regs = davinci_pll_regs;
	regset->nregs = ARRAY_SIZE(davinci_pll_regs);
	regset->base = pll->base;

	d = debugfs_create_regset32("registers", 0400, dentry, regset);
	if (IS_ERR(d)) {
		kfree(regset);
		return PTR_ERR(d);
	}

	return 0;
}
#else
#define davinci_pll_debug_init NULL
#endif

static const struct clk_ops davinci_pll_clk_ops = {
	.recalc_rate	= davinci_pll_clk_recalc,
	.debug_init	= davinci_pll_debug_init,
};

/**
 * davinci_pll_clk_register - Register a PLL clock
 * @name: The clock name
 * @parent_name: The parent clock name
 * @base: The PLL's memory region
 */
struct clk *davinci_pll_clk_register(const char *name,
				     const char *parent_name,
				     void __iomem *base)
{
	struct clk_init_data init;
	struct davinci_pll_clk *pll;
	struct clk *clk;

	pll = kzalloc(sizeof(*pll), GFP_KERNEL);
	if (!pll)
		return ERR_PTR(-ENOMEM);

	init.name = name;
	init.ops = &davinci_pll_clk_ops;
	init.parent_names = (parent_name ? &parent_name : NULL);
	init.num_parents = (parent_name ? 1 : 0);

	pll->base = base;
	pll->hw.init = &init;

	clk = clk_register(NULL, &pll->hw);
	if (IS_ERR(clk))
		kfree(pll);

	return clk;
}

struct davinci_pll_aux_clk {
	struct clk_hw hw;
	struct davinci_pll_clk *pll;
};

/**
 * davinci_pll_aux_clk_register - Register bypass clock (AUXCLK)
 * @name: The clock name
 * @parent_name: The parent clock name (usually "ref_clk" since this bypasses
 *               the PLL)
 * @base: The PLL memory region
 */
struct clk *davinci_pll_aux_clk_register(const char *name,
					 const char *parent_name,
					 void __iomem *base)
{
	return clk_register_gate(NULL, name, parent_name, 0, base + CKEN,
				 CKEN_AUXEN_SHIFT, 0, NULL);
}

/**
 * davinci_pll_bpdiv_clk_register - Register bypass divider clock (SYSCLKBP)
 * @name: The clock name
 * @parent_name: The parent clock name (usually "ref_clk" since this bypasses
 *               the PLL)
 * @base: The PLL memory region
 */
struct clk *davinci_pll_bpdiv_clk_register(const char *name,
					   const char *parent_name,
					   void __iomem *base)
{
	return clk_register_divider(NULL, name, parent_name, 0, base + BPDIV,
				    BPDIV_RATIO_SHIFT, BPDIV_RATIO_WIDTH,
				    CLK_DIVIDER_READ_ONLY, NULL);
}

/**
 * davinci_pll_obs_clk_register - Register oscillator divider clock (OBSCLK)
 * @name: The clock name
 * @parent_names: The parent clock names
 * @num_parents: The number of paren clocks
 * @base: The PLL memory region
 * @table: A table of values cooresponding to the parent clocks (see OCSEL
 *         register in SRM for values)
 */
struct clk *davinci_pll_obs_clk_register(const char *name,
					 const char * const *parent_names,
					 u8 num_parents,
					 void __iomem *base,
					 u32 *table)
{
	struct clk_mux *mux;
	struct clk_gate *gate;
	struct clk_divider *divider;
	struct clk *clk;

	mux = kzalloc(sizeof(*mux), GFP_KERNEL);
	if (!mux)
		return ERR_PTR(-ENOMEM);

	mux->reg = base + OCSEL;
	mux->table = table;

	gate = kzalloc(sizeof(*gate), GFP_KERNEL);
	if (!gate) {
		kfree(mux);
		return ERR_PTR(-ENOMEM);
	}

	gate->reg = base + CKEN;
	gate->bit_idx = CKEN_OBSCLK_SHIFT;

	divider = kzalloc(sizeof(*divider), GFP_KERNEL);
	if (!divider) {
		kfree(gate);
		kfree(mux);
		return ERR_PTR(-ENOMEM);
	}

	divider->reg = base + OSCDIV;
	divider->width = OSCDIV_RATIO_WIDTH;

	clk = clk_register_composite(NULL, name, parent_names, num_parents,
				     &mux->hw, &clk_mux_ops,
				     &divider->hw, &clk_divider_ops,
				     &gate->hw, &clk_gate_ops, 0);
	if (IS_ERR(clk)) {
		kfree(divider);
		kfree(gate);
		kfree(mux);
	}

	return clk;
}

/**
 * davinci_pll_div_clk_register - Register a PLLDIV (SYSCLK) clock
 * @name: The clock name
 * @parent_name: The parent clock name
 * @base: The PLL memory region
 * @id: The id of the divider (n in PLLDIVn)
 */
struct clk *davinci_pll_div_clk_register(const char *name,
					 const char *parent_name,
					 void __iomem *base,
					 u32 id)
{
	const char * const *parent_names = (parent_name ? &parent_name : NULL);
	int num_parents = (parent_name ? 1 : 0);
	struct clk_gate *gate;
	struct clk_divider *divider;
	struct clk *clk;
	u32 reg;

	/* PLLDIVn registers are not entirely consecutive */
	if (id < 4)
		reg = PLLDIV1 + 4 * (id - 1);
	else
		reg = PLLDIV4 + 4 * (id - 4);

	gate = kzalloc(sizeof(*gate), GFP_KERNEL);
	if (!gate)
		return ERR_PTR(-ENOMEM);

	gate->reg = base + reg;
	gate->bit_idx = PLLDIV_ENABLE_SHIFT;

	divider = kzalloc(sizeof(*divider), GFP_KERNEL);
	if (!divider) {
		kfree(gate);
		return ERR_PTR(-ENOMEM);
	}

	divider->reg = base + reg;
	divider->width = PLLDIV_RATIO_WIDTH;
	divider->flags = CLK_DIVIDER_READ_ONLY;

	clk = clk_register_composite(NULL, name, parent_names, num_parents,
				     NULL, NULL, &divider->hw, &clk_divider_ops,
				     &gate->hw, &clk_gate_ops, 0);
	if (IS_ERR(clk)) {
		kfree(divider);
		kfree(gate);
	}

	return clk;
}

#ifdef CONFIG_OF
#define MAX_NAME_SIZE 20

void of_davinci_pll_init(struct device_node *node, const char *name,
			 u8 num_sysclk)
{
	struct device_node *child;
	const char *parent_name;
	void __iomem *base;
	struct clk *clk;

	base = of_iomap(node, 0);
	if (!base) {
		pr_err("%s: ioremap failed\n", __func__);
		return;
	}

	parent_name = of_clk_get_parent_name(node, 0);

	clk = davinci_pll_clk_register(name, parent_name, base);
	if (IS_ERR(clk)) {
		pr_err("%s: failed to register %s (%ld)\n", __func__, name,
		       PTR_ERR(clk));
		return;
	}

	child = of_get_child_by_name(node, "sysclk");
	if (child && of_device_is_available(child)) {
		struct clk_onecell_data *clk_data;
		char child_name[MAX_NAME_SIZE];
		u32 id;

		clk_data = clk_alloc_onecell_data(num_sysclk + 1);
		if (!clk_data) {
			pr_err("%s: out of memory\n", __func__);
			return;
		}

		for (id = 1; id <= num_sysclk; id++) {
			/* Hack to keep DDR PHY clock (pll1_sysclk1) on */
			if (strcmp(name, "pll1") == 0 && id == 1)
				continue;

			snprintf(child_name, MAX_NAME_SIZE, "%s_sysclk%d",
				 name, id);

			clk = davinci_pll_div_clk_register(child_name, name,
							   base, id);
			if (IS_ERR(clk))
				pr_warn("%s: failed to register %s (%ld)\n",
					__func__, child_name, PTR_ERR(clk));
			else
				clk_data->clks[id] = clk;
		}
		of_clk_add_provider(child, of_clk_src_onecell_get, clk_data);

	}
	of_node_put(child);

	child = of_get_child_by_name(node, "auxclk");
	if (child && of_device_is_available(child)) {
		char child_name[MAX_NAME_SIZE];

		snprintf(child_name, MAX_NAME_SIZE, "%s_aux_clk", name);

		clk = davinci_pll_aux_clk_register(child_name, parent_name, base);
		if (IS_ERR(clk))
			pr_warn("%s: failed to register %s (%ld)\n", __func__,
				child_name, PTR_ERR(clk));
		else
			of_clk_add_provider(child, of_clk_src_simple_get, clk);
	}
	of_node_put(child);
}
#endif
