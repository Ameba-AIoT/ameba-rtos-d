/** Add Includes here **/
#include <bt_flags.h>
#if F_BT_LE_GATT_CLIENT_SUPPORT
#include <stdio.h>
#include <string.h>
#include <trace_app.h>
#include <gap_conn_le.h>
#include <bt_ota_gatt_client.h>
#include <os_mem.h>

/**
 * @brief  BLE OTA client Link control block defination.
 */
typedef struct
{
	T_OTA_DISC_STATE disc_state;
	uint16_t         hdl_cache[HDL_OTA_CACHE_LEN];
} T_OTA_LINK, *P_OTA_LINK;

static P_OTA_LINK bt_ota_client_table;

const uint8_t bt_ota_service_uuid[16] = {GATT_UUID128_OTA_SERVICE};
uint8_t bt_ota_push_image_status = 0;
uint8_t bt_ota_client_link_num;

ota_msg_t ota_tx_info = {0};

/**<  BLE OTA client ID. */
static T_CLIENT_ID bt_ota_client = CLIENT_PROFILE_GENERAL_ID;
/**<  Callback used to send data to app from BLE OTA client layer. */
static P_FUN_GENERAL_APP_CB p_ota_client_cb = NULL;

/**
  * @brief  Used by application, to set the handles in OTA handle cache.
  * @param  handle_type: handle types of this specific profile.
  * @param  handle_value: handle value to set.
  * @retval true--set success.
  *         flase--set failed.
  */
bool bt_ota_client_set_hdl_cache(uint8_t conn_id, uint16_t *p_hdl_cache, uint8_t len)
{
	if (conn_id >= bt_ota_client_link_num) {
		APP_PRINT_ERROR1("bt_ota_client_set_hdl_cache: failed invalid conn_id %d", conn_id);
		return false;
	}

	if (bt_ota_client_table[conn_id].disc_state != DISC_OTA_IDLE) {
		APP_PRINT_ERROR1("bt_ota_client_set_hdl_cache: failed invalid state %d",
						 bt_ota_client_table[conn_id].disc_state);
		return false;
	}

	if (len != sizeof(uint16_t) * HDL_OTA_CACHE_LEN) {
		APP_PRINT_ERROR1("bt_ota_client_set_hdl_cache: failed invalid len %d", len);
		return false;
	}

	memcpy(bt_ota_client_table[conn_id].hdl_cache, p_hdl_cache, len);
	bt_ota_client_table[conn_id].disc_state = DISC_OTA_DONE;

	return true;
}

/**
  * @brief  Used by application, to get handle cache.
  * @param[in]  conn_id connection ID.
  * @param[in]  p_hdl_cache pointer of the handle cache table
  * @param[in]  len the length of handle cache table
  * @retval true success.
  * @retval false failed.
  */
bool bt_ota_client_get_hdl_cache(uint8_t conn_id, uint16_t *p_hdl_cache, uint8_t len)
{
	if (conn_id >= bt_ota_client_link_num) {
		APP_PRINT_ERROR1("bt_ota_client_get_hdl_cache: failed invalid conn_id %d", conn_id);
		return false;
	}

	if (bt_ota_client_table[conn_id].disc_state != DISC_OTA_DONE) {
		APP_PRINT_ERROR1("bt_ota_client_get_hdl_cache: failed invalid state %d",
						 bt_ota_client_table[conn_id].disc_state);
		return false;
	}

	if (len != sizeof(uint16_t) * HDL_OTA_CACHE_LEN) {
		APP_PRINT_ERROR1("bt_ota_client_get_hdl_cache: failed invalid len %d", len);
		return false;
	}

	memcpy(p_hdl_cache, bt_ota_client_table[conn_id].hdl_cache, len);

	return true;
}

uint32_t bt_ota_client_checksum_cal(uint8_t *buf, uint16_t len)
{
	uint32_t checksum = 0;
	for (int i = 0; i < len; i++) {
		checksum += *(buf + i);
	}

	return checksum;
}

void bt_ota_client_set_image_param(uint8_t index)
{
	//ota_tx_info.checksum = 0;//debug
	ota_tx_info.cur_offset = 0;
	ota_tx_info.image_offset = bt_ota_client_get_image_offset(index);
	ota_tx_info.cur_image_len = bt_ota_client_get_image_len(index);
}

