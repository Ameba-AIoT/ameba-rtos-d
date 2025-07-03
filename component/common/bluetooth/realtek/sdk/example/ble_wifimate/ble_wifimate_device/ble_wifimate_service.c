/**
*********************************************************************************************************
*               Copyright(c) 2025, Realtek Semiconductor Corporation. All rights reserved.
**********************************************************************************************************
*/
#include "platform_opts_bt.h"
#if defined(CONFIG_BLE_WIFIMATE_DEVICE) && CONFIG_BLE_WIFIMATE_DEVICE
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <trace_app.h>
#include <os_mem.h>
#include <gap.h>
#include <gap_conn_le.h>
#include <lwip_netconf.h>
#include "hal_crypto.h"
#include "wifi_conf.h"
#include "os_sync.h"
#include "os_task.h"
#include "os_sched.h"
#include <profile_server.h>
#include <wifi_structures.h>
#include <ble_wifimate_service.h>

#define BLE_WIFIMATE_MAX_WIFI_SCAN_AP_NUM               (64)

#define CHAR_WIFI_SCAN_INFO_INITIAL_LEN                 (7)
#define CHAR_WIFI_SCAN_INFO_INITIAL_FIRST_ELE_LEN       (2)
#define CHAR_WIFI_SCAN_INFO_INITIAL_SECOND_ELE_LEN      (1)
#define CHAR_WIFI_SCAN_INFO_INITIAL_THIRD_ELE_LEN       (4)

#define CHAR_WIFI_SCAN_INFO_SEG_FIRST_ELE_LEN           (1)
#define CHAR_WIFI_SCAN_INFO_SEG_THIRD_ELE_LEN           (2)
#define CHAR_WIFI_SCAN_INFO_SEG_FORTH_ELE_LEN           (1)
#define CHAR_WIFI_SCAN_INFO_SEG_FIFTH_ELE_LEN           (1)

#define BLE_WIFIMATE_DECRYPT_PASSWORD_LEN_MAX           (128)

enum ble_wifimate_server_state_e {
	BLE_WIFIMATE_SERVER_STATE_IDLE = 0,
	BLE_WIFIMATE_SERVER_STATE_CONNECT = 1,
	BLE_WIFIMATE_SERVER_STATE_WIFI_SCAN = 2,
	BLE_WIFIMATE_SERVER_STATE_WIFI_CONN = 3,
};

typedef struct {
	uint8_t                 ap_num;
	rtw_scan_result_t       ap_info[BLE_WIFIMATE_MAX_WIFI_SCAN_AP_NUM];
} ble_wifimate_wifi_scan_result_t;

T_SERVER_ID ble_wifimate_srv_id = 0;

/**<  Function pointer used to send event to application from ble wifimate profile. Initiated in ble_wifimate_add_service. */
static P_FUN_SERVER_GENERAL_CB pfn_ble_wifimate_service_cb = NULL;

