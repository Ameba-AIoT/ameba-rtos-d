#include <platform_opts.h>

#ifdef CONFIG_USBH_MSC

#include "us_usb.h"
#include "us_debug.h"
#include "us_usual.h"
#include "us_transport.h"
#include "storage.h"
#include "us_os_wrap_via_osdep_api.h"
#include <quirks/quirks.h>
#include <scsi/scsi_cmnd.h>
#include <scsi/scsi_eh.h>
#include "osdep_service.h"

void usb_stor_show_sense(
		unsigned char key,
		unsigned char asc,
		unsigned char ascq);

/* This is the completion handler which will wake us up when an URB
 * completes.
 */
static void usb_stor_blocking_completion(struct urb *urb)
{
	_sema *complete_sema = urb->context;

	rtw_up_sema(complete_sema);  // release control urb semaphore
}

/* This is the common part of the URB message submission code
 *
 * All URBs from the usb-storage driver involved in handling a queued scsi
 * command _must_ pass through this function (or something like it) for the
 * abort mechanisms to work properly.
 */
static int usb_stor_msg_common(struct us_data *us, int timeout)
{
	_sema urb_done;
	int status;
	unsigned long expire;
	/* don't submit URBs during abort processing */
	if (test_bit(US_FLIDX_ABORTING, &us->dflags))
		return -USB_EIO;

	/* set up data structures for the wakeup system */
	rtw_init_sema(&urb_done, 0);
	/* fill the common fields in the URB */
	us->current_urb->context = &urb_done;
	us->current_urb->transfer_flags = 0;

	/* we assume that if transfer_buffer isn't us->iobuf then it
	 * hasn't been mapped for DMA.  Yes, this is clunky, but it's
	 * easier than always having the caller tell us whether the
	 * transfer buffer has already been mapped. */
	if (us->current_urb->transfer_buffer == us->iobuf)
		us->current_urb->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;
	us->current_urb->transfer_dma = us->iobuf_dma;
	/* submit the URB */
	status = usb_submit_urb(us->current_urb);
	if (status) {
		/* something went wrong */
		rtw_free_sema(&urb_done);
		return status;
	}

	/* since the URB has been submitted successfully, it's now okay
	 * to cancel it */
	set_bit(US_FLIDX_URB_ACTIVE, &us->dflags);

	/* did an abort occur during the submission? */
	if (test_bit(US_FLIDX_ABORTING, &us->dflags)) {

		/* cancel the URB, if it hasn't been cancelled already */
		if (test_and_clear_bit(US_FLIDX_URB_ACTIVE, &us->dflags)) {
			US_ERR("Cancelling URB\n");
			usb_unlink_urb(us->current_urb);
		}
	}

	/* wait for the completion of the URB */
	expire = timeout ? timeout : 2000;
	if (rtw_down_timeout_sema(&urb_done, expire) == _FAIL){
		US_ERR("Wait urb time out!!");
		usb_kill_urb(us->current_urb);
	}
	clear_bit(US_FLIDX_URB_ACTIVE, &us->dflags);
	// free memory for sema
	rtw_free_sema(&urb_done);
	/* return the URB status */
	return us->current_urb->status;
}


/*
 * Transfer one control message, with timeouts, and allowing early
 * termination.  Return codes are usual -Exxx, *not* USB_STOR_XFER_xxx.
 */
int usb_stor_control_msg(struct us_data *us, unsigned int pipe,
		 u8 request, u8 requesttype, u16 value, u16 index, 
		 void *data, u16 size, int timeout)
{
	int status;

	US_INFO("rq=%02x rqtype=%02x value=%04x index=%02x len=%d\n",
		     request, requesttype, value, index, size);

	/* fill in the devrequest structure */
	us->cr->bRequestType = requesttype;
	us->cr->bRequest = request;
	us->cr->wValue = cpu_to_le16(value);
	us->cr->wIndex = cpu_to_le16(index);
	us->cr->wLength = cpu_to_le16(size);

	/* fill and submit the URB */
	usb_fill_control_urb(us->current_urb, us->pusb_dev, pipe, 
			 (unsigned char*) us->cr, data, size, 
			 usb_stor_blocking_completion, NULL);
	status = usb_stor_msg_common(us, timeout);

	/* return the actual length of the data transferred if no error */
	if (status == 0)
		status = us->current_urb->actual_length;
	return status;
}


/* This is a version of usb_clear_halt() that allows early termination and
 * doesn't read the status from the device -- this is because some devices
 * crash their internal firmware when the status is requested after a halt.
 *
 * A definitive list of these 'bad' devices is too difficult to maintain or
 * make complete enough to be useful.  This problem was first observed on the
 * Hagiwara FlashGate DUAL unit.  However, bus traces reveal that neither
 * MacOS nor Windows checks the status after clearing a halt.
 *
 * Since many vendors in this space limit their testing to interoperability
 * with these two OSes, specification violations like this one are common.
 */