bool bt_ota_client_write_data(uint8_t conn_id, uint8_t *p_data, uint16_t len, uint16_t handle)
{
	T_GAP_CAUSE ret = client_attr_write(conn_id, bt_ota_client, GATT_WRITE_TYPE_CMD, handle, len, p_data);

	if (ret != GAP_CAUSE_SUCCESS) {
		APP_PRINT_ERROR1("bt_ota_client_write_data: write failed, 0x%02x", ret);
		return false;
	}

	return true;
}

void bt_ota_client_send_ota_header(uint8_t conn_id)
{
	uint16_t handle = 0;
	uint8_t pkt_len = 0;
	uint8_t offset = 0;
	uint8_t *p_data = NULL;
	uint8_t *p_header = NULL;

	le_get_conn_param(GAP_PARAM_CONN_MTU_SIZE, &ota_tx_info.mtu_size, 0);

	p_header = bt_ota_client_get_image();
	if (p_header == NULL) {
		printf("%s: invalid ota header pointer\r\n", __func__);
		return;
	}

	bt_ota_client_handle_image_data(p_header, &ota_tx_info.ota_image_total_len, &ota_tx_info.hdr_total_len);
	ota_tx_info.hdr_num = (uint8_t)bt_ota_client_get_header_num();

	/* send ota header data to remote device, if mtu size is small it need to be send as fragementation */
	while(offset < ota_tx_info.hdr_total_len) {
		if ((ota_tx_info.hdr_total_len - offset) > (ota_tx_info.mtu_size - 3)) {
			pkt_len = ota_tx_info.mtu_size - 3;
		} else {
			pkt_len = ota_tx_info.hdr_total_len - offset;
		}

		p_data = (uint8_t *)os_mem_alloc(RAM_TYPE_DATA_ON, pkt_len);
		if (!p_data) {
			printf("%s: malloc failed!\r\n", __func__);
			break;
		}
		memset(p_data, 0, pkt_len);

		memcpy(p_data, p_header + offset, pkt_len);
		handle = bt_ota_client_table[conn_id].hdl_cache[HDL_OTA_IMG_HDR];

		if (bt_ota_client_write_data(conn_id, p_data, pkt_len, handle) == true) {
			offset = offset + pkt_len;
			os_mem_free(p_data);
			p_data = NULL;
		} else {
			printf("%s: ota header data write fail\r\n", __func__);
			os_mem_free(p_data);
			p_data = NULL;
			break;
		}
	}

	/* send the ota header checksum to remote, if ota header data has been send successfully */
	if (offset == ota_tx_info.hdr_total_len) {
		uint8_t data[OTA_CLIENT_HDR_CHECK_PKT_LEN] = {0}; //4+1
		uint32_t checksum = 0;

		checksum = bt_ota_client_checksum_cal(p_header, ota_tx_info.hdr_total_len);

		LE_UINT32_TO_ARRAY(&data[0], checksum);
		data[4] = ota_tx_info.hdr_total_len;
		handle = bt_ota_client_table[conn_id].hdl_cache[HDL_OTA_HDR_CHECKSUM];

		if (bt_ota_client_write_data(conn_id, data, OTA_CLIENT_HDR_CHECK_PKT_LEN, handle) == false) {
			printf("%s: ota header checksum write fail!\r\n", __func__);
		}
	}
}

void bt_ota_client_send_image_checksum(uint8_t conn_id)
{
	uint32_t checksum = 0;
	uint16_t nImage_size = 0;
	uint8_t *p_header = NULL;
	uint16_t handle = 0;

	p_header = bt_ota_client_get_image();
	p_header = p_header + ota_tx_info.image_offset;  //point to ota fw image start position

	if (ota_tx_info.cur_offset % OTA_CLIENT_BUFFER_SIZE == 0) {
		nImage_size = OTA_CLIENT_BUFFER_SIZE;
	} else {
		nImage_size = ota_tx_info.cur_offset % OTA_CLIENT_BUFFER_SIZE;
	}

	checksum = bt_ota_client_checksum_cal(p_header + ota_tx_info.cur_offset - nImage_size, nImage_size);

	uint8_t data[OTA_CLIENT_DATA_CHECKSUM_PKT_LEN] = {0}; //4+4+2
	LE_UINT32_TO_ARRAY(data, checksum);
	LE_UINT32_TO_ARRAY(data + 4, ota_tx_info.cur_offset);
	LE_UINT16_TO_ARRAY(data + 4 + 4, nImage_size);
	handle = bt_ota_client_table[conn_id].hdl_cache[HDL_OTA_DATA_CHECKSUM];

	if (bt_ota_client_write_data(conn_id, data, OTA_CLIENT_DATA_CHECKSUM_PKT_LEN, handle) == false) {
		printf("%s: send data checksum fail\r\n", __func__);
	}
}

