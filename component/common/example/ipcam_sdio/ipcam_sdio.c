/**
 * @file
 * lwIP netif implementing an IEEE 802.1D MAC Bridge
 */

/*
 * Copyright (c) 2017 Simon Goldschmidt.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
 * SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 *
 * This file is part of the lwIP TCP/IP stack.
 *
 * Author: Simon Goldschmidt <goldsimon@gmx.de>
 *
 */

/**
 * @defgroup bridgeif IEEE 802.1D bridge
 * @ingroup netifs
 * This file implements an IEEE 802.1D bridge by using a multilayer netif approach
 * (one hardware-independent netif for the bridge that uses hardware netifs for its ports).
 * On transmit, the bridge selects the outgoing port(s).
 * On receive, the port netif calls into the bridge (via its netif->input function) and
 * the bridge selects the port(s) (and/or its netif->input function) to pass the received pbuf to.
 *
 * Usage:
 * - add the port netifs just like you would when using them as dedicated netif without a bridge
 *   - only NETIF_FLAG_ETHARP/NETIF_FLAG_ETHERNET netifs are supported as bridge ports
 *   - add the bridge port netifs without IPv4 addresses (i.e. pass 'NULL, NULL, NULL')
 *   - don't add IPv6 addresses to the port netifs!
 * - set up the bridge configuration in a global variable of type 'bridgeif_initdata_t' that contains
 *   - the MAC address of the bridge
 *   - some configuration options controlling the memory consumption (maximum number of ports
 *     and FDB entries)
 *   - e.g. for a bridge MAC address 00-01-02-03-04-05, 2 bridge ports, 1024 FDB entries + 16 static MAC entries:
 *     bridgeif_initdata_t mybridge_initdata = BRIDGEIF_INITDATA1(2, 1024, 16, ETH_ADDR(0, 1, 2, 3, 4, 5));
 * - add the bridge netif (with IPv4 config):
 *   struct netif bridge_netif;
 *   netif_add(&bridge_netif, &my_ip, &my_netmask, &my_gw, &mybridge_initdata, bridgeif_init, tcpip_input);
 *   NOTE: the passed 'input' function depends on BRIDGEIF_PORT_NETIFS_OUTPUT_DIRECT setting,
 *         which controls where the forwarding is done (netif low level input context vs. tcpip_thread)
 * - set up all ports netifs and the bridge netif
 *
 * - When adding a port netif, NETIF_FLAG_ETHARP flag will be removed from a port
 *   to prevent ETHARP working on that port netif (we only want one IP per bridge not per port).
 * - When adding a port netif, its input function is changed to call into the bridge.
 *
 *
 * @todo:
 * - compact static FDB entries (instead of walking the whole array)
 * - add FDB query/read access
 * - add FDB change callback (when learning or dropping auto-learned entries)
 * - prefill FDB with MAC classes that should never be forwarded
 * - multicast snooping? (and only forward group addresses to interested ports)
 * - support removing ports
 * - check SNMP integration
 * - VLAN handling / trunk ports
 * - priority handling? (although that largely depends on TX queue limitations and lwIP doesn't provide tx-done handling)
 */

#include "lwip/sys.h"
#include "lwip/etharp.h"
#include <string.h>
#include "lwip/prot/tcp.h"
#include "lwip/prot/dns.h"
#include "lwip/udp.h"
#include "lwip_netconf.h"

#if CONFIG_LWIP_LAYER
extern struct netif xnetif[NET_IF_NUM]; 
#endif

#define IPCAM_SDIO_DEBUG 0
#define PRINT_IPCAM_SDIO(debug, message) \
				do { \
					if (debug != 0) { printf message; } \
				} while(0)

#define ETH_ALEN	6		/* Octets in one ethernet addr	 */
#define ETH_ILEN	4		/* Octets in one ip addr	 */
#define ETH_HLEN	14		/* Total octets in header.	 */
#define ETH_P_IP	0x0800		/* Internet Protocol packet	*/
#define ETH_P_ARP	0x0806		/* Address Resolution packet	*/
#define ETH_P_IPV6	0x86DD

