/* Copyright (c) 2023 Sony Interactive Entertainment Inc. */

#include <linux/device.h>
#include <linux/regmap.h>
#include <linux/delay.h>
#include <linux/power_supply.h>
#include <linux/regulator/driver.h>
#include <linux/qpnp/qpnp-revid.h>
#include <linux/irq.h>
#include <linux/iio/consumer.h>
#include <linux/pmic-voter.h>
#include <linux/ktime.h>
#include <linux/of_gpio.h>
#include <linux/of_device.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/workqueue.h>
#include <linux/regulator/consumer.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/extcon-provider.h>
#include <linux/usb/typec.h>
#include <linux/input.h>
#include <linux/input/mt.h>

#define USB_HUB_START_DETECT_MS     7000  /* ms */
#define USB_HUB_DETECT_RETRY_TIME   5000  /* ms */
#define USB_HUB_RESET_MS            20   /* ms */
#define USB_HUB_RESET_EXCUTE_MS     2500  /* ms */

#define USB_GPIO_DEBOUNCE_MS        20    /* ms */
#define MS_TO_USEC                  1000  /* us */
#define SKY_CONNECT_DELAY           200   /* ms */

#define QPNP_USB_DEVICE             "device"
#define QPNP_USB_HOST               "host"
#define QPNP_USB_HOST_ALL           "host-all"
#define QPNP_USB_HOST_ENTHER        "host-ethernet"
#define QPNP_USB_HOST_SKY           "host-sky"
#define QPNP_USB_HOST_DAU           "host-dau"

#define BOARD_ID_NAME_EVT         0
#define BOARD_ID_NAME_DVT         1

#define HUB_CONNECT_TIMES         3

#define HUB_RESET_CONTINUE_TIME   50
#define HUB_RESET_ACTIVE_TIME     10
#define SKY_DELAY_GRAP            5
#define HUB_DELAY_GRAP            10
#define CDB_DELAY_GRAP            10
#define VBUS_POWER_GRAP           5
#define RETRY_FAILED_DELAY        15

#if defined(RELEASE_VERSION)
static  int release_version = 1;
#else /* !FACTORY && USER */
static  int release_version = 0;
#endif /* OTHER */

static const unsigned int hub_extcon_cable[] = {
	EXTCON_USB,
	EXTCON_USB_HOST,
	EXTCON_NONE,
};

struct usb_hub_chip {
	struct pinctrl *pinctrl;
	struct pinctrl_state *pin_switch_default;
	struct pinctrl_state *pin_switch_active;
	struct pinctrl_state *hub_reset_default;
	struct pinctrl_state *hub_reset_active;
	struct pinctrl_state *pin_sky_default;
	struct pinctrl_state *pin_sky_active;
	struct pinctrl_state *pin_sky_pwr_default;
	struct pinctrl_state *pin_sky_pwr_active;
	struct pinctrl_state *pin_skyConn_default;
	struct pinctrl_state *pin_skyConn_active;
	struct pinctrl_state *pin_ext_default;
	struct pinctrl_state *pin_ext_active;
	struct pinctrl_state *pin_cdb_default;
	struct pinctrl_state *pin_cdb_active;
	struct pinctrl_state *pin_vbus_default;
	struct pinctrl_state *pin_vbus_active;
	struct pinctrl_state *pin_power_default;
	struct pinctrl_state *pin_power_active;
	struct pinctrl_state *pin_dev_default;
	struct pinctrl_state *pin_dev_active;
	struct pinctrl_state *pin_sky_reset_default;
	struct pinctrl_state *pin_sky_reset_active;
	struct pinctrl_state *pin_cdb_int_default;
	struct pinctrl_state *pin_sky_int_default;

	struct device *dev;
	struct regulator	*vdd;
	struct gpio_desc *host_detect_gpiod;
	int host_detect_irq;
	int ocp_detect_irq;
	int ocp_detect_sky_irq;
	/* extcon for VBUS / ID notification to USB for uUSB */
	struct extcon_dev	*extcon;
	/* work */
	struct delayed_work	hub_detect_work;
	struct delayed_work hub_detect_retry_work;
	struct work_struct	hub_charge_work;
	struct work_struct restart_usb_work;

	struct input_dev *hub_input_dev;
	int id_state;
	bool charge_mode;

	int hub_detect_retry_flag;
	int hub_detect_retry_num;

	struct gpio_desc *gpio_sky;
	struct gpio_desc *gpio_sky_pair;
	struct gpio_desc *gpio_ext;
	struct gpio_desc *gpio_cdb;
	struct gpio_desc *gpio_vbus;
	struct gpio_desc *gpio_hub_power;
	struct gpio_desc *gpio_dev;
	struct gpio_desc *gpio_hub_reset;
	struct gpio_desc *gpio_sky_reset;
	struct gpio_desc *gpio_cdb_int;
	struct gpio_desc *gpio_sky_int;

	bool hub_pwr_enable;
};

enum hub_mount {
	HUB_DEVICE = 0,
	CDB_DEVICE,
	CDB_HIDRAW,
	SKY_DEVICE,
	SKY_HIDRAW,
	STATE_MAX
};

enum hub_found {
	HUB_FOUND_UNKNOW = 0,
	DEVICE_ALL_FOUND,
	DEVICE_ALL_NOTFOUND,
	DEVICE_CDB_NOTFOUND,
	DEVICE_SKY_NOTFOUND,
	DEVICE_MAX
};

extern void set_usb_mount_state(int vidType, int vidValue);
extern int get_usb_mount_state(int vidType);

char* get_type_name(void);
static int setup_secureflag_para(void);
struct usb_hub_chip *g_chargeChip = NULL;
int g_extconUsbGpioState = -1;
bool g_resetFlag = false;
bool g_getUsbDeviceState = false;
bool g_getUsbHostState = false;
static u8 g_chargeActive = 0;

int extcon_usb_gpio_state(void)
{
	return g_extconUsbGpioState;
}
EXPORT_SYMBOL_GPL(extcon_usb_gpio_state);

int get_board_id_name(void)
{
	int current_type = 0;
	if ((strcmp(get_type_name(), "EVT0") == 0) || (strcmp(get_type_name(), "EVT1") == 0)
		|| (strcmp(get_type_name(), "EVT2") == 0)) {
		current_type = BOARD_ID_NAME_EVT;
	} else {
		current_type = BOARD_ID_NAME_DVT;
	}
	return current_type;
}

static int connect_node_all_find(void)
{
	int connect_all_find = 0;

	if ((get_usb_mount_state(HUB_DEVICE) == 1) && (get_usb_mount_state(CDB_DEVICE) == 1) &&
		(get_usb_mount_state(CDB_HIDRAW) == 1) && (get_usb_mount_state(SKY_DEVICE) == 1) &&
		(get_usb_mount_state(SKY_HIDRAW) == 1)) {
		connect_all_find = DEVICE_ALL_FOUND;
	} else if ((get_usb_mount_state(CDB_HIDRAW) == 0) && (get_usb_mount_state(SKY_HIDRAW) == 0)) {
		connect_all_find = DEVICE_ALL_NOTFOUND;
	} else if ((get_usb_mount_state(CDB_HIDRAW) == 0) && (get_usb_mount_state(SKY_HIDRAW) == 1)) {
		connect_all_find = DEVICE_CDB_NOTFOUND;
	} else if ((get_usb_mount_state(CDB_HIDRAW) == 1) && (get_usb_mount_state(SKY_HIDRAW) == 0)) {
		connect_all_find = DEVICE_SKY_NOTFOUND;
	} else {
		connect_all_find = HUB_FOUND_UNKNOW;
	}

	return connect_all_find;
}

static void set_hub_all_flag_default(void)
{
	set_usb_mount_state(HUB_DEVICE, 0);
	set_usb_mount_state(CDB_DEVICE, 0);
	set_usb_mount_state(CDB_HIDRAW, 0);
	set_usb_mount_state(SKY_DEVICE, 0);
	set_usb_mount_state(SKY_HIDRAW, 0);

	pr_info("HUB_DEVICE = %d, CDB_DEVICE = %d, CDB_HIDRAW = %d, SKY_DEVICE = %d, SKY_HIDRAW = %d\n",
		get_usb_mount_state(HUB_DEVICE), get_usb_mount_state(CDB_DEVICE), get_usb_mount_state(CDB_HIDRAW),
		get_usb_mount_state(SKY_DEVICE), get_usb_mount_state(SKY_HIDRAW));
}

