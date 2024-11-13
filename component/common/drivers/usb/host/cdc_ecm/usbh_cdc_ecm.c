/**
  ******************************************************************************
  * @file    usbh_cdc_ecm.c
  * @author  Realsil WLAN5 Team
  * @brief   This file provides the functionalities of the USB CDC ECM Class
  ******************************************************************************
  * @attention
  *
  * This module is a confidential and proprietary property of RealTek and
  * possession or use of this module requires written permission of RealTek.
  *
  * Copyright(c) 2023, Realtek Semiconductor Corporation. All rights reserved.
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/

#include "usbh_cdc_ecm.h"
#include "usb_os.h"
#include "usbh.h"
#include "usbh_cdc_ecm_hal.h"
#include "usbh_cdc_ecm_appx.h"

/* Private defines -----------------------------------------------------------*/
#define ECM_ENABLE_DUMP_DESCRIPYOT_PARSE (0)

/* Private types -------------------------------------------------------------*/

/* Private macros ------------------------------------------------------------*/

/* Private function prototypes -----------------------------------------------*/
static u8 usbh_cdc_ecm_attach(usb_host_t *host);
static u8 usbh_cdc_ecm_detach(usb_host_t *host);
static u8 usbh_cdc_ecm_process(usb_host_t *host);
static u8 usbh_cdc_ecm_setup(usb_host_t *host);
static u8 usbh_cdc_ecm_sof(usb_host_t *host);
static u8 usbh_cdc_ecm_nak(usb_host_t *host, u8 pipe_num);
static void usbh_cdc_ecm_process_bulk_out(usb_host_t *host);
static void usbh_cdc_ecm_process_bulk_in(usb_host_t *host);
static void usbh_cdc_ecm_process_intr_in(usb_host_t *host);
static u8 usbh_cdc_ecm_process_get_statistic(usb_host_t *host);
static u8 usbh_cdc_ecm_process_set_packet_filter(usb_host_t *host);
static u8 usbh_cdc_ecm_process_set_alt(usb_host_t *host);
static u8 usbh_cdc_ecm_process_set_muiticast_filter(usb_host_t *host);
static u8 usbh_cdc_ecm_ctrl_setting(usbh_cdc_ecm_host_t *cdc,u8 *next);
static void usbh_cdc_ecm_next_loop(void);

/* Private variables ---------------------------------------------------------*/

static const char *TAG = "ECM";

/* USB Standard Device Descriptor */
static usbh_class_driver_t usbh_cdc_ecm_driver = {
	.class_code = CDC_CLASS_CODE,
	.attach = usbh_cdc_ecm_attach,
	.detach = usbh_cdc_ecm_detach,
	.setup = usbh_cdc_ecm_setup,
	.process = usbh_cdc_ecm_process,
	.sof = usbh_cdc_ecm_sof,
	.nak = usbh_cdc_ecm_nak,
};

static usbh_cdc_ecm_host_t usbh_cdc_ecm_host;

/* Private functions ---------------------------------------------------------*/

/**
  * @brief Trigger a notify class state change
  * @retval None
  */
static void usbh_cdc_ecm_next_loop(void){
	usbh_cdc_ecm_host_t *cdc = &usbh_cdc_ecm_host;
	usbh_notify_class_state_change(cdc->host);
}

static void usbh_cdc_ecm_deinit_pipe(usb_host_t *host){
	usbh_cdc_ecm_host_t *cdc = &usbh_cdc_ecm_host;

	if (cdc->report_ep.pipe_id) {
		usbh_close_pipe(host, cdc->report_ep.pipe_id);
		usbh_free_pipe(host, cdc->report_ep.pipe_id);
		cdc->report_ep.pipe_id = 0U;
	}
	if (cdc->tx_ep.pipe_id) {
		usbh_close_pipe(host, cdc->tx_ep.pipe_id);
		usbh_free_pipe(host, cdc->tx_ep.pipe_id);
		cdc->tx_ep.pipe_id = 0U;
	}
	if (cdc->rx_ep.pipe_id) {
		usbh_close_pipe(host, cdc->rx_ep.pipe_id);
		usbh_free_pipe(host, cdc->rx_ep.pipe_id);
		cdc->rx_ep.pipe_id = 0U;
	}
}

