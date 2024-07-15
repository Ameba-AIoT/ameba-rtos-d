/*
 *      uvc_driver.c  --  USB Video Class driver
 *
 *      Copyright (C) 2005-2010
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
#include "videodev2.h"
#include "uvcvideo.h"
#include "uvc_os_wrap_via_osdep_api.h"
#include "v4l2_intf.h"
#include "uvc_intf.h"

#define DRIVER_DESC     "USB Video Class driver"

unsigned int uvc_clock_param = 4;
unsigned int uvc_no_drop_param = 0;
//static unsigned int uvc_quirks_param = -1;
unsigned int uvc_trace_param = 0;
unsigned int uvc_timeout_param = UVC_CTRL_STREAMING_TIMEOUT;
extern struct uvc_usr_cb_t *uvch_usr_cb;

/* ------------------------------------------------------------------------
 * Video formats
 */
#include "section_config.h"
SDRAM_DATA_SECTION static struct uvc_format_desc uvc_fmts[] = {
    {
        .name       = "YUV 4:2:2 (YUYV)",
        .guid       = UVC_GUID_FORMAT_YUY2,
        .fcc        = V4L2_PIX_FMT_YUYV,
    },
    {
        .name       = "YUV 4:2:2 (YUYV)",
        .guid       = UVC_GUID_FORMAT_YUY2_ISIGHT,
        .fcc        = V4L2_PIX_FMT_YUYV,
    },
    {
        .name       = "YUV 4:2:0 (NV12)",
        .guid       = UVC_GUID_FORMAT_NV12,
        .fcc        = V4L2_PIX_FMT_NV12,
    },
    {
        .name       = "MJPEG",
        .guid       = UVC_GUID_FORMAT_MJPEG,
        .fcc        = V4L2_PIX_FMT_MJPEG,
    },
    {
        .name       = "YVU 4:2:0 (YV12)",
        .guid       = UVC_GUID_FORMAT_YV12,
        .fcc        = V4L2_PIX_FMT_YVU420,
    },
    {
        .name       = "YUV 4:2:0 (I420)",
        .guid       = UVC_GUID_FORMAT_I420,
        .fcc        = V4L2_PIX_FMT_YUV420,
    },
    {
        .name       = "YUV 4:2:0 (M420)",
        .guid       = UVC_GUID_FORMAT_M420,
        .fcc        = V4L2_PIX_FMT_M420,
    },
    {
        .name       = "YUV 4:2:2 (UYVY)",
        .guid       = UVC_GUID_FORMAT_UYVY,
        .fcc        = V4L2_PIX_FMT_UYVY,
    },
    {
        .name       = "Greyscale 8-bit (Y800)",
        .guid       = UVC_GUID_FORMAT_Y800,
        .fcc        = V4L2_PIX_FMT_GREY,
    },
    {
        .name       = "Greyscale 8-bit (Y8  )",
        .guid       = UVC_GUID_FORMAT_Y8,
        .fcc        = V4L2_PIX_FMT_GREY,
    },
    {
        .name       = "Greyscale 10-bit (Y10 )",
        .guid       = UVC_GUID_FORMAT_Y10,
        .fcc        = V4L2_PIX_FMT_Y10,
    },
    {
        .name       = "Greyscale 12-bit (Y12 )",
        .guid       = UVC_GUID_FORMAT_Y12,
        .fcc        = V4L2_PIX_FMT_Y12,
    },
    {
        .name       = "Greyscale 16-bit (Y16 )",
        .guid       = UVC_GUID_FORMAT_Y16,
        .fcc        = V4L2_PIX_FMT_Y16,
    },
    {
        .name       = "RGB Bayer",
        .guid       = UVC_GUID_FORMAT_BY8,
        .fcc        = V4L2_PIX_FMT_SBGGR8,
    },
    {
        .name       = "RGB565",
        .guid       = UVC_GUID_FORMAT_RGBP,
        .fcc        = V4L2_PIX_FMT_RGB565,
    },
    {
        .name       = "H.264",
        .guid       = UVC_GUID_FORMAT_H264,
        .fcc        = V4L2_PIX_FMT_H264,
    },
    /* edit by Ian -- patch for GEO */
    {
        .name       = "MPEG2 TS",
        .guid       = UVC_GUID_FORMAT_MPEG,
        .fcc        = V4L2_PIX_FMT_MPEG,
    },
    {
        .name       = "MUX",
        .guid       = UVC_GUID_FORMAT_MUX,
        .fcc        = V4L2_PIX_FMT_MUX,
    },
};

struct usb_host_endpoint *uvc_find_endpoint(struct usb_host_interface *alts, __u8 epaddr)
{
    struct usb_host_endpoint *ep;
    unsigned int i;

    if (alts == NULL) {
        UVC_ERROR("Invalid usb_host_interface\n");
        return NULL;
    }

    // each alts 1 ~ 4 all have isochronous endpoint IN 2 except alts 0(only control EP)
    for (i = 0; i < alts->desc.bNumEndpoints; ++i) {
        if (alts->endpoint != NULL) {
            ep = &alts->endpoint[i];

            if ((ep != NULL) && (ep->desc.bEndpointAddress == epaddr)) {
                return ep;
            }
        }
    }

    return NULL;
}
/*
 * return matched video format
 */
static struct uvc_format_desc *uvc_format_by_guid(const __u8 guid[16])
{
    unsigned int len = ARRAY_SIZE(uvc_fmts);
    unsigned int i;

    for (i = 0; i < len; ++i) {
        if (memcmp(guid, uvc_fmts[i].guid, 16) == 0) {
            return &uvc_fmts[i];
        }
    }

    return NULL;
}

static __u32 uvc_colorspace(const __u8 primaries)
{
    static const __u8 colorprimaries[] = {
        0,
        V4L2_COLORSPACE_SRGB,
        V4L2_COLORSPACE_470_SYSTEM_M,
        V4L2_COLORSPACE_470_SYSTEM_BG,
        V4L2_COLORSPACE_SMPTE170M,
        V4L2_COLORSPACE_SMPTE240M,
    };

    if (primaries < ARRAY_SIZE(colorprimaries)) {
        return colorprimaries[primaries];
    }

    return 0;
}

/* Simplify a fraction using a simple continued fraction decomposition. The
 * idea here is to convert fractions such as 333333/10000000 to 1/30 using
 * 32 bit arithmetic only. The algorithm is not perfect and relies upon two
 * arbitrary parameters to remove non-significative terms from the simple
 * continued fraction decomposition. Using 8 and 333 for n_terms and threshold
 * respectively seems to give nice results.
 */
void uvc_simplify_fraction(uint32_t *numerator, uint32_t *denominator,
    unsigned int n_terms, unsigned int threshold)
{
    uint32_t *an;
    uint32_t x, y, r;
    unsigned int i, n;
    an = (uint32_t *)rtw_malloc(n_terms * sizeof * an);

    if (an == NULL) {
        return;
    }

    /* Convert the fraction to a simple continued fraction. See
     * http://mathforum.org/dr.math/faq/faq.fractions.html
     * Stop if the current term is bigger than or equal to the given
     * threshold.
     */
    x = *numerator;
    y = *denominator;

    for (n = 0; n < n_terms && y != 0; ++n) {
        an[n] = x / y;

        if (an[n] >= threshold) {
            if (n < 2) {
                n++;
            }

            break;
        }

        r = x - an[n] * y;
        x = y;
        y = r;
    }

    /* Expand the simple continued fraction back to an integer fraction. */
    x = 0;
    y = 1;

    for (i = n; i > 0; --i) {
        r = y;
        y = an[i - 1] * y + x;
        x = r;
    }

    *numerator = y;
    *denominator = x;
    rtw_free((unsigned char *)an);
}

/* Convert a fraction to a frame interval in 100ns multiples. The idea here is
 * to compute numerator / denominator * 10000000 using 32 bit fixed point
 * arithmetic only.
 */
uint32_t uvc_fraction_to_interval(uint32_t numerator, uint32_t denominator)
{
    uint32_t multiplier;

    /* Saturate the result if the operation would overflow. */
    if (denominator == 0 ||
        numerator / denominator >= ((uint32_t) -1) / 10000000) {
        return (uint32_t) -1;
    }

    /* Divide both the denominator and the multiplier by two until
     * numerator * multiplier doesn't overflow. If anyone knows a better
     * algorithm please let me know.
     */
    multiplier = 10000000;

    while (numerator > ((uint32_t) -1) / multiplier) {
        multiplier /= 2;
        denominator /= 2;
    }

    return denominator ? numerator * multiplier / denominator : 0;
}

/* ------------------------------------------------------------------------
 * Terminal and unit management
 */

struct uvc_entity *uvc_entity_by_id(struct uvc_device *dev, int id)
{
    struct uvc_entity *entity;
    list_for_each_entry(entity, &dev->entities, list, struct uvc_entity) {
        if (entity->id == id) {
            return entity;
        }
    }
    return NULL;
}

static struct uvc_entity *uvc_entity_by_reference(struct uvc_device *dev,
    int id, struct uvc_entity *entity)
{
    unsigned int i;

    if (entity == NULL) {
        entity = list_entry(&dev->entities, struct uvc_entity, list);
    }

    list_for_each_entry_continue(entity, &dev->entities, list, struct uvc_entity) {
        for (i = 0; i < entity->bNrInPins; ++i)
            if (entity->baSourceID[i] == id) {
                return entity;
            }
    }
    return NULL;
}

static struct uvc_streaming *uvc_stream_by_id(struct uvc_device *dev, int id)
{
    struct uvc_streaming *stream;
    list_for_each_entry(stream, &dev->streams, list, struct uvc_streaming) {
        if (stream->header.bTerminalLink == id) {
            return stream;
        }
    }
    return NULL;
}

/* ------------------------------------------------------------------------
 * Descriptors parsing
 */

