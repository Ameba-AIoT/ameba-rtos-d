/**
*****************************************************************************************
*     Copyright(c) 2025, Realtek Semiconductor Corporation. All rights reserved.
*****************************************************************************************
  * @file     ble_wifimate_client.c
  * @brief    Source file for using ble wifimate client.
  * @details  BLE wifimate client functions handling.
  * *************************************************************************************
  */
#include "platform_opts_bt.h"
#if defined(CONFIG_BLE_WIFIMATE_CONFIGURATOR) && CONFIG_BLE_WIFIMATE_CONFIGURATOR
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <trace_app.h>
#include <os_mem.h>
#include <gap_conn_le.h>
#include "hal_crypto.h"
#include <atcmd_bt.h>
#include <ble_wifimate_service.h>
#include <ble_wifimate_client.h>

#define BLE_WIFIMATE_CLIENT_MAX_LINKS 					1
#define BT_UUID_GATT_CCC_VAL                			0x2902

#define BLE_WIFIMATE_CLIENT_RECV_SUCCESS				0
#define BLE_WIFIMATE_CLIENT_RECV_FAIL					1
#define BLE_WIFIMATE_CLIENT_RECV_DONE					2

uint8_t s_ble_wifimate_client_link_num = 0;
 /**<  BLE OTA client ID. */
static T_CLIENT_ID ble_wifimate_client_id = CLIENT_PROFILE_GENERAL_ID;
/**<  Callback used to send data to app from BLE OTA client layer. */
static P_FUN_GENERAL_APP_CB p_ble_wifimate_client_cb = NULL;
/**<  BLE WiFiMate client database */
static ble_wifimate_client_db_t *s_ble_wifimate_db[BLE_WIFIMATE_CLIENT_MAX_LINKS] = {0};

static uint16_t s_mtu_size = 23;
static ble_wifimate_char_send_data_t s_multi_write = {0};
static ble_wifimate_char_recv_data_t s_multi_recv = {0};
/* sp_encrypt point to s_encrypt, no need free in the client */
static ble_wifimate_encrypt_decrypt_t *sp_encrypt = NULL;

static void ble_wifimate_client_disc_state_cb(uint8_t conn_id,  T_DISCOVERY_STATE discovery_state);
static void ble_wifimate_client_disc_result_cb(uint8_t conn_id,
										 T_DISCOVERY_RESULT_TYPE result_type,
										 T_DISCOVERY_RESULT_DATA result_data);
static void ble_wifimate_client_write_result_cb(uint8_t conn_id, T_GATT_WRITE_TYPE type, uint16_t handle,
										  uint16_t cause, uint8_t credits);
static T_APP_RESULT ble_wifimate_client_indicate_result_cb(uint8_t conn_id, bool notify, uint16_t handle,
														uint16_t value_size, uint8_t *p_value);
static void ble_wifimate_client_disconnect_cb(uint8_t conn_id);

/**
 * @brief Dfu Client Callbacks.
 */
const T_FUN_CLIENT_CBS ble_wifimate_client_cbs =
{
	ble_wifimate_client_disc_state_cb,              //!< Discovery State callback function pointer
 	ble_wifimate_client_disc_result_cb,             //!< Discovery result callback function pointer
	NULL,                                           //!< Read response callback function pointer
	ble_wifimate_client_write_result_cb,            //!< Write result callback function pointer
	ble_wifimate_client_indicate_result_cb,      	//!< Notify Indicate callback function pointer
	ble_wifimate_client_disconnect_cb               //!< Link disconnection callback function pointer
};

static bool array_to_little_endian(uint8_t *data, size_t length)
{
	if (!data) {
		APP_PRINT_ERROR1("[%s]Error: array is invalid\r\n", __func__);
		return FALSE;
	}

	for (size_t i = 0; i < length / 2; ++i) {
		uint8_t temp = data[i];
		data[i] = data[length - 1 - i];
		data[length - 1 - i] = temp;
	}

	return TRUE;
}

void ble_wifimate_client_set_mtu_size(uint16_t mtu_size)
{
	s_mtu_size = mtu_size;
}

static uint32_t ble_wifimate_client_checksum_cal(uint16_t len, uint8_t *data)
{
	uint32_t checksum = 0;
	for (uint16_t i = 0; i < len; ++i) {
		checksum += *(data + i);
	}

	return checksum;
}

bool ble_wifimate_client_encrypt_set(ble_wifimate_encrypt_decrypt_t *enc)
{
	if (!enc) {
		return false;
	}

	sp_encrypt = enc;
	return true;
}

static bool ble_wifimate_aes_ecb_encrypt(uint8_t src_len, uint8_t *src, uint8_t key_len, 
											uint8_t *key, uint8_t *result_len, uint8_t *result)
{
	/* If the length of the input plaintext data is not a multiple of the key length,
	   it should be padded with zeros to make it a multiple of the key length. */
	uint8_t align_len = (uint8_t)ceil(src_len / (double)key_len) * key_len;
	uint8_t *message = NULL;
	uint8_t *key_le = NULL;
	int ret;

	if (!src || !key || !result_len || !result) {
		return false;
	}

	if (rtl_cryptoEngine_init() != 0) {
		printf("crypto engine init failed\r\n");
	} else {
		printf("crypto engine init success\r\n");
	}

	key_le = (uint8_t *)os_mem_alloc(RAM_TYPE_DATA_ON, key_len);
	if (!key_le) {
		printf("crypto alloc key_le fail\r\n");
		return false;
	}

	/* use a software key, it needs to be converted to little-endian format.
	    For example, the key is 00112233445566778899aabbccddeeff, the key array should like:
	    uint8_t key1[32] = {
	        0xff, 0xee, 0xdd, 0xcc, 0xbb, 0xaa, 0x99, 0x88, 0x77, 0x66, 0x55, 0x44, 0x33, 0x22, 0x11, 0x00 };.*/
	memcpy(key_le, key, key_len);
	array_to_little_endian(key_le, key_len);
	ret = rtl_crypto_aes_ecb_init(key_le, key_len);
	if ( ret != 0 ) {
		printf("AES ECB init failed\r\n");
		os_mem_free(key_le);
		return false;
	}

	message = (uint8_t *)os_mem_alloc(RAM_TYPE_DATA_ON, align_len);
	if (!message) {
		printf("crypto alloc message fail\r\n");
		os_mem_free(key_le);
		return false;
	}
	memset(message, 0, align_len);
	memcpy(message, src, src_len);
	ret = rtl_crypto_aes_ecb_encrypt(message, align_len, NULL, 0, result);
	if ( ret != 0 ) {
		printf("AES ECB encrypt failed\r\n");
		os_mem_free(key_le);
		os_mem_free(message);
		return false;
	}
	*result_len = align_len;

	os_mem_free(key_le);
	os_mem_free(message);
	return true;
}

