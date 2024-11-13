/**
  ******************************************************************************
  * @file    usbh_cdc_ecm_appx.c
  * @author  Realsil WLAN5 Team
  * @brief   This file provides the functionalities of the USB CDC ECM Class
  ******************************************************************************
  * @attention
  *
  * This module is a confidential and proprietary property of RealTek and
  * possession or use of this module requires written permission of RealTek.
  *
  * Copyright(c) 2024, Realtek Semiconductor Corporation. All rights reserved.
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "usbh.h"
#include "usbh_cdc_ecm.h"
#include "usbh_cdc_ecm_hal.h"
#include "usbh_cdc_ecm_appx.h"

/* Private defines -----------------------------------------------------------*/

/* Private types -------------------------------------------------------------*/

/* Private macros ------------------------------------------------------------*/
#define CDC_SET_LINE_CODING                         0x20U
#define CDC_LINE_CODING_DATA_LEN                    0x07U
/* Private function prototypes -----------------------------------------------*/

/* Private variables ---------------------------------------------------------*/

static const char *TAG = "ECM_APPX";

static usbh_cdc_ecm_appx_t appx_cmd;

static void usbh_cdc_ecm_set_dongle_mac(u8 *mac)
{
	usbh_cdc_ecm_appx_t *atcmd = &appx_cmd;

	if(NULL == mac){
		RTK_LOGE(TAG, "Param error,mac is NULL\n");
		return ;
	}

	memcpy((void *)&(atcmd->mac[0]), (void *)mac, 6);
	atcmd->mac_src_type = CDC_ECM_MAC_UPPER_LAYER_SET;
}

static void usbh_cdc_ecm_set_dongle_led_array(u16 *led, u8 len)
{
	usbh_cdc_ecm_appx_t *atcmd = &appx_cmd;

	if(led == NULL || len == 0){
		RTK_LOGE(TAG, "Param error,led is NULL or len %ld\n", (u32)len);
		return ;
	}

	if(atcmd->led_array){ 
		usb_os_mfree(atcmd->led_array); 
		atcmd->led_array = NULL;
	}

	atcmd->led_array = (u16 *)usb_os_malloc(len*sizeof(u16));
	memcpy((void *)atcmd->led_array, (void *)led, len*sizeof(u16));

	atcmd->led_cnt = len;
}

/**
  * @brief  Start to receive bulk in data
  * @param  none
  * @retval Status
  */
static void usbh_cdc_ecm_appx_rx_thread(void *param)
{
	int i = 0;
	usbh_cdc_ecm_ep_t  *ep ;
	UNUSED(param);
	RTK_LOGD(TAG, "Rx task started \n");

	appx_cmd.task_flag = 1;

	do{
		i++;
		if (usbh_cdc_ecm_check_enum_status() != HAL_OK) { //18s
			if((i%10 == 0)) {
				RTK_LOGD(TAG, "Device disconnected, appx thread sleep!\n");
			}
			usb_os_sleep_ms(1000);
		} else if(usbh_cdc_ecm_get_device_vid() == USB_DEFAULT_VID){
			break;
		} else {
			ep = &appx_cmd.report_ep;//maybe not use this ep
			if(ep->state == CDC_ECM_TRANSFER_STATE_IDLE){
				usbh_cdc_ecm_trigger_transfer(ep);
			}
			ep = &appx_cmd.rx_ep;
			if(ep->state == CDC_ECM_TRANSFER_STATE_IDLE){
				usbh_cdc_ecm_trigger_transfer(ep);
			} 
			usb_os_sleep_ms(1);
		}
	}while(appx_cmd.task_flag);

	RTK_LOGD(TAG, "Rx task exit!\n");

	rtw_thread_exit();
}

/**
  * @brief	Find next standard descriptor interface
  * @param	pbuf: given buffer to find
  			len: given buffer length
  * @retval Pointer of next standard descriptor
  */
static u8 *usbh_cdc_ecm_find_next_ifdesc(u8 *pbuf, u32 *len)
{
	u8 *desc = pbuf;

	while (*len > 0) {
		if (((usb_ecm_descriptor_header *)desc)->bDescriptorType == USB_DESC_TYPE_INTERFACE) {
			return (u8 *) desc;
		} else {
			*len -= ((usb_ecm_descriptor_header *)desc)->bLength;
			desc += ((usb_ecm_descriptor_header *)desc)->bLength;
		}
	}

	return NULL;
}
/**
  * @brief  Get string descriptor by str id
  * @param  host: Host handle
  * @param  pbuf: buffer handle which will save the string value
  * @param  buflen: buffer length
  * @param  str_id: string index
  * @retval Status
  */