static int setup_secureflag_para(void)
{
	struct device_node * of_chosen = NULL;
	char *bootargs = NULL;
	int ret = -1;
	of_chosen = of_find_node_by_path("/chosen");
	if (of_chosen) {
		bootargs = (char *)of_get_property(of_chosen, "bootargs", NULL);
		if (!bootargs) {
			pr_err("%s: failed to get bootargs", __func__);
			return ret;
		}
	} else {
		pr_err("%s: failed to get /chosen", __func__);
		return ret;
	}
	if (strstr(bootargs, "secure.flag=1")) {  //adb auth
		pr_err("%s: success to get secure.flag=1 in bootargs!", __func__);
		ret = 1;
	} else {
		pr_err("%s: fail to get secure.flag=1 in bootargs!", __func__);
		ret = 0;
	}
	return ret;
}

void dwc3_restart_usb_work(struct work_struct *w);

static void usb_hub_notify_extcon_props(struct usb_hub_chip *chip, int id)
{
	union extcon_property_value val;

	val.intval = 0; // 0 (normal) or 1 (flip), default:	0 (normal)
	extcon_set_property(chip->extcon, id, EXTCON_PROP_USB_TYPEC_POLARITY, val);
	val.intval = 0; // 0 (USB/USB2) or 1 (USB3), default:    0 (USB/USB2)
	extcon_set_property(chip->extcon, id, EXTCON_PROP_USB_SS, val);

	return;
}

static void usb_hub_notify_device_mode(struct usb_hub_chip *chip, bool enable)
{
	if (enable) {
		usb_hub_notify_extcon_props(chip, EXTCON_USB);
	}

	g_getUsbDeviceState = enable;
	extcon_set_state_sync(chip->extcon, EXTCON_USB, enable);

	return;
}

static void usb_hub_notify_usb_host(struct usb_hub_chip *chip, bool enable)
{
	if (enable) {
		usb_hub_notify_extcon_props(chip, EXTCON_USB_HOST);
	}

	g_getUsbHostState = enable;
	extcon_set_state_sync(chip->extcon, EXTCON_USB_HOST, enable);

	return;
}

static void switch_power_enable_work(struct work_struct *work)
{
	struct usb_hub_chip *chip = container_of(work, struct usb_hub_chip, hub_charge_work);
	int current_type = get_board_id_name();
	int verify_num = setup_secureflag_para();

	if (release_version == 1 && verify_num == 0) {
		return;
	}

	if (current_type == BOARD_ID_NAME_DVT) {
		pr_err("Prepare to switch mode, chip->charge_mode = %d!!!!\n", chip->charge_mode);
		if (chip->charge_mode) {  //charge mode, gpio75-low
			pr_err("Charge-mode gpio75 low!!!\n");
			pinctrl_select_state(chip->pinctrl, chip->pin_power_default);
		} else {    //other mode, gpio75-high
			pr_err("Other-mode gpio75 high!!!\n");
			pinctrl_select_state(chip->pinctrl, chip->pin_power_active);
		}
	}

	return;
}

void extcon_get_charge_mode(bool charge_mode)
{
	struct usb_hub_chip *chip = g_chargeChip;
	int current_type = get_board_id_name();
	if (chip == NULL) {
		return;
	}

	if (current_type == BOARD_ID_NAME_DVT) {
		pr_err("detect usb cable, usb_mode = %d\n", charge_mode);
		chip->charge_mode = charge_mode;

		if (g_chargeActive == 0) {
			return;
		}

		schedule_work(&chip->hub_charge_work);
	}
}
EXPORT_SYMBOL_GPL(extcon_get_charge_mode);

static void usb_hub_all_power_off(struct usb_hub_chip *chip)
{
	if (chip == NULL) {
		return;
	}

	pinctrl_select_state(chip->pinctrl, chip->pin_sky_pwr_default);
	msleep(SKY_DELAY_GRAP);
	pinctrl_select_state(chip->pinctrl, chip->pin_sky_default);

	pinctrl_select_state(chip->pinctrl, chip->pin_cdb_default);
	pinctrl_select_state(chip->pinctrl, chip->pin_ext_default);
	msleep(HUB_DELAY_GRAP);

	pinctrl_select_state(chip->pinctrl, chip->pin_vbus_default);

	pr_err("usb hub all power off!!!!!!!!!\n");
	return;
}

static void usb_hub_all_power_on(struct usb_hub_chip *chip)
{
	if (chip == NULL) {
		return;
	}

	pinctrl_select_state(chip->pinctrl, chip->pin_vbus_active);
	msleep(VBUS_POWER_GRAP);

	pinctrl_select_state(chip->pinctrl, chip->hub_reset_active);
	msleep(HUB_RESET_ACTIVE_TIME);
	pinctrl_select_state(chip->pinctrl, chip->hub_reset_default);

	msleep(SKY_DELAY_GRAP);
	pinctrl_select_state(chip->pinctrl, chip->pin_sky_active);   //sky-vbus high & sky-rst low
	msleep(SKY_DELAY_GRAP);
	pinctrl_select_state(chip->pinctrl, chip->pin_sky_pwr_active);
	msleep(CDB_DELAY_GRAP);
	pinctrl_select_state(chip->pinctrl, chip->pin_cdb_active);
	pinctrl_select_state(chip->pinctrl, chip->pin_ext_active);

	pr_err("usb hub all power on!!!!!!!!!\n");
	return;
}

static void hub_all_power_switch(struct usb_hub_chip *chip, int val)
{
	if (chip == NULL) {
		return;
	}

	chip->hub_detect_retry_flag = 0;
	chip->hub_detect_retry_num = 0;
	chip->id_state = chip->host_detect_gpiod ?
		gpiod_get_value_cansleep(chip->host_detect_gpiod) : 1;

	if ((val == 0) && (chip->id_state == 0)) {
		cancel_delayed_work_sync(&chip->hub_detect_work);
	}
	cancel_delayed_work_sync(&chip->hub_detect_retry_work);

	if (val == 1) {   //screen-on
		usb_hub_all_power_on(chip);
		chip->hub_pwr_enable = 1;

		if (chip->id_state == 0) {   //host mode
			pr_err("HUB screen on to connect device!!!\n");
			usb_hub_notify_usb_host(chip, true);

			chip->hub_detect_retry_flag = 1;
			chip->hub_detect_retry_num = 0;
			queue_delayed_work(system_power_efficient_wq, &chip->hub_detect_retry_work, msecs_to_jiffies(USB_HUB_DETECT_RETRY_TIME));
		}
	} else {     //screen-off
		if (chip->id_state == 0) {   //host mode
			pr_err("HUB screen off to disconnect device!!!\n");
			usb_hub_all_power_off(chip);
			chip->hub_pwr_enable = 0;
			set_hub_all_flag_default();
			usb_hub_notify_usb_host(chip, false);
			usb_hub_notify_device_mode(chip, false);
			pr_err("cancel_detect_worker hub power off!!!!!!!!!!!\n");
		}
	}
}

static void usb_hub_detect_retry_work(struct work_struct *work)
{
	struct usb_hub_chip *chip = container_of(to_delayed_work(work),
			struct usb_hub_chip, hub_detect_retry_work);
	int hub_find_state = 0;

	if (chip->hub_detect_retry_flag == 1 && chip->hub_detect_retry_num < HUB_CONNECT_TIMES) {
		chip->hub_detect_retry_num++;
		hub_find_state = connect_node_all_find();
		pr_err("hub_find_state = %d\n", hub_find_state);

		if (hub_find_state == DEVICE_ALL_FOUND) {  //all find
			return;
		} else if (hub_find_state == DEVICE_ALL_NOTFOUND) {  //cdb & sky not find
			usb_hub_notify_usb_host(chip, false);
			usb_hub_notify_device_mode(chip, false);
			usb_hub_all_power_off(chip);
			set_usb_mount_state(HUB_DEVICE, 0);   //set hub vid flag is 0

			msleep(RETRY_FAILED_DELAY);
			usb_hub_all_power_on(chip);
			usb_hub_notify_usb_host(chip, true);
		} else if (hub_find_state == DEVICE_SKY_NOTFOUND) {
			pinctrl_select_state(chip->pinctrl, chip->pin_sky_pwr_default);
			msleep(SKY_DELAY_GRAP);
			pinctrl_select_state(chip->pinctrl, chip->pin_sky_default);

			msleep(RETRY_FAILED_DELAY);
			pinctrl_select_state(chip->pinctrl, chip->pin_sky_active);   //sky-vbus high & sky-rst low
			msleep(SKY_DELAY_GRAP);
			pinctrl_select_state(chip->pinctrl, chip->pin_sky_pwr_active);
		} else if (hub_find_state == DEVICE_CDB_NOTFOUND) {
			pinctrl_select_state(chip->pinctrl, chip->pin_cdb_default);
			msleep(SKY_DELAY_GRAP);
			pinctrl_select_state(chip->pinctrl, chip->pin_cdb_active);
		}
		queue_delayed_work(system_power_efficient_wq, &chip->hub_detect_retry_work, msecs_to_jiffies(USB_HUB_DETECT_RETRY_TIME));
	}

	pr_err("hub_detect_retry_num = %d\n", chip->hub_detect_retry_num);
	return;
}