void bt_ota_client_push_image(uint8_t conn_id)
{
	uint8_t *p_header = NULL;
	uint8_t *p_data = NULL;
	uint16_t push_image_size = 0;
	uint32_t image_size = 0; //the image size needed to be sent in a buffer
	uint16_t pkt_max_size = 0;
	uint16_t handle = 0;

	if (ota_tx_info.cur_offset < (ota_tx_info.cur_image_len - ota_tx_info.cur_image_len % OTA_CLIENT_BUFFER_SIZE)) {
		//remain image size relative to the buffer
		image_size = OTA_CLIENT_BUFFER_SIZE - (ota_tx_info.cur_offset % OTA_CLIENT_BUFFER_SIZE);
	} else {
		//remian size relative to current image len
		image_size = ota_tx_info.cur_image_len - ota_tx_info.cur_offset;
	}

	pkt_max_size = ota_tx_info.mtu_size - 3;
	push_image_size = image_size > pkt_max_size ? pkt_max_size : image_size; //actual push image size (actual data len of packet)

	p_header = bt_ota_client_get_image();
	p_header = p_header + ota_tx_info.image_offset;  //point to ota fw image start position
	p_data = (uint8_t *)os_mem_alloc(RAM_TYPE_DATA_ON, push_image_size); //send one packet(mtu size) or remian image size
	if (!p_data) {
		printf("%s: malloc failed!\r\n", __func__);
		return;
	}

	memcpy(p_data, p_header + ota_tx_info.cur_offset, push_image_size);
	handle = bt_ota_client_table[conn_id].hdl_cache[HDL_OTA_DATA];

	if (bt_ota_client_write_data(conn_id, p_data, push_image_size, handle) == false) {
		printf("%s: push image data fail\r\n", __func__);
		os_mem_free(p_data);
		return;
	}

	ota_tx_info.cur_offset += push_image_size;

	if (image_size == push_image_size || ota_tx_info.cur_offset == ota_tx_info.cur_image_len) {
		printf(">>>Image[%d] cur_image_len=%d, cur_offset=%d, image_size=%d, push_image_size=%d\r\n",
			   ota_tx_info.index, ota_tx_info.cur_image_len, ota_tx_info.cur_offset, image_size, push_image_size);
		bt_ota_push_image_status = 1;
	}

	os_mem_free(p_data);
}


/**
  * @brief  Used by application, to enable or disable the indication of peer server's OTA CTRL Characteristic.
  * @param[in]  conn_id connection ID.
  * @param[in]  indicate 0--disable the indication, 1--enable the indication.
  * @retval true send request to upper stack success.
  * @retval false send request to upper stack failed.
  */
bool bt_ota_client_enable_indicate_cccd(uint8_t conn_id, bool indicate)
{
	uint16_t handle = bt_ota_client_table[conn_id].hdl_cache[HDL_OTA_CTRL_CCCD];
	uint16_t length = 0;
	uint16_t cccd_bits;

	if (handle) {
		length = sizeof(uint16_t);
		cccd_bits = indicate ? 2 : 0;
		if (client_attr_write(conn_id, bt_ota_client, GATT_WRITE_TYPE_REQ, handle, length,
							  (uint8_t *)&cccd_bits) == GAP_CAUSE_SUCCESS) {
			return true;
		}
	}

	APP_PRINT_INFO0("bt_ota_client_enable_indicate_cccd: Request fail! Please check!");

	return false;
}

/**
  * @brief  Used by application, to start the discovery procedure of OTA server.
  * @param[in]  conn_id connection ID.
  * @retval true send request to upper stack success.
  * @retval false send request to upper stack failed.
  */
