/**
*****************************************************************************************
*     Copyright(c) 2017, Realtek Semiconductor Corporation. All rights reserved.
*****************************************************************************************
  * @file     simple_ble_service.h
  * @brief    Demonstration of how to implement a self-definition service.
  * @details  Demonstration of different kinds of service interfaces.
  * @author
  * @date
  * @version
  * *************************************************************************************
  */

/* Define to prevent recursive inclusion */
#ifndef _BLE_TRANSFER_MODULE_SERVICE_H_
#define _BLE_TRANSFER_MODULE_SERVICE_H_

#ifdef __cplusplus
extern "C"  {
#endif      /* __cplusplus */

/* Add Includes here */
#include <profile_server.h>

/** @defgroup Transfer_Service BLE Transfer Module Service
  * @brief BLE Transfer Module service
  * @{
  */
/*============================================================================*
 *                              Macros
 *============================================================================*/
/** @defgroup Transfer_Service_Exported_Macros TRANSFER Service Exported Macros
  * @brief
  * @{
  */

#define BLE_TRANSFER_MODULE_UUID_SRV                 0xC00C
#define BLE_TRANSFER_MODULE_UUID_CHAR_VAL_READ       0xD001
#define BLE_TRANSFER_MODULE_UUID_CHAR_VAL_WRITE      0xD002
#define BLE_TRANSFER_MODULE_UUID_CHAR_VAL_NOTIFY     0xD003
#define BLE_TRANSFER_MODULE_UUID_CHAR_VAL_INDICATE   0xD004

#define BLE_TRANSFER_MODULE_ATTR_SERVICE        0
#define BLE_TRANSFER_MODULE_ATTR_READ           1
#define BLE_TRANSFER_MODULE_ATTR_WRITE          2
#define BLE_TRANSFER_MODULE_ATTR_NOTIFY         3
#define BLE_TRANSFER_MODULE_ATTR_INDICATE       4

#define BLE_TRANSFER_MODULE_SERVICE_CHAR_V1_SERVICE_INDEX        0x00
#define BLE_TRANSFER_MODULE_SERVICE_CHAR_V1_READ_INDEX           0x02
#define BLE_TRANSFER_MODULE_SERVICE_CHAR_V2_WRITE_INDEX          0x04
#define BLE_TRANSFER_MODULE_SERVICE_CHAR_V3_NOTIFY_INDEX         0x06
#define BLE_TRANSFER_MODULE_SERVICE_CHAR_V4_INDICATE_INDEX       0x09
#define BLE_TRANSFER_MODULE_SERVICE_CHAR_NOTIFY_CCCD_INDEX       (BLE_TRANSFER_MODULE_SERVICE_CHAR_V3_NOTIFY_INDEX + 1)
#define BLE_TRANSFER_MODULE_SERVICE_CHAR_INDICATE_CCCD_INDEX     (BLE_TRANSFER_MODULE_SERVICE_CHAR_V4_INDICATE_INDEX + 1)

#define TRANSFER_MODULE_APP_ID              12

/** @defgroup Transfer_Service_Application_Parameters TRANSFER Service Application Parameters
  * @brief  Type of parameters set/got from application.
  * @{
  */
typedef enum
{
    BLE_TRANSFER_MODULE_SERVICE_PARAM_V1_READ_CHAR_VAL = 0x01,
} T_TRANSFER_PARAM_TYPE;

/** @} */



/** @defgroup Transfer_Service_Upstream_Message TRANSFER Service Upstream Message
  * @brief  Upstream message used to inform application.
  * @{
  */

/** @defgroup Transfer_Service_Read_Info TRANSFER Service Read Info
  * @brief  Parameter for reading characteristic value.
  * @{
  */
#define TRANSFER_READ_V1                                        1
/** @} */

/** @defgroup Transfer_Service_Write_Info TRANSFER Service Write Info
  * @brief  Parameter for writing characteristic value.
  * @{
  */
#define TRANSFER_WRITE_V2                                       1

/** @} */

/** @defgroup Transfer_Service_Notify_Indicate_Info TRANSFER Service Notify Indicate Info
  * @brief  Parameter for enable or disable notification or indication.
  * @{
  */
#define TRANSFER_NOTIFY_INDICATE_V3_ENABLE     1
#define TRANSFER_NOTIFY_INDICATE_V3_DISABLE    2
#define TRANSFER_NOTIFY_INDICATE_V4_ENABLE     3
#define TRANSFER_NOTIFY_INDICATE_V4_DISABLE    4


/** @} */

#define TRANSFER_READ_V1_MAX_LEN               30
#define TRANSFER_READ_V1_DEFAULT_LEN           10

/** @} End of Transfer_Service_Upstream_Message */


/** @} End of Transfer_Service_Exported_Macros */
/*============================================================================*
 *                              Types
 *============================================================================*/
/** @defgroup Transfer_Service_Exported_Types TRANSFER Service Exported Types
  * @brief
  * @{
  */

/** @defgroup TRANSFER_READ_MSG TRANSFER_READ_MSG
  * @brief BLE Transfer Module service read msg to application.
  * @{
  */
typedef struct
{
    uint8_t read_value_index; //!< ref: @ref Transfer_Service_Read_Info
    uint16_t read_offset;
} TRANSFER_READ_MSG;
/** @} End of TRANSFER_READ_MSG */

/** @defgroup TRANSFER_WRITE_MSG TRANSFER_WRITE_MSG
  * @brief BLE Transfer Module service written msg to application.
  * @{
  */
typedef struct
{
    uint8_t opcode; //!< ref:  @ref Transfer_Service_Write_Info
    T_WRITE_TYPE write_type;
    uint16_t len;
    uint8_t *p_value;
} TRANSFER_WRITE_MSG;
/** @} End of TRANSFER_WRITE_MSG */

/** @defgroup TRANSFER_UPSTREAM_MSG_DATA TRANSFER_UPSTREAM_MSG_DATA
  * @brief BLE Transfer Module service callback message content.
  * @{
  */
typedef union
{
    uint8_t notification_indification_index; //!< ref: @ref Transfer_Service_Notify_Indicate_Info
    TRANSFER_READ_MSG read;
    TRANSFER_WRITE_MSG write;
} TRANSFER_UPSTREAM_MSG_DATA;
/** @} End of TRANSFER_UPSTREAM_MSG_DATA */

/** @defgroup TRANSFER_CALLBACK_DATA TRANSFER_CALLBACK_DATA
  * @brief BLE Transfer Module service data to inform application.
  * @{
  */
typedef struct
{
    uint8_t                 conn_id;
    T_SERVICE_CALLBACK_TYPE msg_type;
    TRANSFER_UPSTREAM_MSG_DATA msg_data;
} TRANSFER_CALLBACK_DATA;
/** @} End of TRANSFER_CALLBACK_DATA */

typedef struct {
	uint16_t service_uuid;
	uint16_t charac_read_uuid;
	uint16_t charac_write_uuid;
	uint16_t charac_notify_uuid;
	uint16_t charac_indicate_uuid;
} transfer_module_uuid_t;


typedef struct {
	uint8_t len;
	uint8_t buf[TRANSFER_READ_V1_MAX_LEN];
} transfer_module_read_val_t;

/** @} End of Transfer_Service_Exported_Types */
/*============================================================================*
 *                              Functions
 *============================================================================*/
/** @defgroup Transfer_Service_Exported_Functions TRANSFER Service Exported Functions
  * @brief
  * @{
  */

/**
  * @brief Add simple BLE service to the BLE stack database.
  *
  * @param[in]   p_func  Callback when service attribute was read, write or cccd update.
  * @return Service id generated by the BLE stack: @ref T_SERVER_ID.
  * @retval 0xFF Operation failure.
  * @retval others Service id assigned by stack.
  *
  */
T_SERVER_ID ble_tranfer_module_service_add_service(void *p_func);

/**
  * @brief send notification of simple notify characteristic value.
  *
  * @param[in] conn_id           connection id
  * @param[in] service_id        service ID of service.
  * @param[in] p_value           characteristic value to notify
  * @param[in] length            characteristic value length to notify
  * @return notification action result
  * @retval 1 true
  * @retval 0 false
  */
bool ble_tranfer_module_service_send_v3_notify(uint8_t conn_id, T_SERVER_ID service_id,
                                     void *p_value, uint16_t length);
/**
  * @brief send indication of simple indicate characteristic value.
  *
  * @param[in] conn_id           connection id
  * @param[in] service_id        service ID of service.
  * @param[in] p_value           characteristic value to notify
  * @param[in] length            characteristic value length to notify
  * @return notification action result
  * @retval 1 true
  * @retval 0 false
  */
bool ble_tranfer_module_service_send_v4_indicate(uint8_t conn_id, T_SERVER_ID service_id,
                                       void *p_value, uint16_t length);

/**
  * @brief get uuid of ble transfer service.
  *
  * @param[in] attr              atrribute type
  * @param[in] uuid              uuid to get
  * @return notification action result
  * @retval 1 true
  * @retval 0 false
  */
bool ble_transfer_module_get_uuid(uint8_t attr, uint16_t *uuid);

/**
  * @brief get uuid of ble transfer service.
  *
  * @param[in] attr              atrribute type
  * @param[in] uuid              uuid to set
  * @return notification action result
  * @retval 1 true
  * @retval 0 false
  */
bool ble_transfer_module_set_uuid(uint8_t attr, uint16_t uuid);
/** @} End of Transfer_Service_Exported_Functions */

/**
  * @brief update read value
  *
  * @param[in] len               length of read value
  * @param[in] data              read value
  * @return notification action result
  * @retval 1 true
  * @retval 0 false
  */
bool ble_transfer_module_update_read_val(uint8_t len, uint8_t *data);

/** @} End of Transfer_Service */
#ifdef __cplusplus
}
#endif

#endif /* _TRANSFERLE_BLE_SERVICE_H_ */
