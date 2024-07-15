/*
 *      uvc_v4l2.c  --  USB Video Class driver - V4L2 API
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
#include "v4l2-common.h"
#include "v4l2-ctrls.h"
#include "v4l2-event.h"
#include "v4l2-ioctl.h"
#include "uvc_os_wrap_via_osdep_api.h"
#include "uvcvideo.h"

extern void *replace_files[16];
#define UVC_NONBLOCK 0

/* ------------------------------------------------------------------------
 * UVC ioctls
 */
static int uvc_ioctl_ctrl_map(struct uvc_video_chain *chain,
    struct uvc_xu_control_mapping *xmap)
{
    struct uvc_control_mapping *map;
    unsigned int size;
    int ret;
    map = (struct uvc_control_mapping *)rtw_zmalloc(sizeof * map);

    if (map == NULL) {
        return -USB_ENOMEM;
    }

    rtw_memset(map, 0, sizeof(*map));
    map->id = xmap->id;
    rtw_memcpy(map->name, xmap->name, sizeof map->name);
    rtw_memcpy(map->entity, xmap->entity, sizeof map->entity);
    map->selector = xmap->selector;
    map->size = xmap->size;
    map->offset = xmap->offset;
    map->v4l2_type = (enum v4l2_ctrl_type)xmap->v4l2_type;
    map->data_type = xmap->data_type;

    switch (xmap->v4l2_type) {
        case V4L2_CTRL_TYPE_INTEGER:
        case V4L2_CTRL_TYPE_BOOLEAN:
        case V4L2_CTRL_TYPE_BUTTON:
            break;

        case V4L2_CTRL_TYPE_MENU:

            /* Prevent excessive memory consumption, as well as integer
             * overflows.
             */
            if (xmap->menu_count == 0 ||
                xmap->menu_count > UVC_MAX_CONTROL_MENU_ENTRIES) {
                ret = -USB_EINVAL;
                goto done;
            }

            size = xmap->menu_count * sizeof(*map->menu_info);
            map->menu_info = (struct uvc_menu_info *)rtw_malloc(size);

            if (map->menu_info == NULL) {
                ret = -USB_ENOMEM;
                goto done;
            }

            if (copy_from_user((void *)map->menu_info, (void *)xmap->menu_info, size)) {
                ret = -USB_EFAULT;
                goto done;
            }

            map->menu_count = xmap->menu_count;
            break;

        default:
            UVC_ERROR("Unsupported V4L2 control type %d.\n", xmap->v4l2_type);
            ret = -USB_ENOTTY;
            goto done;
    }

    ret = uvc_ctrl_add_mapping(chain, map);
done:
    rtw_free((unsigned char *)map->menu_info);
    rtw_free((unsigned char *)map);
    return ret;
}

/* ------------------------------------------------------------------------
 * V4L2 interface
 */

/*
 * Find the frame interval closest to the requested frame interval for the
 * given frame format and size. This should be done by the device as part of
 * the Video Probe and Commit negotiation, but some hardware don't implement
 * that feature.
 */
static __u32 uvc_try_frame_interval(struct uvc_frame *frame, __u32 interval)
{
    unsigned int i;

    if (frame->bFrameIntervalType) {
        __u32 best = -1, dist;

        for (i = 0; i < frame->bFrameIntervalType; ++i) {
            dist = interval > frame->dwFrameInterval[i]
                ? interval - frame->dwFrameInterval[i]
                : frame->dwFrameInterval[i] - interval;

            if (dist > best) {
                break;
            }

            best = dist;
        }

        interval = frame->dwFrameInterval[i - 1];
    } else {
        const __u32 min = frame->dwFrameInterval[0];
        const __u32 max = frame->dwFrameInterval[1];
        const __u32 step = frame->dwFrameInterval[2];
        interval = min + (interval - min + step / 2) / step * step;

        if (interval > max) {
            interval = max;
        }
    }

    return interval;
}

static int uvc_v4l2_try_format(struct uvc_streaming *stream,
    struct v4l2_format *fmt, struct uvc_streaming_control *probe,
    struct uvc_format **uvc_format, struct uvc_frame **uvc_frame)
{
    struct uvc_format *format = NULL;
    struct uvc_frame *frame = NULL;
    __u16 rw, rh;
    unsigned int d, maxd;
    unsigned int i;
    __u32 interval;
    int ret = 0;
    __u8 *fcc;

    if (fmt->type != stream->type) {
        return -USB_EINVAL;
    }

    fcc = (__u8 *)&fmt->fmt.pix.pixelformat;
    UVC_PRINTF("Trying format 0x%x (%c%c%c%c): width %d * height %d.\n", fmt->fmt.pix.pixelformat, fcc[0], fcc[1], fcc[2],
        fcc[3], fmt->fmt.pix.width, fmt->fmt.pix.height);

    /* Check if the hardware supports the requested format, use the default
     * format otherwise.
     */
    for (i = 0; i < stream->nformats; ++i) {
        format = &stream->format[i];

        if (format->fcc == fmt->fmt.pix.pixelformat) {
            break;
        }
    }

    if (i == stream->nformats) {
        format = stream->def_format;
        fmt->fmt.pix.pixelformat = format->fcc;
        //UVC_ERROR("trying format not found, use default format instead...\n");
        //edit by Ian -- return error if trying format not supported
        UVC_ERROR("trying format not found");
        return -USB_EINVAL;
    }

    /* Find the closest image size. The distance between image sizes is
     * the size in pixels of the non-overlapping regions between the
     * requested size and the frame-specified size.
     */
    rw = fmt->fmt.pix.width;
    rh = fmt->fmt.pix.height;
    maxd = (unsigned int) -1;

    for (i = 0; i < format->nframes; ++i) {
        __u16 w = format->frame[i].wWidth;
        __u16 h = format->frame[i].wHeight;
        UVC_PRINTF("trying frame %d: width %d * height %d...\n", i, format->frame[i].wWidth, format->frame[i].wHeight);
        d = min(w, rw) * min(h, rh);
        d = w * h + rw * rh - 2 * d;

        if (d < maxd) {
            maxd = d;
            frame = &format->frame[i];
        }

        if (maxd == 0) {
            break;
        }
    }