static void usb_hub_detect_work(struct work_struct *work)
{
	struct usb_hub_chip *chip = container_of(to_delayed_work(work),
				struct usb_hub_chip, hub_detect_work);
	int ret = 0;

	if (chip->id_state == -1) {
		usb_hub_all_power_on(chip);
		chip->hub_pwr_enable = true;
	}

	chip->id_state = chip->host_detect_gpiod ?
		gpiod_get_value_cansleep(chip->host_detect_gpiod) : 1;
	g_extconUsbGpioState = chip->id_state;

	pr_err("usb_hub_detect_work, id_state=%d.\n", chip->id_state);

	if (chip->extcon == NULL) {
		pr_err("hub_host_detect_irq_handler extcon is null\n");
	} else {
		if (g_resetFlag) {
			ret = pinctrl_select_state(chip->pinctrl, chip->hub_reset_default);
			if (ret) {
				pr_err("hub reset default error:pinctrl_select_state wrong\n");
			}
			g_resetFlag = false;
			usb_hub_notify_usb_host(chip, false);
			usb_hub_notify_device_mode(chip, false);
			queue_delayed_work(system_power_efficient_wq, &chip->hub_detect_work, msecs_to_jiffies(USB_HUB_RESET_EXCUTE_MS));
		} else {
			if (chip->id_state) {
				pr_err("HUB switch MSM to device mode!!!\n");
				usb_hub_notify_device_mode(chip, true);
			} else {
				pr_err("HUB switch MSM to HOST(OTG) mode!!!\n");
				usb_hub_notify_usb_host(chip, true);
				chip->hub_detect_retry_flag = 1;
				chip->hub_detect_retry_num = 0;
				queue_delayed_work(system_power_efficient_wq, &chip->hub_detect_retry_work, msecs_to_jiffies(USB_HUB_DETECT_RETRY_TIME));
			}
		}
	}
	
	return;
}

static int extcon_input_switch_mode_init(struct usb_hub_chip *chip)
{
	int gpio_state = 0;
	struct input_dev *hub_input_dev = chip->hub_input_dev;

	gpio_state = gpiod_get_value(chip->host_detect_gpiod);
	if (gpio_state == 1) {
		input_report_key(hub_input_dev, KEY_HOSTDETECT, 1);
		input_sync(hub_input_dev);
	} else {
		input_report_key(hub_input_dev, KEY_HOSTDETECT, 0);
		input_sync(hub_input_dev);
	}

	return 0;
}

static irqreturn_t hub_host_detect_irq_handler(int irq, void *data)
{
	struct usb_hub_chip *chip = (struct usb_hub_chip *)data;
	int ret = 0;

	if (!chip) {
		pr_err("hub_host_detect_irq_handler null data\n");
		return IRQ_HANDLED;
	}

	if (chip->id_state == -1) {
		pr_err("hub_host_detect_irq_handler  waiting for usb hub first detect worker.\n");
		return IRQ_HANDLED;
	}

	pr_err("hub_host_detect_irq_handler in.\n");

	cancel_delayed_work_sync(&chip->hub_detect_work);
	cancel_delayed_work_sync(&chip->hub_detect_retry_work);
	chip->hub_detect_retry_flag = 0;
	chip->hub_detect_retry_num = 0;
	set_hub_all_flag_default();

	ret = pinctrl_select_state(chip->pinctrl, chip->hub_reset_active);
	if (ret) {
		pr_err("hub reset active error:pinctrl_select_state wrong\n");
	}

	g_resetFlag = true;
	queue_delayed_work(system_power_efficient_wq, &chip->hub_detect_work, msecs_to_jiffies(USB_HUB_RESET_MS));

	extcon_input_switch_mode_init(chip);
	pr_err("hub_host_detect_irq_handler out\n");

	return IRQ_HANDLED;
}

static int host_detect_irq_init(struct device *dev, struct usb_hub_chip *chip)
{
	int ret = 0;

	chip->host_detect_gpiod = devm_gpiod_get_optional(dev, "host-detect", GPIOD_IN);

	if (!chip->host_detect_gpiod) {
		dev_err(dev, "hub  failed to get gpios\n");
		return -ENODEV;
	}

	if (IS_ERR(chip->host_detect_gpiod))
		return PTR_ERR(chip->host_detect_gpiod);

	if (chip->host_detect_gpiod)
		ret = gpiod_set_debounce(chip->host_detect_gpiod,
					 USB_GPIO_DEBOUNCE_MS * MS_TO_USEC);

	if (chip->host_detect_gpiod) {
		chip->host_detect_irq = gpiod_to_irq(chip->host_detect_gpiod);
		if (chip->host_detect_irq < 0) {
			dev_err(dev, "hub failed to get ID IRQ\n");
			return chip->host_detect_irq;
		}

		ret = devm_request_threaded_irq(dev, chip->host_detect_irq, NULL,
					hub_host_detect_irq_handler,
					IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
					"hub_host_detect_GPIO", chip);
		if (ret < 0) {
			dev_err(dev, "hub failed to request handler for ID IRQ\n");
			return ret;
		}

		/* enable irq wake up */
		enable_irq_wake(chip->host_detect_irq);
	}

	return ret;
}

static int extcon_input_load_switch_cdb_init(struct usb_hub_chip *chip)
{
	int gpio_state = 0;
	struct input_dev *hub_input_dev = chip->hub_input_dev;

	gpio_state = gpiod_get_value(chip->gpio_cdb_int);
	if (gpio_state == 1) {
		input_report_key(hub_input_dev, KEY_OCPDETECT_CDB, 1);
		input_sync(hub_input_dev);
	} else {
		input_report_key(hub_input_dev, KEY_OCPDETECT_CDB, 0);
		input_sync(hub_input_dev);
	}

	return 0;
}

static int extcon_input_load_switch_sky_init(struct usb_hub_chip *chip)
{
	int gpio_state = 0;
	struct input_dev *hub_input_dev = chip->hub_input_dev;

	gpio_state = gpiod_get_value(chip->gpio_sky_int);
	if (gpio_state == 1) {
		input_report_key(hub_input_dev, KEY_OCPDETECT_SKY, 1);
		input_sync(hub_input_dev);
	} else {
		input_report_key(hub_input_dev, KEY_OCPDETECT_SKY, 0);
		input_sync(hub_input_dev);
	}

	return 0;
}

static irqreturn_t hub_ocp_detect_cdb_irq_handler(int irq, void *data)
{
	struct usb_hub_chip *chip = (struct usb_hub_chip *)data;

	pr_err("hub_ocp_detect_irq_handler init.");

	extcon_input_load_switch_cdb_init(chip);

	pr_err("hub_ocp_detect_irq_handler init success.");
	return IRQ_HANDLED;
}

static irqreturn_t hub_ocp_detect_sky_irq_handler(int irq, void *data)
{
	struct usb_hub_chip *chip = (struct usb_hub_chip *)data;

	pr_err("hub_ocp_detect_sky_irq_handler init.");

	extcon_input_load_switch_sky_init(chip);

	pr_err("hub_ocp_detect_sky_irq_handler init success.");
	return IRQ_HANDLED;
}

static int ocp_detect_irq_init(struct device *dev, struct usb_hub_chip *chip)
{
	int ret;

	ret = gpiod_set_debounce(chip->gpio_cdb_int,
					USB_GPIO_DEBOUNCE_MS * MS_TO_USEC);

	ret = gpiod_set_debounce(chip->gpio_sky_int,
					USB_GPIO_DEBOUNCE_MS * MS_TO_USEC);

	chip->ocp_detect_irq = gpiod_to_irq(chip->gpio_cdb_int);
	if (chip->ocp_detect_irq < 0) {
		dev_err(dev, "hub failed to get OCP IRQ\n");
		return chip->ocp_detect_irq;
	}

	chip->ocp_detect_sky_irq = gpiod_to_irq(chip->gpio_sky_int);
	if (chip->ocp_detect_sky_irq < 0) {
		dev_err(dev, "hub failed to get OCP sky IRQ\n");
		return chip->ocp_detect_sky_irq;
	}

	ret = devm_request_threaded_irq(dev, chip->ocp_detect_irq, NULL,
				hub_ocp_detect_cdb_irq_handler,
				IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
				"hub_ocp_detect_GPIO", chip);
	if (ret < 0) {
		dev_err(dev, "hub failed to request handler for ocp IRQ\n");
		return ret;
	}

	ret = devm_request_threaded_irq(dev, chip->ocp_detect_sky_irq, NULL,
				hub_ocp_detect_sky_irq_handler,
				IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
				"hub_ocp_detect_sky_GPIO", chip);
	if (ret < 0) {
		dev_err(dev, "hub failed to request handler for ocp IRQ\n");
		return ret;
	}

	/* enable irq wake up */
	enable_irq_wake(chip->ocp_detect_irq);
	enable_irq_wake(chip->ocp_detect_sky_irq);

	return ret;
}

