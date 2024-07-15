IPV6 EXAMPLE

Description:
Example for IPV6.

Configuration:
[lwipopts.h]
    	#define LWIP_IPV6					1
[platform_opts.h]
	#define CONFIG_EXAMPLE_IPV6				1

[example_ipv6.h]
#define UDP_SERVER_IP    "fe80:0000:0000:0000:cd3c:24de:386d:9ad1"
#define TCP_SERVER_IP    "fe80:0000:0000:0000:cd3c:24de:386d:9ad1"
Change the ipv6 address according to your own device.

[example_ipv6.c]  Users can only enable one example at one time.
      example_ipv6_udp_server();
//    example_ipv6_tcp_server();
//    example_ipv6_mcast_server();
//    example_ipv6_udp_client();
//    example_ipv6_tcp_client();
//    example_ipv6_mcast_client();

Execution:
Can make automatical Wi-Fi connection when booting by using wlan fast connect example.
A IPV6 example thread will be started automatically when booting.

Enable one example at one time to verify client or server examples. Modify UDP_SERVER_IP and TCP_SERVER_IP according to your own device.

[Supported List]
	Ameba-D
	Source code not in project :
	    Ameba-1, Ameba-z, Ameba-pro