int usb_stor_clear_halt(struct us_data *us, unsigned int pipe)
{
	int result;
	int endp = usb_pipeendpoint(pipe);
US_ENTER;
	if (usb_pipein (pipe))
		endp |= USB_DIR_IN;

	result = usb_stor_control_msg(us, us->send_ctrl_pipe,
		USB_REQ_CLEAR_FEATURE, USB_RECIP_ENDPOINT,
		USB_ENDPOINT_HALT, endp,
		NULL, 0, 3*SCSI_HZ);

	if (result >= 0)
		usb_reset_endpoint(us->pusb_dev, endp);

	US_INFO("result = %d\n", result);
US_EXIT;
	return result;
}

/*
 * Interpret the results of a URB transfer
 *
 * This function prints appropriate debugging messages, clears halts on
 * non-control endpoints, and translates the status to the corresponding
 * USB_STOR_XFER_xxx return code.
 */
static int interpret_urb_result(struct us_data *us, unsigned int pipe,
		unsigned int length, int result, unsigned int partial)
{
	int res = USB_STOR_XFER_GOOD;
	
US_ENTER;
	US_INFO("Status code %d; transferred %d/%d\n",result, partial, length);
	switch (result) {
		/* no error code; did we send all the data? */
		case 0:
			if (partial != length) {
				US_WARN("Short transfer\n");
				res = USB_STOR_XFER_SHORT;
				break;
			}
			US_INFO("Transfer complete\n");
			res = USB_STOR_XFER_GOOD;
			break;
		/* stalled */
		case -USB_EPIPE:
			/* for control endpoints, (used by CB[I]) a stall indicates
			 * a failed command */
			if (usb_pipecontrol(pipe)) {
				US_ERR("Stall on control pipe\n");
				res = USB_STOR_XFER_STALLED;
				break;
			}

			/* for other sorts of endpoint, clear the stall */
			US_ERR("Stall on bulk pipe(%d), execute clearing halt status.\n", pipe);
			if (usb_stor_clear_halt(us, pipe) < 0){
				res = USB_STOR_XFER_ERROR;
				break;
			}
			res = USB_STOR_XFER_STALLED;
			break;
		/* babble - the device tried to send more than we wanted to read */
		case -USB_EOVERFLOW:
			US_ERR("The device tried to send more than we wanted to read\n");
			res = USB_STOR_XFER_LONG;
			break;
		/* the transfer was cancelled by abort, disconnect, or timeout */
		case -USB_ECONNRESET:
			US_ERR("Transfer cancelled\n");
			res = USB_STOR_XFER_ERROR;
			break;
		/* short scatter-gather read transfer */
		case -USB_EREMOTEIO:
			US_ERR("Short read transfer\n");
			res = USB_STOR_XFER_SHORT;
			break;
		/* abort or disconnect in progress */
		case -USB_EIO:
			US_ERR("Abort or disconnect in progress\n");
			res = USB_STOR_XFER_ERROR;
			break;
		/* the catch-all error case */
		default:
			US_ERR("Unknown error %d\n", result);
			res = USB_STOR_XFER_ERROR;
	}
US_EXIT;	
	return res;
}

/*
 * Transfer one buffer via bulk pipe, without timeouts, but allowing early
 * termination.  Return codes are USB_STOR_XFER_xxx.  If the bulk pipe
 * stalls during the transfer, the halt is automatically cleared.
 */
int usb_stor_bulk_transfer_buf(struct us_data *us, unsigned int pipe,
	void *buf, unsigned int length, unsigned int *act_len)
{
	int result;
	US_INFO("xfer %d bytes\n", length);

	/* fill and submit the URB */
	usb_fill_bulk_urb(us->current_urb, us->pusb_dev, pipe, buf, length,
		      usb_stor_blocking_completion, NULL);
	result = usb_stor_msg_common(us, 0);
	/* store the actual length of the data transferred */
	if (act_len)
		*act_len = us->current_urb->actual_length;
	return interpret_urb_result(us, pipe, length, result, 
			us->current_urb->actual_length);
}


/***********************************************************************
 * Transport routines
 ***********************************************************************/

/* There are so many devices that report the capacity incorrectly,
 * this routine was written to counteract some of the resulting
 * problems.
 */
