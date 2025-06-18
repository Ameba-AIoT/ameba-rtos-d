#ifndef __ATCMD_BT_H__
#define __ATCMD_BT_H__

#include <platform_opts.h>

#if defined(CONFIG_EXAMPLE_UART_ATCMD) && CONFIG_EXAMPLE_UART_ATCMD
#include "atcmd_wifi.h"
#define BT_AT_SYNC 1
#endif

#define CENTRAL_BIT         0x80
#define PERIPHERAL_BIT      0x40
#define SCATTERNET_BIT      0x20
#define BEACON_BIT          0x10
#define CONFIG_BIT          0x08
#define AIRSYNC_CONFIG_BIT  0x04
#define MESH_BIT            0x02
#define STACK_BIT           0x01

#if (defined(BT_AT_SYNC) && BT_AT_SYNC)
#define BT_AT_PRINT(fmt, args...)             at_printf(fmt, ##args)
#else
#define BT_AT_PRINT(fmt, args...)
#endif /* BT_AT_SYNC */

typedef enum
{
	BT_ATCMD_SCAN,
	BT_ATCMD_CONNECT,
	BT_ATCMD_DISCONNECT,
	BT_ATCMD_AUTH,
	BT_ATCMD_GET,
	BT_ATCMD_GET_COON_INFO,
	BT_ATCMD_SEND_USERCONF,
	BT_ATCMD_UPDATE_CONN_REQUEST,
	BT_ATCMD_BOND_INFORMATION,
	BT_ATCMD_READ,
	BT_ATCMD_WRITE,
	BT_ATCMD_MODIFY_WHITELIST,
	BT_ATCMD_SET_SCAN_PARAM,
	BT_ATCMD_SET_PHY,
	BT_ATCMD_SEND_INDI_NOTI,
	BT_ATCMD_BLEGAP,
	BT_ATCMD_BLEGATTC,
	BT_ATCMD_BLEGATTS,
	BT_ATCMD_BLE_WIFIMATE_CONFIGURATOR,
	BT_ATCMD_MAX
} BT_AT_CMD_TYPE;

typedef enum
{
	BT_COMMAND_CENTRAL,
	BT_COMMAND_PERIPHERAL,
	BT_COMMAND_SCATTERNET,
	BT_COMMAND_BEACON,
	BT_COMMAND_CONFIG,
	BT_COMMAND_AIRSYNC_CONFIG,
	BT_COMMAND_MESH,
	BT_COMMAND_STACK,
	BT_COMMAND_SCAN,
	BT_COMMAND_CONNECT,
	BT_COMMAND_DISCONNECT,
	BT_COMMAND_AUTH,
	BT_COMMAND_GET,
	BT_COMMAND_GET_CONN_INFO,
	BT_COMMAND_SEND_USERCONF,
	BT_COMMAND_UPDATE_CONN_REQUEST,
	BT_COMMAND_BOND_INFORMATION,
	BT_COMMAND_READ,
	BT_COMMAND_WRITE,
	BT_COMMAND_MODIFY_WHITELIST,
	BT_COMMAND_SET_SCAN_PARAM,
	BT_COMMAND_SET_PHY,
	BT_COMMAND_MODIFY_ADV_INTERVAL,
	BT_COMMAND_SEND_INDI_NOTI
}BT_COMMAND_TYPE;

typedef enum {
	BT_AT_OK                       =   0x00,   /*!< 0, means success */
	BT_AT_FAIL                     =   0x01,   /*!< 1, means common failure */
	BT_AT_ERR_NOT_READY            =   0x02,   /*!< 2 */
	BT_AT_ERR_NO_MEMORY            =   0x03,   /*!< 3 */
	BT_AT_ERR_OS_OPERATION         =   0x04,   /*!< 4 */
	BT_AT_ERR_PARAM_INVALID        =   0x05,   /*!< 5 */
	BT_AT_ERR_ADV_LENGTH_INVALID   =   0x06,   /*!< 6 */
	BT_AT_ERR_NO_CONNECTION        =   0x07,   /*!< 7 */
	BT_AT_ERR_IRK_NOT_FOUND        =   0x08,   /*!< 8 */
	BT_AT_ERR_NO_BOND              =   0x09,   /*!< 9 */
	BT_AT_ERR_LOWER_STACK          =   0x0A,   /*!< 10 */
	BT_AT_ERR_CREATE_CONN_TIMEOUT  =   0x0B,   /*!< 11 */
	BT_AT_ERR_CMD_INVALID          =   0x0C,   /*!< 12 */
	BT_AT_ERR_CMD_TIMEOUT          =   0x0D,   /*!< 13 */
} bt_at_err_e;

