
include $(MAKE_INCLUDE_GEN)

.PHONY: all clean

#*****************************************************************************#
#                               VARIABLES	                              #
#*****************************************************************************#
DIR = $(BASEDIR)/component/common/file_system/littlefs
#MODULE_IFLAGS += -I$(BASEDIR)/component/common/drivers/usb/host_old/msc/inc
#MODULE_IFLAGS += -I$(BASEDIR)/component/common/drivers/sdio/realtek/sdio_host/inc
#*****************************************************************************#
#                               Source FILE LIST                              #
#*****************************************************************************#
CSRC += $(DIR)/littlefs_adapter.c	\
	$(DIR)/r2.50/lfs.c	\
	$(DIR)/r2.50/lfs_util.c

ifeq ($(CONFIG_SPI_NAND_EN),y)
CSRC += $(DIR)/spi_nand_adapter/littlefs_nand_adapter.c	\
	$(DIR)/spi_nand_adapter/littlefs_nand_ftl.c	\
	$(DIR)/spi_nand_adapter/littlefs_nand_ftl_mfg.c	\
	$(DIR)/spi_nand_adapter/littlefs_spi_nand.c
endif
#*****************************************************************************#
#                               Object FILE LIST                              #
#*****************************************************************************#
OBJS = $(CSRC:.c=.o)

#*****************************************************************************#
#                        RULES TO GENERATE TARGETS                            #
#*****************************************************************************#

# Define the Rules to build the core targets
all: CORE_TARGETS COPY_RAM_OBJS

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
