#include <platform_opts.h>

#ifdef CONFIG_USBH_MSC

#include "usb.h"
#include "storage.h"
#include "us_transport.h"
#include "us_usb.h"
#include "us_usual.h"
#include "us_debug.h"
#include "us_initializers.h"
#include "scsi/scsi.h"
#include "scsi/scsi_cmnd.h"
#include "quirks/quirks.h"
#include "osdep_service.h"

#if defined (CONFIG_PLATFORM_8721D)
#define simple_strtoul(s, ptr, base)		_strtoul(s, ptr, base)
#endif

struct us_data gUS_DATA;
static char quirks[128];
extern _sema us_sto_rdy_sema;
struct us_usr_cb *stor_cb;
_sema us_trd_exit_sema;

/*Example: quirks=0419:aaf5:rl,0421:0433:rc*/


//module_param_string(quirks, quirks, sizeof(quirks), S_IRUGO | S_IWUSR);
//MODULE_PARM_DESC(quirks, "supplemental list of device IDs and their quirks");

bool USB_STORAGE_READY = FALSE;

/*
 * The table of devices
 */
#define UNUSUAL_DEV(idVendor, idProduct, bcdDeviceMin, bcdDeviceMax, \
		    vendor_name, product_name, use_protocol, use_transport, \
		    init_function, Flags) \
{ \
	.vendorName = vendor_name,	\
	.productName = product_name,	\
	.useProtocol = use_protocol,	\
	.useTransport = use_transport,	\
	.initFunction = init_function,	\
}

#define COMPLIANT_DEV	UNUSUAL_DEV

#define USUAL_DEV(use_protocol, use_transport) \
{ \
	.useProtocol = use_protocol,	\
	.useTransport = use_transport,	\
}

#define UNUSUAL_VENDOR_INTF(idVendor, cl, sc, pr, \
		vendor_name, product_name, use_protocol, use_transport, \
		init_function, Flags) \
{ \
	.vendorName = vendor_name,	\
	.productName = product_name,	\
	.useProtocol = use_protocol,	\
	.useTransport = use_transport,	\
	.initFunction = init_function,	\
}

static struct us_unusual_dev us_unusual_dev_list[] = {
#include "unusual_devs.h"
	//NULL//{ }		/* Terminating entry */
};

static struct us_unusual_dev for_dynamic_ids =
		USUAL_DEV(USB_SC_SCSI, USB_PR_BULK);

#undef UNUSUAL_DEV
#undef COMPLIANT_DEV
#undef USUAL_DEV
#undef UNUSUAL_VENDOR_INTF


/* Works only for digits and letters, but small and fast */
#define TOLOWER(x) ((x) | 0x20)

void usb_stor_show_scsi_cmnd(struct scsi_cmnd *srb);

