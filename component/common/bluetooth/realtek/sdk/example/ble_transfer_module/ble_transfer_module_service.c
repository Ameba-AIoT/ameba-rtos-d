/**
*********************************************************************************************************
*               Copyright(c) 2017, Realtek Semiconductor Corporation. All rights reserved.
**********************************************************************************************************
* @file     BLE_TRANSFER_MODULE_SERVICE.c
* @brief    transfer BLE profile source file.
* @details  Demonstration of how to implement a self-definition profile.
* @author
* @date
* @version
*********************************************************************************************************
*/

#include <string.h>
#include <trace_app.h>
#include <ble_transfer_module_service.h>
#include <ble_transfer_module_at_cmd.h>
#include <gap.h>

transfer_module_uuid_t transfer_module_uuids = {
	.service_uuid         =  BLE_TRANSFER_MODULE_UUID_SRV,
	.charac_read_uuid     =  BLE_TRANSFER_MODULE_UUID_CHAR_VAL_READ,
	.charac_write_uuid    =  BLE_TRANSFER_MODULE_UUID_CHAR_VAL_WRITE,
	.charac_notify_uuid   =  BLE_TRANSFER_MODULE_UUID_CHAR_VAL_NOTIFY,
	.charac_indicate_uuid =  BLE_TRANSFER_MODULE_UUID_CHAR_VAL_INDICATE,
};

T_SERVER_ID transfer_service_id;
/**<  Value of transfer read characteristic. */
transfer_module_read_val_t transfer_module_read_val = {
	.len = TRANSFER_READ_V1_DEFAULT_LEN,
	.buf = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0xa},
};
char transfer_v1_user_descr[] = "V1 read characteristic";

/**<  Function pointer used to send event to application from transfer profile. Initiated in ble_tranfer_module_service_add_service. */
static P_FUN_SERVER_GENERAL_CB pfn_ble_transfer_module_service_cb = NULL;