#if (defined(BT_AT_SYNC) && BT_AT_SYNC)
#define BT_AT_SYNC_CONN_HANDLE_INVALID      (-1)

enum bt_at_sync_op_type_e {
	BT_AT_SYNC_OP_TYPE_NONE = 0,
	BT_AT_SYNC_OP_TYPE_START,
	BT_AT_SYNC_OP_TYPE_STOP,
	BT_AT_SYNC_OP_TYPE_MAX,
};

enum bt_at_sync_cmd_type_e {
	BT_AT_SYNC_CMD_TYPE_NONE = 0,
	BT_AT_SYNC_CMD_TYPE_BLE_GAP_ADV,
	BT_AT_SYNC_CMD_TYPE_BLE_GAP_SCAN,
	BT_AT_SYNC_CMD_TYPE_BLE_GAP_CONN,
	BT_AT_SYNC_CMD_TYPE_BLE_GAP_DISCONN,
	BT_AT_SYNC_CMD_TYPE_BLE_GAP_CONN_UPDATE,
	BT_AT_SYNC_CMD_TYPE_BLE_GAP_BOND_DEL,
	BT_AT_SYNC_CMD_TYPE_BLE_GAP_BOND_CLEAR,
	BT_AT_SYNC_CMD_TYPE_BLE_GAP_WL_ADD,
	BT_AT_SYNC_CMD_TYPE_BLE_GAP_WL_REMOVE,
	BT_AT_SYNC_CMD_TYPE_BLE_GAP_WL_CLEAR,
	BT_AT_SYNC_CMD_TYPE_BLE_GATTC_DISC,
	BT_AT_SYNC_CMD_TYPE_BLE_GATTC_READ,
	BT_AT_SYNC_CMD_TYPE_BLE_GATTC_WRITE,
	BT_AT_SYNC_CMD_TYPE_BLE_GATTS_NOTIFY,
	BT_AT_SYNC_CMD_TYPE_BLE_GATTS_INDICATE,
	BT_AT_SYNC_CMD_TYPE_BLE_WIFIMATE_CONFIGURATOR_WIFI_SCAN,
	BT_AT_SYNC_CMD_TYPE_BLE_WIFIMATE_CONFIGURATOR_WIFI_CONNECT,
	BT_AT_SYNC_CMD_TYPE_BLE_WIFIMATE_CONFIGURATOR_ENCRYPT_SET,
	BT_AT_SYNC_CMD_TYPE_OTHER,
	BT_AT_SYNC_CMD_TYPE_MAX,
};

enum bt_at_evt_result_e {
	BT_AT_EVT_RESULT_SUCCESS = 0,
	BT_AT_EVT_RESULT_FAIL = 1,
};

struct bt_at_sync_t {
	uint16_t  exec;
	uint16_t  cmd_type;
	uint16_t  result;
	int32_t   conn_handle;
	void      *psem;
};

uint16_t bt_at_sync_init(uint16_t group, const char* cmd_str);

uint16_t bt_at_sync_sem_take(void);

void bt_at_sync_sem_give(void);

uint16_t bt_at_sync_get_result(void);

void bt_at_sync_set_result(int res);

void bt_at_sync_set_result_by_evt_cause(int res);

void bt_at_sync_deinit(void);

uint8_t bt_at_sync_event_match_check(uint8_t cmd_type);

void bt_at_sync_disconnect_hdl(uint16_t conn_handle);

uint16_t bt_at_sync_sem_wait_check();

void bt_at_sync_update_conn_hdl(uint16_t conn_hdl);

#endif /* BT_AT_SYNC */

#endif  /* __ATCMD_BT_H__ */

