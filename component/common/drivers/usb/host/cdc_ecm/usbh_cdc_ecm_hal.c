/**
  ******************************************************************************
  * @file    usbh_cdc_ecm_hal.c
  * @author  Realsil WLAN5 Team
  * @version V1.0.0
  * @date    2023-01-13
  * @brief   This file provides the apis for USB CDC ECM host class
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
#include <FreeRTOS.h>
#include <task.h>
#include <lwip/sockets.h>
#include <lwip/netdb.h>
#include "usbh_cdc_ecm.h"
#include "usbh_cdc_ecm_hal.h"
#include "usbh_cdc_ecm_appx.h"

/* Private defines -----------------------------------------------------------*/
#define CONFIG_USBH_CDC_ECM_HOT_PLUG_TEST   1     /* Hot plug test */

#define USBH_CDC_ECM_LOOPBACK_CNT           100   /* Loopback test round */

#define USBH_CORE_INIT_FAIL                 (1)
#define USBH_CLASS_INIT_FAIL                (2)


/* Private types -------------------------------------------------------------*/
#pragma pack(push)
#pragma pack(1)
typedef struct {
	usb_report_usbdata			report_data;  			//usb rx callback function
	usbh_cdc_ecm_priv_data_t	*priv_handle;

	volatile u8 				send_success;			//usb tx send flag
	volatile u8 				cdc_ecm_is_ready;		//ecm attached status
	volatile u8 				ecm_hw_connect;		//ecm ethernet connect status:0 disconnect,1 connect
	volatile u8 				ecm_init_success;		//usb init success
#if CONFIG_USBH_CDC_ECM_HOT_PLUG_TEST
	u8   				        hotplug_task_flag;		//usb init success
#endif
} usbh_cdc_ecm_host_hal_t;
#pragma pack(pop)

/* Private macros ------------------------------------------------------------*/

/* Private function prototypes -----------------------------------------------*/
static u8 cdc_ecm_cb_init(void);
static u8 cdc_ecm_cb_deinit(void);
static u8 cdc_ecm_cb_attach(void);
static u8 cdc_ecm_cb_detach(void);
static u8 cdc_ecm_cb_setup(void);
static u8 cdc_ecm_cb_process(usb_host_t *host, u8 id);
static u8 cdc_ecm_cb_bulk_send(usbh_urb_state_t state);
static u8 cdc_ecm_cb_bulk_receive(u8 *pbuf, u32 Len);
static u8 cdc_ecm_cb_intr_receive(u8 *buf, u32 length);
static u8 usbh_cdc_ecm_get_sendflag(void);
static u8 usbh_cdc_ecm_set_sendflag(u8 flag);
static u8 usbh_cdc_ecm_class_init(void);
static u8 usbh_cdc_ecm_class_deinit(void);
static u8 usbh_cdc_ecm_do_sub_init(usb_report_usbdata ecm_cb,usbh_cdc_ecm_priv_data_t *priv);
static u8 usbh_cdc_ecm_do_sub_deinit(void);

/* Private variables ---------------------------------------------------------*/

static const char *TAG = "ECM";

static _sema cdc_ecm_detach_sema;
static _sema cdc_ecm_attach_sema;

static usbh_cdc_ecm_host_hal_t usbh_cdc_ecm_host_user;

static usbh_config_t usbh_ecm_cfg = {
	.pipes = 8U,
	.speed = USB_SPEED_HIGH,
	.dma_enable = FALSE,
	.main_task_priority = USBH_ECM_MAIN_THREAD_PRIORITY,
	.isr_task_priority = USBH_ECM_ISR_THREAD_PRIORITY,
	.rx_fifo_size = 0x200U,
	.nptx_fifo_size = 0x100U,
	.ptx_fifo_size = 0x100U,
};

static usbh_cdc_ecm_state_cb_t cdc_ecm_usb_cb = {
	.init = cdc_ecm_cb_init,
	.deinit = cdc_ecm_cb_deinit,
	.attach = cdc_ecm_cb_attach,
	.detach = cdc_ecm_cb_detach,
	.setup = cdc_ecm_cb_setup,
	.bulk_transmitted = cdc_ecm_cb_bulk_send,
	.bulk_received = cdc_ecm_cb_bulk_receive,
	.intr_received = cdc_ecm_cb_intr_receive,
};

