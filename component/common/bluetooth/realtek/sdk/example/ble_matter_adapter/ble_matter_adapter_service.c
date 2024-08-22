#include <platform_opts_bt.h>
#if defined(CONFIG_BLE_MATTER_ADAPTER) && CONFIG_BLE_MATTER_ADAPTER
#include <gap.h>
#include <os_mem.h>
#include <profile_server.h>
#include <string.h>
#include "platform_stdlib.h"
#include "os_sync.h"
#include <trace_app.h>
#include "ble_matter_adapter_service.h"

/*============================================================================*
 *                              Constants
 *============================================================================*/
#define MATTER_UUID_RX		0x11, 0x9D, 0x9F, 0x42, 0x9C, 0x4F, 0x9F, 0x95, 0x59, 0x45, 0x3D, 0x26, 0xF5, 0x2E, 0xEE, 0x18
#define MATTER_UUID_TX		0x12, 0x9D, 0x9F, 0x42, 0x9C, 0x4F, 0x9F, 0x95, 0x59, 0x45, 0x3D, 0x26, 0xF5, 0x2E, 0xEE, 0x18
#define MATTER_UUID_C3		0x04, 0x8F, 0x21, 0x83, 0x8A, 0x74, 0x7D, 0xB8, 0xF2, 0x45, 0x72, 0x87, 0x38, 0x02, 0x63, 0x64

#define BT_MATTER_ADAPTER_SERVICE_C3_INDEX 0x07

extern T_SERVER_ID ble_matter_adapter_service_id;

/**<  Function pointer used to send event to application from ble config wifi profile. Initiated in bt_matter_adapter_service_add_service. */
static P_FUN_SERVER_GENERAL_CB ble_matter_adapter_service_cb = NULL;

