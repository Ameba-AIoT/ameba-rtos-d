##################################################################################
#                                                                                #
#                             Example example_usbh_cdc_ecm                       #
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
        In this application, Ameba is designed as an USB CDC ECM host which can communicate with USB CDC ECM device and do DHCP test.
    
Setup Guide
~~~~~~~~~~~
        1. Set up the test environment, the TOP is as below: 
           [Ameba] ---- [USB CDC ECM device] ---- [Router]
        2. For GCC environmnet, type command "make menuconfig" and choose CONFIG USB:
               [*] Enable USB
               (HOST_MODE) USB Mode
               (CDC_ECM) USB Host Class
           this will auto-generate following lines in platform_autoconf.h:
               #define CONFIG_USB_OTG_EN              1
               #define CONFIG_USB_HOST_EN             1
               #define CONFIG_USBH_CDC_ECM            1
           And for IAR environment, manually edit platform_autoconf.h as above, include lib/lib_usbh.a (default excluded)
           to and exclude any nother USB libraries from km4_application project.
        3. Make sure following configurations are enabled in platform_opts.h:
               #define CONFIG_ETHERNET                1
               #define CONFIG_EXAMPLE_USBH_CDC_ECM    1
        4. Rebuild the project and download images to Ameba develop board.
        5. Reset and check the log via serial console

Parameter Setting and Configuration
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
        None.
    
Result description
~~~~~~~~~~~~~~~~~~
        On the serial console, USB CDC ECM host loading log will be printed and the DHCP test will be started, 
        make sure there is no error reported. if the test run success, the console will show:
            IP address : xx.xx.xx.xx
            Switch to link !!

Other Reference
~~~~~~~~~~~~~~~
        None.

Supported List
~~~~~~~~~~~~~~
        Supported: Ameba-D
        Source code not in project: Ameba-1, Ameba-z, Ameba-pro