/*
 *      uvc_video.c  --  USB Video Class driver - Video handling
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

#define UVC_URB_SIZE /*12240*/6120

/*set uvc thrughput test switch*/
int uvc_thru_en = 0;
int uvc_pure_thru_en = 0;
static int ok_cnt = 0;
static int err_cnt = 0;
static int async_cnt = 0;
static int frame_size = 0;
//static int show_dur = 30000;
//static u32 show_tick1, show_tick2;

/* ------------------------------------------------------------------------
 * UVC Controls
 */

//  query = 0x87,unit = 0,intfnum = 0,cs = UVC_VS_PROBE_CONTROL, dtat, size = 26, timeout = 5000
static int __uvc_query_ctrl(struct uvc_device *dev, __u8 query, __u8 unit,
    __u8 intfnum, __u8 cs, void *data, __u16 size,
    int timeout)
{
    __u8 type = USB_TYPE_CLASS | USB_RECIP_INTERFACE;
    unsigned int pipe;
    pipe = (query & 0x80) ? usb_rcvctrlpipe(dev->udev, 0)
        : usb_sndctrlpipe(dev->udev, 0);
    type |= (query & 0x80) ? USB_DIR_IN : USB_DIR_OUT;
    return usb_control_msg(dev->udev, pipe, query, type, cs << 8,
            unit << 8 | intfnum, data, size, timeout);
}

/*for debug usage*/

static const char *uvc_query_name(__u8 query)
{
    switch (query) {
        case UVC_SET_CUR:
            return "SET_CUR";

        case UVC_GET_CUR:
            return "GET_CUR";

        case UVC_GET_MIN:
            return "GET_MIN";

        case UVC_GET_MAX:
            return "GET_MAX";

        case UVC_GET_RES:
            return "GET_RES";

        case UVC_GET_LEN:
            return "GET_LEN";

        case UVC_GET_INFO:
            return "GET_INFO";

        case UVC_GET_DEF:
            return "GET_DEF";

        default:
            return "<invalid>";
    }
}


int uvc_query_ctrl(struct uvc_device *dev, __u8 query, __u8 unit,
    __u8 intfnum, __u8 cs, void *data, __u16 size)
{
    int ret;
    
    ret = __uvc_query_ctrl(dev, query, unit, intfnum, cs, data, size,
            UVC_CTRL_CONTROL_TIMEOUT);
    if (ret != size) {
        UVC_ERROR("Failed to query (%s) UVC control %d on unit %d: %d (exp. %d).\n", uvc_query_name(query), cs, unit, ret,
            size);
        return -USB_EIO;
    }

    return 0;
}

static void uvc_fixup_video_ctrl(struct uvc_streaming *stream,
    struct uvc_streaming_control *ctrl)
{
    FUN_ENTER;
    struct uvc_format *format = NULL;
    struct uvc_frame *frame = NULL;
    unsigned int i;

    for (i = 0; i < stream->nformats; ++i) {
        if (stream->format[i].index == ctrl->bFormatIndex) {
            format = &stream->format[i];
            break;
        }
    }

    if (format == NULL) {
        return;
    }

    for (i = 0; i < format->nframes; ++i) {
        if (format->frame[i].bFrameIndex == ctrl->bFrameIndex) {
            frame = &format->frame[i];
            break;
        }
    }

    if (frame == NULL) {
        return;
    }

    if (!(format->flags & UVC_FMT_FLAG_COMPRESSED) ||
        (ctrl->dwMaxVideoFrameSize == 0 &&
            stream->dev->uvc_version < 0x0110))
        ctrl->dwMaxVideoFrameSize =
            frame->dwMaxVideoFrameBufferSize;

    FUN_EXIT;
}

static int uvc_get_video_ctrl(struct uvc_streaming *stream,
    struct uvc_streaming_control *ctrl, int probe, __u8 query)
{
    __u8 *data;
    __u16 size;
    int ret;
    
    //size = 26,  stream->dev->uvc_version = 0x0100
    size = stream->dev->uvc_version >= 0x0110 ? 34 : 26;
    
#if 0
    if ((stream->dev->quirks & UVC_QUIRK_PROBE_DEF) && query == UVC_GET_DEF) {
        return -USB_EIO;
    }
#endif

    data = rtw_malloc(size);
    if (data == NULL) {
        return -USB_ENOMEM;
    }

    //  ,0x87, 0, 0, UVC_VS_PROBE_CONTROL, data, 26, 5000
    //      ,0x81, 0, 0, UVC_VS_PROBE_CONTROL, data, 26, 5000
    ret = __uvc_query_ctrl(stream->dev, query, 0, stream->intfnum,
            probe ? UVC_VS_PROBE_CONTROL : UVC_VS_COMMIT_CONTROL, data,
            size, uvc_timeout_param);

    if ((query == UVC_GET_MIN || query == UVC_GET_MAX) && ret == 2) {
        /* Some cameras, mostly based on Bison Electronics chipsets,
         * answer a GET_MIN or GET_MAX request with the wCompQuality
         * field only.
         */
        UVC_ERROR("UVC non compliance - GET_MIN/MAX(PROBE) incorrectly supported. Enabling workaround.\n");
        rtw_memset(ctrl, 0, sizeof * ctrl);
        ctrl->wCompQuality = le16_to_cpup((__le16 *)data);
        ret = 0;
        goto out;
    } else if (query == UVC_GET_DEF && probe == 1 && ret != size) {
        /* Many cameras don't support the GET_DEF request on their
         * video probe control. Warn once and return, the caller will
         * fall back to GET_CUR.
         */
        UVC_ERROR("UVC non compliance - GET_DEF(PROBE) not supported. ");
        ret = -USB_EIO;
        goto out;
    } else if (ret != size) {
        UVC_ERROR("Fail to query (%d) UVC %s control : %d (exp. %d).\n", query, probe ? "probe" : "commit", ret, size);
        ret = -USB_EIO;
        goto out;
    }

    // reference the UVC spec 4.3.1.1 via p134
    ctrl->bmHint = le16_to_cpup((__le16 *)&data[0]);
    ctrl->bFormatIndex = data[2];
    ctrl->bFrameIndex = data[3];
    ctrl->dwFrameInterval = le32_to_cpup((__le32 *)&data[4]);
    ctrl->wKeyFrameRate = le16_to_cpup((__le16 *)&data[8]);
    ctrl->wPFrameRate = le16_to_cpup((__le16 *)&data[10]);
    ctrl->wCompQuality = le16_to_cpup((__le16 *)&data[12]);
    ctrl->wCompWindowSize = le16_to_cpup((__le16 *)&data[14]);
    ctrl->wDelay = le16_to_cpup((__le16 *)&data[16]);
    ctrl->dwMaxVideoFrameSize = get_unaligned_le32(&data[18]);
    ctrl->dwMaxPayloadTransferSize = get_unaligned_le32(&data[22]);

