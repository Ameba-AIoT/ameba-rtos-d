/**
 * NOTE:
 *
 * HAL_TCP_xxx API reference implementation: wrappers/os/ubuntu/HAL_TCP_linux.c
 *
 */

#include "wrappers_defs.h"

#include "semphr.h"
#include "sys.h"
#include "time.h"
#include "timers.h"

   
/* mask this line, if stdint.h is not defined */
//#define USE_STDINT_H

#ifdef USE_STDINT_H
    #include <stdint.h>
#endif

#ifndef WPA_GET_BE24(a)
#define WPA_GET_BE24(a) ((((u32) (a)[0]) << 16) | (((u32) (a)[1]) << 8) | \
			 ((u32) (a)[2]))
#endif

#if defined(CONFIG_PLATFORM_8711B)
char _firmware_version[IOTX_FIRMWARE_VER_LEN] = "rtl-amebaz-sdk-v4.0-05";
#else
char _firmware_version[IOTX_FIRMWARE_VER_LEN] = "rtl-amebaz2-sdk-v7.1-01";
#endif
#define _DEMO_ 0
#if USE_DEMO_6
/* test_06*/
char _product_key[IOTX_PRODUCT_KEY_LEN + 1]       = "a1uFlHHBNxC";
char _product_secret[IOTX_PRODUCT_SECRET_LEN + 1] = "Szb1OMNqNDBzpA0e";
char _device_name[IOTX_DEVICE_NAME_LEN + 1]       = "test_06";
char _device_secret[IOTX_DEVICE_SECRET_LEN + 1]   = "5UfrihPpOz8UzzQxHSbfqbX9QfiUYVrK";
#else
/*test_05*/
char _product_key[IOTX_PRODUCT_KEY_LEN + 1]       = "a1qKCi0yXLp";
char _product_secret[IOTX_PRODUCT_SECRET_LEN + 1] = "gqEK2CZjygY0ppKx";
char _device_name[IOTX_DEVICE_NAME_LEN + 1]       = "test_05";
char _device_secret[IOTX_DEVICE_SECRET_LEN + 1]   = "sxUJFXfGd1KcIjBZ7xiphOt0U0LTEIVC";
#endif

#define RTW_WIFI_APLIST_SIZE		40
/* Information Element IDs */
#define WLAN_EID_VENDOR_SPECIFIC 221

extern int (*p_wlan_mgmt_filter)(u8 *ie, u16 ie_len, u16 frame_type);
unsigned int g_vendor_oui = 0;
uint32_t g_vendor_frame_type = 0;
volatile int apScanCallBackFinished  = 0;
static char monitor_running;
rtw_wifi_setting_t wifi_config_setting;
uint8_t zbssid[ETH_ALEN]={0};

#define RAW_FRAME 1
awss_wifi_mgmt_frame_cb_t g_vendor_mgmt_filter_callback;
awss_recv_80211_frame_cb_t  p_handler_recv_data_callback;
awss_wifi_scan_result_cb_t  p_handler_scan_result_callback;

/* Parsed Information Elements */
struct i802_11_elems {
	u8 *ie;
	u8 len;
};

typedef  struct  _ApList_str  
{  
        char ssid[HAL_MAX_SSID_LEN];
        uint8_t bssid[ETH_ALEN];
        enum AWSS_AUTH_TYPE auth;
        enum AWSS_ENC_TYPE encry;
        uint8_t channel;
        char rssi;
        int is_last_ap;
}ApList_str; 

//store ap list
typedef  struct  _UwtPara_str  
{  
    char ApNum;       //AP number
    ApList_str * ApList; 
} UwtPara_str;  

UwtPara_str apList_t;


extern struct netif xnetif[NET_IF_NUM]; 

int  linkkit_flash_stream_write(uint32_t address, uint32_t len, uint8_t * data);
void  linkkit_flash_erase_sector(uint32_t address);
int  linkkit_flash_stream_read(uint32_t address, uint32_t len, uint8_t * data);


dct_handle_t aliyun_kv_handle;
uint16_t        len_variable;


int HAL_Kv_Get(const char *key, void *val, int *buffer_len)
{
	link_printf(LINK_INFO, "[%s] %s \n", __FUNCTION__, (char *)key);
	return dct_get_variable_new(&aliyun_kv_handle, (char *)key, (char *)val, (uint16_t *)buffer_len);
}

int HAL_Kv_Set(const char *key, const void *val, int len, int sync)
{
	link_printf(LINK_INFO, "[%s] %s: %s \n", __FUNCTION__, (char *)key, (char *)val);
	return dct_set_variable_new(&aliyun_kv_handle, (char *)key, (char *)val, (uint16_t)len);
}

int HAL_Kv_Del(const char *key)
{
	link_printf(LINK_INFO, "[%s] %s \n", __FUNCTION__, (char *)key);
	return dct_delete_variable_new(&aliyun_kv_handle, (char *)key);
}

                                            /*************os hal*******************/
/**
 * @brief Allocates a block of size bytes of memory, returning a pointer to the beginning of the block.
 *
 * @param [in] size @n specify block size in bytes.
 * @return A pointer to the beginning of the block.
 * @see None.
 * @note Block value is indeterminate.
 */
void *HAL_Malloc(uint32_t size)
{
	return pvPortMalloc(size);
}

/**
 * @brief Deallocate memory block
 *
 * @param[in] ptr @n Pointer to a memory block previously allocated with platform_malloc.
 * @return None.
 * @see None.
 * @note None.
 */
void HAL_Free(void *ptr)
{
 	if (ptr) {
		vPortFree(ptr);
		ptr = NULL;
	}
}

int HAL_get_free_heap_size()
{
	link_printf(LINK_DEBUG, "[%s]\n", __FUNCTION__);
	return xPortGetFreeHeapSize();
}


/**
 * @brief Create a mutex.
 *
 * @retval NULL : Initialize mutex failed.
 * @retval NOT_NULL : The mutex handle.
 * @see None.
 * @note None.
 */
void *HAL_MutexCreate(void)
{
	pthread_mutex_t mutex;
	mutex = xSemaphoreCreateRecursiveMutex();
	return (void *)mutex;

}



/**
 * @brief Destroy the specified mutex object, it will release related resource.
 *
 * @param [in] mutex @n The specified mutex.
 * @return None.
 * @see None.
 * @note None.
 */
void HAL_MutexDestroy(void *mutex)
{

	if(mutex != NULL){
		vQueueDelete(mutex);
	}
	mutex = NULL;
}



/**
 * @brief Waits until the specified mutex is in the signaled state.
 *
 * @param [in] mutex @n the specified mutex.
 * @return None.
 * @see None.
 * @note None.
 */
void HAL_MutexLock(void *mutex)
{

	while(xSemaphoreTakeRecursive(mutex, 1000) != pdTRUE){
		link_err_printf("[%s] %s(%p) failed, retry\n", pcTaskGetTaskName(NULL), __FUNCTION__, mutex);
	}

}


/**
 * @brief Releases ownership of the specified mutex object..
 *
 * @param [in] mutex @n the specified mutex.
 * @return None.
 * @see None.
 * @note None.
 */
void HAL_MutexUnlock(void *mutex)
{
	xSemaphoreGiveRecursive(mutex);
}

/**
 * @brief Sleep thread itself.
 *
 * @param [in] ms @n the time interval for which execution is to be suspended, in milliseconds.
 * @return None.
 * @see None.
 * @note None.
 */
void HAL_SleepMs(uint32_t ms)
{
    vTaskDelay((TickType_t)(ms / portTICK_RATE_MS));
}

/**
 * @brief Retrieves the number of milliseconds that have elapsed since the system was boot.
 *
 * @return the number of milliseconds.
 * @see None.
 * @note None.
 */
uint64_t HAL_UptimeMs(void)
{
	return (uint64_t)xTaskGetTickCount();
}
static uint64_t current_utc_time_ms = 0;
static uint32_t last_utc_time_reference_ms = 0;
static long long os_time_get(void)
{
	link_printf(LINK_INFO, "[%s]\n", __FUNCTION__);
	long long ms;
	uint32_t current_time = xTaskGetTickCount() * (portTICK_RATE_MS);
	int32_t diff_time = current_time -	last_utc_time_reference_ms;
	if( diff_time > 0 )
	{
		current_utc_time_ms += diff_time;
		last_utc_time_reference_ms = current_time;
	}
	ms = current_utc_time_ms/1000;
    return ms;
}

static long long delta_time = 0;
void HAL_UTC_Set(long long ms)
{
    delta_time = ms - os_time_get();
	
}

long long HAL_UTC_Get(void)
{
    return delta_time + os_time_get();
}


void HAL_Reboot()
{
	link_printf(LINK_INFO, "Reboot linkkit !!!\n");
	sys_reset();
	while(1) HAL_SleepMs(1000);
}


/*
 * @brief Writes formatted data to stream.
 *
 * @param [in] fmt: @n String that contains the text to be written, it can optionally contain embedded format specifiers
     that specifies how subsequent arguments are converted for output.
 * @param [in] ...: @n the variable argument list, for formatted and inserted in the resulting string replacing their respective specifiers.
 * @return None.
 * @see None.
 * @note None.
 */

void HAL_Printf(const char *fmt, ...)
{
	va_list args;
    va_start(args, fmt);
    vprintf(fmt,args);
    va_end(args);
	fflush(stdout);
}


/**
 * @brief Writes formatted data to string.
 *
 * @param [out] str: @n String that holds written text.
 * @param [in] len: @n Maximum length of character will be written
 * @param [in] fmt: @n Format that contains the text to be written, it can optionally contain embedded format specifiers
     that specifies how subsequent arguments are converted for output.
 * @param [in] ...: @n the variable argument list, for formatted and inserted in the resulting string replacing their respective specifiers.
 * @return bytes of character successfully written into string.
 * @see None.
 * @note None.
 */
int HAL_Snprintf(char *str, const int len, const char *fmt, ...)
{
    int ret;
    va_list args;
	va_start(args, fmt);
    ret = vsnprintf(str, len - 1, fmt, args);
	va_end(args);
    return ret;

}


int HAL_Vsnprintf(char *str, const int len, const char *format, va_list ap)
{
	int ret;
    ret = vsnprintf(str, len - 1, format, ap);
    return ret;
}

void print_ip_addr(struct sockaddr_in * addr,uintptr_t fd)
{
	ip_addr_t remote_ipaddr;
	const struct sockaddr_in * name_in = (const struct sockaddr_in *)(void*)addr;

	inet_addr_to_ip4addr(&remote_ipaddr, &name_in->sin_addr);
	link_printf(LINK_INFO,"connect(%d, addr=%d.%d.%d.%d)\n", fd,             \
                      &remote_ipaddr != NULL ? ip4_addr1_16(&remote_ipaddr) : 0,       \
                      &remote_ipaddr != NULL ? ip4_addr2_16(&remote_ipaddr) : 0,       \
                      &remote_ipaddr != NULL ? ip4_addr3_16(&remote_ipaddr) : 0,       \
                      &remote_ipaddr != NULL ? ip4_addr4_16(&remote_ipaddr) : 0);

}

uint32_t HAL_Random(uint32_t region)
{
	return region>0?rand()%region:0;
}



void HAL_Srandom(uint32_t seed)
{
     srand(seed);
}



/**
 * @brief   create a semaphore
 *
 * @return semaphore handle.
 * @see None.
 * @note The recommended value of maximum count of the semaphore is 255.
 */
void *HAL_SemaphoreCreate(void)
{
	return xSemaphoreCreateCounting(0xff, 0);	//Set max count 0xff;
}



/**
 * @brief   destory a semaphore
 *
 * @param[in] sem @n the specified sem.
 * @return None.
 * @see None.
 * @note None.
 */
void HAL_SemaphoreDestroy(void *sem)
{
   if(sem != NULL){
		vSemaphoreDelete(sem);
   	}
   sem = NULL;
}


/**
 * @brief   signal thread wait on a semaphore
 *
 * @param[in] sem @n the specified semaphore.
 * @return None.
 * @see None.
 * @note None.
 */
void HAL_SemaphorePost(void *sem)
{
	xSemaphoreGive(sem);
}



/**
 * @brief   wait on a semaphore
 *
 * @param[in] sem @n the specified semaphore.
 * @param[in] timeout_ms @n timeout interval in millisecond.
     If timeout_ms is PLATFORM_WAIT_INFINITE, the function will return only when the semaphore is signaled.
 * @return
   @verbatim
   =  0: The state of the specified object is signaled.
   =  -1: The time-out interval elapsed, and the object's state is nonsignaled.
   @endverbatim
 * @see None.
 * @note None.
 */
int HAL_SemaphoreWait(void *sem, uint32_t timeout_ms)
{
	
	if (sem == NULL) {
		link_err_printf("[%s] , semaphore = NULL.\n", pcTaskGetTaskName(NULL));
		return -1;
	}
	if (PLATFORM_WAIT_INFINITE == timeout_ms) {
		while(xSemaphoreTake(sem, portMAX_DELAY) != pdTRUE) {
			link_err_printf("[%s] , (%p) failed, retry\n", pcTaskGetTaskName(NULL), sem);
			return -1;
		}
	} else {
		while(xSemaphoreTake(sem, timeout_ms / portTICK_RATE_MS) != pdTRUE) {
			link_printf(LINK_DEBUG, "[%s], timeout: %d ms.\n", pcTaskGetTaskName(NULL), timeout_ms);
			return -1;
		}
	}
	return 0;
}

/**
 * @brief  create a thread
 *
 * @param[out] thread_handle @n The new thread handle, memory allocated before thread created and return it, free it after thread joined or exit.
 * @param[in] start_routine @n A pointer to the application-defined function to be executed by the thread.
        This pointer represents the starting address of the thread.
 * @param[in] arg @n A pointer to a variable to be passed to the start_routine.
 * @param[in] hal_os_thread_param @n A pointer to stack params.
 * @param[out] stack_used @n if platform used stack buffer, set stack_used to 1, otherwise set it to 0.
 * @return
   @verbatim
     = 0: on success.
     = -1: error occur.
   @endverbatim
 * @see None.
 * @note None.
 */
