/*
*******************************************************************************
* Copyright(c) 2025, Realtek Semiconductor Corporation. All rights reserved.
*******************************************************************************
*/

#ifndef __BLE_WIFIMATE_SERVICE_H__
#define __BLE_WIFIMATE_SERVICE_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <profile_server.h>

/* BLE WiFiMate service written msg to application */
typedef struct
{
    uint8_t index;
    T_WRITE_TYPE write_type;
    uint16_t len;
    uint8_t *p_value;
} BLE_WIFIMATE_WRITE_MSG;

/* BLE WiFiMate service callback message content */
typedef union
{
    uint8_t 				notify_indicate_index;
    BLE_WIFIMATE_WRITE_MSG 	write;
} BLE_WIFIMATE_UPSTREAM_MSG_DATA;

/* BLE WiFiMate service data to inform application */
typedef struct
{
    uint8_t                 		conn_id;
    T_SERVICE_CALLBACK_TYPE 		msg_type;
    BLE_WIFIMATE_UPSTREAM_MSG_DATA	msg_data;
} BLE_WIFIMATE_CALLBACK_DATA;

#define BLE_WIFIMATE_UUID_SRV                           0xC00D
#define BLE_WIFIMATE_UUID_CHAR_KEY_NEGOTIATE            0xD020
#define BLE_WIFIMATE_UUID_CHAR_WIFI_SCAN_ENABLE         0xD021
#define BLE_WIFIMATE_UUID_CHAR_WIFI_SCAN_INFO           0xD022
#define BLE_WIFIMATE_UUID_CHAR_WIFI_CONNECT_ENABLE      0xD023
#define BLE_WIFIMATE_UUID_CHAR_WIFI_CONNECT_STATE       0xD024

#define BLE_WIFIMATE_KEY_NEGOTIATE_INDEX                        (2)
#define BLE_WIFIMATE_WIFI_SCAN_ENABLE_INDEX                     (4)
#define BLE_WIFIMATE_WIFI_SCAN_INFO_VAL_INDEX                   (6)
#define BLE_WIFIMATE_WIFI_SCAN_INFO_CCCD_INDEX                  (7)
#define BLE_WIFIMATE_WIFI_CONNECT_ENABLE_INDEX                  (9)
#define BLE_WIFIMATE_WIFI_CONNECT_STATE_VAL_INDEX               (11)
#define BLE_WIFIMATE_WIFI_CONNECT_STATE_CCCD_INDEX              (12)

#define BLE_WIFIMATE_INDICATE_ENABLE    (1)
#define BLE_WIFIMATE_INDICATE_DISABLE   (2)

#define BLE_WIFIMATE_KEY_LEN                            (13)

#define BLE_WIFIMATE_CHAR_NOGOTIATE_KEY_LEN             (14)
#define CHAR_NEGOTIATE_KEY_FIRST_ELE_LEN                (1)
#define CHAR_NEGOTIATE_KEY_SECOND_ELE_LEN               (13)

#define CHAR_WIFI_CONN_ENABLE_INITIAL_FIRST_ELE_LEN     (2)
#define CHAR_WIFI_CONN_ENABLE_INITIAL_SECOND_ELE_LEN    (1)
#define CHAR_WIFI_CONN_ENABLE_INITIAL_THIRD_ELE_LEN     (4)

#define CHAR_WIFI_CONN_ENABLE_SEG_FIRST_ELE_LEN         (1)
#define CHAR_WIFI_CONN_ENABLE_SEG_THIRD_ELE_LEN         (1)
#define CHAR_WIFI_CONN_ENABLE_SEG_FORTH_ELE_LEN         (1)

#define BLE_WIFIMATE_CHAR_WIFI_SCAN_ENABLE_DATA_LEN     (1)
#define BLE_WIFIMATE_CHAR_WIFI_CONNECT_STATE_LEN        (2)

#define BLE_WIFIMATE_SSID_LEN_MAX                       (33)

#define BLE_WIFIMATE_ENCRYPT_DECRYPT_KEY_LEN            (16)


typedef struct {
	uint8_t         algorithm_type;
	uint8_t         key[BLE_WIFIMATE_ENCRYPT_DECRYPT_KEY_LEN];
} ble_wifimate_encrypt_decrypt_t;

typedef struct {
	uint8_t         ssid_len;
	unsigned char   ssid[BLE_WIFIMATE_SSID_LEN_MAX];
	int16_t         rssi;
	uint8_t         security;
	uint8_t         channel;
} wifi_scan_info_t;

typedef struct {
	uint8_t         ssid_len;
	unsigned char   ssid[BLE_WIFIMATE_SSID_LEN_MAX];
	uint8_t         security;
	uint8_t         password_len;
	unsigned char   *password;
} wifi_conn_config_t;