    if (size == 34) {
        ctrl->dwClockFrequency = get_unaligned_le32(&data[26]);
        ctrl->bmFramingInfo = data[30];
        ctrl->bPreferedVersion = data[31];
        ctrl->bMinVersion = data[32];
        ctrl->bMaxVersion = data[33];
    } else {
        // into this , size = 26
        // dev->clock_frequency = 0x01C9C380(3000 0000)
        ctrl->dwClockFrequency = stream->dev->clock_frequency;
        ctrl->bmFramingInfo = 0;
        ctrl->bPreferedVersion = 0;
        ctrl->bMinVersion = 0;
        ctrl->bMaxVersion = 0;
    }

    /* Some broken devices return null or wrong dwMaxVideoFrameSize and
     * dwMaxPayloadTransferSize fields. Try to get the value from the
     * format and frame descriptors.
     */
    uvc_fixup_video_ctrl(stream, ctrl);
    ret = 0;
out:
    rtw_free(data);
    return ret;
}

static int uvc_set_video_ctrl(struct uvc_streaming *stream,
    struct uvc_streaming_control *ctrl, int probe)
{
    __u8 *data;
    __u16 size;
    int ret;
    
    size = stream->dev->uvc_version >= 0x0110 ? 34 : 26;
    data = rtw_zmalloc(size);

    if (data == NULL) {
        return -USB_ENOMEM;
    }

    *(__le16 *)&data[0] = cpu_to_le16(ctrl->bmHint);
    data[2] = ctrl->bFormatIndex;
    data[3] = ctrl->bFrameIndex;
    *(__le32 *)&data[4] = cpu_to_le32(ctrl->dwFrameInterval);
    *(__le16 *)&data[8] = cpu_to_le16(ctrl->wKeyFrameRate);
    *(__le16 *)&data[10] = cpu_to_le16(ctrl->wPFrameRate);
    *(__le16 *)&data[12] = cpu_to_le16(ctrl->wCompQuality);
    *(__le16 *)&data[14] = cpu_to_le16(ctrl->wCompWindowSize);
    *(__le16 *)&data[16] = cpu_to_le16(ctrl->wDelay);
    put_unaligned_le32(ctrl->dwMaxVideoFrameSize, &data[18]);
    put_unaligned_le32(ctrl->dwMaxPayloadTransferSize, &data[22]);

    if (size == 34) {
        put_unaligned_le32(ctrl->dwClockFrequency, &data[26]);
        data[30] = ctrl->bmFramingInfo;
        data[31] = ctrl->bPreferedVersion;
        data[32] = ctrl->bMinVersion;
        data[33] = ctrl->bMaxVersion;
    }

    ret = __uvc_query_ctrl(stream->dev, UVC_SET_CUR, 0, stream->intfnum,
            probe ? UVC_VS_PROBE_CONTROL : UVC_VS_COMMIT_CONTROL, data,
            size, uvc_timeout_param);
    UVC_PRINTF("received uvc ctrl buffer length:%d(%d)", ret, probe);

    if (ret != size) {
        UVC_ERROR("Failed to set UVC %s control : %d (exp. %d).\n", probe ? "probe" : "commit", ret, size);
        ret = -USB_EIO;
    }

    rtw_free(data);
    return ret;
}
    
extern u16 min_compression_ratio, max_compression_ratio;

int uvc_probe_video(struct uvc_streaming *stream,
    struct uvc_streaming_control *probe)
{
    struct uvc_streaming_control probe_min, probe_max;
    __u16 bandwidth;
    unsigned int i;
    int ret;
    
    /* Perform probing. The device should adjust the requested values
     * according to its capabilities. However, some devices, namely the
     * first generation UVC Logitech webcams, don't implement the Video
     * Probe control properly, and just return the needed bandwidth. For
     * that reason, if the needed bandwidth exceeds the maximum available
     * bandwidth, try to lower the quality.
     */
    ret = uvc_set_video_ctrl(stream, probe, 1);
    if (ret < 0) {
        goto done;
    }

    /* Get the minimum and maximum values for compression settings. */
    //  if (!(stream->dev->quirks & UVC_QUIRK_PROBE_MINMAX)) {
    ret = uvc_get_video_ctrl(stream, &probe_min, 1, UVC_GET_MIN);
    if (ret < 0) {
        goto done;
    }

    ret = uvc_get_video_ctrl(stream, &probe_max, 1, UVC_GET_MAX);
    if (ret < 0) {
        goto done;
    }

    //add by Ian -- save min&max compression ratio for application layer use
    min_compression_ratio = probe_min.wCompQuality;
    max_compression_ratio = probe_max.wCompQuality;
    //  }

    for (i = 0; i < 2; ++i) {
        ret = uvc_set_video_ctrl(stream, probe, 1);
        if (ret < 0) {
            goto done;
        }

        ret = uvc_get_video_ctrl(stream, probe, 1, UVC_GET_CUR);
        if (ret < 0) {
            goto done;
        }

        if (stream->intf->num_altsetting == 1) {
            break;
        }

        bandwidth = probe->dwMaxPayloadTransferSize;
        if (bandwidth <= stream->maxpsize) {
            break;
        }

#if 0
        if (stream->dev->quirks & UVC_QUIRK_PROBE_MINMAX) {
            ret = -USB_ENOSPC;
            goto done;
        }
#endif

        /* TODO: negotiate compression parameters */
        probe->wKeyFrameRate = probe_min.wKeyFrameRate;
        probe->wPFrameRate = probe_min.wPFrameRate;
        probe->wCompQuality = probe_max.wCompQuality;
        probe->wCompWindowSize = probe_min.wCompWindowSize;
    }

done:
    return ret;
}

int uvc_commit_video(struct uvc_streaming *stream,
    struct uvc_streaming_control *probe)
{
    return uvc_set_video_ctrl(stream, probe, 0);
}

/* ------------------------------------------------------------------------
 * Video codecs
 */

/* Video payload decoding is handled by uvc_video_decode_start(),
 * uvc_video_decode_data() and uvc_video_decode_end().
 *
 * uvc_video_decode_start is called with URB data at the start of a bulk or
 * isochronous payload. It processes header data and returns the header size
 * in bytes if successful. If an error occurs, it returns a negative error
 * code. The following error codes have special meanings.
 *
 * - USB_EAGAIN informs the caller that the current video buffer should be marked
 *   as done, and that the function should be called again with the same data
 *   and a new video buffer. This is used when end of frame conditions can be
 *   reliably detected at the beginning of the next frame only.
 *
 * If an error other than -USB_EAGAIN is returned, the caller will drop the current
 * payload. No call to uvc_video_decode_data and uvc_video_decode_end will be
 * made until the next payload. -USB_ENODATA can be used to drop the current
 * payload if no other error code is appropriate.
 *
 * uvc_video_decode_data is called for every URB with URB data. It copies the
 * data to the video buffer.
 *
 * uvc_video_decode_end is called with header data at the end of a bulk or
 * isochronous payload. It performs any additional header data processing and
 * returns 0 or a negative error code if an error occurred. As header data have
 * already been processed by uvc_video_decode_start, this functions isn't
 * required to perform sanity checks a second time.
 *
 * For isochronous transfers where a payload is always transferred in a single
 * URB, the three functions will be called in a row.
 *
 * To let the decoder process header data and update its internal state even
 * when no video buffer is available, uvc_video_decode_start must be prepared
 * to be called with a NULL buf parameter. uvc_video_decode_data and
 * uvc_video_decode_end will never be called with a NULL buffer.
 */
