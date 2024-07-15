/*
******************************
 */

#include <stdlib.h>
#include "wrappers_defs.h"
#include "cJSON.h"

#define USE_V130
#ifdef USE_V130
#include "iot_export_linkkit.h"
#include "iot_export_compat.h"
//#include "ota_service.h"
#else
#include "dev_model_api.h"
#include "ota_api.h"
#endif

void HAL_Printf(const char *fmt, ...);
int HAL_Snprintf(char *str, const int len, const char *fmt, ...);
uint64_t HAL_UptimeMs(void);
void HAL_ThreadDelete(void *thread_handle);
int HAL_get_free_heap_size();
int HAL_GetProductKey(char product_key[IOTX_PRODUCT_KEY_LEN + 1]);
int HAL_GetProductSecret(char product_secret[IOTX_PRODUCT_SECRET_LEN + 1]);
int HAL_GetDeviceName(char device_name[IOTX_DEVICE_NAME_LEN + 1]);
int HAL_GetDeviceSecret(char device_secret[IOTX_DEVICE_SECRET_LEN + 1]);

int HAL_SetDeviceName(char *device_name);
int HAL_SetDeviceSecret(char *device_secret);
int HAL_SetProductKey(char *product_key);
int HAL_SetProductSecret(char *product_secret);
int HAL_SetFirmwareVersion();
int HAL_GetFirmwareVersion(char *version);

extern int HAL_Kv_Get(const char *key, void *val, int *buffer_len);
extern int HAL_Kv_Set(const char *key, const void *val, int len, int sync);
extern int HAL_Kv_Del(const char *key);

extern int wlan_write_reconnect_data_to_flash(u8 *data, uint32_t len);

#define EXAMPLE_TRACE(...)                                          \
    do {                                                            \
        HAL_Printf("\033[1;32;40m%s.%d: ", __func__, __LINE__);     \
        HAL_Printf(__VA_ARGS__);                                    \
        HAL_Printf("\033[0m\r\n");                                  \
    } while (0)

#define EXAMPLE_MASTER_DEVID            (0)
#define EXAMPLE_YIELD_TIMEOUT_MS        (200)

typedef struct {
    int master_devid;
    int cloud_connected;
    int master_initialized;
} user_example_ctx_t;


enum SERVER_ENV {
	DAILY = 0,
	SANDBOX,
	ONLINE,
	DEFAULT
};

#define USE_DEV_AP 1

static int env = ONLINE;
const char *env_str[] = {"daily", "sandbox", "online", "default"};

static char log_level = IOT_LOG_DEBUG;
static uint8_t g_user_press = 0;
char linkkit_started = 0;
#if USE_DEMO_6
/*test_06*/
char PRODUCT_KEY[IOTX_PRODUCT_KEY_LEN + 1]       = "a1uFlHHBNxC";
char PRODUCT_SECRET[IOTX_PRODUCT_SECRET_LEN + 1] = "Szb1OMNqNDBzpA0e";
char DEVICE_NAME[IOTX_DEVICE_NAME_LEN + 1]       = "test_06";
char DEVICE_SECRET[IOTX_DEVICE_SECRET_LEN + 1]   = "5UfrihPpOz8UzzQxHSbfqbX9QfiUYVrK";
#else
/*test_05*/
char PRODUCT_KEY[IOTX_PRODUCT_KEY_LEN + 1]       = "a1qKCi0yXLp";
char PRODUCT_SECRET[IOTX_PRODUCT_SECRET_LEN + 1] = "gqEK2CZjygY0ppKx";
char DEVICE_NAME[IOTX_DEVICE_NAME_LEN + 1]       = "test_05";
char DEVICE_SECRET[IOTX_DEVICE_SECRET_LEN + 1]   = "sxUJFXfGd1KcIjBZ7xiphOt0U0LTEIVC";
#endif
iotx_linkkit_dev_meta_info_t master_meta_info;
static user_example_ctx_t g_user_example_ctx;

extern char  _firmware_version[IOTX_FIRMWARE_VER_LEN];
char example_dct_variable0[] = "variable0";
char example_dct_variable1[] = "variable1";
char example_dct_value0[] = "value0";
char example_dct_value1[] = "value1";

/** cloud connected event callback */
static int user_connected_event_handler(void)
{
    EXAMPLE_TRACE("Cloud Connected");
    g_user_example_ctx.cloud_connected = 1;
	link_printf(LINK_INFO, "after connect to cloud available heap(%d bytes)\n", HAL_get_free_heap_size());

    return 0;
}

/** cloud disconnected event callback */
static int user_disconnected_event_handler(void)
{
    EXAMPLE_TRACE("Cloud Disconnected");
    g_user_example_ctx.cloud_connected = 0;

    return 0;
}

/* device initialized event callback */
static int user_initialized(const int devid)
{
    EXAMPLE_TRACE("Device Initialized");
    g_user_example_ctx.master_initialized = 1;

    return 0;
}

static user_example_ctx_t *user_example_get_ctx(void)
{
    return &g_user_example_ctx;
}


/** recv property post response message from cloud **/
static int user_report_reply_event_handler(const int devid, const int msgid, const int code, const char *reply,
        const int reply_len)
{
    EXAMPLE_TRACE("Message Post Reply Received, Message ID: %d, Code: %d, Reply: %.*s", msgid, code,
                  reply_len,
                  (reply == NULL)? ("NULL") : (reply));
    return 0;
}

