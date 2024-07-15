#include <platform_opts.h>

#ifdef CONFIG_USBD_CDC_ACM

#include "usbd_cdc_acm.h"
#include "usb_composite.h"

#define USBD_CDC_ACM_STRING_MANUFACTURER        1
#define USBD_CDC_ACM_STRING_PRODUCT             2
#define USBD_CDC_ACM_STRING_SERIALNUMBER        3

static struct usb_string cdc_acm_strings[] = {
	{
		.id = USBD_CDC_ACM_STRING_MANUFACTURER,
		.s  = "Realtek",
	},
	{
		.id = USBD_CDC_ACM_STRING_PRODUCT,
		.s  = "USB CDC ACM Device",
	},
	{
		.id = USBD_CDC_ACM_STRING_SERIALNUMBER,
		.s  = "0123456789AB",
	},
};

static struct usb_gadget_strings cdc_acm_gadget_string_tab = {
	.language = 0x0409,
	.strings = cdc_acm_strings,
};

static struct usb_gadget_strings *cdc_acm_gadget_strings[] = {
	&cdc_acm_gadget_string_tab,
	NULL
};

// CDC ACM Device Descriptor
static struct usb_device_descriptor cdc_acm_device_desc = {
	.bLength            = USB_DT_DEVICE_SIZE,
	.bDescriptorType    = USB_DT_DEVICE,
	.bcdUSB             = 0x0200,
	.bDeviceClass       = USB_CLASS_COMM,
	.bDeviceSubClass    = 0,
	.bDeviceProtocol    = 0,
	.bMaxPacketSize0    = 64,
	.idVendor           = REALTEK_USB_VID,
	.idProduct          = REALTEK_USB_PID,
	.bcdDevice          = 0x0100,
	.iManufacturer      = USBD_CDC_ACM_STRING_MANUFACTURER,
	.iProduct           = USBD_CDC_ACM_STRING_PRODUCT,
	.iSerialNumber      = USBD_CDC_ACM_STRING_SERIALNUMBER,
	.bNumConfigurations = 1,
};

// CDC ACM Communication Interface
static struct usb_interface_descriptor cdc_acm_comm_if_desc = {
	.bLength            = USB_DT_INTERFACE_SIZE,
	.bDescriptorType    = USB_DT_INTERFACE,
	.bInterfaceNumber   = 0,                        // This will be assign automatically
	.bAlternateSetting  = 0,
#if USBD_CDC_ACM_USE_INT_IN_ENDPOINT
	.bNumEndpoints      = 1,
#else
	.bNumEndpoints      = 0,
#endif
	.bInterfaceClass    = USB_CLASS_COMM,
	.bInterfaceSubClass = USBD_CDC_SUBCLASS_ACM,    // Abstract Control Model
	.bInterfaceProtocol = USBD_CDC_ACM_PROTO_NONE,  // None
	.iInterface         = 0,
};

// CDC ACM Function Header Descriptor
static struct usb_cdc_header_desc cdc_acm_header_desc = {
	.bLength            = USBD_CDC_DT_HEADER_SIZE,
	.bDescriptorType    = USB_DT_CS_INTERFACE,
	.bDescriptorSubType = USBD_CDC_HEADER_TYPE,

	.bcdCDC             = 0x0110,
};

// CDC ACM Call Magagement Functional Descriptor
static struct usb_cdc_call_mgmt_descriptor cdc_acm_call_mgmt_desc = {
	.bLength            = USBD_CDC_DT_CALL_MGMT_SIZE,
	.bDescriptorType    = USB_DT_CS_INTERFACE,
	.bDescriptorSubType = USBD_CDC_CALL_MANAGEMENT_TYPE,

	.bmCapabilities     = 0x00,
	.bDataInterface     = 0x01,
};

// CDC ACM ACM Functional Descriptor
static struct usb_cdc_acm_descriptor cdc_acm_acm_desc = {
	.bLength            = USBD_CDC_DT_ACM_SIZE,
	.bDescriptorType    = USB_DT_CS_INTERFACE,
	.bDescriptorSubType = USBD_CDC_ACM_TYPE,

	.bmCapabilities     = 0x02,
};

// CDC ACM Union Functional Descriptor
static struct usb_cdc_union_desc cdc_acm_union_desc = {
	.bLength            = USBD_CDC_DT_UNION_SIZE,
	.bDescriptorType    = USB_DT_CS_INTERFACE,
	.bDescriptorSubType = USBD_CDC_UNION_TYPE,

	.bControlInterface  = 0,     // index of control interface
	.bSubordinateInterface0 = 1, // index of data interface
};

// CDC ACM Communication INT IN Endpoint for Low-speed/Full-speed, unused
struct usb_endpoint_descriptor usbd_cdc_acm_notify_ep_desc_FS = {
	.bLength            = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType    = USB_DT_ENDPOINT,
	.bEndpointAddress   = USBD_CDC_ACM_INT_IN_EP_ADDRESS,
	.bmAttributes       = USB_ENDPOINT_XFER_INT,
	.wMaxPacketSize     = USBD_CDC_ACM_DEFAULT_INT_IN_XFER_SIZE,
	.bInterval          = 16,

};

// CDC ACM Communication INT IN Endpoint for High-speed, unused
struct usb_endpoint_descriptor usbd_cdc_acm_notify_ep_desc_HS = {
	.bLength            = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType    = USB_DT_ENDPOINT,
	.bEndpointAddress   = USBD_CDC_ACM_INT_IN_EP_ADDRESS,
	.bmAttributes       = USB_ENDPOINT_XFER_INT,
	.wMaxPacketSize     = USBD_CDC_ACM_DEFAULT_INT_IN_XFER_SIZE,
	.bInterval          = 16,

};

// CDC ACM Data Interface
static struct usb_interface_descriptor cdc_acm_data_if_desc = {
	.bLength            = USB_DT_INTERFACE_SIZE,
	.bDescriptorType    = USB_DT_INTERFACE,

	.bInterfaceNumber   = 1,
	.bAlternateSetting  = 0,
	.bNumEndpoints      = 2,
	.bInterfaceClass    = USB_CLASS_CDC_DATA,
	.bInterfaceSubClass = 0,
	.bInterfaceProtocol = 0,
	.iInterface         = 0,
};

