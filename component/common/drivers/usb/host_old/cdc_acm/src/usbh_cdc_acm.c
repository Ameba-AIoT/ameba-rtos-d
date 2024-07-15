#include <platform_opts.h>
#ifdef CONFIG_USBH_CDC_ACM

#include "usb.h"
#include "usbh_cdc_acm.h"
#include "usb_errno.h"

_mutex cdc_acm_lock;
_sema cdc_acm_connect_sema;
static u32 cdc_acm_connect_state;
struct cdc_acm *acm;
static u32 read_flag = 0;

static void acm_notification_process(struct urb *urb)
{
	USBH_CDC_ACM_ENTER;
	
	struct cdc_acm *acm = urb->context;
	struct usb_cdc_notification *dr = urb->transfer_buffer;
	u32 length = urb->actual_length;
	unsigned char *data;
	int newctrl;
	int ret;
	int status = urb->status;

	switch (status) {
		case 0:
			break;
		case -USB_ECONNRESET:
		case -USB_ENOENT:
		case -USB_ESHUTDOWN:
			return;
		default:
			goto exit;
	}

	data = (unsigned char *)(dr + 1);
	switch (dr->bNotificationType) {
		case USB_CDC_NOTIFY_NETWORK_CONNECTION:
			USBH_CDC_ACM_INFO("%s - network connection: %d\n", dr->wValue);
			break;
		case USB_CDC_NOTIFY_SERIAL_STATE:
			newctrl = data[0]|(data[1]<<8);
			acm->ctrlin = newctrl;
			USBH_CDC_ACM_INFO("NOTIFY_SERIAL_STATE, value: %x\n", newctrl);
				break;
		default:
			USBH_CDC_ACM_ERROR("unknown notification %d received: index %d "
				"len %d data0 %d data1 %d\n",
				dr->bNotificationType, dr->wIndex,
				dr->wLength, data[0], data[1]);
			break;
	}
exit:
	ret = usb_submit_urb(urb);
	if ( ret < 0)
		USBH_CDC_ACM_ERROR("usb_submit_urb failed: %d\n", ret);

	USBH_CDC_ACM_EXIT;
}


static void acm_read_bulk_callback(struct urb *urb)
{
	USBH_CDC_ACM_ENTER;
	int ret;
	struct acm_rb *rb = urb->context;
	u8* buf =(u8*)rb->base;
	u32 len = urb->actual_length;
	int status = urb->status;

	switch (status) {
		case 0:
			break;
		case -USB_ECONNRESET:
		case -USB_ENOENT:
		case -USB_ESHUTDOWN:
			USBH_CDC_ACM_ERROR("ERROR!URB STATUS: %d\n", status);
			return;
		default:
			goto exit;
	}

	if(acm->cb != NULL && acm->cb->receive !=NULL){
		acm->cb->receive(buf, len);
	}

exit:
	if((read_flag == 1)){
		if(!usbh_get_connect_status()){
			USBH_CDC_ACM_WARN("device disconnected!!");
			return;
		}
		ret = usb_submit_urb(urb);
		if (ret < 0){
			USBH_CDC_ACM_ERROR("usb_submit_urb failed: %d\n", ret);
			usbh_cdc_acm_read_close();
		}
	}
	USBH_CDC_ACM_EXIT;
}

static void acm_write_bulk_callback(struct urb *urb)
{
	USBH_CDC_ACM_ENTER;
	int status = urb->status;

	if(status != 0)
		USBH_CDC_ACM_ERROR("ERROR!URB STATUS: %d\n", status);

	struct acm_wb *wb = urb->context;
	wb->use = 0;
	rtw_up_sema(&wb->comp_sema);

	USBH_CDC_ACM_EXIT;
}

