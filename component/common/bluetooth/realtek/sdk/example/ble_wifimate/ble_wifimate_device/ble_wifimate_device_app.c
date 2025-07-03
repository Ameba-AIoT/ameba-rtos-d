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
#include "platform_stdlib.h"
#include <trace_app.h>
#include <os_timer.h>
#include <gap.h>
#include <gap_adv.h>
#include <gap_bond_le.h>
#include <gap_msg.h>
#include <gap_conn_le.h>
#include <profile_server.h>
#include <app_msg.h>
#include <atcmd_bt.h>
#include <ble_wifimate_service.h>
#include <ble_wifimate_device_app.h>

/*============================================================================*
 *                              Variables
 *============================================================================*/

ble_wifimate_device_link_t ble_wifimate_device_link_table[BLE_WIFIMATE_DEVICE_APP_MAX_LINKS] = {0};

/** @brief  GAP device state */
T_GAP_DEV_STATE ble_wifimate_device_gap_dev_state = {0, 0, 0, 0, 0};

/** @brief GAP connection state */
T_GAP_CONN_STATE ble_wifimate_device_gap_conn_state = GAP_CONN_STATE_DISCONNECTED;

static void *s_ble_wifimate_timer_hdl = NULL;

/** @brief  GAP - Advertisement data (max size = 31 bytes, best kept short to conserve power) */
static uint8_t adv_data[] =
{
    /* Flags */
    0x02,             /* length */
    GAP_ADTYPE_FLAGS, /* type="Flags" */
    GAP_ADTYPE_FLAGS_LIMITED | GAP_ADTYPE_FLAGS_BREDR_NOT_SUPPORTED,
    /* Service */
	0x03,
    GAP_ADTYPE_16BIT_COMPLETE,
    LO_WORD(BLE_WIFIMATE_UUID_SRV),
	HI_WORD(BLE_WIFIMATE_UUID_SRV),
    /* Local name */
    0x14,            /* length */
    GAP_ADTYPE_LOCAL_NAME_COMPLETE,
    'R', 'T', 'K', '_', 'W', 'i', 'F', 'i', 'M', 'a', 't', 'e', '_', 'x', 'x', 'y', 'y', 'z', 'z',
};

extern void ble_wifimate_device_app_send_msg(uint16_t subtype, void *buf);
void ble_wifimate_device_app_handle_gap_msg(T_IO_MSG *p_gap_msg);

static void addr_to_str(uint8_t *addr_val, T_GAP_REMOTE_ADDR_TYPE addr_type, char *str, uint32_t len)
{
	char type[10];
	memset(str, 0, len);

	if (!addr_val || !str) {
		return;
	}

	switch (addr_type) {
	case GAP_LOCAL_ADDR_LE_PUBLIC:
		strncpy(type, "public", 7);
		break;
	case GAP_LOCAL_ADDR_LE_RANDOM:
		strncpy(type, "random", 7);
		break;
#if F_BT_LE_PRIVACY_SUPPORT
	case GAP_LOCAL_ADDR_LE_RAP_OR_PUBLIC:
		strncpy(type, "public-id", 10);
		break;
	case GAP_LOCAL_ADDR_LE_RAP_OR_RAND:
		strncpy(type, "random-id", 10);
		break;
#endif
	default:
		snprintf(type, sizeof(type), "0x%02x", addr_type);
		break;
	}
	snprintf(str, len, "%02X:%02X:%02X:%02X:%02X:%02X(%s)",
			 addr_val[5], addr_val[4], addr_val[3],
			 addr_val[2], addr_val[1], addr_val[0], type);
}

static void ble_wifimate_device_app_handle_qdecode_cmd(uint16_t subtype, void *arg)
{
	(void)arg;
	T_GAP_DEV_STATE state;

	le_get_gap_param(GAP_PARAM_DEV_STATE , &state);
	switch (subtype) {
		case BLE_WIFIMATE_DEVICE_ADV_START: {
			if (state.gap_init_state == GAP_INIT_STATE_STACK_READY) {
				le_adv_start();
			} else {
				printf("BT stack not ready\r\n");
			}
		}
		break;

		case BLE_WIFIMATE_DEVICE_ADV_STOP: {
			if (state.gap_init_state == GAP_INIT_STATE_STACK_READY) {
				le_adv_stop();
			} else {
				printf("BT stack not ready\r\n");
			}
		}
		break;

		default:
			break;
	}
}

/**
 * @brief    All the application messages are pre-handled in this function
 * @note     All the IO MSGs are sent to this function, then the event handling
 *           function shall be called according to the MSG type.
 * @param[in] io_msg  IO message data
 * @return   void
 */