static int regulator_init(struct device *dev, struct usb_hub_chip *chip)
{
	int ret = 0;

	chip->vdd = devm_regulator_get(dev, "vdd");
	if (IS_ERR_OR_NULL(chip->vdd)) {
		if (PTR_ERR(chip->vdd) != -EPROBE_DEFER) {
			pr_err("%s err %d\n", __func__, PTR_ERR(chip->vdd));
		} else {
			pr_warn("%s err %d vdd not ready\n", __func__, PTR_ERR(chip->vdd));
		}
		return PTR_ERR(chip->vdd);
	}

	return ret;
}

static int power_set_default(struct device *dev, struct usb_hub_chip *chip)
{
	int ret = 0;
	int min_uV = 3500000;
	int max_uV = 3500000;
	int min_uA = 800000;
	int max_uA = 800000;
	int uA_load = min_uA;
	int current_type = get_board_id_name();
	int verify_num = setup_secureflag_para();

	pr_err("=================chip->vdd ================= %p.\n", chip->vdd);
	if (!IS_ERR(chip->vdd)) {
		pr_err("hub  set ldo voltage %d to %d.\n", min_uV, max_uV);
		ret = regulator_set_voltage(chip->vdd, min_uV, max_uV);
		if (ret) {
			pr_err("%s: hub regulator_set_voltage(L22A)failed. min_uV=%d,max_uV=%d,ret=%d.\n",
					__func__, min_uV, max_uV, ret);
		}

		pr_err("hub  set ldo current %d to %d.\n", min_uA, max_uA);
		ret = regulator_set_current_limit(chip->vdd, min_uA, max_uA);
		if (ret) {
			pr_err("%s: hub regulator_set_current(L22A)failed. min_uA=%d,max_uA=%d,ret=%d.\n",
					__func__, min_uA, max_uA, ret);
		}

		pr_err("hub  set ldo load %d.\n", uA_load);
		ret = regulator_set_load(chip->vdd, uA_load);
		if (ret < 0) {
			pr_err("%s: hub regulator_set_load(reg=L22A,uA_load=%d) failed. ret=%d.\n",
					__func__, uA_load, ret);
		}

		ret = regulator_enable(chip->vdd);
		if (ret) {
			pr_err("hub  Unable to enable VDD\n");
			return -1;
		}
	}

	ret = pinctrl_select_state(chip->pinctrl, chip->pin_switch_active);
	if (ret) {
		pr_err("usb-hub default error:pinctrl_select_state wrong\n");
	}

	if (release_version == 1 && verify_num == 0) {
		if (current_type == BOARD_ID_NAME_DVT) {
			ret = pinctrl_select_state(chip->pinctrl, chip->pin_power_default);
			if (ret) {
				pr_err("usb-hub hub power default error:pin_power_default wrong\n");
			}
		}
	}

	pinctrl_select_state(chip->pinctrl, chip->pin_vbus_default);
	if (ret) {
		pr_err("usb-hub vbus default error:pinctrl_select_state wrong\n");
	}

	/* set hub all power in default mode*/
	ret = pinctrl_select_state(chip->pinctrl, chip->hub_reset_default);
	if (ret) {
		pr_err("hub reset default error:pinctrl_select_state wrong\n");
	}

	ret = pinctrl_select_state(chip->pinctrl, chip->pin_cdb_default);
	if (ret) {
		pr_err("usb-hub cdb default error:pinctrl_select_state wrong\n");
	}

	ret = pinctrl_select_state(chip->pinctrl, chip->pin_sky_default);
	if (ret) {
		pr_err("usb-hub sky default error:pinctrl_select_state wrong\n");
	}

	ret = pinctrl_select_state(chip->pinctrl, chip->pin_sky_pwr_default);
	if (ret) {
		pr_err("usb-hub sky chip default error:pinctrl_select_state wrong\n");
	}

	ret = pinctrl_select_state(chip->pinctrl, chip->pin_ext_default);
	if (ret) {
		pr_err("usb-hub ethernet default error:pinctrl_select_state wrong\n");
	}

	ret = pinctrl_select_state(chip->pinctrl, chip->pin_cdb_int_default);
	if (ret) {
		pr_err("usb-hub cdb int default error:pinctrl_select_state wrong\n");
	}

	ret = pinctrl_select_state(chip->pinctrl, chip->pin_sky_int_default);
	if (ret) {
		pr_err("usb-hub sky int default error:pinctrl_select_state wrong\n");
	}

	pr_err("usb-hub power init done\n");

	return ret;
}

static int usb_hub_gpio_get_state(struct device *dev, struct usb_hub_chip *chip)
{
	int ret = 0;

	if (dev == NULL || chip == NULL) {
		pr_err("hub devm_gpio_get_state data is NULL\n");
		return -1;
	}

	chip->gpio_sky = devm_gpiod_get_optional(dev, "sky", GPIOD_OUT_LOW);
	if (!chip->gpio_sky) {
		pr_err("gpio sky get failed!\n");
		return -ENODEV;
	}

	chip->gpio_sky_pair = devm_gpiod_get_optional(dev, "sky-pair", GPIOD_OUT_LOW);
	if (!chip->gpio_sky_pair) {
		pr_err("gpio sky reset get failed!\n");
		return -ENODEV;
	}

	chip->gpio_ext = devm_gpiod_get_optional(dev, "ext", GPIOD_OUT_LOW);
	if (!chip->gpio_ext) {
		pr_err("gpio ext get failed!\n");
		return -ENODEV;
	}

	chip->gpio_cdb = devm_gpiod_get_optional(dev, "cdb", GPIOD_OUT_LOW);
	if (!chip->gpio_cdb) {
		pr_err("gpio cdb get failed!\n");
		return -ENODEV;
	}

	chip->gpio_vbus = devm_gpiod_get_optional(dev, "vbus", GPIOD_OUT_LOW);
	if (!chip->gpio_vbus) {
		pr_err("gpio vbus get failed!\n");
		return -ENODEV;
	}

	chip->gpio_hub_power = devm_gpiod_get_optional(dev, "hub-power", GPIOD_OUT_LOW);
	if (!chip->gpio_hub_power) {
		pr_err("gpio hub power get failed!\n");
		return -ENODEV;
	}

	chip->gpio_dev = devm_gpiod_get_optional(dev, "dev", GPIOD_OUT_LOW);
	if (!chip->gpio_dev) {
		pr_err("gpio dev get failed!\n");
		return -ENODEV;
	}

	chip->gpio_hub_reset = devm_gpiod_get_optional(dev, "hub-reset", GPIOD_OUT_LOW);
	if (!chip->gpio_hub_reset) {
		pr_err("gpio hub reset get failed!\n");
		return -ENODEV;
	}

	chip->gpio_sky_reset = devm_gpiod_get_optional(dev, "sky-reset", GPIOD_OUT_LOW);
	if (!chip->gpio_sky_reset) {
		pr_err("gpio sky reset get failed!\n");
		return -ENODEV;
	}

	chip->gpio_cdb_int = devm_gpiod_get_optional(dev, "cdb-int", GPIOD_IN);
	if (!chip->gpio_cdb_int) {
		pr_err("gpio hub power get failed!\n");
		return -ENODEV;
	}

	chip->gpio_sky_int = devm_gpiod_get_optional(dev, "sky-int", GPIOD_IN);
	if (!chip->gpio_sky_int) {
		pr_err("gpio hub power get failed!\n");
		return -ENODEV;
	}

	return ret;
}

