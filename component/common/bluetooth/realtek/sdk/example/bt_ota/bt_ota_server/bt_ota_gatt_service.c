/**
*********************************************************************************************************
*               Copyright(c) 2017, Realtek Semiconductor Corporation. All rights reserved.
**********************************************************************************************************
* @file     bt_ota_gatt_service.c
* @brief    BLE OTA profile source file.
* @details  Demonstration of how to implement a self-definition profile.
* @author
* @date
* @version
*********************************************************************************************************
*/

#include <string.h>
#include <trace_app.h>
#include <gap.h>
#include <gap_conn_le.h>
#include <os_mem.h>
#include <bt_ota_gatt_service.h>


#define OTA_SRV_OTA_IMAGE_HEADER_CHAR_INDEX            0x02
#define OTA_SRV_OTA_HEADER_CHECKSUM_CHAR_INDEX         0x04
#define OTA_SRV_OTA_DATA_CHAR_INDEX                    0x06
#define OTA_SRV_OTA_DATA_CHECKSUM_CHAR_INDEX           0x08
#define OTA_SRV_OTA_CTRL_CHAR_INDEX	                   0x0A
#define OTA_SRV_OTA_CTRL_CCCD_IND_INDEX                (OTA_SRV_OTA_CTRL_CHAR_INDEX + 1)

T_SERVER_ID bt_ota_service_id = 0;
const uint8_t bt_ota_service_uuid128[16] = {GATT_UUID128_OTA_SERVICE};

uint8_t bt_ota_dfu_buffer[OTA_DATA_BUFFER_SIZE] = {0};
uint8_t *p_ota_dfu_buffer = NULL;
uint16_t bt_ota_dfu_buffer_used_size = 0;

uint8_t bt_ota_header_buf[OTA_FILE_HEADER_LEN_MAX] = {0};
uint8_t bt_ota_header_offset = 0;

ota_dfu_ctrl_info_t ota_dfu_ctrl_info = {0};

OTA_DISC_STATUS bt_ota_disconnect_flg = OTA_NO_DISC;

/**<  Function pointer used to send event to application from ble ota profile. Initiated in bt_ota_service_add_service. */
static P_FUN_SERVER_GENERAL_CB pfn_bt_ota_service_cb = NULL;

