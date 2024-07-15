/*
 *      uvc_queue.c  --  USB Video Class driver - Buffers management
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
#include "section_config.h"
#if UVC_BUF_DYNAMIC
#include "psram_reserve.h"
#endif

#if UVC_BUF_DYNAMIC
int video_max_frame;
int uvc_reqbuf_size;
#else
// The video buffer shall be put into PSRAM
PSRAM_BSS_SECTION u8 global_buf[VIDEO_MAX_FRAME][UVC_REQBUF_SIZE];
#endif


/* ------------------------------------------------------------------------
 * Video buffers queue management.
 *
 * Video queues is initialized by uvc_queue_init(). The function performs
 * basic initialization of the uvc_video_queue struct and never fails.
 *
 * Video buffers are managed by videobuf2. The driver uses a mutex to protect
 * the videobuf2 queue operations by serializing calls to videobuf2 and a
 * spinlock to protect the IRQ queue that holds the buffers to be processed by
 * the driver.
 */

#if UVC_BUF_DYNAMIC
static int uvc_queue_setup(struct vb2_queue *vq, const struct v4l2_format *fmt,
    unsigned int *nbuffers, unsigned int *nplanes,
    unsigned int sizes[], void *alloc_ctxs[])
{
    struct uvc_video_queue *queue = vb2_get_drv_priv(vq);
    struct uvc_streaming *stream =
        container_of(queue, struct uvc_streaming, queue);

    //*nbuffer=4
    if (*nbuffers > UVC_MAX_VIDEO_BUFFERS) {
        *nbuffers = UVC_MAX_VIDEO_BUFFERS;
    }

    *nplanes = 1;
    /* edit by Ian -- default uvc requested buffer size */
    sizes[0] = uvc_reqbuf_size;
    UVC_PRINTF("set buffer allocate size:%dB", sizes[0]);
    return 0;
}
#else
static int uvc_queue_setup(struct vb2_queue *vq, const struct v4l2_format *fmt,
    unsigned int *nbuffers, unsigned int *nplanes,
    unsigned int sizes[], void *alloc_ctxs[])
{
    UNUSED(vq);
    UNUSED(fmt);
    UNUSED(alloc_ctxs);

    //*nbuffer=4
    if (*nbuffers > UVC_MAX_VIDEO_BUFFERS) {
        *nbuffers = UVC_MAX_VIDEO_BUFFERS;
    }

    *nplanes = 1;
    
    /* edit by Ian -- default uvc requested buffer size */
    sizes[0] = UVC_REQBUF_SIZE;
    UVC_PRINTF("set buffer allocate size:%dB", sizes[0]);
    
    return 0;
}
#endif

