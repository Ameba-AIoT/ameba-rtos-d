/**
*****************************************************************************************
*     Copyright(c) 2017, Realtek Semiconductor Corporation. All rights reserved.
*****************************************************************************************
   * @file      bt_ota_peripheral_app.c
   * @brief     This file handles BLE OTA peripheral application routines.
   * @author    jane
   * @date      2017-06-06
   * @version   v1.0
   **************************************************************************************
   * @attention
   * <h2><center>&copy; COPYRIGHT 2017 Realtek Semiconductor Corporation</center></h2>
   **************************************************************************************
  */

/*============================================================================*
 *                              Header Files
 *============================================================================*/
#include "platform_opts_bt.h"
#if defined(CONFIG_BT_OTA_SERVER) && CONFIG_BT_OTA_SERVER
#include <stdio.h>
#include <string.h>
#include "platform_stdlib.h"
#include <trace_app.h>
#include <gap.h>
#include <gap_adv.h>
#include <gap_bond_le.h>
#include <gap_msg.h>
#include <gap_conn_le.h>
#include <profile_server.h>
#include <bt_ota_gatt_service.h>
#include <app_msg.h>
#include "bt_ota_server_app_flags.h"
#include "bt_ota_server_app.h"
#include "bt_ota_server_app_task.h"
#include <os_timer.h>

#define BLE_OTA_SERVER_SUPPORT_PAIRING        1 //user optional
#define OTA_DATA_RECV_TIMER_INTERVAL          900000     /*the max time for receiving 4k data*/
#define OTA_DATA_CHECKSUM_TIMER_INTERVAL      10000     /*the max time for waiting checksum */


/** @defgroup  PERIPH_APP Peripheral Application
	* @brief This file handles BLE peripheral application routines.
	* @{
	*/

/*============================================================================*
 *                              Variables
 *============================================================================*/
/** @addtogroup  PERIPH_SEVER_CALLBACK Profile Server Callback Event Handler
	* @brief Handle profile server callback event
	* @{
	*/
T_SERVER_ID bt_ota_srv_id;		/* bt ota service id */
/** @} */ /* End of group PERIPH_SEVER_CALLBACK */

/** @defgroup  PERIPH_GAP_MSG GAP Message Handler
    * @brief Handle GAP Message
    * @{
    */
T_GAP_DEV_STATE bt_ota_server_gap_dev_state = {0, 0, 0, 0, 0};                 /**< GAP device state */
T_GAP_CONN_STATE bt_ota_server_gap_conn_state = GAP_CONN_STATE_DISCONNECTED;   /**< GAP connection state */
/** @} */ /* End of group PERIPH_GAP_MSG */

#if BLE_OTA_SERVER_SUPPORT_PAIRING
uint8_t bt_ota_pairing_flag = 0;
#endif

void *bt_ota_data_recv_timer_hdl = NULL;
void *bt_ota_data_checksum_timer_hdl = NULL;

/*============================================================================*
 *                              Functions
 *============================================================================*/
void bt_ota_server_app_handle_gap_msg(T_IO_MSG	*p_gap_msg);

void bt_ota_server_conn_param_update(uint8_t conn_id)
{
	int ret;
	uint16_t conn_interval_min = 0x30;
	uint16_t conn_interval_max = 0x40;
	uint16_t conn_latency = 0;
	uint16_t supervision_timeout = 0x1f4;

	ret = le_update_conn_param(conn_id,
								conn_interval_min,
								conn_interval_max,
								conn_latency,
								supervision_timeout,
								2 * (conn_interval_min - 1),
								2 * (conn_interval_max - 1));
	if (ret != GAP_CAUSE_SUCCESS) {
		printf("%s: conn param update fail\r\n", __func__);
	}
#if BLE_OTA_SERVER_SUPPORT_PAIRING
	bt_ota_pairing_flag = 1;
#endif
}

void bt_ota_data_recv_timer_callback(void *arg)
{
	(void) arg;
	printf("%s: ota data recv timeout, ota failed and disconnect!\r\n", __func__);

	bt_ota_server_app_send_msg(0);
}

