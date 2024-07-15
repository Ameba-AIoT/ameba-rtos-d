##################################################################################
#                                                                                #
#                             Example example_usbd_composite                     #
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
        In this application, Ameba is designed as a USB CDC ACM and HID composite device, USB host can recognize Ameba
        as a CDC virtual serial port and a HID mouse.

Setup Guide
~~~~~~~~~~~
		1. Connect Ameba to USB host with USB cable.
        2. For GCC environmnet, type command "make menuconfig" and choose CONFIG USB:
               [*] Enable USB
               (DEVICE_MODE) USB Mode
               (CDC_ACM) USB Device Class
           which will auto-generate following lines in platform_autoconf.h:
               #define CONFIG_USB_OTG_EN              1
               #define CONFIG_USB_DEVICE_EN           1
               #define CONFIG_USBD_COMPOSITE          1
           And for IAR environment, manually edit platform_autoconf.h as above, include lib/lib_usbd.a (default excluded)
           to and exclude any nother USB libraries from km4_application project.
        3. Make sure the USB Composite example is enabled (default) in platform_opts.h:
               #define CONFIG_EXAMPLE_USBD_COMPOSITE  1
        4. Rebuild the project and download images to Ameba.
        5. Reset Ameba, following log shall be printed on the LOGUART console, make sure there is no USB related error reported:
			[USB] Composite demo started

		Specially for Win7/XP host, please manually install the CDC ACM driver RtkUsbCdcAcmSetup.INF, and before the installation, 
		please modify the PID/VID value in this INF file to sync with CDC ACM class code:
			[DeviceList]
			%DESCRIPTION%=DriverInstall, USB\VID_0BDA&PID_8721
			[DeviceList.NTamd64] 
			%DESCRIPTION%=DriverInstall, USB\VID_0BDA&PID_8721

Parameter Setting and Configuration
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
        None

Result description
~~~~~~~~~~~~~~~~~~
		Launch any serial port tool (e.g. Realtek Trace Tool), open the virtual serial port against the USB port of
		Ameba board, send messages to the Ameba board and the board will echo back the received messages.

		Type following command from Ameba LOGUART console to send mouse data to PC:
			# mouse <left> <right> <middle> <x_axis> <y_axis> <wheel>
		E.g., to move mouse cursor to right by 50 pixels:
			# mouse 0 0 0 50 0 0
		Then the position of PC mouse pointer shall be changed accordingly.

Other Reference
~~~~~~~~~~~~~~~
        None

Supported List
~~~~~~~~~~~~~~
        Supported: Ameba-D
        Source code not in project: Ameba-1, Ameba-z, Ameba-pro