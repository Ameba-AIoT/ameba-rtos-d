/*
 *      uvc_status.c  --  USB Video Class driver - Status endpoint
 *
 *      Copyright (C) 2005-2009
 *          Laurent Pinchart (laurent.pinchart@ideasonboard.com)
 *
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either version 2 of the License, or
 *      (at your option) any later version.
 *
 */

#include <platform_opts.h>
    
#if defined(CONFIG_USBH_UVC) || defined(REFER_USBH_UVC)

#include "usb.h"
#include "uvcvideo.h"

/* --------------------------------------------------------------------------
 * Input device
 */
#ifdef CONFIG_USB_VIDEO_CLASS_INPUT_EVDEV
static int uvc_input_init(struct uvc_device *dev)
{
    struct input_dev *input;
    int ret;
    input = input_allocate_device();

    if (input == NULL) {
        return -USB_ENOMEM;
    }

    // dev->input_phys = "usb-0000:1d.7-5", 64
    usb_make_path(dev->udev, dev->input_phys, sizeof(dev->input_phys));
    // "usb-0000:1d.7-5/button"
    strlcat(dev->input_phys, "/button", sizeof(dev->input_phys));
    // "QuanJing Webcam"
    input->name = dev->name;
    // "usb-0000:1d.7-5/button"
    input->phys = dev->input_phys;
    usb_to_input_id(dev->udev, &input->id);
    input->dev.parent = &dev->intf->dev;
    __set_bit(EV_KEY, input->evbit);
    __set_bit(KEY_CAMERA, input->keybit);

    if ((ret = input_register_device(input)) < 0) {
        goto error;
    }

    dev->input = input;
    return 0;
error:
    input_free_device(input);
    return ret;
}

static void uvc_input_cleanup(struct uvc_device *dev)
{
    if (dev->input) {
        input_unregister_device(dev->input);
    }
}

static void uvc_input_report_key(struct uvc_device *dev, unsigned int code,
    int value)
{
    if (dev->input) {
        input_report_key(dev->input, code, value);
        input_sync(dev->input);
    }
}

#else
#define uvc_input_init(dev)
#define uvc_input_cleanup(dev)
#define uvc_input_report_key(dev, code, value)
#endif /* CONFIG_USB_VIDEO_CLASS_INPUT_EVDEV */

/* --------------------------------------------------------------------------
 * Status interrupt endpoint
 */
static void uvc_event_streaming(struct uvc_device *dev, __u8 *data, int len)
{
    UNUSED(dev);
    
    if (len < 3) {
        UVC_ERROR("Invalid streaming status event received.\n");
        return;
    }

    if (data[2] == 0) {
        if (len < 4) {
            return;
        }

        UVC_PRINTF("Button (intf %d) %s len %d\n", data[1], data[3] ? "pressed" : "released", len);
        uvc_input_report_key(dev, KEY_CAMERA, data[3]);
    } else {
        UVC_ERROR("Stream %d error event %02x %02x len %d.\n", data[1], data[2], data[3], len);
    }
}

static void uvc_event_control(struct uvc_device *dev, __u8 *data, int len)
{
#if USBH_UVC_DEBUG
    char *attrs[3] = { "value", "info", "failure" };
#endif

    UNUSED(dev);

    if (len < 6 || data[2] != 0 || data[4] > 2) {
        UVC_PRINTF("Invalid control status event received.\n");
        return;
    }

#if USBH_UVC_DEBUG
    UVC_PRINTF("Control %d/%d %s change len %d.\n", data[1], data[3], attrs[data[4]], len);
#endif
}

