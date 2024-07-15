#include <platform_opts.h>

#ifdef CONFIG_USBD_MSC

#include "usbd_msc.h"
#include "usbd_scsi.h"
#include "usb_composite.h"
#include "usb_errno.h"
#include "osdep_service.h"
#include "sd.h"

#define USBD_MSC_STRING_MANUFACTURER         1
#define USBD_MSC_STRING_PRODUCT              2
#define USBD_MSC_STRING_SERIALNUMBER         3

// define global variables
static struct usb_msc_dev_t msc_dev;
static struct usb_msc_common_t msc_common;

// physical disk access methods
static struct usb_msc_opts_t disk_operation;

static struct usb_string msc_strings[] = {
    {
        .id = USBD_MSC_STRING_MANUFACTURER,
        .s  = "Realtek",
    },
    {
        .id = USBD_MSC_STRING_PRODUCT,
        .s  = "USB MSC Device",
    },
    {
        .id = USBD_MSC_STRING_SERIALNUMBER,
        .s  = "0123456789AB",
    },
};

static struct usb_gadget_strings msc_gadget_string_tab = {
    .language = 0x0409,
    .strings = msc_strings,
};
    
static struct usb_gadget_strings *msc_gadget_strings[] = {
    &msc_gadget_string_tab,
    NULL
};

// MSC Device Descriptor
static struct usb_device_descriptor msc_device_desc = {
    .bLength            = USB_DT_DEVICE_SIZE,
    .bDescriptorType    = USB_DT_DEVICE,
    .bcdUSB             = 0x0200,
    .bDeviceClass       = USB_CLASS_PER_INTERFACE,
    .bDeviceSubClass    = 0,
    .bDeviceProtocol    = 0,
    .bMaxPacketSize0    = 64,
    .idVendor           = REALTEK_USB_VID,
    .idProduct          = REALTEK_USB_PID+1,
    .bcdDevice          = 0x0100,
    .iManufacturer      = USBD_MSC_STRING_MANUFACTURER,
    .iProduct           = USBD_MSC_STRING_PRODUCT,
    .iSerialNumber      = USBD_MSC_STRING_SERIALNUMBER,
    .bNumConfigurations = 1,
};

/* MSC Interface, Alternate Setting 0*/
static struct usb_interface_descriptor msc_intf_desc = {
    .bLength            = USB_DT_INTERFACE_SIZE,
    .bDescriptorType    = USB_DT_INTERFACE,
    .bInterfaceNumber   = 0, // this will be assign automatically
    .bAlternateSetting  = 0,
    .bNumEndpoints      = 2,
    .bInterfaceClass    = USB_CLASS_MASS_STORAGE,
    .bInterfaceSubClass = USBD_MSC_SC_SCSI,
    .bInterfaceProtocol = USBD_MSC_PR_BULK,
    .iInterface         = 0,
};

/* MSC Endpoints for Low-speed/Full-speed */
/* Endpoint, EP Bulk IN */
static struct usb_endpoint_descriptor msc_source_desc_FS = {
    .bLength            = USB_DT_ENDPOINT_SIZE,
    .bDescriptorType    = USB_DT_ENDPOINT,
    .bEndpointAddress   = USBD_MSC_BULK_IN_EP_ADDRESS,
    .bmAttributes       = USB_ENDPOINT_XFER_BULK,
    .wMaxPacketSize     = 64,
    .bInterval          = 0,

};
/* Endpoint, EP Bulk OUT */
static struct usb_endpoint_descriptor msc_sink_desc_FS = {
    .bLength            = USB_DT_ENDPOINT_SIZE,
    .bDescriptorType    = USB_DT_ENDPOINT,
    .bEndpointAddress   = USBD_MSC_BULK_OUT_EP_ADDRESS,
    .bmAttributes       = USB_ENDPOINT_XFER_BULK,
    .wMaxPacketSize     = 64,
    .bInterval          = 0,
};

/* MSC Endpoints for High-speed */
/* Endpoint, EP Bulk IN */
static struct usb_endpoint_descriptor msc_source_desc_HS = {
    .bLength            = USB_DT_ENDPOINT_SIZE,
    .bDescriptorType    = USB_DT_ENDPOINT,
    .bEndpointAddress   = USBD_MSC_BULK_IN_EP_ADDRESS,
    .bmAttributes       = USB_ENDPOINT_XFER_BULK,
    .wMaxPacketSize     = 512,
    .bInterval          = 0,
};

/* Endpoint, EP Bulk OUT */
static struct usb_endpoint_descriptor msc_sink_desc_HS = {
    .bLength            = USB_DT_ENDPOINT_SIZE,
    .bDescriptorType    = USB_DT_ENDPOINT,
    .bEndpointAddress   = USBD_MSC_BULK_OUT_EP_ADDRESS,
    .bmAttributes       = USB_ENDPOINT_XFER_BULK,
    .wMaxPacketSize     = 512,
    .bInterval          = 0,
};

static struct usb_descriptor_header *msc_descriptors_FS [] = {
    /* data interface has no altsetting */
    (struct usb_descriptor_header *) &msc_intf_desc,
    (struct usb_descriptor_header *) &msc_source_desc_FS,
    (struct usb_descriptor_header *) &msc_sink_desc_FS,
    NULL,
};

static struct usb_descriptor_header *msc_descriptors_HS [] = {
    /* data interface has no altsetting */
    (struct usb_descriptor_header *) &msc_intf_desc,
    (struct usb_descriptor_header *) &msc_source_desc_HS,
    (struct usb_descriptor_header *) &msc_sink_desc_HS,
    NULL,
};

