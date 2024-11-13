#include <platform_opts_bt.h>
#if (defined(CONFIG_BLE_TRANSFER_MODULE) && CONFIG_BLE_TRANSFER_MODULE)
#include "platform_opts.h"
#include <platform/platform_stdlib.h>
#if SUPPORT_LOG_SERVICE
#include <string.h>
#include <trace_app.h>
#include <gap_bond_le.h>
#include <gap_scan.h>
#include <gap.h>
#include <gap_adv.h>
#include <gap_conn_le.h>
#include <gcs_client.h>
#include <profile_client.h>
#include "log_service.h"
#include "atcmd_bt.h"
#include "os_msg.h"
#include "os_sched.h"
#include "os_mem.h"
#include "ble_transfer_module_app_flags.h"
#include "ble_transfer_module_service.h"
#include "ble_transfer_module_at_cmd.h"
#include "ble_transfer_module_app.h"
#include "ble_transfer_module_link_mgr.h"

extern uint8_t ble_transfer_app_max_links;
extern void *ble_transfer_evt_queue_handle;
extern void *ble_transfer_io_queue_handle;
extern T_GAP_DEV_STATE ble_transfer_gap_dev_state;
extern transfer_module_uuid_t transfer_module_uuids;
extern int ble_transfer_central_app_max_links;
extern uint8_t is_pairing_initiator[BLE_TRANSFER_APP_MAX_LINKS];
extern T_APP_LINK ble_transfer_app_link_table[BLE_TRANSFER_APP_MAX_LINKS];

#define RTK_BT_DEFAULT_DEV_NAME         "RTK_BT_TRANSFER_MODULE"
char dev_name[BLE_TRANSFER_MODULE_DEV_NAME_MAX_LEN] = RTK_BT_DEFAULT_DEV_NAME;
ble_transfer_module_disc_read_type_t disc_read_type[BLE_TRANSFER_CENTRAL_APP_MAX_LINKS] = {0};

static const uint8_t def_adv_data[] =
{
    /* Flags */
    0x02,             /* length */
    GAP_ADTYPE_FLAGS, /* type="Flags" */
    GAP_ADTYPE_FLAGS_LIMITED | GAP_ADTYPE_FLAGS_BREDR_NOT_SUPPORTED,
    /* Local name */
    0x12,             /* length */
    GAP_ADTYPE_LOCAL_NAME_COMPLETE,
    'R', 'T', 'K', '_', 'B', 'T', '_', 'P', 'E', 'R', 'I', 'P', 'H', 'E', 'R', 'A', 'L',
};

static uint8_t def_scan_rsp_data[] = {
	/* ad_len */
	0x3,
	/* ad_type */
	GAP_ADTYPE_APPEARANCE,
	/* ad_data */
	LO_WORD(GAP_GATT_APPEARANCE_GENERIC_PHONE),
	HI_WORD(GAP_GATT_APPEARANCE_GENERIC_PHONE),
};

static ble_transfer_module_adv_param_t def_adv_param = {
	.interval_min = 0x30,
	.interval_max = 0x60,
	.type = GAP_ADTYPE_ADV_IND,
	.own_addr_type = GAP_LOCAL_ADDR_LE_PUBLIC,
	.peer_addr = {
		.type = (T_GAP_LOCAL_ADDR_TYPE)0,
		.addr_val = {0},
	},
	.channel_map = GAP_ADVCHAN_ALL,
	.filter_policy = GAP_ADV_FILTER_ANY,
};

static ble_transfer_module_scan_param_t def_scan_param = {
	.type          = GAP_SCAN_MODE_ACTIVE,
	.interval      = 0x60,
	.window        = 0x30,
	.filter_policy = GAP_SCAN_FILTER_ANY,
	.duplicate_opt = 1,
};

static u8 ctoi(char c)
{
	if((c >= 'A') && (c <= 'F')) {
		return (c - 'A' + 0x0A);
	}

	if((c >= 'a') && (c <= 'f')) {
		return (c - 'a' + 0x0A);
	}

	if((c >= '0') && (c <= '9')) {
		return (c - '0' + 0x00);
	}

	return 0xFF;
}

void ble_transfer_module_addr_to_str(void *paddr, char *str, uint32_t len)
{
	ble_transfer_module_le_addr_t *addr = (ble_transfer_module_le_addr_t *)paddr;
	char type[10];
	memset(str, 0, len);

	switch (addr->type) {
	case GAP_LOCAL_ADDR_LE_PUBLIC:
		strncpy(type, "public", 7);
		break;
	case GAP_LOCAL_ADDR_LE_RANDOM:
		strncpy(type, "random", 7);
		break;
#if F_BT_LE_PRIVACY_SUPPORT
	case GAP_LOCAL_ADDR_LE_RAP_OR_PUBLIC:
		strncpy(type, "public-id", 10);
		break;
	case GAP_LOCAL_ADDR_LE_RAP_OR_RAND:
		strncpy(type, "random-id", 10);
		break;
#endif
	default:
		snprintf(type, sizeof(type), "0x%02x", addr->type);
		break;
	}
	snprintf(str, len, "%02X:%02X:%02X:%02X:%02X:%02X(%s)",
			 addr->addr_val[5], addr->addr_val[4], addr->addr_val[3],
			 addr->addr_val[2], addr->addr_val[1], addr->addr_val[0], type);
}

static u8 hex_str_to_bd_addr(u32 str_len, s8 *str, u8 *num_arr)
{
	num_arr += str_len/2 -1;
	u32 n = 0;
	u8 num = 0;

	if (str_len < 2) {
		return FALSE;
	}
	while (n < str_len) {
		if ((num = ctoi(str[n++])) == 0xFF) {
			return FALSE;
		}
		*num_arr = num << 4;
		if ((num = ctoi(str[n++])) == 0xFF) {
			return FALSE;
		}
		*num_arr |= num;
		num_arr--;
	}
	return TRUE;
}

static int hex_str_to_int(u32 str_len, s8*str)
{
	int result = 0;
	unsigned int n = 2;
	if((str_len < 3) || (str[0] != '0') || ((str[1] != 'x') && (str[1] != 'X'))){
		return -1;
	}
	while(n < str_len){
		result = (result << 4) | (ctoi(str[n++]));
	}
	return result;
}

/* parse string to integer, if string begin with 0x or 0X, treat as hex,
   else treat as decimal */
int str_to_int(char *str)
{
	bool is_hex = FALSE;
	uint32_t str_len = strlen(str);

	if ((str_len > 2) && ('0' == str[0]) && ('x' == str[1] || 'X' == str[1])) {
		is_hex = TRUE;
	}
	if (is_hex) {
		return hex_str_to_int(str_len, ( s8 *)str);
	} else {
		return atoi(str);
	}
}

void bt_at_iouart_dump(uint8_t unit, const char *str, void *buf, uint16_t len)
{
	int i = 0;

	if (!buf || !len) {
		return;
	}

	at_printf("%s", str);
	for (i = 0; i < len; i++) {
		if (unit == 4) {
			at_printf(",%08x", LE_TO_U32((uint8_t *)buf + i * 4)); /* *(buf + i) may crash at AmebaLite when (buf + i) isn't aligned with 4.*/
		} else if (unit == 2) {
			at_printf(",%04x", LE_TO_U16((uint8_t *)buf + i * 2));
		} else {
			at_printf(",%02x", *((uint8_t *)buf + i));
		}
	}
	at_printf("\r\n");
}

void bt_at_iouart_dump_hex(const char *start_str, void *buf, uint16_t len, bool reverse, const char *end_str)
{
	int i = 0;

	if (!buf || !len) {
		// print "\r\n" or ""
		at_printf("%s", end_str);
		return;
	}

	at_printf("%s", start_str);
	for (i = 0; i < len; i++) {
		if (reverse) {
			at_printf("%02x", *((uint8_t *)(buf) + len - 1 - i));
		} else {
			at_printf("%02x", *((uint8_t *)(buf) + i));
		}
	}
	at_printf("%s", end_str);
}

/* This is little endian transform, The hex string is considered as a long number begin with 0x,
    e.g. string:"0x123456789a" transformed to array is arr[]: {0x9a, 0x78, 0x56, 0x34, 0x12} */
bool hexnum_str_to_array(char *str, uint8_t *byte_arr, uint8_t arr_len)
{
	char *str_low = NULL;
	uint32_t str_len = strlen(str);
	uint32_t hex_str_len = 0;
	uint8_t byte_high = 0, byte_low = 0;
	uint32_t n = 0;

	if ((str_len < 3) || (str[0] != '0') || ((str[1] != 'x') && (str[1] != 'X'))) {
		printf("[%s]Error: Hexnum is not begin with 0x or 0X!!!\r\n", __func__);
		return FALSE;
	}

	str_low = str + str_len - 1;
	hex_str_len = str_len - 2;
	if (arr_len < (hex_str_len + 1) / 2) {
		return FALSE;
	}

	memset(byte_arr, 0, arr_len);
	while (n <= (hex_str_len + 1) / 2 - 1) {
		byte_low = ctoi(*(str_low--));

		/* deal with the highest byte, it may be 1_hex_str(e.g. 0xF), or 2_hex_str(e.g. 0xFF) */
		if ((n == ((hex_str_len + 1) / 2 - 1)) && (hex_str_len % 2)) {
			byte_high = 0;
		} else {
			byte_high = ctoi(*(str_low--));
		}

		if (byte_low == 0xFF || byte_high == 0xFF) {
			return FALSE;
		}
		byte_arr[n] = byte_high << 4 | byte_low;
		n++;
	}

	return TRUE;
}

