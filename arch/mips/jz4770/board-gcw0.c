/*
 * board-gcw0.c  -  GCW Zero: JZ4770-based handheld game console
 *
 * File based on Pisces board definition.
 * Copyright (C) 2006-2008, Ingenic Semiconductor Inc.
 * Original author: <jlwei@ingenic.cn>
 *
 * GCW Zero specific changes:
 * Copyright (C) 2012, Maarten ter Huurne <maarten@treewalker.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/init.h>
#include <linux/sched.h>
#include <linux/ioport.h>
#include <linux/mm.h>
#include <linux/console.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/i2c-gpio.h>
#include <linux/input.h>
#include <linux/gpio_keys.h>
#include <linux/leds.h>
#include <linux/platform_device.h>
#include <linux/pwm_backlight.h>

#include <asm/cpu.h>
#include <asm/bootinfo.h>
#include <asm/mipsregs.h>
#include <asm/reboot.h>

#include <linux/mmc/host.h>
#include <linux/act8600_power.h>
#include <linux/platform_data/usb-musb-jz4770.h>
#include <linux/power/gpio-charger.h>
#include <linux/power/jz4770-battery.h>
#include <linux/regulator/fixed.h>
#include <linux/regulator/machine.h>
#include <linux/usb/musb.h>
#include <media/radio-rda5807.h>
#include <sound/jz4770.h>
#include <video/jzpanel.h>
#include <video/panel-nt39016.h>

#include <asm/mach-jz4770/board-gcw0.h>
#include <asm/mach-jz4770/gpio.h>
#include <asm/mach-jz4770/jz4770_fb.h>
#include <asm/mach-jz4770/jz4770i2c.h>
#include <asm/mach-jz4770/jz4770misc.h>
#include <asm/mach-jz4770/mmc.h>
#include <asm/mach-jz4770/platform.h>

#include "clock.h"


/* Video */

#define GPIO_PANEL_SOMETHING	JZ_GPIO_PORTF(0)

static int gcw0_panel_init(void **out_panel,
				     struct device *dev, void *panel_pdata)
{
	int ret;

	ret = nt39016_panel_ops.init(out_panel, dev, panel_pdata);
	if (ret)
		return ret;

	ret = devm_gpio_request(dev, GPIO_PANEL_SOMETHING, "LCD panel unknown");
	if (ret) {
		dev_err(dev,
			"Failed to request LCD panel unknown pin: %d\n", ret);
		return ret;
	}

	gpio_direction_output(GPIO_PANEL_SOMETHING, 1);

	return 0;
}

static void gcw0_panel_exit(void *panel)
{
	nt39016_panel_ops.exit(panel);
}

static void gcw0_panel_enable(void *panel)
{
	act8600_output_enable(6, true);

	nt39016_panel_ops.enable(panel);
}

static void gcw0_panel_disable(void *panel)
{
	nt39016_panel_ops.disable(panel);

	act8600_output_enable(6, false);
}

static struct nt39016_platform_data gcw0_panel_pdata = {
	.gpio_reset		= JZ_GPIO_PORTE(2),
	/* .gpio_clock		= JZ_GPIO_PORTE(15), */
	.gpio_enable		= JZ_GPIO_PORTE(13),
	/* .gpio_data		= JZ_GPIO_PORTE(17), */
};

static struct panel_ops gcw0_panel_ops = {
	.init		= gcw0_panel_init,
	.exit		= gcw0_panel_exit,
	.enable		= gcw0_panel_enable,
	.disable	= gcw0_panel_disable,
};

static struct jzfb_platform_data gcw0_fb_pdata = {
	.panel_ops		= &gcw0_panel_ops,
	.panel_pdata		= &gcw0_panel_pdata,
};


/* Buttons */

