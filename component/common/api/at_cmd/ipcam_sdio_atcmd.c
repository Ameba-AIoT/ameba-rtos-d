#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "log_service.h"
#include "atcmd_wifi.h"
#include <lwip_netconf.h>
#include "tcpip.h"
#include <dhcp/dhcps.h>
#include "spdio_api.h"
#if CONFIG_WLAN
#include <wlan/wlan_test_inc.h>
#include <wifi/wifi_conf.h>
#include <wifi/wifi_util.h>
#endif

#include "platform_opts.h"
#if defined(CONFIG_PLATFORM_8710C)||defined(CONFIG_PLATFORM_8721D)
#include "platform_opts_bt.h"
#endif

extern struct spdio_t ipcam_sdio_dev;

/******************************************************************************/
#define	_AT_WLAN_SET_SSID_          "ASW0"
#define	_AT_WLAN_SET_PASSPHRASE_    "ASW1"
#define	_AT_WLAN_GET_IP_            "ASW6"
#define	_AT_WLAN_AP_ACTIVATE_       "ASWA"
#define	_AT_WLAN_JOIN_NET_          "ASWC"
#define	_AT_WLAN_DISC_NET_          "ASWD"
#define	_AT_WLAN_SCAN_              "ASWS"
#define _AT_WLAN_IWPRIV_            "ASWZ"
#define	_AT_WLAN_INFO_              "ASW?"

#if CONFIG_WLAN
#if CONFIG_LWIP_LAYER
extern struct netif xnetif[NET_IF_NUM]; 
#endif

static rtw_network_info_t ipcam_sdio_wifi = {0};
static rtw_ap_info_t ipcam_sdio_ap = {0};
static unsigned char ipcam_sdio_password[65] = {0};

rtw_mode_t ipcam_sdio_wifi_mode = RTW_MODE_STA;

static void ipcam_sdio_init_wifi_struct(void)
{
	memset(ipcam_sdio_wifi.ssid.val, 0, sizeof(ipcam_sdio_wifi.ssid.val));
	memset(ipcam_sdio_wifi.bssid.octet, 0, ETH_ALEN);	
	memset(ipcam_sdio_password, 0, sizeof(ipcam_sdio_password));
	ipcam_sdio_wifi.ssid.len = 0;
	ipcam_sdio_wifi.password = NULL;
	ipcam_sdio_wifi.password_len = 0;
	ipcam_sdio_wifi.key_id = -1;
	memset(ipcam_sdio_ap.ssid.val, 0, sizeof(ipcam_sdio_ap.ssid.val));
	ipcam_sdio_ap.ssid.len = 0;
	ipcam_sdio_ap.password = NULL;
	ipcam_sdio_ap.password_len = 0;
	ipcam_sdio_ap.channel = 1;
}

