/**
  ******************************************************************************
  * @file    example_usbh_cdc_ecm.c
  * @author  Realsil WLAN5 Team
  * @version V1.0.0
  * @date    2023-1-13
  * @brief   This file provides the demo code for USB CDC ECM host class
  ******************************************************************************
  * @attention
  *
  * This module is a confidential and proprietary property of RealTek and
  * possession or use of this module requires written permission of RealTek.
  *
  * Copyright(c) 2023, Realtek Semiconductor Corporation. All rights reserved.
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------ */
#include <platform_opts.h>
#if defined(CONFIG_EXAMPLE_USBH_CDC_ECM) && CONFIG_EXAMPLE_USBH_CDC_ECM
#include <FreeRTOS.h>
#include <task.h>
#include <lwip/sockets.h>
#include <lwip/netdb.h>
#include "osdep_service.h"
#include "usbh.h"
#include "mbedtls/md5.h"
#include "lwip_netconf.h"
#include "usbh_cdc_ecm_hal.h"

/* Private defines -----------------------------------------------------------*/
extern void rltk_mii_init(void);
extern void ethernetif_mii_recv(u8 *buf, u32 total_len);
extern void dns_setserver(u8_t numdns, const ip_addr_t *dnsserver);
extern struct netif xnetif[NET_IF_NUM];
static void ecm_appx_cb_rxdata(u8 *pbuf, u32 len,u8 type);


/* Private macros ------------------------------------------------------------*/
#define ENABLE_DUMP_FILE                        0
#define ENABLE_REMOTE_FILE_DOWNLOAD             0
#define ENABLE_USER_SET_DONGLE_MAC              0

#define PBUF_MAX_LEN                            (128)
#define NETWORK_INFO_MAX_STR                    (128)

#if ENABLE_REMOTE_FILE_DOWNLOAD
#define MD5_CHECK_BUFFER_LEN                    (2)
/* socket server info */
#define SERVER_HOST                             "www.baidu.com"
#define SERVER_PORT                             80
#define RESOURCE                                "/"
#define BUFFER_SIZE                             1000      //download test buffer length
#define RECV_TO                                 60*1000   // ms
#endif

/* Private types -------------------------------------------------------------*/
/* Ethernet State */
typedef enum {
    ETH_STATUS_IDLE = 0U,
    ETH_STATUS_DEINIT,
    ETH_STATUS_INIT,
    ETH_STATUS_MAX,
} eth_state_t;
/* */
typedef enum {
    DONGLE_STATUS_IDLE = 0U,
    DONGLE_STATUS_ECM_CFG,
    DONGLE_STATUS_SEARCH,
    DONGLE_STATUS_SET_APN,
    DONGLE_STATUS_DIAG,
    DONGLE_STATUS_DIAG_STATUS,
    DONGLE_STATUS_GET_MAC,
    DONGLE_STATUS_MAX,
} dongle_state_t;

typedef struct {
    u8 ip[NETWORK_INFO_MAX_STR];
    u8 mask[NETWORK_INFO_MAX_STR];
    u8 gw[NETWORK_INFO_MAX_STR];
    u8 dns[NETWORK_INFO_MAX_STR];
} network_info_t;


static u16 led_color[1]={0x1122};
static u8 mac_valid[6]= {0x00,0x11,0x22,0x33,0x44,0x55};
static usbh_cdc_ecm_priv_data_t ecm_priv = {
	ecm_appx_cb_rxdata,
#if ENABLE_USER_SET_DONGLE_MAC
	mac_valid,
	led_color,
	sizeof(led_color)/sizeof(led_color[0]),
#else
	NULL,
	NULL,
	0,
#endif
};

/* Private variables ---------------------------------------------------------*/
#if ENABLE_REMOTE_FILE_DOWNLOAD
static unsigned char dl_buf[BUFFER_SIZE + 1];
static mbedtls_md5_context ctx;
#if ENABLE_DUMP_FILE
#define configTOTAL_PSRAM_HEAP_SIZE_TEST        (29000)
static unsigned char dump_psRAMHeap[configTOTAL_PSRAM_HEAP_SIZE_TEST];
#endif
#endif  //ENABLE_REMOTE_FILE_DOWNLOAD

static network_info_t network_info;
static u8 dhcp_done = 0;
static u8 appx_rx = 0;
static u8 appx_cereg_ready = 0; //
static u8 appx_mac_ready = 0;   //
static u8 appx_ip_ready = 0;    //get all the ip info
static u8 dongle_mac[6];