    if (frame == NULL) {
        UVC_ERROR("Unsupported size %dx%d.\n", fmt->fmt.pix.width, fmt->fmt.pix.height);
        return -USB_EINVAL;
    }

    UVC_ERROR("select %c%c%c%c frame No.%d (%d * %d)", fcc[0], fcc[1], fcc[2], fcc[3], frame->bFrameIndex, frame->wWidth,
        frame->wHeight);
    /* Use the default frame interval. */
    interval = frame->dwDefaultFrameInterval;
    UVC_PRINTF("Using default frame interval %d.%d ds (%d.%d fps).\n", interval / 10, interval % 10, 10000000 / interval,
        (100000000 / interval) % 10);
    /* Set the format index, frame index and frame interval. */
    rtw_memset(probe, 0, sizeof * probe);
    probe->bmHint = 1;  /* dwFrameInterval */
    probe->bFormatIndex = format->index;
    probe->bFrameIndex = frame->bFrameIndex;
    probe->dwFrameInterval = uvc_try_frame_interval(frame, interval);
    /* Some webcams stall the probe control set request when the
     * dwMaxVideoFrameSize field is set to zero. The UVC specification
     * clearly states that the field is read-only from the host, so this
     * is a webcam bug. Set dwMaxVideoFrameSize to the value reported by
     * the webcam to work around the problem.
     *
     * The workaround could probably be enabled for all webcams, so the
     * quirk can be removed if needed. It's currently useful to detect
     * webcam bugs and fix them before they hit the market (providing
     * developers test their webcams with the Linux driver as well as with
     * the Windows driver).
     */
    Mutex_Lock(&stream->mutex);
    
#if 0
    if (stream->dev->quirks & UVC_QUIRK_PROBE_EXTRAFIELDS)
        probe->dwMaxVideoFrameSize =
            stream->ctrl.dwMaxVideoFrameSize;
#endif

    probe->dwMaxVideoFrameSize = stream->ctrl.dwMaxVideoFrameSize;

    /* Probe the device. */
    ret = uvc_probe_video(stream, probe);
    Mutex_Unlock(&stream->mutex);

    if (ret < 0) {
        goto done;
    }

    fmt->fmt.pix.width = frame->wWidth;
    fmt->fmt.pix.height = frame->wHeight;
    fmt->fmt.pix.field = V4L2_FIELD_NONE;
    fmt->fmt.pix.bytesperline = format->bpp * frame->wWidth / 8;
    fmt->fmt.pix.sizeimage = probe->dwMaxVideoFrameSize;
    fmt->fmt.pix.colorspace = format->colorspace;
    fmt->fmt.pix.priv = 0;

    if (uvc_format != NULL) {
        *uvc_format = format;
    }

    if (uvc_frame != NULL) {
        *uvc_frame = frame;
    }

done:
    return ret;
}

static int uvc_v4l2_get_format(struct uvc_streaming *stream,
    struct v4l2_format *fmt)
{
    struct uvc_format *format;
    struct uvc_frame *frame;
    int ret = 0;

    if (fmt->type != stream->type) {
        return -USB_EINVAL;
    }

    Mutex_Lock(&stream->mutex);
    format = stream->cur_format;
    frame = stream->cur_frame;

    if (format == NULL || frame == NULL) {
        ret = -USB_EINVAL;
        goto done;
    }

    fmt->fmt.pix.pixelformat = format->fcc;
    fmt->fmt.pix.width = frame->wWidth;
    fmt->fmt.pix.height = frame->wHeight;
    fmt->fmt.pix.field = V4L2_FIELD_NONE;
    fmt->fmt.pix.bytesperline = format->bpp * frame->wWidth / 8;
    fmt->fmt.pix.sizeimage = /*stream->ctrl.dwMaxVideoFrameSize*/format->bpp * frame->wWidth / 8 *
        frame->wHeight;//strict image size to minimal
    fmt->fmt.pix.colorspace = format->colorspace;
    fmt->fmt.pix.priv = 0;
done:
    Mutex_Unlock(&stream->mutex);
    return ret;
}

static int uvc_v4l2_set_format(struct uvc_streaming *stream,
    struct v4l2_format *fmt)
{
    struct uvc_streaming_control probe;
    struct uvc_format *format;
    struct uvc_frame *frame;
    int ret;

    if (fmt->type != stream->type) {
        return -USB_EINVAL;
    }

    ret = uvc_v4l2_try_format(stream, fmt, &probe, &format, &frame);

    if (ret < 0) {
        return ret;
    }

    Mutex_Lock(&stream->mutex);
    
#if 0
    if (uvc_queue_allocated(&stream->queue)) {
        ret = -USB_EBUSY;
        goto done;
    }
#endif

    //edit by Ian
    if (uvc_queue_allocated(&stream->queue) && (uvc_queue_streaming(&stream->queue))) {
        ret = -USB_EBUSY;
        goto done;
    }

    stream->ctrl = probe;
    stream->cur_format = format;
    stream->cur_frame = frame;
    UVC_PRINTF("current format:%d, current frame:%d\n", stream->cur_format->fcc, stream->cur_frame->bFrameIndex);
done:
    Mutex_Unlock(&stream->mutex);
    return ret;
}

static int uvc_v4l2_get_streamparm(struct uvc_streaming *stream,
    struct v4l2_streamparm *parm)
{
    uint32_t numerator, denominator;

    if (parm->type != stream->type) {
        return -USB_EINVAL;
    }

    Mutex_Lock(&stream->mutex);
    numerator = stream->ctrl.dwFrameInterval;
    Mutex_Unlock(&stream->mutex);
    denominator = 10000000;
    uvc_simplify_fraction(&numerator, &denominator, 8, 333);
    rtw_memset(parm, 0, sizeof * parm);
    parm->type = stream->type;