static void ipcam_sdio_print_scan_result( rtw_scan_result_t* record )
{
#if (defined(CONFIG_EXAMPLE_UART_ATCMD) && CONFIG_EXAMPLE_UART_ATCMD) || (defined(CONFIG_EXAMPLE_SPI_ATCMD) && CONFIG_EXAMPLE_SPI_ATCMD)
	at_printf("%s,%d,%s,%d,"MAC_FMT"", record->SSID.val, record->channel,
			( record->security == RTW_SECURITY_OPEN ) ? "Open" :
			( record->security == RTW_SECURITY_WEP_PSK ) ? "WEP" :
			( record->security == RTW_SECURITY_WPA_TKIP_PSK ) ? "WPA TKIP" :
			( record->security == RTW_SECURITY_WPA_AES_PSK ) ? "WPA AES" :
			( record->security == RTW_SECURITY_WPA_MIXED_PSK ) ? "WPA Mixed" :
			( record->security == RTW_SECURITY_WPA2_AES_PSK ) ? "WPA2 AES" :
			( record->security == RTW_SECURITY_WPA2_TKIP_PSK ) ? "WPA2 TKIP" :
			( record->security == RTW_SECURITY_WPA2_MIXED_PSK ) ? "WPA2 Mixed" :
			( record->security == RTW_SECURITY_WPA_WPA2_TKIP_PSK) ? "WPA/WPA2 TKIP" :
			( record->security == RTW_SECURITY_WPA_WPA2_AES_PSK) ? "WPA/WPA2 AES" :
			( record->security == RTW_SECURITY_WPA_WPA2_MIXED_PSK) ? "WPA/WPA2 Mixed" :
			( record->security == RTW_SECURITY_WPA_TKIP_ENTERPRISE ) ? "WPA TKIP Enterprise" :
			( record->security == RTW_SECURITY_WPA_AES_ENTERPRISE ) ? "WPA AES Enterprise" :
			( record->security == RTW_SECURITY_WPA_MIXED_ENTERPRISE ) ? "WPA Mixed Enterprise" :
			( record->security == RTW_SECURITY_WPA2_TKIP_ENTERPRISE ) ? "WPA2 TKIP Enterprise" :
			( record->security == RTW_SECURITY_WPA2_AES_ENTERPRISE ) ? "WPA2 AES Enterprise" :
			( record->security == RTW_SECURITY_WPA2_MIXED_ENTERPRISE ) ? "WPA2 Mixed Enterprise" :
			( record->security == RTW_SECURITY_WPA_WPA2_TKIP_ENTERPRISE ) ? "WPA/WPA2 TKIP Enterprise" :
			( record->security == RTW_SECURITY_WPA_WPA2_AES_ENTERPRISE ) ? "WPA/WPA2 AES Enterprise" :
			( record->security == RTW_SECURITY_WPA_WPA2_MIXED_ENTERPRISE ) ? "WPA/WPA2 Mixed Enterprise" :
#ifdef CONFIG_SAE_SUPPORT
			( record->security == RTW_SECURITY_WPA3_AES_PSK) ? "WPA3-SAE AES" :
			( record->security == RTW_SECURITY_WPA2_WPA3_MIXED) ? "WPA2/WPA3-SAE AES" :
#endif
			"Unknown",
			record->signal_strength, MAC_ARG(record->BSSID.octet)   );
#else
	RTW_API_INFO("%s\t ", ( record->bss_type == RTW_BSS_TYPE_ADHOC ) ? "Adhoc" : "Infra");
	RTW_API_INFO(MAC_FMT, MAC_ARG(record->BSSID.octet));
	RTW_API_INFO(" %d\t ", record->signal_strength);
	RTW_API_INFO(" %d\t  ", record->channel);
	RTW_API_INFO(" %d\t  ", record->wps_type);
	RTW_API_INFO("%s\t\t ", ( record->security == RTW_SECURITY_OPEN ) ? "Open" :
								 ( record->security == RTW_SECURITY_WEP_PSK ) ? "WEP" :
								 ( record->security == RTW_SECURITY_WPA_TKIP_PSK ) ? "WPA TKIP" :
								 ( record->security == RTW_SECURITY_WPA_AES_PSK ) ? "WPA AES" :
								 ( record->security == RTW_SECURITY_WPA_MIXED_PSK ) ? "WPA Mixed" :
								 ( record->security == RTW_SECURITY_WPA2_AES_PSK ) ? "WPA2 AES" :
								 ( record->security == RTW_SECURITY_WPA2_TKIP_PSK ) ? "WPA2 TKIP" :
								 ( record->security == RTW_SECURITY_WPA2_MIXED_PSK ) ? "WPA2 Mixed" :
								 ( record->security == RTW_SECURITY_WPA_WPA2_TKIP_PSK) ? "WPA/WPA2 TKIP" :
								 ( record->security == RTW_SECURITY_WPA_WPA2_AES_PSK) ? "WPA/WPA2 AES" :
								 ( record->security == RTW_SECURITY_WPA_WPA2_MIXED_PSK) ? "WPA/WPA2 Mixed" :
								 ( record->security == RTW_SECURITY_WPA_TKIP_ENTERPRISE ) ? "WPA TKIP Enterprise" :
								 ( record->security == RTW_SECURITY_WPA_AES_ENTERPRISE ) ? "WPA AES Enterprise" :
								 ( record->security == RTW_SECURITY_WPA_MIXED_ENTERPRISE ) ? "WPA Mixed Enterprise" :
								 ( record->security == RTW_SECURITY_WPA2_TKIP_ENTERPRISE ) ? "WPA2 TKIP Enterprise" :
								 ( record->security == RTW_SECURITY_WPA2_AES_ENTERPRISE ) ? "WPA2 AES Enterprise" :
								 ( record->security == RTW_SECURITY_WPA2_MIXED_ENTERPRISE ) ? "WPA2 Mixed Enterprise" :
								 ( record->security == RTW_SECURITY_WPA_WPA2_TKIP_ENTERPRISE ) ? "WPA/WPA2 TKIP Enterprise" :
								 ( record->security == RTW_SECURITY_WPA_WPA2_AES_ENTERPRISE ) ? "WPA/WPA2 AES Enterprise" :
								 ( record->security == RTW_SECURITY_WPA_WPA2_MIXED_ENTERPRISE ) ? "WPA/WPA2 Mixed Enterprise" :
#ifdef CONFIG_SAE_SUPPORT
								 ( record->security == RTW_SECURITY_WPA3_AES_PSK) ? "WPA3-SAE AES" :
								 ( record->security == RTW_SECURITY_WPA2_WPA3_MIXED) ? "WPA2/WPA3-SAE AES" :
#endif
								 "Unknown");

	RTW_API_INFO(" %s ", record->SSID.val);
	RTW_API_INFO("\r\n");
#endif
}

static rtw_result_t ipcam_sdio_app_scan_result_handler( rtw_scan_handler_result_t* malloced_scan_result )
{
	static int ApNum = 0;

	if (malloced_scan_result->scan_complete != RTW_TRUE) {
		rtw_scan_result_t* record = &malloced_scan_result->ap_details;
		record->SSID.val[record->SSID.len] = 0; /* Ensure the SSID is null terminated */

#if (defined(CONFIG_EXAMPLE_UART_ATCMD) && CONFIG_EXAMPLE_UART_ATCMD) || (defined(CONFIG_EXAMPLE_SPI_ATCMD) && CONFIG_EXAMPLE_SPI_ATCMD)
		at_printf("\r\nAP : %d,", ++ApNum);
#else
		RTW_API_INFO("%d\t ", ++ApNum);
#endif
		ipcam_sdio_print_scan_result(record);
	} else{
#if (defined(CONFIG_EXAMPLE_UART_ATCMD) && CONFIG_EXAMPLE_UART_ATCMD) || (defined(CONFIG_EXAMPLE_SPI_ATCMD) && CONFIG_EXAMPLE_SPI_ATCMD)
		at_printf("\r\n[ASWS] OK");
		at_printf(STR_END_OF_ATCMD_RET);
#endif
		ApNum = 0;


		struct spdio_buf_t *tx_buf;
		u8 *bufbody = NULL;
		u32 pktsize = 11;

		tx_buf = sdio_buf_alloc(pktsize);
		if (!tx_buf)
		{
			printf("%s(%d) malloc fail\n", __FUNCTION__, __LINE__);
			return;
		}

		rtw_memset((void *)tx_buf->buf_addr, 0, pktsize);
		
		bufbody = (u8 *)tx_buf->buf_addr;
		bufbody += 2;
		rtw_memcpy(bufbody, _AT_WLAN_SCAN_, 4);

		bufbody += 6;
		*bufbody = RTW_SUCCESS;

		tx_buf->buf_size = pktsize;
		tx_buf->type = SPDIO_TX_DATA_ATCMDRSP;

		if(spdio_tx(&ipcam_sdio_dev, tx_buf) != _TRUE) {
			sdio_buf_free(tx_buf);
		}
	}
	return RTW_SUCCESS;
}