/* Private function prototypes -----------------------------------------------*/
static u32 demo_strlen(u8* pbuf){
	return strlen((char *)pbuf);
}
static void usbh_cdc_ecm_set_netinfo(u8 *pbuf,u8 *name)
{
	const char *pname = (const char *)name;
	u8 len = 0;

	if(pbuf[0] == '"') pbuf ++;
	len = demo_strlen(pbuf);
	if(pbuf[len-1] == '"') pbuf[len-1]=0;
	len = demo_strlen(pbuf);
	printf("Type(%s)=[%s(%d)]\n",name,pbuf,len);

	//whether support netinfo
	if(_strcmp(pname, "gw") == 0){
		memcpy(network_info.gw,pbuf,len);
	} else if(_strcmp(pname, "ip") == 0){
		memcpy(network_info.ip,pbuf,len);
	} else if(_strcmp(pname, "mask") == 0){
		memcpy(network_info.mask,pbuf,len);
	} else if(_strcmp(pname, "dns") == 0){
		memcpy(network_info.dns,pbuf,len);
	} else {
		printf("Unknown name(%s)\n",name);
	}
}
static u8* usbh_cdc_ecm_get_netinfo(u8 *name)
{
	const char *pname = (const char *)name;

	//whether support netinfo
	if(_strcmp(pname, "gw") == 0){
		return network_info.gw;
	} else if(_strcmp(pname, "ip") == 0){
		return network_info.ip;
	} else if(_strcmp(pname, "mask") == 0){
		return network_info.mask;
	} else if(_strcmp(pname, "dns") == 0){
		return network_info.dns;
	} else {
		printf("Unknown name(%s)\n",name);
	}

	return NULL;
}

static void ecm_appx_cb_rxdata(u8 *pbuf, u32 len,u8 type) //type is usb transfer type
{
	u32 i;
	appx_rx = 1;

	//return ;//force to exit
	printf("[ECM]appx(%d) data received len(%d) \n",type,len);

	for (i = 0; i < len;) {
		if (i + 10 <= len) {
			printf("%c%c%c%c%c%c%c%c%c%c", 
				pbuf[i], pbuf[i + 1], pbuf[i + 2], pbuf[i + 3], pbuf[i + 4], 
				pbuf[i + 5], pbuf[i + 6], pbuf[i + 7],pbuf[i + 8],pbuf[i + 9]);
			i += 10;
		} else {
			for (; i < len; i++) {
				printf("%c", pbuf[i]);
			}
			printf("\n\n");
			break;
		}
	}

	/* EG91 */
	if (strstr((char*)pbuf, "+CEREG")) { ///parse to get the network information
		char *buff=(char *)pbuf;
		char* p;
		p = strsep(&buff, ",");
		p = strsep(&buff, ",");
		if(p &&((*p == '1' || *p == '5'))){
			appx_cereg_ready = 1;	
		}
	}
	if (strstr((char*)pbuf, "+WWANINFO")) {
		/*		 Apn:ipv4:ipaddr:GW:Mask:DNS:back DNS
			+WWANINFO:0,"IP","10.16.120.241","10.16.120.242","255.255.255.252","218.2.2.2","218.4.4.4"
		*/
		char *buff=(char *)pbuf;
		char* p = strsep(&buff, ",");
		p = strsep(&buff, ",");
		p = strsep(&buff, ",");
		//parse to get the ip
		usbh_cdc_ecm_set_netinfo((u8*)p,(u8*)"ip");
		p = strsep(&buff, ",");
		usbh_cdc_ecm_set_netinfo((u8*)p,(u8*)"gw");
		p = strsep(&buff, ",");
		//parse to get the gw
		usbh_cdc_ecm_set_netinfo((u8*)p,(u8*)"mask");
		p = strsep(&buff, ",");
		//parse to get the dns
		usbh_cdc_ecm_set_netinfo((u8*)p,(u8*)"dns");
		p = strsep(&buff, ",");

		appx_ip_ready = 1;
	}
	if (strstr((char*)pbuf, "+QNETIFMAC")) { ///parse to get the mac info
		/*
			+QNETIFMAC:"ea:24:64:58:ca:83"
		*/
		char *buff=(char *)pbuf;
		char* p = strsep(&buff, "\"");
		p = strsep(&buff, "\"");
		if(p){
			for(i = 0; i< 6;i++)
			{
				dongle_mac[i] = usbh_cdc_ecm_hex_to_char(p[3*i+0])*16 + usbh_cdc_ecm_hex_to_char(p[3*i+1]) ;
			}
			printf("MAC:%02x:%02x:%02x:%02x:%02x:%02x\n",dongle_mac[0],dongle_mac[1],dongle_mac[2],dongle_mac[3],dongle_mac[4],dongle_mac[5]);
			appx_mac_ready = 1;
		}
	}

	/* EG915 */
	if (strstr((char*)pbuf, "+CGCONTRDP")) { ///parse to get the network information
		char *buff=(char *)pbuf;
		u8 *tail ;
		char* p = strsep(&buff, ",");
		p = strsep(&buff, ",");
		p = strsep(&buff, ",");
		p = strsep(&buff, ",");
		//parse to get the ip/mask
		if(p){	
			u8 dotcount=0;
			u8 ptmp[100];
			memset(ptmp, 0, 100);
			memcpy(ptmp,p,strlen(p));
			tail = ptmp ;
			do{
				if(*tail=='.'){
					dotcount ++;
					if(dotcount == 4)
					{
						*tail=0;
						tail++;
						break;
					}
				}
				tail++;
			}while(1);
			usbh_cdc_ecm_set_netinfo(ptmp,(u8*)"ip");
			usbh_cdc_ecm_set_netinfo(tail,(u8*)"mask");
		}
		p = strsep(&buff, ",");
		//parse to get the gw
		usbh_cdc_ecm_set_netinfo((u8*)p,(u8*)"gw");
		p = strsep(&buff, ",");
		//parse to get the dns
		usbh_cdc_ecm_set_netinfo((u8*)p,(u8*)"dns");
		p = strsep(&buff, ",");

		appx_ip_ready = 1;
	}
}

