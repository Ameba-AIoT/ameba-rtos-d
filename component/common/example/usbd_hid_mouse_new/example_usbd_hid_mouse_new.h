/**
  ******************************************************************************
  * @file    example_usbd_hid_mouse_new.h
  * @author  Realsil WLAN5 Team
  * @version V1.0.0
  * @date    2021-6-18
  * @brief   This file is the header file for example_usbd_hid_mouse_new.c
  ******************************************************************************
  * @attention
  *
  * This module is a confidential and proprietary property of RealTek and
  * possession or use of this module requires written permission of RealTek.
  *
  * Copyright(c) 2021, Realtek Semiconductor Corporation. All rights reserved.
  ******************************************************************************
  */

/* Define to prevent recursive inclusion -------------------------------------*/

#ifndef _EXAMPLE_USBD_HID_H
#define _EXAMPLE_USBD_HID_H


/* Includes ------------------------------------------------------------------*/

#include <platform_opts.h>
#include "ameba_soc.h"

#if defined(CONFIG_EXAMPLE_USBD_HID) && CONFIG_EXAMPLE_USBD_HID


/* Exported types ------------------------------------------------------------*/

struct mouse_data{
	u8 left;			//left button. 0: release, 1: press
	u8 right;			//right button. 0: release, 1: press
	u8 middle;			//wheel button. 0: release, 1: press
	char x_axis;		//x-axis pixels. relative value from -127 to 127, positive for right and negative for left 
	char y_axis;		//y-axis pixels. relative value from -127 to 127, positive for up and negative for down 
	char wheel;			//scrolling units. relative value from -127 to 127, positive for up and negative for down. 
};


/* Exported functions ------------------------------------------------------- */

void example_usbd_hid_mouse(void);

#endif

#endif /* _EXAMPLE_USBD_CDC_ACM_H */

