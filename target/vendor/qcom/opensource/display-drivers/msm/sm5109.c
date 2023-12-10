/*
 * Copyright (C) 2021 Siliconmitus Inc.
 * Copyright (c) 2023 Sony Interactive Entertainment Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */
#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>

#define LCD_BIAS_POSCNTL_REG	0x00
#define LCD_BIAS_NEGCNTL_REG	0x01
#define LCD_BIAS_CONTROL_REG	0x03

enum {
	FIRST_VP_AFTER_VN = 0,
	FIRST_VN_AFTER_VP,
};

enum {
	LCD_BIAS_GPIO_STATE_ENP0 = 0,
	LCD_BIAS_GPIO_STATE_ENP1,
	LCD_BIAS_GPIO_STATE_ENN0,
	LCD_BIAS_GPIO_STATE_ENN1,
	LCD_BIAS_GPIO_STATE_MAX,
};

/*****************************************************************************
 * GLobal Variable
 *****************************************************************************/
static struct i2c_client *lcd_bias_i2c_client;
static struct i2c_client *lcd_bright_i2c_client;
int sm_enp;
int sm_enn;
/*****************************************************************************
 * Function Prototype
 *****************************************************************************/
static int lcd_bias_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id);
static int lcd_bias_i2c_remove(struct i2c_client *client);
static void lcd_bias_gpio_select_state(int s);

static int lcd_bright_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id);
static int lcd_bright_i2c_remove(struct i2c_client *client);
/*****************************************************************************
 * Extern Area
 *****************************************************************************/
static void lcd_bias_write_byte(unsigned char addr, unsigned char value)
{
    int ret = 0;
    unsigned char write_data[2] = {0};

    write_data[0] = addr;
    write_data[1] = value;

    if (NULL == lcd_bias_i2c_client) {
        printk("[LCD][BIAS] lcd_bias_i2c_client is null!!\n");
        return ;
    }
    ret = i2c_master_send(lcd_bias_i2c_client, write_data, 2);

    if (ret < 0)
        printk("[LCD][BIAS] i2c write data fail!! ret=%d\n", ret);
}

static void lcd_bias_set_vpn(unsigned int en, unsigned int seq, unsigned int value)
{
    unsigned char level;

    level = (value - 4000) / 100;  //eg.  5.0V= 4.0V + Hex 0x0A (Bin 0 1010) * 100mV

    if (seq == FIRST_VP_AFTER_VN) {
        if (en) {
            lcd_bias_gpio_select_state(LCD_BIAS_GPIO_STATE_ENP1);
            mdelay(1);
            lcd_bias_write_byte(LCD_BIAS_POSCNTL_REG, level);
            mdelay(5);
            lcd_bias_gpio_select_state(LCD_BIAS_GPIO_STATE_ENN1);
            lcd_bias_write_byte(LCD_BIAS_NEGCNTL_REG, level);
        } else {
            lcd_bias_gpio_select_state(LCD_BIAS_GPIO_STATE_ENP0);
            mdelay(5);
            lcd_bias_gpio_select_state(LCD_BIAS_GPIO_STATE_ENN0);
        }
    } else if (seq == FIRST_VN_AFTER_VP) {
        if (en) {
            lcd_bias_gpio_select_state(LCD_BIAS_GPIO_STATE_ENN1);
            mdelay(1);
            lcd_bias_write_byte(LCD_BIAS_NEGCNTL_REG, level);
            mdelay(5);
            lcd_bias_gpio_select_state(LCD_BIAS_GPIO_STATE_ENP1);
            lcd_bias_write_byte(LCD_BIAS_POSCNTL_REG, level);
        } else {
            lcd_bias_gpio_select_state(LCD_BIAS_GPIO_STATE_ENN0);
            mdelay(5);
            lcd_bias_gpio_select_state(LCD_BIAS_GPIO_STATE_ENP0);
        }
    }
}

void lcd_bias_set_vspn(unsigned int en, unsigned int seq, unsigned int value)
{
    if ((value <= 4000) || (value > 6500)) {
        printk("[LCD][BIAS] unreasonable voltage value:%d\n", value);
        return ;
    }

    lcd_bias_set_vpn(en, seq, value);
}

static void lcd_bright_write_byte(unsigned char addr, unsigned char value)
{
    int ret = 0;
    unsigned char write_data[2] = {0};

    write_data[0] = addr;
    write_data[1] = value;

    if (NULL == lcd_bright_i2c_client) {
        printk("[LCD][BRIGHT] lcd_bright_i2c_client is null!!\n");
        return ;
    }
    ret = i2c_master_send(lcd_bright_i2c_client, write_data, 2);

    if (ret < 0)
        printk("[LCD][BRIGHT] i2c write data fail!! ret=%d\n", ret);
}

void lcd_bright_i2c_set(unsigned int value)
{
    unsigned char level_lsb, level_msb;

    level_lsb = value & 0x0f;
    level_msb = (value >> 4) & 0xff;
    lcd_bright_write_byte(0x1a, level_lsb);
    lcd_bright_write_byte(0x19, level_msb);
}