void bt_ota_data_checksum_timer_callback(void *arg)
{
	(void) arg;
	printf("%s: recv ota data checksum timeout, ota failed and disconnect!\r\n", __func__);

	bt_ota_server_app_send_msg(0);
}

void bt_ota_timer_init(void)
{
	os_timer_create(&bt_ota_data_recv_timer_hdl, "bt_ota_data_recv_timer", NULL,
					OTA_DATA_RECV_TIMER_INTERVAL, false, bt_ota_data_recv_timer_callback);
	os_timer_create(&bt_ota_data_checksum_timer_hdl, "bt_ota_data_checksum_timer", NULL,
					OTA_DATA_CHECKSUM_TIMER_INTERVAL, false, bt_ota_data_checksum_timer_callback);
}

void bt_ota_timer_deinit(void)
{
	if (bt_ota_data_recv_timer_hdl)
	{
		os_timer_delete(&bt_ota_data_recv_timer_hdl);
		bt_ota_data_recv_timer_hdl = NULL;
	}

	if (bt_ota_data_checksum_timer_hdl)
	{
		os_timer_delete(&bt_ota_data_checksum_timer_hdl);
		bt_ota_data_checksum_timer_hdl = NULL;
	}
}

void bt_ota_timer_stop(void)
{
	os_timer_stop(&bt_ota_data_recv_timer_hdl);
	os_timer_stop(&bt_ota_data_checksum_timer_hdl);
}

/** @defgroup  PERIPH_GAP_MSG GAP Message Handler
	* @brief Handle GAP Message
	* @{
	*/

/**
 * @brief    All the application messages are pre-handled in this function
 * @note     All the IO MSGs are sent to this function, then the event handling
 *           function shall be called according to the MSG type.
 * @param[in] io_msg  IO message data
 * @return   void
 */
void bt_ota_server_app_handle_io_msg(T_IO_MSG io_msg)
{
	uint16_t msg_type = io_msg.type;

	switch (msg_type) {
	case IO_MSG_TYPE_BT_STATUS: {
		bt_ota_server_app_handle_gap_msg(&io_msg);
	}
	break;

	default:
		break;
	}
}

/**
 * @brief    Handle msg GAP_MSG_LE_DEV_STATE_CHANGE
 * @note     All the gap device state events are pre-handled in this function.
 *           Then the event handling function shall be called according to the new_state
 * @param[in] new_state  New gap device state
 * @param[in] cause GAP device state change cause
 * @return   void
 */
void bt_ota_server_app_handle_dev_state_evt(T_GAP_DEV_STATE new_state, uint16_t cause)
{
	APP_PRINT_INFO3("bt_ota_server_app_handle_dev_state_evt: init state %d, adv state %d, cause 0x%x",
					new_state.gap_init_state, new_state.gap_adv_state, cause);
	if (bt_ota_server_gap_dev_state.gap_init_state != new_state.gap_init_state) {
		if (new_state.gap_init_state == GAP_INIT_STATE_STACK_READY) {
			APP_PRINT_INFO0("GAP stack ready");
			printf("[BT OTA Server] GAP stack ready\r\n");
			uint8_t bt_addr[6] = {0};
			/*stack ready*/
			gap_get_param(GAP_PARAM_BD_ADDR, bt_addr);
			printf("local bd addr: 0x%02x:%02x:%02x:%02x:%02x:%02x\r\n", \
				   bt_addr[5], bt_addr[4], bt_addr[3], bt_addr[2], bt_addr[1], bt_addr[0]);
			le_adv_start();
		}
	}

	if (bt_ota_server_gap_dev_state.gap_adv_state != new_state.gap_adv_state) {
		if (new_state.gap_adv_state == GAP_ADV_STATE_IDLE) {
			if (new_state.gap_adv_sub_state == GAP_ADV_TO_IDLE_CAUSE_CONN) {
				APP_PRINT_INFO0("GAP adv stoped: because connection created");
				printf("GAP adv stoped: because connection created\r\n");
			} else {
				APP_PRINT_INFO0("GAP adv stoped");
				printf("GAP adv stopped\r\n");
			}
		} else if (new_state.gap_adv_state == GAP_ADV_STATE_ADVERTISING) {
			APP_PRINT_INFO0("GAP adv start");
			printf("GAP adv start\r\n");
		}
	}

	bt_ota_server_gap_dev_state = new_state;
}

