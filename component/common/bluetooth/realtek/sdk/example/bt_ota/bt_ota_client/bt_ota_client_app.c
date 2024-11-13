/**
*****************************************************************************************
*     Copyright(c) 2017, Realtek Semiconductor Corporation. All rights reserved.
*****************************************************************************************
   * @file      central_client_app.c
   * @brief     This file handles BLE central application routines.
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
#if defined(CONFIG_BT_OTA_CLIENT) && CONFIG_BT_OTA_CLIENT
#include <stdio.h>
#include <string.h>
#include <app_msg.h>
#include <trace_app.h>
#include <gap_scan.h>
#include <gap.h>
#include <gap_msg.h>
#include <gap_bond_le.h>
#include <gap_conn_le.h>
#include <gap_le_types.h>
#include "log_service.h"
#include <bt_ota_client_app.h>
#include <bt_ota_client_app_flags.h>
#include <bt_ota_client_link_mgr.h>
#include <bt_ota_gatt_client.h>


#define BLE_OTA_CLIENT_SUPPORT_PAIRING    1 //if BLE_OTA_SERVER_SUPPORT_PAIRING set 1, the BLE_OTA_CLIENT_SUPPORT_PAIRING should set 1.

/** @defgroup  CENTRAL_CLIENT_APP Central Client Application
    * @brief This file handles BLE central client application routines.
    * @{
    */
/*============================================================================*
 *                              Variables
 *============================================================================*/
/** @addtogroup  CENTRAL_CLIIENT_CALLBACK
    * @{
    */
T_CLIENT_ID   bt_ota_client_id;         /**< General Common Services client client id*/
/** @} */ /* End of group CENTRAL_CLIIENT_CALLBACK */

/** @defgroup  CENTRAL_CLIENT_GAP_MSG GAP Message Handler
    * @brief Handle GAP Message
    * @{
    */
T_GAP_DEV_STATE bt_ota_client_gap_dev_state = {0, 0, 0, 0, 0};                /**< GAP device state */
uint8_t bt_ota_client_app_max_links = 0;

char remote_device_name[14] = {'B', 'T', '_', 'O', 'T', 'A', '_', 'S', 'E', 'R', 'V', 'E', 'R', '\0'};

/*============================================================================*
 *                              Functions
 *============================================================================*/
void bt_ota_client_app_handle_gap_msg(T_IO_MSG  *p_gap_msg);

T_GAP_CAUSE bt_ota_client_connect(uint8_t *addr)
{
	uint8_t DestAddr[6] = {0};
	uint8_t DestAddrType = GAP_REMOTE_ADDR_LE_PUBLIC;
	T_GAP_LOCAL_ADDR_TYPE local_addr_type = GAP_LOCAL_ADDR_LE_PUBLIC;
	T_GAP_LE_CONN_REQ_PARAM conn_req_param;

	conn_req_param.scan_interval = 0xA0;	//100ms
	conn_req_param.scan_window = 0x80;		//80ms
	conn_req_param.conn_interval_min = 0x60;	//120ms
	conn_req_param.conn_interval_max = 0x60;	//120ms
	conn_req_param.conn_latency = 0;
	conn_req_param.supv_tout = 1000;
	conn_req_param.ce_len_min = 2 * (conn_req_param.conn_interval_min - 1);
	conn_req_param.ce_len_max = 2 * (conn_req_param.conn_interval_max - 1);
	le_set_conn_param(GAP_CONN_PARAM_1M, &conn_req_param);

	memcpy(DestAddr, addr, 6);
	printf("cmd_con, addr: 0x%02X:0x%02X:0x%02X:0x%02X:0x%02X:0x%02X\r\n",
			DestAddr[5], DestAddr[4], DestAddr[3], DestAddr[2], DestAddr[1], DestAddr[0]);

	T_GAP_CAUSE cause = le_connect(0, DestAddr, (T_GAP_REMOTE_ADDR_TYPE)DestAddrType, local_addr_type, 1000);

	return cause;
}

static u8 ctoi(char c)
{
	if((c >= 'A') && (c <= 'F')) {
		return (c - 'A' + 0x0A);
	}

	if((c >= 'a') && (c <= 'f')) {
		return (c - 'a' + 0x0A);
	}

	if((c >= '0') && (c <= '9')) {
		return (c - '0' + 0x00);
	}

	return 0xFF;
}

static u8 hex_str_to_bd_addr(u32 str_len, s8 *str, u8 *num_arr)
{
	num_arr += str_len/2 -1;
	u32 n = 0;
	u8 num = 0;

	if (str_len < 2) {
		return FALSE;
	}
	while (n < str_len) {
		if ((num = ctoi(str[n++])) == 0xFF) {
			return FALSE;
		}
		*num_arr = num << 4;
		if ((num = ctoi(str[n++])) == 0xFF) {
			return FALSE;
		}
		*num_arr |= num;
		num_arr--;
	}
	return TRUE;
}