u8 usbh_cdc_ecm_get_epinfo(usb_host_t *host){
	usbh_cdc_ecm_host_t 	     *ecm = &usbh_cdc_ecm_host;

	usbh_if_desc_t *comm_if_desc;
	usbh_ep_desc_t *ep_desc;
	usbh_dev_desc_t *pdesc;
	u8 status = HAL_ERR_UNKNOWN;
	u8 ep_id;
	u8 interface;

	pdesc = usbh_get_device_descriptor(ecm->host);
	ecm->vid = pdesc->idVendor;
	ecm->pid = pdesc->idProduct;

	/* Get interface index as per supported class & protocol */

	/*
		cdc ecm ctrl
	*/
	interface = usbh_get_interface(host,
			   CDC_IF_CDC_CTRL_CODE,
			   CDC_IF_CDC_CTRL_SUB_CLASS_ECM_CODE,
			   0x00);
	if (interface == 0xFFU) {
		RTK_LOGE(TAG,  "Fail to get the ecm ctrl interface.\n");
		return status;
	}

	comm_if_desc = usbh_get_interface_descriptor(host, interface, 0);
	if (comm_if_desc == NULL) {
		RTK_LOGE(TAG,  "Fail to get the ecm ctrl common interface.\n");
		return status;
	}

	/* get notification endpoint */
	ep_desc = &comm_if_desc->ep_desc_array[0];
	if (USB_EP_IS_IN(ep_desc->bEndpointAddress)) {
		ecm->report_ep.ep_addr = ep_desc->bEndpointAddress;
		ecm->report_ep.ep_mps = ep_desc->wMaxPacketSize;
		ecm->report_ep.ep_interval  = ep_desc->bInterval;
	}

	/*
		cdc ecm data
	*/
	interface = usbh_get_interface(host,
			   CDC_IF_CDC_DATA_CODE,
			   CDC_IF_CDC_DATA_SUB_CLASS_DATA_CODE,
			   0x00);
	if (interface == 0xFFU) {
		RTK_LOGW(TAG,  "Fail to get the ecm data interface[0].");
		return status;
	}

	comm_if_desc = usbh_get_interface_descriptor(host, interface, 1);
	if (comm_if_desc == NULL) {
		RTK_LOGE(TAG,  "Fail to get the ecm data interface[1].");
		return status;
	}

	ecm->ecm_if = comm_if_desc->bInterfaceNumber;
	ecm->ecm_alt = comm_if_desc->bAlternateSetting;
	RTK_LOGD(TAG,  "Get ECM if(%d)alt(%d)\n",ecm->ecm_if,ecm->ecm_alt);

	/* get transfor endpoint */
	for(ep_id = 0; ep_id<2;ep_id++){
		ep_desc = &comm_if_desc->ep_desc_array[ep_id];
		if (USB_EP_IS_IN(ep_desc->bEndpointAddress)) {
			ecm->rx_ep.ep_addr = ep_desc->bEndpointAddress;
			ecm->rx_ep.ep_mps = ep_desc->wMaxPacketSize;
			ecm->rx_ep.ep_interval  = ep_desc->bInterval;
		}else{
			ecm->tx_ep.ep_addr = ep_desc->bEndpointAddress;
			ecm->tx_ep.ep_mps = ep_desc->wMaxPacketSize;
			ecm->tx_ep.ep_interval  = ep_desc->bInterval;
		}
	}

	return HAL_OK;
}

/**
  * @brief  Usb Attach callback function
  * @param  host: Host handle
  * @retval Status
  */
static u8 usbh_cdc_ecm_attach(usb_host_t *host)
{
	u8 status = HAL_ERR_UNKNOWN;
	u8 pipe_num;
	u32 max_ep_size;

	usbh_cdc_ecm_host_t      *ecm = &usbh_cdc_ecm_host;
	ecm->host = host;

	if (host->config.speed == USB_SPEED_HIGH) {
		max_ep_size = USB_HS_MAX_PACKET_SIZE;
	} else {
		max_ep_size = USB_FS_MAX_PACKET_SIZE;
	}

	status = usbh_cdc_ecm_get_epinfo(host);
	if (status) {
		RTK_LOGE(TAG, "Fail to parse ecm class specific descriptor\n");
		return status;
	}

	status = usbh_cdc_ecm_parse_at_cfgdesc(host,ecm->vid,ecm->pid);
	if (status) {
		RTK_LOGE(TAG,  "Fail to parse ecm class specific descriptor\n");
		return status;
	}
	//limited the max ep size
	if(ecm->report_ep.ep_mps >= max_ep_size)
		ecm->report_ep.ep_mps = max_ep_size;
	if(ecm->tx_ep.ep_mps >= max_ep_size)
		ecm->tx_ep.ep_mps = max_ep_size;
	if(ecm->rx_ep.ep_mps >= max_ep_size)
		ecm->rx_ep.ep_mps = max_ep_size;

	//control
	pipe_num = usbh_alloc_pipe(host, ecm->report_ep.ep_addr);
	if (pipe_num != 0xFFU) {
		ecm->report_ep.pipe_id = pipe_num;
	} else {
		RTK_LOGE(TAG,  "Fail to allocate INTR IN pipe for CDC ECM class.");
		return HAL_ERR_MEM;
	}
	usbh_open_pipe(host,
			   ecm->report_ep.pipe_id,
			   ecm->report_ep.ep_addr,
			   USB_CH_EP_TYPE_INTR,
			   ecm->report_ep.ep_mps);

	//data
	pipe_num = usbh_alloc_pipe(host, ecm->tx_ep.ep_addr);
	if (pipe_num != 0xFFU) {
		ecm->tx_ep.pipe_id = pipe_num;
	} else {
		RTK_LOGE(TAG,  "Fail to allocate BULK OUT pipe for CDC ECM class.");
		usbh_free_pipe(host, ecm->report_ep.pipe_id);
		return HAL_ERR_MEM;
	}

	pipe_num = usbh_alloc_pipe(host, ecm->rx_ep.ep_addr);
	if (pipe_num != 0xFFU) {
		ecm->rx_ep.pipe_id = pipe_num;
	} else {
		RTK_LOGE(TAG, "Fail to allocate BULK IN pipe for CDC ECM class.");
		usbh_free_pipe(host, ecm->report_ep.pipe_id);
		usbh_free_pipe(host, ecm->tx_ep.pipe_id);
		return HAL_ERR_MEM;
	}

	usbh_open_pipe(host,
			   ecm->tx_ep.pipe_id,
			   ecm->tx_ep.ep_addr,
			   USB_CH_EP_TYPE_BULK,
			   ecm->tx_ep.ep_mps);

	usbh_open_pipe(host,
			   ecm->rx_ep.pipe_id,
			   ecm->rx_ep.ep_addr,
			   USB_CH_EP_TYPE_BULK,
			   ecm->rx_ep.ep_mps);

	ecm->rx_ep.xfer_buf = (u8*)usb_os_malloc(ecm->rx_ep.ep_mps);
	ecm->report_ep.xfer_buf = (u8*)usb_os_malloc(ecm->report_ep.ep_mps);

	ecm->state = CDC_ECM_STATE_IDLE;
	ecm->usbh_state = USBH_CDC_ECM_ATTACH;

	if ((ecm->cb != NULL) && (ecm->cb->attach != NULL)) {
		ecm->cb->attach();
	}

	return HAL_OK;
}