// CDC ACM Data Bulk IN Endpoint for Low-speed/Full-speed
static struct usb_endpoint_descriptor cdc_acm_data_bulk_in_ep_desc_FS = {
	.bLength            = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType    = USB_DT_ENDPOINT,
	.bEndpointAddress   = USBD_CDC_ACM_BULK_IN_EP_ADDRESS,
	.bmAttributes       = USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize     = USBD_CDC_ACM_FS_BULK_MAX_PACKET_SIZE,
	.bInterval          = 0,
};

// CDC ACM Data Bulk OUT Endpoint for Low-speed/Full-speed
static struct usb_endpoint_descriptor cdc_acm_data_bulk_out_ep_desc_FS = {
	.bLength            = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType    = USB_DT_ENDPOINT,
	.bEndpointAddress   = USBD_CDC_ACM_BULK_OUT_EP_ADDRESS,
	.bmAttributes       = USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize     = USBD_CDC_ACM_FS_BULK_MAX_PACKET_SIZE,
	.bInterval          = 0,
};

// CDC ACM Data Bulk IN Endpoint for High-speed
static struct usb_endpoint_descriptor cdc_acm_data_bulk_in_ep_desc_HS = {
	.bLength            = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType    = USB_DT_ENDPOINT,
	.bEndpointAddress   = USBD_CDC_ACM_BULK_IN_EP_ADDRESS,
	.bmAttributes       = USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize     = USBD_CDC_ACM_HS_BULK_MAX_PACKET_SIZE,
	.bInterval          = 0,
};

// CDC ACM Data Bulk OUT Endpoint for High-speed
static struct usb_endpoint_descriptor cdc_acm_data_bulk_out_ep_desc_HS = {
	.bLength            = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType    = USB_DT_ENDPOINT,
	.bEndpointAddress   = USBD_CDC_ACM_BULK_OUT_EP_ADDRESS,
	.bmAttributes       = USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize     = USBD_CDC_ACM_HS_BULK_MAX_PACKET_SIZE,
	.bInterval          = 0,
};

static struct usb_descriptor_header *cdc_acm_descriptors_FS[] = {
	(struct usb_descriptor_header *) &cdc_acm_comm_if_desc,
	(struct usb_descriptor_header *) &cdc_acm_header_desc,
	(struct usb_descriptor_header *) &cdc_acm_call_mgmt_desc,
	(struct usb_descriptor_header *) &cdc_acm_acm_desc,
	(struct usb_descriptor_header *) &cdc_acm_union_desc,
#if USBD_CDC_ACM_USE_INT_IN_ENDPOINT
	(struct usb_descriptor_header *) &usbd_cdc_acm_notify_ep_desc_FS,
#endif
	(struct usb_descriptor_header *) &cdc_acm_data_if_desc,
	(struct usb_descriptor_header *) &cdc_acm_data_bulk_in_ep_desc_FS,
	(struct usb_descriptor_header *) &cdc_acm_data_bulk_out_ep_desc_FS,
	NULL,
};

static struct usb_descriptor_header *cdc_acm_descriptors_HS[] = {
	(struct usb_descriptor_header *) &cdc_acm_comm_if_desc,
	(struct usb_descriptor_header *) &cdc_acm_header_desc,
	(struct usb_descriptor_header *) &cdc_acm_call_mgmt_desc,
	(struct usb_descriptor_header *) &cdc_acm_acm_desc,
	(struct usb_descriptor_header *) &cdc_acm_union_desc,
#if USBD_CDC_ACM_USE_INT_IN_ENDPOINT
	(struct usb_descriptor_header *) &usbd_cdc_acm_notify_ep_desc_HS,
#endif
	(struct usb_descriptor_header *) &cdc_acm_data_if_desc,
	(struct usb_descriptor_header *) &cdc_acm_data_bulk_in_ep_desc_HS,
	(struct usb_descriptor_header *) &cdc_acm_data_bulk_out_ep_desc_HS,
	NULL,
};

static struct usb_config_descriptor cdc_acm_config_desc = {
	.bLength             = USB_DT_CONFIG_SIZE,
	.bDescriptorType     = USB_DT_CONFIG,
	.bNumInterfaces      = 2,                  // This will be assign automatically
	.bConfigurationValue = 1,
	.iConfiguration      = 0,
	.bmAttributes	     = USB_CONFIG_ATT_ONE, // | USB_CONFIG_ATT_SELFPOWER,
	.bMaxPower           = 50,
};

static usbd_cdc_acm_line_coding_t acm_line_coding = {
	.dwDTERate   = 115200,                          // baud rate
	.bCharFormat = USBD_CDC_ACM_CHAR_FORMAT_STOP_1, // stop bit - 1
	.bParityType = USBD_CDC_ACM_PARITY_TYPE_NONE,   // parity - none
	.bDataBits   = USBD_CDC_ACM_DATA_BIT_8,         // data bits - 8
};

static usbd_cdc_acm_ctrl_line_state_t acm_ctrl_line_state = {
	.d16 = 0,
};

static struct usb_cdc_dev_t cdc_dev;
#if USBD_CDC_ACM_USE_BULK_IN_THREAD
static void *cdc_bulk_in_xfer_buf;
static u32 cdc_bulk_in_xfer_size;
#endif

static int cdc_acm_receive_data(struct usb_cdc_dev_t *dev);

static struct usb_request *cdc_get_bulk_in_request(struct usb_cdc_dev_t *dev)
{
	struct usb_request  *req = NULL;
	int retry = 0;

	while (++retry <= 500) {
		rtw_mutex_get(&dev->bulk_in_mutex);
		if (!rtw_is_list_empty(&dev->bulk_in_req_list)) {
			req = list_first_entry(&dev->bulk_in_req_list, struct usb_request, list);
			rtw_list_delete(&req->list);
			if (dev->bulk_in_req_act_num > 0) {
				dev->bulk_in_req_act_num--;
			}
			rtw_mutex_put(&dev->bulk_in_mutex);
			break;
		}
		rtw_mutex_put(&dev->bulk_in_mutex);
		rtw_mdelay_os(1);
	}