static struct gpio_keys_button gcw0_buttons[] = {
	/* D-pad up */ {
		.gpio			= JZ_GPIO_PORTE(21),
		.active_low		= 1,
		.code			= KEY_UP,
		.debounce_interval	= 10,
	},
	/* D-pad down */ {
		.gpio			= JZ_GPIO_PORTE(25),
		.active_low		= 1,
		.code			= KEY_DOWN,
		.debounce_interval	= 10,
	},
	/* D-pad left */ {
		.gpio			= JZ_GPIO_PORTE(23),
		.active_low		= 1,
		.code			= KEY_LEFT,
		.debounce_interval	= 10,
	},
	/* D-pad right */ {
		.gpio			= JZ_GPIO_PORTE(24),
		.active_low		= 1,
		.code			= KEY_RIGHT,
		.debounce_interval	= 10,
	},
	/* A button */ {
		.gpio			= JZ_GPIO_PORTE(29),
		.active_low		= 1,
		.code			= KEY_LEFTCTRL,
		.debounce_interval	= 10,
	},
	/* B button */ {
		.gpio			= JZ_GPIO_PORTE(20),
		.active_low		= 1,
		.code			= KEY_LEFTALT,
		.debounce_interval	= 10,
	},
	/* Top button (labeled Y, should be X) */ {
		.gpio			= JZ_GPIO_PORTE(27),
		.active_low		= 1,
		.code			= KEY_SPACE,
		.debounce_interval	= 10,
	},
	/* Left button (labeled X, should be Y) */ {
		.gpio			= JZ_GPIO_PORTE(28),
		.active_low		= 1,
		.code			= KEY_LEFTSHIFT,
		.debounce_interval	= 10,
	},
	/* Left shoulder button */ {
		.gpio			= JZ_GPIO_PORTE(22),
		.active_low		= 1,
		.code			= KEY_TAB,
		.debounce_interval	= 10,
	},
	/* Right shoulder button */ {
		.gpio			= JZ_GPIO_PORTB(28),
		.active_low		= 1,
		.code			= KEY_BACKSPACE,
		.debounce_interval	= 10,
	},
	/* START button */ {
		.gpio			= JZ_GPIO_PORTB(30),
		.active_low		= 1,
		.code			= KEY_ENTER,
		.debounce_interval	= 10,
	},
	/* SELECT button */ {
		.gpio			= JZ_GPIO_PORTD(18),
		/* This is the only button that is active high,
		 * since it doubles as BOOT_SEL1.
		 */
		.active_low		= 0,
		.code			= KEY_ESC,
		.debounce_interval	= 10,
	},
	/* POWER slider */ {
		.gpio		       = JZ_GPIO_PORTE(26),
		.active_low	       = 1,
		.code		       = KEY_POWER,
		.debounce_interval      = 10,
		.wakeup		       = 1,
	},

};

static struct gpio_keys_platform_data gcw0_gpio_keys_pdata = {
	.buttons = gcw0_buttons,
	.nbuttons = ARRAY_SIZE(gcw0_buttons),
	.rep = 1,
};

static struct platform_device gcw0_gpio_keys_device = {
	.name = "gpio-keys",
	.id = -1,
	.dev = {
		.platform_data = &gcw0_gpio_keys_pdata,
	},
};


/* SD cards */

static struct jz_mmc_platform_data gcw_internal_sd_data = {
	.support_sdio		= 0,
	.ocr_mask		= MMC_VDD_32_33 | MMC_VDD_33_34,
	.bus_width		= 4,
	.gpio_card_detect	= -1,
	.gpio_read_only		= -1,
	.gpio_power		= -1,
	.nonremovable		= 1,
};

static struct jz_mmc_platform_data gcw_external_sd_data = {
	.support_sdio		= 0,
	.ocr_mask		= MMC_VDD_32_33 | MMC_VDD_33_34,
	.bus_width		= 4,
	.gpio_card_detect	= JZ_GPIO_PORTB(2),
	.card_detect_active_low	= 1,
	.gpio_read_only		= -1,
	.gpio_power		= JZ_GPIO_PORTE(9),
	.power_active_low	= 1,
};


/* FM radio receiver */

/* static struct rda5807_platform_data gcw0_rda5807_pdata = { */
/* 	.input_flags		= RDA5807_INPUT_LNA_WC_25 | RDA5807_LNA_PORT_P, */
/* 	.output_flags		= RDA5807_OUTPUT_AUDIO_ANALOG, */
/* }; */


/* Power Management Unit */

static struct act8600_outputs_t act8600_outputs[] = {
	{ 4, 0x57, true  }, /* USB OTG: 5.3V */
	{ 5, 0x31, true  }, /* AVD:     2.5V */
	{ 6, 0x39, false }, /* LCD:     3.3V */
	{ 7, 0x39, true  }, /* generic: 3.3V */
	{ 8, 0x24, true  }, /* generic: 1.8V */
};