static u8 usbh_cdc_acm_process_get_string(usb_host_t *host,u8* pbuf,u16 buflen,u8 str_id)
{
	usbh_setup_req_t setup;

	setup.b.bmRequestType = USB_D2H | USB_REQ_RECIPIENT_DEVICE | USB_REQ_TYPE_STANDARD;
	setup.b.bRequest = USB_REQ_GET_DESCRIPTOR;
	setup.b.wValue = USB_DESC_STRING | str_id;
	setup.b.wIndex = 1U;
	setup.b.wLength = buflen;

	return usbh_ctrl_request(host, &setup, pbuf);
}
static u8 usbh_cdc_ecm_at_set_line_coding(usb_host_t *host)
{
	usbh_setup_req_t setup;
	usbh_cdc_ecm_appx_t *atcmd = &appx_cmd;
	usbh_cdc_ecm_line_coding_t* plinecoding = &atcmd->line_coding;

	plinecoding->b.dwDteRate = 115200;
	plinecoding->b.bCharFormat = CDC_ACM_LINE_CODING_CHAR_FORMAT_1_STOP_BITS;
	plinecoding->b.bParityType = CDC_ACM_LINE_CODING_PARITY_NO;
	plinecoding->b.bDataBits = 8;

	setup.b.bmRequestType = USB_H2D | USB_REQ_TYPE_CLASS | USB_REQ_RECIPIENT_INTERFACE;
	setup.b.bRequest = CDC_SET_LINE_CODING;
	setup.b.wValue = 0U;
	setup.b.wIndex = (u16)atcmd->param_item->at_line_idx;
	setup.b.wLength = CDC_LINE_CODING_DATA_LEN;

	return usbh_ctrl_request(host, &setup, plinecoding->d8);
}
static u8 usbh_cdc_ecm_ep_init(usb_host_t *host,usbh_ep_desc_t *ep_desc,usbh_cdc_ecm_ep_t *ep,u8 ep_type){
	u8 pipe_num;

	ep->ep_addr = ep_desc->bEndpointAddress;
	ep->ep_mps = ep_desc->wMaxPacketSize;
	ep->ep_interval = ep_desc->bInterval;

	pipe_num = usbh_alloc_pipe(host, ep->ep_addr);
	if (pipe_num != 0xFFU) {
		ep->pipe_id = pipe_num;
	} else {
		RTK_LOGE(TAG,  "Fail to allocate pipe for CDC ECM class.");
		return HAL_ERR_MEM;
	}

	usbh_open_pipe(host,
	   ep->pipe_id,
	   ep->ep_addr,
	   ep_type,
	   ep->ep_mps);

	return HAL_OK;
}
static u8 usbh_cdc_ecm_get_mac_str(usb_host_t *host)
{
	u8 i;
	u8 mac_is_valid = 0;
	u8 status = HAL_OK;
	usbh_cdc_ecm_appx_t *atcmd = &appx_cmd;

	if(atcmd->mac_src_type == CDC_ECM_MAC_UPPER_LAYER_SET){
		status = HAL_OK;
		RTK_LOGI(TAG, "Upper set mac[%02x %02x %02x %02x %02x %02x]\n", atcmd->mac[0], atcmd->mac[1], atcmd->mac[2], atcmd->mac[3], atcmd->mac[4], atcmd->mac[5]);
	} else {
		status = usbh_cdc_acm_process_get_string(host, atcmd->dongle_ctrl_buf, CDC_ECM_MAC_STRING_LEN, atcmd->iMACAddressStringId);	
		atcmd->mac_src_type = CDC_ECM_MAC_DONGLE_SUPPLY;
	}

	if (status == HAL_OK) {
		if(atcmd->mac_src_type == CDC_ECM_MAC_DONGLE_SUPPLY){
			for (i = 0; i < 6; i++) {
				atcmd->mac[i] = usbh_cdc_ecm_hex_to_char(atcmd->dongle_ctrl_buf[2 + 4 * i]) * 16 + usbh_cdc_ecm_hex_to_char(atcmd->dongle_ctrl_buf[2 + 4 * i + 2]);
				if(atcmd->mac[i]){
					mac_is_valid = 1;
				}
			}
			if(mac_is_valid == 0){
				rtw_get_random_bytes(atcmd->mac,CDC_ECM_MAC_STR_LEN);
				RTK_LOGI(TAG, "Random mac[%02x %02x %02x %02x %02x %02x]\n", atcmd->mac[0], atcmd->mac[1], atcmd->mac[2], atcmd->mac[3], atcmd->mac[4], atcmd->mac[5]);
				atcmd->mac_src_type = CDC_ECM_MAC_RANDOM_SET;
			} else {
				RTK_LOGI(TAG, "Dongle mac[%02x %02x %02x %02x %02x %02x]\n", atcmd->mac[0], atcmd->mac[1], atcmd->mac[2], atcmd->mac[3], atcmd->mac[4], atcmd->mac[5]);
				atcmd->mac_valid = 1;
			}
		}
	} 

	return status;
}

/******************Set MAC for 8152 *****************************/
/**
  * @brief  Set 8152 mac flow ctrl
  * @param  host: Host handle
  * @retval Status
  */
static int usbh_cdc_ecm_process_mac_get_lock(usb_host_t *host)
{
	usbh_setup_req_t setup;

	setup.b.bmRequestType = USB_D2H | USB_REQ_TYPE_VENDOR | USB_REQ_RECIPIENT_DEVICE;
	setup.b.bRequest = 0x05;
	setup.b.wValue = 0xE81C;
	setup.b.wIndex = 0x010F;
	setup.b.wLength = 4;

	return usbh_ctrl_request(host, &setup, appx_cmd.dongle_ctrl_buf);
}

