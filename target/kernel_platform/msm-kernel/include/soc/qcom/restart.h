/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2011-2014, 2016, 2018, 2020 The Linux Foundation. All rights reserved.
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 * Copyright (c) 2023 Sony Interactive Entertainment Inc.
 */

#ifndef _SOC_QCOM_MSM_RESTART_H_
#define _SOC_QCOM_MSM_RESTART_H_

#define RESTART_NORMAL 0x0
#define RESTART_DLOAD  0x1

void msm_set_restart_mode(int mode);
extern int pmic_reset_irq;

void msm_enable_dump_mode(bool enable);

#endif