/* Adjust device flags based on the "quirks=" module parameter */
void usb_stor_adjust_quirks(struct usb_device *udev, unsigned long *fflags)
{
	char *p;
	u16 vid = le16_to_cpu(udev->descriptor.idVendor);
	u16 pid = le16_to_cpu(udev->descriptor.idProduct);
	unsigned f = 0;
	unsigned int mask = (US_FL_SANE_SENSE | US_FL_BAD_SENSE |
			US_FL_FIX_CAPACITY | US_FL_IGNORE_UAS |
			US_FL_CAPACITY_HEURISTICS | US_FL_IGNORE_DEVICE |
			US_FL_NOT_LOCKABLE | US_FL_MAX_SECTORS_64 |
			US_FL_CAPACITY_OK | US_FL_IGNORE_RESIDUE |
			US_FL_SINGLE_LUN | US_FL_NO_WP_DETECT |
			US_FL_NO_READ_DISC_INFO | US_FL_NO_READ_CAPACITY_16 |
			US_FL_INITIAL_READ10 | US_FL_WRITE_CACHE);

	p = quirks;
	while (*p) {
		/* Each entry consists of VID:PID:flags */
		if (vid == simple_strtoul(p, &p, 16) &&
				*p == ':' &&
				pid == simple_strtoul(p+1, &p, 16) &&
				*p == ':')
			break;

		/* Move forward to the next entry */
		while (*p) {
			if (*p++ == ',')
				break;
		}
	}
	if (!*p)	/* No match */
		return;

	/* Collect the flags */
	while (*++p && *p != ',') {
		switch (TOLOWER(*p)) {
		case 'a':
			f |= US_FL_SANE_SENSE;
			break;
		case 'b':
			f |= US_FL_BAD_SENSE;
			break;
		case 'c':
			f |= US_FL_FIX_CAPACITY;
			break;
		case 'd':
			f |= US_FL_NO_READ_DISC_INFO;
			break;
		case 'e':
			f |= US_FL_NO_READ_CAPACITY_16;
			break;
		case 'h':
			f |= US_FL_CAPACITY_HEURISTICS;
			break;
		case 'i':
			f |= US_FL_IGNORE_DEVICE;
			break;
		case 'l':
			f |= US_FL_NOT_LOCKABLE;
			break;
		case 'm':
			f |= US_FL_MAX_SECTORS_64;
			break;
		case 'n':
			f |= US_FL_INITIAL_READ10;
			break;
		case 'o':
			f |= US_FL_CAPACITY_OK;
			break;
		case 'p':
			f |= US_FL_WRITE_CACHE;
			break;
		case 'r':
			f |= US_FL_IGNORE_RESIDUE;
			break;
		case 's':
			f |= US_FL_SINGLE_LUN;
			break;
		case 'u':
			f |= US_FL_IGNORE_UAS;
			break;
		case 'w':
			f |= US_FL_NO_WP_DETECT;
			break;
		/* Ignore unrecognized flag characters */
		}
	}
	*fflags = (*fflags & ~mask) | f;
}


/* Associate our private data with the USB device */
static int associate_dev(struct us_data *us, struct usb_interface *intf)
{
US_ENTER;
	/* Fill in the device-related fields */
	us->pusb_dev = interface_to_usbdev(intf);
	us->pusb_intf = intf;
	us->ifnum = intf->cur_altsetting->desc.bInterfaceNumber;
	
	US_INFO("Vendor: 0x%04x, Product: 0x%04x, Revision: 0x%04x\n",
		     le16_to_cpu(us->pusb_dev->descriptor.idVendor),
		     le16_to_cpu(us->pusb_dev->descriptor.idProduct),
		     le16_to_cpu(us->pusb_dev->descriptor.bcdDevice));
	US_INFO("Interface Subclass: 0x%02x, Protocol: 0x%02x\n",
		     intf->cur_altsetting->desc.bInterfaceSubClass,
		     intf->cur_altsetting->desc.bInterfaceProtocol);
	
	/* Store our private data in the interface */
	usb_set_intfdata(intf, us);

	us->cr = (struct usb_ctrlrequest*)rtw_malloc(sizeof(struct usb_ctrlrequest));
	if (!us->cr){
		US_ERR("USB CTRL request allocation failed\n");
		return -USB_ENOMEM;
	}

 	us->iobuf = (unsigned char*)rtw_malloc(US_IOBUF_SIZE);
	if (!us->iobuf) {
		US_ERR("I/O buffer allocation failed\n");
		return -USB_ENOMEM;
	}
US_EXIT;
	return 0;
}

/* Dissociate from the USB device */
static void dissociate_dev(struct us_data *us)
{
	/* Free the buffers */
	rtw_mfree((u8*)us->cr, sizeof(struct usb_ctrlrequest));
	rtw_mfree((u8*)us->iobuf, US_IOBUF_SIZE);
	/* Remove our private data from the interface */
	usb_set_intfdata(us->pusb_intf, NULL);
}


