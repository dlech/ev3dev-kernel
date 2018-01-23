// SPDX-License-Identifier: GPL-2.0
/*
 * phy-da8xx-usb - TI DaVinci DA8xx USB PHY driver
 *
 * Copyright (C) 2018 David Lechner <david@lechnology.com>
 */

#include <linux/regmap.h>

/**
 * da8xx_usb_phy_platform_data
 * @cfgchip: CFGCHIP syscon regmap
 */
struct da8xx_usb_phy_platform_data {
	struct regmap *cfgchip;
};