static int acm_wb_alloc(void)
{
	USBH_CDC_ACM_ENTER;
	int i;
	int wbn;
	struct acm_wb *wb;

	wbn = 0;
	i = 0;
	for (;;) {
		wb = &acm->wb[wbn];
		if (!wb->use) {
			wb->use = 1;
			return wbn;
		}
		wbn = (wbn + 1) % ACM_NW;
		if (++i >= ACM_NW)
			return -1;
	}

	USBH_CDC_ACM_EXIT;
}


static int acm_write(u8 * buf, int count)
{
	USBH_CDC_ACM_ENTER;
	struct acm_wb * wb;
	int wbn;
	int ret=0;

	rtw_spin_lock(&acm->write_lock);
	wbn = acm_wb_alloc();
	if(wbn < 0){
		USBH_CDC_ACM_ERROR("no wb buffer\n");
		rtw_spin_unlock(&acm->write_lock);
		return -1;
	}
	wb = &acm->wb[wbn];
	wb->urb->transfer_buffer = buf;
	wb->urb->transfer_buffer_length = count;
	wb->urb->dev = acm->dev;

	ret = usb_submit_urb(wb->urb);
	if(ret < 0){
		USBH_CDC_ACM_ERROR("SUBMIT URB FAIL: %d\n", ret);
		wb->use = 0;
		rtw_spin_unlock(&acm->write_lock);
		return ret;
	}

	if ( rtw_down_timeout_sema(&wb->comp_sema, 3000) == _FAIL) {
		USBH_CDC_ACM_ERROR("timed out on ep%d%s len=%d/%d\n",
			usb_endpoint_num(&wb->urb->ep->desc),
			usb_urb_dir_in(wb->urb) ? "in" : "out",
			wb->urb->actual_length,
			wb->urb->transfer_buffer_length);
		usb_kill_urb(wb->urb);
		ret = -USB_ETIMEDOUT;
		wb->use = 0;
	}
	rtw_spin_unlock(&acm->write_lock);

	USBH_CDC_ACM_EXIT;
	return ret;
}

int usbh_cdc_acm_send_encapsulated_command(u8* buf, u32 len)
{
	USBH_CDC_ACM_ENTER;
	int ret;
	
	ret = usb_control_msg(acm->dev, usb_sndctrlpipe(acm->dev, 0),
		CDC_SEND_ENCAPSULATED_COMMAND_REQUEST, USB_DIR_OUT|USB_TYPE_CLASS|USB_RECIP_INTERFACE, 0,
		acm->control->altsetting[0].desc.bInterfaceNumber,
		buf, len, 5000);

	if(ret < 0)
		USBH_CDC_ACM_ERROR("USB CONTROL MSG FAIL\n");

	USBH_CDC_ACM_EXIT;

	return ret;
	
}

int usbh_cdc_acm_get_encapsulated_command(u8* buf, u32 len)
{
	USBH_CDC_ACM_ENTER;
	int ret;
	
	ret = usb_control_msg(acm->dev, usb_rcvctrlpipe(acm->dev, 0),
		CDC_SEND_ENCAPSULATED_COMMAND_REQUEST, USB_DIR_IN|USB_TYPE_CLASS|USB_RECIP_INTERFACE, 0,
		acm->control->altsetting[0].desc.bInterfaceNumber,
		buf, len, 5000);

	if(ret == (int) len){
		ret = USB_ESUCCESS;
	}else if(ret < 0){
		USBH_CDC_ACM_ERROR("USB CONTROL MSG FAIL\n");
	}else{
		USBH_CDC_ACM_ERROR("WRONG MESSAGE LENGTH: %d, IT SHOULD BE %d\n", ret, len);
		ret = -USB_ENODATA;
	}

	USBH_CDC_ACM_EXIT;

	return ret;
	
}

