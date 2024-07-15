##################################################################################
#                                                                                #
#                             Example example_usbd_msc                           #
#                                                                                #
##################################################################################

Date: 2024-1-17

Table of Contents
~~~~~~~~~~~~~~~~~
 - Description
 - Setup Guide
 - Parameter Setting and Configuration
 - Result description
 - Other Reference
 - Supported List

 
Description
~~~~~~~~~~~
        In this application, Ameba is designed as an USB MSC (mass storage class) device with SD card as its physical
        memory media. USB host (e.g. PC or Ameba as USB MSC host) will recognize Ameba as MSC device and access the data on
        SD card via USB interface.
    
Setup Guide
~~~~~~~~~~~
        1. Attach a SD card to Ameba board and connect Ameba board to USB host with USB cable.
        2. Configure the SDIO CD/WP pins as required by modifying sdioh_config in
           component\soc\realtek\amebad\fwlib\usrcfg\rtl8721dhp_intfcfg.c
				SDIOHCFG_TypeDef sdioh_config = {
					.sdioh_bus_speed = SD_SPEED_HS,				// SD_SPEED_DS or SD_SPEED_HS
					.sdioh_bus_width = SDIOH_BUS_WIDTH_4BIT,	// SDIOH_BUS_WIDTH_1BIT or SDIOH_BUS_WIDTH_4BIT
					.sdioh_cd_pin = _PB_25,						// _PB_25/_PA_6/_PNC
					.sdioh_wp_pin = _PNC,						// _PB_24/_PA_5/_PNC
				};
		   E.g. for Realtek AmebaD QFN88 QA board, sdioh_cd_pin shoud be changed to _PA_6.
        3. For GCC environmnet, type command "make menuconfig" and choose CONFIG USB:
               [*] Enable USB
               (DEVICE_MODE) USB Mode
               (MSC) USB Device Class
           which will auto-generate following lines in platform_autoconf.h:
               #define CONFIG_USB_OTG_EN              1
               #define CONFIG_USB_DEVICE_EN           1
               #define CONFIG_USBD_MSC                1
           And for IAR environment, manually edit platform_autoconf.h as above, include lib/lib_usbd.a (default excluded)
           to and exclude any nother USB libraries from km4_application project.
        4. Rebuild the project and download images to Ameba.

Parameter Setting and Configuration
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
        None
    
Result description
~~~~~~~~~~~~~~~~~~
        On the serial console, USB MSC loading log will be printed, make sure there is no error reported.
        After the MSC driver is successfully loaded, USB host end will recognize Ameba as a USB mass storage device. 
        Now user can operate the USB mass storage device by accessing files on the attached SD card.

Other Reference
~~~~~~~~~~~~~~~
        None

Supported List
~~~~~~~~~~~~~~
        Supported: Ameba-D
        Source code not in project: Ameba-1, Ameba-z, Ameba-pro