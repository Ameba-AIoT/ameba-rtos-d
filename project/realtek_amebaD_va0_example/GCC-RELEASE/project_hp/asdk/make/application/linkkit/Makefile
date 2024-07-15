
include $(MAKE_INCLUDE_GEN)

# Include folder list
# -------------------------------------------------------------------
IFLAGS =
IFLAGS += -I$(BASEDIR)/component/common/application/linkkit/v1.3.0/src/infra/log
IFLAGS += -I$(BASEDIR)/component/common/application/linkkit/v1.3.0/src/infra/system
IFLAGS += -I$(BASEDIR)/component/common/application/linkkit/v1.3.0/src/infra/utils
IFLAGS += -I$(BASEDIR)/component/common/application/linkkit/v1.3.0/src/infra/utils/digest
IFLAGS += -I$(BASEDIR)/component/common/application/linkkit/v1.3.0/src/infra/utils/misc
IFLAGS += -I$(BASEDIR)/component/common/application/linkkit/v1.3.0/src/protocol/coap/local
IFLAGS += -I$(BASEDIR)/component/common/application/linkkit/v1.3.0/src/protocol/alcs
IFLAGS += -I$(BASEDIR)/component/common/application/linkkit/v1.3.0/src/protocol/http
IFLAGS += -I$(BASEDIR)/component/common/application/linkkit/v1.3.0/src/protocol/mqtt
IFLAGS += -I$(BASEDIR)/component/common/application/linkkit/v1.3.0/src/protocol/mqtt/client
IFLAGS += -I$(BASEDIR)/component/common/application/linkkit/v1.3.0/src/protocol/mqtt/MQTTPacket
IFLAGS += -I$(BASEDIR)/component/common/application/linkkit/v1.3.0/src/services/dev_bind
IFLAGS += -I$(BASEDIR)/component/common/application/linkkit/v1.3.0/src/services/dev_bind/os
IFLAGS += -I$(BASEDIR)/component/common/application/linkkit/v1.3.0/src/services/dev_bind/product
IFLAGS += -I$(BASEDIR)/component/common/application/linkkit/v1.3.0/src/services/awss
IFLAGS += -I$(BASEDIR)/component/common/application/linkkit/v1.3.0/src/sdk-impl
IFLAGS += -I$(BASEDIR)/component/common/application/linkkit/v1.3.0/src/services/dev_diagnosis
IFLAGS += -I$(BASEDIR)/component/common/application/linkkit/v1.3.0/src/services/linkkit/cm
IFLAGS += -I$(BASEDIR)/component/common/application/linkkit/v1.3.0/src/services/linkkit/dev_reset
IFLAGS += -I$(BASEDIR)/component/common/application/linkkit/v1.3.0/src/services/linkkit/dm
IFLAGS += -I$(BASEDIR)/component/common/application/linkkit/v1.3.0/src/services/linkkit/dm/client
IFLAGS += -I$(BASEDIR)/component/common/application/linkkit/v1.3.0/src/services/linkkit/dm/server
IFLAGS += -I$(BASEDIR)/component/common/application/linkkit/v1.3.0/src/services/ota

IFLAGS += -I$(BASEDIR)/component/common/application/linkkit/v1.3.0/include
IFLAGS += -I$(BASEDIR)/component/common/application/linkkit/v1.3.0/include/exports
IFLAGS += -I$(BASEDIR)/component/common/application/linkkit/v1.3.0/include/imports
IFLAGS += -I$(BASEDIR)/component/common/application/linkkit/v1.3.0/include/platform


IFLAGS               += -I$(OSDIR)/
IFLAGS               += -I$(OSDIR)/freertos
IFLAGS               += -I$(FREERTOSDIR)/include/
IFLAGS               += -I$(FREERTOSDIR)/portable/GCC/RTL8721D_HP/non_secure
IFLAGS               += -I$(FREERTOSDIR)/portable/GCC/RTL8721D_HP/secure

IFLAGS               += -I$(TARGETDIR)/cmsis/
IFLAGS               += -I$(TARGETDIR)/cmsis/device
IFLAGS               += -I$(TARGETDIR)/cmsis-dsp/Source
IFLAGS               += -I$(TARGETDIR)/fwlib/include
IFLAGS               += -I$(TARGETDIR)/fwlib/rom_map_hp
IFLAGS               += -I$(TARGETDIR)/app/monitor/include
IFLAGS               += -I$(TARGETDIR)/app/xmodem
IFLAGS               += -I$(TARGETDIR)/swlib/include
IFLAGS               += -I$(TARGETDIR)/swlib/string
IFLAGS               += -I$(TARGETDIR)/misc
IFLAGS               += -I$(TARGETDIR)/swlib/os_dep/include/
IFLAGS               += -I$(TARGETDIR)/swlib/os_dep/../
IFLAGS               += -I$(TARGETDIR)/swlib/string