int HAL_ThreadCreate(
void **thread_handle,
void *(*work_routine)(void *),
void *arg,
hal_os_thread_param_t *hal_os_thread_param,
int *stack_used)
{
	portBASE_TYPE xReturn;
	
	if (work_routine != NULL) {
		xReturn = xTaskCreate((TaskFunction_t)work_routine, hal_os_thread_param->name, (hal_os_thread_param->stack_size)/4, arg, tskIDLE_PRIORITY + 5, thread_handle);
	}
	
	*stack_used = 0;
	link_printf(LINK_INFO, "name:%s, stack_size=%d, handle=%p\n", hal_os_thread_param->name, hal_os_thread_param->stack_size/4, thread_handle);
	link_printf(LINK_INFO, "after create task heap size:%d\n", HAL_get_free_heap_size());
	
	return ((xReturn == pdPASS) ? 0 : -1);
}

void HAL_ThreadDelete(void *thread_handle)
{
	link_printf(LINK_INFO, "thread delete. \n");
	vTaskDelete(thread_handle);
}

void *HAL_Timer_Create(const char *name, void (*func)(void *), void *user_data)
{
   link_printf(LINK_INFO,"create timer:%s.\n",name); 
   return xTimerCreate((const char *)name,TIMER_PERIOD, pdFALSE, user_data, (TimerCallbackFunction_t)func);	
}


int HAL_Timer_Delete(void *timer)
{
	return (int)xTimerDelete(timer, 0);
}


int HAL_Timer_Start(void *timer, int ms)
{
	link_printf(LINK_DEBUG,"timer start.\n");
	
	xTimerChangePeriod(timer, ms / portTICK_PERIOD_MS, 0);
		
	return (int)xTimerStart(timer, 0);
}


int HAL_Timer_Stop(void *timer)
{
	link_printf(LINK_DEBUG,"timer stop.\n");
	return (int)xTimerStop(timer, 0);	
}
								/*************product hal*******************/

/*
 * @return	 status: Success:1 or Failure: Others.
 */
int  linkkit_flash_stream_read(uint32_t address, uint32_t len, uint8_t * data)
{
	flash_t flash;
	int ret;

	device_mutex_lock(RT_DEV_LOCK_FLASH);
	ret = flash_stream_read(&flash, address, len, data);
	device_mutex_unlock(RT_DEV_LOCK_FLASH);

	return ret;
}

/*
 * @return	 status: Success:1 or Failure: Others.
 */
int  linkkit_flash_stream_write(uint32_t address, uint32_t len, uint8_t * data)
{
	flash_t flash;
	int ret;
	
	device_mutex_lock(RT_DEV_LOCK_FLASH);
	ret = flash_stream_write(&flash, address, len, data);
	device_mutex_unlock(RT_DEV_LOCK_FLASH);

	return ret;
}

/*
 * @return	 none.
 */
void  linkkit_flash_erase_sector(uint32_t address)
{
	flash_t flash;
	
	device_mutex_lock(RT_DEV_LOCK_FLASH);
	flash_erase_sector(&flash, address);
	device_mutex_unlock(RT_DEV_LOCK_FLASH);

	return;
}


int rtl_erase_wifi_config_data()
{
	LwIP_DHCP(0, DHCP_RELEASE_IP);
	LwIP_DHCP(0, DHCP_STOP);
	vTaskDelay(10);
	fATWD(NULL);
	vTaskDelay(20);
	linkkit_flash_erase_sector(FAST_RECONNECT_DATA);
	link_printf(LINK_DEBUG, "%s: clean ap-config data",__FUNCTION__ );
	return 0;
}

void linkkit_erase_wifi_config()
{
	//Erase link config flash
	rtl_erase_wifi_config_data();
	HAL_Reboot();
}

void linkkit_erase_fw_config()
{
	//Erase link config flash
	vTaskDelay(20);
	linkkit_flash_erase_sector(CONFIG_FW_INFO);
	HAL_Reboot();
}



/* write device key/secret into flash
 * argv[1] = "key" or "secret";
 * argv[2] = value;
 * write flash addr = CONFIG_DEVICE_INFO
 * @return	 status: Success:0 or Failure: -1.
 */
int  linkkit_write_device_info(int argc, unsigned char **argv)
{
	if (!strcmp(argv[1], "pk")) {
		if(strlen(argv[2]) >= PRODUCT_KEY_LEN+1) {
			link_printf(LINK_WARNING, "\"key\" must be 20-byte long\n");
			return -1;
		}
		linkkit_flash_erase_sector(CONFIG_DEVICE_INFO);
		//sizeof(secret) == 20B
		if (1 == linkkit_flash_stream_write(CONFIG_DEVICE_INFO, PRODUCT_KEY_LEN+1, argv[2])) {
			link_printf(LINK_INFO, "write \"key\" successfully\n"); 
		} else {
			link_printf(LINK_WARNING, "write \"key\" fail\n");
			return -1;
		}
	} else if(!strcmp(argv[1], "ps")) {
		if(strlen(argv[2]) >= PRODUCT_SECRET_LEN+1) {
			link_printf(LINK_WARNING, "\"secret\" must be 64-byte long\n");
			return -1;
		}
		if(1 == linkkit_flash_stream_write(CONFIG_DEVICE_INFO + PRODUCT_KEY_LEN+1, PRODUCT_SECRET_LEN+1, argv[2])) {
			link_printf(LINK_INFO, "write \"secret\" successfully\n");	
		} else {
			link_printf(LINK_WARNING, "write \"secret\" fail\nl");
			return -1;
		}
	} else if(!strcmp(argv[1], "dn")) {
		if(strlen(argv[2]) >= DEVICE_NAME_LEN+1) {
			link_printf(LINK_WARNING, "\"device name\" must less than 32-byte long\n");
			return -1;
		}
		if(1 == linkkit_flash_stream_write(CONFIG_DEVICE_INFO + PRODUCT_KEY_LEN+PRODUCT_SECRET_LEN+2, DEVICE_NAME_LEN+1, argv[2])) {
			link_printf(LINK_INFO, "write \"device name\" successfully\n"); 
		} else {
			link_printf(LINK_WARNING, "write \"device name\" fail\nl");
			return -1;
		}
	} else if(!strcmp(argv[1], "ds")) {
		if(strlen(argv[2]) >= DEVICE_SECRET_LEN+1) {
			link_printf(LINK_WARNING, "\"secret\" must less than 64-byte long\n");
			return -1;
		}
		if(1 == linkkit_flash_stream_write(CONFIG_DEVICE_INFO + PRODUCT_KEY_LEN+PRODUCT_SECRET_LEN+DEVICE_NAME_LEN+3, DEVICE_SECRET_LEN+1, argv[2])) {
			link_printf(LINK_INFO, "write \"secret\" successfully\n");	
		} else {
			link_printf(LINK_WARNING, "write \"secret\" fail\nl");
			return -1;
		}
	}else if(!strcmp(argv[1], "fw")){
		if(strlen(argv[2]) >= IOTX_FIRMWARE_VER_LEN+1) {
			link_printf(LINK_WARNING, "\"fw version\" must less than 32-byte long\n");
			return -1;
		}		
		//write new firmware version to flash
		if (1 == linkkit_flash_stream_write(CONFIG_FW_INFO, IOTX_FIRMWARE_VER_LEN+1, argv[2])) {
			link_printf(LINK_INFO, "write fw ver successfully\n");	
		} else {
			link_printf(LINK_WARNING, "write fw ver fail\n");
		}

	
	}else {
		link_printf(LINK_WARNING, "wrong operation\n");
		return -1;
	}
	
	return 0;
}

/**
 * @brief Get product key from user's system persistent storage
 *
 * @param [ou] product_key: array to store product key, max length is IOTX_PRODUCT_KEY_LEN
 * @return	the actual length of product key
 */
int HAL_GetProductKey(char *product_key)
{
#if _DEMO_
    strncpy(product_key, _product_key, PRODUCT_KEY_LEN);
#else	
	linkkit_flash_stream_read(CONFIG_DEVICE_INFO, PRODUCT_KEY_LEN+1, product_key);
	if(*((uint32_t *)product_key) == ~0x0) {		
		link_printf(LINK_INFO,"Device has no product key, please write them by 'ATCA' and then reset.\n");
		return -1;
	}

#endif	
    return strlen(product_key);

}


int HAL_GetProductSecret(char *product_secret)
{
#if _DEMO_
	strncpy(product_secret, _product_secret, PRODUCT_SECRET_LEN);
#else		
	linkkit_flash_stream_read(CONFIG_DEVICE_INFO+PRODUCT_KEY_LEN+1, PRODUCT_SECRET_LEN+1, product_secret);
	if(*((uint32_t *)product_secret) == ~0x0) {		
		link_printf(LINK_INFO,"Device has no product secret, please write them by 'ATCA' and then reset.\n");
		return -1;
	}
#endif	
	return strlen(product_secret);
}


/**
 * @brief Get device name from user's system persistent storage
 *
 * @param [ou] device_name: array to store device name, max length is IOTX_DEVICE_NAME_LEN
 * @return the actual length of device name
 */
int HAL_GetDeviceName(char *device_name)
{
#if _DEMO_
    strncpy(device_name, _device_name, DEVICE_NAME_LEN);
#else	
	linkkit_flash_stream_read(CONFIG_DEVICE_INFO+PRODUCT_KEY_LEN+PRODUCT_SECRET_LEN+2, DEVICE_NAME_LEN+1, device_name);
	if(*((uint32_t *)device_name) == ~0x0) {		
		link_printf(LINK_INFO,"Device has no device name, please write them by 'ATCA' and then reset.\n");
		return -1;
	}
#endif	
    return strlen(device_name);

}

/**
 * @brief Get device secret from user's system persistent storage
 *
 * @param [ou] device_secret: array to store device secret, max length is IOTX_DEVICE_SECRET_LEN
 * @return the actual length of device secret
 */
int HAL_GetDeviceSecret(char *device_secret)
{
#if _DEMO_
	strncpy(device_secret, _device_secret, DEVICE_SECRET_LEN);
#else	
	
	linkkit_flash_stream_read(CONFIG_DEVICE_INFO+PRODUCT_KEY_LEN+PRODUCT_SECRET_LEN+DEVICE_NAME_LEN+3, DEVICE_SECRET_LEN+1, device_secret);
	if(*((uint32_t *)device_secret) == ~0x0) {		
		link_printf(LINK_INFO,"Device has no device secret, please write them by 'ATCA' and then reset.\n");
		return -1;
	}
#endif	
    return strlen(device_secret);
}


int HAL_SetDeviceName(char *device_name)
{
	//HAL_GetDeviceName(_device_name);

	int len = strlen(device_name);

	if (len > DEVICE_NAME_LEN) {
		return -1;
	}
	strncpy(_device_name, device_name, DEVICE_NAME_LEN + 1);
	return len;
}


int HAL_SetDeviceSecret(char *device_secret)
{
   //HAL_GetDeviceSecret(_device_secret);
	int len = strlen(device_secret);

	if (len > DEVICE_SECRET_LEN) {
		return -1;
	}
	strncpy(_device_secret, device_secret, DEVICE_SECRET_LEN + 1);

	return len;
}


int HAL_SetProductKey(char *product_key)
{
	//HAL_GetProductKey(_product_key);

	int len = strlen(product_key);
	if (len > PRODUCT_KEY_LEN) {
        return -1;
    }
	strncpy(_product_key, product_key, PRODUCT_KEY_LEN + 1);
	return len;
}


int HAL_SetProductSecret(char *product_secret)
{
	//HAL_GetProductSecret(_product_secret);

	int len = strlen(product_secret);
	if (len > PRODUCT_SECRET_LEN) {
        return -1;
    }
	strncpy(_product_secret, product_secret, PRODUCT_SECRET_LEN + 1);
	return len;
}


/**
 * @brief Get firmware version
 *
 * @param [ou] version: array to store firmware version, max length is IOTX_FIRMWARE_VER_LEN
 * @return the actual length of firmware version
 */
int HAL_GetFirmwareVersion(char *version)
{
#if _DEMO_
	strncpy(version, _firmware_version, FIRMWARE_VERSION_MAXLEN);
	version[FIRMWARE_VERSION_MAXLEN-1] = '\0';
#else	
	linkkit_flash_stream_read(CONFIG_FW_INFO, IOTX_FIRMWARE_VER_LEN+1, version);
	if(*((uint32_t *)version) == ~0x0) {
		link_printf(LINK_INFO,"Device has no firmware version, please write them by 'ATCA' and then reset.\n");
		return -1;
	}
	//version[FIRMWARE_VERSION_MAXLEN-1] = '\0';
#endif
	return strlen(version);

}

int HAL_GetModuleID(char *mid_str)
{
	memset(mid_str, 0x0, MID_STRLEN_MAX);

	strcpy(mid_str, "example.demo.module-id");

	return strlen(mid_str);
}


int HAL_GetPartnerID(char *pid_str)
{
	memset(pid_str, 0x0, PID_STRLEN_MAX);

	strcpy(pid_str, "example.demo.partner-id");

	return strlen(pid_str);
}

int HAL_GetDeviceID(_OU_ char *device_id)
{
	HAL_GetProductSecret(_product_secret);	
	HAL_GetDeviceName(_device_name);

	memset(device_id, 0x0, DEVICE_ID_LEN);

	HAL_Snprintf(device_id, DEVICE_ID_LEN, "%s.%s", _product_key, _device_name);
	device_id[DEVICE_ID_LEN - 1] = '\0';

	return strlen(device_id);
}

int HAL_GetNetifInfo(char *nif_str)
{
	memset(nif_str, 0x0, NIF_STRLEN_MAX);

	/* if the device have only WIFI, then list as follow, note that the len MUST NOT exceed NIF_STRLEN_MAX */
	const char *net_info = "WiFi|03ACDEFF0032";
	strncpy(nif_str, net_info, strlen(net_info));
	/* if the device have ETH, WIFI, GSM connections, then list all of them as follow, note that the len MUST NOT exceed NIF_STRLEN_MAX */
	// const char *multi_net_info = "ETH|0123456789abcde|WiFi|03ACDEFF0032|Cellular|imei_0123456789abcde|iccid_0123456789abcdef01234|imsi_0123456789abcde|msisdn_86123456789ab");
	// strncpy(nif_str, multi_net_info, strlen(multi_net_info));

	return strlen(nif_str);
}

                                            /*************network hal*******************/

#if CONFIG_USE_MBEDTLS

static ssl_hooks_t g_ssl_hooks = {HAL_Malloc, HAL_Free};
typedef struct {
    mbedtls_aes_context ctx;
    uint8_t iv[16];
    uint8_t key[16];
} platform_aes_t;

#define TLS_AUTH_MODE_CA        0
#define TLS_AUTH_MODE_PSK       1

/* config authentication mode */
#ifndef TLS_AUTH_MODE
#define TLS_AUTH_MODE           TLS_AUTH_MODE_CA
#endif