/**
 * @brief    Handle msg GAP_MSG_LE_CONN_STATE_CHANGE
 * @note     All the gap conn state events are pre-handled in this function.
 *           Then the event handling function shall be called according to the new_state
 * @param[in] conn_id Connection ID
 * @param[in] new_state  New gap connection state
 * @param[in] disc_cause Use this cause when new_state is GAP_CONN_STATE_DISCONNECTED
 * @return   void
 */
void bt_ota_server_app_handle_conn_state_evt(uint8_t conn_id, T_GAP_CONN_STATE new_state, uint16_t disc_cause)
{
	APP_PRINT_INFO4("bt_ota_server_app_handle_conn_state_evt: conn_id %d old_state %d new_state %d, disc_cause 0x%x",
					conn_id, bt_ota_server_gap_conn_state, new_state, disc_cause);

	switch (new_state) {
	case GAP_CONN_STATE_DISCONNECTED: {
		if ((disc_cause != (HCI_ERR | HCI_ERR_REMOTE_USER_TERMINATE))
			&& (disc_cause != (HCI_ERR | HCI_ERR_LOCAL_HOST_TERMINATE))) {
			APP_PRINT_ERROR1("bt_ota_server_app_handle_conn_state_evt: connection lost cause 0x%x", disc_cause);
		}
		printf("[BT OTA Server] BT Disconnected, cause 0x%x, start ADV\r\n", disc_cause);

		le_adv_start();
		bt_ota_timer_stop();
		/* update new image if the disconnect is due to OTA complete */
		bt_ota_update_new_image();
	}
	break;

	case GAP_CONN_STATE_CONNECTED: {
		uint16_t conn_interval;
		uint16_t conn_latency;
		uint16_t conn_supervision_timeout;
		uint8_t  remote_bd[6];
		T_GAP_REMOTE_ADDR_TYPE remote_bd_type;

		le_get_conn_param(GAP_PARAM_CONN_INTERVAL, &conn_interval, conn_id);
		le_get_conn_param(GAP_PARAM_CONN_LATENCY, &conn_latency, conn_id);
		le_get_conn_param(GAP_PARAM_CONN_TIMEOUT, &conn_supervision_timeout, conn_id);
		le_get_conn_addr(conn_id, remote_bd, (void *)&remote_bd_type);
		APP_PRINT_INFO5("GAP_CONN_STATE_CONNECTED:remote_bd %s, remote_addr_type %d, conn_interval 0x%x, conn_latency 0x%x, conn_supervision_timeout 0x%x",
						TRACE_BDADDR(remote_bd), remote_bd_type,
						conn_interval, conn_latency, conn_supervision_timeout);
		printf("[BT OTA Server] BT Connected\r\n");
	}
	break;

	default:
		break;
	}

	bt_ota_server_gap_conn_state = new_state;
}

/**
 * @brief    Handle msg GAP_MSG_LE_AUTHEN_STATE_CHANGE
 * @note     All the gap authentication state events are pre-handled in this function.
 *           Then the event handling function shall be called according to the new_state
 * @param[in] conn_id Connection ID
 * @param[in] new_state  New authentication state
 * @param[in] cause Use this cause when new_state is GAP_AUTHEN_STATE_COMPLETE
 * @return   void
 */