static usbh_user_cb_t usbh_ecm_usr_cb = {
	.process = cdc_ecm_cb_process
};

/* Private functions ---------------------------------------------------------*/
static u8 usbh_cdc_ecm_get_sendflag(void)
{
	return usbh_cdc_ecm_host_user.send_success;
}
static u8 usbh_cdc_ecm_set_sendflag(u8 flag)
{
	u8 ret = usbh_cdc_ecm_host_user.send_success;
	usbh_cdc_ecm_host_user.send_success = flag;
	return ret;
}
static u8 cdc_ecm_cb_init(void)
{
	RTK_LOGI(TAG, "CDC ECM INIT\n");
	return HAL_OK;
}
static u8 cdc_ecm_cb_deinit(void)
{
	RTK_LOGI(TAG, "CDC ECM DEINIT\n");
	return HAL_OK;
}
static u8 cdc_ecm_cb_attach(void)
{
	RTK_LOGI(TAG, "ATTACH\n");
	rtw_up_sema(&cdc_ecm_attach_sema);
	return HAL_OK;
}
static u8 cdc_ecm_cb_detach(void)
{
	RTK_LOGI(TAG, "DETACH\n");
	usbh_cdc_ecm_host_user.cdc_ecm_is_ready = 0;
	rtw_up_sema(&cdc_ecm_detach_sema);

	return HAL_OK;
}

static u8 cdc_ecm_cb_setup(void)
{
	RTK_LOGI(TAG, "SETUP\n");
	usbh_cdc_ecm_host_user.cdc_ecm_is_ready = 1;

	return HAL_OK;
}

static u8 cdc_ecm_cb_bulk_receive(u8 *buf, u32 length)
{
	//RTK_LOGD(TAG, "bulk receive\n");
	if (usbh_cdc_ecm_host_user.report_data != NULL) {
		usbh_cdc_ecm_host_user.report_data(buf, length);
	}

	return HAL_OK;
}
static u8 cdc_ecm_cb_bulk_send(usbh_urb_state_t state)
{
	usbh_cdc_ecm_set_sendflag(1);
	if (state != USBH_URB_DONE) {
		RTK_LOGE(TAG, "Transmit fail: %d\n", state);
	}

	return HAL_OK;
}

//parse to get the ethernet status
static u8 cdc_ecm_cb_intr_receive(u8 *buf, u32 len)
{
	//RTK_LOGD(TAG, "INTR RX len=%d\n", length);
	if (buf && len >= 8) {
		/*A1 00 00 00 01 00 00 00 */
		if (len == 8 && buf[0] == 0xA1 && buf[1] == CDC_ECM_NOTIFY_NETWORK_CONNECTION) {
			usbh_cdc_ecm_host_user.ecm_hw_connect = buf[2];
		}
		/*A1 2A 00 00 01 00 00 08 	00 00 00 00 00 00 00 00 */
		else if (len == 16 && buf[0] == 0xA1 && buf[1] == CDC_ECM_NOTIFY_CONNECTION_SPEED_CHANGE) {
			//usbh_cdc_ecm_speed_change_t
		}
	}

	return HAL_OK;
}


static u8 cdc_ecm_cb_process(usb_host_t *host, u8 id)
{
	UNUSED(host);
	RTK_LOGD(TAG, "Process id(%d)\n", id);
	switch (id) {
	case USBH_MSG_USER_SET_CONFIG:
		usbh_cdc_ecm_choose_config(host);
		break;
	case USBH_MSG_DISCONNECTED:
		usbh_cdc_ecm_host_user.cdc_ecm_is_ready = 0;
		break;

	case USBH_MSG_CONNECTED:
		break;

	default:
		break;
	}

	return HAL_OK;
}