static struct act8600_platform_pdata_t act8600_platform_pdata = {
        .outputs = act8600_outputs,
        .nr_outputs = ARRAY_SIZE(act8600_outputs),
};


/* Battery */

static struct jz_battery_platform_data gcw0_battery_pdata = {
	.gpio_charge = -1,
	//.gpio_charge_active_low = 0,
	.info = {
		.name = "battery",
		.technology = POWER_SUPPLY_TECHNOLOGY_LIPO,
		.voltage_max_design = 5700000,
		.voltage_min_design = 4600000,
	},
};


/* Charger */

#define GPIO_DC_CHARGER		JZ_GPIO_PORTF(5)
#define GPIO_USB_CHARGER	JZ_GPIO_PORTB(5)

static char *gcw0_batteries[] = {
	"battery",
};

static struct gpio_charger_platform_data gcw0_dc_charger_pdata = {
	.name = "dc",
	.type = POWER_SUPPLY_TYPE_MAINS,
	.gpio = GPIO_DC_CHARGER,
	.supplied_to = gcw0_batteries,
	.num_supplicants = ARRAY_SIZE(gcw0_batteries),
};

static struct platform_device gcw0_dc_charger_device = {
	.name = "gpio-charger",
	.id = 0,
	.dev = {
		.platform_data = &gcw0_dc_charger_pdata,
	},
};

static struct gpio_charger_platform_data gcw0_usb_charger_pdata = {
	.name = "usb",
	.type = POWER_SUPPLY_TYPE_USB,
	.gpio = GPIO_USB_CHARGER,
	.supplied_to = gcw0_batteries,
	.num_supplicants = ARRAY_SIZE(gcw0_batteries),
};

static struct platform_device gcw0_usb_charger_device = {
	.name = "gpio-charger",
	.id = 1,
	.dev = {
		.platform_data = &gcw0_usb_charger_pdata,
	},
};


/* USB 1.1 Host (OHCI) */

static struct regulator_consumer_supply gcw0_internal_usb_regulator_consumer =
	REGULATOR_SUPPLY("vbus", "jz4770-ohci");

static struct regulator_init_data gcw0_internal_usb_regulator_init_data = {
	.num_consumer_supplies = 1,
	.consumer_supplies = &gcw0_internal_usb_regulator_consumer,
	.constraints = {
		.name = "USB power",
		.min_uV = 3300000,
		.max_uV = 3300000,
		.valid_modes_mask = REGULATOR_MODE_NORMAL,
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
	},
};

static struct fixed_voltage_config gcw0_internal_usb_regulator_data = {
	.supply_name = "USB power",
	.microvolts = 3300000,
	.gpio = JZ_GPIO_PORTF(10),
	.init_data = &gcw0_internal_usb_regulator_init_data,
};

static struct platform_device gcw0_internal_usb_regulator_device = {
	.name = "reg-fixed-voltage",
	.id = -1,
	.dev = {
		.platform_data = &gcw0_internal_usb_regulator_data,
	}
};


/* USB OTG (musb) */

#define GPIO_USB_OTG_ID_PIN	JZ_GPIO_PORTF(18)

static struct jz_otg_board_data gcw0_otg_board_data = {
	.gpio_id_pin = GPIO_USB_OTG_ID_PIN,
	.gpio_id_debounce_ms = 100,
};


/* I2C devices */

/* static struct i2c_board_info gcw0_i2c0_devs[] __initdata = { */
/* 	{ */
/* 		.type		= "radio-rda5807", */
/* 		.addr		= RDA5807_I2C_ADDR, */
/* 		.platform_data	= &gcw0_rda5807_pdata, */
/* 	}, */
/* }; */

static struct i2c_board_info gcw0_i2c1_devs[] __initdata = {
	/* the g-sensor is on this bus, but we don't have a driver for it */
};

static struct i2c_board_info gcw0_i2c2_devs[] __initdata = {
};

static struct i2c_board_info gcw0_i2c3_devs[] __initdata = {
	{
		.type		= ACT8600_NAME,
		.addr		= ACT8600_I2C_ADDR,
		.platform_data	= &act8600_platform_pdata,
	},
};

static struct i2c_board_info gcw0_i2c4_devs[] __initdata = {
	/* the IT6610 is on this bus, but we don't have a driver for it */
};