/**< @brief  profile/service definition.  */
/**should changed according BT team **/
T_ATTRIB_APPL ble_matter_adapter_service_tbl[] =
{
	/* <<Primary Service>>, .. */
	{
		(ATTRIB_FLAG_VALUE_INCL | ATTRIB_FLAG_LE), /* flags */
		{ /* type_value */
			LO_WORD(GATT_UUID_PRIMARY_SERVICE),
			HI_WORD(GATT_UUID_PRIMARY_SERVICE),
			LO_WORD(0xFFF6), /* service UUID */
			HI_WORD(0xFFF6)
		},
		UUID_16BIT_SIZE, /* bValueLen */
		NULL, /* p_value_context */
		GATT_PERM_READ /* permissions */
	},
	/* <<Characteristic>> Data RX */
	{
		ATTRIB_FLAG_VALUE_INCL, /* flags */
		{ /* type_value */
			LO_WORD(GATT_UUID_CHARACTERISTIC),
			HI_WORD(GATT_UUID_CHARACTERISTIC),
			(GATT_CHAR_PROP_WRITE | GATT_CHAR_PROP_WRITE_NO_RSP) /* characteristic properties */
			/* characteristic UUID not needed here, is UUID of next attrib. */
		},
		1, /* bValueLen */
		NULL,
		GATT_PERM_READ /* permissions */
	},
	{
		ATTRIB_FLAG_VALUE_APPL | ATTRIB_FLAG_UUID_128BIT, /* flags */
		{ /* type_value */
			MATTER_UUID_RX
		},
		0, /* bValueLen */
		NULL,
		GATT_PERM_WRITE /* permissions */
	},
		/* <<Characteristic>> Data TX */
	{
		ATTRIB_FLAG_VALUE_INCL, /* flags */
		{ /* type_value */
			LO_WORD(GATT_UUID_CHARACTERISTIC),
			HI_WORD(GATT_UUID_CHARACTERISTIC),
		(GATT_CHAR_PROP_READ | GATT_CHAR_PROP_INDICATE) /* characteristic properties */
		/* characteristic UUID not needed here, is UUID of next attrib. */
		},
		1, /* bValueLen */
		NULL,
		GATT_PERM_READ /* permissions */
	},
	{
		ATTRIB_FLAG_VALUE_APPL | ATTRIB_FLAG_UUID_128BIT, /* flags */
		{ /* type_value */
			MATTER_UUID_TX
		},
		0, /* bValueLen */
		NULL,
		GATT_PERM_READ //GATT_PERM_NONE // permissions 
	},
	/* client characteristic configuration */
	{
		ATTRIB_FLAG_VALUE_INCL | ATTRIB_FLAG_CCCD_APPL, /* flags */
		{ /* type_value */
			LO_WORD(GATT_UUID_CHAR_CLIENT_CONFIG),
			HI_WORD(GATT_UUID_CHAR_CLIENT_CONFIG),
			/* NOTE: this value has an instantiation for each client, a write to */
			/* this attribute does not modify this default value: */
			LO_WORD(GATT_CLIENT_CHAR_CONFIG_DEFAULT), /* client char. config. bit field */
			HI_WORD(GATT_CLIENT_CHAR_CONFIG_DEFAULT)
		},
		2, /* bValueLen */
		NULL,
		(GATT_PERM_READ | GATT_PERM_WRITE) /* permissions */
	},

	/* <<Characteristic>> C3 Data TX */
	{
		ATTRIB_FLAG_VALUE_INCL, /* flags */
		{ /* type_value */
			LO_WORD(GATT_UUID_CHARACTERISTIC),
			HI_WORD(GATT_UUID_CHARACTERISTIC),
			(GATT_CHAR_PROP_READ) /* characteristic properties */
			/* characteristic UUID not needed here, is UUID of next attrib. */
		},
		1, /* bValueLen */
		NULL,
		GATT_PERM_READ /* permissions */
	},
	{
		ATTRIB_FLAG_VALUE_APPL | ATTRIB_FLAG_UUID_128BIT, /* flags */
		{ /* type_value */
			MATTER_UUID_C3
		},
		0, /* bValueLen */
		NULL,
		GATT_PERM_READ //GATT_PERM_NONE // permissions
	},
};
/*============================================================================*
 *                              Functions
 *============================================================================*/
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
T_APP_RESULT  ble_matter_adapter_service_attr_read_cb(uint8_t conn_id, T_SERVER_ID service_id,
                                            uint16_t attrib_index, uint16_t offset, uint16_t *p_length, uint8_t **pp_value)
{
    T_APP_RESULT  cause  = APP_RESULT_SUCCESS;
    switch (attrib_index)
    {
    case BT_MATTER_ADAPTER_SERVICE_C3_INDEX:
        {
            T_MATTER_CALLBACK_DATA callback_data;
            callback_data.msg_type = SERVICE_CALLBACK_TYPE_READ_CHAR_VALUE;
            callback_data.conn_id = conn_id;
            if (ble_matter_adapter_service_cb)
            {
                ble_matter_adapter_service_cb(service_id, (void *)&callback_data);
            }
            *pp_value = callback_data.msg_data.write_read.p_value;
            *p_length = callback_data.msg_data.write_read.len;
        }
        break;
    default:
        printf("bt_matter_adapter_service_attr_read_cb, Attr not found, index %d", attrib_index);
        cause = APP_RESULT_ATTR_NOT_FOUND;
        break;
    }

    return (cause);
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
T_APP_RESULT ble_matter_adapter_service_attr_write_cb(uint8_t conn_id, T_SERVER_ID service_id,
                                            uint16_t attrib_index, T_WRITE_TYPE write_type, uint16_t length, uint8_t *p_value,
                                            P_FUN_WRITE_IND_POST_PROC *p_write_ind_post_proc)
{
    T_MATTER_CALLBACK_DATA callback_data;
    T_APP_RESULT  cause = APP_RESULT_SUCCESS;
    APP_PRINT_INFO1("bt_matter_adapter_service_attr_write_cb write_type = 0x%x", write_type);
    *p_write_ind_post_proc = NULL;

    if (BT_MATTER_ADAPTER_SERVICE_CHAR_RX_INDEX == attrib_index) {
        /* Make sure written value size is valid. */
        if (p_value == NULL) {
            cause  = APP_RESULT_INVALID_VALUE_SIZE;
        } else {
            /* Notify Application. */
            callback_data.msg_type = SERVICE_CALLBACK_TYPE_WRITE_CHAR_VALUE;
            callback_data.conn_id  = conn_id;
            callback_data.msg_data.write_read.len = length;
            callback_data.msg_data.write_read.p_value = p_value;

            //handle_bt_matter_adapter_app_data(p_value, length);
            if (ble_matter_adapter_service_cb) {
                ble_matter_adapter_service_cb(service_id, (void *)&callback_data);
            }
        }
    } else {
            APP_PRINT_ERROR2("bt_matter_adapter_service_attr_write_cb Error: attrib_index 0x%x, length %d",
                attrib_index, length);
            cause = APP_RESULT_ATTR_NOT_FOUND;
    }

    return cause;
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
void ble_matter_adapter_service_cccd_update_cb(uint8_t conn_id, T_SERVER_ID service_id, uint16_t index,
                                     uint16_t cccbits)
{
    T_MATTER_CALLBACK_DATA callback_data;
    bool is_handled = false;
    callback_data.conn_id = conn_id;
    callback_data.msg_type = SERVICE_CALLBACK_TYPE_INDIFICATION_NOTIFICATION;
    switch (index)
    {
    case BT_MATTER_ADAPTER_SERVICE_CHAR_INDICATE_CCCD_INDEX:
        {
            if (cccbits & GATT_CLIENT_CHAR_CONFIG_INDICATE)
            {
                // Enable Notification
                callback_data.msg_data.notification_indification_index = MATTER_NOTIFY_INDICATE_V3_ENABLE;
            }
            else
            {
                // Disable Notification
                callback_data.msg_data.notification_indification_index = MATTER_NOTIFY_INDICATE_V3_DISABLE;
            }
            is_handled =  true;
        }
        break;
    default:
        break;
    }
    /* Notify Application. */
    if (ble_matter_adapter_service_cb && (is_handled == true))
    {
        ble_matter_adapter_service_cb(service_id, (void *)&callback_data);
    }
}

/**
 * @brief Simple ble Service Callbacks.
*/
const T_FUN_GATT_SERVICE_CBS ble_matter_adapter_service_cbs =
{
    ble_matter_adapter_service_attr_read_cb,  // Read callback function pointer
    ble_matter_adapter_service_attr_write_cb, // Write callback function pointer
    ble_matter_adapter_service_cccd_update_cb // CCCD update callback function pointer
};



/**
  * @brief Add simple BLE service to the BLE stack database.
  *
  * @param[in]   p_func  Callback when service attribute was read, write or cccd update.
  * @return Service id generated by the BLE stack: @ref T_SERVER_ID.
  * @retval 0xFF Operation failure.
  * @retval others Service id assigned by stack.
  *
  */
T_SERVER_ID ble_matter_adapter_service_add_service(void *p_func)
{
    if (false == server_add_service(&ble_matter_adapter_service_id,
                                    (uint8_t *)ble_matter_adapter_service_tbl,
                                    sizeof(ble_matter_adapter_service_tbl),
                                    ble_matter_adapter_service_cbs))
    {
        APP_PRINT_ERROR0("ble_matter_adapter_service_add_service: fail");
        ble_matter_adapter_service_id = 0xff;
        return ble_matter_adapter_service_id;
    }

    ble_matter_adapter_service_cb = (P_FUN_SERVER_GENERAL_CB)p_func;
    return ble_matter_adapter_service_id;
}
#endif
