/*
 *  Routines to access hardware
 *
 *  Copyright (c) 2013 Realtek Semiconductor Corp.
 *
 *  This module is a confidential and proprietary property of RealTek and
 *  possession or use of this module requires written permission of RealTek.
 */

#include "ameba_soc.h"
#include "psram_reserve.h"


BOOT_RAM_TEXT_SECTION
void BOOT_Init_Psram(void)
{
	u32 temp;
	PCTL_InitTypeDef  PCTL_InitStruct;

	/*set rwds pull down*/
	temp = HAL_READ32(PINMUX_REG_BASE, 0x104);
	temp &= ~(PAD_BIT_PULL_UP_RESISTOR_EN | PAD_BIT_PULL_DOWN_RESISTOR_EN);
	temp |= PAD_BIT_PULL_DOWN_RESISTOR_EN;
	HAL_WRITE32(PINMUX_REG_BASE, 0x104, temp);

	PSRAM_CTRL_StructInit(&PCTL_InitStruct);
	PSRAM_CTRL_Init(&PCTL_InitStruct);

	PSRAM_PHY_REG_Write(REG_PSRAM_CAL_PARA, 0x02030310);

	/*check psram valid*/
	HAL_WRITE32(PSRAM_BASE, 0, 0);
	assert_param(0 == HAL_READ32(PSRAM_BASE, 0));

	if(_FALSE == PSRAM_calibration())
		return;

	if(FALSE == psram_dev_config.psram_dev_cal_enable) {
		temp = PSRAM_PHY_REG_Read(REG_PSRAM_CAL_CTRL);
		temp &= (~BIT_PSRAM_CFG_CAL_EN);
		PSRAM_PHY_REG_Write(REG_PSRAM_CAL_CTRL, temp);
	}
}


BOOT_RAM_TEXT_SECTION
void BOOT_FLASH_SIG_MMU(void)
{
	/*  init psram */
	BOOT_Init_Psram();

	u32 vAddrStart, vAddrEnd, ImgSize, IsMinus, Offset, SecureBootOn;
	u32 CtrlTemp = 0;
	RSIP_REG_TypeDef* RSIP = ((RSIP_REG_TypeDef *) RSIP_REG_BASE);
	IMAGE_HEADER *Image2Hdr = NULL;
	IMAGE_HEADER *Image2DataHdr = NULL;
	IMAGE_HEADER *PsramHdr = NULL;


	/* get image header */	
	Image2Hdr = (IMAGE_HEADER *)((__flash_text_start__) - IMAGE_HEADER_LEN);
	Image2DataHdr = (IMAGE_HEADER *)(__flash_text_start__ + Image2Hdr->image_size);
	PsramHdr =  (IMAGE_HEADER *)((u32)Image2DataHdr + IMAGE_HEADER_LEN +Image2DataHdr->image_size);


	/* protection_config load from EFUSE: 0x1C0/0x1C1*/
	SecureBootOn = HAL_READ32(SYSTEM_CTRL_BASE_LP, REG_LP_EFUSE_PROTECTION) & BIT_SECURE_BOOT_DIS;
	/* 0x1C1[5]=1, close SBOOT */
	if ((SecureBootOn & BIT_SECURE_BOOT_DIS) != 0)
		goto exit;


	/* check if signature is already mapped,  0xF90 = 0xFFF - length of sbHeader*/
	ImgSize = Image2Hdr->image_size + Image2DataHdr->image_size + PsramHdr->image_size + IMAGE_HEADER_LEN * 3;
	 if ((ImgSize & 0xFFF) < 0xF90)
	 	goto exit;
	 
	
	vAddrStart = RSIP->FLASH_MMU[1].MMU_ENTRYx_STRADDR;
	vAddrEnd = RSIP->FLASH_MMU[1].MMU_ENTRYx_ENDADDR;
	Offset = RSIP->FLASH_MMU[1].MMU_ENTRYx_OFFSET;
	CtrlTemp = RSIP->FLASH_MMU[1].MMU_ENTRYx_CTRL;
	CtrlTemp &= MMU_BIT_ENTRY_OFFSET_MINUS;
	IsMinus = (CtrlTemp >> 1);


	RSIP_MMU_Cmd(1, DISABLE);

	/* Mapping the current image from virtual address to physical address */
	RSIP_MMU_Config(1, vAddrStart, vAddrEnd + 0x1000, IsMinus, Offset); /* add one more sector */
	RSIP_MMU_Cmd(1, ENABLE);


exit:
	/* Clean Dcache */
	DCache_Invalidate((u32)Image2Hdr, IMAGE_HEADER_LEN);
	DCache_Invalidate((u32)Image2DataHdr, IMAGE_HEADER_LEN);
	DCache_Invalidate((u32)PsramHdr, IMAGE_HEADER_LEN);

	return;
}


