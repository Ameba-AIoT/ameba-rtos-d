/**
*****************************************************************************************
*     Copyright(c) 2025, Realtek Semiconductor Corporation. All rights reserved.
*****************************************************************************************
  */

/*============================================================================*
 *                              Header Files
 *============================================================================*/
#include "platform_opts_bt.h"
#if defined(CONFIG_BLE_WIFIMATE_DEVICE) && CONFIG_BLE_WIFIMATE_DEVICE
#include <stdio.h>
#include <string.h>
#include <os_sched.h>
#include <trace_app.h>
#include <app_common_flags.h>
#include <gap.h>
#include <gap_adv.h>
#include <gap_bond_le.h>
#include <gap_msg.h>
#include <gap_config.h>
#include <bte.h>
#include <bt_flags.h>
#include <profile_server.h>
#include "wifi_constants.h"
#include <wifi/wifi_conf.h>
#include "rtk_coex.h"
#include <ble_wifimate_service.h>
#include <ble_wifimate_device_app.h>
#include <ble_wifimate_device_app_task.h>


static const uint8_t scan_rsp_data[] =
{
    0x03,                             /* length */
    GAP_ADTYPE_APPEARANCE,            /* type="Appearance" */
    LO_WORD(GAP_GATT_APPEARANCE_UNKNOWN),
    HI_WORD(GAP_GATT_APPEARANCE_UNKNOWN),
};

/** @brief  Default minimum advertising interval when device is discoverable (units of 625us, 160=100ms) */
#define DEFAULT_ADVERTISING_INTERVAL_MIN            200 //125ms
/** @brief  Default maximum advertising interval */
#define DEFAULT_ADVERTISING_INTERVAL_MAX            250 //156.25ms

static bool s_ble_wifimate_device_app_enable = false;

extern T_SERVER_ID ble_wifimate_srv_id; /**< ble wifimate service id*/

extern ble_wifimate_device_link_t ble_wifimate_device_link_table[BLE_WIFIMATE_DEVICE_APP_MAX_LINKS];

/*============================================================================*
 *							   Functions
 *============================================================================*/
bool ble_wifimate_device_app_is_enable(void)
{
	return s_ble_wifimate_device_app_enable;
}

/**
 * @brief  Config bt stack related feature
 *
 * NOTE: This function shall be called before @ref bte_init is invoked.
 * @return void
 */
void ble_wifimate_device_bt_stack_config_init(void)
{
    gap_config_max_le_link_num(BLE_WIFIMATE_DEVICE_APP_MAX_LINKS);
    gap_config_max_le_paired_device(BLE_WIFIMATE_DEVICE_APP_MAX_LINKS);
    gap_config_max_mtu_size(BLE_WIFIMATE_DEVICE_APP_MAX_MTU_SIZE);
	gap_config_ccc_bits_count(16, 0);
}

/**
  * @brief  Initialize peripheral and gap bond manager related parameters
  * @return void
  */
