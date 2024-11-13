
include $(MAKE_INCLUDE_GEN)

.PHONY: all clean


#*****************************************************************************#
#                            OBJECT FILE LIST                                 #
#*****************************************************************************#
OBJS =

#*****************************************************************************#
#                        RULES TO GENERATE TARGETS                            #
#*****************************************************************************#

# Define the Rules to build the core targets
all:

ifneq ($(CONFIG_TOOLCHAIN_ARM_GCC), y)
ifeq ($(CONFIG_MATTER_EN), y)
	make -C $(MATTER_BUILDDIR)/toolchain km4_toolchain
else
	@if [ ! -f asdk/$(TOOLCHAINNAME) ]; then \
		echo "Download Toolchain And Extract To $(TOOLCHAINDIR_OS). Only Download And Extract Once"; \
		if wget -P asdk $(TOOLCHAINURL); then \
			echo "Download $(TOOLCHAINNAME) Success"; \
		else \
			echo "Download Failed. Please Check If The Network Connection Is Accessible."; \
			exit 1; \
		fi; \
		echo "Download End And Then Extract..."; \
		$(MKDIR) -p $(TOOLCHAINDIR_OS); \
		tar -jxvf asdk/$(TOOLCHAINNAME) -C $(TOOLCHAINDIR_OS)/; \
		echo "INSTALL SUCCESS"; \
	else \
		$(MKDIR) -p $(TOOLCHAINDIR_OS); \
		tar -jxvf asdk/$(TOOLCHAINNAME) -C $(TOOLCHAINDIR_OS)/; \
	fi
endif
else
	$(MKDIR) -p cygwin
	tar -zxvf ../../../tools/arm-none-eabi-gcc/4.8.3-2014q1.tar.gz -C cygwin/
endif

#*****************************************************************************#
#                          GENERATE OBJECT FILES                              #
#*****************************************************************************#
CORE_TARGETS: $(OBJS)