static void uvc_status_complete(struct urb *urb)
{
    struct uvc_device *dev = urb->context;
    int len, ret;

    switch (urb->status) {
        case 0:
            break;

        case -USB_ENOENT:       /* usb_kill_urb() called. */
        case -USB_ECONNRESET:   /* usb_unlink_urb() called. */
        case -USB_ESHUTDOWN:    /* The endpoint is being disabled. */
        case -USB_EPROTO:       /* Device is disconnected (reported by some
                 * host controller). */
            return;

        default:
            UVC_PRINTF("Non-zero status (%d) in status completion handler.\n", urb->status);
            return;
    }

    len = urb->actual_length;

    if (len > 0) {
        switch (dev->status[0] & 0x0f) {
            case UVC_STATUS_TYPE_CONTROL:
                uvc_event_control(dev, dev->status, len);
                break;

            case UVC_STATUS_TYPE_STREAMING:
                uvc_event_streaming(dev, dev->status, len);
                break;

            default:
                UVC_ERROR("Unknown status event type %d.\n", dev->status[0]);
                break;
        }
    }

    /* Resubmit the URB. */
    urb->interval = dev->int_ep->desc.bInterval;

    if ((ret = usb_submit_urb(urb)) < 0) {
        UVC_ERROR("Failed to resubmit status URB (%d).\n", ret);
    }
}

int uvc_status_init(struct uvc_device *dev)
{
    FUN_ENTER;
    struct usb_host_endpoint *ep = dev->int_ep;
    unsigned int pipe;
    int interval;

    if (ep == NULL) {
        UVC_ERROR("rep == NULL\n");
        FUN_EXIT;
        return 0;
    }

    // in ubuntu 12.04 UVC will use this ...but we decide to skip it.
    uvc_input_init(dev);
    dev->status = (__u8 *)rtw_zmalloc(UVC_MAX_STATUS_SIZE);

    if (dev->status == NULL) {
        return -USB_ENOMEM;
    }

    dev->int_urb = usb_alloc_urb(0/*, GFP_KERNEL*/);

    if (dev->int_urb == NULL) {
        rtw_free(dev->status);
        return -USB_ENOMEM;
    }

    /*
        // USB_DT_ENDPOINT: Endpoint descriptor
        struct usb_endpoint_descriptor {
            __u8  bLength = 0x07;
            __u8  bDescriptorType = 0x05;

            __u8  bEndpointAddress = 0x81;  // IN endpoint with endpoint num 1
            __u8  bmAttributes = 0x03;  // INTERRUPT Transfer
            __le16 wMaxPacketSize = 0x10;   // Maximum packet size = 16 Bytes
            __u8  bInterval = 0x07;     // 2^(7-1) = 64 u-frames

            // NOTE:  these two are _only_ in audio endpoints.
            // use USB_DT_ENDPOINT*_SIZE in bLength, not sizeof.
            __u8  bRefresh;
            __u8  bSynchAddress;
        } __attribute__ ((packed));
    */
    pipe = usb_rcvintpipe(dev->udev, ep->desc.bEndpointAddress);
    /* For high-speed interrupt endpoints, the bInterval value is used as
     * an exponent of two. Some developers forgot about it.
     */
    interval = ep->desc.bInterval;
    
#if 0

    if (interval > 16 && dev->udev->speed == USB_SPEED_HIGH &&
        (dev->quirks & UVC_QUIRK_STATUS_INTERVAL)) {
        interval = fls(interval) - 1;
    }

#endif

    UVC_ERROR("interrupt ep interval: %d", interval);
    usb_fill_int_urb(dev->int_urb, dev->udev, pipe,
        dev->status, UVC_MAX_STATUS_SIZE, uvc_status_complete,
        dev, interval);
    FUN_EXIT;
    return 0;
}

void uvc_status_cleanup(struct uvc_device *dev)
{
    usb_kill_urb(dev->int_urb);
    usb_free_urb(dev->int_urb);
    rtw_free(dev->status);
    uvc_input_cleanup(dev);
}

int uvc_status_start(struct uvc_device *dev, gfp_t flags)
{
    UNUSED(flags);
    
    if (dev->int_urb == NULL) {
        return 0;
    }

    return usb_submit_urb(dev->int_urb);
}

void uvc_status_stop(struct uvc_device *dev)
{
    usb_kill_urb(dev->int_urb);
}

#endif // CONFIG_USBH_UVC
