
include $(MAKE_INCLUDE_GEN)

.PHONY: all clean

#*****************************************************************************#
#                               VARIABLES	                              #
#*****************************************************************************#
DRV_DIR = $(BASEDIR)/component/common/drivers/usb
COM_DIR = $(DRV_DIR)/common
HOST_DIR = $(DRV_DIR)/host
IFLAGS += -I$(COM_DIR)
IFLAGS += -I$(HOST_DIR)/core
ifeq ($(CONFIG_USBH_CDC_ACM),y)
DIR = $(HOST_DIR)/cdc_acm
endif
ifeq ($(CONFIG_USBH_CDC_ECM),y)
DIR = $(HOST_DIR)/cdc_ecm
endif

IFLAGS += -I$(DIR)

GLOBAL_CFLAGS += -DCONFIG_USB_NEW_STACK_EN

vpath %.c $(DIR)

#*****************************************************************************#
#                               Source FILE LIST                              #
#*****************************************************************************#
ifeq ($(CONFIG_USBH_CDC_ACM),y)
CSRC = $(DIR)/usbh_cdc_acm.c
endif

ifeq ($(CONFIG_USBH_CDC_ECM),y)
CSRC = $(DIR)/usbh_cdc_ecm.c \
	$(DIR)/usbh_cdc_ecm_hal.c \
	$(DIR)/usbh_cdc_ecm_appx.c
endif

#*****************************************************************************#
#                               Object FILE LIST                              #
#*****************************************************************************#
OBJS = $(notdir $(CSRC:.c=.o))

#*****************************************************************************#
#                        RULES TO GENERATE TARGETS                            #
#*****************************************************************************#

# Define the Rules to build the core targets
COPY_RAM_OBJS: CORE_TARGETS
all: COPY_RAM_OBJS
	
#*****************************************************************************#
#              GENERATE OBJECT FILE 
#*****************************************************************************#
CORE_TARGETS:	$(OBJS)


#*****************************************************************************#
#                        RULES TO CLEAN TARGETS                               #
#*****************************************************************************#
clean: CLEAN_OBJS 
	$(REMOVE) *.o
	$(REMOVE) *.i
	$(REMOVE) *.s*
	$(REMOVE) *.d

-include $(DEPS)	