	return req;
}

static void cdc_put_bulk_in_request(struct usb_cdc_dev_t *dev, struct usb_request  *req)
{
	rtw_mutex_get(&dev->bulk_in_mutex);
	if (dev->bulk_in_req_act_num < dev->bulk_in_req_num) {
		rtw_list_insert_tail(&req->list, &dev->bulk_in_req_list);
		dev->bulk_in_req_act_num++;
	}
	rtw_mutex_put(&dev->bulk_in_mutex);
}

static struct usb_request *cdc_get_bulk_out_request(struct usb_cdc_dev_t *dev)
{
	struct usb_request  *req = NULL;
	int retry = 0;

	while (++retry <= 500) {
		rtw_mutex_get(&dev->bulk_out_mutex);
		if (!rtw_is_list_empty(&dev->bulk_out_req_list)) {
			req = list_first_entry(&dev->bulk_out_req_list, struct usb_request, list);
			rtw_list_delete(&req->list);
			if (dev->bulk_out_req_act_num > 0) {
				dev->bulk_out_req_act_num--;
			}
			rtw_mutex_put(&dev->bulk_out_mutex);
			break;
		}
		rtw_mutex_put(&dev->bulk_out_mutex);
		rtw_mdelay_os(1);
	}

	return req;
}

static void cdc_put_bulk_out_request(struct usb_cdc_dev_t *dev, struct usb_request  *req)
{
	rtw_mutex_get(&dev->bulk_out_mutex);
	if (dev->bulk_out_req_act_num < dev->bulk_out_req_num) {
		rtw_list_insert_tail(&req->list, &dev->bulk_out_req_list);
		dev->bulk_out_req_act_num++;
	}
	rtw_mutex_put(&dev->bulk_out_mutex);
}

static void cdc_ctrl_out_complete(struct usb_ep *ep, struct usb_request *req)
{
	UNUSED(ep);

	USBD_CDC_ENTER;

	if ((req->status == USB_ESUCCESS) &&
		(req->actual == USBD_CDC_ACM_LINE_CODING_SIZE) &&
		(req->direct == EP0_DATA_OUT)) {
		rtw_memcpy((void *)&acm_line_coding, req->buf, req->actual);
	}

	USBD_CDC_EXIT;
}

#if USBD_CDC_ACM_USE_INT_IN_ENDPOINT
static int acm_cdc_notify(struct usb_cdc_dev_t *dev, u8 type, u16 value,
						  void *data, unsigned length)
{
	struct usb_ep *ep = dev->int_in_ep;
	struct usb_request *req = dev->int_in_req;
	struct usb_cdc_notification	*notify;
	u32 len = sizeof(*notify) + length;
	void	*buf;
	int status;

	dev->int_in_req = NULL;
	dev->pending = 0;
	req->length = len;
	notify = (struct usb_cdc_notification *) req->buf;
	buf = notify + 1;

	notify->bmRequestType = USB_DIR_IN | USB_TYPE_CLASS | USB_RECIP_INTERFACE;
	notify->bNotificationType = type;
	notify->wValue = value;
	notify->wIndex = dev->interface_number;
	notify->wLength = length;
	_memcpy(buf, data, length);

	status = usb_ep_queue(ep, req, 1);
	if (status != USB_ESUCCESS) {
		USBD_CDC_ERROR("Fail to equeue bulk in request: %d", status);
	}

	return status;
}

static void cdc_int_in_complete(struct usb_ep *ep, struct usb_request *req)
{
	int status = req->status;
	struct usb_cdc_dev_t *dev = (struct usb_cdc_dev_t *)ep->driver_data;

	dev->int_in_req = req;

	switch (status) {
	case USB_ESUCCESS: 			/* tx successfuly*/
		if (dev->pending == 1) {
			status = acm_cdc_notify(dev, USB_CDC_NOTIFY_SERIAL_STATE, 0, &dev->serial_state, 2);
		}
		break;
	/* FALLTHROUGH */
	case -USB_ECONNRESET:		/* unlink */
		USBD_CDC_ERROR("ECONNRESET status = %d\n", status);
		break;
	case -USB_ESHUTDOWN:		/* disconnect etc */
		USBD_CDC_ERROR("ESHUTDOWN status = %d\n", status);
		break;
	default:
		USBD_CDC_ERROR("in_complete ERROR(%d)\n", status);
	}
}
#endif

static void cdc_bulk_in_complete(struct usb_ep *ep, struct usb_request *req)
{
	int status = req->status;
	struct usb_cdc_dev_t *dev = (struct usb_cdc_dev_t *)ep->driver_data;
	req->zero = 0;

	switch (status) {
	case USB_ESUCCESS:
		break;

	case -USB_ESHUTDOWN:
		USBD_CDC_WARN("Bulk in shutdown\n");
		break;

	default:
		USBD_CDC_WARN("Bulk in failed: %d\n", status);
		break;
	}

	if ((dev->cb != NULL) && (dev->cb->transmit_complete != NULL)) {
		dev->cb->transmit_complete(status);
	}

	cdc_put_bulk_in_request(dev, req);
}

static void cdc_bulk_out_complete(struct usb_ep *ep, struct usb_request *req)
{
	struct usb_cdc_dev_t *dev = (struct usb_cdc_dev_t *)ep->driver_data;
	int result = 0;
	int status = req->status;
	req->zero = 0;

	USBD_CDC_ENTER;

	switch (status) {
	case USB_ESUCCESS:
		if ((req->actual > 0) && (dev->cb->receive != NULL)) {
			result = dev->cb->receive(req->buf, req->actual);
			if (result != USB_ESUCCESS) {
				USBD_CDC_WARN("Fail to run user receive callback\n");
			}
		}

		cdc_put_bulk_out_request(dev, req);
		cdc_acm_receive_data(dev);

		break;

	case -USB_ESHUTDOWN:
		USBD_CDC_WARN("Bulk out shutdown\n");
		cdc_put_bulk_out_request(dev, req);
		break;

	default:
		USBD_CDC_WARN("Bulk out failed: %d\n", status);
		cdc_put_bulk_out_request(dev, req);
		break;
	}

	USBD_CDC_EXIT;
}