/**< @brief  profile/service definition.  */
const T_ATTRIB_APPL bt_ota_attr_tbl[] =
{
	/* <<Primary Service>>, index 0 */
	{
		(ATTRIB_FLAG_VOID | ATTRIB_FLAG_LE),		/* flags	 */
		{											/* type_value */
			LO_WORD(GATT_UUID_PRIMARY_SERVICE),
			HI_WORD(GATT_UUID_PRIMARY_SERVICE),
		},
		UUID_128BIT_SIZE,							/* bValueLen	 */
		(void *) bt_ota_service_uuid128,			/* p_value_context */
		GATT_PERM_READ								/* permissions	*/
	},
	/* <<Characteristic>>, index 1 */
	{
		ATTRIB_FLAG_VALUE_INCL, 					/* flags */
		{											/* type_value */
			LO_WORD(GATT_UUID_CHARACTERISTIC),
			HI_WORD(GATT_UUID_CHARACTERISTIC),
			GATT_CHAR_PROP_WRITE_NO_RSP 			/* characteristic properties */
			/* characteristic UUID not needed here, is UUID of next attrib. */
		},
		1,											/* bValueLen */
		NULL,
		GATT_PERM_READ								/* permissions */
	},
	/* OTA Image Header characteristic value, index 2 */
	{
		ATTRIB_FLAG_VALUE_APPL, 					/* flags */
		{											/* type_value */
			LO_WORD(GATT_UUID_CHAR_OTA_IMAGE_HEADER),
			HI_WORD(GATT_UUID_CHAR_OTA_IMAGE_HEADER)
		},
		0,											/* bValueLen */
		NULL,
		GATT_PERM_WRITE 							/* permissions */
	},
	/* <<Characteristic>>, index 3 */
	{
		ATTRIB_FLAG_VALUE_INCL, 					/* flags */
		{											/* type_value */
			LO_WORD(GATT_UUID_CHARACTERISTIC),
			HI_WORD(GATT_UUID_CHARACTERISTIC),
			GATT_CHAR_PROP_WRITE_NO_RSP 			/* characteristic properties */
			/* characteristic UUID not needed here, is UUID of next attrib. */
		},
		1,											/* bValueLen */
		NULL,
		GATT_PERM_READ								/* permissions */
	},
	/* OTA Header Checksum characteristic value, index 4 */
	{
		ATTRIB_FLAG_VALUE_APPL, 					/* flags */
		{											/* type_value */
			LO_WORD(GATT_UUID_CHAR_OTA_HEADER_CHECKSUM),
			HI_WORD(GATT_UUID_CHAR_OTA_HEADER_CHECKSUM)
		},
		0,											/* bValueLen */
		NULL,
		GATT_PERM_WRITE 							/* permissions */
	},
	/* <<Characteristic>>, index 5 */
	{
		ATTRIB_FLAG_VALUE_INCL, 					/* flags */
		{											/* type_value */
			LO_WORD(GATT_UUID_CHARACTERISTIC),
			HI_WORD(GATT_UUID_CHARACTERISTIC),
			GATT_CHAR_PROP_WRITE_NO_RSP				/* characteristic properties */
			/* characteristic UUID not needed here, is UUID of next attrib. */
		},
		1,											/* bValueLen */
		NULL,
		GATT_PERM_READ								/* permissions */
	},
	/* OTA Data characteristic value, index 6 */
	{
		ATTRIB_FLAG_VALUE_APPL, 					/* flags */
		{											/* type_value */
			LO_WORD(GATT_UUID_CHAR_OTA_DATA),
			HI_WORD(GATT_UUID_CHAR_OTA_DATA),
		},
		0,											/* bValueLen */
		NULL,
		GATT_PERM_WRITE 							/* permissions */
	},
	/* <<Characteristic>>, index 7 */
	{
		ATTRIB_FLAG_VALUE_INCL, 					/* flags */
		{											/* type_value */
			LO_WORD(GATT_UUID_CHARACTERISTIC),
			HI_WORD(GATT_UUID_CHARACTERISTIC),
			GATT_CHAR_PROP_WRITE_NO_RSP				/* characteristic properties */
			/* characteristic UUID not needed here, is UUID of next attrib. */
		},
		1,											/* bValueLen */
		NULL,
		GATT_PERM_READ								/* permissions */
	},
	/* OTA Data Checksum characteristic value, index 8 */
	{
		ATTRIB_FLAG_VALUE_APPL, 					/* flags */
		{											/* type_value */
			LO_WORD(GATT_UUID_CHAR_OTA_DATA_CHECKSUM),
			HI_WORD(GATT_UUID_CHAR_OTA_DATA_CHECKSUM)
		},
		0,											/* bValueLen */
		NULL,
		GATT_PERM_WRITE 							/* permissions */
	},
	/* <<Characteristic>>, index 9 */
	{
		ATTRIB_FLAG_VALUE_INCL, 					/* flags */
		{											/* type_value */
			LO_WORD(GATT_UUID_CHARACTERISTIC),
			HI_WORD(GATT_UUID_CHARACTERISTIC),
			GATT_CHAR_PROP_INDICATE 				/* characteristic properties */
			/* characteristic UUID not needed here, is UUID of next attrib. */
		},
		1,											/* bValueLen */
		NULL,
		GATT_PERM_READ								/* permissions */
	},
	/* OTA Control characteristic value, index 10 */
	{
		ATTRIB_FLAG_VALUE_APPL, 					/* flags */
		{											/* type_value */
			LO_WORD(GATT_UUID_CHAR_OTA_CTRL),
			HI_WORD(GATT_UUID_CHAR_OTA_CTRL)
		},
		0,											/* bValueLen */
		NULL,
		GATT_PERM_NONE 								/* permissions */
	},
	/* client characteristic configuration, index 11 */
	{
		ATTRIB_FLAG_VALUE_INCL | ATTRIB_FLAG_CCCD_APPL,		/* flags */
		{											/* type_value */
			LO_WORD(GATT_UUID_CHAR_CLIENT_CONFIG),
			HI_WORD(GATT_UUID_CHAR_CLIENT_CONFIG),
			/* NOTE: this value has an instantiation for each client, a write to */
			/* this attribute does not modify this default value:				 */
			LO_WORD(GATT_CLIENT_CHAR_CONFIG_DEFAULT), /* client char. config. bit field */
			HI_WORD(GATT_CLIENT_CHAR_CONFIG_DEFAULT)
		},
		2,											/* bValueLen */
		NULL,
		(GATT_PERM_READ | GATT_PERM_WRITE)			/* permissions */
	}
};