void fASWD(void *arg){
	/* To avoid gcc warnings */
	( void ) arg;

	u32 pktsize = 11;
	struct spdio_buf_t *tx_buf;

	int timeout = 20;
	char essid[33];
	volatile int ret = RTW_SUCCESS;

	printf("[ASWD]: _AT_WLAN_DISC_NET_\n\r");
	printf("\n\rDeassociating AP ...");

	if(wext_get_ssid(WLAN0_NAME, (unsigned char *) essid) < 0) {
		printf("\n\rnot connected yet");
		goto exit_success;
	}
	if((ret = wifi_disconnect()) < 0) {
		printf("\n\rERROR: Operation failed!");
		goto exit;
	}

	while(1) {
		if(wext_get_ssid(WLAN0_NAME, (unsigned char *) essid) < 0) {
			printf("\n\rWIFI disconnect succeed");
			break;
		}

		if(timeout == 0) {
			printf("\n\rERROR: Deassoc timeout!");
			ret = RTW_TIMEOUT;
			break;
		}

		vTaskDelay(1 * configTICK_RATE_HZ);
		timeout --;
	}
	printf("\n\r");

#if CONFIG_LWIP_LAYER
	LwIP_ReleaseIP(WLAN0_IDX);
#endif
exit:

	
	if(ret!= RTW_SUCCESS)
	{
		signed char *bufbody = NULL;

		tx_buf = sdio_buf_alloc(pktsize);
		if (!tx_buf)
		{
			printf("%s(%d) malloc fail\n", __FUNCTION__, __LINE__);
			return;
		}

		rtw_memset((void *)tx_buf->buf_addr, 0, pktsize);
		
		bufbody = (signed char *)tx_buf->buf_addr;
		bufbody += 2;
		rtw_memcpy(bufbody, _AT_WLAN_DISC_NET_, 4);

		bufbody += 6;
		*bufbody = ret;
	}
	else
	{
		u8 *bufbody = NULL;

		tx_buf = sdio_buf_alloc(pktsize);
		if (!tx_buf)
		{
			printf("%s(%d) malloc fail\n", __FUNCTION__, __LINE__);
			return;
		}

		rtw_memset((void *)tx_buf->buf_addr, 0, pktsize);
		
		bufbody = (u8 *)tx_buf->buf_addr;
		bufbody += 2;
		rtw_memcpy(bufbody, _AT_WLAN_DISC_NET_, 4);

		bufbody += 6;
		*bufbody = RTW_SUCCESS;
	}

	tx_buf->buf_size = pktsize;
	tx_buf->type = SPDIO_TX_DATA_ATCMDRSP;

	if(spdio_tx(&ipcam_sdio_dev, tx_buf) != _TRUE) {
		sdio_buf_free(tx_buf);
	}

  ipcam_sdio_init_wifi_struct( );
	return;
exit_success:
	{
		u8 *bufbody = NULL;

		tx_buf = sdio_buf_alloc(pktsize);
		if (!tx_buf)
		{
			printf("%s(%d) malloc fail\n", __FUNCTION__, __LINE__);
			return;
		}

		rtw_memset((void *)tx_buf->buf_addr, 0, pktsize);
		
		bufbody = (u8 *)tx_buf->buf_addr;
		bufbody += 2;
		rtw_memcpy(bufbody, _AT_WLAN_DISC_NET_, 4);

		bufbody += 6;
		*bufbody = RTW_SUCCESS;
	}

	tx_buf->buf_size = pktsize;
	tx_buf->type = SPDIO_TX_DATA_ATCMDRSP;

	if(spdio_tx(&ipcam_sdio_dev, tx_buf) != _TRUE) {
		sdio_buf_free(tx_buf);
	}

	ipcam_sdio_init_wifi_struct( );
	return;
}

void fASWS(void *arg){
  char buf[32] = {0};
  u8 *channel_list = NULL;
  u8 *pscan_config = NULL;
  int num_channel = 0;
  int i, argc = 0;
  char *argv[MAX_ARGC] = {0};
	volatile int ret = RTW_SUCCESS;

  printf("[ASWS]: _AT_WLAN_SCAN_\n\r");
  if(arg){
    strncpy(buf, arg, sizeof(buf));
    argc = parse_param(buf, argv);
    if(argc < 2){
			ret = RTW_BADARG;
	    goto exit;
	  }
	  num_channel = atoi(argv[1]);
	  channel_list = (u8*)malloc(num_channel);
	  if(!channel_list){
	    printf("[ASWS]ERROR: Can't malloc memory for channel list\n\r");
	    ret = RTW_BUFFER_UNAVAILABLE_TEMPORARY;

	    goto exit;
	  }

		pscan_config = (u8*)malloc(num_channel);
	  if(!pscan_config){
			printf("[ASWS]ERROR: Can't malloc memory for pscan_config\n\r");
			ret = RTW_BUFFER_UNAVAILABLE_TEMPORARY;

			goto exit;
		}
    //parse command channel list
    for(i = 2; i <= argc -1 ; i++){
      *(channel_list + i - 2) = (u8)atoi(argv[i]);
      *(pscan_config + i - 2) = PSCAN_ENABLE;
    }

    if((ret = wifi_set_pscan_chan(channel_list, pscan_config, num_channel)) < 0){
        printf("[ASWS]ERROR: wifi set partial scan channel fail\n\r");

        goto exit;
    }
  }

	if((ret = wifi_scan_networks(ipcam_sdio_app_scan_result_handler, NULL )) != RTW_SUCCESS){
		printf("[ASWS]ERROR: wifi scan failed\n\r");

		goto exit;
	}

exit:
	if(arg && channel_list)
		free(channel_list);
	if(arg && pscan_config)
		free(pscan_config);
}

