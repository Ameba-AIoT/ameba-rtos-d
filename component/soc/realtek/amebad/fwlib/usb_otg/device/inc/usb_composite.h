#ifndef _USB_COMPOSITE_H_
#define _USB_COMPOSITE_H_

#include "usb.h"
#include "dwc_otg_pcd.h"

#define MAX_CONFIG_INTERFACES           16  /* arbitrary; max 255 */

// EP0 Data Direction
#define EP0_DATA_OUT 0
#define EP0_DATA_IN  1

// Default PID/VID
#define REALTEK_USB_VID             0x0BDA
#define REALTEK_USB_PID             0x4042

/*
 * USB function drivers should return USB_GADGET_DELAYED_STATUS if they
 * wish to delay the data/status stages of the control transfer till they
 * are ready.
 */
#define USB_GADGET_DELAYED_STATUS   0x7FFF

#ifndef MIN
#define MIN(x, y) ((x) < (y) ? (x) : (y))
#endif
#ifndef MAX
#define MAX(x, y) ((x) > (y) ? (x) : (y))
#endif

// Predefine Structure
struct usb_composite_dev;
struct usb_composite_driver;
struct usb_request;
struct usb_ep_ops;
struct usb_gadget;

typedef unsigned int gfp_t;

/**
 * struct usb_ep - device side representation of USB endpoint
 * @name:identifier for the endpoint, such as "ep-a" or "ep9in-bulk"
 * @ops: Function pointers used to access hardware-specific operations.
 * @ep_list:the gadget's ep_list holds all of its endpoints
 * @maxpacket:The maximum packet size used on this endpoint.  The initial
 *  value can sometimes be reduced (hardware allowing), according to
 *      the endpoint descriptor used to configure the endpoint.
 * @driver_data:for use by the gadget driver.  all other fields are
 *  read-only to gadget drivers.
 *
 * the bus controller driver lists all the general purpose endpoints in
 * gadget->ep_list.  the control endpoint (gadget->ep0) is not in that list,
 * and is accessed only in response to a driver setup() callback.
 */
struct usb_ep {
    void *driver_data;
    const char *name;
    const struct usb_ep_ops *ops;
    dwc_list_link_t ep_list;
    unsigned maxpacket: 16;
    const struct usb_endpoint_descriptor *desc;
};

typedef void (*usb_req_complete_t)(struct usb_ep *, struct usb_request *);

/**
 * struct usb_request - describes one i/o request
 * @buf: Buffer used for data.  Always provide this; some controllers
 *  only use PIO, or don't use DMA for some endpoints.
 * @dma: DMA address corresponding to 'buf'.  If you don't set this
 *  field, and the usb controller needs one, it is responsible
 *  for mapping and unmapping the buffer.
 * @length: Length of that data
 * @no_interrupt: If true, hints that no completion irq is needed.
 *  Helpful sometimes with deep request queues that are handled
 *  directly by DMA controllers.
 * @zero: If true, when writing data, makes the last packet be "short"
 *     by adding a zero length packet as needed;
 * @short_not_ok: When reading data, makes short packets be
 *     treated as errors (queue stops advancing till cleanup).
 * @complete: Function called when request completes, so this request and
 *  its buffer may be re-used.
 *  Reads terminate with a short packet, or when the buffer fills,
 *  whichever comes first.  When writes terminate, some data bytes
 *  will usually still be in flight (often in a hardware fifo).
 *  Errors (for reads or writes) stop the queue from advancing
 *  until the completion function returns, so that any transfers
 *  invalidated by the error may first be dequeued.
 * @context: For use by the completion callback
 * @list: For use by the gadget driver.
 * @status: Reports completion code, zero or a negative errno.
 *  Normally, faults block the transfer queue from advancing until
 *  the completion callback returns.
 *  Code "-USB_ESHUTDOWN" indicates completion caused by device disconnect,
 *  or when the driver disabled the endpoint.
 * @actual: Reports bytes transferred to/from the buffer.  For reads (OUT
 *  transfers) this may be less than the requested length.  If the
 *  short_not_ok flag is set, short reads are treated as errors
 *  even when status otherwise indicates successful completion.
 *  Note that for writes (IN transfers) some data bytes may still
 *  reside in a device-side FIFO when the request is reported as
 *  complete.
 *
 * These are allocated/freed through the endpoint they're used with.  The
 * hardware's driver can add extra per-request data to the memory it returns,
 * which often avoids separate memory allocations (potential failures),
 * later when the request is queued.
 *
 * Request flags affect request handling, such as whether a zero length
 * packet is written (the "zero" flag), whether a short read should be
 * treated as an error (blocking request queue advance, the "short_not_ok"
 * flag), or hinting that an interrupt is not required (the "no_interrupt"
 * flag, for use with deep request queues).
 *
 * Bulk endpoints can use any size buffers, and can also be used for interrupt
 * transfers. interrupt-only endpoints can be much less functional.
 */
// NOTE this is analagous to 'struct urb' on the host side,
// except that it's thinner and promotes more pre-allocation.