#define PORT_TO_BOTH	0x0
#define PORT_TO_UP	0x1
#define PORT_TO_HOST	0x2
#define IP_ICMP_REQUEST 0x08
#ifndef DHCP_SERVER_PORT
#define DHCP_SERVER_PORT 67
#endif

#define PORT_FOR_AMEBAD_NUM 2

struct eth_addr ipcam_sdio_mac = {{0x00}};

typedef struct ipcam_sdio_pkt_attrib_s {
	u16_t protocol;
	u16_t src_port;
	u16_t dst_port;
	u8_t src_ip[4];
	u8_t dst_ip[4];
	u8_t flags;
	u8_t type;
	struct eth_addr src_mac;
	struct eth_addr dst_mac;
	u8_t port_idx;
}ipcam_sdio_pkt_attrib_t;

struct ipcam_sdio_port_for_amebad_s {
	u16_t port_num;
	u8_t  used;
};

struct ipcam_sdio_port_for_amebad_s ipcam_sdio_port_array[PORT_FOR_AMEBAD_NUM] = { {0,0}};

void ipcam_sdio_change_mac(struct eth_addr *macaddr)
{
	memcpy(&ipcam_sdio_mac, macaddr, sizeof(struct eth_addr));
	printf("new mac:%2x,%2x,%2x,%2x,%2x,%2x\n", 
				ipcam_sdio_mac.addr[0],ipcam_sdio_mac.addr[1],ipcam_sdio_mac.addr[2],ipcam_sdio_mac.addr[3],ipcam_sdio_mac.addr[4],ipcam_sdio_mac.addr[5]);
}

void ipcam_sdio_port_for_amebad(u16_t port_num)
{
	for(int i=0; i<PORT_FOR_AMEBAD_NUM; ++i)
	{	
		if(ipcam_sdio_port_array[i].used == 0)
		{
			ipcam_sdio_port_array[i].port_num = port_num;
			ipcam_sdio_port_array[i].used = 1;

			printf("%s(%d)[%d]set local port=%d done\n", __FUNCTION__, __LINE__, i, ipcam_sdio_port_array[i].port_num);
			return;
		}
	}
	printf("%s(%d)set local port fail\n", __FUNCTION__, __LINE__);
}

void ipcam_sdio_packet_attrib(struct pbuf *p, ipcam_sdio_pkt_attrib_t *pattrib)
{
	u16_t protocol, src_port = 0, dst_port = 0;
	u8_t type = 0;
	u8_t flags = 0;
	struct ip_hdr  *iph = NULL;
  struct etharp_hdr *arph = NULL;
	struct tcp_hdr *tcph = NULL;
	struct udp_hdr *udph = NULL;
	u8_t *src_ip = NULL, *dst_ip = NULL;
	struct eth_addr *src_addr, *dst_addr;

	dst_addr = (struct eth_addr *)((u8_t *)p->payload);
	src_addr = (struct eth_addr *)(((u8_t *)p->payload) + sizeof(struct eth_addr));
	protocol = *((unsigned short *)(p->payload + 2 * ETH_ALEN));

	if(protocol == lwip_htons(ETH_P_IP))
	{
		/* update src ip/mac mapping */
		iph = (struct ip_hdr *)(p->payload + ETH_HLEN);
		src_ip = (u8_t *)&(iph->src.addr);
		dst_ip = (u8_t *)&(iph->dest.addr);
		type = iph->_proto;

		switch(iph->_proto) {
			case IP_PROTO_TCP://TCP
				tcph = (struct tcp_hdr *)(p->payload + ETH_HLEN + sizeof(struct ip_hdr));
				if(tcph != NULL) {
					src_port = PP_NTOHS(tcph->src);
					dst_port = PP_NTOHS(tcph->dest);
					flags = TCPH_FLAGS(tcph);
				}	
				break;
			case IP_PROTO_UDP://UDP
				udph = (struct udp_hdr *)(p->payload + ETH_HLEN + sizeof(struct ip_hdr));
				if(udph != NULL) {
					src_port = PP_NTOHS(udph->src);
					dst_port = PP_NTOHS(udph->dest);
				}
				break;
			default: //ICMP
				src_port = 0;
				dst_port = 0;
		}
	}
	else if(protocol == lwip_htons(ETH_P_ARP))
	{	 
		arph = (struct etharp_hdr *)(p->payload + ETH_HLEN);
		src_ip = (u8 *)&(arph->sipaddr);
		dst_ip = (u8 *)&(arph->dipaddr);
		PRINT_IPCAM_SDIO(IPCAM_SDIO_DEBUG,("%s(%d),dstip:%d,%d,%d,%d\n",__func__,__LINE__, (uint8_t)*(dst_ip+0),(uint8_t)*(dst_ip+1),(uint8_t)*(dst_ip+2),(uint8_t)*(dst_ip+3)));
	}

	PRINT_IPCAM_SDIO(IPCAM_SDIO_DEBUG,("%s(%d)flags=%x,src_port:%d,dst_port:%d,smac:%2x,%2x,%2x,%2x,%2x,%2x,dmac:%2x,%2x,%2x,%2x,%2x,%2x\n",__func__,__LINE__, flags, src_port, dst_port,
		src_addr->addr[0],src_addr->addr[1],src_addr->addr[2],src_addr->addr[3],src_addr->addr[4],src_addr->addr[5],
		dst_addr->addr[0],dst_addr->addr[1],dst_addr->addr[2],dst_addr->addr[3],dst_addr->addr[4],dst_addr->addr[5]));

	pattrib->protocol = protocol;
	pattrib->src_port = src_port;
	pattrib->dst_port = dst_port;
	pattrib->type = type;
	pattrib->flags = flags;
	memcpy(&pattrib->src_mac, src_addr, sizeof(struct eth_addr));
	memcpy(&pattrib->dst_mac, dst_addr, sizeof(struct eth_addr));
	if(src_ip != NULL)
		memcpy(pattrib->src_ip, src_ip, ETH_ILEN);
	if(dst_ip != NULL)
		memcpy(pattrib->dst_ip, dst_ip, ETH_ILEN);	
}

