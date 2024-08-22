/**
*****************************************************************************************
*     Copyright(c) 2017, Realtek Semiconductor Corporation. All rights reserved.
*****************************************************************************************
   * @file      bt_ota_peripheral_app.h
   * @brief     This file handles BLE OTA peripheral application routines.
   * @author    jane
   * @date      2017-06-06
   * @version   v1.0
   **************************************************************************************
   * @attention
   * <h2><center>&copy; COPYRIGHT 2017 Realtek Semiconductor Corporation</center></h2>
   **************************************************************************************
  */

#ifndef _BT_OTA_SERVER_APP__
#define _BT_OTA_SERVER_APP__

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

/*============================================================================*
 *                              Variables
 *============================================================================*/
extern T_SERVER_ID bt_ota_srv_id;		/* bt ota service id */
extern T_GAP_DEV_STATE bt_ota_server_gap_dev_state;

/*============================================================================*
 *                              Functions
 *============================================================================*/
void bt_ota_server_app_handle_io_msg(T_IO_MSG io_msg);

T_APP_RESULT bt_ota_server_app_gap_callback(uint8_t cb_type, void *p_cb_data);

T_APP_RESULT bt_ota_server_app_profile_callback(T_SERVER_ID service_id, void *p_data);

void bt_ota_timer_init(void);
void bt_ota_timer_deinit(void);


#ifdef __cplusplus
}
#endif

#endif
