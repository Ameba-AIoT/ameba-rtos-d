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
#ifndef _BLE_CENTRAL_LINK_MANAGER_H_
#define _BLE_CENTRAL_LINK_MANAGER_H_
/*============================================================================*
 *                              Header Files
 *============================================================================*/
#include <app_msg.h>
#include <gap_conn_le.h>
#include <ble_central_app_flags.h>

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
#if F_BT_LE_USE_RANDOM_ADDR
typedef struct
{
	uint8_t 	 is_exist;
	uint8_t 	 reserved;		   /**< remote BD type*/
	uint8_t 	 bd_addr[GAP_BD_ADDR_LEN];	/**< remote BD */
} T_APP_STATIC_RANDOM_ADDR;
#endif

/*============================================================================*
 *                              Variables
 *============================================================================*/
/** @brief  App link table */
extern T_APP_LINK ble_central_app_link_table[BLE_CENTRAL_APP_MAX_LINKS];

/*============================================================================*
 *                              Functions
 *============================================================================*/
#if F_BT_LE_USE_RANDOM_ADDR
uint32_t ble_central_app_save_static_random_address(T_APP_STATIC_RANDOM_ADDR *p_addr);
uint32_t ble_central_app_load_static_random_address(T_APP_STATIC_RANDOM_ADDR *p_addr);
#endif
#endif
