#include <platform_opts.h>
#ifdef CONFIG_USBD_HID
#include "usbd_hid.h"
#include "osdep_service.h"


static int hid_deinit_resource(struct usb_hid_device_t *dev);

static struct usb_string hid_strings[] = {
	{
		.id = USBD_HID_STRING_MANUFACTURER,
		.s  = "Realtek",
	},
	{
		.id = USBD_HID_STRING_PRODUCT,
		.s  = "USB HID Device",
	},
	{
		.id = USBD_HID_STRING_SERIALNUMBER,
		.s  = "0123456789AB",
	},
};

static struct usb_gadget_strings hid_gadget_string_tab = {
	.language = 0x0409,
	.strings = hid_strings,
};

struct usb_gadget_strings *hid_gadget_strings[] = {
	&hid_gadget_string_tab,
	NULL
};

struct usb_device_descriptor hid_device_desc = {
	.bLength            = USB_DT_DEVICE_SIZE,
	.bDescriptorType    = USB_DT_DEVICE,
	.bcdUSB             = 0x0100,  					 //USB HID Spec Release 1.0
	.bDeviceClass       = USB_CLASS_PER_INTERFACE,
	.bDeviceSubClass    = 0x00,
	.bDeviceProtocol    = 0x00,
	.bMaxPacketSize0    = 8,    					 //this will be set automatically depends on ep0 setting
	.idVendor           = REALTEK_USB_VID,
	.idProduct          = REALTEK_USB_PID,
	.bcdDevice          = 0x0100,
	.iManufacturer      = USBD_HID_STRING_MANUFACTURER,
	.iProduct           = USBD_HID_STRING_PRODUCT,
	.iSerialNumber      = USBD_HID_STRING_SERIALNUMBER,
	.bNumConfigurations = 0x01,
};

struct usb_config_descriptor hid_config_desc = {
	.bLength             = USB_DT_CONFIG_SIZE,
	.wTotalLength		 = 0x0022, 					 //config+intf+hid+endpoint desc length, not include report desc
	.bDescriptorType     = USB_DT_CONFIG,
	.bNumInterfaces      = 1,
	.bConfigurationValue = 1,
	.iConfiguration      = 0,
	.bmAttributes	     = USB_CONFIG_ATT_ONE, 		 //| USB_CONFIG_ATT_SELFPOWER,
	.bMaxPower           = 50,           			 //50*2 = 100mA
};

/* HID Interface, Alternate Setting 0*/
struct usb_interface_descriptor hid_intf_desc = {
	.bLength            = USB_DT_INTERFACE_SIZE,
	.bDescriptorType    = USB_DT_INTERFACE,
	.bInterfaceNumber   = 0x0, 						 // this will be assign automatically
	.bAlternateSetting  = 0x00,
	.bNumEndpoints      = 0x01,
	.bInterfaceClass    = USB_CLASS_HID,
	.bInterfaceSubClass = 1,
	.bInterfaceProtocol = 2,
	.iInterface         = USBD_HID_STRING_STRING_INTERFACE
};

/*HID Descriptor*/
struct usb_hid_descriptor hid_desc = {
	.bLength 			= USBD_HID_DESC_SIZE,
	.bDescriptorType 	= 0x21,   						//HID descritptor type
	.bcdHID 			= 0x111, 						//HID class specification release number
	.bCountryCode		= 0x00,
	.bNumDescriptors 	= 0x01,
	.bDescriptorTypeSub = 0x22,
	.wDescriptorLength 	= HID_MOUSE_REPORT_DESC_SIZE,
};

/* Endpoint, EP Interrupt IN, for Low-speed/Full-speed */
struct usb_endpoint_descriptor hid_interrupt_source_desc_FS = {
	.bLength          = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType  = USB_DT_ENDPOINT,
	.bEndpointAddress = USBD_HID_INTERRUPT_IN_EP_ADDRESS,
	.bmAttributes     = USB_ENDPOINT_XFER_INT,
	.wMaxPacketSize   = 0x8,
	.bInterval        = 0xA,
};


/* Endpoint, EP Interrupt IN, for High-speed */
struct usb_endpoint_descriptor hid_interrupt_source_desc_HS = {
	.bLength          = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType  = USB_DT_ENDPOINT,
	.bEndpointAddress = USBD_HID_INTERRUPT_IN_EP_ADDRESS,
	.bmAttributes     = USB_ENDPOINT_XFER_INT,
	.wMaxPacketSize   = 0x8,
	.bInterval        = 0xA,
};

