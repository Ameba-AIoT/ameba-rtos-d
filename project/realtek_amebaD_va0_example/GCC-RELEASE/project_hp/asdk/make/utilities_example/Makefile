
include $(MAKE_INCLUDE_GEN)
#IFLAGS               += -I$(BASEDIR)/component/common/network/coap/include
IFLAGS               += -I$(BASEDIR)/component/common/application/mqtt/MQTTClient
IFLAGS               += -I$(BASEDIR)/component/common/bluetooth/realtek/sdk
IFLAGS               += -I$(BASEDIR)/component/common/drivers/sdio/realtek/sdio_host/inc
IFLAGS               += -I$(BASEDIR)/component/common/network/libcoap/include
IFLAGS				+= -I$(BASEDIR)/component/common/file_system/kv

ifeq ($(CONFIG_LINKKIT_AWSS), y)
#aliyun
IFLAGS				 += -I$(BASEDIR)/component/common/application/linkkit/v1.3.0/include
IFLAGS				 += -I$(BASEDIR)/component/common/application/linkkit/v1.3.0/include/imports
IFLAGS				 += -I$(BASEDIR)/component/common/application/linkkit/v1.3.0/include/exports
IFLAGS				 += -I$(BASEDIR)/component/common/application/linkkit/v1.3.0/include/platform
endif

ifeq ($(CONFIG_USB_DEVICE_OLD_EN),y)
IFLAGS += -I$(TARGETDIR)/fwlib/usb_otg/device/inc
IFLAGS += -I$(BASEDIR)/component/common/drivers/usb/device_old/audio/inc
IFLAGS += -I$(BASEDIR)/component/common/drivers/usb/device_old/cdc_acm/inc
IFLAGS += -I$(BASEDIR)/component/common/drivers/usb/device_old/hid/inc
IFLAGS += -I$(BASEDIR)/component/common/drivers/usb/device_old/msc/inc
IFLAGS += -I$(BASEDIR)/component/common/drivers/usb/device_old/vendor/inc
endif
ifeq ($(CONFIG_USB_HOST_OLD_EN),y)
IFLAGS += -I$(TARGETDIR)/fwlib/usb_otg/host/inc
IFLAGS += -I$(BASEDIR)/component/common/drivers/usb/host_old/cdc_acm/inc
IFLAGS += -I$(BASEDIR)/component/common/drivers/usb/host_old/msc/inc
IFLAGS += -I$(BASEDIR)/component/common/drivers/usb/host_old/uvc/inc
IFLAGS += -I$(BASEDIR)/component/common/drivers/usb/host_old/vendor/inc
ifeq ($(CONFIG_USBH_UVC),y)
IFLAGS += -I$(BASEDIR)/component/common/video/v4l2/inc
IFLAGS += -I$(BASEDIR)/component/common/media/rtp_codec
IFLAGS += -I$(BASEDIR)/component/common/media/mmfv2
endif
endif

ifeq ($(CONFIG_USB_DEVICE_EN),y)
GLOBAL_CFLAGS += -DCONFIG_USB_NEW_STACK_EN
IFLAGS += -I$(BASEDIR)/component/common/drivers/usb/common
IFLAGS += -I$(BASEDIR)/component/common/drivers/usb/device/core
IFLAGS += -I$(BASEDIR)/component/common/drivers/usb/device/cdc_acm
IFLAGS += -I$(BASEDIR)/component/common/drivers/usb/device/composite
IFLAGS += -I$(BASEDIR)/component/common/drivers/usb/device/hid
IFLAGS += -I$(BASEDIR)/component/common/drivers/usb/device/msc
endif
ifeq ($(CONFIG_USB_HOST_EN),y)
GLOBAL_CFLAGS += -DCONFIG_USB_NEW_STACK_EN
IFLAGS += -I$(BASEDIR)/component/common/drivers/usb/common
IFLAGS += -I$(BASEDIR)/component/common/drivers/usb/host/core
IFLAGS += -I$(BASEDIR)/component/common/drivers/usb/host/cdc_acm
IFLAGS += -I$(BASEDIR)/component/common/drivers/usb/host/cdc_ecm
endif

.PHONY: all clean

#*****************************************************************************#
#                               VARIABLES	                              #
#*****************************************************************************#
DIR = $(BASEDIR)/component/common/example
DIRS = $(shell find $(DIR) -maxdepth 3 -type d)

vpath %.c $(DIRS)