void ble_wifimate_device_app_handle_io_msg(T_IO_MSG io_msg)
{
	uint16_t msg_type = io_msg.type;

	switch (msg_type) {
	case IO_MSG_TYPE_BT_STATUS: {
		ble_wifimate_device_app_handle_gap_msg(&io_msg);
		break;
	}

	case IO_MSG_TYPE_QDECODE: {
		uint16_t subtype = io_msg.subtype;
		void *arg = io_msg.u.buf;
		ble_wifimate_device_app_handle_qdecode_cmd(subtype, arg);
		break;
	}

	default:
		break;
	}
}

/**
 * @brief    Update connection parameter
 * @param[in] conn_id Connection ID
 * @return   void
 */
void ble_wifimate_device_conn_param_update(uint8_t conn_id)
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
}

/**
  * @brief Callback for gap le to notify app
  * @param[in] cb_type callback msy type @ref GAP_LE_MSG_Types.
  * @param[in] p_cb_data point to callback data @ref T_LE_CB_DATA.
  * @retval result @ref T_APP_RESULT
  */
T_APP_RESULT ble_wifimate_device_app_gap_callback(uint8_t cb_type, void *p_cb_data)
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
	case GAP_MSG_LE_MODIFY_WHITE_LIST:
		APP_PRINT_INFO2("GAP_MSG_LE_MODIFY_WHITE_LIST: operation %d, cause 0x%x",
						p_data->p_le_modify_white_list_rsp->operation,
						p_data->p_le_modify_white_list_rsp->cause);
		break;
	default:
		APP_PRINT_ERROR2("%s: unhandled cb_type 0x%x", __func__, cb_type);
		break;
	}
	return result;
}

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
T_APP_RESULT ble_wifimate_device_app_profile_callback(T_SERVER_ID service_id, void *p_data)
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

			if (p_param->event_data.send_data_result.attrib_idx == BLE_WIFIMATE_WIFI_SCAN_INFO_VAL_INDEX) {
				ble_wifimate_server_send_wifi_scan_info_segment(p_param->event_data.send_data_result.conn_id);
			} else if (p_param->event_data.send_data_result.attrib_idx == BLE_WIFIMATE_WIFI_CONNECT_STATE_VAL_INDEX) {
				le_disconnect(p_param->event_data.send_data_result.conn_id);
			}
		}
		break;

		default:
			break;
		}
	} else if (service_id == ble_wifimate_srv_id) {
		BLE_WIFIMATE_CALLBACK_DATA *p_cb_data = (BLE_WIFIMATE_CALLBACK_DATA *)p_data;

		switch (p_cb_data->msg_type) {
		case SERVICE_CALLBACK_TYPE_INDIFICATION_NOTIFICATION: {
			if (p_cb_data->msg_data.notify_indicate_index == BLE_WIFIMATE_INDICATE_ENABLE) {
				APP_PRINT_INFO0("BLE_WIFIMATE_INDICATE_ENABLE");
				printf("BLE_WIFIMATE_INDICATE_ENABLE\r\n");
			} else {
				APP_PRINT_INFO0("BLE_WIFIMATE_INDICATE_DISABLE");
				printf("BLE_WIFIMATE_INDICATE_DISABLE\r\n");
			}
		}
		break;

		case SERVICE_CALLBACK_TYPE_WRITE_CHAR_VALUE: {
			printf("BLE WiFiMate WRITE CALLBACK: opcode %d, write_type %d, len %d", p_cb_data->msg_data.write.index,
							p_cb_data->msg_data.write.write_type, p_cb_data->msg_data.write.len);
		}
		break;

		default:
			break;
		}
	}
	return app_result;
}

/**
 * @brief    Handle msg GAP_MSG_LE_DEV_STATE_CHANGE
 * @note     All the gap device state events are pre-handled in this function.
 *           Then the event handling function shall be called according to the new_state
 * @param[in] new_state  New gap device state
 * @param[in] cause GAP device state change cause
 * @return   void
 */
