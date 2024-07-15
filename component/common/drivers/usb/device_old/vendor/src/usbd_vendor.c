#include <platform_opts.h>
#ifdef CONFIG_USBD_VENDOR
#include "usbd_vendor.h"
#include "osdep_service.h"

static int vendor_iso_out_complete_cnt = 0;
static usb_vendor_iso_out_record_t *vendor_iso_out_record;
static void vendor_dump_iso_out_record(void);

static struct usb_vendor_device_t vendor_device;
static struct usb_vendor_common_t vendor_common;

static u32 vendor_iso_in_count = 0;
static u32 vendor_iso_in_value = 0;

#if CONFIG_USBD_VENDOR_BULK_INOUT_TEST
static void vendor_free_bulk_request(struct usb_ep *ep, struct usb_request *req)
{
    if ((ep != NULL) && (req != NULL)) {
        if (req->buf != NULL) {
            rtw_free(req->buf);
            req->buf = NULL;
        }

        usb_ep_free_request(ep, req);
    }
}

static void vendor_bulk_in_complete(struct usb_ep *ep, struct usb_request *req)
{
    UNUSED(ep);
    UNUSED(req);
}

static void vendor_bulk_out_complete(struct usb_ep *ep, struct usb_request *req)
{
    int ret;
    int status = req->status;
    struct usb_vendor_device_t *vdev = &vendor_device;
    u8 *val = NULL;
    UNUSED(ep);

    switch (status) {
        case USB_ESUCCESS:
            val = (u8 *)req->buf;
            USBD_VENDOR_INFO("out buffer length:%d, buf[0]:%d\n", req->length, val[0]);
	    
	    ret = usb_ep_queue(vdev->bulk_in_ep, vdev->bulk_in_req, 1);
	    if (ret < 0) {
	        USBD_VENDOR_ERROR("Fail to queue bulk in request\n");
	    }

            break;

        case -USB_ESHUTDOWN:
            USBD_VENDOR_WARN("Bulk out shutdown\n");
            break;

        default:
            USBD_VENDOR_WARN("Bulk out failed: %d\n", status);
            break;
    }
}
#endif // CONFIG_USBD_VENDOR_BULK_INOUT_TEST

#if CONFIG_USBD_VENDOR_ISO_IN_TEST
static void usbd_vendor_iso_in_complete(struct usb_ep *ep, struct usb_iso_request *req)
{
    int status = req->status;
    u8 *mem;
    
    UNUSED(ep);

    /*check request status*/
    switch (status) {
        case USB_ESUCCESS:             /* tx seccussfuly*/
            break;

        /* FALLTHROUGH */
        case -USB_ECONNRESET:   /* unlink */
            USBD_VENDOR_ERROR("ECONNRESET\n");

        case -USB_ESHUTDOWN:    /* disconnect etc */
            USBD_VENDOR_ERROR("ESHUTDOWN\n");
            break;

        default:
            USBD_VENDOR_ERROR("ISO complete with error: %d\n", status);
    }

    vendor_iso_in_count++;

    if (vendor_iso_in_count >= USBD_VENDOR_ISO_IN_CNT) {
        USBD_VENDOR_INFO("USBD ISO IN DONE\n");
        vendor_iso_in_count = 0;
    }

    if (req->proc_buf_num == 0) {
        mem = (u8 *)(req->buf0);
    } else {
        mem = (u8 *)(req->buf1);
    }

    rtw_memset(mem, vendor_iso_in_value, 512);
    vendor_iso_in_value++;
    vendor_iso_in_value %= 256;
}
#endif // CONFIG_USBD_VENDOR_ISO_IN_TEST

