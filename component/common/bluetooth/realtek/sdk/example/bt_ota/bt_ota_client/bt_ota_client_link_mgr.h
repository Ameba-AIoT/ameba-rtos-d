/**
*****************************************************************************************
*     Copyright(c) 2017, Realtek Semiconductor Corporation. All rights reserved.
*****************************************************************************************
   * @file      link_mgr.h
   * @brief     Define multilink manager struct and functions.
   * @author    jane
   * @date      2017-06-06
   * @version   v1.0
   **************************************************************************************
   * @attention
   * <h2><center>&copy; COPYRIGHT 2017 Realtek Semiconductor Corporation</center></h2>
   **************************************************************************************
  */
#ifndef _BT_OTA_CLIENT_LINK_MANAGER_H_
#define _BT_OTA_CLIENT_LINK_MANAGER_H_
/*============================================================================*
 *                              Header Files
 *============================================================================*/
#include <app_msg.h>
#include <gap_conn_le.h>
#include <bt_ota_client_app_flags.h>


#define BT_OTA_CLIENT_APP_MAX_DEVICE_INFO 6

/*============================================================================*
 *                              Constants
 *============================================================================*/
/** @addtogroup  CENTRAL_CLIENT_GAP_MSG
    * @{
    */
/**
 * @brief  Application Link control block defination.
 */
typedef struct
{
	T_GAP_CONN_STATE        conn_state;          /**< Connection state. */
	T_GAP_REMOTE_ADDR_TYPE  bd_type;             /**< remote BD type*/
	uint8_t                 bd_addr[GAP_BD_ADDR_LEN]; /**< remote BD */
	T_GAP_ROLE              role;                   //!< Device role
} T_APP_LINK;
/** @} */ /* End of group CENTRAL_CLIENT_GAP_MSG */

/** @addtogroup  CENTRAL_CLIENT_SCAN_MGR
    * @{
    */
/**
 * @brief  Device list block definition.
 */
typedef struct
{
    uint8_t      bd_addr[GAP_BD_ADDR_LEN];  /**< remote BD */
    uint8_t      bd_type;              /**< remote BD type*/
} T_DEV_INFO;
/** @} */
/*============================================================================*
 *                              Variables
 *============================================================================*/
/** @brief  App link table */
extern T_APP_LINK bt_ota_client_app_link_table[BT_OTA_CLIENT_APP_MAX_LINKS];

/*============================================================================*
 *                              Functions
 *============================================================================*/
bool bt_ota_client_link_mgr_add_device(uint8_t *bd_addr, uint8_t bd_type);
void bt_ota_client_link_mgr_clear_device_list(void);

#endif