static int pinctrl_init(struct device *dev, struct usb_hub_chip *chip)
{
	int ret = 0;

	if (dev == NULL || chip == NULL) {
		pr_err("hub pinctrl_init data is NULL\n");
		return -1;
	}

	usb_hub_gpio_get_state(dev, chip);

	chip->pinctrl = devm_pinctrl_get(dev);
	if (IS_ERR_OR_NULL(chip->pinctrl)) {
		if (PTR_ERR(chip->pinctrl) != -EPROBE_DEFER) {
			pr_err("%s err %d\n", __func__, PTR_ERR(chip->pinctrl));
		} else {
			pr_warn("%s err %d pinctrl not ready\n", __func__, PTR_ERR(chip->pinctrl));
		}
		return PTR_ERR(chip->pinctrl);
	}
	chip->pin_switch_default = pinctrl_lookup_state(chip->pinctrl, "default");
	if (IS_ERR_OR_NULL(chip->pin_switch_default)) {
		pr_err("usb-hub default error:pinctrl_lookup_state wrong\n");
		return -1;
	}
	chip->pin_switch_active = pinctrl_lookup_state(chip->pinctrl, "active");
	if (IS_ERR_OR_NULL(chip->pin_switch_active)) {
		pr_err("usb-hub active error:pinctrl_lookup_state wrong\n");
		return -1;
	}

	chip->hub_reset_default = pinctrl_lookup_state(chip->pinctrl, "reset_default");
	if (IS_ERR_OR_NULL(chip->hub_reset_default)) {
		pr_err("hub reset default error:pinctrl_lookup_state wrong\n");
		return -1;
	}
	chip->hub_reset_active = pinctrl_lookup_state(chip->pinctrl, "reset_active");
	if (IS_ERR_OR_NULL(chip->hub_reset_active)) {
		pr_err("hub reset active error:pinctrl_lookup_state wrong\n");
		return -1;
	}

	chip->pin_sky_default = pinctrl_lookup_state(chip->pinctrl, "sky_default");
	if (IS_ERR_OR_NULL(chip->pin_sky_default)) {
		pr_err("sky default error:pinctrl_lookup_state wrong\n");
		return -1;
	}

	chip->pin_sky_active = pinctrl_lookup_state(chip->pinctrl, "sky_active");
	if (IS_ERR_OR_NULL(chip->pin_sky_active)) {
		pr_err("sky active error:pinctrl_lookup_state wrong\n");
		return -1;
	}

	chip->pin_sky_pwr_default = pinctrl_lookup_state(chip->pinctrl, "sky_pwr_default");
	if (IS_ERR_OR_NULL(chip->pin_sky_pwr_default)) {
		pr_err("sky default error:pinctrl_lookup_state wrong\n");
		return -1;
	}

	chip->pin_sky_pwr_active = pinctrl_lookup_state(chip->pinctrl, "sky_pwr_active");
	if (IS_ERR_OR_NULL(chip->pin_sky_pwr_active)) {
		pr_err("sky active error:pinctrl_lookup_state wrong\n");
		return -1;
	}

	chip->pin_skyConn_default = pinctrl_lookup_state(chip->pinctrl, "skyConn_default");
	if (IS_ERR_OR_NULL(chip->pin_skyConn_default)) {
		pr_err("sky conn default error:pinctrl_lookup_state wrong\n");
		return -1;
	}

	chip->pin_skyConn_active = pinctrl_lookup_state(chip->pinctrl, "skyConn_active");
	if (IS_ERR_OR_NULL(chip->pin_skyConn_active)) {
		pr_err("sky conn active error:pinctrl_lookup_state wrong\n");
		return -1;
	}

	chip->pin_ext_default = pinctrl_lookup_state(chip->pinctrl, "ext_default");
	if (IS_ERR_OR_NULL(chip->pin_ext_default)) {
		pr_err("ext default error:pinctrl_lookup_state wrong\n");
		return -1;
	}

	chip->pin_ext_active = pinctrl_lookup_state(chip->pinctrl, "ext_active");
	if (IS_ERR_OR_NULL(chip->pin_ext_active)) {
		pr_err("ext active error:pinctrl_lookup_state wrong\n");
		return -1;
	}

	chip->pin_cdb_default = pinctrl_lookup_state(chip->pinctrl, "cdb_default");
	if (IS_ERR_OR_NULL(chip->pin_cdb_default)) {
		pr_err("cdb default error:pinctrl_lookup_state wrong\n");
		return -1;
	}

	chip->pin_cdb_active = pinctrl_lookup_state(chip->pinctrl, "cdb_active");
	if (IS_ERR_OR_NULL(chip->pin_cdb_active)) {
		pr_err("cdb active error:pinctrl_lookup_state wrong\n");
		return -1;
	}

	chip->pin_vbus_default = pinctrl_lookup_state(chip->pinctrl, "vbus_default");
	if (IS_ERR_OR_NULL(chip->pin_vbus_default)) {
		pr_err("vbus default error:pinctrl_lookup_state wrong\n");
		return -1;
	}

	chip->pin_vbus_active = pinctrl_lookup_state(chip->pinctrl, "vbus_active");
	if (IS_ERR_OR_NULL(chip->pin_vbus_active)) {
		pr_err("vbus active error:pinctrl_lookup_state wrong\n");
		return -1;
	}

	chip->pin_power_default = pinctrl_lookup_state(chip->pinctrl, "power_default");
	if (IS_ERR_OR_NULL(chip->pin_power_default)) {
		pr_err("power default error:pinctrl_lookup_state wrong\n");
		return -1;
	}

	chip->pin_power_active = pinctrl_lookup_state(chip->pinctrl, "power_active");
	if (IS_ERR_OR_NULL(chip->pin_power_active)) {
		pr_err("power active error:pinctrl_lookup_state wrong\n");
		return -1;
	}

	chip->pin_dev_default = pinctrl_lookup_state(chip->pinctrl, "dev_default");
	if (IS_ERR_OR_NULL(chip->pin_dev_default)) {
		pr_err("dev default error:pinctrl_lookup_state wrong\n");
		return -1;
	}

	chip->pin_dev_active = pinctrl_lookup_state(chip->pinctrl, "dev_active");
	if (IS_ERR_OR_NULL(chip->pin_dev_active)) {
		pr_err("dev active error:pinctrl_lookup_state wrong\n");
		return -1;
	}

	chip->pin_sky_reset_default = pinctrl_lookup_state(chip->pinctrl, "sky_reset_default");
	if (IS_ERR_OR_NULL(chip->pin_sky_reset_default)) {
		pr_err("sky reset default error:pinctrl_lookup_state wrong\n");
		return -1;
	}

	chip->pin_sky_reset_active = pinctrl_lookup_state(chip->pinctrl, "sky_reset_active");
	if (IS_ERR_OR_NULL(chip->pin_sky_reset_active)) {
		pr_err("sky reset active error:pinctrl_lookup_state wrong\n");
		return -1;
	}

	chip->pin_cdb_int_default = pinctrl_lookup_state(chip->pinctrl, "cdbInt_default");
	if (IS_ERR_OR_NULL(chip->pin_cdb_int_default)) {
		pr_err("cdb int default error:pinctrl_lookup_state wrong\n");
		return -1;
	}

	chip->pin_sky_int_default = pinctrl_lookup_state(chip->pinctrl, "skyInt_default");
	if (IS_ERR_OR_NULL(chip->pin_sky_int_default)) {
		pr_err("sky int default error:pinctrl_lookup_state wrong\n");
		return -1;
	}

	return ret;
}

static int qpnp_usb_hub_extcon_register(struct device *dev, struct usb_hub_chip *chip)
{
	int rc = 0;
	if (dev == NULL || chip == NULL) {
		pr_err("qpnp_usb_hub_extcon_register data is NULL\n");
		return -1;
	}

	/* extcon registration */
	chip->extcon = devm_extcon_dev_allocate(dev, hub_extcon_cable);
	if (IS_ERR(chip->extcon)) {
		rc = PTR_ERR(chip->extcon);
		dev_err(dev, "hub failed to allocate extcon device rc=%d\n", rc);
		goto register_error;
	}

	rc = devm_extcon_dev_register(dev, chip->extcon);
	if (rc < 0) {
		dev_err(dev, "hub failed to register extcon device rc=%d\n", rc);
		goto register_error;
	}

	/* Support reporting polarity and speed via properties */
	rc = extcon_set_property_capability(chip->extcon,
			EXTCON_USB, EXTCON_PROP_USB_TYPEC_POLARITY);
	rc |= extcon_set_property_capability(chip->extcon,
			EXTCON_USB, EXTCON_PROP_USB_SS);
	rc |= extcon_set_property_capability(chip->extcon,
			EXTCON_USB_HOST, EXTCON_PROP_USB_TYPEC_POLARITY);
	rc |= extcon_set_property_capability(chip->extcon,
			EXTCON_USB_HOST, EXTCON_PROP_USB_SS);
	if (rc < 0) {
		dev_err(dev, "hub failed to configure extcon capabilities\n");
		goto register_error;
	}

	pr_err("qpnp_usb_hub_extcon_register success!\n");

	return 0;

register_error:
	return -1;
}

