##################################################################################
#                                                                                #
#                          EXAMPLE_SDCARD_UPLOAD_HTTPD                           #
#                                                                                #
##################################################################################


Date: 2019-10-24

Table of Contents
~~~~~~~~~~~~~~~~~
 - Description
 - Setup Guide
 - Parameter Setting and Configuration
 - Result description
 - Supported List


Description
~~~~~~~~~~~
    This example is used to upload the storage data to web server.
    Please make sure the target file is within SD card file system.

Setup Guide
~~~~~~~~~~~
        1. Add example_sdcard_upload_httpd example to SDK
        
        /component/common/example/sdcard_upload_httpd
        .
        |-- example_sdcard_upload_httpd.c
        |-- example_sdcard_upload_httpd.h
        `-- readme.txt

        2. Enable CONFIG_SDCARD_UPLOAD_HTTPD in [platform_opts.h]
        /* For sdcard upload web server example */
        #define CONFIG_SDCARD_UPLOAD_HTTPD  1
        
        Note: 
        There are three FATFS library for costmers to choose, r0.13c r0.10c r0.10b
        To use latest FATFS library r0.13c(released on oct 14,2018) which supports UTF-8 encoding 
        along with other new features, set the parameter as below
        --> Modify #undef FATFS_R_13C macro in platform_opts.h under CONFIG_SDCARD_UPLOAD_HTTPD to 
        #define FATFS_R_13C and #undef FATFS_R_10C macro
        --> Replace existing fatfs lib with latest r0.13c lib related files to IAR project 
        --> For further details on new features, please refer to http://elm-chan.org/fsw/ff/00index_e.html


Parameter Setting and Configuration
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    Modify MP4_NAME with the file name in your SD card.


Result description
~~~~~~~~~~~~~~~~~~
    You need to connect to WiFi manually by AT command. 
    Enter the ameba ip address to browser and it will start to download data from http protocol.


Supported List
~~~~~~~~~~~~~~
[Supported List]
        Supported :
            AmebaD, 