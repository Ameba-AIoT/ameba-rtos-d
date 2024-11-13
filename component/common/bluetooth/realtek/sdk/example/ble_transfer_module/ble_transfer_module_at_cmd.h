#ifndef _BLE_TRANSFER_MODULE_AT_CMD_H_
#define _BLE_TRANSFER_MODULE_AT_CMD_H_

#ifdef  __cplusplus
extern "C" {
#endif      /* __cplusplus */

#include "atcmd_wifi.h"
#include "gap_le_types.h"

/* 0x11223344...\r\n */
#define BT_AT_DUMP_HEXN(buf, len)             bt_at_iouart_dump_hex("0x", buf, len, false, "\r\n")
/* 0x11223344... */
#define BT_AT_DUMP_HEX(buf, len)              bt_at_iouart_dump_hex("0x", buf, len, false, "")
/* 01,02,0a,0b ...*/
#define BT_AT_DUMP(str, buf, len)             bt_at_iouart_dump(1, str, buf, len)
/* 0201,0b0a ...*/
#define BT_AT_DUMP16(str, buf, len)           bt_at_iouart_dump(2, str, buf, len)

#define LE_TO_U32(_a)                                   \
        (((uint32_t)(*((uint8_t *)(_a) + 0)) << 0)  |   \
         ((uint32_t)(*((uint8_t *)(_a) + 1)) << 8)  |   \
         ((uint32_t)(*((uint8_t *)(_a) + 2)) << 16) |   \
         ((uint32_t)(*((uint8_t *)(_a) + 3)) << 24))

#define LE_TO_U16(_a)                           \
        (((uint16_t)(*((uint8_t *)(_a) + 0)) << 0)  |   \
         ((uint16_t)(*((uint8_t *)(_a) + 1)) << 8))

#define BLE_TRANSFER_MODULE_BD_ADDR_LEN		6
/**
 * @struct    ble_transfer_module_le_addr_t
 * @brief     Bluetooth LE device address definition.
 */
typedef struct {
	T_GAP_LOCAL_ADDR_TYPE type;                 /*!< address type */
	uint8_t addr_val[BLE_TRANSFER_MODULE_BD_ADDR_LEN];          /*!< address */
} ble_transfer_module_le_addr_t;

typedef enum {
	BLE_TRANSFER_MODULE_STATUS_DONE = 0,
	BLE_TRANSFER_MODULE_STATUS_CONTINUE,
	BLE_TRANSFER_MODULE_STATUS_FAIL,
} ble_transfer_module_status_t;

typedef enum {
	BLE_TRANSFER_MODULE_GATT_DISCOVER_PRIMARY_ALL,           /*!< Discover all primary services */
	BLE_TRANSFER_MODULE_GATT_DISCOVER_PRIMARY_BY_UUID,       /*!< Discover primary services By service UUID */
	BLE_TRANSFER_MODULE_GATT_DISCOVER_INCLUDE,               /*!< Find included services */
	BLE_TRANSFER_MODULE_GATT_DISCOVER_CHARACTERISTIC_ALL,    /*!< Discover all characteristic of a service */
	BLE_TRANSFER_MODULE_GATT_DISCOVER_CHARACTERISTIC_BY_UUID,/*!< Discover characteristic by UUID */
	BLE_TRANSFER_MODULE_GATT_DISCOVER_DESCRIPTORS_ALL,       /*!< Discover all characteristic descriptors */
} ble_transfer_module_gattc_discover_type_t;

typedef enum {
	BLE_TRANSFER_MODULE_GATT_CHAR_READ_BY_HANDLE,            /*!< Read characteristic value */
	BLE_TRANSFER_MODULE_GATT_CHAR_READ_BY_UUID,              /*!< Read using characteristic UUID */
	BLE_TRANSFER_MODULE_GATT_CHAR_READ_MULTIPLE,             /*!< Read multiple characteristic values */
	BLE_TRANSFER_MODULE_GATT_CHAR_READ_MULTIPLE_VARIABLE,    /*!< Read multiple variable characteristic values */
} ble_transfer_module_gattc_read_type_t;

/**
 * @struct    ble_transfer_module_le_addr_t
 * @brief     Bluetooth LE device address definition.
 */
typedef struct {
	ble_transfer_module_gattc_discover_type_t disc_type;                 /*!< address type */
	ble_transfer_module_gattc_read_type_t read_type;          /*!< address */
} ble_transfer_module_disc_read_type_t;

typedef struct {
	/**
	 * Minimum advertising interval for undirected and low duty cycle\n
	 * directed advertising.\n
	 * Range: 0x0020 to 0x4000\n
	 * Default: 0x0800 (1.28 s)\n
	 * Time = N * 0.625 ms\n
	 * Time Range: 20 ms to 10.24 s
	 */
	uint16_t interval_min;
	/**
	 * Maximum advertising interval for undirected and low duty cycle\n
	 * directed advertising.\n
	 * Range: 0x0020 to 0x4000\n
	 * Default: 0x0800 (1.28 s)\n
	 * Time = N * 0.625 ms\n
	 * Time Range: 20 ms to 10.24 s
	 */
	uint16_t interval_max;
	/** Advertising_Type */
	T_GAP_ADTYPE type;
	/** Own_Address_Type */
	T_GAP_LOCAL_ADDR_TYPE own_addr_type;
	/** Peer_Address */
	ble_transfer_module_le_addr_t peer_addr;
	/** Advertising_Channel_Map */
	uint8_t channel_map;
	/** Advertising_Filter_Policy */
	T_GAP_ADV_FILTER_POLICY filter_policy;
} ble_transfer_module_adv_param_t;

/**
 * @struct    ble_transfer_module_scan_param_t
 * @brief     Bluetooth LE GAP scan paramters definition.
 */
typedef struct {
	/** LE_Scan_Type */
	T_GAP_SCAN_MODE type;
	/**
	 * This is defined as the time interval from when the Controller\n
	 * started its last LE scan until it begins the subsequent LE scan.\n
	 * Range: 0x0004 to 0x4000\n
	 * Default: 0x0010 (10 ms)\n
	 * Time = N * 0.625 ms\n
	 * Time Range: 2.5 ms to 10.24 s\n
	 */
	uint16_t interval;
	/**
	 * The duration of the LE scan. LE_Scan_Window shall be less\n
	 * than or equal to LE_Scan_Interval\n
	 * Range: 0x0004 to 0x4000\n
	 * Default: 0x0010 (10 ms)\n
	 * Time = N * 0.625 ms\n
	 * Time Range: 2.5 ms to 10.24 s\n
	 */
	uint16_t window;
	/** Own_Address_Type */
	T_GAP_LOCAL_ADDR_TYPE own_addr_type;
	/** Scanning_Filter_Policy */
	T_GAP_SCAN_FILTER_POLICY filter_policy;
	/** Scanning_Filter_Duplicated_Option */
	uint8_t duplicate_opt;
} ble_transfer_module_scan_param_t;

/**
 * @struct    ble_transfer_module_security_param_t
 * @brief     Bluetooth BLE SM security parameter type definition.
 */
typedef struct {
	T_GAP_IO_CAP io_cap;                    /*!< IO capabilities */
	uint8_t oob_data_flag;                  /*!< OOB data flag */
	uint8_t bond_flag;                      /*!< Bonding flags */
	uint8_t mitm_flag;                      /*!< MITM flag */
	uint8_t sec_pair_flag;                  /*!< Secure connection pairing support flag */
	uint8_t sec_pair_only_flag;             /*!< Only accept secure connection pairing when local sec_pair_flag is set */
	uint8_t use_fixed_key;                  /*!< Pairing use fixed passkey */
	uint32_t fixed_key;                     /*!< Fixed passkey value */
	uint8_t auto_sec_req;                   /*!< Auto send security request when connected */
} ble_transfer_module_security_param_t;

typedef struct {
	T_GAP_IDENT_ADDR_TYPE type;       /*!< Identity address type */
	uint8_t addr_val[BLE_TRANSFER_MODULE_BD_ADDR_LEN];      /*!< Identity address value */
} ble_transfer_module_ident_addr_t;

typedef struct {
	ble_transfer_module_le_addr_t remote_addr;           /*!< Bonded device address */
	ble_transfer_module_ident_addr_t ident_addr;      /*!< Identity address */
} ble_transfer_module_bond_info_t;

typedef enum {
	BLE_TRANSFER_MODULE_GATT_CHAR_WRITE_REQ = 0x00,          /*!< Write characteristic value request */
	BLE_TRANSFER_MODULE_GATT_CHAR_WRITE_NO_RSP,              /*!< Write characteristic value without response */
	BLE_TRANSFER_MODULE_GATT_CHAR_WRITE_NO_RSP_SIGNED,       /*!< Write characteristic value without response and with signed data */
} ble_transfer_module_gattc_write_type_t;

int ble_transfer_at_cmd_scan(int argc, char **argv);
int ble_transfer_app_handle_at_cmd(uint16_t subtype, void *arg);

#ifdef  __cplusplus
}
#endif      /*  __cplusplus */

#endif