/************************************ Function **********************************************/
void bt_ota_update_new_image(void)
{
	if(ota_dfu_ctrl_info.OtaCompletedFlag) {
		ota_dfu_ctrl_info.OtaCompletedFlag = 0;
		bt_ota_system_reset();
	}
}

void bt_ota_disconnect(uint8_t conn_id)
{
	if (bt_ota_disconnect_flg != OTA_NO_DISC) {
		le_disconnect(conn_id);
		if (bt_ota_disconnect_flg == OTA_COMPLETE_DISC) {
			ota_dfu_ctrl_info.OtaCompletedFlag = 1;
		}
	}
}

uint32_t bt_ota_checksum_cal(uint8_t *buf, uint16_t len)
{
	uint32_t buf_checksum = 0;
	for (int i = 0; i < len; i++) {
		buf_checksum += *(buf + i);
	}

	return buf_checksum;
}

/**
 * @brief send indication of OTA CTRL characteristic value.
 *
 * @param conn_id             connection id.
 * @param service_id          service id.
 * @param opcode              ota ctrl indicate sub-procedures opcode.
 * @param status              result of the target ota ctrl indicate.
 * @param value               value of the target ota ctrl indicate.
 * @return None
*/
bool bt_ota_send_indicate(uint8_t conn_id, T_SERVER_ID service_id, T_OTA_CTRL_IND_OPCODE opcode,
							uint8_t status, uint8_t *value, uint8_t val_len)
{
	uint8_t *p_data = NULL;
	uint8_t length = 0;
	uint8_t send_flg = 1;
	bool ret = false;

	printf("%s: ota ctrl indicate op 0x%02x\r\n", __func__, opcode);

	p_data = (uint8_t *)os_mem_alloc(RAM_TYPE_DATA_ON, 2 + val_len);
	if (p_data == NULL) {
		printf("%s: malloc failed!\r\n", __func__);
		return false;
	}

	memset(p_data, 0, 2 + val_len);
	p_data[0] = opcode;

	switch(opcode) {
	case OTA_IMAGE_HEADER_GET: {
		p_data[1] = status;
		length = 2;
	}
	break;
	case OTA_HEADER_CHECK: {
		p_data[1] = status;
		memcpy(&p_data[2], value, val_len);
		length = 2 + val_len;
	}
	break;
	case OTA_CHECKSUM_VERIFY: {
		p_data[1] = status;
		length = 2;
	}
	break;
	case OTA_COMPLETE: {
		//ota complete indicate packet no value
		length = 1;
	}
	break;
	default: {
		printf("%s: invalid indicate opcode 0x%02x\r\n", __func__, opcode);
		send_flg = 0;
	}
	break;
	}

	if (send_flg) {
		// send indication to client
		ret = server_send_data(conn_id, service_id, OTA_SRV_OTA_CTRL_CHAR_INDEX,
							p_data, length, GATT_PDU_TYPE_INDICATION);
	}

	os_mem_free(p_data);
	p_data = NULL;

	return ret;
}

void bt_ota_start(uint8_t conn_id, T_SERVER_ID service_id)
{
	bt_ota_header_offset = 0;
	p_ota_dfu_buffer = bt_ota_dfu_buffer;
	bt_ota_dfu_buffer_used_size = 0;

	bt_ota_disconnect_flg = OTA_NO_DISC;

	/* after enable indicate cccd, send ota image header get indicate */
	bt_ota_send_indicate(conn_id, service_id, OTA_IMAGE_HEADER_GET, 0x01, NULL, NULL);
}