static struct usb_config_descriptor msc_config_desc = {
    .bLength             = USB_DT_CONFIG_SIZE,
    .bDescriptorType     = USB_DT_CONFIG,
    .bNumInterfaces      = 1,                  // This will be assign automatically
    .bConfigurationValue = 1,
    .iConfiguration      = 0,
	.bmAttributes	     = USB_CONFIG_ATT_ONE, // | USB_CONFIG_ATT_SELFPOWER,
    .bMaxPower           = 50,
};

#if USBD_MSC_RAM_DISK
static u8 *usbd_msc_ram_disk_buf;
#endif

static inline void *msc_malloc(u32 size)
{
    void *buf = rtw_zmalloc(size);
    return buf;
}

static inline void msc_free(void *buf)
{
    if (buf != NULL) {
        rtw_free(buf);
    }
}

int usbd_msc_halt_bulk_in_endpoint(struct usb_msc_dev_t *mscdev)
{
    int rc;
    int error_cnt = 0;
    
    rc = usb_ep_set_halt(mscdev->in_ep);
    if (rc == -USB_EAGAIN) {
        USBD_MSC_ERROR("delayed bulk-in endpoint halt");
    }

    while (rc != 0) {
        if (rc != -USB_EAGAIN) {
            USBD_MSC_ERROR("usb_ep_set_halt -> %d", rc);
            rc = 0;
            break;
        }

        ++error_cnt;
        if (error_cnt > USBD_MSC_HALT_ERROR_TOLERANCE)
        {
            USBD_MSC_ERROR("usb_ep_set_halt fail");
            rc = 0;
            break;
        }

        /* Wait for a short time and then try again */
        rtw_mdelay_os(10);
        rc = usb_ep_set_halt(mscdev->in_ep);;
    }

    return rc;
}

int usbd_msc_bulk_in_transfer(struct usb_msc_dev_t *mscdev, struct usb_request *req)
{
    req->dma = (dma_addr_t)req->buf;
    return usb_ep_queue(mscdev->in_ep, req, 1);
}

int usbd_msc_bulk_out_transfer(struct usb_msc_dev_t *mscdev, struct usb_request *req)
{
    req->dma = (dma_addr_t)req->buf;
    return usb_ep_queue(mscdev->out_ep, req, 1);
}

static void msc_bulk_in_complete(struct usb_ep *ep, struct usb_request *req)
{
    struct usb_msc_bufhd_t *bufhd = (struct usb_msc_bufhd_t *)req->context;
    struct usb_msc_dev_t *mscdev = (struct usb_msc_dev_t *)ep->driver_data;
    struct usb_msc_common_t *common = mscdev->common;
    int status = req->status;
    
    /* check request status */
    switch (status) {
        case USB_ESUCCESS:          /* tx seccussfuly*/
            break;

        case -USB_ECONNRESET:       /* unlink */
            USBD_MSC_ERROR("ECONNRESET");
            break;

        case -USB_ESHUTDOWN:        /* disconnect etc */
            USBD_MSC_ERROR("ESHUTDOWN");
            break;

        default:
            USBD_MSC_ERROR("ERROR(%d)", status);
            break;
    }

    if (bufhd->type == BUFHD_DATA) {
        usbd_msc_put_bufhd(common, bufhd);
    }
}

static thread_return msc_bulk_cmd_handler(thread_context context)
{
    struct usb_msc_dev_t *mscdev = (struct usb_msc_dev_t *)context;
    struct usb_msc_common_t *common = mscdev->common;
    struct usb_msc_bufhd_t *bufhd = NULL;
    struct usb_request *req = NULL;
    int result;

    while (1) {
        result = rtw_down_sema(&(mscdev->bulk_out_cmd_task.wakeup_sema));
        if (FAIL == result) {
            USBD_MSC_ERROR("rtw down sema fail");
            break;
        }

        if (mscdev->bulk_out_cmd_task.blocked) {
            break;
        }

        /* stop the current task */
        if (!mscdev->common->running) {
            continue;
        }

        // handle bulk out request
        rtw_mutex_get(&common->boc_mutex);
        result = rtw_is_list_empty(&common->boc_list);
        rtw_mutex_put(&common->boc_mutex);
        if (result == TRUE) {
            continue;
        }

next:
        if (!mscdev->common->running) {
            continue;
        }

        rtw_mutex_get(&common->boc_mutex);
        bufhd = list_first_entry(&common->boc_list, struct usb_msc_bufhd_t, list);
        rtw_mutex_put(&common->boc_mutex);
        if (bufhd == NULL)
        {
            continue;
        }
        
        req = bufhd->reqout;

        // receive CBW
        if (bufhd->type == BUFHD_CBW) {
            result = usbd_msc_receive_cbw(mscdev, req);
        }

	if(result !=0) {
		struct bulk_cb_wrap *cbw = req->buf;
		if((cbw->Flags == 0) &&
			(cbw->DataTransferLength != 0) &&
			(mscdev->bot_status == USBD_BOT_STATUS_NORMAL)) {
			usb_ep_set_halt(mscdev->out_ep);
		}
		usbd_msc_halt_bulk_in_endpoint(mscdev);
	}

        //if (result == 0) { //response successfully
            rtw_list_delete(&bufhd->list);
        //}

        rtw_mutex_get(&common->boc_mutex);
        result = rtw_is_list_empty(&common->boc_list);
        rtw_mutex_put(&common->boc_mutex);

        if (result == FALSE) {
            goto next;
        }
        /* submit bulk out request agains*/
        else {
            if (mscdev->common->running && (bufhd->type == BUFHD_CBW)) {
                req->length = US_BULK_CB_WRAP_LEN;
                result = usb_ep_queue(mscdev->out_ep, req, 1);
                if (result) {
                    USBD_MSC_ERROR("%s queue req --> %d", mscdev->out_ep->name, result);
                }
            }
        }
    }

    rtw_up_sema(&(mscdev->bulk_out_cmd_task.terminate_sema));
    USBD_MSC_ERROR("TASK DELETE");
    rtw_thread_exit();
}