struct usb_request {
    void *buf;
    unsigned length;
    dma_addr_t dma;

    usb_req_complete_t complete;
    void *context;
    _list list;
    int status;
    unsigned actual;
    unsigned direct;

    unsigned no_interrupt: 1; //not used
    unsigned zero: 1;
    unsigned short_not_ok: 1; // not used
};

// add for iso
struct usb_iso_request {
    void *buf0;
    void *buf1;
    dma_addr_t dma0;
    dma_addr_t dma1;
    /* Time Interval(number of frame) for data exchange, i.e, interval for calling process_buffer(==> a function). */
    /* This variable sould be divisible to bInterval (interms of number of frames, not power of 2) */
    uint32_t buf_proc_intrvl;
    unsigned no_interrupt: 1;
    unsigned zero: 1;
    unsigned short_not_ok: 1;
    uint32_t sync_frame;
    uint32_t data_per_frame;
    uint32_t data_per_frame1;
    uint32_t data_pattern_frame;
    uint32_t start_frame;
    uint32_t flags;
    void (*process_buffer)(struct usb_ep *, struct usb_iso_request *);
    void *context;
    int status;
    struct usb_gadget_iso_packet_descriptor *iso_packet_desc0;
    struct usb_gadget_iso_packet_descriptor *iso_packet_desc1;
    uint32_t proc_buf_num;
};


// add for iso
struct usb_gadget_iso_packet_descriptor {
    unsigned int offset;
    unsigned int length; /* expected length */
    unsigned int actual_length;
    unsigned int status;
};

/*-------------------------------------------------------------------------*/

/* endpoint-specific parts of the api to the usb controller hardware.
 * unlike the urb model, (de)multiplexing layers are not required.
 * (so this api could slash overhead if used on the host side...)
 *
 * note that device side usb controllers commonly differ in how many
 * endpoints they support, as well as their capabilities.
 */
struct usb_ep_ops {
    int (*enable)(struct usb_ep *ep, const struct usb_endpoint_descriptor *desc);
    int (*disable)(struct usb_ep *ep);

    struct usb_request *(*alloc_request)(struct usb_ep *ep, gfp_t gfp_flags);
    void (*free_request)(struct usb_ep *ep, struct usb_request *req);

    int (*queue)(struct usb_ep *ep, struct usb_request *req, gfp_t gfp_flags);
    int (*dequeue)(struct usb_ep *ep, struct usb_request *req);

    int (*set_halt)(struct usb_ep *ep, int value);
    int (*fifo_status)(struct usb_ep *ep);
    void (*fifo_flush)(struct usb_ep *ep);
    
#ifdef USBD_EN_ISOC
    int (*iso_ep_start)(struct usb_ep *, struct usb_iso_request *, gfp_t);
    int (*iso_ep_stop)(struct usb_ep *, struct usb_iso_request *);
    struct usb_iso_request *(*alloc_iso_request)(struct usb_ep *ep, int packets, gfp_t);
    void (*free_iso_request)(struct usb_ep *ep, struct usb_iso_request *req);
#endif
};

struct zero_dev {
    struct usb_gadget *gadget;
    struct usb_request *req;       /* for control responses */

    /* when configured, we have one of two configs:
     * - source data (in to host) and sink it (out from host)
     * - or loop it back (out from host back in to host)
     */
    u8 config;
    
    struct usb_ep *in_ep;
    struct usb_ep *out_ep;
    struct usb_ep *status_ep;
    
    const struct usb_endpoint_descriptor *in;
    const struct usb_endpoint_descriptor *out;
    const struct usb_endpoint_descriptor *status;
};

/*-------------------------------------------------------------------------*/
/**
 * struct usb_gadget - represents a usb slave device
 * @ops: Function pointers used to access hardware-specific operations.
 * @ep0: Endpoint zero, used when reading or writing responses to
 *  driver setup() requests
 * @ep_list: List of other endpoints supported by the device.
 * @speed: Speed of current connection to USB host.
 * @is_dualspeed: True if the controller supports both high and full speed
 *  operation.  If it does, the gadget driver must also support both.
 * @is_otg: True if the USB device port uses a Mini-AB jack, so that the
 *  gadget driver must provide a USB OTG descriptor.
 * @is_a_peripheral: False unless is_otg, the "A" end of a USB cable
 *  is in the Mini-AB jack, and HNP has been used to switch roles
 *  so that the "A" device currently acts as A-Peripheral, not A-Host.
 * @a_hnp_support: OTG device feature flag, indicating that the A-Host
 *  supports HNP at this port.
 * @a_alt_hnp_support: OTG device feature flag, indicating that the A-Host
 *  only supports HNP on a different root port.
 * @b_hnp_enable: OTG device feature flag, indicating that the A-Host
 *  enabled HNP support.
 * @name: Identifies the controller hardware type.  Used in diagnostics
 *  and sometimes configuration.
 * @dev: Driver model state for this abstract device.
 *
 * Gadgets have a mostly-portable "gadget driver" implementing device
 * functions, handling all usb configurations and interfaces.  Gadget
 * drivers talk to hardware-specific code indirectly, through ops vectors.
 * That insulates the gadget driver from hardware details, and packages
 * the hardware endpoints through generic i/o queues.  The "usb_gadget"
 * and "usb_ep" interfaces provide that insulation from the hardware.
 *
 * Except for the driver data, all fields in this structure are
 * read-only to the gadget driver.  That driver data is part of the
 * "driver model" infrastructure in 2.6 (and later) kernels, and for
 * earlier systems is grouped in a similar structure that's not known
 * to the rest of the kernel.
 *
 * Values of the three OTG device feature flags are updated before the
 * setup() call corresponding to USB_REQ_SET_CONFIGURATION, and before
 * driver suspend() calls.  They are valid only when is_otg, and when the
 * device is acting as a B-Peripheral (so is_a_peripheral is false).
 */