/* The hex str is not considered as a number, just regared as a list of byte data,
    e.g. string: "112233445566778899aabb" transformed to arrary is as
    arr[]: {0x11, 0x22, 0x33, 0x44, 0x55, ...}  */
bool hexdata_str_to_array(char *str, uint8_t *byte_arr, uint8_t arr_len)
{
	uint32_t str_len = strlen(str);
	char *str_ptr = str;
	uint32_t n = 0;
	uint8_t byte_high = 0, byte_low = 0;

	if (str_len % 2 || arr_len < str_len / 2) {
		printf("[%s]Error: Hexdata is invalid\r\n", __func__);
		return FALSE;
	}

	while (n < str_len) {
		byte_high = ctoi(*(str_ptr++));
		byte_low = ctoi(*(str_ptr++));
		if (byte_high == 0xFF || byte_low == 0xFF) {
			return FALSE;
		}
		byte_arr[n / 2] = byte_high << 4 | byte_low;
		n += 2;
	}

	return TRUE;
}

#if 0
static int dec_str_to_int(u32 str_len, s8*str)
{
	int result = 0;
	unsigned int n = 0;

	while(n < str_len){
		result = result*10 + (ctoi(str[n++]));
	}
	return result;
}
#endif

void ble_transfer_at_cmd_send_msg(uint16_t sub_type)
{
	uint8_t event = EVENT_IO_TO_APP;

	T_IO_MSG io_msg;

	io_msg.type = IO_MSG_TYPE_QDECODE;
	io_msg.subtype = sub_type;

	if (ble_transfer_evt_queue_handle != NULL && ble_transfer_io_queue_handle != NULL) {
		if (os_msg_send(ble_transfer_io_queue_handle, &io_msg, 0) == false) {
			printf("ble transfer at cmd send msg fail: subtype 0x%x\r\n", io_msg.subtype);
		} else if (os_msg_send(ble_transfer_evt_queue_handle, &event, 0) == false) {
			printf("ble transfer at cmd send event fail: subtype 0x%x\r\n", io_msg.subtype);
		}
	}
}

int ble_transfer_at_cmd_get_name(int argc, char **argv)
{
	(void)argc;
	(void)argv;

	printf("[AT+BTDEMO] Transfer module get device name: %s\r\n", dev_name);
	at_printf("+BTDEMO:transfer_module,get_name,%s\r\n", dev_name);
	return 0;
}

int ble_transfer_at_cmd_set_name(int argc, char **argv)
{
	if (strlen(argv[1]) >= BLE_TRANSFER_MODULE_DEV_NAME_MAX_LEN) {
		printf("[AT+BTDEMO] Transfer module set device name string too long\r\n");
		return -1;
	}

	memset(dev_name, 0, sizeof(dev_name));
	memcpy(dev_name, argv[1], strlen(argv[1]));
	
	le_set_gap_param(GAP_PARAM_DEVICE_NAME, GAP_DEVICE_NAME_LEN, dev_name);
	return 0;
}

int ble_transfer_at_cmd_get_uuid(int argc, char **argv)
{
	uint8_t attr = str_to_int(argv[1]);
	uint16_t uuid = 0;

	if(!ble_transfer_module_get_uuid(attr, &uuid)) {
		printf("[AT+BTDEMO] Transfer module get uuid failed\r\n");
		return -1;
	}
	printf("[AT+BTDEMO] Transfer module get uuid attr=%d uuid=0x%04x\r\n", attr, uuid);
	at_printf("+BTDEMO:transfer_module,get_uuid,%d,0x%04x\r\n", attr, uuid);
	return 0;
}

int ble_transfer_at_cmd_set_uuid(int argc, char **argv)
{
	uint8_t attr = str_to_int(argv[1]);
	uint16_t uuid = (uint16_t)hex_str_to_int(strlen(argv[2]), (s8 *)argv[2]);

	if(!ble_transfer_module_set_uuid(attr, uuid)) {
		printf("[AT+BTDEMO] Transfer module set uuid failed\r\n");
		return -1;
	}
	return 0;
}

int ble_transfer_at_cmd_read_val(int argc, char **argv)
{
	uint8_t  len = 0;
	uint8_t *data = NULL;
	uint16_t ret = 0;

	len = str_to_int(argv[1]);

	if (len > TRANSFER_READ_V1_MAX_LEN) {
		printf("[AT+BTDEMO] Transfer module set read value failed: wrong arg length!\r\n");
		return -1;
	}

	if (len != strlen(argv[2]) / 2) {
		printf("[AT+BTDEMO] Transfer module set read value failed: length unmatch with data!\r\n");
		return -1;
	}
	data = (uint8_t *)osif_mem_alloc(RAM_TYPE_DATA_ON, len);
	if (!data) {
		printf("[AT+BTDEMO] Transfer module set read value failed: cant alloc memory\r\n");
		return -1;
	}

	hexdata_str_to_array(argv[2], (uint8_t *)data, len);

	ret = ble_transfer_module_update_read_val(len, data);
	if (!ret) {
		osif_mem_free((void *)data);
		printf("[AT+BTDEMO] Transfer module set read value failed! err: 0x%x\r\n", ret);
		return -1;
	}

	osif_mem_free((void *)data);
	return 0;
}

int ble_transfer_at_cmd_get_addr(int argc, char **argv)
{
	(void)argc;
	(void)argv;

	T_GAP_CAUSE cause;
	uint8_t addr_val[BLE_TRANSFER_MODULE_BD_ADDR_LEN] = {0};
	char addr_str[30] = {0};

	cause = gap_get_param(GAP_PARAM_BD_ADDR, addr_val);
	if (cause) {
		printf("ble_transfer_module_get_device_addr: cause = %x \r\n", cause);
		return cause;
	}

	snprintf(addr_str, sizeof(addr_str), "%02X:%02X:%02X:%02X:%02X:%02X(public)",
			 addr_val[5], addr_val[4], addr_val[3],
			 addr_val[2], addr_val[1], addr_val[0]);
	printf("GAP get bd_addr: %s\r\n", addr_str);
	at_printf("+BLEGAP:addr,%s\r\n", addr_str);
	return 0;
}

T_GAP_RAND_ADDR_TYPE ble_transfer_rand_addr_type(uint8_t *addr_val)
{
	uint8_t flag_bits = ((addr_val[5] >> 6) & 0x3);

	if (3 == flag_bits) {
		return GAP_RAND_ADDR_STATIC;
	} else if (0 == flag_bits) {
		return GAP_RAND_ADDR_NON_RESOLVABLE;
	} else if (1 == flag_bits) {
		return GAP_RAND_ADDR_RESOLVABLE;
	} else {
		return 0xFF;
	}
}

int ble_transfer_at_cmd_set_rand_addr(int argc, char **argv)
{
	if (argc != 2 && argc != 3) {
		printf("GAP set rand addr failed! wrong args num!\r\n");
		return -1;
	}

	T_GAP_CAUSE cause;
	uint8_t addr[BLE_TRANSFER_MODULE_BD_ADDR_LEN] = {0};
	uint8_t type = 0;

	if (argc == 2) {
		if(hex_str_to_bd_addr(strlen(argv[1]), ( s8 *)argv[1], (u8*)addr) == false) {
			return -1;
		}
		cause = le_set_rand_addr(addr);
		if (cause) {
			return -1;
		}
	} else if (argc == 3) {
		type = str_to_int(argv[2]);
		cause = le_gen_rand_addr((T_GAP_RAND_ADDR_TYPE)type, addr);
		if (cause) {
			return -1;
		}
		cause = le_set_rand_addr(addr);
		if (cause) {
			return -1;
		}
	} else {
		return -1;
	}

	/*  if set static random addr as local addr, it should also be set as identity addr,
	    otherwise, public addr will be distributed when pairing, may cause problem */
	if (GAP_RAND_ADDR_STATIC == ble_transfer_rand_addr_type(addr)) {
		cause = le_cfg_local_identity_address(addr, GAP_IDENT_ADDR_RAND);
		if (cause) {
			return -1;
		}
	}
	printf("GAP set rand addr success!");
	return 0;
}

int ble_transfer_at_cmd_get_mtu_size(int argc, char **argv)
{
	if (argc != 2) {
		printf("GAP get mtu size failed! wrong args num!\r\n");
		return -1;
	}

	T_GAP_CAUSE cause;
	uint8_t mtu_size, conn_id;
	uint16_t conn_handle = str_to_int(argv[1]);

	le_get_conn_id_by_handle(conn_handle, &conn_id);

	if(ble_transfer_app_link_table[conn_id].conn_state != GAP_CONN_STATE_CONNECTED) {
		printf("no active link, conn handle %d\r\n", conn_handle);
		return -1;
	}
	cause = le_get_conn_param(GAP_PARAM_CONN_MTU_SIZE, &mtu_size, conn_id);
	if (cause) {
		return -1;
	}

	printf("GAP get mtu size, conn_handle: %d, mtu_size: %d\r\n", conn_handle, mtu_size);
	at_printf("+BLEGAP:mtu_size,%d,%d\r\n", conn_handle, mtu_size);
	return 0;
}

