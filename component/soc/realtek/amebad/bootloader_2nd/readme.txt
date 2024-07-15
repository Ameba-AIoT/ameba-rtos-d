How to Use KM0 2nd Bootloader with KM4 SRAM:

1. Modify project\realtek_amebaD_va0_example\GCC-RELEASE\project_lp\asdk\rlx8721d_layout.ld, add sram and flash memory layout for 2nd bootloader.
	MEMORY
	{
		...
		BOOTLOADER2_RAM (rwx)  : 	ORIGIN = 0x10005000, LENGTH = 0x10077000 - 0x10005000	/* BOOT2 Loader RAM: 456K */
		...
		KM0_BOOT2 (rx)  :		ORIGIN = 0x08006000+0x20, LENGTH = 0x10000-0x20	/* XIPBOOT2: 64k, 32 Bytes resvd for header*/
		...
	}
	
2. Modify project\realtek_amebaD_va0_example\GCC-RELEASE\project_lp\asdk\ld\rlx8721d_boot.ld.

	Modify:
	
	.ram_image2.entry :
	{
		__ram_image2_text_start__ = .;
		__image2_entry_func__ = .;

	} > BD_RAM
	
	To:

	.ram_boot2nd.entry :
	{
		__ram_boot2nd_text_start__ = .;
		__image2_entry_func__ = .;

	} > BOOTLOADER2_RAM


3. Copy rlx8721d_boot_2nd.ld from component\soc\realtek\amebad\bootloader_2nd to project\realtek_amebaD_va0_example\GCC-RELEASE\project_lp\asdk\ld.

4. Modify component\soc\realtek\amebad\bootloader\boot_flash_lp.c.

	BOOT_RAM_DATA_SECTION
	CPU_PWR_SEQ HSPWR_ON_SEQ[] = {
		{0x4800021C, CPU_PWRSEQ_CMD_WRITE,		(0x011 << 0),			0x00000000}, /* reset 21c[0]&[4]for WDG reset */
		{0x00000000, CPU_PWRSEQ_CMD_LOGE,		0x00000000,			0x00000000/* or 1 */}, /* Enable LOG or not */

		/* 1. Enable SWR */ //PMC
		//{0x48000240, CPU_PWRSEQ_CMD_WRITE,		0x00000000,			(0x01 << 0)},
		//{0x480003F4, CPU_PWRSEQ_CMD_POLLING,	(0x01 << 11),			(0x01 << 11)}, /* temp use delay because FPGA dont have this function */
		//{0x00000000, CPU_PWRSEQ_CMD_DELAY,		0x00000000,			100}, //delay 100us
		//{0x48000240, CPU_PWRSEQ_CMD_WRITE,		0x00000000,			(0x01 << 2)},

		/* 2. Enable xtal/banggap */ //PMC
		//{0x48000260, CPU_PWRSEQ_CMD_WRITE,		0x00000000,			0x03},
		//{0x00000000, CPU_PWRSEQ_CMD_DELAY,		0x00000000,			750}, //delay 750us

		/* 4. Enable HS power */
		{0x48000200, CPU_PWRSEQ_CMD_WRITE,		0x00000000,			(0x01 << 1)},
		{0x48000200, CPU_PWRSEQ_CMD_WRITE,		0x00000000,			(0x01 << 2)},
		{0x00000000, CPU_PWRSEQ_CMD_DELAY,		0x00000000,			200}, //delay 100us
		{0x48000200, CPU_PWRSEQ_CMD_WRITE,		0x00000000,			(0x01 << 3)},
		{0x48000200, CPU_PWRSEQ_CMD_WRITE,		(0x03 << 17),			0x00000000},
		
		/* 5. Enable clock and reset */	
		{0x4800021C, CPU_PWRSEQ_CMD_WRITE,		0x00000000,			(0x01 << 0)},
		{0x4800021C, CPU_PWRSEQ_CMD_WRITE,		0x00000000,			(0x01 << 4)},
		{0x4800021C, CPU_PWRSEQ_CMD_WRITE,		(0x1FF << 16),			(0x101 << 16)}, /* BIT24 is just for FPGA, ASIC dont need */
		{0x4800021C, CPU_PWRSEQ_CMD_WRITE,		0x00000000,			(0x07 << 1)},
		{0x4800021C, CPU_PWRSEQ_CMD_WRITE,		(0x07 << 25),			0x00000000}, //set KM4 200MHz

		/* 1. Recover KM4 Memory power, the memory low power config in sleep_hsram_config[] */
		{0x48000B08, CPU_PWRSEQ_CMD_WRITE,		(0xFFFFFFFF),			0x00000000},

		/* 6. Enable WIFI related */
		{0x48000314, CPU_PWRSEQ_CMD_WRITE,		0x00000000,			0x00000001}, // enable HW auto response

		/* 7. HS IPC Enable*/
		{0x40000200, CPU_PWRSEQ_CMD_WRITE,		0x00000000,	    		BIT_HS_IPC_FEN}, //function enable
		{0x40000210, CPU_PWRSEQ_CMD_WRITE,		0x00000000,	    		BIT_HS_IPC_CKE}, //clock enable

		/* End */
		{0xFFFFFFFF, CPU_PWRSEQ_CMD_END,			0x00000000,			0x00000000},
	};
	
   //3 Image 1
	FLASH_BOOT_TEXT_SECTION
	void BOOT_FLASH_Image1(void)
	{
		
		...
		
		/* open PLL */
		BOOT_ROM_CM4PON((u32)SYSPLL_ON_SEQ);

		/* set dslp boot reason */
		if (HAL_READ32(SYSTEM_CTRL_BASE_LP, REG_AON_BOOT_REASON1) & BIT_AON_BOOT_EXIT_DSLP) {
			/* open PLL */
			//BOOT_ROM_CM4PON((u32)SYSPLL_ON_SEQ);
			BOOT_FLASH_DSLP_FlashInit();
		}

		/* Let KM4 RUN */
		Temp = HAL_READ32(SYSTEM_CTRL_BASE, REG_LP_BOOT_CFG) | BIT_SOC_BOOT_PATCH_KM4_RUN;
		HAL_WRITE32(SYSTEM_CTRL_BASE, REG_LP_BOOT_CFG, Temp);

		BOOT_ROM_CM4PON((u32)HSPWR_ON_SEQ);
		
		while (1) {
			if (BKUP_Read(BKUP_REG0) & BIT_RESVED_BIT17){
				break;
			}
			DelayMs(100);
		}

		...

		BOOT_RAM_SectionInit();

		//img2_hdr = (IMAGE_HEADER *)((__ls_flash_text_start__) - IMAGE_HEADER_LEN);
		img2_hdr = (IMAGE_HEADER *)(SPI_FLASH_BASE + 0x6000);
		
		...
		
		
		/*
			if (OTASelectHook == NULL)
				Temp = BOOT_FLASH_OTA_Select();
			else
				Temp = OTASelectHook();
			
			if(Temp == 0) {
				DBG_PRINTF(MODULE_BOOT, LEVEL_ERROR,"IMG2 Invalid\n");
				goto INVALID_IMG2;
			}
		*/
		
		/* Download image 2 */
		//FlashImage2DataHdr = (IMAGE_HEADER *)(__ls_flash_text_start__ + img2_hdr->image_size);
		FlashImage2DataHdr = (IMAGE_HEADER *)(SPI_FLASH_BASE + 0x6000 + IMAGE_HEADER_LEN + img2_hdr->image_size);
		
		...

	}
	
