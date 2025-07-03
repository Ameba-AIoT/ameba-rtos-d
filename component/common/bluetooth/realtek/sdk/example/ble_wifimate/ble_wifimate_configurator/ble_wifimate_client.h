/**
*****************************************************************************************
*     Copyright(c) 2025, Realtek Semiconductor Corporation. All rights reserved.
*****************************************************************************************
  * @file     ble_wifimate_client.h
  * @brief    Head file for using ble wifimate client.
  * @details  BLE wifimate client data structures and functions declaration.
  * *************************************************************************************
  */
#ifndef __BLE_WIFIMATE_CLINET_H__
#define __BLE_WIFIMATE_CLINET_H__

#ifdef __cplusplus
extern "C"  {
#endif

#include <bt_flags.h>
#if F_BT_LE_GATT_CLIENT_SUPPORT
#include <profile_client.h>
#include <ble_wifimate_service.h>

typedef enum {
    BLE_WIFIMATE_CHAR_NEGOTIATE_KEY_IDX = 0,
    BLE_WIFIMATE_CHAR_WIFI_SCAN_ENABLE_IDX = 1,
    BLE_WIFIMATE_CHAR_WIFI_SCAN_INFO_IDX = 2,
    BLE_WIFIMATE_CHAR_WIFI_CONNECT_ENABLE_IDX = 3,
    BLE_WIFIMATE_CHAR_WIFI_CONNECT_STATE_IDX = 4,
    BLE_WIFIMATE_CHAR_NUM,
} ble_wifimate_charac_index_e;

typedef enum {
    DISC_IDLE = 0,
    DISC_START,
    DISC_DONE,
    DISC_FAILED,
} ble_wifimate_disc_state_t;

typedef struct {
    uint8_t  properties;
    uint16_t uuid;
    uint16_t char_val_handle;
    uint16_t cccd_handle;
} ble_wifimate_charac_db_t;

typedef struct {
    ble_wifimate_disc_state_t       disc_state;
    uint16_t                        start_handle;
    uint16_t                        end_handle;
    ble_wifimate_charac_db_t        char_db[BLE_WIFIMATE_CHAR_NUM];
} ble_wifimate_client_db_t;

/** @brief BLE WiFiMate client write type*/
typedef enum
{
    BLE_WIFIMATE_WRITE_NEGOTIATE_KEY = 1,
    BLE_WIFIMATE_WRITE_WIFI_SCAN_ENABLE,
    BLE_WIFIMATE_WRITE_WIFI_SCAN_INFO_CCCD,
    BLE_WIFIMATE_WRITE_WIFI_CONNECT_ENABLE,
    BLE_WIFIMATE_WRITE_WIFI_CONNECT_STATE_CCCD
} ble_wifimate_write_type_e;

/** @brief BLE WiFiMate client write result*/
typedef struct
{
    ble_wifimate_write_type_e type; //write char identification
    uint16_t cause;
} ble_wifimate_write_result_t;

/** @brief BLE WiFiMate client notif/ind receive type*/
typedef enum
{
    BLE_WIFIMATE_IND_WIFI_SCAN_INFO = 1,
    BLE_WIFIMATE_IND_WIFI_CONNECT_STATE
} ble_wifimate_ind_type_e;

/** @brief BLE WiFiMate indicate receive data*/
typedef struct
{
    uint16_t value_size;
    uint8_t *p_value;
} ble_wifimate_ind_value_t;

/** @brief BLE WiFiMate client indicate receive content*/
typedef struct
{
    ble_wifimate_ind_type_e  type;
    ble_wifimate_ind_value_t data;
} ble_wifimate_ind_recv_data_t;

/** @brief BLE WiFiMate client callback type*/
typedef enum
{
    BLE_WIFIMATE_CLIENT_CB_TYPE_DISC_STATE,          //!< Discovery procedure state, done or pending.
    BLE_WIFIMATE_CLIENT_CB_TYPE_WRITE_RESULT,        //!< Write request result, success or fail.
    BLE_WIFIMATE_CLIENT_CB_TYPE_IND_RESULT,    //!< Notification or indication data received from server.
    BLE_WIFIMATE_CLIENT_CB_TYPE_INVALID              //!< Invalid callback type, no practical usage.
} ble_wifimate_client_cb_type_e;

/** @brief BLE WiFiMate client callback content*/
typedef union
{
    ble_wifimate_disc_state_t disc_state;
    ble_wifimate_write_result_t write_result;
    ble_wifimate_ind_recv_data_t ind_data;
} ble_wifimate_client_cb_content_t;

/** @brief BLE WiFiMate client callback data */
typedef struct
{
    ble_wifimate_client_cb_type_e     cb_type;
    ble_wifimate_client_cb_content_t  cb_content;
} ble_wifimate_client_cb_data_t;

T_CLIENT_ID ble_wifimate_add_client(P_FUN_GENERAL_APP_CB app_cb, uint8_t link_num);
void ble_wifimate_client_connect(uint8_t conn_id);
void ble_wifimate_client_disconnect(uint8_t conn_id);
void ble_wifimate_client_deinit(void);

bool ble_wifimate_client_enable_indicate_cccd(uint8_t conn_id, bool indicate, ble_wifimate_ind_type_e type);
bool ble_wifimate_client_start_discovery(uint8_t conn_id);
bool ble_wifimate_client_start_all_char_discovery(uint8_t conn_id);
bool ble_wifimate_client_start_all_char_desc_discovery(uint8_t conn_id);

void ble_wifimate_client_set_mtu_size(uint16_t mtu_size);
bool ble_wifimate_client_encrypt_set(ble_wifimate_encrypt_decrypt_t *enc);
int ble_wifimate_client_wifi_scan(uint8_t conn_id);
int ble_wifimate_client_wifi_connect(uint8_t conn_id, wifi_conn_config_t *conn_info);

#endif /* F_BT_LE_GATT_CLIENT_SUPPORT */

#ifdef __cplusplus
}
#endif

#endif /* __BLE_WIFIMATE_CLINET_H__ */