/**< @brief  service definition.  */
static const T_ATTRIB_APPL ble_wifimate_attr_tbl[] =
{
	/* <<Primary Service>>: BLE WIFIMATE */
    {
        (ATTRIB_FLAG_VALUE_INCL | ATTRIB_FLAG_LE),  /* flags     */
        {                                           /* type_value */
            LO_WORD(GATT_UUID_PRIMARY_SERVICE),
            HI_WORD(GATT_UUID_PRIMARY_SERVICE),
            LO_WORD(BLE_WIFIMATE_UUID_SRV),      /* service UUID */
            HI_WORD(BLE_WIFIMATE_UUID_SRV)
        },
        UUID_16BIT_SIZE,                            /* bValueLen     */
        NULL,                                       /* p_value_context */
        GATT_PERM_READ                              /* permissions  */
    },
    /* <<Characteristic>>: WI-FI Key Negotiate */
     {
        ATTRIB_FLAG_VALUE_INCL,                     /* flags */
        {                                           /* type_value */
            LO_WORD(GATT_UUID_CHARACTERISTIC),
            HI_WORD(GATT_UUID_CHARACTERISTIC),
            GATT_CHAR_PROP_WRITE                    /* characteristic properties */
            /* characteristic UUID not needed here, is UUID of next attrib. */
        },
        1,                                          /* bValueLen */
        NULL,
        GATT_PERM_READ                              /* permissions */
    },
    {
        ATTRIB_FLAG_VALUE_APPL,                     /* flags */
        {                                          /* type_value */
            LO_WORD(BLE_WIFIMATE_UUID_CHAR_KEY_NEGOTIATE),
            HI_WORD(BLE_WIFIMATE_UUID_CHAR_KEY_NEGOTIATE)
        },
        0,                                          /* bValueLen */
        NULL,
        GATT_PERM_WRITE                             /* permissions */
    },
    /* <<Characteristic>>: WI-FI Scan Enable */
    {
        ATTRIB_FLAG_VALUE_INCL,                     /* flags */
        {                                           /* type_value */
            LO_WORD(GATT_UUID_CHARACTERISTIC),
            HI_WORD(GATT_UUID_CHARACTERISTIC),
            GATT_CHAR_PROP_WRITE                    /* characteristic properties */
            /* characteristic UUID not needed here, is UUID of next attrib. */
        },
        1,                                          /* bValueLen */
        NULL,
        GATT_PERM_READ                              /* permissions */
    },
    {
        ATTRIB_FLAG_VALUE_APPL,                     /* flags */
        {                                          /* type_value */
            LO_WORD(BLE_WIFIMATE_UUID_CHAR_WIFI_SCAN_ENABLE),
            HI_WORD(BLE_WIFIMATE_UUID_CHAR_WIFI_SCAN_ENABLE)
        },
        0,                                          /* bValueLen */
        NULL,
        GATT_PERM_WRITE                             /* permissions */
    },
    /* <<Characteristic>>: WI-FI Scan Info */
    {
        ATTRIB_FLAG_VALUE_INCL,                     /* flags */
        {                                           /* type_value */
            LO_WORD(GATT_UUID_CHARACTERISTIC),
            HI_WORD(GATT_UUID_CHARACTERISTIC),
            (GATT_CHAR_PROP_INDICATE)               /* characteristic properties */
            /* characteristic UUID not needed here, is UUID of next attrib. */
        },
        1,                                          /* bValueLen */
        NULL,
        GATT_PERM_READ                              /* permissions */
    },
    {
        ATTRIB_FLAG_VALUE_APPL,                     /* flags */
        {                                           /* type_value */
            LO_WORD(BLE_WIFIMATE_UUID_CHAR_WIFI_SCAN_INFO),
            HI_WORD(BLE_WIFIMATE_UUID_CHAR_WIFI_SCAN_INFO)
        },
        0,                                          /* bValueLen */
        NULL,
        GATT_PERM_NONE                              /* permissions */
    },
    /* client characteristic configuration */
    {
        ATTRIB_FLAG_VALUE_INCL | ATTRIB_FLAG_CCCD_APPL,                 /* flags */
        {                                         /* type_value */
            LO_WORD(GATT_UUID_CHAR_CLIENT_CONFIG),
            HI_WORD(GATT_UUID_CHAR_CLIENT_CONFIG),
            /* NOTE: this value has an instantiation for each client, a write to */
            /* this attribute does not modify this default value:                */
            LO_WORD(GATT_CLIENT_CHAR_CONFIG_DEFAULT), /* client char. config. bit field */
            HI_WORD(GATT_CLIENT_CHAR_CONFIG_DEFAULT)
        },
        2,                                          /* bValueLen */
        NULL,
        (GATT_PERM_READ | GATT_PERM_WRITE)          /* permissions */
    },
    /* <<Characteristic>>: WI-FI Connect Enable */
    {
        ATTRIB_FLAG_VALUE_INCL,                     /* flags */
        {                                           /* type_value */
            LO_WORD(GATT_UUID_CHARACTERISTIC),
            HI_WORD(GATT_UUID_CHARACTERISTIC),
            GATT_CHAR_PROP_WRITE /* characteristic properties */
            /* characteristic UUID not needed here, is UUID of next attrib. */
        },
        1,                                          /* bValueLen */
        NULL,
        GATT_PERM_READ                              /* permissions */
    },
    {
        ATTRIB_FLAG_VALUE_APPL,                     /* flags */
        {                                          /* type_value */
            LO_WORD(BLE_WIFIMATE_UUID_CHAR_WIFI_CONNECT_ENABLE),
            HI_WORD(BLE_WIFIMATE_UUID_CHAR_WIFI_CONNECT_ENABLE)
        },
        0,                                          /* bValueLen */
        NULL,
        GATT_PERM_WRITE                             /* permissions */
    },
    /* <<Characteristic>>: WI-FI Connect State */
    {
        ATTRIB_FLAG_VALUE_INCL,                     /* flags */
        {                                           /* type_value */
            LO_WORD(GATT_UUID_CHARACTERISTIC),
            HI_WORD(GATT_UUID_CHARACTERISTIC),
            (GATT_CHAR_PROP_INDICATE)               /* characteristic properties */
            /* characteristic UUID not needed here, is UUID of next attrib. */
        },
        1,                                          /* bValueLen */
        NULL,
        GATT_PERM_READ                              /* permissions */
    },
    {
        ATTRIB_FLAG_VALUE_APPL,                     /* flags */
        {                                           /* type_value */
            LO_WORD(BLE_WIFIMATE_UUID_CHAR_WIFI_CONNECT_STATE),
            HI_WORD(BLE_WIFIMATE_UUID_CHAR_WIFI_CONNECT_STATE)
        },
        0,                                          /* bValueLen */
        NULL,
        GATT_PERM_NONE                              /* permissions */
    },
    /* client characteristic configuration */
    {
        ATTRIB_FLAG_VALUE_INCL | ATTRIB_FLAG_CCCD_APPL,                 /* flags */
        {                                         /* type_value */
            LO_WORD(GATT_UUID_CHAR_CLIENT_CONFIG),
            HI_WORD(GATT_UUID_CHAR_CLIENT_CONFIG),
            /* NOTE: this value has an instantiation for each client, a write to */
            /* this attribute does not modify this default value:                */
            LO_WORD(GATT_CLIENT_CHAR_CONFIG_DEFAULT), /* client char. config. bit field */
            HI_WORD(GATT_CLIENT_CHAR_CONFIG_DEFAULT)
        },
        2,                                          /* bValueLen */
        NULL,
        (GATT_PERM_READ | GATT_PERM_WRITE)          /* permissions */
    },
};

static uint16_t s_mtu_size = 23;
static uint8_t s_cccd_ind_en_wifi_scan = 0;
static uint8_t s_cccd_ind_en_wifi_conn = 0;
static uint8_t s_ble_wifimate_state = 0;
static ble_wifimate_encrypt_decrypt_t s_decrypt = {0};

static ble_wifimate_char_send_data_t s_multi_indicate = {0};
static ble_wifimate_char_recv_data_t s_multi_recv = {0};

static void *s_wifi_scan_sema = NULL;

static T_APP_RESULT ble_wifimate_attr_write_cb(uint8_t conn_id, T_SERVER_ID service_id,
                                        uint16_t attrib_index, T_WRITE_TYPE write_type, uint16_t length, uint8_t *p_value,
                                        P_FUN_WRITE_IND_POST_PROC *p_write_ind_post_proc);
static void ble_wifimate_cccd_update_cb(uint8_t conn_id, T_SERVER_ID service_id, uint16_t index,
                                     uint16_t cccbits);
/**
 * @brief BLE WiFiMate Service Callbacks.
*/
static const T_FUN_GATT_SERVICE_CBS ble_wifimate_cbs =
{
	NULL,  //Read callback function pointer
	ble_wifimate_attr_write_cb, //Write callback function pointer
	ble_wifimate_cccd_update_cb //CCCD update callback function pointer
};

void ble_wifimate_server_set_mtu_size(uint16_t mtu_size)
{
	s_mtu_size = mtu_size;
}

static uint32_t ble_wifimate_server_checksum_cal(uint16_t len, uint8_t *data)
{
	uint32_t checksum = 0;
	for (uint16_t i = 0; i < len; ++i) {
		checksum += *(data + i);
	}

	return checksum;
}