/**
 * @brief verify ota header checksum and parse header info.
 *
 * @param p_value             the ota header checksum data.
 * @param len                 the length of ota header checksum data.
 * @param ota_hdr[out]        ota header parse value.
 * @return T_OTA_HDR_CHECK_RESULT
*/
T_OTA_HDR_CHECK_RESULT bt_ota_header_check(uint8_t *p_value, uint16_t len, ota_update_header_info_t *ota_hdr)
{
	uint32_t hdr_checksum = 0;
	uint8_t hdr_len = 0;
	uint32_t cal_checksum = 0;
	T_OTA_HDR_CHECK_RESULT cause = OTA_HDR_CHECK_OK;

	if (len != OTA_HEADER_CHECKSUM_PACKET_LEN) {
		printf("%s: len=%d is error\r\n", __func__, len);
		return OTA_HDR_LEN_ERR;
	}

	LE_ARRAY_TO_UINT32(hdr_checksum, &p_value[0]);
	hdr_len = p_value[4];

	if (hdr_len != bt_ota_header_offset) {
		printf("%s: OTA header length[%d %d] error!\r\n", __func__, hdr_len, bt_ota_header_offset);
		return OTA_HDR_LEN_ERR;
	}

	/* ota header checksum verify */
	cal_checksum = bt_ota_checksum_cal(bt_ota_header_buf, bt_ota_header_offset);
	if (cal_checksum != hdr_checksum) {
		printf("%s: OTA header checksum[0x%08x 0x%08x] error!\r\n", __func__, hdr_checksum, cal_checksum);
		cause = OTA_HDR_CHECKSUM_ERR;
	} else {
		/* parse and check ota header info */
		if (bt_ota_parse_target_header(bt_ota_header_buf, bt_ota_header_offset, ota_hdr) == false) {
			cause = OTA_HDR_INFO_ERR;
		}
	}

	return cause;
}

/**
 * @brief save the ota header data to the buffer.
 *
 * @param p_value             the recv ota header data.
 * @param len                 the length of recv ota header data.
 * @return None
*/
void bt_ota_handle_ota_header_packet(uint8_t conn_id, uint8_t *p_value, uint16_t len)
{
	if (bt_ota_header_offset > OTA_FILE_HEADER_LEN_MAX) {
		printf("%s: bt_ota_header_offset is %d\r\n", __func__, bt_ota_header_offset);
		le_disconnect(conn_id);
		return;
	}

	memcpy(bt_ota_header_buf + bt_ota_header_offset, &p_value[0], len);
	bt_ota_header_offset = bt_ota_header_offset + len;
}

/**
 * @brief handle the received ota header checksum packet.
 *
 * @param conn_id             connection id.
 * @param p_value             ota header checksum packet.
 * @param len                 the length of ota header checksum packet.
 * @return 0--OTA not continue, 1--OTA continue
*/
uint8_t bt_ota_handle_ota_header_checksum_packet(uint8_t conn_id, uint8_t *p_value, uint16_t len)
{
	T_OTA_HDR_CHECK_RESULT cause = OTA_HDR_CHECK_OK;
	ota_update_header_info_t ota_hdr;
	memset(&ota_hdr, 0, sizeof(ota_update_header_info_t));

	/*verify ota header checksum and parse ota header info*/
	cause = bt_ota_header_check(p_value, len, &ota_hdr);
	if (cause != OTA_HDR_CHECK_OK) {
		bt_ota_send_indicate(conn_id, bt_ota_service_id, OTA_HEADER_CHECK, cause, NULL, NULL);
		bt_ota_disconnect_flg = OTA_ERR_DISC;

		return 0;
	}

	/* continue OTA process, if ota header check ok */
	if (bt_ota_image_retransmission_check(&ota_hdr)) { //ota retransmission
		if (ota_dfu_ctrl_info.index < ota_dfu_ctrl_info.OtaHdrInfo.ValidImgCnt) {
			printf("===the target OTA image is the previous ota image!===\r\n");
		} else {
			printf("OTA server has received all OTA data, OTA process completed!!!\r\n");
			bt_ota_send_indicate(conn_id, bt_ota_service_id, OTA_COMPLETE, NULL, NULL, NULL);
			bt_ota_disconnect_flg = OTA_COMPLETE_DISC;

			return 0;
		}
	} else {
		printf("===the target OTA image is a new ota image!===\r\n");
		memset(&ota_dfu_ctrl_info, 0, sizeof(ota_dfu_ctrl_info_t));

		memcpy(&ota_dfu_ctrl_info.OtaHdrInfo, &ota_hdr, sizeof(ota_update_header_info_t));
		ota_dfu_ctrl_info.ota_image_total_len = bt_ota_get_image_len(ota_dfu_ctrl_info.image_len);
		bt_ota_get_flash_addr(ota_dfu_ctrl_info.flash_addr);
		ota_dfu_ctrl_info.index = 0;  //for the first ota fw image, index is 0
		ota_dfu_ctrl_info.cur_offset = 0;
		ota_dfu_ctrl_info.checksum = 0;
	}

	/* ota image header check ok, inform remote device to start ota process */
	uint8_t value[5] = {0};
	uint8_t val_len = 5;
	LE_UINT32_TO_ARRAY(&value[0], ota_dfu_ctrl_info.cur_offset);
	value[4] = ota_dfu_ctrl_info.index;
	bt_ota_send_indicate(conn_id, bt_ota_service_id, OTA_HEADER_CHECK, OTA_HDR_CHECK_OK, value, val_len);

	return 1;
}