/** recv event post response message from cloud **/
static int user_trigger_event_reply_event_handler(const int devid, const int msgid, const int code, const char *eventid,
        const int eventid_len, const char *message, const int message_len)
{
    EXAMPLE_TRACE("Trigger Event Reply Received, Message ID: %d, Code: %d, EventID: %.*s, Message: %.*s",
                  msgid, code,
                  eventid_len,
                  eventid, message_len, message);

    return 0;
}

/** recv property setting message from cloud **/
static int user_property_set_event_handler(const int devid, const char *request, const int request_len)
{
    int res = 0;
    EXAMPLE_TRACE("Property Set Received, Request: %s", request);
    int value=0;
     /* get value of 'PowerSwitch' key */
        
    cJSON *root = NULL,*child=NULL;
    root=cJSON_Parse(request);
    child=cJSON_GetObjectItem(root,"LightSwitch");
    value=child->valueint;
	link_printf(LINK_INFO,"\n\rswitch == %d\n\r",value);
	
    res = IOT_Linkkit_Report(EXAMPLE_MASTER_DEVID, ITM_MSG_POST_PROPERTY,
                             (unsigned char *)request, request_len);
    EXAMPLE_TRACE("Post Property Message ID: %d", res);

    return 0;
}

static int user_property_get_event_handler(const int devid, const char *request, const int request_len, char **response,
        int *response_len)
{
    cJSON *request_root = NULL, *item_propertyid = NULL;
    cJSON *response_root = NULL;
    int index = 0;
    EXAMPLE_TRACE("Property Get Received, Devid: %d, Request: %s", devid, request);

    /* Parse Request */
    request_root = cJSON_Parse(request);
    if (request_root == NULL) {
        EXAMPLE_TRACE("JSON Parse Error");
        return -1;
    }

    /* Prepare Response */
    response_root = cJSON_CreateObject();
    if (response_root == NULL) {
        EXAMPLE_TRACE("No Enough Memory");
        cJSON_Delete(request_root);
        return -1;
    }

    for (index = 0; index < cJSON_GetArraySize(request_root); index++) {
        item_propertyid = cJSON_GetArrayItem(request_root, index);
        if (item_propertyid == NULL ) {
            EXAMPLE_TRACE("JSON Parse Error");
            cJSON_Delete(request_root);
            cJSON_Delete(response_root);
            return -1;
        }

        EXAMPLE_TRACE("Property ID, index: %d, Value: %s", index, item_propertyid->valuestring);

        if (strcmp("WIFI_Tx_Rate", item_propertyid->valuestring) == 0) {
            cJSON_AddNumberToObject(response_root, "WIFI_Tx_Rate", 1111);
        } else if (strcmp("WIFI_Rx_Rate", item_propertyid->valuestring) == 0) {
            cJSON_AddNumberToObject(response_root, "WIFI_Rx_Rate", 2222);
        } else if (strcmp("RGBColor", item_propertyid->valuestring) == 0) {
            cJSON *item_rgbcolor = cJSON_CreateObject();
            if (item_rgbcolor == NULL) {
                cJSON_Delete(request_root);
                cJSON_Delete(response_root);
                return -1;
            }
            cJSON_AddNumberToObject(item_rgbcolor, "Red", 100);
            cJSON_AddNumberToObject(item_rgbcolor, "Green", 100);
            cJSON_AddNumberToObject(item_rgbcolor, "Blue", 100);
            cJSON_AddItemToObject(response_root, "RGBColor", item_rgbcolor);
        } else if (strcmp("HSVColor", item_propertyid->valuestring) == 0) {
            cJSON *item_hsvcolor = cJSON_CreateObject();
            if (item_hsvcolor == NULL) {
                cJSON_Delete(request_root);
                cJSON_Delete(response_root);
                return -1;
            }
            cJSON_AddNumberToObject(item_hsvcolor, "Hue", 50);
            cJSON_AddNumberToObject(item_hsvcolor, "Saturation", 50);
            cJSON_AddNumberToObject(item_hsvcolor, "Value", 50);
            cJSON_AddItemToObject(response_root, "HSVColor", item_hsvcolor);
        } else if (strcmp("HSLColor", item_propertyid->valuestring) == 0) {
            cJSON *item_hslcolor = cJSON_CreateObject();
            if (item_hslcolor == NULL) {
                cJSON_Delete(request_root);
                cJSON_Delete(response_root);
                return -1;
            }
            cJSON_AddNumberToObject(item_hslcolor, "Hue", 70);
            cJSON_AddNumberToObject(item_hslcolor, "Saturation", 70);
            cJSON_AddNumberToObject(item_hslcolor, "Lightness", 70);
            cJSON_AddItemToObject(response_root, "HSLColor", item_hslcolor);
        } else if (strcmp("WorkMode", item_propertyid->valuestring) == 0) {
            cJSON_AddNumberToObject(response_root, "WorkMode", 4);
        } else if (strcmp("NightLightSwitch", item_propertyid->valuestring) == 0) {
            cJSON_AddNumberToObject(response_root, "NightLightSwitch", 1);
        } else if (strcmp("Brightness", item_propertyid->valuestring) == 0) {
            cJSON_AddNumberToObject(response_root, "Brightness", 30);
        } else if (strcmp("LightSwitch", item_propertyid->valuestring) == 0) {
            cJSON_AddNumberToObject(response_root, "LightSwitch", 1);
        } else if (strcmp("ColorTemperature", item_propertyid->valuestring) == 0) {
            cJSON_AddNumberToObject(response_root, "ColorTemperature", 2800);
        } else if (strcmp("PropertyCharacter", item_propertyid->valuestring) == 0) {
            cJSON_AddStringToObject(response_root, "PropertyCharacter", "testprop");
        } else if (strcmp("Propertypoint", item_propertyid->valuestring) == 0) {
            cJSON_AddNumberToObject(response_root, "Propertypoint", 50);
        } else if (strcmp("LocalTimer", item_propertyid->valuestring) == 0) {
            cJSON *array_localtimer = cJSON_CreateArray();
            if (array_localtimer == NULL) {
                cJSON_Delete(request_root);
                cJSON_Delete(response_root);
                return -1;
            }

            cJSON *item_localtimer = cJSON_CreateObject();
            if (item_localtimer == NULL) {
                cJSON_Delete(request_root);
                cJSON_Delete(response_root);
                cJSON_Delete(array_localtimer);
                return -1;
            }
            cJSON_AddStringToObject(item_localtimer, "Timer", "10 11 * * * 1 2 3 4 5");
            cJSON_AddNumberToObject(item_localtimer, "Enable", 1);
            cJSON_AddNumberToObject(item_localtimer, "IsValid", 1);
            cJSON_AddItemToArray(array_localtimer, item_localtimer);
            cJSON_AddItemToObject(response_root, "LocalTimer", array_localtimer);
        }
    }
    cJSON_Delete(request_root);

    *response = cJSON_PrintUnformatted(response_root);
    if (*response == NULL) {
        EXAMPLE_TRACE("No Enough Memory");
        cJSON_Delete(response_root);
        return -1;
    }
    cJSON_Delete(response_root);
    *response_len = strlen(*response);

    EXAMPLE_TRACE("Property Get Response: %s", *response);

    return SUCCESS_RETURN;
}