struct usb_gadget {
    struct usb_ep         *ep0;
    dwc_list_link_t        ep_list; // by jimmy

    int                    speed;
    int                    max_speed;
    unsigned               is_dualspeed: 1;
    unsigned               is_otg: 1;
    unsigned               is_a_peripheral: 1;
    unsigned               b_hnp_enable: 1;
    unsigned               a_hnp_support: 1;
    unsigned               a_alt_hnp_support: 1;
    struct zero_dev        dev;
    void                  *driver_data;
    void                  *device;
};

/*-------------------------------------------------------------------------*/

/**
 * struct usb_gadget_driver - driver for usb 'slave' devices
 * @function: String describing the gadget's function
 * @speed: Highest speed the driver handles.
 * @bind: Invoked when the driver is bound to a gadget, usually
 *  after registering the driver.
 *  At that point, ep0 is fully initialized, and ep_list holds
 *  the currently-available endpoints.
 *  Called in a context that permits sleeping.
 * @setup: Invoked for ep0 control requests that aren't handled by
 *  the hardware level driver. Most calls must be handled by
 *  the gadget driver, including descriptor and configuration
 *  management.  The 16 bit members of the setup data are in
 *  USB byte order. Called in_interrupt; this may not sleep.  Driver
 *  queues a response to ep0, or returns negative to stall.
 * @disconnect: Invoked after all transfers have been stopped,
 *  when the host is disconnected.  May be called in_interrupt; this
 *  may not sleep.  Some devices can't detect disconnect, so this might
 *  not be called except as part of controller shutdown.
 * @unbind: Invoked when the driver is unbound from a gadget,
 *  usually from rmmod (after a disconnect is reported).
 *  Called in a context that permits sleeping.
 * @suspend: Invoked on USB suspend.  May be called in_interrupt.
 * @resume: Invoked on USB resume.  May be called in_interrupt.
 * @driver: Driver model state for this driver.
 *
 * Devices are disabled till a gadget driver successfully bind()s, which
 * means the driver will handle setup() requests needed to enumerate (and
 * meet "chapter 9" requirements) then do some useful work.
 *
 * If gadget->is_otg is true, the gadget driver must provide an OTG
 * descriptor during enumeration, or else fail the bind() call.  In such
 * cases, no USB traffic may flow until both bind() returns without
 * having called usb_gadget_disconnect(), and the USB host stack has
 * initialized.
 *
 * Drivers use hardware-specific knowledge to configure the usb hardware.
 * endpoint addressing is only one of several hardware characteristics that
 * are in descriptors the ep0 implementation returns from setup() calls.
 *
 * Except for ep0 implementation, most driver code shouldn't need change to
 * run on top of different usb controllers.  It'll use endpoints set up by
 * that ep0 implementation.
 *
 * The usb controller driver handles a few standard usb requests.  Those
 * include set_address, and feature flags for devices, interfaces, and
 * endpoints (the get_status, set_feature, and clear_feature requests).
 *
 * Accordingly, the driver's setup() callback must always implement all
 * get_descriptor requests, returning at least a device descriptor and
 * a configuration descriptor.  Drivers must make sure the endpoint
 * descriptors match any hardware constraints. Some hardware also constrains
 * other descriptors. (The pxa250 allows only configurations 1, 2, or 3).
 *
 * The driver's setup() callback must also implement set_configuration,
 * and should also implement set_interface, get_configuration, and
 * get_interface.  Setting a configuration (or interface) is where
 * endpoints should be activated or (config 0) shut down.
 *
 * (Note that only the default control endpoint is supported.  Neither
 * hosts nor devices generally support control traffic except to ep0.)
 *
 * Most devices will ignore USB suspend/resume operations, and so will
 * not provide those callbacks.  However, some may need to change modes
 * when the host is not longer directing those activities.  For example,
 * local controls (buttons, dials, etc) may need to be re-enabled since
 * the (remote) host can't do that any longer; or an error state might
 * be cleared, to make the device behave identically whether or not
 * power is maintained.
 */
