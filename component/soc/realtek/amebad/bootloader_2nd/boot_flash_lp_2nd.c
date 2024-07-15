/*
 *  Routines to access hardware
 *
 *  Copyright (c) 2013 Realtek Semiconductor Corp.
 *
 *  This module is a confidential and proprietary property of RealTek and
 *  possession or use of this module requires written permission of RealTek.
 */

#include "ameba_soc.h"

extern u8 Force_OTA1_GPIO;
extern u32 OTA_Region[2];
extern FuncPtr FwCheckCallback;
extern FuncPtr OTASelectHook;

extern u8 __ls_flash_text_start__[];
extern u8 __hs_flash_text_start__[];

FLASH_BOOT_TEXT_SECTION
static u32 BOOT_FLASH_OTA_Force1Check(void)
{
	GPIO_InitTypeDef  GPIO_InitStruct;
	u32 gpio_pin;
	u32 force_to_OTA1 = FALSE;
	u32 active_state;

	/* polling GPIO to check if user force boot to OTA1 */
	gpio_pin = Force_OTA1_GPIO;

	if (gpio_pin != 0xff) {
		/* pin coding: [7]: active level, [6:5]: port, [4:0]: pin */
		GPIO_InitStruct.GPIO_Pin = gpio_pin & 0x3F;/* pin is 6bits */
		GPIO_InitStruct.GPIO_Mode = GPIO_Mode_IN;

		if (gpio_pin & 0x80) {
			/*  High active */			
			GPIO_InitStruct.GPIO_PuPd = GPIO_PuPd_DOWN;
			active_state = GPIO_PIN_HIGH;
		} else {
			/*  Low active */
			GPIO_InitStruct.GPIO_PuPd = GPIO_PuPd_UP;
			active_state = GPIO_PIN_LOW;
		}

		GPIO_Init(&GPIO_InitStruct);
		if (GPIO_ReadDataBit(GPIO_InitStruct.GPIO_Pin) == active_state) {
			force_to_OTA1 = 1;
		} else {
			force_to_OTA1 = 0;
		}
		GPIO_DeInit(GPIO_InitStruct.GPIO_Pin);
	}

	return (force_to_OTA1);
}

FLASH_BOOT_TEXT_SECTION
static u8 BOOT_FLASH_OTA_MMU(u8 idx, u32 vAddr, u32 pAddr, u32 *size)
{
	IMAGE_HEADER *Img2Hdr, *Img2DataHdr, *PsramHdr;
	u32 ImgSize, IsMinus, Offset, SBheader;
	u8 res = _TRUE;

	/* Calculate mapping offset */
	if(vAddr > pAddr) {
		IsMinus = 1;
		Offset = vAddr - pAddr;
	} else {
		IsMinus = 0;
		Offset = pAddr - vAddr;
	}

	/* Mapping a memory from virtual address to physical address, which is large enough 
		to get Code/Data header information. */
	RSIP_MMU_Config(idx, vAddr, vAddr + 0x02000000 - 1, IsMinus, Offset);
	RSIP_MMU_Cmd(idx, ENABLE);

	/* Verify signature */
	Img2Hdr = (IMAGE_HEADER *)vAddr;

	if((Img2Hdr->signature[0] != 0x35393138) || (Img2Hdr->signature[1] != 0x31313738)) {
		res = _FALSE;
		goto exit;
	}

	/* Get image size, which should be 4KB-aligned */
	Img2DataHdr = (IMAGE_HEADER *)(vAddr + IMAGE_HEADER_LEN + Img2Hdr->image_size);
	ImgSize = Img2Hdr->image_size + Img2DataHdr->image_size + IMAGE_HEADER_LEN * 2;

	/* KM4 image contains PSRAM code+data part */
	if(idx == 1) {
		PsramHdr = (IMAGE_HEADER *)(vAddr + ImgSize);
		ImgSize += (IMAGE_HEADER_LEN + PsramHdr->image_size);

		/*check if KM4 image contains secure image2 signature*/		
		SBheader = Img2Hdr->sb_header;
		if (SBheader != 0xFFFFFFFF) {
			ImgSize += 0x1000;
		} 
	}

	ImgSize = (((ImgSize -1) >> 12) + 1) << 12;  /* 4KB aligned */

	/* Mapping the current image from virtual address to physical address */
	RSIP_MMU_Config(idx, vAddr, vAddr + ImgSize - 1, IsMinus, Offset);

	if(size != NULL)
		*size = ImgSize;

exit:
	/* Clean Dcache */
	DCache_Invalidate((u32)Img2Hdr, IMAGE_HEADER_LEN);
	DCache_Invalidate((u32)Img2DataHdr, IMAGE_HEADER_LEN);

	return res;
}

FLASH_BOOT_TEXT_SECTION
static u8 BOOT_FLASH_OTA_Check(u8 ota_idx)
{
	u32 LsSize;
	u8 res;

	/* Mapping KM0 IMG2 to OTA1*/
	res = BOOT_FLASH_OTA_MMU(0, (u32)__ls_flash_text_start__ - IMAGE_HEADER_LEN, OTA_Region[ota_idx], &LsSize);
	if(res == _FALSE)
		return res;

	/* Mapping KM4 IMG2 to OTA1, should be 4 KB aligned */
	res = BOOT_FLASH_OTA_MMU(1, (u32)__hs_flash_text_start__ - IMAGE_HEADER_LEN, OTA_Region[ota_idx] + LsSize, NULL);
	if(res == _FALSE) {
		while(1) {
			DBG_8195A("\n\nWarnning!!\nAmebaD Flash Memory Layout is modified!\n"
				"Please download km0_km4_image2.bin instead of km0_image2_all.bin & km4_image2_all.bin!!\n\n"
				"Location: project/realtek_amebaD_cm4_gcc_verification/asdk/image\n\n\n");
			DelayMs(2000);
		}
		return res;
	}
	
	/* Check hash/sum if needed */
	if((FwCheckCallback != NULL) && (FwCheckCallback() != _TRUE))
		res = _FALSE;
	else
		res = _TRUE;

	return res;
}