int usbh_cdc_acm_set_ctrl_line_state(u32 ctrl)
{
	USBH_CDC_ACM_ENTER;
	int ret;
	
	if((acm->acm_caps & USB_CDC_CONFIG_SUPPORT_1) == 0){
		USBH_CDC_ACM_ERROR("device not support the request\n");
		return -1;
	}
	ret = usb_control_msg(acm->dev, usb_sndctrlpipe(acm->dev, 0),
		CDC_SET_CONTROL_LINE_STATE_REQUEST, USB_DIR_OUT|USB_TYPE_CLASS|USB_RECIP_INTERFACE, ctrl,
		acm->control->altsetting[0].desc.bInterfaceNumber,
		NULL, 0, 5000);

	if(ret < 0){
		USBH_CDC_ACM_ERROR("USB CONTROL MSG FAIL\n");
	}else{
		acm->ctrlout = ctrl;
	}
	
	USBH_CDC_ACM_EXIT;
	return ret;
}

int usbh_cdc_acm_get_line_coding(CDC_LineCodingTypeDef * LCStruct )
{
	USBH_CDC_ACM_ENTER;
	int ret;

	if((acm->acm_caps & USB_CDC_CONFIG_SUPPORT_1) == 0){
		USBH_CDC_ACM_ERROR("device not support the request\n");
		return -1;
	}
	ret = usb_control_msg(acm->dev, usb_rcvctrlpipe(acm->dev, 0),
		CDC_GET_LINE_CODING_RQUEST, USB_DIR_IN| USB_TYPE_CLASS|USB_RECIP_INTERFACE, 0, 
		acm->control->altsetting[0].desc.bInterfaceNumber,
		LCStruct->Array, LINE_CODING_STRUCTURE_SIZE, 5000);

	if(ret == LINE_CODING_STRUCTURE_SIZE){
		memcpy (acm->line.Array, LCStruct->Array, LINE_CODING_STRUCTURE_SIZE);
		ret = USB_ESUCCESS;
	}else if(ret < 0){
		USBH_CDC_ACM_ERROR("USB CONTROL MSG FAIL\n");
	}else{
		USBH_CDC_ACM_ERROR("WRONG MESSAGE LENGTH: %d, IT SHOULD BE %d\n", ret, LINE_CODING_STRUCTURE_SIZE);
		ret = -USB_ENODATA;
	}

	USBH_CDC_ACM_EXIT;

	return ret;
}

int usbh_cdc_acm_set_line_coding(CDC_LineCodingTypeDef * LCStruct)
{
	USBH_CDC_ACM_ENTER;
	int ret;

	if((acm->acm_caps & USB_CDC_CONFIG_SUPPORT_1) == 0){
		USBH_CDC_ACM_ERROR("device not support the request\n");
		return -1;
	}
	
	ret = usb_control_msg(acm->dev, usb_sndctrlpipe(acm->dev, 0),
		CDC_SET_LINE_CODING_RQUEST, USB_DIR_OUT|USB_TYPE_CLASS|USB_RECIP_INTERFACE,  0, 
		acm->control->altsetting[0].desc.bInterfaceNumber,
		LCStruct->Array, LINE_CODING_STRUCTURE_SIZE, 5000);

	if(ret < 0)
		USBH_CDC_ACM_ERROR("USB CONTROL MSG FAIL\n");

	USBH_CDC_ACM_EXIT;
	return ret;
}


int usbh_cdc_acm_write(u8 * buf, int count)
{
	USBH_CDC_ACM_ENTER;
	int ret;
	
	while(count > 0){
		if(!usbh_get_connect_status()){
			USBH_CDC_ACM_WARN("device disconnected!!");
			return -USB_ENOTCONN;
		}
		if(count > acm->writesize){
			ret = acm_write(buf, acm->writesize);
			if(ret < 0){
				USBH_CDC_ACM_ERROR("acm write fail: %d\n", ret);
				break;
			}
			count -= acm->writesize;
		}else{
			ret = acm_write(buf, count);
			if(ret < 0){
				USBH_CDC_ACM_ERROR("acm write fail: %d\n", ret);
			}
			break;
		}	
	}

	if(acm->cb != NULL && acm->cb->transmit_complete!=NULL){
		acm->cb->transmit_complete(ret);
	}
	
	USBH_CDC_ACM_EXIT;
	return ret;
}