    if (stream->type == V4L2_BUF_TYPE_VIDEO_CAPTURE) {
        parm->parm.capture.capability = V4L2_CAP_TIMEPERFRAME;
        parm->parm.capture.capturemode = 0;
        parm->parm.capture.timeperframe.numerator = numerator;
        parm->parm.capture.timeperframe.denominator = denominator;
        parm->parm.capture.extendedmode = 0;
        parm->parm.capture.readbuffers = 0;
    } else {
        parm->parm.output.capability = V4L2_CAP_TIMEPERFRAME;
        parm->parm.output.outputmode = 0;
        parm->parm.output.timeperframe.numerator = numerator;
        parm->parm.output.timeperframe.denominator = denominator;
    }

    return 0;
}

static int uvc_v4l2_set_streamparm(struct uvc_streaming *stream,
    struct v4l2_streamparm *parm)
{
    struct uvc_streaming_control probe;
    struct v4l2_fract timeperframe;
    uint32_t interval;
    int ret;

    if (parm->type != stream->type) {
        return -USB_EINVAL;
    }

    if (parm->type == V4L2_BUF_TYPE_VIDEO_CAPTURE) {
        timeperframe = parm->parm.capture.timeperframe;
    } else {
        timeperframe = parm->parm.output.timeperframe;
    }

    interval = uvc_fraction_to_interval(timeperframe.numerator,
            timeperframe.denominator);
    UVC_PRINTF("Setting frame interval to %d/%d (%d).\n", timeperframe.numerator, timeperframe.denominator, interval);
    Mutex_Lock(&stream->mutex);

    /* edit by Ian -- according to GEO patch we don't check stream uvc_queue availability. But this might arouse some issues so I keep it here */
    if (uvc_queue_streaming(&stream->queue)) {
        Mutex_Unlock(&stream->mutex);
        return -USB_EBUSY;
    }

    rtw_memcpy(&probe, &stream->ctrl, sizeof probe);
    probe.dwFrameInterval =
        uvc_try_frame_interval(stream->cur_frame, interval);
    /* Probe the device with the new settings. */
    ret = uvc_probe_video(stream, &probe);

    if (ret < 0) {
        Mutex_Unlock(&stream->mutex);
        return ret;
    }

    rtw_memcpy(&stream->ctrl, &probe, sizeof probe);
    Mutex_Unlock(&stream->mutex);
    /* end of edit */
    /* Return the actual frame period. */
    timeperframe.numerator = probe.dwFrameInterval;
    timeperframe.denominator = 10000000;
    uvc_simplify_fraction(&timeperframe.numerator,
        &timeperframe.denominator, 8, 333);
    UVC_PRINTF("set frame interval %d/%d", timeperframe.numerator, timeperframe.denominator);

    if (parm->type == V4L2_BUF_TYPE_VIDEO_CAPTURE) {
        parm->parm.capture.timeperframe = timeperframe;
    } else {
        parm->parm.output.timeperframe = timeperframe;
    }

    return 0;
}

/* ------------------------------------------------------------------------
 * Privilege management
 */

/*
 * Privilege management is the multiple-open implementation basis. The current
 * implementation is completely transparent for the end-user and doesn't
 * require explicit use of the VIDIOC_G_PRIORITY and VIDIOC_S_PRIORITY ioctls.
 * Those ioctls enable finer control on the device (by making possible for a
 * user to request exclusive access to a device), but are not mature yet.
 * Switching to the V4L2 priority mechanism might be considered in the future
 * if this situation changes.
 *
 * Each open instance of a UVC device can either be in a privileged or
 * unprivileged state. Only a single instance can be in a privileged state at
 * a given time. Trying to perform an operation that requires privileges will
 * automatically acquire the required privileges if possible, or return -USB_EBUSY
 * otherwise. Privileges are dismissed when closing the instance or when
 * freeing the video buffers using VIDIOC_REQBUFS.
 *
 * Operations that require privileges are:
 *
 * - VIDIOC_S_INPUT
 * - VIDIOC_S_PARM
 * - VIDIOC_S_FMT
 * - VIDIOC_REQBUFS
 */
static int uvc_acquire_privileges(struct uvc_fh *handle)
{
    /* Always succeed if the handle is already privileged. */
    if (handle->state == UVC_HANDLE_ACTIVE) {
        return 0;
    }

    /* Check if the device already has a privileged handle. */
    if (atomic_inc_return(&handle->stream->active) != 1) {
        atomic_dec(&handle->stream->active);
        UVC_WARN("already have privileged handle");
        return -USB_EBUSY;
    }

    handle->state = UVC_HANDLE_ACTIVE;
    return 0;
}

static void uvc_dismiss_privileges(struct uvc_fh *handle)
{
    if (handle->state == UVC_HANDLE_ACTIVE) {
        atomic_dec(&handle->stream->active);
    }

    handle->state = UVC_HANDLE_PASSIVE;
}

static int uvc_has_privileges(struct uvc_fh *handle)
{
    return handle->state == UVC_HANDLE_ACTIVE;
}

/* ------------------------------------------------------------------------
 * V4L2 file operations
 */

int uvc_v4l2_open(void)
{
    FUN_ENTER;
    struct uvc_streaming *stream;
    struct uvc_fh *handle;
#ifdef UVC_STATUS_EN
    int ret = 0;
#endif

    stream = video_drvdata();
    if (stream->dev->state & UVC_DEV_DISCONNECTED) {
        return -USB_ENODEV;
    }

    /* Create the device handle. */
    handle = (struct uvc_fh *)rtw_zmalloc(sizeof * handle);
    if (handle == NULL) {
        return -USB_ENOMEM;
    }

#ifdef UVC_STATUS_EN
    Mutex_Lock(&stream->dev->lock);

    if (stream->dev->users == 0) {
        ret = uvc_status_start(stream->dev, GFP_KERNEL);
        if (ret < 0) {
            Mutex_Unlock(&stream->dev->lock);
            rtw_free((unsigned char *)handle);
            return ret;
        }
    }

    stream->dev->users++;
    Mutex_Unlock(&stream->dev->lock);
#endif

    v4l2_fh_init(&handle->vfh, stream->vdev);
    v4l2_fh_add(&handle->vfh);
    handle->chain = stream->chain;
    handle->stream = stream;
    handle->state = UVC_HANDLE_PASSIVE;
    replace_files[stream->vdev->minor] = (void *)handle;
    FUN_EXIT;
    return 0;
}

