
#if defined(CONFIG_PLATFORM_8711B)
#include "rtl8710b_ota.h"
#include <FreeRTOS.h>
#include <task.h>
#elif defined(CONFIG_PLATFORM_8195A)
#include <ota_8195a.h>
#elif defined(CONFIG_PLATFORM_8195BHP)
#include <ota_8195b.h>
#elif defined(CONFIG_PLATFORM_8710C)
#include <ota_8710c.h>
#elif defined(CONFIG_PLATFORM_8721D)
#include <platform/platform_stdlib.h>
#include "rtl8721d_ota.h"
#include <FreeRTOS.h>
#include <task.h>
#endif
#include <wifi_constants.h>

#define PORT	443
#define HOST	"192.168.1.100"  //"m-apps.oss-cn-shenzhen.aliyuncs.com"
#define RESOURCE "OTA_All13.bin"     //"051103061600.bin"

extern int wifi_is_ready_to_transceive(rtw_interface_t interface);
#ifdef HTTPS_OTA_UPDATE
void https_update_ota_task(void *param){
	(void)param;
	
	printf("\n\r\n\r\n\r\n\r<<<<<<OTA HTTPS Example start...>>>>>>>\n\r\n\r\n\r\n\r");

	 while(wifi_is_ready_to_transceive(RTW_STA_INTERFACE) != RTW_SUCCESS){
    		printf("Wait for WIFI connection ...\n");
    		vTaskDelay(1000);
  	}
	int ret = -1;
	
	ret = https_update_ota(HOST, PORT, RESOURCE);

	printf("\n\r[%s] Update task exit", __FUNCTION__);
	if(!ret){
		printf("\n\r[%s] Ready to reboot", __FUNCTION__);	
		ota_platform_reset();
	}
	vTaskDelete(NULL);	
}


void example_ota_https(void){
	if(xTaskCreate(https_update_ota_task, (char const *)"http_update_ota_task", 1024, NULL, tskIDLE_PRIORITY + 1, NULL) != pdPASS){
		printf("\n\r[%s] Create update task failed", __FUNCTION__);
	}
}
#endif