static bool ble_wifimate_char_multi_indicate_data_init(ble_wifimate_wifi_scan_result_t *wifi_scan_result)
{
	uint32_t offset = 0;
	uint32_t total_len = 0;
	struct rtw_scan_result *ap_info = NULL;

	if (!wifi_scan_result) {
		return false;
	}

	if (s_multi_indicate.data) {
		os_mem_free((void *)s_multi_indicate.data);
	}

	for (uint8_t i = 0; i < wifi_scan_result->ap_num; ++i) {
		total_len += CHAR_WIFI_SCAN_INFO_SEG_FIRST_ELE_LEN + wifi_scan_result->ap_info[i].SSID.len + \
					 CHAR_WIFI_SCAN_INFO_SEG_THIRD_ELE_LEN + CHAR_WIFI_SCAN_INFO_SEG_FORTH_ELE_LEN + \
					 CHAR_WIFI_SCAN_INFO_SEG_FIFTH_ELE_LEN;
	}

	memset(&s_multi_indicate, 0, sizeof(s_multi_indicate));
	s_multi_indicate.flag = true;
	s_multi_indicate.total_len = total_len;
	s_multi_indicate.seg_num = (uint8_t)ceil((double)total_len / (s_mtu_size - 3 - 1));
	s_multi_indicate.data = (void *)os_mem_alloc(RAM_TYPE_DATA_ON, total_len);
	if (!s_multi_indicate.data) {
		printf("[APP] BLE WifiMate server indicate failed: can't alloc memory\r\n");
		memset(&s_multi_indicate, 0, sizeof(s_multi_indicate));
		return false;
	}
	memset(s_multi_indicate.data, 0, total_len);

	for (uint8_t i = 0; i < wifi_scan_result->ap_num; ++i) {
		ap_info = &wifi_scan_result->ap_info[i];

		memcpy(s_multi_indicate.data + offset, &ap_info->SSID.len, CHAR_WIFI_SCAN_INFO_SEG_FIRST_ELE_LEN);
		offset += CHAR_WIFI_SCAN_INFO_SEG_FIRST_ELE_LEN;

		memcpy(s_multi_indicate.data + offset, ap_info->SSID.val, ap_info->SSID.len);
		offset += ap_info->SSID.len;

		memcpy(s_multi_indicate.data + offset, &ap_info->signal_strength, CHAR_WIFI_SCAN_INFO_SEG_THIRD_ELE_LEN);
		offset += CHAR_WIFI_SCAN_INFO_SEG_THIRD_ELE_LEN;

		memcpy(s_multi_indicate.data + offset, &ap_info->security, CHAR_WIFI_SCAN_INFO_SEG_FORTH_ELE_LEN);
		offset += CHAR_WIFI_SCAN_INFO_SEG_FORTH_ELE_LEN;

		memcpy(s_multi_indicate.data + offset, &ap_info->channel, CHAR_WIFI_SCAN_INFO_SEG_FIFTH_ELE_LEN);
		offset += CHAR_WIFI_SCAN_INFO_SEG_FIFTH_ELE_LEN;
	}

	s_multi_indicate.checksum = ble_wifimate_server_checksum_cal(s_multi_indicate.total_len, s_multi_indicate.data);

	return true;
}

static void ble_wifimate_char_multi_indicate_data_deinit(void)
{
	if (s_multi_indicate.data) {
		os_mem_free((void *)s_multi_indicate.data);
	}
	memset(&s_multi_indicate, 0, sizeof(s_multi_indicate));
}

static bool ble_wifimate_char_multi_recv_data_init(uint16_t total_len, uint8_t seg_num, uint32_t checksum)
{
	if (s_multi_recv.data) {
		os_mem_free((void *)s_multi_recv.data);
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
		printf("[BLE WifiMate server] init multi indication receive data: can't alloc memory\r\n");
		memset(&s_multi_recv, 0, sizeof(s_multi_recv));
		return false;
	}

	return true;
}

static void ble_wifimate_char_multi_recv_data_deinit(void)
{
	if (s_multi_recv.data) {
		os_mem_free((void *)s_multi_recv.data);
	}
	memset(&s_multi_recv, 0, sizeof(s_multi_recv));
}

/**
  * @brief send indication of transfer indicate characteristic value.
  *
  * @param[in] conn_id           connection id
  * @param[in] service_id        service ID of service.
  * @param[in] p_value           characteristic value to notify
  * @param[in] length            characteristic value length to notify
  * @return notification action result
  * @retval 1 true
  * @retval 0 false
  */
bool ble_wifimate_send_indicate(uint8_t conn_id, T_SERVER_ID service_id, uint16_t index, void *p_value,
                                       uint16_t length)
{
    APP_PRINT_INFO0("ble_wifimate_send_indicate");
    // send indication to client
    return server_send_data(conn_id, service_id, index, p_value,
                            length, GATT_PDU_TYPE_ANY);
}

/**
 * @brief update CCCD bits from stack.
 *
 * @param conn_id           connection id.
 * @param service_id          Service ID.
 * @param index          Attribute index of characteristic data.
 * @param cccbits         CCCD bits from stack.
 * @return None
*/
static void ble_wifimate_cccd_update_cb(uint8_t conn_id, T_SERVER_ID service_id, uint16_t index,
                                     uint16_t cccbits)
{
	APP_PRINT_INFO2("bt_ota_cccd_update_cb: index %d, cccbits 0x%x", index, cccbits);

	bool is_handle = false;
	BLE_WIFIMATE_CALLBACK_DATA callback_data;
	callback_data.conn_id = conn_id;
	callback_data.msg_type = SERVICE_CALLBACK_TYPE_INDIFICATION_NOTIFICATION;

    if (index == BLE_WIFIMATE_WIFI_SCAN_INFO_CCCD_INDEX) {
        if (cccbits & GATT_CLIENT_CHAR_CONFIG_INDICATE) {
			callback_data.msg_data.notify_indicate_index = BLE_WIFIMATE_INDICATE_ENABLE;
            s_cccd_ind_en_wifi_scan = 1;
		} else {
			callback_data.msg_data.notify_indicate_index = BLE_WIFIMATE_INDICATE_DISABLE;
            s_cccd_ind_en_wifi_scan = 0;
		}
        is_handle = true;
    } else if (index == BLE_WIFIMATE_WIFI_CONNECT_STATE_CCCD_INDEX) {
		if (cccbits & GATT_CLIENT_CHAR_CONFIG_INDICATE) {
			callback_data.msg_data.notify_indicate_index = BLE_WIFIMATE_INDICATE_ENABLE;
            s_cccd_ind_en_wifi_conn = 1;
		} else {
			callback_data.msg_data.notify_indicate_index = BLE_WIFIMATE_INDICATE_DISABLE;
            s_cccd_ind_en_wifi_conn = 0;
		}
		is_handle = true;
    } else {
		printf("%s: invalid index 0x%02x\r\n", __func__, index);
	}

	if (pfn_ble_wifimate_service_cb && (is_handle == true)) {
		pfn_ble_wifimate_service_cb(service_id, (void *)&callback_data);
	}
}

static rtw_result_t scan_result_handler(rtw_scan_handler_result_t* scan_result)
{
	ble_wifimate_wifi_scan_result_t* wifimate_scan_result = (ble_wifimate_wifi_scan_result_t*) scan_result->user_data;

	if (scan_result->scan_complete != RTW_TRUE) {
		if (wifimate_scan_result->ap_num < BLE_WIFIMATE_MAX_WIFI_SCAN_AP_NUM)
		{
			rtw_scan_result_t* record = &scan_result->ap_details;
			memcpy(&(wifimate_scan_result->ap_info[wifimate_scan_result->ap_num]), record, sizeof(rtw_scan_result_t));
			wifimate_scan_result->ap_num++;
		}
	} else{
        if (s_wifi_scan_sema) {
            os_sem_give(s_wifi_scan_sema);
        }
	}
	return RTW_SUCCESS;
}

