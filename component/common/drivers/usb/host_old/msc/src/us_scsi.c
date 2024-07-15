#include <platform_opts.h>

#ifdef CONFIG_USBH_MSC

/**
 * This file realize the main SCSI command
 * for usb mass storage operation
 */
#include "scsi/scsi_cmnd.h"
#include "us_scsi.h"
#include "us_debug.h"
#include "osdep_service.h"

extern struct us_data	gUS_DATA;
//extern struct us_data us;

/* Command group 3 is reserved and should never be used.  */
const unsigned char scsi_command_size_tbl[8] =
{
	6, 10, 10, 12,
	16, 12, 10, 10
};
/**
 *fill usb mass storage INQUIRY command
 */
void usb_fill_inquiry(struct scsi_cmnd *srb){
	srb->id = 0;
	srb->sc_data_direction = DMA_FROM_DEVICE;
	srb->sdb.length = TRANS_LEN_INQUIRY;
	srb->cmd_len = COMMAND_SIZE(INQUIRY);
	memset(&srb->cmnd,0,srb->cmd_len);
	srb->cmnd[0] = INQUIRY;
	srb->cmnd[4] = 0x24;
}

/**
 *fill usb mass storage READ_10 command
 */
void usb_fill_read_10(struct scsi_cmnd *srb){
	srb->id = 0;
	srb->sc_data_direction = DMA_FROM_DEVICE;
	srb->sdb.length = srb->count*BLOCK_SIZE;
	srb->cmd_len = COMMAND_SIZE(READ_10);

	/* fill command feild */
	memset(&srb->cmnd,0 , srb->cmd_len);

	srb->cmnd[0] = READ_10;
	/*logical block address*/
	srb->cmnd[2] =(__u8)((srb->sector >> 24) & 0xff);
	srb->cmnd[3] =(__u8)((srb->sector >> 16) & 0xff);
	srb->cmnd[4] =(__u8)((srb->sector >> 8) & 0xff);
	srb->cmnd[5] =(__u8)(srb->sector & 0xff);

	/* transfer length */
	srb->cmnd[7] =(__u8)((srb->count >> 8) & 0xff);
	srb->cmnd[8] =(__u8)(srb->count & 0xff);
}

/**
 *fill usb mass storage READ_10 command
 */
void usb_fill_write_10(struct scsi_cmnd *srb){
	srb->id = 0;
	srb->sc_data_direction = DMA_TO_DEVICE;
	srb->sdb.length = srb->count*BLOCK_SIZE;
	srb->cmd_len = COMMAND_SIZE(WRITE_10);

	memset(&srb->cmnd,0,srb->cmd_len);
	
	srb->cmnd[0] = WRITE_10;
	/*logical block address*/
	srb->cmnd[2] =(__u8)((srb->sector >> 24) & 0xff);
	srb->cmnd[3] =(__u8)((srb->sector >> 16) & 0xff);
	srb->cmnd[4] =(__u8)((srb->sector >> 8) & 0xff);
	srb->cmnd[5] =(__u8)(srb->sector & 0xff);

	/* transfer length */
	srb->cmnd[7] =(__u8)((srb->count >> 8) & 0xff);
	srb->cmnd[8] =(__u8)(srb->count & 0xff);
}

/**
 *fill usb mass storage READ_capacity command
 */
void usb_fill_read_capacity_10(struct scsi_cmnd *srb){
	srb->id = 0;
	srb->sc_data_direction = DMA_FROM_DEVICE;
	srb->sdb.length = TRANS_LEN_READ_CAPACITY_10;
	srb->cmd_len = COMMAND_SIZE(READ_CAPACITY);

	memset(&srb->cmnd,0,srb->cmd_len);
	srb->cmnd[0] = READ_CAPACITY;
}

void usb_fill_read_capacity_16(struct scsi_cmnd *srb){
	srb->id = 0;
	srb->sc_data_direction = DMA_FROM_DEVICE;
	srb->sdb.length = TRANS_LEN_READ_CAPACITY_16;
//	srb->cmd_len = COMMAND_SIZE(SAI_READ_CAPACITY_16);
	srb->cmd_len = 16;

	memset(&srb->cmnd,0,srb->cmd_len);
	srb->cmnd[0] = SERVICE_ACTION_IN;
	srb->cmnd[1] = SAI_READ_CAPACITY_16;
}

/**
 *fill usb mass storage REQUEST SENSE command
 */
void usb_fill_request_sense(struct scsi_cmnd *srb){
	srb->id = 0;
	srb->sc_data_direction = DMA_FROM_DEVICE;
	srb->sdb.length = TRANS_LEN_REQUEST_SENSE;
	srb->cmd_len = COMMAND_SIZE(REQUEST_SENSE);

	memset(&srb->cmnd,0,srb->cmd_len);
	srb->cmnd[0] = REQUEST_SENSE;
	srb->cmnd[4] = 0x12;
}
/**
 *fill usb mass storage MODE SENSE command
 */
void usb_fill_mode_sense(struct scsi_cmnd *srb){
	srb->id = 0;
	srb->sc_data_direction = DMA_FROM_DEVICE;
	srb->sdb.length = TRANS_LEN_MODE_SENSE;
	srb->cmd_len = COMMAND_SIZE(MODE_SENSE);

	memset(&srb->cmnd,0,srb->cmd_len);
	srb->cmnd[0] = MODE_SENSE;
	srb->cmnd[2] = 0x3F;
	srb->cmnd[4] = 0xC0;
}
/**
 *fill usb mass storage READ_capacity command
 */