/**
 * @brief handle the received ota image data packet.
 *
 * @param conn_id             connection id.
 * @param p_value             the recv ota image data.
 * @param len                 the length of recv ota image data.
 * @return 0--not reach buffer max length, 1--reach buffer max length or image total length
*/
uint8_t bt_ota_handle_ota_image_data_packet(uint8_t conn_id, uint8_t *p_value, uint16_t len)
{
	uint8_t index = ota_dfu_ctrl_info.index;
	uint32_t cur_image_total_len = ota_dfu_ctrl_info.image_len[index];

	if (bt_ota_dfu_buffer_used_size + len > OTA_DATA_BUFFER_SIZE) {
		printf("%s: buffer overflow! ota_dfu_buffer_used_size=%d, buffer_size=%d\r\n", __func__,
			   bt_ota_dfu_buffer_used_size + len, OTA_DATA_BUFFER_SIZE);
		le_disconnect(conn_id);
		return 0;
	}

	memcpy(p_ota_dfu_buffer + bt_ota_dfu_buffer_used_size, p_value, len);
	bt_ota_dfu_buffer_used_size += len;

	/*if buffer full, received data reach 4k and wait for checksum from remote device*/
	if(bt_ota_dfu_buffer_used_size == OTA_DATA_BUFFER_SIZE ||
		ota_dfu_ctrl_info.cur_offset + bt_ota_dfu_buffer_used_size == cur_image_total_len) {
		return 1;
	}

	return 0;
}

