/* this file is build in no secure img, function in this file will call NSC functions */

#include "ameba_soc.h"

#if defined ( __ICCARM__ )
#pragma section=".ram_image3.bss"
#pragma section=".psram.bss"

SECTION(".data") u8* __image3_bss_start__ = 0;
SECTION(".data") u8* __image3_bss_end__ = 0;
SECTION(".data") u8* __psram_bss_start__ = 0;
SECTION(".data") u8* __psram_bss_end__ = 0;
#endif

u32 RandSeedTZ = 0x12345;

void gen_random_seed_s(void)
{
	u16 value, data;
	int i = 0, j = 0;
	u8 random[4], tmp;

	/*Write reg 0x4801C404[7:6] with 0b'11*/
	u32 Temp, Data;
	Data = HAL_READ32(CTC_REG_BASE, 0x404);
	Temp = Data |0x00C0;
	HAL_WRITE32(CTC_REG_BASE, 0x404, Temp);

	ADC_Cmd(DISABLE);

	ADC->ADC_INTR_CTRL = 0;
	ADC_INTClear();
	ADC_ClearFIFO();
	ADC->ADC_CLK_DIV = 0;
	ADC->ADC_CONF = 0;
	ADC->ADC_IN_TYPE = 0;
	ADC->ADC_CHSW_LIST[0] = 8;
	ADC->ADC_CHSW_LIST[1] = 0;
	ADC->ADC_IT_CHNO_CON = 0;
	ADC->ADC_FULL_LVL = 0;
	ADC->ADC_DMA_CON = 0x700;
	ADC->ADC_DELAY_CNT = 0x00000000;

	ADC_Cmd(ENABLE);

	for(i = 0; i < 4; i++) {
retry:
		tmp = 0;
		for(j = 0; j < 8; j++) {
			ADC_SWTrigCmd(ENABLE);
			while(ADC_Readable() == 0);
			ADC_SWTrigCmd(DISABLE);

			value = ADC_Read();
			data = value & BIT_MASK_DAT_GLOBAL;
			tmp |= ((data & BIT(0)) << j);
		}

		if(tmp == 0 || tmp == 0xFF)
			goto retry;

		random[i] = tmp;
	}

	ADC_Cmd(DISABLE);

	/*Restore reg 0x4801C404[7:6] of initial value 0b'00*/
	HAL_WRITE32(CTC_REG_BASE, 0x404, Data);

	RandSeedTZ = (random[3] << 24) | (random[2] << 16) | (random[1] << 8) | (random[0]);

	return;
}

IMAGE3_ENTRY_SECTION
void NS_ENTRY BOOT_IMG3(void)
{
#if defined ( __ICCARM__ )
	__image3_bss_start__			= (u8*)__section_begin(".ram_image3.bss");
	__image3_bss_end__ 				= (u8*)__section_end(".ram_image3.bss");
#endif

	DBG_8195A("BOOT_IMG3: BSS [%08x~%08x] SEC: %x \n", __image3_bss_start__, __image3_bss_end__,
		TrustZone_IsSecure());
	
	/* reset img3 bss */
	_memset((void *) __image3_bss_start__, 0, (__image3_bss_end__ - __image3_bss_start__));

	/* generate random seed in trustzone */
	gen_random_seed_s();
}