void bt_ota_server_app_handle_authen_state_evt(uint8_t conn_id, uint8_t new_state, uint16_t cause)
{
	APP_PRINT_INFO2("bt_ota_server_app_handle_authen_state_evt:conn_id %d, cause 0x%x", conn_id, cause);

	switch (new_state) {
	case GAP_AUTHEN_STATE_STARTED: {
		APP_PRINT_INFO0("bt_ota_server_app_handle_authen_state_evt: GAP_AUTHEN_STATE_STARTED");
	}
	break;

	case GAP_AUTHEN_STATE_COMPLETE: {
		if (cause == GAP_SUCCESS) {
			printf("Pair success\r\n");
			APP_PRINT_INFO0("bt_ota_server_app_handle_authen_state_evt: GAP_AUTHEN_STATE_COMPLETE pair success");

		} else {
			printf("Pair failed: cause 0x%x\r\n", cause);
			APP_PRINT_INFO0("bt_ota_server_app_handle_authen_state_evt: GAP_AUTHEN_STATE_COMPLETE pair failed");
		}
	}
	break;

	default:
		APP_PRINT_ERROR1("bt_ota_server_app_handle_authen_state_evt: unknown newstate %d", new_state);
		break;
	}
}

/**
 * @brief    Handle msg GAP_MSG_LE_CONN_MTU_INFO
 * @note     This msg is used to inform APP that exchange mtu procedure is completed.
 * @param[in] conn_id Connection ID
 * @param[in] mtu_size  New mtu size
 * @return   void
 */
void bt_ota_server_app_handle_conn_mtu_info_evt(uint8_t conn_id, uint16_t mtu_size)
{
	APP_PRINT_INFO2("bt_ota_server_app_handle_conn_mtu_info_evt: conn_id %d, mtu_size %d", conn_id, mtu_size);
	printf("conn_mtu_info_evt: conn_id %d, mtu_size %d\r\n", conn_id, mtu_size);
	bt_ota_server_conn_param_update(conn_id);
}

/**
 * @brief    Handle msg GAP_MSG_LE_CONN_PARAM_UPDATE
 * @note     All the connection parameter update change  events are pre-handled in this function.
 * @param[in] conn_id Connection ID
 * @param[in] status  New update state
 * @param[in] cause Use this cause when status is GAP_CONN_PARAM_UPDATE_STATUS_FAIL
 * @return   void
 */
void bt_ota_server_app_handle_conn_param_update_evt(uint8_t conn_id, uint8_t status, uint16_t cause)
{
	switch (status) {
	case GAP_CONN_PARAM_UPDATE_STATUS_SUCCESS: {
		uint16_t conn_interval;
		uint16_t conn_slave_latency;
		uint16_t conn_supervision_timeout;

		le_get_conn_param(GAP_PARAM_CONN_INTERVAL, &conn_interval, conn_id);
		le_get_conn_param(GAP_PARAM_CONN_LATENCY, &conn_slave_latency, conn_id);
		le_get_conn_param(GAP_PARAM_CONN_TIMEOUT, &conn_supervision_timeout, conn_id);
		APP_PRINT_INFO3("bt_ota_server_app_handle_conn_param_update_evt update success:conn_interval 0x%x, conn_slave_latency 0x%x, conn_supervision_timeout 0x%x",
						conn_interval, conn_slave_latency, conn_supervision_timeout);
		printf("bt_ota_server_app_handle_conn_param_update_evt update success:conn_interval 0x%x, conn_slave_latency 0x%x, conn_supervision_timeout 0x%x\r\n",
						conn_interval, conn_slave_latency, conn_supervision_timeout);
#if BLE_OTA_SERVER_SUPPORT_PAIRING
		if (bt_ota_pairing_flag) {
			bt_ota_pairing_flag = 0;
			le_bond_pair(conn_id);
		}
#endif
	}
	break;

	case GAP_CONN_PARAM_UPDATE_STATUS_FAIL: {
		APP_PRINT_ERROR1("bt_ota_server_app_handle_conn_param_update_evt update failed: cause 0x%x", cause);
		printf("bt_ota_server_app_handle_conn_param_update_evt update failed: cause 0x%x\r\n", cause);
	}
	break;

	case GAP_CONN_PARAM_UPDATE_STATUS_PENDING: {
		APP_PRINT_INFO0("bt_ota_server_app_handle_conn_param_update_evt update pending.");
		printf("bt_ota_server_app_handle_conn_param_update_evt update pending: conn_id %d\r\n", conn_id);
	}
	break;

	default:
		break;
	}
}

