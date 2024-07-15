##################################################################################
#                                                                                #
#                             Example example_usbd_cdc_acm                       #
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
        In this application, Ameba is designed as an USB CDC ACM device, USB host can recognize Ameba as a virtual
        serial port and communicate with it via serial port tools.
    
Setup Guide
~~~~~~~~~~~
		In order to run this application successfully, the hardware setup should be confirm before moving on, Please refer to AN0400 Section 7.4 for details.
        1. Connect Ameba to USB host with USB cable.
        2. For GCC environmnet, type command "make menuconfig" and choose CONFIG USB:
               [*] Enable USB
               (DEVICE_MODE) USB Mode
               (CDC_ACM) USB Device Class
           which will auto-generate following lines in platform_autoconf.h:
               #define CONFIG_USB_OTG_EN              1
               #define CONFIG_USB_DEVICE_EN           1
               #define CONFIG_USBD_CDC_ACM            1
           And for IAR environment, manually edit platform_autoconf.h as above, include lib/lib_usbd.a (default excluded)
           to and exclude any nother USB libraries from km4_application project.
        3. Rebuild the project and download images to Ameba.
        4. Launch any serial port tool (e.g. Realtek Trace Tool) on USB host, open the virtual serial port against
           the USB port of Ameba, send messages to Ameba and it will echo back the received messages.

		Specially for Win7/XP host, please manually install the CDC ACM driver RtkUsbCdcAcmSetup.INF, and before the
		installation, please modify the PID/VID value in this INF file to sync with CDC ACM class code: 
			  [DeviceList]
			  %DESCRIPTION%=DriverInstall, USB\VID_0BDA&PID_1234
			  [DeviceList.NTamd64]
			  %DESCRIPTION%=DriverInstall, USB\VID_0BDA&PID_1234

Parameter Setting and Configuration
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
        None
    
Result description
~~~~~~~~~~~~~~~~~~
        On the serial console, USB CDC ACM loading log will be printed, make sure there is no error reported.
        After the CDC ACM driver is successfully loaded, USB host end will recognize Ameba as a COM port. 
        Now user can operate the COM port via common COM tools, and Ameba will echo back the received messages.

Other Reference
~~~~~~~~~~~~~~~
        None

Supported List
~~~~~~~~~~~~~~
        Supported: Ameba-D
        Source code not in project: Ameba-1, Ameba-z, Ameba-pro