#if CONFIG_USBD_VENDOR_ISO_OUT_TEST
static void usbd_vendor_iso_out_complete(struct usb_ep *ep, struct usb_iso_request *req)
{
    u8 *buf;
    UNUSED(ep);

    if (req->proc_buf_num == 0) {
        buf = (u8 *)(req->buf0);
        vendor_iso_out_record[vendor_iso_out_complete_cnt].pos = 0;
        vendor_iso_out_record[vendor_iso_out_complete_cnt].data = buf[0];
    } else {
        buf = (u8 *)(req->buf1);
        vendor_iso_out_record[vendor_iso_out_complete_cnt].pos = 1;
        vendor_iso_out_record[vendor_iso_out_complete_cnt].data = buf[0];
    }

    vendor_iso_out_complete_cnt++;

    if (USBD_VENDOR_ISO_OUT_XFER_CNT == vendor_iso_out_complete_cnt) {
        vendor_dump_iso_out_record();
        vendor_iso_out_complete_cnt = 0;
    }
	
}
#endif // CONFIG_USBD_VENDOR_ISO_OUT_TEST

static int vendor_fun_set_alt(struct usb_function *f, unsigned intf, unsigned alt)
{
    struct usb_vendor_device_t *vdev = &vendor_device;
    struct usb_vendor_common_t *common = vdev->common;
    struct usb_iso_request  *isoinreq;
    struct usb_iso_request  *isooutreq;
    int status = 0;
    
    UNUSED(f);
    UNUSED(intf);
    UNUSED(alt);
    
    USBD_VENDOR_ENTER;
    
#if CONFIG_USBD_VENDOR_BULK_INOUT_TEST
 /************************************************
    * bulk test										*
    * receive data from host, when complete, send data		*
    * device send bulk in data in vendor_bulk_out_complete	*
    ***********************************************/
    struct usb_request  *req;
    int i;

    // BULK OUT
    if (vdev->bulk_out_ep != NULL) {
        u8 *bulk_out_buf = NULL;

        if (vdev->bulk_out_ep->driver_data != NULL) {
            usb_ep_disable(vdev->bulk_out_ep);
            vdev->bulk_out_ep->driver_data = NULL;
        }

        if (vdev->bulk_out_req != NULL) {
            vendor_free_bulk_request(vdev->bulk_out_ep, vdev->bulk_out_req);
            vdev->bulk_out_req = NULL;
        }

        vdev->bulk_out_ep->desc = usb_get_descriptor(common->gadget, &vendor_bulk_sink_desc_HS, &vendor_bulk_sink_desc_FS);
        
        status = usb_ep_enable(vdev->bulk_out_ep, vdev->bulk_out_ep->desc);
        if (status < 0) {
            USBD_VENDOR_ERROR("Fail to enable bulk out endpoint: %d\n", status);
            goto vendor_fun_set_alt_exit;
        }

        vdev->bulk_out_ep->driver_data = vdev;
        
        req = usb_ep_alloc_request(vdev->bulk_out_ep, 1);
        if (!req) {
            USBD_VENDOR_ERROR("Fail to allocate bulk out request\n");
            goto vendor_fun_set_alt_exit;
        }

        bulk_out_buf = rtw_zmalloc(16);
        if (bulk_out_buf == NULL) {
            USBD_VENDOR_ERROR("Fail to allocate bulk out data buffer\n");
            goto vendor_fun_set_alt_exit;
        }

        for (i = 0; i < 16; i++) {
            bulk_out_buf[i] = 0x55;
        }

        vdev->bulk_out_req= req;
        req->buf = bulk_out_buf;
        req->length = 16;
        req->complete = vendor_bulk_out_complete;
        
        status = usb_ep_queue(vdev->bulk_out_ep, req, 1);
        if (status) {
            USBD_VENDOR_ERROR("Fail to enqueue bulk out request\n");
            goto vendor_fun_set_alt_exit;
        }
    }

    // BULK IN
    if (vdev->bulk_in_ep != NULL) {
        u8 *bulk_in_buf = NULL;

        if (vdev->bulk_in_ep->driver_data != NULL) {
            usb_ep_disable(vdev->bulk_in_ep);
            vdev->bulk_in_ep->driver_data = NULL;
        }

        if (vdev->bulk_in_req != NULL) {
            vendor_free_bulk_request(vdev->bulk_in_ep, vdev->bulk_in_req);
            vdev->bulk_in_req = NULL;
        }

        vdev->bulk_in_ep->desc = usb_get_descriptor(common->gadget, &vendor_bulk_source_desc_HS, &vendor_bulk_source_desc_FS);
        
        status = usb_ep_enable(vdev->bulk_in_ep, vdev->bulk_in_ep->desc);
        if (status < 0) {
            USBD_VENDOR_ERROR("Fail to enable bulk IN endpoint: %d\n", status);
            goto vendor_fun_set_alt_exit;
        }

        vdev->bulk_in_ep->driver_data = vdev;
        
        req = usb_ep_alloc_request(vdev->bulk_in_ep, 1);
        if (!req) {
            USBD_VENDOR_ERROR("Fail to allocate bulk in request\n");
            goto vendor_fun_set_alt_exit;
        }

        bulk_in_buf = rtw_zmalloc(16);
        if (bulk_in_buf == NULL) {
            USBD_VENDOR_ERROR("Fail to allocate bulk in data buffer\n");
            goto vendor_fun_set_alt_exit;
        }

        for (i = 0; i < 16; i++) {
            bulk_in_buf[i] = 0x67;
        }

        vdev->bulk_in_req= req;
        req->buf = bulk_in_buf;
        req->length = 16;
        req->dma = (dma_addr_t)req->buf;
        req->complete = vendor_bulk_in_complete;
    }

#endif // CONFIG_USBD_VENDOR_BULK_INOUT_TEST

#if CONFIG_USBD_VENDOR_ISO_IN_TEST

    if (vdev->iso_in_ep != NULL) {
        if (vdev->iso_in_ep->driver_data != NULL) {
            iso_ep_stop(vdev->iso_in_ep, vdev->iso_in_req);	
            usb_ep_disable(vdev->iso_in_ep);
            vdev->iso_in_ep->driver_data = NULL;
        }

        if (vdev->iso_in_req != NULL) {
            free_iso_request(vdev->iso_in_ep, vdev->iso_in_req);
            vdev->iso_in_req = NULL;
        }

        vdev->iso_in_ep->desc = usb_get_descriptor(common->gadget, &vendor_iso_source_desc_HS, &vendor_iso_source_desc_FS);
        
        status = usb_ep_enable(vdev->iso_in_ep, vdev->iso_in_ep->desc);
        if (status < 0) {
            USBD_VENDOR_ERROR("Fail to enable iso in endpoint: %d\n", status);
            goto vendor_fun_set_alt_exit;
        }

        vdev->iso_in_ep->driver_data = vdev;
        
        isoinreq = alloc_iso_request(vdev->iso_in_ep, 1, 1);
        if (!isoinreq) {
            USBD_VENDOR_ERROR("Fail to allocate iso in request\n");
            goto vendor_fun_set_alt_exit;
        }

        isoinreq->buf0 = vdev->iso_in_req_buf[0];
        isoinreq->buf1 = vdev->iso_in_req_buf[1];
        isoinreq->dma0 = (dma_addr_t)isoinreq->buf0;
        isoinreq->dma1 = (dma_addr_t)isoinreq->buf1;
        isoinreq->buf_proc_intrvl = (1 << (vdev->iso_in_ep->desc->bInterval - 1));
        isoinreq->zero = 0;
        isoinreq->sync_frame = 0;
        isoinreq->data_pattern_frame = 0;
        isoinreq->data_per_frame = 512;
        isoinreq->data_per_frame1 = 512;
        isoinreq->start_frame = 0;
#if CONFIG_USBD_VENDOR_ISO_IN_REQ_ASAP
        isoinreq->flags |= USB_REQ_ISO_ASAP;
#endif
        isoinreq->proc_buf_num = 0;
        isoinreq->process_buffer = usbd_vendor_iso_in_complete;
        isoinreq->context = vdev;
        
        vdev->iso_in_req = isoinreq;
        
        iso_ep_start(vdev->iso_in_ep, vdev->iso_in_req, 1);
    }

#endif // CONFIG_USBD_VENDOR_ISO_IN_TEST

#if CONFIG_USBD_VENDOR_ISO_OUT_TEST

    if (vdev->iso_out_ep != NULL) {
        if (vdev->iso_out_ep->driver_data != NULL) {
            iso_ep_stop(vdev->iso_out_ep, vdev->iso_out_req);	
            usb_ep_disable(vdev->iso_out_ep);
            vdev->iso_out_ep->driver_data = NULL;
        }

        if (vdev->iso_out_req != NULL) {
            free_iso_request(vdev->iso_out_ep, vdev->iso_out_req);
            vdev->iso_out_req = NULL;
        }

        vdev->iso_out_ep->desc = usb_get_descriptor(common->gadget, &vendor_iso_sink_desc_HS, &vendor_iso_sink_desc_FS);
        
        status = usb_ep_enable(vdev->iso_out_ep, vdev->iso_out_ep->desc);
        if (status < 0) {
            USBD_VENDOR_ERROR("Fail to enable iso out endpoint: %d\n", status);
            goto vendor_fun_set_alt_exit;
        }

        vdev->iso_out_ep->driver_data = vdev;
        
        isooutreq = alloc_iso_request(vdev->iso_out_ep, 1, 1);
        if (!isooutreq) {
            USBD_VENDOR_ERROR("Fail to allocate iso out request\n");
            goto vendor_fun_set_alt_exit;
        }

        isooutreq->buf0 = vdev->iso_out_req_buf[0];
        isooutreq->buf1 = vdev->iso_out_req_buf[1];
        isooutreq->dma0 = (dma_addr_t)isooutreq->buf0;
        isooutreq->dma1 = (dma_addr_t)isooutreq->buf1;
        isooutreq->buf_proc_intrvl = (1 << (vdev->iso_out_ep->desc->bInterval - 1));
        isooutreq->zero = 0;
        isooutreq->sync_frame = 0;
        isooutreq->data_pattern_frame = 0;
        isooutreq->data_per_frame = 64;
        isooutreq->data_per_frame1 = 64;
        isooutreq->start_frame = 0;
#if CONFIG_USBD_VENDOR_ISO_OUT_REQ_ASAP
        isooutreq->flags |= USB_REQ_ISO_ASAP;
#endif
        isooutreq->proc_buf_num = 0;
        isooutreq->process_buffer = usbd_vendor_iso_out_complete;
        isooutreq->context = vdev;
        
        vdev->iso_out_req = isooutreq;
        
        iso_ep_start(vdev->iso_out_ep, vdev->iso_out_req, 1);
    }

#endif // CONFIG_USBD_VENDOR_ISO_OUT_TEST

vendor_fun_set_alt_exit:
    USBD_VENDOR_EXIT;
    return 0;
}

