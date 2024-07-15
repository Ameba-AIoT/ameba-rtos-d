##################################################################################
#                                                                                #
#                             Example captive_portal                             #
#                                                                                #
##################################################################################

Table of Contents
~~~~~~~~~~~~~~~~~
- Description
	- Setup Guide
	- Result description
	- Other Reference
	- Supported List


Description
	~~~~~~~~~~~
	A simple server which provides captive portal page.

Setup Guide
	~~~~~~~~~~~
	1.Enable captive portal example
		[platform_opts.h]
			#define CONFIG_EXAMPLE_CAPTIVE_PORTAL 	1

	2.Expand MEMP_NUM_NETBUF to support for more connections.
		[lwipopts.h] add
			#define MEMP_NUM_NETBUF                 20

	3.Enable captive portal.
		[platform_opts.h]
			#define CONFIG_ENABLE_CAPTIVE_PORTAL  1

Result description
		~~~~~~~~~~~~~~~~~~
		A softap thread will be started automatically when booting. After the phone connects to the softap, a captive portal page will automatically appear,
	which can display information of softap and scan available wifi.
		Several buttons are provided. "Submit" button is used to restart softap when you change the setting. "Scan" button is used to scan available wifi. "Connect"
	button is used to connect target wifi according to your input. "Refresh" button is used to refresh page.

Supported List
		~~~~~~~~~~~~~~
		[Supported List]
	Supported :
		Ameba-D
