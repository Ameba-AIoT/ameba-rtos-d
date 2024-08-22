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
#if defined(CONFIG_BLE_MATTER_ADAPTER) && CONFIG_BLE_MATTER_ADAPTER //to be fixed
#include <os_msg.h>
#include <os_task.h>
#include <gap.h>
#include <gap_le.h>
#include <ble_matter_adapter_app_task.h>
#include <app_msg.h>
#include <ble_matter_adapter_app.h>
#include <basic_types.h>
#include <gap_msg.h>
#include <os_sync.h>

/** @defgroup  CENTRAL_CLIENT_APP_TASK Central Client App Task
    * @brief This file handles the implementation of application task related functions.
    *
    * Create App task and handle events & messages
    * @{
    */
/*============================================================================*
 *                              Macros
 *============================================================================*/
#define BLE_MATTER_ADAPTER_APP_TASK_PRIORITY             4         //!< Task priorities
#define BLE_MATTER_ADAPTER_APP_TASK_STACK_SIZE           256 * 6   //!<  Task stack size
#define BLE_MATTER_ADAPTER_MAX_NUMBER_OF_GAP_MESSAGE     0x20      //!<  GAP message queue size
#define BLE_MATTER_ADAPTER_MAX_NUMBER_OF_IO_MESSAGE      0x20      //!<  IO message queue size
#define BLE_MATTER_ADAPTER_MAX_NUMBER_OF_EVENT_MESSAGE   (BLE_MATTER_ADAPTER_MAX_NUMBER_OF_GAP_MESSAGE + BLE_MATTER_ADAPTER_MAX_NUMBER_OF_IO_MESSAGE)    //!< Event message queue size
//ble matter
#define BLE_MATTER_ADAPTER_CALLBACK_TASK_PRIORITY        4         //!< Task priorities
#define BLE_MATTER_ADAPTER_CALLBACK_TASK_STACK_SIZE      256 * 6   //!<  Task stack size
#define BLE_MATTER_ADAPTER_MAX_NUMBER_OF_CALLBACK_MESSAGE               0x20      //!< Callback message queue size
/*============================================================================*
 *                              Variables
 *============================================================================*/
void *ble_matter_adapter_app_task_handle;   //!< APP Task handle
void *ble_matter_adapter_evt_queue_handle;  //!< Event queue handle
void *ble_matter_adapter_io_queue_handle;   //!< IO queue handle

void *ble_matter_adapter_callback_task_handle = NULL;    //!< Callback task handle
void *ble_matter_adapter_callback_queue_handle = NULL;   //!< Callback queue handle

extern T_GAP_DEV_STATE ble_matter_adapter_gap_dev_state;
extern int ble_matter_adapter_central_app_max_links;
extern int ble_matter_adapter_peripheral_app_max_links;
/*============================================================================*
 *                              Functions
 *============================================================================*/
bool ble_matter_adapter_app_send_api_msg(uint16_t sub_type, void *arg)
{
	T_IO_MSG io_msg;

	uint8_t event = EVENT_IO_TO_APP;

	io_msg.type = IO_MSG_TYPE_QDECODE;
	io_msg.subtype = sub_type;
	io_msg.u.buf = arg;

	if (ble_matter_adapter_evt_queue_handle != NULL && ble_matter_adapter_io_queue_handle != NULL) {
		if (os_msg_send(ble_matter_adapter_io_queue_handle, &io_msg, 0) == false) {
			printf("[%s] send io queue fail! type = 0x%x\r\n", __FUNCTION__, io_msg.subtype);
			return false;
		} else if (os_msg_send(ble_matter_adapter_evt_queue_handle, &event, 0) == false) {
			printf("[%s] send event queue fail! type = 0x%x\r\n", __FUNCTION__, io_msg.subtype);
			return false;
		}
	} else {
		printf("[%s] queue is empty! type = 0x%x\r\n", __FUNCTION__, io_msg.subtype);
		return false;
	}

	return true;
}

bool ble_matter_adapter_send_callback_msg(uint16_t msg_type, uint8_t cb_type, void *arg)
{
	T_IO_MSG callback_msg;
	callback_msg.type = msg_type;

	if( (msg_type==BT_MATTER_SEND_CB_MSG_SEND_DATA_COMPLETE) || (msg_type==BT_MATTER_SEND_CB_MSG_IND_NTF_ENABLE) ||\
		(msg_type==BT_MATTER_SEND_CB_MSG_IND_NTF_DISABLE) || (msg_type==BT_MATTER_SEND_CB_MSG_WRITE_CHAR) )
	{
		callback_msg.subtype = cb_type;
	}

	callback_msg.u.buf = arg;
	if (ble_matter_adapter_callback_queue_handle != NULL) {
		if (os_msg_send(ble_matter_adapter_callback_queue_handle, &callback_msg, 0) == false) {
			printf("bt matter send msg fail: subtype 0x%x", callback_msg.type);
			return false;
		}

		return true;
	}

	return false;
}

