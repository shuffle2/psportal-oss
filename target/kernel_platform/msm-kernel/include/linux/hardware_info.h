/*
 *
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

#ifndef __HARDWARE_INFO_H__
#define __HARDWARE_INFO_H__

#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/types.h>

#define emmc_len  16

typedef struct
{
    int  stage_value;
    char  *pcba_stage_name;
} BOARD_STAGE_TABLE;

typedef struct
{
    int  type_value;
    int  adc_value;
    char  *pcba_type_name;
} BOARD_TYPE_TABLE;

typedef struct
{
    int  value;
    char  *name;
} DDR_INFO_TABLE;

typedef struct
{
    unsigned int  adc_vol;
    char  *revision;
} BOARD_VERSION_TABLE;

typedef struct {
    int stage_value;
    int adc_value;
} SMEM_BOARD_INFO_DATA;

enum hardware_id {
    HWID_NONE = 0x00,
    HWID_DDR = 0x10,
    HWID_EMMC,
    HWID_NAND,
    HWID_FLASH,

    HWID_LCM = 0x20,
    HWID_LCD_BIAS,
    HWID_BACKLIGHT,
    HWID_CTP_DRIVER,
    HWID_CTP_MODULE,
    HWID_CTP_FW_VER,
    HWID_CTP_FW_INFO,
	HWID_CTP_COLOR_INFO,
	HWID_CTP_LOCKDOWN_INFO,

    HWID_MAIN_CAM = 0x30,
    HWID_MAIN_CAM_2,
    HWID_MAIN_CAM_3,
    HWID_SUB_CAM,
    HWID_SUB_CAM_2,
    HWID_MAIN_LENS,
    HWID_MAIN_LENS_2,
    HWID_SUB_LENS,
    HWID_SUB_LENS_2,
    HWID_MAIN_OTP,
    HWID_MAIN_OTP_2,
    HWID_SUB_OTP,
    HWID_SUB_OTP_2,
    HWID_FLASHLIGHT,
    HWID_FLaSHLIGHT_2,

    HWId_HIFI_NAME,

    HWID_GSENSOR = 0x70,
    HWID_ALSPS,
    HWID_GYROSCOPE,
    HWID_MSENSOR,
    HWID_FINGERPRINT,
    HWID_SAR_SENSOR,
    //HWID_SAR_SENSOR_2,
    HWID_IRDA,
    HWID_BAROMETER,
    HWID_PEDOMETER,
    HWID_HUMIDITY,
    HWID_NFC,
    HWID_TEE,

    HWID_BATERY_ID = 0xA0,
    HWID_CHARGER,

    HWID_USB_TYPE_C = 0xE0,

    HWID_SUMMARY = 0xF0,
    HWID_VER,
    HWID_MAIN_CAM_SN,
    HWID_MAIN_CAM_2_SN,
    HWID_MAIN_CAM_3_SN,
    HWID_SUB_CAM_SN,
    HWID_END
};

//Add for camera otp information
struct global_otp_struct {
/*simon modified to show source camera for factory camera mixture in zal1806 start*/
	char *sensor_name;
/*simon modified to show source camera for factory camera mixture in zal1806 end*/
	int otp_valid;
	int vendor_id;
	int module_code;
	int module_ver;
	int sw_ver;
	int year;
	int month;
	int day;
	int vcm_vendorid;
	int vcm_moduleid;
};

typedef struct {
    const char *version;
    const char *lcm;
    const char *ctp_driver;
    const char *ctp_module;
    unsigned char ctp_fw_version[20];
    const char *ctp_fw_info;
	const char *ctp_color_info;
	const char *ctp_lockdown_info;
    const char *main_camera;
    const char *main_camera2;
    const char *main_camera3;
    const char *sub_camera;
    const char * hifi_name;
    const char *alsps;
    const char *gsensor;
    const char *gyroscope;
    const char *msensor;
    const char *fingerprint;
    const char *sar_sensor;
    //const char *sar_sensor_2;
    const char *bat_id;
    const unsigned int *flash;
    const char *nfc;
    const char *main_cam_sn;
    const char *main_cam_2_sn;
    const char *main_cam_3_sn;
    const char *sub_cam_sn;
    //const struct hwinfo_cam_otp *main_otp;
    //const struct hwinfo_cam_otp *sub_otp;
} HARDWARE_INFO;