static int user_service_request_event_handler(const int devid, const char *serviceid, const int serviceid_len,
                                              const char *request, const int request_len,
                                              char **response, int *response_len)
{
    int add_result = 0;
    cJSON *root = NULL, *item_number_a = NULL, *item_number_b = NULL;
    const char *response_fmt = "{\"Result\": %d}";

    EXAMPLE_TRACE("Service Request Received, Service ID: %.*s, Payload: %s", serviceid_len, serviceid, request);

    /* Parse Root */
    root = cJSON_Parse(request);
    if (root == NULL || !cJSON_IsObject(root)) {
        EXAMPLE_TRACE("JSON Parse Error");
        return -1;
    }

    if (strlen("Operation_Service") == serviceid_len && memcmp("Operation_Service", serviceid, serviceid_len) == 0) {
        /* Parse NumberA */
        item_number_a = cJSON_GetObjectItem(root, "NumberA");
        if (item_number_a == NULL || !cJSON_IsNumber(item_number_a)) {
            cJSON_Delete(root);
            return -1;
        }
        EXAMPLE_TRACE("NumberA = %d", item_number_a->valueint);

        /* Parse NumberB */
        item_number_b = cJSON_GetObjectItem(root, "NumberB");
        if (item_number_b == NULL || !cJSON_IsNumber(item_number_b)) {
            cJSON_Delete(root);
            return -1;
        }
        EXAMPLE_TRACE("NumberB = %d", item_number_b->valueint);

        add_result = item_number_a->valueint + item_number_b->valueint;

        /* Send Service Response To Cloud */
        *response_len = strlen(response_fmt) + 10 + 1;
        *response = (char *)HAL_Malloc(*response_len);
        if (*response == NULL) {
            EXAMPLE_TRACE("Memory Not Enough");
            return -1;
        }
        memset(*response, 0, *response_len);
        HAL_Snprintf(*response, *response_len, response_fmt, add_result);
        *response_len = strlen(*response);
    }

    cJSON_Delete(root);
    return 0;
}

static int user_timestamp_reply_event_handler(const char *timestamp)
{
    EXAMPLE_TRACE("Current Timestamp: %s", timestamp);

    return 0;
}

/** fota event handler **/
static int user_fota_event_handler(int type, const char *version)
{
    char buffer[128] = {0};
    int buffer_length = 128;

    /* 0 - new firmware exist, query the new firmware */
    if (type == 0) {
        EXAMPLE_TRACE("New Firmware Version: %s", version);
		memcpy(_firmware_version,version,IOTX_FIRMWARE_VER_LEN);
		link_printf(LINK_INFO, "before ota available heap(%d bytes)\n", HAL_get_free_heap_size());

        IOT_Linkkit_Query(EXAMPLE_MASTER_DEVID, ITM_MSG_QUERY_FOTA_DATA, (unsigned char *)buffer, buffer_length);
    }

    return 0;
}


void user_post_property(void)
{
    static int cnt = 0;
    int res = 0;

    char property_payload[30] = {0};
    HAL_Snprintf(property_payload, sizeof(property_payload), "{\"Counter\": %d}", cnt++);

    res = IOT_Linkkit_Report(EXAMPLE_MASTER_DEVID, ITM_MSG_POST_PROPERTY,
                             (unsigned char *)property_payload, strlen(property_payload));

    EXAMPLE_TRACE("Post Property Message ID: %d", res);
}

