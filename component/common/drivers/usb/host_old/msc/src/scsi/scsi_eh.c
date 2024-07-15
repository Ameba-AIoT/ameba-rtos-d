#include <platform_opts.h>

#ifdef CONFIG_USBH_MSC

#include "scsi/scsi.h"
#include "scsi/scsi_eh.h"


/**
 * scsi_eh_prep_cmnd  - Save a scsi command info as part of error recovery
 * @scmd:       SCSI command structure to hijack
 * @ses:        structure to save restore information
 * @cmnd:       CDB to send. Can be NULL if no new cmnd is needed
 * @cmnd_size:  size in bytes of @cmnd (must be <= BLK_MAX_CDB)
 * @sense_bytes: size of sense data to copy. or 0 (if != 0 @cmnd is ignored)
 *
 * This function is used to save a scsi command information before re-execution
 * as part of the error recovery process.  If @sense_bytes is 0 the command
 * sent must be one that does not transfer any data.  If @sense_bytes != 0
 * @cmnd is ignored and this functions sets up a REQUEST_SENSE command
 * and cmnd buffers to read @sense_bytes into @scmd->sense_buffer.
 */
void scsi_eh_prep_cmnd(struct scsi_cmnd *scmd, struct scsi_eh_save *ses,
			unsigned char *cmnd, int cmnd_size, unsigned sense_bytes)
{
//	struct scsi_device *sdev = scmd->device;

	/*
	 * We need saved copies of a number of fields - this is because
	 * error handling may need to overwrite these with different values
	 * to run different commands, and once error handling is complete,
	 * we will need to restore these values prior to running the actual
	 * command.
	 */
	ses->cmd_len = scmd->cmd_len;

	memcpy(ses->cmnd, scmd->cmnd, BLK_MAX_CDB);

	ses->data_direction = scmd->sc_data_direction;
	ses->sdb = scmd->sdb;
	ses->result = scmd->result;
	ses->underflow = scmd->underflow;

	scmd->eh_eflags = 0;
//	scmd->cmnd = ses->eh_cmnd;

	memset(scmd->cmnd, 0, BLK_MAX_CDB);
	memset(&scmd->sdb, 0, sizeof(scmd->sdb));
//	scmd->request->next_rq = NULL;
	scmd->result = 0;

	if (sense_bytes) {
//		scmd->sdb.length = min_t(unsigned, SCSI_SENSE_BUFFERSIZE,
//					 sense_bytes);
        scmd->sdb.length = min((unsigned)SCSI_SENSE_BUFFERSIZE,sense_bytes);
		scmd->sdb.data_buffer = scmd->sense_buffer;
		/*scatter gather transfer relevant*/
//		sg_init_one(&ses->sense_sgl, scmd->sense_buffer,
//			    scmd->sdb.length);
//		scmd->sdb.table.sgl = &ses->sense_sgl;

		scmd->sc_data_direction = DMA_FROM_DEVICE;
//		scmd->sdb.table.nents = 1;
		scmd->cmnd[0] = REQUEST_SENSE;
		scmd->cmnd[4] = scmd->sdb.length;
		scmd->cmd_len = COMMAND_SIZE(scmd->cmnd[0]);
	} else {
		scmd->sc_data_direction = DMA_NONE;
		if (cmnd) {
			memcpy(scmd->cmnd, cmnd, cmnd_size);
			scmd->cmd_len = COMMAND_SIZE(scmd->cmnd[0]);
		}
	}

	scmd->underflow = 0;

//	if (sdev->scsi_level <= SCSI_2 && sdev->scsi_level != SCSI_UNKNOWN)
//		scmd->cmnd[1] = (scmd->cmnd[1] & 0x1f) |
//			(sdev->lun << 5 & 0xe0);

	/*
	 * Zero the sense buffer.  The scsi spec mandates that any
	 * untransferred sense data should be interpreted as being zero.
	 */
	memset(scmd->sense_buffer, 0, SCSI_SENSE_BUFFERSIZE);
}

/**
 * scsi_eh_restore_cmnd  - Restore a scsi command info as part of error recovery
 * @scmd:       SCSI command structure to restore
 * @ses:        saved information from a coresponding call to scsi_eh_prep_cmnd
 *
 * Undo any damage done by above scsi_eh_prep_cmnd().
 */
