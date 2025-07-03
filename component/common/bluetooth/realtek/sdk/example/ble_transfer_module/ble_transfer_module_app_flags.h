/**
*****************************************************************************************
*     Copyright(c) 2017, Realtek Semiconductor Corporation. All rights reserved.
*****************************************************************************************
   * @file      app_flags.h
   * @brief     This file is used to config app functions.
   * @author    jane
   * @date      2017-06-06
   * @version   v1.0
   **************************************************************************************
   * @attention
   * <h2><center>&copy; COPYRIGHT 2017 Realtek Semiconductor Corporation</center></h2>
   **************************************************************************************
  */
#ifndef _BLE_TRANSFER_MODULE_APP_FLAGS_H_
#define _BLE_TRANSFER_MODULE_APP_FLAGS_H_

#include <trace_app.h>
#include <bt_flags.h>

#include <app_common_flags.h>

/** @defgroup  CENTRAL_CLIENT_Config Central Client App Configuration
    * @brief This file is used to config app functions.
    * @{
    */
/*============================================================================*
 *                              Constants
 *============================================================================*/

/** @brief  Config APP LE link number */
#if defined(CONFIG_PLATFORM_8721D)
#define BLE_TRANSFER_APP_MAX_LINKS  4
#define BLE_TRANSFER_PERIPHERAL_APP_MAX_LINKS   1 //for max slave link num
#define BLE_TRANSFER_CENTRAL_APP_MAX_LINKS      3 //for max master link num
#elif defined(CONFIG_PLATFORM_8710C)
#define BLE_TRANSFER_APP_MAX_LINKS  2
#define BLE_TRANSFER_PERIPHERAL_APP_MAX_LINKS   1 //for max slave link num
#define BLE_TRANSFER_CENTRAL_APP_MAX_LINKS      1 //for max master link num
#endif

/** @brief  Config the discovery table number of gcs_client */
#define BLE_TRANSFER_APP_MAX_DISCOV_TABLE_NUM 40

#define BLE_TRANSFER_MODULE_DEV_NAME_MAX_LEN 40
/** @} */ /* End of group CENTRAL_CLIENT_Config */

typedef enum {
	BTM_OK                       =   0x00,   /*!< 0, means success */
	BTM_FAIL                     =   0x01,   /*!< 1, means common failure */
	BTM_ERR_NO_PERM              =   0x02,   /*!< 2 */
	BTM_ERR_NOT_READY            =   0x03,   /*!< 3 */
	BTM_ERR_NO_MEMORY            =   0x04,   /*!< 4 */
	BTM_ERR_MISMATCH             =   0x05,   /*!< 5 */
	BTM_ERR_BUSY                 =   0x06,   /*!< 6 */
	BTM_ERR_OS_OPERATION         =   0x07,   /*!< 7 */
	BTM_ERR_MSG_SEND             =   0x08,   /*!< 8 */
	BTM_ERR_UNSUPPORTED          =   0x09,   /*!< 9 */
	BTM_ERR_PARAM_INVALID        =   0x0A,   /*!< 10 */
	BTM_ERR_UNHANDLED            =   0x0B,   /*!< 11 */
	BTM_ERR_NEED_TRY_AGAIN       =   0x0C,   /*!< 12 */
	BTM_ERR_ALREADY_DONE         =   0x0D,   /*!< 13 */
	BTM_ERR_ALREADY_IN_PROGRESS  =   0x0E,   /*!< 14 */
	BTM_ERR_POINTER_INVALID      =   0x0F,   /*!< 15 */
	BTM_ERR_NO_CREDITS           =   0x10,   /*!< 16 */
	BTM_ERR_NO_BUFS              =   0x11,   /*!< 17 */
	BTM_ERR_NO_ENTRY             =   0x12,   /*!< 18 */
	BTM_ERR_NO_RESOURCE          =   0x13,   /*!< 19 */
	BTM_ERR_ADV_LENGTH_INVALID   =   0x14,   /*!< 20 */
	BTM_ERR_NO_CONNECTION        =   0x15,   /*!< 21 */
	BTM_ERR_IRK_NOT_FOUND        =   0x16,   /*!< 22 */
	BTM_ERR_STATE_INVALID        =   0x17,   /*!< 23 */
	BTM_ERR_QUEUE_FULL           =   0x18,   /*!< 24 */
	BTM_ERR_SEND_REQ_FAILURE     =   0x19,   /*!< 25 */
	BTM_ERR_PDU_SIZE_INVALID     =   0x1A,   /*!< 26 */
	BTM_ERR_CONN_NUM_LIMIT       =   0x1B,   /*!< 27 */
	BTM_ERR_NO_BOND              =   0x1C,   /*!< 28 */
	BTM_ERR_SYNC_TIMEOUT         =   0x1D,   /*!< 29 */
	BTM_ERR_LOWER_STACK_API      =   0x1E,   /*!< 30 */
	BTM_ERR_LOWER_STACK_CB       =   0x1F,   /*!< 31 */
	BTM_ERR_CREATE_CONN_TIMEOUT  =   0x20,   /*!< 32 */
	BTM_ERR_NO_CASE_ELEMENT      =   0x21,   /*!< 33 */
	BTM_ERR_UNKNOWN              =   0xFF,   /*!< 255 */
} ble_tranfer_module_err_common;

#endif
