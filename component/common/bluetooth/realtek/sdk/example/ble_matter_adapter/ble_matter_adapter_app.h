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

#ifndef _BLE_MATTER_ADAPTER_APP_H_
#define _BLE_MATTER_ADAPTER_APP_H_

#ifdef __cplusplus
extern "C" {
#endif
/*============================================================================*
 *                              Header Files
 *============================================================================*/
#include <profile_client.h>
#include <app_msg.h>
#include <profile_server.h>
#include <gap.h>
#include <gap_msg.h>
#include <gcs_client.h>
#include <gap_conn_le.h>
#include <ble_matter_adapter_app_flags.h>
#include "ble_matter_adapter_service.h"

#define BLE_PRINT    printf
#define BD_ADDR_FMT "%02x:%02x:%02x:%02x:%02x:%02x"
#define BD_ADDR_ARG(x) (x)[5],(x)[4],(x)[3],(x)[2],(x)[1],(x)[0]
#define UUID_128_FORMAT "0x%2X:0x%2X:0x%2X:0x%2X:0x%2X:0x%2X:0x%2X:0x%2X:0x%2X:0x%2X:0x%2X:0x%2X:0x%2X:0x%2X:0x%2X:0x%2X"
#define UUID_128(x)  x[0],x[1],x[2],x[3],x[4],x[5],x[6],x[7],x[8],x[9],x[10],x[11],x[12],x[13],x[14],x[15]

#define MAX_REGISRABLE_SERVICE_NUMBER 12

#define MAX_ADV_NUMBER     2
#define MATTER_MULTI_TASK_STACK_SIZE  256*10
#define MATTER_MULTI_TASK_PRIORITY  4
#define MATTER_MULTI_ADV_DATA_QUEUE_SIZE  0x60

/*============================================================================*
 *                              Variables
 *============================================================================*/
extern T_CLIENT_ID   ble_matter_adapter_gcs_client_id;         /**< General Common Services client client id*/

typedef struct
{
	uint8_t conn_id;
	uint8_t service_id;
	uint16_t attrib_index;
	uint8_t *p_data;
	uint16_t data_len;
	uint8_t type;
} BT_MATTER_SERVER_SEND_DATA;

typedef struct
{
	uint8_t conn_id;
	uint8_t new_state;
	uint16_t disc_cause;
} BT_MATTER_CONN_EVENT;

typedef struct {
	T_GAP_CONN_STATE        conn_state;          /**< Connection state. */
	T_GAP_REMOTE_ADDR_TYPE  bd_type;             /**< remote BD type*/
	uint8_t                 bd_addr[GAP_BD_ADDR_LEN]; /**< remote BD */
	T_GAP_ROLE              role;                   //!< Device role
} T_APP_LINK;
/** @} */
/* End of group */
/** @addtogroup  CENTRAL_CLIENT_SCAN_MGR
	 * @brief  Device list block definition.
	 */
typedef struct {
	uint8_t 	 bd_addr[GAP_BD_ADDR_LEN];	/**< remote BD */
	uint8_t 	 bd_type;			   /**< remote BD type*/
} T_DEV_INFO;

typedef enum
{
	BT_MATTER_MSG_START_ADV = 12,
	BT_MATTER_MSG_STOP_ADV,
	BT_MATTER_MSG_SEND_DATA,
} BT_MATTER_SERVER_MSG_TYPE;

typedef enum
{
	BT_MATTER_SEND_CB_MSG_DISCONNECTED = 1,
	BT_MATTER_SEND_CB_MSG_CONNECTED,
	BT_MATTER_SEND_CB_MSG_SEND_DATA_COMPLETE,
	BT_MATTER_SEND_CB_MSG_IND_NTF_ENABLE,
	BT_MATTER_SEND_CB_MSG_IND_NTF_DISABLE,
	BT_MATTER_SEND_CB_MSG_WRITE_CHAR,
	BLE_MATTER_MSG_CONNECTED_MULTI_ADV,
	BLE_MATTER_MSG_DISCONNECTED_MULTI_ADV,
	BLE_MATTER_MSG_WRITE_CHAR_MULTI_ADV,
	BLE_MATTER_MSG_CCCD_RECV_ENABLE_MULTI_ADV,
	BLE_MATTER_MSG_CCCD_RECV_DISABLE_MULTI_ADV,
	BLE_MATTER_MSG_SEND_DATA_COMPLETE_MULTI_ADV,
} BT_MATTER_SEND_MSG_TYPE;

typedef enum
{
	CB_PROFILE_CALLBACK = 0x01,  /**< bt_matter_adapter_app_profile_callback */
	CB_GAP_CALLBACK = 0x02, /**< bt_matter_adapter_app_gap_callback */
	CB_GAP_MSG_CONN_EVENT = 0x3, /**< bt_matter_adapter_app_handle_gap_msg */
} T_CHIP_BLEMGR_CALLBACK_TYPE;

typedef struct {
	uint8_t 	 adv_datalen;
	uint8_t 	 scanrsp_datalen;
	uint8_t      adv_data[31];
	uint8_t      scanrsp_data[31];
	uint16_t 	 H_adv_intval;
	uint8_t 	 local_bd_type;
	uint8_t 	 is_used;
	uint8_t 	 type;        //1 means matter   2 means customer
	uint8_t 	 adv_id;
	void 		*one_shot_timer;
	void 		*update_adv_mutex;
	uint16_t 	adv_int_min;
	uint16_t 	adv_int_max;
	bool		connect_flag;
} M_MULTI_ADV_PARAM;

typedef struct {
	void *task_handle;
	void *sem_handle;
	void *queue_handle;
	bool deinit_flag;
	uint8_t customer_sta_sto_flag;   //for customer
	uint8_t matter_sta_sto_flag;   //for matter
	uint8_t adv_id;
} T_MULTI_ADV_CONCURRENT;
/*============================================================================*
 *                              Functions
 *============================================================================*/
uint8_t matter_get_unused_adv_index(int type);

bool matter_matter_ble_adv_stop_by_adv_id(uint8_t *adv_id);

bool customer_matter_ble_adv_start_by_adv_id(uint8_t *adv_id, uint8_t *adv_data, uint16_t adv_len, uint8_t *rsp_data, uint16_t rsp_len, uint8_t type);

uint8_t ble_matter_adapter_judge_adv_stop(uint8_t adv_id);

void ble_matter_adapter_multi_adv_task_func(void *arg);

void ble_matter_adapter_multi_adv_init();

void ble_matter_adapter_multi_adv_deinit();

void ble_matter_adapter_send_multi_adv_msg(uint8_t adv_id);

void ble_matter_adapter_legacy_start_adv_callback(void *data);

void ble_matter_adapter_delete_adv(uint8_t adv_id);


int ble_matter_adapter_app_handle_upstream_msg(uint16_t subtype, void *pdata);

void ble_matter_adapter_app_handle_callback_msg(T_IO_MSG callback_msg);

void ble_matter_adapter_app_handle_io_msg(T_IO_MSG io_msg);

void ble_matter_adapter_app_handle_dev_state_evt(T_GAP_DEV_STATE new_state, uint16_t cause);

void ble_matter_adapter_app_handle_conn_state_evt(uint8_t conn_id, T_GAP_CONN_STATE new_state, uint16_t disc_cause);

void ble_matter_adapter_app_handle_authen_state_evt(uint8_t conn_id, uint8_t new_state, uint16_t cause);

void ble_matter_adapter_app_handle_conn_mtu_info_evt(uint8_t conn_id, uint16_t mtu_size);

void ble_matter_adapter_app_handle_conn_param_update_evt(uint8_t conn_id, uint8_t status, uint16_t cause);

void ble_matter_adapter_app_handle_gap_msg(T_IO_MSG *p_gap_msg);

void bt_matter_device_matter_app_parse_scan_info(T_LE_SCAN_INFO *scan_info);

void ble_matter_adapter_gcs_handle_discovery_result(uint8_t conn_id, T_GCS_DISCOVERY_RESULT discov_result);

/**
 * @brief  Callback will be called when data sent from profile client layer.
 * @param  client_id the ID distinguish which module sent the data.
 * @param  conn_id connection ID.
 * @param  p_data  pointer to data.
 * @retval   result @ref T_APP_RESULT
 */
T_APP_RESULT ble_matter_adapter_gcs_client_callback(T_CLIENT_ID client_id, uint8_t conn_id, void *p_data);

/**
  * @brief Callback for gap le to notify app
  * @param[in] cb_type callback msy type @ref GAP_LE_MSG_Types.
  * @param[in] p_cb_data point to callback data @ref T_LE_CB_DATA.
  * @retval result @ref T_APP_RESULT
  */
T_APP_RESULT ble_matter_adapter_app_gap_callback(uint8_t cb_type, void *p_cb_data);

T_APP_RESULT ble_matter_adapter_app_client_callback(T_CLIENT_ID client_id, uint8_t conn_id, void *p_data);

T_APP_RESULT ble_matter_adapter_app_profile_callback(T_SERVER_ID service_id, void *p_data);

void ble_matter_adapter_app_vendor_callback(uint8_t cb_type, void *p_cb_data);

#if F_BT_GAPS_CHAR_WRITEABLE
T_APP_RESULT ble_matter_adapter_gap_service_callback(T_SERVER_ID service_id, void *p_para)
#endif

#ifdef __cplusplus
}
#endif

#endif