static void *_SSLCalloc_wrapper(size_t nelements, size_t elementSize)
{
	unsigned char *buf = NULL;
    mbedtls_mem_info_t *mem_info = NULL;

    if (nelements == 0 || elementSize == 0) {
        return NULL;
    }

    buf = (unsigned char *)(HAL_Malloc(nelements * elementSize + sizeof(mbedtls_mem_info_t)));
    if (NULL == buf) {
        return NULL;
    } else {
        memset(buf, 0, nelements * elementSize + sizeof(mbedtls_mem_info_t));
    }

    mem_info = (mbedtls_mem_info_t *)buf;
    mem_info->magic = MBEDTLS_MEM_INFO_MAGIC;
    mem_info->size = nelements * elementSize;
    buf += sizeof(mbedtls_mem_info_t);

    mbedtls_mem_used += mem_info->size;
    if (mbedtls_mem_used > mbedtls_max_mem_used) {
        mbedtls_max_mem_used = mbedtls_mem_used;
    }

    return buf;

}

//#define _SSLFree_wrapper		vPortFree

void _SSLFree_wrapper(void *ptr)
{
    mbedtls_mem_info_t *mem_info = NULL;
    if (NULL == ptr) {
        return;
    }

    mem_info = (mbedtls_mem_info_t *)((unsigned char *)ptr - sizeof(mbedtls_mem_info_t));
    if (mem_info->magic != MBEDTLS_MEM_INFO_MAGIC) {
        link_printf(LINK_INFO,"Warning - invalid mem info magic: 0x%x\r\n", mem_info->magic);
        return;
    }

    mbedtls_mem_used -= mem_info->size;
    HAL_Free(mem_info);
}


void *init_mbedtls_aes_content()
{
	mbedtls_aes_context *aes_ctx = NULL;
	aes_ctx = HAL_Malloc(sizeof(mbedtls_aes_context));
	if(aes_ctx == NULL){
		link_err_printf("malloc ctx failed\n");
		return  NULL;
	}
	return (void *)aes_ctx;
}



void linkkit_mbedtls_aes_init( void *ctx )
{
	/*move the cryptoEngine from main funtion to here*/
	if ( rtl_cryptoEngine_init() != 0 ) {
	    HAL_Printf("crypto engine init failed\r\n");
	}
	mbedtls_platform_set_calloc_free(_SSLCalloc_wrapper, _SSLFree_wrapper);
	mbedtls_aes_init((mbedtls_aes_context *)ctx);
}


int HAL_mbedtls_aes_crypt_cbc( void *ctx,
                    int mode,
                    size_t length,
                    unsigned char iv[16],
                    const unsigned char *input,
                    unsigned char *output )
{
	int ret;
	device_mutex_lock(RT_DEV_LOCK_CRYPTO);
	ret = mbedtls_aes_crypt_cbc((mbedtls_aes_context *)ctx, mode, length, iv, input, output);
	device_mutex_unlock(RT_DEV_LOCK_CRYPTO);
	return ret;
}


int HAL_Aes128_Cbc_Decrypt(
            p_HAL_Aes128_t aes,
            const void *src,
            size_t blockNum,
            void *dst)
{
	link_printf(LINK_INFO, "[%s] blockNum: %d\n", __FUNCTION__, blockNum);
    int ret = -1;
    platform_aes_t *p_aes128 = (platform_aes_t *)aes;

    if (!aes || !src || !dst) return ret;
	
	HAL_mbedtls_aes_crypt_cbc(&p_aes128->ctx, 0, 16*blockNum, p_aes128->iv, src, dst);

	return 0;

}

int HAL_Aes128_Cbc_Encrypt(
            p_HAL_Aes128_t aes,
            const void *src,
            size_t blockNum,
            void *dst)
{
	link_printf(LINK_INFO, "[%s]\n", __FUNCTION__);
	int ret = -1;
    platform_aes_t *p_aes128 = (platform_aes_t *)aes;

    if (!aes || !src || !dst) return ret;
	
	HAL_mbedtls_aes_crypt_cbc(&p_aes128->ctx, 1, 16*blockNum, p_aes128->iv, src, dst);

	return 0;

}

int HAL_mbedtls_aes_crypt_cfb( void *ctx,
                    int mode,
                    size_t length,
                    size_t *iv_off,
                    unsigned char iv[16],
                    const unsigned char *input,
                    unsigned char *output )
{
	int ret;
	ret = mbedtls_aes_crypt_cfb128((mbedtls_aes_context *)ctx, mode, length,iv_off ,iv, input, output);
	return ret;
}

int HAL_Aes128_Cfb_Decrypt(
            p_HAL_Aes128_t aes,
            const void *src,
            size_t blockNum,
            void *dst)
{
	link_printf(LINK_INFO, "[%s] blockNum: %d\n", __FUNCTION__, blockNum);
    size_t offset=0;
	int ret=-1;
	platform_aes_t *p_aes128 = (platform_aes_t *)aes;
	if(!aes||!src||!dst){
		return ret;
	}
	ret = mbedtls_aes_setkey_enc(&p_aes128->ctx, p_aes128->key, 128);
    ret=HAL_mbedtls_aes_crypt_cfb(&p_aes128->ctx, 0, blockNum,&offset, p_aes128->iv, src,dst);
	return ret;

}

int HAL_Aes128_Cfb_Encrypt(
            p_HAL_Aes128_t aes,
            const void *src,
            size_t blockNum,
            void *dst)
{
	link_printf(LINK_INFO, "[%s] blockNum: %d\n", __FUNCTION__, blockNum);
    size_t offset=0;
	int ret=-1;
	platform_aes_t *p_aes128 = (platform_aes_t *)aes;
	if(!aes||!src||!dst){
		return ret;
	}
	ret=HAL_mbedtls_aes_crypt_cfb(&p_aes128->ctx,1,blockNum,&offset,p_aes128->iv, src,dst);
	return ret;

}



int HAL_Aes128_Destroy(p_HAL_Aes128_t aes)
{
    link_printf(LINK_INFO, "[%s]\n", __FUNCTION__);
	if(!aes)
		return -1;

	mbedtls_aes_free(&((platform_aes_t *)aes)->ctx);
	HAL_Free(aes);
	return 0;
}

p_HAL_Aes128_t HAL_Aes128_Init(
            _IN_ const uint8_t *key,
            _IN_ const uint8_t *iv,
            _IN_ AES_DIR_t dir)
{
    int ret = 0;
    platform_aes_t *p_aes128 = NULL;
	
	link_printf(LINK_INFO, "[%s]start initialize\n", __FUNCTION__);

    if (!key || !iv) return p_aes128;

    p_aes128 = (platform_aes_t *)calloc(1, sizeof(platform_aes_t));
    if (!p_aes128) return p_aes128;

    mbedtls_aes_init(&p_aes128->ctx);

    if (dir == HAL_AES_ENCRYPTION) {
        ret = mbedtls_aes_setkey_enc(&p_aes128->ctx, key, 128);
    } else {
        ret = mbedtls_aes_setkey_dec(&p_aes128->ctx, key, 128);
    }

    if (ret == 0) {
        memcpy(p_aes128->iv, iv, 16);
        memcpy(p_aes128->key, key, 16);
    } else {
        free(p_aes128);
        p_aes128 = NULL;
    }

    return (p_HAL_Aes128_t *)p_aes128;
}



static unsigned int _avRandom()
{
    return (((unsigned int)rand() << 16) + rand());
}


static int _ssl_random(void *p_rng, unsigned char *output, size_t output_len)
{
    uint32_t rnglen = output_len;
    uint8_t   rngoffset = 0;

    while (rnglen > 0) {
        *(output + rngoffset) = (unsigned char)_avRandom() ;
        rngoffset++;
        rnglen--;
    }
	return 0;
}

static void _ssl_debug(void *ctx, int level, const char *file, int line, const char *str)
{
    ((void) level);
    if (NULL != ctx) {
#if 0
        fprintf((FILE *) ctx, "%s:%04d: %s", file, line, str);
        fflush((FILE *) ctx);
#endif
        link_printf(LINK_INFO,"%s\n", str);
    }
}

static int _real_confirm(int verify_result)
{
    link_printf(LINK_INFO,"certificate verification result: 0x%02x\n", verify_result);

#if defined(FORCE_SSL_VERIFY)
    if ((verify_result & MBEDTLS_X509_BADCERT_EXPIRED) != 0) {
        link_err_printf("! fail ! ERROR_CERTIFICATE_EXPIRED\n");
        return -1;
    }

    if ((verify_result & MBEDTLS_X509_BADCERT_REVOKED) != 0) {
        link_err_printf("! fail ! server certificate has been revoked\n");
        return -1;
    }

    if ((verify_result & MBEDTLS_X509_BADCERT_CN_MISMATCH) != 0) {
        link_err_printf("! fail ! CN mismatch\n");
        return -1;
    }

    if ((verify_result & MBEDTLS_X509_BADCERT_NOT_TRUSTED) != 0) {
        link_err_printf("! fail ! self-signed or not signed by a trusted CA\n");
        return -1;
    }
#endif

    return 0;
}

static void _network_ssl_disconnect(TLSDataParams_t *pTlsData)
{
    mbedtls_ssl_close_notify(&(pTlsData->ssl));
    mbedtls_net_free(&(pTlsData->fd));
#if defined(MBEDTLS_X509_CRT_PARSE_C)
    mbedtls_x509_crt_free(&(pTlsData->cacertl));
    if ((pTlsData->pkey).pk_info != NULL) {
        link_err_printf("need release client crt&key\n");
#if defined(MBEDTLS_CERTS_C)
        mbedtls_x509_crt_free(&(pTlsData->clicert));
        mbedtls_pk_free(&(pTlsData->pkey));
#endif
    }
#endif
    mbedtls_ssl_free(&(pTlsData->ssl));
    mbedtls_ssl_config_free(&(pTlsData->conf));
    link_printf(LINK_INFO,"ssl_disconnect\n");
}

static int _network_ssl_read(TLSDataParams_t *pTlsData, char *buffer, int len, int timeout_ms)
{
    uint32_t        readLen = 0;
    static int      net_status = 0;
    int             ret = -1;
    char            err_str[33];

    mbedtls_ssl_conf_read_timeout(&(pTlsData->conf), timeout_ms);
    while (readLen < len) {
        ret = mbedtls_ssl_read(&(pTlsData->ssl), (unsigned char *)(buffer + readLen), (len - readLen));
        if (ret > 0) {
            readLen += ret;
            net_status = 0;
        } else if (ret == 0) {
            /* if ret is 0 and net_status is -2, indicate the connection is closed during last call */
            return (net_status == -2) ? net_status : readLen;
        } else {
            if (MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY == ret) {
                mbedtls_strerror(ret, err_str, sizeof(err_str));
                link_err_printf("ssl recv error: code = %d, err_str = '%s'\n", ret, err_str);
                net_status = -2; /* connection is closed */
                break;
            } else if ((MBEDTLS_ERR_SSL_TIMEOUT == ret)
                       || (MBEDTLS_ERR_SSL_CONN_EOF == ret)
                       || (MBEDTLS_ERR_SSL_SESSION_TICKET_EXPIRED == ret)
                       || (MBEDTLS_ERR_SSL_NON_FATAL == ret)) {
                /* read already complete */
                /* if call mbedtls_ssl_read again, it will return 0 (means EOF) */

                return readLen;
            } else {
                mbedtls_strerror(ret, err_str, sizeof(err_str));
                link_err_printf("ssl recv error: code = %d, err_str = '%s'\n", ret, err_str);
                net_status = -1;
                return -1; /* Connection error */
            }
        }
    }

    return (readLen > 0) ? readLen : net_status;
}


static int _network_ssl_write(TLSDataParams_t *pTlsData, const char *buffer, int len, int timeout_ms)
{
    uint32_t writtenLen = 0;
    int ret = -1;

    if (pTlsData == NULL) {
        return -1;
    }

    while (writtenLen < len) {
        ret = mbedtls_ssl_write(&(pTlsData->ssl), (unsigned char *)(buffer + writtenLen), (len - writtenLen));
        if (ret > 0) {
            writtenLen += ret;
            continue;
        } else if (ret == 0) {
            link_err_printf("ssl write timeout\n");
            return 0;
        } else {
            char err_str[33];
            mbedtls_strerror(ret, err_str, sizeof(err_str));
            link_err_printf("ssl write fail, code=%d, str=%s\n", ret, err_str);
            return -1; /* Connnection error */
        }
    }

    return writtenLen;
}


static int _ssl_client_init(mbedtls_ssl_context *ssl,
                            mbedtls_net_context *tcp_fd,
                            mbedtls_ssl_config *conf,
                            mbedtls_x509_crt *crt509_ca, const char *ca_crt, size_t ca_len,
                            mbedtls_x509_crt *crt509_cli, const char *cli_crt, size_t cli_len,
                            mbedtls_pk_context *pk_cli, const char *cli_key, size_t key_len,  const char *cli_pwd, size_t pwd_len
                           )
{
    int ret = -1;
    /*
     * 0. Initialize the RNG and the session data
     */
#if defined(MBEDTLS_DEBUG_C)
    mbedtls_debug_set_threshold(0);
#endif
    mbedtls_net_init(tcp_fd);
    mbedtls_ssl_init(ssl);
    mbedtls_ssl_config_init(conf);
    mbedtls_x509_crt_init(crt509_ca);

    /*verify_source->trusted_ca_crt==NULL
     * 0. Initialize certificates
     */

    link_printf(LINK_INFO,"Loading the CA root certificate ...\n");
    if (NULL != ca_crt) {
		
		link_printf(LINK_DEBUG,"ca_crt:%s\n",ca_crt);
        if (0 != (ret = mbedtls_x509_crt_parse(crt509_ca, (const unsigned char *)ca_crt, ca_len))) {
            link_err_printf(" failed ! x509parse_crt returned -0x%04x\n", -ret);
            return ret;
        }
    }
    link_printf(LINK_INFO," ok (%d skipped)\n", ret);


    /* Setup Client Cert/Key */
#if defined(MBEDTLS_X509_CRT_PARSE_C)
#if defined(MBEDTLS_CERTS_C)
    mbedtls_x509_crt_init(crt509_cli);
    mbedtls_pk_init(pk_cli);
#endif
    if (cli_crt != NULL && cli_key != NULL) {
#if defined(MBEDTLS_CERTS_C)
        link_printf(LINK_INFO,"start prepare client cert .\n");
        ret = mbedtls_x509_crt_parse(crt509_cli, (const unsigned char *) cli_crt, cli_len);
#else
        {
            ret = 1;
            link_err_printf("MBEDTLS_CERTS_C not defined.\n");
        }
#endif
        if (ret != 0) {
            link_err_printf(" failed!  mbedtls_x509_crt_parse returned -0x%x\n", -ret);
            return ret;
        }

#if defined(MBEDTLS_CERTS_C)
        link_printf(LINK_INFO,"start mbedtls_pk_parse_key[%s]\n", cli_pwd);
        ret = mbedtls_pk_parse_key(pk_cli, (const unsigned char *) cli_key, key_len, (const unsigned char *) cli_pwd, pwd_len);
#else
        {
            ret = 1;
            link_err_printf("MBEDTLS_CERTS_C not defined.\n");
        }
#endif

        if (ret != 0) {
            link_err_printf(" failed\n  !  mbedtls_pk_parse_key returned -0x%x\n", -ret);
            return ret;
        }
    }
#endif /* MBEDTLS_X509_CRT_PARSE_C */
    return 0;
}



