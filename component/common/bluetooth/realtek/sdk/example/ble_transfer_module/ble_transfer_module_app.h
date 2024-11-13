/**
*****************************************************************************************
*     Copyright(c) 2017, Realtek Semiconductor Corporation. All rights reserved.
*****************************************************************************************
   * @file      central_client_app.h
   * @brief     This file handles BLE central client application routines.
   * @author    jane
   * @date      2017-06-06
   * @version   v1.0
   **************************************************************************************
   * @attention
   * <h2><center>&copy; COPYRIGHT 2017 Realtek Semiconductor Corporation</center></h2>
   **************************************************************************************
  */

#ifndef _BLE_TRANSFER_MODULE_APP_H_
#define _BLE_TRANSFER_MODULE_APP_H_

#ifdef __cplusplus
extern "C" {
#endif
/*============================================================================*
 *                              Header Files
 *============================================================================*/
#include <profile_client.h>
#include <app_msg.h>
#include <ble_transfer_module_app_flags.h>
#include <profile_server.h>


#define BD_ADDR_FMT "%02x:%02x:%02x:%02x:%02x:%02x"
#define BD_ADDR_ARG(x) (x)[5],(x)[4],(x)[3],(x)[2],(x)[1],(x)[0]
#define UUID_128_FORMAT "0x%2X:0x%2X:0x%2X:0x%2X:0x%2X:0x%2X:0x%2X:0x%2X:0x%2X:0x%2X:0x%2X:0x%2X:0x%2X:0x%2X:0x%2X:0x%2X"
#define UUID_128(x)  x[0],x[1],x[2],x[3],x[4],x[5],x[6],x[7],x[8],x[9],x[10],x[11],x[12],x[13],x[14],x[15]
#define TRANSFERUUID_128_FORMAT "%2X%2X%2X%2X-%2X%2X-%2X%2X-%2X%2X-%2X%2X%2X%2X%2X%2X\r\n"
#define TRANSFER_UUID_128(x)  x[0],x[1],x[2],x[3],x[4],x[5],x[6],x[7],x[8],x[9],x[10],x[11],x[12],x[13],x[14],x[15]


/*============================================================================*
 *                              Variables
 *============================================================================*/
extern T_CLIENT_ID   ble_transfer_gcs_client_id;         /**< General Common Services client client id*/

typedef enum {
	BLE_TRANSFER_MODULE_ADV_STOP_BY_HOST      = 0,        /*!< 0, stopped by host */
	BLE_TRANSFER_MODULE_ADV_STOP_BY_CONN      = 1,        /*!< 1, stopped due to connection established */
	BLE_TRANSFER_MODULE_ADV_STOP_BY_DURATION  = 2,        /*!< 2, stopped due to duration expired or number of extended adv events exceeded */
	BLE_TRANSFER_MODULE_ADV_STOP_UNKNOWN,                 /*!< unkown */
} ble_tranfer_module_adv_stop_reason_t;

typedef enum {
	BLE_TRANSFER_MODULE_SCAN_STOP_BY_HOST      = 0,        /*!< 0, stopped by host */
	BLE_TRANSFER_MODULE_SCAN_STOP_BY_DURATION  = 1,        /*!< 1, stopped due to scan duration timeout */
	BLE_TRANSFER_MODULE_SCAN_STOP_UNKNOWN,                 /*!< unkown */
} ble_tranfer_module_scan_stop_reason_t;

/*============================================================================*
 *                              Functions
 *============================================================================*/

/**
 * @brief    All the application messages are pre-handled in this function
 * @note     All the IO MSGs are sent to this function, then the event handling
 *           function shall be called according to the MSG type.
 * @param[in] io_msg  IO message data
 * @return   void
 */
void ble_transfer_app_handle_io_msg(T_IO_MSG io_msg);
/**
  * @brief Callback for gap le to notify app
  * @param[in] cb_type callback msy type @ref GAP_LE_MSG_Types.
  * @param[in] p_cb_data point to callback data @ref T_LE_CB_DATA.
  * @retval result @ref T_APP_RESULT
  */
T_APP_RESULT ble_transfer_app_gap_callback(uint8_t cb_type, void *p_cb_data);

/**
 * @brief  Callback will be called when data sent from profile client layer.
 * @param  client_id the ID distinguish which module sent the data.
 * @param  conn_id connection ID.
 * @param  p_data  pointer to data.
 * @retval   result @ref T_APP_RESULT
 */
T_APP_RESULT ble_transfer_gcs_client_callback(T_CLIENT_ID client_id, uint8_t conn_id, void *p_data);
#if F_BT_GAPS_CHAR_WRITEABLE
	T_APP_RESULT ble_transfer_gap_service_callback(T_SERVER_ID service_id, void *p_para);
#endif

/** @defgroup  TRANSFER_MODULE_SEVER_CALLBACK Profile Server Callback Event Handler
	* @brief Handle profile server callback event
	* @{
	*/
/**
	* @brief    All the BT Profile service callback events are handled in this function
	* @note     Then the event handling function shall be called according to the
	*           service_id
	* @param    service_id  Profile service ID
	* @param    p_data      Pointer to callback data
	* @return   T_APP_RESULT, which indicates the function call is successful or not
	* @retval   APP_RESULT_SUCCESS  Function run successfully
	* @retval   others              Function run failed, and return number indicates the reason
	*/
T_APP_RESULT ble_transfer_app_profile_callback(T_SERVER_ID service_id, void *p_data);

#ifdef __cplusplus
}
#endif

#endif