static bool ble_wifimate_client_encrypt(uint8_t src_len, uint8_t *src, uint8_t *des_len, uint8_t *des)
{
	bool ret = false;

	if (!src || !des_len || !des) {
		return false;
	}

	if (!sp_encrypt) {
		printf("%s: sp_encrypt null\r\n", __func__);
		return false;
	}

	switch (sp_encrypt->algorithm_type) {
	case BLE_WIFIMATE_ENCRYPT_DECRYPT_ALGO_AES_ECB: {
		ret = ble_wifimate_aes_ecb_encrypt(src_len, src, BLE_WIFIMATE_ENCRYPT_DECRYPT_KEY_LEN,
										   sp_encrypt->key, des_len, des);
		break;
	}
	default:
		break;
	}

	return ret;
}

static bool ble_wifimate_char_multi_recv_data_init(uint16_t total_len, uint8_t seg_num, uint32_t checksum)
{
	if (s_multi_recv.data) {
		os_mem_free((void *)s_multi_recv.data);
		s_multi_recv.data = NULL;
	}

	memset(&s_multi_recv, 0, sizeof(s_multi_recv));
	s_multi_recv.flag = true;
	s_multi_recv.total_len = total_len;
	s_multi_recv.seg_num = seg_num;
	s_multi_recv.checksum = checksum;
	s_multi_recv.seg_idx = 0;
	s_multi_recv.offset = 0;
	s_multi_recv.data = (void *)os_mem_alloc(RAM_TYPE_DATA_ON, total_len);
	if (!s_multi_recv.data) {
		APP_PRINT_ERROR1("[APP] BLE WifiMate client init multi indication receive data: can't alloc memory, len=%u\r\n", total_len);
		memset(&s_multi_recv, 0, sizeof(s_multi_recv));
		return false;
	}
	memset(s_multi_recv.data, 0, total_len);

	return true;
}

static void ble_wifimate_char_multi_recv_data_deinit(void)
{
	if (s_multi_recv.data) {
		os_mem_free((void *)s_multi_recv.data);
	}
	memset(&s_multi_recv, 0, sizeof(s_multi_recv));
}

static uint16_t ble_wifimate_char_multi_write_data_init(wifi_conn_config_t *conn_info)
{
	uint8_t offset = 0;
	uint8_t char_data_len;
	uint8_t encrypt_len = 0;
	uint8_t *encrypt_res = NULL;

	if (!conn_info) {
		return false;
	}

	if (!sp_encrypt || sp_encrypt->algorithm_type == BLE_WIFIMATE_ENCRYPT_DECRYPT_ALGO_NONE) {
		char_data_len = CHAR_WIFI_CONN_ENABLE_SEG_FIRST_ELE_LEN + conn_info->ssid_len + \
						CHAR_WIFI_CONN_ENABLE_SEG_THIRD_ELE_LEN + CHAR_WIFI_CONN_ENABLE_SEG_FORTH_ELE_LEN + \
						conn_info->password_len;

	} else if (sp_encrypt->algorithm_type == BLE_WIFIMATE_ENCRYPT_DECRYPT_ALGO_AES_ECB) {
		encrypt_len = (uint8_t)ceil(conn_info->password_len / 16.0) * 16;
		if (encrypt_len > 0) {
			encrypt_res = (void *)os_mem_alloc(RAM_TYPE_DATA_ON, encrypt_len);
			if (!encrypt_res) {
				printf("[APP] BLE WifiMate client can't alloc memory\r\n");
				return false;
			}

			if (!ble_wifimate_client_encrypt(conn_info->password_len, conn_info->password, &encrypt_len, encrypt_res)) {
				printf("%s: encrypt fail\r\n");
				os_mem_free(encrypt_res);
				return false;
			}
		}

		char_data_len = CHAR_WIFI_CONN_ENABLE_SEG_FIRST_ELE_LEN + conn_info->ssid_len + \
						CHAR_WIFI_CONN_ENABLE_SEG_THIRD_ELE_LEN + CHAR_WIFI_CONN_ENABLE_SEG_FORTH_ELE_LEN + \
						encrypt_len;
	} else {
		printf("[APP] BLE WifiMate client not support encrypt algo type %d.\r\n", sp_encrypt->algorithm_type);
		return false;
	}

	if (s_multi_write.data) {
		os_mem_free((void *)s_multi_write.data);
	}

	memset(&s_multi_write, 0, sizeof(s_multi_write));
	s_multi_write.flag = true;
	s_multi_write.total_len = char_data_len;
	s_multi_write.seg_num = (uint8_t)ceil((double)char_data_len / (s_mtu_size - 3 - 1));
	s_multi_write.data = (void *)os_mem_alloc(RAM_TYPE_DATA_ON, char_data_len);
	if (!s_multi_write.data) {
		printf("[APP] BLE WifiMate client Write failed: can't alloc memory\r\n");
		memset(&s_multi_write, 0, sizeof(s_multi_write));
		if (encrypt_res) {
			os_mem_free(encrypt_res);
		}
		return false;
	}
	memset(s_multi_write.data, 0, char_data_len);

	memcpy(s_multi_write.data, &conn_info->ssid_len, CHAR_WIFI_CONN_ENABLE_SEG_FIRST_ELE_LEN);
	offset = CHAR_WIFI_CONN_ENABLE_SEG_FIRST_ELE_LEN;
	memcpy(s_multi_write.data + offset, conn_info->ssid, conn_info->ssid_len);
	offset += conn_info->ssid_len;
	memcpy(s_multi_write.data + offset, &conn_info->security, CHAR_WIFI_CONN_ENABLE_SEG_THIRD_ELE_LEN);
	offset += CHAR_WIFI_CONN_ENABLE_SEG_THIRD_ELE_LEN;

	if (!sp_encrypt || sp_encrypt->algorithm_type == BLE_WIFIMATE_ENCRYPT_DECRYPT_ALGO_NONE) {
		memcpy(s_multi_write.data + offset, &conn_info->password_len, CHAR_WIFI_CONN_ENABLE_SEG_FORTH_ELE_LEN);
		offset += CHAR_WIFI_CONN_ENABLE_SEG_FORTH_ELE_LEN;
		memcpy(s_multi_write.data + offset,  conn_info->password, conn_info->password_len);
	} else if (sp_encrypt->algorithm_type == BLE_WIFIMATE_ENCRYPT_DECRYPT_ALGO_AES_ECB) {
		memcpy(s_multi_write.data + offset, &encrypt_len, CHAR_WIFI_CONN_ENABLE_SEG_FORTH_ELE_LEN);
		offset += CHAR_WIFI_CONN_ENABLE_SEG_FORTH_ELE_LEN;
		memcpy(s_multi_write.data + offset,  encrypt_res, encrypt_len);
	}

	s_multi_write.checksum = ble_wifimate_client_checksum_cal(s_multi_write.total_len, s_multi_write.data);

	if (encrypt_res) {
		os_mem_free(encrypt_res);
	}

	return true;
}

