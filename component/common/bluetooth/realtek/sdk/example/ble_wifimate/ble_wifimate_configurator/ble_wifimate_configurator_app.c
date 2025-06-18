/**
*****************************************************************************************
*     Copyright(c) 2025, Realtek Semiconductor Corporation. All rights reserved.
*****************************************************************************************
*/

/*============================================================================*
 *                              Header Files
 *============================================================================*/
#include "platform_opts_bt.h"
#if defined(CONFIG_BLE_WIFIMATE_CONFIGURATOR) && CONFIG_BLE_WIFIMATE_CONFIGURATOR
#include <stdio.h>
#include <string.h>
#include "platform_stdlib.h"
#include <trace_app.h>
#include <gap.h>
#include <gap_scan.h>
#include <gap_bond_le.h>
#include <gap_msg.h>
#include <gap_conn_le.h>
#include <profile_server.h>
#include <app_msg.h>
#include <atcmd_bt.h>
#include <log_service.h>
#include <ble_wifimate_service.h>
#include <ble_wifimate_client.h>
#include <ble_wifimate_configurator_app.h>

/*============================================================================*
 *                              Variables
 *============================================================================*/
/**  @brief  BLE WiFiMate client id */
T_CLIENT_ID   ble_wifimate_client_id;

ble_wifimate_configurator_link_t ble_wifimate_configurator_link_table[BLE_WIFIMATE_CONFIGURATOR_APP_MAX_LINKS] = {0};

/** @brief  GAP device state */
static T_GAP_DEV_STATE ble_wifimate_configurator_gap_dev_state = {0, 0, 0, 0, 0};

/** @brief  scan target device name */
static char remote_device_name[13] = {'R', 'T', 'K', '_', 'W', 'i', 'F', 'i', 'M', 'a', 't', 'e', '\0'};

static uint8_t s_encryt_set_enable = true;
static ble_wifimate_encrypt_decrypt_t s_encrypt = {0};

void ble_wifimate_configurator_app_handle_gap_msg(T_IO_MSG *p_gap_msg);

void ble_wifimate_configurator_app_encrypt_set_enable(uint8_t enable)
{
	s_encryt_set_enable = enable;
}

static int ble_wifimate_configurator_app_encrypt_set(uint8_t type, uint8_t key[BLE_WIFIMATE_KEY_LEN])
{
	if (!key) {
		APP_PRINT_ERROR0("[APP] BLE WifiMate configurator encrypt key invalid.\r\n");
		return BT_AT_ERR_PARAM_INVALID;
	}

	if (!s_encryt_set_enable) {
		APP_PRINT_ERROR0("[APP] BLE WifiMate configurator encrypt inuse, can't set now.\r\n");
		return BT_AT_FAIL;
	}

	s_encrypt.algorithm_type = type;
	memcpy(s_encrypt.key, key, BLE_WIFIMATE_KEY_LEN);

	return BT_AT_OK;
}