static int ecm_netif_init(void)
{
	char* pipaddr;
	char* pgw;
	char* pnetmask;
	char* pdnsserver;
	ip4_addr_t ipaddr, gw, netmask, dnsserver;
	int ret;

	pipaddr = (char*)usbh_cdc_ecm_get_netinfo((u8*)"ip");
	if(NULL == pipaddr) {
		printf("Get ip failed !\n");
		return -1;
	}
	else {
		ret = inet_aton(pipaddr, &ipaddr);
		if(ret == 0) {
			printf("Inet_aton ip failed !\n");
			return -1;
		}
		netif_set_ipaddr(&xnetif[NET_IF_NUM - 1], &ipaddr);
	}
		
	pnetmask = (char*)usbh_cdc_ecm_get_netinfo((u8*)"mask");
	if(NULL == pnetmask) {
		printf("Get mask failed !\n");
		return -1;
	}
	else {
		ret = inet_aton(pnetmask, &netmask);
		if(ret == 0) {
			printf("Inet_aton netmask failed !\n");
			return -1;
		}
		netif_set_netmask(&xnetif[NET_IF_NUM - 1], &netmask);
	}

	pgw = (char*)usbh_cdc_ecm_get_netinfo((u8*)"gw");
	if(NULL == pgw) {
		printf("Get gw failed !\n");
		return -1;
	}
	else {
		ret = inet_aton(pgw, &gw);
		if(ret == 0) {
			printf("Inet_aton gw failed !\n");
			return -1;
		}
		netif_set_gw(&xnetif[NET_IF_NUM - 1], &gw);
	}

	pdnsserver = (char*)usbh_cdc_ecm_get_netinfo((u8*)"dns");
	if(NULL == pdnsserver) {
		printf("Get dns failed !\n");
		return -1;
	}
	else {
		ret = inet_aton(pdnsserver, &dnsserver);
		if(ret == 0) {
			printf("Inet_aton dns failed !\n");
			return -1;
		}
		dns_setserver(0, &dnsserver);
	}	
	printf("Run %s ok. \n", __func__);
	return 0; 
}