/*
    intervals -------------------
                   \|/
                    |
      format  frame          interval
    |   2   |   10      |                  60
    -----------------------------------------------------------------
    |   |   | | | | | | | | | | | | | | | |              |
    |   |   | | | | | | | | | | | | | | | |                  |
    -----------------------------------------------------------------
    \                               /
                 size(624) Bytes


*/
// format: pointer to one buffer to store all the related descriptors,
//         like the above table
// intervals: a pointer which point to interval's base address
// buflen: 602, calculate the size(expect 617 - 15)
// buffer: pointer to the format descriptor follow by the
//          CS_INITERFACE_Descriptor(CS_INPUT_HEADER)
static int uvc_parse_format(struct uvc_device *dev,
    struct uvc_streaming *streaming, struct uvc_format *format,
    __u32 **intervals, unsigned char *buffer, int buflen)
{
    /* interface 1 descriptor */
    struct usb_interface *intf = streaming->intf;
    struct usb_host_interface *alts = intf->cur_altsetting;
    struct uvc_format_desc *fmtdesc;
    struct uvc_frame *frame;
    const unsigned char *start = buffer;
    unsigned int interval;
    int i;
    int n;
    __u8 ftype;
    
    FUN_ENTER;
    // get the bDescriptorSubType1 = VS_FORMAT_MJPEG(0x06)
    format->type = buffer[2];
    // get the bFormatIndex = 0x01
    format->index = buffer[3];

    // get the bDescriptorSubType1 = VS_FORMAT_MJPEG(0x06)
    switch (buffer[2]) {
        case UVC_VS_FORMAT_UNCOMPRESSED/*4*/:
        case UVC_VS_FORMAT_FRAME_BASED/*16*/:
            n = buffer[2] == UVC_VS_FORMAT_UNCOMPRESSED ? 27 : 28;

            if (buflen < n) {
                UVC_ERROR("device %d videostreaming interface %d FORMAT error\n", dev->udev->devnum, alts->desc.bInterfaceNumber);
                return -USB_EINVAL;
            }

            /* Find the format descriptor from its GUID. */
            fmtdesc = uvc_format_by_guid(&buffer[5]);

            if (fmtdesc != NULL) {
                strlcpy(format->name, fmtdesc->name,
                    sizeof format->name);
                format->fcc = fmtdesc->fcc;
            } else {
                UVC_ERROR("Unknown video format %pUl\n", &buffer[5]);
                format->fcc = 0;
            }

            format->bpp = buffer[21];

            if (buffer[2] == UVC_VS_FORMAT_UNCOMPRESSED) {
                ftype = UVC_VS_FRAME_UNCOMPRESSED;
            } else {
                ftype = UVC_VS_FRAME_FRAME_BASED;

                if (buffer[27]) {
                    format->flags = UVC_FMT_FLAG_COMPRESSED;
                }
            }

            break;

        case UVC_VS_FORMAT_MJPEG/*6*/: // first time
            if (buflen < 11) {
                UVC_ERROR("device %d videostreaming interface %d FORMAT error\n",
                    dev->udev->devnum, alts->desc.bInterfaceNumber);
                return -USB_EINVAL;
            }

            strlcpy(format->name, "MJPEG", sizeof format->name);
            format->fcc = V4L2_PIX_FMT_MJPEG;
            format->flags = UVC_FMT_FLAG_COMPRESSED;
            format->bpp = 0;
            ftype = UVC_VS_FRAME_MJPEG;
            break;

        case UVC_VS_FORMAT_DV:
            if (buflen < 9) {
                UVC_ERROR("device %d videostreaming interface %d FORMAT error\n", dev->udev->devnum, alts->desc.bInterfaceNumber);
                return -USB_EINVAL;
            }

            switch (buffer[8] & 0x7f) {
                case 0:
                    strlcpy(format->name, "SD-DV", sizeof format->name);
                    break;

                case 1:
                    strlcpy(format->name, "SDL-DV", sizeof format->name);
                    break;

                case 2:
                    strlcpy(format->name, "HD-DV", sizeof format->name);
                    break;

                default:
                    UVC_ERROR("device %d videostreaming interface %d: unknown DV format %d\n", dev->udev->devnum,
                        alts->desc.bInterfaceNumber, buffer[8]);
                    return -USB_EINVAL;
            }

            strlcat(format->name, buffer[8] & (1 << 7) ? " 60Hz" : " 50Hz",
                sizeof format->name);
            format->fcc = V4L2_PIX_FMT_DV;
            format->flags = UVC_FMT_FLAG_COMPRESSED | UVC_FMT_FLAG_STREAM;
            format->bpp = 0;
            ftype = 0;
            /* Create a dummy frame descriptor. */
            frame = &format->frame[0];
            rtw_memset(&format->frame[0], 0, sizeof format->frame[0]);
            frame->bFrameIntervalType = 1;
            frame->dwDefaultFrameInterval = 1;
            frame->dwFrameInterval = *intervals; // record the interval base address.
            *(*intervals)++ = 1;
            format->nframes = 1;
            break;

        case UVC_VS_FORMAT_MPEG2TS:
            /* edit by Ian -- patch for GEO */
            n = dev->uvc_version >= 0x0110 ? 23 : 7;

            if (buflen < n) {
                UVC_ERROR("device %d videostreaming interface %d FORMAT error\n",
                    dev->udev->devnum, alts->desc.bInterfaceNumber);
                return -USB_EINVAL;
            }

            strlcpy(format->name, "MPEG2 TS", sizeof format->name);
            format->fcc = V4L2_PIX_FMT_MPEG;
            format->flags = UVC_FMT_FLAG_COMPRESSED | UVC_FMT_FLAG_STREAM;
            format->bpp = 0;
            ftype = 0;
            /* Create a dummy frame descriptor. */
            frame = &format->frame[0];
            rtw_memset(&format->frame[0], 0, sizeof format->frame[0]);
            frame->bFrameIntervalType = 0;
            frame->dwDefaultFrameInterval = 1;
            frame->dwFrameInterval = *intervals;
            *(*intervals)++ = 1;
            *(*intervals)++ = 10000000;
            *(*intervals)++ = 1;
            format->nframes = 1;
            break;

        case UVC_VS_FORMAT_STREAM_BASED:

        /* Not supported yet. */
        default:
            UVC_ERROR("device %d videostreaming interface %d unsupported format %d\n", dev->udev->devnum,
                alts->desc.bInterfaceNumber, buffer[2]);
            return -USB_EINVAL;
    }

    // Format format MJPEG.
    UVC_PRINTF("Found format %s, %d bpp,\n", format->name, format->bpp);
    // 602 - 11 = 592
    buflen -= buffer[0];
    // point to the first frame descriptor which follow by mjpeg format descriptor
    buffer += buffer[0];

    /* Parse the frame descriptors. Only uncompressed, MJPEG and frame
     * based formats have frame descriptors.
     */
    // buffer: point to the first Frame descriptor, ftype = UVC_VS_FRAME_MJPEG
    while (buflen > 2 && buffer[1] == USB_DT_CS_INTERFACE && buffer[2] == ftype) {
        // get the nframes order frame. nframes = 0->1->2->3->4->5
        frame = &format->frame[format->nframes];

        // ftype = UVC_VS_FRAME_MJPEG(7),buflen = 592, n = 6(bFrameIntervalType)
        if (ftype != UVC_VS_FRAME_FRAME_BASED/*17*/) {
            n = buflen > 25 ? buffer[25] : 0;
        } else {
            n = buflen > 21 ? buffer[21] : 0;
        }

        // n = 6
        n = n ? n : 3;

        // 592
        if (buflen < 26 + 4 * n) {
            UVC_ERROR("device %d videostreaming interface %d FRAME error\n",
                dev->udev->devnum, alts->desc.bInterfaceNumber);
            return -USB_EINVAL;
        }

        // 0x01
        frame->bFrameIndex = buffer[3];
        // 0x00
        frame->bmCapabilities = buffer[4];
        // 0x0280(640)
        frame->wWidth = get_unaligned_le16(&buffer[5]);
        // 0x01E0(480)
        frame->wHeight = get_unaligned_le16(&buffer[7]);
        // 0x02328000(36864000)
        frame->dwMinBitRate = get_unaligned_le32(&buffer[9]);
        // 0x0D2F0000(221184000)
        frame->dwMaxBitRate = get_unaligned_le32(&buffer[13]);

        if (ftype != UVC_VS_FRAME_FRAME_BASED) {
            // 0x000E1000(921600)
            frame->dwMaxVideoFrameBufferSize =
                get_unaligned_le32(&buffer[17]);
            // 0x00051615(333333)
            frame->dwDefaultFrameInterval =
                get_unaligned_le32(&buffer[21]);
            // 0x06
            frame->bFrameIntervalType = buffer[25];
        } else {
            frame->dwMaxVideoFrameBufferSize = 0;
            frame->dwDefaultFrameInterval =
                get_unaligned_le32(&buffer[17]);
            frame->bFrameIntervalType = buffer[21];
        }

        // record the interval's entry pointer(base address)
        frame->dwFrameInterval = *intervals;

        /* Several UVC chipsets screw up dwMaxVideoFrameBufferSize
         * completely. Observed behaviours range from setting the
         * value to 1.1x the actual frame size to hardwiring the
         * 16 low bits to 0. This results in a higher than necessary
         * memory usage as well as a wrong image size information. For
         * uncompressed formats this can be fixed by computing the
         * value from the frame size.
         */

        if (!(format->flags & UVC_FMT_FLAG_COMPRESSED)) {
            UVC_PRINTF("format uncompressed");
            frame->dwMaxVideoFrameBufferSize = format->bpp
                * frame->wWidth * frame->wHeight / 8;
        }

        /* Some bogus devices report dwMinFrameInterval equal to
         * dwMaxFrameInterval and have dwFrameIntervalStep set to
         * zero. Setting all null intervals to 1 fixes the problem and
         * some other divisions by zero that could happen.
         */
        // n = 6, setup n interval values for this frame
        for (i = 0; i < n; ++i) {
            // get the dwFrameInterval(1)'s base address(integer type)
            interval = get_unaligned_le32(&buffer[26 + 4 * i]);
            // fill the interval value in the correspond place then
            // move to the next location
            *(*intervals)++ = interval ? interval : 1;
        }

        /* Make sure that the default frame interval stays between
         * the boundaries.
         */
        // n(2) = 6 - 0x06 ? 1 : 2
        n -= frame->bFrameIntervalType ? 1 : 2;
        // dwDefaultFrameInterval = 0x00051615(333333)
        frame->dwDefaultFrameInterval = min(frame->dwFrameInterval[n],
                max(frame->dwFrameInterval[0], frame->dwDefaultFrameInterval));
#if 0

        if (dev->quirks & UVC_QUIRK_RESTRICT_FRAME_RATE) {
            frame->bFrameIntervalType = 1;
            frame->dwFrameInterval[0] = frame->dwDefaultFrameInterval;
            UVC_ERROR("UVC_QUIRK_RESTRICT_FRAME_RATE\n");
        }

#endif
        // -280 1E0 (1E.0 fps)
        UVC_PRINTF("%d %d (%d.%d fps)\n", frame->wWidth, frame->wHeight, 10000000 / frame->dwDefaultFrameInterval,
            (100000000 / frame->dwDefaultFrameInterval) % 10);
        // 0->1, 1->2, 2->3, 3->4, 4->5 (mjpeg total 5 frame descriptors)
        format->nframes++;
        // calculate the Remaining size 592 ->542 ->492 ->442 ->392 ->342
        buflen -= buffer[0];
        // move to the next frame descriptor address
        buffer += buffer[0];
    }//;while

    // exit the while loop above, the buffer will point to
    // VS_STILL_IMAGE_FRAME descriptor
    // by pass this descriptor, calculate new size and new pointer address
    if (buflen > 2 && buffer[1] == USB_DT_CS_INTERFACE &&
        buffer[2] == UVC_VS_STILL_IMAGE_FRAME) {
        buflen -= buffer[0];
        buffer += buffer[0];
        UVC_PRINTF("by pass uvc_vs_still_image_frame\n");
    }

    // calculate new size and new pointer address
    if (buflen > 2 && buffer[1] == USB_DT_CS_INTERFACE &&
        buffer[2] == UVC_VS_COLORFORMAT) {
        if (buflen < 6) {
            UVC_ERROR("device %d videostreaming interface %d COLORFORMAT error\n", dev->udev->devnum, alts->desc.bInterfaceNumber);
            return -USB_EINVAL;
        }

        // bColorPrimaries(buffer[3]) = 0x01
        // colorspace = V4L2_COLORSPACE_SRGB(8)
        format->colorspace = uvc_colorspace(buffer[3]);
        UVC_PRINTF("format colorspace: %d", format->colorspace);
        buflen -= buffer[0];
        buffer += buffer[0];
    }

    // now, buffer point to the next format descriptor
    //      (i.e VS_FORMAT_UNCOMPRESSED descriptor).
    // so return (11+50+50+50+50+50+26+6) = 293
    return buffer - start;
}