/**
  * @brief  Usb Detach callback function.
  * @param  host: Host handle
  * @retval HAL_OK
  */
static u8 usbh_cdc_ecm_detach(usb_host_t *host)
{
	usbh_cdc_ecm_host_t *cdc = &usbh_cdc_ecm_host;
	cdc->usbh_state = USBH_CDC_ECM_DETACHED;

	usbh_cdc_ecm_deinit_pipe(host);
	usbh_cdc_ecm_appx_deinit_pipe(host);

	if ((cdc->cb != NULL) && (cdc->cb->detach != NULL)) {
		cdc->cb->detach();
	}

	return HAL_OK;
}

/**
  * @brief  Usb Setup callback function.
  * @param  host: Host handle
  * @retval Status
  */
static u8 usbh_cdc_ecm_setup(usb_host_t *host)
{
	UNUSED(host);
	usbh_cdc_ecm_host_t *cdc = &usbh_cdc_ecm_host;
	
	if ((cdc->cb != NULL) && (cdc->cb->setup != NULL)) {
		cdc->cb->setup();
	}
	cdc->usbh_state = USBH_CDC_ECM_SETUP;

	return HAL_OK;
}
/**
  * @brief  Usb Sof callback function.
  * @param  host: Host handle
  * @retval HAL_OK
  */
static u8 usbh_cdc_ecm_sof(usb_host_t *host)
{
	u8 i = 0;
	usbh_cdc_ecm_host_t *cdc = &usbh_cdc_ecm_host;

	for (i = 0U; i < host->config.pipes; i++) {
		if (cdc->host_pipe[i]) {
			cdc->host_pipe[i] = 0;
			usbh_reactivate_pipe(host, i);
		}
	}

	return HAL_OK;
}
/**
  * @brief  Usb nak callback function.
  * @param  host: Host handle
  * @param  pipe_num: Pipe number
  * @retval return OK while success, else return HAL_ERR_UNKNOWN
  */
static u8 usbh_cdc_ecm_nak(usb_host_t *host, u8 pipe_num)
{
	u8 ep_type ;
	usbh_cdc_ecm_host_t *cdc = &usbh_cdc_ecm_host;
	if (pipe_num > CDC_ECM_HOST_PIPE_COUNT) {
		return HAL_ERR_PARA ;
	}

	ep_type = usbh_get_ep_type(host, pipe_num);
	if (ep_type == USB_CH_EP_TYPE_BULK || ep_type == USB_CH_EP_TYPE_INTR) {
		//do not retransmit in this loop, it will do in next SOF
		cdc->host_pipe[pipe_num] = 1;
		return HAL_OK;
	}

	return HAL_ERR_UNKNOWN ;
}