/**
 * @brief This function connects to the specific SSL server with TLS, and returns a value that indicates whether the connection is create successfully or not. Call #NewNetwork() to initialize network structure before calling this function.
 * @param[in] n is the the network structure pointer.
 * @param[in] addr is the Server Host name or IP address.
 * @param[in] port is the Server Port.
 * @param[in] ca_crt is the Server's CA certification.
 * @param[in] ca_crt_len is the length of Server's CA certification.
 * @param[in] client_crt is the client certification.
 * @param[in] client_crt_len is the length of client certification.
 * @param[in] client_key is the client key.
 * @param[in] client_key_len is the length of client key.
 * @param[in] client_pwd is the password of client key.
 * @param[in] client_pwd_len is the length of client key's password.
 * @sa #NewNetwork();
 * @return If the return value is 0, the connection is created successfully. If the return value is -1, then calling lwIP #socket() has failed. If the return value is -2, then calling lwIP #connect() has failed. Any other value indicates that calling lwIP #getaddrinfo() has failed.
 */

static int _TLSConnectNetwork(TLSDataParams_t *pTlsData, const char *addr, const char *port,
                              const char *ca_crt, size_t ca_crt_len,
                              const char *client_crt,   size_t client_crt_len,
                              const char *client_key,   size_t client_key_len,
                              const char *client_pwd, size_t client_pwd_len)
{
    int ret = -1;
    /*
     * 1. Init
     */
    if (0 != (ret = _ssl_client_init(&(pTlsData->ssl), &(pTlsData->fd), &(pTlsData->conf),
                                     &(pTlsData->cacertl), ca_crt, ca_crt_len,
                                     &(pTlsData->clicert), client_crt, client_crt_len,
                                     &(pTlsData->pkey), client_key, client_key_len, client_pwd, client_pwd_len))) {
        link_printf(LINK_INFO," failed ! ssl_client_init returned -0x%04x\n", -ret);
        return ret;
    }

    /*
     * 2. Start the connection
     */
    link_printf(LINK_INFO,"Connecting to /%s/%s...\n", addr, port);
    if (0 != (ret = mbedtls_net_connect(&(pTlsData->fd), addr, port, MBEDTLS_NET_PROTO_TCP))) {
        link_printf(LINK_INFO," failed ! net_connect returned -0x%04x\n", -ret);
        return ret;
    }
    link_printf(LINK_INFO," ok\n");

    /*
     * 3. Setup stuff
     */
    link_printf(LINK_INFO,"  . Setting up the SSL/TLS structure...\n");
    if ((ret = mbedtls_ssl_config_defaults(&(pTlsData->conf), MBEDTLS_SSL_IS_CLIENT, MBEDTLS_SSL_TRANSPORT_STREAM,
                                           MBEDTLS_SSL_PRESET_DEFAULT)) != 0) {
        link_printf(LINK_INFO," failed! mbedtls_ssl_config_defaults returned %d\n", ret);
        return ret;
    }

    mbedtls_ssl_conf_max_version(&pTlsData->conf, MBEDTLS_SSL_MAJOR_VERSION_3, MBEDTLS_SSL_MINOR_VERSION_3);
    mbedtls_ssl_conf_min_version(&pTlsData->conf, MBEDTLS_SSL_MAJOR_VERSION_3, MBEDTLS_SSL_MINOR_VERSION_3);
    link_printf(LINK_INFO," ok\n");

    /* OPTIONAL is not optimal for security, but makes interop easier in this simplified example */
    if (ca_crt != NULL) {
#if defined(FORCE_SSL_VERIFY)
        mbedtls_ssl_conf_authmode(&(pTlsData->conf), MBEDTLS_SSL_VERIFY_REQUIRED);
#else
        mbedtls_ssl_conf_authmode(&(pTlsData->conf), MBEDTLS_SSL_VERIFY_OPTIONAL);
#endif
    } else {
        mbedtls_ssl_conf_authmode(&(pTlsData->conf), MBEDTLS_SSL_VERIFY_NONE);
    }

#if defined(MBEDTLS_X509_CRT_PARSE_C)
    mbedtls_ssl_conf_ca_chain(&(pTlsData->conf), &(pTlsData->cacertl), NULL);

    if ((ret = mbedtls_ssl_conf_own_cert(&(pTlsData->conf), &(pTlsData->clicert), &(pTlsData->pkey))) != 0) {
        link_printf(LINK_INFO," failed\n  ! mbedtls_ssl_conf_own_cert returned %d\n", ret);
        return ret;
    }
#endif
    mbedtls_ssl_conf_rng(&(pTlsData->conf), _ssl_random, NULL);

    if ((ret = mbedtls_ssl_setup(&(pTlsData->ssl), &(pTlsData->conf))) != 0) {
        link_printf(LINK_INFO,"failed! mbedtls_ssl_setup returned 0x%x\n", ret);
		mbedtls_ssl_free(&(pTlsData->ssl));
		mbedtls_ssl_config_free(&(pTlsData->conf));
        return ret;
    }

	link_printf(LINK_INFO," ok\n");
	
	link_printf(LINK_INFO, "available heap(%d bytes)\n", HAL_get_free_heap_size());
#if defined(ON_PRE) || defined(ON_DAILY)
    link_printf(LINK_INFO,"SKIPPING mbedtls_ssl_set_hostname() when ON_PRE or ON_DAILY defined!\n");
#else
    mbedtls_ssl_set_hostname(&(pTlsData->ssl), addr);
#endif
    mbedtls_ssl_set_bio(&(pTlsData->ssl), &(pTlsData->fd), mbedtls_net_send, mbedtls_net_recv, mbedtls_net_recv_timeout);
    /*
      * 4. Handshake
      */
   // mbedtls_ssl_conf_read_timeout(&(pTlsData->conf), 1000);
    link_printf(LINK_INFO,"Performing the SSL/TLS handshake...\n");

    while ((ret = mbedtls_ssl_handshake(&(pTlsData->ssl))) != 0) {
        if ((ret != MBEDTLS_ERR_SSL_WANT_READ) && (ret != MBEDTLS_ERR_SSL_WANT_WRITE)) {
            printf("failed  ! mbedtls_ssl_handshake returned -0x%04x\n", ret);
            return ret;
        }
    }
	
	link_printf(LINK_INFO, "available heap(%d bytes)\n", HAL_get_free_heap_size());
    link_printf(LINK_INFO," ok\n");

    /*
     * 5. Verify the server certificate
     */
    printf("  . Verifying peer X.509 certificate..\n");
    if (0 != (ret = _real_confirm(mbedtls_ssl_get_verify_result(&(pTlsData->ssl))))) {
        printf(" failed  ! verify result not confirmed.\n");
        return ret;
    }
	
	link_printf(LINK_INFO, "after ssl establish available heap(%d bytes)\n", HAL_get_free_heap_size());
    /* n->my_socket = (int)((n->tlsdataparams.fd).fd); */
    /* WRITE_IOT_DEBUG_LOG("my_socket=%d", n->my_socket); */
    return ret;

}



uintptr_t HAL_SSL_Establish(const char *host, uint16_t port, const char *ca_crt, size_t ca_crt_len)
{
    char                port_str[6];
    const char         *alter = host;
    TLSDataParams_pt    pTlsData;

	//alter="a1qKCi0yXLp.iot-as-mqtt.cn-shanghai.aliyuncs.com";

	if (host == NULL || ca_crt == NULL) {
		link_printf(LINK_INFO,"input params are NULL, abort\n");
		return 0;
	}

	if (!strlen(host) || (strlen(host) < 8)) {
		link_printf(LINK_INFO,"invalid host: '%s'(len=%d), abort\n", host, (int)strlen(host));
		return 0;
	}

	link_printf(LINK_INFO, "[%s]host:%s,port:%d\n", __FUNCTION__,alter,port);

    pTlsData = g_ssl_hooks.malloc(sizeof(TLSDataParams_t));
    if (NULL == pTlsData) {
        return (uintptr_t)NULL;
    }
    memset(pTlsData, 0x0, sizeof(TLSDataParams_t));

    sprintf(port_str, "%u", port);

	link_printf(LINK_INFO, "[%s] Start ssl connect\n", __FUNCTION__); 

    mbedtls_platform_set_calloc_free(_SSLCalloc_wrapper, _SSLFree_wrapper);
	
	link_printf(LINK_INFO, "before ssl establish available heap(%d bytes)\n", HAL_get_free_heap_size());

    if (0 != _TLSConnectNetwork(pTlsData, alter, port_str, (char const *)ca_crt, ca_crt_len, NULL, 0, NULL, 0, NULL, 0)) {
        _network_ssl_disconnect(pTlsData);
        g_ssl_hooks.free((void *)pTlsData);
        return (uintptr_t)NULL;
    }

    return (uintptr_t)pTlsData;

}

int32_t HAL_SSL_Destroy(uintptr_t handle)
{
	link_printf(LINK_INFO,"[%s]ssl_close: %p",__FUNCTION__ ,handle);

	if ((uintptr_t)NULL == handle) {
        link_err_printf("handle is NULL");
        return 0;
    }

    _network_ssl_disconnect((TLSDataParams_t *)handle);
    g_ssl_hooks.free((void *)handle);
    return 0;
}



int32_t HAL_SSL_Read(uintptr_t handle, char *buf, int len, int timeout_ms)
{
	link_printf(LINK_DEBUG, "[%s] recv buf: %s\n", __FUNCTION__, buf); 	
	int ret = 0;
	if (!handle) {
		return -1;
	}

	ret=_network_ssl_read((TLSDataParams_t *)handle, buf, len, timeout_ms);
	
	if (ret < 0) {
		link_err_printf(" SSL receive error, ret = %d\n", ret);
		return -1;
	}

	return ret;

}


int32_t HAL_SSL_Write(uintptr_t handle, const char *buf, int len, int timeout_ms)
{
	link_printf(LINK_DEBUG, "[%s] send buf: %s\n", __FUNCTION__, buf); 	
	int ret = 0;
	if (!handle) {
		return -1;
	}

	ret = _network_ssl_write((TLSDataParams_t *)handle, buf, len, timeout_ms);
	
	
	return (ret > 0) ? ret : -1;
}

int HAL_SSLHooks_set(ssl_hooks_t *hooks)
{
    if (hooks == NULL || hooks->malloc == NULL || hooks->free == NULL) {
        return DTLS_INVALID_PARAM;
    }

    g_ssl_hooks.malloc = hooks->malloc;
    g_ssl_hooks.free = hooks->free;

    return DTLS_SUCCESS;
}



#endif

int HAL_Sys_Net_Is_Ready()
{
	unsigned char* tmp;
	unsigned char ip[4]={0};


	/*get IP address*/
	tmp= LwIP_GetIP(&xnetif[0]);

	ip[0] = tmp[0];
	ip[1] = tmp[1];
	ip[2] = tmp[2];
	ip[3] = tmp[3];
	if ((ip[0] == 192 && ip[1] == 168 && ip[2] == 1 && ip[3] == 80)
		|| (ip[0] == 0 && ip[1] == 0 && ip[2] == 0 && ip[3] == 0))  {
		link_printf(LINK_DEBUG, "[%s] Network is not ready, ret = 0\n", __FUNCTION__); 
	} else if(RTW_SUCCESS == wifi_is_connected_to_ap( )) {
		link_printf(LINK_DEBUG, "[%s] Network is ready, ret = 1\n", __FUNCTION__); 		
		return 1;
	} 
	return 0;
}

int HAL_UDP_close_without_connect(intptr_t sockfd)
{
	link_printf(LINK_DEBUG,"[%s] fd %d\n", __FUNCTION__, ((long)sockfd));
	close((long)sockfd);

	return (int)1;
}

static int network_create_socket( pnetaddr_t netaddr, int type, int protocol, struct sockaddr_in *paddr, long *psock)
{
	struct hostent *hp;
	struct in_addr in;
	uint32_t ip;
	int opt_val = 1;

	link_printf(LINK_INFO, "[%s]host:%s\n", __FUNCTION__,netaddr->host);

	if (NULL == netaddr->host) {
		ip = htonl(INADDR_ANY);
		link_printf(LINK_DEBUG, "[%s]ip:%x\n", __FUNCTION__,ip);
	} else {
	    /*
	     * in some platform gethostbyname() will return bad result
	     * if host is "255.255.255.255", so calling inet_aton first
	     */
		if (inet_aton(netaddr->host, &in)) {
			ip = *(uint32_t *)&in;
		} else {
			link_printf(LINK_DEBUG, "[%s]gethostbyname\n", __FUNCTION__);
			hp = gethostbyname(netaddr->host);
			link_printf(LINK_DEBUG, "[%s]hp->name:%x\n", __FUNCTION__,hp->h_name);
			if (!hp) {
				link_err_printf("can't resolute the host address.");
				return -1;
			}
			ip = *(uint32_t *)(hp->h_addr);

		}
	}
	link_printf(LINK_DEBUG, "[%s] ip: %x\n", __FUNCTION__, ip);		
	
	*psock = socket(AF_INET, type, protocol);
	if (*psock < 0) {
		link_err_printf("Create socket failed!");
		return -1;
	}

	/* allow multiple sockets to use the same PORT number */
	if (0 != setsockopt(*psock, SOL_SOCKET, SO_REUSEADDR, &opt_val, sizeof(opt_val))) {
		link_err_printf(" Set SO_REUSEADDR failed");
		close((int)*psock);
		return -1;
	}

	if (type == SOCK_DGRAM) {
	    if (0 != setsockopt(*psock, SOL_SOCKET, SO_BROADCAST, &opt_val, sizeof(opt_val))) {
	        link_err_printf("UDP BC setsockopt error!");
	        close((int)*psock);
	        return -1;
	    }
	}

	memset(paddr, 0, sizeof(struct sockaddr_in));
	paddr->sin_addr.s_addr = ip;
	paddr->sin_family = AF_INET;
	paddr->sin_port = htons( netaddr->port );
	link_printf(LINK_INFO, "[%s] Bind port = %d\n", __FUNCTION__, netaddr->port);

	return 0;
}