/* Get the unusual_devs entries and the string descriptors */
static int get_device_info(struct us_data *us, const struct usb_device_id *id,
		struct us_unusual_dev *unusual_dev)
{
	struct usb_device *dev = us->pusb_dev;
	struct usb_interface_descriptor *idesc =
		&us->pusb_intf->cur_altsetting->desc;
US_ENTER;
	/* Store the entries */
	us->unusual_dev = unusual_dev;
	us->subclass = (unusual_dev->useProtocol == USB_SC_DEVICE) ?
			idesc->bInterfaceSubClass :
			unusual_dev->useProtocol;
	us->protocol = (unusual_dev->useTransport == USB_PR_DEVICE) ?
			idesc->bInterfaceProtocol :
			unusual_dev->useTransport;
	us->fflags = id->driver_info;
	usb_stor_adjust_quirks(us->pusb_dev, &us->fflags);

	if (us->fflags & US_FL_IGNORE_DEVICE){
		US_ERR("Device ignored\n");
		return -USB_ENODEV;
	}

	/*
	 * This flag is only needed when we're in high-speed, so let's
	 * disable it if we're in full-speed
	 */
	if (dev->speed != USB_SPEED_HIGH)
		us->fflags &= ~US_FL_GO_SLOW;

	if (us->fflags) {
		US_INFO("Quirks match for vid %04x pid %04x: %x\n",
				le16_to_cpu(dev->descriptor.idVendor),
				le16_to_cpu(dev->descriptor.idProduct),
				us->fflags);
    }
    
US_EXIT;
	return 0;
}

/* Get the transport settings */
static void get_transport(struct us_data *us)
{
	switch (us->protocol) {
#if 0
	case USB_PR_CB:
		us->transport_name = "Control/Bulk";
		us->transport = usb_stor_CB_transport;
		us->transport_reset = usb_stor_CB_reset;
		us->max_lun = 7;
		break;

	case USB_PR_CBI:
		us->transport_name = "Control/Bulk/Interrupt";
		us->transport = usb_stor_CB_transport;
		us->transport_reset = usb_stor_CB_reset;
		us->max_lun = 7;
		break;
#endif
	case USB_PR_BULK:
		us->transport_name = "BulkOnly";
		us->transport = usb_stor_Bulk_transport;
		us->transport_reset = usb_stor_Bulk_reset;
		break;
	}
}
/* Get the protocol settings */
static void get_protocol(struct us_data *us)
{
	switch (us->subclass) {
#if 0
	case USB_SC_RBC:
		us->protocol_name = "Reduced Block Commands (RBC)";
		us->proto_handler = usb_stor_transparent_scsi_command;
		break;

	case USB_SC_8020:
		us->protocol_name = "8020i";
		us->proto_handler = usb_stor_pad12_command;
		us->max_lun = 0;
		break;

	case USB_SC_QIC:
		us->protocol_name = "QIC-157";
		us->proto_handler = usb_stor_pad12_command;
		us->max_lun = 0;
		break;

	case USB_SC_8070:
		us->protocol_name = "8070i";
		us->proto_handler = usb_stor_pad12_command;
		us->max_lun = 0;
		break;
	case USB_SC_UFI:
		us->protocol_name = "Uniform Floppy Interface (UFI)";
		us->proto_handler = usb_stor_ufi_command;
		break;
#endif
	case USB_SC_SCSI:
		us->protocol_name = "Protocol SCSI";
		us->proto_handler = usb_stor_transparent_scsi_command;
		break;
	default:
		break;
	}
}

