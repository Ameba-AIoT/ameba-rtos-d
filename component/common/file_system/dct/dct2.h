/**
  ******************************************************************************
  * @file    dct.h
  * @author
  * @version
  * @brief   Device Configuration Table API for storing information in NVRAM(flash).
  ******************************************************************************
  * @attention
  *
  * This module is a confidential and proprietary property of RealTek and possession or use of this module requires written permission of RealTek.
  *
  * Copyright(c) 2016, Realtek Semiconductor Corporation. All rights reserved.
  ******************************************************************************
  */
#ifndef __RTK_DCT2_H__
#define __RTK_DCT2_H__

#include <platform_stdlib.h>
#include <osdep_service.h>
#include <dct.h>

/**
 * @brief      Format device configuration table.
 * @param[in]  begin_address : DCT begin address of flash
 * @param[in]  module_number : total module number
 * @param[in]  variable_name_size : size of variable name
 * @param[in]  variable_value_size : size of variable size
 * @param[in]  enable_backup : enable backup function to backup module, it need double module size
 * @param[in]  enable_wear_leveling : enable wear leveling function, it need sextuple module size
 * @return     0  : SUCCESS
 * @return     <0 : ERROR
 */
int32_t dct_format2(uint32_t begin_address, uint16_t module_number, uint16_t variable_name_size, uint16_t variable_value_size, uint8_t enable_backup,
					uint8_t enable_wear_leveling);

/**
 * @brief      Initialize device configuration table.
 * @param[in]  begin_address : DCT begin address of flash
 * @param[in]  module_number : total module number
 * @param[in]  variable_name_size : size of variable name
 * @param[in]  variable_value_size : size of variable size
 * @param[in]  enable_backup : enable backup function to backup module, it need double module size
 * @param[in]  enable_wear_leveling : enable wear leveling function, it need sextuple module size
 * @return     0  : SUCCESS
 * @return     <0 : ERROR
 */
int32_t dct_init2(uint32_t begin_address, uint16_t module_number, uint16_t variable_name_size, uint16_t variable_value_size, uint8_t enable_backup,
				  uint8_t enable_wear_leveling);

/**
 * @brief      Deinitialize device configuration table.
 */
void dct_deinit2(void);

/**
 * @brief      Register module in DCT.
 * @param[in]  module_name : module name
 * @return     0  : SUCCESS
 * @return     <0 : ERROR
 */
int32_t dct_register_module2(char *module_name);

/**
 * @brief      Unregister and delete module in DCT.
 * @param[in]  module_name : module name
 * @return     0  : SUCCESS
 * @return     <0 : ERROR
 */
int32_t dct_unregister_module2(char *module_name);

/**
 * @brief      Open module in DCT.
 * @param[out] dct_handle : setup module informations in dct handler
 * @param[in]  module_name : module name
 * @return     0  : SUCCESS
 * @return     <0 : ERROR
 */
int32_t dct_open_module2(dct_handle_t *dct_handle, char *module_name);

/**
 * @brief      Close module in DCT.
 * @param[in]  dct_handle : dct handler
 * @return     0  : SUCCESS
 * @return     <0 : ERROR
 */
int32_t dct_close_module2(dct_handle_t *dct_handle);

/**
 * @brief      Write variable name and value in opened module.
 * @param[in]  dct_handle : dct handler
 * @param[in]  variable_name : variable name which you want to store in module
 * @param[in]  variable_value : variable value which you want to store in module
 * @return     0  : SUCCESS
 * @return     <0 : ERROR
 */
int32_t dct_set_variable2(dct_handle_t *dct_handle, char *variable_name, char *variable_value);

/**
 * @brief      Write variable name and value in opened module.
 * @param[in]  dct_handle : dct handler
 * @param[in]  variable_name : variable name which you want to store in module
 * @param[in]  variable_value : variable value which you want to store in module
 * @param[in]  variable_value_length : variable value length which you want to store in module
 * @return     0  : SUCCESS
 * @return     <0 : ERROR
 */
int32_t dct_set_variable_new2(dct_handle_t *dct_handle, char *variable_name, char *variable_value, uint16_t variable_value_length);

/**
 * @brief      read value of variable name in opened module.
 * @param[in]  dct_handle : dct handler
 * @param[in]  variable_name : variable name which you want to get from module
 * @param[out] buffer : read variable value
 * @param[in]  buffer_size : the buffer size
 * @return     0  : SUCCESS
 * @return     <0 : ERROR
 */
int32_t dct_get_variable2(dct_handle_t *dct_handle, char *variable_name, char *buffer, uint16_t buffer_size);

/**
 * @brief         read value of variable name in opened module.
 * @param[in]     dct_handle : dct handler
 * @param[in]     variable_name : variable name which you want to get from module
 * @param[out]    buffer : read variable value
 * @param[in-out] buffer_size : in: the buffer size, out: the real length of variable value
 * @return        0  : SUCCESS
 * @return        <0 : ERROR
 */
int32_t dct_get_variable_new2(dct_handle_t *dct_handle, char *variable_name, char *buffer, uint16_t *buffer_size);

/**
 * @brief      delete variable name and value in opened module.
 * @param[in]  dct_handle : dct handler
 * @param[in]  variable_name : variable name which you want to delete in module
 * @return     0  : SUCCESS
 * @return     <0 : ERROR
 */
int32_t dct_delete_variable2(dct_handle_t *dct_handle, char *variable_name);

/**
 * @brief      delete variable name and value in opened module.
 * @param[in]  dct_handle : dct handler
 * @param[in]  variable_name : variable name which you want to delete in module
 * @return     0  : SUCCESS
 * @return     <0 : ERROR
 */
int32_t dct_delete_variable_new2(dct_handle_t *dct_handle, char *variable_name);

/**
 * @brief      Remaining variable amount in opened module.
 * @param[in]  dct_handle : dct handler
 * @return     integer  : Remaining variable amount
 * @return     <0 : ERROR
 */
int32_t dct_remain_variable2(dct_handle_t *dct_handle);

#endif // __RTK_DCT2_H__