IFLAGS               += -I$(INCDIR)
IFLAGS               += -I$(BASEDIR)/component/common/api
IFLAGS               += -I$(BASEDIR)/component/common/api/platform
IFLAGS               += -I$(BASEDIR)/component/common/api/wifi
IFLAGS               += -I$(BASEDIR)/component/common/api/network
IFLAGS               += -I$(BASEDIR)/component/common/api/network/include
IFLAGS               += -I$(BASEDIR)/component/common/test
IFLAGS               += -I$(BASEDIR)/component/common/example
IFLAGS               += -I$(BASEDIR)/component/common/utilities
IFLAGS				 += -I$(BASEDIR)/component/common/drivers/si

#INIC
IFLAGS               += -I$(BASEDIR)/component/common/drivers/inic/rtl8721d

#RTSP
IFLAGS               += -I$(BASEDIR)/component/common/network/rtsp

#SSL
IFLAGS               += -I$(BASEDIR)/component/common/network/ssl/polarssl-1.3.8/include
IFLAGS               += -I$(BASEDIR)/component/common/network/ssl/ssl_ram_map/rom

#MBEDTLS
IFLAGS               += -I$(BASEDIR)/component/common/network/ssl/mbedtls-2.4.0/include
IFLAGS               += -I$(BASEDIR)/component/common/network/ssl/mbedtls_ram_map/rom
IFLAGS               += -I$(BASEDIR)/component/common/application/amazon/amazon-freertos/lib/include/private

#temp for git server code
IFLAGS               += -I$(BASEDIR)/component/common/mbed/targets/hal/rtl8721d/
IFLAGS               += -I$(BASEDIR)/component/common/mbed/targets/hal/rtl8721d/include
IFLAGS               += -I$(BASEDIR)/component/common/mbed/api/
IFLAGS               += -I$(BASEDIR)/component/common/mbed/hal/
IFLAGS               += -I$(BASEDIR)/component/common/mbed/hal_ext/

#WIFI
IFLAGS               += -I$(BASEDIR)/component/common/drivers/wlan/realtek/include
IFLAGS               += -I$(BASEDIR)/component/common/drivers/wlan/realtek/src/hci
IFLAGS               += -I$(BASEDIR)/component/common/drivers/wlan/realtek/src/osdep
IFLAGS               += -I$(BASEDIR)/component/common/drivers/wlan/realtek/src/hal
IFLAGS               += -I$(BASEDIR)/component/common/drivers/wlan/realtek/src/hal/phydm
IFLAGS               += -I$(BASEDIR)/component/common/drivers/wlan/realtek/src/hal/rtl8721d
IFLAGS               += -I$(BASEDIR)/component/common/drivers/wlan/realtek/wlan_ram_map/rom
IFLAGS               += -I$(BASEDIR)/component/common/drivers/wlan/realtek/src/core/option
IFLAGS               += -I$(BASEDIR)/component/common/network
IFLAGS               += -I$(BASEDIR)/component/common/network/lwip/lwip_v2.0.2/src/include
IFLAGS               += -I$(BASEDIR)/component/common/network/lwip/lwip_v2.0.2/src/include/lwip
IFLAGS               += -I$(BASEDIR)/component/common/network/lwip/lwip_v2.0.2/src/include/netif
#IFLAGS               += -I$(BASEDIR)/component/common/network/lwip/lwip_v2.0.2/src/include/ipv4
IFLAGS               += -I$(BASEDIR)/component/common/network/lwip/lwip_v2.0.2/port/realtek
IFLAGS               += -I$(BASEDIR)/component/common/network/lwip/lwip_v2.0.2/port/realtek/freertos
IFLAGS               += -I$(BASEDIR)/component/common/api
IFLAGS               += -I$(BASEDIR)/component/common/api/at_cmd
IFLAGS               += -I$(BASEDIR)/component/common/api/wifi/rtw_wpa_supplicant/src
IFLAGS               += -I$(BASEDIR)/component/common/api/wifi/rtw_wpa_supplicant/src/crypto
IFLAGS               += -I$(BASEDIR)/component/os/os_dep/include/
#IFLAGS               += -I$(BASEDIR)/component/common/network/http2/nghttp2-1.31.0/includes
IFLAGS               += -I$(BASEDIR)/component/common/file_system/dct
#IFLAGS               += -I$(BASEDIR)/component/soc/realtek/8710c/misc/platform

.PHONY: all clean

#*****************************************************************************#
#                               VARIABLES	                              #
#*****************************************************************************#
DIR = $(BASEDIR)/component/common/application/linkkit/v1.3.0

vpath %.c $(DIR) $(shell find $(DIR) -type d)

#*****************************************************************************#
#                               Source FILE LIST                              #
#*****************************************************************************#



# Source file list
# -------------------------------------------------------------------
#HAL
CSRC += $(DIR)/platform/linkkit_wrapper.c

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

-include $(DEPS)
