/*
 * Single Wire driver for Diodes AL3050 Backlight driver chip
 *
 * Copyright (C) 2020 SHPI GmbH
 * Author: Lutz Harder  <harder.lutz@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * datasheet: https://www.diodes.com/assets/Datasheets/AL3050.pdf
 *
 */

#include <linux/backlight.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/fb.h>
#include <linux/gpio.h>
#include <linux/gpio/consumer.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/property.h>
#include <linux/slab.h>

#define ADDRESS_AL3050 0x5800
#define T_DELAY_NS 100000
#define T_DETECTION_NS 450000
#define T_START_NS 4000
#define T_EOS_NS 4000
#define T_RESET_MS 4
#define T_LOGIC_1_NS 5000
#define T_LOGIC_0_NS 15000
#define T_LOGIC_NS T_LOGIC_1_NS + T_LOGIC_0_NS
#define BRIGHTNESS_MAX 31
#define BRIGHTNESS_BMASK 0x1f
#define RFA_BMASK 0x80
#define RFA_ACK_WAIT 3500
#define RFA_MAX_ACK_TIME 900000

struct al3050_platform_data {

  struct device *fbdev;
};

struct al3050_bl_data {
  struct device *fbdev;
  struct gpio_desc *gpiod, *gpiock, *gpiomosi, *gpiocs;
  int last_brightness;
  int power;
  int rfa_en;
};

static void al3050_init(struct backlight_device *bl) {
  struct al3050_bl_data *pchip = bl_get_data(bl);
  unsigned long flags;
  local_irq_save(flags);
  gpiod_direction_output(pchip->gpiod, 0);
  mdelay(T_RESET_MS);
  gpiod_direction_output(pchip->gpiod, 1);
  ndelay(T_DELAY_NS);
  gpiod_direction_output(pchip->gpiod, 0);
  ndelay(T_DETECTION_NS);
  gpiod_direction_output(pchip->gpiod, 1);
  local_irq_restore(flags);
}

static void al3050_write_lcd_value(struct al3050_bl_data *pchip,
                                   uint16_t data) {

  uint8_t count = 9;

  gpiod_direction_output(pchip->gpiocs, 0);
  do {
    gpiod_direction_output(pchip->gpiomosi,
                           (((data & (1 << (count - 1))) != 0) << 2));
    gpiod_direction_output(pchip->gpiock, 0);
    ndelay(T_START_NS);
    gpiod_direction_output(pchip->gpiock, 1);
    ndelay(T_START_NS);
    count--;

  } while (count);

  gpiod_direction_output(pchip->gpiomosi, 0);
  gpiod_direction_output(pchip->gpiocs, 1);
  ndelay(T_START_NS);
}

static int al3050_backlight_set_value(struct backlight_device *bl) {
  struct al3050_bl_data *pchip = bl_get_data(bl);
  unsigned int data, max_bmask, addr_bmask;
  unsigned long flags;

  max_bmask = 0x1 << 16;
  /* command size */
  addr_bmask = max_bmask >> 8;

  data = ADDRESS_AL3050 | (bl->props.brightness & BRIGHTNESS_BMASK);

  if (pchip->rfa_en)
    data |= RFA_BMASK;

  /* t_start : 4us high before data byte */
  local_irq_save(flags);
  gpiod_direction_output(pchip->gpiod, 1);
  ndelay(T_START_NS);

  for (max_bmask >>= 1; max_bmask > 0x0; max_bmask >>= 1) {
    int t_low;
    if (data & max_bmask)
      t_low = T_LOGIC_1_NS;
    else
      t_low = T_LOGIC_0_NS;

    gpiod_direction_output(pchip->gpiod, 0);
    ndelay(t_low);
    gpiod_direction_output(pchip->gpiod, 1);
    ndelay(T_LOGIC_NS - t_low);

    if (max_bmask == addr_bmask) {
      gpiod_direction_output(pchip->gpiod, 0);
      /* t_eos : low after address byte */
      ndelay(T_EOS_NS);
      gpiod_direction_output(pchip->gpiod, 1);
      /* t_start : high before data byte */
      ndelay(T_START_NS);
    }
  }
  gpiod_direction_output(pchip->gpiod, 0);

  /* t_eos : 4us low after address byte */

  ndelay(T_EOS_NS);

  if (pchip->rfa_en) {
    int max_ack_time = RFA_MAX_ACK_TIME;
    gpiod_direction_input(pchip->gpiod);
    /* read acknowledge from chip */
    while (max_ack_time > 0) {
      if (gpiod_get_value(pchip->gpiod) == 0)
        break;
      max_ack_time -= RFA_ACK_WAIT;
    }
    if (max_ack_time <= 0) {
      printk(KERN_ERR "AL3050 no ack, reinit.");
      al3050_init(bl);
    } else
      ndelay(max_ack_time);
  }
  gpiod_direction_output(pchip->gpiod, 1);
  local_irq_restore(flags);

  pchip->last_brightness = bl->props.brightness;

  return 0;
}