#if CONFIG_USBH_CDC_ECM_HOT_PLUG_TEST
static void usbh_cdc_ecm_hotplug_thread(void *param)
{
	u8 ret = 0;
	UNUSED(param);
	usbh_cdc_ecm_host_user.hotplug_task_flag = 1;
	do {
		rtw_down_sema(&cdc_ecm_detach_sema);
		usb_os_sleep_ms(2000);
		usbh_cdc_ecm_do_sub_deinit();
		usb_os_sleep_ms(1000);
		RTK_LOGI(TAG, "Free heap size: 0x%x\n", xPortGetFreeHeapSize());

		ret = usbh_cdc_ecm_do_sub_init(usbh_cdc_ecm_host_user.report_data,usbh_cdc_ecm_host_user.priv_handle);
		if (ret != 0) {
			RTK_LOGE(TAG, "Fail to init USB host controller: %d\n", ret);
			break;
		}
	}while(usbh_cdc_ecm_host_user.hotplug_task_flag);
	rtw_thread_exit();
}
static u8 usbh_cdc_ecm_create_hotplug_task(void)
{
	int status;
	struct task_struct hotplug_task;

	status = rtw_create_task(&hotplug_task, "ecm_hotplug_thread", 256, USBH_ECM_HOTPLUG_THREAD_PRIORITY, (thread_func_t)usbh_cdc_ecm_hotplug_thread, NULL);
	if (status != pdPASS) {
		RTK_LOGE(TAG, "Fail to create hotplug thread\n");
		return HAL_ERR_UNKNOWN;
	}

	return HAL_OK;
}
#endif
static u8 usbh_cdc_ecm_class_deinit(void)
{
	usbh_cdc_ecm_deinit();
	usbh_deinit();
	return HAL_OK;
}
static u8 usbh_cdc_ecm_class_init(void)
{
	int status;

	status = usbh_init(&usbh_ecm_cfg, &usbh_ecm_usr_cb);
	if (status != HAL_OK) {
		RTK_LOGE(TAG, "Fail to init USB host controller: %d\n", status);
		return USBH_CORE_INIT_FAIL;
	}

	status = usbh_cdc_ecm_init(&cdc_ecm_usb_cb);  /*0 means use default transfer size, and it can not exceed 65536*/
	if (status != HAL_OK) {
		RTK_LOGE(TAG, "Fail to init USB host cdc_ecm driver: %d\n", status);
		return USBH_CLASS_INIT_FAIL;
	}
	if (rtw_down_sema(&cdc_ecm_attach_sema)) {  //attach finish,
		do{
			status = usbh_cdc_ecm_set_alt_setting();
			if(HAL_OK != status){
				usb_os_sleep_ms(100);
			} else {
				break;
			}
		}while(1);
	}

	return HAL_OK;
}
static void usbh_cdc_ecm_class_init_thread(void *param)
{
	int status;
	UNUSED(param);

	status = usbh_cdc_ecm_class_init();
	if (USBH_CORE_INIT_FAIL == status) {
		goto example_exit;
	} else if (USBH_CLASS_INIT_FAIL == status) {
		goto usb_deinit_exit;
	}

	status = usbh_cdc_ecm_task();
	if(status){
		goto usbh_ecm_cdc_deinit_exit;
	}

	goto example_exit;

usbh_ecm_cdc_deinit_exit:
	usbh_cdc_ecm_deinit();

usb_deinit_exit:
	usbh_deinit();

example_exit:
	rtw_thread_exit();
}
static u8 usbh_cdc_ecm_do_sub_deinit(void)
{
	usbh_cdc_ecm_host_user.ecm_init_success = 0;
	return usbh_cdc_ecm_class_deinit();
}
static u8 usbh_cdc_ecm_do_sub_init(usb_report_usbdata ecm_cb,usbh_cdc_ecm_priv_data_t *priv)
{
	int status;
	struct task_struct task;

	usbh_cdc_ecm_host_user.report_data = ecm_cb ;
	usbh_cdc_ecm_host_user.priv_handle = priv ;

	if(priv != NULL){
 		usbh_cdc_ecm_appx_doinit(priv);
	}

	RTK_LOGI(TAG, "USB host init...\n");
	if (0 == usbh_cdc_ecm_host_user.ecm_init_success) {
		status = rtw_create_task(&task, "ecm_init_task", 512, USBH_ECM_INIT_THREAD_PRIORITY, usbh_cdc_ecm_class_init_thread, NULL);
		if (status != pdPASS) {
			RTK_LOGE(TAG, "Fail to create USB host cdc_ecm init thread: %d\n", status);
			return 1 ;
		}
		usbh_cdc_ecm_host_user.ecm_init_success = 1;
	}

	return 0;
}


