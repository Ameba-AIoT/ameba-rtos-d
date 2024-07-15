##################################################################################
#                                                                                #
#                             Example example_usbd_vendor                        #
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
        In this application, Ameba is designed as an USB vendor specific device which can only communicate with the
        USB vendor specific USB host as defined in usbh_vendor application.
        Under development yet.
    
Setup Guide
~~~~~~~~~~~
        1. Connect Ameba board to USB vendor specific host (another Ameba running usbh_vendor application).
        2. For GCC environmnet, type command "make menuconfig" and choose CONFIG USB:
               [*] Enable USB
               (DEVICE_MODE) USB Mode
               (Vendor) USB Device Class
           this will auto-generate following lines in platform_autoconf.h:
               #define CONFIG_USB_OTG_EN              1
               #define CONFIG_USB_DEVICE_EN           1
               #define CONFIG_USBD_VENDOR             1
           And for IAR environment, manually edit platform_autoconf.h as above, include lib/lib_usbd.a (default excluded)
           to and exclude any nother USB libraries from km4_application project.
        3. Rebuild the project and download images to Ameba develop board.
        4. Reset and check the log via serial console.

Parameter Setting and Configuration
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
        None.
    
Result description
~~~~~~~~~~~~~~~~~~
        On the serial console, USB vendor specific device loading log and vendor specific test record will be printed, 
        make sure there is no error reported.

Other Reference
~~~~~~~~~~~~~~~
        None.

Supported List
~~~~~~~~~~~~~~
        Supported: Ameba-D
        Source code not in project: Ameba-1, Ameba-z, Ameba-pro