intptr_t HAL_UDP_create_without_connect(const char *host, unsigned short port)
{
	struct sockaddr_in addr;
	intptr_t server_socket;
	netaddr_t netaddr = {(char *)host, port};
	
	if (0 != network_create_socket(&netaddr, SOCK_DGRAM, IPPROTO_UDP, &addr, (long *)&server_socket)) {
 	        link_err_printf("Create UDP socket failed\n");
		return (intptr_t)HAL_INVALID_FD;
	}
   	link_printf(LINK_INFO, "Create UDP socket successfully\n");

	if (-1 == bind(server_socket, (struct sockaddr *)&addr, sizeof(struct sockaddr_in))) {
        	link_err_printf("Bind socket failed\n");
	    	HAL_UDP_close_without_connect(server_socket);
		return (intptr_t)HAL_INVALID_FD;
	}
	link_printf(LINK_DEBUG, "[%s] UDP Server server_socket: %d, port: %d \n", __FUNCTION__, server_socket, port);
	return server_socket;
}


int HAL_UDP_joinmulticast(intptr_t sockfd,
char *p_group)
{
    int err = -1;
    int socket_id = -1;
    int loop = 0;
    struct ip_mreq mreq;

    if (NULL == p_group) {
        return -1;
    }
	
    /*set loopback*/
    socket_id = (int)sockfd;
    err = setsockopt(socket_id, IPPROTO_IP, IP_MULTICAST_LOOP, &loop, sizeof(loop));
    if (err < 0) {
        printf("setsockopt");
        return err;
    }
	HAL_Printf("MultiCast Address:		  %s", p_group);
	//link_printf(LINK_INFO, "MultiCast Address:		  %s", p_group);

    mreq.imr_multiaddr.s_addr = inet_addr(p_group);
    mreq.imr_interface.s_addr = htonl(INADDR_ANY); /*default networt interface*/

    /*join to the multicast group*/
    err = setsockopt(socket_id, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq));
    if (err < 0) {
        printf("setsockopt");
        return err;
    }

	link_printf(LINK_INFO, "[] succuss.");

    return 0;

}


int HAL_UDP_recvfrom(intptr_t sockfd,
NetworkAddr *p_remote,
unsigned char *p_data,
unsigned int datalen,
unsigned int timeout_ms)
{
	int ret;
	struct sockaddr_in addr;
	socklen_t addr_len = (socklen_t)sizeof(addr);
	int last_err;

	fd_set read_fds;
    struct timeval timeout = {timeout_ms / 1000, (timeout_ms % 1000) * 1000};

    FD_ZERO(&read_fds);
    FD_SET(sockfd, &read_fds);

    ret = select(sockfd + 1, &read_fds, NULL, NULL, &timeout);
    if (ret == 0) {
        return 0;    /* receive timeout */
    }

    if (ret < 0) {
        if (errno == EINTR) {
            return -3;    /* want read */
       }
        return -4; /* receive failed */
    }

	
	ret = recvfrom((long)sockfd, p_data, datalen, 0, (struct sockaddr *)&addr, &addr_len);
	if (ret > 0) {
		if (NULL != p_remote->addr) {
			p_remote->port = ntohs(addr.sin_port); 
			 if (NULL != p_remote->addr) {
				strcpy((char*)p_remote->addr, inet_ntoa(addr.sin_addr));
			}
		link_printf(LINK_DEBUG, "[%s] buffer:%s, netaddr->host: %s, netaddr->port: %d\n", __FUNCTION__, p_data, p_remote->addr, p_remote->port);
		}
		return ret;
	} else {
			last_err = lwip_getsocklasterr((long)sockfd);
			if (last_err) {
				link_err_printf("UDP recv data failed, length:%d, err %d", datalen, last_err);
			} else {
				return 0;
			}
	}
	return -1;

}


int HAL_UDP_sendto(intptr_t sockfd,
const NetworkAddr *p_remote,
const unsigned char *p_data,
unsigned int datalen,
unsigned int timeout_ms)
{
	int ret;
	uint32_t ip;
    struct in_addr in;
	struct hostent *hp;
	struct sockaddr_in addr;
	fd_set write_fds;
    struct timeval timeout = {timeout_ms / 1000, (timeout_ms % 1000) * 1000};

	/*
	 * in some platform gethostbyname() will return bad result
	 * if host is "255.255.255.255", so calling inet_aton first
	 */
	
	 if (p_remote->addr) {
		link_printf(LINK_DEBUG, "[%s] sockfd: %d, buf: %s, len: %d, netaddr->host: %s, netaddr->port: %d\n", __FUNCTION__,
			(intptr_t)sockfd, p_data, datalen, p_remote->addr, p_remote->port);
	 }
	 	
    if (inet_aton((char *)p_remote->addr, &in)) {
        ip = *(uint32_t *)&in;
    } else {
        hp = gethostbyname((char *)p_remote->addr);
		link_printf(LINK_DEBUG, "[%s]hp->name:%x\n", __FUNCTION__,hp->h_name);
        if (!hp) {
            link_err_printf("can't resolute the host address \n");
            return -1;
        }
        ip = *(uint32_t *)(hp->h_addr);
    }

    FD_ZERO(&write_fds);
    FD_SET(sockfd, &write_fds);

    ret = select(sockfd + 1, NULL, &write_fds, NULL, &timeout);
    if (ret == 0) {
        return 0;    /* write timeout */
    }

    if (ret < 0) {
        if (errno == EINTR) {
            return -3;    /* want write */
        }
        return -4; /* write failed */
    }

    addr.sin_addr.s_addr = ip;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(p_remote->port);

    ret = sendto(sockfd, p_data, datalen, 0, (struct sockaddr *)&addr, sizeof(struct sockaddr_in));

    if (ret < 0) {
        link_err_printf("sendto");
    }


	link_printf(LINK_DEBUG,"UDP send successful(addr=%x/%d),ret %d\n",addr.sin_addr.s_addr,ntohs(addr.sin_port),ret);
    return (ret) > 0 ? ret : -1;
}

int32_t HAL_TCP_Destroy(uintptr_t fd)
{
	if (fd >= 0) {
		shutdown((int)fd, 2);
		close(fd);
		link_printf(LINK_DEBUG,"[%s] fd %d\n", __FUNCTION__, fd);
	}

	return (int)1;

}


uintptr_t HAL_TCP_Establish(const char *host, uint16_t port)
{
	struct sockaddr_in addr;
	uintptr_t server_socket;
	netaddr_t netaddr = {(char *)host, port};
	int buffer_size=0;
	int last_err;


	link_printf(LINK_INFO, "[%s] port: %d\n", __FUNCTION__, port);

	if (0 != network_create_socket(&netaddr, SOCK_STREAM, IPPROTO_TCP, &addr, (long *)&server_socket)) {
	        link_err_printf("TCPServer socket create error");
		return (uintptr_t)HAL_INVALID_FD;
	}
	buffer_size = SOCKET_TCPSOCKET_BUFFERSIZE;
	setsockopt( server_socket, SOL_SOCKET, SO_RCVBUF, &buffer_size, 4);
	setsockopt( server_socket, SOL_SOCKET, SO_SNDBUF, &buffer_size, 4);

	print_ip_addr(&addr,server_socket);
	
    link_printf(LINK_DEBUG, "[%s] Create tcp socket successfully.\n", __FUNCTION__ );
	if (connect(server_socket, (struct sockaddr*)&addr, sizeof(struct sockaddr_in)) < 0) {
    	last_err = lwip_getsocklasterr(server_socket);
	 	if(last_err == EINPROGRESS) {
        	link_err_printf("TCP conncet noblock\n");
    	} else {
    		link_err_printf("TCP conncet failed, last_err %d\n", last_err);
    	}
		HAL_TCP_Destroy(server_socket);
		return (uintptr_t)HAL_INVALID_FD;
	}

	/*set receive timeout: 5000ms*/
	int recv_timeout, overtime_ret;
	recv_timeout = 5000;
	overtime_ret = setsockopt(server_socket, SOL_SOCKET, SO_RCVTIMEO, &recv_timeout, sizeof(recv_timeout));
	if(overtime_ret < 0) {
		link_err_printf("\nfail to set timeout\n");
	}
    link_printf(LINK_INFO, "[%s]tcp connect success.\n", __FUNCTION__ );
	return server_socket;

}

int32_t HAL_TCP_Write(uintptr_t fd, const char *buf, uint32_t len, uint32_t timeout_ms)
{
	int bytes_sent;
	int last_err;
	link_printf(LINK_INFO, "[%s] fd: %d, len: %d, buf: %s\n", __FUNCTION__, (uintptr_t)fd, len,buf);

	if(buf == NULL) {
   		return -1;
	}
	bytes_sent = send((long)fd, buf, len, 0);
	/*TCP发送数据需要判断错误码*/
	if(bytes_sent < 0) {
	last_err = lwip_getsocklasterr((long)fd);    
    	if(last_err == EAGAIN || last_err == EWOULDBLOCK || last_err == EINTR) {
		link_err_printf("no error, last_err %d", last_err);        	
    	} else {
		link_err_printf("send failed, last_err %d", last_err);        	
    	}
	return -1;
	}
	link_printf(LINK_DEBUG, "[%s] send success, ret %d\n", __FUNCTION__, bytes_sent);
	return (int32_t)bytes_sent;


}

int32_t HAL_TCP_Read(uintptr_t fd, char *buf, uint32_t len, uint32_t timeout_ms)
{
	int bytes_received;
    	int last_err;

	if(buf == NULL) {
   		return -1;
	}
	bytes_received = recv((long)fd, buf, len, 0);
	if(bytes_received <= 0) {
	last_err = lwip_getsocklasterr(((long)fd));    
    	if(last_err == EAGAIN || last_err == EWOULDBLOCK || last_err == EINTR) {
		link_err_printf("no error, last_err %d", last_err);        	        	
    	} else {  
		link_err_printf("receive failed, last_err %d", last_err);        	
    	}
	return -1;
	}
	link_printf(LINK_DEBUG, "[%s] receive success ret %d\n", __FUNCTION__, bytes_received);
	return (int32_t)bytes_received;

}
                                            /*************wifi hal*******************/
// callback for promisc packets, like rtk_start_parse_packet in SC, wf, 1021
void awss_wifi_promisc_rx(unsigned char* buf, unsigned int len, void* user_data)
{
	/* rtl8188: include 80211 FCS field(4 byte) */
	int with_fcs = 0;
	/* rtl8188: link-type IEEE802_11_RADIO (802.11 plus radiotap header) */
	int link_type = AWSS_LINK_TYPE_NONE;
	
#if CONFIG_UNSUPPORT_PLCPHDR_RPT
	int type = ((ieee80211_frame_info_t *)user_data)->type;
	if(type == RTW_RX_UNSUPPORT)
		link_type = AWSS_LINK_TYPE_HT40_CTRL;
#endif

	if (monitor_running) {
		p_handler_recv_data_callback(buf, len, link_type, with_fcs,((ieee80211_frame_info_t *)user_data)->rssi);
	}
}

void HAL_Awss_Close_Monitor(void)
{
	link_printf(LINK_INFO, "[%s] close monitor\n", __func__);
	monitor_running=0;	
	p_handler_recv_data_callback=NULL;
	wifi_set_promisc(RTW_PROMISC_DISABLE, NULL, 0);
}

void HAL_Awss_Open_Monitor(_IN_ awss_recv_80211_frame_cb_t cb)
{
	link_printf(LINK_INFO, "[%s], start monitor\n", __func__);
	p_handler_recv_data_callback=cb;
	monitor_running=1;
#if CONFIG_AUTO_RECONNECT
    wifi_set_autoreconnect(RTW_AUTORECONNECT_DISABLE);
#endif
	wifi_enter_promisc_mode();
	wifi_set_promisc(RTW_PROMISC_ENABLE_2, awss_wifi_promisc_rx, 0);

}

int rtl_check_ap_mode() 
{
	int mode;
	//Check if in AP mode
	wext_get_mode(WLAN0_NAME, &mode);

	if(mode == RTW_MODE_MASTER) {
#if CONFIG_LWIP_LAYER
		dhcps_deinit();
#endif
		wifi_off();
		HAL_SleepMs(20);
		if (wifi_on(RTW_MODE_STA) < 0){
			link_err_printf("Wifi on failed!Do zconfig reset!");
			HAL_Reboot();
		}
	}
	return 0;
}
// for getting ap encrypt
static int find_ap_from_scan_buf(char*buf, u32 buflen, char *target_ssid, void *user_data)
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
			strcpy((char*)pwifi->ssid, target_ssid);
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