static void msc_bulk_out_complete(struct usb_ep *ep, struct usb_request *req)
{
    struct usb_msc_dev_t *mscdev = ep->driver_data;
    struct usb_msc_common_t *common = mscdev->common;
    struct usb_msc_bufhd_t *bufhd = (struct usb_msc_bufhd_t *)req->context;
    int status = req->status;

    /*check request status*/
    switch (status) {
        case USB_ESUCCESS:  /* normal complete */
            if (req->actual > 0) {
                if (bufhd->type == BUFHD_CBW/* && req->actual == US_BULK_CB_WRAP_LEN*/) {
                    rtw_mutex_get(&common->boc_mutex);
                    rtw_list_insert_tail(&bufhd->list, &common->boc_list);
                    rtw_mutex_put(&common->boc_mutex);
                    /*release sema to wake up bulk out handler*/
                    rtw_up_sema(&(mscdev->bulk_out_cmd_task.wakeup_sema));
                } else if (bufhd->type == BUFHD_DATA && req->actual >= common->curlun->blksize) {
                    rtw_mutex_get(&common->bod_mutex);
                    rtw_list_insert_tail(&bufhd->list, &common->bod_list);
                    rtw_mutex_put(&common->bod_mutex);
                } else {
                    USBD_MSC_ERROR("invalid package received (%d)", req->actual);
		#if 0	//Todo
                    if (req->actual == US_BULK_CB_WRAP_LEN) {
                        struct usb_msc_bufhd_t *bhd = common->cbw_bh;
                        rtw_memcpy(bhd->reqout->buf, req->buf, req->actual);
                        bhd->reqout->actual = req->actual;
                        rtw_mutex_get(&common->boc_mutex);
                        rtw_list_insert_tail(&bhd->list, &common->boc_list);
                        rtw_mutex_put(&common->boc_mutex);
                        /*release sema to wake up bulk out handler*/
                        rtw_up_sema(&(mscdev->bulk_out_cmd_task.wakeup_sema));

                        if (bufhd->type == BUFHD_DATA) {
                            usbd_msc_put_bufhd(common, bufhd);
                        }
                    }
		#endif
                }
            }

            break;

        case -USB_ECONNRESET:       /* unlink, software-driven interface shutdown */
            USBD_MSC_ERROR("ECONNRESET");
            break;

        case -USB_ESHUTDOWN:        /* disconnect etc */
            USBD_MSC_ERROR("ESHUTDOWN");
            break;

        case -USB_ECONNABORTED:     /* endpoint reset */
            USBD_MSC_ERROR("ECONNABORTED");
            break;

        case -USB_EOVERFLOW:
            USBD_MSC_ERROR("EOVERFLOW");
            break;

        default:
            USBD_MSC_ERROR("tx status %d", status);
            break;
    }
}

int usbd_msc_lun_write(struct usb_msc_lun_t *curlun, u32 sector, const u8 *buffer, u32 count)
{
    int ret = 0;

    if (curlun == NULL) {
        return -1;
    }

    rtw_mutex_get(&curlun->lun_mutex);

    if (curlun->lun_opts->disk_write) {
        ret = curlun->lun_opts->disk_write(sector, buffer, count);
    }

    rtw_mutex_put(&curlun->lun_mutex);
    return ret;
}

int usbd_msc_lun_read(struct usb_msc_lun_t *curlun, u32 sector, u8 *buffer, u32 count)
{
    int ret = 0;

    if (curlun == NULL) {
        return -1;
    }

    rtw_mutex_get(&curlun->lun_mutex);

    if (curlun->lun_opts->disk_read) {
        ret = curlun->lun_opts->disk_read(sector, buffer, count);
    }

    rtw_mutex_put(&curlun->lun_mutex);
    return ret;
}


static void msc_lun_close(struct usb_msc_lun_t *curlun)
{
    if (curlun->is_open) {
        if (curlun->lun_opts->disk_deinit) {
            curlun->lun_opts->disk_deinit();
        }

        curlun->is_open = 0;
    }
}

static int msc_lun_open(struct usb_msc_lun_t *curlun)
{
    int     ro = 0;
    u32     num_sectors = 0;
    u32     blkbits;
    u32     blksize;

    /* init hard disk */
    if (curlun->lun_opts->disk_init) {
        if (curlun->lun_opts->disk_init()) {
            return -USB_ENOMEDIUM;
        }

        curlun->is_open = 1;
    } else {
        return -USB_ENOMEDIUM;
    }

    /* get disk capacity */
    if (curlun->lun_opts->disk_getcapacity) {
        curlun->lun_opts->disk_getcapacity(&num_sectors);
    }

    blksize = USBD_MSC_BLK_SIZE;
    blkbits = USBD_MSC_BLK_BITS;
    curlun->blksize = blksize;
    curlun->blkbits = blkbits;
    curlun->ro = ro;
    curlun->num_sectors = num_sectors;
    return 0;
}

static int msc_create_lun(struct usb_msc_common_t *common, unsigned int id)
{
    struct usb_msc_lun_t *lun;
    int status;
    
    lun = (struct usb_msc_lun_t *)msc_malloc(sizeof(struct usb_msc_lun_t));
    if (!lun) {
        USBD_MSC_ERROR("fail to malloc lun");
        return -USB_ENOMEM;
    }

    lun->cdrom = 0;
    lun->ro = 0;
    lun->initially_ro = lun->ro;
    lun->removable = 1;
    common->luns[id] = lun;
    lun->lun_opts = &disk_operation;
    rtw_mutex_init(&lun->lun_mutex);
    status = msc_lun_open(lun);
    return status;
}

