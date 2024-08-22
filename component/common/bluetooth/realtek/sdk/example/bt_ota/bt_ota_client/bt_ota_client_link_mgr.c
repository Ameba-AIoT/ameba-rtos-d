/**
*****************************************************************************************
*     Copyright(c) 2017, Realtek Semiconductor Corporation. All rights reserved.
*****************************************************************************************
   * @file      link_mgr.c
   * @brief     Multilink manager functions.
   * @author    jane
   * @date      2017-06-06
   * @version   v1.0
   **************************************************************************************
   * @attention
   * <h2><center>&copy; COPYRIGHT 2017 Realtek Semiconductor Corporation</center></h2>
   **************************************************************************************
  */
/*============================================================================*
 *                              Header Files
 *============================================================================*/
#include <platform_opts_bt.h>
#if defined(CONFIG_BT_OTA_CLIENT) && CONFIG_BT_OTA_CLIENT
#include <bt_ota_client_link_mgr.h>
#include <trace_app.h>
#include <string.h>


/*============================================================================*
 *                              Variables
 *============================================================================*/
/** @addtogroup  CENTRAL_CLIENT_GAP_MSG
    * @{
    */
T_APP_LINK bt_ota_client_app_link_table[BT_OTA_CLIENT_APP_MAX_LINKS];
/** @} */

/** @addtogroup  CENTRAL_SCAN_MGR
    * @{
    */
T_DEV_INFO bt_ota_client_dev_list[BT_OTA_CLIENT_APP_MAX_DEVICE_INFO];
uint8_t bt_ota_client_dev_list_count = 0;
/** @} */
/*============================================================================*
 *                              Functions
 *============================================================================*/
/** @addtogroup  CENTRAL_SCAN_MGR
	* @{
	*/
/**
 * @brief	Add device information to device list.
 *
 * @param[in] bd_addr Peer device address.
 * @param[in] bd_type Peer device address type.
 * @retval true Success.
 * @retval false Failed, device list is full.
 */
bool bt_ota_client_link_mgr_add_device(uint8_t *bd_addr, uint8_t bd_type)
{
	/* If result count not at max */
	if (bt_ota_client_dev_list_count < BT_OTA_CLIENT_APP_MAX_DEVICE_INFO)
	{
		uint8_t i;
		/* Check if device is already in device list*/
		for (i = 0; i < bt_ota_client_dev_list_count; i++)
		{
			if (memcmp(bd_addr, bt_ota_client_dev_list[i].bd_addr, GAP_BD_ADDR_LEN) == 0)
			{
				return true;
			}
		}

		/*Add addr to device list list*/
		memcpy(bt_ota_client_dev_list[bt_ota_client_dev_list_count].bd_addr, bd_addr, GAP_BD_ADDR_LEN);
		bt_ota_client_dev_list[bt_ota_client_dev_list_count].bd_type = bd_type;

		/*Increment device list count*/
		bt_ota_client_dev_list_count++;
	}
	else
	{
		return false;
	}
	return true;
}

/**
 * @brief Clear device list.
 * @retval None.
 */
void bt_ota_client_link_mgr_clear_device_list(void)
{
    bt_ota_client_dev_list_count = 0;
}
/** @} */
#endif