/* Exported functions --------------------------------------------------------*/
/**
  * @brief  usb deinit, call this will destory all usb host struct
  * @retval status
  */
u8 usbh_cdc_ecm_do_deinit(void)//todo destory all usb task
{
#if CONFIG_USBH_CDC_ECM_HOT_PLUG_TEST
	usbh_cdc_ecm_host_user.hotplug_task_flag = 1;
	//rtw_delete_task(&bulk_task);
#endif
	usbh_cdc_ecm_do_sub_deinit();

	rtw_free_sema(&cdc_ecm_detach_sema);
	rtw_free_sema(&cdc_ecm_attach_sema);


	return 0 ;
}
/**
  * @brief  do the init for the ecm
  * @param  ecm_cb: ecm rx data callback
  * @param  appx_cb: at port rx data callback
  * @retval status 0 means success else fail 
  */
u8 usbh_cdc_ecm_do_init(usb_report_usbdata ecm_cb,usbh_cdc_ecm_priv_data_t *priv)
{
	rtw_init_sema(&cdc_ecm_attach_sema, 0);
	rtw_init_sema(&cdc_ecm_detach_sema, 0);

#if CONFIG_USBH_CDC_ECM_HOT_PLUG_TEST
	usbh_cdc_ecm_create_hotplug_task();
#endif
	return usbh_cdc_ecm_do_sub_init(ecm_cb,priv);
}
/**
  * @brief  get the max packet for ecm rx
  * @retval return the MPS
  */
u16 usbh_cdc_ecm_get_receive_mps(void)
{
	return usbh_cdc_ecm_get_usbin_mps();
}
/**
  * @brief  send the data out throught ecm endpoint
  * @retval return the status 0 send success,else fail
  */
u8 usbh_cdc_ecm_send_data(u8 *buf, u32 len)
{
	u8 ret;
	u8 retry_cnt = 0;
	usbh_cdc_ecm_set_sendflag(0);

	while (1) {
		ret = usbh_cdc_ecm_bulk_send(buf, len);
		if (ret == 0) {
			//success
			break;
		}
		if (++retry_cnt > 3) {
			RTK_LOGI(TAG, "TX drop\n");
			ret = -1;
			break;
		} else {
			rtw_udelay_os(1);
		}
	}

	//wait reply success,wait signal
	while ((ret == 0) && (!usbh_cdc_ecm_get_sendflag())) {
		usb_os_sleep_ms(1);
	}

	return ret;
}
/**
  * @brief  get the usb device connect status
  * @retval 1 dongle connect success,else fail
  */
u8 usbh_cdc_ecm_get_connect_status(void)//1 up
{
	u8 ret,ret1,ret2;
	ret1 = usbh_cdc_ecm_host_user.cdc_ecm_is_ready;
	ret2 = usbh_cdc_ecm_host_user.ecm_hw_connect;
	ret =  ret1 && ret2;
	//RTK_LOGI(TAG, "Status(%d-%d/%d)\n",ret1,ret2,ret);
	if(usbh_cdc_ecm_get_device_vid() == USB_DEFAULT_VID){
		return ret;
	}
	return usbh_cdc_ecm_host_user.cdc_ecm_is_ready ;
}
/**
  * @brief  usb enum success
  * @retval status
  */
u8 usbh_cdc_ecm_usb_is_ready(void)
{
	return (usbh_cdc_ecm_check_enum_status() == HAL_OK);
}
/**
  * @brief  hex to char
  * @retval 
  */
u8 usbh_cdc_ecm_hex_to_char(u8 hex_num)
{
    if ((hex_num>=0x30) && (hex_num<=0x39))
    {
        /* code */
        hex_num = 0 + hex_num - 0x30;
    }
    else if ((hex_num>=0x41) && (hex_num<=0x5A))
    {
        /* code */
        hex_num = 0xA + (hex_num - 0x41);
    }
    else if ( (hex_num>=0x61) && (hex_num<=0x7A))
    {
        /* code */
        hex_num = 0xA + (hex_num - 0x61);
    }
    else
        hex_num = 0x0;

    return hex_num;
}