static void msc_remove_luns(struct usb_msc_common_t *common, int n)
{
    int i;

    if (common->luns) {
        for (i = 0; i < n; ++i) {
            if (common->luns[i]) {
                msc_lun_close(common->luns[i]);
                rtw_mutex_free(&common->luns[i]->lun_mutex);
                msc_free(common->luns[i]);
                common->luns[i] = NULL;
            }
        }
        msc_free(common->luns);
        common->luns = NULL;
    }
}

static int msc_create_luns(struct usb_msc_common_t *common)
{
    struct usb_msc_lun_t **luns;
    int i;
    int status;
    
    luns = (struct usb_msc_lun_t **)msc_malloc(common->nluns * sizeof(struct usb_msc_lun_t *));
    if (!luns) {
        USBD_MSC_ERROR("fail to malloc luns");
        return -USB_ENOMEM;
    }

    common->luns = luns;

    for (i = 0; i < common->nluns; ++i) {
        status = msc_create_lun(common, i);
        if (status) {
            goto err;
        }
    }

    return 0;
err:
    msc_remove_luns(common, i);
    return status;
}

void usbd_msc_put_bufhd(struct usb_msc_common_t *common, struct usb_msc_bufhd_t *bufhd)
{
    rtw_mutex_get(&common->bufhd_mutex);
    if (common->nbufhd_a < common->nbufhd)
    {
        rtw_list_insert_tail(&bufhd->list, &common->bufhd_pool);
        common->nbufhd_a++;
    }
    rtw_mutex_put(&common->bufhd_mutex);
}

struct usb_msc_bufhd_t *usbd_msc_get_bufhd(struct usb_msc_common_t *common)
{
    struct usb_msc_bufhd_t *bufhd = NULL;
    rtw_mutex_get(&common->bufhd_mutex);

    while (rtw_is_list_empty(&common->bufhd_pool) == TRUE) {
        rtw_mutex_put(&common->bufhd_mutex);
        return NULL;
    }

    bufhd = list_first_entry(&common->bufhd_pool, struct usb_msc_bufhd_t, list);
    rtw_list_delete(&bufhd->list);
    common->nbufhd_a--;
    rtw_mutex_put(&common->bufhd_mutex);
    
    return bufhd;
}

static struct usb_msc_bufhd_t *msc_alloc_bufhd(struct usb_msc_common_t *common, int bufsize, usb_msc_bufhd_type type)
{
    struct usb_msc_bufhd_t *bufhd = NULL;
    UNUSED(common);
    bufhd = (struct usb_msc_bufhd_t *)msc_malloc(sizeof(struct usb_msc_bufhd_t));

    if (bufhd) {
        if (type != BUFHD_DATA) {
            bufhd->buf = msc_malloc(bufsize);
            if (bufhd->buf == NULL) {
                msc_free((void *)bufhd);
                USBD_MSC_ERROR("fail to malloc bufhd->buf, size %d", bufsize);
                goto fail;
            }
        } else {
            // for disk_write/read, buf addr should be 32byte align to increase write/read speed
            bufhd->prebuf = msc_malloc(bufsize + 0x1F);
            if (bufhd->prebuf == NULL) {
                msc_free((void *)bufhd);
                USBD_MSC_ERROR("fail to malloc bufhd->prebuf, size %d", bufsize);
                goto fail;
            }

            bufhd->buf = (u8 *)_RND((u32)bufhd->prebuf, 32); //buf 32 byte align
        }

        bufhd->buf_size = bufsize;
    }

    bufhd->type = type;
    return bufhd;
fail:
    return NULL;
}

static void msc_free_bufhds(struct usb_msc_common_t *common)
{
    struct usb_msc_bufhd_t *bufhd = NULL;

    /* free bufhd for cwb*/
    if (common->cbw_bh) {
        bufhd = common->cbw_bh;

        if (bufhd->reqout) {
            usb_ep_free_request(common->mscdev->out_ep, bufhd->reqout);
            bufhd->reqout = NULL;
        }

        if (bufhd->buf) {
            msc_free((void *)bufhd->buf);
            bufhd->buf = NULL;
        }
        
        msc_free((void *)common->cbw_bh);
        common->cbw_bh = NULL;
    }

    /* free bufhd for csb*/
    if (common->csw_bh) {
        bufhd = common->csw_bh;
        
        if (bufhd->reqin) {
            usb_ep_free_request(common->mscdev->in_ep, bufhd->reqin);
            bufhd->reqin = NULL;
        }
        
        if (bufhd->buf) {
            msc_free((void *)bufhd->buf);
            bufhd->buf = NULL;
        }
        
        msc_free((void *)common->csw_bh);
        common->csw_bh = NULL;
    }

    /* free bufhd for data*/
    while (!rtw_is_list_empty(&common->bufhd_pool)) {
        rtw_mutex_get(&common->bufhd_mutex);
        bufhd = list_first_entry(&common->bufhd_pool, struct usb_msc_bufhd_t, list);
        rtw_list_delete(&bufhd->list);
        rtw_mutex_put(&common->bufhd_mutex);

        if (bufhd->reqin) {
            usb_ep_free_request(common->mscdev->in_ep, bufhd->reqin);
            bufhd->reqin = NULL;
        }

        if (bufhd->reqout) {
            usb_ep_free_request(common->mscdev->out_ep, bufhd->reqout);
            bufhd->reqout = NULL;
        }

        if (bufhd->prebuf) {
            msc_free((void *)bufhd->prebuf);
            bufhd->prebuf = NULL;
        }
        msc_free((void *)bufhd);
        if (common->nbufhd_a > 0) {
            common->nbufhd_a--;
        }
    }
}

