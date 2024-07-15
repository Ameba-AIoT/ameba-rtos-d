##################################################################################
#                                                                                #
#                             example_httpd_littlefs                                      #
#                                                                                #
##################################################################################

Description
~~~~~~~~~~~
		This example is based on HTTPD API, an HTTP server example with a simple website and GET/POST/PUT method test pages are provided.
		In addition, this example can test captive portal.
	
Setup Guide
~~~~~~~~~~~
        1. In platform_opts.h, please set #define CONFIG_EXAMPLE_HTTPD_LITTLEFS  1
        2. If you want to enable captive portal, please set #define CONFIG_ENABLE_CAPTIVE_PORTAL  1 in platform_opts.h
		3. When you burn firmware, you need to burn html.bin at address 0x08200000, which is set in #define LFS_FLASH_BASE_ADDR 0x200000.
		   You can use the following command to generate html.bin with mklittlefs:
		   ./mklittlefs -c  html_demo/  -d 0 -b 4096 -p 1024 -s 1048576 html.bin
		4. A httpd example thread is started automatically when booting, then you need to set AmebaD as ap with the following commands:
	ATW3=SSID
	ATW4=PASSWORD
	ATW5=CHANNEL
	ATWA
		5. Connect your device to AmebaD. If you set #define CONFIG_ENABLE_CAPTIVE_PORTAL  1, the browser will automatically open the captive portal. Otherwise, you need to manually enter the IP address of AmebaD in the browser.


Note：
	1. The directory of the html_demo is as follows:
	├─css
	│	└─ *.css
	│
	│─images
	│	├─login
	│	│	└─ *.gif
	│	└─main
	│		└─ *.gif/*.jpg
	│
	│─js
	│ └─ *.js
	│
	└─ *.html
	
	2. PUT&POST method is currently not supported, you can add it in example_httpd_littlefs.c
	
	
	
	
Supported List
~~~~~~~~~~~~~~
[Supported List]
        Supported IC :
                AmebaD