int ble_transfer_at_cmd_set_max_mtu_size(int argc, char **argv)
{
	if (argc != 2) {
		printf("GAP set mtu size failed! wrong args num!\r\n");
		return -1;
	}

	T_GAP_CAUSE cause;
	T_GAP_DEV_STATE dev_state;
	uint8_t active_conn_num = 0;
	uint8_t mtu_size = str_to_int(argv[1]);
	
	cause = le_get_gap_param(GAP_PARAM_DEV_STATE, &dev_state);
	if (cause) {
		return -1;
	}
	active_conn_num = le_get_active_link_num();

	if ((GAP_CONN_STATE_CONNECTING == dev_state.gap_conn_state) || active_conn_num) {
		return -1;
	}

	gap_config_max_mtu_size(mtu_size);
	return 0;
}

int ble_transfer_at_cmd_op_adv(int argc, char **argv)
{
	ble_transfer_module_adv_param_t adv_param;
	uint8_t mtu_size;
	T_GAP_CAUSE cause;
	int en = str_to_int(argv[1]);

	if (0 == en) {
		if (le_adv_stop()) {
			return -1;
		}
	} else if (1 == en) {
		memcpy(&adv_param, &def_adv_param, sizeof(ble_transfer_module_adv_param_t));
		if (argc >= 5) {
			adv_param.type = str_to_int(argv[2]);
			adv_param.own_addr_type = str_to_int(argv[3]);
			adv_param.filter_policy = str_to_int(argv[4]);
		}
		if (argc >= 7) {
			adv_param.peer_addr.type = str_to_int(argv[5]);
			hex_str_to_bd_addr(strlen(argv[6]), ( s8 *)argv[6], (u8*)adv_param.peer_addr.addr_val);
		}
		if (argc >= 10) {
			adv_param.interval_min = str_to_int(argv[7]);
			adv_param.interval_max = str_to_int(argv[8]);
			adv_param.channel_map = str_to_int(argv[9]);
		}

		//step 1: set parameter
		cause = le_adv_set_param(GAP_PARAM_ADV_INTERVAL_MIN, sizeof(adv_param.interval_min),
								 &adv_param.interval_min);
		if (cause) {
			return -1;
		}
		cause = le_adv_set_param(GAP_PARAM_ADV_INTERVAL_MAX, sizeof(adv_param.interval_max),
								 &adv_param.interval_max);
		if (cause) {
			return -1;
		}
		cause = le_adv_set_param(GAP_PARAM_ADV_EVENT_TYPE, sizeof((uint8_t)adv_param.type),
								 &adv_param.type);
		if (cause) {
			return -1;
		}
		cause = le_adv_set_param(GAP_PARAM_ADV_LOCAL_ADDR_TYPE, sizeof((uint8_t)adv_param.own_addr_type),
								 &adv_param.own_addr_type);
		if (cause) {
			return -1;
		}
		cause = le_adv_set_param(GAP_PARAM_ADV_DIRECT_ADDR_TYPE, sizeof((uint8_t)adv_param.peer_addr.type),
								 &adv_param.peer_addr.type);
		if (cause) {
			return -1;
		}
		cause = le_adv_set_param(GAP_PARAM_ADV_DIRECT_ADDR, sizeof(adv_param.peer_addr.addr_val),
								 &adv_param.peer_addr.addr_val);
		if (cause) {
			return -1;
		}
		cause = le_adv_set_param(GAP_PARAM_ADV_CHANNEL_MAP, sizeof((uint8_t)adv_param.channel_map),
								 &adv_param.channel_map);
		if (cause) {
			return -1;
		}
		cause = le_adv_set_param(GAP_PARAM_ADV_FILTER_POLICY, sizeof((uint8_t)adv_param.filter_policy),
								 &adv_param.filter_policy);
		if (cause) {
			return -1;
		}
		if (le_adv_start()) {
			return -1;
		}
	}
	return 0;
}

int ble_transfer_at_cmd_get_adv_param(int argc, char **argv)
{
	(void)argc;
	(void)argv;

	ble_transfer_module_adv_param_t adv_param = {0};
	uint8_t *addr_val = NULL;
	char addr_str[13] = {0};
	T_GAP_CAUSE cause;

	cause = le_adv_get_param(GAP_PARAM_ADV_INTERVAL_MIN, &adv_param.interval_min);
	if (cause) {
		return -1;
	}
	cause = le_adv_get_param(GAP_PARAM_ADV_INTERVAL_MAX, &adv_param.interval_max);
	if (cause) {
		return -1;
	}
	cause = le_adv_get_param(GAP_PARAM_ADV_EVENT_TYPE, &adv_param.type);
	if (cause) {
		return -1;
	}
	cause = le_adv_get_param(GAP_PARAM_ADV_LOCAL_ADDR_TYPE, &adv_param.own_addr_type);
	if (cause) {
		return -1;
	}
	cause = le_adv_get_param(GAP_PARAM_ADV_DIRECT_ADDR_TYPE, &(adv_param.peer_addr.type));
	if (cause) {
		return -1;
	}
	cause = le_adv_get_param(GAP_PARAM_ADV_DIRECT_ADDR, &(adv_param.peer_addr.addr_val));
	if (cause) {
		return -1;
	}
	cause = le_adv_get_param(GAP_PARAM_ADV_CHANNEL_MAP, &adv_param.channel_map);
	if (cause) {
		return -1;
	}
	cause = le_adv_get_param(GAP_PARAM_ADV_FILTER_POLICY, &adv_param.filter_policy);
	if (cause) {
		return -1;
	}

	addr_val = adv_param.peer_addr.addr_val;
	snprintf(addr_str, sizeof(addr_str), "%02x%02x%02x%02x%02x%02x",
			 addr_val[5], addr_val[4], addr_val[3],
			 addr_val[2], addr_val[1], addr_val[0]);

	printf("GAP get adv param success, param: %d,%d,%d,%d,%s,%d,%d,%d\r\n",
			adv_param.type, adv_param.own_addr_type, adv_param.filter_policy,
			adv_param.peer_addr.type, addr_str, adv_param.interval_min,
			adv_param.interval_max, adv_param.channel_map);
	at_printf("+BLEGAP:get_adv_param,%d,%d,%d,%d,%s,%d,%d,%d\r\n",
				adv_param.type, adv_param.own_addr_type, adv_param.filter_policy,
				adv_param.peer_addr.type, addr_str, adv_param.interval_min,
				adv_param.interval_max, adv_param.channel_map);
	return 0;
}

int ble_transfer_at_cmd_op_scan(int argc, char **argv)
{
	if (argc != 2) {
		printf("GAP scan failed! wrong args num!\r\n");
		return -1;
	}

	T_GAP_CAUSE cause;
	int en = str_to_int(argv[1]);

	if (0 == en) {
		cause = le_scan_stop();
		if (cause) {
			printf("GAP stop scan failed! \r\n");
			return -1;
		} else {
			printf("GAP stopping scan ...\r\n");
		}
	} else if (1 == en) {
		cause = le_scan_start();
		if (cause) {
			printf("GAP start scan failed! \r\n");
			return -1;
		} else {
			printf("GAP starting scan ...\r\n");
		}
	} else {
		printf("GAP scan op failed! wrong args!\r\n");
		return -1;
	}
	return 0;
}

int ble_transfer_at_cmd_set_scan_param(int argc, char **argv)
{
	(void)argc;
	(void)argv;

	T_GAP_CAUSE cause;
	ble_transfer_module_scan_param_t scan_param = {0};

	if (argc != 1 && argc != 5 && argc != 7) {
		printf("GAP set scan paramters failed! wrong args num!\r\n");
		return -1;
	}

	memcpy(&scan_param, &def_scan_param, sizeof(ble_transfer_module_scan_param_t));
	if (argc == 1) {
		; /* do nothing, just use default def_scan_param */
	}
	if (argc >= 5) {
		scan_param.type = str_to_int(argv[1]);
		scan_param.own_addr_type = str_to_int(argv[2]);
		scan_param.filter_policy = str_to_int(argv[3]);
		scan_param.duplicate_opt = str_to_int(argv[4]);
	}
	if (argc >= 7) {
		scan_param.interval = str_to_int(argv[5]);
		scan_param.window = str_to_int(argv[6]);
	}

	cause = le_scan_set_param(GAP_PARAM_SCAN_MODE, sizeof(scan_param.type), &scan_param.type);
	if (cause) {
		return -1;
	}
	cause = le_scan_set_param(GAP_PARAM_SCAN_INTERVAL, sizeof(scan_param.interval), &scan_param.interval);
	if (cause) {
		return -1;
	}
	cause = le_scan_set_param(GAP_PARAM_SCAN_WINDOW, sizeof(scan_param.window), &scan_param.window);
	if (cause) {
		return -1;
	}
	cause = le_scan_set_param(GAP_PARAM_SCAN_LOCAL_ADDR_TYPE, sizeof(scan_param.own_addr_type), &scan_param.own_addr_type);
	if (cause) {
		return -1;
	}
	cause = le_scan_set_param(GAP_PARAM_SCAN_FILTER_POLICY, sizeof(scan_param.filter_policy), &scan_param.filter_policy);
	if (cause) {
		return -1;
	}
	cause = le_scan_set_param(GAP_PARAM_SCAN_FILTER_DUPLICATES, sizeof(scan_param.duplicate_opt), &scan_param.duplicate_opt);
	if (cause) {
		return -1;
	}

	printf("GAP set scan param success\r\n");
	return 0;
}