struct usb_gadget_driver {
    int (*bind)(struct usb_gadget *, struct usb_gadget_driver *);
    void (*unbind)(struct usb_gadget *);
    int (*setup)(struct usb_gadget *,   const struct usb_control_request *);
    void (*disconnect)(struct usb_gadget *);
    void (*suspend)(struct usb_gadget *);
    void (*resume)(struct usb_gadget *);
    int (*clr_feature)(struct usb_gadget *,   const struct usb_control_request *);

    void *driver;
    struct usb_config_descriptor *config_desc;
};

struct gadget_wrapper {
    dwc_otg_pcd_t *pcd;

    struct usb_gadget gadget;
    struct usb_gadget_driver *driver;

    struct usb_ep ep0;
    struct usb_ep in_ep[16];
    struct usb_ep out_ep[16];
};

/*-------------------------------------------------------------------------*/

/* utility to simplify dealing with string descriptors */

/**
 * struct usb_string - wraps a C string and its USB id
 * @id:the (nonzero) ID for this string
 * @s:the string, in UTF-8 encoding
 *
 * If you're using usb_gadget_get_string(), use this to wrap a string
 * together with its ID.
 */
struct usb_string {
    u8 id;
    const char *s;
};

/**
 * struct usb_gadget_strings - a set of USB strings in a given language
 * @language:identifies the strings' language (0x0409 for en-us)
 * @strings:array of strings with their ids
 *
 * If you're using usb_gadget_get_string(), use this to wrap all the
 * strings for a given language.
 */
struct usb_gadget_strings {
    u16 language;   /* 0x0409 for en-us */
    struct usb_string *strings;
};

/**
 * struct usb_configuration - represents one gadget configuration
 * @label: For diagnostics, describes the configuration.
 * @strings: Tables of strings, keyed by identifiers assigned during @bind()
 *  and by language IDs provided in control requests.
 * @descriptors: Table of descriptors preceding all function descriptors.
 *  Examples include OTG and vendor-specific descriptors.
 * @unbind: Reverses @bind; called as a side effect of unregistering the
 *  driver which added this configuration.
 * @setup: Used to delegate control requests that aren't handled by standard
 *  device infrastructure or directed at a specific interface.
 * @bConfigurationValue: Copied into configuration descriptor.
 * @iConfiguration: Copied into configuration descriptor.
 * @bmAttributes: Copied into configuration descriptor.
 * @MaxPower: Power consumtion in mA. Used to compute bMaxPower in the
 *  configuration descriptor after considering the bus speed.
 * @cdev: assigned by @usb_add_config() before calling @bind(); this is
 *  the device associated with this configuration.
 *
 * Configurations are building blocks for gadget drivers structured around
 * function drivers.  Simple USB gadgets require only one function and one
 * configuration, and handle dual-speed hardware by always providing the same
 * functionality.  Slightly more complex gadgets may have more than one
 * single-function configuration at a given speed; or have configurations
 * that only work at one speed.
 *
 * Composite devices are, by definition, ones with configurations which
 * include more than one function.
 *
 * The lifecycle of a usb_configuration includes allocation, initialization
 * of the fields described above, and calling @usb_add_config() to set up
 * internal data and bind it to a specific device.  The configuration's
 * @bind() method is then used to initialize all the functions and then
 * call @usb_add_function() for them.
 *
 * Those functions would normally be independent of each other, but that's
 * not mandatory.  CDC WMC devices are an example where functions often
 * depend on other functions, with some functions subsidiary to others.
 * Such interdependency may be managed in any way, so long as all of the
 * descriptors complete by the time the composite driver returns from
 * its bind() routine.
 */
struct usb_configuration {
    //const char          *label;
    struct usb_gadget_strings   **strings;
    const struct usb_descriptor_header **descriptors;

    /* REVISIT:  bind() functions can be marked __init, which
     * makes trouble for section mismatch analysis.  See if
     * we can't restructure things to avoid mismatching...
     */

    /* configuration management: unbind/setup */
    //void (*unbind)(struct usb_configuration *);
    //int (*setup)(struct usb_configuration *, const struct usb_control_request *);

#if 1
    struct usb_config_descriptor *config_descriptor;
#else
    /* fields in the config descriptor */
    uint8_t         bConfigurationValue;
    uint8_t         iConfiguration;
    uint8_t         bmAttributes;
    uint16_t        MaxPower;
#endif

    struct usb_composite_dev    *cdev;
    dwc_list_link_t list;
    dwc_list_link_t function_lists;

    uint8_t         next_interface_id;
    unsigned        superspeed: 1;
    unsigned        highspeed: 1;
    unsigned        fullspeed: 1;
    struct usb_function *interface[MAX_CONFIG_INTERFACES];
};