static int uvc_video_decode_start(struct uvc_streaming *stream,
    struct uvc_buffer *buf, const __u8 *data, int len)
{
    __u8 fid;

    /* Sanity checks:
     * - packet must be at least 2 bytes long
     * - bHeaderLength value must be at least 2 bytes (see above)
     * - bHeaderLength value can't be larger than the packet size.
     */
    if (len < 2 || data[0] < 2 || data[0] > len) {
        stream->stats.frame.nb_invalid++;
        UVC_PRINTF("data empty!");
        return -USB_EINVAL;
    }

    UVC_PRINTF("data[0]:%d data[1]: %d\n", data[0], data[1]);
    fid = data[1] & UVC_STREAM_FID;

    //UVC_PRINTF("Initialized fid: %d Initialized stream.last fid: %d", fid,  stream->last_fid);
    /* Increase the sequence number regardless of any buffer states, so
     * that discontinuous sequence numbers always indicate lost frames.
     */
    if (stream->last_fid != fid) {
        stream->sequence++;
        //      if (stream->sequence)
        //          uvc_video_stats_update(stream);
    }

    //printf("\n\rstream last fid:%d",stream->last_fid);
    //  uvc_video_clock_decode(stream, buf, data, len);
    //  uvc_video_stats_decode(stream, data, len);
    /* Store the payload FID bit and return immediately when the buffer is
     * NULL.
     */
    if (buf == NULL) {
        UVC_PRINTF("uvc buffer is NULL\n");
        //if((len - data[0])> 0)
        //    nobuf_cnt++;
        stream->last_fid = fid;
        return -USB_ENODATA;
    }

    /* Mark the buffer as bad if the error bit is set. */
    if (data[1] & UVC_STREAM_ERR) {
        UVC_PRINTF("Marking buffer as bad (error bit set).\n");
        //will get incorrect length error code or hard fault
        // uvc_get_error_code_ctrl(stream, UVC_GET_CUR);
        //buf->error = 1;
    }

    /* Synchronize to the input stream by waiting for the FID bit to be
     * toggled when the the buffer state is not UVC_BUF_STATE_ACTIVE.
     * stream->last_fid is initialized to -1, so the first isochronous
     * frame will always be in sync.
     *
     * If the device doesn't toggle the FID bit, invert stream->last_fid
     * when the EOF bit is set to force synchronisation on the next packet.
     */
    //  UVC_PRINTF("\n\ruvc_buf state: %d", buf->state);
    if (buf->state != UVC_BUF_STATE_ACTIVE) {
        //struct timespec ts;
        if (fid == stream->last_fid) {
            UVC_PRINTF("Dropping payload (out of sync).\n");

            if (data[1] & UVC_STREAM_EOF) {
                async_cnt++;
            }

#if 0
            if ((stream->dev->quirks & UVC_QUIRK_STREAM_NO_FID) &&
                (data[1] & UVC_STREAM_EOF)) {
                stream->last_fid ^= UVC_STREAM_FID;
            }
#endif
            return -USB_ENODATA;
        }

        buf->buf.v4l2_buf.sequence = stream->sequence;
        buf->buf.v4l2_buf.timestamp = rtw_get_current_time();
        /* TODO: Handle PTS and SCR. */
        buf->state = UVC_BUF_STATE_ACTIVE;
        //      UVC_PRINTF("v4l2_buf.sequence: %08d, v4l2_buf.timestamp: %08d, uvc_buf state: %d", buf->buf.v4l2_buf.sequence, buf->buf.v4l2_buf.timestamp, buf->state);
    }

    /* Mark the buffer as done if we're at the beginning of a new frame.
     * End of frame detection is better implemented by checking the EOF
     * bit (FID bit toggling is delayed by one frame compared to the EOF
     * bit), but some devices don't set the bit at end of frame (and the
     * last payload can be lost anyway). We thus must check if the FID has
     * been toggled.
     *
     * stream->last_fid is initialized to -1, so the first isochronous
     * frame will never trigger an end of frame detection.
     *
     * Empty buffers (bytesused == 0) don't trigger end of frame detection
     * as it doesn't make sense to return an empty buffer. This also
     * avoids detecting end of frame conditions at FID toggling if the
     * previous payload had the EOF bit set.
     */
    //  UVC_PRINTF("fid: %d, stream->last_fid: %d, buf->bytesused:%d", fid, stream->last_fid, buf->bytesused);
    if (fid != stream->last_fid && buf->bytesused != 0) {
        UVC_PRINTF("Frame complete (FID bit toggled).\n");
        buf->state = UVC_BUF_STATE_READY;

        if (buf->error) {
            err_cnt++;
        } else {
            ok_cnt++;
            frame_size += buf->bytesused / 1024;
        }

        return -USB_EAGAIN;
    }

    stream->last_fid = fid;
    return data[0];
}

//add by Ian - dma for memory copy
#if UVC_USE_DMA
#include "dma_api.h"
gdma_t uvc_dma;
//static atomic_t uvc_dma_done;
static ATOMIC_T uvc_dma_done;
void uvc_dma_done_hdl(uint32_t id)
{
    ATOMIC_SET(&uvc_dma_done, 1);
}
#endif
static void uvc_video_decode_data(struct uvc_streaming *stream,
    struct uvc_buffer *buf, const __u8 *data, int len)
{
    int maxlen, nbytes;
    u8 *mem;

    UNUSED(stream);

    if (len <= 0) {
        UVC_PRINTF("empty data");
        return;
    }

    /* Copy the video data to the buffer. */
    maxlen = buf->length - buf->bytesused;
    mem = (u8 *) buf->mem + buf->bytesused;
    nbytes = min(len, maxlen);
    
    //add by Ian
#if UVC_USE_DMA
    if (nbytes > stream->ctrl.dwMaxPayloadTransferSize / 2 \
        && ((u32)mem % 4 == 0) && ((u32)data % 4 == 0) && (nbytes % 4 == 0)) {
        //printf("\n\rd");
        //atomic_set(&uvc_dma_done, 0);
        ATOMIC_SET(&uvc_dma_done, 0);
        dma_memcpy(&uvc_dma, (void *)mem, (void *)data, nbytes);
    } else {
        //atomic_set(&uvc_dma_done, 1);
        ATOMIC_SET(&uvc_dma_done, 1);
        rtw_memcpy((void *)mem, (void *)data, nbytes);
    }

    while (ATOMIC_READ(&uvc_dma_done) != 1) {
        rtw_msleep_os(1);
    }

#else
    rtw_memcpy((void *)mem, (void *)data, nbytes);
#endif

    buf->bytesused += nbytes;
    //  UVC_PRINTF("buf->bytesused: %d, memory address:%08x", buf->bytesused, mem);