static int vendor_fun_bind(struct usb_configuration *c, struct usb_function *f)
{
    struct usb_vendor_device_t *vdev = &vendor_device;
    struct usb_ep *ep;
    int status = -USB_ENODEV;
    int id;
    
    USBD_VENDOR_ENTER;
    
    /* allocate instance-specific interface IDs, and patch descriptors */
    id = usb_interface_id(c, f); // this will return the allocated interface id
    if (id < 0) {
        status = id;
        goto vendor_fun_bind_exit;
    }

    vendor_intf0_desc.bInterfaceNumber = id;

#if 1
    vendor_intf1_desc.bInterfaceNumber = id;
#else
    id = usb_interface_id(c, f); // this will return the allocated interface id
    if (id < 0){
      status = id;
      goto vendor_fun_bind_exit;
    }
    vendor_intf1_desc.bInterfaceNumber = id;
#endif
    
    ep = usb_ep_autoconfig(c->cdev->gadget, &vendor_bulk_source_desc_FS);
    if (!ep) {
        goto vendor_fun_bind_exit;
    }

    ep->driver_data = vdev;
    vdev->bulk_in_ep = ep;
    
    ep = usb_ep_autoconfig(c->cdev->gadget, &vendor_bulk_sink_desc_FS);
    if (!ep) {
        goto vendor_fun_bind_exit;
    }

    ep->driver_data = vdev;
    vdev->bulk_out_ep = ep;
    
    ep = usb_ep_autoconfig(c->cdev->gadget, &vendor_iso_source_desc_FS);
    if (!ep) {
        goto vendor_fun_bind_exit;
    }

    ep->driver_data = vdev;
    vdev->iso_in_ep = ep;
    
    ep = usb_ep_autoconfig(c->cdev->gadget, &vendor_iso_sink_desc_FS);
    if (!ep) {
        goto vendor_fun_bind_exit;
    }

    ep->driver_data = vdev;
    vdev->iso_out_ep = ep;
    
    vendor_bulk_source_desc_HS.bEndpointAddress = vendor_bulk_source_desc_FS.bEndpointAddress;
    vendor_bulk_sink_desc_HS.bEndpointAddress = vendor_bulk_sink_desc_FS.bEndpointAddress;
    vendor_iso_source_desc_HS.bEndpointAddress = vendor_iso_source_desc_FS.bEndpointAddress;
    vendor_iso_sink_desc_HS.bEndpointAddress = vendor_iso_sink_desc_FS.bEndpointAddress;
    
    USBD_VENDOR_INFO("BULK EP address in=0x%02X out=0x%02X\n", vendor_bulk_source_desc_HS.bEndpointAddress,
        vendor_bulk_sink_desc_HS.bEndpointAddress);
    
    USBD_VENDOR_INFO("ISO EP address in=0x%02X out=0x%02X\n", vendor_iso_source_desc_HS.bEndpointAddress,
        vendor_iso_sink_desc_HS.bEndpointAddress);
    
    status = usb_assign_descriptors(f, vendor_descriptors_FS, vendor_descriptors_HS, NULL);
    if (status) {
        USBD_VENDOR_ERROR("Fail to assign descriptors: %d\n", status);
        usb_free_all_descriptors(f);
        goto vendor_fun_bind_exit;
    }

vendor_fun_bind_exit:
    USBD_VENDOR_EXIT;
    return status;
}

