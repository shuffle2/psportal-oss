# SPDX-License-Identifier: GPL-2.0
ifneq ($(HQ_FACTORY_BUILD),true)  # !fac
	ifeq ($(TARGET_BUILD_VARIANT),user)   #user
		EXTRA_CFLAGS	+= -DRELEASE_VERSION
	endif
endif

obj-$(CONFIG_HQ_HARDWARE_INFO)			+= hardware_info.o
obj-$(CONFIG_EXTCON_HUB_GPIO) += extcon-hub-gpio.o
