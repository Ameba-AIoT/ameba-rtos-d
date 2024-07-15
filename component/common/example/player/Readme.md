##################################################################################
#                                                                                #
#                             Example player                                     #
#                                                                                #
##################################################################################

Date: 2023-12-22

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
        This player example shows how to play music use the rtplayer interfaces.

Setup Guide
~~~~~~~~~~~
1. Make sure you can malloc memory from psram with the following steps:
1.1 Set the CONFIG_DYNAMIC_HEAP_SIZE and configUSE_PSRAM_FOR_HEAP_REGION enable in project/realtek_amebaD_va0_example/inc/inc_hp/FreeRTOSConfig.h
CONFIG_DYNAMIC_HEAP_SIZE=1
configUSE_PSRAM_FOR_HEAP_REGION=1
1.2 Set the psram_dev_enable to TRUE in component/soc/realtek/amebad/fwlib/usrcfg/rtl8721dhp_intfcfg.c
psram_dev_enable = TRUE
1.3 Make sure the malloc/calloc/realloc/free have been wrapped in project/realtek_amebaD_va0_example/GCC-RELEASE/project_hp/asdk/Makefile.include.gen
modify
//LD_ARG += -Wl,-wrap,malloc -Wl,-wrap,realloc
//LD_ARG += -Wl,-wrap,calloc -Wl,-wrap,free
to
LD_ARG += -Wl,-wrap,malloc -Wl,-wrap,realloc
LD_ARG += -Wl,-wrap,calloc -Wl,-wrap,free

2. If you want to play the music restore int the SD Card, enable sd card function with the following steps:
2.1 Change the value of sdioh_cd_pin in component/soc/realtek/amebad/fwlib/usrcfg/rtl8721dhp_intfcfg.c
modify
.sdioh_cd_pin = _PB_25, //_PB_25/_PA_6/_PNC
to
.sdioh_cd_pin = _PNC, //_PB_25/_PA_6/_PNC

2.2 Make sure the following config is enable, Add the the following content in the end of project/realtek_amebaD_va0_example/inc/inc_hp/platform_opts.h before #endif
#define CONFIG_FATFS_EN 1
#if CONFIG_FATFS_EN
#define FATFS_R_10C
#define FATFS_DISK_SD 1
#endif
#define FATFS_DISK_USB 0
#define FATFS_DISK_SD 1

2.3 component/common/example/player/example_player.c
Modify
//#define USE_SD_CARD
to
#define USE_SD_CARD

3. Disable fast connect, set the CONFIG_EXAMPLE_WLAN_FAST_CONNECT disable in project/realtek_amebaD_va0_example/inc/inc_hp/platform_opts.h
CONFIG_EXAMPLE_WLAN_FAST_CONNECT 0

4. Use https
Modify component/common/network/ssl/mbedtls-2.4.0/include/mbedtls/config_rsa.h
//#define MBEDTLS_CERTS_C
to
#define MBEDTLS_CERTS_C

//#define MBEDTLS_SSL_SRV_C
to
#define MBEDTLS_SSL_SRV_C

#define MBEDTLS_SSL_MAX_CONTENT_LEN                4096 /**< Maxium fragment length in bytes, determines the size of each of the two internal I/O buffers */
to
#define MBEDTLS_SSL_MAX_CONTENT_LEN                16*1024 /**< Maxium fragment length in bytes, determines the size of each of the two internal I/O buffers */

5. For GCC environmnet, type command "make menuconfig" int the file project/realtek_amebaD_va0_example/GCC-RELEASE/project_hp and choose
< MENUCONFIG FOR CHIP CONFIG  ---> 
< Audio Config  --->
[*] Enable Audio Framework
(PassThrough) Select Audio Framework
[*] Enable Media Framework
< Demux  --->
[*] Demux WAV
[*] Demux MP3
[*] Demux AAC
[*] Demux M4A
< Codec  --->
[*] Codec PCM
[*] Codec MP3
[*] Codec AAC

6. Connect the headphone before you start the tests.

7. Make sure your device connect to the internet.
   For example, I have a mp3 music file in my test sever (192.168.31, 226) named j48.mp3, use the command as follow to test a online music:
        player http://192.168.31.226/j48.mp3

8. Make sure you have enabled the SD card function and insert the SD card to the board.
   For example, I have a mp3 music file named test1.mp3 in the SD card, use the command as follow to test a music file in SD Card.
        player file://test1.mp3

Parameter Setting and Configuration
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
        None.

Result description
~~~~~~~~~~~~~~~~~~
        the corresponding music played in the headphone.

Other Reference
~~~~~~~~~~~~~~~
        None.

Supported List
~~~~~~~~~~~~~~
[Supported List]
        Supported : Ameba-D
        Source code not in project: Ameba-1, Ameba-z, Ameba-pro
        Not supported in IAR
