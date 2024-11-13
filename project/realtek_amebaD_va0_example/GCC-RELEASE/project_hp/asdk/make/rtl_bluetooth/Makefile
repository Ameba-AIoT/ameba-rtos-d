include $(MAKE_INCLUDE_GEN)

.PHONY: all clean

#*****************************************************************************#
#                              OBJECT FILE LIST                               #
#*****************************************************************************#
OBJS =

#*****************************************************************************#
#                             INCLUDE DEPENDENCY                              #
#*****************************************************************************#
-include $(OBJS:.o=.d)

#*****************************************************************************#
#                          RULES TO GENERATE TARGETS                          #
#*****************************************************************************#
all: CORE_TARGETS
ifeq ($(CONFIG_BT_EN),y)
	make -C amebad_bsp all

ifeq ($(CONFIG_BT_PERIPHERAL),y)
	make -C ble_peripheral all
endif

ifeq ($(CONFIG_BT_CENTRAL),y)
	make -C ble_central all
endif

ifeq ($(CONFIG_BT_SCATTERNET),y)
	make -C ble_scatternet all
endif

ifeq ($(CONFIG_BT_BEACON),y)
	make -C bt_beacon all
endif

ifeq ($(CONFIG_BT_CONFIG),y)
	make -C bt_config all
endif

ifeq ($(CONFIG_BT_AIRSYNC_CONFIG),y)
	make -C bt_airsync_config all
endif

ifeq ($(CONFIG_BT_ANCS),y)
	make -C bt_ancs all
endif

ifeq ($(CONFIG_BT_BREEZE),y)
	make -C bt_breeze all
endif

ifeq ($(CONFIG_BT_DISTANCE_DETECTOR),y)
	make -C bt_distance_detector all
endif

ifeq ($(CONFIG_BT_TAG_SCANNER),y)
	make -C bt_tag_scanner all
endif

ifeq ($(CONFIG_BT_OTA_CLIENT),y)
	make -C bt_ota_client all
endif

ifeq ($(CONFIG_BT_OTA_SERVER),y)
	make -C bt_ota_server all
endif

ifeq ($(CONFIG_BT_MESH_PROVISIONER),y)
	make -C mesh_provisioner all
ifeq ($(CONFIG_BT_MESH_PROVISIONER_RTK_DEMO),y)
	make -C bt_mesh_provisioner_rtk_demo all
endif
endif

ifeq ($(CONFIG_BT_MESH_DEVICE),y)
	make -C mesh_device all
ifeq ($(CONFIG_BT_MESH_DEVICE_RTK_DEMO),y)
	make -C bt_mesh_device_rtk_demo all
endif
endif

ifeq ($(CONFIG_BT_MESH_PROVISIONER_MULTIPLE_PROFILE),y)
	make -C mesh_provisioner_multiple_profile all
ifeq ($(CONFIG_BT_MESH_PROVISIONER_RTK_DEMO),y)
	make -C bt_mesh_provisioner_rtk_demo all
endif
endif

ifeq ($(CONFIG_BT_MESH_DEVICE_MULTIPLE_PROFILE),y)
	make -C mesh_device_multiple_profile all
ifeq ($(CONFIG_BT_MESH_DEVICE_RTK_DEMO),y)
	make -C bt_mesh_device_rtk_demo all
endif
endif

ifeq ($(CONFIG_BLE_MATTER_ADAPTER),y)
	make -C ble_matter_adapter all
endif

ifeq ($(CONFIG_BLE_TRANSFER_MODULE),y)
	make -C ble_transfer_module all
endif

endif

#*****************************************************************************#
#                            GENERATE OBJECT FILE                             #
#*****************************************************************************#
CORE_TARGETS: $(OBJS)

#*****************************************************************************#
#                            CLEAN GENERATED FILES                            #
#*****************************************************************************#
clean:
ifeq ($(CONFIG_BT_EN),y)
	make -C amebad_bsp clean

ifeq ($(CONFIG_BT_PERIPHERAL),y)
	make -C ble_peripheral clean
endif

ifeq ($(CONFIG_BT_CENTRAL),y)
	make -C ble_central clean
endif

ifeq ($(CONFIG_BT_SCATTERNET),y)
	make -C ble_scatternet clean
endif

ifeq ($(CONFIG_BT_BEACON),y)
	make -C bt_beacon clean
endif

ifeq ($(CONFIG_BT_CONFIG),y)
	make -C bt_config clean
endif

ifeq ($(CONFIG_BT_AIRSYNC_CONFIG),y)
	make -C bt_airsync_config clean
endif

ifeq ($(CONFIG_BT_ANCS),y)
	make -C bt_ancs clean
endif

ifeq ($(CONFIG_BT_BREEZE),y)
	make -C bt_breeze clean
endif

ifeq ($(CONFIG_BT_DISTANCE_DETECTOR),y)
	make -C bt_distance_detector clean
endif

ifeq ($(CONFIG_BT_TAG_SCANNER),y)
	make -C bt_tag_scanner clean
endif

ifeq ($(CONFIG_BT_OTA_CLIENT),y)
	make -C bt_ota_client clean
endif

ifeq ($(CONFIG_BT_OTA_SERVER),y)
	make -C bt_ota_server clean
endif

ifeq ($(CONFIG_BT_MESH_PROVISIONER),y)
	make -C mesh_provisioner clean
ifeq ($(CONFIG_BT_MESH_PROVISIONER_RTK_DEMO),y)
	make -C bt_mesh_provisioner_rtk_demo clean
endif
endif

ifeq ($(CONFIG_BT_MESH_DEVICE),y)
	make -C mesh_device clean
ifeq ($(CONFIG_BT_MESH_DEVICE_RTK_DEMO),y)
	make -C bt_mesh_device_rtk_demo clean
endif
endif

ifeq ($(CONFIG_BT_MESH_PROVISIONER_MULTIPLE_PROFILE),y)
	make -C mesh_provisioner_multiple_profile clean
ifeq ($(CONFIG_BT_MESH_PROVISIONER_RTK_DEMO),y)
	make -C bt_mesh_provisioner_rtk_demo clean
endif
endif

ifeq ($(CONFIG_BT_MESH_DEVICE_MULTIPLE_PROFILE),y)
	make -C mesh_device_multiple_profile clean
ifeq ($(CONFIG_BT_MESH_DEVICE_RTK_DEMO),y)
	make -C bt_mesh_device_rtk_demo clean
endif
endif

ifeq ($(CONFIG_BLE_MATTER_ADAPTER),y)
	make -C ble_matter_adapter clean
endif

ifeq ($(CONFIG_BLE_TRANSFER_MODULE),y)
	make -C ble_transfer_module clean
endif

endif

-include $(DEPS)