static struct usb_request *cdc_alloc_request(struct usb_ep *ep, u32 buf_len)
{
	struct usb_request *req = NULL;

	req = usb_ep_alloc_request(ep, 1);
	if (!req) {
		USBD_CDC_ERROR("Fail to alloc request\n");
		return NULL;
	}

	req->buf = rtw_malloc(buf_len);
	if (NULL == req->buf) {
		USBD_CDC_ERROR("Fail to alloc request buffer\n");
		usb_ep_free_request(ep, req);
		return NULL;
	}

	req->dma = (dma_addr_t)req->buf;
	req->length = buf_len;
	req->zero = 0;

	return req;
}

static int cdc_equeue_bulk_in_req(struct usb_cdc_dev_t *dev, struct usb_request  *req)
{
	int status;

	status = usb_ep_queue(dev->bulk_in_ep, req, 1);
	if (status != USB_ESUCCESS) {
		USBD_CDC_ERROR("Fail to equeue bulk in request: %d", status);
		cdc_put_bulk_in_request(dev, req);
	}

	return status;
}

static int cdc_equeue_bulk_out_req(struct usb_cdc_dev_t *dev, struct usb_request  *req)
{
	int status;

	status = usb_ep_queue(dev->bulk_out_ep, req, 1);
	if (status != USB_ESUCCESS) {
		USBD_CDC_ERROR("Fail to equeue bulk out request: %d", status);
		cdc_put_bulk_out_request(dev, req);
	}

	return status;
}

static int cdc_acm_receive_data(struct usb_cdc_dev_t *dev)
{
	struct usb_request  *req = NULL;
	int status = -USB_EBUSY;

	USBD_CDC_ENTER;

	req = cdc_get_bulk_out_request(dev);
	if (req != NULL) {
		status = cdc_equeue_bulk_out_req(dev, req);
	}

	USBD_CDC_EXIT;

	return status;
}

static void cdc_free_request(struct usb_ep *ep, struct usb_request *req)
{
	if ((ep != NULL) && (req != NULL)) {
		if (req->buf != NULL) {
			rtw_free(req->buf);
			req->buf = NULL;
		}
		usb_ep_free_request(ep, req);
	}
}

static void cdc_free_bulk_in_resource(struct usb_cdc_dev_t *dev)
{
	struct usb_request *req;

	USBD_CDC_ENTER;

	rtw_mutex_get(&dev->bulk_in_mutex);

	if (dev->bulk_in_ep != NULL) {
		while (!rtw_is_list_empty(&dev->bulk_in_req_list)) {
			req = list_first_entry(&dev->bulk_in_req_list, struct usb_request, list);
			if (req != NULL) {
				rtw_list_delete(&req->list);
				cdc_free_request(dev->bulk_in_ep, req);
			}
		}

		dev->bulk_in_req_act_num = 0;
	}

	rtw_mutex_put(&dev->bulk_in_mutex);

	USBD_CDC_EXIT;
}

static void cdc_free_bulk_out_resource(struct usb_cdc_dev_t *dev)
{
	struct usb_request *req;

	USBD_CDC_ENTER;

	rtw_mutex_get(&dev->bulk_out_mutex);

	if (dev->bulk_out_ep != NULL) {
		while (!rtw_is_list_empty(&dev->bulk_out_req_list)) {
			req = list_first_entry(&dev->bulk_out_req_list, struct usb_request, list);
			if (req != NULL) {
				rtw_list_delete(&req->list);
				cdc_free_request(dev->bulk_out_ep, req);
			}
		}

		dev->bulk_out_req_act_num = 0;
	}

	rtw_mutex_put(&dev->bulk_out_mutex);

	USBD_CDC_EXIT;
}

static void cdc_free_resource(struct usb_cdc_dev_t *dev)
{
	cdc_free_bulk_in_resource(dev);
	cdc_free_bulk_out_resource(dev);

#if USBD_CDC_ACM_USE_INT_IN_ENDPOINT
	if (dev->int_in_req != NULL) {
		cdc_free_request(dev->int_in_ep, dev->int_in_req);
		dev->int_in_req = NULL;
	}
#endif
}

static void cdc_disable_bulk_in_ep(struct usb_cdc_dev_t *dev)
{
	USBD_CDC_ENTER;

	if (dev->bulk_in_ep != NULL) {
		if (dev->bulk_in_ep->driver_data != NULL) {
			usb_ep_disable(dev->bulk_in_ep);
			dev->bulk_in_ep->driver_data = NULL;
		}
	}

	USBD_CDC_EXIT;
}

static void cdc_disable_bulk_out_ep(struct usb_cdc_dev_t *dev)
{
	USBD_CDC_ENTER;

	if (dev->bulk_out_ep != NULL) {
		if (dev->bulk_out_ep->driver_data != NULL) {
			usb_ep_disable(dev->bulk_out_ep);
			dev->bulk_out_ep->driver_data = NULL;
		}
	}

	USBD_CDC_EXIT;
}

