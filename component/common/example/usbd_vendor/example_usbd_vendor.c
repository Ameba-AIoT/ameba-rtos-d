/**
  * This module is a confidential and proprietary property of RealTek and
  * possession or use of this module requires written permission of RealTek.
  *
  * Copyright(c) 2021, Realtek Semiconductor Corporation. All rights reserved.
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------ */

#include <platform_opts.h>
#if defined(CONFIG_EXAMPLE_USBD_VENDOR) && CONFIG_EXAMPLE_USBD_VENDOR
#include "usbd.h"
#include "usbd_vendor.h"
#include "osdep_service.h"

/* Private defines -----------------------------------------------------------*/

// This configuration is used to enable a thread to check hotplug event
// and reset USB stack to avoid memory leak, only for example.
#define CONFIG_USBD_VENDOR_HOTPLUG						1

// USB speed
#ifdef CONFIG_USB_FS
#define CONFIG_USBD_VENDOR_SPEED						USB_SPEED_FULL
#else
#define CONFIG_USBD_VENDOR_SPEED						USB_SPEED_HIGH
#endif

// Loopback ISOC data in an async thread
#if CONFIG_USBD_VENDOR_ISOC_TEST
#define CONFIG_USBD_VENDOR_ISOC_ASYNC_XFER				1
#else
#define CONFIG_USBD_VENDOR_ISOC_ASYNC_XFER				0
#endif

#if CONFIG_USBD_VENDOR_INTR_TEST
// Loopback INTR data in an async thread
#define CONFIG_USBD_VENDOR_INTR_ASYNC_XFER				0
// INTR IN only
#define CONFIG_USBD_VENDOR_INTR_IN_ONLY					0
#else
// Loopback INTR data in an async thread
#define CONFIG_USBD_VENDOR_INTR_ASYNC_XFER				0
// INTR IN only
#define CONFIG_USBD_VENDOR_INTR_IN_ONLY					0
#endif

// Loopback BULK data in an async thread
#if CONFIG_USBD_VENDOR_BULK_TEST
#define CONFIG_USBD_VENDOR_BULK_ASYNC_XFER				1
#else
#define CONFIG_USBD_VENDOR_BULK_ASYNC_XFER				0
#endif

// Thread priorities
#define CONFIG_USBD_VENDOR_INIT_THREAD_PRIORITY			5
#define CONFIG_USBD_VENDOR_ISR_THREAD_PRIORITY			7
#define CONFIG_USBD_VENDOR_HOTPLUG_THREAD_PRIORITY		8 // Should be higher than CONFIG_USBD_VENDOR_ISR_THREAD_PRIORITY
#define CONFIG_USBD_VENDOR_XFER_THREAD_PRIORITY			6 // Should be lower than CONFIG_USBD_VENDOR_ISR_THREAD_PRIORITY

// Enable trace log, set to 0 for better performance
#define CONFIG_USBD_VENDOR_LOG_EN						1

/* Private types -------------------------------------------------------------*/

/* Private macros ------------------------------------------------------------*/

#if CONFIG_USBD_VENDOR_LOG_EN
#define USBD_VENDOR_LOG(fmt, args...)			printf("\n\r%s: " fmt, __FUNCTION__, ## args)
#else
#define USBD_VENDOR_LOG(fmt, args...)
#endif

/* Private function prototypes -----------------------------------------------*/

static u8 vendor_cb_init(void);
static u8 vendor_cb_deinit(void);
static u8 vendor_cb_setup(usb_setup_req_t *req, u8 *buf);
static u8 vendor_cb_set_config(void);
#if CONFIG_USBD_VENDOR_BULK_TEST
static u8 vendor_cb_bulk_received(u8 *buf, u32 len);
#endif
#if CONFIG_USBD_VENDOR_INTR_TEST
static u8 vendor_cb_intr_received(u8 *buf, u32 len);
#if CONFIG_USBD_VENDOR_INTR_IN_ONLY
static void vendor_cb_intr_transmitted(u8 status);
#endif
#endif
#if CONFIG_USBD_VENDOR_ISOC_TEST
static u8 vendor_cb_isoc_received(u8 *buf, u32 len);
#endif
static void vendor_cb_status_changed(u8 status);

/* Private variables ---------------------------------------------------------*/

