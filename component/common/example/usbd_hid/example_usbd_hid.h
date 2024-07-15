/**
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

/* Exported functions ------------------------------------------------------- */

void example_usbd_hid(void);

#endif

#endif /* _EXAMPLE_USBD_HID_H */