/**
 * @brief handle the received ota image data checksum packet.
 *
 * @param conn_id             connection id.
 * @param p_value             the recv ota image data.
 * @param len                 the length of recv ota image data.
 * @return 0--OTA not continue, 1--OTA continue
*/
uint8_t bt_ota_handle_ota_data_checksum_packet(uint8_t conn_id, uint8_t *p_value, uint16_t len)
{
	uint32_t buf_check = 0;
	uint32_t checksum = 0;
	uint32_t remote_offset = 0;
	uint16_t n_image_len = 0;
	uint32_t cur_image_len = 0;
	uint8_t status = 0;

	if (len != 10) {
		printf("%s: len %d error!\r\n", __func__, len);
		return 0;
	}

	/* handle received data and verify checksum */
	LE_ARRAY_TO_UINT32(checksum, p_value);
	p_value += 4;
	LE_ARRAY_TO_UINT32(remote_offset, p_value);
	p_value += 4;
	LE_ARRAY_TO_UINT16(n_image_len, p_value);

	if (remote_offset != ota_dfu_ctrl_info.cur_offset + n_image_len) {
		printf("%s: offset value error\r\n", __func__);
		status = OTA_DATA_OFFSET_ERR;
		bt_ota_disconnect_flg = OTA_ERR_DISC;
		goto exit;
	}

	buf_check = bt_ota_checksum_cal(p_ota_dfu_buffer, bt_ota_dfu_buffer_used_size);

	/* write data to flash if checksum ok */
	if (checksum == buf_check) {
		if (ota_dfu_ctrl_info.cur_offset == 0) {//skip image signature region
			//save image signature data, and then set it 0 to save to flash
			bt_ota_save_image_signature(p_ota_dfu_buffer, ota_dfu_ctrl_info.index);

			/* write flash without the signature */
			bt_ota_write_flash(ota_dfu_ctrl_info.flash_addr[ota_dfu_ctrl_info.index] + 8,
								p_ota_dfu_buffer + 8, bt_ota_dfu_buffer_used_size - 8);
		} else {
			bt_ota_write_flash(ota_dfu_ctrl_info.flash_addr[ota_dfu_ctrl_info.index] + ota_dfu_ctrl_info.cur_offset,
								p_ota_dfu_buffer, bt_ota_dfu_buffer_used_size);
		}

		bt_ota_dfu_buffer_used_size = 0;
		ota_dfu_ctrl_info.checksum = ota_dfu_ctrl_info.checksum + buf_check;
		ota_dfu_ctrl_info.cur_offset += n_image_len;
		cur_image_len = ota_dfu_ctrl_info.image_len[ota_dfu_ctrl_info.index];

		if (ota_dfu_ctrl_info.cur_offset < cur_image_len) {
			printf(">>>Image[%d], n_image_len %d, received %d, image_len %d\r\n", ota_dfu_ctrl_info.index,
				   n_image_len, ota_dfu_ctrl_info.cur_offset, cur_image_len);
			status = OTA_CHECKSUM_OK;
			goto exit;
		} else {
			printf(">>>%s: received %d successfully\r\n", __func__, cur_image_len);
			ota_dfu_ctrl_info.index += 1;
		}

	} else { //checksum verify error
		printf("%s: checksum[0x%08x->0x%08x] error\r\n", __func__, checksum, buf_check);
		status = OTA_DATA_CHECKSUM_ERR;
		bt_ota_disconnect_flg = OTA_ERR_DISC;
		goto exit;
	}

	/* check if next image */
	if (ota_dfu_ctrl_info.index < ota_dfu_ctrl_info.OtaHdrInfo.ValidImgCnt) {
		printf(">>>Check for another fw image\r\n");
		/*initialize param for another fw image*/
		ota_dfu_ctrl_info.cur_offset = 0;
		ota_dfu_ctrl_info.checksum = 0;
		status = OTA_CHECKSUM_OK;
		goto exit;
	} else {
		printf(">>>Receive all OTA image data, total length %d<<<\r\n",ota_dfu_ctrl_info.ota_image_total_len);

		/*if current received ota image checksum verify ok, then update the signature*/
		if (bt_ota_image_checksum_verify(ota_dfu_ctrl_info.flash_addr) == true) {
			bt_ota_update_image_signature(ota_dfu_ctrl_info.flash_addr);
		} else {
			printf("%s: OTA image total checksum error, not update\r\n", __func__);
			status = OTA_IMAGE_CHECKSUM_ERR;
			bt_ota_disconnect_flg = OTA_ERR_DISC;
			goto exit;
		}

		printf("====== OTA update successfully ======\r\n");
		/*inform remote device ota completed successfully*/
		bt_ota_send_indicate(conn_id, bt_ota_service_id, OTA_COMPLETE, NULL, NULL, NULL);
		bt_ota_disconnect_flg = OTA_COMPLETE_DISC;
	}

	return 0;

exit: //send checksum verify result
	bt_ota_send_indicate(conn_id, bt_ota_service_id, OTA_CHECKSUM_VERIFY, status, NULL, NULL);
	return (status == OTA_CHECKSUM_OK) ? 1 : 0;
}