static int usbh_cdc_ecm_process_mac_set_dis_lock(usb_host_t *host)
{
	usbh_setup_req_t setup;

	setup.b.bmRequestType = 0x40;
	setup.b.bRequest = 0x05;
	setup.b.wValue = 0xE81C;
	setup.b.wIndex = 0x010F;
	setup.b.wLength = 4;
	return usbh_ctrl_request(host, &setup, appx_cmd.dongle_ctrl_buf);
}

static int usbh_cdc_ecm_process_mac_set_mac1(usb_host_t *host)
{
	usbh_setup_req_t setup;

	setup.b.bmRequestType = 0x40;
	setup.b.bRequest = 0x05;
	setup.b.wValue = 0xC000;
	setup.b.wIndex = 0x010F;
	setup.b.wLength = 4;
	return usbh_ctrl_request(host, &setup, appx_cmd.dongle_ctrl_buf);
}

static int usbh_cdc_ecm_process_mac_set_mac2(usb_host_t *host)
{
	usbh_setup_req_t setup;

	setup.b.bmRequestType = 0x40;
	setup.b.bRequest = 0x05;
	setup.b.wValue = 0xC004;
	setup.b.wIndex = 0x0103;
	setup.b.wLength = 4;
	return usbh_ctrl_request(host, &setup, appx_cmd.dongle_ctrl_buf);
}

static int usbh_cdc_ecm_process_mac_en_lock(usb_host_t *host)
{
	usbh_setup_req_t setup;

	setup.b.bmRequestType = 0x40;
	setup.b.bRequest = 0x05;
	setup.b.wValue = 0xE81C;
	setup.b.wIndex = 0x010F;
	setup.b.wLength = 4;
	return usbh_ctrl_request(host, &setup, appx_cmd.dongle_ctrl_buf);
}

/**
  * @brief  Set led color for 8152
  * @param  host: Host handle
  * @retval Status
  */
static int usbh_cdc_ecm_process_led_set_ctrl(usb_host_t *host)
{
	usbh_setup_req_t setup;

	setup.b.bmRequestType = 0x40;
	setup.b.bRequest = 0x05;
	setup.b.wValue = 0xDD90;
	setup.b.wIndex = 0x0103;
	setup.b.wLength = 4;
	return usbh_ctrl_request(host, &setup, appx_cmd.dongle_ctrl_buf);
}