#if UVC_BUF_DYNAMIC
static int uvc_buffer_prepare(struct vb2_buffer *vb, unsigned int index)
{
    struct uvc_video_queue *queue = vb2_get_drv_priv(vb->vb2_queue);
    struct uvc_buffer *buf = container_of(vb, struct uvc_buffer, buf);

    if (vb->v4l2_buf.type == V4L2_BUF_TYPE_VIDEO_OUTPUT &&
        vb2_get_plane_payload(vb, 0) > vb2_plane_size(vb, 0)) {
        UVC_ERROR("Bytes used out of bounds.\n");
        return -USB_EINVAL;
    }

    if (queue->flags & UVC_QUEUE_DISCONNECTED) {
        return -USB_ENODEV;
    }

    buf->state = UVC_BUF_STATE_QUEUED;
    buf->error = 0;
    buf->length = vb2_plane_size(vb, 0);

    if (vb->v4l2_buf.type == V4L2_BUF_TYPE_VIDEO_CAPTURE) {
        buf->bytesused = 0;
    } else {
        buf->bytesused = vb2_get_plane_payload(vb, 0);
    }

    if (buf->mem != NULL) {
        UVC_PRINTF("uvc buffer already allocated");
        /*edit by Ian -- memory operation cost too long time*/
        //rtw_memset(buf->mem, 0, buf->length);
        //rtw_memset(buf->mem, 0, buf->bytesused);
        return 0;
    }

    if (index > VIDEO_MAX_FRAME) {
        UVC_ERROR("buffer index exceeds maximum!");
        return 0;
    }

    buf->mem = (void *)Psram_reserve_malloc(uvc_reqbuf_size);
    //buf->mem = malloc(uvc_reqbuf_size);

    if (buf->mem != NULL) {
        rtw_memset(buf->mem, 0, buf->length);
        UVC_PRINTF("uvc buffer memory address:0x%x, buffer size:%d/%d", buf->mem, buf->length, vb->v4l2_planes[0].length);
    } else {
        UVC_ERROR("uvc buffer memory allocate failed");
        return -USB_EINVAL;
    }

    return 0;
}
#else
static int uvc_buffer_prepare(struct vb2_buffer *vb, unsigned int index)
{
    struct uvc_video_queue *queue = vb2_get_drv_priv(vb->vb2_queue);
    struct uvc_buffer *buf = container_of(vb, struct uvc_buffer, buf);

    if (vb->v4l2_buf.type == V4L2_BUF_TYPE_VIDEO_OUTPUT &&
        vb2_get_plane_payload(vb, 0) > vb2_plane_size(vb, 0)) {
        UVC_ERROR("Bytes used out of bounds.\n");
        return -USB_EINVAL;
    }

    if (queue->flags & UVC_QUEUE_DISCONNECTED) {
        return -USB_ENODEV;
    }

    buf->state = UVC_BUF_STATE_QUEUED;
    buf->error = 0;
    buf->length = vb2_plane_size(vb, 0);

    if (vb->v4l2_buf.type == V4L2_BUF_TYPE_VIDEO_CAPTURE) {
        buf->bytesused = 0;
    } else {
        buf->bytesused = vb2_get_plane_payload(vb, 0);
    }

    if (buf->mem != NULL) {
        UVC_PRINTF("uvc buffer already allocated");
        /*edit by Ian -- memory operation cost too long time*/
        //rtw_memset(buf->mem, 0, buf->length);
        //rtw_memset(buf->mem, 0, buf->bytesused);
        return 0;
    }

    if (index > VIDEO_MAX_FRAME) {
        UVC_ERROR("buffer index exceeds maximum!");
        return 0;
    }

    buf->mem = global_buf[index];

    if (buf->mem != NULL) {
        rtw_memset(buf->mem, 0, buf->length);
        UVC_PRINTF("uvc buffer memory address:0x%x, buffer size:%d/%d", buf->mem, buf->length, vb->v4l2_planes[0].length);
    } else {
        UVC_ERROR("uvc buffer memory allocate failed");
        return -USB_EINVAL;
    }

    return 0;
}
#endif // UVC_BUF_DYNAMIC

static void uvc_buffer_queue(struct vb2_buffer *vb)
{
    struct uvc_video_queue *queue = vb2_get_drv_priv(vb->vb2_queue);
    struct uvc_buffer *buf = container_of(vb, struct uvc_buffer, buf);
    
    Mutex_Lock(&queue->irqlock);

    if (!(queue->flags & UVC_QUEUE_DISCONNECTED)) {
        list_add_tail(&buf->queue, &queue->irqqueue);
    } else {
        /* If the device is disconnected return the buffer to userspace
         * directly. The next QBUF call will fail with -USB_ENODEV.
         */
        buf->state = UVC_BUF_STATE_ERROR;
        vb2_buffer_done(&buf->buf, VB2_BUF_STATE_ERROR);
    }

    Mutex_Unlock(&queue->irqlock);
}

static int uvc_buffer_finish(struct vb2_buffer *vb)
{
    UNUSED(vb);
#if 0
    struct uvc_video_queue *queue = vb2_get_drv_priv(vb->vb2_queue);
    struct uvc_streaming *stream =  container_of(queue, struct uvc_streaming, queue);
    struct uvc_buffer *buf = container_of(vb, struct uvc_buffer, buf);
    if (vb->state == VB2_BUF_STATE_DONE) {
        uvc_video_clock_update(stream, &vb->v4l2_buf, buf);
    }
#endif
    return 0;
}

static void uvc_wait_prepare(struct vb2_queue *vq)
{
    struct uvc_video_queue *queue = vb2_get_drv_priv(vq);
    Mutex_Unlock(&queue->mutex);
}

static void uvc_wait_finish(struct vb2_queue *vq)
{
    struct uvc_video_queue *queue = vb2_get_drv_priv(vq);
    Mutex_Lock(&queue->mutex);
}