static u8 usbh_cdc_ecm_ctrl_setting(usbh_cdc_ecm_host_t *cdc,u8 *next)
{
	usb_host_t *host = cdc->host;
	u8 req_status = HAL_OK;
	u8 loop=1;

	switch (cdc->sub_state) {
	case CDC_ECM_STATE_CTRL_ALT_SETTING:
		req_status = usbh_cdc_ecm_process_set_alt(host);
		if (req_status == HAL_OK) {
			cdc->sub_state = CDC_ECM_STATE_CTRL_IDLE;
			usbh_cdc_ecm_change_ctrl_transfer_owner(); //switch to appx ctrl transfer
			RTK_LOGD(TAG,  "ECM alt setting success\n");
		} else if (req_status != HAL_BUSY) {
			RTK_LOGW(TAG,  "ECM alt setting error\n");
			usb_os_sleep_ms(100);
		}
		break;

	case CDC_ECM_STATE_CTRL_SET_ETHERNET_MULTICAST_FILTER:
		req_status = usbh_cdc_ecm_process_set_muiticast_filter(host);
		if (req_status == HAL_OK) {
			cdc->sub_state = CDC_ECM_STATE_CTRL_IDLE;
			RTK_LOGD(TAG,  "ECM set eth multicast filter success\n");
		} else if (req_status != HAL_BUSY) {
			RTK_LOGW(TAG,  "ECM set eth multicast filter error\n");
			usb_os_sleep_ms(100);
		}
		break;

	case CDC_ECM_STATE_CTRL_SET_ETHERNET_PACKET_FILTER:
		req_status = usbh_cdc_ecm_process_set_packet_filter(host);
		if (req_status == HAL_OK) {
			cdc->sub_state = CDC_ECM_STATE_CTRL_IDLE;
			RTK_LOGD(TAG,  "ECM set eth packet filter success\n");
		} else if (req_status != HAL_BUSY) {
			RTK_LOGW(TAG,  "ECM set eth packet filter error\n");
			usb_os_sleep_ms(100);
		}
		break;

	case CDC_ECM_STATE_CTRL_GET_ETHERNET_STATISTIC:
		req_status = usbh_cdc_ecm_process_get_statistic(host);
		if (req_status == HAL_OK) {
			cdc->sub_state = CDC_ECM_STATE_CTRL_IDLE;
			RTK_LOGD(TAG,  "ECM get eth statistic success\n");
		} else if (req_status != HAL_BUSY) {
			RTK_LOGW(TAG,  "ECM get eth statistic error\n");
			usb_os_sleep_ms(100);
		}
		break;

	default:
		loop = 0;
		break;
	}
	if(loop){
		*next =1;
	}

	return req_status;
}
static u8 usbh_cdc_ecm_transfer(usb_host_t *host,u8 *next)
{
	usbh_cdc_ecm_host_t *cdc = &usbh_cdc_ecm_host;
	if(usbh_cdc_ecm_get_ctrl_transfer_owner() == CDC_ECM_CTRL_XFER_OWNER_ECM){
		usbh_cdc_ecm_ctrl_setting(cdc,next);
	}
	usbh_cdc_ecm_process_bulk_in(host);
	usbh_cdc_ecm_process_bulk_out(host);
	usbh_cdc_ecm_process_intr_in(host);
	return HAL_OK;
}
/**
  * @brief  Usb State Machine handling callback
  * @param  host: Host handle
  * @retval Status
  */
static u8 usbh_cdc_ecm_process(usb_host_t *host)
{
	u8 req_status = HAL_OK;
	usbh_cdc_ecm_host_t *cdc = &usbh_cdc_ecm_host;

	switch (cdc->state) {

	case CDC_ECM_STATE_IDLE:
		break;

	case CDC_ECM_STATE_SWITCH_TO_TRANSFER:
		cdc->state = CDC_ECM_STATE_TRANSFER;
		usbh_cdc_ecm_next_loop();
		break;

	case CDC_ECM_STATE_TRANSFER:
		usbh_cdc_ecm_appx_transfer(cdc,&usbh_cdc_ecm_host.next_transfor);
		usbh_cdc_ecm_transfer(host,&usbh_cdc_ecm_host.next_transfor);

		if(usbh_cdc_ecm_host.next_transfor) {
			usbh_cdc_ecm_host.next_transfor = 0;
			usbh_cdc_ecm_next_loop();
		} else {
			//no message, sleep to relase CPU
		}
		break;

	case CDC_ECM_STATE_ERROR:
		req_status = usbh_ctrl_clear_feature(host, 0x00U);
		if (req_status == HAL_OK) {
			cdc->state = CDC_ECM_STATE_IDLE;
		}
		break;

	default:
		break;

	}

	return req_status;
}

/**
  * @brief  Usb alt setting function
  * @param  host: Host handle
  * @retval Status
*/
static u8 usbh_cdc_ecm_process_set_alt(usb_host_t *host)
{
	usbh_cdc_ecm_host_t *cdc = &usbh_cdc_ecm_host;
	usbh_setup_req_t setup;

	setup.b.bmRequestType = USB_H2D | USB_REQ_TYPE_STANDARD | USB_REQ_RECIPIENT_INTERFACE;
	setup.b.bRequest = USB_REQ_SET_INTERFACE;
	setup.b.wValue = cdc->ecm_alt;
	setup.b.wIndex = cdc->ecm_if;
	setup.b.wLength = 0U;

	return usbh_ctrl_request(host, &setup, NULL);
}