int uvc_v4l2_release(void)
{
    FUN_ENTER;
    struct video_device *vdev = video_devdata();
    struct uvc_fh *handle = (struct uvc_fh *)replace_files[vdev->minor];
    struct uvc_streaming *stream = (struct uvc_streaming *)handle->stream;
    UVC_ERROR("uvc_v4l2_release\n");

    /* Only free resources if this is a privileged handle. */
    if (uvc_has_privileges(handle)) {
        uvc_video_enable(stream, 0);
        uvc_free_buffers(&stream->queue);
    }

    /* Release the file handle. */
    uvc_dismiss_privileges(handle);
    v4l2_fh_del(&handle->vfh);
    v4l2_fh_exit(&handle->vfh);
    rtw_free((unsigned char *)handle);
    replace_files[vdev->minor] = NULL;
#ifdef UVC_STATUS_EN
    Mutex_Lock(&stream->dev->lock);

    if (--stream->dev->users == 0) {
        uvc_status_stop(stream->dev);
    }

    Mutex_Unlock(&stream->dev->lock);
#endif
    FUN_EXIT;
    return 0;
}

long uvc_v4l2_do_ioctl(unsigned int cmd, void *arg)
{
    struct video_device *vdev;
    struct uvc_fh *handle;
    struct uvc_video_chain *chain;
    struct uvc_streaming *stream;
    long ret = 0;
    
    vdev = video_devdata();
    if ((vdev == NULL) || (vdev->minor >= 16)) {
        UVC_ERROR("Invalid UVC device");
        return -USB_EINVAL;
    }
    
    handle = (struct uvc_fh *)replace_files[vdev->minor];
    if (handle == NULL) {
        UVC_ERROR("Invalid UVC handle");
        return -USB_EINVAL;
    }
    
    chain = handle->chain;
    stream = handle->stream;

    switch (cmd) {
        /* Query capabilities */
        case VIDIOC_QUERYCAP: {
            UVC_PRINTF("VIDIOC_QUERYCAP\n");
            struct v4l2_capability *cap = arg;
            rtw_memset(cap, 0, sizeof * cap);
            strlcpy((char *)cap->driver, "uvcvideo", sizeof cap->driver);
            strlcpy((char *)cap->card, vdev->name, sizeof cap->card);
            /* edit by Ian -- patch for GEO */
            strlcpy((char *)cap->bus_info, "hidden", sizeof(cap->bus_info));
            cap->version = LINUX_VERSION_CODE;
            cap->capabilities = V4L2_CAP_DEVICE_CAPS | V4L2_CAP_STREAMING
                | chain->caps;

            if (stream->type == V4L2_BUF_TYPE_VIDEO_CAPTURE)
                cap->device_caps = V4L2_CAP_VIDEO_CAPTURE
                    | V4L2_CAP_STREAMING;
            else
                cap->device_caps = V4L2_CAP_VIDEO_OUTPUT
                    | V4L2_CAP_STREAMING;

            break;
        }

        /* Priority */
        case VIDIOC_G_PRIORITY:
            UVC_PRINTF("VIDIOC_G_PRIORITY\n");
            *(u32 *)arg = v4l2_prio_max(vdev->prio);
            break;

        case VIDIOC_S_PRIORITY:
            UVC_PRINTF("VIDIOC_S_PRIORITY\n");
            ret = v4l2_prio_check(vdev->prio, handle->vfh.prio);

            if (ret < 0) {
                return ret;
            }

            return v4l2_prio_change(vdev->prio, &handle->vfh.prio, (enum v4l2_priority)(*(u32 *)arg));

        /* Get, Set & Query control */
        case VIDIOC_QUERYCTRL:
            UVC_PRINTF("VIDIOC_QUERYCTRL\n");
            return uvc_query_v4l2_ctrl(chain, (struct v4l2_queryctrl *)arg);

        case VIDIOC_G_CTRL: {
            UVC_PRINTF("VIDIOC_G_CTRL\n");
            struct v4l2_control *ctrl = arg;
            struct v4l2_ext_control xctrl;
            rtw_memset(&xctrl, 0, sizeof xctrl);
            xctrl.id = ctrl->id;
            ret = uvc_ctrl_begin(chain);

            if (ret < 0) {
                return ret;
            }

            ret = uvc_ctrl_get(chain, &xctrl);
            uvc_ctrl_rollback(handle);

            if (ret >= 0) {
                ctrl->value = xctrl.value;
            }

            break;
        }

        case VIDIOC_S_CTRL: {
            UVC_PRINTF("VIDIOC_S_CTRL\n");
            struct v4l2_control *ctrl = arg;
            struct v4l2_ext_control xctrl;
            ret = v4l2_prio_check(vdev->prio, handle->vfh.prio);

            if (ret < 0) {
                return ret;
            }

            rtw_memset(&xctrl, 0, sizeof xctrl);
            xctrl.id = ctrl->id;
            xctrl.value = ctrl->value;
            ret = uvc_ctrl_begin(chain);

            if (ret < 0) {
                return ret;
            }

            ret = uvc_ctrl_set(chain, &xctrl);

            if (ret < 0) {
                uvc_ctrl_rollback(handle);
                return ret;
            }

            ret = uvc_ctrl_commit(handle, &xctrl, 1);

            if (ret == 0) {
                ctrl->value = xctrl.value;
            }

            break;
        }

        case VIDIOC_QUERYMENU:
            UVC_PRINTF("VIDIOC_QUERYMENU\n");
            return uvc_query_v4l2_menu(chain, (struct v4l2_querymenu *)arg);

        case VIDIOC_G_EXT_CTRLS: {
            UVC_PRINTF("VIDIOC_G_EXT_CTRLS\n");
            struct v4l2_ext_controls *ctrls = arg;
            struct v4l2_ext_control *ctrl = ctrls->controls;
            unsigned int i;
            ret = uvc_ctrl_begin(chain);

            if (ret < 0) {
                return ret;
            }

            for (i = 0; i < ctrls->count; ++ctrl, ++i) {
                ret = uvc_ctrl_get(chain, ctrl);

                if (ret < 0) {
                    uvc_ctrl_rollback(handle);
                    ctrls->error_idx = i;
                    return ret;
                }
            }

            ctrls->error_idx = 0;
            ret = uvc_ctrl_rollback(handle);
            break;
        }

        case VIDIOC_S_EXT_CTRLS:
            UVC_PRINTF("VIDIOC_S_EXT_CTRLS\n");
            ret = v4l2_prio_check(vdev->prio, handle->vfh.prio);

            if (ret < 0) {
                return ret;
            }

        /* Fall through */
        case VIDIOC_TRY_EXT_CTRLS: {
            UVC_PRINTF("VIDIOC_TRY_EXT_CTRLS\n");
            struct v4l2_ext_controls *ctrls = arg;
            struct v4l2_ext_control *ctrl = ctrls->controls;
            unsigned int i;
            ret = uvc_ctrl_begin(chain);

            if (ret < 0) {
                return ret;
            }

            for (i = 0; i < ctrls->count; ++ctrl, ++i) {
                ret = uvc_ctrl_set(chain, ctrl);

                if (ret < 0) {
                    uvc_ctrl_rollback(handle);
                    ctrls->error_idx = cmd == VIDIOC_S_EXT_CTRLS
                        ? ctrls->count : i;
                    return ret;
                }
            }

            ctrls->error_idx = 0;

            if (cmd == VIDIOC_S_EXT_CTRLS)
                ret = uvc_ctrl_commit(handle,
                        ctrls->controls, ctrls->count);
            else {
                ret = uvc_ctrl_rollback(handle);
            }

            break;
        }

        /* Get, Set & Enum input */
        case VIDIOC_ENUMINPUT: {
            UVC_PRINTF("VIDIOC_ENUMINPUT\n");
            const struct uvc_entity *selector = chain->selector;
            struct v4l2_input *input = arg;
            struct uvc_entity *iterm = NULL;
            u32 index = input->index;
            int pin = 0;

            if (selector == NULL /*||
               (chain->dev->quirks & UVC_QUIRK_IGNORE_SELECTOR_UNIT)*/) {
                if (index != 0) {
                    return -USB_EINVAL;
                }

                list_for_each_entry(iterm, &chain->entities, chain, struct uvc_entity) {
                    if (UVC_ENTITY_IS_ITERM(iterm)) {
                        break;
                    }
                }
                pin = iterm->id;
            } else if (index < selector->bNrInPins) {
                pin = selector->baSourceID[index];
                list_for_each_entry(iterm, &chain->entities, chain, struct uvc_entity) {
                    if (!UVC_ENTITY_IS_ITERM(iterm)) {
                        continue;
                    }

                    if (iterm->id == pin) {
                        break;
                    }
                }
            }

            if (iterm == NULL || iterm->id != pin) {
                return -USB_EINVAL;
            }

            rtw_memset(input, 0, sizeof * input);
            input->index = index;
            strlcpy((char *)input->name, iterm->name, sizeof input->name);

            if (UVC_ENTITY_TYPE(iterm) == UVC_ITT_CAMERA) {
                input->type = V4L2_INPUT_TYPE_CAMERA;
            }

            break;
        }

        case VIDIOC_G_INPUT: {
            UVC_PRINTF("VIDIOC_G_INPUT\n");
            u8 input;

            if (chain->selector == NULL /*||
               (chain->dev->quirks & UVC_QUIRK_IGNORE_SELECTOR_UNIT)*/) {
                *(int *)arg = 0;
                break;
            }

            ret = uvc_query_ctrl(chain->dev, UVC_GET_CUR,
                    chain->selector->id, chain->dev->intfnum,
                    UVC_SU_INPUT_SELECT_CONTROL, &input, 1);
            if (ret < 0) {
                return ret;
            }

            *(int *)arg = input - 1;
            break;
        }

        case VIDIOC_S_INPUT: {
            UVC_PRINTF("VIDIOC_S_INPUT\n");
            u32 input = *(u32 *)arg + 1;
        
            ret = v4l2_prio_check(vdev->prio, handle->vfh.prio);
            if (ret < 0) {
                return ret;
            }

            if ((ret = uvc_acquire_privileges(handle)) < 0) {
                return ret;
            }

            if (chain->selector == NULL/* ||
               (chain->dev->quirks & UVC_QUIRK_IGNORE_SELECTOR_UNIT)*/) {
                if (input != 1) {
                    return -USB_EINVAL;
                }

                break;
            }

            if (input == 0 || input > chain->selector->bNrInPins) {
                return -USB_EINVAL;
            }

            return uvc_query_ctrl(chain->dev, UVC_SET_CUR,
                    chain->selector->id, chain->dev->intfnum,
                    UVC_SU_INPUT_SELECT_CONTROL, &input, 1);
        }

        /* Try, Get, Set & Enum format */
        case VIDIOC_ENUM_FMT: {
            UVC_PRINTF("VIDIOC_ENUM_FMT\n");
            struct v4l2_fmtdesc *fmt = arg;
            struct uvc_format *format;
            enum v4l2_buf_type type = (enum v4l2_buf_type)fmt->type;
            __u32 index = fmt->index;

            if (fmt->type != stream->type ||
                fmt->index >= stream->nformats) {
                return -USB_EINVAL;
            }

            rtw_memset(fmt, 0, sizeof(*fmt));
            fmt->index = index;
            fmt->type = type;
            format = &stream->format[fmt->index];
            fmt->flags = 0;

            if (format->flags & UVC_FMT_FLAG_COMPRESSED) {
                fmt->flags |= V4L2_FMT_FLAG_COMPRESSED;
            }

            strlcpy((char *)fmt->description, format->name, sizeof fmt->description);
            fmt->description[sizeof fmt->description - 1] = 0;
            fmt->pixelformat = format->fcc;
            break;
        }

        case VIDIOC_TRY_FMT: {
            UVC_PRINTF("VIDIOC_TRY_FMT\n");
            struct uvc_streaming_control probe;
            return uvc_v4l2_try_format(stream, (struct v4l2_format *)arg, &probe, NULL, NULL);
        }

        case VIDIOC_S_FMT:
            UVC_PRINTF("VIDIOC_S_FMT\n");
            
            ret = v4l2_prio_check(vdev->prio, handle->vfh.prio);
            if (ret < 0) {
                return ret;
            }

            ret = uvc_acquire_privileges(handle);
            if (ret < 0) {
                return ret;
            }

            return uvc_v4l2_set_format(stream, (struct v4l2_format *)arg);

        case VIDIOC_G_FMT:
            UVC_PRINTF("VIDIOC_G_FMT\n");
            return uvc_v4l2_get_format(stream, (struct v4l2_format *)arg);

        /* Frame size enumeration */
        case VIDIOC_ENUM_FRAMESIZES: {
            UVC_PRINTF("VIDIOC_ENUM_FRAMESIZES\n");
            struct v4l2_frmsizeenum *fsize = arg;
            struct uvc_format *format = NULL;
            struct uvc_frame *frame;
            unsigned int i;

            /* Look for the given pixel format */
            for (i = 0; i < stream->nformats; i++) {
                if (stream->format[i].fcc ==
                    fsize->pixel_format) {
                    format = &stream->format[i];
                    break;
                }
            }

            if (format == NULL) {
                return -USB_EINVAL;
            }

            if (fsize->index >= format->nframes) {
                return -USB_EINVAL;
            }

            frame = &format->frame[fsize->index];
            fsize->type = V4L2_FRMSIZE_TYPE_DISCRETE;
            fsize->discrete.width = frame->wWidth;
            fsize->discrete.height = frame->wHeight;
            break;
        }

        /* Frame interval enumeration */
        case VIDIOC_ENUM_FRAMEINTERVALS: {
            UVC_PRINTF("VIDIOC_ENUM_FRAMEINTERVALS\n");
            struct v4l2_frmivalenum *fival = arg;
            struct uvc_format *format = NULL;
            struct uvc_frame *frame = NULL;
            unsigned int i;

            /* Look for the given pixel format and frame size */
            for (i = 0; i < stream->nformats; i++) {
                if (stream->format[i].fcc ==
                    fival->pixel_format) {
                    format = &stream->format[i];
                    break;
                }
            }

            if (format == NULL) {
                return -USB_EINVAL;
            }

            for (i = 0; i < format->nframes; i++) {
                if (format->frame[i].wWidth == fival->width &&
                    format->frame[i].wHeight == fival->height) {
                    frame = &format->frame[i];
                    break;
                }
            }

            if (frame == NULL) {
                return -USB_EINVAL;
            }

            if (frame->bFrameIntervalType) {
                if (fival->index >= frame->bFrameIntervalType) {
                    return -USB_EINVAL;
                }

                fival->type = V4L2_FRMIVAL_TYPE_DISCRETE;
                fival->discrete.numerator =
                    frame->dwFrameInterval[fival->index];
                fival->discrete.denominator = 10000000;
                uvc_simplify_fraction(&fival->discrete.numerator,
                    &fival->discrete.denominator, 8, 333);
            } else {
                fival->type = V4L2_FRMIVAL_TYPE_STEPWISE;
                fival->stepwise.min.numerator =
                    frame->dwFrameInterval[0];
                fival->stepwise.min.denominator = 10000000;
                fival->stepwise.max.numerator =
                    frame->dwFrameInterval[1];
                fival->stepwise.max.denominator = 10000000;
                fival->stepwise.step.numerator =
                    frame->dwFrameInterval[2];
                fival->stepwise.step.denominator = 10000000;
                uvc_simplify_fraction(&fival->stepwise.min.numerator,
                    &fival->stepwise.min.denominator, 8, 333);
                uvc_simplify_fraction(&fival->stepwise.max.numerator,
                    &fival->stepwise.max.denominator, 8, 333);
                uvc_simplify_fraction(&fival->stepwise.step.numerator,
                    &fival->stepwise.step.denominator, 8, 333);
            }

            break;
        }

        /* Get & Set streaming parameters */
        case VIDIOC_G_PARM:
            UVC_PRINTF("VIDIOC_G_PARM\n");
            return uvc_v4l2_get_streamparm(stream, (struct v4l2_streamparm *)arg);

        case VIDIOC_S_PARM:
            UVC_PRINTF("VIDIOC_S_PARM\n");
            ret = v4l2_prio_check(vdev->prio, handle->vfh.prio);

            if (ret < 0) {
                return ret;
            }

            if ((ret = uvc_acquire_privileges(handle)) < 0) {
                return ret;
            }

            return uvc_v4l2_set_streamparm(stream, (struct v4l2_streamparm *)arg);

        /* Cropping and scaling */
        case VIDIOC_CROPCAP: {
            UVC_PRINTF("VIDIOC_CROPCAP\n");
            struct v4l2_cropcap *ccap = arg;

            if (ccap->type != stream->type) {
                return -USB_EINVAL;
            }

            ccap->bounds.left = 0;
            ccap->bounds.top = 0;
            Mutex_Lock(&stream->mutex);
            ccap->bounds.width = stream->cur_frame->wWidth;
            ccap->bounds.height = stream->cur_frame->wHeight;
            Mutex_Unlock(&stream->mutex);
            ccap->defrect = ccap->bounds;
            ccap->pixelaspect.numerator = 1;
            ccap->pixelaspect.denominator = 1;
            break;
        }

        case VIDIOC_G_CROP:
        case VIDIOC_S_CROP:
            UVC_PRINTF("VIDIOC_G_CROP/VIDIOC_S_CROP\n");
            return -USB_ENOTTY;

        /* Buffers & streaming */
        case VIDIOC_REQBUFS:
            UVC_PRINTF("VIDIOC_REQBUFS\n");
            ret = v4l2_prio_check(vdev->prio, handle->vfh.prio);

            if (ret < 0) {
                return ret;
            }

            if ((ret = uvc_acquire_privileges(handle)) < 0) {
                return ret;
            }

            Mutex_Lock(&stream->mutex);
            ret = uvc_alloc_buffers(&stream->queue, arg);
            Mutex_Unlock(&stream->mutex);

            if (ret < 0) {
                UVC_ERROR("VIDIOC_REQBUFS error: %d\n", ret);

                if (ret == -USB_EBUSY) {
                    ret = 0;
                }
            }

            if (ret == 0) {
                uvc_dismiss_privileges(handle);
            }

            ret = 0;
            break;

        case VIDIOC_QUERYBUF: {
            UVC_PRINTF("VIDIOC_QUERYBUF\n");
            struct v4l2_buffer *buf = arg;

            if (!uvc_has_privileges(handle)) {
                return -USB_EBUSY;
            }

            return uvc_query_buffer(&stream->queue, buf);
        }

        case VIDIOC_QBUF:
            UVC_PRINTF("VIDIOC_QBUF\n");

            if (!uvc_has_privileges(handle)) {
                return -USB_EBUSY;
            }

            return uvc_queue_buffer(&stream->queue, (struct v4l2_buffer *)arg);

        case VIDIOC_DQBUF:
            UVC_PRINTF("VIDIOC_DQBUF\n");

            if (!uvc_has_privileges(handle)) {
                return -USB_EBUSY;
            }

            ret = uvc_dequeue_buffer(&stream->queue, (struct v4l2_buffer *)arg, UVC_NONBLOCK);
            return ret;

        case VIDIOC_STREAMON: {
            UVC_PRINTF("VIDIOC_STREAMON\n");
            int *type = arg;
            unsigned long *ltype = arg; 

            if (*type != stream->type) {
                UVC_ERROR("VIDIOC_STREAMON failed, invalid stream type %d, valid type %d, ltype %d", *type, stream->type, *ltype);
                return -USB_EINVAL;
            }

            ret = v4l2_prio_check(vdev->prio, handle->vfh.prio);
            if (ret < 0) {
                return ret;
            }

            if (!uvc_has_privileges(handle)) {
                return -USB_EBUSY;
            }

            Mutex_Lock(&stream->mutex);
            ret = uvc_video_enable(stream, 1);
            Mutex_Unlock(&stream->mutex);

            if (ret < 0) {
                UVC_ERROR("VIDIOC_STREAMON failed to enable video");
                return ret;
            }

            break;
        }

        case VIDIOC_STREAMOFF: {
            UVC_PRINTF("VIDIOC_STREAMOFF\n");
            int *type = arg;

            if (*type != stream->type) {
                return -USB_EINVAL;
            }

            ret = v4l2_prio_check(vdev->prio, handle->vfh.prio);

            if (ret < 0) {
                return ret;
            }

            if (!uvc_has_privileges(handle)) {
                return -USB_EBUSY;
            }

            return uvc_video_enable(stream, 0);
        }

        //event handling not to support in ameba
        case VIDIOC_SUBSCRIBE_EVENT: {
            UVC_PRINTF("VIDIOC_SUBSCRIBE_EVENT\n");
            struct v4l2_event_subscription *sub = arg;

            switch (sub->type) {
                case V4L2_EVENT_CTRL:
#if 0
                    return v4l2_event_subscribe(&handle->vfh, sub, 0,
                            &uvc_ctrl_sub_ev_ops);
#endif
                    UVC_ERROR("event handling not supported!");
                    return -USB_EINVAL;

                default:
                    return -USB_EINVAL;
            }
        }

        case VIDIOC_UNSUBSCRIBE_EVENT:
            UVC_PRINTF("VIDIOC_UNSUBSCRIBE_EVENT\n");
#if 0
            return v4l2_event_unsubscribe(&handle->vfh, arg);
#endif
            UVC_ERROR("event handling not supported!");
            return -USB_EINVAL;

        case VIDIOC_DQEVENT:
            UVC_PRINTF("VIDIOC_DQEVENT\n");
#if 0
            return v4l2_event_dequeue(&handle->vfh, arg, /*0x4000*/O_NONBLOCK);
            //file->f_flags & O_NONBLOCK);
#endif
            UVC_ERROR("event handling not supported!");
            return -USB_EINVAL;

        /* Analog video standards make no sense for digital cameras. */
        case VIDIOC_ENUMSTD:
        case VIDIOC_QUERYSTD:
        case VIDIOC_G_STD:
        case VIDIOC_S_STD:
        case VIDIOC_OVERLAY:
        case VIDIOC_ENUMAUDIO:
        case VIDIOC_ENUMAUDOUT:
        case VIDIOC_ENUMOUTPUT:
            UVC_ERROR("Unsupported ioctl 0x%08x\n", cmd);
            return -USB_ENOTTY;

        case UVCIOC_CTRL_MAP:
            UVC_PRINTF("UVCIOC_CTRL_MAP\n");
            return uvc_ioctl_ctrl_map(chain, (struct uvc_xu_control_mapping *)arg);

        case UVCIOC_CTRL_QUERY:
            UVC_PRINTF("UVCIOC_CTRL_QUERY\n");
            return uvc_xu_ctrl_query(chain, (struct uvc_xu_control_query *)arg);

        default:
            UVC_ERROR("Unknown ioctl 0x%08x\n", cmd);
            return -USB_ENOTTY;
    }

    return ret;
}