void ble_matter_adapter_callback_main_task(void *p_param)
{
	(void)p_param;
	T_IO_MSG callback_msg;
	os_msg_queue_create(&ble_matter_adapter_callback_queue_handle, BLE_MATTER_ADAPTER_MAX_NUMBER_OF_CALLBACK_MESSAGE, sizeof(T_IO_MSG));

	while (true)
	{
		if (os_msg_recv(ble_matter_adapter_callback_queue_handle, &callback_msg, 0xFFFFFFFF) == true)
		{
			ble_matter_adapter_app_handle_callback_msg(callback_msg);
		}
	}
}
/**
 * @brief  Initialize App task
 * @return void
 */
void ble_matter_adapter_app_task_init(void)
{
	os_task_create(&ble_matter_adapter_app_task_handle, "ble_matter_adapter_app", ble_matter_adapter_app_main_task, 0, BLE_MATTER_ADAPTER_APP_TASK_STACK_SIZE,
				   BLE_MATTER_ADAPTER_APP_TASK_PRIORITY);
	os_task_create(&ble_matter_adapter_callback_task_handle, "ble_matter_adapter_callback", ble_matter_adapter_callback_main_task, 0, BLE_MATTER_ADAPTER_CALLBACK_TASK_STACK_SIZE,
				   BLE_MATTER_ADAPTER_CALLBACK_TASK_PRIORITY);
}

/**
 * @brief        App task to handle events & messages
 * @param[in]    p_param    Parameters sending to the task
 * @return       void
 */
void ble_matter_adapter_app_main_task(void *p_param)
{
	(void) p_param;
	uint8_t event;

	os_msg_queue_create(&ble_matter_adapter_io_queue_handle, BLE_MATTER_ADAPTER_MAX_NUMBER_OF_IO_MESSAGE, sizeof(T_IO_MSG));
	os_msg_queue_create(&ble_matter_adapter_evt_queue_handle, BLE_MATTER_ADAPTER_MAX_NUMBER_OF_EVENT_MESSAGE, sizeof(uint8_t));

	gap_start_bt_stack(ble_matter_adapter_evt_queue_handle, ble_matter_adapter_io_queue_handle, BLE_MATTER_ADAPTER_MAX_NUMBER_OF_GAP_MESSAGE);

	while (true) {
		if (os_msg_recv(ble_matter_adapter_evt_queue_handle, &event, 0xFFFFFFFF) == true) {
			if (event == EVENT_IO_TO_APP) {
				T_IO_MSG io_msg;
				if (os_msg_recv(ble_matter_adapter_io_queue_handle, &io_msg, 0) == true) {
					ble_matter_adapter_app_handle_io_msg(io_msg);
				}
			} else {
				gap_handle_msg(event);
			}
		}
	}
}

void ble_matter_adapter_app_task_deinit(void)
{
#ifndef PLATFORM_OHOS
	if (ble_matter_adapter_io_queue_handle) {
		os_msg_queue_delete(ble_matter_adapter_io_queue_handle);
	}
	if (ble_matter_adapter_evt_queue_handle) {
		os_msg_queue_delete(ble_matter_adapter_evt_queue_handle);
	}
	if (ble_matter_adapter_callback_queue_handle) {
		os_msg_queue_delete(ble_matter_adapter_callback_queue_handle);
	}
	if (ble_matter_adapter_app_task_handle) {
		os_task_delete(ble_matter_adapter_app_task_handle);
	}
	if (ble_matter_adapter_callback_task_handle) {
		os_task_delete(ble_matter_adapter_callback_task_handle);
	}
#else
	if (ble_matter_adapter_app_task_handle) {
		os_task_delete(ble_matter_adapter_app_task_handle);
	}
	if (ble_matter_adapter_callback_task_handle) {
		os_task_delete(ble_matter_adapter_callback_task_handle);
	}
	if (ble_matter_adapter_io_queue_handle) {
		os_msg_queue_delete(ble_matter_adapter_io_queue_handle);
	}
	if (ble_matter_adapter_evt_queue_handle) {
		os_msg_queue_delete(ble_matter_adapter_evt_queue_handle);
	}
	if (ble_matter_adapter_callback_queue_handle) {
		os_msg_queue_delete(ble_matter_adapter_callback_queue_handle);
	}
#endif
	ble_matter_adapter_io_queue_handle = NULL;
	ble_matter_adapter_evt_queue_handle = NULL;
	ble_matter_adapter_callback_queue_handle = NULL;
	ble_matter_adapter_app_task_handle = NULL;
	ble_matter_adapter_callback_task_handle = NULL;

	ble_matter_adapter_gap_dev_state.gap_init_state = 0;
	ble_matter_adapter_gap_dev_state.gap_adv_sub_state = 0;
	ble_matter_adapter_gap_dev_state.gap_adv_state = 0;
	ble_matter_adapter_gap_dev_state.gap_scan_state = 0;
	ble_matter_adapter_gap_dev_state.gap_conn_state = 0;

	ble_matter_adapter_central_app_max_links = 0;
	ble_matter_adapter_peripheral_app_max_links = 0;

	//bt matter
	if (ble_matter_adapter_callback_task_handle) {
		os_task_delete(ble_matter_adapter_callback_task_handle);
	}
	if (ble_matter_adapter_callback_queue_handle) {
		os_msg_queue_delete(ble_matter_adapter_callback_queue_handle);
	}

}


/** @} */ /* End of group CENTRAL_CLIENT_APP_TASK */
#endif