static u8 usbh_cdc_ecm_appx_setting(usb_host_t *host,u8* next)
{
	u8 state = HAL_OK;
	u8 loop = 1;
	u16 vid = usbh_cdc_ecm_get_device_vid();
	u16 pid = usbh_cdc_ecm_get_device_pid();

	if((CDC_ECM_STATE_AT_SETTING_IDLE == appx_cmd.sub_status)
	   || (usbh_cdc_ecm_get_ctrl_transfer_owner() != CDC_ECM_CTRL_XFER_OWNER_APPX))
		return state;

	if(vid == USB_DEFAULT_VID){ //rtk
		switch (appx_cmd.sub_status) {
		case CDC_ECM_STATE_GET_MAC_STR:
			state = usbh_cdc_ecm_get_mac_str(host);
			if (state == HAL_OK) {
				if((appx_cmd.mac_valid == 1) && (appx_cmd.mac_src_type == CDC_ECM_MAC_DONGLE_SUPPLY)){
					appx_cmd.sub_status = CDC_ECM_STATE_CTRL_LED_COLOR_SET;
				} else {
					appx_cmd.sub_status = CDC_ECM_STATE_CTRL_MAC_GET_LOCK;
				}
			} else if (state != HAL_BUSY) {
				RTK_LOGW(TAG,  "Get MAC fail error[%d]\n",state);
				usb_os_sleep_ms(10);
			}
			break;

		case CDC_ECM_STATE_CTRL_MAC_GET_LOCK: //8152 mac
			if ((pid != 0x8152) || (appx_cmd.mac_src_type == CDC_ECM_MAC_DONGLE_SUPPLY)) {
				appx_cmd.sub_status++;
			} else {
				usb_os_memset(appx_cmd.mac_ctrl_lock, 0, 4);
				state = usbh_cdc_ecm_process_mac_get_lock(host);
				if (state == HAL_OK) {
					usb_os_memcpy(appx_cmd.mac_ctrl_lock, appx_cmd.dongle_ctrl_buf, CDC_ECM_MAC_CTRL_REG_LEN);
					appx_cmd.sub_status++;
				} else if (state != HAL_BUSY) {
					RTK_LOGE(TAG, "Get MAC lock err\n");
					usb_os_sleep_ms(100);
				}
			}
			break;

		case CDC_ECM_STATE_CTRL_MAC_DISABLE_LOCK: //8152 mac
			if ((pid != 0x8152) || (appx_cmd.mac_src_type == CDC_ECM_MAC_DONGLE_SUPPLY)) {
				appx_cmd.sub_status++;
			} else {
				appx_cmd.mac_ctrl_lock[0] = 0xD0;
				usb_os_memcpy(appx_cmd.dongle_ctrl_buf, appx_cmd.mac_ctrl_lock, CDC_ECM_MAC_CTRL_REG_LEN);
				state = usbh_cdc_ecm_process_mac_set_dis_lock(host);
				if (state == HAL_OK) {
					appx_cmd.sub_status++;
				} else if (state != HAL_BUSY) {
					RTK_LOGE(TAG, "Dis MAC lock err\n");
					usb_os_sleep_ms(100);
				}
			}
			break;

		case CDC_ECM_STATE_CTRL_MAC_SET_MAC1: //8152 mac
			if ((pid != 0x8152) || (appx_cmd.mac_src_type == CDC_ECM_MAC_DONGLE_SUPPLY)) {
				appx_cmd.sub_status++;
			} else {
				usb_os_memcpy(appx_cmd.dongle_ctrl_buf, &(appx_cmd.mac[0]), CDC_ECM_MAC_CTRL_REG_LEN);
				state = usbh_cdc_ecm_process_mac_set_mac1(host);
				if (state == HAL_OK) {
					appx_cmd.sub_status++;
				} else if (state != HAL_BUSY) {
					RTK_LOGE(TAG, "Set MAC1 err\n");
					usb_os_sleep_ms(100);
				}
			}
			break;

		case CDC_ECM_STATE_CTRL_MAC_SET_MAC2: //8152 mac
			if ((pid != 0x8152) ||  (appx_cmd.mac_src_type == CDC_ECM_MAC_DONGLE_SUPPLY)) {
				appx_cmd.sub_status++;
			} else {
				usb_os_memcpy(appx_cmd.dongle_ctrl_buf, &(appx_cmd.mac[4]), CDC_ECM_MAC_CTRL_REG_LEN);
				appx_cmd.dongle_ctrl_buf[2]=appx_cmd.dongle_ctrl_buf[3]=0xFF;
				state = usbh_cdc_ecm_process_mac_set_mac2(host);
				if (state == HAL_OK) {
					appx_cmd.sub_status++;
				} else if (state != HAL_BUSY) {
					RTK_LOGE(TAG, "Set MAC2 err\n");
					usb_os_sleep_ms(100);
				}
			}
			break;

		case CDC_ECM_STATE_CTRL_MAC_ENABLE_LOCK: //8152 mac
			if ((pid != 0x8152) || (appx_cmd.mac_src_type == CDC_ECM_MAC_DONGLE_SUPPLY)) {
				appx_cmd.sub_status++;
			} else {
				appx_cmd.mac_ctrl_lock[0] = 0x10;
				usb_os_memcpy(appx_cmd.dongle_ctrl_buf, appx_cmd.mac_ctrl_lock, CDC_ECM_MAC_CTRL_REG_LEN);
				state = usbh_cdc_ecm_process_mac_en_lock(host);
				if (state == HAL_OK) {
					appx_cmd.sub_status++;
					appx_cmd.mac_valid = 1;
					RTK_LOGI(TAG, "Mac set success\n");
				} else if (state != HAL_BUSY) {
					RTK_LOGE(TAG, "En MAC lock err\n");
					usb_os_sleep_ms(100);
				}
			}
			break;

		case CDC_ECM_STATE_CTRL_LED_COLOR_SET: //8152 led ctrl
			if ((pid != 0x8152) || (appx_cmd.led_cnt == 0) || (appx_cmd.led_array == NULL)) {
				appx_cmd.sub_status = CDC_ECM_STATE_AT_SETTING_IDLE;
			} else {
				usb_os_memset(appx_cmd.dongle_ctrl_buf, 0xFF, 4);
				usb_os_memcpy(appx_cmd.dongle_ctrl_buf, (u8 *)&(appx_cmd.led_array[0]), 2);
				state = usbh_cdc_ecm_process_led_set_ctrl(host);
				if (state == HAL_OK) {
					appx_cmd.sub_status = CDC_ECM_STATE_AT_SETTING_IDLE;
				} else if (state != HAL_BUSY) {
					RTK_LOGE(TAG, "Set led color err\n");
					usb_os_sleep_ms(100);
				}
			}
			break;

		default:
			usbh_cdc_ecm_change_ctrl_transfer_owner();
			loop = 0 ;
			break;
		}
	}
	else if((vid == USB_EF_DONGLE_VID)&&(pid == EF_DONGLE_PID_EG915)){
		switch (appx_cmd.sub_status) {
		case CDC_ECM_STATE_GET_MAC_STR:
			state = usbh_cdc_ecm_get_mac_str(host);
			if (state == HAL_OK) {
				appx_cmd.sub_status = CDC_ECM_STATE_AT_SET_LINE_CODING;
				RTK_LOGD(TAG,  "Get MAC success\n");
			} else if (state != HAL_BUSY) {
				RTK_LOGW(TAG,  "Get MAC fail error[%d]\n",state);
				usb_os_sleep_ms(10);
			}
			break;
		case CDC_ECM_STATE_AT_SET_LINE_CODING:
			state = usbh_cdc_ecm_at_set_line_coding(host);
			if (state == HAL_OK) {
				appx_cmd.sub_status = CDC_ECM_STATE_AT_SETTING_IDLE;
				RTK_LOGD(TAG,  "Set line coding success\n");
			} else if (state != HAL_BUSY) {
				RTK_LOGW(TAG,  "Set line coding error[%d]\n",state);
				usb_os_sleep_ms(100);
			}
			break;
		default:
			usbh_cdc_ecm_change_ctrl_transfer_owner();
			loop = 0 ;
			break;
		}
	} else if((vid == USB_EF_DONGLE_VID)&&(pid == EF_DONGLE_PID_EG91)){
		switch (appx_cmd.sub_status) {
		case CDC_ECM_STATE_GET_MAC_STR:
			state = HAL_OK;
			appx_cmd.sub_status = CDC_ECM_STATE_AT_SET_LINE_CODING;
			break;
		case CDC_ECM_STATE_AT_SET_LINE_CODING:
			state = usbh_cdc_ecm_at_set_line_coding(host);
			if (state == HAL_OK) {
				appx_cmd.sub_status = CDC_ECM_STATE_AT_SETTING_IDLE;
				RTK_LOGD(TAG, "Set line coding success\n");
			} else if (state != HAL_BUSY) {
				RTK_LOGW(TAG, "Set line coding error[%d]\n",state);
				usb_os_sleep_ms(100);
			}
			break;
		default:
			usbh_cdc_ecm_change_ctrl_transfer_owner();
			loop = 0 ;
			break;
		}
	}
	if(loop){
		*next = 1;
	}

	return state;
}
/**
  * @brief  Bulk Out handling function
  * @param  host: Host handle
  * @retval Status
*/
static void usbh_cdc_ecm_at_bulk_out(usbh_cdc_ecm_host_t *ecm,u8* next)
{
	u8 loop =1;
	usb_host_t *host = ecm->host;
	usbh_cdc_ecm_appx_t *atcmd = &appx_cmd;
	usbh_cdc_ecm_ep_t  *ep = &atcmd->tx_ep;
	usbh_urb_state_t urb_state = USBH_URB_IDLE;

	switch (ep->state) {
	case CDC_ECM_TRANSFER_STATE_XFER:
		ep->state = CDC_ECM_TRANSFER_STATE_BUSY;
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
		break;

	case CDC_ECM_TRANSFER_STATE_BUSY:
		urb_state = usbh_get_urb_state(host, ep->pipe_id);
		if (urb_state == USBH_URB_DONE) {
			if (ep->xfer_len >= ep->ep_mps) {
				ep->xfer_len -= ep->ep_mps;
				ep->xfer_buf += ep->ep_mps;
				ep->state = CDC_ECM_TRANSFER_STATE_XFER;
			} else {
				appx_cmd.data_tx_done = 1;
				ep->xfer_len = 0U;
				ep->state = CDC_ECM_TRANSFER_STATE_IDLE;
			}
		} else if (urb_state == USBH_URB_BUSY) { //retransmit
			ep->state = CDC_ECM_TRANSFER_STATE_XFER;
		} else if ((urb_state == USBH_URB_ERROR) || (urb_state == USBH_URB_STALL)) {
			ep->state = CDC_ECM_TRANSFER_STATE_IDLE;
			appx_cmd.data_tx_done = 1;
			RTK_LOGE(TAG, "BULK out failed %d\n", urb_state);
		}
		break;

	default:
		loop = 0;
		break;
	}

	if(loop){
		*next = 1;
	}
}