int scan_networks_with_ssid(int (results_handler)(char*buf, u32 buflen, char *ssid, void *user_data), 
		OUT void* user_data, IN u32 scan_buflen, IN char* ssid, IN int ssid_len)
{
	int scan_cnt = 0, add_cnt = 0;
	scan_buf_arg scan_buf;
	int ret;

	scan_buf.buf_len = scan_buflen;
	scan_buf.buf = (char*)pvPortMalloc(scan_buf.buf_len);
	if(!scan_buf.buf){
		link_err_printf("can't malloc memory(%d)", scan_buf.buf_len);
		return RTW_NOMEM;
	}
	//set ssid
	memset(scan_buf.buf, 0, scan_buf.buf_len);
	memcpy(scan_buf.buf, &ssid_len, sizeof(int));
	memcpy(scan_buf.buf+sizeof(int), ssid, ssid_len);

	//Scan channel	
	if(scan_cnt = (wifi_scan(RTW_SCAN_TYPE_ACTIVE, RTW_BSS_TYPE_ANY, &scan_buf)) < 0){
		link_err_printf("wifi scan failed !\n");
		ret = RTW_ERROR;
	}else{
		if(NULL == results_handler)
		{
			int plen = 0;
			while(plen < scan_buf.buf_len){
				int len, rssi, ssid_len, i, security_mode;
				int wps_password_id;
				char *mac, *ssid;
				//u8 *security_mode;
				HAL_Printf("\n\r");
				// len
				len = (int)*(scan_buf.buf + plen);
				HAL_Printf("len = %d,\t", len);
				// check end
				if(len == 0) break;
				// mac
				mac = scan_buf.buf + plen + 1;
				HAL_Printf("mac = ");
				for(i=0; i<6; i++)
					HAL_Printf("%02x ", (u8)*(mac+i));
				HAL_Printf(",\t");
				// rssi
				rssi = *(int*)(scan_buf.buf + plen + 1 + 6);
				HAL_Printf(" rssi = %d,\t", rssi);
				// security_mode
				security_mode = (int)*(scan_buf.buf + plen + 1 + 6 + 4);
				switch (security_mode) {
					case RTW_ENCODE_ALG_NONE:
						HAL_Printf("sec = open    ,\t");
						break;
					case RTW_ENCODE_ALG_WEP:
						HAL_Printf("sec = wep     ,\t");
						break;
					case RTW_ENCODE_ALG_CCMP:
						HAL_Printf("sec = wpa/wpa2,\t");
						break;
				}
				// password id
				wps_password_id = (int)*(scan_buf.buf + plen + 1 + 6 + 4 + 1);
				HAL_Printf("wps password id = %d,\t", wps_password_id);
				
				HAL_Printf("channel = %d,\t", *(scan_buf.buf + plen + 1 + 6 + 4 + 1 + 1));
				// ssid
				ssid_len = len - 1 - 6 - 4 - 1 - 1 - 1;
				ssid = scan_buf.buf + plen + 1 + 6 + 4 + 1 + 1 + 1;
				HAL_Printf("ssid = ");
				for(i=0; i<ssid_len; i++)
					HAL_Printf("%c", *(ssid+i));
				plen += len;
				add_cnt++;
			}
		}
		ret = RTW_SUCCESS;
	}
	if(results_handler)
		results_handler(scan_buf.buf, scan_buf.buf_len, ssid, user_data);
		
	if(scan_buf.buf)
		vPortFree(scan_buf.buf);

	return ret;
}

static char security_to_encryption(rtw_security_t security)
{
	char encryption;
	switch(security){
	case RTW_SECURITY_OPEN:
		encryption = AWSS_ENC_TYPE_NONE;
		break;
	case RTW_SECURITY_WEP_PSK:
	case RTW_SECURITY_WEP_SHARED:
		encryption = AWSS_ENC_TYPE_WEP;
		break;
	case RTW_SECURITY_WPA_TKIP_PSK:
	case RTW_SECURITY_WPA2_TKIP_PSK:
		encryption = AWSS_ENC_TYPE_TKIP;
		break;
	case RTW_SECURITY_WPA_AES_PSK:
	case RTW_SECURITY_WPA2_AES_PSK:
		encryption = AWSS_ENC_TYPE_AES;
		break;
	case RTW_SECURITY_WPA2_MIXED_PSK:
		encryption = AWSS_ENC_TYPE_MAX;
		break;
	default:
		encryption = AWSS_ENC_TYPE_INVALID;
		link_err_printf("unknow security mode!\n");
		break;
	}

	return encryption;
}

static char security_to_auth(rtw_security_t security)
{
	link_printf(LINK_DEBUG, "[%s]\n", __func__);
	char auth_ret = -1;
	switch(security){
	case RTW_SECURITY_OPEN:
		auth_ret = AWSS_AUTH_TYPE_OPEN;
		break;
	case RTW_SECURITY_WEP_PSK:
	case RTW_SECURITY_WEP_SHARED:
		auth_ret = AWSS_AUTH_TYPE_SHARED;
		break;
	case RTW_SECURITY_WPA_TKIP_PSK:
	case	RTW_SECURITY_WPA_AES_PSK:
		auth_ret = AWSS_AUTH_TYPE_WPAPSK;
		break;
	case RTW_SECURITY_WPA2_AES_PSK:
	case	RTW_SECURITY_WPA2_TKIP_PSK:
	case	RTW_SECURITY_WPA2_MIXED_PSK:
		auth_ret = AWSS_AUTH_TYPE_WPA2PSK;
		break;
	case RTW_SECURITY_WPA_WPA2_MIXED:
		auth_ret = AWSS_AUTH_TYPE_MAX;
		break;
	default:
		auth_ret = AWSS_AUTH_TYPE_INVALID;
		link_err_printf("unknow security mode!\n");
		break;
	}
	return auth_ret;
}

rtw_result_t store_ap_list(char *ssid,
								char *bssid,
								char ssid_len,
								char rssi,
								char channel,
								char encry,
								char auth)
{
	link_printf(LINK_DEBUG, "[%s], store ap list\n", __func__);
	int length;
	if(apList_t.ApNum < RTW_WIFI_APLIST_SIZE) {
		if (ssid_len) {
			length =  ((ssid_len+1) > HAL_MAX_SSID_LEN) ? HAL_MAX_SSID_LEN : ssid_len;
			rtw_memcpy(apList_t.ApList[apList_t.ApNum].ssid, ssid, ssid_len);
			apList_t.ApList[apList_t.ApNum].ssid[length] = '\0';
		}
		rtw_memcpy(apList_t.ApList[apList_t.ApNum].bssid, bssid, 6);
		apList_t.ApList[apList_t.ApNum].rssi = rssi;
		apList_t.ApList[apList_t.ApNum].channel = channel;
		apList_t.ApList[apList_t.ApNum].encry = encry;
		apList_t.ApList[apList_t.ApNum].auth = auth;
		apList_t.ApList[apList_t.ApNum].is_last_ap= 0;
		apList_t.ApNum++;
	}
	return RTW_SUCCESS;
}

static rtw_result_t rtl_scan_result_handler(rtw_scan_handler_result_t *malloced_scan_result)
{
	if (malloced_scan_result->scan_complete != RTW_TRUE){
		rtw_scan_result_t* record = &malloced_scan_result->ap_details;
		record->SSID.val[record->SSID.len] = 0; /* Ensure the SSID is null terminated */
		store_ap_list((char*)record->SSID.val,
					(char*)record->BSSID.octet,
					record->SSID.len,
					record->signal_strength,
					record->channel,
					security_to_encryption(record->security),
					security_to_auth(record->security));
	
	} else {
		int i;
		apList_t.ApList[apList_t.ApNum - 1].is_last_ap= 1;
		link_printf(LINK_INFO, "[%s] scan ap number: %d\n", __func__, apList_t.ApNum);	
		for(i=0; i<apList_t.ApNum; i++) {

			if (NULL != p_handler_scan_result_callback)
				p_handler_scan_result_callback(apList_t.ApList[i].ssid, apList_t.ApList[i].bssid, apList_t.ApList[i].auth, 
					apList_t.ApList[i].encry, apList_t.ApList[i].channel, apList_t.ApList[i].rssi, apList_t.ApList[i].is_last_ap);
		}
		if(apList_t.ApList != NULL) {
			rtw_free(apList_t.ApList);
			apList_t.ApList = NULL;
			apList_t.ApNum = 0;
		}
	}
	apScanCallBackFinished = 1;
	return RTW_SUCCESS;
}


int HAL_Wifi_Scan(awss_wifi_scan_result_cb_t cb)
{
    link_printf(LINK_DEBUG, "[%s]\n", __func__);
    apScanCallBackFinished = 0;

    if(apList_t.ApList) {
            rtw_free(apList_t.ApList);
            apList_t.ApList = NULL;
    }
    apList_t.ApList = (ApList_str *)HAL_Malloc(RTW_WIFI_APLIST_SIZE * sizeof(ApList_str));
    if(!apList_t.ApList) {
            link_err_printf(" malloc ap list failed\n");
            return -1;
    }
    apList_t.ApNum = 0;
    p_handler_scan_result_callback = cb;
    wifi_scan_networks(rtl_scan_result_handler, NULL );

    while (0 == apScanCallBackFinished){
            HAL_SleepMs(50);
    }

    return 0;
}


int get_ap_security_mode(IN char * ssid, OUT rtw_security_t *security_mode, OUT u8 * channel)
{
    rtw_wifi_setting_t wifi;
    u32 scan_buflen = 1000;

    memset(&wifi, 0, sizeof(wifi));

    if(scan_networks_with_ssid(find_ap_from_scan_buf, (void*)&wifi, scan_buflen, ssid, strlen(ssid)) != RTW_SUCCESS){
            link_err_printf("get ap security mode failed!");
            return -1;
    }

    if(strcmp(wifi.ssid, ssid) == 0){
            *security_mode = wifi.security_type;
            *channel = wifi.channel;
            return 0;
    }

    return 0;
}



#if !(CONFIG_EXAMPLE_WLAN_FAST_CONNECT == 1)
void rtl_var_init()
{
  memset(&wifi_config_setting, 0 , sizeof(rtw_wifi_setting_t));
  wifi_config_setting.security_type = RTW_SECURITY_UNKNOWN;
}

void rtl_restore_wifi_info_to_flash()
{
  struct wlan_fast_reconnect * data_to_flash;
  data_to_flash = (struct wlan_fast_reconnect *)HAL_Malloc(sizeof(struct wlan_fast_reconnect));
          /*clean wifi ssid,key and bssid*/
  memset(data_to_flash, 0,sizeof(struct wlan_fast_reconnect));
  if(data_to_flash) {
          memcpy(data_to_flash->psk_essid, wifi_config_setting.ssid, strlen(wifi_config_setting.ssid));
          data_to_flash->psk_essid[strlen(wifi_config_setting.ssid)] = '\0';
          memcpy(data_to_flash->psk_passphrase, wifi_config_setting.password, strlen(wifi_config_setting.password));	
          data_to_flash->psk_passphrase[strlen(wifi_config_setting.password)] = '\0';
          if (wifi_config_setting.channel> 0 && wifi_config_setting.channel < 14) {
                  data_to_flash->channel = (uint32_t)wifi_config_setting.channel;
          } else {
                  data_to_flash->channel = -1;
          }
          link_printf(LINK_INFO, "write wifi info to flash,: ssid = %s, pwd = %s,ssid length = %d, pwd length = %d, channel = %d, security =%d\n",
                         wifi_config_setting.ssid, wifi_config_setting.password, strlen(wifi_config_setting.ssid), strlen(wifi_config_setting.password), data_to_flash->channel, wifi_config_setting.security_type);
          data_to_flash->security_type = wifi_config_setting.security_type;
  }
  wlan_write_reconnect_data_to_flash((u8 *)data_to_flash, sizeof(struct wlan_fast_reconnect));
  if(data_to_flash) {
          HAL_Free(data_to_flash);
  }
}
#endif


static int softAP_start(const char* ap_name, const char *ap_password)
{
  int timeout = 20;
#if CONFIG_LWIP_LAYER 
  struct ip_addr ipaddr;
  struct ip_addr netmask;
  struct ip_addr gw;
  struct netif * pnetif = &xnetif[0];
#endif
  int ret = 0;
  int channel=11;
#if CONFIG_LWIP_LAYER
  dhcps_deinit();
#if LWIP_VERSION_MAJOR >= 2
  IP4_ADDR(ip_2_ip4(&ipaddr), IP_ADDR0, IP_ADDR1, IP_ADDR2, IP_ADDR3);
  IP4_ADDR(ip_2_ip4(&netmask), NETMASK_ADDR0, NETMASK_ADDR1 , NETMASK_ADDR2, NETMASK_ADDR3);
  IP4_ADDR(ip_2_ip4(&gw), GW_ADDR0, GW_ADDR1, GW_ADDR2, GW_ADDR3);
  netif_set_addr(pnetif, ip_2_ip4(&ipaddr), ip_2_ip4(&netmask),ip_2_ip4(&gw));
#else
  IP4_ADDR(&ipaddr, IP_ADDR0, IP_ADDR1, IP_ADDR2, IP_ADDR3);
  IP4_ADDR(&netmask, NETMASK_ADDR0, NETMASK_ADDR1 , NETMASK_ADDR2, NETMASK_ADDR3);
  IP4_ADDR(&gw, GW_ADDR0, GW_ADDR1, GW_ADDR2, GW_ADDR3);

  netif_set_addr(pnetif, &ipaddr, &netmask,&gw);
#endif
#endif

  wifi_off();
  vTaskDelay(20);
  if (wifi_on(RTW_MODE_AP) < 0){
          printf("Wifi on failed!");
          return -1;
  }
  wifi_disable_powersave();//add to close powersave
#ifdef CONFIG_WPS_AP
  wpas_wps_dev_config(pnetif->hwaddr, 1);
#endif
  printf("Starting AP\n");

  if(ap_password) {
          if(wifi_start_ap((char*)ap_name,
                  RTW_SECURITY_WPA2_AES_PSK,
                  (char*)ap_password,
                  strlen((const char *)ap_name),
                  strlen((const char *)ap_password),
                  channel
                  ) != RTW_SUCCESS) 
          {
                  printf("wifi start ap mode failed!\n\r");
                  return -1;
          }
  } else {
          if(wifi_start_ap((char*)ap_name,
                  RTW_SECURITY_OPEN,
                  NULL,
                  strlen((const char *)ap_name),
                  0,
                  channel
                  ) != RTW_SUCCESS) 
          {
                  printf("wifi start ap mode failed!\n\r");
                  return -1;
          }
  }

  link_printf(LINK_INFO,"[start softap:]Start AP succuss\n");

#if CONFIG_LWIP_LAYER
  //LwIP_UseStaticIP(pnetif);
  dhcps_init(pnetif);
#endif
  return ret;
}



int HAL_Awss_Open_Ap(const char *ssid, const char *passwd, int beacon_interval, int hide)
{
  link_printf(LINK_INFO,"start_ap: %s(%s) \r\n", ssid, passwd);

  int ret=0;

  ret=softAP_start(ssid,passwd);

  return ret;

}

int HAL_Awss_Close_Ap()
{
    link_printf(LINK_INFO,"stop_ap");
    wifi_off();
    HAL_SleepMs(100);
    wifi_on(RTW_MODE_STA);
    return 0;
}

int auth_trans(char encry)
{
	int auth_ret = -1;
	switch(encry){
	case AWSS_ENC_TYPE_NONE:
		auth_ret = RTW_SECURITY_OPEN;
		break;
	case AWSS_ENC_TYPE_WEP:
		auth_ret = RTW_SECURITY_WEP_PSK;
		break;
	case AWSS_ENC_TYPE_TKIP:
		auth_ret = RTW_SECURITY_WPA_TKIP_PSK;
		break;
	case AWSS_ENC_TYPE_AES:
	case AWSS_ENC_TYPE_TKIPAES:
		auth_ret = RTW_SECURITY_WPA2_AES_PSK;
		break;
	case AWSS_ENC_TYPE_INVALID:
	default:
		link_printf(LINK_INFO, "unknow security mode!\n");
	}
	return auth_ret;
}