static bool ble_wifimate_server_send_wifi_scan_info_initial(uint8_t conn_id)
{
	uint16_t offset = 0;
	bool ret = false;
	uint8_t ind_data[CHAR_WIFI_SCAN_INFO_INITIAL_LEN] = {0};

	if (s_cccd_ind_en_wifi_scan) {
		uint16_t len = CHAR_WIFI_SCAN_INFO_INITIAL_FIRST_ELE_LEN + \
					   CHAR_WIFI_SCAN_INFO_INITIAL_SECOND_ELE_LEN + \
					   CHAR_WIFI_SCAN_INFO_INITIAL_THIRD_ELE_LEN;

		memcpy(ind_data, &s_multi_indicate.total_len, CHAR_WIFI_CONN_ENABLE_INITIAL_FIRST_ELE_LEN);
		offset = CHAR_WIFI_CONN_ENABLE_INITIAL_FIRST_ELE_LEN;

		memcpy(ind_data + offset, &s_multi_indicate.seg_num, CHAR_WIFI_CONN_ENABLE_INITIAL_SECOND_ELE_LEN);
		offset += CHAR_WIFI_CONN_ENABLE_INITIAL_SECOND_ELE_LEN;

		memcpy(ind_data + offset, &s_multi_indicate.checksum, CHAR_WIFI_CONN_ENABLE_INITIAL_THIRD_ELE_LEN);

        ret = ble_wifimate_send_indicate(conn_id, ble_wifimate_srv_id, BLE_WIFIMATE_WIFI_SCAN_INFO_VAL_INDEX,
                                        ind_data, len);
        if (ret != true) {
			ble_wifimate_char_multi_indicate_data_deinit();
			printf("%s: indicate error 0x%02x\r\n", __func__, ret);
		}

	}

	return ret;
}

bool ble_wifimate_server_send_wifi_scan_info_segment(uint8_t conn_id)
{
	int  remain = s_multi_indicate.total_len - s_multi_indicate.offset;
	bool ret;
	uint8_t *ind_data = NULL;

	if (!s_multi_indicate.flag) {
		printf("[APP] BLE WifiMate server no need indicate wifi scan info charac.\r\n");
		return true;
	}

	if ((s_multi_indicate.seg_idx >= s_multi_indicate.seg_num) ||
		(remain <= 0)) {
		printf("[APP] BLE WifiMate server no need indicate wifi scan info charac done.\r\n");
		ble_wifimate_char_multi_indicate_data_deinit();
		return true;
	}

	if (s_cccd_ind_en_wifi_scan) {
		uint16_t len = (remain < (s_mtu_size - 3 - 1)) ? (remain + 1) : (s_mtu_size - 3);

		ind_data = (void *)os_mem_alloc(RAM_TYPE_DATA_ON, len);
		if (!ind_data) {
			printf("[APP] BLE WifiMate server indicate failed: can't alloc memory\r\n");
			ble_wifimate_char_multi_indicate_data_deinit();
			return false;
		}
		/* set seg_idx */
		memcpy(ind_data, &s_multi_indicate.seg_idx, 1);
		/* set data segment */
		memcpy(ind_data + 1, s_multi_indicate.data + s_multi_indicate.offset, len - 1);
		s_multi_indicate.offset += len - 1;
		s_multi_indicate.seg_idx++;

		APP_PRINT_INFO5("%s: indicate seg_idx=%d offest=%u total_len=%u len=%d\r\n", __func__,
				s_multi_indicate.seg_idx, s_multi_indicate.offset, s_multi_indicate.total_len, len);
		//BT_DUMPD("[APP] GATTS indicate event:\r\n", ind_data, ind_param.len);

        ret = ble_wifimate_send_indicate(conn_id, ble_wifimate_srv_id, BLE_WIFIMATE_WIFI_SCAN_INFO_VAL_INDEX,
                                ind_data, len);
		if (ret != true) {
			ble_wifimate_char_multi_indicate_data_deinit();
			printf("%s: indicate error 0x%02x\r\n", __func__, ret);
		}

		os_mem_free(ind_data);
	}

	return ret;
}

static uint32_t ble_wifimate_convert_wifi_conn_security(uint8_t security)
{
	uint32_t dest =  BWM_SECURITY_UNKNOWN;

	dest = (security == BWM_SECURITY_OPEN) ? RTW_SECURITY_OPEN :
		   (security == BWM_SECURITY_WEP_PSK) ? RTW_SECURITY_WEP_PSK :
		   (security == BWM_SECURITY_WPA_TKIP_PSK) ? RTW_SECURITY_WPA_TKIP_PSK :
		   (security == BWM_SECURITY_WPA_AES_PSK) ? RTW_SECURITY_WPA_AES_PSK :
		   (security == BWM_SECURITY_WPA_MIXED_PSK) ? RTW_SECURITY_WPA_MIXED_PSK :
		   (security == BWM_SECURITY_WPA2_AES_PSK) ?  RTW_SECURITY_WPA2_AES_PSK :
		   (security == BWM_SECURITY_WPA2_TKIP_PSK) ? RTW_SECURITY_WPA2_TKIP_PSK :
		   (security == BWM_SECURITY_WPA2_MIXED_PSK) ? RTW_SECURITY_WPA2_MIXED_PSK :
		   (security == BWM_SECURITY_WPA_WPA2_TKIP_PSK) ? RTW_SECURITY_WPA_WPA2_TKIP_PSK :
		   (security == BWM_SECURITY_WPA_WPA2_AES_PSK) ? RTW_SECURITY_WPA_WPA2_AES_PSK :
		   (security == BWM_SECURITY_WPA_WPA2_MIXED_PSK) ? RTW_SECURITY_WPA_WPA2_MIXED_PSK :
#ifdef CONFIG_SAE_SUPPORT
		   (security == BWM_SECURITY_WPA3_AES_PSK) ? RTW_SECURITY_WPA3_AES_PSK :
		   (security == BWM_SECURITY_WPA2_WPA3_MIXED) ?  RTW_SECURITY_WPA2_WPA3_MIXED :
#endif
#ifdef CONFIG_OWE_SUPPORT
		   (security == BWM_SECURITY_WPA3_OWE) ? RTW_SECURITY_WPA3_OWE :
#endif
		   BWM_SECURITY_WEP_PSK;

	return dest;
}

