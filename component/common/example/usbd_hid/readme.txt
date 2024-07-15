##################################################################################
#                                                                                #
#                             Example example_usbd_hid                           #
#                                                                                #
##################################################################################

Date: 2023-11-13

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
        In this application, Ameba is designed as an USB HID mouse or keyboard device, PC will recognize Ameba as a mouse 
        or keyboard. 
        NOTE: Tested on Windows 7/10, MacOS, Ubuntu.
    
Setup Guide
~~~~~~~~~~~
        1. Connect Ameba to PC with USB cable.
        2. For GCC environmnet, type command "make menuconfig" and choose CONFIG USB:
               [*] Enable USB
               (DEVICE_MODE) USB Mode
               (HID) USB Device Class
           which will auto-generate following lines in platform_autoconf.h:
               #define CONFIG_USB_OTG_EN              1
               #define CONFIG_USB_DEVICE_EN           1
               #define CONFIG_USBD_HID                1
           And for IAR environment, manually edit platform_autoconf.h as above, include lib/lib_usbd.a (default excluded)
           to and exclude any nother USB libraries from km4_application project.
        3. Rebuild the project and download images to Ameba.
        4. Test with HID device.

Parameter Setting and Configuration
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
        Default Ameba acts as a HID mouse, if you want to change to HID keyboard, please modify HID_DEVICE_TYPE to HID_KEYBOARD_DEVICE
        in component/usb/device_new/hid/usbd_hid.h
    
Result description
~~~~~~~~~~~~~~~~~~
        1) For HID mouse: 
           If CONSTANT_MOUSE_DATA is set to 1, PC mouse cursor will move according to array mdata[] once Ameba is connected.
           If SHELL_MOUSE_DATA is set to 1, type following command from Ameba LOGUART console to control the PC cursor behavior:
              # mouse <left> <right> <middle> <x_axis> <y_axis> <wheel> 
        2) For HID keyboard, key data "aA" will report to PC once Ameba board is connected, while type the leds key such as CAPsLock
           and NumLock from PC, PC will send a message to the device.

Other Reference
~~~~~~~~~~~~~~~
        None

Supported List
~~~~~~~~~~~~~~
        Supported: Ameba-D
        Source code not in project: Ameba-1, Ameba-z, Ameba-pro