/**
 * struct usb_composite_driver - groups configurations into a gadget
 * @name: For diagnostics, identifies the driver.
 * @dev: Template descriptor for the device, including default device
 *  identifiers.
 * @strings: tables of strings, keyed by identifiers assigned during @bind
 *  and language IDs provided in control requests. Note: The first entries
 *  are predefined. The first entry that may be used is
 *  USB_GADGET_FIRST_AVAIL_IDX
 * @max_speed: Highest speed the driver supports.
 * @needs_serial: set to 1 if the gadget needs userspace to provide
 *  a serial number.  If one is not provided, warning will be printed.
 * @bind: (REQUIRED) Used to allocate resources that are shared across the
 *  whole device, such as string IDs, and add its configurations using
 *  @usb_add_config(). This may fail by returning a negative errno
 *  value; it should return zero on successful initialization.
 * @unbind: Reverses @bind; called as a side effect of unregistering
 *  this driver.
 * @disconnect: optional driver disconnect method
 * @suspend: Notifies when the host stops sending USB traffic,
 *  after function notifications
 * @resume: Notifies configuration when the host restarts USB traffic,
 *  before function notifications
 * @gadget_driver: Gadget driver controlling this driver
 *
 * Devices default to reporting self powered operation.  Devices which rely
 * on bus powered operation should report this in their @bind method.
 *
 * Before returning from @bind, various fields in the template descriptor
 * may be overridden.  These include the idVendor/idProduct/bcdDevice values
 * normally to bind the appropriate host side driver, and the three strings
 * (iManufacturer, iProduct, iSerialNumber) normally used to provide user
 * meaningful device identifiers.  (The strings will not be defined unless
 * they are defined in @dev and @strings.)  The correct ep0 maxpacket size
 * is also reported, as defined by the underlying controller driver.
 */

struct usb_composite_driver {
    struct usb_device_descriptor   *dev_desc;
    struct usb_config_descriptor   *config_desc;
    struct usb_gadget_strings     **strings;
    struct usb_function            *functions;

    int (*config)(struct usb_configuration *cfg);

    //int (*bind)(struct usb_composite_dev *cdev);
    //int (*unbind)(struct usb_composite_dev *cdev);

    //void (*disconnect)(struct usb_composite_dev *cdev);

    /* global suspend hooks */
    //void (*suspend)(struct usb_composite_dev *cdev);
    //void (*resume)(struct usb_composite_dev *cdev);
    struct usb_gadget_driver        gadget_driver;
    void                           *driver_data;
};
/**
 * struct usb_composite_device - represents one composite usb gadget
 * @gadget: read-only, abstracts the gadget's usb peripheral controller
 * @req: used for control responses; buffer is pre-allocated
 * @os_desc_req: used for OS descriptors responses; buffer is pre-allocated
 * @config: the currently active configuration
 * @qw_sign: qwSignature part of the OS string
 * @b_vendor_code: bMS_VendorCode part of the OS string
 * @use_os_string: false by default, interested gadgets set it
 * @os_desc_config: the configuration to be used with OS descriptors
 *
 * One of these devices is allocated and initialized before the
 * associated device driver's bind() is called.
 *
 * OPEN ISSUE:  it appears that some WUSB devices will need to be
 * built by combining a normal (wired) gadget with a wireless one.
 * This revision of the gadget framework should probably try to make
 * sure doing that won't hurt too much.
 *
 * One notion for how to handle Wireless USB devices involves:
 * (a) a second gadget here, discovery mechanism TBD, but likely
 *     needing separate "register/unregister WUSB gadget" calls;
 * (b) updates to usb_gadget to include flags "is it wireless",
 *     "is it wired", plus (presumably in a wrapper structure)
 *     bandgroup and PHY info;
 * (c) presumably a wireless_ep wrapping a usb_ep, and reporting
 *     wireless-specific parameters like maxburst and maxsequence;
 * (d) configurations that are specific to wireless links;
 * (e) function drivers that understand wireless configs and will
 *     support wireless for (additional) function instances;
 * (f) a function to support association setup (like CBAF), not
 *     necessarily requiring a wireless adapter;
 * (g) composite device setup that can create one or more wireless
 *     configs, including appropriate association setup support;
 * (h) more, TBD.
 */

struct usb_composite_dev {
    struct usb_gadget           *gadget;
    struct usb_request          *req;
    struct usb_configuration    *config;
    struct usb_device_descriptor dev_desc;

    dwc_list_link_t              config_list; // by jimmy
    dwc_list_link_t              gstring_list;// by jimmy

    struct usb_composite_driver *driver;
    unsigned int                 suspended: 1;
};