    /* Complete the current frame if the buffer size was exceeded. */
    if (len > maxlen) {
        UVC_ERROR("Frame complete (overflow).\n");
        buf->state = UVC_BUF_STATE_READY;
        buf->error = 1;
    }
}

static void uvc_video_decode_end(struct uvc_streaming *stream,
    struct uvc_buffer *buf, const __u8 *data, int len)
{
    UNUSED(stream);
    
    /* Mark the buffer as done if the EOF marker is set. */
    if ((data[1] & UVC_STREAM_EOF) && buf->bytesused != 0) {
        UVC_PRINTF("frame complete(EOF found).(%dBYTE)", buf->bytesused);

        if (data[0] == len) {
            UVC_PRINTF("EOF in empty payload.\n");
        }

        buf->state = UVC_BUF_STATE_READY;

        //      UVC_PRINTF("buf->state changed to ready");
        if (data[1] & UVC_STREAM_ERR) {
            buf->error = 1;
        }

#if 0
        if (stream->dev->quirks & UVC_QUIRK_STREAM_NO_FID) {
            stream->last_fid ^= UVC_STREAM_FID;
        }
#endif

        if (buf->error) {
            err_cnt++;
        } else {
            ok_cnt++;
            frame_size += buf->bytesused / 1024;
        }
    }
}

/* Video payload encoding is handled by uvc_video_encode_header() and
 * uvc_video_encode_data(). Only bulk transfers are currently supported.
 *
 * uvc_video_encode_header is called at the start of a payload. It adds header
 * data to the transfer buffer and returns the header size. As the only known
 * UVC output device transfers a whole frame in a single payload, the EOF bit
 * is always set in the header.
 *
 * uvc_video_encode_data is called for every URB and copies the data from the
 * video buffer to the transfer buffer.
 */
static int uvc_video_encode_header(struct uvc_streaming *stream,
    struct uvc_buffer *buf, __u8 *data, int len)
{
    UNUSED(buf);
    UNUSED(len);
    
    data[0] = 2;    /* Header length */
    data[1] = UVC_STREAM_EOH | UVC_STREAM_EOF
        | (stream->last_fid & UVC_STREAM_FID);
    return 2;
}

static int uvc_video_encode_data(struct uvc_streaming *stream,
    struct uvc_buffer *buf, __u8 *data, int len)
{
    struct uvc_video_queue *queue = &stream->queue;
    unsigned int nbytes;
    u8 *mem;
    /* Copy video data to the URB buffer. */
    mem = (u8 *)buf->mem + queue->buf_used;
    nbytes = min((unsigned int)len, buf->bytesused - queue->buf_used);
    nbytes = min(stream->bulk.max_payload_size - stream->bulk.payload_size,
            nbytes);
    rtw_memcpy(data, mem, nbytes);
    queue->buf_used += nbytes;
    return nbytes;
}

#if 0
/* * Set error flag for incomplete buffer. */
static void uvc_video_validate_buffer(const struct uvc_streaming *stream, struct uvc_buffer *buf)
{
    int cnt = 0;

    if (buf->length != buf->bytesused && !(stream->cur_format->flags & UVC_FMT_FLAG_COMPRESSED)) {
        buf->error = 1;
    }

    if (cnt > 1 || cnt == 0) {
        UVC_ERROR("Invalid UVC buffer\n");
        buf->error = 1;
    }
}
#endif

/*
 * Completion handler for video URBs.
 */
static void uvc_video_decode_isoc(struct urb *urb, struct uvc_streaming *stream,
    struct uvc_buffer *buf)
{
    u8 *mem;
    int ret, i;

    //  UVC_PRINTF("urb transfer buffer address:%08x", urb->transfer_buffer);

    for (i = 0; i < urb->number_of_packets; ++i) {
        if (urb->iso_frame_desc[i].status < 0) {
            UVC_WARN("iso frame error: %d\n", urb->iso_frame_desc[i].status);

            /* Mark the buffer as faulty. */
            if (buf != NULL) {
                buf->error = 1;
            }

            continue;
        }

        UVC_PRINTF("iso frame %d actual length:%d", i, urb->iso_frame_desc[i].actual_length);
        /* Decode the payload header. */
        mem = (u8 *)urb->transfer_buffer + urb->iso_frame_desc[i].offset;

        do {
            ret = uvc_video_decode_start(stream, buf, mem,
                    urb->iso_frame_desc[i].actual_length);
            if (ret == -USB_EAGAIN) {
                //uvc_video_validate_buffer(stream, buf);
                buf = uvc_queue_next_buffer(&stream->queue, buf);
            }
        } while (ret == -USB_EAGAIN);

        if (ret < 0) {
            continue;
        }

        /* Decode the payload data. */
        uvc_video_decode_data(stream, buf, mem + ret /*mem*/,
            urb->iso_frame_desc[i].actual_length - ret /*urb->iso_frame_desc[i].actual_length*/);
        /* Process the header again. */
        uvc_video_decode_end(stream, buf, mem,
            urb->iso_frame_desc[i].actual_length);

        if (buf->state == UVC_BUF_STATE_READY) {
#if 0
            if (buf->length != buf->bytesused && !(stream->cur_format->flags & UVC_FMT_FLAG_COMPRESSED)) {
                buf->error = 1;
                UVC_ERROR("\n\rset buf error to 1. none compressed frame imcomplete!\n");
                UVC_ERROR("\n\rcurrent format flag:%d", stream->cur_format->flags);
            }
#endif
            //fill vb2_buffer.v4l2_planes for single planar video
            buf->buf.v4l2_planes[0].bytesused = buf->bytesused;
            buf->buf.v4l2_planes[0].m.mem_offset = (__u32)buf->mem;
            //uvc_video_validate_buffer(stream, buf);
            buf = uvc_queue_next_buffer(&stream->queue, buf);
        }
    }
}

static void uvc_video_decode_bulk(struct urb *urb, struct uvc_streaming *stream,
    struct uvc_buffer *buf)
{
    u8 *mem;
    int len, ret;

    /*
     * Ignore ZLPs if they're not part of a frame, otherwise process them
     * to trigger the end of payload detection.
     */
    if (urb->actual_length == 0 && stream->bulk.header_size == 0) {
        return;
    }

    mem = urb->transfer_buffer;
    len = urb->actual_length;
    stream->bulk.payload_size += len;

    /* If the URB is the first of its payload, decode and save the
     * header.
     */
    if (stream->bulk.header_size == 0 && !stream->bulk.skip_payload) {
        do {
            ret = uvc_video_decode_start(stream, buf, mem, len);

            if (ret == -USB_EAGAIN)
                buf = uvc_queue_next_buffer(&stream->queue,
                        buf);
        } while (ret == -USB_EAGAIN);

        /* If an error occurred skip the rest of the payload. */
        if (ret < 0 || buf == NULL) {
            stream->bulk.skip_payload = 1;
        } else {
            rtw_memcpy(stream->bulk.header, mem, ret);
            stream->bulk.header_size = ret;
            mem += ret;
            len -= ret;
        }
    }