int usbh_cdc_acm_ctrl_close(void)
{
	USBH_CDC_ACM_ENTER;
	
	usb_kill_urb(acm->ctrlurb);			/*bulk endpoint*/

	USBH_CDC_ACM_EXIT;
	return USB_ESUCCESS;
}


int usbh_cdc_acm_ctrl_open(void)
{
	USBH_CDC_ACM_ENTER;
	int ret;

	ret = usb_submit_urb(acm->ctrlurb);			/*interrupt endpoint*/
	if(ret < 0){
		USBH_CDC_ACM_ERROR("SUBMIT URB FAIL\n");
		usbh_cdc_acm_ctrl_close();
	}

	USBH_CDC_ACM_EXIT;
	return ret;
}

int usbh_cdc_acm_read_close(void)
{
	USBH_CDC_ACM_ENTER;
	int i;
	
	rtw_spin_lock(&acm->read_lock);

	for(i = 0; i < ACM_NR; i++){
		usb_kill_urb(acm->rb[i].urb);			/*bulk endpoint*/
	}

	rtw_spin_unlock(&acm->read_lock);

	read_flag = 0;
	USBH_CDC_ACM_EXIT;
	return USB_ESUCCESS;
}


int usbh_cdc_acm_read_open(void)
{
	USBH_CDC_ACM_ENTER;
	int ret;
	int i;

	rtw_spin_lock(&acm->read_lock);

	for(i = 0; i < ACM_NR; i++){
		ret = usb_submit_urb(acm->rb[i].urb);		/*bulk endpoint*/
		if(ret < 0){
			USBH_CDC_ACM_ERROR("SUBMIT URB FAIL: %d\n", ret);
			rtw_spin_unlock(&acm->read_lock);
			usbh_cdc_acm_read_close();
			return ret;
		}
	}
	rtw_spin_unlock(&acm->read_lock);

	read_flag = 1;

	USBH_CDC_ACM_EXIT;
	return ret;
}

