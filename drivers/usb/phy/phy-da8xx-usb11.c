/*
 * USB 1.1 PHY for TI DaVinci DA8XX microcontrollers.
 *
 * Copyright (C) 2016 David Lechner <david@lechnology.com>
 *
 * Based on:
 * NOP USB transceiver for all USB transceiver which are either built-in
 * into USB IP or which are mostly autonomous.
 *
 * Copyright (C) 2009 Texas Instruments Inc
 * Author: Ajay Kumar Gupta <ajay.gupta@ti.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/usb/phy_da8xx_usb11.h>
#include <linux/slab.h>
#include <linux/clk.h>
#include <linux/regulator/consumer.h>
#include <linux/of.h>
#include <linux/delay.h>

struct phy_da8xx_usb11 {
	struct usb_phy phy;
	struct device *dev;
	struct clk *clk;
	struct regulator *vbus;
};

static int nop_set_suspend(struct usb_phy *x, int suspend)
{
	return 0;
}

static void nop_reset(struct phy_da8xx_usb11 *data)
{
	if (!data->gpiod_reset)
		return;

	gpiod_set_value(data->gpiod_reset, 1);
	usleep_range(10000, 20000);
	gpiod_set_value(data->gpiod_reset, 0);
}

int phy_da8xx_usb11_init(struct usb_phy *phy)
{
	struct phy_da8xx_usb11 *data = dev_get_drvdata(phy->dev);

	if (!IS_ERR(data->vcc)) {
		if (regulator_enable(data->vcc))
			dev_err(phy->dev, "Failed to enable power\n");
	}

	if (!IS_ERR(data->clk))
		clk_prepare_enable(data->clk);

	nop_reset(data);

	return 0;
}
EXPORT_SYMBOL_GPL(phy_da8xx_usb11_init);

void phy_da8xx_usb11_shutdown(struct usb_phy *phy)
{
	struct phy_da8xx_usb11 *data = dev_get_drvdata(phy->dev);

	gpiod_set_value(data->gpiod_reset, 1);

	if (!IS_ERR(data->clk))
		clk_disable_unprepare(data->clk);

	if (!IS_ERR(data->vcc)) {
		if (regulator_disable(data->vcc))
			dev_err(phy->dev, "Failed to disable power\n");
	}
}
EXPORT_SYMBOL_GPL(phy_da8xx_usb11_shutdown);

static int nop_set_peripheral(struct usb_otg *otg, struct usb_gadget *gadget)
{
	if (!otg)
		return -ENODEV;

	if (!gadget) {
		otg->gadget = NULL;
		return -ENODEV;
	}

	otg->gadget = gadget;
	otg->state = OTG_STATE_B_IDLE;
	return 0;
}

static int nop_set_host(struct usb_otg *otg, struct usb_bus *host)
{
	if (!otg)
		return -ENODEV;

	if (!host) {
		otg->host = NULL;
		return -ENODEV;
	}

	otg->host = host;
	return 0;
}

int usb_phy_gen_create_phy(struct device *dev, struct phy_da8xx_usb11 *data,
		struct phy_da8xx_usb11_platform_data *pdata)
{
	enum usb_phy_type type = USB_PHY_TYPE_UNDEFINED;
	int err = 0;

	u32 clk_rate = 0;
	bool needs_vcc = false;

	if (dev->of_node) {
		struct device_node *node = dev->of_node;

		if (of_property_read_u32(node, "clock-frequency", &clk_rate))
			clk_rate = 0;

		needs_vcc = of_property_read_bool(node, "vcc-supply");
		data->gpiod_reset = devm_gpiod_get_optional(dev, "reset",
							   GPIOD_ASIS);
		err = PTR_ERR_OR_ZERO(data->gpiod_reset);
		if (!err) {
			data->gpiod_vbus = devm_gpiod_get_optional(dev,
							 "vbus-detect",
							 GPIOD_ASIS);
			err = PTR_ERR_OR_ZERO(data->gpiod_vbus);
		}
	} else if (pdata) {
		type = pdata->type;
		clk_rate = pdata->clk_rate;
		needs_vcc = pdata->needs_vcc;
		if (gpio_is_valid(pdata->gpio_reset)) {
			err = devm_gpio_request_one(dev, pdata->gpio_reset,
						    GPIOF_ACTIVE_LOW,
						    dev_name(dev));
			if (!err)
				data->gpiod_reset =
					gpio_to_desc(pdata->gpio_reset);
		}
		data->gpiod_vbus = pdata->gpiod_vbus;
	}

	if (err == -EPROBE_DEFER)
		return -EPROBE_DEFER;
	if (err) {
		dev_err(dev, "Error requesting RESET or VBUS GPIO\n");
		return err;
	}
	if (data->gpiod_reset)
		gpiod_direction_output(data->gpiod_reset, 1);

	data->phy.otg = devm_kzalloc(dev, sizeof(*data->phy.otg),
			GFP_KERNEL);
	if (!data->phy.otg)
		return -ENOMEM;

	data->clk = devm_clk_get(dev, "main_clk");
	if (IS_ERR(data->clk)) {
		dev_dbg(dev, "Can't get phy clock: %ld\n",
					PTR_ERR(data->clk));
	}

	if (!IS_ERR(data->clk) && clk_rate) {
		err = clk_set_rate(data->clk, clk_rate);
		if (err) {
			dev_err(dev, "Error setting clock rate\n");
			return err;
		}
	}

	data->vcc = devm_regulator_get(dev, "vcc");
	if (IS_ERR(data->vcc)) {
		dev_dbg(dev, "Error getting vcc regulator: %ld\n",
					PTR_ERR(data->vcc));
		if (needs_vcc)
			return -EPROBE_DEFER;
	}

	data->dev		= dev;
	data->phy.dev		= data->dev;
	data->phy.label		= "data-xceiv";
	data->phy.set_suspend	= nop_set_suspend;
	data->phy.type		= type;

	return 0;
}
EXPORT_SYMBOL_GPL(usb_phy_gen_create_phy);

static int phy_da8xx_usb11_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct phy_da8xx_usb11	*data;
	int err;

	data = devm_kzalloc(dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	err = usb_phy_gen_create_phy(dev, data, dev_get_platdata(&pdev->dev));
	if (err)
		return err;
	if (data->gpiod_vbus) {
		err = devm_request_threaded_irq(&pdev->dev,
						gpiod_to_irq(data->gpiod_vbus),
						NULL, nop_gpio_vbus_thread,
						VBUS_IRQ_FLAGS, "vbus_detect",
						data);
		if (err) {
			dev_err(&pdev->dev, "can't request irq %i, err: %d\n",
				gpiod_to_irq(data->gpiod_vbus), err);
			return err;
		}
	}

	data->phy.init		= phy_da8xx_usb11_init;
	data->phy.shutdown	= phy_da8xx_usb11_shutdown;

	err = usb_add_phy_dev(&data->phy);
	if (err) {
		dev_err(&pdev->dev, "can't register transceiver, err: %d\n",
			err);
		return err;
	}

	platform_set_drvdata(pdev, data);

	return 0;
}

static int phy_da8xx_usb11_remove(struct platform_device *pdev)
{
	struct phy_da8xx_usb11 *data = platform_get_drvdata(pdev);

	usb_remove_phy(&data->phy);

	return 0;
}

static const struct of_device_id phy_da8xx_usb11_dt_ids[] = {
	{ .compatible = "ti,phy-da8xx-usb11" },
	{ }
};

MODULE_DEVICE_TABLE(of, phy_da8xx_usb11_dt_ids);

static struct platform_driver phy_da8xx_usb11_driver = {
	.probe		= phy_da8xx_usb11_probe,
	.remove		= phy_da8xx_usb11_remove,
	.driver		= {
		.name	= "phy-da8xx-usb11",
		.of_match_table = phy_da8xx_usb11_dt_ids,
	},
};

module_platform_driver(phy_da8xx_usb11_driver);

MODULE_ALIAS("platform:phy-da8xx-usb11");
MODULE_AUTHOR("David Lechner <david@lechnology.com>");
MODULE_DESCRIPTION("USB 1.1 PHY for TI DaVinci DA8XX");
MODULE_LICENSE("GPL");