// intf: which means the SC_VIDEOSTREAMING(VS) Descriptor(i.e interface 1) in spytank.
static int uvc_parse_streaming(struct uvc_device *dev, struct usb_interface *intf)
{
    /*
        // spy tank USB_DT_INTERFACE: Interface descriptor 1
        struct usb_interface_descriptor {
            __u8  bLength = 0x09;
            __u8  bDescriptorType = 0x04;

            __u8  bInterfaceNumber = 0x01;
            __u8  bAlternateSetting = 0x00;
            __u8  bNumEndpoints = 0x00;
            __u8  bInterfaceClass = 0x0E;//class:video
            __u8  bInterfaceSubClass = 0x02;
            __u8  bInterfaceProtocol = 0x00;
            __u8  iInterface = 0x04;
        } __attribute__ ((packed));
    */
    FUN_ENTER;
    struct uvc_streaming *streaming = NULL;
    struct uvc_format *format;
    struct uvc_frame *frame;
    // get the first interface(alternate_setting 0) address
    struct usb_host_interface *alts = &intf->altsetting[0];
    unsigned char *_buffer;
    // pointer to the CS_INTERFACE_Descriptor which follow by the interface
    // descriptor 1
    unsigned char *buffer = alts->extra;
    int _buflen;
    // 617 (15 + 11 + 50 + 50 + 50 + 50 + 50 + 26 + 6 + 27 + 50 + 50 + 50 + 50 + 50 + 26 + 6)
    int buflen = alts->extralen;
    unsigned int nformats = 0, nframes = 0, nintervals = 0;
    unsigned int i;
    int size, n, p;
    __u32 *interval;
    __u16 psize;
    int ret = -USB_EINVAL;

    // check the bInterfaceSubClass
    if (intf->cur_altsetting->desc.bInterfaceSubClass != UVC_SC_VIDEOSTREAMING) {
        UVC_ERROR("device %d interface %d isn't a video streaming interface\n",
            dev->udev->devnum, intf->altsetting[0].desc.bInterfaceNumber);
        FUN_EXIT;
        return -USB_EINVAL;
    }

    streaming = (struct uvc_streaming *)rtw_zmalloc(sizeof * streaming);

    if (streaming == NULL) {
        return -USB_EINVAL;
    }

    UVC_PRINTF("streaming addr:0x%x uvc_video_queue addr:0x%x", streaming, &streaming->queue);
    Mutex_Init(&streaming->mutex);
    // setup the uvc device to streaming struct
    streaming->dev = dev;
    // increments the reference count of the usb interface structure
    streaming->intf = usb_get_intf(intf);
    // 0x01
    streaming->intfnum = intf->cur_altsetting->desc.bInterfaceNumber;

    /* The Pico iMage webcam has its class-specific interface descriptors
     * after the endpoint descriptors.
     */
    // buflen = 617,so skip this
    if (buflen == 0) {
        for (i = 0; i < alts->desc.bNumEndpoints; ++i) {
            struct usb_host_endpoint *ep = &alts->endpoint[i];

            if (ep->extralen == 0) {
                continue;
            }

            if (ep->extralen > 2 && ep->extra[1] == USB_DT_CS_INTERFACE) {
                UVC_PRINTF("trying extra data from endpoint 0x%x.\n", i);
                buffer = alts->endpoint[i].extra;
                buflen = alts->endpoint[i].extralen;
                break;
            }
        }
    }

    /* Skip the standard interface descriptors. */
    // Normally, buffer will pointer to the first CS_INTERFACE Descriptor follow
    // by the interface 1 descriptor, but sometime the vendor will insert other
    // descriptors between them so we skip those descriptors.
    while (buflen > 2 && buffer[1] != USB_DT_CS_INTERFACE) {
        buflen -= buffer[0];
        buffer += buffer[0];
    }

    if (buflen <= 2) {
        UVC_ERROR("no class-specific streaming interface descriptors found.\n");
        goto error;
    }

    /* Parse the header descriptor. */
    // get the bDescriptorType from CS_INTERFACE Descriptor(VS_INPUT_HEADER1)
    // follow by interface 1
    switch (buffer[2]) {
        case UVC_VS_OUTPUT_HEADER:
            UVC_PRINTF("UVC_VS_OUTPUT_HEADER\n");
            streaming->type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
            size = 9;
            break;

        case UVC_VS_INPUT_HEADER: // spy_tank use this
            UVC_PRINTF("UVC_VS_INPUT_HEADER\n");
            streaming->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            size = 13;
            break;

        default:
            UVC_ERROR("device %d videostreaming interface %d HEADER descriptor not found.\n",
                dev->udev->devnum, alts->desc.bInterfaceNumber);
            goto error;
    }

    // buflen = 617
    // number of format descriptors following this interface. p = 0x02 = 2
    p = (buflen >= 4) ? buffer[3] : 0;
    // get the bControlSize form VS_INPUT_HEADER
    // size = 13 , n = 1 (buffer[12])
    n = (buflen >= size) ? buffer[size - 1] : 0;

    // 617 < 13 + 2 * 1
    if (buflen < size + p * n) {
        UVC_ERROR("device %d videostreaming interface %d HEADER descriptor is invalid.\n",
            dev->udev->devnum, alts->desc.bInterfaceNumber);
        goto error;
    }

    // p = 0x02
    streaming->header.bNumFormats = p;
    // get the bEndpointAddress, 0x82 which meaning Endpoint 2 IN
    streaming->header.bEndpointAddress = buffer[6];

    if (buffer[2] == UVC_VS_INPUT_HEADER) {
        // 0x00, Video Streaming interface Capabilities: the video
        // endpoint of this interface is connected is 0.
        streaming->header.bmInfo = buffer[7];
        // 0x03, the terminal ID of the output terminal to which
        // the video endpoint of this interface is connected is 3.
        streaming->header.bTerminalLink = buffer[8];
        // 0x02,which meaning the still capture method is method 2.
        streaming->header.bStillCaptureMethod = buffer[9];
        // 0x01,which meaning trigger support.
        streaming->header.bTriggerSupport = buffer[10];
        // 0x00,which meaning SW action upon trigger.
        streaming->header.bTriggerUsage = buffer[11];
    } else {
        streaming->header.bTerminalLink = buffer[7];
    }

    // n = 0x01
    streaming->header.bControlSize = n;
    /**
     * kmemdup - duplicate region of memory
     *
     * @src: memory region to duplicate
     * @len: memory region length
     * @gfp: GFP mask to use
     */
    // copy the Fmt1 bmControls(1) and Fmt2 bmControls(1) two bytes content
    //                     (source address, 2 * 1, ...)
    streaming->header.bmaControls = kmemdup(&buffer[size], p * n, GFP_KERNEL);

    if (streaming->header.bmaControls == NULL) {
        ret = -USB_ENOMEM;
        goto error;
    }

    // 602, calculate the new size(expect 617 - 15(VS_INPUT_HEADER))
    buflen -= buffer[0];
    // pointer to the address follow by the CS_INITERFACE_Descriptor(CS_INPUT_HEADER)
    // i.e now buffer pointer to the VS_FORMAT_MJPEG
    buffer += buffer[0];
    _buffer = buffer; // pointer to the VS_FORMAT_MJPEG which follow by VS_INPUT_HEADER
    _buflen = buflen; // 602

    /* Count the format and frame descriptors. */
    // 2 format and 10 frames, nintervals = 60
    while (_buflen > 2 && _buffer[1] == USB_DT_CS_INTERFACE) {
        switch (_buffer[2]) {
            case UVC_VS_FORMAT_UNCOMPRESSED/*4*/:// 1 -> 2 (nformats)
                UVC_PRINTF("UVC_VS_FORMAT_UNCOMPRESSED\n");
                nformats++;
                break;

            case UVC_VS_FORMAT_MJPEG/*6*/:// 0 -> 1 (nformats)
                UVC_PRINTF("UVC_VS_FORMAT_MJPEG\n");
                nformats++;
                break;

            case UVC_VS_FORMAT_FRAME_BASED/*16*/:
                UVC_PRINTF("UVC_VS_FORMAT_FRAME_BASED\n");
                nformats++;
                break;

            case UVC_VS_FORMAT_DV/*12*/:
                UVC_PRINTF("UVC_VS_FORMAT_DV\n");
                /* DV format has no frame descriptor. We will create a
                 * dummy frame descriptor with a dummy frame interval.
                 */
                nformats++;
                nframes++;
                nintervals++;
                break;

            case UVC_VS_FORMAT_MPEG2TS/*10*/:
                /* edit by Ian -- patch for GEO */
                /* MPEG2TS format has no frame descriptor. We will create a
                * dummy frame descriptor with a dummy frame interval range.
                         */
                UVC_PRINTF("UVC_VS_FORMAT_MPEG2TS\n");
                nformats++;
                nframes++;
                nintervals += 3;
                break;

            case UVC_VS_FORMAT_STREAM_BASED/*18*/:
                UVC_ERROR("device %d videostreaming interface %d FORMAT %d is not supported.\n", dev->udev->devnum,
                    alts->desc.bInterfaceNumber, _buffer[2]);
                break;

            case UVC_VS_FRAME_UNCOMPRESSED/*5*/: // 5->6,6->7,7->8,8->9,9->10(nframes)
            case UVC_VS_FRAME_MJPEG/*7*/: // 0->1,1->2,2->3,3->4,4->5(nframes)
                UVC_PRINTF("common frame+1\n");
                nframes++;

                if (_buflen > 25)
                    // 6+6+6+6+6+6+6+6+6+6 = 60
                {
                    nintervals += _buffer[25] ? _buffer[25] : 3;
                }

                break;

            case UVC_VS_FRAME_FRAME_BASED/*17*/:
                UVC_PRINTF("frame based frame+1\n");
                nframes++;

                if (_buflen > 21) {
                    nintervals += _buffer[21] ? _buffer[21] : 3;
                }

                break;
        }

        // calculate the new size which subtract this
        // USB_DT_CS_INTERFACE's descriptor size
        _buflen -= _buffer[0];
        // move the new pointer to the next USB_DT_CS_INTERFACE's
        // descriptor address
        _buffer += _buffer[0];
    }

    // nformats = 2
    if (nformats == 0) {
        UVC_ERROR("device %d videostreaming interface %d has no supported formats defined.\n", dev->udev->devnum,
            alts->desc.bInterfaceNumber);
        goto error;
    }

    // 624 =    2    *       52       +    10   *  28
    size = nformats * sizeof * format + nframes * sizeof * frame
        //    60     *       4
        + nintervals * sizeof * interval;
    format = (struct uvc_format *)rtw_zmalloc(size);

    if (format == NULL) {
        ret = -USB_ENOMEM;
        goto error;
    }

    /*
          format  frame          interval
        |   2   |   10      |                  60
        -----------------------------------------------------------------
        |   |   | | | | | | | | | | | | | | | |              |
        |   |   | | | | | | | | | | | | | | | |                  |
        -----------------------------------------------------------------
        \                               /
                     size(624) Bytes
    */
    //                     2
    frame = (struct uvc_frame *)&format[nformats];
    //                           10
    interval = (__u32 *)&frame[nframes];
    // setup the pointer in streaming struct to point to the format base address
    streaming->format = format;
    // streaming->nformats = 2
    streaming->nformats = nformats;

    /* Parse the format descriptors. */
    // buffer = pointer to the VS_FORMAT_MJPEG which follow by VS_INPUT_HEADER
    // buflen = 602
    while (buflen > 2 && buffer[1] == USB_DT_CS_INTERFACE) {
        switch (buffer[2]) {
            case UVC_VS_FORMAT_UNCOMPRESSED/*4*/: // the second format
            case UVC_VS_FORMAT_MJPEG/*6*/: // the first format

            /* edit by Ian -- patch for GEO */
            case UVC_VS_FORMAT_MPEG2TS:
            case UVC_VS_FORMAT_DV/*12*/:
            case UVC_VS_FORMAT_FRAME_BASED/*16*/:
                // setup the pointer in format struct to point to the frame base address
                format->frame = frame;
                //ret = 293,
                ret = uvc_parse_format(dev, streaming, format,
                        &interval, buffer, buflen);

                if (ret < 0) {
                    goto error;
                }

                // nframe = 5, move the next available frame address
                frame += format->nframes;
                // format = 1
                format++;
                // 602 -293 = 309
                buflen -= ret;
                buffer += ret;
                continue;

            default:
                break;
        } //;switch

        buflen -= buffer[0];
        buffer += buffer[0];
    }

    if (buflen) {
        UVC_ERROR("device %d videostreaming interface %d has %d bytes of trailing descriptor garbage.\n", dev->udev->devnum,
            alts->desc.bInterfaceNumber, buflen);
    }

    /* Parse the alternate settings to find the maximum bandwidth. */
    /*
        // spy tank USB_DT_INTERFACE: Interface descriptor 1
        struct usb_interface_descriptor {
            __u8  bLength = 0x09;
            __u8  bDescriptorType = 0x04;

            __u8  bInterfaceNumber = 0x01;
            __u8  bAlternateSetting = 0x00;
            __u8  bNumEndpoints = 0x00;
            __u8  bInterfaceClass = 0x0E;
            __u8  bInterfaceSubClass = 0x02;
            __u8  bInterfaceProtocol = 0x00;
            __u8  iInterface = 0x04;
        } __attribute__ ((packed));
    */
    // intf->num_altsetting =  5
    for (i = 0; i < intf->num_altsetting; ++i) {
        struct usb_host_endpoint *ep;

        if (intf->altsetting == NULL) {
            UVC_ERROR("Invalid altsetting!!!\n");
            continue;
        }

        // get the interface 1's alternate setting 0 ~ 4
        alts = &intf->altsetting[i];
        //              (0x82)Endpoint 2 IN
        ep = uvc_find_endpoint(alts, streaming->header.bEndpointAddress);

        if (ep == NULL) {
            continue;
        }

        // alts 1:1024(multi =2),alts 2:1024(multi =1),
        // alts 3:1024(multi =0),alts 4: 512(multi =0)
        psize = le16_to_cpu(ep->desc.wMaxPacketSize);
        // calculate total needed beffer size for one isochronous transfer
        // alts 1:1024 * (1+2) = 3072,alts 2:1024 * (1+1) = 2048,
        // alts 3:1024 * (1+0) = 1024,alts 4: 512 * (1+0) = 512.
        psize = (psize & 0x07ff) * (1 + ((psize >> 11) & 3));

        if (psize > streaming->maxpsize) {
            streaming->maxpsize = psize;
        }
    }

    list_add_tail(&streaming->list, &dev->streams);
    FUN_EXIT;
    return 0;
error:
    usb_put_intf(intf);
    Mutex_Free(&streaming->mutex);
    rtw_free((unsigned char *)streaming->format);
    rtw_free((unsigned char *)streaming->header.bmaControls);
    rtw_free((unsigned char *)streaming);
    FUN_EXIT;
    return ret;
}

