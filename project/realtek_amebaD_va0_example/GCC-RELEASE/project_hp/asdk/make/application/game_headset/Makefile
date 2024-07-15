
include $(MAKE_INCLUDE_GEN)

ifeq ($(CONFIG_USB_DEVICE_OLD_EN),y)
IFLAGS += -I$(TARGETDIR)/fwlib/usb_otg/device/inc
IFLAGS += -I$(BASEDIR)/component/common/drivers/usb/device_old/audio/inc
IFLAGS += -I$(BASEDIR)/component/common/drivers/usb/device_old/cdc_acm/inc
IFLAGS += -I$(BASEDIR)/component/common/drivers/usb/device_old/msc/inc
IFLAGS += -I$(BASEDIR)/component/common/drivers/usb/device_old/vendor/inc
IFLAGS += -I$(BASEDIR)/component/common/drivers/usb/device_old/hid/inc
endif

IFLAGS               += -I$(BASEDIR)/component/common/network/libcoap/include
IFLAGS 				 += -I$(BASEDIR)/component/common/api/wifi/rtw_wpa_supplicant/src/utils

.PHONY: all clean

#*****************************************************************************#
#                               VARIABLES	                              #
#*****************************************************************************#
DIR = $(BASEDIR)/component/common/application/game_headset

vpath %.c $(DIR) $(shell find $(DIR) -type d)

#*****************************************************************************#
#                               Source FILE LIST                              #
#*****************************************************************************#
ifeq ($(CONFIG_GHD_EN),y)
CSRC += 	$(DIR)/gh_dongle_wifi.c 
CSRC += 	$(DIR)/gh_dongle_usbd.c 
CSRC += 	$(DIR)/gh_dongle_usbd_sp.c
#CSRC += 	$(DIR)/audio_for_test_delay.c
else ifeq ($(CONFIG_GHH_EN),y)
CSRC += 	$(DIR)/gh_hdst_wifi.c 
CSRC += 	$(DIR)/gh_hdst_spi.c 
CSRC += 	$(DIR)/gh_hdst_uart.c
endif


#*****************************************************************************#
#                               Object FILE LIST                              #
#*****************************************************************************#
OBJS = $(notdir $(CSRC:.c=.o))

#*****************************************************************************#
#                               Include Dependency                            #
#*****************************************************************************#
-include $(OBJS:.o=.d)


#*****************************************************************************#
#                        RULES TO GENERATE TARGETS                            #
#*****************************************************************************#
all: CORE_TARGETS COPY_RAM_OBJS
	$(AR) rvs lib_ghd.a $(OBJS) $(OBJS_SRAM)
	$(FROMELF) --strip-debug lib_ghd.a
	$(MOVE) -f lib_ghd.a $(ROOTDIR)/lib/application
#*****************************************************************************#
#              GENERATE OBJECT FILE 
#*****************************************************************************#
CORE_TARGETS: $(OBJS)

#*****************************************************************************#
#              CLEAN GENERATED FILES                                          #
#*****************************************************************************#
clean: CLEAN_OBJS 
	$(REMOVE) *.o
	$(REMOVE) *.i
	$(REMOVE) *.s
	$(REMOVE) *.d
	$(REMOVE) *.su

-include $(DEPS)