bool bt_ota_client_start_discovery(uint8_t conn_id)
{
	APP_PRINT_INFO0("bt_ota_client_start_discovery");

	T_GAP_CAUSE ret = GAP_CAUSE_SUCCESS;

	if (conn_id > bt_ota_client_link_num) {
		APP_PRINT_INFO1("bt_ota_client_start_discovery: failed invalid conn_id %d", conn_id);
		return false;
	}

	/* clear handle cache */
	memset(&bt_ota_client_table[conn_id], 0, sizeof(T_OTA_LINK));
	bt_ota_client_table[conn_id].disc_state = DISC_OTA_START;

	ret = client_by_uuid128_srv_discovery(conn_id, bt_ota_client, (uint8_t *)bt_ota_service_uuid);
	if (ret != GAP_CAUSE_SUCCESS) {
		return false;
	}

	return true;
}

/**
  * @brief  Used internal, start the discovery of OTA characteristics.
  *         NOTE--user can offer this interface for application if required.
  * @retval true--send request to upper stack success.
  *         flase--send request to upper stack failed.
  */
static bool bt_ota_client_start_all_char_discovery(uint8_t conn_id)
{
	uint16_t start_handle;
	uint16_t end_handle;

	APP_PRINT_INFO0("bt_ota_client_start_char_discovery");

	start_handle = bt_ota_client_table[conn_id].hdl_cache[HDL_OTA_SRV_START];
	end_handle = bt_ota_client_table[conn_id].hdl_cache[HDL_OTA_SRV_END];

	if (client_all_char_discovery(conn_id, bt_ota_client, start_handle,
								  end_handle) == GAP_CAUSE_SUCCESS) {
		return true;
	}

	return false;
}

/**
  * @brief  Used internal, start the discovery of OTA characteristics descriptor.
  *         NOTE--user can offer this interface for application if required.
  * @retval true--send request to upper stack success.
  *         flase--send request to upper stack failed.
  */
static bool bt_ota_client_start_all_char_desc_discovery(uint8_t conn_id, uint16_t start_handle,
														uint16_t end_handle)
{
	APP_PRINT_INFO0("bt_ota_client_start_all_char_desc_discovery");

	return (client_all_char_descriptor_discovery(conn_id, bt_ota_client, start_handle,
												 end_handle) == GAP_CAUSE_SUCCESS);
}

static void bt_ota_client_disc_state_cb(uint8_t conn_id,  T_DISCOVERY_STATE discovery_state)
{
	APP_PRINT_INFO1("bt_ota_client_disc_state_cb: discovery_state = %d", discovery_state);

	bool cb_flag = false;
	T_OTA_CLIENT_CB_DATA cb_data;
	cb_data.cb_type = OTA_CLIENT_CB_TYPE_DISC_STATE;

	switch(discovery_state) {
	case DISC_STATE_SRV_DONE: {
		/* Indicate that service handle found. Start discover characteristic. */
		if ((bt_ota_client_table[conn_id].hdl_cache[HDL_OTA_SRV_START] != 0) &&
			(bt_ota_client_table[conn_id].hdl_cache[HDL_OTA_SRV_END] != 0)) {
			if (bt_ota_client_start_all_char_discovery(conn_id) == false) {
				bt_ota_client_table[conn_id].disc_state = DISC_OTA_FAILED;
				cb_flag = true;
			}
		} else { //No ota handle found. Discover procedure complete.
			bt_ota_client_table[conn_id].disc_state = DISC_OTA_FAILED;
			cb_flag = true;
		}
	}
	break;
	case DISC_STATE_CHAR_DONE: {
		if (bt_ota_client_table[conn_id].hdl_cache[HDL_OTA_CTRL] != 0) {
			uint16_t start_handle = bt_ota_client_table[conn_id].hdl_cache[HDL_OTA_CTRL] + 1;
			uint16_t end_handle = bt_ota_client_table[conn_id].hdl_cache[HDL_OTA_SRV_END];

			if (bt_ota_client_start_all_char_desc_discovery(conn_id, start_handle, end_handle) == false) {
				bt_ota_client_table[conn_id].disc_state = DISC_OTA_FAILED;
				cb_flag = true;
			}
		} else {
			bt_ota_client_table[conn_id].disc_state = DISC_OTA_FAILED;
			cb_flag = true;
		}
	}
	break;
	case DISC_STATE_CHAR_DESCRIPTOR_DONE: {
		if (bt_ota_client_table[conn_id].hdl_cache[HDL_OTA_CTRL_CCCD] != 0) {
			bt_ota_client_table[conn_id].disc_state = DISC_OTA_DONE;
			cb_flag = true;
		}
	}
	break;
	default:
		break;
	}

	/* Send disconver state to application if needed. */
	if (cb_flag && p_ota_client_cb) {
		cb_data.cb_content.disc_state = bt_ota_client_table[conn_id].disc_state;
		(*p_ota_client_cb)(bt_ota_client, conn_id, &cb_data);
	}
}

