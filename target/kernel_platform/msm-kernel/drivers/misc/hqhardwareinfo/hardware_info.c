/* Huaqin  Inc. (C) 2011. All rights reserved.
 *
 * BY OPENING THIS FILE, RECEIVER HEREBY UNEQUIVOCALLY ACKNOWLEDGES AND AGREES
 * THAT THE SOFTWARE/FIRMWARE AND ITS DOCUMENTATIONS ("HUAQIN SOFTWARE")
 * RECEIVED FROM HUAQIN AND/OR ITS REPRESENTATIVES ARE PROVIDED TO RECEIVER ON
 * AN "AS-IS" BASIS ONLY. HUAQIN EXPRESSLY DISCLAIMS ANY AND ALL WARRANTIES,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE OR NONINFRINGEMENT.
 * NEITHER DOES HUAQIN PROVIDE ANY WARRANTY WHATSOEVER WITH RESPECT TO THE
 * SOFTWARE OF ANY THIRD PARTY WHICH MAY BE USED BY, INCORPORATED IN, OR
 * SUPPLIED WITH THE HUAQIN SOFTWARE, AND RECEIVER AGREES TO LOOK ONLY TO SUCH
 * THIRD PARTY FOR ANY WARRANTY CLAIM RELATING THERETO. RECEIVER EXPRESSLY ACKNOWLEDGES
 * THAT IT IS RECEIVER'S SOLE RESPONSIBILITY TO OBTAIN FROM ANY THIRD PARTY ALL PROPER LICENSES
 * CONTAINED IN HUAQIN SOFTWARE. HUAQIN SHALL ALSO NOT BE RESPONSIBLE FOR ANY HUAQIN
 * SOFTWARE RELEASES MADE TO RECEIVER'S SPECIFICATION OR TO CONFORM TO A PARTICULAR
 * STANDARD OR OPEN FORUM. RECEIVER'S SOLE AND EXCLUSIVE REMEDY AND HUAQIN'S ENTIRE AND
 * CUMULATIVE LIABILITY WITH RESPECT TO THE HUAQIN SOFTWARE RELEASED HEREUNDER WILL BE,
 * AT HUAQIN'S OPTION, TO REVISE OR REPLACE THE HUAQIN SOFTWARE AT ISSUE,
 * OR REFUND ANY SOFTWARE LICENSE FEES OR SERVICE CHARGE PAID BY RECEIVER TO
 * HUAQIN FOR SUCH HUAQIN SOFTWARE AT ISSUE.
 *
 */

/*******************************************************************************
* Dependency
*******************************************************************************/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/hardware_info.h>
#include <linux/regulator/consumer.h>

#include <linux/mm.h>
#include <linux/genhd.h>

#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/soc/qcom/smem.h>
#include <linux/rwsem.h>
//liukangping@huaqin.com add hardinfo sensor begin
#if defined(CONFIG_NANOHUB) && defined(CONFIG_CUSTOM_KERNEL_SENSORHUB)
#include "../mediatek/sensors-1.0/sensorHub/inc_v1/SCP_sensorHub.h"
#include "../mediatek/sensors-1.0/hwmon/include/hwmsensor.h"
#endif

//liukangping@huaqin.com add hardinfo sensor end
ddr_details_entry   *g_ddr_details = NULL;

int sim1_card_slot = -1;
int sim2_card_slot = -1;
#define HARDWARE_INFO_VERSION   "SM6225"
#define HARDWARE_INFO_WCN       "WCN3998"
#define PROJECT_NAME            "6329A"
/******************************************************************************
 * EMMC Configuration
*******************************************************************************/
#define DDR_TYPE                "LPDDR4"

/******************************************************************************
 * HW_ID Configuration
*******************************************************************************/
// static int current_stage_value = 0;
// static BOARD_STAGE_TABLE pcba_stage_table[] =
// {
//     { .stage_value = 0,  .pcba_stage_name = "0", },
//     { .stage_value = 1,  .pcba_stage_name = "A", },
//     { .stage_value = 2,  .pcba_stage_name = "B", },
//     { .stage_value = 3,  .pcba_stage_name = "C", },
//     { .stage_value = 4,  .pcba_stage_name = "D", },
//     { .stage_value = 5,  .pcba_stage_name = "1.0", },
//     { .stage_value = 6,  .pcba_stage_name = "1", },
//     { .stage_value = -1, .pcba_stage_name = "UNKNOWN", },
// };

static int current_type_value = 0;

static BOARD_TYPE_TABLE pcba_type_table[] =
{
    { .type_value = 0,  .pcba_type_name = "EVT0", },
    { .type_value = 1,  .pcba_type_name = "EVT1", },
    { .type_value = 2,  .pcba_type_name = "EVT2", },
    { .type_value = 3,  .pcba_type_name = "DVT1", },
    { .type_value = 4,  .pcba_type_name = "DVT2", },
    { .type_value = 8,  .pcba_type_name = "DVT2-2", },
    { .type_value = 5,  .pcba_type_name = "PVT-B", },
    { .type_value = 9,  .pcba_type_name = "PVT-A", },
    { .type_value = 6,  .pcba_type_name = "MP-B", },
    { .type_value = 10,  .pcba_type_name = "MP-A", },
    { .type_value = 7, .pcba_type_name = "UNKNOWN", },
};

static DDR_INFO_TABLE  ddr_type_table[] =
{
    { .value = DDR_TYPE_LPDDR1,                    .name = "LPDDR1", },
    { .value = DDR_TYPE_LPDDR2,                    .name = "LPDDR2", },
    { .value = DDR_TYPE_PCDDR3,                    .name = "PCDDR3", },
    { .value = DDR_TYPE_LPDDR3,                    .name = "LPDDR3", },
    { .value = DDR_TYPE_LPDDR4,                    .name = "LPDDR4", },
    { .value = DDR_TYPE_LPDDR4X,                   .name = "LPDDR4X", },
    { .value = DDR_TYPE_RESERVED,                  .name = "UNKONWN", },
};