static void last_sector_hacks(struct us_data *us, struct scsi_cmnd *srb)
{
    UNUSED(us);
    UNUSED(srb);
#if 0
	struct gendisk *disk;
	struct scsi_disk *sdkp;
	u32 sector;

	/* To Report "Medium Error: Record Not Found */
	static unsigned char record_not_found[18] = {
		[0]	= 0x70,			/* current error */
		[2]	= MEDIUM_ERROR,		/* = 0x03 */
		[7]	= 0x0a,			/* additional length */
		[12]	= 0x14			/* Record Not Found */
	};

	/* If last-sector problems can't occur, whether because the
	 * capacity was already decremented or because the device is
	 * known to report the correct capacity, then we don't need
	 * to do anything.
	 */
	if (!us->use_last_sector_hacks)
		return;

	/* Was this command a READ(10) or a WRITE(10)? */
	if (srb->cmnd[0] != READ_10 && srb->cmnd[0] != WRITE_10)
		goto done;

	/* Did this command access the last sector? */
	sector = (srb->cmnd[2] << 24) | (srb->cmnd[3] << 16) |
			(srb->cmnd[4] << 8) | (srb->cmnd[5]);
	disk = srb->request->rq_disk;
	if (!disk)
		goto done;
	sdkp = scsi_disk(disk);
	if (!sdkp)
		goto done;
	if (sector + 1 != sdkp->capacity)
		goto done;

	if (srb->result == SAM_STAT_GOOD && scsi_get_resid(srb) == 0) {

		/* The command succeeded.  We know this device doesn't
		 * have the last-sector bug, so stop checking it.
		 */
		us->use_last_sector_hacks = 0;

	} else {
		/* The command failed.  Allow up to 3 retries in case this
		 * is some normal sort of failure.  After that, assume the
		 * capacity is wrong and we're trying to access the sector
		 * beyond the end.  Replace the result code and sense data
		 * with values that will cause the SCSI core to fail the
		 * command immediately, instead of going into an infinite
		 * (or even just a very long) retry loop.
		 */
		if (++us->last_sector_retries < 3)
			return;
		srb->result = SAM_STAT_CHECK_CONDITION;
		memcpy(srb->sense_buffer, record_not_found,
				sizeof(record_not_found));
	}

 done:
	/* Don't reset the retry counter for TEST UNIT READY commands,
	 * because they get issued after device resets which might be
	 * caused by a failed last-sector access.
	 */
	if (srb->cmnd[0] != TEST_UNIT_READY)
		us->last_sector_retries = 0;
#endif
}


/* Invoke the transport and basic error-handling/recovery methods
 *
 * This is used by the protocol layers to actually send the message to
 * the device and receive the response.
 */
