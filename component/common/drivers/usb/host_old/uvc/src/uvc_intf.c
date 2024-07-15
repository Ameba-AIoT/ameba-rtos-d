#include <platform_opts.h>
    
#if defined(CONFIG_USBH_UVC) || defined(REFER_USBH_UVC)

#include "usb.h"
#include "uvc_os_wrap_via_osdep_api.h"
#include "videodev2.h"
#include "uvcvideo.h"
#include "v4l2_intf.h"
#include "uvc_intf.h"

extern streaming_state stream_state;
extern Sema stream_sema;
extern unsigned int n_buffers[VIDEO_NUM_DEVICES];
extern int uvc_thru_en;
extern int uvc_pure_thru_en;
struct uvc_usr_cb_t *uvch_usr_cb;

int uvc_probe_entry(struct usb_interface *intf);
void uvc_disconnect_function(struct usb_interface *intf);
static struct usb_driver usb_video_driver = {
    .name = "usb_video",
    .probe = uvc_probe_entry,
    .disconnect = uvc_disconnect_function,
};

void uvc_disconnect_function(struct usb_interface *intf)
{
	if ((uvch_usr_cb != NULL) && (uvch_usr_cb->detach != NULL)) {
		uvch_usr_cb->detach();
	}
	
	uvc_stream_free();
	uninit_v4l2_device();
	v4l_dev_release();
	uvc_disconnect(intf);
}

void uvc_register_usr_cb(struct uvc_usr_cb_t *cb)
{	
	if (cb != NULL) {
        	uvch_usr_cb = cb;
    	}
}

void uvc_stream_free(void)
{
    if (stream_state == STREAMING_ON) {
        stop_capturing();
    }

    stream_state = STREAMING_OFF;
    /* uvc driver deregister & free here if any */
}

#if UVC_BUF_DYNAMIC

extern int video_max_frame;
extern int uvc_reqbuf_size;

int uvc_stream_init(int buf_num, int buf_size)
{
    int ret = 0;
    int timer = 100;
    FUN_ENTER;

    if (buf_num > 2) {
        video_max_frame = buf_num;
    } else {
        video_max_frame = 3;
    }

    if (buf_size > 50000) {
        uvc_reqbuf_size = buf_size;
    } else {
        uvc_reqbuf_size = 50000;
    }

    /* uvc register & probe here */
    ret = usb_register_class_driver(&usb_video_driver);

    if (ret < 0) {
        goto error;
    }

    /*wait until uvc init ready*/
    while (!uvc_is_stream_ready()) {
        rtw_mdelay_os(100);

        if ((--timer) == 0) {
            ret = -1;  //timeout
            goto error;
        }
    }

    FUN_EXIT;
    return ret;
error:
    uvc_stream_free();
    FUN_EXIT;
    return ret;
}

#else

int uvc_stream_init(void)
{
    int ret = 0;
    int timer = 150;
    FUN_ENTER;
    /* uvc register & probe here */
    ret = usb_register_class_driver(&usb_video_driver);

    if (ret < 0) {
        goto error;
    }

    /*wait until uvc init ready*/

    while (!uvc_is_stream_ready()) {
        rtw_mdelay_os(100);

        if ((--timer) == 0) {
            ret = -1;  //timeout
            goto error;
        }
    }

    FUN_EXIT;
    return ret;
error:
    uvc_stream_free();
    FUN_EXIT;
    return ret;
}

#endif

int uvc_stream_on(void)
{
    int ret = 0;
    int timer = 200;
    FUN_ENTER;
    
    /* first check if stream is already on */
    if (uvc_is_stream_on()) {
        UVC_WARN("already streaming on!");
        return 0;
    }

	ret = start_capturing();
	if(ret == 0) {
		stream_state = STREAMING_ON;
	} 
 /* 
    Sema_Up(&stream_sema);
    
    while (!uvc_is_stream_on()) {
        rtw_mdelay_os(100);
        if ((--timer) == 0) {
            UVC_ERROR("Stream on failed: timeout!");
            ret = -1;
            break;
        }
    }
  */
    FUN_EXIT;
    return ret;
}

void uvc_stream_off(void)
{
    /* first check if stream is already off */
    if (stream_state != STREAMING_ON) {
        UVC_WARN("already streaming off!");
        return;
    }

    stream_state = STREAMING_PAUSED;
    stop_capturing();
}

int uvc_is_stream_ready(void)
{
    if (stream_state == STREAMING_READY) {
        return 1;
    } else {
        return 0;
    }
}

int uvc_is_stream_on(void)
{
    if (stream_state == STREAMING_ON) {
        return 1;
    } else {
        return 0;
    }
}

int uvc_is_stream_off(void)
{
    if (stream_state == STREAMING_OFF) {
        return 1;
    } else {
        return 0;
    }
}