void user_post_event(void)
{
    int res = 0;
    char *event_id = "HardwareError";
    char *event_payload = "{\"ErrorCode\": 0}";

    res = IOT_Linkkit_TriggerEvent(EXAMPLE_MASTER_DEVID, event_id, strlen(event_id),
                                   event_payload, strlen(event_payload));
    EXAMPLE_TRACE("Post Event Message ID: %d", res);
}

void user_post_raw_data(void)
{
    int res = 0;
    user_example_ctx_t *user_example_ctx = user_example_get_ctx();
    unsigned char raw_data[2] = {0x01, 0x02};

    res = IOT_Linkkit_Report(user_example_ctx->master_devid, ITM_MSG_POST_RAW_DATA,
                             raw_data, 7);
    EXAMPLE_TRACE("Post Raw Data Message ID: %d", res);
}

static int user_down_raw_data_arrived_event_handler(const int devid, const unsigned char *payload,
        const int payload_len)
{
    EXAMPLE_TRACE("Down Raw Message, Devid: %d, Payload Length: %d", devid, payload_len);
	EXAMPLE_TRACE("payload:%s", payload);
   // user_post_raw_data();
    return 0;

}


void user_deviceinfo_update(void)
{
    int res = 0;
    char *device_info_update = "[{\"attrKey\":\"abc\",\"attrValue\":\"hello,world\"}]";

    res = IOT_Linkkit_Report(EXAMPLE_MASTER_DEVID, ITM_MSG_DEVICEINFO_UPDATE,
                             (unsigned char *)device_info_update, strlen(device_info_update));
    EXAMPLE_TRACE("Device Info Update Message ID: %d", res);
}

void user_deviceinfo_delete(void)
{
    int res = 0;
    char *device_info_delete = "[{\"attrKey\":\"abc\"}]";

    res = IOT_Linkkit_Report(EXAMPLE_MASTER_DEVID, ITM_MSG_DEVICEINFO_DELETE,
                             (unsigned char *)device_info_delete, strlen(device_info_delete));
    EXAMPLE_TRACE("Device Info Delete Message ID: %d", res);
}

/*
 * Note:
 * the linkkit_event_monitor must not block and should run to complete fast
 * if user wants to do complex operation with much time,
 * user should post one task to do this, not implement complex operation in
 * linkkit_event_monitor
 */
static void linkkit_event_monitor(int event)
{
    switch (event) {
        case IOTX_AWSS_START: // AWSS start without enbale, just supports device discover
            // operate led to indicate user
            link_printf(LINK_INFO,"IOTX_AWSS_START\n");
            break;
        case IOTX_AWSS_ENABLE: // AWSS enable, AWSS doesn't parse awss packet until AWSS is enabled.
            link_printf(LINK_INFO,"IOTX_AWSS_ENABLE\n");
			link_printf(LINK_INFO,"waiting for wifimgr...\n");
            // operate led to indicate user
            break;
        case IOTX_AWSS_LOCK_CHAN: // AWSS lock channel(Got AWSS sync packet)
            link_printf(LINK_INFO,"IOTX_AWSS_LOCK_CHAN\n");
            // operate led to indicate user
            break;
        case IOTX_AWSS_PASSWD_ERR: // AWSS decrypt passwd error
            link_printf(LINK_INFO,"IOTX_AWSS_PASSWD_ERR\n");
            // operate led to indicate user
            break;
        case IOTX_AWSS_GOT_SSID_PASSWD:
            link_printf(LINK_INFO,"IOTX_AWSS_GOT_SSID_PASSWD\n");
            // operate led to indicate user
            break;
        case IOTX_AWSS_SETUP_NOTIFY: // AWSS sends out device setup information
            // (AP and router solution)
            link_printf(LINK_INFO,"IOTX_AWSS_SETUP_NOTIFY\n");
            // operate led to indicate user
            break;
        case IOTX_AWSS_CONNECT_ROUTER: // AWSS try to connect destination router
            link_printf(LINK_INFO,"IOTX_AWSS_CONNECT_ROUTER\n");
            // operate led to indicate user
            break;
        case IOTX_AWSS_CONNECT_ROUTER_FAIL: // AWSS fails to connect destination
            // router.
            link_printf(LINK_INFO,"IOTX_AWSS_CONNECT_ROUTER_FAIL\n");
            // operate led to indicate user
            break;
        case IOTX_AWSS_GOT_IP: // AWSS connects destination successfully and got
            // ip address
            link_printf(LINK_INFO,"IOTX_AWSS_GOT_IP\n");
            // operate led to indicate user
            break;
        case IOTX_AWSS_SUC_NOTIFY: // AWSS sends out success notify (AWSS
            // sucess)
            link_printf(LINK_INFO,"IOTX_AWSS_SUC_NOTIFY\n");
            // operate led to indicate user
            break;
        case IOTX_AWSS_BIND_NOTIFY: // AWSS sends out bind notify information to
            // support bind between user and device
            link_printf(LINK_INFO,"IOTX_AWSS_BIND_NOTIFY\n");
            // operate led to indicate user
            break;
		case IOTX_CONN_REPORT_TOKEN_SUC: // AWSS sends out bind notify information to
            // support bind between user and device
            link_printf(LINK_INFO,"IOTX_CONN_REPORT_TOKEN_SUC\n");
            // operate led to indicate user
            break;
        case IOTX_AWSS_ENABLE_TIMEOUT: // AWSS enable timeout
            // user needs to enable awss again to support get ssid & passwd of router
            link_printf(LINK_INFO,"IOTX_AWSS_ENALBE_TIMEOUT\n");
            // operate led to indicate user
            break;
        case IOTX_CONN_CLOUD: // Device try to connect cloud
            link_printf(LINK_INFO,"IOTX_CONN_CLOUD\n");
            // operate led to indicate user
            break;
        case IOTX_CONN_CLOUD_FAIL: // Device fails to connect cloud, refer to
            // net_sockets.h for error code
            link_printf(LINK_INFO,"IOTX_CONN_CLOUD_FAIL\n");
            // operate led to indicate user
            break;
        case IOTX_CONN_CLOUD_SUC: // Device connects cloud successfully
            link_printf(LINK_INFO,"IOTX_CONN_CLOUD_SUC\n");
            // operate led to indicate user
            break;
        case IOTX_RESET: // Linkkit reset success (just got reset response from
            // cloud without any other operation)
            link_printf(LINK_INFO,"IOTX_RESET\n");
            // operate led to indicate user
            break;
        default:
            break;
    }
}