FLASH_BOOT_TEXT_SECTION
static u32 BOOT_FLASH_OTA_Select(void)
{
	u8 OtaValid;
	u8 OtaSelect;
	
	/* check OTA1 force pin trigger */ 
	if (BOOT_FLASH_OTA_Force1Check() == _TRUE) {
		DBG_PRINTF(MODULE_BOOT, LEVEL_INFO,"GPIO force OTA1 \n");
	} else {
		/* check OTA2 if valid or not */
		OtaValid = BOOT_FLASH_OTA_Check(OTA_INDEX_2);
		if(OtaValid == _TRUE) {
			OtaSelect = 2;
			DBG_PRINTF(MODULE_BOOT, LEVEL_INFO,"OTA2 USE\n");
			goto exit;
		}
	}

	/* check OTA1 if valid or not */
	OtaValid = BOOT_FLASH_OTA_Check(OTA_INDEX_1);
	if(OtaValid == _TRUE) {
		OtaSelect = 1;
		DBG_PRINTF(MODULE_BOOT, LEVEL_INFO,"OTA1 USE\n");
	} else {
		OtaSelect = 0;
		RSIP_MMU_Cmd(0, DISABLE);
		RSIP_MMU_Cmd(1, DISABLE);
	}

exit: 
	return OtaSelect;
}

//3 Image 1
FLASH_BOOT_TEXT_SECTION
void BOOT_FLASH_Image1_2nd(void)
{
	IMAGE_HEADER *img2_hdr = NULL;
	IMAGE_HEADER *FlashImage2DataHdr = NULL;
	PRAM_START_FUNCTION Image2EntryFun = NULL;
	u32 BssLen = 0;
	u32 Temp = 0;
	u8* image2_validate_code = NULL;

	DBG_PRINTF(MODULE_BOOT, LEVEL_INFO,"BOOT2ND START\n");

	BOOT2_RAM_SectionInit();

	img2_hdr = (IMAGE_HEADER *)((__ls_flash_text_start__) - IMAGE_HEADER_LEN);
	Image2EntryFun = (PRAM_START_FUNCTION)__image2_entry_func__;
	image2_validate_code = (u8*)((u32)__image2_entry_func__ + sizeof(RAM_START_FUNCTION));
	BssLen = (__image1_bss_end__ - __image1_bss_start__);

	DBG_PRINTF(MODULE_BOOT, LEVEL_INFO,"BOOT2ND ENTER ROMSUB:%d\n", (SYSCFG_ROMINFO_Get() >> 8) & 0xFF);

	_memset((void *) __image1_bss_start__, 0, BssLen);

	/* backup flash_init_para address for KM4 */
	BKUP_Write(BKUP_REG7, (u32)&flash_init_para);

	if (OTASelectHook == NULL)
		Temp = BOOT_FLASH_OTA_Select();
	else
		Temp = OTASelectHook();
	
	if(Temp == 0) {
		DBG_PRINTF(MODULE_BOOT, LEVEL_ERROR,"IMG2 Invalid\n");
		goto INVALID_IMG2;
	}

	/* Download image 2 */
	FlashImage2DataHdr = (IMAGE_HEADER *)(__ls_flash_text_start__ + img2_hdr->image_size);

	DBG_PRINTF(MODULE_BOOT, LEVEL_INFO,"IMG2 DATA[0x%x:%d:0x%x]\n", (u32)(FlashImage2DataHdr + 1),
		FlashImage2DataHdr->image_size, FlashImage2DataHdr->image_addr);

	if (FlashImage2DataHdr->image_addr == 0xFFFFFFFF || FlashImage2DataHdr->image_size > 0x100000) {
		DBG_PRINTF(MODULE_BOOT, LEVEL_ERROR,"IMG2 ADDR Invalid\n");
		goto INVALID_IMG2;
	}


	/* load image2 data into RAM */
	_memcpy((void*)FlashImage2DataHdr->image_addr, (void*)(FlashImage2DataHdr + 1), FlashImage2DataHdr->image_size);
	
	/* Jump to image 2 */
	DBG_PRINTF(MODULE_BOOT, LEVEL_INFO,"IMG2 SIGN[%s(%x)]\n",image2_validate_code, (u32)image2_validate_code);
	DBG_PRINTF(MODULE_BOOT, LEVEL_INFO,"IMG2 ENTRY[0x%x:0x%x]\n",__image2_entry_func__, Image2EntryFun->RamStartFun);

	if (_strcmp((const char *)image2_validate_code, (const char *)"RTKWin")) {
		DBG_PRINTF(MODULE_BOOT, LEVEL_ERROR,"IMG2 SIGN Invalid\n");
		goto INVALID_IMG2;
	}

	/* goto IMG2 */
	Image2EntryFun->RamStartFun();

	return;


INVALID_IMG2:
	while (1) {
		shell_rom(1000);//each delay is 100us
	}
}