static DDR_INFO_TABLE  ddr_manufacture_table[] =
{
    { .value = RESERVED_0,              .name = "RESERVED_0", },
    { .value = SAMSUNG,                 .name = "SAMSUNG", },
    { .value = QIMONDA,                 .name = "QIMONDA", },
    { .value = ELPIDA,                  .name = "ELPIDA", },
    { .value = ETRON,                   .name = "ETRON", },
    { .value = NANYA,                   .name = "NANYA", },
    { .value = HYNIX,                   .name = "HYNIX", },
    { .value = MOSEL,                   .name = "MOSEL", },
    { .value = WINBOND,                 .name = "WINBOND", },
    { .value = ESMT,                    .name = "ESMT", },
    { .value = RESERVED_1,              .name = "RESERVED_1", },
    { .value = SPANSION,                .name = "SPANSION", },
    { .value = SST,                     .name = "SST", },
    { .value = ZMOS,                    .name = "ZMOS", },
    { .value = INTEL,                   .name = "INTEL", },
    { .value = NUMONYX,                 .name = "NUMONYX", },
    { .value = MICRON,                  .name = "MICRON", },
};

//static struct delayed_work psSetSensorConf_work;

/******************************************************************************
 * Hardware Info Driver
*************************`*****************************************************/
struct global_otp_struct hw_info_main_otp;
struct global_otp_struct hw_info_main2_otp;
struct global_otp_struct hw_info_main3_otp;
struct global_otp_struct hw_info_sub_otp;
static HARDWARE_INFO hwinfo_data;
const char * pcb_name = NULL;

/*
void do_psSetSensorConf_work(struct work_struct *work)
{
    printk("%s: current_stage_value = %d\n",__func__,current_stage_value);
    sensor_set_cmd_to_hub(ID_PROXIMITY, CUST_ACTION_SET_SENSOR_CONF, &current_stage_value);
}
*/

void get_hardware_info_data(enum hardware_id id, const void *data)
{
    if (NULL == data) {
        printk("[HWINFO] %s the data of hwid %d is NULL\n", __func__, id);
    } else {
        switch (id) {
        case HWID_LCM:
            hwinfo_data.lcm = data;
            break;
        case HWID_CTP_DRIVER:
            hwinfo_data.ctp_driver = data;
            break;
        case HWID_CTP_MODULE:
            hwinfo_data.ctp_module = data;
            break;
        case HWID_CTP_FW_VER:
            strcpy(hwinfo_data.ctp_fw_version,data);
            break;
        case HWID_CTP_COLOR_INFO:
            hwinfo_data.ctp_color_info = data;
            break;
        case HWID_CTP_LOCKDOWN_INFO:
            hwinfo_data.ctp_lockdown_info = data;
            break;
        case HWID_CTP_FW_INFO:
            hwinfo_data.ctp_fw_info = data;
            break;
        case HWID_MAIN_CAM:
            hwinfo_data.main_camera = data;
            break;
		case HWID_MAIN_CAM_2:
			hwinfo_data.main_camera2 = data;
			break;
		case HWID_MAIN_CAM_3:
			hwinfo_data.main_camera3 = data;
			break;
        case HWID_SUB_CAM:
            hwinfo_data.sub_camera = data;
            break;
        case HWId_HIFI_NAME:
            hwinfo_data.hifi_name= data;
            break;
        case HWID_FLASH:
            hwinfo_data.flash = data;
            break;
        case HWID_ALSPS:
            hwinfo_data.alsps = data;
            break;
        case HWID_GSENSOR:
            hwinfo_data.gsensor = data;
            break;
        case HWID_GYROSCOPE:
            hwinfo_data.gyroscope = data;
            break;
        case HWID_MSENSOR:
            hwinfo_data.msensor = data;
            break;
        case HWID_SAR_SENSOR:
            hwinfo_data.sar_sensor = data;
            break;
        // case HWID_SAR_SENSOR_2:
        //     hwinfo_data.sar_sensor_2 = data;
        //     break;
        case HWID_BATERY_ID:
            hwinfo_data.bat_id = data;
            break;
        case HWID_NFC:
            hwinfo_data.nfc = data;
            break;
        case HWID_FINGERPRINT:
            hwinfo_data.fingerprint = data;
            break;
        case HWID_MAIN_CAM_SN:
            hwinfo_data.main_cam_sn = data;
            break;
        case HWID_MAIN_CAM_2_SN:
            hwinfo_data.main_cam_2_sn = data;
            break;
        case HWID_MAIN_CAM_3_SN:
            hwinfo_data.main_cam_3_sn = data;
            break;
        case HWID_SUB_CAM_SN:
            hwinfo_data.sub_cam_sn = data;
            break;
        default:
            printk("[HWINFO] %s Invalid HWID\n", __func__);
            break;
        }
    }
}

static ssize_t show_lcm(struct device *dev, struct device_attribute *attr, char *buf)
{
    if (NULL != hwinfo_data.lcm) {
        return sprintf(buf, "lcd name :%s\n", hwinfo_data.lcm);
    } else {
       return sprintf(buf, "lcd name :Not Found\n");
    }
}

static ssize_t show_hifi(struct device *dev, struct device_attribute *attr, char *buf)
{
    if (NULL != hwinfo_data.hifi_name) {
        return sprintf(buf, "hifi_name: %s\n", hwinfo_data.hifi_name);
    } else {
       return sprintf(buf, "hifi :Not Found\n");
    }
}


static ssize_t show_ctp(struct device *dev, struct device_attribute *attr, char *buf)
{
    if ((NULL != hwinfo_data.ctp_driver) || (NULL != hwinfo_data.ctp_module) ) {
        return sprintf(buf, "ctp name :%s\n", hwinfo_data.ctp_driver);
    } else {
        return sprintf(buf, "ctp name :Not Found\n");
    }
}

static ssize_t show_fingerprint(struct device *dev, struct device_attribute *attr, char *buf)
{
    if (NULL != hwinfo_data.fingerprint) {
        return sprintf(buf, "fingerprint name :%s\n", hwinfo_data.fingerprint);
    } else {
        return sprintf(buf, "fingerprint name :Not Found\n");
    }
}

static ssize_t show_fw_info(struct device *dev, struct device_attribute *attr, char *buf)
{
    if (NULL != hwinfo_data.ctp_fw_info) {
        return sprintf(buf, "%s\n", hwinfo_data.ctp_fw_info);
    } else {
        return sprintf(buf, "Invalid\n");
    }
}