/* I2C busses */

static struct i2c_jz4770_platform_data gcw0_i2c0_platform_data __initdata = {
	.use_dma		= false,
};

static struct i2c_jz4770_platform_data gcw0_i2c1_platform_data __initdata = {
	.use_dma		= false,
};

static struct i2c_jz4770_platform_data gcw0_i2c2_platform_data __initdata = {
	.use_dma		= false,
};

#if !defined(CONFIG_I2C_JZ4770)

static struct i2c_gpio_platform_data gcw0_i2c0_gpio_data = {
	.sda_pin		= JZ_GPIO_PORTD(30),
	.scl_pin		= JZ_GPIO_PORTD(31),
	.udelay			= 2, /* 250 kHz */
};

static struct platform_device gcw0_i2c0_gpio_device = {
	.name			= "i2c-gpio",
	.id			= 0,
	.dev			= {
		.platform_data = &gcw0_i2c0_gpio_data,
	},
};

static struct i2c_gpio_platform_data gcw0_i2c1_gpio_data = {
	.sda_pin		= JZ_GPIO_PORTE(30),
	.scl_pin		= JZ_GPIO_PORTE(31),
	.udelay			= 2, /* 250 kHz */
};

static struct platform_device gcw0_i2c1_gpio_device = {
	.name			= "i2c-gpio",
	.id			= 1,
	.dev			= {
		.platform_data = &gcw0_i2c1_gpio_data,
	},
};

static struct i2c_gpio_platform_data gcw0_i2c2_gpio_data = {
	.sda_pin		= JZ_GPIO_PORTF(16),
	.scl_pin		= JZ_GPIO_PORTF(17),
	.udelay			= 2, /* 250 kHz */
};

static struct platform_device gcw0_i2c2_gpio_device = {
	.name			= "i2c-gpio",
	.id			= 2,
	.dev			= {
		.platform_data = &gcw0_i2c2_gpio_data,
	},
};

#endif

static struct i2c_gpio_platform_data gcw0_i2c3_gpio_data = {
	.sda_pin		= JZ_GPIO_PORTD(5),
	.scl_pin		= JZ_GPIO_PORTD(4),
	.udelay			= 2, /* 250 kHz */
};

static struct platform_device gcw0_i2c3_gpio_device = {
	.name			= "i2c-gpio",
	.id			= 3,
	.dev			= {
		.platform_data = &gcw0_i2c3_gpio_data,
	},
};

static struct i2c_gpio_platform_data gcw0_i2c4_gpio_data = {
	.sda_pin		= JZ_GPIO_PORTD(6),
	.scl_pin		= JZ_GPIO_PORTD(7),
	.udelay			= 5, /* 100 kHz */
};

static struct platform_device gcw0_i2c4_gpio_device = {
	.name			= "i2c-gpio",
	.id			= 4,
	.dev			= {
		.platform_data = &gcw0_i2c4_gpio_data,
	},
};


/* LCD backlight */

static struct platform_pwm_backlight_data gcw0_backlight_pdata = {
	.pwm_id = 1,
	.max_brightness = 255,
	.dft_brightness = 145,
	.pwm_period_ns = 40000, /* 25 kHz: outside human hearing range */
};

static struct platform_device gcw0_backlight_device = {
	.name = "pwm-backlight",
	.id = -1,
	.dev = {
		.platform_data = &gcw0_backlight_pdata,
	},
};


/* Audio */

static struct jz4770_icdc_platform_data gcw0_icdc_pdata = {
	.mic_mode = JZ4770_MIC_1,
};

static struct platform_device gcw0_audio_device = {
	.name = "gcw0-audio",
	.id = -1,
};


struct jz_clk_board_data jz_clk_bdata = {
	/* These two are fixed in hardware. */
	.ext_rate	=   12000000,
	.rtc_rate	=      32768,
	/*
	 * Pick 432 MHz as it is the least common multiple of 27 MHz (required
	 * by TV encoder) and 48 MHz (required by USB host).
	 */
	.pll1_rate	=  432000000,
};

/* Power LED */

/* static struct gpio_led gcw0_leds[] = { */
/* 	{ */
/* 		.name = "power", */
/* 		.gpio = JZ_GPIO_PORTB(30), */
/* 		.active_low = 1, */
/* 		.default_state = LEDS_GPIO_DEFSTATE_ON, */
/* 	}, */
/* }; */