/*****************************************************************************
 * Data Structure
 *****************************************************************************/
static const struct of_device_id i2c_of_match[] = {
    { .compatible = "qualcomm,i2c_lcd_bias", },
    {},
};

static const struct i2c_device_id lcd_bias_i2c_id[] = {
    {"lcd_Bias_I2C", 0},
    {},
};

static struct i2c_driver lcd_bias_i2c_driver = {
    .id_table = lcd_bias_i2c_id,
    .probe = lcd_bias_i2c_probe,
    .remove = lcd_bias_i2c_remove,
    .driver = {
        .name = "lcd_bias_i2c",
        .of_match_table = i2c_of_match,
    },
};


static const struct of_device_id i2c_of_match1[] = {
    { .compatible = "qualcomm,i2c_lcd_bright", },
    {},
};

static const struct i2c_device_id lcd_bright_i2c_id[] = {
    {"lcd_Bright_I2C", 0},
    {},
};

static struct i2c_driver lcd_bright_i2c_driver = {
    .id_table = lcd_bright_i2c_id,
    .probe = lcd_bright_i2c_probe,
    .remove = lcd_bright_i2c_remove,
    .driver = {
        .name = "lcd_bright_i2c",
        .of_match_table = i2c_of_match1,
    },
};
/*****************************************************************************
 * Function
 *****************************************************************************/
static long lcd_bias_set_state(unsigned int s)
{
    int ret = 0;

    switch (s) {
	case LCD_BIAS_GPIO_STATE_ENP0:
		gpio_set_value(sm_enp, 0);
		break;
	case LCD_BIAS_GPIO_STATE_ENP1:
		gpio_set_value(sm_enp, 1);
		break;
	case LCD_BIAS_GPIO_STATE_ENN0:
		gpio_set_value(sm_enn, 0);
		break;
	case LCD_BIAS_GPIO_STATE_ENN1:
		gpio_set_value(sm_enn, 1);
		break;
	default:
		return -EINVAL;
	}

    return ret; /* Good! */
}

void lcd_bias_gpio_select_state(int s)
{
    BUG_ON(!((unsigned int)(s) < (unsigned int)(LCD_BIAS_GPIO_STATE_MAX)));
    lcd_bias_set_state(s);
}

static int lcd_bias_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
    struct device *dev = &client->dev;
    struct device_node *np = dev->of_node;

    if (NULL == client) {
        printk("[LCD][BIAS] i2c_client is NULL\n");
        return -1;
    }

    lcd_bias_i2c_client = client;
    printk("[LCD][BIAS] lcd_bias_i2c_probe success addr = 0x%x\n", client->addr);

    sm_enp = of_get_named_gpio(np, "qualcomm,enp", 0);
    if (!gpio_is_valid(sm_enp)) {
        if (sm_enp != -EPROBE_DEFER) {
            pr_err("%s: invaid GPIO %d\n", __func__, sm_enp);
        } else {
            pr_warn("%s:qualcomm,enp not ready %d\n", __func__, sm_enp);
        }
        return sm_enp;
    }

    sm_enn = of_get_named_gpio(np, "qualcomm,enn", 0);
    if (!gpio_is_valid(sm_enn)) {
        if (sm_enn != -EPROBE_DEFER) {
            pr_err("%s: invaid GPIO %d\n", __func__, sm_enn);
        } else {
            pr_warn("%s:qualcomm,enn not ready %d\n", __func__, sm_enn);
        }
        return sm_enn;
    }
    pr_warn("%s:  sm_enp %d sm_enn  %d\n", __func__, sm_enp, sm_enn);

    return 0;
}

static int lcd_bias_i2c_remove(struct i2c_client *client)
{
    lcd_bias_i2c_client = NULL;
    i2c_unregister_device(client);

    return 0;
}


static int lcd_bright_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
    if (NULL == client) {
        printk("[LCD][BRIGHT] i2c_client is NULL\n");
        return -1;
    }

    lcd_bright_i2c_client = client;
    printk("[LCD][BRIGHT] lcd_bright_i2c_probe success addr = 0x%x\n", client->addr);

    return 0;
}

static int lcd_bright_i2c_remove(struct i2c_client *client)
{
    lcd_bright_i2c_client = NULL;
    i2c_unregister_device(client);

    return 0;
}
int __init lcd_bias_init(void)
{
    if (i2c_add_driver(&lcd_bias_i2c_driver)) {
        printk("[LCD][BIAS] Failed to register lcd_bias_i2c_driver!\n");
        return -1;
    }

    return 0;
}

void __exit lcd_bias_exit(void)
{
    i2c_del_driver(&lcd_bias_i2c_driver);
}


int __init lcd_bright_init(void)
{
    if (i2c_add_driver(&lcd_bright_i2c_driver)) {
        printk("[LCD][BRIGHT] Failed to register lcd_bright_i2c_driver!\n");
        return -1;
    }

    return 0;
}

void __exit lcd_bright_exit(void)
{
    i2c_del_driver(&lcd_bright_i2c_driver);
}