// get the bDescriptorSubType1, buffer[2] = VS_EXTENSION_UNIT(6)
// get the bUnitID, buffer[3] = 0x06
// num_pads = 2 (i.e p+1 = 2, bNrInPins)
// extra_size = 3 (i.e n = 3, bControlSize)
static struct uvc_entity *uvc_alloc_entity(u16 type, u8 id,
    unsigned int num_pads, unsigned int extra_size)
{
    struct uvc_entity *entity;
    unsigned int num_inputs;
    unsigned int size;
    unsigned int i;
    
    // #define ALIGN(x, mask) (((x) + (mask)) & ~(mask))
    // align the address according to the sizeof pads
    // extra_size = 3, extra_size = 3,sizeof(*entity->pads) = 12 ??? , (((3)+(12)-1)&~(12-1))
    extra_size = ALIGN(extra_size, sizeof(*entity->pads), unsigned int);
    //extra_size = ALIGN(extra_size, sizeof(*entity->pads));
    // num_inputs = 1, type = VS_EXTENSION_UNIT(0x06) , num_pads = 2
    num_inputs = (type & UVC_TERM_OUTPUT) ? num_pads : num_pads - 1;
    size = sizeof(*entity) + extra_size + sizeof(*entity->pads) * num_pads
        + num_inputs;
    entity = (struct uvc_entity *)rtw_zmalloc(size);

    if (entity == NULL) {
        return NULL;
    }

    rtw_memset(entity, 0, size);
    entity->id = id;
    entity->type = type;
    entity->num_links = 0;
    entity->num_pads = num_pads;
    entity->pads = (struct media_pad *)((u8 *)entity + sizeof(*entity) + extra_size);

    // num_inputs = 0x01
    for (i = 0; i < num_inputs; ++i) {
        entity->pads[i].flags = MEDIA_PAD_FL_SINK;
    }

    if (!UVC_ENTITY_IS_OTERM(entity)) {
        entity->pads[num_pads - 1].flags = MEDIA_PAD_FL_SOURCE;
    }

    entity->bNrInPins = num_inputs;
    entity->baSourceID = /*(__u8 *)(&entity->pads[num_pads]*/(__u8 *)entity->pads + sizeof(*entity->pads) * num_pads;
    return entity;
}

/* Parse vendor-specific extensions. */
static int uvc_parse_vendor_control(struct uvc_device *dev,
    const unsigned char *buffer, int buflen)
{
    struct usb_device *udev = dev->udev;
    struct usb_host_interface *alts = dev->intf->cur_altsetting;
    struct uvc_entity *unit;
    int n, p;
    int handled = 0;

    switch (le16_to_cpu(dev->udev->descriptor.idVendor)) {
        case 0x046d:        /* Logitech */
            if (buffer[1] != 0x41 || buffer[2] != 0x01) {
                break;
            }

            /* Logitech implements several vendor specific functions
             * through vendor specific extension units (LXU).
             *
             * The LXU descriptors are similar to XU descriptors
             * (see "USB Device Video Class for Video Devices", section
             * 3.7.2.7 "Extension Unit Descriptor") with the following
             * differences:
             *
             * Offset       Field          Size      Value
             * ----------------------------------------------------------
             * 0        bLength     1    Number
             *
             * Description :
             * Size of this descriptor, in bytes: 24+p+n*2
             *
             * ----------------------------------------------------------
             * 23+p+n   bmControlsType  N   Bitmap
             *
             * Description :
             *  Individual bits in the set are defined:
             *  0: Absolute
             *  1: Relative
             *
             *  This bitset is mapped exactly the same as bmControls.
             * ----------------------------------------------------------
             * 23+p+n*2 bReserved   1   Boolean
             * ----------------------------------------------------------
             * 24+p+n*2 iExtension  1   Index
             *  Index of a string descriptor that describes this
             *  extension unit.
             * ----------------------------------------------------------
             */
            p = buflen >= 22 ? buffer[21] : 0;
            n = buflen >= 25 + p ? buffer[22 + p] : 0;

            if (buflen < 25 + p + 2 * n) {
                UVC_ERROR("device %d videocontrol interface %d EXTENSION_UNIT error\n", udev->devnum, alts->desc.bInterfaceNumber);
                break;
            }

            unit = uvc_alloc_entity(UVC_VC_EXTENSION_UNIT, buffer[3],
                    p + 1, 2 * n);

            if (unit == NULL) {
                return -USB_ENOMEM;
            }

            rtw_memcpy((void *)unit->extension.guidExtensionCode, (void *)&buffer[4], 16);
            unit->extension.bNumControls = buffer[20];
            rtw_memcpy((void *)unit->baSourceID, (void *)&buffer[22], p);
            unit->extension.bControlSize = buffer[22 + p];
            unit->extension.bmControls = (__u8 *)unit + sizeof(*unit);
            unit->extension.bmControlsType = (__u8 *)unit + sizeof(*unit)
                + n;
            rtw_memcpy((void *)unit->extension.bmControls, (void *)&buffer[23 + p], 2 * n);

            if (buffer[24 + p + 2 * n] != 0)
                usb_string(udev, buffer[24 + p + 2 * n], unit->name,
                    sizeof unit->name);

            list_add_tail(&unit->list, &dev->entities);
            handled = 1;
            break;
    }

    return handled;
}