static int cdc_init_resource(struct usb_cdc_dev_t *dev)
{
	struct usb_request *req;
	int status = -USB_EINVAL;
	int i = 0;

	USBD_CDC_ENTER;

	// Enable data in endpoint
	if (dev->bulk_in_ep != NULL) {
		cdc_disable_bulk_in_ep(dev);
		cdc_free_bulk_in_resource(dev);

		dev->bulk_in_ep->desc = usb_get_descriptor(dev->gadget, &cdc_acm_data_bulk_in_ep_desc_HS,
								&cdc_acm_data_bulk_in_ep_desc_FS);

		for (i = 0; i < dev->bulk_in_req_num; i++) {
			req = cdc_alloc_request(dev->bulk_in_ep, dev->bulk_in_buf_size);
			if (!req) {
				USBD_CDC_ERROR("Fail to malloc bulk in request\n");
				status = -USB_ENOMEM;
				goto cdc_init_resource_exit;
			} else {
				req->complete = cdc_bulk_in_complete;
				req->context = dev;
				rtw_list_insert_tail(&req->list, &dev->bulk_in_req_list);
			}
		}

		dev->bulk_in_req_act_num = dev->bulk_in_req_num;

		status = usb_ep_enable(dev->bulk_in_ep, dev->bulk_in_ep->desc);
		if (status < 0) {
			USBD_CDC_ERROR("Fail to enable bulk in endpoint\n");
			goto cdc_init_resource_exit;
		}
		dev->bulk_in_ep->driver_data = dev;
	}

	// Enable data out endpoint and alloc requset to recieve data from host
	if (dev->bulk_out_ep != NULL) {
		cdc_disable_bulk_out_ep(dev);
		cdc_free_bulk_out_resource(dev);

		dev->bulk_out_ep->desc = usb_get_descriptor(dev->gadget, &cdc_acm_data_bulk_out_ep_desc_HS,
								 &cdc_acm_data_bulk_out_ep_desc_FS);

		for (i = 0; i < dev->bulk_out_req_num; i++) {
			req = cdc_alloc_request(dev->bulk_out_ep, dev->bulk_out_buf_size);
			if (!req) {
				USBD_CDC_ERROR("Fail to malloc bulk out request\n");
				status = -USB_ENOMEM;
				goto cdc_init_resource_exit;
			} else {
				req->complete = cdc_bulk_out_complete;
				req->context = dev;
				rtw_list_insert_tail(&req->list, &dev->bulk_out_req_list);
			}
		}

		dev->bulk_out_req_act_num = dev->bulk_out_req_num;

		status = usb_ep_enable(dev->bulk_out_ep, dev->bulk_out_ep->desc);
		if (status < 0) {
			USBD_CDC_ERROR("Fail to enable bulk out endpoint\n");
			goto cdc_init_resource_exit;
		}

		dev->bulk_out_ep->driver_data = dev;
	}

#if USBD_CDC_ACM_USE_INT_IN_ENDPOINT
	if (dev->int_in_ep != NULL) {

		if (dev->int_in_ep->driver_data != NULL) {
			usb_ep_disable(dev->int_in_ep);
			dev->int_in_ep->driver_data = NULL;
		}

		if (dev->int_in_req != NULL) {
			cdc_free_request(dev->int_in_ep, dev->int_in_req);
			dev->int_in_req = NULL;
		}

		dev->int_in_ep->desc = usb_get_descriptor(dev->gadget, &usbd_cdc_acm_notify_ep_desc_HS,
							   &usbd_cdc_acm_notify_ep_desc_FS);

		req = cdc_alloc_request(dev->int_in_ep, USBD_CDC_ACM_DEFAULT_INT_IN_XFER_SIZE);
		if (!req) {
			USBD_CDC_ERROR("Fail to allocate interrupt in request\n");
			goto cdc_init_resource_exit;
		} else {
			dev->int_in_req = req;
			req->complete = cdc_int_in_complete;
			req->context = dev;
		}

		status = usb_ep_enable(dev->int_in_ep, dev->int_in_ep->desc);
		if (status < 0) {
			USBD_CDC_ERROR("Fail to enable interrupt in endpoint: %d\n", status);
			goto cdc_init_resource_exit;
		}

		dev->int_in_ep->driver_data = dev;
	}
#endif

	USBD_CDC_EXIT;

	return USB_ESUCCESS;

cdc_init_resource_exit:
	cdc_free_resource(dev);
	return status;
}

static int cdc_deinit_resource(struct usb_cdc_dev_t *dev)
{
	cdc_free_resource(dev);
	return USB_ESUCCESS;
}

static int cdc_fun_setup(struct usb_function *f, const struct usb_control_request *ctrl)
{
	struct usb_cdc_dev_t *dev = &cdc_dev;
	struct usb_composite_dev *cdev = f->config->cdev;
	struct usb_request *req0 = cdev->req;
	struct usb_ep *ep0 = dev->gadget->ep0;
	u8 bmRequestType = ctrl->bmRequestType;
	u8 bRequest = ctrl->bRequest;
	u16 wLength = ctrl->wLength;
	u16 wIndex = ctrl->wIndex;
	u16 wValue = ctrl->wValue;
	int value = -USB_EOPNOTSUPP;

	USBD_CDC_ENTER;

	if (USB_GET_TYPE(bmRequestType) != USB_TYPE_CLASS) {
		USBD_CDC_EXIT_ERR;
		return -USB_EOPNOTSUPP;
	}

	switch (bRequest) {
	case USBD_CDC_SET_LINE_CODING:
		USBD_CDC_INFO("USBD_CDC_SET_LINE_CODING");
		if ((bmRequestType == USB_REQ_TYPE_WRITE_CLASS_INTERFACE) &&
			(wIndex == dev->interface_number) &&
			(wValue == 0) &&
			(wLength == USBD_CDC_ACM_LINE_CODING_SIZE)) {
			value = wLength;
			req0->direct = EP0_DATA_OUT;
			req0->complete = cdc_ctrl_out_complete;
		}
		break;

	case USBD_CDC_GET_LINE_CODING:
		USBD_CDC_INFO("USBD_CDC_GET_LINE_CODING");
		if ((bmRequestType == USB_REQ_TYPE_READ_CLASS_INTERFACE) &&
			(wIndex == dev->interface_number) &&
			(wValue == 0) &&
			(wLength == USBD_CDC_ACM_LINE_CODING_SIZE)) {
			u8 *buf = (u8 *)req0->buf;
			buf[0] = (u8)(acm_line_coding.dwDTERate & 0xFF);
			buf[1] = (u8)((acm_line_coding.dwDTERate >> 8) & 0xFF);
			buf[2] = (u8)((acm_line_coding.dwDTERate >> 16) & 0xFF);
			buf[3] = (u8)((acm_line_coding.dwDTERate >> 24) & 0xFF);
			buf[4] = acm_line_coding.bCharFormat;
			buf[5] = acm_line_coding.bParityType;
			buf[6] = acm_line_coding.bDataBits;
			value = wLength;
			req0->direct = EP0_DATA_IN;
		}
		break;
	case USBD_CDC_SET_CONTROL_LINE_STATE:
		USBD_CDC_INFO("USBD_CDC_SET_CONTROL_LINE_STATE");
		if ((bmRequestType == USB_REQ_TYPE_WRITE_CLASS_INTERFACE) &&
			(wIndex == dev->interface_number) &&
			(wLength == 0)) {
			acm_ctrl_line_state.d16 = wValue;
			if (acm_ctrl_line_state.b.activate) {  // To avoid compiling warning
				USBD_CDC_INFO("New line state: present = %d, activate = %d", acm_ctrl_line_state.b.present, acm_ctrl_line_state.b.activate);
			}
			value = USB_GADGET_DELAYED_STATUS;
			req0->direct = EP0_DATA_OUT;
		}
		break;

	default:
		USBD_CDC_WARN("Unsupported request = %d", bRequest);
		break;
	}

	if ((value >= 0) && (value != USB_GADGET_DELAYED_STATUS)) {
		req0->length = value;
		req0->zero = 0;
		value = usb_ep_queue(ep0, req0, 1);
		if (value != 0 && value != -USB_ESHUTDOWN) {
			USBD_CDC_WARN("EP0 request queue error: %d", value);
		}
	}

	USBD_CDC_EXIT;
	return value;
}