u16_t ipcam_sdio_rcvpkt_dir(ipcam_sdio_pkt_attrib_t *pattrib, struct pbuf *p)
{
	PRINT_IPCAM_SDIO(IPCAM_SDIO_DEBUG,("%s(%d)src_port:%d,dst_port:%d\n",__func__,__LINE__,pattrib->src_port,pattrib->dst_port));

	if(pattrib->protocol == lwip_htons(ETH_P_ARP))
	{	
		PRINT_IPCAM_SDIO(IPCAM_SDIO_DEBUG,("%s(%d)toup\n",__func__, __LINE__));
		return PORT_TO_UP;
	}

	if (pattrib->type == IP_PROTO_ICMP)
	{
		unsigned char icmp_type = *(unsigned char *)(p->payload + ETH_HLEN + sizeof(struct ip_hdr));

		if (icmp_type == IP_ICMP_REQUEST)
		{	
			PRINT_IPCAM_SDIO(IPCAM_SDIO_DEBUG,("%s(%d)toup\n",__func__, __LINE__));
			return PORT_TO_UP;
		}
		else
		{
			PRINT_IPCAM_SDIO(IPCAM_SDIO_DEBUG,("%s(%d)toboth\n",__func__, __LINE__));
			return PORT_TO_BOTH;
		}
	}

	if (pattrib->src_port == 0 || pattrib->dst_port == 0)
		return PORT_TO_BOTH;

	if (pattrib->type == IP_PROTO_TCP)
	{
		//check if pkt should go to amebad
		for(int i=0; i<PORT_FOR_AMEBAD_NUM; ++i)
		{	
			if(ipcam_sdio_port_array[i].used == 1 && ipcam_sdio_port_array[i].port_num == pattrib->dst_port)
			{
				PRINT_IPCAM_SDIO(IPCAM_SDIO_DEBUG,("%s(%d)toup\n",__func__, __LINE__));
				return PORT_TO_UP;
			}
		}

		PRINT_IPCAM_SDIO(IPCAM_SDIO_DEBUG,("%s(%d)tohost\n",__func__, __LINE__));
		//not for amebad, go to host
		return PORT_TO_HOST;
	}
	else if (pattrib->type == IP_PROTO_UDP)
	{
		if(pattrib->src_port == DNS_SERVER_PORT)
		{
			PRINT_IPCAM_SDIO(IPCAM_SDIO_DEBUG,("%s(%d)toboth\n",__func__, __LINE__));
			return PORT_TO_BOTH;
		}
		else if(pattrib->src_port == DHCP_SERVER_PORT)
		{
			PRINT_IPCAM_SDIO(IPCAM_SDIO_DEBUG,("%s(%d)toup\n",__func__, __LINE__));
			return PORT_TO_UP;
		}
		else
		{
			PRINT_IPCAM_SDIO(IPCAM_SDIO_DEBUG,("%s(%d)tohost\n",__func__, __LINE__));
			return PORT_TO_HOST;
		}
	}
}

