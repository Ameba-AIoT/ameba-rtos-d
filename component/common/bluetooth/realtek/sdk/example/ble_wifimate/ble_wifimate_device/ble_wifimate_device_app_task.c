/**
*****************************************************************************************
*     Copyright(c) 2017, Realtek Semiconductor Corporation. All rights reserved.
*****************************************************************************************
  */

/*============================================================================*
 *                              Header Files
 *============================================================================*/
#include "platform_opts_bt.h"
#if defined(CONFIG_BLE_WIFIMATE_DEVICE) && CONFIG_BLE_WIFIMATE_DEVICE
#include <stdio.h>
#include <string.h>
#include <os_msg.h>
//#include <os_task.h>
#include <gap.h>
#include <gap_le.h>
#include <gap_msg.h>
#include <app_msg.h>
#include <os_task.h>
#include <ble_wifimate_device_app.h>
#include <ble_wifimate_device_app_task.h>

/*============================================================================*
 * 							   Constants
 *============================================================================*/
#define BLE_WIFIMATE_DEVICE_APP_TASK_PRIORITY             1         //!< Task priorities
#define BLE_WIFIMATE_DEVICE_APP_TASK_STACK_SIZE           256 * 8   //!< Task stack size
#define BLE_WIFIMATE_DEVICE_MAX_NUMBER_OF_GAP_MESSAGE     0x20      //!< GAP message queue size
#define BLE_WIFIMATE_DEVICE_MAX_NUMBER_OF_IO_MESSAGE      0x20      //!< IO message queue size
#define BLE_WIFIMATE_DEVICE_MAX_NUMBER_OF_EVENT_MESSAGE   \
            (BLE_WIFIMATE_DEVICE_MAX_NUMBER_OF_GAP_MESSAGE + BLE_WIFIMATE_DEVICE_MAX_NUMBER_OF_IO_MESSAGE)    //!< Event message queue size

/*============================================================================*
 *                              Variables
 *============================================================================*/
void *ble_wifimate_device_app_task_handle = NULL;	  //!< APP Task handle
void *ble_wifimate_device_evt_queue_handle = NULL;  //!< Event queue handle
void *ble_wifimate_device_io_queue_handle = NULL;	  //!< IO queue handle

extern T_GAP_DEV_STATE ble_wifimate_device_gap_dev_state;

void ble_wifimate_device_app_send_msg(uint16_t subtype, void *buf)
{
	uint8_t event = EVENT_IO_TO_APP;
	T_IO_MSG io_msg;

	io_msg.type = IO_MSG_TYPE_QDECODE;
	io_msg.subtype = subtype;
	io_msg.u.buf = buf;

	if (ble_wifimate_device_evt_queue_handle != NULL && ble_wifimate_device_io_queue_handle != NULL) {
		if (os_msg_send(ble_wifimate_device_io_queue_handle, &io_msg, 0) == false) {
			printf("ble wifimate device app send msg fail: subtype 0x%x\r\n", io_msg.subtype);
		} else if (os_msg_send(ble_wifimate_device_evt_queue_handle, &event, 0) == false) {
			printf("ble wifimate device app send event fail: subtype 0x%x\r\n", io_msg.subtype);
		}
	}
}

/**
 * @brief        App task to handle events & messages
 * @param[in]    p_param    Parameters sending to the task
 * @return       void
 */
void ble_wifimate_device_app_main_task(void *p_param)
{
    (void)p_param;
    uint8_t event;
    printf("ble_wifimate_device_app_main_task\r\n");
    os_msg_queue_create(&ble_wifimate_device_io_queue_handle, BLE_WIFIMATE_DEVICE_MAX_NUMBER_OF_IO_MESSAGE, sizeof(T_IO_MSG));
    os_msg_queue_create(&ble_wifimate_device_evt_queue_handle, BLE_WIFIMATE_DEVICE_MAX_NUMBER_OF_EVENT_MESSAGE, sizeof(uint8_t));

    gap_start_bt_stack(ble_wifimate_device_evt_queue_handle,
                        ble_wifimate_device_io_queue_handle,
                        BLE_WIFIMATE_DEVICE_MAX_NUMBER_OF_GAP_MESSAGE);

    while (true) {
        if (os_msg_recv(ble_wifimate_device_evt_queue_handle, &event, 0xFFFFFFFF) == true) {
            if (event == EVENT_IO_TO_APP) {
                T_IO_MSG io_msg;
                if (os_msg_recv(ble_wifimate_device_io_queue_handle, &io_msg, 0) == true) {
                    ble_wifimate_device_app_handle_io_msg(io_msg);
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
void ble_wifimate_device_app_task_init(void)
{
    os_task_create(&ble_wifimate_device_app_task_handle, "ble_wifimate_device_app", ble_wifimate_device_app_main_task, 0,
                   BLE_WIFIMATE_DEVICE_APP_TASK_STACK_SIZE, BLE_WIFIMATE_DEVICE_APP_TASK_PRIORITY);
}

void ble_wifimate_device_app_task_deinit(void)
{
	if (ble_wifimate_device_app_task_handle) {
		os_task_delete(ble_wifimate_device_app_task_handle);
        ble_wifimate_device_app_task_handle = NULL;
	}
	if (ble_wifimate_device_io_queue_handle) {
		os_msg_queue_delete(ble_wifimate_device_io_queue_handle);
        ble_wifimate_device_io_queue_handle = NULL;
	}
	if (ble_wifimate_device_evt_queue_handle) {
		os_msg_queue_delete(ble_wifimate_device_evt_queue_handle);
        ble_wifimate_device_evt_queue_handle = NULL;
	}

    memset(&ble_wifimate_device_gap_dev_state, 0, sizeof(ble_wifimate_device_gap_dev_state));
}

#endif /* CONFIG_BLE_WIFIMATE_DEVICE */