u32 translate_format(uvc_fmt_t fmt_type)
{
    u32 formatIn = V4L2_PIX_FMT_MJPEG;

    switch (fmt_type) {
        case (UVC_FORMAT_MJPEG):
            formatIn = V4L2_PIX_FMT_MJPEG;
            break;

        case (UVC_FORMAT_H264):
            formatIn = V4L2_PIX_FMT_H264;
            break;

        default:
            UVC_ERROR("unknown format--try MJPEG instead");
    }

    return formatIn;
}

int uvc_xu_compress_ctrl(u8 compress_ratio)
{
	struct uvc_xu_control_query  xu_ctrl;
	struct video_device *vdev = video_devdata();
	u8 addr[8] = {0x00, 0x82, 0x4, 0xc1, 0x1, 0 ,0, 0};
	u8 data[8];
	int ret;

	rtw_memset(&xu_ctrl, 0, sizeof(xu_ctrl));
	rtw_memset(data, 0 ,8);
	data[0] = compress_ratio;    //only data[0] used
	
	xu_ctrl.query =UVC_SET_CUR;
	xu_ctrl.unit = 0x4;        //entity id: 0x4
	xu_ctrl.size = 0x8;        //8Byte addr
	xu_ctrl.selector = 0xA;   //selector 
	xu_ctrl.data = addr;

	ret = vdev->fops->unlocked_ioctl(UVCIOC_CTRL_QUERY, (unsigned long)&xu_ctrl);
	if (ret < 0) {
		UVC_ERROR("\nFail to set addr:%d\n", ret);
		return ret;
	}
	
	xu_ctrl.query =UVC_SET_CUR;
	xu_ctrl.unit = 0x4;        //entity id: 0x4
	xu_ctrl.size = 0x8;        //8Byte data
	xu_ctrl.selector = 0xB;   //selector 
	xu_ctrl.data = data;

	ret = vdev->fops->unlocked_ioctl(UVCIOC_CTRL_QUERY, (unsigned long)&xu_ctrl);
	if (ret < 0) {
		UVC_ERROR("\nFail to set value:%d\n", ret);
		return ret;
	}

	return 0;
}

int uvc_set_param(uvc_fmt_t fmt_type, int *width, int *height, int *frame_rate, int *compression_ratio)
{
	int ret;
	int comp = 0;
	u32 format = translate_format(fmt_type);
	
 	ret = v4l_set_param(format, width, height, frame_rate, &comp);
	if(ret < 0)
		return ret;
	
#if RTS5876
	if(*compression_ratio != 0)
		ret = uvc_xu_compress_ctrl((u8)*compression_ratio);
#endif

	return ret;
}

int uvc_get_user_ctrl(struct uvc_user_ctrl *user_ctrl)
{
    int ret;
    struct v4l2_control ctrl;
    struct v4l2_queryctrl queryctrl;
    struct video_device *vdev = video_devdata();
    rtw_memset(&ctrl, 0, sizeof(ctrl));
    rtw_memset(&queryctrl, 0, sizeof(queryctrl));

    switch (user_ctrl->ctrl_id) {
        case (USER_CTRL_BRIGHTNESS):
            queryctrl.id = ctrl.id = V4L2_CID_BRIGHTNESS;
            ctrl.value = user_ctrl->ctrl_value;
            break;

        case (USER_CTRL_CONTRAST):
            queryctrl.id = ctrl.id = V4L2_CID_CONTRAST;
            ctrl.value = user_ctrl->ctrl_value;
            break;

        case (USER_CTRL_SATURATION):
            queryctrl.id = ctrl.id = V4L2_CID_SATURATION;
            ctrl.value = user_ctrl->ctrl_value;
            break;

        case (USER_CTRL_HUE):
            queryctrl.id = ctrl.id = V4L2_CID_CONTRAST;
            ctrl.value = user_ctrl->ctrl_value;
            break;

        case (USER_CTRL_AUTO_WHITE_BALANCE):
            queryctrl.id = ctrl.id = V4L2_CID_AUTO_WHITE_BALANCE;
            ctrl.value = user_ctrl->ctrl_value;
            break;

        case (USER_CTRL_GAMMA):
            queryctrl.id = ctrl.id = V4L2_CID_GAMMA;
            ctrl.value = user_ctrl->ctrl_value;
            break;

        case (USER_CTRL_EXPOSURE):
            queryctrl.id = ctrl.id = V4L2_CID_EXPOSURE;
            ctrl.value = user_ctrl->ctrl_value;
            break;

        default:
            UVC_ERROR("Unsupported user ctrl!");
            return -1;
    }

    //QUERY CTRL FIRST
    if (vdev->fops->unlocked_ioctl(VIDIOC_QUERYCTRL, (unsigned long)&queryctrl) == 0) {
        UVC_WARN("Control %s\n\r", queryctrl.name);

        if (!(queryctrl.flags & V4L2_CTRL_FLAG_DISABLED) && (queryctrl.type != V4L2_CTRL_TYPE_MENU)) {
            //print out control info
            UVC_PRINTF("max:%d min:%d step:%d", queryctrl.maximum, queryctrl.minimum, queryctrl.step);
        }
    }

    ret = vdev->fops->unlocked_ioctl(VIDIOC_G_CTRL, (unsigned long)&ctrl);
    if (ret == 0) {
        user_ctrl->ctrl_value = ctrl.value;
        UVC_WARN("current value:%d\n", user_ctrl->ctrl_value);
    }

    return ret;
}

