#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <bt_types.h>
#include <bt_ota_client_platform.h>


static ota_hdr_t ota_client_hdr = {0};

void bt_ota_client_handle_image_data(uint8_t *p_header, uint32_t *image_len, uint8_t *hdr_len)
{
	uint8_t i = 0;
	uint8_t offset = 0;
	uint32_t img_len = 0;

	memset(&ota_client_hdr, 0, sizeof(ota_client_hdr));

	LE_ARRAY_TO_UINT32(ota_client_hdr.HdrVer, p_header + offset);
	offset += 4;
	LE_ARRAY_TO_UINT32(ota_client_hdr.HdrNum, p_header + offset);
	offset += 4;

	for (i = 0; i < ota_client_hdr.HdrNum; i++) {
		memcpy(&ota_client_hdr.ota_img_hdr[i].Signature[0], p_header + offset, 4);
		offset += 4;

		LE_ARRAY_TO_UINT32(ota_client_hdr.ota_img_hdr[i].HdrLen, p_header + offset);
		offset += 4;
		LE_ARRAY_TO_UINT32(ota_client_hdr.ota_img_hdr[i].Checksum, p_header + offset);
		offset += 4;
		LE_ARRAY_TO_UINT32(ota_client_hdr.ota_img_hdr[i].ImgLen, p_header + offset);
		offset += 4;
		LE_ARRAY_TO_UINT32(ota_client_hdr.ota_img_hdr[i].Offset, p_header + offset);
		offset += 4;

		img_len += ota_client_hdr.ota_img_hdr[i].ImgLen; //ota image total lenth
	}

	*hdr_len = OTA_FILE_HDR_LEN + OTA_FILE_IMG_HDR_LEN * i; //ota image header total lenth
	*image_len = img_len;
}

uint32_t bt_ota_client_get_image_offset(uint8_t index)
{
	return ota_client_hdr.ota_img_hdr[index].Offset;
}

uint32_t bt_ota_client_get_image_len(uint8_t index)
{
	return ota_client_hdr.ota_img_hdr[index].ImgLen;
}

uint32_t bt_ota_client_get_header_num(void)
{
	return ota_client_hdr.HdrNum;
}

uint8_t* bt_ota_client_get_image(void)
{
	uint8_t *address = (uint8_t *)OTA_IMAGE_FLASH_ADDRESS;
	return address;
}