/* static struct gpio_led_platform_data gcw0_led_pdata = { */
/* 	.leds = gcw0_leds, */
/* 	.num_leds = ARRAY_SIZE(gcw0_leds), */
/* }; */

/* static struct platform_device gcw0_led_device = { */
/* 	.name = "leds-gpio", */
/* 	.id = -1, */
/* 	.dev = { */
/* 		.platform_data = &gcw0_led_pdata, */
/* 	}, */
/* }; */


/* Device registration */

static struct platform_device *jz_platform_devices[] __initdata = {
	&gcw0_internal_usb_regulator_device,
	&jz4770_usb_ohci_device,
	&jz4770_usb_otg_xceiv_device,
	&jz4770_usb_otg_device,
	&jz4770_lcd_device,
	&jz4770_i2s_device,
	&jz4770_pcm_device,
	&jz4770_icdc_device,
#if defined(CONFIG_I2C_JZ4770)
	&jz4770_i2c0_device,
	&jz4770_i2c1_device,
	&jz4770_i2c2_device,
#else
	&gcw0_i2c0_gpio_device,
	&gcw0_i2c1_gpio_device,
	&gcw0_i2c2_gpio_device,
#endif
	&gcw0_i2c3_gpio_device,
	&gcw0_i2c4_gpio_device,
	&jz4770_pwm_device,
	&jz4770_adc_device,
	&jz4770_rtc_device,
	&gcw0_gpio_keys_device,
	&gcw0_backlight_device,
	&gcw0_audio_device,
	&jz4770_msc0_device,
	&jz4770_msc1_device,
	/* &gcw0_led_device, */
	&gcw0_dc_charger_device,
	&gcw0_usb_charger_device,
	&jz4770_vpu_device,
};

static int __init gcw0_init_platform_devices(void)
{
	struct musb_hdrc_platform_data *otg_platform_data =
			jz4770_usb_otg_device.dev.platform_data;
	otg_platform_data->board_data = &gcw0_otg_board_data;

	jz4770_lcd_device.dev.platform_data = &gcw0_fb_pdata;
	jz4770_adc_device.dev.platform_data = &gcw0_battery_pdata;
	jz4770_msc0_device.dev.platform_data = &gcw_internal_sd_data;
	jz4770_msc1_device.dev.platform_data = &gcw_external_sd_data;
	jz4770_icdc_device.dev.platform_data = &gcw0_icdc_pdata;

	return platform_add_devices(jz_platform_devices,
				    ARRAY_SIZE(jz_platform_devices));
}

static void __init board_i2c_init(void)
{
	jz4770_i2c0_device.dev.platform_data = &gcw0_i2c0_platform_data;
	jz4770_i2c1_device.dev.platform_data = &gcw0_i2c1_platform_data;
	jz4770_i2c2_device.dev.platform_data = &gcw0_i2c2_platform_data;

	/* i2c_register_board_info(0, gcw0_i2c0_devs, ARRAY_SIZE(gcw0_i2c0_devs)); */
	i2c_register_board_info(1, gcw0_i2c1_devs, ARRAY_SIZE(gcw0_i2c1_devs));
	i2c_register_board_info(2, gcw0_i2c2_devs, ARRAY_SIZE(gcw0_i2c2_devs));
	i2c_register_board_info(3, gcw0_i2c3_devs, ARRAY_SIZE(gcw0_i2c3_devs));
	i2c_register_board_info(4, gcw0_i2c4_devs, ARRAY_SIZE(gcw0_i2c4_devs));
}

static void __init board_gpio_setup(void)
{
	/* SELECT button */
	jz_gpio_disable_pullup(JZ_GPIO_PORTD(18));

	/* DC power source present (high active) */
	jz_gpio_disable_pullup(GPIO_DC_CHARGER);

	/* USB power source present (high active) */
	jz_gpio_disable_pullup(GPIO_USB_CHARGER);
}

static int __init gcw0_board_setup(void)
{
	printk(KERN_INFO "GCW Zero JZ4770 setup\n");

	board_gpio_setup();
	board_i2c_init();

	if (gcw0_init_platform_devices())
		panic("Failed to initialize platform devices");

	return 0;
}

arch_initcall(gcw0_board_setup);