int HAL_Awss_Connect_Ap(
_IN_ uint32_t connection_timeout_ms,
_IN_ char ssid[HAL_MAX_SSID_LEN],
_IN_ char passwd[HAL_MAX_PASSWD_LEN],
_IN_OPT_ enum AWSS_AUTH_TYPE auth,
_IN_OPT_ enum AWSS_ENC_TYPE encry,
_IN_OPT_ uint8_t bssid[ETH_ALEN],
_IN_OPT_ uint8_t channel)
{	
	u8 *ifname[2] = {WLAN0_NAME,WLAN1_NAME};
	rtw_wifi_setting_t setting;
	rtw_network_info_t wifi_info;
	unsigned char ssid_len,passwd_len;
	memset(&wifi_info, 0 , sizeof(rtw_network_info_t));
	u8 connect_retry = 3;
	int ret;
	//u8 channel;
	int try_scan_cnt = 8;
	ssid_len=(unsigned char)strlen(ssid);
	passwd_len=(unsigned char)strlen(passwd);

	link_printf(LINK_INFO, "[%s]ssid: %s, pwd: %s, channel: %d, auth: %d, encry: %d, BSSID: %2.2x:%2.2x:%2.2x:%2.2x:%2.2x:%2.2x\n",
		__func__, ssid, passwd, channel, auth, encry, bssid[0], bssid[1], bssid[2], bssid[3], bssid[4], bssid[5]);

	rtl_check_ap_mode();

#if CONFIG_AUTO_RECONNECT
	wifi_set_autoreconnect(1);//RTW_AUTORECONNECT_INFINITE
#endif

	//wifi_disable_powersave();//add to close powersave

  	wifi_info.password_len = passwd_len;
	wifi_info.ssid.len = ssid_len;
	memcpy(wifi_info.ssid.val, ssid, ssid_len);
	memset(wifi_info.bssid.octet, 0, sizeof(wifi_info.bssid.octet));
	wifi_info.password = (unsigned char *)passwd;

	memcpy(zbssid,bssid,strlen(bssid));
	link_printf(LINK_INFO,"zbssid:%2.2x:%2.2x:%2.2x:%2.2x:%2.2x:%2.2x\n",zbssid[0],zbssid[1],zbssid[2],zbssid[3],zbssid[4],zbssid[5]);

	while ((try_scan_cnt--) > 0) {
		if(get_ap_security_mode(wifi_info.ssid.val, &wifi_info.security_type, &channel) != 0)
		{
			channel = 0;
			if (try_scan_cnt == 0) {
				wifi_info.security_type = RTW_SECURITY_WPA2_AES_PSK;
				wifi_config_setting.security_type = wifi_info.security_type;
				link_printf(LINK_INFO,"Warning : unknow security type, default set to WPA2_AES\r\n");
				break;
			} else {
				link_printf(LINK_INFO,"Warning: no security type detected, retry %d ...\r\n", try_scan_cnt);
				continue;
			}
		} else {
			wifi_config_setting.security_type = wifi_info.security_type;
			link_printf(LINK_INFO,"Security type (%d) detected by scanning.\r\n", wifi_info.security_type);
			break;
		}
	}

	if (wifi_info.security_type == RTW_SECURITY_WEP_PSK) {
		if(wifi_info.password_len == 10) {
			u32 p[5];
			u8 pwd[6], i = 0; 
			sscanf((const char*)wifi_info.password, "%02x%02x%02x%02x%02x", &p[0], &p[1], &p[2], &p[3], &p[4]);
			for(i=0; i< 5; i++)
				pwd[i] = (u8)p[i];
			pwd[5] = '\0';
			memset(wifi_info.password, 0, 33);
			strcpy(wifi_info.password, (char const *)pwd);
		} else if (wifi_info.password_len == 26) {
			u32 p[13];
			u8 pwd[14], i = 0;
			sscanf((const char*)wifi_info.password, "%02x%02x%02x%02x%02x%02x%02x"\
				"%02x%02x%02x%02x%02x%02x", &p[0], &p[1], &p[2], &p[3], &p[4],\
				&p[5], &p[6], &p[7], &p[8], &p[9], &p[10], &p[11], &p[12]);
			for(i=0; i< 13; i++)
				pwd[i] = (u8)p[i];
			pwd[13] = '\0';
			memset(wifi_info.password, 0, 33);
			strcpy(wifi_info.password, (char const *)pwd);
		}
		memset(wifi_config_setting.password, 0, 65);
		memcpy(wifi_config_setting.password, wifi_info.password, strlen(wifi_info.password));
	}
	while (connect_retry) {
		link_printf(LINK_INFO,"\r\nwifi_connect to ssid: %s, type %d\r\n", wifi_info.ssid.val, wifi_info.security_type);
		ret = wifi_connect(wifi_info.ssid.val, wifi_info.security_type, 
				   wifi_info.password, wifi_info.ssid.len, 
				   wifi_info.password_len,
				   0,NULL);
		if (ret == 0) {
                    //    WifiStatusHandler(NOTIFY_STATION_UP);
			ret = LwIP_DHCP(0, DHCP_START);
                        if (ret == DHCP_TIMEOUT) {
                            HAL_Reboot();
                        }
			int i = 0;
			for(i=0;i<NET_IF_NUM;i++){
				if(rltk_wlan_running(i)){
					wifi_get_setting((const char*)ifname[i],&setting);
				}
			}

              // need to set again because umesh may change this.
	        netif_set_default(&xnetif[0]);       
            
			wifi_config_setting.channel = setting.channel;

			memcpy(wifi_config_setting.ssid, ssid, ssid_len);		
			memcpy(wifi_config_setting.password, passwd, passwd_len);
			hal_wifi_ip_stat_t stat;
            u8 *ip = LwIP_GetIP(&xnetif[0]);
            u8 *gw = LwIP_GetGW(&xnetif[0]);
            u8 *mask = LwIP_GetMASK(&xnetif[0]);
            u8 *mac = LwIP_GetMAC(&xnetif[0]);
    
                stat.dhcp = DHCP_CLIENT;

			link_printf(LINK_INFO, "Connected!\n");
#if !(CONFIG_EXAMPLE_WLAN_FAST_CONNECT == 1)

			rtl_restore_wifi_info_to_flash();
#endif             
            snprintf(stat.ip, 16, "%d.%d.%d.%d",  ip[0],  ip[1],  ip[2],  ip[3]);
            snprintf(stat.gate, 16, "%d.%d.%d.%d",  gw[0],  gw[1],  gw[2],  gw[3]);
            snprintf(stat.mask, 16, "%d.%d.%d.%d",  mask[0],  mask[1],  mask[2],  mask[3]);
            snprintf(stat.dns, 16, "%d.%d.%d.%d",  gw[0],  gw[1],  gw[2],  gw[3]);
            snprintf(stat.mac, 16, "%x:%x:%x:%x:%x:%x",  mac[0],  mac[1],  mac[2],  mac[3], mac[4], mac[5]);
            link_printf(LINK_INFO,"\n\rip %s gw %s mask %s dns %s mac %s \r\n", stat.ip, stat.gate, stat.mask, stat.dns, stat.mac);
            //NetCallback(&stat);
			return 0;
		}else{
                    //WifiStatusHandler(NOTIFY_STATION_DOWN);
		     link_printf(LINK_INFO,"\r\nwifi connect failed, ret %d, retry %d\r\n", ret, connect_retry);
		}
	
		connect_retry --;
        
		if (connect_retry == 0) {
			return -1;
		}
	}
	return 0;
}

int HAL_Awss_Get_Channelscan_Interval_Ms(void)
{
	return 250;
}


int HAL_Awss_Get_Timeout_Interval_Ms(void)
{
	return 3 * 60 * 1000;
}

int HAL_Wifi_Get_Ap_Info(char ssid[HAL_MAX_SSID_LEN],char passwd[HAL_MAX_PASSWD_LEN],uint8_t bssid[ETH_ALEN])
{

	u8 *ifname[2] = {WLAN0_NAME,WLAN1_NAME};
	rtw_wifi_setting_t wifi_setting;
	static int tmp = 0;
	int i = 0;
    link_printf(LINK_INFO,"[%s]\n",__FUNCTION__);
	if (wifi_is_ready_to_transceive(RTW_STA_INTERFACE) == RTW_SUCCESS) {
		link_printf(LINK_INFO,"[%s]\n",__FUNCTION__);

		wifi_get_setting((const char*)ifname[0],&wifi_setting);
		if(ssid != NULL){
			memcpy(ssid, wifi_setting.ssid, strlen((char*)wifi_setting.ssid));
			link_printf(LINK_INFO,"[%s]\n",ssid);
		}
		if(passwd != NULL){
			memcpy(passwd, wifi_setting.password, strlen((char*)wifi_setting.password));
			link_printf(LINK_INFO,"[%s]\n", passwd);
		}
		
		char *ssid_test = wifi_setting.ssid;
		char *passwd_test = wifi_setting.password;
		memcpy(ssid_test, wifi_setting.ssid, strlen((char*)wifi_setting.ssid));
		memcpy(passwd_test, wifi_setting.password, strlen((char*)wifi_setting.password));
		link_printf(LINK_INFO,"[%s], [%s]\n",ssid_test, passwd_test);
		//link_printf(LINK_INFO,"[%s], [%s]\n",ssid, passwd);
		if((zbssid[0] == 0 && zbssid[1] == 0 &&
			zbssid[2] == 0 && zbssid[3] == 0 &&
			zbssid[4] == 0 && zbssid[5] == 0) || (zbssid == NULL)){			
			wext_get_bssid(ifname[0],bssid);
		}else{
			memcpy(bssid,zbssid,strlen(zbssid));
		}
		link_printf(LINK_INFO,"[%s] bssid=%02x:%02x:%02x:%02x:%02x:%02x\n", 
					__FUNCTION__,  bssid[0], bssid[1], bssid[2], bssid[3], bssid[4], bssid[5]);
		
	} else {
		if (tmp % 100 == 0) {
			tmp = 0;
			link_printf(LINK_INFO, "[%s], disconnect with AP.", __FUNCTION__);
		} 		
		tmp++;
		return -1;
	}
	
	return 0;
}


uint32_t HAL_Wifi_Get_IP(char ip_str[NETWORK_ADDR_LEN], const char *ifname)
{
	uint32_t ip_addr;
	unsigned char* tmp;
	unsigned char ip[4]={0};
	char ifname_buff[IFNAMSIZ] = "wlan0";

	if((NULL == ifname || strlen(ifname) == 0) ){
		ifname = ifname_buff;
	}
	/*get IP address*/
	tmp = LwIP_GetIP(&xnetif[0]);

	ip[0] = tmp[0];
	ip[1] = tmp[1];
	ip[2] = tmp[2];
	ip[3] = tmp[3];
	sprintf((char *)ip_str,"%d.%d.%d.%d",ip[0],ip[1],ip[2],ip[3]);
	ip_addr = inet_addr(ip_str);
	return ip_addr;
}


char *HAL_Wifi_Get_Mac(char mac_str[HAL_MAC_LEN])
{

	unsigned char* tmp;
	unsigned char mac[6]={0};
	/*get mac address*/
	tmp= LwIP_GetMAC(&xnetif[0]);
	mac[0] = tmp[0];
	mac[1] = tmp[1];
	mac[2] = tmp[2];
	mac[3] = tmp[3];
	mac[4] = tmp[4];
	mac[5] = tmp[5];

	sprintf((char *)mac_str,"%02X:%02X:%02X:%02X:%02X:%02X",mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);
	return mac_str;
}

int HAL_Wifi_Get_Link_Stat(_OU_ int *p_rssi,_OU_ int *p_channel)

{
    link_printf(LINK_INFO,"get_link_stat\r\n");

    u8 *ifname[2] = {WLAN0_NAME,WLAN1_NAME};
    rtw_wifi_setting_t setting;
    wifi_get_setting((const char*)ifname[0],&setting);
    wifi_show_setting((const char*)ifname[0],&setting);
    wifi_get_rssi(p_rssi);
    p_channel = (int *)setting.channel;
	
    link_printf(LINK_INFO,"rssi: %d,channel: %d\r\n",p_rssi,p_channel);   
    return 0;
}


void HAL_Awss_Switch_Channel(char primary_channel, char secondary_channel, uint8_t bssid[ETH_ALEN])
{
	wifi_set_channel(primary_channel);
}

int HAL_Awss_Get_Encrypt_Type(void)
{
	return 3;
}

int HAL_Awss_Get_Conn_Encrypt_Type(void)
{
	return 4;
}

static int parse_vendor_specific(u8 *pos, u8 elen,  struct i802_11_elems *elems)
{
	link_printf(LINK_INFO, "[%s]\n", __FUNCTION__);
	unsigned int oui;
	int ret = -1;

	/* first 3 bytes in vendor specific information element are the IEEE
	 * OUI of the vendor. The following byte is used a vendor specific
	 * sub-type. */
	if (elen < 4) {
		return ret;
	}
	
	oui = WPA_GET_BE24(pos);
	if ( g_vendor_oui == oui ) {
		elems->ie = pos;
		elems->len = elen;
		ret = 0;
		link_printf(LINK_INFO, "oui = %x\n", oui);
	}
	
	return ret;
}



int wlan_mgmt_filter(u8 *start, u16 len, u16 frame_type)
{

//type 0xDD
	/*bit0--beacon, bit1--probe request, bit2--probe response*/
	int link_frame_type = 0;
	int value_find= -1;
	int rssi;
	struct i802_11_elems elems;
	elems.ie = NULL;
	elems.len = len;

	switch (frame_type) {
		 case WIFI_BEACON:
		 	link_frame_type = BIT(1);
			break;
		case WIFI_PROBEREQ:
			link_frame_type = BIT(2);
			link_printf(LINK_INFO,"g_vendor_frame_type: %d, frame_type: %d\n", g_vendor_frame_type, link_frame_type);
			break;
		case WIFI_PROBERSP:
			link_frame_type = BIT(3);
			break;
		default:
			printf("Unsupport  management frames\n");
			return -1;
	}

	wifi_get_rssi(&rssi);
	
	if (g_vendor_frame_type & (uint32_t)link_frame_type) {
		u8 *pos = start;
		u8 id, elen;
		while (len >=  2) {
			id = *pos++;
			elen = *pos++;
			len -= 2;
			if (elen > len) {
				return -1;
			}
			if (id == WLAN_EID_VENDOR_SPECIFIC) {
				value_find = parse_vendor_specific(pos, elen, &elems);
				if (value_find == 0) {
					pos = pos - 2;
					if (g_vendor_mgmt_filter_callback) {
						g_vendor_mgmt_filter_callback((uint8_t *)pos, elems.len+2, 0, 1);
						return 0;
					}
				}
			}
			len -= elen;
			pos += elen;		
			
		}
	}

	return -1;
}