static ssize_t show_color_info(struct device *dev, struct device_attribute *attr, char *buf)
{
    if (NULL != hwinfo_data.ctp_color_info) {
        return sprintf(buf, "%s\n", hwinfo_data.ctp_color_info);
    } else {
        return sprintf(buf, "Invalid\n");
    }
}

static ssize_t show_lockdown_info(struct device *dev, struct device_attribute *attr, char *buf)
{
    if (NULL != hwinfo_data.ctp_lockdown_info) {
        return sprintf(buf, "%s\n", hwinfo_data.ctp_lockdown_info);
    } else {
        return sprintf(buf, "Invalid\n");
    }
}


static ssize_t show_main_camera(struct device *dev, struct device_attribute *attr, char *buf)
{
/*simon modified to show source camera for factory camera mixture in zal1806 start*/
    if (NULL != hw_info_main_otp.sensor_name)
		hwinfo_data.main_camera = hw_info_main_otp.sensor_name;
/*simon modified to show source camera for factory camera mixture in zal1806 end*/
    if (NULL != hwinfo_data.main_camera) {
        return sprintf(buf , "main camera :%s\n", hwinfo_data.main_camera);
    } else {
        return sprintf(buf , "main camera :Not Found\n");
    }
}

static ssize_t show_main_camera2(struct device *dev, struct device_attribute *attr, char *buf)
{
/*simon modified to show source camera for factory camera mixture in zal1806 start*/
    if (NULL != hw_info_main2_otp.sensor_name)
		hwinfo_data.main_camera2 = hw_info_main2_otp.sensor_name;
/*simon modified to show source camera for factory camera mixture in zal1806 end*/
    if (NULL != hwinfo_data.main_camera2) {
        return sprintf(buf , "main camera 2 :%s\n", hwinfo_data.main_camera2);
    } else {
        return sprintf(buf , "main camera :Not Found\n");
    }
}

static ssize_t show_main_camera3(struct device *dev, struct device_attribute *attr, char *buf)
{
    if (NULL != hwinfo_data.main_camera3) {
        return sprintf(buf , "main camera 3 :%s\n", hwinfo_data.main_camera3);
    } else {
        return sprintf(buf , "main camera :Not Found\n");
    }
}

static ssize_t show_sub_camera(struct device *dev, struct device_attribute *attr, char *buf)
{
/*simon modified to show source camera for factory camera mixture in zal1806 start*/
    if (NULL != hw_info_sub_otp.sensor_name)
		hwinfo_data.sub_camera = hw_info_sub_otp.sensor_name;
/*simon modified to show source camera for factory camera mixture in zal1806 end*/
    if (NULL != hwinfo_data.sub_camera) {
        return sprintf(buf , "sub camera :%s\n", hwinfo_data.sub_camera);
    } else {
        return sprintf(buf , "sub camera :Not Found\n");
    }
}

static ssize_t show_main_otp(struct device *dev, struct device_attribute *attr, char *buf)
{
    if (hw_info_main_otp.otp_valid) {
        return sprintf(buf, "main otp :Vendor 0x%0x, ModuleCode 0x%x, ModuleVer 0x%x, SW_ver 0x%x, Date %d-%d-%d, VcmVendor 0x%0x, VcmModule 0x%x \n",
                            hw_info_main_otp.vendor_id, hw_info_main_otp.module_code, hw_info_main_otp.module_ver, hw_info_main_otp.sw_ver, hw_info_main_otp.year,
                            hw_info_main_otp.month, hw_info_main_otp.day, hw_info_main_otp.vcm_vendorid, hw_info_main_otp.vcm_moduleid);
    } else {
        return sprintf(buf, "main otp :No Valid OTP\n");
    }
}

static ssize_t show_main2_otp(struct device *dev, struct device_attribute *attr, char *buf)
{
    if (hw_info_main2_otp.otp_valid) {
        return sprintf(buf, "main otp :Vendor 0x%0x, ModuleCode 0x%x, ModuleVer 0x%x, SW_ver 0x%x, Date %d-%d-%d, VcmVendor 0x%0x, VcmModule 0x%x \n",
                            hw_info_main2_otp.vendor_id, hw_info_main2_otp.module_code, hw_info_main2_otp.module_ver, hw_info_main2_otp.sw_ver, hw_info_main2_otp.year,
                            hw_info_main2_otp.month, hw_info_main2_otp.day, hw_info_main2_otp.vcm_vendorid, hw_info_main2_otp.vcm_moduleid);
    } else {
        return sprintf(buf, "main otp :No Valid OTP\n");
    }
}

static ssize_t show_main3_otp(struct device *dev, struct device_attribute *attr, char *buf)
{
    if (hw_info_main3_otp.otp_valid) {
        return sprintf(buf, "main otp :Vendor 0x%0x, ModuleCode 0x%x, ModuleVer 0x%x, SW_ver 0x%x, Date %d-%d-%d, VcmVendor 0x%0x, VcmModule 0x%x \n",
                            hw_info_main3_otp.vendor_id, hw_info_main3_otp.module_code, hw_info_main3_otp.module_ver, hw_info_main3_otp.sw_ver, hw_info_main3_otp.year,
                            hw_info_main3_otp.month, hw_info_main3_otp.day, hw_info_main3_otp.vcm_vendorid, hw_info_main3_otp.vcm_moduleid);
    } else {
        return sprintf(buf, "main otp :No Valid OTP\n");
    }
}

static ssize_t show_sub_otp(struct device *dev, struct device_attribute *attr, char *buf)
{
    if (hw_info_sub_otp.otp_valid) {
        return sprintf(buf, "sub otp :Vendor 0x%0x, ModuleCode 0x%x, ModuleVer 0x%x, SW_ver 0x%x, Date %d-%d-%d, VcmVendor 0x%0x, VcmModule 0x%0x \n",
                            hw_info_sub_otp.vendor_id, hw_info_sub_otp.module_code, hw_info_sub_otp.module_ver, hw_info_sub_otp.sw_ver, hw_info_sub_otp.year,
                            hw_info_sub_otp.month, hw_info_sub_otp.day, hw_info_sub_otp.vcm_vendorid, hw_info_sub_otp.vcm_moduleid);
    } else {
        return sprintf(buf, "sub otp :No Valid OTP\n");
    }
}

