#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <bt_types.h>
#include <bt_ota_server_platform.h>
#include <device_lock.h>
#include <rtl8721d.h>
#include <os_mem.h>
#include <os_sched.h>


static uint32_t IMG_ADDR[OTA_FILE_IMAGE_MAX_NUM][2] = {
	{IMG2_OTA1_ADDR, IMG2_OTA2_ADDR},
};

static ota_update_header_info_t OtaHdrMsg = {0};
extern void ota_platform_reset(void);
extern u32 ota_get_cur_index(void);
extern int  ota_writestream_user(u32 address, u32 len, u8 * data);
extern int  ota_readstream_user(u32 address, u32 len, u8 * data);


/**
  * @brief	 erase the flash space for new firmware.
  * @param   addr: new image address
  * @param   len: new image length
  * @retval  none
  */
void bt_ota_erase_target_flash(uint32_t addr, uint32_t len)
{
	uint32_t sector_cnt;
	sector_cnt = ((len - 1) / OTA_FLASH_SECTOR) + 1;

	device_mutex_lock(RT_DEV_LOCK_FLASH);
	for(int i = 0; i < sector_cnt; i++) {
		FLASH_EraseXIP(EraseSector, addr + i * OTA_FLASH_SECTOR);
	}

	device_mutex_unlock(RT_DEV_LOCK_FLASH);
}

void bt_ota_get_flash_addr(uint32_t *flash_addr)
{
	uint8_t target_idx = OTA_ADDR_INDEX_1;

	if (ota_get_cur_index() == OTA_ADDR_INDEX_1) {
		target_idx = OTA_ADDR_INDEX_2;
	} else {
		target_idx = OTA_ADDR_INDEX_1;
	}

	for (int i = 0; i < OtaHdrMsg.ValidImgCnt; i++) {
		flash_addr[i] = IMG_ADDR[i][target_idx] - SPI_FLASH_BASE;
		bt_ota_erase_target_flash(flash_addr[i], OtaHdrMsg.ImgHdr[i].ImgLen);
		printf("Image[%d]: OTA%d flash space will be upgreded, flash start 0x%08x\r\n", i, target_idx + 1, flash_addr[i]);
	}
}

void bt_ota_write_flash(uint32_t addr, uint8_t *buf, uint16_t len)
{
	device_mutex_lock(RT_DEV_LOCK_FLASH);
	if(ota_writestream_user(addr, len, buf) < 0) {
		printf("%s: Write sector failed\r\n", __func__);
	}
	device_mutex_unlock(RT_DEV_LOCK_FLASH);
}

uint32_t bt_ota_get_image_len(uint32_t *image_len)
{
	int i = 0;
	uint32_t image_total_len = 0;

	for (i = 0; i < OtaHdrMsg.ValidImgCnt; i++) {
		image_len[i] = OtaHdrMsg.ImgHdr[i].ImgLen;
		image_total_len += OtaHdrMsg.ImgHdr[i].ImgLen;
	}

	return image_total_len;
}

void bt_ota_save_image_signature(uint8_t *buf, uint8_t index)
{
	memcpy(&OtaHdrMsg.Sign[index][0], buf, 8);
	memset(buf, 0xFF, 8);
}

void bt_ota_update_image_signature(uint32_t *addr)
{
	int index = 0;
	uint8_t *signature = NULL;
	uint8_t empty_sig = 0;
	uint32_t ota_cur_index = OTA_ADDR_INDEX_1;
	uint32_t ota_cur_addr = 0;

	ota_cur_index = ota_get_cur_index();

	device_mutex_lock(RT_DEV_LOCK_FLASH);

	for (index = 0; index < OtaHdrMsg.ValidImgCnt; index++) {
		signature = &OtaHdrMsg.Sign[index][0];

		/* write the signature finally */
		if(FLASH_WriteStream(addr[index], 8, signature) < 0){
			printf("%s: Write sector failed\r\n", __func__);
			device_mutex_unlock(RT_DEV_LOCK_FLASH);
			return;
		}
	}

	for (index = 0; index < OtaHdrMsg.ValidImgCnt; index++) {
		ota_cur_addr = IMG_ADDR[index][ota_cur_index];

		/*clear the old FW signature finally*/
		if(FLASH_WriteStream(ota_cur_addr - SPI_FLASH_BASE, 4, &empty_sig) < 0){
			printf("%s: clear old fw signature failed\r\n", __func__);
			break;
		}
	}

	device_mutex_unlock(RT_DEV_LOCK_FLASH);
}