static void bt_ota_client_disc_result_cb(uint8_t conn_id,
										 T_DISCOVERY_RESULT_TYPE result_type,
										 T_DISCOVERY_RESULT_DATA result_data)
{
	uint16_t handle = 0;

	APP_PRINT_INFO1("bt_ota_client_disc_result_cb: result_type = %d", result_type);

	switch (result_type) {
	case DISC_RESULT_SRV_DATA: {
		/* send service handle range to application. */
		bt_ota_client_table[conn_id].hdl_cache[HDL_OTA_SRV_START] = result_data.p_srv_disc_data->att_handle;
		bt_ota_client_table[conn_id].hdl_cache[HDL_OTA_SRV_END] = result_data.p_srv_disc_data->end_group_handle;
	}
	break;
	case DISC_RESULT_CHAR_UUID16: {
		/* When use clientAPI_AllCharDiscovery. */
		handle = result_data.p_char_uuid16_disc_data->value_handle;
		switch(result_data.p_char_uuid16_disc_data->uuid16) {
		case GATT_UUID_CHAR_OTA_IMAGE_HEADER:
			bt_ota_client_table[conn_id].hdl_cache[HDL_OTA_IMG_HDR] = handle;
			break;
		case GATT_UUID_CHAR_OTA_HEADER_CHECKSUM:
			bt_ota_client_table[conn_id].hdl_cache[HDL_OTA_HDR_CHECKSUM] = handle;
			break;
		case GATT_UUID_CHAR_OTA_DATA:
			bt_ota_client_table[conn_id].hdl_cache[HDL_OTA_DATA] = handle;
			break;
		case GATT_UUID_CHAR_OTA_DATA_CHECKSUM:
			bt_ota_client_table[conn_id].hdl_cache[HDL_OTA_DATA_CHECKSUM] = handle;
			break;
		case GATT_UUID_CHAR_OTA_CTRL:
			bt_ota_client_table[conn_id].hdl_cache[HDL_OTA_CTRL] = handle;
			break;
		default:
			break;
		}
	}
	break;
	case DISC_RESULT_CHAR_DESC_UUID16: {
		/* When use clientAPI_AllCharDescriptorDiscovery. */
		if (result_data.p_char_desc_uuid16_disc_data->uuid16 == GATT_UUID_CHAR_CLIENT_CONFIG)
		{
			uint16_t temp_handle = result_data.p_char_desc_uuid16_disc_data->handle;
			if ((temp_handle > bt_ota_client_table[conn_id].hdl_cache[HDL_OTA_CTRL]) &&
				(bt_ota_client_table[conn_id].hdl_cache[HDL_OTA_CTRL_CCCD] == 0)) {
				bt_ota_client_table[conn_id].hdl_cache[HDL_OTA_CTRL_CCCD] = temp_handle;
			}
        }
	}
	break;
	default:
		APP_PRINT_INFO0("dfu_client_disc_result_cb: Invalid Discovery Result Type!");
		break;
	}
}


static void bt_ota_client_write_result_cb(uint8_t conn_id, T_GATT_WRITE_TYPE type, uint16_t handle,
										  uint16_t cause, uint8_t credits)
{
	APP_PRINT_INFO4("bt_ota_client_write_result_cb: type=%d, handle=0x%x cause=%d, credits=%d",
					type, handle, cause, credits);

	T_OTA_CLIENT_CB_DATA cb_data;
	cb_data.cb_type = OTA_CLIENT_CB_TYPE_WRITE_RESULT;
	cb_data.cb_content.write_result.cause = cause;

	if (handle == bt_ota_client_table[conn_id].hdl_cache[HDL_OTA_IMG_HDR]) {
		cb_data.cb_content.write_result.type = OTA_WRITE_IMG_HDR;
	} else if (handle == bt_ota_client_table[conn_id].hdl_cache[HDL_OTA_HDR_CHECKSUM]) {
		cb_data.cb_content.write_result.type = OTA_WRITE_HDR_CHECKSUM;
	} else if (handle == bt_ota_client_table[conn_id].hdl_cache[HDL_OTA_DATA]) {
		cb_data.cb_content.write_result.type = OTA_WRITE_DATA;
	} else if (handle == bt_ota_client_table[conn_id].hdl_cache[HDL_OTA_DATA_CHECKSUM]) {
		cb_data.cb_content.write_result.type = OTA_WRITE_DATA_CHECKSUM;
	} else if (handle == bt_ota_client_table[conn_id].hdl_cache[HDL_OTA_CTRL_CCCD]) {
		cb_data.cb_content.write_result.type = OTA_WRITE_CTRL_CCCD;
	} else {
		return;
	}

	/* Inform application the write result */
	if (p_ota_client_cb) {
		(*p_ota_client_cb)(bt_ota_client, conn_id, &cb_data);
	}
}

