
include $(MAKE_INCLUDE_GEN)

.PHONY: all clean

#*****************************************************************************#
#                               VARIABLES	                              #
#*****************************************************************************#
DIR = $(BASEDIR)/component/soc/realtek/amebad/bootloader_2nd
CFGDIR = $(BASEDIR)/component/soc/realtek/amebad/fwlib/usrcfg
DIR_COM = $(BASEDIR)/component/soc/realtek/amebad/fwlib/ram_common
DIR_RAM_LP = $(BASEDIR)/component/soc/realtek/amebad/fwlib/ram_lp
MODULE_IFLAGS = -I$(DIR)/../include/

vpath %.c $(DIR) $(CFGDIR) $(DIR_RAM_LP) $(DIR_COM)

#*****************************************************************************#
#                               Source FILE LIST                              #
#*****************************************************************************#
CSRC = $(DIR)/boot_ram_lp_2nd.c \
	$(DIR)/boot_flash_lp_2nd.c

CSRC += $(CFGDIR)/rtl8721d_bootcfg.c

#*****************************************************************************#
#                               Object FILE LIST                              #
#*****************************************************************************#
OBJS = $(notdir $(CSRC:.c=.o))

#*****************************************************************************#
#                        RULES TO GENERATE TARGETS                            #
#*****************************************************************************#

# Define the Rules to build the core targets
#all: CORE_TARGETS
all: CORE_TARGETS COPY_RAM_OBJS

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
	$(REMOVE) *.d
	$(REMOVE) *.su

-include $(DEPS)