int vendor_fun_setup(struct usb_function *f, const struct usb_control_request *ctrl)
{
    struct usb_vendor_device_t *vdev = &vendor_device;
    struct usb_ep *ep0 = vdev->common->ep0;
    struct usb_request  *req0 = vdev->common->req0;
    int status;
    
    UNUSED(f);
    
    USBD_VENDOR_ENTER;
    
    USBD_VENDOR_INFO("Setup request type=0x%02X request=0x%02X value=0x%04X index=0x%04X length=0x%04X\n",
        ctrl->bmRequestType, ctrl->bRequest, ctrl->wValue,
        ctrl->wIndex, ctrl->wLength);
    
    req0->context = NULL;
    req0->length = ctrl->wLength;
    rtw_memcpy(req0->buf, (void *)ctrl, sizeof(struct usb_control_request));
    
    status = usb_ep_queue(ep0, req0, 1);
    if (status != 0 && status != -USB_ESHUTDOWN) {
        USBD_VENDOR_WARN("Fail to queue EP0 request: %d\n", status);
    }

    USBD_VENDOR_EXIT;
    return status;
}

void vendor_fun_disable(struct usb_function *f)
{
    struct usb_vendor_device_t *vdev = &vendor_device;
    UNUSED(f);
    
    USBD_VENDOR_ENTER;

#if CONFIG_USBD_VENDOR_BULK_INOUT_TEST
 if (vdev->bulk_out_ep != NULL) {
        if (vdev->bulk_out_ep->driver_data != NULL) {
            usb_ep_disable(vdev->bulk_out_ep);
            vdev->bulk_out_ep->driver_data = NULL;
        }
    }

 if (vdev->bulk_in_ep != NULL) {
        if (vdev->bulk_in_ep->driver_data != NULL) {
            usb_ep_disable(vdev->bulk_in_ep);
            vdev->bulk_in_ep->driver_data = NULL;
        }
    }
 #endif
 
#if CONFIG_USBD_VENDOR_ISO_OUT_TEST
    if (vdev->iso_out_ep != NULL) {
        if (vdev->iso_out_ep->driver_data != NULL) {
            iso_ep_stop(vdev->iso_out_ep, vdev->iso_out_req);
            usb_ep_disable(vdev->iso_out_ep);
            vdev->iso_out_ep->driver_data = NULL;
        }
    }
#endif

#if CONFIG_USBD_VENDOR_ISO_IN_TEST
    if (vdev->iso_in_ep != NULL) {
        if (vdev->iso_in_ep->driver_data != NULL) {
            iso_ep_stop(vdev->iso_in_ep, vdev->iso_in_req);	
            usb_ep_disable(vdev->iso_in_ep);
            vdev->iso_in_ep->driver_data = NULL;
        }
    }
#endif
    
    USBD_VENDOR_EXIT;
}