static void ble_wifimate_char_multi_write_data_deinit(void)
{
	if (s_multi_write.data) {
		os_mem_free((void *)s_multi_write.data);
	}
	memset(&s_multi_write, 0, sizeof(s_multi_write));
}

bool ble_wifimate_client_negotiate_key(uint8_t conn_id, key_negotiate_config_t *config)
{
	uint16_t handle;
	uint16_t length = 0;
	uint8_t *data = NULL;
	bool ret = false;

	if (!config) {
		return false;
	}

	if (conn_id > s_ble_wifimate_client_link_num || !s_ble_wifimate_db[conn_id]) {
		printf("%s: failed invalid conn_id %d\r\n", __func__, conn_id);
		return false;
	}

	if (s_ble_wifimate_db[conn_id]->disc_state != DISC_DONE) {
		printf("[APP] BLE WifiMate client need discover service before write charac\r\n");
		return false;
	}

	length = BLE_WIFIMATE_CHAR_NOGOTIATE_KEY_LEN;
	data = (uint8_t *)os_mem_alloc(RAM_TYPE_DATA_ON, length);
	if (!data) {
		printf("[APP] BLE WifiMate client Write failed: can't alloc memory\r\n");
		return false;
	}

	memcpy(data, &config->algorithm, CHAR_NEGOTIATE_KEY_FIRST_ELE_LEN);
	memcpy(data + CHAR_NEGOTIATE_KEY_FIRST_ELE_LEN, &config->key, CHAR_NEGOTIATE_KEY_SECOND_ELE_LEN);

	handle = s_ble_wifimate_db[conn_id]->char_db[BLE_WIFIMATE_CHAR_NEGOTIATE_KEY_IDX].char_val_handle;
	if (handle) {
		if (client_attr_write(conn_id, ble_wifimate_client_id, GATT_WRITE_TYPE_REQ, handle, length,
							  data) == GAP_CAUSE_SUCCESS) {
			ret = true;
		}
	}

	if (data) {
		os_mem_free(data);
	}
	return ret;
}

int ble_wifimate_client_wifi_scan(uint8_t conn_id)
{
	uint8_t enable = true;
	uint16_t handle;
	uint16_t length = 0;
	bool ret = BT_AT_OK;

	if (conn_id > s_ble_wifimate_client_link_num || !s_ble_wifimate_db[conn_id]) {
		printf("%s: failed invalid conn_id %d\r\n", __func__, conn_id);
		return BT_AT_FAIL;
	}

	if (s_ble_wifimate_db[conn_id]->disc_state != DISC_DONE) {
		printf("[APP] BLE WifiMate client need discover service before write charac\r\n");
		return BT_AT_FAIL;
	}

	handle = s_ble_wifimate_db[conn_id]->char_db[BLE_WIFIMATE_CHAR_WIFI_SCAN_ENABLE_IDX].char_val_handle;
	if (handle) {
		length = BLE_WIFIMATE_CHAR_WIFI_SCAN_ENABLE_DATA_LEN;
		if (client_attr_write(conn_id, ble_wifimate_client_id, GATT_WRITE_TYPE_REQ, handle, length,
							  (uint8_t *)&enable) != GAP_CAUSE_SUCCESS) {
			ret = BT_AT_ERR_LOWER_STACK;
			printf("%s: Write fail! Please check\r\n", __func__);
		}
	}

	return ret;
}

static uint16_t ble_wifimate_write_wlconn_enable_segment(uint16_t value_handle, uint8_t conn_id)
{
	uint8_t *data = NULL;
	uint16_t ret = BLE_WIFIMATE_CLIENT_RECV_SUCCESS;
	uint16_t length = 0;
	int     remain = s_multi_write.total_len - s_multi_write.offset;

	if (!s_multi_write.flag) {
		printf("[APP] BLE WifiMate client no need write wifi connect enable charac.\r\n");
		return BLE_WIFIMATE_CLIENT_RECV_DONE;
	}

	if ((s_multi_write.seg_idx >= s_multi_write.seg_num) ||
		(remain <= 0)) {
		printf("[APP] BLE WifiMate client write wifi connect enable charac done.\r\n");
		ble_wifimate_char_multi_write_data_deinit();
		return BLE_WIFIMATE_CLIENT_RECV_DONE;
	}

	length = (remain < (s_mtu_size - 3 - 1)) ? (remain + 1) : (s_mtu_size - 3);
	data = (void *)os_mem_alloc(RAM_TYPE_DATA_ON, length);
	if (!data) {
		printf("[APP] BLE WifiMate client Write failed: can't alloc memory\r\n");
		ble_wifimate_char_multi_write_data_deinit();
		return BLE_WIFIMATE_CLIENT_RECV_FAIL;
	}
	memset(data, 0, length);
	memcpy(data, &s_multi_write.seg_idx, 1);
	memcpy(data + 1, s_multi_write.data + s_multi_write.offset, length - 1);
	s_multi_write.offset += length - 1;
	s_multi_write.seg_idx++;

	if (client_attr_write(conn_id, ble_wifimate_client_id, GATT_WRITE_TYPE_REQ,
							value_handle, length, data) != GAP_CAUSE_SUCCESS) {
		ble_wifimate_char_multi_write_data_deinit();
		ret = BLE_WIFIMATE_CLIENT_RECV_FAIL;
	}

	os_mem_free(data);
	return ret;
}