/**< @brief  profile/service definition.  */
T_ATTRIB_APPL BLE_TRANSFER_MODULE_SERVICE_tbl[] =
{
    /* <<Primary Service>>, .. */
    {
        (ATTRIB_FLAG_VALUE_INCL | ATTRIB_FLAG_LE),  /* flags     */
        {                                           /* type_value */
            LO_WORD(GATT_UUID_PRIMARY_SERVICE),
            HI_WORD(GATT_UUID_PRIMARY_SERVICE),
            LO_WORD(BLE_TRANSFER_MODULE_UUID_SRV),      /* service UUID */
            HI_WORD(BLE_TRANSFER_MODULE_UUID_SRV)
        },
        UUID_16BIT_SIZE,                            /* bValueLen     */
        NULL,                                       /* p_value_context */
        GATT_PERM_READ                              /* permissions  */
    },
    /* <<Characteristic>> demo for read */
    {
        ATTRIB_FLAG_VALUE_INCL,                     /* flags */
        {                                           /* type_value */
            LO_WORD(GATT_UUID_CHARACTERISTIC),
            HI_WORD(GATT_UUID_CHARACTERISTIC),
            GATT_CHAR_PROP_READ                     /* characteristic properties */
            /* characteristic UUID not needed here, is UUID of next attrib. */
        },
        1,                                          /* bValueLen */
        NULL,
        GATT_PERM_READ                              /* permissions */
    },
    {
        ATTRIB_FLAG_VALUE_APPL,                     /* flags */
        {                                           /* type_value */
            LO_WORD(BLE_TRANSFER_MODULE_UUID_CHAR_VAL_READ),
            HI_WORD(BLE_TRANSFER_MODULE_UUID_CHAR_VAL_READ)
        },
        0,                                          /* bValueLen */
        NULL,
#if defined(TRANSFER_SRV_AUTHEN_EN) && TRANSFER_SRV_AUTHEN_EN
        GATT_PERM_READ_AUTHEN_REQ                   /* permissions */
#else
        GATT_PERM_READ                              /* permissions */
#endif
    },
    /* <<Characteristic>> demo for write */
    {
        ATTRIB_FLAG_VALUE_INCL,                     /* flags */
        {                                           /* type_value */
            LO_WORD(GATT_UUID_CHARACTERISTIC),
            HI_WORD(GATT_UUID_CHARACTERISTIC),
            (GATT_CHAR_PROP_WRITE | GATT_CHAR_PROP_WRITE_NO_RSP) /* characteristic properties */
            /* characteristic UUID not needed here, is UUID of next attrib. */
        },
        1,                                          /* bValueLen */
        NULL,
        GATT_PERM_READ                              /* permissions */
    },
    {
        ATTRIB_FLAG_VALUE_APPL,                     /* flags */
        {                                          /* type_value */
            LO_WORD(BLE_TRANSFER_MODULE_UUID_CHAR_VAL_WRITE),
            HI_WORD(BLE_TRANSFER_MODULE_UUID_CHAR_VAL_WRITE)
        },
        0,                                          /* bValueLen */
        NULL,
#if defined(TRANSFER_SRV_AUTHEN_EN) && TRANSFER_SRV_AUTHEN_EN
        GATT_PERM_WRITE_AUTHEN_REQ                  /* permissions */
#else
        GATT_PERM_WRITE                             /* permissions */
#endif
    },
    /* <<Characteristic>>, demo for notify */
    {
        ATTRIB_FLAG_VALUE_INCL,                     /* flags */
        {                                          /* type_value */
            LO_WORD(GATT_UUID_CHARACTERISTIC),
            HI_WORD(GATT_UUID_CHARACTERISTIC),
            (GATT_CHAR_PROP_NOTIFY)                 /* characteristic properties */
            /* characteristic UUID not needed here, is UUID of next attrib. */
        },
        1,                                          /* bValueLen */
        NULL,
        GATT_PERM_READ                              /* permissions */
    },
    {
        ATTRIB_FLAG_VALUE_APPL,                     /* flags */
        {                                         /* type_value */
            LO_WORD(BLE_TRANSFER_MODULE_UUID_CHAR_VAL_NOTIFY),
            HI_WORD(BLE_TRANSFER_MODULE_UUID_CHAR_VAL_NOTIFY)
        },
        0,                                          /* bValueLen */
        NULL,
        GATT_PERM_NONE                              /* permissions */
    },
    /* client characteristic configuration */
    {
        ATTRIB_FLAG_VALUE_INCL | ATTRIB_FLAG_CCCD_APPL,                 /* flags */
        {                                          /* type_value */
            LO_WORD(GATT_UUID_CHAR_CLIENT_CONFIG),
            HI_WORD(GATT_UUID_CHAR_CLIENT_CONFIG),
            /* NOTE: this value has an instantiation for each client, a write to */
            /* this attribute does not modify this default value:                */
            LO_WORD(GATT_CLIENT_CHAR_CONFIG_DEFAULT), /* client char. config. bit field */
            HI_WORD(GATT_CLIENT_CHAR_CONFIG_DEFAULT)
        },
        2,                                          /* bValueLen */
        NULL,
#if defined(TRANSFER_SRV_AUTHEN_EN) && TRANSFER_SRV_AUTHEN_EN
        (GATT_PERM_READ_AUTHEN_REQ | GATT_PERM_WRITE_AUTHEN_REQ) /* permissions */
#else
        (GATT_PERM_READ | GATT_PERM_WRITE)          /* permissions */
#endif
    },
    /* <<Characteristic>> demo for indicate */
    {
        ATTRIB_FLAG_VALUE_INCL,                     /* flags */
        {                                           /* type_value */
            LO_WORD(GATT_UUID_CHARACTERISTIC),
            HI_WORD(GATT_UUID_CHARACTERISTIC),
            (GATT_CHAR_PROP_INDICATE)               /* characteristic properties */
            /* characteristic UUID not needed here, is UUID of next attrib. */
        },
        1,                                          /* bValueLen */
        NULL,
        GATT_PERM_READ                              /* permissions */
    },
    {
        ATTRIB_FLAG_VALUE_APPL,                     /* flags */
        {                                           /* type_value */
            LO_WORD(BLE_TRANSFER_MODULE_UUID_CHAR_VAL_INDICATE),
            HI_WORD(BLE_TRANSFER_MODULE_UUID_CHAR_VAL_INDICATE)
        },
        0,                                          /* bValueLen */
        NULL,
        GATT_PERM_NONE                              /* permissions */
    },
    /* client characteristic configuration */
    {
        ATTRIB_FLAG_VALUE_INCL | ATTRIB_FLAG_CCCD_APPL,                 /* flags */
        {                                         /* type_value */
            LO_WORD(GATT_UUID_CHAR_CLIENT_CONFIG),
            HI_WORD(GATT_UUID_CHAR_CLIENT_CONFIG),
            /* NOTE: this value has an instantiation for each client, a write to */
            /* this attribute does not modify this default value:                */
            LO_WORD(GATT_CLIENT_CHAR_CONFIG_DEFAULT), /* client char. config. bit field */
            HI_WORD(GATT_CLIENT_CHAR_CONFIG_DEFAULT)
        },
        2,                                          /* bValueLen */
        NULL,
#if defined(TRANSFER_SRV_AUTHEN_EN) && TRANSFER_SRV_AUTHEN_EN
        (GATT_PERM_READ_AUTHEN_REQ | GATT_PERM_WRITE_AUTHEN_REQ) /* permissions */
#else
        (GATT_PERM_READ | GATT_PERM_WRITE)          /* permissions */
#endif
    },
};