/**
 * @brief write characteristic data from service.
 *
 * @param conn_id           Connection id
 * @param service_id        ServiceID to be written.
 * @param attrib_index      Attribute index of characteristic.
 * @param length            length of value to be written.
 * @param p_value           value to be written.
 * @return Profile procedure result
*/
T_APP_RESULT bt_ota_attr_write_cb(uint8_t conn_id, T_SERVER_ID service_id,uint16_t attrib_index,
											T_WRITE_TYPE write_type, uint16_t length, uint8_t *p_value,
											P_FUN_WRITE_IND_POST_PROC *p_write_post_proc)
{
    APP_PRINT_INFO2("bt_ota_attr_write_cb: write_type=0x%02x, attrib_index=0x%02x", write_type, attrib_index);

    T_APP_RESULT  cause = APP_RESULT_SUCCESS;
	T_OTA_CALLBACK_DATA callback_data;

	if (attrib_index == OTA_SRV_OTA_IMAGE_HEADER_CHAR_INDEX) {
		callback_data.msg_data.write.opcode = OTA_IMAGE_HEADER_WRITE;
	} else if (attrib_index == OTA_SRV_OTA_HEADER_CHECKSUM_CHAR_INDEX) {
		callback_data.msg_data.write.opcode = OTA_HEADER_CHECKSUM_WRITE;
	} else if (attrib_index == OTA_SRV_OTA_DATA_CHAR_INDEX) {
		callback_data.msg_data.write.opcode = OTA_DATA_WRITE;
	} else if (attrib_index == OTA_SRV_OTA_DATA_CHECKSUM_CHAR_INDEX) {
		callback_data.msg_data.write.opcode = OTA_DATA_CHECKSUM_WRITE;
	} else {
		APP_PRINT_ERROR2("bt_ota_attr_write_cb Error: attrib_index 0x%x, length %d", attrib_index, length);
        cause = APP_RESULT_ATTR_NOT_FOUND;
	}

	if (cause == APP_RESULT_SUCCESS) {
		callback_data.conn_id = conn_id;
		callback_data.msg_type = SERVICE_CALLBACK_TYPE_WRITE_CHAR_VALUE;
		callback_data.msg_data.write.write_type = write_type;
		callback_data.msg_data.write.len = length;
		callback_data.msg_data.write.p_value = p_value;

		if (pfn_bt_ota_service_cb) {
			pfn_bt_ota_service_cb(service_id, (void *)&callback_data);
		}
	}

	return cause;
}

/**
 * @brief update CCCD bits from stack.
 *
 * @param conn_id        connection id.
 * @param service_id     Service ID.
 * @param index          Attribute index of characteristic data.
 * @param cccbits        CCCD bits from stack.
 * @return None
*/
void bt_ota_cccd_update_cb(uint8_t conn_id, T_SERVER_ID service_id, uint16_t index, uint16_t cccbits)
{
	APP_PRINT_INFO2("bt_ota_cccd_update_cb: index %d, cccbits 0x%x", index, cccbits);

	bool is_handle = false;
	T_OTA_CALLBACK_DATA callback_data;
	callback_data.conn_id = conn_id;
	callback_data.msg_type = SERVICE_CALLBACK_TYPE_INDIFICATION_NOTIFICATION;

	if (index == OTA_SRV_OTA_CTRL_CCCD_IND_INDEX) {
		if (cccbits & GATT_CLIENT_CHAR_CONFIG_INDICATE) {
			callback_data.msg_data.notify_indicate_status = OTA_CTRL_INDICATE_ENABLE;
		} else {
			callback_data.msg_data.notify_indicate_status = OTA_CTRL_INDICATE_DISABLE;
		}
		is_handle = true;
	} else {
		printf("%s: invalid index 0x%02x\r\n", __func__, index);
	}

	if (pfn_bt_ota_service_cb && (is_handle == true)) {
		pfn_bt_ota_service_cb(service_id, (void *)&callback_data);
	}
}

/**
 * @brief BLE OTA Service Callbacks.
*/
const T_FUN_GATT_SERVICE_CBS bt_ota_cbs =
{
	NULL,  //Read callback function pointer
	bt_ota_attr_write_cb, //Write callback function pointer
	bt_ota_cccd_update_cb //CCCD update callback function pointer
};

/**
  * @brief Add BLE OTA service to the BLE stack database.
  *
  * @param[in]   p_func  Callback when service attribute was read, write or cccd update.
  * @return Service id generated by the BLE stack: @ref T_SERVER_ID.
  * @retval 0xFF Operation failure.
  * @retval others Service id assigned by stack.
  *
  */
T_SERVER_ID bt_ota_add_service(void *p_func)
{
	if (false == server_add_service(&bt_ota_service_id,
									(uint8_t *)bt_ota_attr_tbl,
									sizeof(bt_ota_attr_tbl),
									bt_ota_cbs))
	{
		APP_PRINT_ERROR0("bt_ota_add_service: fail");
		bt_ota_service_id = 0xff;
		return bt_ota_service_id;
	}

	pfn_bt_ota_service_cb = (P_FUN_SERVER_GENERAL_CB)p_func;
	return bt_ota_service_id;
}