static int ble_wifimate_write_wlconn_enable_initial(uint8_t conn_id, wifi_conn_config_t *conn_info)
{
	uint8_t offset = 0;
	uint8_t *data = NULL;
	uint16_t handle;
	uint16_t length = 0;
	int ret = BT_AT_OK;

	if (conn_id > s_ble_wifimate_client_link_num || !s_ble_wifimate_db[conn_id]) {
		printf("%s: failed invalid conn_id %d\r\n", __func__, conn_id);
		return BT_AT_FAIL;
	}

	if (s_ble_wifimate_db[conn_id]->disc_state != DISC_DONE) {
		printf("[APP] BLE WifiMate client need discover service before write charac\r\n");
		return BT_AT_FAIL;
	}

	ret = ble_wifimate_char_multi_write_data_init(conn_info);
	if (!ret) {
		printf("[APP] BLE WifiMate client init multi write fail\r\n");
		return BT_AT_FAIL;
	}

	handle = s_ble_wifimate_db[conn_id]->char_db[BLE_WIFIMATE_CHAR_WIFI_CONNECT_ENABLE_IDX].char_val_handle;
	length = CHAR_WIFI_CONN_ENABLE_INITIAL_FIRST_ELE_LEN + \
			CHAR_WIFI_CONN_ENABLE_INITIAL_SECOND_ELE_LEN + \
			CHAR_WIFI_CONN_ENABLE_INITIAL_THIRD_ELE_LEN;
	data = (uint8_t *)os_mem_alloc(RAM_TYPE_DATA_ON, length);
	if (!data) {
		printf("[APP] BLE WifiMate client Write failed: can't alloc memory\r\n");
		ble_wifimate_char_multi_write_data_deinit();
		return BT_AT_ERR_NO_MEMORY;
	}

	memset(data, 0, length);

	memcpy(data, &s_multi_write.total_len, CHAR_WIFI_CONN_ENABLE_INITIAL_FIRST_ELE_LEN);
	offset = CHAR_WIFI_CONN_ENABLE_INITIAL_FIRST_ELE_LEN;

	memcpy(data + offset, &s_multi_write.seg_num, CHAR_WIFI_CONN_ENABLE_INITIAL_SECOND_ELE_LEN);
	offset += CHAR_WIFI_CONN_ENABLE_INITIAL_SECOND_ELE_LEN;

	memcpy(data + offset, &s_multi_write.checksum, CHAR_WIFI_CONN_ENABLE_INITIAL_THIRD_ELE_LEN);

	ret = client_attr_write(conn_id, ble_wifimate_client_id, GATT_WRITE_TYPE_REQ,
							handle, length, data);
	if (ret) {
		ble_wifimate_char_multi_write_data_deinit();
		ret = BT_AT_ERR_LOWER_STACK;
	}

	if (data) {
		os_mem_free(data);
	}
	return ret;
}

int ble_wifimate_client_wifi_connect(uint8_t conn_id, wifi_conn_config_t *conn_info)
{
	return ble_wifimate_write_wlconn_enable_initial(conn_id, conn_info);
}

/**
  * @brief  Used by application, to enable or disable the indication of peer server's characteristic.
  * @param[in]  conn_id connection ID.
  * @param[in]  indicate 0--disable the indication, 1--enable the indication.
  * @retval true send request to upper stack success.
  * @retval false send request to upper stack failed.
  */
bool ble_wifimate_client_enable_indicate_cccd(uint8_t conn_id, bool indicate, ble_wifimate_ind_type_e type)
{
	uint16_t char_index = BLE_WIFIMATE_CHAR_NUM;
	uint16_t handle;
	uint16_t length = 0;
	uint16_t cccd_bits;
	bool ret = false;

	if (conn_id > s_ble_wifimate_client_link_num || !s_ble_wifimate_db[conn_id]) {
		printf("%s: failed invalid conn_id %d\r\n", __func__, conn_id);
		return false;
	}

	switch(type) {
	case BLE_WIFIMATE_IND_WIFI_SCAN_INFO:
		char_index = BLE_WIFIMATE_CHAR_WIFI_SCAN_INFO_IDX;
		break;
	case BLE_WIFIMATE_IND_WIFI_CONNECT_STATE:
		char_index = BLE_WIFIMATE_CHAR_WIFI_CONNECT_STATE_IDX;
		break;
	default:
		break;
	}

	if (char_index >= BLE_WIFIMATE_CHAR_NUM) {
		printf("%s: invalid param char_index %d\r\n", __func__, char_index);
		return false;
	}

	handle = s_ble_wifimate_db[conn_id]->char_db[char_index].cccd_handle;
	if (handle) {
		length = sizeof(uint16_t);
		cccd_bits = indicate ? 2 : 0;
		if (client_attr_write(conn_id, ble_wifimate_client_id, GATT_WRITE_TYPE_REQ, handle, length,
							  (uint8_t *)&cccd_bits) == GAP_CAUSE_SUCCESS) {
			ret = true;
		} else {
			printf("%s: Request fail! Please check\r\n", __func__);
		}
	}

	return ret;
}

/**
  * @brief  Used by application, to start the discovery procedure of OTA server.
  * @param[in]  conn_id connection ID.
  * @retval true send request to upper stack success.
  * @retval false send request to upper stack failed.
  */
bool ble_wifimate_client_start_discovery(uint8_t conn_id)
{
	T_GAP_CAUSE ret = GAP_CAUSE_SUCCESS;

	if (conn_id > s_ble_wifimate_client_link_num || !s_ble_wifimate_db[conn_id]) {
		printf("%s: failed invalid conn_id %d\r\n", __func__, conn_id);
		return false;
	}

	s_ble_wifimate_db[conn_id]->disc_state = DISC_START;

	ret = client_by_uuid_srv_discovery(conn_id, ble_wifimate_client_id, BLE_WIFIMATE_UUID_SRV);
	if (ret != GAP_CAUSE_SUCCESS) {
		return false;
	}

	return true;
}