/**
 * struct usb_function - describes one function of a configuration
 * @name: For diagnostics, identifies the function.
 * @strings: tables of strings, keyed by identifiers assigned during bind()
 *  and by language IDs provided in control requests
 * @fs_descriptors: Table of full (or low) speed descriptors, using interface and
 *  string identifiers assigned during @bind().  If this pointer is null,
 *  the function will not be available at full speed (or at low speed).
 * @hs_descriptors: Table of high speed descriptors, using interface and
 *  string identifiers assigned during @bind().  If this pointer is null,
 *  the function will not be available at high speed.
 * @ss_descriptors: Table of super speed descriptors, using interface and
 *  string identifiers assigned during @bind(). If this
 *  pointer is null after initiation, the function will not
 *  be available at super speed.
 * @config: assigned when @usb_add_function() is called; this is the
 *  configuration with which this function is associated.
 * @os_desc_table: Table of (interface id, os descriptors) pairs. The function
 *  can expose more than one interface. If an interface is a member of
 *  an IAD, only the first interface of IAD has its entry in the table.
 * @os_desc_n: Number of entries in os_desc_table
 * @bind: Before the gadget can register, all of its functions bind() to the
 *  available resources including string and interface identifiers used
 *  in interface or class descriptors; endpoints; I/O buffers; and so on.
 * @unbind: Reverses @bind; called as a side effect of unregistering the
 *  driver which added this function.
 * @free_func: free the struct usb_function.
 * @set_alt: (REQUIRED) Reconfigures altsettings; function drivers may
 *  initialize usb_ep.driver data at this time (when it is used).
 *  Note that setting an interface to its current altsetting resets
 *  interface state, and that all interfaces have a disabled state.
 * @get_alt: Returns the active altsetting.  If this is not provided,
 *  then only altsetting zero is supported.
 * @disable: (REQUIRED) Indicates the function should be disabled.  Reasons
 *  include host resetting or reconfiguring the gadget, and disconnection.
 * @setup: Used for interface-specific control requests.
 * @suspend: Notifies functions when the host stops sending USB traffic.
 * @resume: Notifies functions when the host restarts USB traffic.
 * @get_status: Returns function status as a reply to
 *  GetStatus() request when the recipient is Interface.
 * @func_suspend: callback to be called when
 *  SetFeature(FUNCTION_SUSPEND) is reseived
 *
 * A single USB function uses one or more interfaces, and should in most
 * cases support operation at both full and high speeds.  Each function is
 * associated by @usb_add_function() with a one configuration; that function
 * causes @bind() to be called so resources can be allocated as part of
 * setting up a gadget driver.  Those resources include endpoints, which
 * should be allocated using @usb_ep_autoconfig().
 *
 * To support dual speed operation, a function driver provides descriptors
 * for both high and full speed operation.  Except in rare cases that don't
 * involve bulk endpoints, each speed needs different endpoint descriptors.
 *
 * Function drivers choose their own strategies for managing instance data.
 * The simplest strategy just declares it "static', which means the function
 * can only be activated once.  If the function needs to be exposed in more
 * than one configuration at a given speed, it needs to support multiple
 * usb_function structures (one for each configuration).
 *
 * A more complex strategy might encapsulate a @usb_function structure inside
 * a driver-specific instance structure to allows multiple activations.  An
 * example of multiple activations might be a CDC ACM function that supports
 * two or more distinct instances within the same configuration, providing
 * several independent logical data links to a USB host.
 */

struct usb_function {
    //const char                    *name;
    struct usb_gadget_strings    **strings;
    struct usb_descriptor_header **fs_descriptors;
    struct usb_descriptor_header **hs_descriptors;
    struct usb_configuration      *config;

    /* REVISIT:  bind() functions can be marked __init, which
     * makes trouble for section mismatch analysis.  See if
     * we can't restructure things to avoid mismatching.
     * Related:  unbind() may kfree() but bind() won't...
     */

    /* configuration management:  bind/unbind */
    int (*bind)(struct usb_configuration *, struct usb_function *);
    void (*unbind)(struct usb_configuration *, struct usb_function *);
    void (*free_func)(struct usb_function *f);

    /* runtime state management */
    int (*set_alt)(struct usb_function *, unsigned interface, unsigned alt);
    int (*get_alt)(struct usb_function *, unsigned interface);
    void (*disable)(struct usb_function *);
    int (*setup)(struct usb_function *, const struct usb_control_request *);
    int (*clr_feature)(struct usb_function *, const struct usb_control_request *);
    void (*suspend)(struct usb_function *);
    void (*resume)(struct usb_function *);

    /* USB 3.0 additions */
    int (*get_status)(struct usb_function *);
    dwc_list_link_t list; // by jimmy
    uint32_t function_endp_bitmap;//bitmap// Rom2 add: store which endpoint belongs to this function
};

inline struct usb_endpoint_descriptor *usb_get_descriptor(struct usb_gadget *gadget,
    struct usb_endpoint_descriptor *hs, struct usb_endpoint_descriptor *fs)
{
    return (gadget->speed == USB_SPEED_HIGH) ? hs : fs;
}

/*-------------------------------------------------------------------------*/