typedef enum {
	BLE_WIFIMATE_ENCRYPT_DECRYPT_ALGO_NONE = 0,
	BLE_WIFIMATE_ENCRYPT_DECRYPT_ALGO_AES_ECB = 1,
	BLE_WIFIMATE_ENCRYPT_DECRYPT_ALGO_NUM = 2,
} encypt_decrypt_algorithm_e;

typedef struct {
	uint8_t         algorithm;
	uint8_t         key[BLE_WIFIMATE_KEY_LEN];
} key_negotiate_config_t;

typedef struct {
	uint8_t         flag;
	uint16_t        total_len;
	uint8_t         seg_num;
	uint32_t        checksum;
	uint8_t         seg_idx;
	uint16_t        offset;
	uint8_t         *data;
} ble_wifimate_char_send_data_t;

typedef struct {
	uint8_t         flag;
	uint16_t        total_len;
	uint8_t         seg_num;
	uint32_t        checksum;
	uint8_t         seg_idx;
	uint16_t        offset;
	uint8_t         *data;
} ble_wifimate_char_recv_data_t;

typedef enum {
	BWM_WIFI_STATE_IDLE = 0,
	BWM_WIFI_STATE_CONNECTED = 1,
} ble_wifimate_wifi_conn_state_e;

typedef enum {
	BWM_OK = 0,
	BWM_ERR_ERROR = 1,
	BWM_ERR_PARAM_INVALID = 2,
	BWM_ERR_SSID_EMPTY = 3,
	BWM_ERR_PASSWORD_WRONG = 4,
	BWM_ERR_AP_NOT_FOUND = 5,
	BWM_ERR_BUSY = 6,
	BWM_ERR_NOMEM = 7,
	BWM_ERR_TIMEOUT = 8,
	BWM_ERR_AUTH_FAIL = 9,
	BWM_ERR_ASSOC_FAIL = 10,
	BWM_ERR_DHCP_ADDRESS_ASSIGN_FAIL = 11,
} ble_wifimate_wifi_conn_error_code_e;

typedef enum {
	BWM_SECURITY_OPEN               = 0,    /**< Open security                           */
	BWM_SECURITY_WEP_PSK            = 1,    /**< WEP Security with open authentication   */
	BWM_SECURITY_WEP_SHARED         = 2,    /**< WEP Security with shared authentication */
	BWM_SECURITY_WPA_TKIP_PSK       = 3,    /**< WPA Security with TKIP                  */
	BWM_SECURITY_WPA_AES_PSK        = 4,    /**< WPA Security with AES                   */
	BWM_SECURITY_WPA_MIXED_PSK      = 5,    /**< WPA Security with AES & TKIP            */
	BWM_SECURITY_WPA2_TKIP_PSK      = 6,    /**< WPA2 Security with TKIP                 */
	BWM_SECURITY_WPA2_AES_PSK       = 7,    /**< WPA2 Security with AES                  */
	BWM_SECURITY_WPA2_MIXED_PSK     = 8,    /**< WPA2 Security with AES & TKIP           */
	BWM_SECURITY_WPA_WPA2_TKIP_PSK  = 9,    /**< WPA/WPA2 Security with TKIP             */
	BWM_SECURITY_WPA_WPA2_AES_PSK   = 10,   /**< WPA/WPA2 Security with AES              */
	BWM_SECURITY_WPA_WPA2_MIXED_PSK = 11,   /**< WPA/WPA2 Security with AES & TKIP       */
	BWM_SECURITY_WPA3_AES_PSK       = 12,   /**< WPA3-SAE with AES security            */
	BWM_SECURITY_WPA3_OWE           = 13,   /**< WPA3-OWE with AES security            */
	BWM_SECURITY_WPA2_WPA3_MIXED    = 14,   /**< WPA3-SAE/WPA2 with AES security           */
	BWM_SECURITY_WPA2_AES_CMAC      = 15,
	BWM_SECURITY_UNKNOWN            = 255,
} ble_wifimate_security_type_e;

bool ble_wifimate_server_connect(uint8_t conn_id);
void ble_wifimate_server_disconnect(uint8_t conn_id);
void ble_wifimate_server_deinit(void);
T_SERVER_ID ble_wifimate_add_service(void *p_func);

void ble_wifimate_server_set_mtu_size(uint16_t mtu_size);
bool ble_wifimate_server_send_wifi_scan_info_segment(uint8_t conn_id);

#ifdef __cplusplus
}
#endif

#endif /* __BLE_WIFIMATE_SERVICE_H__ */