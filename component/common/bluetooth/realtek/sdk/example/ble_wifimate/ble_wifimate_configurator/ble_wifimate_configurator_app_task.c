/**
*****************************************************************************************
*     Copyright(c) 2017, Realtek Semiconductor Corporation. All rights reserved.
*****************************************************************************************
*/

/*============================================================================*
 *                              Header Files
 *============================================================================*/
#include <platform_opts_bt.h>
#if defined(CONFIG_BLE_WIFIMATE_CONFIGURATOR) && CONFIG_BLE_WIFIMATE_CONFIGURATOR
#include <stdio.h>
#include <basic_types.h>
#include <os_msg.h>
#include <os_task.h>
#include <gap.h>
#include <gap_le.h>
#include <gap_msg.h>
#include <trace_app.h>
#include <app_msg.h>
#include <ble_wifimate_configurator_app.h>
#include <ble_wifimate_configurator_app_task.h>

/** @defgroup  BLE_WIFIMATE_CONFIGURATOR_APP_TASK BLE WiFiMate Cconfigurator App Task
    * @brief This file handles the implementation of application task related functions.
    *
    * Create App task and handle events & messages
    * @{
    */
/*============================================================================*
 *                              Macros
 *============================================================================*/
#define BLE_WIFIMATE_CONFIGURATOR_APP_TASK_PRIORITY             1         //!< Task priorities
#define BLE_WIFIMATE_CONFIGURATOR_APP_TASK_STACK_SIZE           256 * 6   //!<  Task stack size
#define BLE_WIFIMATE_CONFIGURATOR_MAX_NUMBER_OF_GAP_MESSAGE     0x20      //!<  GAP message queue size
#define BLE_WIFIMATE_CONFIGURATOR_MAX_NUMBER_OF_IO_MESSAGE      0x20      //!<  IO message queue size
#define BLE_WIFIMATE_CONFIGURATOR_MAX_NUMBER_OF_EVENT_MESSAGE   \
            (BLE_WIFIMATE_CONFIGURATOR_MAX_NUMBER_OF_GAP_MESSAGE + BLE_WIFIMATE_CONFIGURATOR_MAX_NUMBER_OF_IO_MESSAGE)    //!< Event message queue size

/*============================================================================*
 *                              Variables
 *============================================================================*/
void *ble_wifimate_configurator_app_task_handle;   //!< APP Task handle
void *ble_wifimate_configurator_evt_queue_handle;  //!< Event queue handle
void *ble_wifimate_configurator_io_queue_handle;   //!< IO queue handle

/*============================================================================*
 *                              Functions
 *============================================================================*/
void ble_wifimate_configurator_app_send_msg(uint16_t subtype, void *buf)
{
	uint8_t event = EVENT_IO_TO_APP;
	T_IO_MSG io_msg;

	io_msg.type = IO_MSG_TYPE_QDECODE;
	io_msg.subtype = subtype;
	io_msg.u.buf = buf;

	if (ble_wifimate_configurator_evt_queue_handle != NULL && ble_wifimate_configurator_io_queue_handle != NULL) {
		if (os_msg_send(ble_wifimate_configurator_io_queue_handle, &io_msg, 0) == false) {
			printf("bt wifimate client app send msg fail: subtype 0x%x\r\n", io_msg.subtype);
		} else if (os_msg_send(ble_wifimate_configurator_evt_queue_handle, &event, 0) == false) {
			printf("bt wifimate client app send event fail: subtype 0x%x\r\n", io_msg.subtype);
		}
	}
}

/**
 * @brief        App task to handle events & messages
 * @param[in]    p_param    Parameters sending to the task
 * @return       void
 */
void ble_wifimate_configurator_app_main_task(void *p_param)
{
    (void) p_param;
    uint8_t event;

    os_msg_queue_create(&ble_wifimate_configurator_io_queue_handle, BLE_WIFIMATE_CONFIGURATOR_MAX_NUMBER_OF_IO_MESSAGE, sizeof(T_IO_MSG));
    os_msg_queue_create(&ble_wifimate_configurator_evt_queue_handle, BLE_WIFIMATE_CONFIGURATOR_MAX_NUMBER_OF_EVENT_MESSAGE, sizeof(uint8_t));

    gap_start_bt_stack(ble_wifimate_configurator_evt_queue_handle,
                        ble_wifimate_configurator_io_queue_handle,
                        BLE_WIFIMATE_CONFIGURATOR_MAX_NUMBER_OF_GAP_MESSAGE);

    while (true)
    {
        if (os_msg_recv(ble_wifimate_configurator_evt_queue_handle, &event, 0xFFFFFFFF) == true)
        {
            if (event == EVENT_IO_TO_APP)
            {
                T_IO_MSG io_msg;
                if (os_msg_recv(ble_wifimate_configurator_io_queue_handle, &io_msg, 0) == true)
                {
                    ble_wifimate_configurator_app_handle_io_msg(io_msg);
                }
            }
            else
            {
                gap_handle_msg(event);
            }
        }
    }
}

/**
 * @brief  Initialize App task
 * @return void
 */
void ble_wifimate_configurator_app_task_init()
{
    os_task_create(&ble_wifimate_configurator_app_task_handle, "ble_wifimate_configurator_app", ble_wifimate_configurator_app_main_task, 0,
				   BLE_WIFIMATE_CONFIGURATOR_APP_TASK_STACK_SIZE, BLE_WIFIMATE_CONFIGURATOR_APP_TASK_PRIORITY);
}

/**
 * @brief  Deinitialize App task
 * @return void
 */
void ble_wifimate_configurator_app_task_deinit(void)
{
	if (ble_wifimate_configurator_app_task_handle) {
		os_task_delete(ble_wifimate_configurator_app_task_handle);
        ble_wifimate_configurator_app_task_handle = NULL;
	}

	if (ble_wifimate_configurator_io_queue_handle) {
		os_msg_queue_delete(ble_wifimate_configurator_io_queue_handle);
        ble_wifimate_configurator_io_queue_handle = NULL;
	}

	if (ble_wifimate_configurator_evt_queue_handle) {
		os_msg_queue_delete(ble_wifimate_configurator_evt_queue_handle);
        ble_wifimate_configurator_evt_queue_handle = NULL;
	}
}
/** @} */ /* End of group BLE_WIFIMATE_CONFIGURATOR_APP_TASK */

#endif /* CONFIG_BLE_WIFIMATE_CONFIGURATOR */