static unsigned int get_emmc_size(void)
{
    return 0;
}

#define K(x) ((x) << (PAGE_SHIFT - 10))
static unsigned int get_ram_size(void)
{
    unsigned int ram_size, temp_size;
    struct sysinfo info;

    si_meminfo(&info);

    temp_size = K(info.totalram) / 1024;
    if (temp_size > 8192) {
        ram_size = 9;
    } else if (temp_size > 7168) {
        ram_size = 8;
    } else if (temp_size > 6144) {
        ram_size = 7;
    }else if (temp_size > 5120) {
        ram_size = 6;
    }else if (temp_size > 4096) {
        ram_size = 5;
    }else if (temp_size > 3072) {
        ram_size = 4;
    } else if (temp_size > 2048) {
        ram_size = 3;
    } else if (temp_size > 1024) {
        ram_size = 2;
    } else if(temp_size > 512) {
        ram_size = 1;
    } else {
        ram_size = 0;
    }

    return ram_size;
}

static ssize_t show_emmc_size(struct device *dev, struct device_attribute *attr, char *buf)
{
    return sprintf(buf, "%dGB\n", get_emmc_size());
}

static ssize_t show_ram_size(struct device *dev, struct device_attribute *attr, char *buf)
{
    return sprintf(buf, "%dGB\n", get_ram_size());
}

static ssize_t show_flash(struct device *dev, struct device_attribute *attr, char *buf)
{
    return sprintf(buf, "DDR INFO :%dGB  %s %s\n",
                 get_ram_size(),  get_ddr_type(),get_ddr_manufacture_name());
}

static ssize_t show_wifi(struct device *dev, struct device_attribute *attr, char *buf)
{
    return sprintf(buf, "wifi name :%s\n", HARDWARE_INFO_WCN);
}

static ssize_t show_bt(struct device *dev, struct device_attribute *attr, char *buf)
{
    return sprintf(buf, "bt name :%s\n", HARDWARE_INFO_WCN);
}

static ssize_t show_gps(struct device *dev, struct device_attribute *attr, char *buf)
{
    return sprintf(buf, "gps name :%s\n", HARDWARE_INFO_WCN);
}

static ssize_t show_fm(struct device *dev, struct device_attribute *attr, char *buf)
{
    return sprintf(buf, "fm name :%s\n", HARDWARE_INFO_WCN);
}

static ssize_t show_alsps(struct device *dev, struct device_attribute *attr, char *buf)
{
    if (NULL != hwinfo_data.alsps) {
        printk("[HWINFO] %s success. %s %s\n", __func__, buf,hwinfo_data.alsps);
        return sprintf(buf, "alsps name :%s\n", hwinfo_data.alsps);
    } else {
        printk("[HWINFO] %s alsps. not found\n", __func__);
        return sprintf(buf, "alsps name :Not Found\n");
    }

    printk("[HWINFO] %s alsps name :Not Support ALSPS\n", __func__);
    return sprintf(buf, "alsps name :Not Support ALSPS\n");
}
static ssize_t store_alsps(struct device *dev, struct device_attribute *attr,\
        const char *buf, size_t count)
{
    int ret;
    static char name[100] = "Not found";
    ret = snprintf(name, count, "%s", buf);
    hwinfo_data.alsps = name;
    if (ret) {
        printk("[HWINFO] %s success. %s\n", __func__, buf);
    } else {
        hwinfo_data.alsps = "Not found";
        printk("[HWINFO] %s failed.\n", __func__);
    }

    return count;
}

static ssize_t show_gsensor(struct device *dev, struct device_attribute *attr, char *buf)
{
//liukangping@huaqin.com add hardinfo sensor begin
#ifdef CONFIG_CUSTOM_KERNEL_ACCELEROMETER
#if defined(CONFIG_NANOHUB) && defined(CONFIG_CUSTOM_KERNEL_SENSORHUB)
	struct sensorInfo_t devinfo;
	int err = 0;

	err = sensor_set_cmd_to_hub(ID_ACCELEROMETER, CUST_ACTION_GET_SENSOR_INFO, &devinfo);
	if(err == 0)
		return sprintf(buf, "gsensor name :%s\n",devinfo.name);
	else
		return sprintf(buf, "gsensor name :Not Found\n");
#else
    if (NULL != hwinfo_data.gsensor) {
        return sprintf(buf, "gsensor name :%s\n", hwinfo_data.gsensor);
    } else {
        return sprintf(buf, "gsensor name :Not Found\n");
    }
#endif
#else
    return sprintf(buf, "gsensor name :Not support GSensor\n");
#endif


/*
#ifdef QCOM_SENSOR_INFO_NEED_SET
    if (NULL != hwinfo_data.gsensor) {
        return sprintf(buf, "gsensor name :%s\n", hwinfo_data.gsensor);
    } else {
        return sprintf(buf, "gsensor name :Not Found\n");
    }
#else
    return sprintf(buf, "gsensor name :Not support GSensor\n");
#endif
*/
//liukangping@huaqin.com add hardinfo sensor end
}
static ssize_t store_gsensor(struct device *dev, struct device_attribute *attr,\
        const char *buf, size_t count)
{
#ifdef QCOM_SENSOR_INFO_NEED_SET
    int ret;
    static char name[100] = "Not found";
    ret = snprintf(name, count, "%s", buf);
    hwinfo_data.gsensor = name;
    if (ret) {
        printk("[HWINFO] %s success .%s\n", __func__, buf);
    } else {
        hwinfo_data.gsensor = "Not found";
        printk("[HWINFO] %s failed.\n", __func__);
    }
#else
    hwinfo_data.gsensor = "Not found";
#endif
    return count;
}