static int al3050_backlight_update_status(struct backlight_device *bl) {
  struct al3050_bl_data *pchip = bl_get_data(bl);

  if (bl->props.power != FB_BLANK_UNBLANK ||
      bl->props.state & (BL_CORE_SUSPENDED | BL_CORE_FBBLANK)) {

    gpiod_direction_output(pchip->gpiod, 0);
    bl->props.brightness = 0;
    pchip->power = 1;
    return 0;
  }

  else {

    if (pchip->power == 1) {
      printk(KERN_ERR "AL3050 init.");
      al3050_init(bl);
      pchip->power = bl->props.power;
      bl->props.brightness = pchip->last_brightness;
      al3050_backlight_set_value(bl);
      return 0;
    } else {
      return al3050_backlight_set_value(bl);
    }
  }
}

/*
static int al3050_backlight_check_fb(struct backlight_device *bl,
                                   struct fb_info *info)
{
        struct al3050_bl_data *pchip = bl_get_data(bl);

        return pchip->fbdev == NULL || pchip->fbdev == info->dev;
}
*/

static const struct backlight_ops al3050_bl_ops = {
    .options = BL_CORE_SUSPENDRESUME,
    .update_status = al3050_backlight_update_status,
    //.check_fb	= al3050_backlight_check_fb,
};

static struct of_device_id al3050_backlight_of_match[] = {
    {.compatible = "hypertouch40_bl"}, {}};

MODULE_DEVICE_TABLE(of, al3050_backlight_of_match);

static int al3050_backlight_probe(struct platform_device *pdev)