static uint8_t ble_wifimate_convert_wifi_scan_security(uint32_t security)
{
	uint8_t dest =  BWM_SECURITY_UNKNOWN;

	dest = (security == RTW_SECURITY_OPEN) ? BWM_SECURITY_OPEN :
		   (security == RTW_SECURITY_WEP_PSK) ? BWM_SECURITY_WEP_PSK :
		   (security == RTW_SECURITY_WPA_TKIP_PSK) ? BWM_SECURITY_WPA_TKIP_PSK :
		   (security == RTW_SECURITY_WPA_AES_PSK) ? BWM_SECURITY_WPA_AES_PSK :
		   (security == RTW_SECURITY_WPA_MIXED_PSK) ? BWM_SECURITY_WPA_MIXED_PSK :
		   (security == RTW_SECURITY_WPA2_AES_PSK) ? BWM_SECURITY_WPA2_AES_PSK :
		   (security == RTW_SECURITY_WPA2_TKIP_PSK) ? BWM_SECURITY_WPA2_TKIP_PSK :
		   (security == RTW_SECURITY_WPA2_MIXED_PSK) ? BWM_SECURITY_WPA2_MIXED_PSK :
		   (security == RTW_SECURITY_WPA_WPA2_TKIP_PSK) ? BWM_SECURITY_WPA_WPA2_TKIP_PSK :
		   (security == RTW_SECURITY_WPA_WPA2_AES_PSK) ? BWM_SECURITY_WPA_WPA2_AES_PSK :
		   (security == RTW_SECURITY_WPA_WPA2_MIXED_PSK) ? BWM_SECURITY_WPA_WPA2_MIXED_PSK :
		   (security == (RTW_SECURITY_WPA_TKIP_PSK | ENTERPRISE_ENABLED)) ? BWM_SECURITY_WPA_TKIP_PSK :
		   (security == (RTW_SECURITY_WPA_AES_PSK | ENTERPRISE_ENABLED)) ? BWM_SECURITY_WPA_AES_PSK :
		   (security == (RTW_SECURITY_WPA_MIXED_PSK | ENTERPRISE_ENABLED)) ? BWM_SECURITY_WPA_MIXED_PSK :
		   (security == (RTW_SECURITY_WPA2_TKIP_PSK | ENTERPRISE_ENABLED)) ? BWM_SECURITY_WPA2_TKIP_PSK :
		   (security == (RTW_SECURITY_WPA2_AES_PSK | ENTERPRISE_ENABLED)) ? BWM_SECURITY_WPA2_AES_PSK :
		   (security == (RTW_SECURITY_WPA2_MIXED_PSK | ENTERPRISE_ENABLED)) ? BWM_SECURITY_WPA2_MIXED_PSK :
		   (security == (RTW_SECURITY_WPA_WPA2_TKIP_PSK | ENTERPRISE_ENABLED)) ? BWM_SECURITY_WPA_WPA2_TKIP_PSK :
		   (security == (RTW_SECURITY_WPA_WPA2_AES_PSK | ENTERPRISE_ENABLED)) ? BWM_SECURITY_WPA_WPA2_AES_PSK :
		   (security == (RTW_SECURITY_WPA_WPA2_MIXED_PSK | ENTERPRISE_ENABLED)) ? BWM_SECURITY_WPA_WPA2_MIXED_PSK :
#ifdef CONFIG_SAE_SUPPORT
		   (security == RTW_SECURITY_WPA3_AES_PSK) ? BWM_SECURITY_WPA3_AES_PSK :
		   (security == RTW_SECURITY_WPA2_WPA3_MIXED) ? BWM_SECURITY_WPA2_WPA3_MIXED :
		   (security == (WPA3_SECURITY | ENTERPRISE_ENABLED)) ? BWM_SECURITY_WPA3_AES_PSK :
#endif
#ifdef CONFIG_OWE_SUPPORT
		   (security == RTW_SECURITY_WPA3_OWE) ? BWM_SECURITY_WPA3_OWE :
#endif
		   BWM_SECURITY_UNKNOWN;

	return dest;
}

static bool ble_wifimate_server_write_wifi_scan_hdl(uint8_t conn_id, uint16_t len, uint8_t *data)
{
	int ret = -1;
	uint8_t enable = 0;
	ble_wifimate_wifi_scan_result_t *wifi_scan_result = NULL;

	if (len != 1 || !data) {
		APP_PRINT_ERROR0("[APP] BLE WifiMate server wifi scan length unmatch\r\n");
		return false;
	}

	enable = *data;
	if (!enable) {
		APP_PRINT_ERROR0("[APP] BLE WifiMate server wifi scan disable\r\n");
		return false;
	}

	wifi_scan_result = (void *)os_mem_alloc(RAM_TYPE_DATA_ON, sizeof(ble_wifimate_wifi_scan_result_t));
	if (!wifi_scan_result) {
		APP_PRINT_ERROR0("[APP] BLE WifiMate server can't alloc memory for wifi scan result\r\n");
		return false;
	}
	memset(wifi_scan_result, 0, sizeof(ble_wifimate_wifi_scan_result_t));

    os_sem_create(&s_wifi_scan_sema, 0, 1);
	wifi_scan_result->ap_num = 0;

    ret = wifi_scan_networks(scan_result_handler,(void*) wifi_scan_result);
	if(ret != RTW_SUCCESS) {
		printf("wifi scan failed (%d)\r\n",ret);
        os_mem_free(wifi_scan_result);
		return false;
	}
    os_sem_take(s_wifi_scan_sema, 0xFFFFFFFF);
	os_sem_delete(s_wifi_scan_sema);
	s_wifi_scan_sema = NULL;

    for (uint32_t i = 0; i < BLE_WIFIMATE_MAX_WIFI_SCAN_AP_NUM; i++) {
        wifi_scan_result->ap_info[i].SSID.val[wifi_scan_result->ap_info[i].SSID.len] = 0;
        wifi_scan_result->ap_info[i].security = ble_wifimate_convert_wifi_scan_security(wifi_scan_result->ap_info[i].security);
    }

    ret = ble_wifimate_char_multi_indicate_data_init(wifi_scan_result);
    if (wifi_scan_result) {
        os_mem_free(wifi_scan_result);
    }
    if (ret == false) {
        ble_wifimate_char_multi_indicate_data_deinit();
        return ret;
    }
    return ble_wifimate_server_send_wifi_scan_info_initial(conn_id);
}