void fASWx(void *arg){
	/* To avoid gcc warnings */
	( void ) arg;

	u32 pktsize;
	struct spdio_buf_t *tx_buf;
	u32 wifinum = 0, clientnum = 0;
	rtw_mac_t * clientprt = NULL;

	int i = 0;
#if CONFIG_LWIP_LAYER
	u8 *mac = LwIP_GetMAC(&xnetif[0]);
	u8 *ip = LwIP_GetIP(&xnetif[0]);
	u8 *gw = LwIP_GetGW(&xnetif[0]);
	u8 *msk = LwIP_GetMASK(&xnetif[0]);
#endif
	u8 *ifname[2] = {(u8*)WLAN0_NAME,(u8*)WLAN1_NAME};
	rtw_wifi_setting_t setting;
	s8 statusCheck = 0;

	printf("[ASW?]: _AT_WLAN_INFO_\n\r");

	for(i=0;i<NET_IF_NUM;i++){
		if(rltk_wlan_running(i)){
			wifinum++;

#if CONFIG_LWIP_LAYER
			mac = LwIP_GetMAC(&xnetif[i]);
			ip = LwIP_GetIP(&xnetif[i]);
			gw = LwIP_GetGW(&xnetif[i]);
			msk = LwIP_GetMASK(&xnetif[i]);
#endif
			printf("\n\r\nWIFI %s Status: Running",  ifname[i]);
			printf("\n\r==============================");

			rltk_wlan_statistic(i);

			statusCheck = wifi_get_setting((const char*)ifname[i],&setting);
			if(statusCheck != -1) {
				setting.security_type = rltk_get_security_mode_full((const char*)ifname[i]);
			}
			wifi_show_setting((const char*)ifname[i],&setting);

#if CONFIG_LWIP_LAYER
#if (defined(CONFIG_EXAMPLE_UART_ATCMD) && CONFIG_EXAMPLE_UART_ATCMD) || (defined(CONFIG_EXAMPLE_SPI_ATCMD) && CONFIG_EXAMPLE_SPI_ATCMD)
			at_printf("%02x:%02x:%02x:%02x:%02x:%02x,", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]) ;
			at_printf("%d.%d.%d.%d,", ip[0], ip[1], ip[2], ip[3]);
			at_printf("%d.%d.%d.%d", gw[0], gw[1], gw[2], gw[3]);
#endif
			printf("\n\rInterface (%s)", ifname[i]);
			printf("\n\r==============================");
			printf("\n\r\tMAC => %02x:%02x:%02x:%02x:%02x:%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]) ;
			printf("\n\r\tIP  => %d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
			printf("\n\r\tGW  => %d.%d.%d.%d", gw[0], gw[1], gw[2], gw[3]);
			printf("\n\r\tmsk  => %d.%d.%d.%d\n\r", msk[0], msk[1], msk[2], msk[3]);
#endif
			if(setting.mode == RTW_MODE_AP || i == 1)
			{
				int client_number;
				struct {
					int    count;
					rtw_mac_t mac_list[AP_STA_NUM];
				} client_info;

				client_info.count = AP_STA_NUM;
				wifi_get_associated_client_list(&client_info, sizeof(client_info));

				printf("\n\rAssociated Client List:");
				printf("\n\r==============================");

				clientnum = client_info.count;

				if(client_info.count == 0)
					printf("\n\rClient Num: %d\n\r", client_info.count);
				else
				{
					clientprt = (rtw_mac_t*)rtw_zmalloc(clientnum);
					if(clientprt == NULL)
					{
						printf("%s(%d) malloc fail\n", __FUNCTION__, __LINE__);
						return;
					}

					printf("\n\rClient Num: %d", client_info.count);
					for( client_number=0; client_number < client_info.count; client_number++ )
					{
						printf("\n\rClient %d:", client_number + 1);
						printf("\n\r\tMAC => "MAC_FMT"",
										MAC_ARG(client_info.mac_list[client_number].octet));
#if (defined(CONFIG_EXAMPLE_UART_ATCMD) && CONFIG_EXAMPLE_UART_ATCMD) || (defined(CONFIG_EXAMPLE_SPI_ATCMD) && CONFIG_EXAMPLE_SPI_ATCMD)
						at_printf("\r\nCLIENT : %d,"MAC_FMT"", client_number + 1, MAC_ARG(client_info.mac_list[client_number].octet));
#endif

						rtw_memcpy((clientprt + client_number)->octet, client_info.mac_list[client_number].octet, sizeof(clientprt->octet));
					}
					printf("\n\r");
				}
			}
		}
	}



	pktsize = 11 + sizeof(rtw_wifi_setting_t) * wifinum + sizeof(rtw_mac_t) * clientnum;
	printf("%s(%d) wifinum=%d,clientnum=%d,pktsize=%d\n", __FUNCTION__, __LINE__, wifinum, clientnum, pktsize);
	printf("%s(%d) sizeof(rtw_wifi_setting_t)=%d,sizeof(rtw_mac_t)=%d\n", __FUNCTION__, __LINE__, sizeof(rtw_wifi_setting_t), sizeof(rtw_mac_t));
	u8 *bufbody = NULL;
	rtw_wifi_setting_t wifimsg = {0};

	tx_buf = sdio_buf_alloc(pktsize);
	if (!tx_buf)
	{
		printf("%s(%d) malloc fail\n", __FUNCTION__, __LINE__);
		if(clientprt)
			free(clientprt);

		return;
	}

	rtw_memset((void *)tx_buf->buf_addr, 0, pktsize);
	
	bufbody = (u8 *)tx_buf->buf_addr;
	bufbody += 2;
	rtw_memcpy(bufbody, _AT_WLAN_INFO_, 4);

	bufbody += 6;
	*bufbody = RTW_SUCCESS;

	bufbody += 3;

	for(int idx=0; idx<wifinum; ++idx)
	{
		rtw_memset(&wifimsg, 0, sizeof(rtw_wifi_setting_t));

		if(wifi_get_setting((const char*)ifname[idx], &wifimsg) != -1) {
			wifimsg.security_type = rltk_get_security_mode_full((const char*)ifname[idx]);
		}
		rtw_memcpy((rtw_wifi_setting_t *)bufbody, &wifimsg, sizeof(rtw_wifi_setting_t));

		bufbody += sizeof(rtw_wifi_setting_t);
	}

	if(clientnum != 0)
		rtw_memcpy((rtw_mac_t*)bufbody, clientprt, sizeof(rtw_mac_t) * clientnum);

	if(clientprt)
		free(clientprt);

	tx_buf->buf_size = pktsize;
	tx_buf->type = SPDIO_TX_DATA_ATCMDRSP;

	if(spdio_tx(&ipcam_sdio_dev, tx_buf) != _TRUE) {
		sdio_buf_free(tx_buf);
	}


#if defined(configUSE_TRACE_FACILITY) && (configUSE_TRACE_FACILITY == 1) && (configUSE_STATS_FORMATTING_FUNCTIONS == 1)
	{
		signed char pcWriteBuffer[1024];
		vTaskList((char*)pcWriteBuffer);
		printf("\n\rTask List: \n\r%s", pcWriteBuffer);
	}
#endif

#if (defined(CONFIG_EXAMPLE_UART_ATCMD) && CONFIG_EXAMPLE_UART_ATCMD) || (defined(CONFIG_EXAMPLE_SPI_ATCMD) && CONFIG_EXAMPLE_SPI_ATCMD)
	at_printf("\r\n[ASW?] OK");
#endif

}

#if ATCMD_VER == ATVER_1
void fASW0(void *arg){
	volatile int ret = RTW_SUCCESS;
	(void) ret;
  if(!arg){
    printf("[ASW0]Usage: ASW0=SSID\n\r");
		ret = RTW_BADARG;
		goto exit;
  }
	printf("[ASW0]: _AT_WLAN_SET_SSID_ [%s]\n\r", (char*)arg);
	strncpy((char *)ipcam_sdio_wifi.ssid.val, (char*)arg, sizeof(ipcam_sdio_wifi.ssid.val));
	ipcam_sdio_wifi.ssid.len = strlen((char*)arg);
exit:
	return;
}

void fASW1(void *arg){
	volatile int ret = RTW_SUCCESS;
	(void) ret;
	if(!arg){
		printf("[ASW1]Usage: ASW1=PASSPHRASE\n\r");
		ret = RTW_BADARG;
		goto exit;
	}	
	printf("[ASW1]: _AT_WLAN_SET_PASSPHRASE_ [%s]\n\r", (char*)arg); 

	strncpy((char *)ipcam_sdio_password, (char*)arg, sizeof(ipcam_sdio_password));	
	ipcam_sdio_wifi.password = ipcam_sdio_password;
	ipcam_sdio_wifi.password_len = strlen((char*)arg);
exit:
	return;	
}

void fASW3(void *arg){
	volatile int ret = RTW_SUCCESS;
	(void) ret;
  if(!arg){
    printf("[ASW3]Usage: ASW3=SSID\n\r");
		ret = RTW_BADARG;
		goto exit;
  }

	ipcam_sdio_ap.ssid.len = strlen((char*)arg);

	if(ipcam_sdio_ap.ssid.len > 32){
	  printf("[ASW3]Error: SSID length can't exceed 32\n\r");
		ret = RTW_BADARG;
		goto exit;
  }
	strncpy((char *)ipcam_sdio_ap.ssid.val, (char*)arg, sizeof(ipcam_sdio_ap.ssid.val));

	printf("[ASW3]: _AT_WLAN_AP_SET_SSID_ [%s]\n\r", ipcam_sdio_ap.ssid.val); 
exit:
	return;
}

void fASW4(void *arg){
  volatile int ret = RTW_SUCCESS;
	(void) ret;
  if(!arg){
    printf("[ASW4]Usage: ASW4=PASSWORD\n\r");
    ret = RTW_BADARG;
    goto exit;
  }

  strncpy((char *)ipcam_sdio_password, (char*)arg, sizeof(ipcam_sdio_password));
  ipcam_sdio_ap.password = ipcam_sdio_password;
  ipcam_sdio_ap.password_len = strlen((char*)arg);
  printf("[ASW4]: _AT_WLAN_AP_SET_SEC_KEY_ [%s]\n\r", ipcam_sdio_ap.password);

exit:
  return;
}

void fASW5(void *arg){
  volatile int ret = RTW_SUCCESS;
	(void) ret;
  if(!arg){
   printf("[ASW5]Usage: ASW5=CHANNEL\n\r");
   ret = RTW_BADARG;
   goto exit;
  }

  ipcam_sdio_ap.channel = (unsigned char) atoi((const char *)arg);
  printf("[ASW5]: _AT_WLAN_AP_SET_CHANNEL_ [channel %d]\n\r", ipcam_sdio_ap.channel);
exit:
  return;
}

void fASW6(void *arg){
  /* To avoid gcc warnings */
	( void ) arg;

	u32 pktsize = 11 + 4;
	u8 tempip[4] = {0};
	struct spdio_buf_t *tx_buf;
	u8 *bufbody = NULL;

	printf("[ASW6]: _AT_WLAN_GET_IP_\n\r");

	u8 *ip = LwIP_GetIP(&xnetif[0]);

	printf("[ASW6]: IP:%d.%d.%d.%d\n", ip[0], ip[1], ip[2], ip[3]);

	tx_buf = sdio_buf_alloc(pktsize);
	if (!tx_buf)
	{
		printf("%s(%d) malloc fail\n", __FUNCTION__, __LINE__);
		return;
	}

	rtw_memset((void *)tx_buf->buf_addr, 0, pktsize);
	
	bufbody = (u8 *)tx_buf->buf_addr;
	bufbody += 2;
	rtw_memcpy(bufbody, _AT_WLAN_GET_IP_, 4);

	bufbody += 6;
	*bufbody = RTW_SUCCESS;

	bufbody += 3;
	tempip[0] = ip[3];
	tempip[1] = ip[2];
	tempip[2] = ip[1];
	tempip[3] = ip[0];
	rtw_memcpy(bufbody, tempip, 4);

	tx_buf->buf_size = pktsize;
	tx_buf->type = SPDIO_TX_DATA_ATCMDRSP;

	if(spdio_tx(&ipcam_sdio_dev, tx_buf) != _TRUE) {
		sdio_buf_free(tx_buf);
	}

  return;
}

void fASWA(void *arg){
	/* To avoid gcc warnings */
	( void ) arg;
	u32 pktsize;
	struct spdio_buf_t *tx_buf;

#if CONFIG_LWIP_LAYER
	struct ip_addr ipaddr;
	struct ip_addr netmask;
	struct ip_addr gw;
	struct netif * pnetif = &xnetif[0];
#endif
	int timeout = 20;
	volatile int ret = RTW_SUCCESS;
	printf("[ASWA]: _AT_WLAN_AP_ACTIVATE_\n\r"); 
	if(ipcam_sdio_ap.ssid.val[0] == 0){
		printf("[ASWA]Error: SSID can't be empty\n\r");
		ret = RTW_BADARG;
		goto exit;
	}
	if(ipcam_sdio_ap.password == NULL){
		ipcam_sdio_ap.security_type = RTW_SECURITY_OPEN;
	}
	else{
		if(ipcam_sdio_ap.password_len <= RTW_WPA2_MAX_PSK_LEN &&
			ipcam_sdio_ap.password_len >= RTW_MIN_PSK_LEN){ 
			ipcam_sdio_ap.security_type = RTW_SECURITY_WPA2_AES_PSK;
			if(ipcam_sdio_ap.password_len == RTW_WPA2_MAX_PSK_LEN){//password_len=64 means pre-shared key, pre-shared key should be 64 hex characters
				unsigned char i,j;
				for(i = 0;i < RTW_WPA2_MAX_PSK_LEN;i++){
					j = ipcam_sdio_ap.password[i];
					if(!((j >='0' && j<='9') || (j >='A' && j<='F') || (j >='a' && j<='f'))){
						printf("[ASWA]Error: password should be 64 hex characters or 8-63 ASCII characters\n\r");
						ret = RTW_INVALID_KEY;
						goto exit;
					}
				}
			}
		}
		else{
			printf("[ASWA]Error: password should be 64 hex characters or 8-63 ASCII characters\n\r");
			ret = RTW_INVALID_KEY;
			goto exit;
		}
	}	

#if CONFIG_LWIP_LAYER
	dhcps_deinit();
#if LWIP_VERSION_MAJOR >= 2
	IP4_ADDR(ip_2_ip4(&ipaddr), GW_ADDR0, GW_ADDR1, GW_ADDR2, GW_ADDR3);
	IP4_ADDR(ip_2_ip4(&netmask), NETMASK_ADDR0, NETMASK_ADDR1 , NETMASK_ADDR2, NETMASK_ADDR3);
	IP4_ADDR(ip_2_ip4(&gw), GW_ADDR0, GW_ADDR1, GW_ADDR2, GW_ADDR3);
	netif_set_addr(pnetif, ip_2_ip4(&ipaddr), ip_2_ip4(&netmask),ip_2_ip4(&gw));
#else
	IP4_ADDR(&ipaddr, GW_ADDR0, GW_ADDR1, GW_ADDR2, GW_ADDR3);
	IP4_ADDR(&netmask, NETMASK_ADDR0, NETMASK_ADDR1 , NETMASK_ADDR2, NETMASK_ADDR3);
	IP4_ADDR(&gw, GW_ADDR0, GW_ADDR1, GW_ADDR2, GW_ADDR3);
	netif_set_addr(pnetif, &ipaddr, &netmask,&gw);
#endif
#ifdef CONFIG_DONT_CARE_TP
	pnetif->flags |= NETIF_FLAG_IPSWITCH;
#endif
#endif

#if (defined(CONFIG_PLATFORM_8710C)|| defined(CONFIG_PLATFORM_8721D)) && (defined(CONFIG_BT) && CONFIG_BT)
	if (wifi_set_mode(RTW_MODE_AP) < 0){
	    printf("\n\rERROR: Wifi on failed!");
	    ret = RTW_ERROR;
	    goto exit;
	}
#else
	wifi_off();
	vTaskDelay(20);
	if (wifi_on(RTW_MODE_AP) < 0){
		printf("\n\rERROR: Wifi on failed!");
		ret = RTW_ERROR;
		goto exit;
	}
#endif
	printf("\n\rStarting AP ...");

	if((ret = wifi_start_ap((char*)ipcam_sdio_ap.ssid.val, ipcam_sdio_ap.security_type, (char*)ipcam_sdio_ap.password, ipcam_sdio_ap.ssid.len, ipcam_sdio_ap.password_len, ipcam_sdio_ap.channel) )< 0) {
		printf("\n\rERROR: Operation failed!");
		goto exit;
	}

	while(1) {
		char essid[33];

		if(wext_get_ssid(WLAN0_NAME, (unsigned char *) essid) > 0) {
			if(strcmp((const char *) essid, (const char *)ipcam_sdio_ap.ssid.val) == 0) {
				printf("\n\r%s started\n", ipcam_sdio_ap.ssid.val);
				ret = RTW_SUCCESS;
				break;
			}
		}

		if(timeout == 0) {
			printf("\n\rERROR: Start AP timeout!");
			ret = RTW_TIMEOUT;		
			break;
		}

		vTaskDelay(1 * configTICK_RATE_HZ);
		timeout --;
	}

#if CONFIG_LWIP_LAYER
	dhcps_init(pnetif);
#endif

exit:
	if(ret!= RTW_SUCCESS)
	{
		pktsize = 11;
		signed char *bufbody = NULL;

		tx_buf = sdio_buf_alloc(pktsize);
		if (!tx_buf)
		{
			printf("%s(%d) malloc fail\n", __FUNCTION__, __LINE__);
			return;
		}

		rtw_memset((void *)tx_buf->buf_addr, 0, pktsize);
		
		bufbody = (signed char *)tx_buf->buf_addr;
		bufbody += 2;
		rtw_memcpy(bufbody, _AT_WLAN_AP_ACTIVATE_, 4);

		bufbody += 6;
		*bufbody = ret;
	}
	else
	{
		pktsize = 11 + sizeof(rtw_wifi_setting_t);
		rtw_wifi_setting_t wifimsg = {0};
		u8 *bufbody = NULL;

		if(wifi_get_setting(WLAN0_NAME,&wifimsg) != -1) {
			wifimsg.security_type = rltk_get_security_mode_full(WLAN0_NAME);
		}

		tx_buf = sdio_buf_alloc(pktsize);
		if (!tx_buf)
		{
			printf("%s(%d) malloc fail\n", __FUNCTION__, __LINE__);
			return;
		}

		rtw_memset((void *)tx_buf->buf_addr, 0, pktsize);
		
		bufbody = (u8 *)tx_buf->buf_addr;
		bufbody += 2;
		rtw_memcpy(bufbody, _AT_WLAN_AP_ACTIVATE_, 4);

		bufbody += 6;
		*bufbody = RTW_SUCCESS;

		bufbody += 3;
		rtw_memcpy((rtw_wifi_setting_t *)bufbody, &wifimsg, sizeof(rtw_wifi_setting_t));
	}

	tx_buf->buf_size = pktsize;
	tx_buf->type = SPDIO_TX_DATA_ATCMDRSP;

	if(spdio_tx(&ipcam_sdio_dev, tx_buf) != _TRUE) {
		sdio_buf_free(tx_buf);
	}

#if defined(CONFIG_INIC_CMD_RSP) && CONFIG_INIC_CMD_RSP
	inic_c2h_wifi_info("ASWA", ret);
#endif
	ipcam_sdio_init_wifi_struct( );
}

static int _find_ap_from_scan_buf(char*buf, int buflen, char *target_ssid, void *user_data)
{
	rtw_wifi_setting_t *pwifi = (rtw_wifi_setting_t *)user_data;
	int plen = 0;
	
	while(plen < buflen){
		u8 len, ssid_len, security_mode;
		char *ssid;

		// len offset = 0
		len = (int)*(buf + plen);
		// check end
		if(len == 0) break;
		// ssid offset = 14
		ssid_len = len - 14;
		ssid = buf + plen + 14 ;
		if((ssid_len == strlen(target_ssid))
			&& (!memcmp(ssid, target_ssid, ssid_len)))
		{
			strncpy((char*)pwifi->ssid, target_ssid, 33);
			// channel offset = 13
			pwifi->channel = *(buf + plen + 13);
			// security_mode offset = 11
			security_mode = (u8)*(buf + plen + 11);
			if(security_mode == RTW_ENCODE_ALG_NONE)
				pwifi->security_type = RTW_SECURITY_OPEN;
			else if(security_mode == RTW_ENCODE_ALG_WEP)
				pwifi->security_type = RTW_SECURITY_WEP_PSK;
			else if(security_mode == RTW_ENCODE_ALG_CCMP)
				pwifi->security_type = RTW_SECURITY_WPA2_AES_PSK;
			break;
		}
		plen += len;
	}
	return 0;
}

static int _get_ap_security_mode(IN char * ssid, OUT rtw_security_t *security_mode, OUT u8 * channel)
{
	rtw_wifi_setting_t wifi;
	u32 scan_buflen = 1000;

	memset(&wifi, 0, sizeof(wifi));

	if(wifi_scan_networks_with_ssid(_find_ap_from_scan_buf, (void*)&wifi, scan_buflen, ssid, strlen(ssid)) != RTW_SUCCESS){
		printf("Wifi scan failed!\n");
		return 0;
	}

	if(strcmp(wifi.ssid, ssid) == 0){
		*security_mode = wifi.security_type;
		*channel = wifi.channel;
		return 1;
	}
	
	return 0;
}
extern u8 rltk_wlan_channel_switch_announcement_is_enable(void);
void fASWC(void *arg){
	/* To avoid gcc warnings */
	( void ) arg;

	u32 pktsize;
	struct spdio_buf_t *tx_buf;

	int mode, ret;
	unsigned long tick1 = xTaskGetTickCount();
	unsigned long tick2, tick3;
	char empty_bssid[6] = {0}, assoc_by_bssid = 0;

	printf("[ASWC]: _AT_WLAN_JOIN_NET_\n\r");
	if(memcmp (ipcam_sdio_wifi.bssid.octet, empty_bssid, 6))
		assoc_by_bssid = 1;
	else if(ipcam_sdio_wifi.ssid.val[0] == 0){
		printf("[ASWC]Error: SSID can't be empty\n\r");
		ret = RTW_BADARG;
		goto EXIT;
	}
	if(ipcam_sdio_wifi.password != NULL){
		if((ipcam_sdio_wifi.key_id >= 0)&&(ipcam_sdio_wifi.key_id <= 3)) {
			ipcam_sdio_wifi.security_type = RTW_SECURITY_WEP_PSK;
		}
		else{
			ipcam_sdio_wifi.security_type = RTW_SECURITY_WPA2_AES_PSK;
		}
	}
	else{
		ipcam_sdio_wifi.security_type = RTW_SECURITY_OPEN;
	}
	//Check if in AP mode
	wext_get_mode(WLAN0_NAME, &mode);
	if(mode == RTW_MODE_MASTER) {
#if CONFIG_LWIP_LAYER
		dhcps_deinit();
#endif

		wifi_off();
		vTaskDelay(20);
		if (wifi_on(RTW_MODE_STA) < 0){
			printf("\n\rERROR: Wifi on failed!");
            ret = RTW_ERROR;
			goto EXIT;
		}
	}

	if(rltk_wlan_channel_switch_announcement_is_enable()){
		if(ipcam_sdio_wifi_mode == RTW_MODE_STA_AP){
			u8 connect_channel;
			int security_retry_count = 0;
			while (1) {
				if (_get_ap_security_mode((char*)ipcam_sdio_wifi.ssid.val, &ipcam_sdio_wifi.security_type, &connect_channel))
					break;
				security_retry_count++;
				if(security_retry_count >= 3){
					printf("Can't get AP security mode and channel.\n");
					ret = RTW_NOTFOUND;
					goto EXIT;
				}
			}
			//disable wlan1 issue_deauth when channel switched by wlan0
			ret = wifi_set_ch_deauth(0);
			if(ret != 0){
				printf("wifi_set_ch_deauth FAIL\n");
				goto EXIT;
			}
			//softap switch chl and inform
			if(wifi_ap_switch_chl_and_inform(connect_channel)!=RTW_SUCCESS)
				printf("wifi_ap_switch_chl_and_inform FAIL\n");
		}
	}

	if(assoc_by_bssid){
		printf("\n\rJoining BSS by BSSID "MAC_FMT" ...\n\r", MAC_ARG(ipcam_sdio_wifi.bssid.octet));
		ret = wifi_connect_bssid(ipcam_sdio_wifi.bssid.octet, (char*)ipcam_sdio_wifi.ssid.val, ipcam_sdio_wifi.security_type, (char*)ipcam_sdio_wifi.password, 
						ETH_ALEN, ipcam_sdio_wifi.ssid.len, ipcam_sdio_wifi.password_len, ipcam_sdio_wifi.key_id, NULL);		
	} else {
		printf("\n\rJoining BSS by SSID %s...\n\r", (char*)ipcam_sdio_wifi.ssid.val);
		ret = wifi_connect((char*)ipcam_sdio_wifi.ssid.val, ipcam_sdio_wifi.security_type, (char*)ipcam_sdio_wifi.password, ipcam_sdio_wifi.ssid.len,
						ipcam_sdio_wifi.password_len, ipcam_sdio_wifi.key_id, NULL);
	}
	
	if(ret!= RTW_SUCCESS){
		if(ret == RTW_INVALID_KEY)
			printf("\n\rERROR:Invalid Key ");
		
		printf("\n\rERROR: Can't connect to AP, ret=%d\n", ret);
		goto EXIT;
	}

	tick2 = xTaskGetTickCount();
	printf("\r\nConnected after %dms.\n", (tick2-tick1));
#if CONFIG_LWIP_LAYER
		/* Start DHCPClient */
	LwIP_DHCP(0, DHCP_START);
	tick3 = xTaskGetTickCount();
	printf("\r\n\nGot IP after %dms.\n", (tick3-tick1));
#endif

	printf("\n\r");
EXIT:
	if(ret!= RTW_SUCCESS)
	{
		pktsize = 11;
		signed char *bufbody = NULL;

		tx_buf = sdio_buf_alloc(pktsize);
		if (!tx_buf)
		{
			printf("%s(%d) malloc fail\n", __FUNCTION__, __LINE__);
			return;
		}

		rtw_memset((void *)tx_buf->buf_addr, 0, pktsize);
		
		bufbody = (signed char *)tx_buf->buf_addr;
		bufbody += 2;
		rtw_memcpy(bufbody, _AT_WLAN_JOIN_NET_, 4);

		bufbody += 6;
		*bufbody = ret;
	}
	else
	{
		pktsize = 11 + sizeof(rtw_wifi_setting_t) + 4;
		//printf("%s(%d) sizeof(rtw_wifi_setting_t)=%d\n", __FUNCTION__, __LINE__, sizeof(rtw_wifi_setting_t));
		rtw_wifi_setting_t wifimsg = {0};
		u8 *bufbody = NULL;
		u8 tempip[4] = {0};

		if(wifi_get_setting(WLAN0_NAME,&wifimsg) != -1) {
			wifimsg.security_type = rltk_get_security_mode_full(WLAN0_NAME);
		}

		tx_buf = sdio_buf_alloc(pktsize);
		if (!tx_buf)
		{
			printf("%s(%d) malloc fail\n", __FUNCTION__, __LINE__);
			return;
		}

		rtw_memset((void *)tx_buf->buf_addr, 0, pktsize);
		
		bufbody = (u8 *)tx_buf->buf_addr;
		bufbody += 2;
		rtw_memcpy(bufbody, _AT_WLAN_JOIN_NET_, 4);

		bufbody += 6;
		*bufbody = RTW_SUCCESS;

		bufbody += 3;
		rtw_memcpy((rtw_wifi_setting_t *)bufbody, &wifimsg, sizeof(rtw_wifi_setting_t));

		bufbody += sizeof(rtw_wifi_setting_t);
		u8 *ip = LwIP_GetIP(&xnetif[0]);
		tempip[0] = ip[3];
		tempip[1] = ip[2];
		tempip[2] = ip[1];
		tempip[3] = ip[0];
		rtw_memcpy(bufbody, tempip, 4);
	}

	tx_buf->buf_size = pktsize;
	tx_buf->type = SPDIO_TX_DATA_ATCMDRSP;

	if(spdio_tx(&ipcam_sdio_dev, tx_buf) != _TRUE) {
		sdio_buf_free(tx_buf);
	}

	ipcam_sdio_init_wifi_struct( );
}
#endif // end of #if ATCMD_VER == ATVER_1

#endif // end of #if CONFIG_WLAN

log_item_t at_ipcam_sdio_wifi_items[ ] = {
	{"ASW0", fASW0,{NULL,NULL}},
	{"ASW1", fASW1,{NULL,NULL}},
	{"ASW3", fASW3,{NULL,NULL}},
	{"ASW4", fASW4,{NULL,NULL}},
	{"ASW5", fASW5,{NULL,NULL}},	
	{"ASW6", fASW6,{NULL,NULL}},
	{"ASWA", fASWA,{NULL,NULL}},
	{"ASWC", fASWC,{NULL,NULL}},
	{"ASWD", fASWD,{NULL,NULL}},
	{"ASWS", fASWS,{NULL,NULL}},
	{"ASW?", fASWx,{NULL,NULL}},
};

void at_ipcam_sdio_wifi_init(void)
{
#if CONFIG_WLAN
	ipcam_sdio_init_wifi_struct();
#endif
	log_service_add_table(at_ipcam_sdio_wifi_items, sizeof(at_ipcam_sdio_wifi_items)/sizeof(at_ipcam_sdio_wifi_items[0]));
}