// buffer: pointer to the first CS_INTERFACE_Descriptor(i.e VC_HEADER) follow by interface 0
// buflen: 91
//  13(VC_HEADER)+28(VC_EXTENSION_UNIT)+18(VC_INPUT_TERMINAL)+
//  11(VC_PROCESSING_UNIT)+9(VC_INPUT_TERMINAL)+7(ENDPOINT)+5(CS_INTERFACE)
static int uvc_parse_standard_control(struct uvc_device *dev,
    const unsigned char *buffer, int buflen)
{
    struct usb_device *udev = dev->udev;
    struct uvc_entity *unit, *term;
    struct usb_interface *intf;
    struct usb_host_interface *alts = dev->intf->cur_altsetting;
    int i, n, p, len;
    __u16 type;
    
    FUN_ENTER;

    /* get the VC_HEADER's bDescriptorType1 to switch case */
    switch (buffer[2]) {
        case UVC_VC_HEADER: // first time into this
            UVC_PRINTF("UVC_VC_HEADER\n");
            // get the bInCollection which means the number of
            // VideoStreaminh interfaces. n = 0x01,
            n = buflen >= 12 ? buffer[11] : 0;

            if (buflen < 12 /*|| buflen < 12*/ + n) {
                UVC_ERROR("device %d videocontrol interface %d HEADER error\n",
                    udev->devnum, alts->desc.bInterfaceNumber);
                FUN_EXIT;
                return -USB_EINVAL;
            }

            // 0x0100
            dev->uvc_version = get_unaligned_le16(&buffer[3]);
            // 0x01C9C380(3000 0000);new(48000000)
            UVC_PRINTF("total descriptor length: %d\n", buffer[5]);
            dev->clock_frequency = get_unaligned_le32(&buffer[7]);

            /* Parse all USB Video Streaming(VS) interfaces. (n=1)*/
            for (i = 0; i < n; ++i) {
                /*
                    // spy tank USB_DT_INTERFACE: Interface descriptor 1
                    struct usb_interface_descriptor {
                        __u8  bLength = 0x09;
                        __u8  bDescriptorType = 0x04;

                        __u8  bInterfaceNumber = 0x01;
                        __u8  bAlternateSetting = 0x00;
                        __u8  bNumEndpoints = 0x00;
                        __u8  bInterfaceClass = 0x0E;
                        __u8  bInterfaceSubClass = 0x02;
                        __u8  bInterfaceProtocol = 0x00;
                        __u8  iInterface = 0x04;
                    } __attribute__ ((packed));
                */
                intf = (struct usb_interface *)usb_ifnum_to_if(udev, buffer[12 + i]); // buffer[12+i] get the baInterfaceNr(1)

                if (intf == NULL) {
                    UVC_ERROR("device %d interface %d doesn't exists\n", udev->devnum, i);
                    continue;
                }

                UVC_PRINTF("the cur_altsetting of video streaming interface %d\n\r", buffer[12 + i]);
                UVC_PRINTF("bLength = 0x%x\n\r", intf->cur_altsetting->desc.bLength);
                UVC_PRINTF("bDescriptorType = 0x%x\n\r", intf->cur_altsetting->desc.bDescriptorType);
                UVC_PRINTF("bInterfaceNumber = 0x%x\n\r", intf->cur_altsetting->desc.bInterfaceNumber);
                UVC_PRINTF("bAlternateSetting = 0x%x\n\r", intf->cur_altsetting->desc.bAlternateSetting);
                UVC_PRINTF("bNumEndpoints = 0x%x\n\r", intf->cur_altsetting->desc.bNumEndpoints);
                UVC_PRINTF("bInterfaceClass = 0x%x\n\r", intf->cur_altsetting->desc.bInterfaceClass);
                UVC_PRINTF("bInterfaceSubClass = 0x%x\n\r", intf->cur_altsetting->desc.bInterfaceSubClass);
                UVC_PRINTF("bInterfaceProtocol = 0x%x\n\r", intf->cur_altsetting->desc.bInterfaceProtocol);
                UVC_PRINTF("iInterface = 0x%x\n\r", intf->cur_altsetting->desc.iInterface);
                uvc_parse_streaming(dev, intf);
            }

            break;

        case UVC_VC_INPUT_TERMINAL: // the three time into this
            UVC_PRINTF("UVC_VC_INPUT_TERMINAL\n");

            // buflen = 50
            if (buflen < 8) {
                UVC_ERROR("device %d videocontrol interface %d INPUT_TERMINAL error\n",
                    udev->devnum, alts->desc.bInterfaceNumber);
                FUN_EXIT;
                return -USB_EINVAL;
            }

            /* Make sure the terminal type MSB is not null, otherwise it
             * could be confused with a unit.
             */
            // get the wTerminalType from the VC_INPUT_TERMINAL descriptor
            // type = 0x0201(ITT_CAMERA)
            type = get_unaligned_le16(&buffer[4]);

            if ((type & 0xff00) == 0) {
                UVC_ERROR("device %d videocontrol interface %d INPUT_TERMINAL %d has invalid type 0x%x, skipping\n", udev->devnum,
                    alts->desc.bInterfaceNumber, buffer[3], type);
                FUN_EXIT;
                return 0;
            }

            n = 0;
            p = 0;
            len = 8;

            if (type == UVC_ITT_CAMERA) {
                // buflen = 50,get bcontrolSize(0x03)
                // so => n = 3
                n = buflen >= 15 ? buffer[14] : 0;
                len = 15;
            } else if (type == UVC_ITT_MEDIA_TRANSPORT_INPUT) {
                n = buflen >= 9 ? buffer[8] : 0;
                p = buflen >= 10 + n ? buffer[9 + n] : 0;
                len = 10;
            }

            // buflen = 50 , (len = 15) + (n = 3) + (p = 0)
            if (buflen < len + n + p) {
                UVC_ERROR("device %d videocontrol interface %d INPUT_TERMINAL error\n", udev->devnum, alts->desc.bInterfaceNumber);
                FUN_EXIT;
                return -USB_EINVAL;
            }

            //  0x0201(UVC_ITT_CAMERA)|0x0000(UVC_TERM_INPUT)
            //  get the bTerminalID(0x01) i.e buffer[3] , n = 3, p = 0
            term = uvc_alloc_entity(type | UVC_TERM_INPUT, buffer[3], 1, n + p);

            if (term == NULL) {
                UVC_ERROR("uvc_alloc_entity fail\n");
                FUN_EXIT;
                return -USB_ENOMEM;
            }

            // term->type = 0x0201(UVC_ITT_CAMERA)
            if (UVC_ENTITY_TYPE(term) == UVC_ITT_CAMERA) {
                // bControlSize = 0x03
                term->camera.bControlSize = n;
                // pointer to the bmControls buffer base address
                term->camera.bmControls = (__u8 *)term + sizeof * term;
                // 0x0000
                term->camera.wObjectiveFocalLengthMin =
                    get_unaligned_le16(&buffer[8]);
                // 0x0000
                term->camera.wObjectiveFocalLengthMax =
                    get_unaligned_le16(&buffer[10]);
                // 0x0000
                term->camera.wOcularFocalLength =
                    get_unaligned_le16(&buffer[12]);
                //bmControls[3] ={0x00,0x00,0x00}
                rtw_memcpy((void *)term->camera.bmControls, (void *)&buffer[15], n);
            } else if (UVC_ENTITY_TYPE(term) ==
                UVC_ITT_MEDIA_TRANSPORT_INPUT) {
                term->media.bControlSize = n;
                term->media.bmControls = (__u8 *)term + sizeof * term;
                term->media.bTransportModeSize = p;
                term->media.bmTransportModes = (__u8 *)term
                    + sizeof * term + n;
                rtw_memcpy((void *)term->media.bmControls, (void *)&buffer[9], n);
                rtw_memcpy((void *)term->media.bmTransportModes, (void *)&buffer[10 + n], p);
            }

            // the iTerminal value = 0x00 which represent the index of a
            // string to describing the Input Terminal
            if (buffer[7] != 0) {
                usb_string(udev, buffer[7], term->name, sizeof term->name);
            } else if (UVC_ENTITY_TYPE(term) == UVC_ITT_CAMERA) { // term->name = "Camera 0"
                sprintf(term->name, "Camera %d", buffer[3]);
            } else if (UVC_ENTITY_TYPE(term) == UVC_ITT_MEDIA_TRANSPORT_INPUT) {
                sprintf(term->name, "Media %d", buffer[3]);
            } else {
                sprintf(term->name, "Input %d", buffer[3]);
            }

            UVC_PRINTF("type:%s", term->name);
            list_add_tail(&term->list, &dev->entities);
            //UVC_PRINTF("entity id:%d, entity type:0x%x", term->id, term->type);
            break;

        case UVC_VC_OUTPUT_TERMINAL: // the fifth into this
            UVC_PRINTF("UVC_VC_OUTPUT_TERMINAL\n");

            // buflen = 21
            if (buflen < 9) {
                UVC_ERROR("device %d videocontrol interface %d OUTPUT_TERMINAL error\n", udev->devnum, alts->desc.bInterfaceNumber);
                FUN_EXIT;
                return -USB_EINVAL;
            }

            /* Make sure the terminal type MSB is not null, otherwise it
             * could be confused with a unit.
             */
            // type = 0x03(VC_OUTPUT_TERMINAL)
            type = get_unaligned_le16(&buffer[4]);

            if ((type & 0xff00) == 0) {
                UVC_ERROR("device %d videocontrol interface %d OUTPUT_TERMINAL %d has invalid type 0x%04x, skipping\n", udev->devnum,
                    alts->desc.bInterfaceNumber, buffer[3], type);
                FUN_EXIT;
                return 0;
            }

            // 0x03(VC_OUTPUT_TERMINAL)| 0x8000(UVC_TERM_OUTPUT)
            // buffer[3] = bTerminalID = 0x03
            term = uvc_alloc_entity(type | UVC_TERM_OUTPUT, buffer[3],
                    1, 0);

            if (term == NULL) {
                UVC_ERROR("uvc_alloc_entity fail\n");
                FUN_EXIT;
                return -USB_ENOMEM;
            }

            // baSourceID = 0x02
            rtw_memcpy((void *)term->baSourceID, (void *)&buffer[7], 1);

            // buffer[8] = 0x02
            if (buffer[8] != 0)
                usb_string(udev, buffer[8], term->name,
                    sizeof term->name);
            else {
                sprintf(term->name, "Output %d", buffer[3]);
            }

            UVC_PRINTF("type:%s", term->name);
            list_add_tail(&term->list, &dev->entities);
            //UVC_PRINTF("entity id:%d, entity type:0x%x, baSourceID:%d/%d", term->id, term->type, (u8)term->baSourceID[0], (u8)buffer[7]);
            break;

        case UVC_VC_SELECTOR_UNIT:
            UVC_PRINTF("UVC_VC_SELECTOR_UNIT\n");
            p = buflen >= 5 ? buffer[4] : 0;

            if (buflen < 5 || buflen < 6 + p) {
                UVC_ERROR("device %d videocontrol interface %d SELECTOR_UNIT error\n", udev->devnum, alts->desc.bInterfaceNumber);
                FUN_EXIT;
                return -USB_EINVAL;
            }

            unit = uvc_alloc_entity(buffer[2], buffer[3], p + 1, 0);

            if (unit == NULL) {
                UVC_ERROR("uvc_alloc_entity fail\n");
                FUN_EXIT;
                return -USB_ENOMEM;
            }

            rtw_memcpy((void *)unit->baSourceID, (void *)&buffer[5], p);

            if (buffer[5 + p] != 0)
                usb_string(udev, buffer[5 + p], unit->name,
                    sizeof unit->name);
            else {
                sprintf(unit->name, "Selector %d", buffer[3]);
            }

            UVC_PRINTF("type:%s", unit->name);
            list_add_tail(&unit->list, &dev->entities);
            //UVC_PRINTF("entity id:%d, entity type:%d, baSourceID:%d", unit->id, unit->type, (u8)unit->baSourceID[0]);
            break;

        case UVC_VC_PROCESSING_UNIT: // the fourth time into this
            UVC_PRINTF("UVC_VC_PROCESSING_UNIT\n");
            // buflen = 32 , get the bControlSize value, n= 0x02
            n = buflen >= 8 ? buffer[7] : 0;
            // dev->uvc_version = 0x0100, p = 9
            p = dev->uvc_version >= 0x0110 ? 10 : 9;

            if (buflen < p + n) {
                UVC_ERROR("device %d videocontrol interface %d PROCESSING_UNIT error\n", udev->devnum, alts->desc.bInterfaceNumber);
                return -USB_EINVAL;
            }

            // buffer[2] = 0x05(VC_PROCESSING_UNIT),
            // buffer[3] = 0x02 , n = 0x02
            unit = uvc_alloc_entity(buffer[2], buffer[3], 2, n);

            if (unit == NULL) {
                UVC_ERROR("uvc_alloc_entity fail\n");
                FUN_EXIT;
                return -USB_ENOMEM;
            }

            // ??? cant see thhis field from device configure descriptor
            rtw_memcpy((void *)unit->baSourceID, (void *)&buffer[4], 1);
            // 0x0000
            unit->processing.wMaxMultiplier =
                get_unaligned_le16(&buffer[5]);
            // 0x02
            unit->processing.bControlSize = buffer[7];
            // pointer to the bmControls buffer base address
            unit->processing.bmControls = (__u8 *)unit + sizeof * unit;
            // copy the bmControl 0~1 content
            rtw_memcpy((void *)unit->processing.bmControls, (void *)&buffer[8], n);

            // dev->uvc_versio = 0x0100
            if (dev->uvc_version >= 0x0110) {
                unit->processing.bmVideoStandards = buffer[9 + n];
            }

            // buffer[8+2] = 0
            if (buffer[8 + n] != 0)
                usb_string(udev, buffer[8 + n], unit->name,
                    sizeof unit->name);
            else {
                sprintf(unit->name, "Processing %d", buffer[3]);    // unit->name = "Processing 2"
            }

            UVC_PRINTF("type:%s", unit->name);
            list_add_tail(&unit->list, &dev->entities);
            //UVC_PRINTF("entity id:%d, entity type:0x%x, baSourceID:%d/%d", unit->id, unit->type, (u8)unit->baSourceID[0], (u8)buffer[4]);
            break;

        case UVC_VC_EXTENSION_UNIT: //second time into this
            UVC_PRINTF("UVC_VC_EXTENSION_UNIT\n");
            //p = 1, buflen = 78, get the bNrInPins from the VS_EXTENSION_UNIT descriptor
            p = buflen >= 22 ? buffer[21] : 0;
            //n = 3, buflen = 78, get the bControlSize from the VS_EXTENSION_UNIT descriptor
            n = buflen >= 24 + p ? buffer[22 + p] : 0;

            // buflen = 78, 24 + 1 + 4
            if (buflen < 24 + p + n) {
                UVC_ERROR("device %d videocontrol interface %d EXTENSION_UNIT error\n", udev->devnum, alts->desc.bInterfaceNumber);
                return -USB_EINVAL;
            }

            // get the bDescriptorSubType1, buffer[2] = VS_EXTENSION_UNIT(6)
            // get the bUnitID, buffer[3] = 0x06
            // p = 1, bNrInPins
            // n = 3, bControlSize
            unit = uvc_alloc_entity(buffer[2], buffer[3], p + 1, n);
            if (unit == NULL) {
                return -USB_ENOMEM;
            }

            // get the guidExtensionCode value which represent vendor-specific code
            // B0D0BB68-A461834B-90B7A621-5F3C4F70 in this Extension(VC_EXTENSION_UNIT)
            rtw_memcpy((void *)unit->extension.guidExtensionCode, (void *)&buffer[4], 16);
            // get the bNumControls value 0x18(24) which means the number of
            // controls in this Extension(VC_EXTENSION_UNIT)
            unit->extension.bNumControls = buffer[20];
            // get the baSourceID(1) value 0x02(2) which means ID of the
            // Unit or Terminal to which the first Input pin of this
            // Extension(VC_EXTENSION_UNIT) is connected 2 , p = 1
            rtw_memcpy((void *)unit->baSourceID, (void *)&buffer[22], p);
            // get the bControlSize value 0x03(3) which means the size of
            // the bmControls field,in bytes is 3.
            unit->extension.bControlSize = buffer[22 + p];
            // pointer to the bmControls buffer base address
            unit->extension.bmControls = (__u8 *)unit + sizeof * unit;
            // get the bControls 0 ~ 3 value 0xFF which means the number of
            // bmControls bytes is 3.
            rtw_memcpy((void *)unit->extension.bmControls, (void *)&buffer[23 + p], n);
            UVC_PRINTF("extension unit bmControls:0x%x %x %x", buffer[15], buffer[16], buffer[17]);

            if (buffer[23 + p + n] != 0) {
                usb_string(udev, buffer[23 + p + n], unit->name, sizeof unit->name);
            } else {
                sprintf(unit->name, "Extension %d", buffer[3]);
            }

            UVC_PRINTF("type:%s", unit->name);
            list_add_tail(&unit->list, &dev->entities);
            //UVC_PRINTF("entity id:%d, entity type:0x%x, baSourceID:%d/%d", unit->id, unit->type, (u8)unit->baSourceID[0], (u8)buffer[22]);
            break;

        default:
            UVC_ERROR("Found an unknown CS_INTERFACE descriptor (%d)\n", buffer[2]);
            break;
    }

    FUN_EXIT;
    return 0;
}