static int cdc_fun_set_alt(struct usb_function *f, unsigned intf, unsigned alt)
{
	int status = -USB_EINVAL;
	struct usb_cdc_dev_t *dev = &cdc_dev;

	UNUSED(f);
	UNUSED(intf);
	UNUSED(alt);

	USBD_CDC_ENTER;

	dev->bulk_in_busy = 0;

	status = cdc_init_resource(dev);
	if (status != USB_ESUCCESS) {
		cdc_deinit_resource(dev);
		USBD_CDC_EXIT_ERR;
		return status;
	}

	cdc_acm_receive_data(dev);

	USBD_CDC_EXIT;

	return USB_ESUCCESS;
}

static void cdc_fun_disable(struct usb_function *f)
{
	struct usb_cdc_dev_t *dev = &cdc_dev;
	UNUSED(f);

	USBD_CDC_ENTER;

	cdc_disable_bulk_in_ep(dev);
	cdc_disable_bulk_out_ep(dev);

#if USBD_CDC_ACM_USE_INT_IN_ENDPOINT
	if (dev->int_in_ep->driver_data != NULL) {
		usb_ep_disable(dev->int_in_ep);
		dev->int_in_ep->driver_data = NULL;
	}
#endif
	USBD_CDC_EXIT;
}

#if USBD_CDC_ACM_USE_BULK_IN_THREAD

static thread_return cdc_bulk_in_handler(thread_context context)
{
	struct usb_cdc_dev_t *dev = &cdc_dev;
	struct usb_request  *req = NULL;
	int status = -USB_ENODEV;
	u32 residue = 0;
	void *buf = NULL;
	u16 buf_size = 0;

	UNUSED(context);

	USBD_CDC_ENTER;

	while (TRUE) {
		status = rtw_down_sema(&(dev->bulk_in_task.wakeup_sema));
		if (FAIL == status) {
			USBD_CDC_ERROR("Fail to get bulk in task wakeup sema");
			break;
		}

		if (dev->bulk_in_task.blocked) {
			USBD_CDC_WARN("CDC_BULK_IN_TASK aborted\n");
			break;
		}

		residue = cdc_bulk_in_xfer_size;
		buf = cdc_bulk_in_xfer_buf;

		USBD_CDC_INFO("Xfer [%p] %d bytes", buf, residue);

		if ((0 == dev->bulk_in_busy) || (NULL == buf) || (0 == residue)) {
			USBD_CDC_WARN("Invalid bulk in status");
			continue;
		}

		buf_size = dev->bulk_in_buf_size;
		while (residue > 0) {
			req = cdc_get_bulk_in_request(dev);
			if (req != NULL) {
				if (residue > buf_size) {
					req->length = buf_size;
					residue -= buf_size;
				} else {
					req->length = residue;
					residue = 0;
					if (req->length == dev->bulk_in_buf_size) {
						req->zero = 1;
					}
				}

				rtw_memcpy((void *)req->buf, buf, req->length);
				status = cdc_equeue_bulk_in_req(dev, req);
				if (status != USB_ESUCCESS) {
					break;
				}
				buf = (void *)((u32)buf + req->length);
				//rtw_udelay_os(100); // Descrease or eliminate the delay properly to speed up as per specific application
			} else {
				status = -USB_EBUSY;
				USBD_CDC_WARN("Fail to get bulk in request");
				break;
			}
		}

		if ((residue > 0) || (status != USB_ESUCCESS)) {
			USBD_CDC_WARN("Fail to bulk in data: total=%d, residue=%d", cdc_bulk_in_xfer_size, residue);
		}

		dev->bulk_in_busy = 0;
		if ((dev->cb != NULL) && (dev->cb->transmit_complete != NULL)) {
			dev->cb->transmit_complete(status);
		}
	}

	rtw_up_sema(&(dev->bulk_in_task.terminate_sema));

	USBD_CDC_WARN("CDC_BULK_IN_TASK exit\n");

	rtw_thread_exit();

	USBD_CDC_EXIT;
}

#endif

