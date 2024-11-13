/**
*****************************************************************************************
*     Copyright(c) 2017, Realtek Semiconductor Corporation. All rights reserved.
*****************************************************************************************
  * @file     bt_ota_gatt_service.h
  * @brief    Demonstration of how to implement a self-definition service.
  * @details  Demonstration of different kinds of service interfaces.
  * @author
  * @date
  * @version
  * *************************************************************************************
  */

/* Define to prevent recursive inclusion */
#ifndef _BT_OTA_GATT_SERVICE_H_
#define _BT_OTA_GATT_SERVICE_H_

#ifdef __cplusplus
extern "C"  {
#endif      /* __cplusplus */

/* Add Includes here */
#include <profile_server.h>
#include <bt_ota_server_platform.h>


#define OTA_DATA_BUFFER_SIZE        4096

#define GATT_UUID128_OTA_SERVICE                0x12, 0xA2, 0x4D, 0x2E, 0xFE, 0x14, 0x48, 0x8e, 0x93, 0xD2, 0x17, 0x3C, 0x87, 0xFF, 0x00, 0x00
#define GATT_UUID_CHAR_OTA_IMAGE_HEADER	        0x87FE
#define GATT_UUID_CHAR_OTA_HEADER_CHECKSUM      0x87FD
#define GATT_UUID_CHAR_OTA_DATA	                0x87FC
#define GATT_UUID_CHAR_OTA_DATA_CHECKSUM        0x87FB
#define GATT_UUID_CHAR_OTA_CTRL	                0x87FA

#define OTA_CTRL_INDICATE_ENABLE    1
#define OTA_CTRL_INDICATE_DISABLE   2

#define OTA_HEADER_CHECKSUM_PACKET_LEN    5 //4+1


typedef enum
{
	OTA_NO_DISC,
	OTA_ERR_DISC,
	OTA_COMPLETE_DISC,
	OTA_DISC_STATUS_MAX
} OTA_DISC_STATUS;

typedef enum
{
	OTA_IMAGE_HEADER_GET = 0x01,
	OTA_HEADER_CHECK = 0x02,
	OTA_CHECKSUM_VERIFY = 0x03,
	OTA_COMPLETE = 0x04
} T_OTA_CTRL_IND_OPCODE;

typedef enum
{
	OTA_HDR_CHECK_OK = 0x01,
	OTA_HDR_CHECKSUM_ERR,
	OTA_HDR_INFO_ERR,
	OTA_HDR_LEN_ERR,
} T_OTA_HDR_CHECK_RESULT;

typedef enum
{
	OTA_CHECKSUM_OK = 0x01,
	OTA_DATA_CHECKSUM_ERR,
	OTA_IMAGE_CHECKSUM_ERR,
	OTA_DATA_OFFSET_ERR
} T_OTA_DATA_CHECKSUM_VERIFY_RESULT;

/*************************** ota callback msg defination **************************************/

typedef enum
{
	OTA_IMAGE_HEADER_WRITE = 1,
	OTA_HEADER_CHECKSUM_WRITE = 2,
	OTA_DATA_WRITE = 3,
	OTA_DATA_CHECKSUM_WRITE = 4
} T_OTA_CALLBACK_WRITE_IDX;

typedef struct
{
    T_OTA_CALLBACK_WRITE_IDX opcode;
    T_WRITE_TYPE write_type;
    uint16_t len;
    uint8_t *p_value;
} T_OTA_WRITE_MSG;

typedef union
{
    uint8_t notify_indicate_status;
    T_OTA_WRITE_MSG write;
} T_OTA_UPSTREAM_MSG_DATA;

typedef struct
{
    uint8_t                 conn_id;
    T_SERVICE_CALLBACK_TYPE msg_type;
    T_OTA_UPSTREAM_MSG_DATA msg_data;
} T_OTA_CALLBACK_DATA;

/************************************************************************/

typedef struct
{
	ota_update_header_info_t OtaHdrInfo;
	uint8_t index;                                  /* index of current handling ota fw image */
	//uint8_t target_idx;                           /* (0 - OTA1, 1 - OTA2) which flash space the new fw image will be updated */
	uint8_t OtaCompletedFlag;                       /* the ota process completed flag */
	//uint8_t SysReset;                               /* system reset flag */
	uint32_t flash_addr[OTA_FILE_IMAGE_MAX_NUM];    /* flash address of ota image upgrade */
	uint32_t cur_offset;                            /* received data length of current handling ota fw image */
	uint32_t image_offset;                          /* start position of ota fw image in current image */
	uint32_t image_len[OTA_FILE_IMAGE_MAX_NUM];     /* current image length */
	uint32_t ota_image_total_len;                   /* all image total length */
	uint32_t checksum;                              /* checksum of current handling ota fw image */
} ota_dfu_ctrl_info_t;


void bt_ota_disconnect(uint8_t conn_id);

void bt_ota_update_new_image(void);
bool bt_ota_send_indicate(uint8_t conn_id, T_SERVER_ID service_id, T_OTA_CTRL_IND_OPCODE opcode,
							uint8_t status, uint8_t *value, uint8_t val_len);
void bt_ota_start(uint8_t conn_id, T_SERVER_ID service_id);
void bt_ota_handle_ota_header_packet(uint8_t conn_id, uint8_t *p_value, uint16_t len);
uint8_t bt_ota_handle_ota_header_checksum_packet(uint8_t conn_id, uint8_t *p_value, uint16_t len);
uint8_t bt_ota_handle_ota_image_data_packet(uint8_t conn_id, uint8_t *p_value, uint16_t len);
uint8_t bt_ota_handle_ota_data_checksum_packet(uint8_t conn_id, uint8_t *p_value, uint16_t len);
T_SERVER_ID bt_ota_add_service(void *p_func);


#ifdef __cplusplus
}
#endif

#endif /* _BLE_OTA_SERVICE_H_ */
