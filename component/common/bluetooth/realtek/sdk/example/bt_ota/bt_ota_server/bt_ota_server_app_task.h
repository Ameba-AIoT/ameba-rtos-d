/**
*****************************************************************************************
*     Copyright(c) 2017, Realtek Semiconductor Corporation. All rights reserved.
*****************************************************************************************
   * @file      bt_ota_peripheral_app_task.h
   * @brief     Routines to create BT ota peripheral app task and handle events & messages
   * @author    jane
   * @date      2017-06-02
   * @version   v1.0
   **************************************************************************************
   * @attention
   * <h2><center>&copy; COPYRIGHT 2017 Realtek Semiconductor Corporation</center></h2>
   **************************************************************************************
  */
#ifndef _BT_OTA_SERVER_APP_TASK_H_
#define _BT_OTA_SERVER_APP_TASK_H_
#include <stdint.h>

void bt_ota_server_app_task_init(void);

void bt_ota_server_app_task_deinit(void);

void bt_ota_server_app_send_msg(uint16_t subtype);


#endif