#*****************************************************************************#
#                               Source FILE LIST                              #
#*****************************************************************************#
CSRC += $(DIR)/example_entry.c
CSRC += $(DIR)/cm_backtrace/example_cm_backtrace.c
CSRC += $(DIR)/bcast/example_bcast.c
CSRC += $(DIR)/eap/example_eap.c
CSRC += $(DIR)/get_beacon_frame/example_get_beacon_frame.c
CSRC += $(DIR)/high_load_memory_use/example_high_load_memory_use.c
CSRC += $(DIR)/game_headset_dongle/example_game_headset_dongle.c
CSRC += $(DIR)/http_client/example_http_client.c
CSRC += $(DIR)/http_download/example_http_download.c
CSRC += $(DIR)/mcast/example_mcast.c
CSRC += $(DIR)/mdns/example_mdns.c
CSRC += $(DIR)/nonblock_connect/example_nonblock_connect.c
CSRC += $(DIR)/rarp/example_rarp.c
CSRC += $(DIR)/sntp_showtime/example_sntp_showtime.c
CSRC += $(DIR)/socket_select/example_socket_select.c
CSRC += $(DIR)/ssl_download/example_ssl_download.c
CSRC += $(DIR)/ssl_server/example_ssl_server.c
CSRC += $(DIR)/tcp_keepalive/example_tcp_keepalive.c
CSRC += $(DIR)/uart_atcmd/example_uart_atcmd.c
CSRC += $(DIR)/wifi_mac_monitor/example_wifi_mac_monitor.c
CSRC += $(DIR)/wlan_fast_connect/example_wlan_fast_connect.c
CSRC += $(DIR)/wlan_repeater/example_wlan_repeater.c
CSRC += $(DIR)/wlan_scenario/example_wlan_scenario.c
CSRC += $(DIR)/websocket_client/example_wsclient.c
CSRC += $(DIR)/websocket_server/example_ws_server.c
CSRC += $(DIR)/xml/example_xml.c
CSRC += $(DIR)/mqtt/example_mqtt.c
CSRC += $(DIR)/coap/example_coap.c
CSRC += $(DIR)/coap_client/example_coap_client.c
CSRC += $(DIR)/coap_server/example_coap_server.c
CSRC += $(DIR)/httpd/example_httpd.c
CSRC += $(DIR)/httpd/example_httpc.c
CSRC += $(DIR)/httpd/example_httpd_littlefs.c
CSRC += $(DIR)/ota_http/example_ota_http.c
CSRC += $(DIR)/ota_https/example_ota_https.c
CSRC += $(DIR)/ota_sdcard/example_ota_sdcard.c
CSRC += $(DIR)/dct/example_dct.c
CSRC += $(DIR)/wifi_manager/example_wifi_manager.c
CSRC += $(DIR)/wifi_roaming/example_wifi_roaming.c
#CSRC += $(DIR)/wifi_roaming_plus/example_wifi_roaming_plus.c
CSRC += $(DIR)/wifi_connection_priority/example_wifi_conn_pri_cond.c
CSRC += $(DIR)/uart_firmware_update/example_uart_update.c
CSRC += $(DIR)/ipv6/example_ipv6.c
CSRC += $(DIR)/cJSON/example_cJSON.c
CSRC += $(DIR)/sdcard_upload_httpd/example_sdcard_upload_httpd.c
CSRC += $(DIR)/socket_tcp_trx/example_socket_tcp_trx_1.c
CSRC += $(DIR)/socket_tcp_trx/example_socket_tcp_trx_2.c
#CSRC += $(DIR)/rtsp_client/example_rtsp_client.c
# MMF AUDIO EXAMPLE
# CSRC += $(DIR)/media_framework/example_media_framework.c
# CSRC += $(DIR)/media_framework/mmf2_example_audioloop_init.c
# CSRC += $(DIR)/media_framework/mmf2_example_pcmu_array_rtsp_init.c
# CSRC += $(DIR)/media_framework/mmf2_example_aac_array_rtsp_init.c
# CSRC += $(DIR)/media_framework/mmf2_example_a_init.c
# CSRC += $(DIR)/media_framework/mmf2_example_rtp_aad_init.c
# CSRC += $(DIR)/media_framework/mmf2_example_2way_audio_init.c
# CSRC += $(DIR)/media_framework/mmf2_example_aacloop_init.c
# CSRC += $(DIR)/media_framework/mmf2_example_g711loop_init.c