#if UVC_BUF_DYNAMIC
static void uvc_buffer_cleanup(struct vb2_buffer *vb)
{
    struct uvc_buffer *buf = container_of(vb, struct uvc_buffer, buf);
    //rtw_free(buf->mem);
    Psram_reserve_free(buf->mem);
    buf->mem = NULL;
}
#else
static void uvc_buffer_cleanup(struct vb2_buffer *vb)
{
    struct uvc_buffer *buf = container_of(vb, struct uvc_buffer, buf);
    //rtw_free(buf->mem);
    buf->mem = NULL;
}
#endif

static int uvc_get_connect_status(void)
{
	if(usbh_get_connect_status())
		return 1;
	else
		return 0;
}

static struct vb2_ops uvc_queue_qops = {
    .queue_setup = uvc_queue_setup,
    .buf_prepare = uvc_buffer_prepare,
    .buf_queue = uvc_buffer_queue,
    .buf_finish = uvc_buffer_finish,
    .wait_prepare = uvc_wait_prepare,
    .wait_finish = uvc_wait_finish,
    .buf_cleanup = uvc_buffer_cleanup,
    //.start_streaming = uvc_start_streaming,
    //.stop_streaming = uvc_stop_streaming,
    .get_connect_status = uvc_get_connect_status,
};

int uvc_queue_init(struct uvc_video_queue *queue, enum v4l2_buf_type type,
    int drop_corrupted)
{
    int ret;
    
    queue->queue.type = type;
    queue->queue.io_modes = VB2_MMAP | VB2_USERPTR | VB2_DMABUF;
    queue->queue.drv_priv = queue;
    queue->queue.buf_struct_size = sizeof(struct uvc_buffer);
    queue->queue.ops = &uvc_queue_qops;
    queue->queue.mem_ops = /*&vb2_vmalloc_memops*/NULL;
    queue->queue.timestamp_type = V4L2_BUF_FLAG_TIMESTAMP_MONOTONIC;
    
    ret = vb2_queue_init(&queue->queue);
    if (ret) {
        return ret;
    }

    Mutex_Init(&queue->mutex);
    Mutex_Init(&queue->irqlock);
    INIT_LIST_HEAD(&queue->irqqueue);
    queue->flags = drop_corrupted ? UVC_QUEUE_DROP_CORRUPTED : 0;
    
    return 0;
}

/* -----------------------------------------------------------------------------
 * V4L2 queue operations
 */

int uvc_alloc_buffers(struct uvc_video_queue *queue,
    struct v4l2_requestbuffers *rb)
{
    int ret;
    
    Mutex_Lock(&queue->mutex);
    ret = vb2_reqbufs(&queue->queue, rb);
    Mutex_Unlock(&queue->mutex);
    
    return ret ? ret : (int)rb->count;
}

void uvc_free_buffers(struct uvc_video_queue *queue)
{
    Mutex_Lock(&queue->mutex);
    vb2_queue_release(&queue->queue);
    Mutex_Unlock(&queue->mutex);
}

int uvc_query_buffer(struct uvc_video_queue *queue, struct v4l2_buffer *buf)
{
    int ret;
    
    Mutex_Lock(&queue->mutex);
    ret = vb2_querybuf(&queue->queue, buf);
    Mutex_Unlock(&queue->mutex);
    
    return ret;
}

int uvc_queue_buffer(struct uvc_video_queue *queue, struct v4l2_buffer *buf)
{
    int ret;
    
    Mutex_Lock(&queue->mutex);
    ret = vb2_qbuf(&queue->queue, buf);
    Mutex_Unlock(&queue->mutex);
    
    return ret;
}

int uvc_dequeue_buffer(struct uvc_video_queue *queue, struct v4l2_buffer *buf,
    int nonblocking)
{
    int ret;
    
    Mutex_Lock(&queue->mutex);
    ret = vb2_dqbuf(&queue->queue, buf, nonblocking);
    Mutex_Unlock(&queue->mutex);
    
    return ret;
}

int uvc_queue_mmap(struct uvc_video_queue *queue)
{
    int ret;
    
    Mutex_Lock(&queue->mutex);
    ret = vb2_mmap(&queue->queue);
    Mutex_Unlock(&queue->mutex);
    
    return ret;
}

/*
 * Check if buffers have been allocated.
 */
