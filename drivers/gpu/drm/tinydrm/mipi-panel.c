/*
 * DRM driver for MIPI DBI compatible panels
 *
 * Copyright 2016 Noralf Trønnes
 * Copyright 2017 David Lechner <david@lechnology.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <drm/tinydrm/ili9341.h>
#include <drm/tinydrm/mipi-dbi.h>
#include <drm/tinydrm/st7586.h>
#include <drm/tinydrm/tinydrm-helpers.h>
#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/module.h>
#include <linux/property.h>
#include <linux/regulator/consumer.h>
#include <linux/spi/spi.h>
#include <video/mipi_display.h>

enum mipi_panel_type {
	MIPI_PANEL_TYPE_UNKNOWN,
	MIPI_PANEL_TYPE_LEGO_EV3_LCD,
	MIPI_PANEL_TYPE_MULTI_INNO_MI0283QT,
};

struct mipi_panel_info {
	int (*init)(struct mipi_dbi *mipi);
	const struct drm_display_mode mode;
	enum mipi_dcs_pixel_format pixel_fmt;
};

static int mipi_panel_init_ili9341(struct mipi_dbi *mipi)
{
	struct tinydrm_device *tdev = &mipi->tinydrm;
	struct device *dev = tdev->drm->dev;
	u8 addr_mode;
	int ret;

	DRM_DEBUG_KMS("\n");

	ret = regulator_enable(mipi->regulator);
	if (ret) {
		dev_err(dev, "Failed to enable regulator %d\n", ret);
		return ret;
	}

	/* Avoid flicker by skipping setup if the bootloader has done it */
	if (mipi_dbi_display_is_on(mipi))
		return 0;

	mipi_dbi_hw_reset(mipi);
	ret = mipi_dbi_command(mipi, MIPI_DCS_SOFT_RESET);
	if (ret) {
		dev_err(dev, "Error sending command %d\n", ret);
		regulator_disable(mipi->regulator);
		return ret;
	}

	msleep(20);

	mipi_dbi_command(mipi, MIPI_DCS_SET_DISPLAY_OFF);

	mipi_dbi_command(mipi, ILI9341_PWCTRLB, 0x00, 0x83, 0x30);
	mipi_dbi_command(mipi, ILI9341_PWRSEQ, 0x64, 0x03, 0x12, 0x81);
	mipi_dbi_command(mipi, ILI9341_DTCTRLA, 0x85, 0x01, 0x79);
	mipi_dbi_command(mipi, ILI9341_PWCTRLA, 0x39, 0x2c, 0x00, 0x34, 0x02);
	mipi_dbi_command(mipi, ILI9341_PUMPCTRL, 0x20);
	mipi_dbi_command(mipi, ILI9341_DTCTRLB, 0x00, 0x00);

	/* Power Control */
	mipi_dbi_command(mipi, ILI9341_PWCTRL1, 0x26);
	mipi_dbi_command(mipi, ILI9341_PWCTRL2, 0x11);
	/* VCOM */
	mipi_dbi_command(mipi, ILI9341_VMCTRL1, 0x35, 0x3e);
	mipi_dbi_command(mipi, ILI9341_VMCTRL2, 0xbe);

	/* Memory Access Control */
	mipi_dbi_command(mipi, MIPI_DCS_SET_PIXEL_FORMAT, 0x55);

	switch (mipi->rotation) {
	default:
		addr_mode = ILI9341_MADCTL_MV | ILI9341_MADCTL_MY |
			    ILI9341_MADCTL_MX;
		break;
	case 90:
		addr_mode = ILI9341_MADCTL_MY;
		break;
	case 180:
		addr_mode = ILI9341_MADCTL_MV;
		break;
	case 270:
		addr_mode = ILI9341_MADCTL_MX;
		break;
	}
	addr_mode |= ILI9341_MADCTL_BGR;
	mipi_dbi_command(mipi, MIPI_DCS_SET_ADDRESS_MODE, addr_mode);

	/* Frame Rate */
	mipi_dbi_command(mipi, ILI9341_FRMCTR1, 0x00, 0x1b);

	/* Gamma */
	mipi_dbi_command(mipi, ILI9341_EN3GAM, 0x08);
	mipi_dbi_command(mipi, MIPI_DCS_SET_GAMMA_CURVE, 0x01);
	mipi_dbi_command(mipi, ILI9341_PGAMCTRL,
		       0x1f, 0x1a, 0x18, 0x0a, 0x0f, 0x06, 0x45, 0x87,
		       0x32, 0x0a, 0x07, 0x02, 0x07, 0x05, 0x00);
	mipi_dbi_command(mipi, ILI9341_NGAMCTRL,
		       0x00, 0x25, 0x27, 0x05, 0x10, 0x09, 0x3a, 0x78,
		       0x4d, 0x05, 0x18, 0x0d, 0x38, 0x3a, 0x1f);

	/* DDRAM */
	mipi_dbi_command(mipi, ILI9341_ETMOD, 0x07);

	/* Display */
	mipi_dbi_command(mipi, ILI9341_DISCTRL, 0x0a, 0x82, 0x27, 0x00);
	mipi_dbi_command(mipi, MIPI_DCS_EXIT_SLEEP_MODE);
	msleep(100);

	mipi_dbi_command(mipi, MIPI_DCS_SET_DISPLAY_ON);
	msleep(100);

	return 0;
}