int set_iotx_info()
{
    char ver[IOTX_FIRMWARE_VER_LEN + 1] = {0};
	int ret=0;

	ret=HAL_GetProductKey(PRODUCT_KEY);
	
	if(ret<0) 
		return -1;
	ret=HAL_GetProductSecret(PRODUCT_SECRET);
	
	if(ret<0){
		return -1;
	}
	ret=HAL_GetDeviceName(DEVICE_NAME);
	
	if(ret<0){
		return -1;
	}
	ret=HAL_GetDeviceSecret(DEVICE_SECRET);

	if(ret<0){
		return -1;
	}

	ret=HAL_GetFirmwareVersion(ver);
	
	if(ret<0){
		return -1;
	}

	
	link_printf(LINK_INFO,"fw ver(%s).\n",ver);
	link_printf(LINK_INFO,"product_key:%s\n", PRODUCT_KEY);
	link_printf(LINK_INFO,"product_secret:%s\n", PRODUCT_SECRET);
	link_printf(LINK_INFO,"device_name:%s\n", DEVICE_NAME);
	link_printf(LINK_INFO,"device_secret:%s\n", DEVICE_SECRET);
	return 0;
    
}


int linkkit_load_wifi_config(struct wlan_fast_reconnect	* data)
{
	int ret;
	unsigned char    pscan_config;
	unsigned char    scan_channel;
	int dhcp_retry = 1;	
	int connect_retry = 0;
	
	pscan_config = PSCAN_ENABLE;
	scan_channel	= (data->channel) & 0xFF;
	if(data->security_type == RTW_SECURITY_WEP_PSK) {
		/*wifi->password_len*/
		if(sizeof(data->psk_passphrase) == 10)
		{
                  u32 p[5];
                  u8 pwd[6], i = 0; 
                  sscanf((const char*)data->psk_passphrase, "%02x%02x%02x%02x%02x", &p[0], &p[1], &p[2], &p[3], &p[4]);
                  for(i=0; i< 5; i++)
                          pwd[i] = (u8)p[i];
                  pwd[5] = '\0';
                  memset(data->psk_passphrase, 0, 65);
                  strcpy((char*)data->psk_passphrase, (char*)pwd);
		} else if(sizeof(data->psk_passphrase) == 26) {
                    u32 p[13];
                    u8 pwd[14], i = 0;
                    sscanf((const char*)data->psk_passphrase, "%02x%02x%02x%02x%02x%02x%02x"\
                            "%02x%02x%02x%02x%02x%02x", &p[0], &p[1], &p[2], &p[3], &p[4],\
                            &p[5], &p[6], &p[7], &p[8], &p[9], &p[10], &p[11], &p[12]);
                    for(i=0; i< 13; i++)
                            pwd[i] = (u8)p[i];
                    pwd[13] = '\0';
                    memset(data->psk_passphrase, 0, 65);
                    strcpy((char*)data->psk_passphrase, (char*)pwd);
		}
	}

	wifi_disable_powersave();//add to close powersave

	link_printf(LINK_INFO, "Set pscan channel = %d\n", scan_channel); 
	if(wifi_set_pscan_chan(&scan_channel, &pscan_config, 1) < 0) {
		link_err_printf("wifi set partial scan channel fail");
		return LINK_ERR;
	}

	while(1){
		ret = wifi_connect((char*)data->psk_essid,
					  (rtw_security_t)data->security_type,
					  (char*)data->psk_passphrase,
					  (int)strlen((char const *)data->psk_essid),
					  (int)strlen((char const *)data->psk_passphrase),
					  0,
					  NULL);
		if (ret == LINK_OK) {
Try_again:		
			ret = LwIP_DHCP(0, DHCP_START);	
			if(ret != DHCP_ADDRESS_ASSIGNED) {
				if(dhcp_retry){
					link_err_printf( "DHCP failed. Goto try again!\n\r");
					dhcp_retry--;
					goto Try_again;
				} else {
					link_err_printf("DHCP failed. \n\r");
				}
			} else {	
				link_printf(LINK_INFO, "[%s] wifi connect ok !\n", __func__);
				if (connect_retry) {
					wlan_write_reconnect_data_to_flash((u8 *)data, sizeof(struct wlan_fast_reconnect));
				}
				break;
			}
		} else {
			++connect_retry;
			link_err_printf("wifi connect fail !");
			if (connect_retry) {
				if(get_ap_security_mode((char*)data->psk_essid, &data->security_type, &scan_channel) != 0) {
					scan_channel = 0;
					data->security_type = RTW_SECURITY_WPA2_AES_PSK;
					link_printf(LINK_WARNING, "Warning : unknow security type, default set to WPA2_AES");
				} else {
					if ((data->channel)  != (uint32_t) scan_channel ) {
						(data->channel)  = (uint32_t) scan_channel;
						link_printf(LINK_INFO, "Reset pscan channel = %d\n", scan_channel); 
						u8 pscan_config = PSCAN_ENABLE;
						if(scan_channel > 0 && scan_channel < 14) {
							if(wifi_set_pscan_chan(&scan_channel, &pscan_config, 1) < 0) {
								link_printf(LINK_WARNING, "wifi set partial scan channel fail\n");
							}	
						} 
					}
				}
			}

			return LINK_ERR;

		}	
		HAL_SleepMs(1000);
	}
	
	return LINK_OK;
}


