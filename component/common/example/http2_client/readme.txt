##################################################################################
#                                                                                #
#                           example_http2_client                                  #
#                                                                                #
##################################################################################

Date: 2019-11-21

Table of Contents
~~~~~~~~~~~~~~~~~
 - Description
 - Setup Guide
 - Result description
 - Parameter Setting and Configuration
 - Other Reference
 - Supported List

 
Description
~~~~~~~~~~~
        In this example, http2 client send request to get server response.
	
Setup Guide
~~~~~~~~~~~
		1)Add/Enable CONFIG_EXAMPLE_HTTP2_CLIENT in platform_opts.h
		#define CONFIG_EXAMPLE_HTTP2_CLIENT 1
		
		2)Add example source file to project
        (a) For IAR project, add http2_client example to group <example> 
            $PROJ_DIR$\..\..\..\component\common\example\http2_client\example_http2_client.c
        (b) For GCC project, add http2_client example to utilities_example Makefile
            CSRC += $(DIR)/http2_client\example_http2_client.c
		
		3) Add include directories to project
		(a) For IAR project
			add $PROJ_DIR$\..\..\..\component/common/network/http2/nghttp2-1.31.0/includes
		(b) For GCC project
			IFLAGS += -I$(BASEDIR)/component/common/network/http2/nghttp2-1.31.0/includes
		
		4)Add lib_http2.a to project and modify makefile/IAR project setting to link it
		(a) For IAR project
			lib_http2.a is located at component/soc/realtek/amebad/misc/bsp/lib/common/IAR, add it to group <lib> and rebuild the project
		(b) For GCC project
			For AmebaD, lib_http2.a is located at project/realtek_amebaD_va0_example/GCC-RELEASE/project_hp/asdk/lib/application, modify makefile to link it, such as LINK_APP_LIB += $(ROOTDIR)/lib/application/lib_http2.a
Parameter Setting and Configuration
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
        Change REQ_HOST, REQ_PORT, REQ_PATH, REQ_HOSTPORT to user define.
	
Result description
~~~~~~~~~~~~~~~~~~
        This example will send http2 request to server.

Supported List
~~~~~~~~~~~~~~
[Supported List]
        Supported IC :
				AmebaD

		Source code not in project :
				Ameba-1, Ameba-z, Ameba-pro