{
  struct device *dev = &pdev->dev;
  struct device_node *node = dev->of_node;
  struct al3050_bl_data *pchip;
  struct backlight_properties props;
  struct al3050_platform_data *pdata = dev_get_platdata(dev);
  struct backlight_device *bl;
  int ret, x;
  u32 value;
  uint16_t data_lcd[] = {
      0x0FF, 0x1FF, 0x198, 0x106, 0x104, 0x101, 0x008,  0x110, 0x021, 0x10D,
      0x030, 0x102, 0x031, 0x100, 0x040, 0x110, 0x041,  0x155, 0x042, 0x102,
      0x043, 0x184, 0x044, 0x184, 0x050, 0x178, 0x051,  0x178, 0x052, 0x100,
      0x053, 0x177, 0x057, 0x160, 0x060, 0x107, 0x061,  0x100, 0x062, 0x108,
      0x063, 0x100, 0x0A0, 0x100, 0x0A1, 0x107, 0x0A2,  0x10C, 0x0A3, 0x10B,
      0x0A4, 0x103, 0x0A5, 0x107, 0x0A6, 0x106, 0x0A7,  0x104, 0x0A8, 0x108,
      0x0A9, 0x10C, 0x0AA, 0x113, 0x0AB, 0x106, 0x0AC,  0x10D, 0x0AD, 0x119,
      0x0AE, 0x110, 0x0AF, 0x100, 0x0C0, 0x100, 0x0C1,  0x107, 0x0C2, 0x10C,
      0x0C3, 0x10B, 0x0C4, 0x103, 0x0C5, 0x107, 0x0C6,  0x107, 0x0C7, 0x104,
      0x0C8, 0x108, 0x0C9, 0x10C, 0x0CA, 0x113, 0x0CB,  0x106, 0x0CC, 0x10D,
      0x0CD, 0x118, 0x0CE, 0x110, 0x0CF, 0x100, 0x0FF,  0x1FF, 0x198, 0x106,
      0x104, 0x106, 0x000, 0x120, 0x001, 0x10A, 0x002,  0x100, 0x003, 0x100,
      0x004, 0x101, 0x005, 0x101, 0x006, 0x198, 0x007,  0x106, 0x008, 0x101,
      0x009, 0x180, 0x00A, 0x100, 0x00B, 0x100, 0x00C,  0x101, 0x00D, 0x101,
      0x00E, 0x100, 0x00F, 0x100, 0x010, 0x1F0, 0x011,  0x1F4, 0x012, 0x101,
      0x013, 0x100, 0x014, 0x100, 0x015, 0x1C0, 0x016,  0x108, 0x017, 0x100,
      0x018, 0x100, 0x019, 0x100, 0x01A, 0x100, 0x01B,  0x100, 0x01C, 0x100,
      0x01D, 0x100, 0x020, 0x101, 0x021, 0x123, 0x022,  0x145, 0x023, 0x167,
      0x024, 0x101, 0x025, 0x123, 0x026, 0x145, 0x027,  0x167, 0x030, 0x111,
      0x031, 0x111, 0x032, 0x100, 0x033, 0x1EE, 0x034,  0x1FF, 0x035, 0x1BB,
      0x036, 0x1AA, 0x037, 0x1DD, 0x038, 0x1CC, 0x039,  0x166, 0x03A, 0x177,
      0x03B, 0x122, 0x03C, 0x122, 0x03D, 0x122, 0x03E,  0x122, 0x03F, 0x122,
      0x040, 0x122, 0x052, 0x110, 0x053, 0x110, 0x054,  0x113, 0x0FF, 0x1FF,
      0x198, 0x106, 0x104, 0x107, 0x018, 0x11D, 0x017,  0x122, 0x002, 0x177,
      0x026, 0x1B2, 0x0E1, 0x179, 0x0FF, 0x1FF, 0x198,  0x106, 0x104, 0x100,
      0x03A, 0x160, 0x035, 0x100, 0x011, 0x100, 0xffff, 0x029, 0x100,
  };

  pchip = devm_kzalloc(dev, sizeof(*pchip), GFP_KERNEL);

  if (pchip == NULL)
    return -ENOMEM;

  if (pdata)
    pchip->fbdev = pdata->fbdev;

  pchip->gpiod = devm_gpiod_get(dev, "bl", GPIOD_ASIS);
  if (IS_ERR(pchip->gpiod)) {
    ret = PTR_ERR(pchip->gpiod);
    if (ret != -EPROBE_DEFER)
      dev_err(dev, "Error: The bl-gpios parameter is missing or invalid.\n");
    return ret;
  }

  ret = of_property_read_u32(node, "rfa_en", &value);
  if (ret < 0)
    pchip->rfa_en = 0;
  pchip->rfa_en = value;

  memset(&props, 0, sizeof(props));
  props.brightness = BRIGHTNESS_MAX;
  props.max_brightness = BRIGHTNESS_MAX;
  props.type = BACKLIGHT_RAW;
  pchip->power = 0;
  bl = devm_backlight_device_register(dev, dev_name(dev), dev, pchip,
                                      &al3050_bl_ops, &props);

  if (IS_ERR(bl))
    return PTR_ERR(bl);

  pchip->gpiock = devm_gpiod_get(dev, "clk", GPIOD_ASIS);

  if (IS_ERR(pchip->gpiock)) {
    ret = PTR_ERR(pchip->gpiock);
    if (ret != -EPROBE_DEFER)
      dev_err(dev, "Error: The clk-gpios parameter is missing or invalid.\n");
    return ret;
  }

  pchip->gpiomosi = devm_gpiod_get(dev, "mosi", GPIOD_ASIS);

  if (IS_ERR(pchip->gpiomosi)) {
    ret = PTR_ERR(pchip->gpiomosi);
    if (ret != -EPROBE_DEFER)
      dev_err(dev, "Error: The mosi-gpios parameter is missing or invalid.\n");
    return ret;
  }

  pchip->gpiocs = devm_gpiod_get(dev, "cs", GPIOD_ASIS);

  if (IS_ERR(pchip->gpiocs)) {
    ret = PTR_ERR(pchip->gpiocs);
    if (ret != -EPROBE_DEFER)
      dev_err(dev, "Error: The cs-gpios parameter is missing or invalid.\n");
    return ret;
  }

  if (!IS_ERR(pchip->gpiock))

  {
    gpiod_direction_output(pchip->gpiocs, 1);
    gpiod_direction_output(pchip->gpiock, 1);
    gpiod_direction_output(pchip->gpiomosi, 0);
    mdelay(100);

    for (x = 0; x < sizeof(data_lcd) / sizeof(uint16_t); x++) {

      if (data_lcd[x] == 0xffff) {
        mdelay(100);
      } else {
        al3050_write_lcd_value(pchip, data_lcd[x]);
      }
    }

    gpiod_direction_input(pchip->gpiock);

    // gpio_unexport(pchip->gpiock);
    // gpio_free(pchip->gpiock);

  } else {
    dev_err(dev, "Error: GPIO setup for display init failed.\n");
  }

  platform_set_drvdata(pdev, bl);

  /* Single Wire init *
   * Single Wire Detection Window
   *   - detect delay : 100us
   *   - detect time  : 450us
   */

  al3050_init(bl);

  dev_info(dev, "AL3050 backlight is initialized\n");

  return 0;
}

static struct platform_driver al3050_backlight_driver = {
    .driver =
        {
            .name = "al3050_bl",
            .of_match_table = al3050_backlight_of_match,
        },
    .probe = al3050_backlight_probe,
};

module_platform_driver(al3050_backlight_driver);

MODULE_DESCRIPTION("Single Wire AL3050 Backlight Driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:al3050_bl");