BOOT_RAM_TEXT_SECTION
void BOOT_Psram_Load_NS(void)
{
	IMAGE_HEADER *Image2Hdr = NULL;
	IMAGE_HEADER *Image2DataHdr = NULL;
	IMAGE_HEADER *PsramHdr = NULL;

	/* get image header */	
	Image2Hdr = (IMAGE_HEADER *)((__flash_text_start__) - IMAGE_HEADER_LEN);
	Image2DataHdr = (IMAGE_HEADER *)(__flash_text_start__ + Image2Hdr->image_size);
	PsramHdr =  (IMAGE_HEADER *)((u32)Image2DataHdr + IMAGE_HEADER_LEN +Image2DataHdr->image_size);
	
	DBG_PRINTF(MODULE_BOOT, LEVEL_INFO,"IMG2 PSRAM_NS:[0x%x:%d:0x%x]\n", (u32)(PsramHdr + 1),
			PsramHdr->image_size, PsramHdr->image_addr);

	/* load nonsecure psram header+code+data into PSRAM */
	if((PsramHdr->image_size != 0) && \
		(PsramHdr->image_addr == 0x02000020) && \
		(PsramHdr->signature[0] == 0x35393138) && \
		(PsramHdr->signature[1] == 0x31313738)) {

		_memcpy((void*)((PsramHdr->image_addr) - IMAGE_HEADER_LEN), (void*)PsramHdr, (PsramHdr->image_size) + IMAGE_HEADER_LEN);
	}
}


BOOT_RAM_TEXT_SECTION
u32 BOOT_FLASH_Image2SignatureCheck(void)
{
	u32 SecureBootOn = 0;
	u8 pk[32];
	unsigned char SecureBootSig[64];
	int index = 0;
	SB_HEADER *SbHeaderVAddr; 

	IMAGE_HEADER *Image2Hdr = NULL;
	IMAGE_HEADER *Image2DataHdr = NULL;
	IMAGE_HEADER *PsramHdr = NULL;

	/* get image header */	
	Image2Hdr = (IMAGE_HEADER *)((__flash_text_start__) - IMAGE_HEADER_LEN);
	Image2DataHdr = (IMAGE_HEADER *)(__flash_text_start__ + Image2Hdr->image_size);
	PsramHdr =  (IMAGE_HEADER *)((u32)Image2DataHdr + IMAGE_HEADER_LEN +Image2DataHdr->image_size);

	ROM_SECURE_CALL_NS_ENTRY *prom_sec_call_ns_entry = (ROM_SECURE_CALL_NS_ENTRY *)__rom_entry_ns_start__;


	/* protection_config load from EFUSE: 0x1C0/0x1C1*/
	SecureBootOn = HAL_READ32(SYSTEM_CTRL_BASE_LP, REG_LP_EFUSE_PROTECTION) & BIT_SECURE_BOOT_DIS;

	/* 0x1C1[5]=1, close SBOOT */
	if ((SecureBootOn & BIT_SECURE_BOOT_DIS) != 0) {
		DBG_8195A("IMG2 SBOOT OFF\n");
		return 1;
	}

	
	/* get public key*/
	for (index = 0; index < 32; index++) {
		EFUSERead8(0, (SBOOT_PK_ADDR+index), &pk[index], L25EOUTVOLTAGE);
	}

	SbHeaderVAddr = (SB_HEADER *)((u32)PsramHdr + 0x20 + PsramHdr->image_size);


	if((u32)SbHeaderVAddr == 0xFFFFFFFF)
		goto SBOOT_FAIL;

	for (int i = 0; i < 64; i++) {
		SecureBootSig[i] = SbHeaderVAddr->sb_sig[i];
	}

	u32 len = PsramHdr->image_size + IMAGE_HEADER_LEN; //header included


	/* We can call ed25519_verify_signature directly here because all memory address is Secure when boot*/
	if (prom_sec_call_ns_entry->ed25519_verify_signature(SecureBootSig, (const unsigned char *)(PsramHdr->image_addr - 0x20), len, pk) == 0) {
		DBG_8195A("IMG2 SBOOT OK\n");

		return 1;
	} else {
		goto SBOOT_FAIL;
	}

SBOOT_FAIL:
	while (1) {
		DelayMs(1000);
		DBG_8195A("IMG2 SBOOT FAIL\n");
	}
}


BOOT_RAM_TEXT_SECTION
u32 BOOT_FLASH_SBoot(void)
{
	/* load non_secure psram image */
	BOOT_Psram_Load_NS();
	
	/*check image2 signature after loading non_secure psram image*/
	BOOT_FLASH_Image2SignatureCheck();	
}