bool bt_ota_image_checksum_verify(uint32_t *addr)
{
	int index, offset, i;
	uint32_t ImgLen = 0;
	uint32_t len = 0;
	uint32_t flash_checksum = 0;
	uint8_t *signature;
	uint8_t *p_buf;
	bool ret = true;

	p_buf = (uint8_t *)os_mem_alloc(RAM_TYPE_DATA_ON, OTA_FLASH_SECTOR);
	memset(p_buf, 0, OTA_FLASH_SECTOR);

	for (index = 0; index < OtaHdrMsg.ValidImgCnt; index++) {
		signature = OtaHdrMsg.Sign[index];
		ImgLen = OtaHdrMsg.ImgHdr[index].ImgLen;

		/* read data block from falsh and calculate checksum */
		for (offset = 8; offset < ImgLen; offset += OTA_FLASH_SECTOR) {
			len = (ImgLen - offset) > OTA_FLASH_SECTOR ? OTA_FLASH_SECTOR : (ImgLen - offset);
			ota_readstream_user(addr[index] + offset, len, p_buf);

			for (i = 0; i < len; i++) {
				flash_checksum += p_buf[i];
			}
		}

		/* add signature's checksum */
		for (i = 0; i < 8; i++) {
			flash_checksum += signature[i];
		}

		/* image checksum verify */
		if (flash_checksum != OtaHdrMsg.ImgHdr[index].Checksum) {
			printf("%s: OTA image[%d] checksum error, flash_checksum 0x%08x, img_checksum 0x%08x\r\n", __func__,
				   index, flash_checksum, OtaHdrMsg.ImgHdr[index].Checksum);
			ret = false;
			break;
		}
	}

	os_mem_free(p_buf);
	return ret;
}

bool bt_ota_parse_target_header(uint8_t *p_value, uint16_t len, ota_update_header_info_t *ota_header_info)
{
	if (len < OTA_FILE_HEADER_LEN + OTA_FILE_IMAGE_HEADER_LEN || len > OTA_FILE_HEADER_LEN_MAX) {
		printf("%s: header length %d error!\r\n", __func__, len);
		return false;
	}

	LE_ARRAY_TO_UINT32(ota_header_info->FileHdr.HdrVer, p_value);
	p_value += 4;
	LE_ARRAY_TO_UINT32(ota_header_info->FileHdr.HdrNum, p_value);
	p_value += 4;

	for (int i = 0; i < ota_header_info->FileHdr.HdrNum; i++)
	{
		memcpy(&ota_header_info->ImgHdr[i].Signature[0], p_value, 4);
		p_value += 4;

		if (memcmp("OTA", (char *)(ota_header_info->ImgHdr[i].Signature), 3) == 0)
		{
			LE_ARRAY_TO_UINT32(ota_header_info->ImgHdr[i].HdrLen, p_value);
			p_value += 4;
			LE_ARRAY_TO_UINT32(ota_header_info->ImgHdr[i].Checksum, p_value);
			p_value += 4;
			LE_ARRAY_TO_UINT32(ota_header_info->ImgHdr[i].ImgLen, p_value);
			p_value += 4;
			LE_ARRAY_TO_UINT32(ota_header_info->ImgHdr[i].Offset, p_value);
			p_value += 4;
			memcpy(&ota_header_info->ImgHdr[i].rsvd[0], p_value, 4); //reserved
			p_value += 4;

			ota_header_info->ValidImgCnt += 1;
		}
		else
		{
			printf("%s: invalid ota image[%d]!\r\n", __func__, i);
			return false;
		}
	}

	if (ota_header_info->ValidImgCnt == 0) {
		printf("%s: no valid image!\r\n", __func__);
		return false;
	}

	return true;
}

uint8_t bt_ota_image_retransmission_check(ota_update_header_info_t *ota_hdr)
{
	if (ota_hdr->FileHdr.HdrVer != OtaHdrMsg.FileHdr.HdrVer) {
		/*the received ota image check as a new ota image, update the ota image header info as new info here*/
		memset(&OtaHdrMsg, 0, sizeof(ota_update_header_info_t));
		memcpy(&OtaHdrMsg, ota_hdr, sizeof(ota_update_header_info_t));

		return 0;
	}

	return 1; //retransmission previous ota image
}

void bt_ota_system_reset(void)
{
	printf("%s: Ready to reset\r\n", __func__);
	os_delay(20);
	ota_platform_reset();
}