int ble_transfer_at_cmd_get_scan_param(int argc, char **argv)
{
	(void)argc;
	(void)argv;

	T_GAP_CAUSE cause;
	ble_transfer_module_scan_param_t scan_param = {0};

	cause = le_scan_get_param(GAP_PARAM_SCAN_MODE, &scan_param.type);
	if (cause) {
		return -1;
	}
	cause = le_scan_get_param(GAP_PARAM_SCAN_INTERVAL, &scan_param.interval);
	if (cause) {
		return -1;
	}
	cause = le_scan_get_param(GAP_PARAM_SCAN_WINDOW, &scan_param.window);
	if (cause) {
		return -1;
	}
	cause = le_scan_get_param(GAP_PARAM_SCAN_LOCAL_ADDR_TYPE, &scan_param.own_addr_type);
	if (cause) {
		return -1;
	}
	cause = le_scan_get_param(GAP_PARAM_SCAN_FILTER_POLICY, &scan_param.filter_policy);
	if (cause) {
		return -1;
	}
	cause = le_scan_get_param(GAP_PARAM_SCAN_FILTER_DUPLICATES, &scan_param.duplicate_opt);
	if (cause) {
		return -1;
	}

	printf("GAP get scan param success, param: %d,%d,%d,%d,%d,%d\r\n",
			scan_param.type, scan_param.own_addr_type, scan_param.filter_policy,
			scan_param.duplicate_opt, scan_param.interval, scan_param.window);
	at_printf("+BLEGAP:get_scan_param,%d,%d,%d,%d,%d,%d\r\n",
				scan_param.type, scan_param.own_addr_type, scan_param.filter_policy,
				scan_param.duplicate_opt, scan_param.interval, scan_param.window);
	return 0;
}

int ble_transfer_at_cmd_connect(int argc, char **argv)
{
	if(ble_transfer_central_app_max_links >= BLE_TRANSFER_CENTRAL_APP_MAX_LINKS){
		printf("transfer module: exceed the max links number\r\n");
		return -1;
	}
	T_GAP_CAUSE cause;
	u8 DestAddr[BLE_TRANSFER_MODULE_BD_ADDR_LEN] = {0};
	u8 DestAddrType = GAP_REMOTE_ADDR_LE_PUBLIC;
	u8 filter_policy = 0;
	T_GAP_LOCAL_ADDR_TYPE local_addr_type = GAP_LOCAL_ADDR_LE_PUBLIC;
	T_GAP_LE_CONN_REQ_PARAM conn_req_param;
	uint8_t *peer_addr_val = NULL;

	conn_req_param.scan_interval = 0x60;
	conn_req_param.scan_window = 0x30;
	conn_req_param.conn_interval_min = 0x60;
	conn_req_param.conn_interval_max = 0x60;
	conn_req_param.conn_latency = 0;
	conn_req_param.supv_tout = 0x100;
	conn_req_param.ce_len_min = 2 * (conn_req_param.conn_interval_min - 1);
	conn_req_param.ce_len_max = 2 * (conn_req_param.conn_interval_max - 1);

	if (argc >= 3) {
		DestAddrType = str_to_int(argv[1]);
		if (strlen(argv[2]) != 2 * BLE_TRANSFER_MODULE_BD_ADDR_LEN) {
			printf("ERROR: mac address length error!\r\n");
			return -1;
		}
		hex_str_to_bd_addr(strlen(argv[2]), ( s8 *)argv[2], (u8*)DestAddr);
	}
	if (argc >= 5) {
		local_addr_type = str_to_int(argv[3]);
		filter_policy= str_to_int(argv[4]);
	}
	if (argc >= 11) {
		conn_req_param.scan_interval = str_to_int(argv[5]);
		conn_req_param.scan_window = str_to_int(argv[6]);
		conn_req_param.conn_interval_min = str_to_int(argv[7]);
		conn_req_param.conn_interval_max = str_to_int(argv[8]);
		conn_req_param.conn_latency = str_to_int(argv[9]);
		conn_req_param.supv_tout = str_to_int(argv[10]);
	}

	if(le_set_conn_param(GAP_CONN_PARAM_1M, &conn_req_param)) {
		return -1;
	}

	printf("cmd_con, DestAddr: 0x%02X:0x%02X:0x%02X:0x%02X:0x%02X:0x%02X\r\n",
			DestAddr[5], DestAddr[4], DestAddr[3], DestAddr[2], DestAddr[1], DestAddr[0]);

	if (filter_policy == 0) {
		cause = le_connect(0, DestAddr, (T_GAP_REMOTE_ADDR_TYPE)DestAddrType, local_addr_type, 1000);
	} else {
		cause = le_connect(0, NULL, (T_GAP_REMOTE_ADDR_TYPE)DestAddrType, local_addr_type, 1000);
	}
	
	return cause;
}

int ble_transfer_at_cmd_disconnect(int argc, char **argv)
{	
	if (argc != 2) {
		printf("GAP disconnect failed! wrong args num!\r\n");
		return -1;
	}

	T_GAP_CAUSE cause;
	uint8_t conn_id;
	uint16_t conn_handle = str_to_int(argv[1]);

	le_get_conn_id_by_handle(conn_handle, &conn_id);

	cause = le_disconnect(conn_id);
	if (cause) {
		return -1;
	}
	return 0;
}

int ble_transfer_at_cmd_get_conn_info(int argc, char **argv)
{	
	if (argc != 2) {
		printf("GAP get connect info failed! wrong args num!\r\n");
		return -1;
	}

	T_GAP_CAUSE cause;
	uint8_t conn_id;
	uint16_t conn_handle = str_to_int(argv[1]);
	T_GAP_CONN_INFO conn_info;
	char bd_addr_str[30] = {0};
	uint16_t interval, latency, supv_timeout;
	uint8_t tx_phy, rx_phy;
	ble_transfer_module_le_addr_t addr = {0};

	le_get_conn_id_by_handle(conn_handle, &conn_id);
	
	if (le_get_conn_info(conn_id, &conn_info) == false) {
		printf("GAP get connection info failed!\r\n");
	}
	memcpy(addr.addr_val, conn_info.remote_bd, sizeof(conn_info.remote_bd));
	addr.type = conn_info.remote_bd_type;
	ble_transfer_module_addr_to_str(&addr, bd_addr_str, sizeof(bd_addr_str));

	cause = le_get_conn_param(GAP_PARAM_CONN_INTERVAL, &interval, conn_id);
	if (cause) {
		return -1;
	}
	cause = le_get_conn_param(GAP_PARAM_CONN_LATENCY, &latency, conn_id);
	if (cause) {
		return -1;
	}
	cause = le_get_conn_param(GAP_PARAM_CONN_TIMEOUT, &supv_timeout, conn_id);
	if (cause) {
		return -1;
	}
#if defined(F_BT_LE_5_0_SET_PHYS_SUPPORT) && F_BT_LE_5_0_SET_PHYS_SUPPORT
	cause = le_get_conn_param(GAP_PARAM_CONN_TX_PHY_TYPE, &tx_phy, conn_id);
	if (cause) {
		return -1;
	}
	cause = le_get_conn_param(GAP_PARAM_CONN_RX_PHY_TYPE, &rx_phy, conn_id);
	if (cause) {
		return -1;
	}
#else /* the default phy is 1M uncoded */
	tx_phy = 1;
	rx_phy = 1;
#endif

	printf("GAP get connection info:\r\n"
			"conn_handle: %d, local role: %s, peer bd_addr: (%s),\r\n"
			"interval: 0x%x, latency: 0x%x, supv_timeout: 0x%x, tx_phy: 0x%x, rx_phy: 0x%x\r\n",
			conn_handle, (conn_info.role == GAP_LINK_ROLE_MASTER) ?
			"master" : ((conn_info.role == GAP_LINK_ROLE_SLAVE) ? "slave" : "unknown"),
			bd_addr_str, interval, latency, supv_timeout, tx_phy, rx_phy);
	at_printf("+BLEGAP:conn_info,%d,%s,%s,0x%x,0x%x,0x%x,0x%x,0x%x\r\n",
				conn_handle, (conn_info.role == GAP_LINK_ROLE_MASTER) ?
				"master" : ((conn_info.role == GAP_LINK_ROLE_SLAVE) ? "slave" : "unknown"),
				bd_addr_str, interval, latency, supv_timeout, tx_phy, rx_phy);
	return 0;
}

int ble_transfer_at_cmd_update_conn_request(int argc, char **argv)
{
	if (argc != 6) {
		printf("GAP update conn failed! wrong args num!\r\n");
		return -1;
	}

	T_GAP_CAUSE cause;
	uint8_t conn_id;
	uint16_t conn_handle = str_to_int(argv[1]);
	uint16_t conn_interval_min = str_to_int(argv[2]);
	uint16_t conn_interval_max = str_to_int(argv[3]);
	uint16_t conn_latency = str_to_int(argv[4]);
	uint16_t supervision_timeout = str_to_int(argv[5]);

	le_get_conn_id_by_handle(conn_handle, &conn_id);
	cause = le_update_conn_param(conn_id,
								conn_interval_min,
								conn_interval_max,
								conn_latency,
								supervision_timeout,
								2 * (conn_interval_min - 1),
								2 * (conn_interval_max - 1));
	return cause;
}