void ble_wifimate_device_app_handle_dev_state_evt(T_GAP_DEV_STATE new_state, uint16_t cause)
{
	APP_PRINT_INFO3("ble_wifimate_device_app_handle_dev_state_evt: init state %d, adv state %d, cause 0x%x",
					new_state.gap_init_state, new_state.gap_adv_state, cause);
	if (ble_wifimate_device_gap_dev_state.gap_init_state != new_state.gap_init_state) {
		if (new_state.gap_init_state == GAP_INIT_STATE_STACK_READY) {
			uint8_t bt_addr[6] = {0};
			char device_name[30] = "RTK_WiFiMate_xxyyzz";

			APP_PRINT_INFO0("GAP stack ready");
			printf("[BLE WiFiMate Device] GAP stack ready\r\n");
			/*stack ready*/
			gap_get_param(GAP_PARAM_BD_ADDR, bt_addr);
			printf("local bd addr: 0x%02x:%02x:%02x:%02x:%02x:%02x\r\n", \
				   bt_addr[5], bt_addr[4], bt_addr[3], bt_addr[2], bt_addr[1], bt_addr[0]);
			sprintf((char *)device_name, "RTK_WiFiMate_%02X%02X%02X", bt_addr[2], bt_addr[1], bt_addr[0]);
			memcpy(adv_data + 9, device_name, strlen((char const *)device_name));

			le_adv_set_param(GAP_PARAM_ADV_DATA, sizeof(adv_data), (void *)adv_data);
			le_set_gap_param(GAP_PARAM_DEVICE_NAME, strlen(device_name), device_name);

			le_adv_start();
		}
	}

	if (ble_wifimate_device_gap_dev_state.gap_adv_state != new_state.gap_adv_state) {
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

			if (s_ble_wifimate_timer_hdl) {
				printf("[APP] ble wifimate timer start succeed!\r\n");
				os_timer_start(&s_ble_wifimate_timer_hdl);
			}
		}
#if defined(BT_AT_SYNC) && BT_AT_SYNC
        if ((new_state.gap_adv_state == GAP_ADV_STATE_IDLE) ||
            (new_state.gap_adv_state == GAP_ADV_STATE_ADVERTISING)) {
            if (bt_at_sync_event_match_check(BT_AT_SYNC_CMD_TYPE_BLE_GAP_ADV)) {
                bt_at_sync_set_result_by_evt_cause(cause);
                bt_at_sync_sem_give();
            }
        }
#endif
	}

	ble_wifimate_device_gap_dev_state = new_state;
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
void ble_wifimate_device_app_handle_conn_state_evt(uint8_t conn_id, T_GAP_CONN_STATE new_state, uint16_t disc_cause)
{
	APP_PRINT_INFO4("ble_wifimate_device_app_handle_conn_state_evt: conn_id %d old_state %d new_state %d, disc_cause 0x%x",
					conn_id, ble_wifimate_device_gap_conn_state, new_state, disc_cause);

	switch (new_state) {
	case GAP_CONN_STATE_DISCONNECTED: {
		if ((disc_cause != (HCI_ERR | HCI_ERR_REMOTE_USER_TERMINATE))
			&& (disc_cause != (HCI_ERR | HCI_ERR_LOCAL_HOST_TERMINATE))) {
			APP_PRINT_ERROR1("ble_wifimate_device_app_handle_conn_state_evt: connection lost cause 0x%x", disc_cause);
		}
		printf("[BLE WiFiMate Device] BT Disconnected, cause 0x%x, start ADV\r\n", disc_cause);

		le_adv_start();
		// ble wifimate server action
		ble_wifimate_server_disconnect(conn_id);

#if defined(BT_AT_SYNC) && BT_AT_SYNC
		uint8_t  remote_bd[6];
		uint16_t conn_handle = ble_wifimate_device_link_table[conn_id].conn_handle;

		if (disc_cause != (HCI_ERR | HCI_ERR_LOCAL_HOST_TERMINATE)) {
			char le_addr[30] = {0};
			addr_to_str(ble_wifimate_device_link_table[conn_id].bd_addr,
						ble_wifimate_device_link_table[conn_id].bd_type,
						le_addr,
						sizeof(le_addr));
			BT_AT_PRINT("[$]+BLEGAP:disconn,%d,%s\r\n", conn_handle, le_addr);
		}

		bt_at_sync_disconnect_hdl(conn_handle);
#endif
		memset(&ble_wifimate_device_link_table[conn_id], 0, sizeof(ble_wifimate_device_link_table[conn_id]));
	}
	break;

	case GAP_CONN_STATE_CONNECTED: {
		uint16_t conn_interval;
		uint16_t conn_latency;
		uint16_t conn_supervision_timeout;
		uint8_t  remote_bd[6];
		uint16_t conn_handle = le_get_conn_handle(conn_id);
		T_GAP_REMOTE_ADDR_TYPE remote_bd_type;

		le_get_conn_param(GAP_PARAM_CONN_INTERVAL, &conn_interval, conn_id);
		le_get_conn_param(GAP_PARAM_CONN_LATENCY, &conn_latency, conn_id);
		le_get_conn_param(GAP_PARAM_CONN_TIMEOUT, &conn_supervision_timeout, conn_id);
		le_get_conn_addr(conn_id, remote_bd, (void *)&remote_bd_type);
		ble_wifimate_device_link_table[conn_id].conn_handle = conn_handle;
		ble_wifimate_device_link_table[conn_id].bd_type = remote_bd_type;
		memcpy(ble_wifimate_device_link_table[conn_id].bd_addr, remote_bd, sizeof(remote_bd));

		APP_PRINT_INFO5("GAP_CONN_STATE_CONNECTED:remote_bd %s, remote_addr_type %d, conn_interval 0x%x, conn_latency 0x%x, conn_supervision_timeout 0x%x",
						TRACE_BDADDR(remote_bd), remote_bd_type,
						conn_interval, conn_latency, conn_supervision_timeout);
		printf("[BLE WiFiMate Device] BT Connected, conn_handle=%d\r\n", conn_handle);

		ble_wifimate_server_connect(conn_id);

		if (s_ble_wifimate_timer_hdl) {
			os_timer_stop(&s_ble_wifimate_timer_hdl);
			printf("[APP] ble wifimate stop timer\r\n");
		}
#if defined(BT_AT_SYNC) && BT_AT_SYNC
		char le_addr[30] = {0};
		addr_to_str(remote_bd, remote_bd_type, le_addr, sizeof(le_addr));
		BT_AT_PRINT("[$]+BLEGAP:conn,%d,%s\r\n", conn_handle, le_addr);
#endif
	}
	break;

	default:
		break;
	}

	ble_wifimate_device_gap_conn_state = new_state;
}