static usbd_config_t vendor_cfg = {
	.speed = CONFIG_USBD_VENDOR_SPEED,
	.dma_enable = 0U,
	.rx_fifo_depth = 504U,
	.nptx_fifo_depth = 256U,
	.ptx_fifo_depth = 256U,
	.isr_priority = CONFIG_USBD_VENDOR_ISR_THREAD_PRIORITY,
	.ext_intr_en =  USBD_EOPF_INTR | USBD_ICII_INTR,
	.intr_use_ptx_fifo = 0U,
};

static usbd_vendor_cb_t vendor_cb = {
	.init = vendor_cb_init,
	.deinit = vendor_cb_deinit,
	.setup = vendor_cb_setup,
	.set_config = vendor_cb_set_config,
#if CONFIG_USBD_VENDOR_BULK_TEST
	.bulk_received = vendor_cb_bulk_received,
#endif
#if CONFIG_USBD_VENDOR_ISOC_TEST
	.isoc_received = vendor_cb_isoc_received,
#endif
#if CONFIG_USBD_VENDOR_INTR_TEST
	.intr_received = vendor_cb_intr_received,
#if CONFIG_USBD_VENDOR_INTR_IN_ONLY
	.isoc_transmitted = vendor_cb_intr_transmitted,
#endif
#endif
	.status_changed = vendor_cb_status_changed,
};

#if CONFIG_USBD_VENDOR_ISOC_ASYNC_XFER
static u8 *vendor_isoc_tx_buf = NULL;
static u32 vendor_isoc_tx_len = 0;
static _sema vendor_isoc_async_xfer_sema;
#endif

#if CONFIG_USBD_VENDOR_INTR_ASYNC_XFER
static u8 *vendor_intr_tx_buf = NULL;
static u32 vendor_intr_tx_len = 0;
static _sema vendor_intr_async_xfer_sema;
#endif

#if CONFIG_USBD_VENDOR_INTR_IN_ONLY
static u8 vendor_intr_tx_num = 0;
static u8 vendor_intr_in_buf[USBD_VENDOR_INTR_IN_BUF_SIZE];
#endif

#if CONFIG_USBD_VENDOR_BULK_ASYNC_XFER
static u8 *vendor_bulk_tx_buf = NULL;
static u32 vendor_bulk_tx_len = 0;
static _sema vendor_bulk_async_xfer_sema;
#endif

#if CONFIG_USBD_VENDOR_HOTPLUG
static u8 vendor_attach_status;
static _sema vendor_attach_status_changed_sema;
#endif

/* Private functions ---------------------------------------------------------*/

/**
  * @brief  Handle the vendor class control requests
  * @param  cmd: Command code
  * @param  buf: Buffer containing command data (request parameters)
  * @param  len: Number of data to be sent (in bytes)
  * @param  value: Value for the command code
  * @retval Status
  */
static u8 vendor_cb_setup(usb_setup_req_t *req, u8 *buf)
{
	UNUSED(buf);

	if ((req->bmRequestType & USB_REQ_DIR_MASK) == USB_D2H) {
		// TBD
	} else {
		// TBD
	}

	return HAL_OK;
}

/**
  * @brief  Initializes vendor application layer
  * @param  None
  * @retval Status
  */
static u8 vendor_cb_init(void)
{
#if CONFIG_USBD_VENDOR_INTR_ASYNC_XFER
	vendor_intr_tx_buf = NULL;
	vendor_intr_tx_len = 0;
	rtw_init_sema(&vendor_intr_async_xfer_sema, 0);
#endif
#if CONFIG_USBD_VENDOR_ISOC_ASYNC_XFER
	vendor_isoc_tx_buf = NULL;
	vendor_isoc_tx_len = 0;
	rtw_init_sema(&vendor_isoc_async_xfer_sema, 0);
#endif
#if CONFIG_USBD_VENDOR_BULK_ASYNC_XFER
	vendor_bulk_tx_buf = NULL;
	vendor_bulk_tx_len = 0;
	rtw_init_sema(&vendor_bulk_async_xfer_sema, 0);
#endif
	return HAL_OK;
}

/**
  * @brief  DeInitializes vendor application layer
  * @param  None
  * @retval Status
  */
static u8 vendor_cb_deinit(void)
{
#if CONFIG_USBD_VENDOR_INTR_ASYNC_XFER
	rtw_free_sema(&vendor_intr_async_xfer_sema);
#endif
#if CONFIG_USBD_VENDOR_ISOC_ASYNC_XFER
	rtw_free_sema(&vendor_isoc_async_xfer_sema);
#endif
#if CONFIG_USBD_VENDOR_BULK_ASYNC_XFER
	rtw_free_sema(&vendor_bulk_async_xfer_sema);
#endif
	return HAL_OK;
}