/**
 * @brief    All the BT GAP MSG are pre-handled in this function.
 * @note     Then the event handling function shall be called according to the
 *           subtype of T_IO_MSG
 * @param[in] p_gap_msg Pointer to GAP msg
 * @return   void
 */
void bt_ota_server_app_handle_gap_msg(T_IO_MSG *p_gap_msg)
{
	T_LE_GAP_MSG gap_msg;
	uint8_t conn_id;
	memcpy(&gap_msg, &p_gap_msg->u.param, sizeof(p_gap_msg->u.param));

	APP_PRINT_TRACE1("bt_ota_server_app_handle_gap_msg: subtype %d", p_gap_msg->subtype);
	switch (p_gap_msg->subtype) {
	case GAP_MSG_LE_DEV_STATE_CHANGE: {
		bt_ota_server_app_handle_dev_state_evt(gap_msg.msg_data.gap_dev_state_change.new_state,
								 gap_msg.msg_data.gap_dev_state_change.cause);
	}
	break;

	case GAP_MSG_LE_CONN_STATE_CHANGE: {
		bt_ota_server_app_handle_conn_state_evt(gap_msg.msg_data.gap_conn_state_change.conn_id,
								  (T_GAP_CONN_STATE)gap_msg.msg_data.gap_conn_state_change.new_state,
								  gap_msg.msg_data.gap_conn_state_change.disc_cause);
	}
	break;

	case GAP_MSG_LE_CONN_MTU_INFO: {
		bt_ota_server_app_handle_conn_mtu_info_evt(gap_msg.msg_data.gap_conn_mtu_info.conn_id,
									 gap_msg.msg_data.gap_conn_mtu_info.mtu_size);
	}
	break;

	case GAP_MSG_LE_CONN_PARAM_UPDATE: {
		bt_ota_server_app_handle_conn_param_update_evt(gap_msg.msg_data.gap_conn_param_update.conn_id,
										 gap_msg.msg_data.gap_conn_param_update.status,
										 gap_msg.msg_data.gap_conn_param_update.cause);
	}
	break;

	case GAP_MSG_LE_AUTHEN_STATE_CHANGE: {
		bt_ota_server_app_handle_authen_state_evt(gap_msg.msg_data.gap_authen_state.conn_id,
									gap_msg.msg_data.gap_authen_state.new_state,
									gap_msg.msg_data.gap_authen_state.status);
	}
	break;

	case GAP_MSG_LE_BOND_JUST_WORK: {
		conn_id = gap_msg.msg_data.gap_bond_just_work_conf.conn_id;
		le_bond_just_work_confirm(conn_id, GAP_CFM_CAUSE_ACCEPT);
		APP_PRINT_INFO0("GAP_MSG_LE_BOND_JUST_WORK");
	}
	break;

	case GAP_MSG_LE_BOND_PASSKEY_DISPLAY: {
		uint32_t display_value = 0;
		conn_id = gap_msg.msg_data.gap_bond_passkey_display.conn_id;
		le_bond_get_display_key(conn_id, &display_value);
		APP_PRINT_INFO1("GAP_MSG_LE_BOND_PASSKEY_DISPLAY:passkey %d", display_value);
		le_bond_passkey_display_confirm(conn_id, GAP_CFM_CAUSE_ACCEPT);
		printf("GAP_MSG_LE_BOND_PASSKEY_DISPLAY:passkey %06d\r\n", display_value);
	}
	break;

	case GAP_MSG_LE_BOND_USER_CONFIRMATION: {
		uint32_t display_value = 0;
		conn_id = gap_msg.msg_data.gap_bond_user_conf.conn_id;
		le_bond_get_display_key(conn_id, &display_value);
		APP_PRINT_INFO1("GAP_MSG_LE_BOND_USER_CONFIRMATION: passkey %d", display_value);
		printf("GAP_MSG_LE_BOND_USER_CONFIRMATION: passkey %06d\r\n", display_value);
		//le_bond_user_confirm(conn_id, GAP_CFM_CAUSE_ACCEPT);
	}
	break;

	case GAP_MSG_LE_BOND_PASSKEY_INPUT: {
		//uint32_t passkey = 888888;
		conn_id = gap_msg.msg_data.gap_bond_passkey_input.conn_id;
		APP_PRINT_INFO1("GAP_MSG_LE_BOND_PASSKEY_INPUT: conn_id %d", conn_id);
		//le_bond_passkey_input_confirm(conn_id, passkey, GAP_CFM_CAUSE_ACCEPT);
	    printf("GAP_MSG_LE_BOND_PASSKEY_INPUT: conn_id %d\r\n", conn_id);
	}
	break;
#if F_BT_LE_SMP_OOB_SUPPORT
	case GAP_MSG_LE_BOND_OOB_INPUT: {
		uint8_t oob_data[GAP_OOB_LEN] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
		conn_id = gap_msg.msg_data.gap_bond_oob_input.conn_id;
		APP_PRINT_INFO0("GAP_MSG_LE_BOND_OOB_INPUT");
		le_bond_set_param(GAP_PARAM_BOND_OOB_DATA, GAP_OOB_LEN, oob_data);
		le_bond_oob_input_confirm(conn_id, GAP_CFM_CAUSE_ACCEPT);
	}
	break;
#endif
	default:
		APP_PRINT_ERROR1("bt_ota_server_app_handle_gap_msg: unknown subtype %d", p_gap_msg->subtype);
		break;
	}
}
/** @} */ /* End of group PERIPH_GAP_MSG */

