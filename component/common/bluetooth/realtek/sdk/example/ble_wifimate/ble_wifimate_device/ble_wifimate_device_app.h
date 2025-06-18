/**
*********************************************************************************************************
*               Copyright(c) 2025, Realtek Semiconductor Corporation. All rights reserved.
**********************************************************************************************************
*/
#ifndef __BLE_WIFIMATE_DEVICE_APP_H__
#define __BLE_WIFIMATE_DEVICE_APP_H__

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================*
 *                              Header Files
 *============================================================================*/
#include <app_msg.h>
#include <gap.h>
#include <gap_msg.h>
#include <profile_server.h>

/** @brief  Config APP LE link number */
#define BLE_WIFIMATE_DEVICE_APP_MAX_LINKS       (1)
#define BLE_WIFIMATE_DEVICE_APP_MAX_MTU_SIZE    (512)

typedef enum
{
    BLE_WIFIMATE_DEVICE_ADV_START,
    BLE_WIFIMATE_DEVICE_ADV_STOP
} BLE_WIFIMATE_DEVICE_QDECODE_SBUTYPE;

typedef struct
{
    uint16_t                conn_handle;
    uint8_t                 bd_addr[GAP_BD_ADDR_LEN];
    T_GAP_REMOTE_ADDR_TYPE  bd_type;
} ble_wifimate_device_link_t;

/*============================================================================*
 *                              Variables
 *============================================================================*/
extern T_SERVER_ID ble_wifimate_srv_id;		/* ble wifimate service id */

T_APP_RESULT ble_wifimate_device_app_gap_callback(uint8_t cb_type, void *p_cb_data);
T_APP_RESULT ble_wifimate_device_app_profile_callback(T_SERVER_ID service_id, void *p_data);
void ble_wifimate_device_app_handle_io_msg(T_IO_MSG io_msg);
bool ble_wifimate_device_timer_init(uint16_t timeout);
void ble_wifimate_device_timer_deinit(void);

#ifdef __cplusplus
}
#endif

#endif /* __BLE_WIFIMATE_DEVICE_APP_H__ */