int ble_transfer_at_cmd_add_whitelist(int argc, char **argv)
{
	if (argc != 3) {
		printf("GAP add whitelist failed! wrong args num!\r\n");
		return -1;
	}

	T_GAP_CAUSE cause;
	T_GAP_REMOTE_ADDR_TYPE addr_type = str_to_int(argv[1]);
	u8 DestAddr[BLE_TRANSFER_MODULE_BD_ADDR_LEN] = {0};

	hex_str_to_bd_addr(strlen(argv[2]), ( s8 *)argv[2], (u8*)DestAddr);
	cause = le_modify_white_list(GAP_WHITE_LIST_OP_ADD, DestAddr, addr_type);
	
	return cause;
}

int ble_transfer_at_cmd_remove_whitelist(int argc, char **argv)
{
	if (argc != 3) {
		printf("GAP remove whitelist failed! wrong args num!\r\n");
		return -1;
	}

	T_GAP_CAUSE cause;
	T_GAP_REMOTE_ADDR_TYPE addr_type = str_to_int(argv[1]);
	u8 DestAddr[BLE_TRANSFER_MODULE_BD_ADDR_LEN] = {0};

	hex_str_to_bd_addr(strlen(argv[2]), ( s8 *)argv[2], (u8*)DestAddr);
	cause = le_modify_white_list(GAP_WHITE_LIST_OP_REMOVE, DestAddr, addr_type);
	
	return cause;
}

int ble_transfer_at_cmd_clear_whitelist(int argc, char **argv)
{
	(void) argc;
	(void) argv;
	T_GAP_CAUSE cause;

	cause = le_modify_white_list(GAP_WHITE_LIST_OP_CLEAR, NULL, GAP_REMOTE_ADDR_LE_PUBLIC);
	return cause;
}

int ble_transfer_at_cmd_set_security_param(int argc, char **argv)
{
	if (argc != 1 && argc != 8 && argc != 10) {
		printf("GAP set security paramters failed! wrong args num!\r\n");
		return -1;
	}

	T_GAP_CAUSE cause;
	uint16_t auth_flags = 0;
	uint16_t auth_sec_req_flags = 0;
	ble_transfer_module_security_param_t sec_param = {
			.io_cap = GAP_IO_CAP_KEYBOARD_DISPLAY,
			.oob_data_flag = 0,
			.bond_flag = 1,
			.mitm_flag = 0,
			.sec_pair_flag = 0,
			.sec_pair_only_flag = 0,
			.use_fixed_key = 0,
			.fixed_key = 000000,
			.auto_sec_req = 0,
	};

	if (argc > 7) {
		sec_param.io_cap = (T_GAP_IO_CAP)str_to_int(argv[1]);
		sec_param.oob_data_flag = (uint8_t)str_to_int(argv[2]);
		sec_param.bond_flag = (uint8_t)str_to_int(argv[3]);
		sec_param.mitm_flag = (uint8_t)str_to_int(argv[4]);
		sec_param.sec_pair_flag = (uint8_t)str_to_int(argv[5]);
		sec_param.use_fixed_key = (uint8_t)str_to_int(argv[6]);
		sec_param.fixed_key = (uint32_t)str_to_int(argv[7]);
	}
	if (argc > 9) {
		sec_param.sec_pair_only_flag = (uint8_t)str_to_int(argv[8]);
		sec_param.auto_sec_req = (uint8_t)str_to_int(argv[9]);
	}

	cause = gap_set_param(GAP_PARAM_BOND_IO_CAPABILITIES, sizeof(uint8_t), &sec_param.io_cap);
	if (cause) {
		return -1;
	}
#if defined(F_BT_LE_SMP_OOB_SUPPORT) && F_BT_LE_SMP_OOB_SUPPORT
	cause = gap_set_param(GAP_PARAM_BOND_OOB_ENABLED, sizeof(uint8_t), &sec_param.oob_data_flag);
	if (cause) {
		return -1;
	}
#endif
	auth_flags = (!!sec_param.bond_flag) | (!!sec_param.mitm_flag) << 2 |
				 (!!sec_param.sec_pair_flag) << 3 | (!!sec_param.sec_pair_only_flag) << 9;
	cause = gap_set_param(GAP_PARAM_BOND_AUTHEN_REQUIREMENTS_FLAGS, sizeof(uint16_t), &auth_flags);
	if (cause) {
		return -1;
	}

	cause = le_bond_set_param(GAP_PARAM_BOND_SEC_REQ_ENABLE, sizeof(uint8_t), &sec_param.auto_sec_req);
	if (cause) {
		return -1;
	}

	auth_sec_req_flags = auth_flags;
	cause = le_bond_set_param(GAP_PARAM_BOND_SEC_REQ_REQUIREMENT, sizeof(uint16_t), &auth_sec_req_flags);
	if (cause) {
		return -1;
	}

	if (sec_param.use_fixed_key) {
		cause = le_bond_set_param(GAP_PARAM_BOND_FIXED_PASSKEY_ENABLE, sizeof(uint8_t), &sec_param.use_fixed_key);
		if (cause) {
			return -1;
		}
		cause = le_bond_set_param(GAP_PARAM_BOND_FIXED_PASSKEY, sizeof(uint32_t), &sec_param.fixed_key);
		if (cause) {
			return -1;
		}
	}
	cause = gap_set_pairable_mode();
	if (cause) {
		return -1;
	}
	return 0;
}

int ble_transfer_at_cmd_security(int argc, char **argv)
{
	if (argc != 2) {
		printf("GAP start security failed! wrong args num!\r\n");
		return -1;
	}

	T_GAP_CAUSE cause;
	uint8_t conn_id;
	uint16_t conn_handle = str_to_int(argv[1]);

	le_get_conn_id_by_handle(conn_handle, &conn_id);
	is_pairing_initiator[conn_id] = 1;
	cause = le_bond_pair(conn_id);
	return cause;
}

int ble_transfer_at_cmd_confirm_pair(int argc, char **argv)
{
	if (argc != 3) {
		printf("GAP confirm pair failed! wrong args num!\r\n");
		return -1;
	}

	T_GAP_CAUSE cause;
	uint8_t conn_id;
	uint16_t conn_handle = str_to_int(argv[1]);
	T_GAP_CFM_CAUSE confirm = (T_GAP_CFM_CAUSE)str_to_int(argv[2]);

	le_get_conn_id_by_handle(conn_handle, &conn_id);
	
	if (confirm < GAP_CFM_CAUSE_ACCEPT || confirm > GAP_CFM_CAUSE_REJECT) {
		printf("GAP pair confirm failed!\r\n");
	}
	cause = le_bond_just_work_confirm(conn_id, confirm);
	return cause;
}

int ble_transfer_at_cmd_input_auth_key(int argc, char **argv)
{
	if (argc != 3) {
		printf("GAP auth key failed! wrong args num!\r\n");
		return -1;
	}

	T_GAP_CAUSE cause;
	uint8_t conn_id;
	uint16_t conn_handle = str_to_int(argv[1]);
	uint32_t passkey = (uint32_t)str_to_int(argv[2]);

	le_get_conn_id_by_handle(conn_handle, &conn_id);

	if (passkey > 999999) {
		printf("GAP input auth key failed!\r\n");
	}
	cause = le_bond_passkey_input_confirm(conn_id, passkey, GAP_CFM_CAUSE_ACCEPT);
	return cause;
}

int ble_transfer_at_cmd_confirm_auth_key(int argc, char **argv)
{
	if (argc != 3) {
		printf("GAP confirm auth key failed! wrong args num!\r\n");
		return -1;
	}

	T_GAP_CAUSE cause;
	uint8_t conn_id;
	uint16_t conn_handle = str_to_int(argv[1]);
	T_GAP_CFM_CAUSE confirm = (T_GAP_CFM_CAUSE)str_to_int(argv[2]);

	le_get_conn_id_by_handle(conn_handle, &conn_id);
	if (confirm < GAP_CFM_CAUSE_ACCEPT || confirm > GAP_CFM_CAUSE_REJECT) {
		printf("GAP auth key confirm failed!\r\n");
	}
	cause = le_bond_user_confirm(conn_id, confirm);
	return cause;
}

int ble_transfer_at_cmd_input_auth_oob(int argc, char **argv)
{
	if (argc != 3) {
		printf("GAP auth oob failed! wrong args num!\r\n");
		return -1;
	}

	T_GAP_CAUSE cause;
	uint8_t conn_id;
	uint16_t conn_handle = str_to_int(argv[1]);
	uint8_t oob_key[16] = {0};

	le_get_conn_id_by_handle(conn_handle, &conn_id);
	hexnum_str_to_array(argv[2], (uint8_t *)oob_key, 16);

	cause = le_bond_set_param(GAP_PARAM_BOND_OOB_DATA, sizeof(oob_key), oob_key);
	if (cause) {
		return -1;
	}
	cause = le_bond_oob_input_confirm(conn_id, GAP_CFM_CAUSE_ACCEPT);
	if (cause) {
		return -1;
	}
	return 0;
}