/**
  * @brief  Set config callback
  * @param  None
  * @retval Status
  */
static u8 vendor_cb_set_config(void)
{
#if CONFIG_USBD_VENDOR_INTR_IN_ONLY
	vendor_intr_tx_num = 0;
	rtw_memset((void *)vendor_intr_in_buf, 0, USBD_VENDOR_INTR_IN_BUF_SIZE);
	usbd_vendor_transmit_intr_data(vendor_intr_in_buf, USBD_VENDOR_INTR_IN_BUF_SIZE);
#endif
	return HAL_OK;
}

#if CONFIG_USBD_VENDOR_INTR_TEST

/**
  * @brief  Data received over USB INTR OUT endpoint
  * @param  buf: RX buffer
  * @param  len: RX data length (in bytes)
  * @retval Status
  */
static u8 vendor_cb_intr_received(u8 *buf, u32 len)
{
#if CONFIG_USBD_VENDOR_INTR_ASYNC_XFER
	USBD_VENDOR_LOG("INTR RX len=%ld data=%d\n", len, buf[0]);
	vendor_intr_tx_buf = buf;
	vendor_intr_tx_len = len;
	rtw_up_sema(&vendor_intr_async_xfer_sema);
#else
	USBD_VENDOR_LOG("INTR RX/TX len=%ld data=%d\n", len, buf[0]);
	usbd_vendor_transmit_intr_data(buf, len);
#endif // CONFIG_USBD_VENDOR_INTR_ASYNC_XFER
	return usbd_vendor_receive_intr_data();
}

#if CONFIG_USBD_VENDOR_INTR_IN_ONLY
/**
  * @brief  Data transmitted over USB INTR IN endpoint
  * @param  state: Transmitted status
  * @retval Status
  */
static void vendor_cb_intr_transmitted(u8 status)
{
	UNUSED(status);
	rtw_memset((void *)vendor_intr_in_buf, ++vendor_intr_tx_num, USBD_VENDOR_INTR_IN_BUF_SIZE);
	usbd_vendor_transmit_intr_data(vendor_intr_in_buf, USBD_VENDOR_INTR_IN_BUF_SIZE);
}
#endif // CONFIG_USBD_VENDOR_INTR_IN_ONLY

#if CONFIG_USBD_VENDOR_INTR_ASYNC_XFER
static void vendor_intr_xfer_thread(void *param)
{
	UNUSED(param);

	for (;;) {
		if (rtw_down_sema(&vendor_intr_async_xfer_sema)) {
			if ((vendor_intr_tx_buf != NULL) && (vendor_intr_tx_len != 0)) {
				USBD_VENDOR_LOG("INTR TX len=%ld data=%d\n", vendor_intr_tx_len, vendor_intr_tx_buf[0]);
				usbd_vendor_transmit_intr_data(vendor_intr_tx_buf, vendor_intr_tx_len);
			}
		}
	}
}
#endif // CONFIG_USBD_VENDOR_INTR_ASYNC_XFER

#endif // CONFIG_USBD_VENDOR_INTR_TEST

#if CONFIG_USBD_VENDOR_ISOC_TEST
/**
  * @brief  Data received over USB ISOC OUT endpoint
  * @param  buf: RX buffer
  * @param  len: RX data length (in bytes)
  * @retval Status
  */
static u8 vendor_cb_isoc_received(u8 *buf, u32 len)
{
#if CONFIG_USBD_VENDOR_ISOC_ASYNC_XFER
	USBD_VENDOR_LOG("ISOC RX len=%ld data=%d\n", len, buf[0]);
	vendor_isoc_tx_buf = buf;
	vendor_isoc_tx_len = len;
	rtw_up_sema(&vendor_isoc_async_xfer_sema);
#else
	USBD_VENDOR_LOG("ISOC RX/TX len=%ld data=%d\n", len, buf[0]);
	usbd_vendor_transmit_isoc_data(buf, len);
#endif // CONFIG_USBD_VENDOR_ISOC_ASYNC_XFER
	return usbd_vendor_receive_isoc_data();
}

