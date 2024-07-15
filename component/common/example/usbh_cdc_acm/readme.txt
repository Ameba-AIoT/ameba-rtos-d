##################################################################################
#                                                                                #
#                             Example example_usbh_cdc_acm                       #
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
        In this application, Ameba is designed as an USB CDC ACM host which can communicate with specific USB CDC ACM device
        as defined in usbd_cdc_acm application.
    
Setup Guide
~~~~~~~~~~~
        1. Connect USB CDC ACM specific device (other Ameba running the usbd_cdc_acm application) to the USB port on 
           Ameba board.
        2. For GCC environmnet, type command "make menuconfig" and choose CONFIG USB:
               [*] Enable USB
               (HOST_MODE) USB Mode
               (CDC_ACM) USB Host Class
           this will auto-generate following lines in platform_autoconf.h:
               #define CONFIG_USB_OTG_EN              1
               #define CONFIG_USB_HOST_EN             1
               #define CONFIG_USBH_CDC_ACM            1
           And for IAR environment, manually edit platform_autoconf.h as above, include lib/lib_usbh.a (default excluded)
           to and exclude any nother USB libraries from km4_application project.
        3. Rebuild the project and download images to Ameba board.
        4. Reset and check the log via serial console.

Parameter Setting and Configuration
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
        None.
    
Result description
~~~~~~~~~~~~~~~~~~
        On the serial console, USB cdc acm specific host loading log will be printed, 
        make sure there is no error reported.

Other Reference
~~~~~~~~~~~~~~~
        None.

Supported List
~~~~~~~~~~~~~~
        Supported: Ameba-D
        Source code not in project: Ameba-1, Ameba-z, Ameba-pro