int ble_transfer_at_cmd_get_security_param(int argc, char **argv)
{
	T_GAP_CAUSE cause;
	uint16_t auth_flags = 0;
	ble_transfer_module_security_param_t sec_param = {0};

	cause = gap_get_param(GAP_PARAM_BOND_IO_CAPABILITIES, &sec_param.io_cap);
	if (cause) {
		return -1;
	}
	cause = gap_get_param(GAP_PARAM_BOND_OOB_ENABLED, &sec_param.oob_data_flag);
	if (cause) {
		return -1;
	}
	cause = gap_get_param(GAP_PARAM_BOND_AUTHEN_REQUIREMENTS_FLAGS, &auth_flags);
	if (cause) {
		return -1;
	}

	sec_param.bond_flag = auth_flags & 0x1;
	sec_param.mitm_flag = (auth_flags >> 2) & 0x1;
	sec_param.sec_pair_flag = (auth_flags >> 3) & 0x1;
	sec_param.sec_pair_only_flag = (auth_flags >> 9) & 0x1;

	cause = le_bond_get_param(GAP_PARAM_BOND_SEC_REQ_ENABLE, &sec_param.auto_sec_req);
	if (cause) {
		return -1;
	}
	cause = le_bond_get_param(GAP_PARAM_BOND_FIXED_PASSKEY_ENABLE, &sec_param.use_fixed_key);
	if (cause) {
		return -1;
	}
	cause = le_bond_get_param(GAP_PARAM_BOND_FIXED_PASSKEY, &sec_param.fixed_key);
	if (cause) {
		return -1;
	}

	printf("GAP get security paramters success, param: %d,%d,%d,%d,%d,%d,%d,%d,%d\r\n",
			sec_param.io_cap, sec_param.oob_data_flag, sec_param.bond_flag, sec_param.mitm_flag,
			sec_param.sec_pair_flag, sec_param.use_fixed_key, sec_param.fixed_key,
			sec_param.sec_pair_only_flag, sec_param.auto_sec_req);
	at_printf("+BLEGAP:get_sec_param,%d,%d,%d,%d,%d,%d,%d,%d,%d\r\n",
				sec_param.io_cap, sec_param.oob_data_flag, sec_param.bond_flag, sec_param.mitm_flag,
				sec_param.sec_pair_flag, sec_param.use_fixed_key, sec_param.fixed_key,
				sec_param.sec_pair_only_flag, sec_param.auto_sec_req);
	return 0;
}

int ble_transfer_at_cmd_get_bond_info(int argc, char **argv)
{
	(void)argc;
	(void)argv;
	T_LE_KEY_ENTRY *key_entry;
	uint8_t i, bond_size;
	uint8_t writed_size = 0;
	char addr_str[30] = {0};
	char ident_addr_str[30] = {0};

	bond_size = le_get_bond_dev_num();
	if(bond_size == 0) {
		printf("GAP no bond device! \r\n");
		return 0;
	}

	ble_transfer_module_bond_info_t *bond_info =
		(ble_transfer_module_bond_info_t *)osif_mem_alloc(0, bond_size * sizeof(ble_transfer_module_bond_info_t));
	memset(bond_info, 0, bond_size * sizeof(ble_transfer_module_bond_info_t));

	for (i = 0; i < bond_storage_num; i++) {
		key_entry = le_find_key_entry_by_idx(i);
		if (key_entry) {
			bond_info[writed_size].remote_addr.type = (T_GAP_LOCAL_ADDR_TYPE)key_entry->remote_bd.remote_bd_type;
			memcpy(bond_info[writed_size].remote_addr.addr_val, key_entry->remote_bd.addr, BLE_TRANSFER_MODULE_BD_ADDR_LEN);
			bond_info[writed_size].ident_addr.type = (T_GAP_LOCAL_ADDR_TYPE)key_entry->resolved_remote_bd.remote_bd_type;
			memcpy(bond_info[writed_size].ident_addr.addr_val, key_entry->resolved_remote_bd.addr, BLE_TRANSFER_MODULE_BD_ADDR_LEN);
			writed_size++;
		}
		if(writed_size >= bond_size)
			break;
	}
	bond_size = writed_size;
	//printf("GAP get bond info:\r\n");
	at_printf("+BLEGAP:bond_info");
	for (int i = 0; i < bond_size; i++) {
		ble_transfer_module_addr_to_str(&bond_info[i].remote_addr, addr_str, sizeof(addr_str));
		ble_transfer_module_addr_to_str(&bond_info[i].ident_addr, ident_addr_str, sizeof(ident_addr_str));
		//printf("[%d] %s, resolved: %s\r\n", i, addr_str, ident_addr_str);
		at_printf(",%d,%s,%s", i, addr_str, ident_addr_str);
	}
	at_printf("\r\n");

	if (bond_info) {
		osif_mem_free(bond_info);
	}
	return 0;
}

int ble_transfer_at_cmd_delete_bond(int argc, char **argv)
{
	if (argc != 3) {
		printf("GAP delete bond failed! wrong args num!\r\n");
		return -1;
	}
	T_GAP_CAUSE cause;
	T_GAP_REMOTE_ADDR_TYPE type = str_to_int(argv[1]);
	uint8_t addr_val[BLE_TRANSFER_MODULE_BD_ADDR_LEN] = {0};
	
	if(hex_str_to_bd_addr(strlen(argv[2]), ( s8 *)argv[2], (u8*)addr_val) == false) {
		return -1;
	}
	cause = le_bond_delete_by_bd(addr_val, (T_GAP_REMOTE_ADDR_TYPE)type);
	if(cause) {
		return -1;
	}

	return 0;
}

int ble_transfer_at_cmd_clear_bond(int argc, char **argv)
{
	(void)argc;
	(void)argv;

	le_bond_clear_all_keys();
	return 0;
}

int ble_transfer_at_cmd_set_adv_data(int argc, char **argv)
{
	uint8_t adv_data[31] = {0};
	uint16_t adv_len  = 0;
	T_GAP_CAUSE cause;

	if(argc == 1) {
		le_adv_set_param(GAP_PARAM_SCAN_RSP_DATA, sizeof(def_adv_data), def_adv_data);
		printf("GAP set default adv data success\r\n");
		return 0;
	}

	adv_len = strlen(argv[1]) / 2;
	if (adv_len > 31) {
		printf("GAP set adv data failed! too long!\r\n");
		return -1;
	}
	hexdata_str_to_array(argv[1], adv_data, adv_len);
	cause = le_adv_set_param(GAP_PARAM_ADV_DATA, sizeof(adv_data), adv_data);
	if(cause) {
		return -1;
	}
	printf("GAP set adv data success\r\n");
	return 0;
}

int ble_transfer_at_cmd_set_scan_resp(int argc, char **argv)
{
	T_GAP_CAUSE cause;
	uint8_t scan_resp_data[32] = {0};
	uint32_t scan_resp_len  = 0;

	if(argc == 1) {
		cause = le_adv_set_param(GAP_PARAM_SCAN_RSP_DATA, sizeof(def_scan_rsp_data), def_scan_rsp_data);
		if(cause) {
			return -1;
		}
		printf("GAP set default scan rsp success\r\n");
		return 0;
	}

	scan_resp_len = strlen(argv[1]) / 2;
	if (scan_resp_len > 31) {
		printf("GAP set scan resp data failed! too long!\r\n");
		return -1;
	}
	hexdata_str_to_array(argv[1], scan_resp_data, scan_resp_len);
	cause = le_adv_set_param(GAP_PARAM_SCAN_RSP_DATA, scan_resp_len, scan_resp_data);
	if(cause) {
		return -1;
	}
	printf("GAP set scan resp data success\r\n");
	return 0;
}

int ble_transfer_at_cmd_blegap(int argc, char **argv)
{
	int ret = 0;
	if (strcmp("addr", argv[1]) == 0) {
		ret = ble_transfer_at_cmd_get_addr(argc - 1, &argv[1]);
	} else if (strcmp("rand_addr", argv[1]) == 0) {
		ret = ble_transfer_at_cmd_set_rand_addr(argc - 1, &argv[1]);
	} else if (strcmp("mtu_size", argv[1]) == 0) {
		ret = ble_transfer_at_cmd_get_mtu_size(argc - 1, &argv[1]);
	} else if (strcmp("set_mtu", argv[1]) == 0) {
		ret = ble_transfer_at_cmd_set_max_mtu_size(argc - 1, &argv[1]);
	} else if (strcmp("adv", argv[1]) == 0) {
		ret = ble_transfer_at_cmd_op_adv(argc - 1, &argv[1]);
	} else if (strcmp("get_adv_param", argv[1]) == 0) {
		ret = ble_transfer_at_cmd_get_adv_param(argc - 1, &argv[1]);
	} else if (strcmp("scan", argv[1]) == 0) {
		ret = ble_transfer_at_cmd_op_scan(argc - 1, &argv[1]);
	} else if (strcmp("scan_param", argv[1]) == 0) {
		ret = ble_transfer_at_cmd_set_scan_param(argc - 1, &argv[1]);
	} else if (strcmp("get_scan_param", argv[1]) == 0) {
		ret = ble_transfer_at_cmd_get_scan_param(argc - 1, &argv[1]);
	} else if (strcmp("conn", argv[1]) == 0) {
		ret = ble_transfer_at_cmd_connect(argc - 1, &argv[1]);
	} else if (strcmp("disconn", argv[1]) == 0) {
		ret = ble_transfer_at_cmd_disconnect(argc - 1, &argv[1]);
	}else if (strcmp("conn_info", argv[1]) == 0) {
		ret = ble_transfer_at_cmd_get_conn_info(argc - 1, &argv[1]);
	}else if (strcmp("conn_update", argv[1]) == 0) {
		ret = ble_transfer_at_cmd_update_conn_request(argc - 1, &argv[1]);
	}else if (strcmp("wl_add", argv[1]) == 0) {
		ret = ble_transfer_at_cmd_add_whitelist(argc - 1, &argv[1]);
	}else if (strcmp("wl_remove", argv[1]) == 0) {
		ret = ble_transfer_at_cmd_remove_whitelist(argc - 1, &argv[1]);
	}else if (strcmp("wl_clear", argv[1]) == 0) {
		ret = ble_transfer_at_cmd_clear_whitelist(argc - 1, &argv[1]);
	}else if (strcmp("sec_param", argv[1]) == 0) {
		ret = ble_transfer_at_cmd_set_security_param(argc - 1, &argv[1]);
	}else if (strcmp("sec", argv[1]) == 0) {
		ret = ble_transfer_at_cmd_security(argc - 1, &argv[1]);
	}else if (strcmp("pair_cfm", argv[1]) == 0) {
		ret = ble_transfer_at_cmd_confirm_pair(argc - 1, &argv[1]);
	}else if (strcmp("auth_key", argv[1]) == 0) {
		ret = ble_transfer_at_cmd_input_auth_key(argc - 1, &argv[1]);
	}else if (strcmp("auth_keycfm", argv[1]) == 0) {
		ret = ble_transfer_at_cmd_confirm_auth_key(argc - 1, &argv[1]);
	}else if (strcmp("auth_oob", argv[1]) == 0) {
		ret = ble_transfer_at_cmd_input_auth_oob(argc - 1, &argv[1]);
	}else if (strcmp("get_sec_param", argv[1]) == 0) {
		ret = ble_transfer_at_cmd_get_security_param(argc - 1, &argv[1]);
	}else if (strcmp("bond_info", argv[1]) == 0) {
		ret = ble_transfer_at_cmd_get_bond_info(argc - 1, &argv[1]);
	}else if (strcmp("bond_del", argv[1]) == 0) {
		ret = ble_transfer_at_cmd_delete_bond(argc - 1, &argv[1]);
	}else if (strcmp("bond_clear", argv[1]) == 0) {
		ret = ble_transfer_at_cmd_clear_bond(argc - 1, &argv[1]);
	}else if (strcmp("adv_data", argv[1]) == 0) {
		ret = ble_transfer_at_cmd_set_adv_data(argc - 1, &argv[1]);
	}else if (strcmp("scan_rsp", argv[1]) == 0) {
		ret = ble_transfer_at_cmd_set_scan_resp(argc - 1, &argv[1]);
	}else {
		printf("GAP blegap cmd failed, wrong param %s \r\n", argv[1]);
		ret = -1;
	}
	return ret;
}