long uvc_v4l2_ioctl(unsigned int cmd, unsigned long arg)
{
    int ret = 0;
    ret = uvc_v4l2_do_ioctl(cmd, (void *)arg);
    return ret;
}

#ifdef CONFIG_COMPAT
struct uvc_xu_control_mapping32 {
    __u32 id;
    __u8 name[32];
    __u8 entity[16];
    __u8 selector;

    __u8 size;
    __u8 offset;
    __u32 v4l2_type;
    __u32 data_type;

    compat_caddr_t menu_info;
    __u32 menu_count;

    __u32 reserved[4];
};

static int uvc_v4l2_get_xu_mapping(struct uvc_xu_control_mapping *kp,
    const struct uvc_xu_control_mapping32 __user *up)
{
    struct uvc_menu_info __user *umenus;
    struct uvc_menu_info __user *kmenus;
    compat_caddr_t p;

    if (!access_ok(VERIFY_READ, up, sizeof(*up)) ||
        __copy_from_user(kp, up, offsetof(typeof(*up), menu_info)) ||
        __get_user(kp->menu_count, &up->menu_count)) {
        return -USB_EFAULT;
    }

    rtw_memset(kp->reserved, 0, sizeof(kp->reserved));

    if (kp->menu_count == 0) {
        kp->menu_info = NULL;
        return 0;
    }

    if (__get_user(p, &up->menu_info)) {
        return -USB_EFAULT;
    }

    umenus = compat_ptr(p);

    if (!access_ok(VERIFY_READ, umenus, kp->menu_count * sizeof(*umenus))) {
        return -USB_EFAULT;
    }

    kmenus = compat_alloc_user_space(kp->menu_count * sizeof(*kmenus));

    if (kmenus == NULL) {
        return -USB_EFAULT;
    }

    kp->menu_info = kmenus;

    if (copy_in_user(kmenus, umenus, kp->menu_count * sizeof(*umenus))) {
        return -USB_EFAULT;
    }

    return 0;
}