void link_to_cloud()
{
    int res = 0;
    int domain_type = 0, dynamic_register = 0, post_reply_need = 0;
    user_example_ctx_t             *user_example_ctx = user_example_get_ctx();
    memset(user_example_ctx, 0, sizeof(user_example_ctx_t));	

    link_printf(LINK_INFO,"Start Connect Aliyun Server(cloud-region-shanghai)\n");
	
    /* Choose Login Server, domain should be configured before IOT_Linkkit_Open() */
#if USE_CUSTOME_DOMAIN
    IOT_Ioctl(IOTX_IOCTL_SET_MQTT_DOMAIN, (void *)CUSTOME_DOMAIN_MQTT);
    IOT_Ioctl(IOTX_IOCTL_SET_HTTP_DOMAIN, (void *)CUSTOME_DOMAIN_HTTP);
#else
    domain_type = IOTX_CLOUD_REGION_SHANGHAI;
    IOT_Ioctl(IOTX_IOCTL_SET_DOMAIN, (void *)&domain_type);
#endif

    /* Choose Login Method */
    dynamic_register = 0;
    IOT_Ioctl(IOTX_IOCTL_SET_DYNAMIC_REGISTER, (void *)&dynamic_register);

    /* Choose Whether You Need Post Property/Event Reply */
    post_reply_need = 1;
    IOT_Ioctl(IOTX_IOCTL_RECV_EVENT_REPLY, (void *)&post_reply_need);

    /* Create Master Device Resources */
    user_example_ctx->master_devid = IOT_Linkkit_Open(IOTX_LINKKIT_DEV_TYPE_MASTER, &master_meta_info);
    if (user_example_ctx->master_devid < 0) {
        EXAMPLE_TRACE("IOT_Linkkit_Open Failed\n");
    }
    /* Start Connect Aliyun Server */
    do{
      res = IOT_Linkkit_Connect(user_example_ctx->master_devid);
      if (res < 0) {
          EXAMPLE_TRACE("IOT_Linkkit_Connect Failed\n");
      }
    } while (res < 0);
	link_printf(LINK_INFO,"IOT_Linkkit_Connect\n");
	//awss_report_reset();	
}

void main_loop(void)
{
	int cnt = 0;
	while(1){

#ifdef RAW_DATA_DEVICE
		/*
		* Note: post data to cloud,
		* use sample user_post_raw_data()
		*/
		/* sample for raw data device */
		user_post_raw_data();

#else
        IOT_Linkkit_Yield(EXAMPLE_YIELD_TIMEOUT_MS);
        /* Post Property Example */
        if ((cnt % 2) == 0) {
            //user_post_property();
			link_printf(LINK_INFO,"user post property\n");
        }

        /* Post Event Example */
        if ((cnt % 10) == 0) {
       		//user_post_event();
			link_printf(LINK_INFO,"user post event\n");
        }		

#endif
		cnt++;
		HAL_SleepMs(2000);
	} 
}

/* activate sample */
char active_data_tx_buffer[128];
#define ActivateDataFormat    "{\"ErrorCode\": { \"value\": \"%d\" }}"
#define CONFIG_GPIO_TASK	1
#define GPIO_BUTTON PA_20
#define GPIOIRQ_BUTTON PA_19

int awss_running=0;

#if CONFIG_GPIO_TASK 
void *gpio_exception_sema=NULL;
#endif

void do_awss_active()
{
    awss_running=1;
    link_printf(LINK_INFO,"do_awss_active %d\n", awss_running);
#ifdef WIFI_PROVISION_ENABLED
    extern int awss_config_press();
    awss_config_press();
#endif
}


static void linkkit_reset_gpio_irq_handler(uint32_t id, gpio_irq_event event)
{
	link_printf(LINK_INFO, "GPIO push button !\n");
	do_awss_active();
}

