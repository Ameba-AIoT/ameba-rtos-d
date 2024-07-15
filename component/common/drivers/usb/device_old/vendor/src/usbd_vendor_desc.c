#include <platform_opts.h>
#ifdef CONFIG_USBD_VENDOR
#include "usb.h"
#include "usb_composite.h"
#include "usbd_vendor.h"

// String index
#define USBD_VENDOR_STRING_MANUFACTURER         1
#define USBD_VENDOR_STRING_PRODUCT              2
#define USBD_VENDOR_STRING_SERIALNUMBER         3
#define USBD_VENDOR_STRING_STRING_INTERFACE     4

// Endpoint address
#define USBD_VENDOR_BULK_IN_EP_ADDRESS          0x81
#define USBD_VENDOR_BULK_OUT_EP_ADDRESS         0x02
#define USBD_VENDOR_ISO_IN_EP_ADDRESS           0x83
#define USBD_VENDOR_ISO_OUT_EP_ADDRESS          0x04

static struct usb_string vendor_strings[] = {
    {
        .id = USBD_VENDOR_STRING_MANUFACTURER,
        .s  = "Realtek",
    },
    {
        .id = USBD_VENDOR_STRING_PRODUCT,
        .s  = "USB Vendor Device",
    },
    {
        .id = USBD_VENDOR_STRING_SERIALNUMBER,
        .s  = "0123456789AB",
    },
    {
        .id = USBD_VENDOR_STRING_STRING_INTERFACE,
        .s  = "USB Vendor Interface",
    }
};

static struct usb_gadget_strings vendor_gadget_string_tab = {
    .language = 0x0409,
    .strings = vendor_strings,
};
    
struct usb_gadget_strings *vendor_gadget_strings[] = {
    &vendor_gadget_string_tab,
    NULL
};

struct usb_device_descriptor vendor_device_desc = {
    .bLength            = USB_DT_DEVICE_SIZE,
    .bDescriptorType    = USB_DT_DEVICE,
    .bcdUSB             = 0x0200,
    .bDeviceClass       = USB_CLASS_PER_INTERFACE,
    .bDeviceSubClass    = 0x00,
    .bDeviceProtocol    = 0x00,
    .bMaxPacketSize0    = 64,    // this will be set automatically depends on ep0 setting
    .idVendor           = REALTEK_USB_VID,
    .idProduct          = REALTEK_USB_PID,
    .bcdDevice          = 0x0100,
    .iManufacturer      = USBD_VENDOR_STRING_MANUFACTURER,
    .iProduct           = USBD_VENDOR_STRING_PRODUCT,
    .iSerialNumber      = USBD_VENDOR_STRING_SERIALNUMBER,
    .bNumConfigurations = 0x01,
};

struct usb_config_descriptor vendor_config_desc = {
    .bLength             = USB_DT_CONFIG_SIZE,
    .bDescriptorType     = USB_DT_CONFIG,
    .bNumInterfaces      = 1,                  // This will be assign automatically
    .bConfigurationValue = 1,
    .iConfiguration      = 0,
	.bmAttributes	     = USB_CONFIG_ATT_ONE, // | USB_CONFIG_ATT_SELFPOWER,
    .bMaxPower           = 50,
};

/* Vendor Interface, Alternate Setting 0*/
struct usb_interface_descriptor vendor_intf0_desc = {
    .bLength            = USB_DT_INTERFACE_SIZE,
    .bDescriptorType    = USB_DT_INTERFACE,
    .bInterfaceNumber   = 0x00, // this will be assign automatically
    .bAlternateSetting  = 0x00,
    .bNumEndpoints      = 0x04,
    .bInterfaceClass    = USB_CLASS_VENDOR_SPEC,
    .bInterfaceSubClass = 0,
    .bInterfaceProtocol = 0,
    .iInterface         = USBD_VENDOR_STRING_STRING_INTERFACE,
};

struct usb_interface_descriptor vendor_intf1_desc = {
    .bLength            = USB_DT_INTERFACE_SIZE,
    .bDescriptorType    = USB_DT_INTERFACE,
    .bInterfaceNumber   = 0x00, // this will be assign automatically
    .bAlternateSetting  = 0x01,
    .bNumEndpoints      = 0x02,
    .bInterfaceClass    = USB_CLASS_VENDOR_SPEC,
    .bInterfaceSubClass = 0,
    .bInterfaceProtocol = 0,
    .iInterface         = USBD_VENDOR_STRING_STRING_INTERFACE,
};

/* Endpoint, EP Bulk IN, for Low-speed/Full-speed */
struct usb_endpoint_descriptor vendor_bulk_source_desc_FS = {
    .bLength          = USB_DT_ENDPOINT_SIZE,
    .bDescriptorType  = USB_DT_ENDPOINT,
    .bEndpointAddress = USBD_VENDOR_BULK_IN_EP_ADDRESS,
    .bmAttributes     = USB_ENDPOINT_XFER_BULK,
    .wMaxPacketSize   = 64,
    .bInterval        = 0x00,

};
    