/** @defgroup  PERIPH_GAP_CALLBACK GAP Callback Event Handler
    * @brief Handle GAP callback event
    * @{
    */
/**
  * @brief Callback for gap le to notify app
  * @param[in] cb_type callback msy type @ref GAP_LE_MSG_Types.
  * @param[in] p_cb_data point to callback data @ref T_LE_CB_DATA.
  * @retval result @ref T_APP_RESULT
  */
T_APP_RESULT bt_ota_server_app_gap_callback(uint8_t cb_type, void *p_cb_data)
{
	T_APP_RESULT result = APP_RESULT_SUCCESS;
	T_LE_CB_DATA *p_data = (T_LE_CB_DATA *)p_cb_data;

	switch (cb_type) {
#if F_BT_LE_4_2_DATA_LEN_EXT_SUPPORT
	case GAP_MSG_LE_DATA_LEN_CHANGE_INFO: {
		APP_PRINT_INFO3("GAP_MSG_LE_DATA_LEN_CHANGE_INFO: conn_id %d, tx octets 0x%x, max_tx_time 0x%x",
						p_data->p_le_data_len_change_info->conn_id,
						p_data->p_le_data_len_change_info->max_tx_octets,
						p_data->p_le_data_len_change_info->max_tx_time);
	}
	break;
#endif
#if F_BT_LE_5_0_SET_PHY_SUPPORT
	case GAP_MSG_LE_PHY_UPDATE_INFO: {
		APP_PRINT_INFO4("GAP_MSG_LE_PHY_UPDATE_INFO:conn_id %d, cause 0x%x, rx_phy %d, tx_phy %d",
						p_data->p_le_phy_update_info->conn_id,
						p_data->p_le_phy_update_info->cause,
						p_data->p_le_phy_update_info->rx_phy,
						p_data->p_le_phy_update_info->tx_phy);
		printf("GAP_MSG_LE_PHY_UPDATE_INFO:conn_id %d, cause 0x%x, rx_phy %d, tx_phy %d\r\n",
						p_data->p_le_phy_update_info->conn_id,
						p_data->p_le_phy_update_info->cause,
						p_data->p_le_phy_update_info->rx_phy,
						p_data->p_le_phy_update_info->tx_phy);
	}
	break;

	case GAP_MSG_LE_REMOTE_FEATS_INFO: {
		uint8_t  remote_feats[8];
		APP_PRINT_INFO3("GAP_MSG_LE_REMOTE_FEATS_INFO: conn id %d, cause 0x%x, remote_feats %b",
						p_data->p_le_remote_feats_info->conn_id,
						p_data->p_le_remote_feats_info->cause,
						TRACE_BINARY(8, p_data->p_le_remote_feats_info->remote_feats));
		if (p_data->p_le_remote_feats_info->cause == GAP_SUCCESS) {
			memcpy(remote_feats, p_data->p_le_remote_feats_info->remote_feats, 8);
			if (remote_feats[LE_SUPPORT_FEATURES_MASK_ARRAY_INDEX1] & LE_SUPPORT_FEATURES_LE_2M_MASK_BIT) {
				APP_PRINT_INFO0("GAP_MSG_LE_REMOTE_FEATS_INFO: support 2M");
				printf("GAP_MSG_LE_REMOTE_FEATS_INFO: support 2M\r\n");
			}
			if (remote_feats[LE_SUPPORT_FEATURES_MASK_ARRAY_INDEX1] & LE_SUPPORT_FEATURES_LE_CODED_PHY_MASK_BIT) {
				APP_PRINT_INFO0("GAP_MSG_LE_REMOTE_FEATS_INFO: support CODED");
				printf("GAP_MSG_LE_REMOTE_FEATS_INFO: support CODED\r\n");
			}
		}
	}
	break;
#endif
	case GAP_MSG_LE_MODIFY_WHITE_LIST:
		APP_PRINT_INFO2("GAP_MSG_LE_MODIFY_WHITE_LIST: operation %d, cause 0x%x",
						p_data->p_le_modify_white_list_rsp->operation,
						p_data->p_le_modify_white_list_rsp->cause);
		break;
	default:
		APP_PRINT_ERROR1("bt_ota_server_app_gap_callback: unhandled cb_type 0x%x", cb_type);
		break;
	}
	return result;
}
/** @} */ /* End of group PERIPH_GAP_CALLBACK */