/**
  * @brief  Bulk Out handling function
  * @param  host: Host handle
  * @retval Status
*/
static void usbh_cdc_ecm_process_bulk_out(usb_host_t *host)
{
	usbh_cdc_ecm_host_t *cdc = &usbh_cdc_ecm_host;
	usbh_cdc_ecm_ep_t  *ep = &cdc->tx_ep;
	usbh_urb_state_t urb_state = USBH_URB_IDLE;

	switch (ep->state) {
	case CDC_ECM_TRANSFER_STATE_XFER:
		if (ep->xfer_len > ep->ep_mps) {
			usbh_bulk_send_data(host,
					ep->xfer_buf,
					ep->ep_mps,
					ep->pipe_id);
		} else {
			usbh_bulk_send_data(host,
					ep->xfer_buf,
					ep->xfer_len,
					ep->pipe_id);
		}
		ep->state = CDC_ECM_TRANSFER_STATE_BUSY;
		cdc->next_transfor = 1;
		break;

	case CDC_ECM_TRANSFER_STATE_BUSY:
		urb_state = usbh_get_urb_state(host, ep->pipe_id);
		if (urb_state == USBH_URB_DONE) {
			if (ep->xfer_len >= ep->ep_mps) {
				ep->xfer_len -= ep->ep_mps;
				ep->xfer_buf += ep->ep_mps;
				ep->state = CDC_ECM_TRANSFER_STATE_XFER;
			} else {
				ep->xfer_len = 0U;
				ep->state = CDC_ECM_TRANSFER_STATE_IDLE;
				if ((cdc->cb != NULL) && (cdc->cb->bulk_transmitted != NULL)) {
					cdc->cb->bulk_transmitted(urb_state);
				}
			}
			cdc->next_transfor = 1;
		} else if (urb_state == USBH_URB_BUSY) { //retransmit
			ep->state = CDC_ECM_TRANSFER_STATE_XFER;
			cdc->next_transfor = 1;
		} else if ((urb_state == USBH_URB_ERROR) || (urb_state == USBH_URB_STALL)) {
			ep->state = CDC_ECM_TRANSFER_STATE_IDLE;
			if ((cdc->cb != NULL) && (cdc->cb->bulk_transmitted != NULL)) {
				cdc->cb->bulk_transmitted(urb_state);
			}
			RTK_LOGW(TAG, "BULK out failed %d\n", urb_state);
		}
		break;

	default:
		break;
	}
}


/**
  * @brief  Bulk In handling  function
  * @param  host: Host handle
  * @retval Status
*/
static void usbh_cdc_ecm_process_bulk_in(usb_host_t *host)
{
	usbh_cdc_ecm_host_t *cdc = &usbh_cdc_ecm_host;
	usbh_cdc_ecm_ep_t  *ep = &cdc->rx_ep;
	usbh_urb_state_t urb_state = USBH_URB_IDLE;
	u32 len;

	switch (ep->state) {
	case CDC_ECM_TRANSFER_STATE_XFER:
		usbh_bulk_receive_data(host,
			   ep->xfer_buf,
			   ep->ep_mps,
			   ep->pipe_id);
		ep->state = CDC_ECM_TRANSFER_STATE_BUSY;
		cdc->next_transfor = 1;
		break;

	case CDC_ECM_TRANSFER_STATE_BUSY:
		urb_state = usbh_get_urb_state(host, ep->pipe_id);
		if (urb_state == USBH_URB_DONE) {
			len = usbh_get_last_transfer_size(host, ep->pipe_id);
			if ((cdc->cb != NULL) && (cdc->cb->bulk_received != NULL)) {
				cdc->cb->bulk_received(ep->xfer_buf, len);
			}
			if (len >= ep->ep_mps ) {	//it is not the last packet
				ep->state = CDC_ECM_TRANSFER_STATE_XFER;
			} else {
				ep->state = CDC_ECM_TRANSFER_STATE_IDLE;  //set rx to accelerate speed
			}
			cdc->next_transfor = 1;
		} else if (urb_state == USBH_URB_BUSY) {
		} else if ((urb_state == USBH_URB_ERROR) || (urb_state == USBH_URB_STALL)) {
			ep->state = CDC_ECM_TRANSFER_STATE_IDLE;
			RTK_LOGW(TAG, "BULK in failed %d\n", urb_state);
		}
		break;

	default:
		break;
	}
}
/**
  * @brief  Intr in function
  * @param  host: Host handle
  * @retval Status
*/
static void usbh_cdc_ecm_process_intr_in(usb_host_t *host)
{
	usbh_cdc_ecm_host_t *cdc = &usbh_cdc_ecm_host;
	usbh_cdc_ecm_ep_t  *ep = &cdc->report_ep;
	usbh_urb_state_t urb_state = USBH_URB_IDLE;
	u32 len;

	switch (ep->state) {
	case CDC_ECM_TRANSFER_STATE_XFER:
		usbh_intr_receive_data(host,
			   ep->xfer_buf,
			   ep->ep_mps,
			   ep->pipe_id);

		ep->state = CDC_ECM_TRANSFER_STATE_BUSY;
		cdc->next_transfor = 1;
		break;

	case CDC_ECM_TRANSFER_STATE_BUSY:
		urb_state = usbh_get_urb_state(host, ep->pipe_id);
		if (urb_state == USBH_URB_DONE) {
			len = usbh_get_last_transfer_size(host, ep->pipe_id);
			if (len && (cdc->cb != NULL) && (cdc->cb->intr_received != NULL)) {
				cdc->cb->intr_received(ep->xfer_buf, len);
			}
			if (len >= ep->ep_mps) {
				ep->state = CDC_ECM_TRANSFER_STATE_XFER;
			} else {
				ep->state = CDC_ECM_TRANSFER_STATE_IDLE;
			}
			cdc->next_transfor = 1;
		} else if (urb_state == USBH_URB_BUSY) {
		} else if ((urb_state == USBH_URB_ERROR) || (urb_state == USBH_URB_STALL)) {
			ep->state = CDC_ECM_TRANSFER_STATE_IDLE;
			RTK_LOGW(TAG, "INTR in failed %d\n", urb_state);
		} else if (urb_state == USBH_URB_IDLE) {
			//todo fix this issue:
			//while run usbh_intr_receive_data , but the interrupt not happen
			cdc->intr_idle_state_cnt ++ ;
			if (cdc->intr_idle_state_cnt > 10) {
				cdc->intr_idle_state_cnt = 0  ;
				ep->state = CDC_ECM_TRANSFER_STATE_IDLE;
				cdc->next_transfor = 1;
			}
		}
		break;

	default:
		break;
	}
}