static int uvc_parse_control(struct uvc_device *dev)
{
    FUN_ENTER;
    /*
        // spy tank USB_DT_INTERFACE: Interface descriptor 0
        struct usb_interface_descriptor {
            __u8  bLength = 0x09;
            __u8  bDescriptorType = 0x04;

            __u8  bInterfaceNumber = 0x00;
            __u8  bAlternateSetting = 0x00;
            __u8  bNumEndpoints = 0x01;
            __u8  bInterfaceClass = 0x0E;
            __u8  bInterfaceSubClass = 0x01;
            __u8  bInterfaceProtocol = 0x00;
            __u8  iInterface = 0x04;
        } __attribute__ ((packed));
    */
    struct usb_host_interface *alts = dev->intf->cur_altsetting;
    // the address follow by the current interface 0 descriptor
    // ex: CS_INTERFACE_Descriptor(i.e VC_HEADER) in spy tank
    UVC_PRINTF("bLength = 0x%x\n\r", alts->desc.bLength);
    UVC_PRINTF("bDescriptorType = 0x%x\n\r", alts->desc.bDescriptorType);
    UVC_PRINTF("bInterfaceNumber = 0x%x\n\r", alts->desc.bInterfaceNumber);
    UVC_PRINTF("bAlternateSetting = 0x%x\n\r", alts->desc.bAlternateSetting);
    UVC_PRINTF("bNumEndpoints = 0x%x\n\r", alts->desc.bNumEndpoints);
    UVC_PRINTF("bInterfaceClass = 0x%x\n\r", alts->desc.bInterfaceClass);
    UVC_PRINTF("bInterfaceSubClass = 0x%x\n\r", alts->desc.bInterfaceSubClass);
    UVC_PRINTF("bInterfaceProtocol = 0x%x\n\r", alts->desc.bInterfaceProtocol);
    UVC_PRINTF("iInterface = 0x%x\n\r", alts->desc.iInterface);
    unsigned char *buffer = alts->extra;
    // alts->extralen =  91(13+28+18+11+9+7+5)
    int buflen = alts->extralen;
    int ret;

    /* Parse the default alternate setting only, as the UVC specification
     * defines a single alternate setting, the default alternate setting
     * zero.
     */

    while (buflen > 2) {
        // only apply for logitech's device, spy tank not into this
        if (buffer[1] != USB_DT_CS_INTERFACE) {
            goto next_descriptor;
        }

        if (uvc_parse_vendor_control(dev, buffer, buflen)) {
            goto next_descriptor;
        }

        // mainly handle to parse the CS_INTERFACE_Descriptors
        if ((ret = uvc_parse_standard_control(dev, buffer, buflen)) < 0) {
            return ret;
        }

next_descriptor:
        buflen -= buffer[0];
        buffer += buffer[0];
    }

    /* Check if the optional status endpoint is present. Built-in iSight
     * webcams have an interrupt endpoint but spit proprietary(¯S©w) data that
     * don't conform to the UVC status endpoint messages. Don't try to
     * handle the interrupt endpoint for those cameras.
     */
    // spy tank into this
    if (alts->desc.bNumEndpoints == 1/* && !(dev->quirks & UVC_QUIRK_BUILTIN_ISIGHT)*/) {
        //UVC_ERROR("IN set up status endpoint; quirks=%d",dev->quirks);
        struct usb_host_endpoint *ep = &alts->endpoint[0];
        struct usb_endpoint_descriptor *desc = &ep->desc;

        /* USB_DT_ENDPOINT: Endpoint descriptor 0
            struct usb_endpoint_descriptor {
            __u8  bLength = 0x07;
            __u8  bDescriptorType = 0x05;   // ENDPOINT Descriptor Type

            __u8  bEndpointAddress = 0x81;  // IN Endpoint with num 1
            __u8  bmAttributes = 0x03;  // Transfer Type is Interrupt
            __le16 wMaxPacketSize = 0x0010; // maximum packet = 16 bytes
            __u8  bInterval = 0x07;     // 2^(7-1) = 64 u-frames

            // NOTE:  these two are _only_ in audio endpoints.
            // use USB_DT_ENDPOINT*_SIZE in bLength, not sizeof.
            __u8  bRefresh;
            __u8  bSynchAddress;
        } __attribute__ ((packed));
        */
        if (usb_endpoint_is_int_in(desc) && le16_to_cpu(desc->wMaxPacketSize) >= 8 && desc->bInterval != 0) {
            UVC_PRINTF("Found a Status endpoint (addr %02x).\n", desc->bEndpointAddress);
            // set up the interrupt status endpoint
            dev->int_ep = ep;
        }
    }

    FUN_EXIT;
    return 0;
}

/* ------------------------------------------------------------------------
 * UVC device scan
 */

/*
 * Scan the UVC descriptors to locate a chain starting at an Output Terminal
 * and containing the following units:
 *
 * - one or more Output Terminals (USB Streaming or Display)
 * - zero or one Processing Unit
 * - zero, one or more single-input Selector Units
 * - zero or one multiple-input Selector Units, provided all inputs are
 *   connected to input terminals
 * - zero, one or mode single-input Extension Units
 * - one or more Input Terminals (Camera, External or USB Streaming)
 *
 * The terminal and units must match on of the following structures:
 *
 * ITT_*(0) -> +---------+    +---------+    +---------+ -> TT_STREAMING(0)
 * ...         | SU{0,1} | -> | PU{0,1} | -> | XU{0,n} |    ...
 * ITT_*(n) -> +---------+    +---------+    +---------+ -> TT_STREAMING(n)
 *
 *                 +---------+    +---------+ -> OTT_*(0)
 * TT_STREAMING -> | PU{0,1} | -> | XU{0,n} |    ...
 *                 +---------+    +---------+ -> OTT_*(n)
 *
 * The Processing Unit and Extension Units can be in any order. Additional
 * Extension Units connected to the main chain as single-unit branches are
 * also supported. Single-input Selector Units are ignored.
 */
static int uvc_scan_chain_entity(struct uvc_video_chain *chain,
    struct uvc_entity *entity)
{
    // input : ITT_CAMERA
    // output: TT_STREAMING
    switch (UVC_ENTITY_TYPE(entity)) {
        case UVC_VC_EXTENSION_UNIT:
            if (uvc_trace_param & UVC_TRACE_PROBE) {
                UVC_PRINTF("<- XU %d", entity->id);
            }

            if (entity->bNrInPins != 1) {
                UVC_PRINTF("Extension unit %d has more than 1 input pin.\n", entity->id);
                return -1;
            }

            break;

        case UVC_VC_PROCESSING_UNIT:
            if (uvc_trace_param & UVC_TRACE_PROBE) {
                UVC_PRINTF("<- PU %d", entity->id);
            }

            if (chain->processing != NULL) {
                UVC_PRINTF("Found multiple Processing Units in chain.\n");
                return -1;
            }

            chain->processing = entity;
            break;

        case UVC_VC_SELECTOR_UNIT:
            if (uvc_trace_param & UVC_TRACE_PROBE) {
                UVC_PRINTF("<- SU %d", entity->id);
            }

            /* Single-input selector units are ignored. */
            if (entity->bNrInPins == 1) {
                break;
            }

            if (chain->selector != NULL) {
                UVC_PRINTF("Found multiple Selector Units in chain.\n");
                return -1;
            }

            chain->selector = entity;
            break;

        case UVC_ITT_VENDOR_SPECIFIC:
        case UVC_ITT_CAMERA:
        case UVC_ITT_MEDIA_TRANSPORT_INPUT:
            if (uvc_trace_param & UVC_TRACE_PROBE) {
                UVC_PRINTF("<- IT %d\n", entity->id);
            }

            break;

        case UVC_OTT_VENDOR_SPECIFIC:
        case UVC_OTT_DISPLAY:
        case UVC_OTT_MEDIA_TRANSPORT_OUTPUT:
            if (uvc_trace_param & UVC_TRACE_PROBE) {
                UVC_PRINTF("OT %d\n", entity->id);
            }

            break;

        case UVC_TT_STREAMING:                  // first
            // entity
            if (UVC_ENTITY_IS_ITERM(entity)) {
                if (uvc_trace_param & UVC_TRACE_PROBE) {
                    UVC_PRINTF("<- IT %d\n", entity->id);
                }
            } else {
                if (uvc_trace_param & UVC_TRACE_PROBE) {
                    UVC_PRINTF("OT %d\n", entity->id);
                }
            }

            break;

        default:
            UVC_ERROR("Unsupported entity type 0x%04x found in chain.\n", UVC_ENTITY_TYPE(entity));
            return -1;
    }

    list_add_tail(&entity->chain, &chain->entities);
    return 0;
}

static int uvc_scan_chain_forward(struct uvc_video_chain *chain,
    struct uvc_entity *entity, struct uvc_entity *prev)
{
    struct uvc_entity *forward;
    int found;
    /* Forward scan */
    forward = NULL;
    found = 0;

    while (1) {
        forward = uvc_entity_by_reference(chain->dev, entity->id,
                forward);

        if (forward == NULL) {
            break;
        }

        if (forward == prev) {
            continue;
        }

        switch (UVC_ENTITY_TYPE(forward)) {
            case UVC_VC_EXTENSION_UNIT:
                if (forward->bNrInPins != 1) {
                    UVC_PRINTF("Extension unit %d has more than 1 input pin.\n", entity->id);
                    return -USB_EINVAL;
                }

                list_add_tail(&forward->chain, &chain->entities);

                if (uvc_trace_param & UVC_TRACE_PROBE) {
                    if (!found) {
                        UVC_PRINTF(" (->");
                    }

                    UVC_PRINTF(" XU %d\n", forward->id);
                    found = 1;
                }

                break;

            case UVC_OTT_VENDOR_SPECIFIC:
            case UVC_OTT_DISPLAY:
            case UVC_OTT_MEDIA_TRANSPORT_OUTPUT:
            case UVC_TT_STREAMING:
                if (UVC_ENTITY_IS_ITERM(forward)) {
                    UVC_ERROR("Unsupported input terminal %d.\n", forward->id);
                    return -USB_EINVAL;
                }

                list_add_tail(&forward->chain, &chain->entities);

                if (uvc_trace_param & UVC_TRACE_PROBE) {
                    if (!found) {
                        UVC_PRINTF(" (->");
                    }

                    UVC_PRINTF(" OT %d\n", forward->id);
                    found = 1;
                }

                break;
        }
    }

    return 0;
}

static int uvc_scan_chain_backward(struct uvc_video_chain *chain,
    struct uvc_entity **_entity)
{
    struct uvc_entity *entity = *_entity;
    struct uvc_entity *term;
    int id = -USB_EINVAL, i;

    switch (UVC_ENTITY_TYPE(entity)) {
        case UVC_VC_EXTENSION_UNIT:
        case UVC_VC_PROCESSING_UNIT:
            id = entity->baSourceID[0];
            //UVC_PRINTF("entity id:%d, entity type:0x%x, baSourceID:%d (addr:0x%x)", entity->id, entity->type, id, entity->baSourceID);
            break;

        case UVC_VC_SELECTOR_UNIT:

            /* Single-input selector units are ignored. */
            if (entity->bNrInPins == 1) {
                id = entity->baSourceID[0];
                //UVC_PRINTF("entity id:%d, entity type:%d, baSourceID:%d", entity->id, entity->type, id);
                break;
            }

            if (uvc_trace_param & UVC_TRACE_PROBE) {
                UVC_PRINTF(" <- IT\n");
            }

            chain->selector = entity;

            for (i = 0; i < entity->bNrInPins; ++i) {
                id = entity->baSourceID[i];
                term = uvc_entity_by_id(chain->dev, id);

                if (term == NULL || !UVC_ENTITY_IS_ITERM(term)) {
                    UVC_ERROR("Selector unit %d input %d isn't connected to an input terminal\n", entity->id, i);
                    return -1;
                }

                if (uvc_trace_param & UVC_TRACE_PROBE) {
                    UVC_PRINTF("term id = %d\n", term->id);
                }

                list_add_tail(&term->chain, &chain->entities);
                uvc_scan_chain_forward(chain, term, entity);
            }

            id = 0;
            break;

        case UVC_ITT_VENDOR_SPECIFIC:
        case UVC_ITT_CAMERA:
        case UVC_ITT_MEDIA_TRANSPORT_INPUT:
        case UVC_OTT_VENDOR_SPECIFIC:
        case UVC_OTT_DISPLAY:
        case UVC_OTT_MEDIA_TRANSPORT_OUTPUT:
        case UVC_TT_STREAMING:
            id = UVC_ENTITY_IS_OTERM(entity) ? entity->baSourceID[0] : 0;
            //UVC_PRINTF("entity id:%d, entity type:0x%x, baSourceID:%d (addr:0x%x)", entity->id, entity->type, id, entity->baSourceID);
            break;
    }

    if (id <= 0) {
        *_entity = NULL;
        return id;
    }

    entity = uvc_entity_by_id(chain->dev, id);

    if (entity == NULL) {
        UVC_ERROR("Found reference to unknown entity %d.\n", id);
        return -USB_EINVAL;
    }

    *_entity = entity;
    return 0;
}

static int uvc_scan_chain(struct uvc_video_chain *chain, struct uvc_entity *term)
{
    struct uvc_entity *entity, *prev;
    entity = term;
    prev = NULL;

    while (entity != NULL) {
        /* Entity must not be part of an existing chain */
        if (entity->chain.next || entity->chain.prev) {
            UVC_ERROR("Found reference to entity %d already in chain.\n", entity->id);
            return -USB_EINVAL;
        }

        /* Process entity */
        // output: TT_STREAMING
        if (uvc_scan_chain_entity(chain, entity) < 0) {
            UVC_ERROR("uvc_scan_chain_entity fail\n");
            return -USB_EINVAL;
        }

        /* Forward scan */
        if (uvc_scan_chain_forward(chain, entity, prev) < 0) {
            UVC_ERROR("uvc_scan_chain_forward fail\n");
            return -USB_EINVAL;
        }

        /* Backward scan */
        prev = entity;

        if (uvc_scan_chain_backward(chain, &entity) < 0) {
            UVC_ERROR("uvc_scan_chain_backward fail\n");
            return -USB_EINVAL;
        }
    }

    return 0;
}

/*
 * Scan the device for video chains and register video devices.
 *
 * Chains are scanned starting at their output terminals and walked backwards.
 */