/** @defgroup  PERIPH_SEVER_CALLBACK Profile Server Callback Event Handler
    * @brief Handle profile server callback event
    * @{
    */
/**
    * @brief    All the BT Profile service callback events are handled in this function
    * @note     Then the event handling function shall be called according to the
    *           service_id
    * @param    service_id  Profile service ID
    * @param    p_data      Pointer to callback data
    * @return   T_APP_RESULT, which indicates the function call is successful or not
    * @retval   APP_RESULT_SUCCESS  Function run successfully
    * @retval   others              Function run failed, and return number indicates the reason
    */
T_APP_RESULT bt_ota_server_app_profile_callback(T_SERVER_ID service_id, void *p_data)
{
	T_APP_RESULT app_result = APP_RESULT_SUCCESS;

	if (service_id == SERVICE_PROFILE_GENERAL_ID) {
		T_SERVER_APP_CB_DATA *p_param = (T_SERVER_APP_CB_DATA *)p_data;
		switch (p_param->eventId) {
		case PROFILE_EVT_SRV_REG_COMPLETE: {// srv register result event.
			APP_PRINT_INFO1("PROFILE_EVT_SRV_REG_COMPLETE: result %d",
							p_param->event_data.service_reg_result);
		}
		break;

		case PROFILE_EVT_SEND_DATA_COMPLETE: {
			APP_PRINT_INFO5("PROFILE_EVT_SEND_DATA_COMPLETE: conn_id %d, cause 0x%x, service_id %d, attrib_idx 0x%x, credits %d",
							p_param->event_data.send_data_result.conn_id,
							p_param->event_data.send_data_result.cause,
							p_param->event_data.send_data_result.service_id,
							p_param->event_data.send_data_result.attrib_idx,
							p_param->event_data.send_data_result.credits);
			printf("PROFILE_EVT_SEND_DATA_COMPLETE: conn_id %d, cause 0x%x, service_id %d, attrib_idx 0x%x, credits %d\r\n",
							p_param->event_data.send_data_result.conn_id,
							p_param->event_data.send_data_result.cause,
							p_param->event_data.send_data_result.service_id,
							p_param->event_data.send_data_result.attrib_idx,
							p_param->event_data.send_data_result.credits);

			if (p_param->event_data.send_data_result.cause == GAP_SUCCESS) {
				bt_ota_disconnect(p_param->event_data.send_data_result.conn_id);
			} else {
				APP_PRINT_ERROR0("PROFILE_EVT_SEND_DATA_COMPLETE failed");
				printf("PROFILE_EVT_SEND_DATA_COMPLETE failed\r\n");
			}
		}
		break;

		default:
			break;
		}
	} else if (service_id == bt_ota_srv_id) {
		T_OTA_CALLBACK_DATA *p_ota_cb_data = (T_OTA_CALLBACK_DATA *)p_data;
		uint8_t conn_id = p_ota_cb_data->conn_id;

		switch (p_ota_cb_data->msg_type) {
		case SERVICE_CALLBACK_TYPE_INDIFICATION_NOTIFICATION: {
			if (p_ota_cb_data->msg_data.notify_indicate_status == OTA_CTRL_INDICATE_ENABLE) {
				APP_PRINT_INFO0("OTA_CTRL_INDICATE_ENABLE");
				printf("OTA_CTRL_INDICATE_ENABLE\r\n");
				bt_ota_start(conn_id, service_id);
			} else {
				APP_PRINT_INFO0("OTA_CTRL_INDICATE_DISABLE");
				printf("OTA_CTRL_INDICATE_DISABLE\r\n");
			}
		}
		break;

		case SERVICE_CALLBACK_TYPE_WRITE_CHAR_VALUE: {
			APP_PRINT_INFO3("OTA WRITE CALLBACK: opcode %d, write_type %d, len %d", p_ota_cb_data->msg_data.write.opcode,
							p_ota_cb_data->msg_data.write.write_type, p_ota_cb_data->msg_data.write.len);

			uint8_t *p_value = p_ota_cb_data->msg_data.write.p_value;
			uint16_t len = p_ota_cb_data->msg_data.write.len;
			uint8_t ret = 0;

			switch (p_ota_cb_data->msg_data.write.opcode) {
			case OTA_IMAGE_HEADER_WRITE: {
				printf("OTA_IMAGE_HEADER_WRITE\r\n");
				bt_ota_handle_ota_header_packet(conn_id, p_value, len);
			}
			break;
			case OTA_HEADER_CHECKSUM_WRITE: {
				printf("OTA_HEADER_CHECKSUM_WRITE\r\n");
				ret = bt_ota_handle_ota_header_checksum_packet(conn_id, p_value, len);
				/* get ota header successfully and start the timer to receive ota data */
				if (ret) {
					os_timer_start(&bt_ota_data_recv_timer_hdl);
				}
			}
			break;
			case OTA_DATA_WRITE: {
				ret = bt_ota_handle_ota_image_data_packet(conn_id, p_value, len);
				/* handle a buffer data successfully, reset the timer*/
				if (ret) {
					/* received 4k data, stop timer immediately. */
					os_timer_stop(&bt_ota_data_recv_timer_hdl);
					/* start checksum_timer to wait checksum */
					os_timer_start(&bt_ota_data_checksum_timer_hdl);
				}
			}
			break;
			case OTA_DATA_CHECKSUM_WRITE: {
				/*recv remote checksum and stop the timer immediately*/
				os_timer_stop(&bt_ota_data_checksum_timer_hdl);

				ret = bt_ota_handle_ota_data_checksum_packet(conn_id, p_value, len);
				if (ret) {
					/*restart the recv_data_timer for receiving remain image data */
					os_timer_start(&bt_ota_data_recv_timer_hdl);
				}
			}
			break;
			default:
				break;
			}
		}
		break;

		default:
			break;
		}
	}

	return app_result;
}
/** @} */ /* End of group PERIPH_SEVER_CALLBACK */

#endif
