/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2023 Sony Interactive Entertainment Inc.
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 */

#ifndef __SM5109_H__
#define __SM5109_H__

void lcd_bias_set_vspn(unsigned int en, unsigned int seq, unsigned int value);
void lcd_bright_i2c_set(unsigned int value);
void __exit lcd_bias_exit(void);
int __init lcd_bias_init(void);
void __exit lcd_bright_exit(void);
int __init lcd_bright_init(void);

#endif /* __SM5109_H__ */