T_GAP_CAUSE ble_wifimate_configurator_connect(uint8_t *addr)
{
	uint8_t DestAddr[6] = {0};
	uint8_t DestAddrType = GAP_REMOTE_ADDR_LE_PUBLIC;
	T_GAP_LOCAL_ADDR_TYPE local_addr_type = GAP_LOCAL_ADDR_LE_PUBLIC;
	T_GAP_LE_CONN_REQ_PARAM conn_req_param;

	memset(&conn_req_param, 0, sizeof(conn_req_param));
	conn_req_param.scan_interval = 0x60;	//60ms
	conn_req_param.scan_window = 0x30;		//30ms
	conn_req_param.conn_interval_min = 0x60;	//120ms
	conn_req_param.conn_interval_max = 0x60;	//120ms
	conn_req_param.conn_latency = 0;
	conn_req_param.supv_tout = 0x100;
	conn_req_param.ce_len_min = 2 * (conn_req_param.conn_interval_min - 1);
	conn_req_param.ce_len_max = 2 * (conn_req_param.conn_interval_max - 1);
	le_set_conn_param(GAP_CONN_PARAM_1M, &conn_req_param);

	memcpy(DestAddr, addr, 6);
	printf("cmd_con, addr: %02X:%02X:%02X:%02X:%02X:%02X\r\n",
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

/* The hex str is not considered as a number, just regared as a list of byte data,
    e.g. string: "112233445566778899aabb" transformed to arrary is as
    arr[]: {0x11, 0x22, 0x33, 0x44, 0x55, ...}  */
static bool hexdata_str_to_array(char *str, uint8_t *byte_arr, uint8_t arr_len)
{
	uint32_t str_len = strlen(str);
	char *str_ptr = str;
	uint32_t n = 0;
	uint8_t byte_high = 0, byte_low = 0;

	if (str_len % 2 || arr_len < str_len / 2) {
		APP_PRINT_ERROR1("[%s]Error: Hexdata is invalid\r\n", __func__);
		return FALSE;
	}

	while (n < str_len) {
		byte_high = ctoi(*(str_ptr++));
		byte_low = ctoi(*(str_ptr++));
		if (byte_high == 0xFF || byte_low == 0xFF) {
			return FALSE;
		}
		byte_arr[n / 2] = byte_high << 4 | byte_low;
		n += 2;
	}

	return TRUE;
}

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

static int ble_wifimate_configurator_parse_param(char *buf, char **argv)
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

static int ble_wifimate_configurator_app_cmd_encrypt_set(int argc, char **argv)
{
	uint8_t algo_type = 0;
	uint8_t key[BLE_WIFIMATE_KEY_LEN] = {0};

	if (!argv) {
		return BT_AT_ERR_PARAM_INVALID;
	}

	algo_type = atoi(argv[0]);
	if (algo_type !=  BLE_WIFIMATE_ENCRYPT_DECRYPT_ALGO_NONE) {
		if (argc == 2 && argv[1]) {
			uint8_t str_len = strlen(argv[1]);

			if (str_len != 2 * BLE_WIFIMATE_KEY_LEN) {
				printf("[AT+BTDEMO] BLE wifimate configurator: Invalid key string\r\n");
				return BT_AT_ERR_PARAM_INVALID;
			}

			if (false == hexdata_str_to_array(argv[1], key, BLE_WIFIMATE_KEY_LEN)) {
				printf("[AT+BTDEMO] BLE wifimate configurator encrypt key invalid\r\n");
				return BT_AT_ERR_PARAM_INVALID;
			}
		} else {
			printf("[AT+BTDEMO] BLE wifimate configurator encrypt parameter invalid\r\n");
			return BT_AT_ERR_PARAM_INVALID;
		}
	}
	printf("[AT+BTDEMO] BLE wifimate configurator encrypt set algo_type=%d \
				key=%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x\r\n",
			algo_type, key[0], key[1], key[2], key[3], key[4], key[5], key[6],
			key[7], key[8], key[9], key[10], key[11], key[12]);

	return ble_wifimate_configurator_app_encrypt_set(algo_type, key);
}

static int ble_wifimate_configurator_app_cmd_wifi_connect(int argc, char **argv)
{
	uint8_t  conn_id;
	uint16_t conn_handle;
	wifi_conn_config_t conn_info;

	if ((argc < 3) || !argv) {
		APP_PRINT_ERROR0("BLE wifimate configurator wifi connect param invalid\r\n");
		return BT_AT_ERR_PARAM_INVALID;
	}

	conn_handle = (uint16_t)atoi(argv[0]);
	if (!le_get_conn_id_by_handle(conn_handle, &conn_id)) {
		APP_PRINT_ERROR1("BLE wifimate configurator wifi connect conn_handle %d invalid\r\n", conn_handle);
		return BT_AT_ERR_LOWER_STACK;
	}

	memset(&conn_info, 0, sizeof(conn_info));
	conn_info.ssid_len = strlen(argv[1]);
	memcpy(conn_info.ssid, argv[1], conn_info.ssid_len);
	conn_info.security = (uint8_t)atoi(argv[2]);

	if (argc > 3) {
		conn_info.password_len = strlen(argv[3]);
		conn_info.password = (unsigned char *)(argv[3]);

		APP_PRINT_INFO6("BLE wifimate configurator conn_handle=%d, ssid_len=%d, ssid=%s, security=%d, password_len=%d, password=%s\r\n",
				conn_handle, conn_info.ssid_len, conn_info.ssid,
				conn_info.security, conn_info.password_len, conn_info.password);
	} else {
		APP_PRINT_INFO4("BLE wifimate configurator conn_handle=%d, ssid_len=%d, ssid=%s, security=%d, password_len=0, password=\r\n",
				conn_handle, conn_info.ssid_len, conn_info.ssid, conn_info.security);
	}

	return ble_wifimate_client_wifi_connect(conn_id, &conn_info);
}

void ble_wifimate_configurator_app_handle_qdecode_cmd(uint16_t subtype, void *arg)
{
	int argc = 0;
	char *argv[MAX_ARGC] = {0};
	T_GAP_DEV_STATE state;
	int ret = 0;
	bool sem_wait = true; 

	if (arg) {
		argc = ble_wifimate_configurator_parse_param(arg, argv);
	}

	le_get_gap_param(GAP_PARAM_DEV_STATE , &state);
	switch (subtype) {
		case BLE_WIFIMATE_CONFIGURATOR_SCAN_START: {
			if (state.gap_init_state == GAP_INIT_STATE_STACK_READY) {
				ret = (le_scan_start() == GAP_CAUSE_SUCCESS) ? BT_AT_OK : BT_AT_ERR_LOWER_STACK;
			} else {
				printf("BT stack not ready\r\n");
				ret = BT_AT_ERR_NOT_READY;
			}
		}
		break;

		case BLE_WIFIMATE_CONFIGURATOR_SCAN_STOP: {
			if (state.gap_init_state == GAP_INIT_STATE_STACK_READY) {
				ret = (le_scan_stop() == GAP_CAUSE_SUCCESS) ? BT_AT_OK : BT_AT_ERR_LOWER_STACK;
			} else {
				printf("BT stack not ready\r\n");
				ret = BT_AT_ERR_NOT_READY;
			}
		}
		break;

		case BLE_WIFIMATE_CONFIGURATOR_BLE_CONNECT: {
			uint8_t addr[6] = {0};
			if (argc != 4 || !argv[3]) {
				ret = BT_AT_ERR_PARAM_INVALID;
			} else {
				hex_str_to_bd_addr(strlen(argv[3]), (s8 *)argv[3], (u8*)addr);
				ret = (ble_wifimate_configurator_connect(addr) == GAP_CAUSE_SUCCESS) ? \
						BT_AT_OK : BT_AT_ERR_LOWER_STACK;
			}
		}
		break;

		case BLE_WIFIMATE_CONFIGURATOR_WIFI_SCAN: {
			uint8_t conn_id;
			uint16_t conn_handle;
			
			if (argc != 4 || !argv[3]) {
				ret = BT_AT_ERR_PARAM_INVALID;
			} else {
				conn_handle = atoi(argv[3]);
				if (le_get_conn_id_by_handle(conn_handle, &conn_id)) {
					ret = ble_wifimate_client_wifi_scan(conn_id);
				} else {
					printf("%s: conn_handle %d invalid\r\n", __func__, conn_handle);
					ret = BT_AT_ERR_LOWER_STACK;
				}
			}
		}
		break;

		case BLE_WIFIMATE_CONFIGURATOR_WIFI_CONNECT: {
			if (argc >= 6) {
				ret = ble_wifimate_configurator_app_cmd_wifi_connect(argc - 3, &argv[3]);
			} else {
				ret = BT_AT_ERR_PARAM_INVALID;
			}
		}
		break;

		case BLE_WIFIMATE_CONFIGURATOR_ENCRYPT_SET: {
			if (argc >= 4) {
				ret = ble_wifimate_configurator_app_cmd_encrypt_set(argc - 3, &argv[3]);
				sem_wait = false;
			} else {
				ret = BT_AT_ERR_PARAM_INVALID;
			}
		}
		break;

		default:
			break;
	}

#if defined(BT_AT_SYNC) && BT_AT_SYNC
	if ((ret != BT_AT_OK) || sem_wait == false) {
		bt_at_sync_set_result(ret);
		bt_at_sync_sem_give();
	}
#endif
}

/**
 * @brief    All the application messages are pre-handled in this function
 * @note     All the IO MSGs are sent to this function, then the event handling
 *           function shall be called according to the MSG type.
 * @param[in] io_msg  IO message data
 * @return   void
 */
void ble_wifimate_configurator_app_handle_io_msg(T_IO_MSG io_msg)
{
	uint16_t msg_type = io_msg.type;

	switch (msg_type) {
	case IO_MSG_TYPE_BT_STATUS: {
		ble_wifimate_configurator_app_handle_gap_msg(&io_msg);
		break;
	}

	case IO_MSG_TYPE_QDECODE: {
		uint16_t subtype = io_msg.subtype;
		void *arg = io_msg.u.buf;
		ble_wifimate_configurator_app_handle_qdecode_cmd(subtype, arg);		
		break;
	}

	default:
		break;
	}
}

bool ble_wifimate_configurator_filter_scan_info_by_name(T_LE_SCAN_INFO *scan_info, uint8_t *p_remote_name)
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
				if (strstr((const char *)buffer, (const char *)p_remote_name) != NULL) {
					printf("%s: found %s\r\n", __func__, p_remote_name);
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

/**
  * @brief Callback for gap le to notify app
  * @param[in] cb_type callback msy type @ref GAP_LE_MSG_Types.
  * @param[in] p_cb_data point to callback data @ref T_LE_CB_DATA.
  * @retval result @ref T_APP_RESULT
  */
T_APP_RESULT ble_wifimate_configurator_app_gap_callback(uint8_t cb_type, void *p_cb_data)
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

		if (ble_wifimate_configurator_filter_scan_info_by_name(p_data->p_le_scan_info, (uint8_t *)remote_device_name)) {
			uint8_t *addr = p_data->p_le_scan_info->bd_addr;
			char le_addr[30] = {0};
			printf("Scan found device %s: [%02x:%02x:%02x:%02x:%02x:%02x]\r\n", remote_device_name,
				   addr[5], addr[4], addr[3], addr[2], addr[1], addr[0]);

#if defined(BT_AT_SYNC) && BT_AT_SYNC
			addr_to_str(p_data->p_le_scan_info->bd_addr, p_data->p_le_scan_info->remote_addr_type, le_addr, sizeof(le_addr));
			BT_AT_PRINT("+BLEGAP:scan,%s,%d,%d,%d\r\n",
						le_addr, p_data->p_le_scan_info->adv_type, p_data->p_le_scan_info->rssi, p_data->p_le_scan_info->data_len);
#endif	   
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

/**
 * @brief  Callback will be called when data sent from gcs client.
 * @param  client_id the ID distinguish which module sent the data.
 * @param  conn_id connection ID.
 * @param  p_data  pointer to data.
 * @retval   result @ref T_APP_RESULT
 */
T_APP_RESULT ble_wifimate_client_callback(T_CLIENT_ID client_id, uint8_t conn_id, void *p_data)
{
    T_APP_RESULT  result = APP_RESULT_SUCCESS;
    uint16_t conn_handle = le_get_conn_handle(conn_id);
    APP_PRINT_INFO3("%s: client_id %d, conn_handle %d",
                    __func__, client_id, conn_handle);
    if (client_id == ble_wifimate_client_id)
    {
		ble_wifimate_client_cb_data_t *p_cb_data = (ble_wifimate_client_cb_data_t *)p_data;
        switch (p_cb_data->cb_type)
        {
        case BLE_WIFIMATE_CLIENT_CB_TYPE_DISC_STATE:
			APP_PRINT_INFO1("[BLE WiFiMate Configurator] discover %s", 
                            (p_cb_data->cb_content.disc_state == DISC_DONE) ? "done" : "fail");
			printf("[BLE WiFiMate Configurator] discover %s\r\n", 
                            (p_cb_data->cb_content.disc_state == DISC_DONE) ? "done" : "fail");
            break;
        case BLE_WIFIMATE_CLIENT_CB_TYPE_WRITE_RESULT:
            APP_PRINT_INFO2("WRITE RESULT: cause 0x%x, type %d",
                            p_cb_data->cb_content.write_result.cause,
                            p_cb_data->cb_content.write_result.type);
            printf("WRITE RESULT: cause 0x%x, type %d\r\n",
                            p_cb_data->cb_content.write_result.cause,
                            p_cb_data->cb_content.write_result.type);
            break;
        case BLE_WIFIMATE_CLIENT_CB_TYPE_IND_RESULT:
            APP_PRINT_INFO1("INDICATION: type %d", p_cb_data->cb_content.ind_data.type);
            printf("INDICATION: type %d\r\n", p_cb_data->cb_content.ind_data.type);
            break;
        default:
            break;
        }
    }

    return result;
}

/**
 * @brief    Handle msg GAP_MSG_LE_DEV_STATE_CHANGE
 * @note     All the gap device state events are pre-handled in this function.
 *           Then the event handling function shall be called according to the new_state
 * @param[in] new_state  New gap device state
 * @param[in] cause GAP device state change cause
 * @return   void
 */
void ble_wifimate_configurator_app_handle_dev_state_evt(T_GAP_DEV_STATE new_state, uint16_t cause)
{
	APP_PRINT_INFO4("%s: init state  %d, scan state %d, cause 0x%x",
					__func__, new_state.gap_init_state, new_state.gap_scan_state, cause);

    if (ble_wifimate_configurator_gap_dev_state.gap_init_state != new_state.gap_init_state)
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

	if (ble_wifimate_configurator_gap_dev_state.gap_scan_state != new_state.gap_scan_state)
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

#if defined(BT_AT_SYNC) && BT_AT_SYNC
        if ((new_state.gap_scan_state == GAP_SCAN_STATE_IDLE) ||
            (new_state.gap_scan_state == GAP_SCAN_STATE_SCANNING)) {
            if (bt_at_sync_event_match_check(BT_AT_SYNC_CMD_TYPE_BLE_GAP_SCAN)) {
                bt_at_sync_set_result_by_evt_cause(cause);
                bt_at_sync_sem_give();
            }
        }
#endif
	}

	ble_wifimate_configurator_gap_dev_state = new_state;
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
void ble_wifimate_configurator_app_handle_conn_state_evt(uint8_t conn_id, T_GAP_CONN_STATE new_state, uint16_t disc_cause)
{
	switch (new_state) {
	case GAP_CONN_STATE_DISCONNECTED: {
		if ((disc_cause != (HCI_ERR | HCI_ERR_REMOTE_USER_TERMINATE))
			&& (disc_cause != (HCI_ERR | HCI_ERR_LOCAL_HOST_TERMINATE))) {
			APP_PRINT_ERROR1("ble_wifimate_configurator_app_handle_conn_state_evt: connection lost cause 0x%x", disc_cause);
		}
		printf("[BLE WiFiMate Configurator] BT Disconnected, cause 0x%x\r\n", disc_cause);
		ble_wifimate_configurator_app_encrypt_set_enable(true);
		ble_wifimate_client_disconnect(conn_id);

#if defined(BT_AT_SYNC) && BT_AT_SYNC
		uint8_t  remote_bd[6];
		uint16_t conn_handle = ble_wifimate_configurator_link_table[conn_id].conn_handle;

		if (disc_cause != (HCI_ERR | HCI_ERR_LOCAL_HOST_TERMINATE)) {
			char le_addr[30] = {0};
			addr_to_str(ble_wifimate_configurator_link_table[conn_id].bd_addr, 
						ble_wifimate_configurator_link_table[conn_id].bd_type, 
						le_addr,
						sizeof(le_addr));
			BT_AT_PRINT("[$]+BLEGAP:disconn,%d,%s\r\n", conn_handle, le_addr);
		}

		bt_at_sync_disconnect_hdl(conn_handle);
#endif
		memset(&ble_wifimate_configurator_link_table[conn_id], 0, sizeof(ble_wifimate_configurator_link_table[conn_id]));
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
		ble_wifimate_configurator_link_table[conn_id].conn_handle = conn_handle;
		ble_wifimate_configurator_link_table[conn_id].bd_type = remote_bd_type;
		memcpy(ble_wifimate_configurator_link_table[conn_id].bd_addr, remote_bd, sizeof(remote_bd));
		APP_PRINT_INFO5("GAP_CONN_STATE_CONNECTED:remote_bd %s, remote_addr_type %d, conn_interval 0x%x, conn_latency 0x%x, conn_supervision_timeout 0x%x",
						TRACE_BDADDR(remote_bd), remote_bd_type,
						conn_interval, conn_latency, conn_supervision_timeout);
		printf("[BLE WiFiMate Configurator] BT Connected, conn_handle:%d\r\n", conn_handle);

		/* update encrypt key */
		if (s_encrypt.algorithm_type != BLE_WIFIMATE_ENCRYPT_DECRYPT_ALGO_NONE) {
			s_encrypt.key[BLE_WIFIMATE_ENCRYPT_DECRYPT_KEY_LEN - 3] = remote_bd[2];
			s_encrypt.key[BLE_WIFIMATE_ENCRYPT_DECRYPT_KEY_LEN - 2] = remote_bd[1];
			s_encrypt.key[BLE_WIFIMATE_ENCRYPT_DECRYPT_KEY_LEN - 1] = remote_bd[0];
		}
		if (ble_wifimate_client_encrypt_set(&s_encrypt)) {
			APP_PRINT_INFO0("[APP] BLE wifimate client set encrypt success.\r\n");
		}

		ble_wifimate_configurator_app_encrypt_set_enable(false);
		ble_wifimate_client_connect(conn_id);

#if defined(BT_AT_SYNC) && BT_AT_SYNC
		char le_addr[30] = {0};
		addr_to_str(remote_bd, remote_bd_type, le_addr, sizeof(le_addr));
		BT_AT_PRINT("+BLEGAP:conn,%d,%s\r\n", conn_handle, le_addr);
		if (bt_at_sync_event_match_check(BT_AT_SYNC_CMD_TYPE_BLE_GAP_CONN)) {
			bt_at_sync_set_result_by_evt_cause(disc_cause);
			bt_at_sync_sem_give();
        }
#endif
	}
	break;

	default:
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
void ble_wifimate_configurator_app_handle_conn_mtu_info_evt(uint8_t conn_id, uint16_t mtu_size)
{
	APP_PRINT_INFO2("ble_wifimate_configurator_app_handle_conn_mtu_info_evt: conn_id %d, mtu_size %d", conn_id, mtu_size);
	printf("conn_mtu_info_evt: conn_id %d, mtu_size %d\r\n", conn_id, mtu_size);
	ble_wifimate_client_set_mtu_size(mtu_size);
}

/**
 * @brief    Handle msg GAP_MSG_LE_CONN_PARAM_UPDATE
 * @note     All the connection parameter update change  events are pre-handled in this function.
 * @param[in] conn_id Connection ID
 * @param[in] status  New update state
 * @param[in] cause Use this cause when status is GAP_CONN_PARAM_UPDATE_STATUS_FAIL
 * @return   void
 */
void ble_wifimate_configurator_app_handle_conn_param_update_evt(uint8_t conn_id, uint8_t status, uint16_t cause)
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

		//discover ble wifimate service
		ble_wifimate_client_start_discovery(conn_id);
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
void ble_wifimate_configurator_app_handle_authen_state_evt(uint8_t conn_id, uint8_t new_state, uint16_t cause)
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
void ble_wifimate_configurator_app_handle_gap_msg(T_IO_MSG *p_gap_msg)
{
	T_LE_GAP_MSG gap_msg;
	uint8_t conn_id;
	memcpy(&gap_msg, &p_gap_msg->u.param, sizeof(p_gap_msg->u.param));

	APP_PRINT_TRACE1("ble_wifimate_configurator_app_handle_gap_msg: subtype %d", p_gap_msg->subtype);
	switch (p_gap_msg->subtype) {
	case GAP_MSG_LE_DEV_STATE_CHANGE: {
		ble_wifimate_configurator_app_handle_dev_state_evt(gap_msg.msg_data.gap_dev_state_change.new_state,
								 gap_msg.msg_data.gap_dev_state_change.cause);
	}
	break;

	case GAP_MSG_LE_CONN_STATE_CHANGE: {
		ble_wifimate_configurator_app_handle_conn_state_evt(gap_msg.msg_data.gap_conn_state_change.conn_id,
								  (T_GAP_CONN_STATE)gap_msg.msg_data.gap_conn_state_change.new_state,
								  gap_msg.msg_data.gap_conn_state_change.disc_cause);
	}
	break;

	case GAP_MSG_LE_CONN_MTU_INFO: {
		ble_wifimate_configurator_app_handle_conn_mtu_info_evt(gap_msg.msg_data.gap_conn_mtu_info.conn_id,
									 gap_msg.msg_data.gap_conn_mtu_info.mtu_size);
	}
	break;

	case GAP_MSG_LE_CONN_PARAM_UPDATE: {
		ble_wifimate_configurator_app_handle_conn_param_update_evt(gap_msg.msg_data.gap_conn_param_update.conn_id,
										 gap_msg.msg_data.gap_conn_param_update.status,
										 gap_msg.msg_data.gap_conn_param_update.cause);
	}
	break;

	case GAP_MSG_LE_AUTHEN_STATE_CHANGE: {
		ble_wifimate_configurator_app_handle_authen_state_evt(gap_msg.msg_data.gap_authen_state.conn_id,
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
	}
	break;

	case GAP_MSG_LE_BOND_PASSKEY_INPUT: {
		conn_id = gap_msg.msg_data.gap_bond_passkey_input.conn_id;
		APP_PRINT_INFO1("GAP_MSG_LE_BOND_PASSKEY_INPUT: conn_id %d", conn_id);
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

void ble_wifimate_configurator_app_reset(void)
{
	memset(&ble_wifimate_configurator_gap_dev_state, 0, sizeof(ble_wifimate_configurator_gap_dev_state));
	memset(&s_encrypt, 0, sizeof(s_encrypt));
	ble_wifimate_configurator_app_encrypt_set_enable(true);
}

#endif /* CONFIG_BLE_WIFIMATE_CONFIGURATOR */