// ddr detail info start

#define DDRINFO_SMEM_ID 0x25B	//603
#if !defined(MAX_NUM_CLOCK_PLAN)
#define MAX_NUM_CLOCK_PLAN  14
#endif

#if !defined(NUM_CH)
#define NUM_CH 1
#endif

#define DDR_DETAILS_STRUCT_VERSION 0x0000000000040000
#define MAX_IDX 8
/** DDR types. */
typedef enum
{
  DDR_TYPE_LPDDR1 = 0,           /**< Low power DDR1. */
  DDR_TYPE_LPDDR2 = 2,            /**< Low power DDR2  set to 2 for compatibility*/
  DDR_TYPE_PCDDR2 = 3,           /**< Personal computer DDR2. */
  DDR_TYPE_PCDDR3 = 4,           /**< Personal computer DDR3. */

  DDR_TYPE_LPDDR3 = 5,           /**< Low power DDR3. */
  DDR_TYPE_LPDDR4 = 6,           /**< Low power DDR4. */
  DDR_TYPE_LPDDR4X = 7,          /**< Low power DDR4X (with lower IO voltage). */

  DDR_TYPE_RESERVED = 8,         /**< Reserved for future use. */
  DDR_TYPE_UNUSED = 0x7FFFFFFF  /**< For compatibility with deviceprogrammer(features not using DDR). */
} DDR_TYPE;

/** DDR manufacturers. */
typedef enum
{
  RESERVED_0,                        /**< Reserved for future use. */
  SAMSUNG,                           /**< Samsung. */
  QIMONDA,                           /**< Qimonda. */
  ELPIDA,                            /**< Elpida Memory, Inc. */
  ETRON,                             /**< Etron Technology, Inc. */
  NANYA,                             /**< Nanya Technology Corporation. */
  HYNIX,                             /**< Hynix Semiconductor Inc. */
  MOSEL,                             /**< Mosel Vitelic Corporation. */
  WINBOND,                           /**< Winbond Electronics Corp. */
  ESMT,                              /**< Elite Semiconductor Memory Technology Inc. */
  RESERVED_1,                        /**< Reserved for future use. */
  SPANSION,                          /**< Spansion Inc. */
  SST,                               /**< Silicon Storage Technology, Inc. */
  ZMOS,                              /**< ZMOS Technology, Inc. */
  INTEL,                             /**< Intel Corporation. */
  NUMONYX = 254,                     /**< Numonyx, acquired by Micron Technology, Inc. */
  MICRON = 255,                      /**< Micron Technology, Inc. */
  DDR_MANUFACTURES_MAX = 0x7FFFFFFF  /**< Forces the enumerator to 32 bits. */
} DDR_MANUFACTURES;

struct ddr_freq_table{
   u32 freq_khz;
   u8  enable;
};

typedef struct ddr_freq_plan_entry_info
{
  struct ddr_freq_table ddr_freq[MAX_NUM_CLOCK_PLAN];
  u8  num_ddr_freqs;
  u32* clk_period_address;
}ddr_freq_plan_entry;

struct ddr_part_details
{
  u8 revision_id1[2];
  u8 revision_id2[2];
  u8 width[2];
  u8 density[2];
};

typedef struct ddr_details_entry_info
{

  u8   manufacturer_id; /*hynix or micron and so on*/
  u8   device_type;  /*LPDDRX type*/
  struct  ddr_part_details ddr_params[MAX_IDX];
  ddr_freq_plan_entry     ddr_freq_tbl;
  u8   num_channels;
  u8 num_ranks[2]; /* number of ranks per channel */
  u8 hbb[2][2]; /* Highest Bank Bit per rank per channel */

}ddr_details_entry;
// ddr detail info end

void get_hardware_info_data(enum hardware_id id, const void *data);
char* get_type_name(void);
char* get_ddr_type(void);
char* get_ddr_manufacture_name(void);
#endif
