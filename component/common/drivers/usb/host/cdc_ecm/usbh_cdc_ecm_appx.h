/**
  ******************************************************************************
  * The header file for usbh_cdc_ecm_appx.h
  *
  * This module is a confidential and proprietary property of RealTek and
  * possession or use of this module requires written permission of RealTek.
  *
  * Copyright(c) 2024, Realtek Semiconductor Corporation. All rights reserved.
  ******************************************************************************
  */

#ifndef USBH_CDC_ECM_APPX_H
#define USBH_CDC_ECM_APPX_H

/* Includes ------------------------------------------------------------------*/

#include "usbh.h"

#define CDC_ECM_MAC_STRING_LEN                                  (26)
#define CDC_ECM_MAC_STR_LEN                                     (6)
#define CDC_ECM_LINE_CODING_DATA_LEN                            0x07U

/* Exported defines ----------------------------------------------------------*/
typedef enum {
    USBH_ECM_APP_RX_THREAD_PRIORITY = USBH_ECM_RX_THREAD_PRIORITY-1 ,   
} usbh_cdc_ecm_appx_task_priority_t;

/* Line coding char format */
typedef enum {
    CDC_ACM_LINE_CODING_CHAR_FORMAT_1_STOP_BITS    = 0U,
    CDC_ACM_LINE_CODING_CHAR_FORMAT_1_5_STOP_BITS,
    CDC_ACM_LINE_CODING_CHAR_FORMAT_2_STOP_BITS
} usbh_cdc_acm_line_coding_char_format_t;

/* Line coding parity type */
typedef enum {
    CDC_ACM_LINE_CODING_PARITY_NO   = 0U,
    CDC_ACM_LINE_CODING_PARITY_ODD,
    CDC_ACM_LINE_CODING_PARITY_EVEN,
    CDC_ACM_LINE_CODING_PARITY_MARK,
    CDC_ACM_LINE_CODING_PARITY_SPACE
} usbh_cdc_acm_line_coding_parity_t;
/* Line coding structure */
typedef union {
    u8 d8[CDC_ECM_LINE_CODING_DATA_LEN];
    struct {
        u32 dwDteRate;    /* Data terminal rate, in bits per second */
        u8  bCharFormat;  /* Stop bits */
        u8  bParityType;  /* Parity */
        u8  bDataBits;    /* Data bits (5, 6, 7, 8 or 16) */
    } b;
} usbh_cdc_ecm_line_coding_t;

/* Exported types ------------------------------------------------------------*/
typedef enum {
    CDC_ECM_STATE_AT_SETTING_IDLE      = 0U,
    CDC_ECM_STATE_GET_MAC_STR,
    CDC_ECM_STATE_AT_SET_LINE_CODING,
    CDC_ECM_STATE_AT_SETTING_MAX,
} usbh_cdc_ecm_at_set_state_t;


typedef struct {
    usbh_cdc_ecm_at_set_state_t   sub_status;
    usbh_cdc_ecm_ep_t             report_ep;
    usbh_cdc_ecm_ep_t             tx_ep;
    usbh_cdc_ecm_ep_t             rx_ep;
    volatile u8                   data_tx_done;
    u8                            task_flag;

    usbh_cdc_ecm_appx_param_t     *param_item;
    usbh_cdc_ecm_appx_param_t     *param_array;    //at port interface idx
    usbh_cdc_ecm_line_coding_t    line_coding;    //acm line setting

    usb_appx_report               rx_report;
    u8                            mac_ready;
    u8                            iMACAddressStringId;//get the mac address
    u8                            mac_string[CDC_ECM_MAC_STRING_LEN];//msg get from device
    u8                            mac[CDC_ECM_MAC_STR_LEN];
} usbh_cdc_ecm_appx_t;

/**/
u8 usbh_cdc_ecm_parse_at_cfgdesc(usb_host_t *host,u16 vid,u16 pid);
u8 usbh_cdc_ecm_appx_doinit(usb_appx_report appx_cb);
u8 usbh_cdc_ecm_appx_task(void);
u8 usbh_cdc_ecm_appx_transfer(usbh_cdc_ecm_host_t *cdc,u8* next);
u8 usbh_cdc_ecm_appx_deinit_pipe(usb_host_t *host);
u8 usbh_cdc_ecm_appx_deinit(usb_host_t *host);

#endif  /* USBH_CDC_ECM_APPX_H */