    /* The buffer queue might have been cancelled while a bulk transfer
     * was in progress, so we can reach here with buf equal to NULL. Make
     * sure buf is never dereferenced if NULL.
     */

    /* Process video data. */
    if (!stream->bulk.skip_payload && buf != NULL) {
        uvc_video_decode_data(stream, buf, mem, len);
    }

    /* Detect the payload end by a URB smaller than the maximum size (or
     * a payload size equal to the maximum) and process the header again.
     */
    if (urb->actual_length < urb->transfer_buffer_length ||
        stream->bulk.payload_size >= stream->bulk.max_payload_size) {
        if (!stream->bulk.skip_payload && buf != NULL) {
            uvc_video_decode_end(stream, buf, stream->bulk.header,
                stream->bulk.payload_size);

            if (buf->state == UVC_BUF_STATE_READY)
                //fill vb2_buffer.v4l2_planes for single planar video
            {
                buf->buf.v4l2_planes[0].bytesused = buf->bytesused;
            }

            buf->buf.v4l2_planes[0].m.mem_offset = (__u32)buf->mem;
            buf = uvc_queue_next_buffer(&stream->queue,
                    buf);
        }

        stream->bulk.header_size = 0;
        stream->bulk.skip_payload = 0;
        stream->bulk.payload_size = 0;
    }
}

static void uvc_video_encode_bulk(struct urb *urb, struct uvc_streaming *stream,
    struct uvc_buffer *buf)
{
    u8 *mem = urb->transfer_buffer;
    int len = stream->urb_size, ret;

    if (buf == NULL) {
        urb->transfer_buffer_length = 0;
        return;
    }

    /* If the URB is the first of its payload, add the header. */
    if (stream->bulk.header_size == 0) {
        ret = uvc_video_encode_header(stream, buf, mem, len);
        stream->bulk.header_size = ret;
        stream->bulk.payload_size += ret;
        mem += ret;
        len -= ret;
    }

    /* Process video data. */
    ret = uvc_video_encode_data(stream, buf, mem, len);
    stream->bulk.payload_size += ret;
    len -= ret;

    if (buf->bytesused == stream->queue.buf_used ||
        stream->bulk.payload_size == stream->bulk.max_payload_size) {
        if (buf->bytesused == stream->queue.buf_used) {
            stream->queue.buf_used = 0;
            buf->state = UVC_BUF_STATE_READY;
            buf->buf.v4l2_buf.sequence = ++stream->sequence;
            uvc_queue_next_buffer(&stream->queue, buf);
            stream->last_fid ^= UVC_STREAM_FID;
        }

        stream->bulk.header_size = 0;
        stream->bulk.payload_size = 0;
    }

    urb->transfer_buffer_length = stream->urb_size - len;
}

//add by Ian -- declare uvc complete handle resource
static u8 uvc_complete_on = 0;
static _xqueue urb_wait_queue;
static _xqueue urb_giveback_queue;
static int urb_wait_threshold = (UVC_URBS);
static int urb_giveback_threshold = (UVC_URBS);
static u32 wait_send_timeout = /*portMAX_DELAY*/0; //in os tick
static u32 wait_recv_timeout = 1; //in os tick
static u32 giveback_send_timeout = 0; //in os tick
static u32 giveback_recv_timeout = 0; //in os tick
static u8 urb_queue_flag = 0;
static void uvc_complete_handle(void *param)
{
    struct urb *tmp_urb;

    UNUSED(param);
    
    uvc_complete_on = 1;
    while (uvc_complete_on) {
	if(!usbh_get_connect_status()) {
		rtw_thread_exit();
		return;
	}
        if (rtw_pop_from_xqueue(&urb_wait_queue, (void *)&tmp_urb, wait_recv_timeout) < 0) {
            //rtw_mdelay_os(1);
            continue;
        }

        //printf("\n\rtmp urb addr:0x%x", tmp_urb);
        struct uvc_streaming *stream = (struct uvc_streaming *) tmp_urb->context;
        struct uvc_video_queue *queue = &stream->queue;
        struct uvc_buffer *buf = NULL;
        
        //  UVC_PRINTF("submit urb status = %d.\n", urb->status);

        if (tmp_urb->status != 0) {
            UVC_ERROR("Non-zero status (%d) in video completion handler.\n", tmp_urb->status);

            switch (tmp_urb->status) {
                case -USB_ENOENT:       /* usb_kill_urb() called. */
                    if (stream->frozen) {
                        UVC_ERROR("stream frozen\n");
                        rtw_thread_exit();
                        return;
                    }

                case -USB_ECONNRESET:   /* usb_unlink_urb() called. */
                case -USB_ESHUTDOWN:    /* The endpoint is being disabled. */
                    UVC_ERROR("endpoint being disabled\n");
                    uvc_queue_cancel(queue, tmp_urb->status == -USB_ESHUTDOWN);
                    rtw_thread_exit();
                    return;
            }
        }

        Mutex_Lock(&queue->irqlock);

        if (!list_empty(&queue->irqqueue)) {
            buf = list_first_entry(&queue->irqqueue, struct uvc_buffer, queue);
        }

        Mutex_Unlock(&queue->irqlock);

        if (!uvc_pure_thru_en) {
            stream->decode(tmp_urb, stream, buf);
        }

        //give urb back to usb otg task
        while (rtw_push_to_xqueue(&urb_giveback_queue, (void *)&tmp_urb, giveback_send_timeout) < 0) {
            //to do... unlikely happen
            UVC_ERROR("Fail to push urb\n");
        }

        //edit by Ian -- show uvc statistics
#if 0

        if (show_tick1 == 0 && show_tick2 == 0) {
            //printf("\n\r++");
            show_tick1 = rtw_get_current_time();
        }

        show_tick2 = rtw_get_current_time();

        if (show_tick2 < show_tick1) {
            //printf("\n\r--");
            err_cnt = 0;
            ok_cnt = 0;
            async_cnt = 0;
            //nobuf_cnt = 0;
            frame_size = 0;
            show_tick1 = show_tick2 = 0;
        }

        if ((show_tick2 - show_tick1) > show_dur) {
            printf("\n\r%do-%de-%da %ds-%dMB", ok_cnt, err_cnt, async_cnt, (show_tick2 - show_tick1) / 1000, frame_size / 1024);
            err_cnt = 0;
            ok_cnt = 0;
            async_cnt = 0;
            //nobuf_cnt = 0;
            frame_size = 0;
            show_tick1 = show_tick2 = 0;
        }

#endif
    }

    rtw_thread_exit();
}

//add by Ian -- release real uvc complete handle resource here
void uvc_complete_handle_deinit(void)
{
    uvc_complete_on = 0;
    urb_queue_flag = 0;
    rtw_deinit_xqueue(&urb_wait_queue);
    rtw_deinit_xqueue(&urb_giveback_queue);
}