/* Get the pipe settings */
static int get_pipes(struct us_data *us)
{
	struct usb_host_interface *altsetting =
		us->pusb_intf->cur_altsetting;
	int i;
	struct usb_endpoint_descriptor *ep;
	struct usb_endpoint_descriptor *ep_in = NULL;
	struct usb_endpoint_descriptor *ep_out = NULL;
	struct usb_endpoint_descriptor *ep_int = NULL;
US_ENTER;
	/*
	 * Find the first endpoint of each type we need.
	 * We are expecting a minimum of 2 endpoints - in and out (bulk).
	 * An optional interrupt-in is OK (necessary for CBI protocol).
	 * We will ignore any others.
	 */
	for (i = 0; i < altsetting->desc.bNumEndpoints; i++) {
		ep = &altsetting->endpoint[i].desc;
		
		if (usb_endpoint_xfer_bulk(ep)) { // make sure this is bulk transfer ep
			if (usb_endpoint_dir_in(ep)) {
				if (!ep_in)
					ep_in = ep;
			} else {
				if (!ep_out)
					ep_out = ep;
			}
		}

		else if (usb_endpoint_is_int_in(ep)) {
			if (!ep_int)
				ep_int = ep;
		}
	}

	if (!ep_in || !ep_out || (us->protocol == USB_PR_CBI && !ep_int)) {
		US_INFO("Endpoint sanity check failed! Rejecting dev.\n");
		return -USB_EIO;
	}

	/* Calculate and store the pipe values */
	us->send_ctrl_pipe = usb_sndctrlpipe(us->pusb_dev, 0U);
	us->recv_ctrl_pipe = usb_rcvctrlpipe(us->pusb_dev, 0U);
	us->send_bulk_pipe = usb_sndbulkpipe(us->pusb_dev,
		usb_endpoint_num(ep_out));
	us->recv_bulk_pipe = usb_rcvbulkpipe(us->pusb_dev,
		usb_endpoint_num(ep_in));
	if (ep_int) {
		us->recv_intr_pipe = usb_rcvintpipe(us->pusb_dev,
			usb_endpoint_num(ep_int));
		us->ep_bInterval = ep_int->bInterval;
	}
US_EXIT;
	return 0;
}

static u32 rtw_down_sema_stor(_sema *sema)
{
	while(rtw_down_timeout_sema(sema, 3000) != _TRUE) {
		if(!usbh_get_connect_status()) {
			return _FALSE;
		}
	}
	return _TRUE;
}

/**
 * this thread will sleep until any SCSI command transported
 */
void usb_stor_control_thread(void * param)
{
	struct us_data *us = (struct us_data *)param;

    US_INFO("start");
	// Tell the main thread the control thread is already begin and wake up the main thread
	rtw_mutex_put(&(us->notify));

	for (;;) {
		// 1: the usb_stor_release_resource will release this semaphore
		// 2: the queuecommand will release this semaphore
		if(!rtw_down_sema_stor(&(us->cmnd_ready))){ // wait here until awoke
			US_WARN("Get command ready semaphore fail.\n");
			break;
		}

		// protect data when scsi command is running
		/* lock the device pointers */
		rtw_mutex_get(&(us->dev_mutex));
		if(!usbh_get_connect_status()) {
			rtw_mutex_put(&(us->dev_mutex));
			break;
		}
		// protect resource while doing srb operation
		/* When we are called with no command pending, we're done */
		if (us->srb == NULL) {
			rtw_mutex_put(&(us->dev_mutex));
			US_ERR("SRB is NULL, exiting\n");
			break;
		}

		/* has the command timed out *already* ? */
		if (test_bit(US_FLIDX_TIMED_OUT, &us->dflags)) {
			us->srb->result = DID_ABORT << 16;
			US_ERR("Command timeout\n");
			goto SkipForAbort;
		}

		/* reject the command if the direction indicator
		 * is UNKNOWN
		 */
		if (us->srb->sc_data_direction == DMA_BIDIRECTIONAL) {
			US_ERR("UNKNOWN data direction\n");
			us->srb->result = DID_ERROR << 16;
		}
		// for most usb flash driver, it is not support multi-target, so the target id = 0
		// and the target LUN (mcsi LUN) is no more than usb storage LUN
		/* reject if target != 0 or if LUN is higher than
		 * the maximum known LUN
		 */
		else if (us->srb->id &&!(us->fflags & US_FL_SCM_MULT_TARG)) {
			US_ERR("Bad target number (%d:%d)\n",us->srb->id, us->srb->lun);
			us->srb->result = DID_BAD_TARGET << 16;
		}

		else if (us->srb->lun > us->max_lun) {
			US_ERR("Bad LUN (%d:%d)\n", us->srb->id, us->srb->lun);
			us->srb->result = DID_BAD_TARGET << 16;
		}
#if 0
		/* Handle those devices which need us to fake
		 * their inquiry data */   // this software response to UBQYURY cmnd
		else if ((us->srb->cmnd[0] == INQUIRY) &&
			    (us->fflags & US_FL_FIX_INQUIRY)) {
			unsigned char data_ptr[36] = {
			    0x00, 0x80, 0x02, 0x02,
			    0x1F, 0x00, 0x00, 0x00};

			US_INFO("Faking INQUIRY command\n");
			fill_inquiry_response(us, data_ptr, 36);
			us->srb->result = SAM_STAT_GOOD;
		}
#endif
		// execute the scsi command
		/* we've got a command, let's do it! */
		else {
			usb_stor_show_scsi_cmnd(us->srb);
			us->proto_handler(us->srb, us);
		}

		/* indicate that the command is done */
		if (us->srb->result != DID_ABORT << 16) {
			US_INFO("scsi cmd done, result=0x%x\n",us->srb->result);
			us->srb->scsi_done(us->srb);
		} else {
SkipForAbort:
			US_ERR("scsi command aborted\n");
		}

		/* If an abort request was received we need to signal that
		 * the abort has finished.  The proper test for this is
		 * the TIMED_OUT flag, not srb->result == DID_ABORT, because
		 * the timeout might have occurred after the command had
		 * already completed with a different result code. */
		if (test_bit(US_FLIDX_TIMED_OUT, &us->dflags)) {
			rtw_mutex_put(&(us->notify));

			/* Allow USB transfers to resume */
			clear_bit(US_FLIDX_ABORTING, &us->dflags);
			clear_bit(US_FLIDX_TIMED_OUT, &us->dflags);
		}
		/* unlock the device pointers */
		rtw_mutex_put(&us->dev_mutex);
	}
	rtw_up_sema(&us_trd_exit_sema);
	rtw_thread_exit();
}


