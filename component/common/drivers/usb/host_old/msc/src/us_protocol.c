#include <platform_opts.h>

#ifdef CONFIG_USBH_MSC

#include "scsi/scsi_cmnd.h"
#include "us_usb.h"

void usb_stor_transparent_scsi_command(struct scsi_cmnd* srb, struct us_data* us)
{
	/* send the command to the transport layer */
	usb_stor_invoke_transport(srb, us);
}

#endif // CONFIG_USBH_MSC