static int mipi_panel_init_st7586(struct mipi_dbi *mipi)
{
	struct tinydrm_device *tdev = &mipi->tinydrm;
	struct device *dev = tdev->drm->dev;
	u8 addr_mode;
	int ret;

	DRM_DEBUG_KMS("\n");

	ret = regulator_enable(mipi->regulator);
	if (ret) {
		dev_err(dev, "Failed to enable regulator %d\n", ret);
		return ret;
	}

	/* Avoid flicker by skipping setup if the bootloader has done it */
	if (mipi_dbi_display_is_on(mipi))
		return 0;

	mipi_dbi_hw_reset(mipi);
	ret = mipi_dbi_command(mipi, ST7586_AUTO_READ_CTRL, 0x9f);
	if (ret) {
		dev_err(dev, "Error sending command %d\n", ret);
		regulator_disable(mipi->regulator);
		return ret;
	}

	mipi_dbi_command(mipi, ST7586_OTP_RW_CTRL, 0x00);

	msleep(10);

	mipi_dbi_command(mipi, ST7586_OTP_READ);

	msleep(20);

	mipi_dbi_command(mipi, ST7586_OTP_CTRL_OUT);
	mipi_dbi_command(mipi, MIPI_DCS_EXIT_SLEEP_MODE);
	mipi_dbi_command(mipi, MIPI_DCS_SET_DISPLAY_OFF);

	msleep(50);

	mipi_dbi_command(mipi, ST7586_SET_VOP_OFFSET, 0x00);
	mipi_dbi_command(mipi, ST7586_SET_VOP, 0xe3, 0x00);
	mipi_dbi_command(mipi, ST7586_SET_BIAS_SYSTEM, 0x02);
	mipi_dbi_command(mipi, ST7586_SET_BOOST_LEVEL, 0x04);
	mipi_dbi_command(mipi, ST7586_ENABLE_ANALOG, 0x1d);
	mipi_dbi_command(mipi, ST7586_SET_NLINE_INV, 0x00);
	mipi_dbi_command(mipi, ST7586_DISP_MODE_GRAY);
	mipi_dbi_command(mipi, ST7586_ENABLE_DDRAM, 0x02);

	switch (mipi->rotation) {
	default:
		addr_mode = 0x00;
		break;
	case 90:
		addr_mode = ST7586_DISP_CTRL_MY;
		break;
	case 180:
		addr_mode = ST7586_DISP_CTRL_MX | ST7586_DISP_CTRL_MY;
		break;
	case 270:
		addr_mode = ST7586_DISP_CTRL_MX;
		break;
	}
	mipi_dbi_command(mipi, MIPI_DCS_SET_ADDRESS_MODE, addr_mode);

	mipi_dbi_command(mipi, ST7586_SET_DISP_DUTY, 0x7f);
	mipi_dbi_command(mipi, ST7586_SET_PART_DISP, 0xa0);
	mipi_dbi_command(mipi, MIPI_DCS_SET_PARTIAL_AREA, 0x00, 0x00, 0x00, 0x77);
	mipi_dbi_command(mipi, MIPI_DCS_EXIT_INVERT_MODE);

	msleep(100);

	mipi_dbi_command(mipi, MIPI_DCS_SET_DISPLAY_ON);

	return 0;
}

static void mipi_panel_fini(void *data)
{
	struct mipi_dbi *mipi = data;

	DRM_DEBUG_KMS("\n");
	regulator_disable(mipi->regulator);
}

static const struct drm_simple_display_pipe_funcs mipi_panel_pipe_funcs = {
	.enable = mipi_dbi_pipe_enable,
	.disable = mipi_dbi_pipe_disable,
	.update = tinydrm_display_pipe_update,
	.prepare_fb = tinydrm_display_pipe_prepare_fb,
};

static const struct mipi_panel_info mipi_panel_infos[] = {
	[MIPI_PANEL_TYPE_LEGO_EV3_LCD] = {
		.init		= mipi_panel_init_st7586,
		.mode		= { TINYDRM_MODE(178, 128, 37, 27) },
		.pixel_fmt	= MIPI_DCS_PIXEL_FMT_ST7586_332,
	},
	[MIPI_PANEL_TYPE_MULTI_INNO_MI0283QT] = {
		.init		= mipi_panel_init_ili9341,
		.mode		= { TINYDRM_MODE(320, 240, 58, 43) },
		.pixel_fmt	= MIPI_DCS_PIXEL_FMT_16BIT,
	},
};

DEFINE_DRM_GEM_CMA_FOPS(mipi_panel_fops);