void vendor_fun_unbind(struct usb_configuration *c, struct usb_function *f)
{
    UNUSED(c);
    USBD_VENDOR_ENTER;
    usb_free_all_descriptors(f);
    USBD_VENDOR_EXIT;
}

static void vendor_fun_free(struct usb_function *f)
{
    struct usb_vendor_device_t *vdev = &vendor_device;
    UNUSED(f);
    USBD_VENDOR_ENTER;

#if CONFIG_USBD_VENDOR_BULK_INOUT_TEST
    if (vdev->bulk_out_req != NULL) {
        vendor_free_bulk_request(vdev->bulk_out_ep, vdev->bulk_out_req);
        vdev->bulk_out_req = NULL;
    }

    if (vdev->bulk_in_req != NULL) {
        vendor_free_bulk_request(vdev->bulk_in_ep, vdev->bulk_in_req);
        vdev->bulk_in_req = NULL;
    }
#endif

#if CONFIG_USBD_VENDOR_ISO_OUT_TEST
    if (vdev->iso_out_req != NULL) {
        free_iso_request(vdev->iso_out_ep, vdev->iso_out_req);
        vdev->iso_out_req = NULL;
    }
#endif

#if CONFIG_USBD_VENDOR_ISO_IN_TEST
    if (vdev->iso_in_req != NULL) {
        free_iso_request(vdev->iso_in_ep, vdev->iso_in_req);
        vdev->iso_in_req = NULL;
    }
#endif

    usb_free_all_descriptors(f);
    
    USBD_VENDOR_EXIT;
}