bool ble_transfer_module_get_uuid(uint8_t attr, uint16_t *uuid)
{
	uint16_t ret = true;
	if (!uuid) {
		return false;
	}

	switch (attr) {
	case BLE_TRANSFER_MODULE_ATTR_SERVICE:
		*uuid = transfer_module_uuids.service_uuid;
		break;
	case BLE_TRANSFER_MODULE_ATTR_READ:
		*uuid = transfer_module_uuids.charac_read_uuid;
		break;
	case BLE_TRANSFER_MODULE_ATTR_WRITE:
		*uuid = transfer_module_uuids.charac_write_uuid;
		break;
	case BLE_TRANSFER_MODULE_ATTR_NOTIFY:
		*uuid = transfer_module_uuids.charac_notify_uuid;
		break;
	case BLE_TRANSFER_MODULE_ATTR_INDICATE:
		*uuid = transfer_module_uuids.charac_indicate_uuid;
		break;
	default:
		printf("[APP] Invalid transfer module attribute attr=%d\r\n", attr);
		ret = false;
		break;
	}
	return ret;
}

bool ble_transfer_module_set_uuid(uint8_t attr, uint16_t uuid)
{
	uint16_t ret = true;

	switch (attr) {
	case BLE_TRANSFER_MODULE_ATTR_SERVICE:
		transfer_module_uuids.service_uuid = uuid;
		break;
	case BLE_TRANSFER_MODULE_ATTR_READ:
		transfer_module_uuids.charac_read_uuid = uuid;
		break;
	case BLE_TRANSFER_MODULE_ATTR_WRITE:
		transfer_module_uuids.charac_write_uuid = uuid;
		break;
	case BLE_TRANSFER_MODULE_ATTR_NOTIFY:
		transfer_module_uuids.charac_notify_uuid = uuid;
		break;
	case BLE_TRANSFER_MODULE_ATTR_INDICATE:
		transfer_module_uuids.charac_indicate_uuid = uuid;
		break;
	default:
		printf("[APP] Invalid transfer module attribute attr=%d\r\n", attr);
		ret = false;
		break;
	}
	return ret;
}

bool ble_transfer_module_update_read_val(uint8_t len, uint8_t *data)
{
	if (!len || !data) {
		return false;
	}

	if (len > TRANSFER_READ_V1_MAX_LEN) {
		return false;
	}

	transfer_module_read_val.len = len;
	memset(transfer_module_read_val.buf, 0, sizeof(transfer_module_read_val.buf));
	memcpy(transfer_module_read_val.buf, data, len);

	return true;
}

