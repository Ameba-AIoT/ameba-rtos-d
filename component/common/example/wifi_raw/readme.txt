##################################################################################
#                                                                                #
#                           example_wifi_raw                                     #
#                                                                                #
##################################################################################

Table of Contents
~~~~~~~~~~~~~~~~~
- Description
- Setup Guide
- Result description
- Supported List


Description
~~~~~~~~~~~
	This example shows how to send probe_req and receive probe_rsp packets.

Setup Guide
~~~~~~~~~~~
	1. Include example to project.

	2. In example_wifi_raw.c, change addr1, addr2 and addr3 as required.
		u8 addr1[6] = {0x50, 0x64, 0x2B, 0x98, 0xE7, 0x69};	// Address 1, should be the mac address of destination
		u8 addr2[6] = {0x2C, 0x05, 0x47, 0x94, 0xB6, 0x37};	// Address 2, should be the mac address of myself as master
		u8 addr3[6] = {0x50, 0x64, 0x2B, 0x98, 0xE7, 0x69};	// Address 3, should be the BSSID

Result description
~~~~~~~~~~~~~~~~~~
	The example thread will be started automatically when booting.

Supported List
~~~~~~~~~~~~~~
	AmebaD