void ble_wifimate_device_app_le_gap_init(void)
{
    /* Device name and device appearance */
    uint8_t  device_name[GAP_DEVICE_NAME_LEN] = "RTK_WiFiMate_xxyyzz";
    uint16_t appearance = GAP_GATT_APPEARANCE_UNKNOWN;
    uint8_t  slave_init_mtu_req = true;

    /* Advertising parameters */
    uint8_t  adv_evt_type = GAP_ADTYPE_ADV_IND;
    uint8_t  adv_direct_type = GAP_REMOTE_ADDR_LE_PUBLIC;
    uint8_t  adv_direct_addr[GAP_BD_ADDR_LEN] = {0};
    uint8_t  adv_chann_map = GAP_ADVCHAN_ALL;
    uint8_t  adv_filter_policy = GAP_ADV_FILTER_ANY;
    uint16_t adv_int_min = DEFAULT_ADVERTISING_INTERVAL_MIN;
    uint16_t adv_int_max = DEFAULT_ADVERTISING_INTERVAL_MAX;

    /* GAP Bond Manager parameters */
    uint8_t  auth_pair_mode = GAP_PAIRING_MODE_PAIRABLE;
    uint16_t auth_flags = GAP_AUTHEN_BIT_BONDING_FLAG;
    uint8_t  auth_io_cap = GAP_IO_CAP_NO_INPUT_NO_OUTPUT;
#if F_BT_LE_SMP_OOB_SUPPORT
    uint8_t  auth_oob = false;
#endif
    uint8_t  auth_use_fix_passkey = false;
    uint32_t auth_fix_passkey = 0;
    uint8_t  auth_sec_req_enable = false;
    uint16_t auth_sec_req_flags = GAP_AUTHEN_BIT_BONDING_FLAG;
    /* Set device name and device appearance */

    le_set_gap_param(GAP_PARAM_DEVICE_NAME, GAP_DEVICE_NAME_LEN, device_name);
    le_set_gap_param(GAP_PARAM_APPEARANCE, sizeof(appearance), &appearance);
    le_set_gap_param(GAP_PARAM_SLAVE_INIT_GATT_MTU_REQ, sizeof(slave_init_mtu_req),
                     &slave_init_mtu_req);

    /* Set advertising parameters */
    le_adv_set_param(GAP_PARAM_ADV_EVENT_TYPE, sizeof(adv_evt_type), &adv_evt_type);
    le_adv_set_param(GAP_PARAM_ADV_DIRECT_ADDR_TYPE, sizeof(adv_direct_type), &adv_direct_type);
    le_adv_set_param(GAP_PARAM_ADV_DIRECT_ADDR, sizeof(adv_direct_addr), adv_direct_addr);
    le_adv_set_param(GAP_PARAM_ADV_CHANNEL_MAP, sizeof(adv_chann_map), &adv_chann_map);
    le_adv_set_param(GAP_PARAM_ADV_FILTER_POLICY, sizeof(adv_filter_policy), &adv_filter_policy);
    le_adv_set_param(GAP_PARAM_ADV_INTERVAL_MIN, sizeof(adv_int_min), &adv_int_min);
    le_adv_set_param(GAP_PARAM_ADV_INTERVAL_MAX, sizeof(adv_int_max), &adv_int_max);
    /* not set adv data here, set adv data after bt stack enable */
    le_adv_set_param(GAP_PARAM_SCAN_RSP_DATA, sizeof(scan_rsp_data), (void *)scan_rsp_data);

    /* Setup the GAP Bond Manager */
    gap_set_param(GAP_PARAM_BOND_PAIRING_MODE, sizeof(auth_pair_mode), &auth_pair_mode);
    gap_set_param(GAP_PARAM_BOND_AUTHEN_REQUIREMENTS_FLAGS, sizeof(auth_flags), &auth_flags);
    gap_set_param(GAP_PARAM_BOND_IO_CAPABILITIES, sizeof(auth_io_cap), &auth_io_cap);
#if F_BT_LE_SMP_OOB_SUPPORT
    gap_set_param(GAP_PARAM_BOND_OOB_ENABLED, sizeof(auth_oob), &auth_oob);
#endif
    le_bond_set_param(GAP_PARAM_BOND_FIXED_PASSKEY, sizeof(auth_fix_passkey), &auth_fix_passkey);
    le_bond_set_param(GAP_PARAM_BOND_FIXED_PASSKEY_ENABLE, sizeof(auth_use_fix_passkey),
                      &auth_use_fix_passkey);
    le_bond_set_param(GAP_PARAM_BOND_SEC_REQ_ENABLE, sizeof(auth_sec_req_enable), &auth_sec_req_enable);
    le_bond_set_param(GAP_PARAM_BOND_SEC_REQ_REQUIREMENT, sizeof(auth_sec_req_flags),
                      &auth_sec_req_flags);

    /* register gap message callback */
    le_register_app_cb(ble_wifimate_device_app_gap_callback);

#if F_BT_LE_5_0_SET_PHY_SUPPORT
	uint8_t phys_prefer = GAP_PHYS_PREFER_ALL;
	uint8_t tx_phys_prefer = GAP_PHYS_PREFER_1M_BIT | GAP_PHYS_PREFER_2M_BIT;
	uint8_t rx_phys_prefer = GAP_PHYS_PREFER_1M_BIT | GAP_PHYS_PREFER_2M_BIT;
	le_set_gap_param(GAP_PARAM_DEFAULT_PHYS_PREFER, sizeof(phys_prefer), &phys_prefer);
	le_set_gap_param(GAP_PARAM_DEFAULT_TX_PHYS_PREFER, sizeof(tx_phys_prefer), &tx_phys_prefer);
	le_set_gap_param(GAP_PARAM_DEFAULT_RX_PHYS_PREFER, sizeof(rx_phys_prefer), &rx_phys_prefer);
#endif
}