#if CONFIG_USBD_VENDOR_ISOC_ASYNC_XFER
static void vendor_isoc_xfer_thread(void *param)
{
	UNUSED(param);

	for (;;) {
		if (rtw_down_sema(&vendor_isoc_async_xfer_sema)) {
			if ((vendor_isoc_tx_buf != NULL) && (vendor_isoc_tx_len != 0)) {
				USBD_VENDOR_LOG("ISOC TX len=%ld data=%d\n", vendor_isoc_tx_len, vendor_isoc_tx_buf[0]);
				usbd_vendor_transmit_isoc_data(vendor_isoc_tx_buf, vendor_isoc_tx_len);
			}
		}
	}
}
#endif // CONFIG_USBD_VENDOR_ISOC_ASYNC_XFER

#endif // CONFIG_USBD_VENDOR_ISOC_TEST

#if CONFIG_USBD_VENDOR_BULK_TEST

/**
  * @brief  Data received over USB BULK OUT endpoint
  * @param  buf: RX buffer
  * @param  len: RX data length (in bytes)
  * @retval Status
  */
static u8 vendor_cb_bulk_received(u8 *buf, u32 len)
{
#if CONFIG_USBD_VENDOR_BULK_ASYNC_XFER
	USBD_VENDOR_LOG("BULK RX len=%ld data=%d\n", len, buf[0]);
	vendor_bulk_tx_buf = buf;
	vendor_bulk_tx_len = len;
	rtw_up_sema(&vendor_bulk_async_xfer_sema);
#else
	USBD_VENDOR_LOG("BULK RX/TX len=%ld data=%d\n", len, buf[0]);
	usbd_vendor_transmit_bulk_data(buf, len);
#endif // CONFIG_USBD_VENDOR_BULK_ASYNC_XFER
	return usbd_vendor_receive_bulk_data();
}

#if CONFIG_USBD_VENDOR_BULK_ASYNC_XFER
static void vendor_bulk_xfer_thread(void *param)
{
	UNUSED(param);

	for (;;) {
		if (rtw_down_sema(&vendor_bulk_async_xfer_sema)) {
			if ((vendor_bulk_tx_buf != NULL) && (vendor_bulk_tx_len != 0)) {
				USBD_VENDOR_LOG("BULK TX len=%ld data=%d\n", vendor_bulk_tx_len, vendor_bulk_tx_buf[0]);
				usbd_vendor_transmit_bulk_data(vendor_bulk_tx_buf, vendor_bulk_tx_len);
			}
		}
	}
}
#endif // CONFIG_USBD_VENDOR_BULK_ASYNC_XFER

#endif // CONFIG_USBD_VENDOR_BULK_TEST

static void vendor_cb_status_changed(u8 status)
{
	printf("\nUSB status changed: %d\n", status);
#if CONFIG_USBD_VENDOR_HOTPLUG
	vendor_attach_status = status;
	rtw_up_sema(&vendor_attach_status_changed_sema);
#endif
}

#if CONFIG_USBD_VENDOR_HOTPLUG
static void vendor_hotplug_thread(void *param)
{
	int ret = 0;

	UNUSED(param);

	for (;;) {
		if (rtw_down_sema(&vendor_attach_status_changed_sema)) {
			if (vendor_attach_status == USBD_ATTACH_STATUS_DETACHED) {
				printf("\nUSB DETACHED\n");
				usbd_vendor_deinit();
				ret = usbd_deinit();
				if (ret != 0) {
					printf("\nFail to de-init USBD driver\n");
					break;
				}
				printf("\nFree heap size: 0x%lx\n", rtw_getFreeHeapSize());
				ret = usbd_init(&vendor_cfg);
				if (ret != 0) {
					printf("\nFail to re-init USBD driver\n");
					break;
				}
				ret = usbd_vendor_init(&vendor_cb);
				if (ret != 0) {
					printf("\nFail to re-init USBD VENDOR class\n");
					usbd_deinit();
					break;
				}
			} else if (vendor_attach_status == USBD_ATTACH_STATUS_ATTACHED) {
				printf("\nUSB ATTACHED\n");
			} else {
				printf("\nUSB INIT\n");
			}
		}
	}

	rtw_thread_exit();
}
#endif // CONFIG_USBD_VENDOR_HOTPLUG