static int extcon_input_load_switch_init(struct device *dev, struct usb_hub_chip *chip)
{
	int ret = 0;

	chip->hub_input_dev = devm_input_allocate_device(dev);
	if (!chip->hub_input_dev) {
		pr_err("Failed to allocate memory for hub_input device");
		return -ENOMEM;
	}

	chip->hub_input_dev->name = "hub-detect";
	chip->hub_input_dev->phys = "/host-detect/input0";

	chip->hub_input_dev->evbit[0] = BIT_MASK(EV_KEY);
	input_set_capability(chip->hub_input_dev, EV_KEY, KEY_OCPDETECT_CDB);
	input_set_capability(chip->hub_input_dev, EV_KEY, KEY_OCPDETECT_SKY);
	input_set_capability(chip->hub_input_dev, EV_KEY, KEY_HOSTDETECT);

	ret = input_register_device(chip->hub_input_dev);
	if (ret) {
		pr_err("Input device registration failed");
		input_free_device(chip->hub_input_dev);
		chip->hub_input_dev = NULL;
		return ret;
	}

	return ret;
}

//hub all power ctrl, [1] power-on [0] power-off
static ssize_t qpnp_hub_all_pwr_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int ret;
	struct usb_hub_chip *chip = dev_get_drvdata(dev);
	if (!chip) {
		pr_err("hub all pwr show NULL!\n");
		return 0;
	}

	ret = chip->hub_pwr_enable;
	pr_err("qpnp_smb5_usb:show hub_all_pwr_state = %d\n", ret);

	return snprintf(buf, PAGE_SIZE, "%d\n", ret);
}

static ssize_t qpnp_hub_all_pwr_store(struct device *dev,
				struct device_attribute *attr, const char *buf, size_t count)
{
	int ret, val;
	struct usb_hub_chip *chip = dev_get_drvdata(dev);
	if (!chip) {
		pr_err("hub all pwr store NULL!\n");
		return 0;
	}

	ret = kstrtouint(buf, 0, &val);
	if (ret < 0) {
		return ret;
	}

	pr_err("chip->hub_pwr_enable == %d, val = %d\n", chip->hub_pwr_enable, val);
	if(chip->hub_pwr_enable == val) {
		return count;
	}
	hub_all_power_switch(chip, val);

	pr_err("qpnp_smb5_usb:hub_all_pwr_state = %d\n", val);

	return count;
}

//1 as active, 0 as default
static ssize_t qpnp_hub_show_sky(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int ret;
	int sky_state;
	struct usb_hub_chip *chip = dev_get_drvdata(dev);
	if (!chip) {
		pr_err("sky chip show NULL!\n");
		return 0;
	}

	ret = gpiod_get_value(chip->gpio_sky);
	if (ret) {
		sky_state =  1;
	} else {
		sky_state =  0;
	}
	pr_err("qpnp_smb5_usb:show sky_state = %d\n", sky_state);

	return snprintf(buf, PAGE_SIZE, "%d\n", sky_state);
}

static ssize_t qpnp_hub_store_sky(struct device *dev,
				struct device_attribute *attr, const char *buf, size_t count)
{
	int ret, val;
	int hub_state = -1;
	struct usb_hub_chip *chip = dev_get_drvdata(dev);
	if (!chip) {
		pr_err("sky chip store NULL!\n");
		return 0;
	}

	hub_state = chip->host_detect_gpiod ?
		gpiod_get_value_cansleep(chip->host_detect_gpiod) : 1;
	ret = kstrtouint(buf, 0, &val);
	if (ret < 0) {
		return ret;
	}

	if (val == 1) {
		pinctrl_select_state(chip->pinctrl, chip->pin_sky_active);
	} else if (val == 0) {
		if (!hub_state) {
			pinctrl_select_state(chip->pinctrl, chip->pin_sky_default);
		}
	}
	pr_err("qpnp_smb5_usb:store_sky_state = %d\n", val);

	return count;
}

static ssize_t qpnp_hub_show_skyConn(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int ret;
	int sky_pair_state;
	struct usb_hub_chip *chip = dev_get_drvdata(dev);
	if (!chip) {
		pr_err("sky chip show NULL!\n");
		return 0;
	}

	ret = gpiod_get_value(chip->gpio_sky_pair);
	if (ret) {
		sky_pair_state =  1;
	} else {
		sky_pair_state =  0;
	}
	pr_err("qpnp_smb5_usb:show sky_pair_state = %d\n", sky_pair_state);

	return snprintf(buf, PAGE_SIZE, "%d\n", sky_pair_state);
}

static ssize_t qpnp_hub_store_skyConn(struct device *dev,
				struct device_attribute *attr, const char *buf, size_t count)
{
	int ret, val;
	struct usb_hub_chip *chip = dev_get_drvdata(dev);
	if (!chip) {
		pr_err("sky chip store NULL!\n");
		return 0;
	}

	ret = kstrtouint(buf, 0, &val);
	if (ret < 0) {
		return ret;
	}

	if (val == 1) {
		pinctrl_select_state(chip->pinctrl, chip->pin_skyConn_default);
		msleep(SKY_CONNECT_DELAY);
		pinctrl_select_state(chip->pinctrl, chip->pin_skyConn_active);
	} else if (val == 0) {
		pinctrl_select_state(chip->pinctrl, chip->pin_skyConn_default);
	}
	pr_err("qpnp_smb5_usb:store_skyPair_state = %d\n", val);

	return count;
}

static ssize_t qpnp_hub_show_ext(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int ret;
	int ext_state;
	struct usb_hub_chip *chip = dev_get_drvdata(dev);
	if (!chip) {
		pr_err("ext chip show NULL!\n");
		return 0;
	}

	ret = gpiod_get_value(chip->gpio_ext);
	if (ret) {
		ext_state =  1;
	} else {
		ext_state =  0;
	}
	pr_err("qpnp_smb5_usb:show ext_state = %d\n", ext_state);

	return snprintf(buf, PAGE_SIZE, "%d\n", ext_state);
}

static ssize_t qpnp_hub_store_ext(struct device *dev,
				struct device_attribute *attr, const char *buf, size_t count)
{
	int ret, val;
	struct usb_hub_chip *chip = dev_get_drvdata(dev);
	if (!chip) {
		pr_err("ext chip store NULL!\n");
		return 0;
	}

	ret = kstrtouint(buf, 0, &val);
	if (ret < 0) {
		return ret;
	}

	if (val == 1) {
		pinctrl_select_state(chip->pinctrl, chip->pin_ext_active);
	} else if (val == 0) {
		pinctrl_select_state(chip->pinctrl, chip->pin_ext_default);
	}
	pr_err("qpnp_smb5_usb:store_ext_state = %d\n", val);

	return count;
}

static ssize_t qpnp_hub_show_cdb(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int ret;
	int cdb_state;
	struct usb_hub_chip *chip = dev_get_drvdata(dev);
	if (!chip) {
		pr_err("cdb chip show NULL!\n");
		return 0;
	}

	ret = gpiod_get_value(chip->gpio_cdb);
	if (ret) {
		cdb_state =  1;
	} else {
		cdb_state =  0;
	}
	pr_err("qpnp_smb5_usb:show cdb_state = %d\n", cdb_state);

	return snprintf(buf, PAGE_SIZE, "%d\n", cdb_state);
}

static ssize_t qpnp_hub_store_cdb(struct device *dev,
				struct device_attribute *attr, const char *buf, size_t count)
{
	int ret, val;
	int hub_state = -1;
	struct usb_hub_chip *chip = dev_get_drvdata(dev);
	if (!chip) {
		pr_err("cdb chip store NULL!\n");
		return 0;
	}

	hub_state = chip->host_detect_gpiod ?
		gpiod_get_value_cansleep(chip->host_detect_gpiod) : 1;
	ret = kstrtouint(buf, 0, &val);
	if (ret < 0) {
		return ret;
	}

	if (val == 1) {
		pinctrl_select_state(chip->pinctrl, chip->pin_cdb_active);
	} else if (val == 0) {
		if (!hub_state) {
			pinctrl_select_state(chip->pinctrl, chip->pin_cdb_default);
		}
	}
	pr_err("qpnp_smb5_usb:store_cdb_state = %d\n", val);

	return count;
}