void ble_transfer_modlule_set_service_tbl(void)
{
    BLE_TRANSFER_MODULE_SERVICE_tbl[BLE_TRANSFER_MODULE_SERVICE_CHAR_V1_SERVICE_INDEX].type_value[2] = LO_WORD(transfer_module_uuids.service_uuid);
    BLE_TRANSFER_MODULE_SERVICE_tbl[BLE_TRANSFER_MODULE_SERVICE_CHAR_V1_SERVICE_INDEX].type_value[3] = HI_WORD(transfer_module_uuids.service_uuid);

    BLE_TRANSFER_MODULE_SERVICE_tbl[BLE_TRANSFER_MODULE_SERVICE_CHAR_V1_READ_INDEX].type_value[0] = LO_WORD(transfer_module_uuids.charac_read_uuid);
    BLE_TRANSFER_MODULE_SERVICE_tbl[BLE_TRANSFER_MODULE_SERVICE_CHAR_V1_READ_INDEX].type_value[1] = HI_WORD(transfer_module_uuids.charac_read_uuid);

    BLE_TRANSFER_MODULE_SERVICE_tbl[BLE_TRANSFER_MODULE_SERVICE_CHAR_V2_WRITE_INDEX].type_value[0] = LO_WORD(transfer_module_uuids.charac_write_uuid);
    BLE_TRANSFER_MODULE_SERVICE_tbl[BLE_TRANSFER_MODULE_SERVICE_CHAR_V2_WRITE_INDEX].type_value[1] = HI_WORD(transfer_module_uuids.charac_write_uuid);

    BLE_TRANSFER_MODULE_SERVICE_tbl[BLE_TRANSFER_MODULE_SERVICE_CHAR_V3_NOTIFY_INDEX].type_value[0] = LO_WORD(transfer_module_uuids.charac_notify_uuid);
    BLE_TRANSFER_MODULE_SERVICE_tbl[BLE_TRANSFER_MODULE_SERVICE_CHAR_V3_NOTIFY_INDEX].type_value[1] = HI_WORD(transfer_module_uuids.charac_notify_uuid);

    BLE_TRANSFER_MODULE_SERVICE_tbl[BLE_TRANSFER_MODULE_SERVICE_CHAR_V4_INDICATE_INDEX].type_value[0] = LO_WORD(transfer_module_uuids.charac_indicate_uuid);
    BLE_TRANSFER_MODULE_SERVICE_tbl[BLE_TRANSFER_MODULE_SERVICE_CHAR_V4_INDICATE_INDEX].type_value[1] = HI_WORD(transfer_module_uuids.charac_indicate_uuid);
}

/**
 * @brief read characteristic data from service.
 *
 * @param service_id          ServiceID of characteristic data.
 * @param attrib_index        Attribute index of getting characteristic data.
 * @param offset              Used for Blob Read.
 * @param p_length            length of getting characteristic data.
 * @param pp_value            data got from service.
 * @return Profile procedure result
*/
T_APP_RESULT  ble_tranfer_module_service_attr_read_cb(uint8_t conn_id, T_SERVER_ID service_id,
                                            uint16_t attrib_index, uint16_t offset, uint16_t *p_length, uint8_t **pp_value)
{
    (void)offset;
    T_APP_RESULT  cause  = APP_RESULT_SUCCESS;
    uint8_t err_code = 0;
    uint16_t conn_handle = le_get_conn_handle(conn_id);

    switch (attrib_index)
    {
    default:
        APP_PRINT_ERROR1("ble_tranfer_module_service_attr_read_cb, Attr not found, index %d", attrib_index);
        cause = APP_RESULT_ATTR_NOT_FOUND;
        err_code = ATT_ERR_ATTR_NOT_FOUND;
        break;
    case BLE_TRANSFER_MODULE_SERVICE_CHAR_V1_READ_INDEX:
        {
            TRANSFER_CALLBACK_DATA callback_data;
            callback_data.msg_type = SERVICE_CALLBACK_TYPE_READ_CHAR_VALUE;
            callback_data.msg_data.read.read_value_index = TRANSFER_READ_V1;
            callback_data.msg_data.read.read_offset = offset;
            callback_data.conn_id = conn_id;
            if (pfn_ble_transfer_module_service_cb)
            {   
                pfn_ble_transfer_module_service_cb(service_id, (void *)&callback_data);
            }
            *pp_value = transfer_module_read_val.buf;
            *p_length = transfer_module_read_val.len;
        }
        break;
    }
    at_printf("+BLEGATTS:read_rsp,0,%d,%d,%d,%d\r\n",
				TRANSFER_MODULE_APP_ID, conn_handle, attrib_index, err_code);

    return (cause);
}

void transfer_write_post_callback(uint8_t conn_id, T_SERVER_ID service_id, uint16_t attrib_index,
                                uint16_t length, uint8_t *p_value)
{
    (void)p_value;
    APP_PRINT_INFO4("transfer_write_post_callback: conn_id %d, service_id %d, attrib_index 0x%x, length %d",
                    conn_id, service_id, attrib_index, length);
}