static int uvc_scan_device(struct uvc_device *dev)
{
    FUN_ENTER;
    struct uvc_video_chain *chain;
    struct uvc_entity *term;
    
    // MJPEG: extension_unit->input_terminal->processing_unit->output_terminal
    list_for_each_entry(term, &dev->entities, list, struct uvc_entity) {
        // extension and  process and input will return here.
        if (!UVC_ENTITY_IS_OTERM(term)) {
            continue;
        }

        /* If the terminal is already included in a chain, skip it.
         * This can happen for chains that have multiple output
         * terminals, where all output terminals beside the first one
         * will be inserted in the chain in forward scans.
         */
        if (term->chain.next || term->chain.prev) {
            continue;
        }

        chain = (struct uvc_video_chain *)rtw_zmalloc(sizeof(*chain));
        if (chain == NULL) {
            UVC_ERROR("chain == NULL\n");
            FUN_EXIT;
            return -USB_ENOMEM;
        }

        // initialize the chain's head to pointer itself.
        INIT_LIST_HEAD(&chain->entities);
        Mutex_Init(&chain->ctrl_mutex);
        // setup the dev address in chain to point to the uvc_device
        chain->dev = dev;
        // reset priority flag = 0
        v4l2_prio_init(&chain->prio);
        term->flags |= UVC_ENTITY_FLAG_DEFAULT;

        if (uvc_scan_chain(chain, term) < 0) {
            Mutex_Free(&chain->ctrl_mutex);
            rtw_free((unsigned char *)chain);
            continue;
        }

        // add this chain->list to dev->chains
        list_add_tail(&chain->list, &dev->chains);
    }

    if (list_empty(&dev->chains)) {
        UVC_ERROR("No valid video chain found.\n");
        FUN_EXIT;
        return -1;
    }

    FUN_EXIT;
    return 0;
}

/* ------------------------------------------------------------------------
 * Video device registration and unregistration
 */

/*
 * Delete the UVC device.
 *
 * Called by the kernel when the last reference to the uvc_device structure
 * is released.
 *
 * As this function is called after or during disconnect(), all URBs have
 * already been canceled by the USB core. There is no need to kill the
 * interrupt URB manually.
 */
static void uvc_delete(struct uvc_device *dev)
{
    struct list_head *p, *n;
    
    usb_put_intf(dev->intf);
    usb_put_dev(dev->udev);
#ifdef UVC_STATUS_EN
    uvc_status_cleanup(dev);
#endif
    uvc_ctrl_cleanup_device(dev);

#if 0
    if (dev->vdev.dev) {
        v4l2_device_unregister(&dev->vdev);
    }

#endif

#ifdef CONFIG_MEDIA_CONTROLLER

    if (media_devnode_is_registered(&dev->mdev.devnode)) {
        media_device_unregister(&dev->mdev);
    }

#endif
    list_for_each_safe(p, n, &dev->chains) {
        struct uvc_video_chain *chain;
        chain = list_entry(p, struct uvc_video_chain, list);
        if(chain != NULL){
            Mutex_Free(&chain->ctrl_mutex);
        }
        rtw_free((unsigned char *)chain);
    }
    list_for_each_safe(p, n, &dev->entities) {
        struct uvc_entity *entity;
        entity = list_entry(p, struct uvc_entity, list);
#ifdef CONFIG_MEDIA_CONTROLLER
        uvc_mc_cleanup_entity(entity);
#endif

        if (entity->vdev) {
            video_device_release(entity->vdev);
            entity->vdev = NULL;
        }

        rtw_free((unsigned char *)entity);
    }
    list_for_each_safe(p, n, &dev->streams) {
        struct uvc_streaming *streaming;
        streaming = list_entry(p, struct uvc_streaming, list);
        usb_put_intf(streaming->intf);
        Mutex_Free(&streaming->mutex);
        rtw_free((unsigned char *)streaming->format);
        rtw_free((unsigned char *)streaming->header.bmaControls);
        rtw_free((unsigned char *)streaming);
    }
    Mutex_Free(&dev->lock);
    rtw_free((unsigned char *)dev);
}

static void uvc_release(struct video_device *vdev)
{
    struct uvc_streaming *stream = video_get_drvdata(vdev);
    struct uvc_device *dev = stream->dev;
    
    video_device_release(vdev);

    /* Decrement the registered streams count and delete the device when it
     * reaches zero.
     */
    if (atomic_dec_and_test(&dev->nstreams)) {
        uvc_delete(dev);
    }
}

/*
 * Unregister the video devices.
 */
static void uvc_unregister_video(struct uvc_device *dev)
{
    struct uvc_streaming *stream;
    
    /* Unregistering all video devices might result in uvc_delete() being
     * called from inside the loop if there's no open file handle. To avoid
     * that, increment the stream count before iterating over the streams
     * and decrement it when done.
     */
    atomic_inc(&dev->nstreams);
    list_for_each_entry(stream, &dev->streams, list, struct uvc_streaming) {
        if (stream->vdev == NULL) {
            continue;
        }

        video_unregister_device(stream->vdev);
        stream->vdev = NULL;
        if (&stream->queue.queue.done_wq != NULL) {
            Sema_Free(&stream->queue.queue.done_wq);
        }
        Mutex_Free(&stream->queue.queue.done_lock);
        Mutex_Free(&stream->queue.mutex);
        Mutex_Free(&stream->queue.irqlock);
    }

    /* Decrement the stream count and call uvc_delete explicitly if there
     * are no stream left.
     */
    if (atomic_dec_and_test(&dev->nstreams)) {
        uvc_delete(dev);
    }
}

static int uvc_register_video(struct uvc_device *dev,
    struct uvc_streaming *stream)
{
    struct video_device *vdev;
    int ret;
    
    /* Initialize the streaming interface with default streaming
     * parameters.
     */
    ret = uvc_video_init(stream);
    if (ret < 0) {
        UVC_ERROR("Failed to initialize the device (%d).\n", ret);
        return ret;
    }

    /* Register the device with V4L. (start) */
    vdev = video_device_alloc(); //allocate video device which contains v4l2 device pointer
    if (vdev == NULL) {
        UVC_ERROR("Failed to allocate video device (%d).\n", ret);
        return -USB_ENOMEM;
    }

    /* We already hold a reference to dev->udev. The video device will be
     * unregistered before the reference is released, so we don't need to
     * get another one.
     */
    vdev->v4l2_dev = &dev->vdev;        /* set to pointer to the parent device(v4l2_device) */
    vdev->fops = &uvc_fops;         /* hook the v4l2_file_operations */
    /*  edit by Ian -- hook internal v4l2 device ioctl_ops if any */
    vdev->ioctl_ops = NULL;
    vdev->release = uvc_release;
    vdev->prio = &stream->chain->prio;      /* setup priority */
    set_bit(V4L2_FL_USE_FH_PRIO, &vdev->flags);     /* setup the flags */

    if (stream->type == V4L2_BUF_TYPE_VIDEO_OUTPUT) {
        vdev->vfl_dir = VFL_DIR_TX;    /* if output device then set this */
    }

    /* fill into the unique and descriptive string for video device */
    strlcpy(vdev->name, dev->name, sizeof vdev->name);
    
    /* Set the driver data before calling video_register_device, otherwise
     * uvc_v4l2_open might race us.
     */
    stream->vdev = vdev;
    video_set_drvdata(vdev, stream);
    
    /*edit by Ian -- map video device minor number to stream associated interface number*/
    //ret = video_register_device(vdev, VFL_TYPE_GRABBER, -1);
    ret = video_register_device(vdev, VFL_TYPE_GRABBER, stream->intfnum);
    if (ret < 0) {
        UVC_ERROR("Failed to register video device (%d).\n", ret);
        stream->vdev = NULL;
        video_device_release(vdev);
        return ret;
    }

    /* Register the device with V4L. (End) */

    if (stream->type == V4L2_BUF_TYPE_VIDEO_CAPTURE) {
        stream->chain->caps |= V4L2_CAP_VIDEO_CAPTURE;
    } else {
        stream->chain->caps |= V4L2_CAP_VIDEO_OUTPUT;
    }

    /* increase the number of stream */
    atomic_inc(&dev->nstreams);
    return 0;
}

/*
 * Register all video devices in all chains.
 */
static int uvc_register_terms(struct uvc_device *dev,
    struct uvc_video_chain *chain)
{
    struct uvc_streaming *stream;
    struct uvc_entity *term;
    int ret;
    list_for_each_entry(term, &chain->entities, chain, struct uvc_entity) {
        // only the output terminal can keep going
        if (UVC_ENTITY_TYPE(term) != UVC_TT_STREAMING) {
            continue;
        }

        // term->id = 0x03
        stream = uvc_stream_by_id(dev, term->id);

        if (stream == NULL) {
            UVC_ERROR("No streaming interface found for terminal %d.", term->id);
            continue;
        }

        stream->chain = chain;
        ret = uvc_register_video(dev, stream);

        if (ret < 0) {
            return ret;
        }

        term->vdev = stream->vdev;
    }
    return 0;
}

static int uvc_register_chains(struct uvc_device *dev)
{
    FUN_ENTER;
    struct uvc_video_chain *chain;
    int ret;
    
    list_for_each_entry(chain, &dev->chains, list, struct uvc_video_chain) {
        ret = uvc_register_terms(dev, chain);

        if (ret < 0) {
            UVC_ERROR("uvc_register_terms fail\n");
            FUN_EXIT;
            return ret;
        }

#ifdef CONFIG_MEDIA_CONTROLLER
        ret = uvc_mc_register_entities(chain);
        if (ret < 0) {
            UVC_ERROR("Failed to register entites (%d).\n", ret);
        }
#endif

    }
    FUN_EXIT;
    return 0;
}

#if 0
#include "section_config.h"
SDRAM_DATA_SECTION static struct usb_device_id uvc_auth_ids[] = {
    /* Realtek RTS5821 */
    {
        .match_flags        = USB_DEVICE_ID_MATCH_DEVICE
                            | USB_DEVICE_ID_MATCH_INT_INFO,
        .idVendor           = 0x0bda,
        .idProduct          = 0x5821,
        .bInterfaceClass    = USB_CLASS_VIDEO,
        .bInterfaceSubClass = 1,
        .bInterfaceProtocol = 0,
    },
    /* Realtek RTS5840 */
    {
        .match_flags        = USB_DEVICE_ID_MATCH_DEVICE
                            | USB_DEVICE_ID_MATCH_INT_INFO,
        .idVendor           = 0x0bda,
        .idProduct          = 0x5840,
        .bInterfaceClass    = USB_CLASS_VIDEO,
        .bInterfaceSubClass = 1,
        .bInterfaceProtocol = 0,
    },
    /* Realtek RTS5798 */
    {
        .match_flags        = USB_DEVICE_ID_MATCH_DEVICE
                            | USB_DEVICE_ID_MATCH_INT_INFO,
        .idVendor           = 0x0bda,
        .idProduct          = 0x5798,
        .bInterfaceClass    = USB_CLASS_VIDEO,
        .bInterfaceSubClass = 1,
        .bInterfaceProtocol = 0,
    },
    /* Suyin  */
    {
        .match_flags        = USB_DEVICE_ID_MATCH_DEVICE
                            | USB_DEVICE_ID_MATCH_INT_INFO,
        .idVendor           = 0x064e,
        .idProduct          = 0x980c,
        .bInterfaceClass    = USB_CLASS_VIDEO,
        .bInterfaceSubClass = 1,
        .bInterfaceProtocol = 0,
    },
    /* for GEO*/
    {
        .match_flags        = USB_DEVICE_ID_MATCH_DEVICE
                            | USB_DEVICE_ID_MATCH_INT_INFO,
        .idVendor           = 0x29fe,
        .idProduct          = 0xb00c,
        .bInterfaceClass    = USB_CLASS_VIDEO,
        .bInterfaceSubClass = 1,
        .bInterfaceProtocol = 0,
    },
    {
        .match_flags        = USB_DEVICE_ID_MATCH_DEVICE
                            | USB_DEVICE_ID_MATCH_INT_INFO,
        .idVendor           = 0x29fe,
        .idProduct          = 0x4d53,
        .bInterfaceClass    = USB_CLASS_VIDEO,
        .bInterfaceSubClass = 1,
        .bInterfaceProtocol = 0,
    },
    /* CNFDH48-1 */
    {
        .match_flags        = USB_DEVICE_ID_MATCH_DEVICE
                            | USB_DEVICE_ID_MATCH_INT_INFO,
        .idVendor           = 0x04f2,
        .idProduct          = 0xb413,
        .bInterfaceClass    = USB_CLASS_VIDEO,
        .bInterfaceSubClass = 1,
        .bInterfaceProtocol = 0,
    },
    /* logitech c920 with h264 */
    {
        .match_flags        = USB_DEVICE_ID_MATCH_DEVICE
                            | USB_DEVICE_ID_MATCH_INT_INFO,
        .idVendor           = 0x046d,
        .idProduct          = 0x082d,
        .bInterfaceClass    = USB_CLASS_VIDEO,
        .bInterfaceSubClass = 1,
        .bInterfaceProtocol = 0,
    }
};
#endif