static ssize_t qpnp_hub_show_vbus(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int ret;
	int vbus_state;
	struct usb_hub_chip *chip = dev_get_drvdata(dev);
	if (!chip) {
		pr_err("vbus chip show NULL!\n");
		return 0;
	}

	ret = gpiod_get_value(chip->gpio_vbus);
	if (ret) {
		vbus_state =  1;
	} else {
		vbus_state =  0;
	}
	pr_err("qpnp_smb5_usb:show vbus_state = %d\n", vbus_state);

	return snprintf(buf, PAGE_SIZE, "%d\n", vbus_state);
}

static ssize_t qpnp_hub_store_vbus(struct device *dev,
				struct device_attribute *attr, const char *buf, size_t count)
{
	int ret, val;
	int hub_state = -1;
	struct usb_hub_chip *chip = dev_get_drvdata(dev);
	if (!chip) {
		pr_err("vbus chip store NULL!\n");
		return 0;
	}

	hub_state = chip->host_detect_gpiod ?
		gpiod_get_value_cansleep(chip->host_detect_gpiod) : 1;
	ret = kstrtouint(buf, 0, &val);
	if (ret < 0) {
		return ret;
	}

	if (val == 1) {
		pinctrl_select_state(chip->pinctrl, chip->pin_vbus_active);
		if (!hub_state) {
			usb_hub_notify_usb_host(chip, true);
		}
	} else if (val == 0) {
		if (!hub_state) {
			pinctrl_select_state(chip->pinctrl, chip->pin_vbus_default);
			usb_hub_notify_usb_host(chip, false);
			usb_hub_notify_device_mode(chip, false);
		}
	}
	pr_err("qpnp_smb5_usb:store_vbus_state = %d\n", val);

	return count;
}

static ssize_t qpnp_hub_show_hub_power(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int ret;
	int hub_power_state;
	struct usb_hub_chip *chip = dev_get_drvdata(dev);
	if (!chip) {
		pr_err("hub power chip show NULL!\n");
		return 0;
	}

	ret = gpiod_get_value(chip->gpio_hub_power);
	if (ret) {
		hub_power_state =  1;
	} else {
		hub_power_state =  0;
	}
	pr_err("qpnp_smb5_usb:show hub_power_state = %d\n", hub_power_state);

	return snprintf(buf, PAGE_SIZE, "%d\n", hub_power_state);
}

static ssize_t qpnp_hub_store_hub_power(struct device *dev,
				struct device_attribute *attr, const char *buf, size_t count)
{
    int ret, val;
	struct usb_hub_chip *chip = dev_get_drvdata(dev);
	if (!chip) {
		pr_err("hub power chip store NULL!\n");
		return 0;
	}

	ret = kstrtouint(buf, 0, &val);
	if (ret < 0) {
		return ret;
	}

	if (val == 1) {
		pinctrl_select_state(chip->pinctrl, chip->pin_power_active);
	} else if (val == 0) {
		pinctrl_select_state(chip->pinctrl, chip->pin_power_default);
	}
	pr_err("qpnp_smb5_usb:store_hub_power_state = %d\n", val);

	return count;
}

static ssize_t qpnp_hub_show_dev(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int ret;
	int dev_state;
	struct usb_hub_chip *chip = dev_get_drvdata(dev);
	if (!chip) {
		pr_err("dev chip show NULL!\n");
		return 0;
	}

	ret = gpiod_get_value(chip->gpio_dev);
	if (ret) {
		dev_state =  1;
	} else {
		dev_state =  0;
	}
	pr_err("qpnp_smb5_usb:show dev_state = %d\n", dev_state);

	return snprintf(buf, PAGE_SIZE, "%d\n", dev_state);
}

static ssize_t qpnp_hub_store_dev(struct device *dev,
				struct device_attribute *attr, const char *buf, size_t count)
{
    int ret, val;
	struct usb_hub_chip *chip = dev_get_drvdata(dev);
	if (!chip) {
		pr_err("dev chip store NULL!\n");
		return 0;
	}

	ret = kstrtouint(buf, 0, &val);
	if (ret < 0) {
		return ret;
	}

	if (val == 1) {
		pinctrl_select_state(chip->pinctrl, chip->pin_dev_active);
	} else if (val == 0) {
		pinctrl_select_state(chip->pinctrl, chip->pin_dev_default);
	}
	pr_err("qpnp_smb5_usb:store_dev_state = %d\n", val);

	return count;
}

static ssize_t qpnp_hub_show_hub_reset(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int ret;
	int hub_reset_state;
	struct usb_hub_chip *chip = dev_get_drvdata(dev);
	if (!chip) {
		pr_err("hub reset chip show NULL!\n");
		return 0;
	}

	ret = gpiod_get_value(chip->gpio_hub_reset);
	if (ret) {
		hub_reset_state =  1;
	} else {
		hub_reset_state =  0;
	}
	pr_err("qpnp_smb5_usb:show hub_reset_state = %d\n", hub_reset_state);

	return snprintf(buf, PAGE_SIZE, "%d\n", hub_reset_state);
}

static ssize_t qpnp_hub_store_hub_reset(struct device *dev,
				struct device_attribute *attr, const char *buf, size_t count)
{
	int ret, val;
	struct usb_hub_chip *chip = dev_get_drvdata(dev);
	if (!chip) {
		pr_err("hub reset chip store NULL!\n");
		return 0;
	}

	ret = kstrtouint(buf, 0, &val);
	if (ret < 0) {
		return ret;
	}

	if (val == 1) {
		pinctrl_select_state(chip->pinctrl, chip->hub_reset_active);
	} else if (val == 0) {
		pinctrl_select_state(chip->pinctrl, chip->hub_reset_default);
	}
	pr_err("qpnp_smb5_usb:store_hub_reset_state = %d\n", val);

	return count;
}

static ssize_t qpnp_hub_show_sky_reset(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int ret;
	int sky_reset_state;
	struct usb_hub_chip *chip = dev_get_drvdata(dev);
	if (!chip) {
		pr_err("sky reset chip show NULL!\n");
		return 0;
	}

	ret = gpiod_get_value(chip->gpio_sky_reset);
	if (ret) {
		sky_reset_state =  1;
	} else {
		sky_reset_state =  0;
	}
	pr_err("qpnp_smb5_usb:show sky_reset_state = %d\n", sky_reset_state);

	return snprintf(buf, PAGE_SIZE, "%d\n", sky_reset_state);
}

static ssize_t qpnp_hub_store_sky_reset(struct device *dev,
				struct device_attribute *attr, const char *buf, size_t count)
{
	int ret, val;
	struct usb_hub_chip *chip = dev_get_drvdata(dev);
	if (!chip) {
		pr_err("sky reset chip store NULL!\n");
		return 0;
	}

	ret = kstrtouint(buf, 0, &val);
	if (ret < 0) {
		return ret;
	}

	if (val == 1) {
		pinctrl_select_state(chip->pinctrl, chip->pin_sky_reset_active);
	} else if (val == 0) {
		pinctrl_select_state(chip->pinctrl, chip->pin_sky_reset_default);
	}
	pr_err("qpnp_smb5_usb:store_sky_reset_state = %d\n", val);

	return count;
}

static ssize_t qpnp_hub_show_detect(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int ret;
	int detect_state;
	struct usb_hub_chip *chip = dev_get_drvdata(dev);
	if (!chip) {
		pr_err("detect chip show NULL!\n");
		return 0;
	}

	ret = gpiod_get_value(chip->host_detect_gpiod);
	if (ret) {
		detect_state =  1;
	} else {
		detect_state =  0;
	}
	pr_err("qpnp_smb5_usb:show detect_state = %d\n", detect_state);

	return snprintf(buf, PAGE_SIZE, "%d\n", detect_state);
}

static ssize_t qpnp_hub_store_detect(struct device *dev,
				struct device_attribute *attr, const char *buf, size_t count)
{
	/* this is a input node, cannot store state */
	return count;
}

static ssize_t qpnp_hub_show_cdb_int(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int ret;
	int cdb_int_state;
	struct usb_hub_chip *chip = dev_get_drvdata(dev);
	if (!chip) {
		pr_err("cdb int chip show NULL!\n");
		return 0;
	}

	ret = gpiod_get_value(chip->gpio_cdb_int);
	if (ret) {
		cdb_int_state =  1;
	} else {
		cdb_int_state =  0;
	}
	pr_err("qpnp_smb5_usb:show cdb_int_state = %d\n", cdb_int_state);

	return snprintf(buf, PAGE_SIZE, "%d\n", cdb_int_state);
}