static bool ble_wifimate_server_key_negotiate_hdl(uint16_t len, uint8_t *data)
{
	uint8_t bt_addr[6] = {0};

	if (!data) {
		return false;
	}

	if (len != BLE_WIFIMATE_CHAR_NOGOTIATE_KEY_LEN) {
		APP_PRINT_ERROR1("[APP] BLE WiFiMate server write nogotiate key length unmatch:%d\r\n", len);
		return false;
	}

	memset(&s_decrypt, 0, sizeof(s_decrypt));
	memcpy(&s_decrypt.algorithm_type, data, CHAR_NEGOTIATE_KEY_FIRST_ELE_LEN);
	memcpy(&s_decrypt.key, data + CHAR_NEGOTIATE_KEY_FIRST_ELE_LEN, CHAR_NEGOTIATE_KEY_SECOND_ELE_LEN);

	if (s_decrypt.algorithm_type >= BLE_WIFIMATE_ENCRYPT_DECRYPT_ALGO_NUM) {
		APP_PRINT_ERROR1("[APP] BLE WiFiMate server does not support encryption and decryption algorithms type=%d.\r\n", s_decrypt.algorithm_type);
		memset(&s_decrypt, 0, sizeof(s_decrypt));
		return false;
	}

	if (s_decrypt.algorithm_type != BLE_WIFIMATE_ENCRYPT_DECRYPT_ALGO_NONE) {
        if (gap_get_param(GAP_PARAM_BD_ADDR, bt_addr) != GAP_CAUSE_SUCCESS) {
            return false;
        }

		s_decrypt.key[BLE_WIFIMATE_ENCRYPT_DECRYPT_KEY_LEN - 3] = bt_addr[2];
		s_decrypt.key[BLE_WIFIMATE_ENCRYPT_DECRYPT_KEY_LEN - 2] = bt_addr[1];
		s_decrypt.key[BLE_WIFIMATE_ENCRYPT_DECRYPT_KEY_LEN - 1] = bt_addr[0];
	}

	printf("[APP] BLE WiFiMate server negotiate key procedure: algorithm=%d, \
			key=%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x\r\n",
			s_decrypt.algorithm_type,
			s_decrypt.key[0], s_decrypt.key[1], s_decrypt.key[2], s_decrypt.key[3],
			s_decrypt.key[4], s_decrypt.key[5], s_decrypt.key[6], s_decrypt.key[7],
			s_decrypt.key[8], s_decrypt.key[9], s_decrypt.key[10], s_decrypt.key[11],
			s_decrypt.key[12], s_decrypt.key[13], s_decrypt.key[14], s_decrypt.key[15]);

	return true;
}

static uint8_t ble_wifimate_wifi_conn_result_to_bwm_errcode(int wifi_res)
{
	uint8_t err_code = 0;

	switch (wifi_res) {
	case RTW_SUCCESS:
		err_code = BWM_OK;
		break;
	case RTW_BADARG:
		err_code = BWM_ERR_PARAM_INVALID;
		break;
	case RTW_BUSY:
		err_code = BWM_ERR_BUSY;
		break;
	case RTW_NOMEM:
		err_code = BWM_ERR_NOMEM;
		break;
	case RTW_TIMEOUT:
		err_code = BWM_ERR_TIMEOUT;
		break;
	case RTW_INVALID_KEY:
		err_code = BWM_ERR_PASSWORD_WRONG;
		break;
	case RTW_DOES_NOT_EXIST:
		err_code = BWM_ERR_AP_NOT_FOUND;
		break;
	case RTW_NOT_AUTHENTICATED:
		err_code = BWM_ERR_AUTH_FAIL;
		break;
	case RTW_NOTASSOCIATED:
		err_code = BWM_ERR_ASSOC_FAIL;
		break;
	default:
		err_code = BWM_ERR_ERROR;
		break;
	}

	return err_code;
}

static uint16_t ble_wifimate_server_indicate_wifi_conn_state(uint8_t conn_id, uint8_t state, uint8_t err_code)
{
	bool ret;
    uint16_t length;
	uint8_t ind_data[BLE_WIFIMATE_CHAR_WIFI_CONNECT_STATE_LEN] = {0};

	if (s_cccd_ind_en_wifi_conn) {
		ind_data[0] = state;
		ind_data[1] = err_code;

		length = BLE_WIFIMATE_CHAR_WIFI_CONNECT_STATE_LEN;
        ret = ble_wifimate_send_indicate(conn_id, ble_wifimate_srv_id, BLE_WIFIMATE_WIFI_CONNECT_STATE_VAL_INDEX, 
                                        ind_data, length);
		if (!ret) {
			printf("%s: indicate error 0x%02x\r\n", __func__, ret);
		}
		printf("%s: state=%d err_code=%d length=%d\r\n", __func__, state, err_code, length);
	}

	return ret;
}

static bool ble_wifimate_wifi_connect(uint8_t conn_id, wifi_conn_config_t *conn_config)
{
	int ret, mode;
	struct rtw_network_info wifi;

	if (!conn_config) {
		return false;
	}

	memset(&wifi, 0, sizeof(wifi));
	wifi.ssid.len = conn_config->ssid_len;
	memcpy(wifi.ssid.val, conn_config->ssid, wifi.ssid.len);
	wifi.password_len = conn_config->password_len;
	wifi.password = conn_config->password;
	wifi.security_type = ble_wifimate_convert_wifi_conn_security(conn_config->security);
	wifi.key_id = 0; // WEP key ID missed in BT Config, default WEP key ID 0

	if (wifi.ssid.len == 0) {
		printf("[APP] BLE WiFiMate server error: SSID can't be empty\r\n");
		return false;
	}

	//Check if in AP mode
	wext_get_mode(WLAN0_NAME, &mode);
	if(mode != RTW_MODE_INFRA) {
        wifi_set_mode(RTW_MODE_STA);
	}

    ret = wifi_connect((char*)wifi.ssid.val, wifi.security_type, (char*)wifi.password, wifi.ssid.len,
                wifi.password_len, wifi.key_id, NULL);
	if (ret != RTW_SUCCESS) {
		uint8_t err_code = ble_wifimate_wifi_conn_result_to_bwm_errcode(ret);
		printf("[APP] BLE WiFiMate server can't connect to AP, ret=%d err_code=%d\r\n", ret, err_code);
		return ble_wifimate_server_indicate_wifi_conn_state(conn_id, BWM_WIFI_STATE_IDLE, err_code);
	}

	printf("[APP] BLE WiFiMate server wifi connected.\r\n");

#if defined(CONFIG_LWIP_LAYER) && CONFIG_LWIP_LAYER
	uint8_t DCHP_state;
	/* Start DHCPClient */
	DCHP_state = LwIP_DHCP(0, DHCP_START);

	if (DCHP_state != DHCP_ADDRESS_ASSIGNED) {
		printf("[APP] BLE WiFiMate server DHCP fail, DHCP_state=%d\r\n", DCHP_state);
		return ble_wifimate_server_indicate_wifi_conn_state(conn_id, BWM_WIFI_STATE_IDLE, BWM_ERR_DHCP_ADDRESS_ASSIGN_FAIL);
	}
	printf("[APP] BLE WiFiMate server wifi got IP.\r\n");
#endif
	return ble_wifimate_server_indicate_wifi_conn_state(conn_id, BWM_WIFI_STATE_CONNECTED, BWM_OK);
}

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