static ssize_t show_msensor(struct device *dev, struct device_attribute *attr, char *buf)
{
//liukangping@huaqin.com add hardinfo sensor begin
#ifdef CONFIG_CUSTOM_KERNEL_MAGNETOMETER
#if defined(CONFIG_NANOHUB) && defined(CONFIG_CUSTOM_KERNEL_SENSORHUB)
	struct sensorInfo_t devinfo;
	int err = 0;
	err = sensor_set_cmd_to_hub(ID_MAGNETIC,
		CUST_ACTION_GET_SENSOR_INFO, &devinfo);

	if(err == 0)
		return sprintf(buf, "msensor name :%s\n",devinfo.mag_dev_info.libname);
	else
		return sprintf(buf, "msensor name :Not Found\n");
#else
    if (NULL != hwinfo_data.msensor) {
        return sprintf(buf, "msensor name :%s\n", hwinfo_data.msensor);
    } else {
        return sprintf(buf, "msensor name :Not Found\n");
    }
#endif
#else
    return sprintf(buf, "msensor name :Not support MSensor\n");
#endif

#ifdef QCOM_SENSOR_INFO_NEED_SET
    if (NULL != hwinfo_data.msensor) {
        return sprintf(buf, "msensor name :%s\n", hwinfo_data.msensor);
    } else {
        return sprintf(buf, "msensor name :Not Found\n");
    }
#else
    return sprintf(buf, "msensor name :Not support MSensor\n");
#endif
}
static ssize_t store_msensor(struct device *dev, struct device_attribute *attr,\
        const char *buf, size_t count)
{
#ifdef QCOM_SENSOR_INFO_NEED_SET
    int ret;
    static char name[100] = "Not found";
    ret = snprintf(name, 100, "%s", buf);
    hwinfo_data.msensor = name;
    if (ret) {
        printk("[HWINFO] %s success. %s\n", __func__, buf);
    } else {
        hwinfo_data.msensor = "Not found";
        printk("[HWINFO] %s failed.\n", __func__);
    }
#else
    hwinfo_data.msensor = "Not found";
#endif
    return count;
}

static ssize_t show_gyro(struct device *dev, struct device_attribute *attr, char *buf)
{
#if defined(CONFIG_CUSTOM_KERNEL_GYROSCOPE)
#if defined(CONFIG_NANOHUB) && defined(CONFIG_CUSTOM_KERNEL_SENSORHUB)
	struct sensorInfo_t devinfo;
	int err = -1;

	memset(&devinfo, 0, sizeof(devinfo));

	err = sensor_set_cmd_to_hub(ID_GYROSCOPE, CUST_ACTION_GET_SENSOR_INFO, &devinfo);
	if (err == 0) {
		return sprintf(buf, "gyro name :%s\n", devinfo.name);
	} else {
		return sprintf(buf, "gyro name :Not Found\n");
	}
#else
	if (NULL != hwinfo_data.gyroscope) {
		return sprintf(buf, "gyro name :%s\n", hwinfo_data.gyroscope);
	} else {
		return sprintf(buf, "gyro name :Not Found\n");
	}
#endif
#elif defined(QCOM_SENSOR_INFO_NEED_SET)
    if (NULL != hwinfo_data.gyroscope) {
        return sprintf(buf, "gyro name :%s\n", hwinfo_data.gyroscope);
    } else {
        return sprintf(buf, "gyro name :Not Found\n");
    }
#else
    return sprintf(buf, "gyro name :Not support Gyro\n");
#endif
}

static ssize_t store_gyro(struct device *dev, struct device_attribute *attr,\
        const char *buf, size_t count)
{
#ifdef QCOM_SENSOR_INFO_NEED_SET
    int ret;
    static char name[100] = "Not found";
    ret = snprintf(name, 100, "%s", buf);
    hwinfo_data.gyroscope = name;
    if (ret) {
        printk("[HWINFO] %s success. %s\n", __func__, buf);
    } else {
        hwinfo_data.gyroscope = "Not found";
        printk("[HWINFO] %s failed.\n", __func__);
    }
#else
    hwinfo_data.gyroscope = "Not found";
#endif
    return count;
}

static ssize_t show_sar_sensor(struct device *dev, struct device_attribute *attr, char *buf)
{
    if (NULL != hwinfo_data.sar_sensor) {
        return sprintf(buf, "sar_sensor name :%s\n", hwinfo_data.sar_sensor);
    } else {
        return sprintf(buf, "sar_sensor name :Not Found\n");
    }
}
// static ssize_t show_sar_sensor_2(struct device *dev, struct device_attribute *attr, char *buf)
// {
//     if (NULL != hwinfo_data.sar_sensor_2) {
//         return sprintf(buf, "sar_sensor_2 name :%s\n", hwinfo_data.sar_sensor_2);
//     } else {
//         return sprintf(buf, "sar_sensor_2 name :Not Found\n");
//     }
// }
static ssize_t store_sar_sensor(struct device *dev, struct device_attribute *attr,\
        const char *buf, size_t count)
{
#ifdef QCOM_SENSOR_INFO_NEED_SET
    int ret;
    static char name[100] = "Not found";
    ret = snprintf(name, 100, "%s", buf);
    hwinfo_data.sar_sensor = name;
    if (ret) {
        printk("[HWINFO] %s success. %s\n", __func__, buf);
    } else {
        hwinfo_data.sar_sensor = "Not found";
        printk("[HWINFO] %s failed.\n", __func__);
    }
#else
    hwinfo_data.sar_sensor = "Not found";
#endif
    return count;
}
// static ssize_t store_sar_sensor_2(struct device *dev, struct device_attribute *attr,\
//         const char *buf, size_t count)
// {
// #ifdef QCOM_SENSOR_INFO_NEED_SET
//     int ret;
//     static char name[100] = "Not found";
//     ret = snprintf(name, 100, "%s", buf);
//     hwinfo_data.sar_sensor_2 = name;
//     if (ret) {
//         printk("[HWINFO] %s success. %s\n", __func__, buf);
//     } else {
//         hwinfo_data.sar_sensor_2 = "Not found";
//         printk("[HWINFO] %s failed.\n", __func__);
//     }
// #else
//     hwinfo_data.sar_sensor_2 = "Not found";
// #endif
//     return count;
// }

static ssize_t show_version(struct device *dev, struct device_attribute *attr, char *buf)
{
    return sprintf(buf, "version:%s\n", HARDWARE_INFO_VERSION);
}