static int msc_alloc_bufhds(struct usb_msc_common_t *common)
{
    struct usb_msc_bufhd_t *bufhd = NULL;
    int i, rc;
    /* malloc bufhd for cwb*/
    bufhd = msc_alloc_bufhd(common, 512, BUFHD_CBW);	//US_BULK_CB_WRAP_LEN, use 512 for msc cv test long length command
    if (bufhd) {
        common->cbw_bh = bufhd;
    } else {
        USBD_MSC_ERROR("fail to malloc bufhd cbw");
        rc = -USB_ENOMEM;
        goto fail;
    }

    /* malloc bufhd for csb*/
    bufhd = msc_alloc_bufhd(common, US_BULK_CS_WRAP_LEN, BUFHD_CSW);
    if (bufhd) {
        common->csw_bh = bufhd;
    } else {
        USBD_MSC_ERROR("fail to malloc bufhd csw");
        rc = -USB_ENOMEM;
        goto fail;
    }

    /* malloc bufhd for bulk data stage*/
    for (i = 0; i < common->nbufhd; i++) {
        bufhd = msc_alloc_bufhd(common, USBD_MSC_BUFLEN, BUFHD_DATA);

        if (bufhd) {
            /* add bufhd to list*/
            rtw_init_listhead(&bufhd->list);
            rtw_list_insert_tail(&bufhd->list, &common->bufhd_pool);
        } else {
            USBD_MSC_ERROR("fail to malloc bufhd(No.%d)", i + 1);
            rc = -USB_ENOMEM;
            goto fail;
        }
    }

    common->nbufhd_a = common->nbufhd;
    return 0;
fail:
    msc_free_bufhds(common);
    return rc;
}

static int msc_fun_set_alt(struct usb_function *f, unsigned interface, unsigned alt)
{
    struct usb_msc_dev_t *mscdev = &msc_dev;
    struct usb_msc_common_t *common = mscdev->common;
    struct usb_msc_bufhd_t *bufhd;
    struct usb_request  *req;
    int status = 0;
    
    UNUSED(f);
    UNUSED(interface);
    UNUSED(alt);

    USBD_MSC_ENTER;

    if (common->running) {
        /* deallocate the request */
        list_for_each_entry(bufhd, &common->bufhd_pool, list, struct usb_msc_bufhd_t) {
            if (bufhd->reqin) {
                usb_ep_free_request(mscdev->in_ep, bufhd->reqin);
                bufhd->reqin = NULL;
            }

            if (bufhd->reqout) {
                usb_ep_free_request(mscdev->out_ep, bufhd->reqout);
                bufhd->reqout = NULL;
            }
        }

        bufhd = common->csw_bh;
        if (bufhd && bufhd->reqin) {
            usb_ep_free_request(mscdev->in_ep, bufhd->reqin);
            bufhd->reqin = NULL;
        }

        bufhd = common->cbw_bh;
        if (bufhd && bufhd->reqout) {
            usb_ep_free_request(mscdev->out_ep, bufhd->reqout);
            bufhd->reqout = NULL;
        }

        /* Disable the endpoints */
        if (mscdev->bulk_in_enabled) {
            usb_ep_disable(mscdev->in_ep);
            mscdev->in_ep->driver_data = NULL;
            mscdev->bulk_in_enabled = 0;
        }

        if (mscdev->bulk_out_enabled) {
            usb_ep_disable(mscdev->out_ep);
            mscdev->out_ep->driver_data = NULL;
            mscdev->bulk_out_enabled = 0;
        }

        common->mscdev = NULL;
        common->running = 0;
    }

    common->mscdev = mscdev;

    // enable bulk in endpoint
    if (mscdev->in_ep != NULL) {
        /* restart endpoint */
        if (mscdev->in_ep->driver_data != NULL) {
            usb_ep_disable(mscdev->in_ep);
        }

        /*
         * assign an corresponding endpoint descriptor to an endpoint
         * according to the gadget speed
         */
        mscdev->in_ep->desc = usb_get_descriptor(common->gadget, &msc_source_desc_HS, &msc_source_desc_FS);
        mscdev->in_ep->driver_data = mscdev;
        
        /*malloc request for CSW*/
        req = usb_ep_alloc_request(mscdev->in_ep, 1);
        if (!req) {
            USBD_MSC_ERROR("fail to malloc bufhd->reqin");
            goto fail;
        }

        bufhd = common->csw_bh;
        bufhd->reqin = req;
        bufhd->type = BUFHD_CSW;
        req->buf = bufhd->buf;
        req->dma = (dma_addr_t)bufhd->buf;
        req->context = bufhd;
        req->complete = (usb_req_complete_t)msc_bulk_in_complete;
        
        status = usb_ep_enable(mscdev->in_ep, mscdev->in_ep->desc);
        if (status < 0) {
            USBD_MSC_ERROR("fail to enable IN endpoint");
            goto fail;
        }
        
        mscdev->bulk_in_enabled = 1;
    }

    // enable bulk out endpoint and alloc requset to recieve data from host
    if (mscdev->out_ep != NULL) {
        /* restart endpoint */
        if (mscdev->out_ep->driver_data != NULL) {
            usb_ep_disable(mscdev->out_ep);
        }

        /*
        * assign an corresponding endpoint descriptor to an endpoint
        * according to the gadget speed
        */
        mscdev->out_ep->desc = usb_get_descriptor(common->gadget, &msc_sink_desc_HS, &msc_sink_desc_FS);
        mscdev->out_ep->driver_data = mscdev;
        
        /*malloc request for CBW*/
        req = usb_ep_alloc_request(mscdev->out_ep, 1);
        if (!req) {
            USBD_MSC_ERROR("fail to malloc bufhd->reqin");
            goto fail;
        }

        bufhd = common->cbw_bh;
        bufhd->reqout = req;
        bufhd->type = BUFHD_CBW;
        req->buf = bufhd->buf;
        req->dma = (dma_addr_t)bufhd->buf;
        req->context = bufhd;
        req->complete = (usb_req_complete_t)msc_bulk_out_complete;
        
        status = usb_ep_enable(mscdev->out_ep, mscdev->out_ep->desc);
        if (status < 0) {
            USBD_MSC_ERROR("fail to enable out endpoint");
            goto fail;
        }
        mscdev->bulk_out_enabled = 1;
    }

    /* malloc request for data*/
    list_for_each_entry(bufhd, &common->bufhd_pool, list, struct usb_msc_bufhd_t) {
        bufhd->reqin = usb_ep_alloc_request(mscdev->in_ep, 1);
        if (!bufhd->reqin) {
            USBD_MSC_ERROR("fail to malloc bufhd->reqin");
            goto fail;
        }

        bufhd->reqout = usb_ep_alloc_request(mscdev->out_ep, 1);
        if (!bufhd->reqout) {
            USBD_MSC_ERROR("fail to malloc bufhd->reqout");
            goto fail;
        }

        bufhd->reqin->buf = bufhd->reqout->buf = bufhd->buf;
        bufhd->reqin->dma = bufhd->reqout->dma = (dma_addr_t)bufhd->buf;
        bufhd->reqin->context = bufhd->reqout->context = bufhd;
        bufhd->reqin->complete = (usb_req_complete_t)msc_bulk_in_complete;
        bufhd->reqout->complete = (usb_req_complete_t)msc_bulk_out_complete;
        bufhd->type = BUFHD_DATA;
    }

    common->running = 1;

    if ((mscdev->out_ep != NULL) && (common->cbw_bh != NULL)) {
        /* enqueue CBW request to receive SCSI command from host*/
        req = common->cbw_bh->reqout;
        if (req != NULL) {
            req->length = US_BULK_CB_WRAP_LEN;
            status = usb_ep_queue(mscdev->out_ep, req, 1);
            if (status) {
                USBD_MSC_ERROR("fail to enqueue out endpoint");
                goto fail;
            }
        }
    }
    
    USBD_MSC_EXIT;
    
    return 0;
    
fail:
    USBD_MSC_EXIT_ERR;
    return status;
}