int ble_transfer_at_cmd_read(int argc, char **argv)
{
	uint8_t conn_id;
	uint16_t conn_handle = str_to_int(argv[1]);
	uint8_t read_type = str_to_int(argv[2]);
	uint8_t handle, start_handle, end_handle, uuid_type;
	uint32_t handle_count = 0;
	uint16_t handles_arr[10] = {0};
	uint8_t uuid[16] = {0}; 
	uint16_t uuid16 = 0;

	le_get_conn_id_by_handle(conn_handle, &conn_id);
	switch(read_type) {
	case BLE_TRANSFER_MODULE_GATT_CHAR_READ_BY_HANDLE:
		if(argc != 4) {
			goto WRONG_ARG_NUM;
		}
		handle = str_to_int(argv[3]);
		if(gcs_attr_read(conn_id, handle)) {
			return -1;
		}
		disc_read_type[conn_id].read_type = BLE_TRANSFER_MODULE_GATT_CHAR_READ_BY_HANDLE;
		break;
	case BLE_TRANSFER_MODULE_GATT_CHAR_READ_BY_UUID:
		if(argc != 6) {
			goto WRONG_ARG_NUM;
		}
		start_handle = str_to_int(argv[3]);
		end_handle = str_to_int(argv[4]);
		if (strlen(argv[5]) == (2 + 4)) {
			uuid_type = 0;
			hexnum_str_to_array(argv[5], (uint8_t *)uuid, 2);
			uuid16 = uuid[1]<<8 | uuid[0];
			if(gcs_attr_read_using_uuid16(conn_id, start_handle, end_handle, uuid16)){
				return -1;
			}
			disc_read_type[conn_id].read_type = BLE_TRANSFER_MODULE_GATT_CHAR_READ_BY_UUID;
		} else if (strlen(argv[5]) == (2 + 16 * 2)) {
			uuid_type = 2;
			hexnum_str_to_array(argv[5], (uint8_t *)uuid, 16);
			if(gcs_attr_read_using_uuid128(conn_id, start_handle, end_handle, uuid)) {
				return -1;
			}
			disc_read_type[conn_id].read_type = BLE_TRANSFER_MODULE_GATT_CHAR_READ_BY_UUID;
		} else {
			printf("GATTC Read failed, wrong uuid!\r\n");
			return -1;
		}
		break;
	case BLE_TRANSFER_MODULE_GATT_CHAR_READ_MULTIPLE:
		printf("GATTC Read_multiple not supported temporarily!\r\n");
		return -1;
	case BLE_TRANSFER_MODULE_GATT_CHAR_READ_MULTIPLE_VARIABLE:
		if (argc < 5) {
			goto WRONG_ARG_NUM;
		}
		handle_count = str_to_int(argv[3]);
		for (uint32_t i = 0; i < handle_count; i++) {
			handles_arr[i] = str_to_int(argv[4 + i]);
		}
		break;
	default:
		printf("GATTC Read failed, wrong type: %d!\r\n", read_type);
		return -1;
		break;
	}

	return 0;

WRONG_ARG_NUM:
	printf("GATTC Read failed: type (%d) with wrong args number!\r\n", read_type);
	return -1;
}

int ble_transfer_at_cmd_write(int argc, char **argv)
{
	if(argc != 6) {
		printf("GATTC write failed! wrong args num!\r\n");
		return -1;
	}
	uint8_t conn_id;
	uint16_t conn_handle = str_to_int(argv[1]);
	uint8_t type = str_to_int(argv[2]);
	uint16_t handle = str_to_int(argv[3]);
	uint16_t length = str_to_int(argv[4]);
	T_GATT_WRITE_TYPE write_type = 0;

	le_get_conn_id_by_handle(conn_handle, &conn_id);

	if(type == BLE_TRANSFER_MODULE_GATT_CHAR_WRITE_REQ) {
		write_type = GATT_WRITE_TYPE_REQ;
	}else if(type == BLE_TRANSFER_MODULE_GATT_CHAR_WRITE_NO_RSP) {
		write_type = GATT_WRITE_TYPE_CMD;
	}else if(type == BLE_TRANSFER_MODULE_GATT_CHAR_WRITE_NO_RSP_SIGNED) {
		write_type = GATT_WRITE_TYPE_SIGNED_CMD;
	}

	if(length != strlen(argv[5]) / 2) {
		goto WRONG_ARG_LEN;
	}

	uint8_t *data = (uint8_t *)osif_mem_alloc(0, length);
	if(!data) {
		printf("GATTC Write failed: cant alloc memory\r\n");
		return -1;
	}
	hexdata_str_to_array(argv[5], (uint8_t *)data, length);
	if(gcs_attr_write(conn_id, write_type, handle, length, data)) {
		osif_mem_free(data);
		return -1;
	}
	osif_mem_free(data);
	return 0;

WRONG_ARG_LEN:
	printf("GATTC Write failed: wrong args length!\r\n");
	return -1;
}

