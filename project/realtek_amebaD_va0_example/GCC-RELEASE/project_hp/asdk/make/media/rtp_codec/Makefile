
include $(MAKE_INCLUDE_GEN)

.PHONY: all clean

#*****************************************************************************#
#                               VARIABLES	                              #
#*****************************************************************************#
DIR = $(BASEDIR)/component/common/media/rtp_codec
MODULE_IFLAGS += -I$(DIR)
TARGET_LIB = lib_rtp_codec

vpath %.c $(DIR) $(shell find $(DIR) -type d)

#*****************************************************************************#
#                               Source FILE LIST                              #
#*****************************************************************************#
CSRC += $(DIR)/aac/aac.c
CSRC += $(DIR)/avcodec.c
CSRC += $(DIR)/g711/g711.c
#CSRC += $(DIR)/h264/h264.c
CSRC += $(DIR)/mjpeg/mjpeg.c

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
	$(AR) rvs $(TARGET_LIB).a $(OBJS)
	$(FROMELF) --strip-debug $(TARGET_LIB).a
	$(MOVE) -f $(TARGET_LIB).a $(ROOTDIR)/lib/application
	
#*****************************************************************************#
#              GENERATE OBJECT FILE 
#*****************************************************************************#
CORE_TARGETS: $(OBJS)

#*****************************************************************************#
#                        RULES TO CLEAN TARGETS                               #
#*****************************************************************************#
clean: CLEAN_OBJS
	$(REMOVE) *.o
	$(REMOVE) *.i
	$(REMOVE) *.s
	$(REMOVE) *.su
	$(REMOVE) *.d

-include $(DEPS)	