static void msc_fun_disable(struct usb_function *f)
{
    struct usb_msc_dev_t *mscdev = &msc_dev;
    
    USBD_MSC_ENTER;

    UNUSED(f);
    
    if (mscdev->bulk_in_enabled) {
        usb_ep_disable(mscdev->in_ep);
        mscdev->in_ep->driver_data = NULL;
        mscdev->bulk_in_enabled = 0;
    }

    if (mscdev->bulk_out_enabled) {
        usb_ep_disable(mscdev->out_ep);
        mscdev->out_ep->driver_data = NULL;
        mscdev->bulk_out_enabled = 0;
    }
        
    USBD_MSC_EXIT;
}

static int msc_fun_bind(struct usb_configuration *c, struct usb_function *f)
{
    struct usb_msc_dev_t *mscdev = &msc_dev;
    struct usb_ep *ep;
    int status = -USB_ENODEV;
    int id;

    USBD_MSC_ENTER;

    if (rtw_create_task(&mscdev->bulk_out_cmd_task, "MSC_BULK_CMD",
            USBD_MSC_BULK_CMD_TASK_STACK_SIZE, USBD_MSC_BULK_CMD_THREAD_PRIORITY, msc_bulk_cmd_handler, (void *)mscdev) != pdPASS) {
        USBD_MSC_ERROR("fail to create thread MSC_BULK_CMD");
        goto fail;
    }

    /* allocate instance-specific interface IDs, and patch descriptors */
    id = usb_interface_id(c, f); // this will return the allocated interface id
    if (id < 0) {
        status = id;
        goto fail;
    }

    msc_intf_desc.bInterfaceNumber = id;
    mscdev->interface_number = id; // record interface number
    
    /* search endpoint according to endpoint descriptor and modify endpoint address*/
    ep = usb_ep_autoconfig(c->cdev->gadget, &msc_source_desc_FS);
    if (!ep) {
        goto fail;
    }

    ep->driver_data = mscdev;/* claim */
    mscdev->in_ep = ep;
    ep = usb_ep_autoconfig(c->cdev->gadget, &msc_sink_desc_FS);
    if (!ep) {
        goto fail;
    }

    ep->driver_data = mscdev;/* claim */
    mscdev->out_ep = ep;
    msc_source_desc_HS.bEndpointAddress = msc_source_desc_FS.bEndpointAddress;
    msc_sink_desc_HS.bEndpointAddress = msc_sink_desc_FS.bEndpointAddress;

    status = usb_assign_descriptors(f, msc_descriptors_FS, msc_descriptors_HS, NULL);
    if (status) {
        goto fail;
    }

    USBD_MSC_EXIT;

    return 0;
    
fail:
    
    usb_free_all_descriptors(f);

    if (mscdev->bulk_out_cmd_task.task != NULL) {
        rtw_delete_task(&mscdev->bulk_out_cmd_task);
    }

    USBD_MSC_EXIT_ERR;

    return status;
}

