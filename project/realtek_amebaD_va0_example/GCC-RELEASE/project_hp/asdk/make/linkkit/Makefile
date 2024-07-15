
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

#IFLAGS               += -I$(BASEDIR)/component/soc/realtek/8710c/misc/platform

.PHONY: all clean

#*****************************************************************************#
#                               VARIABLES	                              #
#*****************************************************************************#
DIR = $(BASEDIR)/component/common/application/linkkit/v1.3.0

#vpath %.c $(DIR) $(shell find $(DIR) -type d)

#*****************************************************************************#
#                               Source FILE LIST                              #
#*****************************************************************************#



# Source file list
# -------------------------------------------------------------------
#certs
CSRC += $(DIR)/src/infra/system/ca.c

#infra
CSRC += $(DIR)/src/infra/system/aos_support.c
CSRC += $(DIR)/src/infra/system/device.c
CSRC += $(DIR)/src/infra/system/event.c
CSRC += $(DIR)/src/infra/system/guider.c
CSRC += $(DIR)/src/infra/system/redirect_region.c
CSRC += $(DIR)/src/infra/system/report.c
CSRC += $(DIR)/src/infra/system/facility/fac_string.c
CSRC += $(DIR)/src/infra/log/iotx_log.c
CSRC += $(DIR)/src/infra/utils/misc/json_parser.c
CSRC += $(DIR)/src/infra/utils/misc/json_token.c
CSRC += $(DIR)/src/infra/utils/misc/lite-cjson.c
CSRC += $(DIR)/src/infra/utils/misc/log_report.c
CSRC += $(DIR)/src/infra/utils/misc/mem_stats.c
CSRC += $(DIR)/src/infra/utils/misc/string_utils.c
CSRC += $(DIR)/src/infra/utils/misc/utils_epoch_time.c
CSRC += $(DIR)/src/infra/utils/misc/utils_event.c
CSRC += $(DIR)/src/infra/utils/misc/utils_httpc.c
CSRC += $(DIR)/src/infra/utils/misc/utils_net.c
CSRC += $(DIR)/src/infra/utils/misc/utils_timer.c
CSRC += $(DIR)/src/infra/utils/digest/utils_base64.c
CSRC += $(DIR)/src/infra/utils/digest/utils_hmac.c
CSRC += $(DIR)/src/infra/utils/digest/utils_md5.c
CSRC += $(DIR)/src/infra/utils/digest/utils_sha1.c
CSRC += $(DIR)/src/infra/utils/digest/utils_sha256.c

#alcs
CSRC += $(DIR)/src/protocol/alcs/alcs_adapter.c
CSRC += $(DIR)/src/protocol/alcs/alcs_api.c
CSRC += $(DIR)/src/protocol/alcs/alcs_client.c
CSRC += $(DIR)/src/protocol/alcs/alcs_coap.c
CSRC += $(DIR)/src/protocol/alcs/alcs_localsetup.c
CSRC += $(DIR)/src/protocol/alcs/alcs_mqtt.c
CSRC += $(DIR)/src/protocol/alcs/alcs_server.c

#coap server
CSRC += $(DIR)/src/protocol/coap/local/CoAPExport.c
CSRC += $(DIR)/src/protocol/coap/local/CoAPMessage.c
CSRC += $(DIR)/src/protocol/coap/local/CoAPNetwork.c
CSRC += $(DIR)/src/protocol/coap/local/CoAPObserve.c
CSRC += $(DIR)/src/protocol/coap/local/CoAPPlatform.c
CSRC += $(DIR)/src/protocol/coap/local/CoAPResource.c
CSRC += $(DIR)/src/protocol/coap/local/CoAPServer.c

#coap packet
CSRC += $(DIR)/src/protocol/coap/local/CoAPDeserialize.c
CSRC += $(DIR)/src/protocol/coap/local/CoAPSerialize.c

#http
CSRC += $(DIR)/src/protocol/http/iotx_http_api.c

#mqtt
CSRC += $(DIR)/src/protocol/mqtt/client/mqtt_client.c
CSRC += $(DIR)/src/protocol/mqtt/MQTTPacket/MQTTConnectClient.c
CSRC += $(DIR)/src/protocol/mqtt/MQTTPacket/MQTTDeserializePublish.c
CSRC += $(DIR)/src/protocol/mqtt/MQTTPacket/MQTTSerializePublish.c
CSRC += $(DIR)/src/protocol/mqtt/MQTTPacket/MQTTPacket.c
CSRC += $(DIR)/src/protocol/mqtt/MQTTPacket/MQTTSubscribeClient.c
CSRC += $(DIR)/src/protocol/mqtt/MQTTPacket/MQTTUnsubscribeClient.c

#sdk-impl
CSRC += $(DIR)/src/sdk-impl/impl_dynreg.c
CSRC += $(DIR)/src/sdk-impl/impl_linkkit.c
CSRC += $(DIR)/src/sdk-impl/impl_logpost.c
CSRC += $(DIR)/src/sdk-impl/impl_ntp.c
CSRC += $(DIR)/src/sdk-impl/sdk-impl.c

#ota
CSRC += $(DIR)/src/services/ota/iotx_ota.c
CSRC += $(DIR)/src/services/ota/impl/ota_fetch.c
CSRC += $(DIR)/src/services/ota/impl/ota_lib.c
CSRC += $(DIR)/src/services/ota/prot/ota_mqtt.c
CSRC += $(DIR)/src/services/ota/prot/ota_coap.c