static T_APP_RESULT bt_ota_client_notify_indi_result_cb(uint8_t conn_id, bool notify, uint16_t handle,
														uint16_t value_size, uint8_t *p_value)
{
	APP_PRINT_INFO3("bt_ota_client_notify_indi_result_cb: handle=%d, len=%d, is_notify=%d", handle, value_size, notify);

	T_APP_RESULT app_result = APP_RESULT_SUCCESS;
	T_OTA_CLIENT_CB_DATA cb_data;

	if (handle == bt_ota_client_table[conn_id].hdl_cache[HDL_OTA_CTRL]) {
		cb_data.cb_type = OTA_CLIENT_CB_TYPE_NOTIF_IND_RESULT;
		cb_data.cb_content.notif_ind_data.type = OTA_INDI_RECEIVE_OTA_CTRL_INDICATE;
		cb_data.cb_content.notif_ind_data.data.value_size= value_size;
		cb_data.cb_content.notif_ind_data.data.p_value= p_value;

		/* Inform application the notif/ind result. */
		if (p_ota_client_cb) {
			app_result = (*p_ota_client_cb)(bt_ota_client, conn_id, &cb_data);
		}
	}

	return app_result;
}

static void bt_ota_client_disconnect_cb(uint8_t conn_id)
{
	APP_PRINT_INFO0("bt_ota_client_disconnect_cb.");
	if (conn_id >= bt_ota_client_link_num)
	{
		APP_PRINT_ERROR1("bt_ota_client_disconnect_cb: failed invalid conn_id %d", conn_id);
		return;
	}
	memset(&bt_ota_client_table[conn_id], 0, sizeof(T_OTA_LINK));
	return;
}

/**
 * @brief Dfu Client Callbacks.
*/
const T_FUN_CLIENT_CBS bt_ota_client_cbs =
{
	bt_ota_client_disc_state_cb,            //!< Discovery State callback function pointer
 	bt_ota_client_disc_result_cb,           //!< Discovery result callback function pointer
	NULL,                                   //!< Read response callback function pointer
	bt_ota_client_write_result_cb,          //!< Write result callback function pointer
	bt_ota_client_notify_indi_result_cb,    //!< Notify Indicate callback function pointer
	bt_ota_client_disconnect_cb             //!< Link disconnection callback function pointer
};

T_CLIENT_ID bt_ota_add_client(P_FUN_GENERAL_APP_CB app_cb, uint8_t link_num)
{
	uint16_t size;
	if (link_num > BLE_OTA_MAX_LINKS)
	{
		APP_PRINT_ERROR1("bt_ota_add_client: invalid link_num %d", link_num);
		return 0xff;
	}

	if (false == client_register_spec_client_cb(&bt_ota_client, &bt_ota_client_cbs))
	{
		bt_ota_client = CLIENT_PROFILE_GENERAL_ID;
		APP_PRINT_ERROR0("bt_ota_add_client failed");
		return bt_ota_client;
	}

	APP_PRINT_INFO1("bt_ota_add_client: bt_ota_client %d", bt_ota_client);

	/* register callback for profile to inform application that some events happened. */
	p_ota_client_cb = app_cb;
	bt_ota_client_link_num = link_num;
	size = bt_ota_client_link_num * sizeof(T_OTA_LINK);
	bt_ota_client_table = os_mem_zalloc(RAM_TYPE_DATA_ON, size);

	return bt_ota_client;
}

void bt_ota_delete_client(void)
{
	if (bt_ota_client_table != NULL) {
		os_mem_free(bt_ota_client_table);
		bt_ota_client_table = NULL;
	}
}

#endif