int cdc_acm_probe(struct usb_interface *intf) 
{
	USBH_CDC_ACM_ENTER;
	struct usb_cdc_union_desc *union_header = NULL;
	unsigned char *buffer = intf->altsetting->extra;
	int buflen = intf->altsetting->extralen;
	struct usb_interface *control_interface;
	struct usb_interface *data_interface;
	struct usb_endpoint_descriptor *epctrl = NULL;
	struct usb_endpoint_descriptor *epread = NULL;
	struct usb_endpoint_descriptor *epwrite = NULL;
	struct usb_device *usb_dev = interface_to_usbdev(intf);
	u8 ac_management_function = 0;
	u8 call_management_function = 0;
	int call_interface_num = -1;
	int data_interface_num = -1;
	int i;
	int combined_interfaces = 0;
	int acm_flag = 0;

/*====================== step1: parse cdc class specific descriptors============================*/
	USBH_CDC_ACM_INFO("parse cdc class specific descriptors\n");
	while (buflen > 0) {
		if (buffer[1] != USB_DT_CS_INTERFACE) {
			USBH_CDC_ACM_WARN( "skipping garbage\n");
			goto next_desc;
		}
		switch (buffer[2]) {
		case USB_CDC_UNION_TYPE: 
			if (union_header) {
				USBH_CDC_ACM_WARN("More than one union descriptor, skipping ...\n");
				goto next_desc;
			}
			union_header = (struct usb_cdc_union_desc *)buffer;
			USBH_CDC_ACM_INFO("master:%d, slave:%d\n", union_header->bMasterInterface0, union_header->bSlaveInterface0);
			break;
		case USB_CDC_HEADER_TYPE:
			USBH_CDC_ACM_INFO("header\n");
			break; 
		case USB_CDC_ACM_TYPE:
			acm_flag = 1;
			ac_management_function = buffer[3];
			USBH_CDC_ACM_INFO("acm_managment:%d\n", ac_management_function);
			break;
		case USB_CDC_CALL_MANAGEMENT_TYPE:
			call_management_function = buffer[3];
			call_interface_num = buffer[4];
			USBH_CDC_ACM_INFO("call:%d, %d\n", call_management_function, call_interface_num);
			break;
		case USB_CDC_COUNTRY_TYPE:
			USBH_CDC_ACM_WARN("country operation is not supported\n");
		default:
			USBH_CDC_ACM_INFO("Ignoring descriptor:type %02x, length %d\n", buffer[2], buffer[0]);
			break;
		}
next_desc:
		buflen -= buffer[0];
		buffer += buffer[0];
	}

	if(acm_flag != 1){
		return -USB_ENOPROTOOPT;
	}

/*==================== step2: parse cdc interfaces ============================*/	
	USBH_CDC_ACM_INFO("parse cdc interfaces\n");
	if (!union_header) {
		if (call_interface_num > 0) {
			USBH_CDC_ACM_WARN("No union descriptor, using call management descriptor\n");
			data_interface = usb_ifnum_to_if(usb_dev, (data_interface_num = call_interface_num));
			control_interface = intf;
		} else {
			if (intf->cur_altsetting->desc.bNumEndpoints != 3) {
				USBH_CDC_ACM_ERROR("No union descriptor, giving up\n");
				return -USB_ENODEV;
			} else {
				USBH_CDC_ACM_WARN("No union descriptor, testing for castrated device\n");
				combined_interfaces = 1;
				control_interface = data_interface = intf;
				goto look_for_collapsed_interface;
			}
		}
	} else {
		control_interface = usb_ifnum_to_if(usb_dev, union_header->bMasterInterface0);
		data_interface = usb_ifnum_to_if(usb_dev, (data_interface_num = union_header->bSlaveInterface0));
	}

	if (!control_interface || !data_interface) {
		USBH_CDC_ACM_ERROR("no interfaces\n");
		return -USB_ENODEV;
	}

	if (data_interface_num != call_interface_num)
		USBH_CDC_ACM_INFO("Separate call data interface..\n");
	
/*==================== step3: parse cdc endpoints ============================*/
	USBH_CDC_ACM_INFO("parse cdc endpoints \n");
	if (control_interface == data_interface) {
		USBH_CDC_ACM_INFO("Control and data interfaces are not separated!\n");
		combined_interfaces = 1;
		if (data_interface->cur_altsetting->desc.bNumEndpoints != 3) {
			USBH_CDC_ACM_ERROR("This needs exactly 3 endpoints\n");
			return -USB_EINVAL;
		}
look_for_collapsed_interface:
		USBH_CDC_ACM_INFO("look_for_collapsed_interface\n");
		for (i = 0; i < 3; i++) {
			struct usb_endpoint_descriptor *ep;
			ep = &data_interface->cur_altsetting->endpoint[i].desc;
			if (usb_endpoint_is_int_in(ep))
				epctrl = ep;
			else if (usb_endpoint_is_bulk_out(ep))
				epwrite = ep;
			else if (usb_endpoint_is_bulk_in(ep))
				epread = ep;
			else
				return -USB_EINVAL;
		}
		if (!epctrl || !epread || !epwrite)
			return -USB_ENODEV;
		else
			goto acm_struct_init;
	}

	/*workaround for switched interfaces */
	if (data_interface->cur_altsetting->desc.bInterfaceClass!= CDC_DATA_INTERFACE_TYPE) {
		if (control_interface->cur_altsetting->desc.bInterfaceClass == CDC_DATA_INTERFACE_TYPE) {
			struct usb_interface *t;
			USBH_CDC_ACM_INFO("Your device has switched interfaces.\n");
			t = control_interface;
			control_interface = data_interface;
			data_interface = t;
		} else {
			return -USB_EINVAL;
		}
	}

	if (!combined_interfaces && intf != control_interface){
		USBH_CDC_ACM_ERROR("Accept probe requests only for the control interface\n");
		return -USB_ENODEV;
	}

	if (data_interface->cur_altsetting->desc.bNumEndpoints < 2 ||
		control_interface->cur_altsetting->desc.bNumEndpoints == 0){
		USBH_CDC_ACM_ERROR("data or control interface endpoint number is not standard\n");
		return -USB_EINVAL;

	}

	epctrl = &control_interface->cur_altsetting->endpoint[0].desc;
	epread = &data_interface->cur_altsetting->endpoint[0].desc;
	epwrite = &data_interface->cur_altsetting->endpoint[1].desc;

	/* workaround for switched endpoints */
	if (!usb_endpoint_dir_in(epread)) {
		struct usb_endpoint_descriptor *t;
		USBH_CDC_ACM_INFO("The data interface has switched endpoints\n");
		t = epread;
		epread = epwrite;
		epwrite = t;
	}

/*==================== step4: init acm struct member ============================*/
acm_struct_init:
	USBH_CDC_ACM_INFO("init acm struct member\n");
	acm->intf = intf;
	acm->combined_interfaces = combined_interfaces;
	acm->comm_interface_protocol = control_interface->cur_altsetting->desc.bInterfaceProtocol;
	acm->control = control_interface;
	acm->data = data_interface;
	acm->dev = usb_dev;
	acm->acm_caps = ac_management_function;
	acm->ctrlsize = usb_endpoint_maxp(epctrl);

	rtw_spinlock_init(&acm->write_lock);
	rtw_spinlock_init(&acm->read_lock);

	if (!usb_endpoint_xfer_bulk(epread)||!usb_endpoint_xfer_bulk(epwrite)){
		USBH_CDC_ACM_ERROR("read/write endpoint is not bulk transfer\n");
		return -USB_ENODATA;
	}
	
	acm->ctrl_buffer = rtw_malloc(acm->ctrlsize);
	if (!acm->ctrl_buffer) {
		printf("(ctrl buffer alloc fail)\n");
		goto alloc_fail1;
	}

	acm->ctrlurb = usb_alloc_urb(0);
	if (!acm->ctrlurb) {
		USBH_CDC_ACM_ERROR( "alloc fail (ctrlurb malloc)\n");
		goto alloc_fail2;
	}

	usb_fill_int_urb(acm->ctrlurb, usb_dev,
			 usb_rcvintpipe(usb_dev, epctrl->bEndpointAddress),
			 acm->ctrl_buffer, acm->ctrlsize, acm_notification_process, acm,
			 epctrl->bInterval ? epctrl->bInterval : 16);
	acm->ctrlurb->context = acm;
	
	for (i = 0; i < ACM_NR; i++) {
		struct acm_rb *rb = &(acm->rb[i]);
		struct urb *urb;

		rb->base = rtw_malloc(acm->readsize);
		if (!rb->base) {
			USBH_CDC_ACM_ERROR( "alloc fail (read bufs usb_alloc_coherent)\n");
			goto alloc_fail3;
		}
		rb->len = acm->readsize;
		rb->instance = acm;

		urb = usb_alloc_urb(0);
		if (!urb) {
			USBH_CDC_ACM_ERROR("alloc fail (read urbs usb_alloc_urb)\n");
			goto alloc_fail3;
		}

		usb_fill_bulk_urb(urb, acm->dev,
				 usb_rcvbulkpipe(usb_dev, epread->bEndpointAddress),
				 rb->base, acm->readsize,
				 acm_read_bulk_callback, rb);
		rb->urb = urb;
		urb->context = rb;
	}
	
	for (i = 0; i < ACM_NW; i++) {
		struct acm_wb *snd = &(acm->wb[i]);
		rtw_init_sema(&snd->comp_sema, 0);
		snd->urb = usb_alloc_urb(0);
		if (snd->urb == NULL) {
			USBH_CDC_ACM_ERROR("alloc fail (write urbs usb_alloc_urb)\n");
			goto alloc_fail4;
		}
		
		usb_fill_bulk_urb(snd->urb, usb_dev,
				usb_sndbulkpipe(usb_dev, epwrite->bEndpointAddress),
				NULL, 0, acm_write_bulk_callback, snd);
		snd->urb->context = snd;
		snd->instance = acm;
	}

	usb_set_intfdata(intf, acm);

	usb_set_intfdata(data_interface, acm);

	usbh_cdc_acm_read_open();
	//usbh_cdc_acm_ctrl_open();		/*some device do not enable interrput endpoint so close it*/

	USBH_CDC_ACM_EXIT;

	return USB_ESUCCESS;
alloc_fail4:
	usb_set_intfdata(intf, NULL);
	for (i = 0; i < ACM_NW; i++){
		usb_free_urb(acm->wb[i].urb);
		rtw_free_sema(&acm->wb[i].comp_sema);
	}
alloc_fail3:
	for (i = 0; i < ACM_NR; i++){
		usb_free_urb(acm->rb[i].urb);
		if(acm->rb[i].base != NULL)
			rtw_free(acm->rb[i].base);
	}
	usb_free_urb(acm->ctrlurb);
alloc_fail2:
	rtw_free(acm->ctrl_buffer);
alloc_fail1:
	rtw_free(acm);
	rtw_spinlock_free(&acm->write_lock);
	rtw_spinlock_free(&acm->read_lock);
	return -USB_ENODATA;
}