//add by Ian -- create real uvc complete handle task here
int uvc_complete_handle_init(void)
{
    struct task_struct task;
    
    if (rtw_init_xqueue(&urb_wait_queue, (const char *)("urb_wait_queue"), sizeof(struct urb *), urb_wait_threshold) < 0) {
        return -1;
    }

    if (rtw_init_xqueue(&urb_giveback_queue, (const char *)("urb_giveback_queue"), sizeof(struct urb *), urb_giveback_threshold) < 0) {
        rtw_deinit_xqueue(&urb_wait_queue);
        return -1;
    }

    urb_queue_flag = 1;
	
    if (rtw_create_task(&task, "uvc_complete_handle", 256, tskIDLE_PRIORITY + 5, uvc_complete_handle, NULL) != pdPASS) {
        UVC_ERROR("Fail to create uvc complete handle thread\n");
        return -1;
    }

    return 0;
}

static void uvc_video_complete(struct urb *urb)
{
    int ret;
    struct urb *tmp_urb;
    tmp_urb = urb;

	if(!usbh_get_connect_status())
		return;
	
	if(!urb_queue_flag)
		return;

    if (rtw_push_to_xqueue(&urb_wait_queue, (void *)&tmp_urb, wait_send_timeout) < 0) {
        //unlikely happen - should we resubmit urb here?
        UVC_ERROR("Fail to push urb");
        return;
    }

    tmp_urb = NULL;

    if (rtw_pop_from_xqueue(&urb_giveback_queue, (void *)&tmp_urb, giveback_recv_timeout) == 0) {
        if (tmp_urb != NULL) {
            rtw_enter_critical(NULL, NULL);

            if ((ret = usb_submit_urb(tmp_urb)) < 0) {
                UVC_ERROR("Failed to resubmit video URB (%d).\n", ret);
            }

            rtw_exit_critical(NULL, NULL);
        } else {
            UVC_ERROR("Fail to pop urb");
        }
    }

    rtw_mdelay_os(1);
}

/*
 * Free transfer buffers.
 */
static void uvc_free_urb_buffers(struct uvc_streaming *stream)
{
    unsigned int i;

    for (i = 0; i < UVC_URBS; ++i) {
        if (stream->urb_buffer[i]) {
            rtw_free(stream->urb_buffer[i]);
            stream->urb_buffer[i] = NULL;
        }
    }

    stream->urb_size = 0;
}

/*
 * Allocate transfer buffers. This function can be called with buffers
 * already allocated when resuming from suspend, in which case it will
 * return without touching the buffers.
 *
 * Limit the buffer size to UVC_MAX_PACKETS bulk/isochronous packets. If the
 * system is too low on memory try successively smaller numbers of packets
 * until allocation succeeds.
 *
 * Return the number of allocated packets on success or 0 when out of memory.
 */
static int uvc_alloc_urb_buffers(struct uvc_streaming *stream,
    unsigned int size, unsigned int psize, gfp_t gfp_flags)
{
    unsigned int npackets;
    unsigned int i;

    UNUSED(gfp_flags);

    /* Buffers are already allocated, bail out. */
    if (stream->urb_size) {
        return stream->urb_size / psize;
    }

    /* Compute the number of packets. Bulk endpoints might transfer UVC
     * payloads across multiple URBs.
     */
    npackets = DIV_ROUND_UP(size, psize);

    if (npackets > UVC_MAX_PACKETS) {
        npackets = UVC_MAX_PACKETS;
    }

    /* Retry allocations until one succeed. */
    for (; npackets > 1; npackets /= 2) {
        for (i = 0; i < UVC_URBS; ++i) {
            stream->urb_size = psize * npackets;
            stream->urb_buffer[i] = (char *)rtw_malloc(stream->urb_size);

            if (!stream->urb_buffer[i]) {
                uvc_free_urb_buffers(stream);
                break;
            }

            UVC_PRINTF("urb_buffer %d allocated address:0x%x", i, stream->urb_buffer[i]);
        }

        if (i == UVC_URBS) {
            UVC_PRINTF("Allocated %d URB buffers of %d %d bytes each.\n", UVC_URBS, npackets, psize);
            return npackets;
        }
    }

    UVC_ERROR("Failed to allocate URB buffers (%d bytes per packet).\n", psize);
    return 0;
}

/*
 * Uninitialize isochronous/bulk URBs and free transfer buffers.
 */
static void uvc_uninit_video(struct uvc_streaming *stream, int free_buffers)
{
    struct urb *urb;
    unsigned int i;
    //int isStreaming = stream->queue.queue.streaming;
    /* edit by Ian -- disable uvc statistic api */
    //uvc_video_stats_stop(stream);
    uvc_complete_handle_deinit();

    for (i = 0; i < UVC_URBS; ++i) {
        urb = stream->urb[i];

        if (urb == NULL) {
            continue;
        }

        usb_kill_urb(urb);
        usb_free_urb(urb);
        stream->urb[i] = NULL;
    }

    if (free_buffers) {
        uvc_free_urb_buffers(stream);
    }
}

/*
 * Compute the maximum number of bytes per interval for an endpoint.
 */
static unsigned int uvc_endpoint_max_bpi(struct usb_device *dev,
    struct usb_host_endpoint *ep)
{
    u16 psize;

    switch (dev->speed) {
        case USB_SPEED_HIGH:
            psize = usb_endpoint_maxp(&ep->desc);
            return (psize & 0x07ff) * (1 + ((psize >> 11) & 3));

        default:
            psize = usb_endpoint_maxp(&ep->desc);
            return psize & 0x07ff;
    }
}

/*
 * Initialize isochronous URBs and allocate transfer buffers. The packet size
 * is given by the endpoint.
 */
int uvc_init_video_isoc(struct uvc_streaming *stream,
    struct usb_host_endpoint *ep, gfp_t gfp_flags)
{
    FUN_ENTER;
    struct urb *urb;
    unsigned int npackets, i, j;
    u16 psize;
    u32 size;
    psize = uvc_endpoint_max_bpi(stream->dev->udev, ep);
    //  size = stream->ctrl.dwMaxVideoFrameSize;
    size = UVC_URB_SIZE;
    npackets = uvc_alloc_urb_buffers(stream, size, psize, gfp_flags);

    if (npackets == 0) {
        return -USB_ENOMEM;
    }

    for (i = 0; i < UVC_URBS; ++i) {
        urb = usb_alloc_urb(npackets/*, gfp_flags*/);

        if (urb == NULL) {
            uvc_uninit_video(stream, 1);
            return -USB_ENOMEM;
        }

        urb->dev = stream->dev->udev;
        urb->context = stream;
        urb->pipe = usb_rcvisocpipe(stream->dev->udev, ep->desc.bEndpointAddress);
        urb->transfer_flags = URB_ISO_ASAP;
        urb->interval = ep->desc.bInterval;
        urb->transfer_buffer = stream->urb_buffer[i];
        urb->complete = uvc_video_complete;
        urb->number_of_packets = npackets;
        urb->transfer_buffer_length = size;

        for (j = 0; j < npackets; ++j) {
            urb->iso_frame_desc[j].offset = j * psize;
            urb->iso_frame_desc[j].length = psize;
        }

        stream->urb[i] = urb;
    }