/**
  * @brief  Used internal, start the discovery of OTA characteristics.
  *         NOTE--user can offer this interface for application if required.
  * @retval true--send request to upper stack success.
  *         flase--send request to upper stack failed.
  */
bool ble_wifimate_client_start_all_char_discovery(uint8_t conn_id)
{
	T_GAP_CAUSE ret;

	if (conn_id > s_ble_wifimate_client_link_num || !s_ble_wifimate_db[conn_id]) {
		printf("%s: failed invalid conn_id %d\r\n", __func__, conn_id);
		return false;
	}

	ret = client_all_char_discovery(conn_id,
									ble_wifimate_client_id,
									s_ble_wifimate_db[conn_id]->start_handle,
								  	s_ble_wifimate_db[conn_id]->end_handle);

	if (ret == GAP_CAUSE_SUCCESS) {
		return true;
	}

	return false;
}

/**
  * @brief  Used internal, start the discovery of OTA characteristics descriptor.
  *         NOTE--user can offer this interface for application if required.
  * @retval true--send request to upper stack success.
  *         flase--send request to upper stack failed.
  */
bool ble_wifimate_client_start_all_char_desc_discovery(uint8_t conn_id)
{
	T_GAP_CAUSE ret;

	if (conn_id > s_ble_wifimate_client_link_num || !s_ble_wifimate_db[conn_id]) {
		printf("%s: failed invalid conn_id %d\r\n", __func__, conn_id);
		return false;
	}

	ret = client_all_char_descriptor_discovery(conn_id, 
												ble_wifimate_client_id, 
												s_ble_wifimate_db[conn_id]->start_handle,
												s_ble_wifimate_db[conn_id]->end_handle);

	if (ret == GAP_CAUSE_SUCCESS) {
		return true;
	}

	return false;
}

static void ble_wifimate_client_disc_state_cb(uint8_t conn_id,  T_DISCOVERY_STATE discovery_state)
{
	bool cb_flag = false;
	ble_wifimate_client_cb_data_t	cb_data = {0};
	cb_data.cb_type = BLE_WIFIMATE_CLIENT_CB_TYPE_DISC_STATE;

	if (conn_id > s_ble_wifimate_client_link_num || !s_ble_wifimate_db[conn_id]) {
		printf("%s: failed invalid conn_id %d\r\n", __func__, conn_id);
		return;
	}

	switch(discovery_state) {
	case DISC_STATE_SRV_DONE: {
		/* Indicate that service handle found. Start discover characteristic. */
		if ((s_ble_wifimate_db[conn_id]->start_handle != 0) &&
			(s_ble_wifimate_db[conn_id]->end_handle != 0)) {
			if (ble_wifimate_client_start_all_char_discovery(conn_id) == false) {
				s_ble_wifimate_db[conn_id]->disc_state = DISC_FAILED;
				cb_flag = true;
			}
		} else { //No ble wifimate handle found. Discover procedure complete.
			s_ble_wifimate_db[conn_id]->disc_state = DISC_FAILED;
			cb_flag = true;
		}
	}
	break;
	case DISC_STATE_CHAR_DONE: {
		if ((s_ble_wifimate_db[conn_id]->start_handle != 0) &&
			(s_ble_wifimate_db[conn_id]->end_handle != 0)) {

			if (ble_wifimate_client_start_all_char_desc_discovery(conn_id) == false) {
				s_ble_wifimate_db[conn_id]->disc_state = DISC_FAILED;
				cb_flag = true;
			}
		} else {
			s_ble_wifimate_db[conn_id]->disc_state = DISC_FAILED;
			cb_flag = true;
		}
	}
	break;
	case DISC_STATE_CHAR_DESCRIPTOR_DONE: {
		if ((s_ble_wifimate_db[conn_id]->start_handle != 0) &&
			(s_ble_wifimate_db[conn_id]->end_handle != 0)) {
			s_ble_wifimate_db[conn_id]->disc_state = DISC_DONE;
			cb_flag = true;
			ble_wifimate_client_enable_indicate_cccd(conn_id, true, BLE_WIFIMATE_IND_WIFI_SCAN_INFO);
		} else {
			s_ble_wifimate_db[conn_id]->disc_state = DISC_FAILED;
			cb_flag = true;
		}
	}
	break;
	default:
		break;
	}

	/* Send disconver state to application if needed. */
	if (cb_flag && p_ble_wifimate_client_cb) {
		cb_data.cb_content.disc_state = s_ble_wifimate_db[conn_id]->disc_state;
		(*p_ble_wifimate_client_cb)(ble_wifimate_client_id, conn_id, &cb_data);
	}
}