5. Modify component\soc\realtek\amebad\fwlib\ram_lp\rtl8721dlp_km4.c.
	void km4_boot_on(void)
	{
		u32 Temp;

		pmu_acquire_wakelock(PMU_KM4_RUN);

		/* Let KM4 RUN */
		//Temp = HAL_READ32(SYSTEM_CTRL_BASE, REG_LP_BOOT_CFG) | BIT_SOC_BOOT_PATCH_KM4_RUN;
		//HAL_WRITE32(SYSTEM_CTRL_BASE, REG_LP_BOOT_CFG, Temp);

		//BOOT_ROM_CM4PON((u32)HSPWR_ON_SEQ);

		BKUP_Set(BKUP_REG0, BIT_RESVED_BIT16);

		/*IPC table initialization*/
		ipc_table_init();
	}
	
	void km4_flash_highspeed_init(void)
	{
		//BOOT_ROM_CM4PON((u32)SYSPLL_ON_SEQ);
		/* calibration high speed before KM4 run */
		flash_operation_config();
	}
	
6. Modify component\soc\realtek\amebad\bootloader\boot_flash_hp.c.
	//3 Image 1
	BOOT_RAM_TEXT_SECTION
	void BOOT_FLASH_Image1(void)
	{
		
		...

		/* Secure Vector table init */
		irq_table_init(0x1007FFFC);
		__set_MSP(0x1007FFFC);

		/* Config Non-Security World Registers */
		BOOT_RAM_TZCfg();
		BKUP_Set(BKUP_REG0, BIT_RESVED_BIT17);

		while (1) {
			if (BKUP_Read(BKUP_REG0) & BIT_RESVED_BIT16){
				break;
			}
			DelayMs(100);
		}
		
		...

		/* Config Non-Security World Registers */
		//BOOT_RAM_TZCfg();

		...
		
	}