u8 HID_MOUSE_ReportDesc[HID_MOUSE_REPORT_DESC_SIZE] __attribute__((aligned(32))) = {
	0x05, 0x01,                    // USAGE_PAGE (Generic Desktop)
	0x09, 0x02,                    // USAGE (Mouse)
	0xa1, 0x01,                    // COLLECTION (Application)
	0x09, 0x01,                    //   USAGE (Pointer)
	0xa1, 0x00,                    //   COLLECTION (Physical)
	0x05, 0x09,                    //     USAGE_PAGE (Button)
	0x19, 0x01,                    //     USAGE_MINIMUM (Button 1)
	0x29, 0x03,                    //     USAGE_MAXIMUM (Button 3)
	0x15, 0x00,                    //     LOGICAL_MINIMUM (0)
	0x25, 0x01,                    //     LOGICAL_MAXIMUM (1)
	0x95, 0x03,                    //     REPORT_COUNT (3)
	0x75, 0x01,                    //     REPORT_SIZE (1)
	0x81, 0x02,                    //     INPUT (Data,Var,Abs)
	0x95, 0x01,                    //     REPORT_COUNT (1)
	0x75, 0x05,                    //     REPORT_SIZE (5)
	0x81, 0x03,                    //     INPUT (Cnst,Var,Abs)
	0x05, 0x01,                    //     USAGE_PAGE (Generic Desktop)
	0x09, 0x30,                    //     USAGE (X)
	0x09, 0x31,                    //     USAGE (Y)
	0x09, 0x38,                    //     USAGE (Wheel)
	0x15, 0x81,                    //     LOGICAL_MINIMUM (-127)
	0x25, 0x7f,                    //     LOGICAL_MAXIMUM (127)
	0x75, 0x08,                    //     REPORT_SIZE (8)
	0x95, 0x03,                    //     REPORT_COUNT (3)
	0x81, 0x06,                    //     INPUT (Data,Var,Rel)
	0xc0,                          //   END_COLLECTION
	0xc0                           // END_COLLECTION
};


struct usb_descriptor_header *hid_descriptors_FS[] = {
	(struct usb_descriptor_header *) &hid_intf_desc,
	(struct usb_descriptor_header *) &hid_desc,
	(struct usb_descriptor_header *) &hid_interrupt_source_desc_FS,
	NULL,
};

struct usb_descriptor_header *hid_descriptors_HS[] = {
	(struct usb_descriptor_header *) &hid_intf_desc,
	(struct usb_descriptor_header *) &hid_desc,
	(struct usb_descriptor_header *) &hid_interrupt_source_desc_HS,
	NULL,
};

static struct usb_hid_device_t hid_device;
u32 USBD_HID_Protocol = 0;
u32 USBD_HID_IdleState = 0;

static int hid_send_data(struct usb_hid_device_t *dev)
{
	USBD_HID_ENTER;

	struct usb_ep *ep = dev->int_in_ep;
	struct usb_request *req = dev->int_in_req;
	int status;

	dev->int_in_req = NULL;
	dev->pending_data = 0;
	req->length = sizeof(struct mouse);
	rtw_memcpy(req->buf, &dev->data, req->length);

	status = usb_ep_queue(ep, req, 1);
	if (status != USB_ESUCCESS) {
		USBD_HID_ERROR("Fail to equeue interrupt in request: %d", status);
	}

	rtw_memset(&dev->data, 0, sizeof(struct mouse));

	USBD_HID_EXIT;
	return status;
}

static void hid_int_in_complete(struct usb_ep *ep, struct usb_request *req)
{
	int status = req->status;
	struct usb_hid_device_t *dev = (struct usb_hid_device_t *)ep->driver_data;

	USBD_HID_ENTER;

	dev->int_in_req = req;

	switch (status) {
	case USB_ESUCCESS: 			/* tx successfuly*/
		if ((dev->cb != NULL) && (dev->cb->transmit_complete != NULL)) {
			dev->cb->transmit_complete(status);
		}
		if (dev->pending_data == 1) {
			hid_send_data(dev);
		}
		break;
	/* FALLTHROUGH */
	case -USB_ECONNRESET:		/* unlink */
		USBD_HID_ERROR("ECONNRESET status = %d\n", status);
		break;
	case -USB_ESHUTDOWN:		/* disconnect etc */
		USBD_HID_ERROR("ESHUTDOWN status = %d\n", status);
		break;
	default:
		USBD_HID_ERROR("hid_int_in_complete ERROR(%d)\n", status);
	}

	USBD_HID_EXIT;
}