static ssize_t show_bat_id(struct device *dev, struct device_attribute *attr, char *buf)
{
#ifdef CONFIG_HQ_BATTERY_ID
    if (NULL != hwinfo_data.bat_id) {
        return sprintf(buf, "bat_id name :%s\n", hwinfo_data.bat_id);
    } else {
        return sprintf(buf, "bat_id name :Not found\n");
    }
#else
    return sprintf(buf, "bat_id name :Not support Bat_id\n");
#endif
}

static ssize_t show_nfc(struct device *dev, struct device_attribute *attr, char *buf)
{
#ifdef CONFIG_NFC_CHIP_PN553
    if (NULL != hwinfo_data.nfc) {
        return sprintf(buf, "nfc name :%s\n", hwinfo_data.nfc);
    } else {
        return sprintf(buf, "nfc name :Not found\n");
    }
#else
    return sprintf(buf, "nfc name :Not support nfc\n");
#endif
}

// static char* get_stage_name(void)
// {
//     unsigned int i;

//     for (i = 0; i < ARRAY_SIZE(pcba_stage_table); i ++) {
//         if (current_stage_value == pcba_stage_table[i].stage_value)
//             return pcba_stage_table[i].pcba_stage_name;
//     }

//     return pcba_stage_table[ARRAY_SIZE(pcba_stage_table) - 1].pcba_stage_name;
// }

char* get_type_name(void)
{
    unsigned int i;

    for (i = 0; i < ARRAY_SIZE(pcba_type_table); i ++) {
        if (current_type_value == pcba_type_table[i].type_value) {
            return pcba_type_table[i].pcba_type_name;
        }
    }

    return pcba_type_table[ARRAY_SIZE(pcba_type_table) - 1].pcba_type_name;
}
EXPORT_SYMBOL(get_type_name);

char* get_ddr_type(void)
{
    unsigned int i;

    for (i = 0; i < ARRAY_SIZE(ddr_type_table); i ++) {
        if (g_ddr_details->device_type == ddr_type_table[i].value) {
            return ddr_type_table[i].name;
        }
    }

    return ddr_type_table[ARRAY_SIZE(ddr_type_table) - 1].name;
}

char* get_ddr_manufacture_name(void)
{
    unsigned int i;

    for (i = 0; i < ARRAY_SIZE(ddr_manufacture_table); i ++) {
        if (g_ddr_details->manufacturer_id == ddr_manufacture_table[i].value) {
            return ddr_manufacture_table[i].name;
        }
    }

    return ddr_manufacture_table[ARRAY_SIZE(ddr_manufacture_table) - 1].name;
}

static ssize_t show_hw_id(struct device *dev, struct device_attribute *attr, char *buf)
{
    return sprintf(buf, "PCBA_%s_%s\n", PROJECT_NAME, get_type_name());
}

static ssize_t show_main_cam_sn(struct device *dev, struct device_attribute *attr, char *buf)
{
    if (hwinfo_data.main_cam_sn != NULL)
        return sprintf(buf, "MAIN:%s\n", hwinfo_data.main_cam_sn);
    else
        return sprintf(buf, "MAIN:Not_found\n");
}

static ssize_t show_main_cam_2_sn(struct device *dev, struct device_attribute *attr, char *buf)
{
    if (hwinfo_data.main_cam_2_sn != NULL)
        return sprintf(buf, "MAIN2:%s\n", hwinfo_data.main_cam_2_sn);
    else
        return sprintf(buf, "MAIN2:Not_found\n");
}

static ssize_t show_main_cam_3_sn(struct device *dev, struct device_attribute *attr, char *buf)
{
    if (hwinfo_data.main_cam_3_sn != NULL)
        return sprintf(buf, "MAIN3:%s\n", hwinfo_data.main_cam_3_sn);
    else
        return sprintf(buf, "MAIN3:Not_found\n");
}

static ssize_t show_sub_cam_sn(struct device *dev, struct device_attribute *attr, char *buf)
{
    if (hwinfo_data.sub_cam_sn != NULL)
        return sprintf(buf, "SUB:%s\n", hwinfo_data.sub_cam_sn);
    else
        return sprintf(buf, "SUB:Not_found\n");
}
static ssize_t show_sim1_card_slot_status(struct device *dev, struct device_attribute *attr, char *buf)
{
    int sim1_card_slot_status;
    sim1_card_slot_status = gpio_get_value(sim1_card_slot);
    if (sim1_card_slot_status)
        return sprintf(buf, "%s\n", "1");
    else
        return sprintf(buf, "%s\n", "0");
}

static ssize_t show_sim2_card_slot_status(struct device *dev, struct device_attribute *attr, char *buf)
{
    int sim2_card_slot_status;
    sim2_card_slot_status = gpio_get_value(sim2_card_slot);
    if (sim2_card_slot_status)
        return sprintf(buf, "%s\n", "1");
    else
        return sprintf(buf, "%s\n", "0");
}
static DEVICE_ATTR(version, 0444, show_version, NULL);
static DEVICE_ATTR(lcm, 0444, show_lcm, NULL);
static DEVICE_ATTR(ctp, 0444, show_ctp, NULL);
static DEVICE_ATTR(ctp_fw, 0444, show_fw_info, NULL);
static DEVICE_ATTR(ctp_color, 0444, show_color_info, NULL);
static DEVICE_ATTR(ctp_lockdown, 0444, show_lockdown_info, NULL);
static DEVICE_ATTR(main_camera, 0444, show_main_camera, NULL);
static DEVICE_ATTR(main_camera2, 0444, show_main_camera2, NULL);
static DEVICE_ATTR(main_camera3, 0444, show_main_camera3, NULL);
static DEVICE_ATTR(sub_camera, 0444, show_sub_camera, NULL);
static DEVICE_ATTR(flash, 0444, show_flash, NULL);
static DEVICE_ATTR(hifi_name, 0444, show_hifi, NULL);
static DEVICE_ATTR(emmc_size, 0444, show_emmc_size, NULL);
static DEVICE_ATTR(ram_size, 0444, show_ram_size, NULL);
static DEVICE_ATTR(gsensor, 0644, show_gsensor, store_gsensor);
static DEVICE_ATTR(msensor, 0644, show_msensor, store_msensor);
static DEVICE_ATTR(alsps, 0644, show_alsps, store_alsps);
static DEVICE_ATTR(gyro, 0644, show_gyro, store_gyro);
static DEVICE_ATTR(wifi, 0444, show_wifi, NULL);
static DEVICE_ATTR(bt, 0444, show_bt, NULL);
static DEVICE_ATTR(gps, 0444, show_gps, NULL);
static DEVICE_ATTR(fm, 0444, show_fm, NULL);
static DEVICE_ATTR(main_otp, 0444, show_main_otp, NULL);
static DEVICE_ATTR(main2_otp, 0444, show_main2_otp, NULL);
static DEVICE_ATTR(main3_otp, 0444, show_main3_otp, NULL);
static DEVICE_ATTR(sub_otp, 0444, show_sub_otp, NULL);
static DEVICE_ATTR(sar_sensor, 0644, show_sar_sensor, store_sar_sensor);
//static DEVICE_ATTR(sar_sensor_2, 0644, show_sar_sensor_2, store_sar_sensor_2);
static DEVICE_ATTR(bat_id, 0444, show_bat_id,NULL );
static DEVICE_ATTR(nfc, 0444, show_nfc,NULL );
static DEVICE_ATTR(hw_id, 0444, show_hw_id, NULL);
static DEVICE_ATTR(fingerprint, 0444, show_fingerprint, NULL);
static DEVICE_ATTR(main_cam_sn, 0444, show_main_cam_sn, NULL);
static DEVICE_ATTR(main_cam_2_sn, 0444, show_main_cam_2_sn, NULL);
static DEVICE_ATTR(main_cam_3_sn, 0444, show_main_cam_3_sn, NULL);
static DEVICE_ATTR(sub_cam_sn, 0444, show_sub_cam_sn, NULL);
static DEVICE_ATTR(sim1_card_slot_status, 0444, show_sim1_card_slot_status, NULL);
static DEVICE_ATTR(sim2_card_slot_status, 0444, show_sim2_card_slot_status, NULL);