void usb_stor_invoke_transport(struct scsi_cmnd *srb, struct us_data *us)
{
	int need_auto_sense;
	int result;
US_ENTER;
	/* send the command to the transport layer */
	scsi_set_resid(srb, 0); // resid: number of bytes requested less actual transferred
	result = us->transport(srb, us);
	
	/* if the command gets aborted by the higher layers, we need to
	 * short-circuit all other processing
	 */
	if (test_bit(US_FLIDX_TIMED_OUT, &us->dflags)) {
		US_ERR("Command was aborted\n");
		srb->result = DID_ABORT << 16;
		goto Handle_Errors;
	}

	/* if there is a transport error, reset and don't auto-sense */
	if (result == USB_STOR_TRANSPORT_ERROR) {
		US_ERR("Transport indicates error, resetting\n");
		srb->result = DID_ERROR << 16;
		goto Handle_Errors;
	}

	/* if the transport provided its own sense data, don't auto-sense */
	if (result == USB_STOR_TRANSPORT_NO_SENSE) {
		srb->result = SAM_STAT_CHECK_CONDITION;
		last_sector_hacks(us, srb);
		US_EXIT;
		return;
	}

	srb->result = SAM_STAT_GOOD;

	/* Determine if we need to auto-sense
	 *
	 * I normally don't use a flag like this, but it's almost impossible
	 * to understand what's going on here if I don't.
	 */
	need_auto_sense = 0;

	/*
	 * If we're running the CB transport, which is incapable
	 * of determining status on its own, we will auto-sense
	 * unless the operation involved a data-in transfer.  Devices
	 * can signal most data-in errors by stalling the bulk-in pipe.
	 */
	if ((us->protocol == USB_PR_CB || us->protocol == USB_PR_DPCM_USB) &&
			srb->sc_data_direction != DMA_FROM_DEVICE) {
		US_INFO("CB transport device requiring auto-sense\n");
		need_auto_sense = 1;
	}

	/*
	 * If we have a failure, we're going to do a REQUEST_SENSE 
	 * automatically.  Note that we differentiate between a command
	 * "failure" and an "error" in the transport mechanism.
	 */
	if (result == USB_STOR_TRANSPORT_FAILED) {
		US_INFO("Transport indicates command failure, invoke auto-sense\n");
		need_auto_sense = 1;
	}

	/*
	 * Determine if this device is SAT by seeing if the
	 * command executed successfully.  Otherwise we'll have
	 * to wait for at least one CHECK_CONDITION to determine
	 * SANE_SENSE support
	 */
	if (unlikely((srb->cmnd[0] == ATA_16 || srb->cmnd[0] == ATA_12) &&
	    result == USB_STOR_TRANSPORT_GOOD &&
	    !(us->fflags & US_FL_SANE_SENSE) &&
	    !(us->fflags & US_FL_BAD_SENSE) &&
	    !(srb->cmnd[2] & 0x20))) {
		US_INFO("SAT supported, increasing auto-sense\n");
		us->fflags |= US_FL_SANE_SENSE;
	}

	/*
	 * A short transfer on a command where we don't expect it
	 * is unusual, but it doesn't mean we need to auto-sense.
	 */
	if ((scsi_get_resid(srb) > 0) &&
	    !((srb->cmnd[0] == REQUEST_SENSE) ||
	      (srb->cmnd[0] == INQUIRY) ||
	      (srb->cmnd[0] == MODE_SENSE) ||
	      (srb->cmnd[0] == LOG_SENSE) ||
	      (srb->cmnd[0] == MODE_SENSE_10))) {
		US_INFO("Unexpectedly short transfer\n");
	}
	/* Now, if we need to do the auto-sense, let's do it */
	if (need_auto_sense) {
		int temp_result;
		struct scsi_eh_save ses; // save state (the old command)
		int sense_size = US_SENSE_SIZE;
		struct scsi_sense_hdr sshdr;
		const u8 *scdd;
		u8 fm_ili;

		unsigned char sense_buff[SCSI_SENSE_BUFFERSIZE];
		memset(sense_buff,0,SCSI_SENSE_BUFFERSIZE);
		srb->sense_buffer = sense_buff;

		/* device supports and needs bigger sense buffer */
		if (us->fflags & US_FL_SANE_SENSE)
			sense_size = ~0;
//Retry_Sense:
		US_INFO("Issuing auto-REQUEST_SENSE\n");

		scsi_eh_prep_cmnd(srb, &ses, NULL, 0, sense_size);

		/* FIXME: we must do the protocol translation here */
		if (us->subclass == USB_SC_RBC || us->subclass == USB_SC_SCSI ||
				us->subclass == USB_SC_CYP_ATACB)
			srb->cmd_len = 6;
		else
			srb->cmd_len = 12;

		/* issue the auto-sense command */
		scsi_set_resid(srb, 0);
		temp_result = us->transport(srb, us);

		/* let's clean up right away */
		// restore the old set of parameter
		scsi_eh_restore_cmnd(srb, &ses);

		if (test_bit(US_FLIDX_TIMED_OUT, &us->dflags)) {
			US_ERR("Auto-sense aborted\n");
			srb->result = DID_ABORT << 16;

			/* If SANE_SENSE caused this problem, disable it */
			if (sense_size != US_SENSE_SIZE) {
				us->fflags &= ~US_FL_SANE_SENSE;
				us->fflags |= US_FL_BAD_SENSE;
			}
			goto Handle_Errors;
		}
//
//		/* Some devices claim to support larger sense but fail when
//		 * trying to request it. When a transport failure happens
//		 * using US_FS_SANE_SENSE, we always retry with a standard
//		 * (small) sense request. This fixes some USB GSM modems
//		 */
//		if (temp_result == USB_STOR_TRANSPORT_FAILED &&
//				sense_size != US_SENSE_SIZE) {
//			USB_US_PRINTF("-- auto-sense failure, retry small sense\n");
//			sense_size = US_SENSE_SIZE;
//			us->fflags &= ~US_FL_SANE_SENSE;
//			us->fflags |= US_FL_BAD_SENSE;
//			goto Retry_Sense;
//		}
//
		/* Other failures */
		if (temp_result != USB_STOR_TRANSPORT_GOOD) {
			US_ERR("Auto-sense failed\n");

			/* we skip the reset if this happens to be a
			 * multi-target device, since failure of an
			 * auto-sense is perfectly valid
			 */
			srb->result = DID_ERROR << 16;
			if (!(us->fflags & US_FL_SCM_MULT_TARG))
				goto Handle_Errors;
			return;
		}
//
		/* If the sense data returned is larger than 18-bytes then we
		 * assume this device supports requesting more in the future.
		 * The response code must be 70h through 73h inclusive.
		 */
		if (srb->sense_buffer[7] > (US_SENSE_SIZE - 8) &&
		    !(us->fflags & US_FL_SANE_SENSE) &&
		    !(us->fflags & US_FL_BAD_SENSE) &&
		    (srb->sense_buffer[0] & 0x7C) == 0x70) {
			US_INFO("SANE_SENSE support enabled\n");
			us->fflags |= US_FL_SANE_SENSE;

			/* Indicate to the user that we truncated their sense
			 * because we didn't know it supported larger sense.
			 */
			US_INFO("Sense data truncated to %d from %d\n",
				     US_SENSE_SIZE,
				     srb->sense_buffer[7] + 8);
			srb->sense_buffer[7] = (US_SENSE_SIZE - 8);
		}

		scsi_normalize_sense(srb->sense_buffer, SCSI_SENSE_BUFFERSIZE,
				     &sshdr);

		US_INFO("Result from auto-sense is %d\n",
			     temp_result);
		US_INFO("Code: 0x%x, key: 0x%x, ASC: 0x%x, ASCQ: 0x%x\n",
			     sshdr.response_code, sshdr.sense_key,
			     sshdr.asc, sshdr.ascq);

		usb_stor_show_sense(sshdr.sense_key, sshdr.asc, sshdr.ascq);

//
		/* set the result so the higher layers expect this data */
		srb->result = SAM_STAT_CHECK_CONDITION;

		scdd = scsi_sense_desc_find(srb->sense_buffer,
					    SCSI_SENSE_BUFFERSIZE, 4);
		fm_ili = (scdd ? scdd[3] : srb->sense_buffer[2]) & 0xA0;
//
		/* We often get empty sense data.  This could indicate that
		 * everything worked or that there was an unspecified
		 * problem.  We have to decide which.
		 */
		if (sshdr.sense_key == 0 && sshdr.asc == 0 && sshdr.ascq == 0 &&
		    fm_ili == 0) {
			/* If things are really okay, then let's show that.
			 * Zero out the sense buffer so the higher layers
			 * won't realize we did an unsolicited auto-sense.
			 */
			if (result == USB_STOR_TRANSPORT_GOOD) {
				srb->result = SAM_STAT_GOOD;
				srb->sense_buffer[0] = 0x0;

			/* If there was a problem, report an unspecified
			 * hardware error to prevent the higher layers from
			 * entering an infinite retry loop.
			 */
			} else {
				srb->result = DID_ERROR << 16;
				if ((sshdr.response_code & 0x72) == 0x72)
					srb->sense_buffer[1] = HARDWARE_ERROR;
				else
					srb->sense_buffer[2] = HARDWARE_ERROR;
			}
		}
	} /* end of need_auto_sense*/

	/*
	 * Some devices don't work or return incorrect data the first
	 * time they get a READ(10) command, or for the first READ(10)
	 * after a media change.  If the INITIAL_READ10 flag is set,
	 * keep track of whether READ(10) commands succeed.  If the
	 * previous one succeeded and this one failed, set the REDO_READ10
	 * flag to force a retry.
	 */
	if (unlikely((us->fflags & US_FL_INITIAL_READ10) &&
			srb->cmnd[0] == READ_10)) {
		if (srb->result == SAM_STAT_GOOD) {
			set_bit(US_FLIDX_READ10_WORKED, &us->dflags);
		} else if (test_bit(US_FLIDX_READ10_WORKED, &us->dflags)) {
			clear_bit(US_FLIDX_READ10_WORKED, &us->dflags);
			set_bit(US_FLIDX_REDO_READ10, &us->dflags);
		}

		/*
		 * Next, if the REDO_READ10 flag is set, return a result
		 * code that will cause the SCSI core to retry the READ(10)
		 * command immediately.
		 */
		if (test_bit(US_FLIDX_REDO_READ10, &us->dflags)) {
			clear_bit(US_FLIDX_REDO_READ10, &us->dflags);
			srb->result = DID_IMM_RETRY << 16;
			srb->sense_buffer[0] = 0;
		}
	}

	/* Did we transfer less than the minimum amount required? */
	if ((srb->result == SAM_STAT_GOOD || srb->sense_buffer[2] == 0) &&
			scsi_bufflen(srb) - scsi_get_resid(srb) < srb->underflow)
		srb->result = DID_ERROR << 16;

	last_sector_hacks(us, srb);
US_EXIT;
	return;

	/* Error and abort processing: try to resynchronize with the device
	 * by issuing a port reset.  If that fails, try a class-specific
	 * device reset. */
Handle_Errors:
	/* Set the RESETTING bit, and clear the ABORTING bit so that
	 * the reset may proceed. */
	set_bit(US_FLIDX_RESETTING, &us->dflags);
	clear_bit(US_FLIDX_ABORTING, &us->dflags);

	/* We must release the device lock because the pre_reset routine
	 * will want to acquire it. */
	rtw_mutex_put(&us->dev_mutex);
	result = usb_stor_port_reset(us);
	rtw_mutex_get(&us->dev_mutex);

	if (result < 0) {
		us->transport_reset(us);
	}
	clear_bit(US_FLIDX_RESETTING, &us->dflags);
	last_sector_hacks(us, srb);
US_EXIT;
}