/* Endpoint, EP Bulk OUT, for Low-speed/Full-speed */
struct usb_endpoint_descriptor vendor_bulk_sink_desc_FS = {
    .bLength          = USB_DT_ENDPOINT_SIZE,
    .bDescriptorType  = USB_DT_ENDPOINT,
    .bEndpointAddress = USBD_VENDOR_BULK_OUT_EP_ADDRESS,
    .bmAttributes     = USB_ENDPOINT_XFER_BULK,
    .wMaxPacketSize   = 64,
    .bInterval        = 0x00,
};

/* Endpoint, EP Bulk IN, for High-speed */
struct usb_endpoint_descriptor vendor_bulk_source_desc_HS = {
    .bLength          = USB_DT_ENDPOINT_SIZE,
    .bDescriptorType  = USB_DT_ENDPOINT,
    .bEndpointAddress = USBD_VENDOR_BULK_IN_EP_ADDRESS,
    .bmAttributes     = USB_ENDPOINT_XFER_BULK,
    .wMaxPacketSize   = 512,
    .bInterval        = 0x00,
};

/* Endpoint, EP Bulk OUT, for High-speed */
struct usb_endpoint_descriptor vendor_bulk_sink_desc_HS = {
    .bLength          = USB_DT_ENDPOINT_SIZE,
    .bDescriptorType  = USB_DT_ENDPOINT,
    .bEndpointAddress = USBD_VENDOR_BULK_OUT_EP_ADDRESS,
    .bmAttributes     = USB_ENDPOINT_XFER_BULK,
    .wMaxPacketSize   = 512,
    .bInterval        = 0x00,
};

/* Endpoint, EP ISO IN, for Low-speed/Full-speed */
struct usb_endpoint_descriptor vendor_iso_source_desc_FS = {
    .bLength          = USB_DT_ENDPOINT_SIZE,
    .bDescriptorType  = USB_DT_ENDPOINT,
    .bEndpointAddress = USBD_VENDOR_ISO_IN_EP_ADDRESS,
    .bmAttributes     = USB_ENDPOINT_XFER_ISOC,
    .wMaxPacketSize   = 128,
    .bInterval        = 0x01,

};
/* Endpoint, EP ISO OUT, for Low-speed/Full-speed */
struct usb_endpoint_descriptor vendor_iso_sink_desc_FS = {
    .bLength          = USB_DT_ENDPOINT_SIZE,
    .bDescriptorType  = USB_DT_ENDPOINT,
    .bEndpointAddress = USBD_VENDOR_ISO_OUT_EP_ADDRESS,
    .bmAttributes     = USB_ENDPOINT_XFER_ISOC,
    .wMaxPacketSize   = 512,
    .bInterval        = 0x01,
};

/* Endpoint, EP ISO IN, for High-speed */
struct usb_endpoint_descriptor vendor_iso_source_desc_HS = {
    .bLength          = USB_DT_ENDPOINT_SIZE,
    .bDescriptorType  = USB_DT_ENDPOINT,
    .bEndpointAddress = USBD_VENDOR_ISO_IN_EP_ADDRESS,
    .bmAttributes     = USB_ENDPOINT_XFER_ISOC,
    .wMaxPacketSize   = 512,
    .bInterval        = 0x02,
};

/* Endpoint, EP ISO OUT, for High-speed */
struct usb_endpoint_descriptor vendor_iso_sink_desc_HS = {
    .bLength          = USB_DT_ENDPOINT_SIZE,
    .bDescriptorType  = USB_DT_ENDPOINT,
    .bEndpointAddress = USBD_VENDOR_ISO_OUT_EP_ADDRESS,
    .bmAttributes     = USB_ENDPOINT_XFER_ISOC,
    .wMaxPacketSize   = 512,
    .bInterval        = 0x02,
};

struct usb_descriptor_header *vendor_descriptors_FS[] = {
    /* data interface has no altsetting */
    (struct usb_descriptor_header *) &vendor_intf0_desc,
    (struct usb_descriptor_header *) &vendor_bulk_source_desc_FS,
    (struct usb_descriptor_header *) &vendor_bulk_sink_desc_FS,
    (struct usb_descriptor_header *) &vendor_iso_source_desc_FS,
    (struct usb_descriptor_header *) &vendor_iso_sink_desc_FS,
    (struct usb_descriptor_header *) &vendor_intf1_desc,
    (struct usb_descriptor_header *) &vendor_bulk_source_desc_FS,
    (struct usb_descriptor_header *) &vendor_bulk_sink_desc_FS,
    NULL,
};

struct usb_descriptor_header *vendor_descriptors_HS[] = {
    /* data interface has no altsetting */
    (struct usb_descriptor_header *) &vendor_intf0_desc,
    (struct usb_descriptor_header *) &vendor_bulk_source_desc_HS,
    (struct usb_descriptor_header *) &vendor_bulk_sink_desc_HS,
    (struct usb_descriptor_header *) &vendor_iso_source_desc_HS,
    (struct usb_descriptor_header *) &vendor_iso_sink_desc_HS,
    (struct usb_descriptor_header *) &vendor_intf1_desc,
    (struct usb_descriptor_header *) &vendor_bulk_source_desc_HS,
    (struct usb_descriptor_header *) &vendor_bulk_sink_desc_HS,
    NULL,
};

#endif // CONFIG_USBD_VENDOR