static void hid_free_request(struct usb_ep *ep, struct usb_request *req)
{
	if ((ep != NULL) && (req != NULL)) {
		if (req->buf != NULL) {
			rtw_free(req->buf);
			req->buf = NULL;
		}
		usb_ep_free_request(ep, req);
	}
}

static struct usb_request *hid_alloc_request(struct usb_ep *ep, u32 buf_len)
{
	struct usb_request *req = NULL;

	req = usb_ep_alloc_request(ep, 1);
	if (!req) {
		USBD_HID_ERROR("Fail to alloc request\n");
		return NULL;
	}

	req->buf = rtw_malloc(buf_len);
	if (NULL == req->buf) {
		USBD_HID_ERROR("Fail to alloc request buffer\n");
		usb_ep_free_request(ep, req);
		return NULL;
	}

	req->dma = (dma_addr_t)req->buf;
	req->length = buf_len;
	req->zero = 0;

	return req;
}

static void hid_free_resource(struct usb_hid_device_t *dev)
{
	USBD_HID_ENTER;
	if (dev->int_in_req != NULL) {
		hid_free_request(dev->int_in_ep, dev->int_in_req);
		dev->int_in_req = NULL;
	}
	USBD_HID_ENTER;
}

static int hid_init_resource(struct usb_hid_device_t *dev)
{
	struct usb_request *req;
	int status = -USB_EINVAL;

	USBD_HID_ENTER;

	if (dev->int_in_ep != NULL) {

		if (dev->int_in_ep->driver_data != NULL) {
			usb_ep_disable(dev->int_in_ep);
			dev->int_in_ep->driver_data = NULL;
		}

		if (dev->int_in_req != NULL) {
			hid_free_request(dev->int_in_ep, dev->int_in_req);
			dev->int_in_req = NULL;
		}

		dev->int_in_ep->desc = usb_get_descriptor(dev->gadget, &hid_interrupt_source_desc_HS,
							   &hid_interrupt_source_desc_FS);

		req = hid_alloc_request(dev->int_in_ep, USBD_HID_DEFAULT_INT_IN_XFER_SIZE);
		if (!req) {
			USBD_HID_ERROR("Fail to allocate interrupt in request\n");
			goto hid_init_resource_fail;
		} else {
			dev->int_in_req = req;
			req->complete = hid_int_in_complete;
			req->context = dev;
		}

		status = usb_ep_enable(dev->int_in_ep, dev->int_in_ep->desc);
		if (status < 0) {
			USBD_HID_ERROR("Fail to enable interrupt in endpoint: %d\n", status);
			goto hid_init_resource_fail;
		}

		dev->int_in_ep->driver_data = dev;
	}

	return USB_ESUCCESS;

hid_init_resource_fail:
	hid_deinit_resource(dev);
	return -USB_EINVAL;;
}

static int hid_fun_set_alt(struct usb_function *f, unsigned intf, unsigned alt)
{
	int status = -USB_EINVAL;
	struct usb_hid_device_t *dev = &hid_device;

	UNUSED(f);
	UNUSED(intf);
	UNUSED(alt);

	USBD_HID_ENTER;

	status = hid_init_resource(dev);
	if (status != USB_ESUCCESS) {
		hid_deinit_resource(dev);
		USBD_HID_EXIT;
		return status;
	}

	USBD_HID_EXIT;

	return USB_ESUCCESS;
}