/**
 * usb_ep_enable - configure endpoint, making it usable
 * @ep:the endpoint being configured.  may not be the endpoint named "ep0".
 *  drivers discover endpoints through the ep_list of a usb_gadget.
 * @desc:descriptor for desired behavior.  caller guarantees this pointer
 *  remains valid until the endpoint is disabled; the data byte order
 *  is little-endian (usb-standard).
 *
 * when configurations are set, or when interface settings change, the driver
 * will enable or disable the relevant endpoints.  while it is enabled, an
 * endpoint may be used for i/o until the driver receives a disconnect() from
 * the host or until the endpoint is disabled.
 *
 * the ep0 implementation (which calls this routine) must ensure that the
 * hardware capabilities of each endpoint match the descriptor provided
 * for it.  for example, an endpoint named "ep2in-bulk" would be usable
 * for interrupt transfers as well as bulk, but it likely couldn't be used
 * for iso transfers or for endpoint 14.  some endpoints are fully
 * configurable, with more generic names like "ep-a".  (remember that for
 * USB, "in" means "towards the USB master".)
 *
 * returns zero, or a negative error code.
 */
inline int usb_ep_enable(struct usb_ep *ep, const struct usb_endpoint_descriptor *desc)
{
    return ep->ops->enable(ep, desc);
}

/**
 * usb_ep_disable - endpoint is no longer usable
 * @ep:the endpoint being unconfigured.  may not be the endpoint named "ep0".
 *
 * no other task may be using this endpoint when this is called.
 * any pending and uncompleted requests will complete with status
 * indicating disconnect (-USB_ESHUTDOWN) before this call returns.
 * gadget drivers must call usb_ep_enable() again before queueing
 * requests to the endpoint.
 *
 * returns zero, or a negative error code.
 */
inline int usb_ep_disable(struct usb_ep *ep)
{
    return ep->ops->disable(ep);    // ep_disable()
}

/**
 * usb_ep_alloc_request - allocate a request object to use with this endpoint
 * @ep:the endpoint to be used with with the request
 * @gfp_flags:GFP_* flags to use
 *
 * Request objects must be allocated with this call, since they normally
 * need controller-specific setup and may even need endpoint-specific
 * resources such as allocation of DMA descriptors.
 * Requests may be submitted with usb_ep_queue(), and receive a single
 * completion callback.  Free requests with usb_ep_free_request(), when
 * they are no longer needed.
 *
 * Returns the request, or null if one could not be allocated.
 */
inline struct usb_request *usb_ep_alloc_request(struct usb_ep *ep, gfp_t gfp_flags)
{
    return ep->ops->alloc_request(ep, gfp_flags);  //dwc_otg_pcd_alloc_request()
}

/**
 * usb_ep_free_request - frees a request object
 * @ep:the endpoint associated with the request
 * @req:the request being freed
 *
 * Reverses the effect of usb_ep_alloc_request().
 * Caller guarantees the request is not queued, and that it will
 * no longer be requeued (or otherwise used).
 */
inline void usb_ep_free_request(struct usb_ep *ep, struct usb_request *req)
{
    ep->ops->free_request(ep, req); // dwc_otg_pcd_free_request()
}

/**
 * usb_ep_queue - queues (submits) an I/O request to an endpoint.
 * @ep:the endpoint associated with the request
 * @req:the request being submitted
 * @gfp_flags: GFP_* flags to use in case the lower level driver couldn't
 *  pre-allocate all necessary memory with the request.
 *
 * This tells the device controller to perform the specified request through
 * that endpoint (reading or writing a buffer).  When the request completes,
 * including being canceled by usb_ep_dequeue(), the request's completion
 * routine is called to return the request to the driver.  Any endpoint
 * (except control endpoints like ep0) may have more than one transfer
 * request queued; they complete in FIFO order.  Once a gadget driver
 * submits a request, that request may not be examined or modified until it
 * is given back to that driver through the completion callback.
 *
 * Each request is turned into one or more packets.  The controller driver
 * never merges adjacent requests into the same packet.  OUT transfers
 * will sometimes use data that's already buffered in the hardware.
 * Drivers can rely on the fact that the first byte of the request's buffer
 * always corresponds to the first byte of some USB packet, for both
 * IN and OUT transfers.
 *
 * Bulk endpoints can queue any amount of data; the transfer is packetized
 * automatically.  The last packet will be short if the request doesn't fill it
 * out completely.  Zero length packets (ZLPs) should be avoided in portable
 * protocols since not all usb hardware can successfully handle zero length
 * packets.  (ZLPs may be explicitly written, and may be implicitly written if
 * the request 'zero' flag is set.)  Bulk endpoints may also be used
 * for interrupt transfers; but the reverse is not true, and some endpoints
 * won't support every interrupt transfer.  (Such as 768 byte packets.)
 *
 * Interrupt-only endpoints are less functional than bulk endpoints, for
 * example by not supporting queueing or not handling buffers that are
 * larger than the endpoint's maxpacket size.  They may also treat data
 * toggle differently.
 *
 * Control endpoints ... after getting a setup() callback, the driver queues
 * one response (even if it would be zero length).  That enables the
 * status ack, after transfering data as specified in the response.  Setup
 * functions may return negative error codes to generate protocol stalls.
 * (Note that some USB device controllers disallow protocol stall responses
 * in some cases.)  When control responses are deferred (the response is
 * written after the setup callback returns), then usb_ep_set_halt() may be
 * used on ep0 to trigger protocol stalls.
 *
 * For periodic endpoints, like interrupt or isochronous ones, the usb host
 * arranges to poll once per interval, and the gadget driver usually will
 * have queued some data to transfer at that time.
 *
 * Returns zero, or a negative error code.  Endpoints that are not enabled
 * report errors; errors will also be
 * reported when the usb peripheral is disconnected.
 */
