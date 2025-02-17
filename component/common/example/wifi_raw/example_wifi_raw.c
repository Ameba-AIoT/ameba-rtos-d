#include "platform_opts.h"
#include "FreeRTOS.h"
#include "osdep_service.h"
#include "wifi/wifi_conf.h"
#include "example_wifi_raw.h"

#define TX_FRAME_BUF_LEN	42

u8 addr1[6] = {0x50, 0x64, 0x2B, 0x98, 0xE7, 0x69};	// Address 1, should be the mac address of destination
u8 addr2[6] = {0x2C, 0x05, 0x47, 0x94, 0xB6, 0x37};	// Address 2, should be the mac address of myself as master
u8 addr3[6] = {0x50, 0x64, 0x2B, 0x98, 0xE7, 0x69};	// Address 3, should be the BSSID

void wifi_raw_tx(void *param)
{
	printf("%s start.\n", __func__);

	unsigned char tx_frame_buf[TX_FRAME_BUF_LEN] = {
		0x00, 0x00, 												// Frame Control field
		0x00, 0x00, 												// Duration/ID field
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 						// Address 1, should be the mac address of destination
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 						// Address 2, should be the mac address of myself as master
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 						// Address 3, should be the BSSID
		0x00, 0x00, 												// Sequence Control field
		0x00, 0x00,													// SSID field
		0x01, 0x08, 0x82, 0x84, 0x8b, 0x96, 0x8c, 0x12, 0x98, 0x24,	// Supported Rates field
		0x32, 0x04, 0xb0, 0x48, 0x60, 0x6c							// Extended Supported Rates field
	};

	SetFrameSubType(&tx_frame_buf, WIFI_PROBEREQ);

	rtw_memcpy(&tx_frame_buf[4], addr1, 6);
	rtw_memcpy(&tx_frame_buf[10], addr2, 6);
	rtw_memcpy(&tx_frame_buf[16], addr3, 6);

	while (1) {
		if (wext_send_mgnt(WLAN0_NAME, tx_frame_buf, TX_FRAME_BUF_LEN, 0) < 0) {
			printf("%s failed!\n", __func__);
		}
		rtw_msleep_os(100);
	}

exit:
	rtw_thread_exit();
}

void wifi_raw_rx_callback(char *buf, int buf_len, int flags, void *userdata)
{
	u8 stype;
	int i;

	stype = GetFrameSubType(buf);

	if (stype == WIFI_PROBERSP) {
		printf("Probe Rsp frame:\n");
		for (i = 0; i < buf_len; i++) {
			printf("%x ", buf[i]);
		}
		printf("\n\n");
	}
}

void wifi_raw_rx(void *param)
{
	printf("%s start.\n", __func__);

	wifi_set_indicate_mgnt(2);

	wifi_reg_event_handler(WIFI_EVENT_RX_MGNT, wifi_raw_rx_callback, NULL);

	while (1) {
		rtw_msleep_os(10);
	}

exit:
	rtw_thread_exit();
}

void wifi_raw_task(void *param)
{
	struct task_struct tx_task;
	struct task_struct rx_task;

	while(wifi_is_ready_to_transceive(RTW_STA_INTERFACE) != RTW_SUCCESS){ 
		printf("Wait for WIFI connection ...\n");
		rtw_msleep_os(1000);
	}

	if (rtw_create_task(&tx_task, ((const char *)"wifi_raw_tx_task"), 1024, tskIDLE_PRIORITY + 4, wifi_raw_tx, NULL) != pdPASS) {
		printf("\n\r[%s] Create wifi raw tx task failed", __FUNCTION__);
		goto exit;
	}

	if (rtw_create_task(&rx_task, ((const char *)"wifi_raw_rx_task"), 1024, tskIDLE_PRIORITY + 4, wifi_raw_rx, NULL) != pdPASS) {
		printf("\n\r[%s] Create wifi raw rx task failed", __FUNCTION__);
		goto exit;
	}

exit:
	rtw_thread_exit();
}

void example_wifi_raw(void)
{
	struct task_struct task;
	if (rtw_create_task(&task, ((const char *)"wifi_raw_task"), 1024, tskIDLE_PRIORITY + 1, wifi_raw_task, NULL) != pdPASS) {
		printf("\n\r[%s] Create wifi raw task failed", __FUNCTION__);
	}
}