void cdc_acm_probe_thread(void *param)
{
	USBH_CDC_ACM_ENTER;
	struct usb_interface *intf = (struct usb_interface*) param;
	int ret;
	
	if ((intf != NULL) && (intf->cur_altsetting->desc.bInterfaceClass == CDC_COMM_INTERFACE_TYPE)) {
		rtw_spin_lock(&cdc_acm_lock);
		if(!usbh_get_connect_status()) {
			rtw_spin_unlock(&cdc_acm_lock);
			rtw_up_sema(&cdc_acm_connect_sema);
			rtw_thread_exit();
			return;
		}
		ret = cdc_acm_probe(intf);
		if(ret < 0){
			USBH_CDC_ACM_ERROR("CDC ACM PROBE FAIL: %d\n", ret);
			rtw_spin_unlock(&cdc_acm_lock);
			rtw_up_sema(&cdc_acm_connect_sema);
			rtw_thread_exit();
			return;
		}
		
		if ((acm->cb != NULL) && (acm->cb->attach != NULL)) {
			acm->cb->attach();
		}
		rtw_spin_unlock(&cdc_acm_lock);
	}else{
		USBH_CDC_ACM_ERROR("INTERFACE TYPE IS NOT USB_CLASS_COMM\n");
	}

	cdc_acm_connect_state = 1;
	rtw_up_sema(&cdc_acm_connect_sema);

	USBH_CDC_ACM_EXIT;
	rtw_thread_exit();
	
}