/**
  * @brief  Set Ethernet Packet Filter
  * @param  host: Host handle
  * @retval Status
  */
static u8 usbh_cdc_ecm_process_set_packet_filter(usb_host_t *host)
{
	usbh_setup_req_t setup;
	usbh_cdc_ecm_host_t *cdc = &usbh_cdc_ecm_host;

	setup.b.bmRequestType = USB_H2D | USB_REQ_TYPE_CLASS | USB_REQ_RECIPIENT_INTERFACE;
	setup.b.bRequest = CDC_ECM_SET_ETHERNET_PACKET_FILTER;
	setup.b.wValue = cdc->packet_filter;
	setup.b.wIndex = 1U;
	setup.b.wLength = 0;

	return usbh_ctrl_request(host, &setup, NULL);
}
/**
  * @brief  Set Ethernet Muticast Filter
  * @param  host: Host handle
  * @retval Status
  */
static u8 usbh_cdc_ecm_process_set_muiticast_filter(usb_host_t *host)
{
	usbh_setup_req_t setup;
	usbh_cdc_ecm_host_t *cdc = &usbh_cdc_ecm_host;

	setup.b.bmRequestType = USB_H2D | USB_REQ_TYPE_CLASS | USB_REQ_RECIPIENT_INTERFACE;
	setup.b.bRequest = CDC_ECM_SET_ETHERNET_MULTICAST_FILTERS;
	setup.b.wValue = 1;
	setup.b.wIndex = 1U;
	setup.b.wLength = cdc->muticast_filter_len;

	return usbh_ctrl_request(host, &setup, cdc->muticast_filter);
}

/**
  * @brief  Get Ethernet Statistic
  * @param  host: Host handle
  * @retval Status
  */
static u8 usbh_cdc_ecm_process_get_statistic(usb_host_t *host)
{
	usbh_setup_req_t setup;
	usbh_cdc_ecm_host_t *cdc = &usbh_cdc_ecm_host;

	setup.b.bmRequestType = USB_D2H | USB_REQ_TYPE_CLASS | USB_REQ_RECIPIENT_INTERFACE;
	setup.b.bRequest = CDC_ECM_GET_ETHERNET_STATISTIC;
	setup.b.wValue = cdc->feature_selector;
	setup.b.wIndex = 1U;
	setup.b.wLength = 4;

	//Todo (u8 *)&cdc->eth_statistic_count
	//ECM issue, not support right now
	return usbh_ctrl_request(host, &setup, NULL);
}

static void usbh_cdc_ecm_intr_rx_thread(void *param)
{
	int i = 0;
	usbh_cdc_ecm_host_t *cdc = &usbh_cdc_ecm_host;
	usbh_cdc_ecm_ep_t *ep = &(cdc->report_ep);
	UNUSED(param);
	cdc->notify_task_flag = 1;

	RTK_LOGD(TAG, "INTR rx task started\n");
	do{
		i++;
		if (usbh_cdc_ecm_check_enum_status() != HAL_OK) { //18s
			if((i%10 == 0)) {
				RTK_LOGD(TAG, "Device disconnected, INTR test wait\n");
			}
			usb_os_sleep_ms(1000);
		} else if(ep){
			if(ep->state == CDC_ECM_TRANSFER_STATE_IDLE) {
				usbh_cdc_ecm_trigger_transfer(ep);
			} else {
				usb_os_sleep_ms(1*50);//should sleep by bInterval value
			}
		}
	}while(cdc->notify_task_flag);

	RTK_LOGI(TAG, "INTR rx task exit\n");
	rtw_thread_exit();
}

