include $(MAKE_INCLUDE_GEN)

.PHONY: all clean

#*****************************************************************************#
#                               VARIABLES	                              #
#*****************************************************************************#

#*****************************************************************************#
#                        RULES TO GENERATE TARGETS                            #
#*****************************************************************************#
all:
ifeq ($(CONFIG_TRUSTZONE), y)
	@make -C img3 all
	@make -C test all
	@make -C crypto all
	@make -C os all
ifeq ($(CONFIG_MATTER_SECURE_EN),y)
	@make -C $(MATTER_BUILDDIR)/make/mbedtls_secure all
else
	#@make -C mbedtls all
endif
endif

#*****************************************************************************#
#              CLEAN GENERATED FILES                                          #
#*****************************************************************************#
clean:
	@make -C img3 clean
	@make -C test clean
	@make -C crypto clean
	@make -C os clean
ifeq ($(CONFIG_MATTER_SECURE_EN),y)
	@make -C $(MATTER_BUILDDIR)/make/mbedtls_secure clean
endif
	@make -C mbedtls clean

-include $(DEPS)