void scsi_eh_restore_cmnd(struct scsi_cmnd* scmd, struct scsi_eh_save *ses)
{
	/*
	 * Restore original data
	 */
	scmd->cmd_len = ses->cmd_len;
	memcpy(scmd->cmnd,ses->cmnd,BLK_MAX_CDB);
	scmd->sc_data_direction = ses->data_direction;
	scmd->sdb = ses->sdb;
//	scmd->request->next_rq = ses->next_rq;
	scmd->result = ses->result;
	scmd->underflow = ses->underflow;
//	scmd->prot_op = ses->prot_op;
}

/**
 * scsi_normalize_sense - normalize main elements from either fixed or
 *			descriptor sense data format into a common format.
 *
 * @sense_buffer:	byte array containing sense data returned by device
 * @sb_len:		number of valid bytes in sense_buffer
 * @sshdr:		pointer to instance of structure that common
 *			elements are written to.
 *
 * Notes:
 *	The "main elements" from sense data are: response_code, sense_key,
 *	asc, ascq and additional_length (only for descriptor format).
 *
 *	Typically this function can be called after a device has
 *	responded to a SCSI command with the CHECK_CONDITION status.
 *
 * Return value:
 *	1 if valid sense data information found, else 0;
 */
int scsi_normalize_sense(const u8 *sense_buffer, int sb_len,
                         struct scsi_sense_hdr *sshdr)
{
	if (!sense_buffer || !sb_len)
		return 0;

	memset(sshdr, 0, sizeof(struct scsi_sense_hdr));

	sshdr->response_code = (sense_buffer[0] & 0x7f);

	if (!scsi_sense_valid(sshdr))
		return 0;

	if (sshdr->response_code >= 0x72) {
		/*
		 * descriptor format
		 */
		if (sb_len > 1)
			sshdr->sense_key = (sense_buffer[1] & 0xf);
		if (sb_len > 2)
			sshdr->asc = sense_buffer[2];
		if (sb_len > 3)
			sshdr->ascq = sense_buffer[3];
		if (sb_len > 7)
			sshdr->additional_length = sense_buffer[7];
	} else {
		/*
		 * fixed format
		 */
		if (sb_len > 2)
			sshdr->sense_key = (sense_buffer[2] & 0xf);
		if (sb_len > 7) {
			sb_len = (sb_len < (sense_buffer[7] + 8)) ?
					 sb_len : (sense_buffer[7] + 8);
			if (sb_len > 12)
				sshdr->asc = sense_buffer[12];
			if (sb_len > 13)
				sshdr->ascq = sense_buffer[13];
		}
	}
	return 1;
}

/**
 * scsi_sense_desc_find - search for a given descriptor type in	descriptor sense data format.
 * @sense_buffer:	byte array of descriptor format sense data
 * @sb_len:		number of valid bytes in sense_buffer
 * @desc_type:		value of descriptor type to find
 *			(e.g. 0 -> information)
 *
 * Notes:
 *	only valid when sense data is in descriptor format
 *
 * Return value:
 *	pointer to start of (first) descriptor if found else NULL
 */
const u8 * scsi_sense_desc_find(const u8 * sense_buffer, int sb_len,
				int desc_type)
{
	int add_sen_len, add_len, desc_len, k;
	const u8 * descp;

	if ((sb_len < 8) || (0 == (add_sen_len = sense_buffer[7])))
		return NULL;
	if ((sense_buffer[0] < 0x72) || (sense_buffer[0] > 0x73))
		return NULL;
	add_sen_len = (add_sen_len < (sb_len - 8)) ?
			add_sen_len : (sb_len - 8);
	descp = &sense_buffer[8];
	for (desc_len = 0, k = 0; k < add_sen_len; k += desc_len) {
		descp += desc_len;
		add_len = (k < (add_sen_len - 1)) ? descp[1]: -1;
		desc_len = add_len + 2;
		if (descp[0] == desc_type)
			return descp;
		if (add_len < 0) // short descriptor ??
			break;
	}
	return NULL;
}

#endif // CONFIG_USBH_MSC

