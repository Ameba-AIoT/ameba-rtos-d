
include $(MAKE_INCLUDE_GEN)

.PHONY: all clean

#*****************************************************************************#
#                               VARIABLES	                              #
#*****************************************************************************#
DIR = $(BASEDIR)/component/common/video/v4l2/src
MODULE_IFLAGS += -I$(DIR)/../inc
MODULE_IFLAGS += -I$(TARGETDIR)/fwlib/usb_otg/host/inc
MODULE_IFLAGS += -I$(BASEDIR)/component/common/drivers/usb/host_old/uvc/inc
MODULE_IFLAGS += -DREFER_USBH_UVC
TARGET_LIB = lib_v4l2

vpath %.c $(DIR)

#*****************************************************************************#
#                               Source FILE LIST                              #
#*****************************************************************************#
CSRC +=	$(V4L2)/v4l2-dev.c \
	$(V4L2)/v4l2-device.c \
	$(V4L2)/v4l2-fh.c \
	$(V4L2)/v4l2_driver.c \
	$(V4L2)/v4l2_intf.c \
	$(V4L2)/videobuf2-core.c
	
#*****************************************************************************#
#                               Object FILE LIST                              #
#*****************************************************************************#
OBJS = $(notdir $(CSRC:.c=.o))
	
#*****************************************************************************#
#                        RULES TO GENERATE TARGETS                            #
#*****************************************************************************#

# Define the Rules to build the core targets
all: CORE_TARGETS COPY_RAM_OBJS
	$(AR) rvs $(TARGET_LIB).a $(OBJS)
	$(FROMELF) --strip-debug $(TARGET_LIB).a
	$(MOVE) -f $(TARGET_LIB).a $(ROOTDIR)/lib/application
	
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
	$(REMOVE) *.s
	$(REMOVE) *.d

-include $(DEPS)
