IPCAM SDIO EXAMPLE

Description:
For ipcam sdio host and amebad sdio device.

Configuration:
1. Modify \project\realtek_amebaD_va0_example\inc\inc_hp\platform_opts.h.
	#define CONFIG_IPCAM_SDIO			1//on or off for ipcam app
	
	#if CONFIG_IPCAM_SDIO
	#undef CONFIG_EXAMPLE_WLAN_FAST_CONNECT
	#define CONFIG_EXAMPLE_WLAN_FAST_CONNECT 1
	#endif

2. Modify \component\common\example\example_entry.c
	void example_entry(void)
	{	
	#if defined(CONFIG_IPCAM_SDIO) && CONFIG_IPCAM_SDIO
		example_ipcam_sdio();
	#endif
	}

3. Modify \project\realtek_amebaD_va0_example\GCC-RELEASE\project_hp\asdk\make\utilities_example\Makefile.
	add:
		CSRC += $(DIR)/ipcam_sdio/example_ipcam_sdio.c
		CSRC += $(DIR)/ipcam_sdio/ipcam_sdio.c
   Modify \project\realtek_amebaD_va0_example\GCC-RELEASE\project_hp\asdk\make\api\at_cmd\Makefile.
    add:
		$(DIR)/ipcam_sdio_atcmd.c \