/**
  * @brief  Bulk In handling  function
  * @param  host: Host handle
  * @retval Status
*/
static void usbh_cdc_ecm_at_bulk_in(usbh_cdc_ecm_host_t *ecm,u8* next)
{
	u8 loop =1;
	usb_host_t *host = ecm->host;
	usbh_cdc_ecm_appx_t *atcmd = &appx_cmd;
	usbh_cdc_ecm_ep_t  *ep = &atcmd->rx_ep;
	usbh_urb_state_t urb_state = USBH_URB_IDLE;
	u32 len;

	switch (ep->state) {
	case CDC_ECM_TRANSFER_STATE_XFER:
		usbh_bulk_receive_data(host,
			   ep->xfer_buf,
			   ep->ep_mps,
			   ep->pipe_id);
		ep->state = CDC_ECM_TRANSFER_STATE_BUSY;
		break;

	case CDC_ECM_TRANSFER_STATE_BUSY:
		urb_state = usbh_get_urb_state(host, ep->pipe_id);
		if (urb_state == USBH_URB_DONE) {
			len = usbh_get_last_transfer_size(host, ep->pipe_id);
			if (atcmd->rx_report != NULL) {
				atcmd->rx_report(ep->xfer_buf, len,USB_CH_EP_TYPE_BULK);
			}
			if (len >= ep->ep_mps ) {
				ep->state = CDC_ECM_TRANSFER_STATE_XFER;
			} else {
				ep->state = CDC_ECM_TRANSFER_STATE_IDLE;  //set rx to accelerate speed
			}
		} else if (urb_state == USBH_URB_BUSY) {
			usb_os_sleep_ms(1);
		} else if ((urb_state == USBH_URB_ERROR) || (urb_state == USBH_URB_STALL)) {
			ep->state = CDC_ECM_TRANSFER_STATE_IDLE;
			RTK_LOGW(TAG, "BULK in failed %d\n", urb_state);
		}
		break;

	default:
		loop = 0;
		break;
	}

	if(loop){
		*next = 1;
	}
}
/**
  * @brief  Intr in function
  * @param  host: Host handle
  * @retval Status
*/
static void usbh_cdc_ecm_at_intr_in(usbh_cdc_ecm_host_t *ecm,u8* next)
{
	usb_host_t *host = ecm->host;
	usbh_cdc_ecm_appx_t *atcmd = &appx_cmd;
	usbh_cdc_ecm_ep_t  *ep = &atcmd->report_ep;
	usbh_urb_state_t urb_state = USBH_URB_IDLE;
	u32 len;

	switch (ep->state) {
	case CDC_ECM_TRANSFER_STATE_XFER:
		usbh_intr_receive_data(host,
			   ep->xfer_buf,
			   ep->ep_mps,
			   ep->pipe_id);

		ep->state = CDC_ECM_TRANSFER_STATE_BUSY;
		*next = 1;
		break;

	case CDC_ECM_TRANSFER_STATE_BUSY:
		urb_state = usbh_get_urb_state(host, ep->pipe_id);
		if (urb_state == USBH_URB_DONE) {
			len = usbh_get_last_transfer_size(host, ep->pipe_id);
			if (atcmd->rx_report != NULL) {
				atcmd->rx_report(ep->xfer_buf, len,USB_CH_EP_TYPE_INTR);
			}
			if (len >= ep->ep_mps) {
				ep->state = CDC_ECM_TRANSFER_STATE_XFER;
			} else {
				ep->state = CDC_ECM_TRANSFER_STATE_IDLE;
			}
			*next = 1;
		} else if (urb_state == USBH_URB_BUSY) {
		} else if ((urb_state == USBH_URB_ERROR) || (urb_state == USBH_URB_STALL)) {
			RTK_LOGW(TAG, "INTR in failed %d\n", urb_state);
		}
		else if(urb_state == USBH_URB_IDLE) {
			//todo fix this issue:
			//while run usbh_intr_receive_data , but the interrupt not happen
		}
		break;

	default:
		break;
	}
}
//bulk out
static u8 usbh_cdc_ecm_appx_send(u8 *buf,u32 len){
	u8 ret = HAL_BUSY;
	usbh_cdc_ecm_appx_t *atcmd = &appx_cmd;
	usbh_cdc_ecm_ep_t  *ep = &atcmd->tx_ep;

	if(ep->state == CDC_ECM_TRANSFER_STATE_IDLE){
		ep->xfer_buf = buf;
		ep->xfer_len = len;
		ret = usbh_cdc_ecm_trigger_transfer(ep);
	}else {
		RTK_LOGW(TAG, "BULK Out busy %d\n",ep->state);
	}

	return ret;
}