static void usbh_cdc_ecm_bulk_rx_thread(void *param)
{
	u32 i = 0;
	UNUSED(param);
	usbh_cdc_ecm_host_t *cdc = &usbh_cdc_ecm_host;
	usbh_cdc_ecm_ep_t *ep = &(cdc->rx_ep);
	cdc->rx_task_flag = 1;

	RTK_LOGD(TAG, "BULK send task started \n");
	do{
		i++;
		if (usbh_cdc_ecm_check_enum_status() != HAL_OK) {
			if((i%10 == 0)) {
				RTK_LOGD(TAG, "Device disconnected, bulk test wait\n");
			}
			usb_os_sleep_ms(1000);
		} else if(ep) {
			if(ep->state == CDC_ECM_TRANSFER_STATE_IDLE) {
				usbh_cdc_ecm_trigger_transfer(ep);
			} else {
				usb_os_sleep_ms(1*1);
			}
		} 
		//usb_os_sleep_ms(1000);
	}while(cdc->rx_task_flag);

	RTK_LOGI(TAG, "BULK rx task exit\n");
	rtw_thread_exit();
}

/* Exported functions --------------------------------------------------------*/
/*
	below is the private apis that called by ecm_hal
*/
u8 usbh_cdc_ecm_task(void){
	int status;
	struct task_struct intr_task;
	struct task_struct bulk_task;

	status = rtw_create_task(&intr_task, "ecm_irx_thread", 256, USBH_ECM_RX_THREAD_PRIORITY, (thread_func_t)usbh_cdc_ecm_intr_rx_thread, NULL);
	if (status != pdPASS) {
		RTK_LOGE(TAG, "Fail to create INTR Rx thread\n");
		return HAL_ERR_UNKNOWN;
	}

	status = rtw_create_task(&bulk_task, "ecm_brx_thread", 256, USBH_ECM_RX_THREAD_PRIORITY, (thread_func_t)usbh_cdc_ecm_bulk_rx_thread, NULL);
	if (status != pdPASS) {
		RTK_LOGE(TAG, "Fail to create BULK RX thread\n");
		goto delete_intr_task_exit;
	}

	//start the at cmd task
	if (usbh_cdc_ecm_appx_task() != HAL_OK) {
		RTK_LOGE(TAG, "Fail to create sub thread\n");
		goto delete_bukk_task_exit;
	}

	RTK_LOGI(TAG, "All task create success !\n");

	return HAL_OK;

delete_bukk_task_exit:
	rtw_delete_task(&bulk_task);

delete_intr_task_exit:
	rtw_delete_task(&intr_task);

	return HAL_ERR_UNKNOWN;
}


/**
  * @brief  Init CDC ECM class
  * @param  cb: User callback
  * @retval Status
  */
u8 usbh_cdc_ecm_init(usbh_cdc_ecm_state_cb_t *cb)
{
	u8 ret = HAL_OK;
	usbh_cdc_ecm_host_t *cdc = &usbh_cdc_ecm_host;
	usb_os_memset(cdc, 0x00, sizeof(usbh_cdc_ecm_host_t));

	if (cb != NULL) {
		cdc->cb = cb;
		if (cb->init != NULL) {
			ret = cb->init();
			if (ret != HAL_OK) {
				RTK_LOGE(TAG, "CDC ECM user init error: %d", ret);
				return ret;
			}
		}
	}
	cdc->usbh_state = USBH_CDC_ECM_IDLE;
	cdc->ctrl_transfer_owner = CDC_ECM_CTRL_XFER_OWNER_ECM;

	usbh_register_class(&usbh_cdc_ecm_driver);

	return ret;
}

/**
  * @brief  Deinit CDC ECM class
  * @retval Status
  */
u8 usbh_cdc_ecm_deinit(void)
{
	u8 ret = HAL_OK;
	usbh_cdc_ecm_host_t *cdc = &usbh_cdc_ecm_host;
	usb_host_t *host = cdc->host;

	//stop the task
	cdc->rx_task_flag = 0;
	cdc->notify_task_flag = 0;

	if ((cdc->cb != NULL) && (cdc->cb->deinit != NULL)) {
		cdc->cb->deinit();
	}

	if ((host != NULL) && (host->state == USBH_CLASS_READY)) {
		cdc->state = CDC_ECM_STATE_IDLE;
		usbh_cdc_ecm_deinit_pipe(host);
	}

	usbh_cdc_ecm_appx_deinit(host);	

	if(cdc->rx_ep.xfer_buf) {
		usb_os_mfree(cdc->rx_ep.xfer_buf);
		cdc->rx_ep.xfer_buf = NULL;
	}
	if(cdc->report_ep.xfer_buf) {
		usb_os_mfree(cdc->report_ep.xfer_buf);
		cdc->report_ep.xfer_buf = NULL;
	}

	usbh_unregister_class(&usbh_cdc_ecm_driver);

	return ret;
}

/**
  * @brief  Start to send bulk out data
  * @param  buf: Data buffer
  * @param  len: Data length
  * @retval Status
  */
u8 usbh_cdc_ecm_bulk_send(u8 *buf, u32 len)
{
	u8 ret = HAL_BUSY;
	usbh_cdc_ecm_host_t *cdc = &usbh_cdc_ecm_host;
	usbh_cdc_ecm_ep_t  *ep = &cdc->tx_ep;

	if(ep->state == CDC_ECM_TRANSFER_STATE_IDLE){
		ep->xfer_buf = buf;
		ep->xfer_len = len;
		ret = usbh_cdc_ecm_trigger_transfer(ep);
	}else {
		RTK_LOGD(TAG, "BULK ep tx not idle(%d)\n",ep->state);
	}

	return ret;
}