/**
 * @brief write characteristic data from service.
 *
 * @param conn_id
 * @param service_id        ServiceID to be written.
 * @param attrib_index      Attribute index of characteristic.
 * @param length            length of value to be written.
 * @param p_value           value to be written.
 * @return Profile procedure result
*/
T_APP_RESULT ble_tranfer_module_service_attr_write_cb(uint8_t conn_id, T_SERVER_ID service_id,
                                            uint16_t attrib_index, T_WRITE_TYPE write_type, uint16_t length, uint8_t *p_value,
                                            P_FUN_WRITE_IND_POST_PROC *p_write_ind_post_proc)
{
    TRANSFER_CALLBACK_DATA callback_data;
    T_APP_RESULT  cause = APP_RESULT_SUCCESS;
    APP_PRINT_INFO1("ble_tranfer_module_service_attr_write_cb write_type = 0x%x", write_type);
    *p_write_ind_post_proc = transfer_write_post_callback;
    uint8_t err_code = 0;
    uint16_t conn_handle = le_get_conn_handle(conn_id);

    if (BLE_TRANSFER_MODULE_SERVICE_CHAR_V2_WRITE_INDEX == attrib_index)
    {
        /* Make sure written value size is valid. */
        if (p_value == NULL)
        {
            cause  = APP_RESULT_INVALID_VALUE_SIZE;
        }
        else
        {
            /* Notify Application. */
            callback_data.msg_type = SERVICE_CALLBACK_TYPE_WRITE_CHAR_VALUE;
            callback_data.conn_id  = conn_id;
            callback_data.msg_data.write.opcode = TRANSFER_WRITE_V2;
            callback_data.msg_data.write.write_type = write_type;
            callback_data.msg_data.write.len = length;
            callback_data.msg_data.write.p_value = p_value;

            if (pfn_ble_transfer_module_service_cb)
            {
                pfn_ble_transfer_module_service_cb(service_id, (void *)&callback_data);
            }

            at_printf("+BLEGATTS:write,%d,%d,%d,%d,%d",
					TRANSFER_MODULE_APP_ID, conn_handle, attrib_index,
					write_type, length);
            BT_AT_DUMP("", p_value, length);
        }
    }
    else
    {
        APP_PRINT_ERROR2("ble_tranfer_module_service_attr_write_cb Error: attrib_index 0x%x, length %d",
                         attrib_index,
                         length);
        cause = APP_RESULT_ATTR_NOT_FOUND;
        err_code = ATT_ERR_ATTR_NOT_FOUND;
    }
    if(write_type == WRITE_REQUEST || write_type == WRITE_LONG) {
        at_printf("+BLEGATTS:write_rsp,0,%d,%d,%d,%d,%d\r\n",
					TRANSFER_MODULE_APP_ID,
					conn_handle, attrib_index,
					write_type, err_code);
    }
    return cause;
}

/**
  * @brief send notification of transfer notify characteristic value.
  *
  * @param[in] conn_id           connection id
  * @param[in] service_id        service ID of service.
  * @param[in] p_value           characteristic value to notify
  * @param[in] length            characteristic value length to notify
  * @return notification action result
  * @retval 1 true
  * @retval 0 false
  */
bool ble_tranfer_module_service_send_v3_notify(uint8_t conn_id, T_SERVER_ID service_id, void *p_value,
                                     uint16_t length)
{
    APP_PRINT_INFO0("ble_tranfer_module_service_send_v3_notify");
    // send notification to client
    return server_send_data(conn_id, service_id, BLE_TRANSFER_MODULE_SERVICE_CHAR_V3_NOTIFY_INDEX, p_value,
                            length,
                            GATT_PDU_TYPE_ANY);
}

/**
  * @brief send indication of transfer indicate characteristic value.
  *
  * @param[in] conn_id           connection id
  * @param[in] service_id        service ID of service.
  * @param[in] p_value           characteristic value to notify
  * @param[in] length            characteristic value length to notify
  * @return notification action result
  * @retval 1 true
  * @retval 0 false
  */
bool ble_tranfer_module_service_send_v4_indicate(uint8_t conn_id, T_SERVER_ID service_id, void *p_value,
                                       uint16_t length)
{
    APP_PRINT_INFO0("ble_tranfer_module_service_send_v4_indicate");
    // send indication to client
    return server_send_data(conn_id, service_id, BLE_TRANSFER_MODULE_SERVICE_CHAR_V4_INDICATE_INDEX, p_value,
                            length, GATT_PDU_TYPE_ANY);
}

