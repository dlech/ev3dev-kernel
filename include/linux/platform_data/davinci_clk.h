// SPDX-License-Identifier: GPL-2.0
/*
 * TI DaVinci Clock support
 *
 * Copyright (C) 2017 David Lechner <david@lechnology.com>
 */

#ifndef __PLATFORM_DATA_DAVINCI_CLK_H
#define __PLATFORM_DATA_DAVINCI_CLK_H

#include <linux/types.h>

/**
 * da8xx_cfgchip_clk_data - DA8xx CFGCHIP clock platform data
 * @usb0_use_refclkin: when true, use USB_REFCLKIN, otherwise use AUXCLK for
 *                     USB 2.0 PHY clock
 * @usb1_use_refclkin: when true, use USB_REFCLKIN, otherwise use USB 2.0 PHY
 *                     PLL for USB 1.1 PHY clock
 */
struct da8xx_cfgchip_clk_data {
	bool usb0_use_refclkin;
	bool usb1_use_refclkin;
};

#endif /* __PLATFORM_DATA_DAVINCI_CLK_H */