static struct attribute *hdinfo_attributes[] = {
    &dev_attr_version.attr,
    &dev_attr_lcm.attr,
    &dev_attr_ctp.attr,
    &dev_attr_ctp_fw.attr,
    &dev_attr_ctp_color.attr,
    &dev_attr_ctp_lockdown.attr,
    &dev_attr_main_camera.attr,
    &dev_attr_main_camera2.attr,
    &dev_attr_main_camera3.attr,
    &dev_attr_sub_camera.attr,
    &dev_attr_flash.attr,
    &dev_attr_hifi_name.attr,
    &dev_attr_emmc_size.attr,
    &dev_attr_ram_size.attr,
    &dev_attr_gsensor.attr,
    &dev_attr_msensor.attr,
    &dev_attr_alsps.attr,
    &dev_attr_gyro.attr,
    &dev_attr_wifi.attr,
    &dev_attr_bt.attr,
    &dev_attr_gps.attr,
    &dev_attr_fm.attr,
    &dev_attr_main_otp.attr,
    &dev_attr_main2_otp.attr,
    &dev_attr_main3_otp.attr,
    &dev_attr_sub_otp.attr,
    &dev_attr_sar_sensor.attr,
    //&dev_attr_sar_sensor_2.attr,
    &dev_attr_bat_id.attr,
    &dev_attr_nfc.attr,
    &dev_attr_hw_id.attr,
    &dev_attr_fingerprint.attr,
    &dev_attr_main_cam_sn.attr,
    &dev_attr_main_cam_2_sn.attr,
    &dev_attr_main_cam_3_sn.attr,
    &dev_attr_sub_cam_sn.attr,
    &dev_attr_sim1_card_slot_status.attr,
    &dev_attr_sim2_card_slot_status.attr,
    NULL
};

static struct attribute_group hdinfo_attribute_group = {
    .attrs = hdinfo_attributes
};

