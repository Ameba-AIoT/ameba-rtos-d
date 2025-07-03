/**
*********************************************************************************************************
*               Copyright(c) 2025, Realtek Semiconductor Corporation. All rights reserved.
**********************************************************************************************************
*/
#ifndef __BLE_WIFIMATE_CONFIGURATOR_APP_H__
#define __BLE_WIFIMATE_CONFIGURATOR_APP_H__

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================*
 *                              Header Files
 *============================================================================*/
#include <app_msg.h>
#include <gap.h>
#include <gap_msg.h>
#include <profile_client.h>

/** @brief  Config APP LE link number */
#define BLE_WIFIMATE_CONFIGURATOR_APP_MAX_LINKS     1
#define BLE_WIFIMATE_CONFIGURATOR_APP_MAX_MTU_SIZE  512

typedef enum
{
	BLE_WIFIMATE_CONFIGURATOR_SCAN_START,
	BLE_WIFIMATE_CONFIGURATOR_SCAN_STOP,
	BLE_WIFIMATE_CONFIGURATOR_BLE_CONNECT,
    BLE_WIFIMATE_CONFIGURATOR_WIFI_SCAN,
    BLE_WIFIMATE_CONFIGURATOR_WIFI_CONNECT,
    BLE_WIFIMATE_CONFIGURATOR_ENCRYPT_SET
} BLE_WIFIMATE_CONFIGURATOR_QDECODE_SBUTYPE;

typedef struct
{
    uint16_t                conn_handle;
    uint8_t                 bd_addr[GAP_BD_ADDR_LEN];
    T_GAP_REMOTE_ADDR_TYPE  bd_type;
} ble_wifimate_configurator_link_t;

extern T_CLIENT_ID  ble_wifimate_client_id;

T_APP_RESULT ble_wifimate_client_callback(T_CLIENT_ID client_id, uint8_t conn_id, void *p_data);
T_APP_RESULT ble_wifimate_configurator_app_gap_callback(uint8_t cb_type, void *p_cb_data);
void ble_wifimate_configurator_app_reset(void);
void ble_wifimate_configurator_app_handle_io_msg(T_IO_MSG io_msg);

#ifdef __cplusplus
}
#endif

#endif /* __BLE_WIFIMATE_CONFIGURATOR_APP_H__ */
