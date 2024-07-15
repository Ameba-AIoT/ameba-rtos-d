/**
  * This module is a confidential and proprietary property of RealTek and
  * possession or use of this module requires written permission of RealTek.
  *
  * Copyright(c) 2021, Realtek Semiconductor Corporation. All rights reserved.
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------ */

#include <platform_opts.h>
#if defined(CONFIG_EXAMPLE_USBD_MSC) && CONFIG_EXAMPLE_USBD_MSC
#include "usbd.h"
#include "usbd_msc.h"
#include "osdep_service.h"

/* Private defines -----------------------------------------------------------*/

// This configuration is used to enable a thread to check hotplug event
// and reset USB stack to avoid memory leak, only for example.
#define CONFIG_USBD_MSC_HOTPLUG						1

// USB speed
#ifdef CONFIG_USB_FS
#define CONFIG_USBD_MSC_SPEED						USB_SPEED_FULL
#else
#define CONFIG_USBD_MSC_SPEED						USB_SPEED_HIGH
#endif

// Thread priorities
#define CONFIG_USBD_MSC_INIT_THREAD_PRIORITY		5
#define CONFIG_USBD_MSC_ISR_THREAD_PRIORITY			7
#define CONFIG_USBD_MSC_HOTPLUG_THREAD_PRIORITY		8 // Should be higher than CONFIG_USBD_MSC_ISR_THREAD_PRIORITY

/* Private types -------------------------------------------------------------*/

/* Private macros ------------------------------------------------------------*/

/* Private function prototypes -----------------------------------------------*/

static void msc_cb_status_changed(u8 status);

/* Private variables ---------------------------------------------------------*/

static usbd_config_t msc_cfg = {
	.speed = CONFIG_USBD_MSC_SPEED,
	.dma_enable = 1U,
	.isr_priority = CONFIG_USBD_MSC_ISR_THREAD_PRIORITY,
	.nptx_max_err_cnt = {0U, 2000U, },
};

static usbd_msc_cb_t msc_cb = {
	.status_changed = msc_cb_status_changed
};

#if CONFIG_USBD_MSC_HOTPLUG
static u8 msc_attach_status;
static _sema msc_attach_status_changed_sema;
#endif

/* Private functions ---------------------------------------------------------*/

static void msc_cb_status_changed(u8 status)
{
	printf("\n[MSC] USB status changed: %d\n", status);
#if CONFIG_USBD_MSC_HOTPLUG
	msc_attach_status = status;
	rtw_up_sema(&msc_attach_status_changed_sema);
#endif
}

#if CONFIG_USBD_MSC_HOTPLUG
static void msc_hotplug_thread(void *param)
{
	int ret = 0;

	UNUSED(param);

	for (;;) {
		if (rtw_down_sema(&msc_attach_status_changed_sema)) {
			if (msc_attach_status == USBD_ATTACH_STATUS_DETACHED) {
				printf("\n[MSC] USB DETACHED\n");
				usbd_msc_deinit();
				ret = usbd_deinit();
				if (ret != 0) {
					printf("\n[MSC] Fail to de-init USBD driver\n");
					break;
				}
				printf("\n[MSC] Free heap size: 0x%lx\n", rtw_getFreeHeapSize());
				ret = usbd_init(&msc_cfg);
				if (ret != 0) {
					printf("\n[MSC] Fail to re-init USBD driver\n");
					break;
				}
				ret = usbd_msc_init(&msc_cb);
				if (ret != 0) {
					printf("\n[MSC] Fail to re-init MSC class\n");
					usbd_deinit();
					break;
				}
			} else if (msc_attach_status == USBD_ATTACH_STATUS_ATTACHED) {
				printf("\n[MSC] USB ATTACHED\n");
			} else {
				printf("\n[MSC] USB INIT\n");
			}
		}
	}

	rtw_thread_exit();
}
#endif // CONFIG_USBD_MSC_HOTPLUG

static void example_usbd_msc_thread(void *param)
{
	int status = 0;
#if CONFIG_USBD_MSC_HOTPLUG
	struct task_struct task;
#endif

	UNUSED(param);

#if CONFIG_USBD_MSC_HOTPLUG
	rtw_init_sema(&msc_attach_status_changed_sema, 0);
#endif

	status = usbd_init(&msc_cfg);
	if (status != HAL_OK) {
		printf("\n[MSC] USB device driver init fail\n");
		goto example_usbd_msc_thread_fail;
	}

	status = usbd_msc_init(&msc_cb);
	if (status != HAL_OK) {
		printf("\n[MSC] USB MSC init fail\n");
		usbd_deinit();
		goto example_usbd_msc_thread_fail;
	}

#if CONFIG_USBD_MSC_HOTPLUG
	status = rtw_create_task(&task, "msc_hotplug_thread", 512, CONFIG_USBD_MSC_HOTPLUG_THREAD_PRIORITY, msc_hotplug_thread, NULL);
	if (status != pdPASS) {
		printf("\n[MSC] USB create status check thread fail\n");
		usbd_msc_deinit();
		usbd_deinit();
		goto example_usbd_msc_thread_fail;
	}
#endif // CONFIG_USBD_MSC_HOTPLUG

	printf("\n[MSC] USBD MSC demo started\n");

	rtw_thread_exit();

	return;

example_usbd_msc_thread_fail:
#if CONFIG_USBD_MSC_HOTPLUG
	rtw_free_sema(&msc_attach_status_changed_sema);
#endif

	rtw_thread_exit();
}

/* Exported functions --------------------------------------------------------*/

void example_usbd_msc(void)
{
	int ret;
	struct task_struct task;

	ret = rtw_create_task(&task, "example_usbd_msc_thread", 1024, CONFIG_USBD_MSC_INIT_THREAD_PRIORITY, example_usbd_msc_thread, NULL);
	if (ret != pdPASS) {
		printf("\n[MSC] USBD MSC create thread fail\n");
	}
}

#endif