static struct drm_driver mipi_panel_driver = {
	.driver_features	= DRIVER_GEM | DRIVER_MODESET | DRIVER_PRIME |
				  DRIVER_ATOMIC,
	.fops			= &mipi_panel_fops,
	TINYDRM_GEM_DRIVER_OPS,
	.lastclose		= tinydrm_lastclose,
	.debugfs_init		= mipi_dbi_debugfs_init,
	.name			= "mipi-panel",
	.desc			= "MIPI DBI compatible panel",
	.date			= "20160614",
	.major			= 1,
	.minor			= 0,
};

static const struct of_device_id mipi_panel_of_match[] = {
	{ .compatible = "lego,ev3-lcd" },
	{ .compatible = "multi-inno,mi0283qt" },
	{},
};
MODULE_DEVICE_TABLE(of, mipi_panel_of_match);

static const struct spi_device_id mipi_panel_id[] = {
	{ "ev3-lcd", MIPI_PANEL_TYPE_LEGO_EV3_LCD },
	{ "mi0283qt", MIPI_PANEL_TYPE_MULTI_INNO_MI0283QT },
	{ },
};
MODULE_DEVICE_TABLE(spi, mipi_panel_id);

static int mipi_panel_probe(struct spi_device *spi)
{
	const struct spi_device_id *id = spi_get_device_id(spi);
	enum mipi_panel_type type = id->driver_data;
	struct device *dev = &spi->dev;
	struct tinydrm_device *tdev;
	struct mipi_dbi *mipi;
	struct gpio_desc *dc;
	u32 rotation = 0;
	int ret;

	if (type == MIPI_PANEL_TYPE_UNKNOWN) {
		dev_err(dev, "Unknown panel type\n");
		return -EINVAL;
	}

	mipi = devm_kzalloc(dev, sizeof(*mipi), GFP_KERNEL);
	if (!mipi)
		return -ENOMEM;

	mipi->init = mipi_panel_infos[type].init;

	mipi->reset = devm_gpiod_get_optional(dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(mipi->reset)) {
		dev_err(dev, "Failed to get gpio 'reset'\n");
		return PTR_ERR(mipi->reset);
	}

	dc = devm_gpiod_get_optional(dev, "dc", GPIOD_OUT_LOW);
	if (IS_ERR(dc)) {
		dev_err(dev, "Failed to get gpio 'dc'\n");
		return PTR_ERR(dc);
	}

	mipi->regulator = devm_regulator_get(dev, "power");
	if (IS_ERR(mipi->regulator))
		return PTR_ERR(mipi->regulator);

	mipi->backlight = tinydrm_of_find_backlight(dev);
	if (IS_ERR(mipi->backlight))
		return PTR_ERR(mipi->backlight);

	device_property_read_u32(dev, "rotation", &rotation);

	ret = mipi_dbi_spi_init(spi, mipi, dc, &mipi_panel_pipe_funcs,
				&mipi_panel_driver, &mipi_panel_infos[type].mode,
				mipi_panel_infos[type].pixel_fmt, rotation);
	if (ret)
		return ret;

	ret = mipi->init(mipi);
	if (ret)
		return ret;

	/* use devres to fini after drm unregister (drv->remove is before) */
	ret = devm_add_action(dev, mipi_panel_fini, mipi);
	if (ret) {
		mipi_panel_fini(mipi);
		return ret;
	}

	tdev = &mipi->tinydrm;

	ret = devm_tinydrm_register(tdev);
	if (ret)
		return ret;

	spi_set_drvdata(spi, mipi);

	DRM_DEBUG_DRIVER("Initialized %s:%s @%uMHz on minor %d\n",
			 tdev->drm->driver->name, dev_name(dev),
			 spi->max_speed_hz / 1000000,
			 tdev->drm->primary->index);

	return 0;
}

static void mipi_panel_shutdown(struct spi_device *spi)
{
	struct mipi_dbi *mipi = spi_get_drvdata(spi);

	tinydrm_shutdown(&mipi->tinydrm);
}

static int __maybe_unused mipi_panel_pm_suspend(struct device *dev)
{
	struct mipi_dbi *mipi = dev_get_drvdata(dev);
	int ret;

	ret = tinydrm_suspend(&mipi->tinydrm);
	if (ret)
		return ret;

	mipi_panel_fini(mipi);

	return 0;
}

static int __maybe_unused mipi_panel_pm_resume(struct device *dev)
{
	struct mipi_dbi *mipi = dev_get_drvdata(dev);
	int ret;

	ret = mipi->init(mipi);
	if (ret)
		return ret;

	return tinydrm_resume(&mipi->tinydrm);
}

static const struct dev_pm_ops mipi_panel_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(mipi_panel_pm_suspend, mipi_panel_pm_resume)
};

static struct spi_driver mipi_panel_spi_driver = {
	.driver = {
		.name = "mipi-panel",
		.owner = THIS_MODULE,
		.of_match_table = mipi_panel_of_match,
		.pm = &mipi_panel_pm_ops,
	},
	.id_table = mipi_panel_id,
	.probe = mipi_panel_probe,
	.shutdown = mipi_panel_shutdown,
};
module_spi_driver(mipi_panel_spi_driver);

MODULE_DESCRIPTION("MIPI DBI panel DRM driver");
MODULE_AUTHOR("Noralf Trønnes");
MODULE_LICENSE("GPL");
