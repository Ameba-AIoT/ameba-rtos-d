/**
*****************************************************************************************
*     Copyright(c) 2017, Realtek Semiconductor Corporation. All rights reserved.
*****************************************************************************************
   * @file      bt_ota_peripheral_app_task.c
   * @brief     Source file for BLE OTA PERIPHERAL project, mainly used for initialize modules
   * @author    jane
   * @date      2017-06-12
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
#if defined(CONFIG_BT_OTA_SERVER) && CONFIG_BT_OTA_SERVER
#include <stdio.h>
#include <os_msg.h>
#include <os_task.h>
#include <gap.h>
#include <gap_le.h>
#include <basic_types.h>
#include <app_msg.h>
#include "bt_ota_server_app_task.h"
#include "bt_ota_server_app.h"

/** @defgroup  PERIPH_APP_TASK Peripheral App Task
    * @brief This file handles the implementation of application task related functions.
    *
    * Create App task and handle events & messages
    * @{
    */

/*============================================================================*
 * 							   Constants
 *============================================================================*/
#define BT_OTA_SERVER_APP_TASK_PRIORITY             1         //!< Task priorities
#define BT_OTA_SERVER_APP_TASK_STACK_SIZE           256 * 8   //!< Task stack size
#define BT_OTA_SERVER_MAX_NUMBER_OF_GAP_MESSAGE     0x20      //!< GAP message queue size
#define BT_OTA_SERVER_MAX_NUMBER_OF_IO_MESSAGE      0x20      //!< IO message queue size
#define BT_OTA_SERVER_MAX_NUMBER_OF_EVENT_MESSAGE   (BT_OTA_SERVER_MAX_NUMBER_OF_GAP_MESSAGE + BT_OTA_SERVER_MAX_NUMBER_OF_IO_MESSAGE)    //!< Event message queue size

/*============================================================================*
 *                              Variables
 *============================================================================*/
void *bt_ota_server_app_task_handle = NULL;	  //!< APP Task handle
void *bt_ota_server_evt_queue_handle = NULL;  //!< Event queue handle
void *bt_ota_server_io_queue_handle = NULL;	  //!< IO queue handle

/*============================================================================*
 *                              Functions
 *============================================================================*/
void bt_ota_server_app_send_msg(uint16_t subtype)
{
	uint8_t event = EVENT_IO_TO_APP;
	T_IO_MSG io_msg;

	io_msg.type = IO_MSG_TYPE_QDECODE;
	io_msg.subtype = subtype;

	if (bt_ota_server_evt_queue_handle != NULL && bt_ota_server_io_queue_handle != NULL) {
		if (os_msg_send(bt_ota_server_io_queue_handle, &io_msg, 0) == false) {
			printf("bt ota server app send msg fail: subtype 0x%x\r\n", io_msg.subtype);
		} else if (os_msg_send(bt_ota_server_evt_queue_handle, &event, 0) == false) {
			printf("bt ota server app send event fail: subtype 0x%x\r\n", io_msg.subtype);
		}
	}
}

/**
 * @brief        App task to handle events & messages
 * @param[in]    p_param    Parameters sending to the task
 * @return       void
 */
void bt_ota_server_app_main_task(void *p_param)
{
    (void)p_param;
    uint8_t event;
    os_msg_queue_create(&bt_ota_server_io_queue_handle, BT_OTA_SERVER_MAX_NUMBER_OF_IO_MESSAGE, sizeof(T_IO_MSG));
    os_msg_queue_create(&bt_ota_server_evt_queue_handle, BT_OTA_SERVER_MAX_NUMBER_OF_EVENT_MESSAGE, sizeof(uint8_t));

    gap_start_bt_stack(bt_ota_server_evt_queue_handle, bt_ota_server_io_queue_handle, BT_OTA_SERVER_MAX_NUMBER_OF_GAP_MESSAGE);

    while (true) {
        if (os_msg_recv(bt_ota_server_evt_queue_handle, &event, 0xFFFFFFFF) == true) {
            if (event == EVENT_IO_TO_APP) {
                T_IO_MSG io_msg;
                if (os_msg_recv(bt_ota_server_io_queue_handle, &io_msg, 0) == true) {
                    bt_ota_server_app_handle_io_msg(io_msg);
                }
            } else {
                gap_handle_msg(event);
            }
        }
    }
}

/**
 * @brief  Initialize App task
 * @return void
 */
void bt_ota_server_app_task_init(void)
{
    os_task_create(&bt_ota_server_app_task_handle, "bt_ota_server_app", bt_ota_server_app_main_task, 0,
                   BT_OTA_SERVER_APP_TASK_STACK_SIZE, BT_OTA_SERVER_APP_TASK_PRIORITY);
}

void bt_ota_server_app_task_deinit(void)
{
	if (bt_ota_server_app_task_handle) {
		os_task_delete(bt_ota_server_app_task_handle);
	}
	if (bt_ota_server_io_queue_handle) {
		os_msg_queue_delete(bt_ota_server_io_queue_handle);
	}
	if (bt_ota_server_evt_queue_handle) {
		os_msg_queue_delete(bt_ota_server_evt_queue_handle);
	}
	bt_ota_server_io_queue_handle = NULL;
	bt_ota_server_evt_queue_handle = NULL;
	bt_ota_server_app_task_handle = NULL;

	bt_ota_server_gap_dev_state.gap_init_state = 0;
	bt_ota_server_gap_dev_state.gap_adv_sub_state = 0;
	bt_ota_server_gap_dev_state.gap_adv_state = 0;
	bt_ota_server_gap_dev_state.gap_scan_state = 0;
	bt_ota_server_gap_dev_state.gap_conn_state = 0;
}

/** @} */ /* End of group PERIPH_APP_TASK */
#endif