static int uvc_v4l2_put_xu_mapping(const struct uvc_xu_control_mapping *kp,
    struct uvc_xu_control_mapping32 __user *up)
{
    struct uvc_menu_info __user *umenus;
    struct uvc_menu_info __user *kmenus = kp->menu_info;
    compat_caddr_t p;

    if (!access_ok(VERIFY_WRITE, up, sizeof(*up)) ||
        __copy_to_user(up, kp, offsetof(typeof(*up), menu_info)) ||
        __put_user(kp->menu_count, &up->menu_count)) {
        return -USB_EFAULT;
    }

    if (__clear_user(up->reserved, sizeof(up->reserved))) {
        return -USB_EFAULT;
    }

    if (kp->menu_count == 0) {
        return 0;
    }

    if (get_user(p, &up->menu_info)) {
        return -USB_EFAULT;
    }

    umenus = compat_ptr(p);

    if (copy_in_user(umenus, kmenus, kp->menu_count * sizeof(*umenus))) {
        return -USB_EFAULT;
    }

    return 0;
}

struct uvc_xu_control_query32 {
    __u8 unit;
    __u8 selector;
    __u8 query;
    __u16 size;
    compat_caddr_t data;
};

static int uvc_v4l2_get_xu_query(struct uvc_xu_control_query *kp,
    const struct uvc_xu_control_query32 __user *up)
{
    u8 __user *udata;
    u8 __user *kdata;
    compat_caddr_t p;

    if (!access_ok(VERIFY_READ, up, sizeof(*up)) ||
        __copy_from_user(kp, up, offsetof(typeof(*up), data))) {
        return -USB_EFAULT;
    }

    if (kp->size == 0) {
        kp->data = NULL;
        return 0;
    }

    if (__get_user(p, &up->data)) {
        return -USB_EFAULT;
    }

    udata = compat_ptr(p);

    if (!access_ok(VERIFY_READ, udata, kp->size)) {
        return -USB_EFAULT;
    }

    kdata = compat_alloc_user_space(kp->size);

    if (kdata == NULL) {
        return -USB_EFAULT;
    }

    kp->data = kdata;

    if (copy_in_user(kdata, udata, kp->size)) {
        return -USB_EFAULT;
    }

    return 0;
}