static void ble_wifimate_client_disc_result_cb(uint8_t conn_id,
										 T_DISCOVERY_RESULT_TYPE result_type,
										 T_DISCOVERY_RESULT_DATA result_data)
{
	uint16_t uuid = 0;

	APP_PRINT_INFO2("%s: result_type = %d", __func__, result_type);

	if (conn_id > s_ble_wifimate_client_link_num || !s_ble_wifimate_db[conn_id]) {
		APP_PRINT_INFO2("%s: failed invalid conn_id %d", __func__, conn_id);
		return;
	}

	switch (result_type) {
	case DISC_RESULT_SRV_DATA: {
		/* send service handle range to application. */
		s_ble_wifimate_db[conn_id]->start_handle = result_data.p_srv_disc_data->att_handle;
		s_ble_wifimate_db[conn_id]->end_handle = result_data.p_srv_disc_data->end_group_handle;
	}
	break;
	case DISC_RESULT_CHAR_UUID16: {
		/* When use clientAPI_AllCharDiscovery. */
		ble_wifimate_charac_db_t *char_db = NULL;
		uuid = result_data.p_char_uuid16_disc_data->uuid16;
		switch(uuid) {
		case BLE_WIFIMATE_UUID_CHAR_KEY_NEGOTIATE:
			char_db = &s_ble_wifimate_db[conn_id]->char_db[BLE_WIFIMATE_CHAR_NEGOTIATE_KEY_IDX];
			break;
		case BLE_WIFIMATE_UUID_CHAR_WIFI_SCAN_ENABLE:
			char_db = &s_ble_wifimate_db[conn_id]->char_db[BLE_WIFIMATE_CHAR_WIFI_SCAN_ENABLE_IDX];
			break;
		case BLE_WIFIMATE_UUID_CHAR_WIFI_SCAN_INFO:
			char_db = &s_ble_wifimate_db[conn_id]->char_db[BLE_WIFIMATE_CHAR_WIFI_SCAN_INFO_IDX];
			break;
		case BLE_WIFIMATE_UUID_CHAR_WIFI_CONNECT_ENABLE:
			char_db = &s_ble_wifimate_db[conn_id]->char_db[BLE_WIFIMATE_CHAR_WIFI_CONNECT_ENABLE_IDX];
			break;
		case BLE_WIFIMATE_UUID_CHAR_WIFI_CONNECT_STATE:
			char_db = &s_ble_wifimate_db[conn_id]->char_db[BLE_WIFIMATE_CHAR_WIFI_CONNECT_STATE_IDX];
			break;
		default:
			break;
		}
		if (char_db) {
			char_db->char_val_handle = result_data.p_char_uuid16_disc_data->value_handle;
			char_db->properties      = result_data.p_char_uuid16_disc_data->properties;
			char_db->uuid            = uuid;
		}
	}
	break;
	case DISC_RESULT_CHAR_DESC_UUID16: {
		/* When use clientAPI_AllCharDescriptorDiscovery. */
		if (result_data.p_char_desc_uuid16_disc_data->uuid16 == GATT_UUID_CHAR_CLIENT_CONFIG)
		{
			uint16_t desc_handle = result_data.p_char_desc_uuid16_disc_data->handle;
			if (desc_handle > s_ble_wifimate_db[conn_id]->char_db[BLE_WIFIMATE_CHAR_WIFI_SCAN_INFO_IDX].char_val_handle &&
				desc_handle < s_ble_wifimate_db[conn_id]->char_db[BLE_WIFIMATE_CHAR_WIFI_CONNECT_ENABLE_IDX].char_val_handle) {
				s_ble_wifimate_db[conn_id]->char_db[BLE_WIFIMATE_CHAR_WIFI_SCAN_INFO_IDX].cccd_handle = desc_handle;
			} else if (desc_handle > s_ble_wifimate_db[conn_id]->char_db[BLE_WIFIMATE_CHAR_WIFI_CONNECT_STATE_IDX].char_val_handle &&
					desc_handle <= s_ble_wifimate_db[conn_id]->end_handle) {
				s_ble_wifimate_db[conn_id]->char_db[BLE_WIFIMATE_CHAR_WIFI_CONNECT_STATE_IDX].cccd_handle = desc_handle;
			}
        }
	}
	break;
	default:
		APP_PRINT_INFO0("dfu_client_disc_result_cb: Invalid Discovery Result Type!");
		break;
	}
}

static void ble_wifimate_client_write_result_cb(uint8_t conn_id, T_GATT_WRITE_TYPE type, uint16_t handle,
										  uint16_t cause, uint8_t credits)
{
	uint16_t ret;
	bool cb_flag = false;
	ble_wifimate_client_cb_data_t cb_data;
	ble_wifimate_client_db_t 	  *conn_db = NULL;
	APP_PRINT_INFO4("ble_wifimate_client_write_result_cb: type=%d, handle=0x%x cause=%d, credits=%d",
					type, handle, cause, credits);

	if (conn_id > s_ble_wifimate_client_link_num || !s_ble_wifimate_db[conn_id]) {
		APP_PRINT_INFO2("%s: failed invalid conn_id %d", __func__, conn_id);
		return;
	}

	conn_db = s_ble_wifimate_db[conn_id];
	cb_data.cb_type = BLE_WIFIMATE_CLIENT_CB_TYPE_WRITE_RESULT;
	cb_data.cb_content.write_result.cause = cause;

	if (handle == conn_db->char_db[BLE_WIFIMATE_CHAR_WIFI_SCAN_INFO_IDX].cccd_handle) {
		cb_data.cb_content.write_result.type = BLE_WIFIMATE_WRITE_WIFI_SCAN_INFO_CCCD;
		ble_wifimate_client_enable_indicate_cccd(conn_id, true, BLE_WIFIMATE_IND_WIFI_CONNECT_STATE);
	} else if (handle == conn_db->char_db[BLE_WIFIMATE_CHAR_WIFI_CONNECT_STATE_IDX].cccd_handle) {
		key_negotiate_config_t config = {0};
		cb_data.cb_content.write_result.type = BLE_WIFIMATE_WRITE_WIFI_CONNECT_STATE_CCCD;

		if (!sp_encrypt) {
			APP_PRINT_ERROR1("%s: sp_encrypt null", __func__);
			return;
		}
		config.algorithm = sp_encrypt->algorithm_type;
		memcpy(config.key, sp_encrypt->key, sizeof(config.key));
		ble_wifimate_client_negotiate_key(conn_id, &config);
	} else if (handle == conn_db->char_db[BLE_WIFIMATE_CHAR_NEGOTIATE_KEY_IDX].char_val_handle) {
		cb_data.cb_content.write_result.type = BLE_WIFIMATE_WRITE_NEGOTIATE_KEY;
		cb_flag = true;
	} else if (handle == conn_db->char_db[BLE_WIFIMATE_CHAR_WIFI_SCAN_ENABLE_IDX].char_val_handle) {
		cb_data.cb_content.write_result.type = BLE_WIFIMATE_WRITE_WIFI_SCAN_ENABLE;
		cb_flag = true;
	} else if (handle == conn_db->char_db[BLE_WIFIMATE_CHAR_WIFI_CONNECT_ENABLE_IDX].char_val_handle) {
		ret = ble_wifimate_write_wlconn_enable_segment(handle, conn_id);
		if ((ret == BLE_WIFIMATE_CLIENT_RECV_DONE) || (ret == BLE_WIFIMATE_CLIENT_RECV_FAIL)) {
			cb_data.cb_content.write_result.type = BLE_WIFIMATE_WRITE_WIFI_CONNECT_ENABLE;
			cb_flag = true;
		}
	}

	/* Inform application the write result */
	if (cb_flag && p_ble_wifimate_client_cb) {
		(*p_ble_wifimate_client_cb)(ble_wifimate_client_id, conn_id, &cb_data);
	}
}