static int cdc_acm_probe_entry(struct usb_interface *intf)
{
	USBH_CDC_ACM_ENTER;
	
	int ret;
	struct task_struct task;
	
	ret = rtw_create_task(&task, "cdc_acm_probe_thread", 2048, USBH_CDC_ACM_TASK_PRIORITY, cdc_acm_probe_thread, intf);
	if (ret != pdPASS) {
		USBH_CDC_ACM_ERROR("\nFail to create cdc acm probe thread\n");
		ret = -1;
	}
	
	USBH_CDC_ACM_EXIT;

	return ret;
}

static void cdc_acm_free(void)
{
	USBH_CDC_ACM_ENTER;
	int i;

	if(cdc_acm_connect_state == 1){
		if (acm == NULL) {
			return;
		}

		for (i = 0; i < ACM_NW; i++){
			usb_kill_urb(acm->wb[i].urb);
			rtw_free_sema(&acm->wb[i].comp_sema);
			usb_free_urb(acm->wb[i].urb);
		}

		for (i = 0; i < ACM_NR; i++){
			usb_kill_urb(acm->rb[i].urb);
			usb_free_urb(acm->rb[i].urb);
			if(acm->rb[i].base != NULL)
				rtw_free(acm->rb[i].base);
		}

		usb_kill_urb(acm->ctrlurb);
		usb_free_urb(acm->ctrlurb);
		rtw_free(acm->ctrl_buffer);

		if (acm->dev != NULL && acm->intf !=NULL) {
			usb_disable_interface(acm->dev, acm->intf, TRUE);
			usb_disable_interface(acm->dev, acm->data, TRUE);
		}

		if (acm->dev != NULL) {
			rtw_free(acm->dev);
		}

		if (acm->intf != NULL) {
			usb_set_intfdata(acm->intf, NULL);
			rtw_free(acm->intf);
		}

		rtw_spinlock_free(&acm->write_lock);
		rtw_spinlock_free(&acm->read_lock);

		cdc_acm_connect_state = 0;
	}
	USBH_CDC_ACM_EXIT;
}