static void msc_fun_unbind(struct usb_configuration *c, struct usb_function *f)
{
    struct usb_msc_dev_t *mscdev = &msc_dev;
    
    UNUSED(c);
    
    USBD_MSC_ENTER;

    if (mscdev->bulk_out_cmd_task.task != NULL) {
        rtw_delete_task(&mscdev->bulk_out_cmd_task);
    }

    usb_free_all_descriptors(f);

    USBD_MSC_EXIT;
}

static int msc_fun_setup(struct usb_function *fun, const struct usb_control_request *ctrl)
{
    struct usb_msc_dev_t *mscdev = &msc_dev;
    struct usb_composite_dev *cdev = fun->config->cdev;
    struct usb_request *req0 = cdev->req;
    struct usb_ep *ep0 = mscdev->common->gadget->ep0;
    u16 wLength = ctrl->wLength;
    u16 wIndex = ctrl->wIndex;
    u16 wValue = ctrl->wValue;
    int value = -USB_EOPNOTSUPP;
    req0->context = NULL;
    req0->length = 0;

    USBD_MSC_ENTER;

    if (mscdev->common->running) {
        switch (ctrl->bRequest) {
            case USBD_MSC_REQUEST_RESET:
                USBD_MSC_INFO("USBD_MSC_REQUEST_RESET\n");
                if ((ctrl->bmRequestType == USB_REQ_TYPE_WRITE_CLASS_INTERFACE) &&
                    (wIndex == mscdev->interface_number) &&
                    (wValue == 0) &&
                    (wLength == 0)) {
                    mscdev->bot_status = USBD_BOT_STATUS_RECOVERY;
                    value = USB_GADGET_DELAYED_STATUS;
                }
                break;
            case USBD_MSC_REQUEST_GET_MAX_LUN:
                USBD_MSC_INFO("USBD_MSC_REQUEST_GET_MAX_LUN\n");
                if ((ctrl->bmRequestType == USB_REQ_TYPE_READ_CLASS_INTERFACE) &&
                    (wIndex == mscdev->interface_number) &&
                    (wValue == 0) &&
                    (wLength == 1)) {
                    *(u8 *)req0->buf = mscdev->common->nluns - 1;
                    req0->length = 1;
                    value = usb_ep_queue(ep0, req0, 1);
                    if (value != 0) {
                        /* We can't do much more than wait for a reset */
                        USBD_MSC_WARN("EP0 request queue fail: %d", value);
                    }
                }
                break;
            default:
                break;
        }
    }

    USBD_MSC_EXIT;

    return value;
}

static int msc_fun_clr_feature(struct usb_function *fun, const struct usb_control_request *ctrl)
{
	struct usb_msc_dev_t *mscdev = &msc_dev;
	struct usb_msc_common_t *common = mscdev->common;
	struct usb_msc_lun_t  *curlun = common->curlun;
	int value = -1;
	UNUSED(fun);
	struct usb_request  *req;

	USBD_MSC_ENTER;
	if(USB_ENDPOINT_GET_DIR(ctrl->wIndex) == USB_DIR_OUT) {
		value = usb_ep_disable(mscdev->out_ep);
		if (value < 0) {
			USBD_MSC_ERROR("fail to disable out endpoint");
	        }

		value = usb_ep_enable(mscdev->out_ep, mscdev->out_ep->desc);
	        if (value < 0) {
			USBD_MSC_ERROR("fail to enable out endpoint");
	        }

		req = common->cbw_bh->reqout;
	        if (req != NULL) {
			req->length = US_BULK_CB_WRAP_LEN;
			value = usb_ep_queue(mscdev->out_ep, req, 1);
			if (value) {
				USBD_MSC_ERROR("fail to enqueue out endpoint");
			}
	        }
	}

	if(mscdev->bot_status == USBD_BOT_STATUS_ERROR)/* Bad CBW Signature */
	{
		value = usbd_msc_halt_bulk_in_endpoint(mscdev);
		mscdev->bot_status = USBD_BOT_STATUS_NORMAL;
	} else {
		return -1;
	}

	USBD_MSC_EXIT;

	return value;
}

static void msc_fun_free(struct usb_function *f)
{
    struct usb_msc_dev_t *mscdev = &msc_dev;
    struct usb_msc_common_t *common = mscdev->common;
    
    USBD_MSC_ENTER;
    
    if (mscdev->bulk_out_cmd_task.task != NULL) {
        rtw_delete_task(&mscdev->bulk_out_cmd_task);
    }
    
#if defined (CONFIG_PLATFORM_8721D)
    rtw_free_sema(&mscdev->bulk_out_cmd_task.wakeup_sema);
    rtw_free_sema(&mscdev->bulk_out_cmd_task.terminate_sema);
#endif

    usb_free_all_descriptors(f);

    msc_remove_luns(common, common->nluns);
    msc_free_bufhds(common);
    rtw_mutex_free(&common->bod_mutex);
    rtw_mutex_free(&common->boc_mutex);
    rtw_mutex_free(&common->bufhd_mutex);

    USBD_MSC_EXIT;
}

#if USBD_MSC_RAM_DISK

static SD_RESULT RAM_init(void)
{
    SD_RESULT result = SD_OK;
    usbd_msc_ram_disk_buf = (u8 *)msc_malloc(USBD_MSC_RAM_DISK_SIZE);
    if (usbd_msc_ram_disk_buf == NULL) {
        USBD_MSC_ERROR("fail to allocate RAM disk buffer");
        result = SD_NODISK;
    }
    return result;
}