static bool ble_wifimate_aes_ecb_decrypt(uint8_t src_len, uint8_t *src, uint8_t key_len,
											 uint8_t *key, uint8_t *result_len, uint8_t *result)
{
	uint8_t *key_le = NULL;
	int ret;

	if (!src || !key || !result_len || !result) {
		return false;
	}

	if (rtl_cryptoEngine_init()  != 0) {
		printf("crypto engine init failed\r\n");
	} else {
		printf("crypto engine init success\r\n");
	}

	key_le = (uint8_t *)os_mem_alloc(RAM_TYPE_DATA_ON, key_len);
	if (!key_le) {
		printf("crypto engine alloc key_le fail\r\n");
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

	ret = rtl_crypto_aes_ecb_decrypt(src, src_len, NULL, 0, result);
	if ( ret != 0 ) {
		printf("AES ECB init failed\r\n");
		os_mem_free(key_le);
		return false;
	}

	*result_len = strlen((char *)result);
	os_mem_free(key_le);
	return true;
}

static bool ble_wifimate_server_decrypt(uint8_t src_len, uint8_t *src, uint8_t *des_len, uint8_t *des)
{
	bool ret;

	if (!src || !des_len || !des) {
		return false;
	}

	switch (s_decrypt.algorithm_type) {
	case BLE_WIFIMATE_ENCRYPT_DECRYPT_ALGO_AES_ECB: {
		ret = ble_wifimate_aes_ecb_decrypt(src_len, src, 16, s_decrypt.key, des_len, des);
		break;
	}
	default:
		ret = false;
		break;
	}

	return ret;
}

static bool ble_wifimate_parse_wifi_connect_config(
	uint16_t len, uint8_t *data, uint8_t *decrypt_pw, wifi_conn_config_t *conn_config)
{
	uint16_t offset = 0;
	bool ret = true;

	if (!conn_config || !data || !decrypt_pw) {
		printf("[APP] BLE WiFiMate server parse wifi connect param invalid.\r\n");
		return false;
	}

	memset(conn_config, 0, sizeof(conn_config));

	if (len > (offset + CHAR_WIFI_CONN_ENABLE_SEG_FIRST_ELE_LEN)) {
		conn_config->ssid_len = *data;
		offset += CHAR_WIFI_CONN_ENABLE_SEG_FIRST_ELE_LEN;
	}

	if (len > (offset + conn_config->ssid_len)) {
		memcpy(conn_config->ssid, data + offset, conn_config->ssid_len);
		offset += conn_config->ssid_len;
	}

	if (len > (offset + CHAR_WIFI_CONN_ENABLE_SEG_THIRD_ELE_LEN)) {
		conn_config->security = *(data + offset);
		offset += CHAR_WIFI_CONN_ENABLE_SEG_THIRD_ELE_LEN;
	}

	if (len > (offset + CHAR_WIFI_CONN_ENABLE_SEG_FORTH_ELE_LEN)) {
		conn_config->password_len = *(data + offset);
		offset += CHAR_WIFI_CONN_ENABLE_SEG_FORTH_ELE_LEN;
	}

	if (len == (offset + conn_config->password_len)) {
		if (conn_config->password_len > 0) {
			conn_config->password = data + offset;
		} else {
			conn_config->password = NULL;
		}
	}

	if (s_decrypt.algorithm_type) {
		if (conn_config->password_len > BLE_WIFIMATE_DECRYPT_PASSWORD_LEN_MAX) {
			printf("[APP] BLE WiFiMate wifi connect decrypt password len(%d) unsupport\r\n", conn_config->password_len);
			return false;
		}
		if (conn_config->password_len > 0) {
			ret = ble_wifimate_server_decrypt(conn_config->password_len, conn_config->password, &conn_config->password_len, decrypt_pw);
			conn_config->password = decrypt_pw;
		}
	}
	return ret;
}

static bool ble_wifimate_server_write_wifi_connect_hdl(uint8_t conn_id, uint16_t len, uint8_t *data)
{
	uint16_t ret = 0;

	if (!data) {
		printf("[APP] BLE WiFiMate indicate wifi_scan_info value NULL\r\n");
		return false;
	}

	if (!s_multi_recv.flag) {
		uint16_t total_len;
		uint8_t seg_num;
		uint32_t checksum;

		if (len != 7) {
			printf("[APP] BLE WiFiMate indicate wifi_scan_info length invalid %d!\r\n", len);
			return false;
		}

		memcpy(&total_len, data, sizeof(uint16_t));
		memcpy(&seg_num,   data + sizeof(uint16_t), sizeof(uint8_t));
		memcpy(&checksum,  data + sizeof(uint16_t) + sizeof(uint8_t), sizeof(uint32_t));

		if (seg_num > 0) {
			ble_wifimate_char_multi_recv_data_init(total_len, seg_num, checksum);
		} else {
			printf("[APP] BLE WiFiMate conn_id=%d wifi connect enable invalid!\r\n", conn_id);
			return false;
		}
	} else {
		uint32_t checksum;
		memcpy(&s_multi_recv.seg_idx, data, sizeof(uint8_t));
		memcpy(s_multi_recv.data + s_multi_recv.offset, data + sizeof(uint8_t), len - 1);
		s_multi_recv.offset += len - 1;

		if (s_multi_recv.total_len == s_multi_recv.offset) {
			checksum = ble_wifimate_server_checksum_cal(s_multi_recv.total_len, s_multi_recv.data);
			if (checksum == s_multi_recv.checksum) {
				uint8_t decrypt_pw[BLE_WIFIMATE_DECRYPT_PASSWORD_LEN_MAX] ALIGNMTO(CACHE_LINE_SIZE) = {0};
				wifi_conn_config_t conn_config = {0};

				ret = ble_wifimate_parse_wifi_connect_config(s_multi_recv.total_len, s_multi_recv.data, decrypt_pw, &conn_config);
				if (ret) {
					ret = ble_wifimate_wifi_connect(conn_id, &conn_config);
				}
			} else {
				ret = false;
				printf("[APP] BLE WiFiMate conn_id=%d wifi connect recv checksum fail!\r\n", conn_id);
			}
			ble_wifimate_char_multi_recv_data_deinit();
		}
	}

	return ret;
}

static bool ble_wifimate_state_check_update(uint8_t hdl_idx)
{
	bool check = false;

	switch (hdl_idx) {
	case BLE_WIFIMATE_KEY_NEGOTIATE_INDEX:
		if (s_ble_wifimate_state == BLE_WIFIMATE_SERVER_STATE_CONNECT) {
			check = true;
		}
		break;
	case BLE_WIFIMATE_WIFI_SCAN_ENABLE_INDEX:
		if ((s_ble_wifimate_state == BLE_WIFIMATE_SERVER_STATE_CONNECT) || \
			(s_ble_wifimate_state == BLE_WIFIMATE_SERVER_STATE_WIFI_SCAN)) {
			s_ble_wifimate_state = BLE_WIFIMATE_SERVER_STATE_WIFI_SCAN;
			check = true;
		}
		break;
	case BLE_WIFIMATE_WIFI_CONNECT_ENABLE_INDEX:
		if ((s_ble_wifimate_state == BLE_WIFIMATE_SERVER_STATE_WIFI_SCAN) || \
			(s_ble_wifimate_state == BLE_WIFIMATE_SERVER_STATE_WIFI_CONN)) {
			s_ble_wifimate_state = BLE_WIFIMATE_SERVER_STATE_WIFI_CONN;
			check = true;
		}

		break;
	default:
		break;
	}
	return check;
}

void ble_wifimate_write_post_callback(uint8_t conn_id, T_SERVER_ID service_id, uint16_t attrib_index,
                                uint16_t length, uint8_t *p_value)
{
	if (service_id != ble_wifimate_srv_id) {
		printf("%s: service_id %d unmatch\r\n", __func__, service_id);
        return;
	}

	if (!ble_wifimate_state_check_update(attrib_index)) {
        printf("%s: ble wifimate state check fail\r\n", __func__);
        return;
    }

    switch(attrib_index)
    {
    case BLE_WIFIMATE_KEY_NEGOTIATE_INDEX: {
        ble_wifimate_server_key_negotiate_hdl(length, p_value);
        break;
    }
    case BLE_WIFIMATE_WIFI_SCAN_ENABLE_INDEX: {
        ble_wifimate_server_write_wifi_scan_hdl(conn_id, length, p_value);
        break;
    }
    case BLE_WIFIMATE_WIFI_CONNECT_ENABLE_INDEX: {
        ble_wifimate_server_write_wifi_connect_hdl(conn_id, length, p_value);
        break;
    }
    default:
        break;
    }
}

/**
 * @brief write characteristic data from service.
 *
 * @param conn_id
 * @param service_id        ServiceID to be written.
 * @param attrib_index      Attribute index of characteristic.
 * @param length            length of value to be written.
 * @param p_value           value to be written.
 * @return Profile procedure result
 */
static T_APP_RESULT ble_wifimate_attr_write_cb(uint8_t conn_id, T_SERVER_ID service_id,
                                        uint16_t attrib_index, T_WRITE_TYPE write_type, uint16_t length, uint8_t *p_value,
                                        P_FUN_WRITE_IND_POST_PROC *p_write_ind_post_proc)
{
    (void)p_write_ind_post_proc;
    BLE_WIFIMATE_CALLBACK_DATA callback_data;
    T_APP_RESULT  cause = APP_RESULT_SUCCESS;
	*p_write_ind_post_proc = ble_wifimate_write_post_callback;

	memset(&callback_data, 0, sizeof(callback_data));
	callback_data.conn_id = conn_id;
	callback_data.msg_type = SERVICE_CALLBACK_TYPE_WRITE_CHAR_VALUE;
	callback_data.msg_data.write.index = attrib_index;
	callback_data.msg_data.write.write_type = write_type;
	callback_data.msg_data.write.len = length;
	callback_data.msg_data.write.p_value = p_value;

	if (pfn_ble_wifimate_service_cb) {
		cause = pfn_ble_wifimate_service_cb(service_id, (void *)&callback_data);
	}
    

    return cause;
}

/**
  * @brief Add BLE wifimate service to the BLE stack database.
  *
  * @param[in]   p_func  Callback when service attribute was read, write or cccd update.
  * @return Service id generated by the BLE stack: @ref T_SERVER_ID.
  * @retval 0xFF Operation failure.
  * @retval others Service id assigned by stack.
  *
  */
T_SERVER_ID ble_wifimate_add_service(void *p_func)
{
	if (false == server_add_service(&ble_wifimate_srv_id,
									(uint8_t *)ble_wifimate_attr_tbl,
									sizeof(ble_wifimate_attr_tbl),
									ble_wifimate_cbs))
	{
		printf("ble_wifimate_add_service: fail\r\n");
		ble_wifimate_srv_id = 0xff;
		return ble_wifimate_srv_id;
	}

	pfn_ble_wifimate_service_cb = (P_FUN_SERVER_GENERAL_CB)p_func;

	return ble_wifimate_srv_id;
}

bool ble_wifimate_server_connect(uint8_t conn_id)
{
	(void)conn_id;

	if (s_ble_wifimate_state != BLE_WIFIMATE_SERVER_STATE_IDLE) {
		printf("[APP] ble wifimate server has already connected.\r\n");
        return false;
	}
	s_ble_wifimate_state = BLE_WIFIMATE_SERVER_STATE_CONNECT;

	return true;
}

void ble_wifimate_server_disconnect(uint8_t conn_id)
{
	(void)conn_id;

	s_cccd_ind_en_wifi_scan = 0;
	s_cccd_ind_en_wifi_conn = 0;
	s_ble_wifimate_state = BLE_WIFIMATE_SERVER_STATE_IDLE;
	memset(&s_decrypt, 0, sizeof(s_decrypt));

	ble_wifimate_char_multi_indicate_data_deinit();
	ble_wifimate_char_multi_recv_data_deinit();
}

void ble_wifimate_server_deinit(void)
{
	if (s_wifi_scan_sema) {
		os_sem_delete(s_wifi_scan_sema);
		s_wifi_scan_sema = NULL;
	}
	s_cccd_ind_en_wifi_scan = 0;
	s_cccd_ind_en_wifi_conn = 0;
	s_ble_wifimate_state = BLE_WIFIMATE_SERVER_STATE_IDLE;
	memset(&s_decrypt, 0, sizeof(s_decrypt));

	ble_wifimate_char_multi_indicate_data_deinit();
	ble_wifimate_char_multi_recv_data_deinit();
}

#endif /* CONFIG_BLE_WIFIMATE_DEVICE */