static void ble_wifimate_client_print_wifi_scan_info(void)
{
	uint8_t ap_num = 0;
	uint8_t ap_len = 0;
	uint32_t offset = 0;
	wifi_scan_info_t ap_info = {0};

	if (!s_multi_recv.flag || !s_multi_recv.data) {
		printf("[APP] BLE WiFiMate s_multi_recv invalid\r\n");
		return;
	}
	for (uint32_t idx = 0; idx < s_multi_recv.total_len; idx += ap_len) {
		memset(&ap_info, 0, sizeof(ap_info));

		memcpy(&ap_info.ssid_len, s_multi_recv.data + offset, sizeof(uint8_t));
		offset += sizeof(uint8_t);

		if (ap_info.ssid_len <= BLE_WIFIMATE_SSID_LEN_MAX) {
			memcpy(ap_info.ssid, s_multi_recv.data + offset, ap_info.ssid_len);
			offset += ap_info.ssid_len;
		} else {
			printf("[APP] BLE WiFiMate ssid length(%d) invalid\r\n", ap_info.ssid_len);
			break;
		}

		memcpy(&ap_info.rssi, s_multi_recv.data + offset, sizeof(int16_t));
		offset += sizeof(int16_t);

		memcpy(&ap_info.security, s_multi_recv.data + offset, sizeof(uint8_t));
		offset += sizeof(uint8_t);

		memcpy(&ap_info.channel, s_multi_recv.data + offset, sizeof(uint8_t));
		offset += sizeof(uint8_t);

		ap_len = ap_info.ssid_len + 5;

		++ap_num;

		printf("[APP] WiFi scan info: ssid=%s, rssi=%d, security=%d, channel=%d\r\n",
				ap_info.ssid, ap_info.rssi, ap_info.security, ap_info.channel);
#if defined(BT_AT_SYNC) && BT_AT_SYNC
		BT_AT_PRINT("+BTDEMO:ble_wifimate_configurator,wifi_scan,%s,%d,%d,%d\r\n",
					ap_info.ssid, ap_info.rssi, ap_info.security, ap_info.channel);
#endif
	}
}

static uint16_t ble_wifimate_client_receive_wifi_scan_info(uint8_t conn_id, uint16_t len, uint8_t *value)
{
	uint16_t ret = BLE_WIFIMATE_CLIENT_RECV_SUCCESS;
	if (!value) {
		printf("[APP] BLE WiFiMate indicate wifi_scan_info value NULL %d!\r\n");
		return BLE_WIFIMATE_CLIENT_RECV_FAIL;
	}

	if (!s_multi_recv.flag) {
		uint16_t offset = 0;
		uint16_t total_len;
		uint8_t seg_num;
		uint32_t checksum;

		if (len != 7) {
			printf("[APP] BLE WiFiMate indicate wifi_scan_info length invalid %d!\r\n", len);
			return BLE_WIFIMATE_CLIENT_RECV_FAIL;
		}

		memcpy(&total_len, value, sizeof(uint16_t));
		offset += sizeof(uint16_t);

		memcpy(&seg_num,   value + offset, sizeof(uint8_t));
		offset += sizeof(uint8_t);

		memcpy(&checksum,  value + offset, sizeof(uint32_t));

		if (seg_num > 0) {
			ble_wifimate_char_multi_recv_data_init(total_len, seg_num, checksum);
		} else {
			printf("[APP] BLE WiFiMate conn_id=%d wifi scan failed!\r\n", conn_id);
			return BLE_WIFIMATE_CLIENT_RECV_FAIL;
		}
	} else {
		uint32_t checksum;
		memcpy(&s_multi_recv.seg_idx, value, 1);
		memcpy(s_multi_recv.data + s_multi_recv.offset, value + 1, len - 1);
		s_multi_recv.offset += len - 1;

		if (s_multi_recv.total_len == s_multi_recv.offset) {
			checksum = ble_wifimate_client_checksum_cal(s_multi_recv.total_len, s_multi_recv.data);
			if (checksum == s_multi_recv.checksum) {
				printf("[APP] BLE WiFiMate wifi scan info checksum pass!\r\n");
				ble_wifimate_client_print_wifi_scan_info();
				ret = BLE_WIFIMATE_CLIENT_RECV_DONE;
			} else {
				ret = BLE_WIFIMATE_CLIENT_RECV_FAIL;
				printf("[APP] BLE WiFiMate wifi scan info checksum fail\r\n");
			}
			ble_wifimate_char_multi_recv_data_deinit();
#if defined(BT_AT_SYNC) && BT_AT_SYNC
			if (bt_at_sync_event_match_check(BT_AT_SYNC_CMD_TYPE_BLE_WIFIMATE_CONFIGURATOR_WIFI_SCAN)) {
				bt_at_sync_set_result((ret == BLE_WIFIMATE_CLIENT_RECV_DONE) ? BT_AT_OK : BT_AT_FAIL);
				bt_at_sync_sem_give();
			}
#endif
		}
	}

	return ret;
}

static bool ble_wifimate_client_receive_wifi_conn_state(uint8_t conn_id, uint16_t len, uint8_t *value)
{
	uint8_t wifi_state;
	uint8_t err_code;
	uint16_t conn_handle;

	if (!value) {
		printf("[APP] BLE WiFiMate indicate wifi_conn_state value NULL\r\n");
		return false;
	}

	if (len != 2) {
		printf("[APP] BLE WiFiMate indicate wifi_conn_state length invalid %d!\r\n", len);
		return false;
	}

	memcpy(&wifi_state, value, 1);
	memcpy(&err_code, value + 1, 1);
	conn_handle = le_get_conn_handle(conn_id);

	printf("[APP] BLE WiFiMate Client conn_handle=%d, wifi_state=%d, err_code=%d\r\n", conn_handle, wifi_state, err_code);
#if defined(BT_AT_SYNC) && BT_AT_SYNC
	BT_AT_PRINT("+BTDEMO:ble_wifimate_configurator,wifi_connect,%d,%d,%d\r\n", conn_handle, wifi_state, err_code);
	if (bt_at_sync_event_match_check(BT_AT_SYNC_CMD_TYPE_BLE_WIFIMATE_CONFIGURATOR_WIFI_CONNECT)) {
		bt_at_sync_set_result(err_code ? BT_AT_FAIL : BT_AT_OK);
		bt_at_sync_sem_give();
	}
#endif
	return true;
}