static int hid_fun_bind(struct usb_configuration *c, struct usb_function *f)
{
	struct usb_hid_device_t *dev = &hid_device;
	struct usb_ep *ep;
	int status = -USB_ENODEV;
	int id;

	USBD_HID_ENTER;

	/* allocate instance-specific interface IDs, and patch descriptors */
	id = usb_interface_id(c, f); // this will return the allocated interface id
	if (id < 0) {
		status = id;
		goto hid_fun_bind_exit;
	}

	hid_intf_desc.bInterfaceNumber = id;

	ep = usb_ep_autoconfig(c->cdev->gadget, &hid_interrupt_source_desc_FS);
	if (!ep) {
		goto hid_fun_bind_exit;
	}

	ep->driver_data = dev;
	dev->int_in_ep = ep;

	hid_interrupt_source_desc_HS.bEndpointAddress = hid_interrupt_source_desc_FS.bEndpointAddress;

	status = usb_assign_descriptors(f, hid_descriptors_FS, hid_descriptors_HS, NULL);
	if (status) {
		USBD_HID_ERROR("Fail to assign descriptors: %d\n", status);
		usb_free_all_descriptors(f);
		goto hid_fun_bind_exit;
	}

hid_fun_bind_exit:
	USBD_HID_EXIT;
	return status;
}

int hid_fun_setup(struct usb_function *f, const struct usb_control_request *ctrl)
{
	struct usb_hid_device_t *dev = &hid_device;
	struct usb_composite_dev *cdev = f->config->cdev;
	struct usb_request *req = cdev->req;
	struct usb_ep *ep = dev->gadget->ep0;
	int status = -USB_EOPNOTSUPP;

	USBD_HID_ENTER;

	USBD_HID_INFO("Setup request type=0x%02X request=0x%02X value=0x%04X index=0x%04X length=0x%04X\n",
				  ctrl->bmRequestType, ctrl->bRequest, ctrl->wValue,
				  ctrl->wIndex, ctrl->wLength);

	switch (ctrl->bRequest) {
	case USB_REQ_GET_DESCRIPTOR:
		if ((ctrl->wValue >> 8) == HID_REPORT_DESC) {
			USBD_HID_INFO("USB_REQ_GET_DESCRIPTOR: HID_REPORT_DESC");
			req->length = MIN(HID_MOUSE_REPORT_DESC_SIZE, ctrl->wLength);
			rtw_memcpy(req->buf, HID_MOUSE_ReportDesc, HID_MOUSE_REPORT_DESC_SIZE);
			req->zero = 0;
			req->direct = EP0_DATA_IN;
			status = usb_ep_queue(ep, req, 1);
			if (status != 0) {
				USBD_HID_WARN("EP0 request queue error: %d", status);
			}
			dev->plug_in = 1;
		} else {
			USBD_HID_WARN("unknown wValue\n");
		}
		goto response;
	case USBD_HID_GET_REPORT:
		USBD_HID_INFO("USBD_HID_GET_REPORT");
		rtw_memset(req->buf, 0, ctrl->wLength);                    //send an empty report
		req->zero = 0;
		req->length = ctrl->wLength;
		status = usb_ep_queue(ep, req, 1);
		if (status != 0) {
			USBD_HID_WARN("EP0 request queue error: %d", status);
		}
		goto response;
	case USBD_HID_GET_IDLE:
		USBD_HID_INFO("USBD_HID_GET_IDLE");
		goto stall;
	case USBD_HID_GET_PROTOCOL:
		USBD_HID_INFO("USBD_HID_GET_PROTOCOL");
		goto stall;
	case USBD_HID_SET_REPORT:
		USBD_HID_INFO("USBD_HID_SET_REPORT");
		goto stall;
	case USBD_HID_SET_IDLE:
		USBD_HID_INFO("USBD_HID_SET_IDLE");
		USBD_HID_IdleState = ctrl->wValue >> 8;
		req->direct = EP0_DATA_OUT;
		req->length = ctrl->wLength;
		req->zero = 0;
		status = usb_ep_queue(ep, req, 1);
		if (status != 0) {
			USBD_HID_WARN("EP0 request queue error: %d", status);
		}
		goto response;
	case USBD_HID_SET_PROTOCOL:
		USBD_HID_INFO("USBD_HID_SET_PROTOCOL");
		USBD_HID_Protocol = ctrl->wValue;
		goto stall;
	default:
		USBD_HID_WARN("not hid request = %d", ctrl->bRequest);
		goto stall;
	}

stall:
	USBD_HID_EXIT;
	return -USB_EOPNOTSUPP;

response:
	USBD_HID_EXIT;
	return status;
}