#CSRC += $(DIR)/audio_sport/audio_amr/example_audio_amr.c
#CSRC += $(DIR)/audio_sport/audio_mp3/example_audio_mp3.c
#CSRC += $(DIR)/audio_sport/audio_ac3/example_audio_ac3.c
#CSRC += $(DIR)/audio_sport/audio_helix_aac/example_audio_helix_aac.c
#CSRC += $(DIR)/audio_sport/audio_helix_mp3/example_audio_helix_mp3.c
#CSRC += $(DIR)/audio_sport/audio_m4a_selfparse/example_audio_m4a_selfparse.c
#CSRC += $(DIR)/audio_sport/audio_m4a/example_audio_m4a.c
#CSRC += $(DIR)/audio_sport/audio_pcm_upload/example_audio_pcm_upload.c
#CSRC += $(DIR)/audio_sport/audio_hls/example_audio_hls.c
#CSRC += $(DIR)/audio_sport/audio_recorder/example_audio_recorder.c
#CSRC +=	$(DIR)/audio_sport/audio_flac/example_audio_flac.c
#CSRC += $(DIR)/audio_sport/audio_amr_flash/example_audio_amr_flash.c
#CSRC += $(DIR)/audio_sport/audio_amr_flash/audio_play.c
#CSRC += $(DIR)/audio_sport/audio_signal_generate/example_audio_signal_generate.c
#CSRC += $(DIR)/audio_sport/audio_tts/example_audio_tts.c
CSRC += $(DIR)/fatfs/example_fatfs.c
CSRC += $(DIR)/dct/example_dct.c
CSRC += $(DIR)/littlefs/example_littlefs.c
CSRC += $(DIR)/kv/example_kv.c
ifeq ($(CONFIG_SPI_NAND_EN),y)
CSRC += $(DIR)/littlefs_spi_nand/example_spi_nand_littlefs.c
endif
#CSRC += $(DIR)/competitive_headphones/example_competitive_headphones.c
#CSRC += $(DIR)/competitive_headphones/headphone_rl6548_audio.c
#CSRC += $(DIR)/competitive_headphones/headphone_burst.c
#CSRC += $(DIR)/competitive_headphones/headphone_packet_compensation.c
#CSRC += $(DIR)/competitive_headphones_dongle/example_competitive_headphones_dongle.c
CSRC += $(DIR)/captive_portal/example_captive_portal.c

ifeq ($(CONFIG_MEDIA_PLAYER),y)
IFLAGS += -I$(BASEDIR)/component/example/player
IFLAGS += -I$(BASEDIR)/component/audio/frameworks/media/playback/interfaces
IFLAGS += -I$(BASEDIR)/component/audio/base/libs/src/include
IFLAGS += -I$(BASEDIR)/component/audio/base/log/interfaces
IFLAGS += -I$(BASEDIR)/component/audio/base/osal/osal_c/interfaces
IFLAGS += -I$(BASEDIR)/component/audio/base/osal/osal_cxx/interfaces
IFLAGS += -I$(BASEDIR)/component/audio/base/cutils/interfaces
IFLAGS += -I$(BASEDIR)/component/audio/base/utils/interfaces
ifeq ($(CONFIG_AUDIO_MIXER),y)
IFLAGS += -I$(BASEDIR)/component/audio/frameworks/media/audio/interfaces
endif
CSRC += $(DIR)/player/player_app_example.c
CSRC += $(DIR)/player/example_player.c
endif

ifeq ($(CONFIG_AUDIO_FWK),y)
IFLAGS += -I$(BASEDIR)/component/audio/frameworks/media/audio/interfaces
IFLAGS += -I$(BASEDIR)/component/audio/base/osal/osal_c/interfaces
CSRC += $(DIR)/audio_track/audio_track_app_example.c
CSRC += $(DIR)/audio_track/example_audio_track.c
endif

ifeq ($(CONFIG_USB_OTG_EN),y)
ifeq ($(CONFIG_USB_DEVICE_OLD_EN),y)
CSRC += $(DIR)/usbd_audio_old/example_usbd_audio_sp_old.c
CSRC += $(DIR)/usbd_audio_old/example_usbd_audio_old.c
CSRC += $(DIR)/usbd_cdc_acm_old/example_usbd_cdc_acm_old.c
CSRC += $(DIR)/usbd_hid_old/example_usbd_hid_old.c
CSRC += $(DIR)/usbd_msc_old/example_usbd_msc_old.c
CSRC += $(DIR)/usbd_vendor_old/example_usbd_vendor_old.c
endif
ifeq ($(CONFIG_USB_HOST_OLD_EN),y)
CSRC += $(DIR)/usbh_cdc_acm_old/example_usbh_cdc_acm_old.c
CSRC += $(DIR)/usbh_msc_old/example_usbh_msc_old.c
CSRC += $(DIR)/usbh_uvc_old/example_usbh_uvc_old.c
CSRC += $(DIR)/usbh_vendor_old/example_usbh_vendor_old.c
endif
ifeq ($(CONFIG_USB_DEVICE_EN),y)
CSRC += $(DIR)/usbd_cdc_acm/example_usbd_cdc_acm.c
CSRC += $(DIR)/usbd_composite/example_usbd_composite.c
CSRC += $(DIR)/usbd_hid/example_usbd_hid.c
CSRC += $(DIR)/usbd_msc/example_usbd_msc.c
endif
ifeq ($(CONFIG_USB_HOST_EN),y)
CSRC += $(DIR)/usbh_cdc_acm/example_usbh_cdc_acm.c
CSRC += $(DIR)/usbh_cdc_ecm/example_usbh_cdc_ecm.c
endif
endif

ifeq ($(CONFIG_LINKKIT_AWSS), y)
#linkkit
CSRC += $(DIR)/linkkit/linkkit_awss_example.c
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
	$(REMOVE) *.c

-include $(DEPS)