/*
 * Bulk only transport
 */

/* Determine what the maximum LUN supported is */
int usb_stor_Bulk_max_lun(struct us_data *us)
{
	int result;

	/* issue the command */
	us->iobuf[0] = 0;
	result = usb_stor_control_msg(us, us->recv_ctrl_pipe,
				 US_BULK_GET_MAX_LUN, 
				 USB_DIR_IN | USB_TYPE_CLASS | 
				 USB_RECIP_INTERFACE,
				 0, us->ifnum, us->iobuf, 1, 5*SCSI_HZ);

	US_INFO("GetMaxLUN command result is %d, data is %d\n",
		     result, us->iobuf[0]);

	/* if we have a successful request, return the result */
	if (result > 0)
		return us->iobuf[0];

	/*
	 * Some devices don't like GetMaxLUN.  They may STALL the control
	 * pipe, they may return a zero-length result, they may do nothing at
	 * all and timeout, or they may fail in even more bizarrely creative
	 * ways.  In these cases the best approach is to use the default
	 * value: only one LUN.
	 */
	return 0;
}


int usb_stor_Bulk_transport(struct scsi_cmnd *srb, struct us_data *us)
{
	struct bulk_cb_wrap *bcb = (struct bulk_cb_wrap *) us->iobuf;
	struct bulk_cs_wrap *bcs = (struct bulk_cs_wrap *) us->iobuf;

	unsigned int transfer_length = scsi_bufflen(srb);
	unsigned int residue;
	int result;
	int fake_sense = 0;
	unsigned int cbwlen = US_BULK_CB_WRAP_LEN;
	unsigned int act_cbwlen;
	unsigned int cswlen = US_BULK_CS_WRAP_LEN;
	unsigned int act_cswlen;

US_ENTER;
	/* Take care of BULK32 devices; set extra byte to 0 */
	if (unlikely(us->fflags & US_FL_BULK32)) {
		cbwlen = 32;
		us->iobuf[31] = 0;
	}

	/* set up the command wrapper */
	bcb->Signature = cpu_to_le32(US_BULK_CB_SIGN);
	bcb->DataTransferLength = cpu_to_le32(transfer_length);
	bcb->Flags = srb->sc_data_direction == DMA_FROM_DEVICE?US_BULK_FLAG_IN : US_BULK_FLAG_OUT;
	bcb->Tag = ++us->tag;
	bcb->Lun = srb->lun;
//	if (us->fflags & US_FL_SCM_MULT_TARG)
//		bcb->Lun |= srb->device->id << 4;
	bcb->Length = srb->cmd_len;  // the length of command
    
	/* copy the command payload */
	memset(bcb->CDB, 0, sizeof(bcb->CDB));
	memcpy(bcb->CDB, srb->cmnd, bcb->Length);

	//2 COMMAND STAGE 
	/* send it to out endpoint */
	US_INFO("Bulk Command S 0x%x T 0x%x L %d F %d Trg %d LUN %d CL %d\n",
		     le32_to_cpu(bcb->Signature), bcb->Tag,
		     le32_to_cpu(bcb->DataTransferLength), bcb->Flags,
		     (bcb->Lun >> 4), (bcb->Lun & 0x0F),
		     bcb->Length);
	result = usb_stor_bulk_transfer_buf(us, us->send_bulk_pipe, bcb, cbwlen, &act_cbwlen);
	US_INFO("Bulk command transfer result = %d,Transferred:%d/%d", result, act_cbwlen, cbwlen);
	if (result != USB_STOR_XFER_GOOD){
		US_ERR("Bolk out Command fail.\n");
US_EXIT;
		return USB_STOR_TRANSPORT_ERROR;
	}
	//2 DATA STAGE
	/* send/receive data payload, if there is any */

	/* Some USB-IDE converter chips need a 100us delay between the
	 * command phase and the data phase.  Some devices need a little
	 * more than that, probably because of clock rate inaccuracies. */
	if (unlikely(us->fflags & US_FL_GO_SLOW))
		udelay(125);

	if (transfer_length) {
		unsigned int pipe = srb->sc_data_direction == DMA_FROM_DEVICE ?
				us->recv_bulk_pipe : us->send_bulk_pipe;
		US_INFO("Bulk data transfer begin\n");

#if CONFIG_SG//scatter-gather DMA transfer
		result = usb_stor_bulk_srb(us, pipe, srb);
#else //block DMA transfer
		unsigned int partial;
		result = usb_stor_bulk_transfer_buf(us, pipe, srb->sdb.data_buffer, \
				transfer_length, &partial);
		US_INFO("Bulk data transfer result: 0x%x;Transferred: %d/%d\n", result, \
				partial,transfer_length);
#endif
		if (result == USB_STOR_XFER_ERROR){
			US_ERR("Bolk in/out data fail.\n");
US_EXIT;
			return USB_STOR_TRANSPORT_ERROR;
		}
		/* If the device tried to send back more data than the
		 * amount requested, the spec requires us to transfer
		 * the CSW anyway.  Since there's no point retrying the
		 * the command, we'll return fake sense data indicating
		 * Illegal Request, Invalid Field in CDB.
		 */
		if (result == USB_STOR_XFER_LONG)
			fake_sense = 1;
	}

    memset(bcs, 0, sizeof(struct bulk_cs_wrap));

	//2 STATUS STAGE
	/* get CSW for device status */
	US_INFO("Attempting to get CSW...\n");
	result = usb_stor_bulk_transfer_buf(us, us->recv_bulk_pipe,
				bcs, cswlen, &act_cswlen);

	/* Some broken devices add unnecessary zero-length packets to the
	 * end of their data transfers.  Such packets show up as 0-length
	 * CSWs.  If we encounter such a thing, try to read the CSW again.
	 */
	if (result == USB_STOR_XFER_SHORT && cswlen == 0) {
		US_ERR("Received 0-length CSW; retrying...\n");
        memset(bcs, 0, sizeof(struct bulk_cs_wrap));
		result = usb_stor_bulk_transfer_buf(us, us->recv_bulk_pipe,
				bcs, US_BULK_CS_WRAP_LEN, &cswlen);
	}

	/* did the attempt to read the CSW fail? */
	if (result == USB_STOR_XFER_STALLED) {

		/* get the status again */
		US_WARN("Attempting to get CSW (2nd try)...\n");
        memset(bcs, 0, sizeof(struct bulk_cs_wrap));
		result = usb_stor_bulk_transfer_buf(us, us->recv_bulk_pipe,
				bcs, US_BULK_CS_WRAP_LEN, NULL);
	}

	/* if we still have a failure at this point, we're in trouble */
	US_INFO("Bulk status result = %d; Received:%d/%d \n", \
			result, act_cswlen, cswlen);
	if (result != USB_STOR_XFER_GOOD){
		US_ERR("Receive CWS fail.\n");
US_EXIT;

		return USB_STOR_TRANSPORT_ERROR;
	}

	/* check bulk status */
	residue = le32_to_cpu(bcs->Residue);
	US_INFO("Bulk Status S 0x%x T 0x%x R %d Stat 0x%x\n",
		     le32_to_cpu(bcs->Signature), bcs->Tag,
		     residue, bcs->Status);
	if (!(bcs->Tag == us->tag || (us->fflags & US_FL_BULK_IGNORE_TAG)) ||
		bcs->Status > US_BULK_STAT_PHASE) {
		US_ERR("Bulk logical error\n");
US_EXIT;
		return USB_STOR_TRANSPORT_ERROR;
	}

	/* Some broken devices report odd signatures, so we do not check them
	 * for validity against the spec. We store the first one we see,
	 * and check subsequent transfers for validity against this signature.
	 */
	if (!us->bcs_signature) {
		us->bcs_signature = bcs->Signature;
		if (us->bcs_signature != cpu_to_le32(US_BULK_CS_SIGN)) {
			US_INFO("Learnt BCS signature 0x%08X\n", le32_to_cpu(us->bcs_signature));
        }
	} else if (bcs->Signature != us->bcs_signature) {
		US_ERR("Signature mismatch: got %08X, expecting %08X\n",
			     le32_to_cpu(bcs->Signature),
			     le32_to_cpu(us->bcs_signature));
US_EXIT;
		return USB_STOR_TRANSPORT_ERROR;
	}

	/* try to compute the actual residue, based on how much data
	 * was really transferred and what the device tells us */
	if (residue && !(us->fflags & US_FL_IGNORE_RESIDUE)) {

		/* Heuristically detect devices that generate bogus residues
		 * by seeing what happens with INQUIRY and READ CAPACITY
		 * commands.
		 */
		if (bcs->Status == US_BULK_STAT_OK &&
				scsi_get_resid(srb) == 0 &&
					((srb->cmnd[0] == INQUIRY &&
						transfer_length == 36) ||
					(srb->cmnd[0] == READ_CAPACITY &&
						transfer_length == 8))) {
			us->fflags |= US_FL_IGNORE_RESIDUE;

		} else {
			residue = min(residue, transfer_length);
			scsi_set_resid(srb, max(scsi_get_resid(srb),
			                                       (int) residue));
		}
	}

	/* based on the status code, we report good or bad */
	switch (bcs->Status) {
		case US_BULK_STAT_OK:
			/* device babbled -- return fake sense data */
			if (fake_sense) {
				memcpy(srb->sense_buffer, 
				       usb_stor_sense_invalidCDB, 
				       sizeof(usb_stor_sense_invalidCDB));
US_EXIT;
				return USB_STOR_TRANSPORT_NO_SENSE;
			}
			/* command good -- note that data could be short */
			US_INFO("SCSI command passed\n");
US_EXIT;
			return USB_STOR_TRANSPORT_GOOD;

		case US_BULK_STAT_FAIL:
			US_ERR("SCSI command failed\n");
US_EXIT;
			/* command failed */
			return USB_STOR_TRANSPORT_FAILED;

		case US_BULK_STAT_PHASE:
			US_ERR("SCSI command phase error\n");
US_EXIT;
			/* phase error -- note that a transport reset will be
			 * invoked by the invoke_transport() function
			 */
			return USB_STOR_TRANSPORT_ERROR;
		default:
			US_ERR("SCSI command status reserved\n");
US_EXIT;
			break;
	}
US_EXIT;
	/* we should never get here, but if we do, we're in trouble */
	return USB_STOR_TRANSPORT_ERROR;
}