u32_t ipcam_sdio_send_to_ports(ipcam_sdio_pkt_attrib_t *pattrib, struct pbuf *p)
{
	u16_t dstports = 2;
	struct ip_hdr *iphdr = (struct ip_hdr *)(p->payload + ETH_HLEN);

	if (pattrib->port_idx == 0)
	{
		PRINT_IPCAM_SDIO(IPCAM_SDIO_DEBUG,("%s(%d)in>tohost\n",__func__, __LINE__));
		/* Ameba->RTS3916 */
		dstports = 1;
		/* dst mac*/
		memcpy(p->payload, ipcam_sdio_mac.addr, ETH_ALEN);

		/* src mac */
		memcpy(p->payload + ETH_ALEN, xnetif[0].hwaddr, ETH_ALEN);
	}
	else if (pattrib->port_idx == 1)
	{
		PRINT_IPCAM_SDIO(IPCAM_SDIO_DEBUG,("%s(%d)out>toap\n",__func__, __LINE__));
		/* RTS3916->Ameba */
		dstports = 0;
		/* src mac */
		memcpy(p->payload + ETH_ALEN, xnetif[0].hwaddr, ETH_ALEN);
	}

	if(dstports == 0)
	{
		/* remove eth header */
		pbuf_header(p, -ETH_HLEN);
		/* send to etharp_output */
		PRINT_IPCAM_SDIO(IPCAM_SDIO_DEBUG,("%s(%d)out>toap\n",__func__, __LINE__));
		xnetif[0].output(&xnetif[0], p, &iphdr->dest);
	}
	else
	{
		/* direct send to sdio host */
		PRINT_IPCAM_SDIO(IPCAM_SDIO_DEBUG,("%s(%d)in>tohost\n",__func__, __LINE__));
		xnetif[1].linkoutput(&xnetif[1], p);
	}
			
	return 0;
}

/** The actual input function. Port netif's input is changed to call
 * here. This function decides where the frame is forwarded.
 */
 err_t