/* add by Ian -- check if the device needs special v4l2 probe, return 1 if yes */
static int is_special_dev(u16 idvendor, u16 idproduct)
{
#include "section_config.h"
    SDRAM_DATA_SECTION static struct usb_device_id spec_dev_list[] = {
        /* GEO gc6500 use their own v4l2_probe function */
        {
            .match_flags        = USB_DEVICE_ID_MATCH_DEVICE
                                | USB_DEVICE_ID_MATCH_INT_INFO,
            .idVendor           = 0x29fe,
            .idProduct          = 0x4d53,
            .bInterfaceClass    = USB_CLASS_VIDEO,
            .bInterfaceSubClass = 1,
            .bInterfaceProtocol = 0,
        }
    };
        
    int i;
    int list_len = ARRAY_SIZE(spec_dev_list);

    for (i = 0; i < list_len; i++) {
        if ((spec_dev_list[i].idVendor == idvendor) && (spec_dev_list[i].idProduct == idproduct)) {
            return 1;
        }
    }

    return 0;
}

/* ------------------------------------------------------------------------
 * USB probe, disconnect, suspend and resume
 */
v4l2_probe_t v4l2_probe = NULL; //function pointer to v4l2_probe function implementation

void general_v4l2_probe(void);

int uvc_probe(struct usb_interface *intf)   //, const struct usb_device_id *id)
{
    /* after the interface and id table match then prob will be called.
     * this udev means the usb device corresponend to the interface */
    //struct usb_interface *intf = (struct usb_interface *)param;
    /* block illogical interface probe entry                        */
#if 0
    int intfSubClass = intf->cur_altsetting->desc.bInterfaceSubClass;
    UVC_PRINTF("interface subclass: %x", intfSubClass);

    if (intfSubClass != UVC_VC_HEADER) {
        goto exit;
    }

#endif
    struct usb_device *udev = interface_to_usbdev(intf);
    struct uvc_device *dev;
    
    FUN_ENTER;
    
    //UVC_PRINTF("device vid:0x%x pid:0x%x", udev->descriptor.idVendor, udev->descriptor.idProduct);
#if 0
    //match device id; to do
    int idlen = ARRAY_SIZE(uvc_auth_ids)/*47*/;
    //UVC_PRINTF("uvc id table length:%d", idlen);
    int i;

    for (i = 0; i < idlen; i++) {
        if (uvc_auth_ids[i].idVendor == udev->descriptor.idVendor && uvc_auth_ids[i].idProduct == udev->descriptor.idProduct) {
            UVC_PRINTF("Probing authorized UVC device %d(%x:%x)\n", udev->devnum, udev->descriptor.idVendor,
                udev->descriptor.idProduct);
            break;
        }
    }

    if (i == idlen) {
        UVC_ERROR("Probing unauthorized UVC device %d - driver may not support\n", udev->devnum);
    }

#else
    UVC_PRINTF("Probing generic UVC device %d\n", udev->devnum);
#endif

    /* allocate memory for the device and initialize it. */
    if ((dev = (struct uvc_device *)rtw_zmalloc(sizeof * dev)) == NULL) {
        UVC_PRINTF("Fail to malloc UVC device\n");
        goto exit;
    }

    INIT_LIST_HEAD(&dev->entities);
    INIT_LIST_HEAD(&dev->chains);
    /* video streaming */
    INIT_LIST_HEAD(&dev->streams);
    atomic_set(&dev->nstreams, 0);
    //atomic_set(&dev->users, 0);
    atomic_set(&dev->nmappings, 0);
    /* protect user's lock */
    Mutex_Init(&dev->lock);
    /* get the pointer to the usb device(i.e udev) with
     * the incremented reference counter. */
    dev->udev = usb_get_dev(udev);
    /* get the pointer to the interface with the
     * incremented reference counter. */
    dev->intf = usb_get_intf(intf);
    /* setup the interface number within uvc_device by interface 0*/
    dev->intfnum = intf->cur_altsetting->desc.bInterfaceNumber;
    dev->quirks = -1;
    //dev->quirks = (uvc_quirks_param == -1) ? id->driver_info : uvc_quirks_param;
    UVC_PRINTF("UVC Camera (%04x:%04x)",
        le16_to_cpu(udev->descriptor.idVendor),
        le16_to_cpu(udev->descriptor.idProduct));

    /* Parse the Video Class Control descriptors. */
    if (uvc_parse_control(dev) < 0) {
        UVC_ERROR("Unable to parse UVC descriptors.\n");
        goto error;
    }

    // "Found UVC 1.00 device QuanJing Webcam (058f:0362)
    UVC_PRINTF("Found UVC %d.%d device %s (%x:%x)\n", dev->uvc_version >> 8,
        dev->uvc_version & 0xff, /*udev->product ? udev->product : */"<unnamed>", le16_to_cpu(udev->descriptor.idVendor),
        le16_to_cpu(udev->descriptor.idProduct));
#if 0

    if (dev->quirks != id->driver_info) {
        UVC_ERROR("\n\rForcing device quirks to 0x%x by module parameter for testing purpose.\n", dev->quirks);
        UVC_ERROR("\n\rPlease report required quirks to the linux-uvc-devel mailing list.\n");
    }

#endif

    /* Register the media and V4L2 devices. */
#ifdef CONFIG_MEDIA_CONTROLLER
    dev->mdev.dev = &intf->dev;   //media device setting
    strlcpy(dev->mdev.model, dev->name, sizeof(dev->mdev.model));

    if (udev->serial)
        strlcpy(dev->mdev.serial, udev->serial, sizeof(dev->mdev.serial));

    strcpy(dev->mdev.bus_info, udev->devpath);
    dev->mdev.hw_revision = le16_to_cpu(udev->descriptor.bcdDevice);
    dev->mdev.driver_version = LINUX_VERSION_CODE;

    if (media_device_register(&dev->mdev) < 0) {
        goto error;
    }

    dev->vdev.mdev = &dev->mdev;
#endif

    /* Initialize controls. */
    if (uvc_ctrl_init_device(dev) < 0) {
        goto error;
    }

    /* Scan the device for video chains. */
    if (uvc_scan_device(dev) < 0) {
        goto error;
    }

    /* Register video device nodes. */
    if (uvc_register_chains(dev) < 0) {
        goto error;
    }

    /* Save our data pointer in the interface data. Then intf knows info about dev*/
    usb_set_intfdata(intf, dev);
    
#ifdef UVC_STATUS_EN
    /* Initialize the interrupt URB. */
    if ((ret = uvc_status_init(dev)) < 0) {
        UVC_ERROR("Unable to initialize the status endpoint (%d), status interrupt will not be supported.\n", ret);
    }
#endif

    UVC_PRINTF("UVC device initialized.\n");

    if (is_special_dev(udev->descriptor.idVendor, udev->descriptor.idProduct)) {
        if (intf->cur_altsetting->desc.bInterfaceNumber != 0) { //only for geo -- cos we only enter the first UVC CTRL intf
            goto error;
        }

        v4l2_probe = spec_v4l2_probe;
    } else {
        v4l2_probe = general_v4l2_probe;
    }

    if (v4l2_probe) {
        v4l2_probe();
    }

    if ((uvch_usr_cb != NULL) && (uvch_usr_cb->attach != NULL)) {
        uvch_usr_cb->attach();
    }
	
    goto exit;

error:
    uvc_unregister_video(dev);
exit:
    FUN_EXIT;
    return 0;
}

static u8 uvc_probe_owner_cnt = 0;
static Mutex uvc_probe_owner_lock = NULL;
void uvc_probe_thread(void *param)
{
    FUN_ENTER;
    struct usb_interface *intf   = (struct usb_interface *)param;

    if ((intf != NULL) && (intf->cur_altsetting->desc.bInterfaceClass == USB_CLASS_VIDEO) &&
        (intf->cur_altsetting->desc.bInterfaceSubClass == UVC_VC_HEADER)) {
        uvc_probe_owner_cnt++;
    } else {
        goto exit;
    }

    if (uvc_probe_owner_lock == NULL) {
        Mutex_Init(&uvc_probe_owner_lock);
    }

    Mutex_Lock(&uvc_probe_owner_lock);
    uvc_probe(intf);
    Mutex_Unlock(&uvc_probe_owner_lock);
    uvc_probe_owner_cnt--;

    if ((uvc_probe_owner_lock != NULL) && (uvc_probe_owner_cnt == 0)) {
        Mutex_Free(&uvc_probe_owner_lock);
        uvc_probe_owner_lock = NULL;
    }

exit:
    FUN_EXIT;
    rtw_thread_exit();
}

int uvc_probe_entry(struct usb_interface *intf)
{
    int ret = 0;
    struct task_struct task;
    
    FUN_ENTER;
    
    ret = rtw_create_task(&task, "uvc_probe_thread", 2048, UVC_TASK_PRIORITY, uvc_probe_thread, intf);
    if (ret != pdPASS) {
        UVC_ERROR("Fail to create UVC probe task: %d\n", ret);
        ret = -1;
    }

    FUN_EXIT;
    return ret;
}

void uvc_disconnect(struct usb_interface *intf)
{
    struct uvc_device *dev = usb_get_intfdata(intf);
    
    /* Set the USB interface data to NULL. This can be done outside the
     * lock, as there's no other reader.
     */
    usb_set_intfdata(intf, NULL);

    if (intf->cur_altsetting->desc.bInterfaceSubClass == UVC_SC_VIDEOSTREAMING) {
        return;
    }

    dev->state |= UVC_DEV_DISCONNECTED;
    uvc_unregister_video(dev);
}

int uvc_suspend(struct usb_interface *intf)
{
    struct uvc_device *dev = usb_get_intfdata(intf);
    struct uvc_streaming *stream;
    
    UVC_ERROR("Suspending interface %d\n", intf->cur_altsetting->desc.bInterfaceNumber);

    /* Controls are cached on the fly so they don't need to be saved. */
    if (intf->cur_altsetting->desc.bInterfaceSubClass ==
        UVC_SC_VIDEOCONTROL) {
#ifdef UVC_STATUS_EN
        Mutex_Lock(&dev->lock);

        if (dev->users) {
            uvc_status_stop(dev);
        }

        Mutex_Unlock(&dev->lock);
#endif
        return 0;
    }

    list_for_each_entry(stream, &dev->streams, list, struct uvc_streaming) {
        if (stream->intf == intf) {
            return uvc_video_suspend(stream);
        }
    }
    UVC_ERROR("Suspend: video streaming USB interface mismatch.\n");
    return -USB_EINVAL;
}

static int __uvc_resume(struct usb_interface *intf, int reset)
{
    struct uvc_device *dev = usb_get_intfdata(intf);
    struct uvc_streaming *stream;
    
    UVC_ERROR("Resuming interface %d\n", intf->cur_altsetting->desc.bInterfaceNumber);

    if (intf->cur_altsetting->desc.bInterfaceSubClass ==
        UVC_SC_VIDEOCONTROL) {
        int ret = 0;

        if (reset) {
            ret = uvc_ctrl_resume_device(dev);

            if (ret < 0) {
                return ret;
            }
        }

#ifdef UVC_STATUS_EN
        Mutex_Lock(&dev->lock);

        if (dev->users) {
            ret = uvc_status_start(dev, GFP_NOIO);
        }

        Mutex_Unlock(&dev->lock);
#endif
        return ret;
    }

    list_for_each_entry(stream, &dev->streams, list, struct uvc_streaming) {
        if (stream->intf == intf) {
            return uvc_video_resume(stream, reset);
        }
    }
    UVC_ERROR("Resume: video streaming USB interface mismatch.\n");
    return -USB_EINVAL;
}

int uvc_resume(struct usb_interface *intf)
{
    return __uvc_resume(intf, 0);
}

// Reset uvc ccontrol setting to default as well
int uvc_reset_resume(struct usb_interface *intf)
{
    return __uvc_resume(intf, 1);
}

#endif // CONFIG_USBH_UVC