    FUN_EXIT;
    return 0;
}

static int uvc_init_video_bulk(struct uvc_streaming *stream,
    struct usb_host_endpoint *ep, gfp_t gfp_flags)
{
    struct urb *urb;
    unsigned int npackets, pipe, i;
    u16 psize;
    u32 size;
    psize = usb_endpoint_maxp(&ep->desc) & 0x7ff;
    size = stream->ctrl.dwMaxPayloadTransferSize;
    stream->bulk.max_payload_size = size;
    npackets = uvc_alloc_urb_buffers(stream, size, psize, gfp_flags);

    if (npackets == 0) {
        return -USB_ENOMEM;
    }

    size = npackets * psize;

    if (usb_endpoint_dir_in(&ep->desc))
        pipe = usb_rcvbulkpipe(stream->dev->udev,
                ep->desc.bEndpointAddress);
    else
        pipe = usb_sndbulkpipe(stream->dev->udev,
                ep->desc.bEndpointAddress);

    if (stream->type == V4L2_BUF_TYPE_VIDEO_OUTPUT) {
        size = 0;
    }

    for (i = 0; i < UVC_URBS; ++i) {
        urb = usb_alloc_urb(0/*, gfp_flags*/);

        if (urb == NULL) {
            uvc_uninit_video(stream, 1);
            return -USB_ENOMEM;
        }

        usb_fill_bulk_urb(urb, stream->dev->udev, pipe,
            stream->urb_buffer[i], size, uvc_video_complete,
            stream);
        stream->urb[i] = urb;
    }

    return 0;
}


/*
 * Initialize isochronous/bulk URBs and allocate transfer buffers.
 */
static int uvc_init_video(struct uvc_streaming *stream, gfp_t gfp_flags)
{
    struct usb_interface *intf = stream->intf;
    struct usb_host_endpoint *ep;
    unsigned int i;
    int ret;
    //edit by Ian -- For statistic record
    //show_tick1 = show_tick2 = 0;
    stream->sequence = -1;
    stream->last_fid = -1;
    stream->bulk.header_size = 0;
    stream->bulk.skip_payload = 0;
    stream->bulk.payload_size = 0;

    /* edit by Ian -- disable uvc statistic api */
    //uvc_video_stats_start(stream);
    if (intf->num_altsetting > 1) {
        struct usb_host_endpoint *best_ep = NULL;
        unsigned int best_psize = /*UINT_MAX*/(~0U);
        unsigned int bandwidth = 0;
        unsigned int altsetting = 0;
        int intfnum = stream->intfnum;
        /* Isochronous endpoint, select the alternate setting. */
        bandwidth = (stream->ctrl.dwMaxPayloadTransferSize > 1024) ? stream->ctrl.dwMaxPayloadTransferSize : 1024;

        if (bandwidth == 0) {
            UVC_ERROR("Device requested null bandwidth, defaulting to lowest.\n");
            bandwidth = 1;
        } else {
            UVC_PRINTF("Device requested %d B/frame bandwidth.\n", bandwidth);
        }

        for (i = 0; i < intf->num_altsetting; ++i) {
            struct usb_host_interface *alts;
            unsigned int psize;

            if (intf->altsetting == NULL) {
                UVC_ERROR("Invalid altsetting!!!\n");
                continue;
            }

            alts = &intf->altsetting[i];
            ep = uvc_find_endpoint(alts, stream->header.bEndpointAddress);

            if (ep == NULL) {
                continue;
            }

            /* Check if the bandwidth is high enough. */
            psize = uvc_endpoint_max_bpi(stream->dev->udev, ep);

            if (psize >= bandwidth && psize <= best_psize) {
                altsetting = alts->desc.bAlternateSetting;
                best_psize = psize;
                best_ep = ep;
            }
        }

        if (best_ep == NULL) {
            UVC_ERROR("No fast enough alt setting for requested bandwidth.\n");
            return -USB_EIO;
        }

        UVC_PRINTF("Selecting interface %d alternate setting %d (%d B/frame bandwidth).\n", intfnum, altsetting, best_psize);
        ret = usb_set_interface(stream->dev->udev, intfnum, altsetting);

        if (ret < 0) {
            return ret;
        }

        ret = uvc_init_video_isoc(stream, best_ep, gfp_flags);
    } else {
        /* Bulk endpoint, proceed to URB initialization. */
        ep = uvc_find_endpoint(&intf->altsetting[0],
                stream->header.bEndpointAddress);

        if (ep == NULL) {
            return -USB_EIO;
        }

        ret = uvc_init_video_bulk(stream, ep, gfp_flags);
    }

    if (ret < 0) {
        return ret;
    }

    //add by Ian -- Create task to handle real uvc complete process
    if (uvc_complete_handle_init() < 0) {
        return ret;
    }

    //submit urb
    rtw_enter_critical(NULL, NULL);

    for (i = 0; i < UVC_URBS; ++i) {
        ret = usb_submit_urb(stream->urb[i]/*, gfp_flags*/);

        if (ret < 0) {
            UVC_ERROR("Failed to submit URB %d (%d).\n", i, ret);
            uvc_uninit_video(stream, 1);
            return ret;
        }
    }

    rtw_exit_critical(NULL, NULL);
    return 0;
}


/* --------------------------------------------------------------------------
 * Suspend/resume
 */

/*
 * Stop streaming without disabling the video queue.
 *
 * To let userspace applications resume without trouble, we must not touch the
 * video buffers in any way. We mark the device as frozen to make sure the URB
 * completion handler won't try to cancel the queue when we kill the URBs.
 */
int uvc_video_suspend(struct uvc_streaming *stream)
{
    if (!uvc_queue_streaming(&stream->queue)) {
        return 0;
    }

    stream->frozen = 1;
    uvc_uninit_video(stream, 0);
    usb_set_interface(stream->dev->udev, stream->intfnum, 0);
    return 0;
}

/*
 * Reconfigure the video interface and restart streaming if it was enabled
 * before suspend.
 *
 * If an error occurs, disable the video queue. This will wake all pending
 * buffers, making sure userspace applications are notified of the problem
 * instead of waiting forever.
 */
int uvc_video_resume(struct uvc_streaming *stream, int reset)
{
    int ret;

    /* If the bus has been reset on resume, set the alternate setting to 0.
     * This should be the default value, but some devices crash or otherwise
     * misbehave if they don't receive a SET_INTERFACE request before any
     * other video control request.
     */
    if (reset) {
        usb_set_interface(stream->dev->udev, stream->intfnum, 0);
    }

    stream->frozen = 0;

    /* edit by Ian -- disable uvc clock api*/
    //uvc_video_clock_reset(stream);
    /* edit by Ian -- patch for GEO */
    if (!uvc_queue_streaming(&stream->queue)) {
        return 0;
    }

    ret = uvc_commit_video(stream, &stream->ctrl);

    if (ret < 0) {
        uvc_queue_enable(&stream->queue, 0);
        return ret;
    }

#if 0
    if (!uvc_queue_streaming(&stream->queue)) {
        return 0;
    }
#endif

    /* end of edit */
    ret = uvc_init_video(stream, GFP_NOIO);

    if (ret < 0) {
        uvc_queue_enable(&stream->queue, 0);
    }

    return ret;
}

