/*
 *  Routines to access hardware
 *
 *  Copyright (c) 2013 Realtek Semiconductor Corp.
 *
 *  This module is a confidential and proprietary property of RealTek and
 *  possession or use of this module requires written permission of RealTek.
 */

#include "ameba_soc.h"

//#define SDM32K_NEW Please close it in rtl8721dlp_clk.c

#if defined ( __ICCARM__ )
#pragma section=".ram_boot2nd.bss"
#pragma section=".ram_image2.entry"


BOOT_RAM_RODATA_SECTION u8* __image2_entry_func__ = 0;
#endif

BOOT_RAM_TEXT_SECTION
PRAM_START_FUNCTION BOOT2_RAM_SectionInit(void)
{
#if defined ( __ICCARM__ )
	// only need __bss_start__, __bss_end__
	__image2_entry_func__		= (u8*)__section_begin(".ram_image2.entry");
#endif
	return (PRAM_START_FUNCTION)__image2_entry_func__;
}

IMAGE1_VALID_PATTEN_SECTION
const u8 RAM_BOOT2_VALID_PATTEN[20] = {
	'R', 'T', 'K', 'W', 'i', 'n', 0x0, 0xff, 
	(FW_VERSION&0xff), ((FW_VERSION >> 8)&0xff),
	(FW_SUBVERSION&0xff), ((FW_SUBVERSION >> 8)&0xff),
	(FW_CHIP_ID&0xff), ((FW_CHIP_ID >> 8)&0xff),
	(FW_CHIP_VER),
	(FW_BUS_TYPE),
	(FW_INFO_RSV1),
	(FW_INFO_RSV2),
	(FW_INFO_RSV3),
	(FW_INFO_RSV4)
};

IMAGE1_ENTRY_SECTION
RAM_START_FUNCTION boot2EntryFun0 = {
	BOOT_FLASH_Image1_2nd,
		NULL,//SOCPS_WakeFromPG
		NULL,
	
};