static void vendor_dump_iso_out_record(void)
{
    int i;

    for (i = 0; i < vendor_iso_out_complete_cnt; i++)
        USBD_VENDOR_INFO("receive: %d, expext: %d\n", vendor_iso_out_record[i].data, i);

    for (i = 0; i < vendor_iso_out_complete_cnt; i++) {
        if(vendor_iso_out_record[i].data != i) {
            USBD_VENDOR_ERROR("usbd iso out data fail\n");
            return;
        }
    }

    USBD_VENDOR_INFO("usbd iso out data success\n");
}

static int vendor_config(struct usb_configuration *c)
{
    struct usb_vendor_device_t *vdev = &vendor_device;
    struct usb_vendor_common_t *common = &vendor_common;
    USBD_VENDOR_ENTER;
    vdev->common = common;
    common->gadget = c->cdev->gadget;
    common->ep0 = c->cdev->gadget->ep0;
    common->req0 = c->cdev->req;
    USBD_VENDOR_EXIT;
    return USB_ESUCCESS;
}

static struct usb_function vendor_funcions = {
    .bind      = vendor_fun_bind,
    .unbind    = vendor_fun_unbind,
    .setup     = vendor_fun_setup,
    .set_alt   = vendor_fun_set_alt,
    .disable   = vendor_fun_disable,
    .free_func = vendor_fun_free,
};