void usb_fill_test_unit_ready(struct scsi_cmnd *srb){
	srb->id = 0;
	srb->sc_data_direction = DMA_TO_DEVICE;
	srb->sdb.length = TRANS_LEN_TEST_UNIT_READY;
	srb->cmd_len = COMMAND_SIZE(TEST_UNIT_READY);

	memset(&srb->cmnd,0,srb->cmd_len);
	srb->cmnd[0] = TEST_UNIT_READY;
}

/**
 * called when the scsi command is done
 */
void usb_scsi_done_op(struct scsi_cmnd *srb){
	/*clean the us->srb first, to ensure the next command running*/
	struct us_data * us = &gUS_DATA;
	us->srb = NULL;

	US_INFO("SCSI done!!\n");
	rtw_up_sema(&(srb->cmnd_done));
}

/*
 * fill a scsi_cmnd according to standard SCSI command
 * */
void usb_fill_srb(struct scsi_cmnd *srb, __u8 scsi_op){
	if(srb == NULL)
		return;
	switch (scsi_op){
		case TEST_UNIT_READY:
			usb_fill_test_unit_ready(srb);
			break;
		case INQUIRY:
			usb_fill_inquiry(srb);
			break;
		case READ_10:
			usb_fill_read_10(srb);
			break;
		case WRITE_10:
			usb_fill_write_10(srb);
			break;
		case READ_CAPACITY:
			usb_fill_read_capacity_10(srb);
			break;
		case SAI_READ_CAPACITY_16:
			usb_fill_read_capacity_16(srb);
			break;
		case REQUEST_SENSE:
			usb_fill_request_sense(srb);
			break;
		case MODE_SENSE:
			usb_fill_mode_sense(srb);
			break;
		default:
			break;
	}
	return;
}

int queuecommand(struct scsi_cmnd *srb,
	void (*done)(struct scsi_cmnd *))
{
	struct us_data * us = &gUS_DATA;
	if (us->srb != NULL) {
			US_ERR("SRB is still runing\n");
			return SCSI_MLQUEUE_HOST_BUSY;
		}

	/* fail the command if we are disconnecting */
	if (test_bit(US_FLIDX_DISCONNECTING, &us->dflags)) {
		US_ERR("Fail command during disconnect\n");
		srb->result = DID_NO_CONNECT << 16;
		done(srb);
		return 0;
	}

	/* enqueue the command and wake up the control thread */
	srb->scsi_done = done;	
	us->srb = srb;
	rtw_up_sema(&(us->cmnd_ready)); // wake up the usb_stor_control-thread
	return 0;
}

/*
 * Execute SCSI command, fill a SRB and evoke the control thread.
 * return when the command is finished
 */
int scsi_cmnd_execute(char cmnd, unsigned char * _buff,
						unsigned long _sector, unsigned int _count){
	int retval;
US_ENTER;
	struct scsi_cmnd *srb;
    srb = (struct scsi_cmnd *)rtw_malloc(sizeof(struct scsi_cmnd));

    srb->lun = 0;
	srb->sdb.data_buffer = _buff;
	
	srb->sector = _sector;
	srb->count = _count;
	srb->underflow = 0;
	rtw_init_sema(&(srb->cmnd_done),0);
	usb_fill_srb(srb, cmnd);
	queuecommand(srb, usb_scsi_done_op);
	rtw_down_sema(&(srb->cmnd_done));// wait here until scsi command execute done

	/*Check the command execution status*/
	switch(srb->result){
		case SAM_STAT_GOOD:
			break;
		case SAM_STAT_CHECK_CONDITION:
			US_ERR("Auto-sense was evoked\n");
			//1 show sense info
			// TODO:

			//2 execute command again
			queuecommand(srb, usb_scsi_done_op);
			rtw_down_sema(&(srb->cmnd_done));// wait here until scsi command execute done
			break;
		case SAM_STAT_BUSY:
			// todo
			break;
			// todo:
//			case SAM_STAT_CONDITION_MET:
//			case SAM_STAT_BUSY:
//			case SAM_STAT_INTERMEDIATE:
//			case SAM_STAT_INTERMEDIATE_CONDITION_MET:
//			case SAM_STAT_RESERVATION_CONFLICT:
//			case SAM_STAT_COMMAND_TERMINATED:	/* obsolete in SAM-3 */
//			case SAM_STAT_TASK_SET_FULL:
//			case SAM_STAT_ACA_ACTIVE:
//			case SAM_STAT_TASK_ABORTED:
		default:
			US_ERR("Unknown SCSI execute status! result = %d\n",srb->result);
			break;
	}

US_EXIT;
	retval = srb->result;
	// free memory
	rtw_free_sema(&(srb->cmnd_done));
	rtw_mfree((u8*)srb, sizeof(struct scsi_cmnd));
	return retval;
}

/* To Report "Illegal Request: Invalid Field in CDB */
unsigned char usb_stor_sense_invalidCDB[18] = {
	[0]		= 0x70,			    /* current error */
	[2]		= ILLEGAL_REQUEST,	    /* Illegal Request = 0x05 */
	[7]		= 0x0a,			    /* additional length */
	[12]	= 0x24			    /* Invalid Field in CDB */
};

#endif // CONFIG_USBH_MSC

