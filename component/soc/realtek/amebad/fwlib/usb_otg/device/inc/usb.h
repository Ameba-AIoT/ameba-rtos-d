#ifndef _USB_H_
#define _USB_H_

#include <stdint.h>
#include "osdep_service.h"
#include "usb_errno.h"
#include "usb_ch9.h"

#define USB_SPEED_UNKNOWN    0 /* enumerating */
#define USB_SPEED_LOW        1 /* usb 1.1 */
#define USB_SPEED_FULL       2 /* usb 1.1 */
#define USB_SPEED_HIGH       3 /* usb 2.0 */
#define USB_SPEED_VARIABLE   4 /* wireless (usb 2.5) */

enum usb_status_t {
    USB_STATUS_INIT = 0,  // Initial status
    USB_STATUS_DETACHED,  // Detached from USB host or charger
    USB_STATUS_ATTACHED   // Attached to USB host
};

#ifndef UNUSED
#define UNUSED(x) ((void)(x))
#endif

#ifdef CONFIG_GHD_EN
#define USBD_IRQ_THREAD_PRIORITY  (tskIDLE_PRIORITY + 9)
#else
#define USBD_IRQ_THREAD_PRIORITY  (tskIDLE_PRIORITY + 4)
#endif 

int usb_init(int speed);
void usb_deinit(void);
int usb_get_status(void);

#endif /* _USB_H_ */