7. Modify component\soc\realtek\amebad\fwlib\usrcfg\rtl8721d_bootcfg.c according to new layout.

	BOOT_RAM_DATA_SECTION
	u32 OTA_Region[2] = {
		0x08016000,		/* OTA1 region start address */ 
		0x08116000,		/* OTA2 region start address */
	};


8. Modify project\realtek_amebaD_va0_example\GCC-RELEASE\project_lp\Makefile, add make -C asdk bootloader_2nd to all and xip.
	
	# Define the Rules to build the core targets
	all: CORE_TARGETS
		make -C asdk bootloader_2nd
		
	xip: CORE_TARGETS
		make -C asdk bootloader_2nd

9. Modify project\realtek_amebaD_va0_example\GCC-RELEASE\project_lp\asdk\Makefile.
	
	bootloader_2nd: build_target_folder copy_ld_img1_2nd check_toolchain build_info.h make_subdirs_2nd_bootloader linker_loader_2nd gen_rom_symbol
	
	copy_ld_img1_2nd:
		$(COPY) $(ROOTDIR)/ld/rlx8721d_boot_2nd.ld $(BUILD_TARGET_FOLDER)/rlx8721d.ld
		cat $(LINK_ROM_SYMBOL) >> $(BUILD_TARGET_FOLDER)/rlx8721d.ld
	
	make_subdirs_2nd_bootloader:
		@make -C make/bootloader_2nd all
		
	linker_loader_2nd:
		@echo "========= linker loader start ========="
		$(LD) $(LD_ARG) target_loader_2nd.axf $(RAM_OBJS_LIST) $(LINK_ROM_LIB) 

		$(MOVE) text.map $(IMAGE_TARGET_FOLDER)/text_loader_2nd.map
		$(MOVE) target_loader_2nd.axf $(IMAGE_TARGET_FOLDER)
		$(NM) $(IMAGE_TARGET_FOLDER)/target_loader_2nd.axf | sort > $(IMAGE_TARGET_FOLDER)/target_loader_2nd.map
		$(OBJDUMP) -d $(IMAGE_TARGET_FOLDER)/target_loader_2nd.axf > $(IMAGE_TARGET_FOLDER)/target_loader_2nd.asm
		$(COPY) $(IMAGE_TARGET_FOLDER)/target_loader_2nd.axf $(IMAGE_TARGET_FOLDER)/target_pure_loader_2nd.axf
		$(STRIP) $(IMAGE_TARGET_FOLDER)/target_pure_loader_2nd.axf

		$(FROMELF) -j .ram_boot2nd.entry  -j .ram_boot2nd.text -j .ram_boot2nd.data \
		-Obinary $(IMAGE_TARGET_FOLDER)/target_pure_loader_2nd.axf $(IMAGE_TARGET_FOLDER)/ram_1_2nd.bin

		$(FROMELF) -j .xip_boot2nd.text \
		-Obinary $(IMAGE_TARGET_FOLDER)/target_pure_loader_2nd.axf $(IMAGE_TARGET_FOLDER)/xip_boot_2nd.bin

		@echo "========== Image Info HEX =========="
		$(CC_SIZE) -A --radix=16 $(IMAGE_TARGET_FOLDER)/target_loader_2nd.axf
		$(CC_SIZE) -t --radix=16 $(IMAGE_TARGET_FOLDER)/target_loader_2nd.axf
		@echo "========== Image Info HEX =========="

		@echo "========== Image Info DEC =========="
		$(CC_SIZE) -A --radix=10 $(IMAGE_TARGET_FOLDER)/target_loader_2nd.axf
		$(CC_SIZE) -t --radix=10 $(IMAGE_TARGET_FOLDER)/target_loader_2nd.axf
		@echo "========== Image Info DEC =========="

		$(RM) -f $(RAM_OBJS_LIST)
		@echo "========== linker end =========="

		@echo "========== Image manipulating start =========="
		$(PREPENDTOOL) $(IMAGE_TARGET_FOLDER)/ram_1_2nd.bin  __ram_boot2nd_text_start__  $(IMAGE_TARGET_FOLDER)/target_loader_2nd.map
		$(PREPENDTOOL) $(IMAGE_TARGET_FOLDER)/xip_boot_2nd.bin  __flash_boot2nd_text_start__  $(IMAGE_TARGET_FOLDER)/target_loader_2nd.map
		$(CODE_ANALYZE_PYTHON)

		cat $(IMAGE_TARGET_FOLDER)/xip_boot_2nd_prepend.bin $(IMAGE_TARGET_FOLDER)/ram_1_2nd_prepend.bin > $(IMAGE_TARGET_FOLDER)/km0_boot_all_2nd.bin

		$(UTILITYDIR)/image_tool/imagetool.sh $(IMAGE_TARGET_FOLDER)/km0_boot_all_2nd.bin

		@echo "========== Image manipulating end =========="
	
	