static struct usb_composite_driver vendor_driver = {
    .dev_desc    = &vendor_device_desc,
    .strings     = vendor_gadget_strings,
    .config_desc = &vendor_config_desc,
    .functions   = &vendor_funcions,
    .driver_data = &vendor_device,
    .config      = vendor_config,
};

static void vendor_deinit_resource(void)
{
    int i;
    
    for (i = 0; i < USBD_VENDOR_ISO_REQ_NUM; i++) {
        if (vendor_device.iso_out_req_buf[i]) {
            rtw_free(vendor_device.iso_out_req_buf[i]);
            vendor_device.iso_out_req_buf[i] = NULL;
        }
        if (vendor_device.iso_in_req_buf[i]) {
            rtw_free(vendor_device.iso_in_req_buf[i]);
            vendor_device.iso_in_req_buf[i] = NULL;
        }
    }
    if (vendor_iso_out_record) {
        rtw_free(vendor_iso_out_record);
        vendor_iso_out_record = NULL;
    }
}

static int vendor_init_resource(void)
{
    int i = 0;
    u8 *buf = NULL;
    
    rtw_memset(&vendor_device, 0, sizeof(struct usb_vendor_device_t));
    rtw_memset(&vendor_common, 0, sizeof(struct usb_vendor_common_t));

    for (i = 0; i < USBD_VENDOR_ISO_REQ_NUM; i++) {
        buf = (u8 *)rtw_zmalloc(512);
        if (buf) {
            vendor_device.iso_out_req_buf[i] = buf;
        } else {
            USBD_VENDOR_ERROR("Fail to allocate iso out request buffer\n");
            goto vendor_init_resource_fail;
        }
    }

    for (i = 0; i < USBD_VENDOR_ISO_REQ_NUM; i++) {
        buf = (u8 *)rtw_zmalloc(512);
        if (buf) {
            vendor_device.iso_in_req_buf[i] = buf;
            rtw_memset(vendor_device.iso_in_req_buf[i], vendor_iso_in_value, 512);
            vendor_iso_in_value++;
            vendor_iso_in_value %= 256;
        } else {
            USBD_VENDOR_ERROR("Fail to allocate iso in request buffer\n");
            goto vendor_init_resource_fail;
        }
    }

    vendor_iso_out_record = (usb_vendor_iso_out_record_t *)rtw_zmalloc(USBD_VENDOR_ISO_OUT_XFER_CNT * sizeof(
                usb_vendor_iso_out_record_t));
    if (vendor_iso_out_record == NULL) {
        USBD_VENDOR_ERROR("Fail to allocate buffer for iso out record\n");
        goto vendor_init_resource_fail;
    }

    return USB_ESUCCESS;

vendor_init_resource_fail:  
    vendor_deinit_resource();
    return -USB_EINVAL;;
}

int usbd_vendor_init(void)
{
    int status = -USB_EINVAL;
    
    USBD_VENDOR_ENTER;
    
    vendor_init_resource();

    status = usb_composite_register(&vendor_driver);
    if (status != 0) {
        USBD_VENDOR_ERROR("Fail to register composite driver");
        usb_composite_unregister(&vendor_driver);
        goto usbd_vendor_init_fail;
    }

    USBD_VENDOR_EXIT;
    return USB_ESUCCESS;
    
usbd_vendor_init_fail:  
    USBD_VENDOR_EXIT_ERR;
    return status;
}

void usbd_vendor_deinit(void)
{
    USBD_VENDOR_ENTER;

    vendor_deinit_resource();
    
    usb_composite_unregister(&vendor_driver);
    
    USBD_VENDOR_EXIT;
}

#endif // CONFIG_USBD_VENDOR