int uvc_set_user_ctrl(struct uvc_user_ctrl *user_ctrl)
{
    int ret;
    struct v4l2_control ctrl;
    struct video_device *vdev = video_devdata();

    switch (user_ctrl->ctrl_id) {
        case (USER_CTRL_BRIGHTNESS):
            ctrl.id = V4L2_CID_BRIGHTNESS;
            ctrl.value = user_ctrl->ctrl_value;
            break;

        case (USER_CTRL_CONTRAST):
            ctrl.id = V4L2_CID_CONTRAST;
            ctrl.value = user_ctrl->ctrl_value;
            break;

        case (USER_CTRL_SATURATION):
            ctrl.id = V4L2_CID_SATURATION;
            ctrl.value = user_ctrl->ctrl_value;
            break;

        case (USER_CTRL_HUE):
            ctrl.id = V4L2_CID_CONTRAST;
            ctrl.value = user_ctrl->ctrl_value;
            break;

        case (USER_CTRL_AUTO_WHITE_BALANCE):
            ctrl.id = V4L2_CID_AUTO_WHITE_BALANCE;
            ctrl.value = user_ctrl->ctrl_value;
            break;

        case (USER_CTRL_GAMMA):
            ctrl.id = V4L2_CID_GAMMA;
            ctrl.value = user_ctrl->ctrl_value;
            break;

        case (USER_CTRL_EXPOSURE):
            ctrl.id = V4L2_CID_EXPOSURE;
            ctrl.value = user_ctrl->ctrl_value;
            break;

        default:
            UVC_ERROR("unsupported user ctrl!");
            return -1;
    }

    ret = vdev->fops->unlocked_ioctl(VIDIOC_S_CTRL, (unsigned long)&ctrl);
    return ret;
}

int uvc_buf_check(struct uvc_buf_context *b)
{
    int ret = 0;
    struct video_device *vdev = video_devdata();

    if ((b->index > (int)n_buffers[vdev->minor]) || (b->data == NULL) || (b->len > (int)UVC_REQBUF_SIZE) || (b->len < 0)) {
        ret = -1;
    }

    return ret;
}

int uvc_dqbuf(struct uvc_buf_context *b)
{
    FUN_ENTER;
    int ret;
    struct v4l2_buffer buf;
    struct video_device *vdev = video_devdata();
    
    rtw_memset(&buf, 0, sizeof(struct v4l2_buffer));
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP /*V4L2_MEMORY_DMABUF*/;

    ret = vdev->fops->unlocked_ioctl(VIDIOC_DQBUF, (unsigned long)&buf);
    if (ret < 0) {
        switch (ret) {
            case -USB_ENODATA:
            case -USB_EAGAIN:
            case -USB_EIO:
                b->index = -1;
                b->data = NULL;
                b->len = 0;
                UVC_WARN("Fail to dequeue UVC buffer: %d", ret);
                FUN_EXIT;
                return 0;

            default:
                UVC_ERROR("Fail to dequeue UVC buffer: %d", ret);
                FUN_EXIT;
                return ret;
        }
    }

    b->index = buf.index;
    b->data = (unsigned char *)buf.m.offset;
    b->len = buf.bytesused;
    b->timestamp = buf.timestamp;
    
    FUN_EXIT;
    return ret;
}

int uvc_qbuf(struct uvc_buf_context *b)
{
    int ret;
    struct v4l2_buffer buf;
    struct video_device *vdev = video_devdata();
    
    rtw_memset(&buf, 0, sizeof(struct v4l2_buffer));
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP /*V4L2_MEMORY_DMABUF*/;
    buf.index = b->index;
    //buf.m.offset = (uint32_t)b->data;
    //buf.bytesused = b->len;
    buf.field = V4L2_FIELD_NONE;
    buf.timestamp = rtw_get_current_time();
    ret = vdev->fops->unlocked_ioctl(VIDIOC_QBUF, (unsigned long)&buf);
    
    return ret;
}

int is_pure_thru_on(void)
{
    return uvc_pure_thru_en;
}

void uvc_pure_thru_on(void)
{
    uvc_pure_thru_en = 1;
    uvc_thru_en = 1;
}

void uvc_dec_thru_on(void)
{
    uvc_pure_thru_en = 0;
    uvc_thru_en = 1;
}

void uvc_thru_off(void)
{
    uvc_thru_en = 0;
    uvc_pure_thru_en = 0;
}

#endif // CONFIG_USBH_UVC
