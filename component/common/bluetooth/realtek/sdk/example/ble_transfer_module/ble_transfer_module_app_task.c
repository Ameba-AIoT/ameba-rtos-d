/**
*****************************************************************************************
*     Copyright(c) 2017, Realtek Semiconductor Corporation. All rights reserved.
*****************************************************************************************
   * @file      app_task.c
   * @brief     Routines to create App task and handle events & messages
   * @author    jane
   * @date      2017-06-02
   * @version   v1.0
   **************************************************************************************
   * @attention
   * <h2><center>&copy; COPYRIGHT 2017 Realtek Semiconductor Corporation</center></h2>
   **************************************************************************************
  */

/*============================================================================*
 *                              Header Files
 *============================================================================*/
#include <platform_opts_bt.h>
#if defined(CONFIG_BLE_TRANSFER_MODULE) && CONFIG_BLE_TRANSFER_MODULE
#include <os_msg.h>
#include <os_task.h>
#include <gap.h>
#include <gap_le.h>
#include <trace_app.h>
#include <ble_transfer_module_app_task.h>
#include <app_msg.h>
#include <ble_transfer_module_app.h>
#include <basic_types.h>
#include <gap_msg.h>
#include <stdio.h>

/** @defgroup  CENTRAL_CLIENT_APP_TASK Central Client App Task
    * @brief This file handles the implementation of application task related functions.
    *
    * Create App task and handle events & messages
    * @{
    */
/*============================================================================*
 *                              Macros
 *============================================================================*/
#define BLE_TRANSFER_APP_TASK_PRIORITY             1         //!< Task priorities
#define BLE_TRANSFER_APP_TASK_STACK_SIZE           256 * 6   //!<  Task stack size
#define BLE_TRANSFER_MAX_NUMBER_OF_GAP_MESSAGE     0x20      //!<  GAP message queue size
#define BLE_TRANSFER_MAX_NUMBER_OF_IO_MESSAGE      0x20      //!<  IO message queue size
#define BLE_TRANSFER_MAX_NUMBER_OF_EVENT_MESSAGE   (BLE_TRANSFER_MAX_NUMBER_OF_GAP_MESSAGE + BLE_TRANSFER_MAX_NUMBER_OF_IO_MESSAGE)    //!< Event message queue size

/*============================================================================*
 *                              Variables
 *============================================================================*/
void *ble_transfer_app_task_handle;   //!< APP Task handle
void *ble_transfer_evt_queue_handle;  //!< Event queue handle
void *ble_transfer_io_queue_handle;   //!< IO queue handle

extern T_GAP_DEV_STATE ble_transfer_gap_dev_state;
extern int ble_transfer_central_app_max_links;
extern int ble_transfer_peripheral_app_max_links;


/*============================================================================*
 *                              Functions
 *============================================================================*/
void ble_transfer_send_msg(uint16_t sub_type)
{
    uint8_t event = EVENT_IO_TO_APP;

    T_IO_MSG io_msg;

    io_msg.type = IO_MSG_TYPE_QDECODE;
    io_msg.subtype = sub_type;

    if (ble_transfer_evt_queue_handle != NULL && ble_transfer_io_queue_handle != NULL) {
        if (os_msg_send(ble_transfer_io_queue_handle, &io_msg, 0) == false) {
            printf("ble transfer send msg fail: subtype 0x%x\r\n", io_msg.subtype);
        } else if (os_msg_send(ble_transfer_evt_queue_handle, &event, 0) == false) {
            printf("ble transfer send event fail: subtype 0x%x\r\n", io_msg.subtype);
        }
    }
}

void ble_transfer_app_main_task(void *p_param);

/**
 * @brief  Initialize App task
 * @return void
 */
void ble_transfer_app_task_init(void)
{
    os_task_create(&ble_transfer_app_task_handle, "ble_transfer_app", ble_transfer_app_main_task, 0, BLE_TRANSFER_APP_TASK_STACK_SIZE,
                   BLE_TRANSFER_APP_TASK_PRIORITY);
}

/**
 * @brief        App task to handle events & messages
 * @param[in]    p_param    Parameters sending to the task
 * @return       void
 */
void ble_transfer_app_main_task(void *p_param)
{
    (void) p_param;
    uint8_t event;

    os_msg_queue_create(&ble_transfer_io_queue_handle, BLE_TRANSFER_MAX_NUMBER_OF_IO_MESSAGE, sizeof(T_IO_MSG));
    os_msg_queue_create(&ble_transfer_evt_queue_handle, BLE_TRANSFER_MAX_NUMBER_OF_EVENT_MESSAGE, sizeof(uint8_t));

    gap_start_bt_stack(ble_transfer_evt_queue_handle, ble_transfer_io_queue_handle, BLE_TRANSFER_MAX_NUMBER_OF_GAP_MESSAGE);

    while (true)
    {
        if (os_msg_recv(ble_transfer_evt_queue_handle, &event, 0xFFFFFFFF) == true)
        {
            if (event == EVENT_IO_TO_APP)
            {
                T_IO_MSG io_msg;
                if (os_msg_recv(ble_transfer_io_queue_handle, &io_msg, 0) == true)
                {
                    ble_transfer_app_handle_io_msg(io_msg);
                }
            }
            else
            {
                gap_handle_msg(event);
            }
        }
    }
}

void ble_transfer_app_task_deinit(void)
{
	if (ble_transfer_app_task_handle) {
		os_task_delete(ble_transfer_app_task_handle);
	}
	if (ble_transfer_io_queue_handle) {
		os_msg_queue_delete(ble_transfer_io_queue_handle);
	}
	if (ble_transfer_evt_queue_handle) {
		os_msg_queue_delete(ble_transfer_evt_queue_handle);
	}
	ble_transfer_io_queue_handle = NULL;
	ble_transfer_evt_queue_handle = NULL;
	ble_transfer_app_task_handle = NULL;

	ble_transfer_gap_dev_state.gap_init_state = 0;
	ble_transfer_gap_dev_state.gap_adv_sub_state = 0;
	ble_transfer_gap_dev_state.gap_adv_state = 0;
	ble_transfer_gap_dev_state.gap_scan_state = 0;
	ble_transfer_gap_dev_state.gap_conn_state = 0;

	ble_transfer_central_app_max_links = 0;
	ble_transfer_peripheral_app_max_links = 0;

}


/** @} */ /* End of group CENTRAL_CLIENT_APP_TASK */
#endif