static void example_usbd_vendor_thread(void *param)
{
	int ret = 0;
#if CONFIG_USBD_VENDOR_HOTPLUG
	struct task_struct check_status_task;
#endif
#if CONFIG_USBD_VENDOR_INTR_ASYNC_XFER
	struct task_struct intr_async_xfer_task;
#endif
#if CONFIG_USBD_VENDOR_ISOC_ASYNC_XFER
	struct task_struct isoc_async_xfer_task;
#endif
#if CONFIG_USBD_VENDOR_BULK_ASYNC_XFER
	struct task_struct bulk_async_xfer_task;
#endif

	UNUSED(param);

#if CONFIG_USBD_VENDOR_HOTPLUG
	rtw_init_sema(&vendor_attach_status_changed_sema, 0);
#endif

	ret = usbd_init(&vendor_cfg);
	if (ret != HAL_OK) {
		printf("\nFail to init USB device driver\n");
		goto exit;
	}

	ret = usbd_vendor_init(&vendor_cb);
	if (ret != HAL_OK) {
		printf("\nFail to init USB vendor class\n");
		goto clear_usb_driver_exit;
	}

#if CONFIG_USBD_VENDOR_HOTPLUG
	ret = rtw_create_task(&check_status_task, "vendor_hotplug_thread", 512, CONFIG_USBD_VENDOR_HOTPLUG_THREAD_PRIORITY, vendor_hotplug_thread, NULL);
	if (ret != pdPASS) {
		printf("\nFail to create usbd vendor status check thread\n");
		goto clear_usb_class_exit;
	}
#endif // CONFIG_USBD_VENDOR_HOTPLUG
#if CONFIG_USBD_VENDOR_INTR_ASYNC_XFER
	// The priority of transfer thread shall be lower than USB isr priority
	ret = rtw_create_task(&intr_async_xfer_task, "vendor_intr_xfer_thread", 512, CONFIG_USBD_VENDOR_XFER_THREAD_PRIORITY, vendor_intr_xfer_thread, NULL);
	if (ret != pdPASS) {
		printf("\nFail to create vendor intr transfer thread\n");
		goto clear_check_status_task;
	}
#endif // CONFIG_USBD_VENDOR_INTR_ASYNC_XFER
#if CONFIG_USBD_VENDOR_ISOC_ASYNC_XFER
	// The priority of transfer thread shall be lower than USB isr priority
	ret = rtw_create_task(&isoc_async_xfer_task, "vendor_isoc_xfer_thread", 512, CONFIG_USBD_VENDOR_XFER_THREAD_PRIORITY, vendor_isoc_xfer_thread, NULL);
	if (ret != pdPASS) {
		printf("\nFail to create vendor isoc transfer thread\n");
		goto clear_intr_async_task;
	}
#endif // CONFIG_USBD_VENDOR_ISOC_ASYNC_XFER
#if CONFIG_USBD_VENDOR_BULK_ASYNC_XFER
	// The priority of transfer thread shall be lower than USB isr priority
	ret = rtw_create_task(&bulk_async_xfer_task, "vendor_bulk_xfer_thread", 512, CONFIG_USBD_VENDOR_XFER_THREAD_PRIORITY, vendor_bulk_xfer_thread, NULL);
	if (ret != pdPASS) {
		printf("\nFail to create vendor bulk transfer thread\n");
		goto clear_isoc_async_task;
	}
#endif // CONFIG_USBD_VENDOR_BULK_ASYNC_XFER

	rtw_mdelay_os(100);

	printf("\nUSB device vendor demo started...\n");

	rtw_thread_exit();

	return;

#if CONFIG_USBD_VENDOR_BULK_ASYNC_XFER
clear_isoc_async_task:
#endif

#if CONFIG_USBD_VENDOR_ISOC_ASYNC_XFER
	rtw_delete_task(&isoc_async_xfer_task);
clear_intr_async_task:
#endif

#if CONFIG_USBD_VENDOR_INTR_ASYNC_XFER
	rtw_delete_task(&intr_async_xfer_task);
clear_check_status_task:
#endif

#if CONFIG_USBD_VENDOR_HOTPLUG
	rtw_delete_task(&check_status_task);

clear_usb_class_exit:
#endif
	usbd_vendor_deinit();

clear_usb_driver_exit:
	usbd_deinit();

exit:
#if CONFIG_USBD_VENDOR_HOTPLUG
	rtw_free_sema(&vendor_attach_status_changed_sema);
#endif
	rtw_thread_exit();
}

/* Exported functions --------------------------------------------------------*/

void example_usbd_vendor(void)
{
	int status;
	struct task_struct task;

	status = rtw_create_task(&task, "example_usbd_vendor_thread", 1024, CONFIG_USBD_VENDOR_INIT_THREAD_PRIORITY, example_usbd_vendor_thread, NULL);
	if (status != pdPASS) {
		printf("\nFail to create USB vendor device thread: %d\n", status);
	}
}

#endif