/**
 * @brief update CCCD bits from stack.
 *
 * @param conn_id           connection id.
 * @param service_id          Service ID.
 * @param index          Attribute index of characteristic data.
 * @param cccbits         CCCD bits from stack.
 * @return None
*/
void ble_tranfer_module_service_cccd_update_cb(uint8_t conn_id, T_SERVER_ID service_id, uint16_t index,
                                     uint16_t cccbits)
{
    TRANSFER_CALLBACK_DATA callback_data;
    bool is_handled = false;
    callback_data.conn_id = conn_id;
    callback_data.msg_type = SERVICE_CALLBACK_TYPE_INDIFICATION_NOTIFICATION;
    APP_PRINT_INFO2("ble_tranfer_module_service_cccd_update_cb: index = %d, cccbits 0x%x", index, cccbits);
    uint8_t noti_indi_en = 0;
    uint16_t conn_handle = le_get_conn_handle(conn_id);

    switch (index)
    {
    case BLE_TRANSFER_MODULE_SERVICE_CHAR_NOTIFY_CCCD_INDEX:
        {
            if (cccbits & GATT_CLIENT_CHAR_CONFIG_NOTIFY)
            {
                // Enable Notification
                noti_indi_en = 1;
                callback_data.msg_data.notification_indification_index = TRANSFER_NOTIFY_INDICATE_V3_ENABLE;
            }
            else
            {
                // Disable Notification
                noti_indi_en = 0;
                callback_data.msg_data.notification_indification_index = TRANSFER_NOTIFY_INDICATE_V3_DISABLE;
            }
            is_handled = true;
            at_printf("+BLEGATTS:cccd,notify,%d,%d,%d,%d\r\n",
					noti_indi_en, TRANSFER_MODULE_APP_ID, conn_handle, index);
        }
        break;
    case BLE_TRANSFER_MODULE_SERVICE_CHAR_INDICATE_CCCD_INDEX:
        {
            if (cccbits & GATT_CLIENT_CHAR_CONFIG_INDICATE)
            {
                // Enable Indication
                noti_indi_en = 1;
                callback_data.msg_data.notification_indification_index = TRANSFER_NOTIFY_INDICATE_V4_ENABLE;
            }
            else
            {
                // Disable Indication
                noti_indi_en = 0;
                callback_data.msg_data.notification_indification_index = TRANSFER_NOTIFY_INDICATE_V4_DISABLE;
            }
            is_handled =  true;
            at_printf("+BLEGATTS:cccd,indicate,%d,%d,%d,%d\r\n",
					noti_indi_en, TRANSFER_MODULE_APP_ID, conn_handle, index);
        }
        break;

    default:
        break;
    }
    /* Notify Application. */
    if (pfn_ble_transfer_module_service_cb && (is_handled == true))
    {
        pfn_ble_transfer_module_service_cb(service_id, (void *)&callback_data);
    }
}

/**
 * @brief transfer ble Service Callbacks.
*/
const T_FUN_GATT_SERVICE_CBS ble_tranfer_module_service_cbs =
{
    ble_tranfer_module_service_attr_read_cb,  // Read callback function pointer
    ble_tranfer_module_service_attr_write_cb, // Write callback function pointer
    ble_tranfer_module_service_cccd_update_cb // CCCD update callback function pointer
};

/**
  * @brief Add transfer BLE service to the BLE stack database.
  *
  * @param[in]   p_func  Callback when service attribute was read, write or cccd update.
  * @return Service id generated by the BLE stack: @ref T_SERVER_ID.
  * @retval 0xFF Operation failure.
  * @retval others Service id assigned by stack.
  *
  */
T_SERVER_ID ble_transfer_module_service_add_service(void *p_func)
{
    ble_transfer_modlule_set_service_tbl();
    if (false == server_add_service(&transfer_service_id,
                                    (uint8_t *)BLE_TRANSFER_MODULE_SERVICE_tbl,
                                    sizeof(BLE_TRANSFER_MODULE_SERVICE_tbl),
                                    ble_tranfer_module_service_cbs))
    {
        APP_PRINT_ERROR0("ble_tranfer_module_service_add_service: fail");
        transfer_service_id = 0xff;
        return transfer_service_id;
    }
    pfn_ble_transfer_module_service_cb = (P_FUN_SERVER_GENERAL_CB)p_func;
    return transfer_service_id;
}