/**
  * @brief  set the alternate setting
  * @param  NONE
  * @retval Status
  */
u8 usbh_cdc_ecm_set_alt_setting(void)
{
	usbh_cdc_ecm_host_t *cdc = &usbh_cdc_ecm_host;
	usb_host_t *host = cdc->host;
	int ret = HAL_ERR_UNKNOWN;

	if (usbh_cdc_ecm_check_enum_status() != HAL_OK) {
		return ret;
	}

	if ((host->state == USBH_CLASS_REQUEST) || (host->state == USBH_CLASS_READY)) {
		cdc->state = CDC_ECM_STATE_SWITCH_TO_TRANSFER;
		cdc->sub_state = CDC_ECM_STATE_CTRL_ALT_SETTING;
		usbh_cdc_ecm_next_loop();
		ret = HAL_OK;
	}

	return ret;
}


/* Exported functions --------------------------------------------------------*/
/*
	below is the public apis that called by other file
*/
/**
  * @brief  check USB device enum status
  * @retval return HAL_OK if enum success, else return HAL_BUSY
  */
u8 usbh_cdc_ecm_check_enum_status(void)
{
	usbh_cdc_ecm_host_t *cdc = &usbh_cdc_ecm_host;

	if(cdc->usbh_state < USBH_CDC_ECM_SETUP)
	{
		RTK_LOGD(TAG, "Device not setup success,wait...\n");
		return HAL_BUSY;
	}

	return HAL_OK;
}
/**
  * @brief  return bulkin MPS
  * @retval Status
  */
u16 usbh_cdc_ecm_get_usbin_mps(void)
{
	u32 i = 0;
	usbh_cdc_ecm_host_t *cdc = &usbh_cdc_ecm_host;
	while (cdc->usbh_state < USBH_CDC_ECM_SETUP && i < 10) {
		usb_os_sleep_ms(1000);
		i++;
		RTK_LOGW(TAG, "ECM hostState=%d,want=%d,keep wait ...\n", cdc->usbh_state, USBH_CDC_ECM_SETUP);
	}
	return cdc->rx_ep.ep_mps;
}


/**
  * @brief  choose the right config descriptor index while there has more then one config descriptor
  * @param  host: Host handle
  * @param  cfg_num: config index
  * @retval Status
  */
u8 usbh_cdc_ecm_choose_config(usb_host_t *host)
{
	//choose ecm cfg_num
	u8 cfg_id = usbh_get_cfgid_from_subclass(host,CDC_ETHERNET_NETWORK_CONTROL_MODEL);
	//RTK_LOGD(TAG, "Choose the cfgid(%d)\n",cfg_id);
	return usbh_set_configuration(host,cfg_id);
}

u8 usbh_cdc_ecm_change_ctrl_transfer_owner(void){
	u8 old_step;
	usbh_cdc_ecm_host_t *cdc = &usbh_cdc_ecm_host;
	old_step = cdc->ctrl_transfer_owner;
	cdc->ctrl_transfer_owner ++ ;

	return old_step;
}
u8 usbh_cdc_ecm_get_ctrl_transfer_owner(void){
	usbh_cdc_ecm_host_t *cdc = &usbh_cdc_ecm_host;
	return cdc->ctrl_transfer_owner;
}
/**
  * @brief Trigger a transfer for the EP
  * @param  ep: ep handle
  * @retval status
  */
u8 usbh_cdc_ecm_trigger_transfer(usbh_cdc_ecm_ep_t  *ep)
{
	usbh_cdc_ecm_host_t *cdc = &usbh_cdc_ecm_host;
	u8 ret = HAL_BUSY;

	if (usbh_cdc_ecm_check_enum_status() != HAL_OK || ep->state != CDC_ECM_TRANSFER_STATE_IDLE) {
		RTK_LOGW(TAG, "Transfer out fail(%d)\n",ep->state);
		return ret;
	}

	if ((cdc->state == CDC_ECM_STATE_IDLE) || (cdc->state == CDC_ECM_STATE_TRANSFER)) {
		ep->state = CDC_ECM_TRANSFER_STATE_XFER;
		cdc->state = CDC_ECM_STATE_TRANSFER;
		usbh_cdc_ecm_next_loop();
		ret = HAL_OK;
	} else {
		RTK_LOGW(TAG, "ECM process not ready(%d)\n",cdc->state);
	}

	return ret;
}
/**
  * @brief return the device vid
  * @retval
  */
u16 usbh_cdc_ecm_get_device_vid(void)
{
	usbh_cdc_ecm_host_t *cdc = &usbh_cdc_ecm_host;
	return cdc->vid;
}
/**
  * @brief return the device pid
  * @retval
  */
u16 usbh_cdc_ecm_get_device_pid(void)
{
	usbh_cdc_ecm_host_t *cdc = &usbh_cdc_ecm_host;
	return cdc->pid;
}