/**
 * @brief  Add GATT services and register callbacks
 * @return void
 */
void ble_wifimate_device_app_le_profile_init(void)
{
    server_init(1);
    ble_wifimate_srv_id = ble_wifimate_add_service((void *)ble_wifimate_device_app_profile_callback);
    server_register_app_cb(ble_wifimate_device_app_profile_callback);
}

void ble_wifimate_device_task_init(uint16_t timeout)
{
    ble_wifimate_device_app_task_init();
    ble_wifimate_device_timer_init(timeout);
}
/**
 * @brief    Entry of APP code
 * @return   int (To avoid compile warning)
 */
int ble_wifimate_device_app_main(uint16_t timeout)
{
    bt_trace_init();
    ble_wifimate_device_bt_stack_config_init();
    bte_init();
    le_gap_init(BLE_WIFIMATE_DEVICE_APP_MAX_LINKS);
    ble_wifimate_device_app_le_gap_init();
    ble_wifimate_device_app_le_profile_init();
    ble_wifimate_device_task_init(timeout);

    return 0;
}

int ble_wifimate_device_app_init(uint16_t timeout)
{
	T_GAP_DEV_STATE new_state;

	if (s_ble_wifimate_device_app_enable == true) {
		printf("[BLE WiFiMate Device] BLE wifimate device app already init\r\n");
		return -1;
	}

	/*Wait WIFI init complete*/
	while(!(wifi_is_up(RTW_STA_INTERFACE) || wifi_is_up(RTW_AP_INTERFACE))) {
		os_delay(1000);
	}

	//judge bt ota peripheral is already on
	le_get_gap_param(GAP_PARAM_DEV_STATE , &new_state);
	if (new_state.gap_init_state == GAP_INIT_STATE_STACK_READY) {
		printf("[BLE WiFiMate Device]BT Stack already on\r\n");
		return 0;
	}
	else
		ble_wifimate_device_app_main(timeout);

	bt_coex_init();

	/*Wait BT init complete*/
	do {
		os_delay(100);
		le_get_gap_param(GAP_PARAM_DEV_STATE , &new_state);
	}while(new_state.gap_init_state != GAP_INIT_STATE_STACK_READY);

    s_ble_wifimate_device_app_enable = true;

	return 0;
}

int ble_wifimate_device_app_deinit(void)
{
    T_GAP_DEV_STATE state;

    if (s_ble_wifimate_device_app_enable == false) {
		printf("[BLE WiFiMate Device] BLE wifimate device app already deinit\r\n");
		return -1;
	}
	s_ble_wifimate_device_app_enable = false;

	le_get_gap_param(GAP_PARAM_DEV_STATE , &state);
	if (state.gap_init_state != GAP_INIT_STATE_STACK_READY) {
		printf("[BLE WiFiMate Device]BT Stack is not running\r\n");
        return -1;
	}
	else {
#if F_BT_DEINIT
		bte_deinit();
		bt_trace_uninit();
#endif
        memset((void *)ble_wifimate_device_link_table, 0,
                sizeof(ble_wifimate_device_link_t) * BLE_WIFIMATE_DEVICE_APP_MAX_LINKS);
        ble_wifimate_server_deinit();
        ble_wifimate_device_timer_deinit();
        ble_wifimate_device_app_task_deinit();
		printf("[BLE WiFiMate Device]BT Stack deinitalized\r\n");
	}

    return 0;
}

#endif /* CONFIG_BLE_WIFIMATE_DEVICE */