IMAGE3_ENTRY_SECTION
NS_ENTRY void load_psram_image_s(void)
{
	PRAM_FUNCTION_START_TABLE pRamStartFun = (PRAM_FUNCTION_START_TABLE)__ram_start_table_start__;
	u32 RDPPsramStartAddr = pRamStartFun->ExportTable->psram_s_start_addr;
	u8 Key[16], CompKey[16];
	u8 RdpStatus = 0;
	u8 *buf;
	u8 i;

	/* Read key from Efuse 0x170~0x17F */ 
	for (i = 0; i < 16; i++) {
		EFUSERead8(HAL_READ32(SYSTEM_CTRL_BASE, REG_HS_EFUSE_CTRL1_S),
			EFUSE_RDP_KEY_ADDR + i, &Key[i], L25EOUTVOLTAGE);
	}

	_memset(CompKey, 0xFF, 16);
	if(_memcmp(CompKey, Key, 16) == 0) {
		RdpStatus = 0;						/* key empty */
	} else {
		_memset(CompKey, 0x0, 16);
		if(_memcmp(CompKey, Key, 16) == 0) {
			RdpStatus = 1;					/* key invalid */
		} else
			RdpStatus = 2;					/* key valid */
	}

	if(RdpStatus == 2) {
		buf = (u8*)pvPortMalloc(4096);
		pRamStartFun->ExportTable->rdp_decrypt_func(RDPPsramStartAddr, Key, 1, buf);
		vPortFree(buf);

	} else if (RdpStatus == 0) {  /* MP image*/
		u32 OTF_Enable = HAL_READ32(SYSTEM_CTRL_BASE_LP, REG_SYS_EFUSE_SYSCFG3) & BIT_SYS_FLASH_ENCRYPT_EN;

		IMAGE_HEADER *Img3SecureHdr = (IMAGE_HEADER *)RDPPsramStartAddr;

		/* RSIP mask to prevent psram IMG3 from being decrypted. If customers use RDP+RSIP function, IMG3 only need
			RDP encryption, and don't need RSIP encryption. */
		if(OTF_Enable) {
			RSIP_OTF_Mask(3, (u32)Img3SecureHdr, 1, ENABLE);
			RSIP_OTF_Mask(3, (u32)Img3SecureHdr, (((Img3SecureHdr->image_size + IMAGE_HEADER_LEN) - 1) >> 12) + 1, ENABLE);
		}

		if((Img3SecureHdr->signature[0] == 0x35393138) && (Img3SecureHdr->signature[1] == 0x31313738)) {			
			DBG_PRINTF(MODULE_BOOT, LEVEL_INFO,"IMG3 PSRAM_S:[0x%x:%d:0x%x]\n", Img3SecureHdr->image_addr, Img3SecureHdr->image_size, (void*)(Img3SecureHdr+1));
			_memcpy((void*)Img3SecureHdr->image_addr, (void*)(Img3SecureHdr+1), Img3SecureHdr->image_size);
		}

		/* Relase temporary-used RSIP Mask entry */
		if(OTF_Enable)
			RSIP_OTF_Mask(3, 0, 0, DISABLE);
	}
	/*init psram bss for secure area*/	
#if defined ( __ICCARM__ )
	__psram_bss_start__ = (u8*)__section_begin(".psram.bss");
	__psram_bss_end__	= (u8*)__section_end(".psram.bss"); 
#endif

	memset(__psram_bss_start__, 0, __psram_bss_end__ - __psram_bss_start__);	
	DBG_PRINTF(MODULE_BOOT, LEVEL_INFO, "IMG3 PSRAM_S: BSS [%08x~%08x]\n", __psram_bss_start__, __psram_bss_end__);
}

IMAGE3_ENTRY_SECTION
NS_ENTRY void mpu_s_no_cache_init(void)
{
	mpu_region_config mpu_cfg;
	u32 mpu_entry = 0;

	mpu_init();

	/* set Security PSRAM Memory Write-Back */
    mpu_entry = mpu_entry_alloc();
    mpu_cfg.region_base = 0x02000000;
    mpu_cfg.region_size = 0x400000;
    mpu_cfg.xn = MPU_EXEC_ALLOW;
    mpu_cfg.ap = MPU_UN_PRIV_RW;
    mpu_cfg.sh = MPU_NON_SHAREABLE;
    mpu_cfg.attr_idx = MPU_MEM_ATTR_IDX_WB_T_RWA;
    mpu_region_cfg(mpu_entry, &mpu_cfg);

}