int uvc_queue_allocated(struct uvc_video_queue *queue)
{
    int allocated;
    
    Mutex_Lock(&queue->mutex);
    allocated = vb2_is_busy(&queue->queue);
    Mutex_Unlock(&queue->mutex);
    
    return allocated;
}

/*
 * Enable or disable the video buffers queue.
 *
 * The queue must be enabled before starting video acquisition and must be
 * disabled after stopping it. This ensures that the video buffers queue
 * state can be properly initialized before buffers are accessed from the
 * interrupt handler.
 *
 * Enabling the video queue returns -USB_EBUSY if the queue is already enabled.
 *
 * Disabling the video queue cancels the queue and removes all buffers from
 * the main queue.
 *
 * This function can't be called from interrupt context. Use
 * uvc_queue_cancel() instead.
 */
int uvc_queue_enable(struct uvc_video_queue *queue, int enable)
{
    int ret;
    
    Mutex_Lock(&queue->mutex);

    if (enable) {
        ret = vb2_streamon(&queue->queue, queue->queue.type);
        if (ret < 0) {
            goto done;
        }

        queue->buf_used = 0;
    } else {
        ret = vb2_streamoff(&queue->queue, queue->queue.type);
        if (ret < 0) {
            goto done;
        }

        Mutex_Lock(&queue->irqlock);
        INIT_LIST_HEAD(&queue->irqqueue);
        Mutex_Unlock(&queue->irqlock);
    }

done:
    Mutex_Unlock(&queue->mutex);
    return ret;
}

/*
 * Cancel the video buffers queue.
 *
 * Cancelling the queue marks all buffers on the irq queue as erroneous,
 * wakes them up and removes them from the queue.
 *
 * If the disconnect parameter is set, further calls to uvc_queue_buffer will
 * fail with -USB_ENODEV.
 *
 * This function acquires the irq spinlock and can be called from interrupt
 * context.
 */
void uvc_queue_cancel(struct uvc_video_queue *queue, int disconnect)
{
    struct uvc_buffer *buf;
    
    Mutex_Lock(&queue->irqlock);

    while (!list_empty(&queue->irqqueue)) {
        buf = list_first_entry(&queue->irqqueue, struct uvc_buffer, queue);
        list_del(&buf->queue);
        buf->state = UVC_BUF_STATE_ERROR;
        vb2_buffer_done(&buf->buf, VB2_BUF_STATE_ERROR);
    }

    /* This must be protected by the irqlock spinlock to avoid race
     * conditions between uvc_buffer_queue and the disconnection event that
     * could result in an interruptible wait in uvc_dequeue_buffer. Do not
     * blindly replace this logic by checking for the UVC_QUEUE_DISCONNECTED
     * state outside the queue code.
     */
    if (disconnect) {
        queue->flags |= UVC_QUEUE_DISCONNECTED;
    }

    Mutex_Unlock(&queue->irqlock);
}

struct uvc_buffer *uvc_queue_next_buffer(struct uvc_video_queue *queue,
    struct uvc_buffer *buf)
{
    struct uvc_buffer *nextbuf;

    if ((queue->flags & UVC_QUEUE_DROP_CORRUPTED) && buf->error) {
        buf->error = 0;
        buf->state = UVC_BUF_STATE_QUEUED;
        /*edit by Ian -- memory operation cost too long time*/
        //rtw_memset(buf->mem, 0, buf->length);
        //rtw_memset(buf->mem, 0, buf->bytesused);
        buf->bytesused = 0;
        vb2_set_plane_payload(&buf->buf, 0, 0);
        UVC_PRINTF("\n\rqueue drop corrupted...\n");
        return buf;
    }

    Mutex_Lock(&queue->irqlock);
    list_del(&buf->queue);

    if (!list_empty(&queue->irqqueue))
        nextbuf = list_first_entry(&queue->irqqueue, struct uvc_buffer, queue);
    else {
        nextbuf = NULL;
    }

    Mutex_Unlock(&queue->irqlock);
    buf->state = buf->error ? UVC_BUF_STATE_ERROR : UVC_BUF_STATE_DONE;
    vb2_set_plane_payload(&buf->buf, 0, buf->bytesused);
    vb2_buffer_done(&buf->buf, (enum vb2_buffer_state)buf->state);
    
    return nextbuf;
}

#endif // CONFIG_USBH_UVC