/***********************************************************************
 * Reset routines
 ***********************************************************************/

/* This is the common part of the device reset code.
 *
 * It's handy that every transport mechanism uses the control endpoint for
 * resets.
 *
 * Basically, we send a reset with a 5-second timeout, so we don't get
 * jammed attempting to do the reset.
 */
static int usb_stor_reset_common(struct us_data *us,
		u8 request, u8 requesttype,
		u16 value, u16 index, void *data, u16 size)
{
	int result;
	int result2;

	if (test_bit(US_FLIDX_DISCONNECTING, &us->dflags)) {
		US_ERR("No reset during disconnect\n");
		return -USB_EIO;
	}

	result = usb_stor_control_msg(us, us->send_ctrl_pipe,
			request, requesttype, value, index, data, size,
			5*SCSI_HZ);
	if (result < 0) {
		US_ERR("Soft reset failed: %d\n", result);
		return result;
	}

	/* Give the device some time to recover from the reset,
	 * but don't delay disconnect processing. */

//	wait_event_interruptible_timeout(us->delay_wait,
//			test_bit(US_FLIDX_DISCONNECTING, &us->dflags),
//			SCSI_HZ*6);
	if (test_bit(US_FLIDX_DISCONNECTING, &us->dflags)) {
		US_ERR("Reset interrupted by disconnect\n");
		return -USB_EIO;
	} else {
        udelay(SCSI_HZ*6);
    }

	US_INFO("Soft reset: clearing bulk-in endpoint halt\n");
	result = usb_stor_clear_halt(us, us->recv_bulk_pipe);

	US_INFO("Soft reset: clearing bulk-out endpoint halt\n");
	result2 = usb_stor_clear_halt(us, us->send_bulk_pipe);

	/* return a result code based on the result of the clear-halts */
	if (result >= 0) {
		result = result2;
    }
    
	if (result < 0) {
		US_ERR("Soft reset failed\n");
    } else {
		US_INFO("Soft reset done\n");
    }
    
	return result;
}