static void linkkit_reset_gpio_init()
{
	gpio_t	gpio_softap_reset_button;
	gpio_irq_t	gpioirq_softap_reset_button;

	gpio_init(&gpio_softap_reset_button, GPIO_BUTTON);
    gpio_dir(&gpio_softap_reset_button, PIN_OUTPUT);    // Direction: Output
    gpio_mode(&gpio_softap_reset_button, PullNone);     // No pull

	gpio_irq_init(&gpioirq_softap_reset_button, GPIOIRQ_BUTTON, linkkit_reset_gpio_irq_handler, (uint32_t)(&gpio_softap_reset_button));
	gpio_irq_set(&gpioirq_softap_reset_button, IRQ_FALL, 1);
	gpio_irq_enable(&gpioirq_softap_reset_button);
}


void linkkit_dev_init()
{
	linkkit_reset_gpio_init();
	link_printf(LINK_INFO,"gpio initialized!\n");

}

void linkkit_example_thread(int argc,char *argv[])
{
	link_printf(LINK_INFO, "\n [%s] link server: %s,  log level: %d\n",
	        __FUNCTION__, env_str[env], log_level);
	link_printf(LINK_INFO, "\n before firmware update!\n");
	int ret=0;
	ret=set_iotx_info();
	if(ret<0){
		goto END;
	}
	IOT_SetLogLevel(IOT_LOG_NONE);
	//linkkit_dev_init();
	iotx_event_regist_cb(linkkit_event_monitor);

	/* reload or config wifi */
	
	struct wlan_fast_reconnect	*data;
	data = (struct wlan_fast_reconnect *)rtw_zmalloc(sizeof(struct wlan_fast_reconnect));
	if (data) {
		flash_t 	flash;
		device_mutex_lock(RT_DEV_LOCK_FLASH);
		flash_stream_read(&flash, FAST_RECONNECT_DATA, sizeof(struct wlan_fast_reconnect), (uint8_t *)data);
		device_mutex_unlock(RT_DEV_LOCK_FLASH);

		/* reload ap profile*/
		if((*((uint32_t *) data) != ~0x0) && strlen(data->psk_essid)) {
				link_printf(LINK_INFO, "Read AP profile from flash : ssid = %s, pwd = %s, ssid length = %d, pwd length = %d, channel = %d, security =%d\n", 
					data->psk_essid, data->psk_passphrase, strlen(data->psk_essid), strlen(data->psk_passphrase), data->channel, data->security_type);
				ret=linkkit_load_wifi_config(data);
		}else{			
			link_printf(LINK_INFO, "No AP profile read from FLASH, start awss...\n");
			HAL_SleepMs(1000);

#if USE_DEV_AP
			awss_dev_ap_start();
#else
			awss_config_press();
			awss_start();
#endif
                     

		}
#if CONFIG_AUTO_RECONNECT
		wifi_set_autoreconnect(1);
		link_printf(LINK_INFO, "wifi_set_autoreconnect\n");
#endif
		if(data) {
			HAL_Free((struct wlan_fast_reconnect *)data);
		}
	}else{	
		link_printf(LINK_INFO, "malloc failed!\n");
		goto END;
	}

	/* Register Callback */
	IOT_RegisterCallback(ITE_CONNECT_SUCC, user_connected_event_handler);
	IOT_RegisterCallback(ITE_DISCONNECTED, user_disconnected_event_handler);
	//IOT_RegisterCallback(ITE_SERVICE_REQUEST, user_service_request_event_handler);
	IOT_RegisterCallback(ITE_PROPERTY_SET, user_property_set_event_handler);
	//IOT_RegisterCallback(ITE_PROPERTY_GET, user_property_get_event_handler);
	//IOT_RegisterCallback(ITE_REPORT_REPLY, user_report_reply_event_handler);
	//IOT_RegisterCallback(ITE_TRIGGER_EVENT_REPLY, user_trigger_event_reply_event_handler);
	//IOT_RegisterCallback(ITE_TIMESTAMP_REPLY, user_timestamp_reply_event_handler);
	IOT_RegisterCallback(ITE_INITIALIZE_COMPLETED, user_initialized);
	IOT_RegisterCallback(ITE_FOTA, user_fota_event_handler);
	IOT_RegisterCallback(ITE_RAWDATA_ARRIVED, user_down_raw_data_arrived_event_handler);
	
	memset(&master_meta_info, 0, sizeof(iotx_linkkit_dev_meta_info_t));
	memcpy(master_meta_info.product_key, PRODUCT_KEY, strlen(PRODUCT_KEY));
	memcpy(master_meta_info.product_secret, PRODUCT_SECRET, strlen(PRODUCT_SECRET));
	memcpy(master_meta_info.device_name, DEVICE_NAME, strlen(DEVICE_NAME));
	memcpy(master_meta_info.device_secret, DEVICE_SECRET, strlen(DEVICE_SECRET));

	link_to_cloud();
/* loop report to cloud */
	main_loop();      

#if USE_DEV_AP
    awss_dev_ap_stop();
#endif  
    IOT_Linkkit_Close(g_user_example_ctx.master_devid);
    IOT_DumpMemoryStats(IOT_LOG_DEBUG);
    IOT_SetLogLevel(IOT_LOG_NONE);
END:	
	HAL_ThreadDelete(NULL);
}


xTaskHandle xTask2Handle;