/* ------------------------------------------------------------------------
 * Video device
 */

/*
 * Initialize the UVC video device by switching to alternate setting 0 and
 * retrieve the default format.
 *
 * Some cameras (namely the Fuji Finepix) set the format and frame
 * indexes to zero. The UVC standard doesn't clearly make this a spec
 * violation, so try to silently fix the values if possible.
 *
 * This function is called before registering the device with V4L.
 */
int uvc_video_init(struct uvc_streaming *stream)
{
    struct uvc_streaming_control *probe = &stream->ctrl;
    struct uvc_format *format = NULL;
    struct uvc_frame *frame = NULL;
    unsigned int i;
    int ret;

    // stream->nformats = 2
    if (stream->nformats == 0) {
        UVC_ERROR("No supported video formats found.\n");
        return -USB_EINVAL;
    }

    atomic_set(&stream->active, 0);
    /* Initialize the video buffers queue. */
    ret = uvc_queue_init(&stream->queue, stream->type, !uvc_no_drop_param);

    if (ret) {
        return ret;
    }

    /* Alternate setting 0 should be the default, yet the XBox Live Vision
     * Cam (and possibly other devices) crash or otherwise misbehave if
     * they don't receive a SET_INTERFACE request before any other video
     * control request.
     */
    // stream->dev->udev = the usb device whicg you want to configure
    //               stream->intfnum = 1,alternating_setting = 0
    /*
    * usb_set_interface - Makes a particular alternate setting be current
    * @dev: the device whose interface is being updated
    * @interface: the interface being updated
    * @alternate: the setting being chosen.
    */
    usb_set_interface(stream->dev->udev, stream->intfnum, 0);

    /* Set the streaming probe control with default streaming parameters
     * retrieved from the device. Webcams that don't suport GET_DEF
     * requests on the probe control will just keep their current streaming
     * parameters.
     */
    ret = uvc_get_video_ctrl(stream, probe, 1, UVC_GET_DEF);
    if (ret == 0) {
        ret = uvc_set_video_ctrl(stream, probe, 1);
    }

    //printf("\n\rget default video ctrl status:%d", ret);
    /* Initialize the streaming parameters with the probe control current
     * value. This makes sure SET_CUR requests on the streaming commit
     * control will always use values retrieved from a successful GET_CUR
     * request on the probe control, as required by the UVC specification.
     */
    // get the current setting
    ret = uvc_get_video_ctrl(stream, probe, 1, UVC_GET_CUR);

    if (ret < 0) {
        return ret;
    }

    /* Check if the default format descriptor exists. Use the first
     * available format otherwise.
     */
    // stream->nformats = 2
    for (i = stream->nformats; i > 0; --i) {
        format = &stream->format[i - 1];
#if 0
        /*edit by Ian -- use MJPEG as stream default format and ignore default format descriptor*/
        if (format->type == UVC_VS_FORMAT_MJPEG) {
            break;
        }
#endif
#if 1

        // format->index = 0 ~ 1 ,bFormatIndex mains device take format index
        if (format->index == probe->bFormatIndex) {
            break;
        }

#endif
    }

    // format->nframes = 5
    if (format->nframes == 0) {
        UVC_ERROR("No frame descriptor found for the default format.\n");
        return -USB_EINVAL;
    }

    /* Zero bFrameIndex might be correct. Stream-based formats (including
     * MPEG-2 TS and DV) do not support frames but have a dummy(i.e fake) frame
     * descriptor with bFrameIndex set to zero. If the default frame
     * descriptor is not found, use the first available frame.
     */
    for (i = format->nframes; i > 0; --i) {
        frame = &format->frame[i - 1];

        if (frame->bFrameIndex == probe->bFrameIndex) {
            break;
        }
    }

    probe->bFormatIndex = format->index;
    probe->bFrameIndex = frame->bFrameIndex;
    stream->def_format = format;
    stream->cur_format = format;
    stream->cur_frame = frame;

    /* Select the video decoding function */
    if (stream->type == V4L2_BUF_TYPE_VIDEO_CAPTURE) {
        //      if (stream->dev->quirks & UVC_QUIRK_BUILTIN_ISIGHT){
        //          stream->decode = uvc_video_decode_isight;
        //      UVC_PRINTF("isight called...");
        //      }
        /*else*/ if (stream->intf->num_altsetting > 1) {
            stream->decode = uvc_video_decode_isoc; // use this
            UVC_PRINTF("isochronous mode called...");
        } else {
            UVC_PRINTF("bulk mode called...");
            stream->decode = uvc_video_decode_bulk;
        }
    } else {
        if (stream->intf->num_altsetting == 1) {
            stream->decode = uvc_video_encode_bulk;
        } else {
            UVC_ERROR("Isochronous endpoints are not supported for video output devices.\n");
            return -USB_EINVAL;
        }
    }

    return 0;
}

/*
 * Enable or disable the video stream.
 */
int uvc_video_enable(struct uvc_streaming *stream, int enable)
{
    int ret;

    if (!enable) {
        uvc_uninit_video(stream, 1);
        usb_set_interface(stream->dev->udev, stream->intfnum, 0);
        uvc_queue_enable(&stream->queue, 0);
        /* edit by Ian -- disable uvc clock api */
        //uvc_video_clock_cleanup(stream);
#if UVC_USE_DMA
        dma_memcpy_deinit(&uvc_dma);
#endif
        return 0;
    }

    /* edit by Ian -- disable uvc clock api */
#if 0
    ret = uvc_video_clock_init(stream);

    if (ret < 0) {
        return ret;
    }
#endif

    ret = uvc_queue_enable(&stream->queue, 1);
    if (ret < 0) {
        UVC_ERROR("Fail to enable uvc queue: %d", ret);
        goto error_queue;
    }

    /* Commit the streaming parameters. */
    ret = uvc_commit_video(stream, &stream->ctrl);
    if (ret < 0) {
        UVC_ERROR("Fail to commit stream parameters: %d", ret);
        goto error_commit;
    }

#if UVC_USE_DMA
    dma_memcpy_init(&uvc_dma, &uvc_dma_done_hdl, (u32)&uvc_dma);
    ATOMIC_SET(&uvc_dma_done, 1);
#endif

    ret = uvc_init_video(stream, GFP_KERNEL);
    if (ret < 0) {
        UVC_ERROR("Fail to init uvc urb: %d", ret);
        goto error_video;
    }

    return 0;
error_video:
    usb_set_interface(stream->dev->udev, stream->intfnum, 0);
error_commit:
    uvc_queue_enable(&stream->queue, 0);
error_queue:
    /* edit by Ian -- disable uvc clock api */
    //uvc_video_clock_cleanup(stream);
    return ret;
}

#endif // CONFIG_USBH_UVC