static u8 usbh_cdc_ecm_parse_get_mac_id(usb_host_t *host)
{
	u8 find_mac = 0 ;
	u32 cfglen;
	usbh_cfg_desc_t *desc;
	usbh_if_desc_t *pbuf;
	usbh_cdc_ecm_appx_t *atcmd = &appx_cmd;

	/*
		parse to get mac info
	*/
	desc = (usbh_cfg_desc_t *)usbh_get_raw_configuration_descriptor(host);
	cfglen = (u32)desc->wTotalLength;
	pbuf = (usbh_if_desc_t *) desc;

	while (!find_mac) {
		pbuf = (usbh_if_desc_t *)usbh_cdc_ecm_find_next_ifdesc((u8 *)pbuf, &cfglen);
		if (pbuf == NULL) {
			break;
		}
		if((pbuf->bInterfaceClass == CDC_IF_CDC_CTRL_CODE) //find the right interface
			&&(pbuf->bInterfaceSubClass == CDC_IF_CDC_CTRL_SUB_CLASS_ECM_CODE)){ 
			cfglen -= pbuf->bLength;
			pbuf = (usbh_if_desc_t *)((u8*)pbuf + pbuf->bLength);
			while(!find_mac) {
				if( USB_DT_CS_INTERFACE == ((usbh_cdc_ecm_network_func_t *)pbuf)->bDescriptorType
				  && CDC_ECM_NETWORK_FUNC_DESCRIPTOR == ((usbh_cdc_ecm_network_func_t *)pbuf)->bDescriptorSubtype) {
				  	atcmd->iMACAddressStringId = ((usbh_cdc_ecm_network_func_t*)pbuf)->iMACAddress;
					RTK_LOGD(TAG, "Mac string id(%d)\n",atcmd->iMACAddressStringId);
					find_mac = 1;
					break;
				} else{
					cfglen -= ((usb_ecm_descriptor_header *) pbuf)->bLength;
					pbuf = (usbh_if_desc_t *)((u8*)pbuf + ((usb_ecm_descriptor_header *) pbuf)->bLength);
				}
			}
		} else {
			cfglen -= pbuf->bLength;
			pbuf = (usbh_if_desc_t *)((u8*)pbuf + pbuf->bLength);
		}
	}

	return HAL_OK;
}