static int cdc_fun_bind(struct usb_configuration *c, struct usb_function *f)
{
	struct usb_cdc_dev_t *dev = &cdc_dev;
	struct usb_ep *ep;
	int status = -USB_ENODEV;
	int id;

	USBD_CDC_ENTER;

#if USBD_CDC_ACM_USE_BULK_IN_THREAD
	if (rtw_create_task(&dev->bulk_in_task, "CDC_BULK_IN_TASK",
						USBD_CDC_ACM_BULK_IN_TASK_STACK_SIZE, USBD_CDC_ACM_BULK_IN_TASK_PRIORITY, cdc_bulk_in_handler, (void *)NULL) != pdPASS) {
		USBD_CDC_ERROR("Fail to create thread CDC_BULK_IN_TASK");
		goto fail;
	}
#endif

	// Allocate instance-specific interface ID
	id = usb_interface_id(c, f); // this will return the allocated interface id
	if (id < 0) {
		status = id;
		goto fail;
	}

	cdc_acm_comm_if_desc.bInterfaceNumber = id;
	cdc_acm_union_desc.bControlInterface = id;
	dev->interface_number = id; // record comunication interface number

	// Allocate instance-specific interface ID
	id = usb_interface_id(c, f); // this will return the allocated interface id
	if (id < 0) {
		status = id;
		goto fail;
	}

	cdc_acm_data_if_desc.bInterfaceNumber = id;
	cdc_acm_union_desc.bSubordinateInterface0 = id;
	cdc_acm_call_mgmt_desc.bDataInterface = id;

	// Search endpoint according to endpoint descriptor and modify endpoint address
	ep = usb_ep_autoconfig(c->cdev->gadget, &cdc_acm_data_bulk_in_ep_desc_FS);
	if (!ep) {
		goto fail;
	}

	ep->driver_data = dev;
	dev->bulk_in_ep = ep;

	// Search endpoint according to endpoint descriptor and modify endpoint address
	ep = usb_ep_autoconfig(c->cdev->gadget, &cdc_acm_data_bulk_out_ep_desc_FS);
	if (!ep) {
		goto fail;
	}

	ep->driver_data = dev;
	dev->bulk_out_ep = ep;
#if USBD_CDC_ACM_USE_INT_IN_ENDPOINT
	ep = usb_ep_autoconfig(c->cdev->gadget, &usbd_cdc_acm_notify_ep_desc_FS);
	if (!ep) {
		goto fail;
	}

	ep->driver_data = dev;
	dev->int_in_ep = ep;
	usbd_cdc_acm_notify_ep_desc_HS.bEndpointAddress = usbd_cdc_acm_notify_ep_desc_FS.bEndpointAddress;
#endif
	cdc_acm_data_bulk_in_ep_desc_HS.bEndpointAddress = cdc_acm_data_bulk_in_ep_desc_FS.bEndpointAddress;
	cdc_acm_data_bulk_out_ep_desc_HS.bEndpointAddress = cdc_acm_data_bulk_out_ep_desc_FS.bEndpointAddress;

	status = usb_assign_descriptors(f, cdc_acm_descriptors_FS, cdc_acm_descriptors_HS, NULL);
	if (status != USB_ESUCCESS) {
		goto fail;
	}

	USBD_CDC_EXIT;
	return USB_ESUCCESS;

fail:
#if USBD_CDC_ACM_USE_BULK_IN_THREAD
	if (dev->bulk_in_task.task != NULL) {
		rtw_delete_task(&dev->bulk_in_task);
	}
#endif
	usb_free_all_descriptors(f);
	USBD_CDC_EXIT_ERR;
	return status;
}

static void cdc_fun_unbind(struct usb_configuration *c, struct usb_function *f)
{
	UNUSED(c);
	USBD_CDC_ENTER;
#if USBD_CDC_ACM_USE_BULK_IN_THREAD
	struct usb_cdc_dev_t *dev = &cdc_dev;
	if (dev->bulk_in_task.task != NULL) {
		rtw_delete_task(&dev->bulk_in_task);
	}
#endif
	usb_free_all_descriptors(f);
	USBD_CDC_EXIT;
}

static void cdc_fun_free(struct usb_function *f)
{
	USBD_CDC_ENTER;
	UNUSED(f);

	struct usb_cdc_dev_t *dev = &cdc_dev;
	cdc_free_resource(dev);
	usb_free_all_descriptors(f);

	USBD_CDC_EXIT;
}

static int cdc_config(struct usb_configuration *c)
{
	struct usb_cdc_dev_t *dev = &cdc_dev;

	USBD_CDC_ENTER;

	dev->gadget = c->cdev->gadget;

	USBD_CDC_EXIT;
	return USB_ESUCCESS;
}

static struct usb_function cdc_acm_funcions = {
	.bind      = cdc_fun_bind,
	.unbind    = cdc_fun_unbind,
	.setup     = cdc_fun_setup,
	.set_alt   = cdc_fun_set_alt,
	.disable   = cdc_fun_disable,
	.free_func = cdc_fun_free,
};

static struct usb_composite_driver cdc_acm_driver = {
	.dev_desc	 = &cdc_acm_device_desc,
	.strings 	 = cdc_acm_gadget_strings,
	.config_desc = &cdc_acm_config_desc,
	.functions   = &cdc_acm_funcions,
	.driver_data = &cdc_dev,
	.config      = cdc_config,
};