int ble_transfer_at_cmd_discovery(int argc, char **argv)
{
	uint8_t conn_id;
	uint8_t disc_type;
	uint8_t uuid_type;
	uint8_t uuid[16] = {0};
	uint16_t start_handle;
	uint16_t end_handle;
	uint16_t uuid16;

	uint16_t conn_handle = str_to_int(argv[1]);
	disc_type = str_to_int(argv[2]);

	le_get_conn_id_by_handle(conn_handle, &conn_id);

	switch (disc_type) {
	case BLE_TRANSFER_MODULE_GATT_DISCOVER_PRIMARY_ALL:
		if (argc != 3) {
			goto WRONG_ARG_NUM;
		}
		if(gcs_all_primary_srv_discovery(conn_id)) {
			return -1;
		}
		disc_read_type[conn_id].disc_type = BLE_TRANSFER_MODULE_GATT_DISCOVER_PRIMARY_ALL;
		break;
	case BLE_TRANSFER_MODULE_GATT_DISCOVER_PRIMARY_BY_UUID:
		if(argc != 4){
			goto WRONG_ARG_NUM;
		}
		if (strlen(argv[3]) == (2 + 4)) { /* 0xNNNN 2+4 */
			hexnum_str_to_array(argv[3], (uint8_t *)uuid, 2);
			uuid16 = (uuid[1]<<8) | uuid[0];
			if(gcs_by_uuid_srv_discovery(conn_id, uuid16)) {
				return -1;
			}
			disc_read_type[conn_id].disc_type = BLE_TRANSFER_MODULE_GATT_DISCOVER_PRIMARY_BY_UUID;
		} else if (strlen(argv[3]) == (2 + 16 * 2)) { /* 0x001122...ff 2+16*2 */
			hexnum_str_to_array(argv[3], (uint8_t *)uuid, 16);
			if(gcs_by_uuid128_srv_discovery(conn_id, uuid)) {
				return -1;
			}
			disc_read_type[conn_id].disc_type = BLE_TRANSFER_MODULE_GATT_DISCOVER_PRIMARY_BY_UUID;
		} else {
			printf("GATTC Discover failed, wrong uuid!\r\n");
			return -1;
		}
		break;
	case BLE_TRANSFER_MODULE_GATT_DISCOVER_INCLUDE:
		if(argc != 5){
			goto WRONG_ARG_NUM;
		}
		start_handle = str_to_int(argv[3]);
		end_handle = str_to_int(argv[4]);
		if(gcs_include_srv_discovery(conn_id, start_handle, end_handle)) {
				return -1;
		}
		disc_read_type[conn_id].disc_type = BLE_TRANSFER_MODULE_GATT_DISCOVER_INCLUDE;
		break;
	case BLE_TRANSFER_MODULE_GATT_DISCOVER_CHARACTERISTIC_ALL:
		if(argc != 5){
			goto WRONG_ARG_NUM;
		}
		start_handle = str_to_int(argv[3]);
		end_handle = str_to_int(argv[4]);
		if(gcs_all_char_discovery(conn_id, start_handle, end_handle)) {
				return -1;
		}
		disc_read_type[conn_id].disc_type = BLE_TRANSFER_MODULE_GATT_DISCOVER_CHARACTERISTIC_ALL;
		break;
	case BLE_TRANSFER_MODULE_GATT_DISCOVER_CHARACTERISTIC_BY_UUID:
		if(argc != 6){
			goto WRONG_ARG_NUM;
		}
		start_handle = str_to_int(argv[3]);
		end_handle = str_to_int(argv[4]);
		if (strlen(argv[5]) == (2 + 4)) { /* 0xNNNN 2+4 */
			hexnum_str_to_array(argv[5], (uint8_t *)uuid, 2);
			uuid16 = (uuid[1]<<8) | uuid[0];
			if(gcs_by_uuid_char_discovery(conn_id, start_handle, end_handle, uuid16)) {
				return -1;
			}
			disc_read_type[conn_id].disc_type = BLE_TRANSFER_MODULE_GATT_DISCOVER_CHARACTERISTIC_BY_UUID;
		} else if (strlen(argv[5]) == (2 + 16 * 2)) { /* 0x001122...ff 2+16*2 */
			hexnum_str_to_array(argv[5], (uint8_t *)uuid, 16);
			if(gcs_by_uuid128_char_discovery(conn_id, start_handle, end_handle, uuid)) {
				return -1;
			}
			disc_read_type[conn_id].disc_type = BLE_TRANSFER_MODULE_GATT_DISCOVER_CHARACTERISTIC_BY_UUID;
		} else {
			printf("GATTC Discover failed, wrong uuid!\r\n");
			return -1;
		}
		break;
	case BLE_TRANSFER_MODULE_GATT_DISCOVER_DESCRIPTORS_ALL:
		if(argc != 5){
			goto WRONG_ARG_NUM;
		}
		start_handle = str_to_int(argv[3]);
		end_handle = str_to_int(argv[4]);
		if(gcs_all_char_descriptor_discovery(conn_id, start_handle, end_handle)) {
				return -1;
		}
		disc_read_type[conn_id].disc_type = BLE_TRANSFER_MODULE_GATT_DISCOVER_DESCRIPTORS_ALL;
		break;
	default:
		printf("GATTC Discover failed, wrong type: %d!\r\n", disc_type);
		return -1;
		break;
	}

	return 0;
WRONG_ARG_NUM:
	printf("GATTC Discover failed: type (%d) with wrong args number!\r\n", disc_type);
	return -1;
}

int ble_transfer_at_cmd_blegattc(int argc, char **argv)
{
	int ret = 0;
	if (strcmp("read", argv[1]) == 0) {
		ret = ble_transfer_at_cmd_read(argc - 1, &argv[1]);
	}else if (strcmp("write", argv[1]) == 0) {
		ret = ble_transfer_at_cmd_write(argc - 1, &argv[1]);
	}else if (strcmp("disc", argv[1]) == 0) {
		ret = ble_transfer_at_cmd_discovery(argc - 1, &argv[1]);
	}else {
		printf("GAP blegattc cmd failed, wrong param %s \r\n", argv[1]);
		ret = -1;
	}
	return ret;
}

int ble_transfer_at_cmd_notify(int argc, char **argv)
{
	if(argc != 6) {
		printf("GATTC indicate failed! wrong args num!\r\n");
		return -1;
	}

	uint8_t conn_id;
	uint16_t conn_handle = str_to_int(argv[1]);
	uint16_t app_id = str_to_int(argv[2]);
	uint16_t attrib_index = str_to_int(argv[3]);
	int length = str_to_int(argv[4]);
	int data_count;
	uint8_t service_id = 0;

	le_get_conn_id_by_handle(conn_handle, &conn_id);

	if (app_id == TRANSFER_MODULE_APP_ID) {
		service_id = 1;
	} else {
		printf("ble transfer module notify failed, wrong app id!");
	}

	uint8_t *data = (uint8_t *)os_mem_alloc(0, length * sizeof(uint8_t));

	hexdata_str_to_array(argv[5], (uint8_t *)data, length);

	if(!server_send_data(conn_id, service_id, attrib_index, data, length, GATT_PDU_TYPE_NOTIFICATION)) {
		os_mem_free(data);
		return -1;
	}

	if (data != NULL)
		os_mem_free(data);

	return 0;
}

int ble_transfer_at_cmd_indicate(int argc, char **argv)
{
	if(argc != 6) {
		printf("GATTC indicate failed! wrong args num!\r\n");
		return -1;
	}

	uint8_t conn_id;
	uint16_t conn_handle = str_to_int(argv[1]);
	uint16_t app_id = str_to_int(argv[2]);
	uint16_t attrib_index = str_to_int(argv[3]);
	int length = str_to_int(argv[4]);
	int data_count;
	uint8_t service_id = 0;

	le_get_conn_id_by_handle(conn_handle, &conn_id);

	if (app_id == TRANSFER_MODULE_APP_ID) {
		service_id = 1;
	} else {
		printf("ble transfer module indicate failed, wrong app id!");
	}

	uint8_t *data = (uint8_t *)os_mem_alloc(0, length * sizeof(uint8_t));

	hexdata_str_to_array(argv[5], (uint8_t *)data, length);

	if(!server_send_data(conn_id, service_id, attrib_index, data, length, GATT_PDU_TYPE_INDICATION)) {
		os_mem_free(data);
		return -1;
	}

	if (data != NULL)
		os_mem_free(data);

	return 0;
}

int ble_transfer_at_cmd_blegatts(int argc, char **argv)
{
	int ret = 0;
	if (strcmp("notify", argv[1]) == 0) {
		ret = ble_transfer_at_cmd_notify(argc - 1, &argv[1]);
	}else if (strcmp("indicate", argv[1]) == 0) {
		ret = ble_transfer_at_cmd_indicate(argc - 1, &argv[1]);
	}else {
		printf("GAP blegatts cmd failed, wrong param %s \r\n", argv[1]);
		ret = -1;
	}
	return ret;
}

int ble_transfer_parse_param(char *buf, char **argv)
{
	int argc = 1;
	char str_buf[256];
	memset(str_buf, 0, 256);
	int str_count = 0;
	int buf_cnt = 0;
	static char temp_buf[256];
	char *buf_pos = temp_buf;
	memset(temp_buf, 0, sizeof(temp_buf));

	if(buf == NULL)
		goto exit;
	strncpy(temp_buf, buf, sizeof(temp_buf));

	while((argc < MAX_ARGC) && (*buf_pos != '\0')) {
		while((*buf_pos == ',') || (*buf_pos == '[') || (*buf_pos == ']')){
			if((*buf_pos == ',') && (*(buf_pos+1) == ',')){
				argv[argc] = NULL;
				argc++;
			}
			*buf_pos = '\0';
			buf_pos++;
		}

		if(*buf_pos == '\0')
			break;
		else if(*buf_pos == '"'){
			memset(str_buf,'\0',256);
			str_count = 0;
			buf_cnt = 0;
			*buf_pos = '\0';
			buf_pos ++;
			if(*buf_pos == '\0')
			break;
			argv[argc] = buf_pos;
			while((*buf_pos != '"')&&(*buf_pos != '\0')){
				if(*buf_pos == '\\'){
				buf_pos ++;
					buf_cnt++;
				}
				str_buf[str_count] = *buf_pos;
				str_count++;
				buf_cnt++;
				buf_pos ++;
			}
			*buf_pos = '\0';
			memcpy(buf_pos-buf_cnt,str_buf,buf_cnt);
		}
		else{
			argv[argc] = buf_pos;
		}
		argc++;
		buf_pos++;

		while( (*buf_pos != ',')&&(*buf_pos != '\0')&&(*buf_pos != '[')&&(*buf_pos != ']') )
			buf_pos++;
	}
exit:
	return argc;
}
#endif

int ble_transfer_app_handle_at_cmd(uint16_t subtype, void *arg)
{
#if SUPPORT_LOG_SERVICE
	int argc = 0;
	int ret = 0;
	char *argv[MAX_ARGC] = {0};

	if (arg) {
		argc = ble_transfer_parse_param(arg, argv);
	}

	switch (subtype) {
		case BT_ATCMD_BLEGAP:
			ret = ble_transfer_at_cmd_blegap(argc, argv);
			break;
		case BT_ATCMD_BLEGATTC:
			ret = ble_transfer_at_cmd_blegattc(argc, argv);
			break;
		case BT_ATCMD_BLEGATTS:
			ret = ble_transfer_at_cmd_blegatts(argc, argv);
			break;
		default:
			break;
	}
	if(ret == 0) {
		at_printf("OK\r\n");
	}else {
		at_printf("ERROR\r\n");
	}
	return 0;
#else
	return 0;
#endif
}
#endif