inline int usb_ep_queue(struct usb_ep *ep, void *req, gfp_t gfp_flags)
{
    return ep->ops->queue(ep, req, gfp_flags); // static int ep_queue(...)
}

/**
 * usb_ep_dequeue - dequeues (cancels, unlinks) an I/O request from an endpoint
 * @ep:the endpoint associated with the request
 * @req:the request being canceled
 *
 * if the request is still active on the endpoint, it is dequeued and its
 * completion routine is called (with status -USB_ECONNRESET); else a negative
 * error code is returned.
 *
 * note that some hardware can't clear out write fifos (to unlink the request
 * at the head of the queue) except as part of disconnecting from usb.  such
 * restrictions prevent drivers from supporting configuration changes,
 * even to configuration zero (a "chapter 9" requirement).
 */
inline int usb_ep_dequeue(struct usb_ep *ep, struct usb_request *req)
{
    return ep->ops->dequeue(ep, req);   // ep_dequeue
}

/**
 * usb_ep_set_halt - sets the endpoint halt feature.
 * @ep: the non-isochronous endpoint being stalled
 *
 * Use this to stall an endpoint, perhaps as an error report.
 * Except for control endpoints,
 * the endpoint stays halted (will not stream any data) until the host
 * clears this feature; drivers may need to empty the endpoint's request
 * queue first, to make sure no inappropriate transfers happen.
 *
 * Note that while an endpoint CLEAR_FEATURE will be invisible to the
 * gadget driver, a SET_INTERFACE will not be.  To reset endpoints for the
 * current altsetting, see usb_ep_clear_halt().  When switching altsettings,
 * it's simplest to use usb_ep_enable() or usb_ep_disable() for the endpoints.
 *
 * Returns zero, or a negative error code.  On success, this call sets
 * underlying hardware state that blocks data transfers.
 * Attempts to halt IN endpoints will fail (returning -USB_EAGAIN) if any
 * transfer requests are still queued, or if the controller hardware
 * (usually a FIFO) still holds bytes that the host hasn't collected.
 */
inline int usb_ep_set_halt(struct usb_ep *ep)
{
    return ep->ops->set_halt(ep, 1);
}

#ifdef USBD_EN_ISOC
/**
* Submit an ISOC transfer request to an EP.
*
*  - Every time a sync period completes the request's completion callback
*    is called to provide data to the gadget driver.
*  - Once submitted the request cannot be modified.
*  - Each request is turned into periodic data packets untill ISO
*    Transfer is stopped..
*/
int iso_ep_start(struct usb_ep *usb_ep, struct usb_iso_request *req, gfp_t gfp_flags);

/**
* Stops ISOC EP periodic data transfer.
*/
int iso_ep_stop(struct usb_ep *usb_ep, struct usb_iso_request *req);

/**
* Allocate ISOC transfer request.
*/
struct usb_iso_request *alloc_iso_request(struct usb_ep *ep, int packets, gfp_t gfp_flags);

/**
* Free ISOC transfer request.
*/
void free_iso_request(struct usb_ep *ep, struct usb_iso_request *req);

#endif // USBD_EN_ISOC

/**
* usb_interface_id() - allocate an unused interface ID
* @config: configuration associated with the interface
* @function: function handling the interface
* Context: single threaded during gadget setup
*
* usb_interface_id() is called from usb_function.bind() callbacks to
* allocate new interface IDs.  The function driver will then store that
* ID in interface, association, CDC union, and other descriptors.  It
* will also handle any control requests targeted at that interface,
* particularly changing its altsetting via set_alt().  There may
* also be class-specific or vendor-specific requests to handle.
*
* All interface identifier should be allocated using this routine, to
* ensure that for example different functions don't wrongly assign
* different meanings to the same identifier.  Note that since interface
* identifiers are configuration-specific, functions used in more than
* one configuration (or more than once in a given configuration) need
* multiple versions of the relevant descriptors.
*
* Returns the interface ID which was allocated; or -USB_ENODEV if no
* more interface IDs can be allocated.
*/
int usb_interface_id(struct usb_configuration *config, struct usb_function *function);
    
struct usb_ep *usb_ep_autoconfig(struct usb_gadget *, struct usb_endpoint_descriptor *);

int usb_assign_descriptors(struct usb_function *f,
    struct usb_descriptor_header **fs,
    struct usb_descriptor_header **hs,
    struct usb_descriptor_header **ss);

void usb_free_all_descriptors(struct usb_function *f);

int usb_composite_register(struct usb_composite_driver *driver);

void usb_composite_unregister(struct usb_composite_driver *driver);

#endif