static ssize_t qpnp_hub_store_cdb_int(struct device *dev,
				struct device_attribute *attr, const char *buf, size_t count)
{
    /* this is a input node, cannot store state */
	return count;
}

static ssize_t qpnp_hub_show_sky_int(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int ret;
	int sky_int_state;
	struct usb_hub_chip *chip = dev_get_drvdata(dev);
	if (!chip) {
		pr_err("sky int chip show NULL!\n");
		return 0;
	}

	ret = gpiod_get_value(chip->gpio_sky_int);
	if (ret) {
		sky_int_state =  1;
	} else {
		sky_int_state =  0;
	}
	pr_err("qpnp_smb5_usb:show sky_int_state = %d\n", sky_int_state);

	return snprintf(buf, PAGE_SIZE, "%d\n", sky_int_state);
}

static ssize_t qpnp_hub_store_sky_int(struct device *dev,
				struct device_attribute *attr, const char *buf, size_t count)
{
    /* this is a input node, cannot store state */
	return count;
}

static ssize_t qpnp_hub_show_wake(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", 0);
}
static ssize_t qpnp_hub_store_wake(struct device *dev,
				struct device_attribute *attr, const char *buf, size_t count)
{
	int ret, val;
	struct usb_hub_chip *chip = dev_get_drvdata(dev);
	if (!chip) {
		pr_err("cdb sleep and wake NULL\n");
		return 0;
	}

	ret = kstrtouint(buf, 0, &val);
	if (ret < 0) {
		return ret;
	}

	if (val == 1) {
		schedule_work(&chip->restart_usb_work);
	} else if (val == 0) {
		// do nothing
	}
	pr_err("cdb sleep and wake, switch to store = %d\n", val);
	return count;
}

static struct device_attribute qpnp_hub_attrs[] = {
	__ATTR(skyCtrl, 0664, qpnp_hub_show_sky, qpnp_hub_store_sky),  //gpio17
	__ATTR(skyConnCtrl, 0664, qpnp_hub_show_skyConn, qpnp_hub_store_skyConn),  //gpio84
	__ATTR(extCtrl, 0664, qpnp_hub_show_ext, qpnp_hub_store_ext),  //gpio112
	__ATTR(cdbCtrl, 0664, qpnp_hub_show_cdb, qpnp_hub_store_cdb),  //gpio19
	__ATTR(vbusCtrl, 0664, qpnp_hub_show_vbus, qpnp_hub_store_vbus),  //gpio3
	__ATTR(hubPowerCtrl, 0664, qpnp_hub_show_hub_power, qpnp_hub_store_hub_power), //gpio75
	__ATTR(devCtrl, 0664, qpnp_hub_show_dev, qpnp_hub_store_dev),  //gpio36
	__ATTR(detectShow, 0664, qpnp_hub_show_detect, qpnp_hub_store_detect),  //gpio28
	__ATTR(hubResetCtrl, 0664, qpnp_hub_show_hub_reset, qpnp_hub_store_hub_reset),  //gpio18
	__ATTR(skyResetCtrl, 0664, qpnp_hub_show_sky_reset, qpnp_hub_store_sky_reset),  //gpio31
	__ATTR(cdbIntCtrl, 0664, qpnp_hub_show_cdb_int, qpnp_hub_store_cdb_int),  //gpio24
	__ATTR(skyIntCtrl, 0664, qpnp_hub_show_sky_int, qpnp_hub_store_sky_int),  //gpio25
	__ATTR(wakesleepCtrl, 0664, qpnp_hub_show_wake, qpnp_hub_store_wake),
	__ATTR(hubAllPowerCtrl, 0664, qpnp_hub_all_pwr_show, qpnp_hub_all_pwr_store),
};

static int qpnp_usb_hub_probe(struct platform_device *pdev)
{
	struct usb_hub_chip *chip;
	struct device *dev = &pdev->dev;
	int ret = 0;
	int i = 0;
	pr_err("USB-HUB start resgistered\n");
	chip = devm_kzalloc(dev, sizeof(*chip), GFP_KERNEL);
	if (!chip) {
		return -ENOMEM;
	}

	g_chargeChip = chip;
	chip->dev = dev;
	ret = pinctrl_init(dev, chip);
	if (ret) {
		pr_err("hub pinctrl init error %d\n", ret);
		return ret;
	}

	ret = regulator_init(dev, chip);
	if (ret) {
		pr_err("hub egulator init error %d\n", ret);
		return ret;
	}

	ret = power_set_default(dev, chip);
	if (ret) {
		pr_err("hub power set default error %d\n", ret);
	}

	INIT_DELAYED_WORK(&chip->hub_detect_work, usb_hub_detect_work);
	INIT_DELAYED_WORK(&chip->hub_detect_retry_work, usb_hub_detect_retry_work);
	INIT_WORK(&chip->hub_charge_work, switch_power_enable_work);
	INIT_WORK(&chip->restart_usb_work, dwc3_restart_usb_work);

	ret = qpnp_usb_hub_extcon_register(dev, chip);
	if (ret) {
		pr_err("hub qpnp_usb_hub_extcon_register error %d\n", ret);
	}

	ret = extcon_input_load_switch_init(dev, chip);
	if (ret) {
		pr_err("hub extcon_input_load_switch_init error %d\n", ret);
	}

	chip->id_state = -1;
	chip->hub_detect_retry_flag = 0;
	chip->hub_detect_retry_num = 0;
	ret = host_detect_irq_init(dev, chip);
	if (ret) {
		pr_err("hub host detect irq init error %d\n", ret);
	}

	ret = ocp_detect_irq_init(dev, chip);
	if (ret) {
		pr_err("hub ocp detect irq init error %d\n", ret);
	}

	queue_delayed_work(system_power_efficient_wq, &chip->hub_detect_work, msecs_to_jiffies(USB_HUB_START_DETECT_MS));

	for (i = 0; i < ARRAY_SIZE(qpnp_hub_attrs); i++) {
		ret = sysfs_create_file(&chip->dev->kobj, &qpnp_hub_attrs[i].attr);
		if (ret < 0) {
			dev_err(&pdev->dev, "Error in create sysfs file, ret = %d\n");
			goto sysfs_fail;
		}
	}
	platform_set_drvdata(pdev, chip);
	g_chargeActive = 1;

	pr_err("USB-HUB successfully resgistered\n");

	return 0;

sysfs_fail:
	for (; i >=0; i--)
		sysfs_remove_file(&chip->dev->kobj, &qpnp_hub_attrs[i].attr);
	return ret;
}

static int qpnp_usb_hub_remove(struct platform_device *pdev)
{
	struct usb_hub_chip *chip = platform_get_drvdata(pdev);

	cancel_delayed_work_sync(&chip->hub_detect_work);

	pr_err("USB-HUB remove done\n");

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int usb_hub_suspend(struct device *dev)
{
	// struct usb_hub_chip *chip = dev_get_drvdata(dev);
	int ret = 0;

	pr_debug("USB-HUB suspend\n");

	return ret;
}

static int usb_hub_resume(struct device *dev)
{
	struct usb_hub_chip *chip = dev_get_drvdata(dev);
	int ret = 0;
	int hub_state = -1;

	hub_state = extcon_usb_gpio_state();
	if (!g_getUsbDeviceState && !g_getUsbHostState) {
		pr_debug("HUB switch to none mode, not resume!!!\n");
	} else {
		if (hub_state) {
			pr_err("HUB resume to device mode!!!\n");
			usb_hub_notify_device_mode(chip, true);
		} else {
			pr_err("HUB resume to HOST(OTG) mode!!!\n");
			usb_hub_notify_usb_host(chip, true);
		}
	}

	pr_debug("USB-HUB resume\n");

	return ret;
}

static SIMPLE_DEV_PM_OPS(usb_hub_pm_ops,
			 usb_hub_suspend, usb_hub_resume);
#endif

static const struct of_device_id usb_match_table[] = {
	{.compatible = "qcom,qpnp-usb-hub-gpio"},
	{ },
};
MODULE_DEVICE_TABLE(of, usb_match_table);

static struct platform_driver qpnp_usb_hub_driver = {
	.driver = {
		.name = "qcom,qpnp-usb-hub-gpio",
		.owner = THIS_MODULE,
#ifdef CONFIG_PM_SLEEP
		.pm	= &usb_hub_pm_ops,
#endif
		.of_match_table	= usb_match_table,
	},
	.probe = qpnp_usb_hub_probe,
	.remove = qpnp_usb_hub_remove,
};
module_platform_driver(qpnp_usb_hub_driver);

MODULE_DESCRIPTION("QPNP SMB5 USB HUB Driver");
MODULE_LICENSE("GPL v2");