#if 0
/* This issues a CB[I] Reset to the device in question
 */
#define CB_RESET_CMD_SIZE	12

int usb_stor_CB_reset(struct us_data *us)
{
	memset(us->iobuf, 0xFF, CB_RESET_CMD_SIZE);
	us->iobuf[0] = SEND_DIAGNOSTIC;
	us->iobuf[1] = 4;
	return usb_stor_reset_common(us, US_CBI_ADSC, 
				 USB_TYPE_CLASS | USB_RECIP_INTERFACE,
				 0, us->ifnum, us->iobuf, CB_RESET_CMD_SIZE);
}
#endif

/* This issues a Bulk-only Reset to the device in question, including
 * clearing the subsequent endpoint halts that may occur.
 */
int usb_stor_Bulk_reset(struct us_data *us)
{
	return usb_stor_reset_common(us, US_BULK_RESET_REQUEST,
				 USB_TYPE_CLASS | USB_RECIP_INTERFACE,
				 0, us->ifnum, NULL, 0);
}

/* Issue a USB port reset to the device.  The caller must not hold
 * us->dev_mutex.
 */
int usb_stor_port_reset(struct us_data *us)
{
	int result;

	/*for these devices we must use the class specific method */
	if (us->pusb_dev->quirks & USB_QUIRK_RESET)  // The device can not be reset
		return -USB_EPERM;
#if 0
	result = usb_lock_device_for_reset(us->pusb_dev, us->pusb_intf);
	if (result < 0)
		US_INFO("unable to lock device for reset: %d\n",
			     result);
	else {
		/* Were we disconnected while waiting for the lock? */
		if (test_bit(US_FLIDX_DISCONNECTING, &us->dflags)) {
			result = -USB_EIO;
			USB_US_PRINTF("No reset during disconnect\n");
		} else {
			result = usb_reset_device(us->pusb_dev);
			US_INFO("usb_reset_device returns %d\n",
				     result);
		}
		usb_unlock_device(us->pusb_dev);
	}
#endif
	rtw_mutex_get(&us->dev_mutex);
//	result = usb_reset_device(us->pusb_dev);
	result = -1;
	rtw_mutex_put(&us->dev_mutex);
	return result;
}

#endif // CONFIG_USBH_MSC