static int uvc_v4l2_put_xu_query(const struct uvc_xu_control_query *kp,
    struct uvc_xu_control_query32 __user *up)
{
    u8 __user *udata;
    u8 __user *kdata = kp->data;
    compat_caddr_t p;

    if (!access_ok(VERIFY_WRITE, up, sizeof(*up)) ||
        __copy_to_user(up, kp, offsetof(typeof(*up), data))) {
        return -USB_EFAULT;
    }

    if (kp->size == 0) {
        return 0;
    }

    if (get_user(p, &up->data)) {
        return -USB_EFAULT;
    }

    udata = compat_ptr(p);

    if (!access_ok(VERIFY_READ, udata, kp->size)) {
        return -USB_EFAULT;
    }

    if (copy_in_user(udata, kdata, kp->size)) {
        return -USB_EFAULT;
    }

    return 0;
}

#define UVCIOC_CTRL_MAP32   _IOWR('u', 0x20, struct uvc_xu_control_mapping32)
#define UVCIOC_CTRL_QUERY32 _IOWR('u', 0x21, struct uvc_xu_control_query32)


static long uvc_v4l2_compat_ioctl32(unsigned int cmd, unsigned long arg)
{
    union {
        struct uvc_xu_control_mapping xmap;
        struct uvc_xu_control_query xqry;
    } karg;
    void __user *up = compat_ptr(arg);
    mm_segment_t old_fs;
    long ret;

    switch (cmd) {
        case UVCIOC_CTRL_MAP32:
            cmd = UVCIOC_CTRL_MAP;
            ret = uvc_v4l2_get_xu_mapping(&karg.xmap, up);
            break;

        case UVCIOC_CTRL_QUERY32:
            cmd = UVCIOC_CTRL_QUERY;
            ret = uvc_v4l2_get_xu_query(&karg.xqry, up);
            break;

        default:
            return -ENOIOCTLCMD;
    }

    old_fs = get_fs();
    set_fs(KERNEL_DS);
    ret = uvc_v4l2_ioctl(cmd, (unsigned long)&karg);
    set_fs(old_fs);

    if (ret < 0) {
        return ret;
    }

    switch (cmd) {
        case UVCIOC_CTRL_MAP:
            ret = uvc_v4l2_put_xu_mapping(&karg.xmap, up);
            break;

        case UVCIOC_CTRL_QUERY:
            ret = uvc_v4l2_put_xu_query(&karg.xqry, up);
            break;
    }

    return ret;
}
#endif

const struct v4l2_file_operations uvc_fops = {
#ifdef CONFIG_COMPAT
    .compat_ioctl32 = uvc_v4l2_compat_ioctl32,
#endif
    .open       = uvc_v4l2_open,
    .release    = uvc_v4l2_release,
    .unlocked_ioctl = uvc_v4l2_ioctl,
};

#endif // CONFIG_USBH_UVC