#wifi provision
CSRC += $(DIR)/src/services/awss/awss.c
CSRC += $(DIR)/src/services/awss/awss_main.c
CSRC += $(DIR)/src/services/awss/awss_enrollee.c
CSRC += $(DIR)/src/services/awss/awss_registrar.c
CSRC += $(DIR)/src/services/awss/awss_info.c
CSRC += $(DIR)/src/services/awss/awss_info_notify.c
CSRC += $(DIR)/src/services/awss/awss_aplist.c
CSRC += $(DIR)/src/services/awss/awss_statis.c
CSRC += $(DIR)/src/services/awss/awss_crypt.c
CSRC += $(DIR)/src/services/awss/awss_manufact_ap_find.c
CSRC += $(DIR)/src/services/awss/awss_security.c
CSRC += $(DIR)/src/services/awss/zconfig_protocol.c
CSRC += $(DIR)/src/services/awss/zconfig_vendor_common.c
CSRC += $(DIR)/src/services/awss/zconfig_ieee80211.c
CSRC += $(DIR)/src/services/awss/zconfig_utils.c
CSRC += $(DIR)/src/services/awss/awss_dev_ap.c
CSRC += $(DIR)/src/services/awss/awss_wps.c
CSRC += $(DIR)/src/services/awss/awss_smartconfig.c
CSRC += $(DIR)/src/services/awss/awss_smartconfig_mcast.c
CSRC += $(DIR)/src/services/awss/connect_ap.c
CSRC += $(DIR)/src/services/awss/awss_wifimgr.c

#dev bind
CSRC += $(DIR)/src/services/dev_bind/awss_bind.c
CSRC += $(DIR)/src/services/dev_bind/awss_bind_statis.c
CSRC += $(DIR)/src/services/dev_bind/awss_cmp_coap.c
CSRC += $(DIR)/src/services/dev_bind/awss_cmp_mqtt.c
CSRC += $(DIR)/src/services/dev_bind/awss_event.c
CSRC += $(DIR)/src/services/dev_bind/awss_notify.c
CSRC += $(DIR)/src/services/dev_bind/awss_packet.c
CSRC += $(DIR)/src/services/dev_bind/awss_report.c
CSRC += $(DIR)/src/services/dev_bind/awss_timer.c
CSRC += $(DIR)/src/services/dev_bind/passwd.c
CSRC += $(DIR)/src/services/dev_bind/sha256.c
CSRC += $(DIR)/src/services/dev_bind/os/os_misc.c

#dev_diagnosis
CSRC += $(DIR)/src/services/dev_diagnosis/dev_diagnosis.c
CSRC += $(DIR)/src/services/dev_diagnosis/dev_errcode.c
CSRC += $(DIR)/src/services/dev_diagnosis/dev_offline_ota.c
CSRC += $(DIR)/src/services/dev_diagnosis/dev_state_machine.c
CSRC += $(DIR)/src/services/dev_diagnosis/diagnosis_offline_log.c

#linkkit/cm
CSRC += $(DIR)/src/services/linkkit/cm/iotx_cm.c
CSRC += $(DIR)/src/services/linkkit/cm/iotx_cm_coap.c
CSRC += $(DIR)/src/services/linkkit/cm/iotx_cm_mqtt.c

#linkkit_dm
CSRC += $(DIR)/src/services/linkkit/dm/dm_api.c
CSRC += $(DIR)/src/services/linkkit/dm/dm_cota.c
CSRC += $(DIR)/src/services/linkkit/dm/dm_fota.c
CSRC += $(DIR)/src/services/linkkit/dm/dm_ipc.c
CSRC += $(DIR)/src/services/linkkit/dm/dm_manager.c
CSRC += $(DIR)/src/services/linkkit/dm/dm_message.c
CSRC += $(DIR)/src/services/linkkit/dm/dm_message_cache.c
CSRC += $(DIR)/src/services/linkkit/dm/dm_msg_process.c
CSRC += $(DIR)/src/services/linkkit/dm/dm_opt.c
CSRC += $(DIR)/src/services/linkkit/dm/dm_ota.c
CSRC += $(DIR)/src/services/linkkit/dm/dm_utils.c
CSRC += $(DIR)/src/services/linkkit/dm/client/dm_client.c
CSRC += $(DIR)/src/services/linkkit/dm/client/dm_client_adapter.c
CSRC += $(DIR)/src/services/linkkit/dm/server/dm_server.c
CSRC += $(DIR)/src/services/linkkit/dm/server/dm_server_adapter.c

#dev_reset
CSRC += $(DIR)/src/services/linkkit/dev_reset/awss_reset.c
CSRC += $(DIR)/src/services/linkkit/dev_reset/awss_reset_statis.c

#HAL
#CSRC += $(DIR)/platform/linkkit_wrapper.c

#*****************************************************************************#
#                               Object FILE LIST                              #
#*****************************************************************************#
OBJS = $(CSRC:.c=.o)

#*****************************************************************************#
#                               Include Dependency                            #
#*****************************************************************************#
-include $(OBJS:.o=.d)


#*****************************************************************************#
#                        RULES TO GENERATE TARGETS                            #
#*****************************************************************************#
all: CORE_TARGETS
	$(AR) rvs lib_aliiot.a $(OBJS)
	$(FROMELF) --strip-debug lib_aliiot.a
	$(MOVE) -f lib_aliiot.a $(ROOTDIR)/lib/application
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