/* Exported functions --------------------------------------------------------*/
/*
	below is the public apis that called by other file
*/
u8 usbh_cdc_ecm_appx_doinit(usbh_cdc_ecm_priv_data_t *priv)
{
	usbh_cdc_ecm_appx_t *atcmd = &appx_cmd;

	if(priv == NULL) {
		RTK_LOGE(TAG, "Param error\n");
		return HAL_ERR_PARA;
	}

	if(priv->mac_value){
		usbh_cdc_ecm_set_dongle_mac(priv->mac_value);
	}
	if((priv->led_array != NULL) && (priv->led_cnt > 0)){
		usbh_cdc_ecm_set_dongle_led_array(priv->led_array,priv->led_cnt);
	}

	atcmd->rx_report = priv->appx_cb;

	atcmd->dongle_ctrl_buf = (u8 *)usb_os_malloc(CDC_ECM_MAC_STRING_LEN);
	if (NULL == atcmd->dongle_ctrl_buf) {
		RTK_LOGE(TAG, "Alloc mem %d fail\n", CDC_ECM_MAC_STRING_LEN);
		return HAL_ERR_MEM;
	}

	atcmd->mac_valid = 0;

	return HAL_OK;
}

u8 usbh_cdc_ecm_appx_deinit(usb_host_t *host){
	usbh_cdc_ecm_appx_t *atcmd = &appx_cmd;

	appx_cmd.task_flag = 0;
	usbh_cdc_ecm_appx_deinit_pipe(host);

	if(atcmd->dongle_ctrl_buf) {
		usb_os_mfree(atcmd->dongle_ctrl_buf);
		atcmd->dongle_ctrl_buf = NULL;
	}

	if(atcmd->report_ep.xfer_buf) {
		usb_os_mfree(atcmd->report_ep.xfer_buf);
		atcmd->report_ep.xfer_buf = NULL;
	}
	if(atcmd->rx_ep.xfer_buf) {
		usb_os_mfree(atcmd->rx_ep.xfer_buf);
		atcmd->rx_ep.xfer_buf = NULL;
	}

	return HAL_OK;
}
u8 usbh_cdc_ecm_parse_at_cfgdesc(usb_host_t *host,u16 vid,u16 pid){
	u8 ret = HAL_OK;
	usbh_cdc_ecm_appx_t *pappx_cmd = &appx_cmd;
	usbh_if_desc_t *comm_if_desc;
	usbh_ep_desc_t *ep_desc;
	u8 ep_id;
	usbh_cdc_ecm_appx_param_t *data;
	u8 status = HAL_ERR_UNKNOWN;

	pappx_cmd->param_item = NULL;
	data = pappx_cmd->param_array;
	while (data->vid != 0) {
		if((data->vid == vid)&&((vid == USB_DEFAULT_VID)||(data->pid == pid))){ /*rtk dongle*/
			pappx_cmd->param_item = data;
			break;
		}
		data++;
	}
	if(NULL == pappx_cmd->param_item){
		RTK_LOGW(TAG, "Can not choose the right vid(%d)pid(%d) config.\n",vid,pid);
		pappx_cmd->param_item = pappx_cmd->param_array;
	}

	RTK_LOGD(TAG, "Choose vid(0x%x)pid(0x%x)atid(%d) \n",pappx_cmd->param_item->vid,pappx_cmd->param_item->pid,pappx_cmd->param_item->at_line_idx);
	if(vid == USB_DEFAULT_VID){
		//parse to get the mac string
		usbh_cdc_ecm_parse_get_mac_id(host);
		pappx_cmd->sub_status = CDC_ECM_STATE_GET_MAC_STR;
	} else {
		/* Get Communication Interface */
		comm_if_desc = usbh_get_interface_descriptor(host, pappx_cmd->param_item->at_line_idx, 0);///at port
		if (comm_if_desc == NULL) {
			RTK_LOGE(TAG, "Fail to get the %d common interface.\n",pappx_cmd->param_item->at_line_idx);
			return status;
		}

		if(comm_if_desc->bNumEndpoints != 3){
			RTK_LOGW(TAG, "Maybe Error Happen(id=%d)\n",comm_if_desc->bNumEndpoints);
		}
		for(ep_id = 0 ; ep_id <comm_if_desc->bNumEndpoints ; ep_id++){
			ep_desc = &comm_if_desc->ep_desc_array[ep_id];

			if(ep_desc->bmAttributes == USB_CH_EP_TYPE_BULK){
				if(USB_EP_IS_IN(ep_desc->bEndpointAddress)){
					usbh_cdc_ecm_ep_init(host,ep_desc,&pappx_cmd->rx_ep,USB_CH_EP_TYPE_BULK);
				} else {
					usbh_cdc_ecm_ep_init(host,ep_desc,&pappx_cmd->tx_ep,USB_CH_EP_TYPE_BULK);
				}
			} else if(ep_desc->bmAttributes == USB_CH_EP_TYPE_INTR){
				usbh_cdc_ecm_ep_init(host,ep_desc,&pappx_cmd->report_ep,USB_CH_EP_TYPE_INTR);
			} else {
				RTK_LOGW(TAG, "Unknown support transfer type(%d)\n",ep_desc->bmAttributes);
			}
		}

		pappx_cmd->report_ep.xfer_buf = (u8*)usb_os_malloc(pappx_cmd->report_ep.ep_mps);
		pappx_cmd->rx_ep.xfer_buf = (u8*)usb_os_malloc(pappx_cmd->rx_ep.ep_mps);

		usbh_cdc_ecm_parse_get_mac_id(host);

		pappx_cmd->sub_status = CDC_ECM_STATE_GET_MAC_STR;
	}

	return ret;
}
u8 usbh_cdc_ecm_appx_deinit_pipe(usb_host_t *host){
	usbh_cdc_ecm_appx_t *pappx_cmd = &appx_cmd;

	if (pappx_cmd->report_ep.pipe_id) {
		usbh_close_pipe(host, pappx_cmd->report_ep.pipe_id);
		usbh_free_pipe(host, pappx_cmd->report_ep.pipe_id);
		pappx_cmd->report_ep.pipe_id = 0U;
	}
	if (pappx_cmd->rx_ep.pipe_id) {
		usbh_close_pipe(host, pappx_cmd->rx_ep.pipe_id);
		usbh_free_pipe(host, pappx_cmd->rx_ep.pipe_id);
		pappx_cmd->rx_ep.pipe_id = 0U;
	}
	if (pappx_cmd->tx_ep.pipe_id) {
		usbh_close_pipe(host, pappx_cmd->tx_ep.pipe_id);
		usbh_free_pipe(host, pappx_cmd->tx_ep.pipe_id);
		pappx_cmd->tx_ep.pipe_id = 0U;
	}

	return HAL_OK;
}
u8 usbh_cdc_ecm_appx_transfer(usbh_cdc_ecm_host_t *ecm,u8* next){
	usbh_cdc_ecm_appx_setting(ecm->host,next);
	if(usbh_cdc_ecm_get_device_vid() == USB_DEFAULT_VID){
		return HAL_OK;
	}else {
		usbh_cdc_ecm_at_bulk_out(ecm,next);
		usbh_cdc_ecm_at_bulk_in(ecm,next);
		usbh_cdc_ecm_at_intr_in(ecm,next);
		return HAL_OK;
	}
}