int bt_ota_client_parse_param(char *buf, char **argv)
{
	int argc = 1;
	char str_buf[256];
	memset(str_buf, 0, 256);
	int str_count = 0;
	int buf_cnt = 0;
	static char temp_buf[256];
	char *buf_pos = temp_buf;
	memset(temp_buf, 0, sizeof(temp_buf));

	if(buf == NULL)
		goto exit;
	strncpy(temp_buf, buf, sizeof(temp_buf));

	while((argc < MAX_ARGC) && (*buf_pos != '\0')) {
		while((*buf_pos == ',') || (*buf_pos == '[') || (*buf_pos == ']')){
			if((*buf_pos == ',') && (*(buf_pos+1) == ',')){
				argv[argc] = NULL;
				argc++;
			}
			*buf_pos = '\0';
			buf_pos++;
		}

		if(*buf_pos == '\0')
			break;
		else if(*buf_pos == '"'){
			memset(str_buf,'\0',256);
			str_count = 0;
			buf_cnt = 0;
			*buf_pos = '\0';
			buf_pos ++;
			if(*buf_pos == '\0')
			break;
			argv[argc] = buf_pos;
			while((*buf_pos != '"')&&(*buf_pos != '\0')){
				if(*buf_pos == '\\'){
				buf_pos ++;
					buf_cnt++;
				}
				str_buf[str_count] = *buf_pos;
				str_count++;
				buf_cnt++;
				buf_pos ++;
			}
			*buf_pos = '\0';
			memcpy(buf_pos-buf_cnt,str_buf,buf_cnt);
		}
		else{
			argv[argc] = buf_pos;
		}
		argc++;
		buf_pos++;

		while( (*buf_pos != ',')&&(*buf_pos != '\0')&&(*buf_pos != '[')&&(*buf_pos != ']') )
			buf_pos++;
	}
exit:
	return argc;
}