/* Initialize all the dynamic resources we need */
static int usb_stor_acquire_resources(struct us_data *us)
{
	int p;
US_ENTER;
	us->current_urb = usb_alloc_urb(0);
	if (!us->current_urb) {
		US_ERR("URB allocation failed\n");
		return -USB_ENOMEM;
	}
	/* Just before we start our control thread, initialize
	 * the device if it needs initialization */
	// just for a specific unusual device
	if (us->unusual_dev->initFunction) {
		p = us->unusual_dev->initFunction(us);
		if (p)
			return p;
	}
	
	if(!rtw_create_task(&us->ctl_task, "USB storage thread", 512, tskIDLE_PRIORITY + 2,
						usb_stor_control_thread, us))
	{
		US_ERR("Create Control Task Error");
		return 1;
	 }
US_EXIT;
	return 0;
}

/* Release all our dynamic resources */
static void usb_stor_release_resources(struct us_data *us)
{
	/* Tell the control thread to exit.  The SCSI host must
	 * already have been removed and the DISCONNECTING flag set
	 * so that we won't accept any more commands.
	 */
	US_INFO("sending exit command to thread\n");
	rtw_up_sema(&us->cmnd_ready); // release cmnd_ready semaphore, so
								// the usb_stor_control_thread will exit
	// the task will kill itsself
/*
	if(us->ctl_task)
		vTaskDelete(us->ctl_task);
*/
	/* Call the destructor routine, if it exists */
/*								
	if (us->extra_destructor) {
		US_INFO("-- calling extra_destructor()\n");
		us->extra_destructor(us->extra);
	}
*/
	usb_free_urb(us->current_urb);
}

/* Second stage of disconnect processing: deallocate all resources */
static void release_everything(struct us_data *us)
{
	usb_stor_release_resources(us);
	if(us->pusb_dev &&  us->pusb_intf) {
		usb_disable_interface(us->pusb_dev, us->pusb_intf, TRUE);
	}
	dissociate_dev(us);
	rtw_mutex_free(&(us->dev_mutex));
	rtw_free_sema(&us->cmnd_ready);
	rtw_mutex_free(&(us->notify));
}