ipcam_sdio_input(struct pbuf *p, struct netif *netif)
{
	struct pbuf *q;
	ipcam_sdio_pkt_attrib_t *pattrib;

	if (p == NULL || netif == NULL) {
		PRINT_IPCAM_SDIO(IPCAM_SDIO_DEBUG,("%s(%d)\n",__func__,__LINE__));
		return ERR_VAL;
	}

	pattrib = (ipcam_sdio_pkt_attrib_t *)malloc(sizeof(ipcam_sdio_pkt_attrib_t));
	ipcam_sdio_packet_attrib(p, pattrib);
	PRINT_IPCAM_SDIO(IPCAM_SDIO_DEBUG,("%s(%d)portnum=%d,protocol=0x%x\n", __FUNCTION__, __LINE__, netif->num, lwip_ntohs(pattrib->protocol)));

	if (pattrib->protocol == lwip_htons(ETH_P_IPV6)) {
		pbuf_free(p);
		free(pattrib);
		return ERR_VAL;
	}
	pattrib->port_idx = netif->num;

    /* is this for one of the local ports? */
    PRINT_IPCAM_SDIO(IPCAM_SDIO_DEBUG,("\n\r%s(%d):port_num:%d,protocol:%x,dst:%02x:%02x:%02x:%02x:%02x:%02x,src:%02x:%02x:%02x:%02x:%02x:%02x\n",
  	__func__, __LINE__,
  	netif->num,
  	pattrib->protocol,
  	pattrib->dst_mac.addr[0],pattrib->dst_mac.addr[1],pattrib->dst_mac.addr[2],pattrib->dst_mac.addr[3],pattrib->dst_mac.addr[4],pattrib->dst_mac.addr[5],
  	pattrib->src_mac.addr[0],pattrib->src_mac.addr[1],pattrib->src_mac.addr[2],pattrib->src_mac.addr[3],pattrib->src_mac.addr[4],pattrib->src_mac.addr[5]));

  //it is a pkt from host, process it
	if(netif->num == 1)
	{
		PRINT_IPCAM_SDIO(IPCAM_SDIO_DEBUG,("%s(%d)\n",__func__,__LINE__));

		if(pattrib->protocol == lwip_htons(ETH_P_ARP)) //if it is an arp pkt, then amebad responds it with own mac
		{
			/* dst mac*/
			memcpy(p->payload, ipcam_sdio_mac.addr, ETH_ALEN);

			/* src mac */
			memcpy(p->payload + ETH_ALEN, xnetif[0].hwaddr, ETH_ALEN);

			struct eth_addr *src_addr, *dst_addr;
		    dst_addr = (struct eth_addr *)((u8_t *)p->payload);
		    src_addr = (struct eth_addr *)(((u8_t *)p->payload) + sizeof(struct eth_addr));
			PRINT_IPCAM_SDIO(IPCAM_SDIO_DEBUG,("%s(%d),smac:%2x,%2x,%2x,%2x,%2x,%2x,dmac:%2x,%2x,%2x,%2x,%2x,%2x\n",__func__,__LINE__,
				src_addr->addr[0],src_addr->addr[1],src_addr->addr[2],src_addr->addr[3],src_addr->addr[4],src_addr->addr[5],
				dst_addr->addr[0],dst_addr->addr[1],dst_addr->addr[2],dst_addr->addr[3],dst_addr->addr[4],dst_addr->addr[5]));

			etharp_raw(netif,
                 (struct eth_addr *)netif->hwaddr, &ipcam_sdio_mac,
                 (struct eth_addr *)netif->hwaddr, pattrib->dst_ip,
                 &ipcam_sdio_mac, pattrib->src_ip,
                 ARP_REPLY);

			pbuf_free(p);
			free(pattrib);
			return ERR_OK;
		}
	
		//host to ap
		ipcam_sdio_send_to_ports(pattrib, p);
		pbuf_free(p);
		free(pattrib);
		return ERR_OK;
	}
	else //it is a pkt from wlan0, process it
	{
		u16_t to_dir = ipcam_sdio_rcvpkt_dir(pattrib, p);
		PRINT_IPCAM_SDIO(IPCAM_SDIO_DEBUG,("%s(%d),to_dir:%d\n",__func__,__LINE__,to_dir));

		switch(to_dir) {
			case PORT_TO_BOTH:
				//pbuf copy
				q = pbuf_alloc(PBUF_RAW, p->tot_len, PBUF_POOL);
				if (q == NULL) {
				  PRINT_IPCAM_SDIO(IPCAM_SDIO_DEBUG, ("%s,pbuf alloc failed!\n",__func__));

				  free(pattrib);
				  return ERR_VAL;
				}

				if(ERR_OK != pbuf_copy(q, p)) {
				  PRINT_IPCAM_SDIO(IPCAM_SDIO_DEBUG, ("%s,pbuf copy failed!\n",__func__));
				  pbuf_free(q);

				  free(pattrib);
				  return ERR_VAL;
				}

				q->if_idx = netif_get_index(netif);

				PRINT_IPCAM_SDIO(IPCAM_SDIO_DEBUG,("%s(%d)\n",__func__,__LINE__));
				//send copy to upper level
				if(tcpip_input(p, netif) != ERR_OK ) {
				  pbuf_free(p);
				}

				//send to host
				ipcam_sdio_send_to_ports(pattrib, q);
				free(pattrib);
				pbuf_free(q);
				break;
			case PORT_TO_UP:
				//send to upper level
				PRINT_IPCAM_SDIO(IPCAM_SDIO_DEBUG,("%s(%d)\n",__func__,__LINE__));
				free(pattrib);
				tcpip_input(p, netif);
				break;
			case PORT_TO_HOST:
				//send to host
				ipcam_sdio_send_to_ports(pattrib, p);
				free(pattrib);
				pbuf_free(p);
				break;
		}
		return ERR_OK;
	}
}