static SD_RESULT RAM_deinit(void)
{
    if (usbd_msc_ram_disk_buf != NULL) {
        msc_free((void *)usbd_msc_ram_disk_buf);
        usbd_msc_ram_disk_buf = NULL;
    }
    return SD_OK;
}

static SD_RESULT RAM_GetCapacity(u32* sector_count)
{
    *sector_count = USBD_MSC_RAM_DISK_SECTORS;
    return SD_OK;
}

static SD_RESULT RAM_ReadBlocks(u32 sector,u8 *data,u32 count)
{
    SD_RESULT result = SD_ERROR;
    if (sector + count <= USBD_MSC_RAM_DISK_SECTORS) {
        rtw_memcpy((void *)data, (void *)(usbd_msc_ram_disk_buf + sector * USBD_MSC_BLK_SIZE), count * USBD_MSC_BLK_SIZE);
        result = SD_OK;
    }
    return result;
}

static SD_RESULT RAM_WriteBlocks(u32 sector,const u8 *data,u32 count)
{
    SD_RESULT result = SD_ERROR;
    if (sector + count <= USBD_MSC_RAM_DISK_SECTORS) {
        rtw_memcpy((void *)(usbd_msc_ram_disk_buf + sector * USBD_MSC_BLK_SIZE), (void *)data, count * USBD_MSC_BLK_SIZE);
        result = SD_OK;
    }
    return result;
}

#endif // USBD_MSC_RAM_DISK

static int msc_config(struct usb_configuration *c)
{
    struct usb_msc_dev_t *mscdev;
    struct usb_msc_common_t *common;
    int status = 0;
    
    USBD_MSC_ENTER;
    
    common = &msc_common;
    common->gadget = c->cdev->gadget;
    common->nluns = USBD_MSC_MAX_LUN;
    common->nbufhd = USBD_MSC_NBR_BUFHD;
    common->can_stall = 0;
    common->running = 0;
        
    mscdev = &msc_dev;
    mscdev->common = common;
    mscdev->bot_status = USBD_BOT_STATUS_NORMAL;

#if defined (CONFIG_PLATFORM_8721D)
    // rtw_create_task in Ameba1 has semaphore and initial inside,
    // but AmebaPro(AmebaD) didn't have it, so we create semaphore by ourselves
    rtw_init_sema(&mscdev->bulk_out_cmd_task.wakeup_sema, 0);
    rtw_init_sema(&mscdev->bulk_out_cmd_task.terminate_sema, 0);
#endif    
    
    rtw_init_listhead(&common->bufhd_pool);
    rtw_mutex_init(&common->bufhd_mutex);
    rtw_init_listhead(&common->boc_list);
    rtw_mutex_init(&common->boc_mutex);
    rtw_init_listhead(&common->bod_list);
    rtw_mutex_init(&common->bod_mutex);
    
    /* alloc buffer header */
    status = msc_alloc_bufhds(common);
    if (status) {
        USBD_MSC_ERROR("fail to malloc buffer header");
        msc_free_bufhds(mscdev->common);
        goto msc_config_fail;
    }

    /* create logic unit */
    status = msc_create_luns(common);
    if (status < 0) {
        msc_remove_luns(common, common->nluns);
        msc_free_bufhds(mscdev->common);
        goto msc_config_fail;
    }

    USBD_MSC_EXIT;
    return USB_ESUCCESS;

msc_config_fail:
    
    rtw_mutex_free(&common->bod_mutex);
    rtw_mutex_free(&common->boc_mutex);
    rtw_mutex_free(&common->bufhd_mutex);
    
    USBD_MSC_EXIT_ERR;
    
    return status;
}

static struct usb_function msc_funcions = {
    .bind      = msc_fun_bind,
    .unbind    = msc_fun_unbind,
    .setup     = msc_fun_setup,
    .set_alt   = msc_fun_set_alt,
    .disable   = msc_fun_disable,
    .free_func = msc_fun_free,
    .clr_feature     = msc_fun_clr_feature,
};

static struct usb_composite_driver msc_driver = {
	.dev_desc	 = &msc_device_desc,
	.config_desc = &msc_config_desc,
	.strings 	 = msc_gadget_strings,
	.functions   = &msc_funcions,
	.driver_data = &msc_dev,
	.config      = msc_config,
};

int usbd_msc_init(void)
{
    int status = 0;
    
    USBD_MSC_ENTER;

#if USBD_MSC_RAM_DISK
    disk_operation.disk_init = RAM_init;
    disk_operation.disk_deinit = RAM_deinit;
    disk_operation.disk_getcapacity = RAM_GetCapacity;
    disk_operation.disk_read = RAM_ReadBlocks;
    disk_operation.disk_write = RAM_WriteBlocks;
#else
    disk_operation.disk_init = SD_Init;
    disk_operation.disk_deinit = SD_DeInit;
    disk_operation.disk_getcapacity = SD_GetCapacity;
    disk_operation.disk_read = SD_ReadBlocks;
    disk_operation.disk_write = SD_WriteBlocks;
#endif

    status = usb_composite_register(&msc_driver);
    if (status != 0) {
        USBD_MSC_ERROR("Register composite driver fail");
        usb_composite_unregister(&msc_driver);
        goto usbd_msc_init_fail;
    }
    
    USBD_MSC_EXIT;
    return USB_ESUCCESS;
    
usbd_msc_init_fail:
    USBD_MSC_EXIT_ERR;
    return status;
}

void usbd_msc_deinit(void)
{
    USBD_MSC_ENTER;
    
	usb_composite_unregister(&msc_driver);
    
    USBD_MSC_EXIT;
}

#endif // CONFIG_USBD_MSC
