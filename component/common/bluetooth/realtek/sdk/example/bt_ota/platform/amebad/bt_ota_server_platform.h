/*
 *******************************************************************************
 * Copyright(c) 2023, Realtek Semiconductor Corporation. All rights reserved.
 *******************************************************************************
 * note: amebad ble ota server platform include file
 */

#ifndef _BT_OTA_SERVER_PLATFORM_H_
#define _BT_OTA_SERVER_PLATFORM_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>

#define OTA_FLASH_SECTOR  4096

#define OTA_FILE_IMAGE_MAX_NUM      2
#define OTA_FILE_HEADER_LEN         8
#define OTA_FILE_IMAGE_HEADER_LEN   24
#define OTA_FILE_HEADER_LEN_MAX     (OTA_FILE_IMAGE_MAX_NUM * OTA_FILE_IMAGE_HEADER_LEN + OTA_FILE_HEADER_LEN)


#define IMG2_OTA1_ADDR	0x08006000				/* KM0 IMG2 OTA1 start address*/
#define IMG2_OTA2_ADDR	0x08106000				/* KM0 IMG2 OTA2 start address*/

#define OTA_ADDR_INDEX_1  0
#define OTA_ADDR_INDEX_2  1



/**
  * @brief  OTA update file header structure definition
  */
typedef struct
{
	uint32_t HdrVer;  /* the version of OTA header */
	uint32_t HdrNum;  /* the number of OTA header */
} ota_file_hdr_t;

/**
  * @brief  OTA update file image header structure definition
  */
typedef struct
{
	uint8_t Signature[4];  /* the OTA signature, this value is "OTA" */
	uint32_t HdrLen;       /* the length of OTA header */
	uint32_t Checksum;     /* the checksum of OTA fw image */
	uint32_t ImgLen;       /* the length of OTA fw image */
	uint32_t Offset;       /* the start position of OTA fw image in the OTA file */
	uint8_t rsvd[4];
} ota_file_image_hdr_t;

/**
  * @brief  OTA update file info structure definition
  */
typedef struct
{
	ota_file_hdr_t FileHdr;                                   /* ota update file header */
	ota_file_image_hdr_t ImgHdr[OTA_FILE_IMAGE_MAX_NUM];      /* ota update file target image header */
	uint8_t Sign[OTA_FILE_IMAGE_MAX_NUM][9];			      /* Specifies the signature of target image */
	uint8_t ValidImgCnt;                                      /* valid image number in ota file, 0<=ValidImgCnt<=HdrNum */
} ota_update_header_info_t;


void bt_ota_erase_target_flash(uint32_t addr, uint32_t len);
void bt_ota_get_flash_addr(uint32_t *flash_addr);
void bt_ota_write_flash(uint32_t addr, uint8_t *buf, uint16_t len);
uint32_t bt_ota_get_image_len(uint32_t *image_len);
void bt_ota_save_image_signature(uint8_t *buf, uint8_t index);
void bt_ota_update_image_signature(uint32_t *addr);
bool bt_ota_image_checksum_verify(uint32_t *addr);
bool bt_ota_parse_target_header(uint8_t *p_value, uint16_t len, ota_update_header_info_t *ota_header_info);
uint8_t bt_ota_image_retransmission_check(ota_update_header_info_t *ota_hdr);
void bt_ota_system_reset(void);


#ifdef __cplusplus
}
#endif

#endif //_AMEBA_OTA_CONFIG__