static u8 cdc_ecm_do_init(void)
{
	return usbh_cdc_ecm_do_init(ethernetif_mii_recv,&ecm_priv);
}
static u8 ecm_dongle_EG915_diag(void)
{
	u8 heart_beat = 0;
	u8 idx;
	u8 pbuf[PBUF_MAX_LEN];
	dongle_state_t dongle = DONGLE_STATUS_ECM_CFG;

	u8* pdata0=(u8*)"AT+qdbgcfg=\"tracecfg\",0,1";
	u8* pdata1=(u8*)"AT+QCFG=\"usbnet\",1";
	u8* pdata2=(u8*)"AT+QNWINFO;+CEREG?;+CREG?;+QCSQ;+CSQ;+COPS?;+CPIN?;+QCCID";
	u8* pdata3=(u8*)"AT+CGDCONT=1,\"IP\",\"orange.m2m.spec\"";
	u8* pdata4=(u8*)"AT+QNETDEVCTL=1,1,1";
	u8* pdata5=(u8*)"AT+CGCONTRDP";

	while(1) {
		if(heart_beat++ % 10 == 0){
			printf("\n\nEnter dongle(%d)--------------------------------------------\n",dongle);
		}
		switch(dongle){
			case DONGLE_STATUS_IDLE: //for debug issue
				while(appx_rx == 0){
					rtw_msleep_os(10);
				}
				memset(pbuf,0x00,PBUF_MAX_LEN);
				memcpy(pbuf,pdata0,demo_strlen(pdata0));
				pbuf[demo_strlen(pdata0)+0]= 0x0D;
				pbuf[demo_strlen(pdata0)+1]= 0x0A;
				usbh_cdc_ecm_appx_send_data(pbuf,demo_strlen(pbuf));
				dongle ++;
				break;
			case DONGLE_STATUS_ECM_CFG:
				if(1){
					memset(pbuf,0x00,PBUF_MAX_LEN);
					memcpy(pbuf,pdata1,demo_strlen(pdata1));
					pbuf[demo_strlen(pdata1)+0]= 0x0D;
					pbuf[demo_strlen(pdata1)+1]= 0x0A;
					usbh_cdc_ecm_appx_send_data(pbuf,demo_strlen(pbuf));
				}
				dongle ++;
				break;
			case DONGLE_STATUS_SEARCH:
				if(1){
					memset(pbuf,0x00,PBUF_MAX_LEN);
					memcpy(pbuf,pdata2,demo_strlen(pdata2));
					pbuf[demo_strlen(pdata2)+0]= 0x0D;
					pbuf[demo_strlen(pdata2)+1]= 0x0A;
					usbh_cdc_ecm_appx_send_data(pbuf,demo_strlen(pbuf));
				}
				dongle ++;
				break;
			case DONGLE_STATUS_SET_APN:
				if(1){
					memset(pbuf,0x00,PBUF_MAX_LEN);
					memcpy(pbuf,pdata3,demo_strlen(pdata3));
					pbuf[demo_strlen(pdata3)+0]= 0x0D;
					pbuf[demo_strlen(pdata3)+1]= 0x0A;
					usbh_cdc_ecm_appx_send_data(pbuf,demo_strlen(pbuf));
				}
				dongle ++;
				break;
			case DONGLE_STATUS_DIAG:
				if(1){
					memset(pbuf,0x00,PBUF_MAX_LEN);
					memcpy(pbuf,pdata4,demo_strlen(pdata4));
					pbuf[demo_strlen(pdata4)+0]= 0x0D;
					pbuf[demo_strlen(pdata4)+1]= 0x0A;
					usbh_cdc_ecm_appx_send_data(pbuf,demo_strlen(pbuf));
					rtw_msleep_os(15*1000);
				}
				dongle ++;
				break;
			case DONGLE_STATUS_DIAG_STATUS:
				if(0 == appx_ip_ready){
					memset(pbuf,0x00,PBUF_MAX_LEN);
					memcpy(pbuf,pdata5,demo_strlen(pdata5));
					pbuf[demo_strlen(pdata5)+0]= 0x0D;
					pbuf[demo_strlen(pdata5)+1]= 0x0A;
					usbh_cdc_ecm_appx_send_data(pbuf,demo_strlen(pbuf));
				}
				idx = 0;
				do{
					idx ++;
					if(appx_ip_ready)
						break;
					rtw_msleep_os(1000);
				}while(idx < 10);
				if(appx_ip_ready){
					dongle = DONGLE_STATUS_MAX; ///maybe should add a check for AT RX msg,and allow to switch to next stage
				}
				break;
			case DONGLE_STATUS_MAX:
				printf("Get all network info !\n");
			default:
				rtw_msleep_os(10);
				return 0;
				break;
		}
		rtw_msleep_os(1000);///force sleep to wait the reply
	}
	return 1;
}
static u8 ecm_dongle_EG91_diag(void)
{
	u8 heart_beat = 0;
	u8 idx;
	u8 pbuf[PBUF_MAX_LEN];
	dongle_state_t dongle = DONGLE_STATUS_ECM_CFG;

	u8* pdata0=(u8*)"AT+qdbgcfg=\"tracecfg\",0,1";
	u8* pdata1=(u8*)"AT+QCFG=\"usbnet\",1";
	u8* pdata2=(u8*)"AT+QNWINFO;+CEREG?;+CREG?;+QCSQ;+CSQ;+COPS?;+CPIN?;+QCCID";
	u8* pdata3=(u8*)"AT+CGDCONT=1,\"IP\"";
	u8* pdata4=(u8*)"AT+CEREG?";    //
	u8* pdata5=(u8*)"at+qwwaninfo=0,\"IP\"";
	u8* pdata6=(u8*)"AT+qnetifmac=\"ecm0\"";

	while(1) {
		if(heart_beat++ % 10 == 0){
			printf("\n\nEnter dongle(%d)--------------------------------------------\n",dongle);
		}
		switch(dongle){
			case DONGLE_STATUS_IDLE: //for debug issue
				while(appx_rx == 0){
					rtw_msleep_os(10);
				}
				memset(pbuf,0x00,PBUF_MAX_LEN);
				memcpy(pbuf,pdata0,demo_strlen(pdata0));
				pbuf[demo_strlen(pdata0)+0]= 0x0D;
				pbuf[demo_strlen(pdata0)+1]= 0x0A;
				usbh_cdc_ecm_appx_send_data(pbuf,demo_strlen(pbuf));
				dongle ++;
				break;
			case DONGLE_STATUS_ECM_CFG:
				if(1){
					memset(pbuf,0x00,PBUF_MAX_LEN);
					memcpy(pbuf,pdata1,demo_strlen(pdata1));
					pbuf[demo_strlen(pdata1)+0]= 0x0D;
					pbuf[demo_strlen(pdata1)+1]= 0x0A;
					usbh_cdc_ecm_appx_send_data(pbuf,demo_strlen(pbuf));
				}
				dongle ++;
				break;
			case DONGLE_STATUS_SEARCH:
				if(1){
					memset(pbuf,0x00,PBUF_MAX_LEN);
					memcpy(pbuf,pdata2,demo_strlen(pdata2));
					pbuf[demo_strlen(pdata2)+0]= 0x0D;
					pbuf[demo_strlen(pdata2)+1]= 0x0A;
					usbh_cdc_ecm_appx_send_data(pbuf,demo_strlen(pbuf));
				}
				dongle ++;
				break;
			case DONGLE_STATUS_SET_APN:
				if(1){
					memset(pbuf,0x00,PBUF_MAX_LEN);
					memcpy(pbuf,pdata3,demo_strlen(pdata3));
					pbuf[demo_strlen(pdata3)+0]= 0x0D;
					pbuf[demo_strlen(pdata3)+1]= 0x0A;
					usbh_cdc_ecm_appx_send_data(pbuf,demo_strlen(pbuf));
				}
				dongle ++;
				break;
			case DONGLE_STATUS_DIAG:
				if(1){
					memset(pbuf,0x00,PBUF_MAX_LEN);
					memcpy(pbuf,pdata4,demo_strlen(pdata4));
					pbuf[demo_strlen(pdata4)+0]= 0x0D;
					pbuf[demo_strlen(pdata4)+1]= 0x0A;
					usbh_cdc_ecm_appx_send_data(pbuf,demo_strlen(pbuf));
				}
				idx = 0;
				do{
					idx ++;
					if(appx_cereg_ready)
						break;
					rtw_msleep_os(1000);
				}while(idx < 10);
				if(appx_cereg_ready){
					//EG 91 use DHCP to get mac
					dongle = DONGLE_STATUS_GET_MAC;
				}
				break;
			case DONGLE_STATUS_DIAG_STATUS:
				if(1){
					rtw_msleep_os(15*1000);
					memset(pbuf,0x00,PBUF_MAX_LEN);
					memcpy(pbuf,pdata5,demo_strlen(pdata5));
					pbuf[demo_strlen(pdata5)+0]= 0x0D;
					pbuf[demo_strlen(pdata5)+1]= 0x0A;
					usbh_cdc_ecm_appx_send_data(pbuf,demo_strlen(pbuf));
				}
				idx = 0;
				do{
					idx ++;
					if(appx_ip_ready)
						break;
					rtw_msleep_os(1000);
				}while(idx < 10);
				if(appx_ip_ready){
					dongle = DONGLE_STATUS_GET_MAC; ///maybe should add a check for AT RX msg,and allow to switch to next stage
				}
				break;
			case DONGLE_STATUS_GET_MAC:
				if(1){
					memset(pbuf,0x00,PBUF_MAX_LEN);
					memcpy(pbuf,pdata6,demo_strlen(pdata6));
					pbuf[demo_strlen(pdata6)+0]= 0x0D;
					pbuf[demo_strlen(pdata6)+1]= 0x0A;
					usbh_cdc_ecm_appx_send_data(pbuf,demo_strlen(pbuf));
				}
				idx = 0;
				do{
					idx ++;
					if(appx_mac_ready)
						break;
					rtw_msleep_os(1000);
				}while(idx < 10);
				if(appx_mac_ready){
					dongle = DONGLE_STATUS_MAX;
				}
				break;
			case DONGLE_STATUS_MAX:
				printf("Send request finish!\n");
			default:
				rtw_msleep_os(10);
				return 0;
				break;
		}
		rtw_msleep_os(1000);///force sleep to wait the reply
	}
	return 1;
}
/*
	for different dongle, do the prepare for the ecm
	default:no nothing
	EG915/EG91/xx send cmd to AT port
*/
static u8 ecm_dongle_diag_cmd(void)
{
	u16 vid = usbh_cdc_ecm_get_device_vid();
	u16 pid = usbh_cdc_ecm_get_device_pid();
	if(USB_DEFAULT_VID == vid){
		return 0;
	} else if(USB_EF_DONGLE_VID == vid){
		if(pid == EF_DONGLE_PID_EG915){
			return ecm_dongle_EG915_diag();
		} else if(pid == EF_DONGLE_PID_EG91){
			return ecm_dongle_EG91_diag();
		} else {
			printf("Error happen, check the pid(%d)\n",pid);
		}
	} else {
		printf("Error happen, check the vid(%d)\n",vid);
	}
	return 0;
}
/*
	
*/
static usbh_cdc_ecm_appx_param_t dongle_array[]={
	{USB_DEFAULT_VID,0,0},
	{USB_EF_DONGLE_VID, EF_DONGLE_PID_EG915, 2},
	{USB_EF_DONGLE_VID, EF_DONGLE_PID_EG91,  2},
	{0,0,0},
};
static void ecm_link_change_thread(void *param) {
	UNUSED(param);
	u8 *mac;
	u8 link_is_up = 0;
	u16 vid,pid ;
	u32 dhcp_status = 0;
	eth_state_t ethernet_unplug = ETH_STATUS_IDLE;

	cdc_ecm_do_init();
	rltk_mii_init();
	usbh_cdc_ecm_appx_set_param(dongle_array);
	do{
		if(usbh_cdc_ecm_usb_is_ready())
			break;
		rtw_msleep_os(1000);
	}while(1);
	printf("Ecm is ready!\n");

	ecm_dongle_diag_cmd();
	vid = usbh_cdc_ecm_get_device_vid();
	pid = usbh_cdc_ecm_get_device_pid();

	while(1) {
		link_is_up = usbh_cdc_ecm_get_connect_status();
		//printf("Link_is_up = %d\n", link_is_up);
		if((USB_DEFAULT_VID == vid)
			|| ((USB_EF_DONGLE_VID == vid) &&(pid == EF_DONGLE_PID_EG91))){
			//dhcp issue, ethernet
			if(1 == link_is_up && (ethernet_unplug < ETH_STATUS_INIT)) {	// unlink -> link
				ethernet_unplug = ETH_STATUS_INIT;
				if((vid == USB_EF_DONGLE_VID)&&(pid == EF_DONGLE_PID_EG91)){
					mac = dongle_mac;
				} else {
					mac = (u8*)usbh_cdc_ecm_process_mac_str();
				}
				printf("Will do dhcp MAC:%02x:%02x:%02x:%02x:%02x:%02x\n", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
				memcpy(xnetif[NET_IF_NUM - 1].hwaddr, mac, 6);

				dhcp_status = LwIP_DHCP(NET_IF_NUM - 1, DHCP_START);
				if(DHCP_ADDRESS_ASSIGNED == dhcp_status) {
					netif_set_link_up(&xnetif[NET_IF_NUM - 1]);
					netif_set_default(&xnetif[NET_IF_NUM - 1]);	//Set default gw to ether netif
					dhcp_done = 1;
					printf("Switch to link !!\n");
				} else {
					printf("Dhcp fail !!\n");
				}
			}
			else if(0 == link_is_up && (ethernet_unplug >= ETH_STATUS_INIT)) {	// link -> unlink
				ethernet_unplug = ETH_STATUS_DEINIT;
				netif_set_default(&xnetif[0]);
				printf("Swicth to unlink !!\n");
				//should switch to ecm_dongle_diag_cmd
			}
			else {
				rtw_msleep_os(1000);
			}
		} else if((vid == USB_EF_DONGLE_VID)&&(pid == EF_DONGLE_PID_EG915)){
			if(1 == link_is_up && (ethernet_unplug < ETH_STATUS_INIT)) {
				mac = (u8*)usbh_cdc_ecm_process_mac_str();
				printf("MAC:%02x:%02x:%02x:%02x:%02x:%02x\n", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
				memcpy(xnetif[NET_IF_NUM - 1].hwaddr, mac, 6);
				if(ecm_netif_init() == 0) {
					ethernet_unplug = ETH_STATUS_INIT;
					netif_set_up(&xnetif[NET_IF_NUM - 1]);
					netif_set_link_up(&xnetif[NET_IF_NUM - 1]);
					netif_set_default(&xnetif[NET_IF_NUM - 1]);//choose the route path
					dhcp_done = 1;
					printf("Switch to link !!\n");
				}
			} else if(0 == link_is_up && (ethernet_unplug >= ETH_STATUS_INIT)) {
				ethernet_unplug = ETH_STATUS_DEINIT;
				netif_set_default(&xnetif[0]);
				printf("Swicth to unlink !!\n");
				//should switch to ecm_dongle_diag_cmd
			} else {
				rtw_msleep_os(1000);
			}
		} else {
			printf("Error, the VID(%d)PID(%d) can not match! \n",vid,pid);
		}
	}
}



#if ENABLE_REMOTE_FILE_DOWNLOAD
static void save_to_memory(char *pdata, unsigned int length)
{
#if ENABLE_DUMP_FILE
	static unsigned int psram_pos = 0;
	if (configTOTAL_PSRAM_HEAP_SIZE_TEST >= psram_pos + length) {
		memcpy((void *)&dump_psRAMHeap[psram_pos], pdata, length);
		psram_pos += length;
	}
#else
	UNUSED(length);
	UNUSED(pdata);
#endif
}
static int update_data_check(char *pdata, unsigned int length)
{
	printf("Data length = %d\n",length);
	if(0==length)
		return 0;
	save_to_memory(pdata,length);
	mbedtls_md5_update(&ctx, (const unsigned char *)pdata, length);
	return  0;
}
static void write_data_to_flash(void)
{
#if ENABLE_DUMP_FILE
	printf("Dump mem to flash start\n");
	FLASH_WriteStream(0x100000, configTOTAL_PSRAM_HEAP_SIZE_TEST, dump_psRAMHeap);
	printf("Dump mem to flash done\n");
#endif
}
static void ecm_download_thread(void *param)
{
	int server_fd = -1;
	u8 heart_beat = 0;
	struct sockaddr_in server_addr;
	struct hostent *server_host;
	u32 resource_size = 0 ;
	u32 content_len = 0;
	unsigned char output[8 * MD5_CHECK_BUFFER_LEN];
	int pos = 0, read_size = 0,  header_removed = 0;
	UNUSED(param);

	printf("\n[Download]enter example \n");
	memset(output, 0x00, 8 * MD5_CHECK_BUFFER_LEN);

	while (0 == dhcp_done) {
		if( ++heart_beat % 30 == 0){
			printf("Wait for ethernet connection ...\n");
		}
		usb_os_sleep_ms(1000);
	}

	printf("\n[Download]start HTTP download \n");

	if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		printf("ERROR: socket\n");
		goto exit;
	} else {
		int recv_timeout_ms = RECV_TO;
	// lwip 1.5.0
		struct timeval recv_timeout;
		recv_timeout.tv_sec = recv_timeout_ms / 1000;
		recv_timeout.tv_usec = recv_timeout_ms % 1000 * 1000;
		setsockopt(server_fd, SOL_SOCKET, SO_RCVTIMEO, &recv_timeout, sizeof(recv_timeout));
	// lwip 1.4.1
		//setsockopt(server_fd, SOL_SOCKET, SO_RCVTIMEO, &recv_timeout_ms, sizeof(recv_timeout_ms));
	}
	printf("[Download]%s Line %d server_fd=%d \n", __func__, __LINE__, server_fd);
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(SERVER_PORT);

	// Support SERVER_HOST in IP or domain name
	server_host = gethostbyname(SERVER_HOST);
	if (server_host != NULL) {
		memcpy((void *) &server_addr.sin_addr, (void *) server_host->h_addr, 4);
	} else {
		printf("ERROR: server host\n");
		goto exit;
	}
	printf("[Download]will do connect %s\n", SERVER_HOST);
	if (connect(server_fd, (struct sockaddr *) &server_addr, sizeof(server_addr)) == 0) {
		pos = 0, read_size = 0, resource_size = 0, content_len = 0, header_removed = 0;
		printf("[Download]connect success \n");
		sprintf((char *)dl_buf, "GET %s HTTP/1.1\r\nHost: %s\r\n\r\n", RESOURCE, SERVER_HOST);
		u32 max = strlen((char const *)dl_buf);

		if (0) {
			u32 index = 0 ;
			printf("\n[Download][get Msg %d]", max);
			for (index = 0; index < max; index++) {
				printf("0x%x ", dl_buf[index]);
			}
			printf("\n\n\n");
		}

		write(server_fd, (char const *)dl_buf, strlen((char const *)dl_buf));
		printf("[Download]will call read \n");
		mbedtls_md5_init(&ctx);
		mbedtls_md5_starts(&ctx);

		while(((content_len == 0)||(resource_size < content_len)) /**/
			&&((read_size = read(server_fd, dl_buf + pos, BUFFER_SIZE - pos)) > 0)) {
			if(header_removed == 0) {
				char *header = NULL;

				pos += read_size;
				dl_buf[pos] = 0;
				header = strstr((char const *)dl_buf, "\r\n\r\n");

				if (header) {
					char *body, *content_len_pos;

					body = header + strlen("\r\n\r\n");
					*(body - 2) = 0;
					header_removed = 1;
					printf("\nHTTP Header: %s\n", dl_buf);

					// Remove header size to get first read size of data from body head
					read_size = pos - ((unsigned char *) body - dl_buf);
					update_data_check(body, read_size);
					printf("[Download]body=0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x \n", body[0], body[1], body[2], body[3], body[4], body[5], body[6], body[7]);
					pos = 0;
					content_len_pos = strstr((char const *)dl_buf, "Content-Length: ");
					if (content_len_pos) {
						content_len_pos += strlen("Content-Length: ");
						*(char *)(strstr(content_len_pos, "\r\n")) = 0;
						content_len = atoi(content_len_pos);
						printf("[Download]content len: %d\n", content_len);
					}
				} else {
					if (pos >= BUFFER_SIZE) {
						printf("ERROR: HTTP header\n");
						goto exit;
					}
					continue;
				}
			} else {
				update_data_check((char *)(dl_buf + pos), (unsigned int)read_size);
			}

			resource_size += read_size;
			printf("[Download]read resource %d bytes/receive=%d/total=%d ------------------------------------------------\n",
				   read_size, resource_size, content_len);
		}
		write_data_to_flash();
		mbedtls_md5_finish(&ctx, output);
		mbedtls_md5_free(&ctx);

		printf("[Download]mbedtls_md5_finish md5 \n");
		for (u8 hh = 0; hh < MD5_CHECK_BUFFER_LEN; hh++) {
			printf("[Download]md5 %d=%02x%02x%02x%02x%02x%02x%02x%02x\n\n", hh,
				   output[8 * hh + 0], output[8 * hh + 1], output[8 * hh + 2], output[8 * hh + 3],
				   output[8 * hh + 4], output[8 * hh + 5], output[8 * hh + 6], output[8 * hh + 7]);
		}

		printf("[Download]exit read. ret = %d/%d\n", read_size, __LINE__);
		printf("[Download]http content-length = %d bytes, download resource size = %d bytes/%d\n", content_len, resource_size, __LINE__);
		if (0) {
			u32 index = 0 ;
			printf("\n[Download][2get Msg %d]", max);
			for (index = 0; index < max; index++) {
				printf("0x%x ", dl_buf[index]);
			}
			printf("\n\nwill call write \n");
		}
	} else {
		printf("[Download]ERROR: connect\n");
	}

exit:
	if (server_fd >= 0) {
		close(server_fd);
	}

	rtw_thread_exit();
}
#endif

/* Exported functions --------------------------------------------------------*/
void example_usbh_cdc_ecm(void)
{
	int status;
	struct task_struct monitor_task;
#if ENABLE_REMOTE_FILE_DOWNLOAD
	struct task_struct download_task;
#endif
	printf("\n[CDC_ECM] USB host cdc_ecm demo started...\n");

	status = rtw_create_task(&monitor_task, "ecm_link_thread", 5*512, tskIDLE_PRIORITY + 3, (thread_func_t)ecm_link_change_thread, NULL);
	if (status != pdPASS) {
		printf("\nFail to create ECM monitor_link_change thread: %d\n", status);
	}
#if ENABLE_REMOTE_FILE_DOWNLOAD
	status = rtw_create_task(&download_task, "ecm_download_thread", 5 * 512, tskIDLE_PRIORITY + 2, (thread_func_t)ecm_download_thread, NULL);
	if (status != pdPASS) {
		printf("\nFail to create ECM download  thread\n");
	}
#endif
}

/*
	for AT cmd test entry
*/
static u32 usbh_ecm_at_test(u16 argc, u8 *argv[]){
	u8 *cmd;
	u8 pbuf[PBUF_MAX_LEN];

	if (argc == 0) {
		printf("Invalid argument\n");
		return HAL_ERR_PARA;
	}

	cmd = (u8 *)argv[0];

	printf("At cmd(%s)\n",cmd);
	if(USB_EF_DONGLE_VID == usbh_cdc_ecm_get_device_vid()){
		memset(pbuf,0x00,PBUF_MAX_LEN);
		memcpy(pbuf,cmd,demo_strlen(cmd));
		pbuf[demo_strlen(cmd)+0]= 0x0D;
		pbuf[demo_strlen(cmd)+1]= 0x0A;
		usbh_cdc_ecm_appx_send_data(pbuf,demo_strlen(pbuf));
	} else {
	}
	printf("At cmd(%s) finish!\n",cmd);
	return 0;
}

CMD_TABLE_DATA_SECTION
const COMMAND_TABLE usbh_ecm_tx_atcmd[] = {
	{
		(const u8 *)"ecmh", 3, usbh_ecm_at_test, (const u8 *)"\tUSB host ECM AT test cmd:\n"
		"\t\t ecmh atcmd\n"
	}
};

#endif