static void cdc_acm_disconnect(struct usb_interface *intf)
{
	UNUSED(intf);
	USBH_CDC_ACM_ENTER;
	
	if ( rtw_down_timeout_sema(&cdc_acm_connect_sema, 1000) == _FAIL) {
		USBH_CDC_ACM_ERROR("\nGet connection sema time out\n");
	}

	if ((acm->cb != NULL) && (acm->cb->detach != NULL)) {
		acm->cb->detach();
	}

	cdc_acm_free();

	USBH_CDC_ACM_EXIT;
}

static struct usb_driver cdc_acm_driver = {
	.name = "cdc_acm_driver",
	.probe = cdc_acm_probe_entry,
	.disconnect = cdc_acm_disconnect,
};

void usbh_cdc_acm_deinit(void)
{
	USBH_CDC_ACM_ENTER;
	
	cdc_acm_free();
	rtw_free(acm);
	rtw_free_sema(&cdc_acm_connect_sema);
	rtw_spinlock_free(&cdc_acm_lock);
	usb_unregister_class_driver(&cdc_acm_driver);
	
	USBH_CDC_ACM_EXIT;
}

/**
  * @brief  Initializes the CDC ACM host.
  * @param  bulk_in_transfer_size: bulk in transfer size. Max value is 65536 and 0 means default value 1024.
  * @param  bulk_out_transfer_size: bulk out transfer size. Max value is 65536 and 0 means default value 1024.
  * @param  cb: user call back function. please refer to struct  usbh_cdc_acm_usr_cb_t.
  * @retval status. 
  */
int usbh_cdc_acm_init(u32 bulk_in_transfer_size,u32 bulk_out_transfer_size, usbh_cdc_acm_usr_cb_t *cb)
{
	int ret = 0;
	USBH_CDC_ACM_ENTER;

	if(acm==NULL){
		acm = (struct cdc_acm * )rtw_zmalloc(sizeof(struct cdc_acm));
			if (!acm ) {
			USBH_CDC_ACM_ERROR("alloc acm fail\n");
			return -1;
		}
	}
	
	if(bulk_in_transfer_size == 0){
		acm->readsize = BULK_IN_TRANSFER_SIZE;
	}else{
		acm->readsize = bulk_in_transfer_size;
	}
	
	if(bulk_out_transfer_size == 0){
		acm->writesize = BULK_OUT_TRANSFER_SIZE;
	}else{
		acm->writesize = bulk_out_transfer_size;
	}
	
	if(cb !=NULL){
		acm->cb = cb;
	}

	if(acm->cb != NULL && acm->cb->init !=NULL){
		acm->cb->init();
	}

	rtw_spinlock_init(&cdc_acm_lock);
	rtw_init_sema(&cdc_acm_connect_sema, 0);

	ret = usb_register_class_driver(&cdc_acm_driver);
	if (ret) {
		usbh_cdc_acm_deinit();
	}

	USBH_CDC_ACM_EXIT;

	return ret;
}

#endif // CONFIG_USBH_CDC_ACM