/* First part of general USB mass-storage probing */
int usb_stor_probe1(struct us_data *us,
	struct usb_interface *intf,
	const struct usb_device_id *id,
		struct us_unusual_dev *unusual_dev)
{
	int result;
US_ENTER;
	US_INFO("USB Mass Storage device detected\n");

	rtw_mutex_init(&(us->dev_mutex));
	rtw_init_sema(&(us->cmnd_ready), 0); // Init and get the cmnd_ready semaphore, ensure the control_thread will sleep when it start

	rtw_mutex_init(&(us->notify));
	rtw_mutex_get(&(us->notify));

	/* Associate the us_data structure with the USB device */
	result = associate_dev(us, intf);
	if (result)
		goto BadDevice;
	
	/* Get the unusual_devs entries and the descriptors */
	result = get_device_info(us, id, unusual_dev);
	if (result)
		goto BadDevice;

	/* Get standard transport and protocol settings */
	get_transport(us);
	get_protocol(us);

	/* Give the caller a chance to fill in specialized transport
	 * or protocol settings.
	 */
US_EXIT;
	return 0;

BadDevice:
	US_ERR("Failed\n");
	release_everything(us);
US_EXIT;
	return result;
}

/* Second part of general USB mass-storage probing */
int usb_stor_probe2(struct us_data *us)
{
	int result;
US_ENTER;
	/* Make sure the transport and protocol have both been set */
	if (!us->transport || !us->proto_handler) {
		result = -USB_ENXIO;
		goto BadDevice;
	}
	US_INFO(" %s\n", us->transport_name);
	US_INFO(" %s\n", us->protocol_name);

	/* fix for single-lun devices */
	if (us->fflags & US_FL_SINGLE_LUN)
		us->max_lun = 0;

//	if (!(us->fflags & US_FL_SCM_MULT_TARG))
//		us_to_host(us)->max_id = 1;

	/* Find the endpoints and calculate pipe values */
	result = get_pipes(us);
	if (result)
		goto BadDevice;

	/*
	 * If the device returns invalid data for the first READ(10)
	 * command, indicate the command should be retried.
	 */
	if (us->fflags & US_FL_INITIAL_READ10)
		set_bit(US_FLIDX_REDO_READ10, &us->dflags);

	/* Acquire all the other resources and add the host */
	// the usb storage driver core
	result = usb_stor_acquire_resources(us);
	if (result)
		goto BadDevice;
	// Wait for the control thread begin
	rtw_mutex_get(&(us->notify));
	/* For bulk-only devices, determine the max LUN value */
	if (us->protocol == USB_PR_BULK && !(us->fflags & US_FL_SINGLE_LUN)) {
		rtw_mutex_get(&us->dev_mutex);
		us->max_lun = usb_stor_Bulk_max_lun(us);
		rtw_mutex_put(&us->dev_mutex);
	}

	/* Submit the delayed_work for SCSI-device scanning */
//	set_bit(US_FLIDX_SCAN_PENDING, &us->dflags);

US_EXIT;
	return 0;

	/* We come here if there are any problems */
BadDevice:
	US_ERR("Failed %d\n", result);
	release_everything(us);
US_EXIT;
	return result;
}

/* Handle a USB mass-storage disconnect */
void usb_stor_disconnect(struct usb_interface *intf)
{
    struct us_data *us;

    if (intf == NULL) {
        return;
    }
	
    if ( rtw_down_timeout_sema(&us_trd_exit_sema, 5000) == _FAIL) {
        US_ERR("\nGet thread exit sema time out\n");
    }
    
	us = usb_get_intfdata(intf);
US_ENTER;
//	quiesce_and_remove_host(us);
	/* Prevent any new commands from being accepted and cut short
	 * reset delays.
	 */
	set_bit(US_FLIDX_DISCONNECTING, &us->dflags);
	release_everything(us);
	if (stor_cb && stor_cb->detach) {
		stor_cb->detach();
	}
US_EXIT;
}

extern struct usb_device_id usb_storage_usb_ids[];

