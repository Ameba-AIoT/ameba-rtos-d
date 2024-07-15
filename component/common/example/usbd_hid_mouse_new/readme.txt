##################################################################################
#                                                                                #
#                             Example example_usbd_hid                           #
#                                                                                #
##################################################################################

Date: 2021-8-18

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
        With USB device v2.0 interface, Ameba can be designed to a USB HID class device.
        In this application, Ameba boots up as a mouse device, PC can recognize Ameba and
        think of it as a mouse.
        NOTE: It can be recognized by Windows10, Windows7, MacOS, Ubuntu.
    
Setup Guide
~~~~~~~~~~~
        1. Connect Ameba to PC with a micro USB cable. Ameba DEV board has designed a micro USB
           connector on board.
        2. For GCC environmnet, type command "make menuconfig" and choose CONFIG USB => Enable USB => USB OTG Type => 
           USB_OTG_DEVICE_MODE and then choose USB Device Type => USB_DEVICE_HID,
           which will auto-generate following lines in platform_autoconf.h:
               #define CONFIG_USB_OTG_EN              1
               #define CONFIG_USB_DEVICE_EN           1
               #define CONFIG_USBD_HID            1
           And for IAR environment, manually edit platform_autoconf.h as above.
        3. Make sure the USB HID mouse example is enabled (default) in platform_opts.h:
               #define CONFIG_EXAMPLE_USBD_HID    1
        4. Rebuild the project and download firmware to DEV board.

Parameter Setting and Configuration
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
        None
    
Result description
~~~~~~~~~~~~~~~~~~
        If Setting macro CONSTANT_MOUSE_DATA to 1, cursor of PC will do process according to array mdata[] once connected to PC.
		If Setting macro SHELL_MOUSE_DATA to 1, you can type mouse command to control the cursor behavior.

Other Reference
~~~~~~~~~~~~~~~
        None

Supported List
~~~~~~~~~~~~~~
[Supported List]
        Supported : Ameba-D
        Source code not in project: Ameba-1, Ameba-z, Ameba-pro