void kv_test1_thread(int argc,char *argv[])
{	
	unsigned portBASE_TYPE uxPriority;
	uxPriority = uxTaskPriorityGet( NULL );


	int 			ret = -1;
	char			value[16];
	dct_handle_t	dct_handle;

	/**************************** DCT ************************************************************/	
		// set test variable 0
		ret = HAL_Kv_Set(example_dct_variable0, example_dct_value0, sizeof(example_dct_value0), 0);
		link_printf(LINK_INFO, "[%d] ret:%d \n", __LINE__, ret);
			
		// get value of test variable 0		
		len_variable = sizeof(value);
		memset(value, 0, sizeof(value));
		ret = HAL_Kv_Get(example_dct_variable0, value, &len_variable);
		if(ret == DCT_SUCCESS)
			printf("%s: %s\n", example_dct_variable0, value);
					
		// delete test variable 0
		//ret = HAL_Kv_Del(example_dct_variable0);
		// get value of test variable 1
		memset(value, 0, sizeof(value));
		len_variable = sizeof(value);
		ret = HAL_Kv_Get(example_dct_variable1, value, &len_variable);
		if(ret == DCT_ERR_NOT_FIND)
			printf("Delete %s success.\n", example_dct_variable1);				
	/*****************************************************************************************************/	
	vTaskPrioritySet( xTask2Handle, ( uxPriority + 1 ) );
	HAL_ThreadDelete(NULL);
}

void kv_test2_thread(int argc,char *argv[])
{
	unsigned portBASE_TYPE uxPriority;
	uxPriority = uxTaskPriorityGet( NULL );

	link_printf(LINK_INFO, "[%d] in kv_test2_thread \n", __LINE__);

	int 			ret = -1;
	char			value[16];
	dct_handle_t	dct_handle;
	/**************************** DCT ************************************************************/	
		// set test variable 1
		ret = HAL_Kv_Set(example_dct_variable1, example_dct_value1, sizeof(example_dct_value1), 0);
		link_printf(LINK_INFO, "[%d] ret:%d \n", __LINE__, ret);
					
		// get value of test variable 1		
		len_variable = sizeof(value);
		memset(value, 0, sizeof(value));
		ret = HAL_Kv_Get(example_dct_variable1, value, &len_variable);
		if(ret == DCT_SUCCESS)
			printf("%s: %s\n", example_dct_variable1, value);

		// get value of test variable 0
		memset(value, 0, sizeof(value));
		len_variable = sizeof(value);
		ret = HAL_Kv_Get(example_dct_variable0, value, &len_variable);
		if(ret == DCT_SUCCESS)
			printf("%s: %s\n", example_dct_variable0, value);

		// delete test variable 1
		ret = HAL_Kv_Del(example_dct_variable1);
				
		// get value of test variable 1
		memset(value, 0, sizeof(value));
		len_variable = sizeof(value);
		ret = HAL_Kv_Get(example_dct_variable1, value, &len_variable);
		if(ret == DCT_ERR_NOT_FIND)
			printf("Delete %s success.\n", example_dct_variable1);		
			
		//aliyun_deinit_dct();
		ret = dct_close_module(&aliyun_kv_handle);
		printf("dct close success.\n");
	/*****************************************************************************************************/
	vTaskPrioritySet( xTask2Handle, ( uxPriority - 2 ) );
	HAL_ThreadDelete(NULL);
}

void test_linkkit_kv_thread()
{
	int 			ret = -1;
	// initial DCT
	ret = dct_init(KV_BEGIN_ADDR, KV_MODULE_NUM, KV_VARIABLE_NAME_SIZE, KV_VARIABLE_VALUE_SIZE, 0, 0);	
	link_printf(LINK_INFO, "[%d] ret:%d \n", __LINE__, ret);
	
	// register module
	ret = dct_register_module(KV_MODULE_NAME);	
	link_printf(LINK_INFO, "[%d] ret:%d \n", __LINE__, ret);
	
	// open module
	ret = dct_open_module(&aliyun_kv_handle, KV_MODULE_NAME);
	link_printf(LINK_INFO, "[%d] ret:%d \n", __LINE__, ret);	

	link_printf(LINK_INFO, "before create task heap size:%d\n", HAL_get_free_heap_size());

	p_wlan_init_done_callback=NULL;
	if(xTaskCreate((TaskFunction_t)kv_test1_thread, (char const *)"kv_test1", 1024, NULL, tskIDLE_PRIORITY + 6, NULL) != pdPASS) {
		link_err_printf("xTaskCreate failed\n");	
	}

	if(xTaskCreate((TaskFunction_t)kv_test2_thread, (char const *)"kv_test2", 1024, NULL, tskIDLE_PRIORITY + 5, &xTask2Handle) != pdPASS) {
		link_err_printf("xTaskCreate failed\n");	
	}

	link_printf(LINK_INFO, "after create task heap size:%d\n", HAL_get_free_heap_size());
}

int start_linkkit_thread()
{
	link_printf(LINK_INFO, "before create task heap size:%d\n", HAL_get_free_heap_size());

	p_wlan_init_done_callback=NULL;
	if(xTaskCreate((TaskFunction_t)linkkit_example_thread, (char const *)"aws_start", 1024*9, NULL, tskIDLE_PRIORITY + 5, NULL) != pdPASS) {
		link_err_printf("xTaskCreate failed\n");	
	}

	link_printf(LINK_INFO, "after create task heap size:%d\n", HAL_get_free_heap_size());
    link_printf(LINK_INFO, "start awss thread\n");

	return 0;
}

void example_ali_awss()
{
	p_wlan_init_done_callback = start_linkkit_thread;
	//p_wlan_init_done_callback = test_linkkit_kv_thread;
}