void hid_fun_disable(struct usb_function *f)
{
	struct usb_hid_device_t *dev = &hid_device;
	UNUSED(f);

	USBD_HID_ENTER;

	if (dev->int_in_ep->driver_data != NULL) {
		usb_ep_disable(dev->int_in_ep);
		dev->int_in_ep->driver_data = NULL;
	}

	USBD_HID_EXIT;
}

void hid_fun_unbind(struct usb_configuration *c, struct usb_function *f)
{
	UNUSED(c);
	USBD_HID_ENTER;

	usb_free_all_descriptors(f);

	USBD_HID_EXIT;
}

static void hid_fun_free(struct usb_function *f)
{
	struct usb_hid_device_t *dev = &hid_device;

	USBD_HID_ENTER;

	hid_free_resource(dev);
	usb_free_all_descriptors(f);

	USBD_HID_EXIT;
}


static int hid_config(struct usb_configuration *c)
{
	struct usb_hid_device_t *dev = &hid_device;
	USBD_HID_ENTER;

	dev->gadget = c->cdev->gadget;

	USBD_HID_EXIT;
	return USB_ESUCCESS;
}

static struct usb_function hid_funcions = {
	.bind      = hid_fun_bind,
	.unbind    = hid_fun_unbind,
	.setup     = hid_fun_setup,
	.set_alt   = hid_fun_set_alt,
	.disable   = hid_fun_disable,
	.free_func = hid_fun_free,
};

static struct usb_composite_driver hid_driver = {
	.dev_desc    = &hid_device_desc,
	.strings     = hid_gadget_strings,
	.config_desc = &hid_config_desc,
	.functions   = &hid_funcions,
	.driver_data = &hid_device,
	.config      = hid_config,
};

static int hid_deinit_resource(struct usb_hid_device_t *dev)
{
	USBD_HID_ENTER;

	dev->plug_in = 0;
	hid_free_resource(dev);

	USBD_HID_EXIT;
	return USB_ESUCCESS;
}

//Send mouse data by interrput in pipe.
int usbd_hid_send_mouse(struct mouse_data *data)
{
	USBD_HID_ENTER;

	u32 status;
	struct usb_hid_device_t *dev = &hid_device;

	if (data->left != 0) {
		dev->data.button |= MOUSE_BUTTON_LEFT;
	}
	if (data->right != 0) {
		dev->data.button |= MOUSE_BUTTON_RIGHT;
	}
	if (data->middle != 0) {
		dev->data.button |= MOUSE_BUTTON_MIDDLE;
	}

	dev->data.button |= MOUSE_BUTTON_RESERVED;
	dev->data.x_axis = data->x_axis;
	dev->data.y_axis = data->y_axis;
	dev->data.wheel = data->wheel;

	if (dev->int_in_req) {
		status = hid_send_data(dev);
	} else {
		dev->pending_data = 1;
		status = 0;
	}

	USBD_HID_EXIT;
	return status;
}


//Check if hid device enumerate completely.
int usbd_hid_status(void)
{
	USBD_HID_ENTER;
	struct usb_hid_device_t *dev = &hid_device;

	USBD_HID_EXIT;
	return dev->plug_in;
}

int usbd_hid_init(usbd_hid_usr_cb_t *cb)
{
	int status = -USB_EINVAL;
	struct usb_hid_device_t *dev = &hid_device;

	USBD_HID_ENTER;

	dev->cb = cb;
	dev->pending_data = 0;
	dev->plug_in = 0;

	if ((dev->cb != NULL) && (dev->cb->init != NULL)) {
		dev->cb->init();
	}

	status = usb_composite_register(&hid_driver);
	if (status != 0) {
		USBD_HID_ERROR("Fail to register composite driver");
		usb_composite_unregister(&hid_driver);
		goto usbd_hid_init_fail;
	}

	USBD_HID_EXIT;
	return USB_ESUCCESS;

usbd_hid_init_fail:
	USBD_HID_EXIT_ERR;
	return status;
}

void usbd_hid_deinit(void)
{
	struct usb_hid_device_t *dev = &hid_device;

	USBD_HID_ENTER;

	hid_deinit_resource(dev);
	usb_composite_unregister(&hid_driver);

	if ((dev->cb != NULL) && (dev->cb->deinit != NULL)) {
		dev->cb->deinit();
	}

	USBD_HID_EXIT;
}

#endif // CONFIG_USBD_HID