// USBD CDC ACM init
// bulk_in_buf_size: Bulk in transfer buffer size, i.e. the transfer bytes of single bulk in transfer, should be <= 65535, limited by USBD core lib,
// bulk_out_buf_size: Bulk out transfer buffer size, i.e. the transfer bytes of single bulk out transfer, should be <= 65535, limited by USBD core lib,
// cb: User callback
int usbd_cdc_acm_init(int speed, usbd_cdc_acm_usr_cb_t *cb)
{
	int status = -USB_EINVAL;
	struct usb_cdc_dev_t *dev = &cdc_dev;

	USBD_CDC_ENTER;

	// transfer length should equal to max packet length in ep descriptor because host will not send
	// zlp packet after send packet that length equal to max packet length
	dev->bulk_in_buf_size = (speed == USB_SPEED_HIGH) ? USBD_CDC_ACM_HS_BULK_MAX_PACKET_SIZE :
							USBD_CDC_ACM_FS_BULK_MAX_PACKET_SIZE;

	dev->bulk_out_buf_size = (speed == USB_SPEED_HIGH) ? USBD_CDC_ACM_HS_BULK_MAX_PACKET_SIZE :
							 USBD_CDC_ACM_FS_BULK_MAX_PACKET_SIZE;


	dev->bulk_in_req_num = USBD_CDC_ACM_BULK_IN_REQ_NUM;
	dev->bulk_out_req_num = USBD_CDC_ACM_BULK_OUT_REQ_NUM;

	if (cb != NULL) {
		dev->cb = cb;
	}

	if ((dev->cb != NULL) && (dev->cb->init != NULL)) {
		status = dev->cb->init();
		if (status != 0) {
			USBD_CDC_ERROR("Fail to run user init callback");
			goto usbd_cdc_acm_init_exit;
		}
	}

#if USBD_CDC_ACM_USE_BULK_IN_THREAD
#if defined (CONFIG_PLATFORM_8721D) || defined(CONFIG_PLATFORM_AMEBAD2)
	// rtw_create_task in Ameba1 has semaphore and initial inside,
	// but AmebaPro(AmebaD) didn't have it, so we create semaphore by ourselves
	rtw_init_sema(&dev->bulk_in_task.wakeup_sema, 0);
	rtw_init_sema(&dev->bulk_in_task.terminate_sema, 0);
#endif
#endif

	rtw_init_listhead(&dev->bulk_in_req_list);
	rtw_mutex_init(&dev->bulk_in_mutex);
	rtw_init_listhead(&dev->bulk_out_req_list);
	rtw_mutex_init(&dev->bulk_out_mutex);

	status = usb_composite_register(&cdc_acm_driver);
	if (status != 0) {
		USBD_CDC_ERROR("Register composite driver fail");
		goto usbd_cdc_acm_init_exit_free;
	}

	USBD_CDC_EXIT;
	return USB_ESUCCESS;

usbd_cdc_acm_init_exit_free:

#if USBD_CDC_ACM_USE_BULK_IN_THREAD
#if defined (CONFIG_PLATFORM_8721D) || defined(CONFIG_PLATFORM_AMEBAD2)
	rtw_free_sema(&dev->bulk_in_task.wakeup_sema);
	rtw_free_sema(&dev->bulk_in_task.terminate_sema);
#endif
#endif

	rtw_mutex_free(&dev->bulk_in_mutex);
	rtw_mutex_free(&dev->bulk_out_mutex);

	if ((dev->cb->deinit != NULL)) {
		dev->cb->deinit();
	}

usbd_cdc_acm_init_exit:
	USBD_CDC_EXIT_ERR;

	return status;
}

void usbd_cdc_acm_deinit(void)
{
	struct usb_cdc_dev_t *dev = &cdc_dev;

	USBD_CDC_ENTER;

	usb_composite_unregister(&cdc_acm_driver);

#if USBD_CDC_ACM_USE_BULK_IN_THREAD
#if defined (CONFIG_PLATFORM_8721D) || defined(CONFIG_PLATFORM_AMEBAD2)
	rtw_free_sema(&dev->bulk_in_task.wakeup_sema);
	rtw_free_sema(&dev->bulk_in_task.terminate_sema);
#endif
#endif

	rtw_mutex_free(&dev->bulk_in_mutex);
	rtw_mutex_free(&dev->bulk_out_mutex);

	if ((dev->cb != NULL) && (dev->cb->deinit != NULL)) {
		dev->cb->deinit();
	}

	USBD_CDC_EXIT;
}

/**
* See section 6.3.5 of the CDC 1.1 specification for information
* about the only notification we issue:  SerialState change.
*/
#if USBD_CDC_ACM_USE_NOTIFY_SERIAL_STATE
int usbd_cdc_acm_notify_serial_state(u16 serial_state)
{
	struct usb_cdc_dev_t *dev = &cdc_dev;
	int status;
	dev->serial_state = serial_state;
	if (dev->int_in_req) {
		status = acm_cdc_notify(dev, USB_CDC_NOTIFY_SERIAL_STATE, 0, &serial_state, 2);
	} else {
		dev->pending = 1;
		status = 0;
	}
	return status;
}
#endif

// Synchronously transfer user data to host via bulk in endpoint
// buf: user data buffer
// length: user data length
int usbd_cdc_acm_sync_transmit_data(void *buf, u16 length)
{
	struct usb_cdc_dev_t *dev = &cdc_dev;
	struct usb_request  *req = NULL;
	int status = -USB_EBUSY;

	USBD_CDC_ENTER;

	if ((buf == NULL) || (length == 0) || (length > dev->bulk_in_buf_size)) {
		return -USB_EINVAL;
	}

	req = cdc_get_bulk_in_request(dev);
	if (req != NULL) {
		req->length = length;
		rtw_memcpy((void *)req->buf, buf, length);
		if (length == dev->bulk_in_buf_size) {
			req->zero = 1;
		} else {
			req->zero = 0;
		}
		status = cdc_equeue_bulk_in_req(dev, req);
	} else {
		USBD_CDC_ERROR("Fail to get bulk in request");
	}

	USBD_CDC_EXIT;

	return status;
}

// Asynchronously transfer user data to host via bulk in endpoint
// buf: user data buffer, shall be properly maintained by user application
// length: user data length
int usbd_cdc_acm_async_transmit_data(void *buf, u32 length)
{
#if USBD_CDC_ACM_USE_BULK_IN_THREAD

	struct usb_cdc_dev_t *dev = &cdc_dev;
#if USBD_CDC_ACM_ALLOC_ASYNC_BULK_IN_BUF
	void *xfer_buf = NULL;
#endif

	USBD_CDC_ENTER;

	if ((buf == NULL) || (length == 0)) {
		USBD_CDC_EXIT_ERR;
		return -USB_EINVAL;
	}

	if (dev->bulk_in_busy) {
		USBD_CDC_EXIT_ERR;
		return -USB_EBUSY;
	}

#if USBD_CDC_ACM_ALLOC_ASYNC_BULK_IN_BUF
	xfer_buf = rtw_malloc(length);
	if (xfer_buf == NULL) {
		USBD_CDC_ERROR("Fail to malloc async bulk in xfer buffer");
		USBD_CDC_EXIT_ERR;
		return -USB_ENOMEM;
	}
	cdc_bulk_in_xfer_buf = xfer_buf;
	rtw_memcpy(cdc_bulk_in_xfer_buf, buf, length);
#else
	cdc_bulk_in_xfer_buf = buf;
#endif
	cdc_bulk_in_xfer_size = length;

	dev->bulk_in_busy = 1;

	rtw_up_sema(&(dev->bulk_in_task.wakeup_sema));

	USBD_CDC_EXIT;

	return USB_ESUCCESS;

#else

	UNUSED(buf);
	UNUSED(length);

	return -USB_EOPNOTSUPP;

#endif
}

#endif // CONFIG_USBD_CDC_ACM