static int hw_info_parse_dt(struct device_node *np)
{
    int ret = -1;

   // int stage_gpio_pin0 = -1, stage_gpio_pin1 = -1,stage_gpio_pin2 = -1;
    int type_gpio_pin0 = -1,type_gpio_pin1 = -1,type_gpio_pin2 = -1,type_gpio_pin3 = -1;
    if (np) {
    /*    stage_gpio_pin0 = of_get_named_gpio(np, "pcb_stage_gpios0", 0);
        ret = gpio_request(stage_gpio_pin0, "pcb_stage_gpio0");
        if (ret) {
                printk("[HWINFO] pcb_stage_gpio not available (ret=%d)\n", ret);
                current_stage_value = -1;
                goto err;
        }
        stage_gpio_pin1 = of_get_named_gpio(np, "pcb_stage_gpios1", 0);
        ret = gpio_request(stage_gpio_pin1, "pcb_stage_gpio1");
        if (ret) {
                printk("[HWINFO] pcb_stage_gpio not available (ret=%d)\n", ret);
                current_stage_value = -1;
                goto err;
        }
        stage_gpio_pin2 = of_get_named_gpio(np, "pcb_stage_gpios2", 0);
        ret = gpio_request(stage_gpio_pin2, "pcb_stage_gpio2");
        if (ret) {
                printk("[HWINFO] pcb_stage_gpio not available (ret=%d)\n", ret);
                current_stage_value = -1;
                goto err;
        }
        current_stage_value = (gpio_get_value(stage_gpio_pin0) << 0) | (gpio_get_value(stage_gpio_pin1) << 1) | (gpio_get_value(stage_gpio_pin2) << 2);
        printk("[HWINFO] stage_value %d\n",current_stage_value);
*/
        type_gpio_pin0 = of_get_named_gpio(np, "pcb_type_gpios0", 0);
        if (!gpio_is_valid(type_gpio_pin0)) {
            if (type_gpio_pin0 != -EPROBE_DEFER) {
                pr_err("%s invalid GPIO %d\n", __func__, type_gpio_pin0);
            } else {
                pr_warn("%s pcb_type_gpios0 not ready %d\n", __func__, type_gpio_pin0);
            }
            ret = type_gpio_pin0;
            goto err0;
        }
        ret = gpio_request(type_gpio_pin0, "pcb_type_gpios0");
        if (ret) {
                printk("[HWINFO] pcb_type_gpio not available (ret=%d)\n", ret);
                current_type_value = -1;
                goto err0;
        }
        type_gpio_pin1 = of_get_named_gpio(np, "pcb_type_gpios1", 0);
        if (!gpio_is_valid(type_gpio_pin1)) {
            if (type_gpio_pin0 != -EPROBE_DEFER) {
                pr_err("%s invalid GPIO %d\n", __func__, type_gpio_pin1);
            } else {
                pr_warn("%s pcb_type_gpios1 not ready %d\n", __func__, type_gpio_pin1);
            }
            ret = type_gpio_pin1;
            goto err1;
        }
        ret = gpio_request(type_gpio_pin1, "pcb_type_gpios1");
        if (ret) {
                printk("[HWINFO] pcb_type_gpio not available (ret=%d)\n", ret);
                current_type_value = -1;
                goto err1;
        }
        type_gpio_pin2 = of_get_named_gpio(np, "pcb_type_gpios2", 0);
        if (!gpio_is_valid(type_gpio_pin2)) {
            if (type_gpio_pin2 != -EPROBE_DEFER) {
                pr_err("%s invalid GPIO %d\n", __func__, type_gpio_pin2);
            } else {
                pr_warn("%s pcb_type_gpios2 not ready %d\n", __func__, type_gpio_pin2);
            }
            ret = type_gpio_pin2;
            goto err2;
        }
        ret = gpio_request(type_gpio_pin2, "pcb_type_gpios2");
        if (ret) {
                printk("[HWINFO] pcb_type_gpio not available (ret=%d)\n", ret);
                goto err2;
        }
        type_gpio_pin3 = of_get_named_gpio(np, "pcb_type_gpios3", 0);
        if (!gpio_is_valid(type_gpio_pin3)) {
            if (type_gpio_pin3 != -EPROBE_DEFER) {
                pr_err("%s invalid GPIO %d\n", __func__, type_gpio_pin3);
            } else {
                pr_warn("%s pcb_type_gpios3 not ready %d\n", __func__, type_gpio_pin3);
            }
            ret = type_gpio_pin3;
            goto err3;
        }
        ret = gpio_request(type_gpio_pin3, "pcb_type_gpios3");
        if (ret) {
                printk("[HWINFO] pcb_type_gpio not available (ret=%d)\n", ret);
                current_type_value = -1;
                goto err3;
        }
        current_type_value = (gpio_get_value(type_gpio_pin3) << 0) | (gpio_get_value(type_gpio_pin2) << 1) | (gpio_get_value(type_gpio_pin1) << 2) | (gpio_get_value(type_gpio_pin0) << 3) ;
        pr_info("[HWINFO] type_value   0x%x\n",current_type_value);
    } else {
        goto err0;    
    }
err3:
    gpio_free(type_gpio_pin2);
err2:
    gpio_free(type_gpio_pin1);
err1:
    gpio_free(type_gpio_pin0);
err0:
     return ret;
}

static int HardwareInfo_driver_probe(struct platform_device *pdev)
{
    int ret = -1;
    size_t size;
    pr_info("HardwareInfo_driver_probe start\n");
    memset(&hwinfo_data, 0, sizeof(hwinfo_data));
    memset(&hw_info_main_otp, 0, sizeof(hw_info_main_otp));
    memset(&hw_info_main2_otp, 0, sizeof(hw_info_main2_otp));
    memset(&hw_info_main3_otp, 0, sizeof(hw_info_main3_otp));

    ret = sysfs_create_group(&(pdev->dev.kobj), &hdinfo_attribute_group);
    if (ret < 0) {
        printk("[HWINFO] sysfs_create_group failed! (ret=%d)\n", ret);
        goto err;
    }

    //INIT_DELAYED_WORK(&psSetSensorConf_work, do_psSetSensorConf_work);

    ret = hw_info_parse_dt(pdev->dev.of_node);
    if (ret < 0) {
        printk("[HWINFO] hw_info_parse_dt failed! (ret=%d)\n", ret);
        goto err;
    }
    pcb_name = get_type_name();
    if(pcb_name && strcmp(pcb_name, "UNKNOWN") == 0){
        printk("byron:  UNKNOWN pcb name \n");
    }

    /* Get Global DDRINFO_SMEM_ID info*/
    g_ddr_details = qcom_smem_get(QCOM_SMEM_HOST_ANY, DDRINFO_SMEM_ID, &size);
    pr_info("[HWINFO]: g_ddr_details is %pa\n", g_ddr_details);
    if (IS_ERR_OR_NULL(g_ddr_details)) {
        pr_err("[HWINFO]:SMEM is not initialized.\n");
        goto err;
    }

    pr_info("[HWINFO]: g_ddr_details is  manufacturer_id %d  type:%d\n", g_ddr_details->manufacturer_id, g_ddr_details->device_type);
    pr_info("[HWINFO]:DDR INFO:%s %s\n",get_ddr_type(),get_ddr_manufacture_name());
err:
    pr_info("HardwareInfo_driver_probe end ret %d\n", ret); 
    return ret;
}

static int HardwareInfo_driver_remove(struct platform_device *pdev)
{
    sysfs_remove_group(&(pdev->dev.kobj), &hdinfo_attribute_group);

    return 0;
}

static const struct of_device_id hwinfo_dt_match[] = {
    {.compatible = "huaqin,hardware_info"},
    {}
};

static struct platform_driver HardwareInfo_driver = {
    .probe = HardwareInfo_driver_probe,
    .remove = HardwareInfo_driver_remove,
    .driver = {
        .name = "HardwareInfo",
        .of_match_table = hwinfo_dt_match,
    },
};

static int __init HardwareInfo_mod_init(void)
{
    int ret = -1;

    ret = platform_driver_register(&HardwareInfo_driver);
    if (ret) {
        printk("[HWINFO] HardwareInfo_driver registed failed! (ret=%d)\n", ret);
    }

    return ret;
}

static void __exit HardwareInfo_mod_exit(void)
{
    platform_driver_unregister(&HardwareInfo_driver);
}


fs_initcall(HardwareInfo_mod_init);
module_exit(HardwareInfo_mod_exit);

MODULE_AUTHOR("Oly Peng <penghoubing@huaqin.com>");
MODULE_DESCRIPTION("Huaqin Hareware Info driver");
MODULE_LICENSE("GPL");