static T_APP_RESULT ble_wifimate_client_indicate_result_cb(uint8_t conn_id, bool notify, uint16_t handle,
														uint16_t value_size, uint8_t *p_value)
{
	(void)notify;
	uint16_t ret = 0;
	bool cb_flag = false;
	T_APP_RESULT app_result = APP_RESULT_SUCCESS;
	ble_wifimate_client_cb_data_t cb_data;

	memset(&cb_data, 0, sizeof(ble_wifimate_client_cb_data_t));
	if (conn_id > s_ble_wifimate_client_link_num || !s_ble_wifimate_db[conn_id]) {
		printf("%s: failed invalid conn_id %d\r\n", __func__, conn_id);
		return false;
	}

	if (handle == s_ble_wifimate_db[conn_id]->char_db[BLE_WIFIMATE_CHAR_WIFI_SCAN_INFO_IDX].char_val_handle) {
		ret = ble_wifimate_client_receive_wifi_scan_info(conn_id, value_size, p_value);
		if ((ret == BLE_WIFIMATE_CLIENT_RECV_FAIL) || (ret == BLE_WIFIMATE_CLIENT_RECV_DONE)) {
			cb_data.cb_content.ind_data.type = BLE_WIFIMATE_IND_WIFI_SCAN_INFO;
			cb_flag = true;
		}
	} else if (handle == s_ble_wifimate_db[conn_id]->char_db[BLE_WIFIMATE_CHAR_WIFI_CONNECT_STATE_IDX].char_val_handle) {
		ble_wifimate_client_receive_wifi_conn_state(conn_id, value_size, p_value);
		cb_data.cb_content.ind_data.type = BLE_WIFIMATE_IND_WIFI_CONNECT_STATE;
		cb_flag = true;
	}

	if (cb_flag) {
		/* Inform application the indicate result. */
		if (p_ble_wifimate_client_cb) {
			cb_data.cb_type = BLE_WIFIMATE_CLIENT_CB_TYPE_IND_RESULT;
			cb_data.cb_content.ind_data.data.value_size = value_size;
			cb_data.cb_content.ind_data.data.p_value = p_value;
			app_result = (*p_ble_wifimate_client_cb)(ble_wifimate_client_id, conn_id, &cb_data);
		}
	}

	return app_result;
}

static void ble_wifimate_client_database_free(uint8_t conn_id)
{
	if (conn_id > s_ble_wifimate_client_link_num) {
		return;
	}

	if (!s_ble_wifimate_db[conn_id]) {
		return;
	}

	os_mem_free(s_ble_wifimate_db[conn_id]);
	s_ble_wifimate_db[conn_id] = NULL;

	return;
}

static void ble_wifimate_client_disconnect_cb(uint8_t conn_id)
{
	if (conn_id > s_ble_wifimate_client_link_num) {
		printf("%s: failed invalid conn_id %d\r\n", __func__, conn_id);
		return;
	}

	ble_wifimate_char_multi_write_data_deinit();
	ble_wifimate_char_multi_recv_data_deinit();

	ble_wifimate_client_database_free(conn_id);
	return;
}

T_CLIENT_ID ble_wifimate_add_client(P_FUN_GENERAL_APP_CB app_cb, uint8_t link_num)
{
	if (link_num > BLE_WIFIMATE_CLIENT_MAX_LINKS)
	{
		APP_PRINT_ERROR1("ble_wifimate_add_client: invalid link_num %d", link_num);
		return 0xff;
	}

	if (false == client_register_spec_client_cb(&ble_wifimate_client_id, &ble_wifimate_client_cbs))
	{
		ble_wifimate_client_id = CLIENT_PROFILE_GENERAL_ID;
		APP_PRINT_ERROR0("ble_wifimate_add_client failed");
		return ble_wifimate_client_id;
	}

	APP_PRINT_INFO1("ble_wifimate_add_client: ble_wifimate_client %d", ble_wifimate_client_id);

	/* register callback for profile to inform application that some events happened. */
	p_ble_wifimate_client_cb = app_cb;
	s_ble_wifimate_client_link_num = link_num;

	return ble_wifimate_client_id;
}

void ble_wifimate_client_connect(uint8_t conn_id)
{
	if (conn_id > s_ble_wifimate_client_link_num) {
		return;
	}

	if (!s_ble_wifimate_db[conn_id]) {
		s_ble_wifimate_db[conn_id] = (ble_wifimate_client_db_t *)
									os_mem_alloc(RAM_TYPE_DATA_ON, sizeof(ble_wifimate_client_db_t));
	}

	if (!s_ble_wifimate_db[conn_id]) {
		printf("[APP] BLE WifiMate client alloc database fail\r\n");
		return;
	}
	memset(s_ble_wifimate_db[conn_id], 0, sizeof(ble_wifimate_client_db_t));
}

void ble_wifimate_client_disconnect(uint8_t conn_id)
{
	if (conn_id > s_ble_wifimate_client_link_num) {
		printf("%s: invalid conn_id %d", __func__, conn_id);
		return;
	}

	ble_wifimate_char_multi_write_data_deinit();
	ble_wifimate_char_multi_recv_data_deinit();

	ble_wifimate_client_database_free(conn_id);
}

void ble_wifimate_client_deinit(void)
{
	ble_wifimate_char_multi_write_data_deinit();
	ble_wifimate_char_multi_recv_data_deinit();

	for (uint8_t idx = 0; idx < BLE_WIFIMATE_CLIENT_MAX_LINKS; ++idx) {
		ble_wifimate_client_database_free(idx);
	}
}

#endif /* CONFIG_BLE_WIFIMATE_CONFIGURATOR */

