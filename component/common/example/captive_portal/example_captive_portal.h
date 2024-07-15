#ifndef EXAMPLE_CAPTIVE_PORTAL_H
#define EXAMPLE_CAPTIVE_PORTAL_H

/* ------------------------ FreeRTOS includes ----------------------------- */
#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"

/* ------------------------ lwIP includes --------------------------------- */
#include "lwip/api.h"
#include "lwip/ip.h"
#include "lwip/memp.h"
#include <lwip_netconf.h>
#include "lwip/stats.h"
#include "lwip/tcp.h"
#include "lwip/tcpip.h"
#include "netif/loopif.h"

/* ------------------------ Project includes ------------------------------ */
#include "main.h"
#include "osdep_service.h"
#include "wifi_conf.h"
#include "wifi_constants.h"
#include "wifi_structures.h"
#include "wlan_intf.h"

void example_captive_portal(void);

typedef struct {
	char ssid[32 + 1];         /* The SSID of an access point. */
	signed char  ap_power;             /* Received Signal Strength Indication, min: -110, max: 0 */
	char bssid[6];             /* The BSSID of an access point. */
	u8   channel;              /* The RF frequency, 1-13 */
	rtw_security_t sec_type;     /* Security type, @ref wlan_sec_type_t */
} ap_list_t;

/*
 *  Scan result using normal scan.
 */
typedef struct {
	char ap_num; /* The number of access points found in scanning. */
	ap_list_t *ap_list;
} wifi_scan_result_t, scan_ap_list_t;

typedef struct _web_conn {
	struct netconn *conn;
	struct task_struct task;
	int status;						/*!< Client socket descriptor for connection */
} web_conn;

typedef unsigned long rtw_bool_t;

#define SOFTAP_SSID				"captive_portal_test"
#define SOFTAP_PASSWORD			"12345678"				// NULL for RTW_SECURITY_OPEN
#define SOFTAP_CHANNEL			6
#define SOFTAP_SECURITY			RTW_SECURITY_WPA2_AES_PSK
#define SCAN_AP_LIST_MAX		10
#define MAX_HTTP_CONNECTIONS	(DEFAULT_ACCEPTMBOX_SIZE + 1)
#define CAPTIVE_PORTAL_DEBUG	0
#define CONFIG_READ_FLASH		0

#ifndef WLAN0_NAME
#define WLAN0_NAME		"wlan0"
#endif

#ifndef WLAN1_NAME
#define WLAN1_NAME      	"wlan1"
#endif

struct netconn *pxHTTPListener = NULL;
web_conn *web_connections = NULL;

wifi_scan_result_t scan_result = {0};
_sema scan_done_sema;
xTaskHandle webs_task = NULL;
xSemaphoreHandle webs_sema = NULL;
xSemaphoreHandle webs_wpage_sema = NULL;
u8_t webs_terminate = 0;

u8 connect_target_ap = 0;
u8 scan_ap = 0;

extern rtw_mode_t wifi_mode;
extern struct netif xnetif[NET_IF_NUM]; /* network interface structure */

#endif /* EXAMPLE_CAPTIVE_PORTAL_H */
