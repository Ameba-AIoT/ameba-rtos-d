#ifndef _WRAPPERS_DEFS_H_
#define _WRAPPERS_DEFS_H_

#ifdef __cplusplus
extern "C"
{
#endif

#define STD_PRINTF

#include <stdio.h>
#include <string.h>
#include "platform_stdlib.h"
#include "flash_api.h"
#include "gpio_irq_api.h"
#include <device_lock.h>
#include "osdep_service.h"
#include "FreeRTOS.h"
#include "task.h"
#include "platform_opts.h"
#include "crypto_api.h"

#if CONFIG_USE_MBEDTLS 
#include "mbedtls/aes.h"
#include "mbedtls/config.h"
#include "mbedtls/platform.h"
#include "mbedtls/net_sockets.h"
#include "mbedtls/ssl.h"
#include "mbedtls/error.h"
#include "mbedtls/debug.h"
#endif/* CONFIG_USE_MBEDTLS */

#include "lwip/netdb.h"
#include "lwip_netconf.h"
#include "wifi_conf.h"
#include "lwip/inet.h"
#include <lwip/sockets.h>
#include <wlan_fast_connect/example_wlan_fast_connect.h>
//#include "infra_compat.h"
#include "iot_import.h"
#include "iot_export.h"
#include "rtl8721d_ota.h"
#include "dct.h"


/****************************************************************************/

extern int ota_file_size;
/****************************************************************************/

#define CONFIG_FW_INFO          (0x100200 - 0x0100) // 4KB flash space for firmware version
#define CONFIG_DEVICE_INFO		(0x100200 - 0x0200)	// 4KB flash space for device key&&secret

#define USE_DEMO_6 0

#define PLATFORM_WAIT_INFINITE (~0)
#define MID_STRLEN_MAX         (64)
/*******************************************************************************/
#define KV_BEGIN_ADDR           0x101000    /*!< DCT begin address of flash, ex: 0x100000 = 1M */
#define KV_MODULE_NUM               1           /*!< max number of module */
#define KV_VARIABLE_NAME_SIZE       64         /*!< max size of the variable name */
#define KV_VARIABLE_VALUE_SIZE_L     4           /*!< size of the variable value length */
#define KV_VARIABLE_VALUE_SIZE_V     64           /*!< size of the variable value length */
#define KV_VARIABLE_VALUE_SIZE      (KV_VARIABLE_VALUE_SIZE_L + KV_VARIABLE_VALUE_SIZE_V)
#define KV_MODULE_NAME          "aliyun_kv_module"

extern dct_handle_t aliyun_kv_handle;
extern uint16_t        len_variable;

/****************************************************************************/
#if CONFIG_USE_MBEDTLS

static unsigned int mbedtls_mem_used = 0;
static unsigned int mbedtls_max_mem_used = 0;

#define MBEDTLS_MEM_INFO_MAGIC   0x12345678
#define TIMER_PERIOD 10000/portTICK_PERIOD_MS  

typedef struct _TLSDataParams {
    mbedtls_ssl_context ssl;          /**< mbed TLS control context. */
    mbedtls_net_context fd;           /**< mbed TLS network context. */
    mbedtls_ssl_config conf;          /**< mbed TLS configuration context. */
    mbedtls_x509_crt cacertl;         /**< mbed TLS CA certification. */
    mbedtls_x509_crt clicert;         /**< mbed TLS Client certification. */
    mbedtls_pk_context pkey;          /**< mbed TLS Client key. */
} TLSDataParams_t, *TLSDataParams_pt;

typedef struct {
    int magic;
    int size;
} mbedtls_mem_info_t;

#define IV_BUF_LEN 16
typedef struct _aes_content_t
{
	mbedtls_aes_context *ctx;
	uint8_t iv_buf[IV_BUF_LEN];
	uint8_t key[IV_BUF_LEN];
}aes_content_t;

typedef struct _mbedtls_aes_content_t
{
	unsigned char iv_buf[IV_BUF_LEN];
	void *ctx;

}mbedtls_aes_content_t;

#endif

typedef void DTLSContext;


 /* @brief this is a network address structure, including host(ip or host name) and port.*/
typedef struct
{
	char *host; /**< host ip(dotted-decimal notation) or host name(string) */
	uint16_t port; /**< udp port or tcp port */
} netaddr_t, *pnetaddr_t;

/****************************************************************************/

typedef void* pthread_mutex_t; 


#define HAL_SOCKET_MAXNUMS			(8)//(10)
#define HAL_INVALID_FD             ((void *)-1)
#define LINK_OK 		(0)
#define LINK_ERR 		(-1)

#define SOCKET_TCPSOCKET_BUFFERSIZE    (1*1536)

/****************************************************************************/

/**
 * @brief management frame handler
 *
 * @param[in] buffer @n 80211 raw frame or ie(information element) buffer
 * @param[in] len @n buffer length
 * @param[in] rssi_dbm @n rssi in dbm, set it to 0 if not supported
 * @param[in] buffer_type @n 0 when buffer is a 80211 frame,
 *                          1 when buffer only contain IE info
 * @return None.
 * @see None.
 * @note None.
 */
typedef void (*awss_wifi_mgnt_frame_cb_t)(_IN_ uint8_t *buffer,
        _IN_ int len, _IN_ char rssi_dbm, _IN_ int buffer_type);


typedef struct {
    char wifi_mode;              /* DHCP mode: @ref wlanInterfaceTypedef. */
    char wifi_ssid[32 + 1];      /* SSID of the wlan needs to be connected. */
    char wifi_key[64 + 1];       /* Security key of the wlan needs to be connected, ignored in an open system. */
    char local_ip_addr[16];      /* Static IP configuration, Local IP address. */
    char net_mask[16];           /* Static IP configuration, Netmask. */
    char gateway_ip_addr[16];    /* Static IP configuration, Router IP address. */
    char dns_server_ip_addr[16]; /* Static IP configuration, DNS server IP address. */
    char dhcp_mode;              /* DHCP mode, @ref DHCP_Disable, @ref DHCP_Client and @ref DHCP_Server. */
    char reserved[32];
    int  wifi_retry_interval;    /* Retry interval if an error is occured when connecting an access point,
                                    time unit is millisecond. */
} hal_wifi_init_type_t;

typedef struct {
    uint8_t dhcp;     /* DHCP mode: @ref DHCP_Disable, @ref DHCP_Client, @ref DHCP_Server. */
    char    ip[16];   /* Local IP address on the target wlan interface: @ref wlanInterfaceTypedef. */
    char    gate[16]; /* Router IP address on the target wlan interface: @ref wlanInterfaceTypedef. */
    char    mask[16]; /* Netmask on the target wlan interface: @ref wlanInterfaceTypedef. */
    char    dns[16];  /* DNS server IP address. */
    char    mac[16];  /* MAC address, example: "C89346112233". */
    char    broadcastip[16];
} hal_wifi_ip_stat_t;

typedef struct {
    int     is_connected;  /* The link to wlan is established or not, 0: disconnected, 1: connected. */
    int     wifi_strength; /* Signal strength of the current connected AP */
    uint8_t ssid[32 + 1];  /* SSID of the current connected wlan */
    uint8_t bssid[6];      /* BSSID of the current connected wlan */
    int     channel;       /* Channel of the current connected wlan */
} hal_wifi_link_stat_t;

#ifndef BIT
#define BIT(n)                   (1<<n)
#endif

enum WIFI_FRAME_TYPE {
	WIFI_MGT_TYPE  =	(0),
	WIFI_CTRL_TYPE =	(BIT(2)),
	WIFI_DATA_TYPE =	(BIT(3)),
	WIFI_QOS_DATA_TYPE	= (BIT(7)|BIT(3)),	//!< QoS Data	
};

enum WIFI_FRAME_SUBTYPE {

    // below is for mgt frame
    WIFI_ASSOCREQ       = (0 | WIFI_MGT_TYPE),
    WIFI_ASSOCRSP       = (BIT(4) | WIFI_MGT_TYPE),
    WIFI_REASSOCREQ     = (BIT(5) | WIFI_MGT_TYPE),
    WIFI_REASSOCRSP     = (BIT(5) | BIT(4) | WIFI_MGT_TYPE),
    WIFI_PROBEREQ       = (BIT(6) | WIFI_MGT_TYPE),
    WIFI_PROBERSP       = (BIT(6) | BIT(4) | WIFI_MGT_TYPE),
    WIFI_BEACON         = (BIT(7) | WIFI_MGT_TYPE),
    WIFI_ATIM           = (BIT(7) | BIT(4) | WIFI_MGT_TYPE),
    WIFI_DISASSOC       = (BIT(7) | BIT(5) | WIFI_MGT_TYPE),
    WIFI_AUTH           = (BIT(7) | BIT(5) | BIT(4) | WIFI_MGT_TYPE),
    WIFI_DEAUTH         = (BIT(7) | BIT(6) | WIFI_MGT_TYPE),
    WIFI_ACTION         = (BIT(7) | BIT(6) | BIT(4) | WIFI_MGT_TYPE),

    // below is for control frame
    WIFI_PSPOLL         = (BIT(7) | BIT(5) | WIFI_CTRL_TYPE),
    WIFI_RTS            = (BIT(7) | BIT(5) | BIT(4) | WIFI_CTRL_TYPE),
    WIFI_CTS            = (BIT(7) | BIT(6) | WIFI_CTRL_TYPE),
    WIFI_ACK            = (BIT(7) | BIT(6) | BIT(4) | WIFI_CTRL_TYPE),
    WIFI_CFEND          = (BIT(7) | BIT(6) | BIT(5) | WIFI_CTRL_TYPE),
    WIFI_CFEND_CFACK    = (BIT(7) | BIT(6) | BIT(5) | BIT(4) | WIFI_CTRL_TYPE),

    // below is for data frame
    WIFI_DATA           = (0 | WIFI_DATA_TYPE),
    WIFI_DATA_CFACK     = (BIT(4) | WIFI_DATA_TYPE),
    WIFI_DATA_CFPOLL    = (BIT(5) | WIFI_DATA_TYPE),
    WIFI_DATA_CFACKPOLL = (BIT(5) | BIT(4) | WIFI_DATA_TYPE),
    WIFI_DATA_NULL      = (BIT(6) | WIFI_DATA_TYPE),
    WIFI_CF_ACK         = (BIT(6) | BIT(4) | WIFI_DATA_TYPE),
    WIFI_CF_POLL        = (BIT(6) | BIT(5) | WIFI_DATA_TYPE),
    WIFI_CF_ACKPOLL     = (BIT(6) | BIT(5) | BIT(4) | WIFI_DATA_TYPE),
    WIFI_QOS_DATA_NULL	= (BIT(6) | WIFI_QOS_DATA_TYPE),
};


/*NETMASK*/
#ifndef RTL_NETMASK_ADDR0
#define RTL_NETMASK_ADDR0   255
#define RTL_NETMASK_ADDR1   255
#define RTL_NETMASK_ADDR2   255
#define RTL_NETMASK_ADDR3   0
#endif

/*Gateway Address*/
#ifndef RTL_GW_ADDR0
#define RTL_GW_ADDR0   192
#define RTL_GW_ADDR1   168
#define RTL_GW_ADDR2   1
#define RTL_GW_ADDR3   1
#endif

/* DHCP Assigned Starting Address*/
#ifndef RTL_DHCP_START_ADDR0  
#define RTL_DHCP_START_ADDR0   192
#define RTL_DHCP_START_ADDR1   168
#define RTL_DHCP_START_ADDR2   1
#define RTL_DHCP_START_ADDR3   100  
#endif

/* DHCP Assigned Ending Address*/
#ifndef RTL_DHCP_END_ADDR0   
#define RTL_DHCP_END_ADDR0   192
#define RTL_DHCP_END_ADDR1   168
#define RTL_DHCP_END_ADDR2   1
#define RTL_DHCP_END_ADDR3   255 
#endif

/**
 *  @brief  wlan network interface enumeration definition.
 */
typedef enum
{
    SOFT_AP,  /**< Act as an access point, and other station can connect, 4 stations Max*/
    STATION   /**< Act as a station which can connect to an access point*/
} hal_wifi_type_t;

enum {
    DHCP_DISABLE = 0,
    DHCP_CLIENT,
    DHCP_SERVER,
};
/****************************************************************************/

int  HAL_write_device_info(int argc, unsigned char **argv);
int HAL_get_free_heap_size();

#define LINK_ERROR	    0
#define LINK_WARNING	1
#define LINK_INFO	    2
#define LINK_DEBUG	    3
#define LINK_NONE	    0xFF
#define LINK_DEBUG_LEVEL	LINK_INFO

#if (LINK_DEBUG_LEVEL == LINK_NONE)
#define link_printf(level, fmt, arg...)
#else
#define link_printf(level, fmt, arg...)     \
do {\
	if (level <= LINK_DEBUG_LEVEL) {\
		if (level <= LINK_ERROR) {\
			printf("\r\nERROR: " fmt, ##arg);\
		} \
		else {\
			printf("\r\n"fmt, ##arg);\
		} \
	}\
}while(0)
#endif

#undef link_err_printf
#define link_err_printf(fmt, args...) \
	     	printf("%s(): " fmt "\n", __FUNCTION__, ##args); 


#define test_printf printf
#define u_print_data(d, l) \
do{\
	int i;\
	test_printf("\n Len=%d\n",  (l));\
	for(i = 0; i < (l); i++)\
		test_printf("%02x ", (d)[i]);\
	test_printf("\n");\
}while(0);


#endif