static void wifi_rx_mgnt_hdl(uint8_t *buf, int buf_len, int flags, void *userdata)
{
    int8_t rssi = (int8_t)flags;

    /* only deal with Probe Request*/
    if(g_vendor_mgmt_filter_callback && buf[0] == 0x40){
		u_print_data((unsigned char *)buf, buf_len);
        g_vendor_mgmt_filter_callback((uint8_t*)buf, buf_len,rssi,0);
    }
}


int HAL_Wifi_Enable_Mgmt_Frame_Filter(
_IN_ uint32_t filter_mask,
_IN_OPT_ uint8_t vendor_oui[3],
_IN_ awss_wifi_mgmt_frame_cb_t callback)
{
	g_vendor_oui = WPA_GET_BE24(vendor_oui);
	g_vendor_frame_type = (int) filter_mask;
	g_vendor_mgmt_filter_callback = callback;
	link_printf(LINK_INFO, "[%s], oui = %x, frame_type = %o\n", __FUNCTION__, g_vendor_oui, filter_mask);
#if RAW_FRAME
	if (callback) {
		wifi_set_indicate_mgnt(2);
	} else {
		wifi_set_indicate_mgnt(0);
		return -1;
	}

	 wifi_reg_event_handler(WIFI_EVENT_RX_MGNT, (rtw_event_handler_t)wifi_rx_mgnt_hdl, NULL);
#else
	if (callback) {
		p_wlan_mgmt_filter = wlan_mgmt_filter;
	} else {
		p_wlan_mgmt_filter = 0;
		return -1;
	}
#endif
   	return 0;
}

int HAL_Wifi_Send_80211_Raw_Frame(_IN_ enum HAL_Awss_Frame_Type type,
_IN_ uint8_t *buffer, _IN_ int len)
{
	int ret = -2;
	const char *ifname = WLAN0_NAME;
	if (type == FRAME_BEACON || type == FRAME_PROBE_REQ) {
		ret = wext_send_mgnt(ifname, (char*)buffer, len, 1);
		link_printf(LINK_INFO, "[%s]buffer:%s ret = %d type = %d\n", __FUNCTION__,buffer, ret, type);
        u_print_data((unsigned char *)buffer, len);
	}	

	return ret;
}

                                            /*************OTA hal*******************//*OTA*/

static unsigned int linknewImg2Addr = 0xFFFFFFFF;
char *HeadBuffer = NULL;
int link_size = 0, ota_file_size=0; 
u8 link_signature[HEADER_BAK_LEN] = {0};
uint32_t ota_target_index = OTA_INDEX_2;
update_ota_target_hdr OtaTargetHdr;

typedef struct {
	u32 recv_total_len;
	u32 address;
	int remain_bytes;
	u16 hdr_count;
	u8   is_get_ota_hdr;
	u8   ota_exit;
	u8   sig_flg;	/*skip the signature*/
	u8   sig_cnt;
	u8   sig_end;
} ota_update_var;

ota_update_var gvar_ota;


void HAL_Firmware_Persistence_Start(void)
{
	memset(&gvar_ota, 0, sizeof(ota_update_var));	
	/* check OTA index we should update */
	if (ota_get_cur_index() == OTA_INDEX_1) {
		ota_target_index = OTA_INDEX_2;
		link_printf(LINK_INFO, "\n\r[%s] OTA2 address space will be upgraded \n", __FUNCTION__);
			
	} else {
		ota_target_index = OTA_INDEX_1;
		link_printf(LINK_INFO, "\n\r[%s] OTA1 address space will be upgraded \n", __FUNCTION__);
	}
	link_size = 0;
	HeadBuffer = HAL_Malloc(1024);
	if (HeadBuffer == NULL) {
		link_err_printf("malloc headbuffer failed\n");
	}
}

int HAL_Firmware_Persistence_Stop(void)
{
	flash_t flash;
	link_printf(LINK_INFO, "size = %d \n", link_size);
	if (link_size > 0){
		link_printf(LINK_INFO, "buffer signature is: = %s \n", link_signature);
		 /*------------- verify checksum and update signature-----------------*/
		if(verify_ota_checksum(&OtaTargetHdr)){
			if(!change_ota_signature(&OtaTargetHdr, ota_target_index)) {
				printf("\n\rChange signature failed\n");
				return LINK_ERR;
			}
			//write new firmware version to flash
			linkkit_flash_erase_sector(CONFIG_FW_INFO);
			if (1 == linkkit_flash_stream_write(CONFIG_FW_INFO, IOTX_FIRMWARE_VER_LEN, _firmware_version)) {
				link_printf(LINK_INFO, "write fw ver(%s) successfully\n",_firmware_version);	
			} else {
				link_printf(LINK_WARNING, "write fw ver(%s) fail\n",_firmware_version);
				return LINK_ERR;
			}
			link_printf(LINK_INFO, "\n[%s]flash reboot after 3s.\n", __FUNCTION__);	
			HAL_SleepMs(3000);
			HAL_Reboot();
			return LINK_OK;
		}
	} else {
		/*if checksum error, clear the signature zone which has been 
		written in flash in case of boot from the wrong firmware*/
	#if 1
		device_mutex_lock(RT_DEV_LOCK_FLASH);
		flash_erase_sector(&flash, linknewImg2Addr - SPI_FLASH_BASE);
		device_mutex_unlock(RT_DEV_LOCK_FLASH);
	#endif
		link_err_printf("check_sum error\n");
		return LINK_ERR;
	}
}

int HAL_Firmware_Persistence_Write(char *buffer, uint32_t length)
{
	char *buf;
	flash_t flash;
	update_file_hdr OtaFileHdr;
	static u8 ota_tick_log = 0;
	static unsigned long tick1=0, tick2=0;
	u8 * signature;
	
	if(gvar_ota.ota_exit) {
		link_err_printf("Failed to save OTA firmware\n");
		return LINK_ERR;
	}
	if (ota_tick_log == 0) {
		tick1 = xTaskGetTickCount();
		ota_tick_log = 1;
	}
	/*-----read 4 Dwords from server, get image header number and header length*/
	if (HeadBuffer != NULL) {
		buf = HeadBuffer + gvar_ota.recv_total_len;
	}

	
	gvar_ota.recv_total_len += length;

	if (gvar_ota.is_get_ota_hdr == 0) {
		buf = HeadBuffer;
		memcpy(buf+gvar_ota.hdr_count, buffer, gvar_ota.recv_total_len - gvar_ota.hdr_count);
		memcpy((u8*)(&OtaTargetHdr.FileHdr), buf, sizeof(OtaTargetHdr.FileHdr));
		memcpy((u8*)(&OtaTargetHdr.FileImgHdr), buf+8, 8);
		memcpy((u8*)(&OtaTargetHdr.Sign), buf+32, 8);

		
		/*------acquire the desired Header info from buffer*/
		buf = HeadBuffer;

		if (!get_ota_tartget_header((uint8_t *)buf, gvar_ota.recv_total_len, &OtaTargetHdr, ota_target_index)) {
			link_err_printf("\n\rget OTA header failed\n");
			goto update_ota_exit;
		}
		gvar_ota.address = OtaTargetHdr.FileImgHdr[0].FlashAddr;
		gvar_ota.remain_bytes = OtaTargetHdr.FileImgHdr[0].ImgLen;
		linknewImg2Addr = OtaTargetHdr.FileImgHdr[0].FlashAddr;

		link_printf(LINK_INFO, "linknewImg2Addr = %x\n",linknewImg2Addr);
		signature = &OtaTargetHdr.Sign[0][0];
		link_printf(LINK_INFO, "buffer signature is: = %s \n", signature);
		link_printf(LINK_INFO, "ValidImgCnt is: = %d \n", OtaTargetHdr.ValidImgCnt);
		link_printf(LINK_INFO,"fw ver:%d, hdr:%d, img id:%d, hdr len:%d, chksum:%X, img len:%d, img offset:%d, flash offset:%X \n",
			OtaTargetHdr.FileHdr.FwVer,
			OtaTargetHdr.FileHdr.HdrNum,
			OtaTargetHdr.FileImgHdr[0].ImgId,
			OtaTargetHdr.FileImgHdr[0].ImgHdrLen,
			OtaTargetHdr.FileImgHdr[0].Checksum,
			OtaTargetHdr.FileImgHdr[0].ImgLen,
			OtaTargetHdr.FileImgHdr[0].Offset,
			OtaTargetHdr.FileImgHdr[0].FlashAddr);
		
		/*-------------------erase flash space for new firmware--------------*/
		link_printf(LINK_INFO,"\n\rErase is ongoing... \n");		
		for(int i = 0; i < OtaTargetHdr.ValidImgCnt; i++) {
			erase_ota_target_flash(OtaTargetHdr.FileImgHdr[i].FlashAddr, OtaTargetHdr.FileImgHdr[i].ImgLen);
		}
		link_printf(LINK_INFO,"\n\rErase done! \n");
		
		/*the upgrade space should be masked, because the encrypt firmware is used 
		for checksum calculation*/
		//OTF_Mask(1, (linknewImg2Addr - SPI_FLASH_BASE), NewImg2BlkSize, 1);
		/* get OTA image and Write New Image to flash, skip the signature, 
		not write signature first for power down protection*/
		gvar_ota.address = linknewImg2Addr -SPI_FLASH_BASE + 8;
		gvar_ota.remain_bytes = OtaTargetHdr.FileImgHdr[0].ImgLen - 8;
		gvar_ota.is_get_ota_hdr = 1;
		
		link_printf(LINK_INFO, "gvar_ota.address = 0x%x,remain_bytes = %d\n", gvar_ota.address,gvar_ota.remain_bytes);
		
		if (HeadBuffer != NULL) {
			HAL_Free(HeadBuffer);
			HeadBuffer = NULL;
		}
	}

	/*download the new firmware from server*/
	if(gvar_ota.remain_bytes > 0) {
		if(gvar_ota.recv_total_len > OtaTargetHdr.FileImgHdr[0].Offset) {
			if(!gvar_ota.sig_flg) {
				u32 Cnt = 0;
				/*reach the the desired image, the first packet process*/
				gvar_ota.sig_flg = 1;
				Cnt = gvar_ota.recv_total_len -OtaTargetHdr.FileImgHdr[0].Offset;
				if(Cnt < 8) {
					gvar_ota.sig_cnt = Cnt;
				} else {
					gvar_ota.sig_cnt = 8;
				}

				memcpy(link_signature, buffer + length -Cnt, gvar_ota.sig_cnt);
				link_printf(LINK_INFO, "link_signature=%s, link_size=%d\n", link_signature, link_size);
				if((gvar_ota.sig_cnt < 8) || (Cnt -8 == 0)) {
					return LINK_OK;
				}				
				device_mutex_lock(RT_DEV_LOCK_FLASH);
				if(flash_stream_write(&flash, gvar_ota.address + link_size, Cnt -8, buffer + (length -Cnt + 8)  ) < 0){
					link_err_printf("Write sector failed");
					device_mutex_unlock(RT_DEV_LOCK_FLASH);
					goto update_ota_exit;
				}
				device_mutex_unlock(RT_DEV_LOCK_FLASH);
				link_size += (Cnt - 8);
				gvar_ota.remain_bytes -= link_size;
			} else {					
				/*normal packet process*/
				if(gvar_ota.sig_cnt < 8) {
					if(length < (8 -gvar_ota.sig_cnt)) {
						memcpy(link_signature + gvar_ota.sig_cnt, buffer, length);
						gvar_ota.sig_cnt += length;
						return LINK_OK;
					} else {
						memcpy(link_signature + gvar_ota.sig_cnt, buffer, (8 -gvar_ota.sig_cnt));
						gvar_ota.sig_end = 8 -gvar_ota.sig_cnt;
						length -= (8 -gvar_ota.sig_cnt) ;
						gvar_ota.sig_cnt = 8;
						if(!length) {
							return LINK_OK;
						}
						gvar_ota.remain_bytes -= length;
						if (gvar_ota.remain_bytes <= 0) {
							length = length - (-gvar_ota.remain_bytes);
						}
						device_mutex_lock(RT_DEV_LOCK_FLASH);
						if (flash_stream_write(&flash, gvar_ota.address+link_size, length, buffer+gvar_ota.sig_end) < 0){
							link_err_printf("Write sector failed");
							device_mutex_unlock(RT_DEV_LOCK_FLASH);
							goto update_ota_exit;
						}
						device_mutex_unlock(RT_DEV_LOCK_FLASH);
						link_size += length;
						return LINK_OK;
					}
				}
				gvar_ota.remain_bytes -= length;
				int end_ota = 0;
				if(gvar_ota.remain_bytes <= 0) {
					length = length - (-gvar_ota.remain_bytes);
					end_ota = 1;
				}
				device_mutex_lock(RT_DEV_LOCK_FLASH);
				if(flash_stream_write(&flash, gvar_ota.address + link_size, length, (uint8_t *)buffer) < 0){
					link_err_printf("Write sector failed");
					device_mutex_unlock(RT_DEV_LOCK_FLASH);
					goto update_ota_exit;
				}
				device_mutex_unlock(RT_DEV_LOCK_FLASH);
				link_size += length;
				if (end_ota) {
					link_printf(LINK_INFO, "size = %d \n", link_size);
				}
				tick2 = xTaskGetTickCount();
				if (tick2 - tick1 > 3000) {
					link_printf(LINK_INFO, "Download OTA file: %d B, RemainBytes = %d\n", (link_size), gvar_ota.remain_bytes);
					ota_tick_log = 0;
				}
			}
		}
	}
	return LINK_OK;
	
update_ota_exit:
	if (HeadBuffer != NULL) {
		HAL_Free(HeadBuffer);
		HeadBuffer = NULL;
	}
	gvar_ota.ota_exit = 1;
	memset(&gvar_ota, 0, sizeof(ota_update_var));	
	link_err_printf("Update task exit");	
	return LINK_ERR;	
}

                                       