u8 usbh_cdc_ecm_appx_task(void){
	int status;
	struct task_struct appx_task;

	status = rtw_create_task(&appx_task, "ecm_arx_thread", 256, USBH_ECM_APP_RX_THREAD_PRIORITY, (thread_func_t)usbh_cdc_ecm_appx_rx_thread, NULL);
	if (status != pdPASS) {
		RTK_LOGE(TAG, "Fail to create rx thread\n");
		return HAL_ERR_UNKNOWN;
	}

	return HAL_OK;
}


/* Exported functions public api ----------------------------------------*/
/**
  * @brief  get the mac str, if the dongle support the standard CDC ECM
  * @retval mac string,the length is 6 Bytes
  */
const u8* usbh_cdc_ecm_process_mac_str(void)
{
	u8 i = 0;
	usbh_cdc_ecm_appx_t *pappx_cmd = &appx_cmd;
	while(!pappx_cmd->mac_valid && i < 10)
	{
		usb_os_sleep_ms(1000);
		i++;
		RTK_LOGD(TAG, "Wait to get MAC string!\n");
	}
	return pappx_cmd->mac;
}
/**
  * @brief  send the data out throught at port endpoint
  * @retval return the status 0 send success,else fail
  */
u8 usbh_cdc_ecm_appx_send_data(u8 *buf, u32 len){
	u8 ret = HAL_TIMEOUT;
	u32 retry_cnt = 0;
	RTK_LOGD(TAG, "Try to send msg(%s)\n",buf);

	appx_cmd.data_tx_done = 0;
	while(1){
		ret = usbh_cdc_ecm_appx_send(buf,len);
		if(ret == HAL_OK){
			break;
		}
		if(++retry_cnt > 100){ //check 1s
			RTK_LOGI(TAG, "TX drop ! \n");
			ret = HAL_TIMEOUT;
			break;
		} else  {
			usb_os_sleep_ms(10);
		}
	}
	//wait reply success,wait signal
	if(0 == HAL_OK){
		while(0 == appx_cmd.data_tx_done){
			usb_os_sleep_ms(1);
		};
	}
	RTK_LOGD(TAG, "Send msg(%s) success \n",buf);
	return ret;
}
/**
  * @brief  set the params for ecm appx, use to find out the right dongle type
  * @retval HAL_OK
  */
u8 usbh_cdc_ecm_appx_set_param(usbh_cdc_ecm_appx_param_t *param){
	usbh_cdc_ecm_appx_t *atcmd = &appx_cmd;
	atcmd->param_array = param;
	return HAL_OK;
}