/* The main probe routine for standard devices */
int storage_probe(struct usb_interface *intf){
	//struct usb_device *udev;
	struct us_unusual_dev *unusual_dev;
	struct usb_device_id *id = NULL;
	struct us_data* us = NULL;
	int result;
	int size;
	int index;
US_ENTER;

//    us = (struct us_data*)RtlZmalloc(sizeof(struct us_data));
	us = &gUS_DATA;
	if(!us){
		US_ERR("Malloc USB Storage(us) structure fail\n");
		return 1;
	}

	memset(us, 0, sizeof(*us));
	
#if 1
	/*
	 * If the device isn't standard (is handled by a subdriver
	 * module) then don't accept it.
	 */
	if (usb_usual_ignore_device(intf)){
		US_ERR("This device is not a stadard USB storage devices.\n");
		return -USB_ENXIO;
	}
#endif

	//udev = interface_to_usbdev(intf);

	// this should be done at enumeration stage
#if 0	
	usb_detect_quirks(udev);
	if (udev->quirks & USB_QUIRK_DELAY_INIT)
		rtw_mdelay_os(1000);
	usb_detect_interface_quirks(udev);
#endif

	/*match dynamic device id*/
	//id = usb_match_dynamic_id(intf, driver);
	if(!id){
		// match stastic id table
		size = ARRAY_SIZE(us_unusual_dev_list);
		for(index=0;index < size; index++){
			id = usb_storage_usb_ids + index;
			if (usb_match_one_id(intf, id))
				break;
		}
		if(index == size){
			US_ERR("Match usb_device_id fail\n");
			id = NULL;
		}
	}
	
	if(!id){
		return -USB_ENODEV;
	}

	US_INFO("GOT an id(index:%d/%d).\n", index, size);
	 
	/*
	 * Call the general probe procedures.
	 *
	 * The unusual_dev_list array is parallel to the usb_storage_usb_ids
	 * table, so we use the index of the id entry to find the
	 * corresponding unusual_devs entry.
	 */

	// usb_storage_usb_ids denote the set of usb storage device supported by the driver
	// id and usb_storage_usb_ids are both pointer, point to the address
	size = ARRAY_SIZE(us_unusual_dev_list);  // get an unusual dev id size
	if (id >= usb_storage_usb_ids && id < usb_storage_usb_ids + size) {
		unusual_dev = (id - usb_storage_usb_ids) + us_unusual_dev_list;
	} else {
		unusual_dev = &for_dynamic_ids; // default for usb flash storage
		US_INFO("Use Bulk-Only transport with the Transparent SCSI protocol for dynamic id: 0x%04x 0x%04x\n",
			id->idVendor, id->idProduct);
	}

	result = usb_stor_probe1(us, intf, id, unusual_dev);
	if (result)
		return result;

	result = usb_stor_probe2(us);
	if(!result){
		USB_STORAGE_READY = TRUE;
		rtw_up_sema(&us_sto_rdy_sema);
		if (stor_cb && stor_cb->attach){
			stor_cb->attach();
		}
        US_INFO("done");
        }
US_EXIT;
	return result;
}

static struct usb_driver usb_storage_driver = {
	.name =		"MSC",
	.probe =	storage_probe,
	.disconnect =	usb_stor_disconnect,
//	.suspend =	usb_stor_suspend,
//	.resume =	usb_stor_resume,
//	.reset_resume =	usb_stor_reset_resume,
//	.pre_reset =	usb_stor_pre_reset,
//	.post_reset =	usb_stor_post_reset,
	.id_table =	usb_storage_usb_ids,
};

int usb_storage_init(void){
	int res = 0;
  	res = usb_register_class_driver(&usb_storage_driver);
	rtw_init_sema(&us_trd_exit_sema, 0);
	return res;
}

int usb_storage_deinit(void){
	rtw_free_sema(&us_trd_exit_sema);
  	usb_unregister_class_driver(&usb_storage_driver);
    return 0;
}

void usb_msc_register_usr_cb(struct us_usr_cb *cb)
{	
	if (cb != NULL) {
		stor_cb = cb;
	}
}

#endif // CONFIG_USBH_MSC