/**
 * @brief    Handle msg GAP_MSG_LE_CONN_MTU_INFO
 * @note     This msg is used to inform APP that exchange mtu procedure is completed.
 * @param[in] conn_id Connection ID
 * @param[in] mtu_size  New mtu size
 * @return   void
 */
void ble_wifimate_device_app_handle_conn_mtu_info_evt(uint8_t conn_id, uint16_t mtu_size)
{
	APP_PRINT_INFO2("ble_wifimate_device_app_handle_conn_mtu_info_evt: conn_id %d, mtu_size %d", conn_id, mtu_size);
	printf("conn_mtu_info_evt: conn_id %d, mtu_size %d\r\n", conn_id, mtu_size);
	ble_wifimate_server_set_mtu_size(mtu_size);
	ble_wifimate_device_conn_param_update(conn_id);
}

/**
 * @brief    Handle msg GAP_MSG_LE_CONN_PARAM_UPDATE
 * @note     All the connection parameter update change  events are pre-handled in this function.
 * @param[in] conn_id Connection ID
 * @param[in] status  New update state
 * @param[in] cause Use this cause when status is GAP_CONN_PARAM_UPDATE_STATUS_FAIL
 * @return   void
 */
void ble_wifimate_device_app_handle_conn_param_update_evt(uint8_t conn_id, uint8_t status, uint16_t cause)
{
	switch (status) {
	case GAP_CONN_PARAM_UPDATE_STATUS_SUCCESS: {
		uint16_t conn_interval;
		uint16_t conn_slave_latency;
		uint16_t conn_supervision_timeout;

		le_get_conn_param(GAP_PARAM_CONN_INTERVAL, &conn_interval, conn_id);
		le_get_conn_param(GAP_PARAM_CONN_LATENCY, &conn_slave_latency, conn_id);
		le_get_conn_param(GAP_PARAM_CONN_TIMEOUT, &conn_supervision_timeout, conn_id);
		APP_PRINT_INFO4("%s update success:conn_interval 0x%x, conn_slave_latency 0x%x, conn_supervision_timeout 0x%x",
						__func__, conn_interval, conn_slave_latency, conn_supervision_timeout);
		printf("%s update success:conn_interval 0x%x, conn_slave_latency 0x%x, conn_supervision_timeout 0x%x\r\n",
				__func__, conn_interval, conn_slave_latency, conn_supervision_timeout);
	}
	break;

	case GAP_CONN_PARAM_UPDATE_STATUS_FAIL: {
		APP_PRINT_ERROR2("%s update failed: cause 0x%x", __func__, cause);
		printf("%s update failed: cause 0x%x\r\n", __func__, cause);
	}
	break;

	case GAP_CONN_PARAM_UPDATE_STATUS_PENDING: {
		APP_PRINT_INFO1("%s update pending.", __func__);
		printf("%s update pending: conn_id %d\r\n", __func__, conn_id);
	}
	break;

	default:
		break;
	}
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
void ble_wifimate_device_app_handle_authen_state_evt(uint8_t conn_id, uint8_t new_state, uint16_t cause)
{
	APP_PRINT_INFO3("%s:conn_id %d, cause 0x%x", __func__, conn_id, cause);

	switch (new_state) {
	case GAP_AUTHEN_STATE_STARTED: {
		APP_PRINT_INFO1("%s: GAP_AUTHEN_STATE_STARTED", __func__);
	}
	break;

	case GAP_AUTHEN_STATE_COMPLETE: {
		if (cause == GAP_SUCCESS) {
			printf("Pair success\r\n");
			APP_PRINT_INFO1("%s: GAP_AUTHEN_STATE_COMPLETE pair success", __func__);

		} else {
			printf("Pair failed: cause 0x%x\r\n", cause);
			APP_PRINT_INFO1("%s: GAP_AUTHEN_STATE_COMPLETE pair failed", __func__);
		}
	}
	break;

	default:
		APP_PRINT_ERROR2("%s: unknown newstate %d", __func__, new_state);
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
void ble_wifimate_device_app_handle_gap_msg(T_IO_MSG *p_gap_msg)
{
	T_LE_GAP_MSG gap_msg;
	uint8_t conn_id;
	memcpy(&gap_msg, &p_gap_msg->u.param, sizeof(p_gap_msg->u.param));

	APP_PRINT_TRACE1("ble_wifimate_device_app_handle_gap_msg: subtype %d", p_gap_msg->subtype);
	switch (p_gap_msg->subtype) {
	case GAP_MSG_LE_DEV_STATE_CHANGE: {
		ble_wifimate_device_app_handle_dev_state_evt(gap_msg.msg_data.gap_dev_state_change.new_state,
								 gap_msg.msg_data.gap_dev_state_change.cause);
	}
	break;

	case GAP_MSG_LE_CONN_STATE_CHANGE: {
		ble_wifimate_device_app_handle_conn_state_evt(gap_msg.msg_data.gap_conn_state_change.conn_id,
								  (T_GAP_CONN_STATE)gap_msg.msg_data.gap_conn_state_change.new_state,
								  gap_msg.msg_data.gap_conn_state_change.disc_cause);
	}
	break;

	case GAP_MSG_LE_CONN_MTU_INFO: {
		ble_wifimate_device_app_handle_conn_mtu_info_evt(gap_msg.msg_data.gap_conn_mtu_info.conn_id,
									 gap_msg.msg_data.gap_conn_mtu_info.mtu_size);
	}
	break;

	case GAP_MSG_LE_CONN_PARAM_UPDATE: {
		ble_wifimate_device_app_handle_conn_param_update_evt(gap_msg.msg_data.gap_conn_param_update.conn_id,
										 gap_msg.msg_data.gap_conn_param_update.status,
										 gap_msg.msg_data.gap_conn_param_update.cause);
	}
	break;

	case GAP_MSG_LE_AUTHEN_STATE_CHANGE: {
		ble_wifimate_device_app_handle_authen_state_evt(gap_msg.msg_data.gap_authen_state.conn_id,
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
		APP_PRINT_ERROR2("%s: unknown subtype %d", __func__, p_gap_msg->subtype);
		break;
	}
}

static void ble_wifimate_timeout_handler(void *arg)
{
	(void)arg;
	ble_wifimate_device_app_send_msg(BLE_WIFIMATE_DEVICE_ADV_STOP, NULL);
	printf("[APP] ble wifimate timeout, stop adv...\r\n");
}

/**
 * @brief    Init timer.
 * @param[in] timeout timer timeout value, uint: second(s)
 * @return   bool
 */
bool ble_wifimate_device_timer_init(uint16_t timeout)
{
	os_timer_create(&s_ble_wifimate_timer_hdl, "ble_wifimate_start_timer", NULL,
					timeout * 1000, false, ble_wifimate_timeout_handler);
	if (s_ble_wifimate_timer_hdl == NULL) {
		printf("[APP] ble wifimate create timer failed!\r\n");
		return false;
	}
	return true;
}

void ble_wifimate_device_timer_deinit(void)
{
	if (s_ble_wifimate_timer_hdl)
	{
		os_timer_delete(&s_ble_wifimate_timer_hdl);
		s_ble_wifimate_timer_hdl = NULL;
	}
}

#endif /* CONFIG_BLE_WIFIMATE_DEVICE */