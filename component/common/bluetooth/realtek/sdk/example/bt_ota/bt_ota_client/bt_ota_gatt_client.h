/**
*****************************************************************************************
*     Copyright(c) 2017, Realtek Semiconductor Corporation. All rights reserved.
*****************************************************************************************
  * @file     bt_ota_gatt_client.h
  * @brief    Head file for using bt ota gatt client.
  * @details  bt ota gatt client data structs and external functions declaration.
  * @author
  * @date
  * @version
  * *************************************************************************************
  */

#ifndef _BT_OTA_GATT_CLIENT_H_
#define _BT_OTA_GATT_CLIENT_H_

#ifdef __cplusplus
extern "C"  {
#endif      /* __cplusplus */

/* Add Includes here */
#include <stdbool.h>
#include <stdint.h>
#include <profile_client.h>
#include <bt_ota_client_platform.h>


/** @brief  Define links number. range: 0-4 */
#define BLE_OTA_MAX_LINKS  4
/** End of SIMP_Client_Exported_Macros
  * @}
  */

#define GATT_UUID128_OTA_SERVICE                0x12, 0xA2, 0x4D, 0x2E, 0xFE, 0x14, 0x48, 0x8e, 0x93, 0xD2, 0x17, 0x3C, 0x87, 0xFF, 0x00, 0x00
#define GATT_UUID_CHAR_OTA_IMAGE_HEADER	        0x87FE
#define GATT_UUID_CHAR_OTA_HEADER_CHECKSUM      0x87FD
#define GATT_UUID_CHAR_OTA_DATA	                0x87FC
#define GATT_UUID_CHAR_OTA_DATA_CHECKSUM        0x87FB
#define GATT_UUID_CHAR_OTA_CTRL	                0x87FA

#define OTA_CLIENT_HDR_CHECK_PKT_LEN        5  // 4 + 1
#define OTA_CLIENT_DATA_CHECKSUM_PKT_LEN    10 // 4 + 4 + 2

#define OTA_CLIENT_BUFFER_SIZE        4096


typedef enum
{
	OTA_IMG_HDR_GET = 0x01,
	OTA_HDR_CHECK = 0x02,
	OTA_CHECKSUM_VERIFY = 0x03,
	OTA_COMPLETE = 0x04
} T_OTA_CTRL_INDI_RCV_OP;

typedef enum
{
	OTA_HDR_OK = 0x01,
	OTA_HDR_ERR_CHECKSUM,
	OTA_HDR_ERR_INFO,
	OTA_HDR_ERR_LEN
} T_OTA_CLIENT_RECV_HDR_CHECK_RESULT;

typedef enum
{
	OTA_CHECKSUM_OK = 0x01,
	OTA_CHECKSUM_ERR_DATA,
	OTA_CHECKSUM_ERR_IMAGE,
	OTA_CHECKSUM_ERR_OTHERS
} T_OTA_CLIENT_RECV_CHECKSUM_VERIFY_RESULT;

/** @brief BLE OTA client handle type*/
typedef enum
{
    HDL_OTA_SRV_START,     //!< start handle of ota service
    HDL_OTA_SRV_END,       //!< end handle of ota service
    HDL_OTA_IMG_HDR,       //!< ota header characteristic value handle
    HDL_OTA_HDR_CHECKSUM,  //!< ota header checksum characteristic value handle
    HDL_OTA_DATA,          //!< ota data characteristic value handle
    HDL_OTA_DATA_CHECKSUM, //!< ota data checksum characteristic value handle
    HDL_OTA_CTRL,          //!< ota ctrl characteristic value handle
    HDL_OTA_CTRL_CCCD,     //!< ota ctrl characteristic CCCD handle
    HDL_OTA_CACHE_LEN
} T_OTA_HANDLE_TYPE;

/** @brief BLE OTA client discovery state*/
typedef enum
{
    DISC_OTA_IDLE,
    DISC_OTA_START,
    DISC_OTA_DONE,
    DISC_OTA_FAILED
} T_OTA_DISC_STATE;

/** @brief BLE OTA client write type*/
typedef enum
{
    OTA_WRITE_IMG_HDR,
    OTA_WRITE_HDR_CHECKSUM,
    OTA_WRITE_DATA,
    OTA_WRITE_DATA_CHECKSUM,
    OTA_WRITE_CTRL_CCCD
} T_OTA_WRTIE_TYPE;

/** @brief BLE OTA client write result*/
typedef struct
{
    T_OTA_WRTIE_TYPE type; //ota write char identification
    uint16_t cause;
} T_OTA_WRITE_RESULT;

/** @brief BLE OTA client notif/ind receive type*/
typedef enum
{
    OTA_INDI_RECEIVE_OTA_CTRL_INDICATE = 1,
} T_OTA_NOTIF_IND_TYPE;

/** @brief BLE OTA client notif/ind receive data*/
typedef struct
{
    uint16_t value_size;
    uint8_t *p_value;
} T_OTA_NOTIF_IND_VALUE;

/** @brief OTA client notif/ind receive content*/
typedef struct
{
    T_OTA_NOTIF_IND_TYPE type;
    T_OTA_NOTIF_IND_VALUE data;
} T_OTA_NOTIF_IND_DATA;

/** @brief BLE OTA client callback type*/
typedef enum
{
    OTA_CLIENT_CB_TYPE_DISC_STATE,          //!< Discovery procedure state, done or pending.
    //OTA_CLIENT_CB_TYPE_READ_RESULT,         //!< Read request's result data, responsed from server.
    OTA_CLIENT_CB_TYPE_WRITE_RESULT,        //!< Write request result, success or fail.
    OTA_CLIENT_CB_TYPE_NOTIF_IND_RESULT,    //!< Notification or indication data received from server.
    OTA_CLIENT_CB_TYPE_INVALID              //!< Invalid callback type, no practical usage.
} T_OTA_CLIENT_CB_TYPE;

/** @brief BLE OTA client callback content*/
typedef union
{
    T_OTA_DISC_STATE disc_state;
    //T_OTA_READ_RESULT read_result;
    T_OTA_WRITE_RESULT write_result;
    T_OTA_NOTIF_IND_DATA notif_ind_data;
} T_OTA_CLIENT_CB_CONTENT;

/** @brief BLE OTA client callback data */
typedef struct
{
    T_OTA_CLIENT_CB_TYPE     cb_type;
    T_OTA_CLIENT_CB_CONTENT  cb_content;
} T_OTA_CLIENT_CB_DATA;


/***************************************************************************/
typedef struct
{
	uint16_t mtu_size;
	uint8_t index;                 //index of current handling ota fw image info
	uint8_t hdr_total_len;         //ota file header total len
	uint8_t hdr_num;               //the ota header number in ota file
	uint32_t image_offset;         //start position of fw image in image file
	uint32_t cur_offset;           //have sended data offset (without header)
	uint32_t cur_image_len;        //current image length
	uint32_t ota_image_total_len;  // total_len=image1 len( +image2 len)
} ota_msg_t;


extern uint8_t bt_ota_push_image_status;
extern ota_msg_t ota_tx_info;


void bt_ota_client_set_image_param(uint8_t index);

void bt_ota_client_send_ota_header(uint8_t conn_id);

void bt_ota_client_send_image_checksum(uint8_t conn_id);

void bt_ota_client_push_image(uint8_t conn_id);

bool bt_ota_client_enable_indicate_cccd(uint8_t conn_id, bool indicate);

bool bt_ota_client_start_discovery(uint8_t conn_id);

T_CLIENT_ID bt_ota_add_client(P_FUN_GENERAL_APP_CB app_cb, uint8_t link_num);

void bt_ota_delete_client(void);


#ifdef __cplusplus
}
#endif

#endif