void bt_ota_client_app_handle_qdecode_cmd(uint16_t subtype, void *arg)
{
	int argc = 0;
	char *argv[MAX_ARGC] = {0};

	if (arg) {
		argc = bt_ota_client_parse_param(arg, argv);
	}

	switch (subtype) {
		case OTA_SCAN_START: {
			le_scan_start();
		}
		break;

		case OTA_SCAN_STOP: {
			le_scan_stop();
		}
		break;

		case OTA_CONNECT: {
			uint8_t addr[6] = {0};
			hex_str_to_bd_addr(strlen(argv[3]), (s8 *)argv[3], (u8*)addr);
			bt_ota_client_connect(addr);
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
void bt_ota_client_app_handle_io_msg(T_IO_MSG io_msg)
{
	uint16_t msg_type = io_msg.type;

	switch (msg_type) {
	case IO_MSG_TYPE_BT_STATUS: {
		bt_ota_client_app_handle_gap_msg(&io_msg);
	}
	break;

	case IO_MSG_TYPE_QDECODE: {
		uint16_t subtype = io_msg.subtype;
		void *arg = io_msg.u.buf;
		bt_ota_client_app_handle_qdecode_cmd(subtype, arg);
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
void bt_ota_client_app_handle_dev_state_evt(T_GAP_DEV_STATE new_state, uint16_t cause)
{
	APP_PRINT_INFO3("bt_ota_client_app_handle_dev_state_evt: init state  %d, scan state %d, cause 0x%x",
					new_state.gap_init_state, new_state.gap_scan_state, cause);

    if (bt_ota_client_gap_dev_state.gap_init_state != new_state.gap_init_state)
    {
		if (new_state.gap_init_state == GAP_INIT_STATE_STACK_READY)
		{
			uint8_t bt_addr[6];
			APP_PRINT_INFO0("GAP stack ready");
			/*stack ready*/
			gap_get_param(GAP_PARAM_BD_ADDR, bt_addr);
			printf("local bd addr: 0x%02x:%02x:%02x:%02x:%02x:%02x\r\n",
				   bt_addr[5], bt_addr[4], bt_addr[3], bt_addr[2], bt_addr[1], bt_addr[0]);
		}
	}

	if (bt_ota_client_gap_dev_state.gap_scan_state != new_state.gap_scan_state)
	{
		if (new_state.gap_scan_state == GAP_SCAN_STATE_IDLE)
		{
			APP_PRINT_INFO0("GAP scan stop");
			printf("GAP scan stop\r\n");
		}
		else if (new_state.gap_scan_state == GAP_SCAN_STATE_SCANNING)
		{
			APP_PRINT_INFO0("GAP scan start");
			printf("GAP scan start\r\n");
		}
	}

	bt_ota_client_gap_dev_state = new_state;
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
void bt_ota_client_app_handle_conn_state_evt(uint8_t conn_id, T_GAP_CONN_STATE new_state, uint16_t disc_cause)
{
	if (conn_id >= BT_OTA_CLIENT_APP_MAX_LINKS)
	{
		return;
	}

	APP_PRINT_INFO4("bt_ota_client_app_handle_conn_state_evt: conn_id %d, conn_state(%d -> %d), disc_cause 0x%x",
					conn_id, bt_ota_client_app_link_table[conn_id].conn_state, new_state, disc_cause);

	bt_ota_client_app_link_table[conn_id].conn_state = new_state;
	switch (new_state)
	{
	case GAP_CONN_STATE_DISCONNECTED:
		{
			if ((disc_cause != (HCI_ERR | HCI_ERR_REMOTE_USER_TERMINATE))
				&& (disc_cause != (HCI_ERR | HCI_ERR_LOCAL_HOST_TERMINATE)))
			{
				APP_PRINT_ERROR2("bt_ota_client_app_handle_conn_state_evt: connection lost, conn_id %d, cause 0x%x", conn_id,
								 disc_cause);
			}

			printf("Disconnect conn_id %d, cause 0x%x\r\n", conn_id, disc_cause);
			if (bt_ota_client_app_link_table[conn_id].role == GAP_LINK_ROLE_MASTER) {
				bt_ota_client_app_max_links--;
			}
			memset(&bt_ota_client_app_link_table[conn_id], 0, sizeof(T_APP_LINK));
		}
		break;

	case GAP_CONN_STATE_CONNECTED:
		{
			le_get_conn_addr(conn_id, bt_ota_client_app_link_table[conn_id].bd_addr,
							 (void *)&bt_ota_client_app_link_table[conn_id].bd_type);
			printf("Connected success conn_id %d\r\n", conn_id);
			bt_ota_client_app_link_table[conn_id].role = GAP_LINK_ROLE_MASTER;
			bt_ota_client_app_max_links++;
#if F_BT_LE_5_0_SET_PHY_SUPPORT
			{
			uint8_t tx_phy;
			uint8_t rx_phy;
			le_get_conn_param(GAP_PARAM_CONN_RX_PHY_TYPE, &rx_phy, conn_id);
			le_get_conn_param(GAP_PARAM_CONN_TX_PHY_TYPE, &tx_phy, conn_id);
			APP_PRINT_INFO2("GAP_CONN_STATE_CONNECTED: tx_phy %d, rx_phy %d", tx_phy, rx_phy);
			printf("GAP_CONN_STATE_CONNECTED: tx_phy %d, rx_phy %d\r\n", tx_phy, rx_phy);
			}
#endif

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
void bt_ota_client_app_handle_authen_state_evt(uint8_t conn_id, uint8_t new_state, uint16_t cause)
{
	APP_PRINT_INFO2("bt_ota_client_app_handle_authen_state_evt:conn_id %d, cause 0x%x", conn_id, cause);

	switch (new_state)
	{
	case GAP_AUTHEN_STATE_STARTED:
		{
			APP_PRINT_INFO0("bt_ota_client_app_handle_authen_state_evt: GAP_AUTHEN_STATE_STARTED");
		}
		break;

	case GAP_AUTHEN_STATE_COMPLETE:
		{
			if (cause == GAP_SUCCESS)
			{
				printf("Pair success\r\n");
				APP_PRINT_INFO0("bt_ota_client_app_handle_authen_state_evt: GAP_AUTHEN_STATE_COMPLETE pair success");

#if BLE_OTA_CLIENT_SUPPORT_PAIRING
				printf("\r\nStart discover services\r\n");
				bt_ota_client_start_discovery(conn_id);
#endif
			}
			else
			{
				printf("Pair failed: cause 0x%x\r\n", cause);
				APP_PRINT_INFO0("bt_ota_client_app_handle_authen_state_evt: GAP_AUTHEN_STATE_COMPLETE pair failed");
			}
		}
		break;

	default:
		{
			APP_PRINT_ERROR1("bt_ota_client_app_handle_authen_state_evt: unknown newstate %d", new_state);
		}
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
void bt_ota_client_app_handle_conn_mtu_info_evt(uint8_t conn_id, uint16_t mtu_size)
{
	APP_PRINT_INFO2("bt_ota_client_app_handle_conn_mtu_info_evt: conn_id %d, mtu_size %d", conn_id, mtu_size);
	printf("bt_ota_client_app_handle_conn_mtu_info_evt: conn_id %d, mtu_size %d\r\n", conn_id, mtu_size);
}

/**
 * @brief    Handle msg GAP_MSG_LE_CONN_PARAM_UPDATE
 * @note     All the connection parameter update change  events are pre-handled in this function.
 * @param[in] conn_id Connection ID
 * @param[in] status  New update state
 * @param[in] cause Use this cause when status is GAP_CONN_PARAM_UPDATE_STATUS_FAIL
 * @return   void
 */
void bt_ota_client_app_handle_conn_param_update_evt(uint8_t conn_id, uint8_t status, uint16_t cause)
{
	switch (status)
	{
	case GAP_CONN_PARAM_UPDATE_STATUS_SUCCESS:
		{
			uint16_t conn_interval;
			uint16_t conn_slave_latency;
			uint16_t conn_supervision_timeout;

			le_get_conn_param(GAP_PARAM_CONN_INTERVAL, &conn_interval, conn_id);
			le_get_conn_param(GAP_PARAM_CONN_LATENCY, &conn_slave_latency, conn_id);
			le_get_conn_param(GAP_PARAM_CONN_TIMEOUT, &conn_supervision_timeout, conn_id);
			APP_PRINT_INFO4("bt_ota_client_app_handle_conn_param_update_evt update success:conn_id %d, conn_interval 0x%x, conn_slave_latency 0x%x, conn_supervision_timeout 0x%x",
							conn_id, conn_interval, conn_slave_latency, conn_supervision_timeout);
			printf("bt_ota_client_app_handle_conn_param_update_evt success: conn_id %d, conn_interval 0x%x, conn_slave_latency 0x%x, conn_supervision_timeout 0x%x \r\n",
				   conn_id, conn_interval, conn_slave_latency, conn_supervision_timeout);

#if !BLE_OTA_CLIENT_SUPPORT_PAIRING
		printf("\r\nStart discover services\r\n");
		bt_ota_client_start_discovery(conn_id);
#endif
        }
        break;

	case GAP_CONN_PARAM_UPDATE_STATUS_FAIL:
		{
			APP_PRINT_ERROR2("bt_ota_client_app_handle_conn_param_update_evt update failed: conn_id %d, cause 0x%x",
							 conn_id, cause);
			printf("bt_ota_client_app_handle_conn_param_update_evt update failed: conn_id %d, cause 0x%x\r\n",
				   conn_id, cause);

		}
		break;

	case GAP_CONN_PARAM_UPDATE_STATUS_PENDING:
		{
			APP_PRINT_INFO1("bt_ota_client_app_handle_conn_param_update_evt update pending: conn_id %d", conn_id);
			printf("bt_ota_client_app_handle_conn_param_update_evt update pending: conn_id %d\r\n", conn_id);

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
void bt_ota_client_app_handle_gap_msg(T_IO_MSG *p_gap_msg)
{
	T_LE_GAP_MSG gap_msg;
	uint8_t conn_id;
	memcpy(&gap_msg, &p_gap_msg->u.param, sizeof(p_gap_msg->u.param));

	APP_PRINT_TRACE1("bt_ota_client_app_handle_gap_msg: subtype %d", p_gap_msg->subtype);
	switch (p_gap_msg->subtype)
	{
	case GAP_MSG_LE_DEV_STATE_CHANGE:
		{
			bt_ota_client_app_handle_dev_state_evt(gap_msg.msg_data.gap_dev_state_change.new_state,
												   gap_msg.msg_data.gap_dev_state_change.cause);
		}
		break;

	case GAP_MSG_LE_CONN_STATE_CHANGE:
		{
			bt_ota_client_app_handle_conn_state_evt(gap_msg.msg_data.gap_conn_state_change.conn_id,
													(T_GAP_CONN_STATE)gap_msg.msg_data.gap_conn_state_change.new_state,
													gap_msg.msg_data.gap_conn_state_change.disc_cause);
		}
		break;

	case GAP_MSG_LE_CONN_MTU_INFO:
		{
			bt_ota_client_app_handle_conn_mtu_info_evt(gap_msg.msg_data.gap_conn_mtu_info.conn_id,
													   gap_msg.msg_data.gap_conn_mtu_info.mtu_size);
		}
		break;

	case GAP_MSG_LE_CONN_PARAM_UPDATE:
		{
			bt_ota_client_app_handle_conn_param_update_evt(gap_msg.msg_data.gap_conn_param_update.conn_id,
														   gap_msg.msg_data.gap_conn_param_update.status,
														   gap_msg.msg_data.gap_conn_param_update.cause);
		}
		break;

	case GAP_MSG_LE_AUTHEN_STATE_CHANGE:
		{
			bt_ota_client_app_handle_authen_state_evt(gap_msg.msg_data.gap_authen_state.conn_id,
													  gap_msg.msg_data.gap_authen_state.new_state,
													  gap_msg.msg_data.gap_authen_state.status);
		}
		break;

	case GAP_MSG_LE_BOND_JUST_WORK:
		{
			conn_id = gap_msg.msg_data.gap_bond_just_work_conf.conn_id;
			le_bond_just_work_confirm(conn_id, GAP_CFM_CAUSE_ACCEPT);
			APP_PRINT_INFO0("GAP_MSG_LE_BOND_JUST_WORK");
		}
		break;

	case GAP_MSG_LE_BOND_PASSKEY_DISPLAY:
		{
			uint32_t display_value = 0;
			conn_id = gap_msg.msg_data.gap_bond_passkey_display.conn_id;
			le_bond_get_display_key(conn_id, &display_value);
			APP_PRINT_INFO2("GAP_MSG_LE_BOND_PASSKEY_DISPLAY: conn_id %d, passkey %d",
							conn_id, display_value);
			le_bond_passkey_display_confirm(conn_id, GAP_CFM_CAUSE_ACCEPT);
			printf("GAP_MSG_LE_BOND_PASSKEY_DISPLAY: conn_id %d, passkey %06d\r\n",
				   conn_id, display_value);
		}
		break;

	case GAP_MSG_LE_BOND_USER_CONFIRMATION:
		{
			uint32_t display_value = 0;
			conn_id = gap_msg.msg_data.gap_bond_user_conf.conn_id;
			le_bond_get_display_key(conn_id, &display_value);
			APP_PRINT_INFO2("GAP_MSG_LE_BOND_USER_CONFIRMATION: conn_id %d, passkey %d",
							conn_id, display_value);
			printf("GAP_MSG_LE_BOND_USER_CONFIRMATION: conn_id %d, passkey %06d\r\n",
				   conn_id, display_value);
			//le_bond_user_confirm(conn_id, GAP_CFM_CAUSE_ACCEPT);
		}
		break;

	case GAP_MSG_LE_BOND_PASSKEY_INPUT:
		{
			//uint32_t passkey = 888888;
			conn_id = gap_msg.msg_data.gap_bond_passkey_input.conn_id;
			APP_PRINT_INFO1("GAP_MSG_LE_BOND_PASSKEY_INPUT: conn_id %d", conn_id);
			printf("GAP_MSG_LE_BOND_PASSKEY_INPUT: conn_id %d\r\n", conn_id);
			//le_bond_passkey_input_confirm(conn_id, passkey, GAP_CFM_CAUSE_ACCEPT);
		}
		break;

	case GAP_MSG_LE_BOND_OOB_INPUT:
		{
#if F_BT_LE_SMP_OOB_SUPPORT
			uint8_t oob_data[GAP_OOB_LEN] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
#endif
			conn_id = gap_msg.msg_data.gap_bond_oob_input.conn_id;
			APP_PRINT_INFO1("GAP_MSG_LE_BOND_OOB_INPUT: conn_id %d", conn_id);
#if F_BT_LE_SMP_OOB_SUPPORT
			le_bond_set_param(GAP_PARAM_BOND_OOB_DATA, GAP_OOB_LEN, oob_data);
			le_bond_oob_input_confirm(conn_id, GAP_CFM_CAUSE_ACCEPT);
#endif
		}
		break;

	default:
		APP_PRINT_ERROR1("bt_ota_client_app_handle_gap_msg: unknown subtype %d", p_gap_msg->subtype);
		break;
	}
}
/** @} */ /* End of group CENTRAL_CLIENT_GAP_MSG */

bool bt_ota_filter_scan_info_by_name(T_LE_SCAN_INFO *scan_info, uint8_t *p_remote_name)
{
	uint8_t buffer[32];
	uint8_t pos = 0;

	while (pos < scan_info->data_len) {
		/* Length of the AD structure. */
		uint8_t length = scan_info->data[pos++];
		uint8_t type;

		if ((length > 0x01) && ((pos + length) <= 31)) {
			/* Copy the AD Data to buffer. */
			memcpy(buffer, scan_info->data + pos + 1, length - 1);
			/* AD Type, one octet. */
			type = scan_info->data[pos];

			switch(type) {
			case GAP_ADTYPE_LOCAL_NAME_SHORT:
			case GAP_ADTYPE_LOCAL_NAME_COMPLETE: {
				buffer[length - 1] = '\0';
				if (memcmp(buffer, p_remote_name, length - 1) == 0) {
					//APP_PRINT_INFO1("GAP_ADTYPE_LOCAL_NAME_XXX: %s", TRACE_STRING(buffer));
					//printf("GAP_ADTYPE_LOCAL_NAME_XXX: %s.\r\n", buffer);
					return true;
				}
			}
			break;

			default:
				break;
			}
		} else {
			return false;
		}

		pos += length;
	}

	return false;
}


/** @defgroup  CENTRAL_CLIENT_GAP_CALLBACK GAP Callback Event Handler
    * @brief Handle GAP callback event
    * @{
    */
/**
  * @brief Used to parse advertising data and scan response data
  * @param[in] scan_info point to scan information data.
  * @retval void
  */
void bt_ota_client_app_parse_scan_info(T_LE_SCAN_INFO *scan_info)
{
	uint8_t buffer[32];
	uint8_t pos = 0;

	while (pos < scan_info->data_len)
	{
		/* Length of the AD structure. */
		uint8_t length = scan_info->data[pos++];
		uint8_t type;

		if ((length > 0x01) && ((pos + length) <= 31))
		{
			/* Copy the AD Data to buffer. */
			memcpy(buffer, scan_info->data + pos + 1, length - 1);
			/* AD Type, one octet. */
			type = scan_info->data[pos];

			APP_PRINT_TRACE2("ble_central_app_parse_scan_info: AD Structure Info: AD type 0x%x, AD Data Length %d", type,
							 length - 1);
//			printf("ble_central_app_parse_scan_info: AD Structure Info: AD type 0x%x, AD Data Length %d\r\n", type,
//							 length - 1);


			switch (type)
			{
			case GAP_ADTYPE_FLAGS:
				{
					/* (flags & 0x01) -- LE Limited Discoverable Mode */
					/* (flags & 0x02) -- LE General Discoverable Mode */
					/* (flags & 0x04) -- BR/EDR Not Supported */
					/* (flags & 0x08) -- Simultaneous LE and BR/EDR to Same Device Capable (Controller) */
					/* (flags & 0x10) -- Simultaneous LE and BR/EDR to Same Device Capable (Host) */
					uint8_t flags = scan_info->data[pos + 1];
					APP_PRINT_INFO1("GAP_ADTYPE_FLAGS: 0x%x", flags);
					printf("GAP_ADTYPE_FLAGS: 0x%x\r\n", flags);

				}
				break;

			case GAP_ADTYPE_16BIT_MORE:
			case GAP_ADTYPE_16BIT_COMPLETE:
			case GAP_ADTYPE_SERVICES_LIST_16BIT:
				{
					uint16_t *p_uuid = (uint16_t *)(buffer);
					uint8_t i = length - 1;

					while (i >= 2)
					{
						APP_PRINT_INFO1("GAP_ADTYPE_16BIT_XXX: 0x%x", *p_uuid);
						printf("GAP_ADTYPE_16BIT_XXX: 0x%x\r\n", *p_uuid);
						p_uuid ++;
						i -= 2;
					}
				}
				break;

			case GAP_ADTYPE_32BIT_MORE:
			case GAP_ADTYPE_32BIT_COMPLETE:
				{
					uint32_t *p_uuid = (uint32_t *)(buffer);
					uint8_t    i	 = length - 1;

					while (i >= 4)
					{
						APP_PRINT_INFO1("GAP_ADTYPE_32BIT_XXX: 0x%x", *p_uuid);
						printf("GAP_ADTYPE_32BIT_XXX: 0x%x\r\n", (unsigned int)*p_uuid);
						p_uuid ++;

						i -= 4;
					}
				}
				break;

			case GAP_ADTYPE_128BIT_MORE:
			case GAP_ADTYPE_128BIT_COMPLETE:
			case GAP_ADTYPE_SERVICES_LIST_128BIT:
				{
					uint32_t *p_uuid = (uint32_t *)(buffer);
					APP_PRINT_INFO4("GAP_ADTYPE_128BIT_XXX: 0x%8.8x%8.8x%8.8x%8.8x",
									p_uuid[3], p_uuid[2], p_uuid[1], p_uuid[0]);
					printf("GAP_ADTYPE_128BIT_XXX: 0x%8.8x%8.8x%8.8x%8.8x\r\n",
									(unsigned int)p_uuid[3], (unsigned int)p_uuid[2], (unsigned int)p_uuid[1], (unsigned int)p_uuid[0]);

				}
				break;

			case GAP_ADTYPE_LOCAL_NAME_SHORT:
			case GAP_ADTYPE_LOCAL_NAME_COMPLETE:
				{
					buffer[length - 1] = '\0';
					APP_PRINT_INFO1("GAP_ADTYPE_LOCAL_NAME_XXX: %s", TRACE_STRING(buffer));
					printf("GAP_ADTYPE_LOCAL_NAME_XXX: %s\r\n", buffer);

				}
				break;

			case GAP_ADTYPE_POWER_LEVEL:
				{
					APP_PRINT_INFO1("GAP_ADTYPE_POWER_LEVEL: 0x%x", scan_info->data[pos + 1]);
					printf("GAP_ADTYPE_POWER_LEVEL: 0x%x\r\n", scan_info->data[pos + 1]);

				}
				break;

			case GAP_ADTYPE_SLAVE_CONN_INTERVAL_RANGE:
				{
					uint16_t *p_min = (uint16_t *)(buffer);
					uint16_t *p_max = p_min + 1;
					APP_PRINT_INFO2("GAP_ADTYPE_SLAVE_CONN_INTERVAL_RANGE: 0x%x - 0x%x", *p_min,
									*p_max);
					printf("GAP_ADTYPE_SLAVE_CONN_INTERVAL_RANGE: 0x%x - 0x%x\r\n", *p_min,
									*p_max);

				}
				break;

			case GAP_ADTYPE_SERVICE_DATA:
				{
					uint16_t *p_uuid = (uint16_t *)(buffer);
					uint8_t data_len = length - 3;

					APP_PRINT_INFO3("GAP_ADTYPE_SERVICE_DATA: UUID 0x%x, len %d, data %b", *p_uuid,
									data_len, TRACE_BINARY(data_len, &buffer[2]));
					printf("GAP_ADTYPE_SERVICE_DATA: UUID 0x%x, len %d\r\n", *p_uuid,
									data_len);

				}
				break;
			case GAP_ADTYPE_APPEARANCE:
				{
					uint16_t *p_appearance = (uint16_t *)(buffer);
					APP_PRINT_INFO1("GAP_ADTYPE_APPEARANCE: %d", *p_appearance);
					printf("GAP_ADTYPE_APPEARANCE: %d\r\n", *p_appearance);

				}
				break;

			case GAP_ADTYPE_MANUFACTURER_SPECIFIC:
				{
					uint8_t data_len = length - 3;
					uint16_t *p_company_id = (uint16_t *)(buffer);
					APP_PRINT_INFO3("GAP_ADTYPE_MANUFACTURER_SPECIFIC: company_id 0x%x, len %d, data %b",
									*p_company_id, data_len, TRACE_BINARY(data_len, &buffer[2]));
					printf("GAP_ADTYPE_MANUFACTURER_SPECIFIC: company_id 0x%x, len %d\r\n",
									*p_company_id, data_len);

				}
				break;

			default:
				{
					uint8_t i = 0;

					for (i = 0; i < (length - 1); i++)
					{
						APP_PRINT_INFO1("  AD Data: Unhandled Data = 0x%x", scan_info->data[pos + 1 + i]);
//						printf("  AD Data: Unhandled Data = 0x%x\r\n", scan_info->data[pos + 1 + i]);

					}
				}
				break;
			}
		}

		if (pos + length > 0xFF)	//Avoid pos + length overflow uint8_t
			pos = 0xFF;
		else
			pos += length;
	}
}

/**
  * @brief Callback for gap le to notify app
  * @param[in] cb_type callback msy type @ref GAP_LE_MSG_Types.
  * @param[in] p_cb_data point to callback data @ref T_LE_CB_DATA.
  * @retval result @ref T_APP_RESULT
  */
T_APP_RESULT bt_ota_client_app_gap_callback(uint8_t cb_type, void *p_cb_data)
{
	T_APP_RESULT result = APP_RESULT_SUCCESS;
	T_LE_CB_DATA *p_data = (T_LE_CB_DATA *)p_cb_data;
	char adv_type[20];
	char remote_addr_type[10];

	switch (cb_type)
	{
	case GAP_MSG_LE_SCAN_INFO:
		APP_PRINT_INFO5("GAP_MSG_LE_SCAN_INFO:adv_type 0x%x, bd_addr %s, remote_addr_type %d, rssi %d, data_len %d",
						p_data->p_le_scan_info->adv_type,
						TRACE_BDADDR(p_data->p_le_scan_info->bd_addr),
						p_data->p_le_scan_info->remote_addr_type,
						p_data->p_le_scan_info->rssi,
						p_data->p_le_scan_info->data_len);
		/* If you want to parse the scan info, please reference function ble_central_app_parse_scan_info. */
		sprintf(adv_type,"%s",(p_data->p_le_scan_info->adv_type ==GAP_ADV_EVT_TYPE_UNDIRECTED)? "CON_UNDIRECT":
							  (p_data->p_le_scan_info->adv_type ==GAP_ADV_EVT_TYPE_DIRECTED)? "CON_DIRECT":
							  (p_data->p_le_scan_info->adv_type ==GAP_ADV_EVT_TYPE_SCANNABLE)? "SCANABLE_UNDIRCT":
							  (p_data->p_le_scan_info->adv_type ==GAP_ADV_EVT_TYPE_NON_CONNECTABLE)? "NON_CONNECTABLE":
							  (p_data->p_le_scan_info->adv_type ==GAP_ADV_EVT_TYPE_SCAN_RSP)? "SCAN_RSP":"unknown");
		sprintf(remote_addr_type,"%s",(p_data->p_le_scan_info->remote_addr_type == GAP_REMOTE_ADDR_LE_PUBLIC)? "public":
							   (p_data->p_le_scan_info->remote_addr_type == GAP_REMOTE_ADDR_LE_RANDOM)? "random":"unknown");

		if (bt_ota_filter_scan_info_by_name(p_data->p_le_scan_info, (uint8_t *)remote_device_name)) {
			uint8_t *addr = p_data->p_le_scan_info->bd_addr;
			printf("Found device %s: [%02x:%02x:%02x:%02x:%02x:%02x]\r\n", remote_device_name,
				   addr[5], addr[4], addr[3], addr[2], addr[1], addr[0]);
		}

		break;

	case GAP_MSG_LE_CONN_UPDATE_IND:
		APP_PRINT_INFO5("GAP_MSG_LE_CONN_UPDATE_IND: conn_id %d, conn_interval_max 0x%x, conn_interval_min 0x%x, conn_latency 0x%x,supervision_timeout 0x%x",
						p_data->p_le_conn_update_ind->conn_id,
						p_data->p_le_conn_update_ind->conn_interval_max,
						p_data->p_le_conn_update_ind->conn_interval_min,
						p_data->p_le_conn_update_ind->conn_latency,
						p_data->p_le_conn_update_ind->supervision_timeout);
		printf("GAP_MSG_LE_CONN_UPDATE_IND: conn_id %d, conn_interval_max 0x%x, conn_interval_min 0x%x, conn_latency 0x%x,supervision_timeout 0x%x\r\n",
						p_data->p_le_conn_update_ind->conn_id,
						p_data->p_le_conn_update_ind->conn_interval_max,
						p_data->p_le_conn_update_ind->conn_interval_min,
						p_data->p_le_conn_update_ind->conn_latency,
						p_data->p_le_conn_update_ind->supervision_timeout);
		/* if reject the proposed connection parameter from peer device, use APP_RESULT_REJECT. */
		result = APP_RESULT_ACCEPT;
		break;

#if F_BT_LE_5_0_SET_PHY_SUPPORT
	case GAP_MSG_LE_PHY_UPDATE_INFO:
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
		break;

	case GAP_MSG_LE_REMOTE_FEATS_INFO:
		{
			uint8_t  remote_feats[8];
			APP_PRINT_INFO3("GAP_MSG_LE_REMOTE_FEATS_INFO: conn id %d, cause 0x%x, remote_feats %b",
							p_data->p_le_remote_feats_info->conn_id,
							p_data->p_le_remote_feats_info->cause,
							TRACE_BINARY(8, p_data->p_le_remote_feats_info->remote_feats));
			if (p_data->p_le_remote_feats_info->cause == GAP_SUCCESS)
			{
				memcpy(remote_feats, p_data->p_le_remote_feats_info->remote_feats, 8);
				if (remote_feats[LE_SUPPORT_FEATURES_MASK_ARRAY_INDEX1] & LE_SUPPORT_FEATURES_LE_2M_MASK_BIT)
				{
					APP_PRINT_INFO0("GAP_MSG_LE_REMOTE_FEATS_INFO: support 2M");
					printf("GAP_MSG_LE_REMOTE_FEATS_INFO: support 2M\r\n");
				}
				if (remote_feats[LE_SUPPORT_FEATURES_MASK_ARRAY_INDEX1] & LE_SUPPORT_FEATURES_LE_CODED_PHY_MASK_BIT)
				{
					APP_PRINT_INFO0("GAP_MSG_LE_REMOTE_FEATS_INFO: support CODED");
					printf("GAP_MSG_LE_REMOTE_FEATS_INFO: support CODED\r\n");
				}
			}
		}
		break;
#endif

	case GAP_MSG_LE_MODIFY_WHITE_LIST:
		APP_PRINT_INFO2("GAP_MSG_LE_MODIFY_WHITE_LIST: operation  0x%x, cause 0x%x",
					p_data->p_le_modify_white_list_rsp->operation,
					p_data->p_le_modify_white_list_rsp->cause);
		printf("GAP_MSG_LE_MODIFY_WHITE_LIST: operation  0x%x, cause 0x%x\r\n",
					p_data->p_le_modify_white_list_rsp->operation,
					p_data->p_le_modify_white_list_rsp->cause);
		break;

	default:
		APP_PRINT_ERROR1("bt_ota_client_app_gap_callback: unhandled cb_type 0x%x", cb_type);
		break;
	}
	return result;
}
/** @} */ /* End of group CENTRAL_CLIENT_GAP_CALLBACK */

void bt_ota_client_handle_indicate_msg(uint8_t conn_id, void *p_data)
{
	T_OTA_NOTIF_IND_DATA *indi_data = (T_OTA_NOTIF_IND_DATA *)p_data;
	uint8_t type = indi_data->type;
	uint8_t *p_value = indi_data->data.p_value;
	uint8_t opcode = p_value[0];

	printf("%s: received indicate op=0x%02x\r\n", __func__, opcode);

	if (type == OTA_INDI_RECEIVE_OTA_CTRL_INDICATE) {
		switch (opcode) {
		case OTA_IMG_HDR_GET: {
			uint8_t result = p_value[1];
			if (result == 0x01) { //remote request to get ota header
				bt_ota_client_send_ota_header(conn_id);
			}
		}
		break;

		case OTA_HDR_CHECK: {
			uint8_t check_result = p_value[1];
			uint32_t offset = 0;
			uint8_t index = 0;
			LE_ARRAY_TO_UINT32(offset, &p_value[2]);
			index = p_value[6];

			if (check_result == OTA_HDR_OK) {
				printf("Start push OTA image: img_idx %d, offset %d\r\n", index, offset);
				ota_tx_info.index = index;
				bt_ota_client_set_image_param(ota_tx_info.index);
				ota_tx_info.cur_offset = offset;

				bt_ota_push_image_status = 0;
				bt_ota_client_push_image(conn_id);
			} else {
				printf("%s: OTA header check error 0x%02x\r\n", __func__, check_result);
			}
		}
		break;

		case OTA_CHECKSUM_VERIFY: {
			uint8_t check_result = p_value[1];
			if (check_result == OTA_CHECKSUM_OK) {
				printf("OTA checksum verify ok, continue to send next block data!\r\n");
				if (ota_tx_info.cur_offset == ota_tx_info.cur_image_len && ota_tx_info.index + 1 < ota_tx_info.hdr_num) {
					ota_tx_info.index += 1;
					bt_ota_client_set_image_param(ota_tx_info.index); //set next image param
				}

				/* continue to push remain data */
				bt_ota_client_push_image(conn_id);
			} else {
				printf("%s: OTA checksum error 0x%02x\r\n", __func__, check_result);
			}
		}
		break;

		case OTA_COMPLETE: {
			printf("============ OTA process completed!!! ============\r\n");
		}
		break;

		default:
			break;
		}
	}
}


/**
 * @brief  Callback will be called when data sent from gcs client.
 * @param  client_id the ID distinguish which module sent the data.
 * @param  conn_id connection ID.
 * @param  p_data  pointer to data.
 * @retval   result @ref T_APP_RESULT
 */
T_APP_RESULT bt_ota_client_ota_client_callback(T_CLIENT_ID client_id, uint8_t conn_id, void *p_data)
{
	T_APP_RESULT  result = APP_RESULT_SUCCESS;

	if (client_id == bt_ota_client_id)
	{
		T_OTA_CLIENT_CB_DATA *p_ota_client_cb_data = (T_OTA_CLIENT_CB_DATA *)p_data;

		APP_PRINT_INFO3("bt_ota_client_ota_client_callback: client_id %d, conn_id %d, cb_type %d",
						client_id, conn_id, p_ota_client_cb_data->cb_type);

		switch(p_ota_client_cb_data->cb_type)
		{
			case OTA_CLIENT_CB_TYPE_DISC_STATE:
			{
				switch (p_ota_client_cb_data->cb_content.disc_state) {
				case DISC_OTA_DONE:
					printf("bt_ota_client_ota_client_callback: discover procedure done.\r\n");
					/* discover procedure done then enable indicate cccd */
					bt_ota_client_enable_indicate_cccd(conn_id, true);
					break;
				case DISC_OTA_FAILED:
					printf("bt_ota_client_ota_client_callback: discover request failed\r\n");
					break;
				default:
					break;
				}
			}
			break;

			case OTA_CLIENT_CB_TYPE_WRITE_RESULT:
			{
				uint8_t write_type = p_ota_client_cb_data->cb_content.write_result.type;
				uint16_t cause = p_ota_client_cb_data->cb_content.write_result.cause;

				if (cause == 0) {
					switch (write_type)
					{
						case OTA_WRITE_CTRL_CCCD:
							printf("bt_ota_client_ota_client_callback: OTA_WRITE_CTRL_CCCD success\r\n");
							break;
						case OTA_WRITE_IMG_HDR:
							printf("bt_ota_client_ota_client_callback: OTA_WRITE_IMG_HDR success\r\n");
							break;
						case OTA_WRITE_HDR_CHECKSUM:
							printf("bt_ota_client_ota_client_callback: OTA_WRITE_HDR_CHECKSUM success\r\n");
							break;
						case OTA_WRITE_DATA_CHECKSUM:
							printf("bt_ota_client_ota_client_callback: OTA_WRITE_DATA_CHECKSUM success\r\n");
							bt_ota_push_image_status = 0;
							break;
						case OTA_WRITE_DATA:
							if (bt_ota_push_image_status == 0) {
								bt_ota_client_push_image(conn_id);
							} else {
								bt_ota_client_send_image_checksum(conn_id);
							}
							break;
						default:
							break;
					}
				} else {
					printf("bt_ota_client_ota_client_callback: write failed, type=%d, cause=0x%x\r\n", write_type, cause);
					le_disconnect(conn_id);
				}
			}
			break;

			case OTA_CLIENT_CB_TYPE_NOTIF_IND_RESULT: {
				client_attr_ind_confirm(conn_id);
				bt_ota_client_handle_indicate_msg(conn_id, (void *)&p_ota_client_cb_data->cb_content.notif_ind_data);
				result = APP_RESULT_PENDING;
			}
			break;

			default:
				APP_PRINT_ERROR1("BLE_OTA_CENTRAL_CLIENT_CB_TYPE_INVALID, 0x%02x!", p_ota_client_cb_data->cb_type);
				printf("BLE_OTA_CENTRAL_CLIENT_CB_TYPE_INVALID, 0x%02x!\r\n", p_ota_client_cb_data->cb_type);
				break;
		}

	}

	return result;
}
#endif
/** @} */ /* End of group GCS_CLIIENT_CALLBACK */
/** @} */ /* End of group CENTRAL_CLIENT_APP */