/**
*****************************************************************************************
*     Copyright(c) 2017, Realtek Semiconductor Corporation. All rights reserved.
*****************************************************************************************
   * @file      central_client_app.c
   * @brief     This file handles BLE central application routines.
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
#if defined(CONFIG_BLE_MATTER_ADAPTER) && CONFIG_BLE_MATTER_ADAPTER
#include <stdio.h>
#include <app_msg.h>
#include <string.h>
#include <trace_app.h>
#include <gap_scan.h>
#include <gap.h>
#include <gap_adv.h>
#include <gap_msg.h>
#include <gap_bond_le.h>
#include <gap_conn_le.h>
#include <gcs_client.h>
#include "gatt_builtin_services.h"
#include "os_mem.h"
#include "os_msg.h"
#include "os_sync.h"
#include "os_queue.h"
#include <ble_matter_adapter_app_flags.h>
#include <ble_matter_adapter_app.h>
#include <ble_matter_adapter_app_task.h>
#include "ble_matter_adapter_app_main.h"
#include "vendor_cmd_bt.h"
#include "matter_blemgr_common.h"
#include "os_timer.h"
/*============================================================================*
 *                              Constants
 *============================================================================*/
#define BLE_MATTER_ADAPTER_APP_MAX_DEVICE_INFO 6

/*============================================================================*
 *                              Variables
 *============================================================================*/
int bt_matter_device_matter_scan_state = 0;
int ble_matter_adapter_peripheral_app_max_links = 0;
int ble_matter_adapter_central_app_max_links = 0;

T_GAP_DEV_STATE ble_matter_adapter_gap_dev_state = {0, 0, 0, 0, 0};                /**< GAP device state */
T_APP_LINK ble_matter_adapter_app_link_table[BLE_MATTER_ADAPTER_APP_MAX_LINKS];

T_CLIENT_ID ble_matter_adapter_gcs_client_id;         /**< General Common Services client client id*/
T_SERVER_ID ble_matter_adapter_service_id;	/**< Matter service id */

M_MULTI_ADV_PARAM matter_multi_adv_param_array[MAX_ADV_NUMBER] = {0};
T_MULTI_ADV_CONCURRENT matter_multi_adapter = {0};

uint8_t matter_local_public_addr[6] = {0};
uint8_t matter_local_static_random_addr[6] = {0};

uint8_t link_customer = 0;    //1  means connection is matter   2 means connection is customer
#if CONFIG_BLE_MATTER_MULTI_ADV_ON
uint8_t customer_adv_id=2;
#endif
uint8_t matter_multi_adv_bt_deinit_id=0xff;
extern uint8_t matter_adv_id;

extern matter_blemgr_callback matter_blemgr_callback_func;
extern void *matter_blemgr_callback_data;

extern uint16_t matter_adv_interval;
extern uint16_t matter_adv_int_min;
extern uint16_t matter_adv_int_max;
extern uint8_t customer_adv_data[];
extern uint8_t customer_rsp_data[];
extern uint8_t customer_adv_data_length;
extern uint8_t customer_rsp_data_length;
extern uint8_t matter_adv_data[31];
extern uint8_t matter_adv_data_length;
extern bool matter_server_is_commissioned();
/*============================================================================*
 *                              Multi ADV Functions
 *============================================================================*/
uint8_t matter_get_unused_adv_index(int type)
{
	if (type == 1) { //matter
		if(!matter_multi_adv_param_array[0].is_used)
		{
			return 0;
		}
	} else if (type == 2) { //customer
		if(!matter_multi_adv_param_array[1].is_used)
		{
			return 1;
		}
	}
	return MAX_ADV_NUMBER;
}

//reserve for future use
bool matter_multi_adv_stop_by_id(uint8_t *adv_id)
{
	if(*adv_id < 0 || *adv_id >= MAX_ADV_NUMBER){
			printf("[%s] wrong input advId:%d\r\n",__func__, *adv_id);
			return 1;
	}	
	if(!matter_multi_adv_param_array[*adv_id].is_used){
		printf("[%s] adv id %d is already stop or not start\r\n",__func__, *adv_id);
		return 1;
	}

	if(matter_multi_adv_param_array[*adv_id].one_shot_timer){
		os_timer_delete(&matter_multi_adv_param_array[*adv_id].one_shot_timer);
	}
	if (matter_multi_adv_param_array[*adv_id].update_adv_mutex) {
		os_mutex_delete(matter_multi_adv_param_array[*adv_id].update_adv_mutex);
	}
	memset(&matter_multi_adv_param_array[*adv_id], 0, sizeof(M_MULTI_ADV_PARAM));
	*adv_id = MAX_ADV_NUMBER;

	return 0;
}

bool matter_multi_adv_start_by_id(uint8_t *adv_id, uint8_t *adv_data, uint16_t adv_len, uint8_t *rsp_data, uint16_t rsp_len, uint8_t type)
{
	if (ble_matter_adapter_peripheral_app_max_links != 0) {
		printf("[%s] as slave role, BLE is conneted\r\n", __func__);
		return 1;
	}

	if ((MAX_ADV_NUMBER != *adv_id) && (matter_multi_adv_param_array[*adv_id].is_used == 1)) {
		os_timer_stop(&matter_multi_adv_param_array[*adv_id].one_shot_timer);
	} else {

		*adv_id = matter_get_unused_adv_index(type);
		if(MAX_ADV_NUMBER == *adv_id){
			printf("[%s] Extend the max adv num %d\r\n", __func__, MAX_ADV_NUMBER);
			return 1;
		}
	}

	uint8_t adv_index = *adv_id;
	M_MULTI_ADV_PARAM *h_adv_param;
	h_adv_param = matter_multi_adv_param_array + adv_index;
		
	if (!h_adv_param->update_adv_mutex) {
		os_mutex_create(&h_adv_param->update_adv_mutex);
	}
	if (type == 1) { //matter
		os_mutex_take(h_adv_param->update_adv_mutex, 0xFFFF);
		memcpy(h_adv_param->adv_data, adv_data, adv_len);
		h_adv_param->adv_datalen = adv_len;
		h_adv_param->adv_int_min = matter_adv_int_min;
		h_adv_param->adv_int_max = matter_adv_int_max;
		h_adv_param->local_bd_type = GAP_LOCAL_ADDR_LE_RANDOM;
		h_adv_param->H_adv_intval = matter_adv_interval;
		h_adv_param->is_used = 1;
		h_adv_param->type = 1;
		h_adv_param->adv_id = 1;
		matter_multi_adapter.matter_sta_sto_flag = false;
		os_mutex_give(h_adv_param->update_adv_mutex);
#if CONFIG_BLE_MATTER_MULTI_ADV_ON
	} else if (type == 2) { //customer
		os_mutex_take(h_adv_param->update_adv_mutex, 0xFFFF);
		memcpy(h_adv_param->adv_data, adv_data, adv_len);
		memcpy(h_adv_param->scanrsp_data, rsp_data, rsp_len);
		h_adv_param->adv_datalen = adv_len;
		h_adv_param->scanrsp_datalen = rsp_len;
		h_adv_param->local_bd_type = GAP_LOCAL_ADDR_LE_PUBLIC;
		h_adv_param->H_adv_intval = 320;
		h_adv_param->is_used = 1;
		h_adv_param->type = 2;
		h_adv_param->adv_id = 2;
		matter_multi_adapter.customer_sta_sto_flag = false;
		os_mutex_give(h_adv_param->update_adv_mutex);
#endif
	}

	if(h_adv_param->one_shot_timer == NULL){
		if (os_timer_create(&h_adv_param->one_shot_timer, "start adv_timer", adv_index, (int)(h_adv_param->H_adv_intval*0.625), 1, ble_matter_adapter_legacy_start_adv_callback) == false){
			printf("os_timer_create h_adv_param->one_shot_timer fail!\r\n");
			return 1;
		}
	}

	if (matter_multi_adapter.deinit_flag == true) { // If deinit, return 1
		printf("[%s]device ble deinit flag is set, return 1\r\n", __func__);
		return 1;
	}
	ble_matter_adapter_send_multi_adv_msg(adv_index);
	os_timer_start(&h_adv_param->one_shot_timer);

	return 0;
}

uint8_t ble_matter_adapter_judge_adv_stop(uint8_t adv_id)
{
	uint8_t flag = 0;
	if (adv_id == matter_adv_id) {
		matter_multi_adapter.adv_id = adv_id;
		if (matter_multi_adapter.matter_sta_sto_flag == true) {
			flag = 1;
		}
#if CONFIG_BLE_MATTER_MULTI_ADV_ON
	} else if (adv_id == customer_adv_id) {
		matter_multi_adapter.adv_id = adv_id;
		if (matter_multi_adapter.customer_sta_sto_flag == true) {
			flag = 1;
		}
#endif
	}

	return flag;
}

void ble_matter_adapter_multi_adv_task_func(void *arg)
{
	(void)arg;
	uint8_t adv_index;
	uint8_t adv_stop_flag = 0;

	while (true) {
		if (os_msg_recv(matter_multi_adapter.queue_handle, &adv_index, 0xFFFFFFFF) == true) {
			if (adv_index == matter_multi_adv_bt_deinit_id) { // If deinit, break the outer while loop
				//printf("[%s]device ble deinit flag is set, break\r\n", __func__);
				break;
#if CONFIG_BLE_MATTER_MULTI_ADV_ON
			} else if (adv_index == customer_adv_id) {
				//printf("[%s] adv_index = %d  customer_adv_id\r\n", __func__, adv_index);
				matter_multi_adapter.adv_id = customer_adv_id;
				if (matter_multi_adapter.customer_sta_sto_flag == true) {
					printf("[%s]stop customer conn adv flag is set[%d]continue\r\n", __func__, adv_index);
					continue;
				}
#endif
			}
			else if (adv_index == matter_adv_id)
			{
				//printf("[%s] adv_index = %d  matter_adv_id\r\n", __func__, adv_index);
				matter_multi_adapter.adv_id = matter_adv_id;
				if (matter_multi_adapter.matter_sta_sto_flag == true) {
					printf("[%s]stop matter conn adv flag is set[%d], continue\r\n", __func__, adv_index);
					continue;
				}
			}
			else
			{
				continue;
			}

			if (matter_multi_adv_param_array[adv_index].local_bd_type == GAP_LOCAL_ADDR_LE_PUBLIC) {
				le_cfg_local_identity_address(matter_local_public_addr, GAP_IDENT_ADDR_PUBLIC);
			} else if (matter_multi_adv_param_array[adv_index].local_bd_type == GAP_LOCAL_ADDR_LE_RANDOM) {
				le_cfg_local_identity_address(matter_local_static_random_addr, GAP_IDENT_ADDR_RAND);
			}
			le_adv_set_param(GAP_PARAM_ADV_LOCAL_ADDR_TYPE, sizeof(matter_multi_adv_param_array[adv_index].local_bd_type), &matter_multi_adv_param_array[adv_index].local_bd_type);
			if (matter_multi_adv_param_array[adv_index].update_adv_mutex) {
				os_mutex_take(matter_multi_adv_param_array[adv_index].update_adv_mutex, 0xFFFF);
				le_adv_set_param(GAP_PARAM_ADV_DATA, matter_multi_adv_param_array[adv_index].adv_datalen, (void *)matter_multi_adv_param_array[adv_index].adv_data);
				if (matter_multi_adv_param_array[adv_index].type == 2) { //customer
					le_adv_set_param(GAP_PARAM_SCAN_RSP_DATA, matter_multi_adv_param_array[adv_index].scanrsp_datalen, (void *)matter_multi_adv_param_array[adv_index].scanrsp_data);
				} else if (matter_multi_adv_param_array[adv_index].type == 1) { //matter
					le_adv_set_param(GAP_PARAM_ADV_INTERVAL_MIN, sizeof(matter_multi_adv_param_array[adv_index].adv_int_min), &matter_multi_adv_param_array[adv_index].adv_int_min);
					le_adv_set_param(GAP_PARAM_ADV_INTERVAL_MAX, sizeof(matter_multi_adv_param_array[adv_index].adv_int_max), &matter_multi_adv_param_array[adv_index].adv_int_max);
				}
				os_mutex_give(matter_multi_adv_param_array[adv_index].update_adv_mutex);
			} else {
				printf("[%s]update_adv_mutex is NULL[%d]\r\n", __func__, adv_index);
			}
			if (ble_matter_adapter_app_send_api_msg(0, &matter_multi_adv_param_array[adv_index]) == false) {
				printf("[%s]send api msg fail\r\n", __func__);
				continue;
			}
			if (os_sem_take(matter_multi_adapter.sem_handle, 0xFFFFFFFF) == false) {
				printf("os_sem_take matter_multi_adapter.sem_handle fail!\r\n");
			}
		}
	}
	printf("delete multi_adv_task\r\n");
	os_sem_delete(matter_multi_adapter.sem_handle);
	os_msg_queue_delete(matter_multi_adapter.queue_handle);
	memset(&matter_multi_adapter, 0, sizeof(matter_multi_adapter));
	os_task_delete(NULL);
}

void ble_matter_adapter_multi_adv_init()
{
	memset(&matter_multi_adapter, 0, sizeof(matter_multi_adapter));
	matter_multi_adapter.deinit_flag = false;
	matter_multi_adapter.matter_sta_sto_flag = true;
	matter_multi_adapter.customer_sta_sto_flag = true;
	matter_multi_adapter.adv_id = MAX_ADV_NUMBER;
	matter_multi_adv_param_array[0].connect_flag = false;
	matter_multi_adv_param_array[1].connect_flag = false;

	if (os_sem_create(&matter_multi_adapter.sem_handle, 0, 1) == false) {
		printf("os_sem_create ms_milti_adapter.sem_handle fail!\r\n");
	}
	if (os_msg_queue_create(&matter_multi_adapter.queue_handle, MATTER_MULTI_ADV_DATA_QUEUE_SIZE, sizeof(uint8_t)) == false) {
		printf("os_msg_queue_create ms_milti_adapter.queue_handle fail!\r\n");
	}

	if (os_task_create(&matter_multi_adapter.task_handle, "multi_adv_task", ble_matter_adapter_multi_adv_task_func, NULL, MATTER_MULTI_TASK_STACK_SIZE, MATTER_MULTI_TASK_PRIORITY) == false) {
		printf("os_task_create ms_milti_adapter.task_handle fail!\r\n");
	}

	memset(matter_multi_adv_param_array, 0, sizeof(M_MULTI_ADV_PARAM) * MAX_ADV_NUMBER);
}

void ble_matter_adapter_multi_adv_deinit()
{
	for(int i = 0; i < MAX_ADV_NUMBER; i ++){
		if(matter_multi_adv_param_array[i].one_shot_timer){
			os_timer_delete(&matter_multi_adv_param_array[i].one_shot_timer);
		}
	}
	memset(matter_multi_adv_param_array, 0, MAX_ADV_NUMBER * sizeof(M_MULTI_ADV_PARAM));
	matter_multi_adapter.deinit_flag = true;
	ble_matter_adapter_send_multi_adv_msg(matter_multi_adv_bt_deinit_id);

	if (matter_multi_adapter.sem_handle) {
		if (os_sem_give(matter_multi_adapter.sem_handle) == false) {
			printf("[%s]os_sem_give fail\r\n", __func__);
		}
	}
}

void ble_matter_adapter_send_multi_adv_msg(uint8_t adv_id)
{
	if (matter_multi_adapter.deinit_flag == true) {
		return;
	}
	if (matter_multi_adapter.queue_handle != NULL) {
		if(os_msg_send(matter_multi_adapter.queue_handle, &adv_id, 0) == false){
		printf("Send adv id to adv data queue fail[%d]\r\n", adv_id);
		}
	} else {
		printf("matter_multi_adapter.queue_handle is NULL\r\n");
	}
}

void ble_matter_adapter_legacy_start_adv_callback(void *data)
{
	int timerID;
	os_timer_id_get(&data,&timerID);
	if (matter_multi_adapter.deinit_flag == true) {
		return;
	}
	ble_matter_adapter_send_multi_adv_msg(timerID);
}

void ble_matter_adapter_delete_adv(uint8_t adv_id)
{
	if(matter_multi_adv_param_array[adv_id].one_shot_timer){
		os_timer_delete(&matter_multi_adv_param_array[adv_id].one_shot_timer);
	}
	if(matter_multi_adv_param_array[adv_id].update_adv_mutex) {
		os_mutex_delete(matter_multi_adv_param_array[adv_id].update_adv_mutex);
	}
	memset(&matter_multi_adv_param_array[adv_id], 0, sizeof(M_MULTI_ADV_PARAM));

}
/*============================================================================*
 *                              Functions
 *============================================================================*/
int ble_matter_adapter_app_handle_upstream_msg(uint16_t subtype, void *pdata)
{
    int ret = 0;

    BT_MATTER_SERVER_SEND_DATA *param = (BT_MATTER_SERVER_SEND_DATA *)pdata;
    if(param)
    {
        server_send_data(param->conn_id, param->service_id, param->attrib_index, param->p_data, param->data_len, param->type);
        os_mem_free(param->p_data);
        os_mem_free(param);
    }

    return ret;
}

void ble_matter_adapter_app_handle_callback_msg(T_IO_MSG callback_msg)
{
    uint16_t msg_type = callback_msg.type;
    switch (msg_type)
    {
	case BLE_MATTER_MSG_CONNECTED_MULTI_ADV: {
            BT_MATTER_CONN_EVENT *connected = callback_msg.u.buf;
            T_MATTER_BLEMGR_GAP_CONNECT_CB_ARG gap_connect_cb_arg;
            gap_connect_cb_arg.conn_id = connected->conn_id;
            if (matter_blemgr_callback_func) {
                matter_blemgr_callback_func(matter_blemgr_callback_data, MATTER_BLEMGR_GAP_CONNECT_CB, &gap_connect_cb_arg);
            }
            os_mem_free(callback_msg.u.buf);
            callback_msg.u.buf = NULL;
        }
        break;

	case BLE_MATTER_MSG_DISCONNECTED_MULTI_ADV: {
            BT_MATTER_CONN_EVENT *disconnected = callback_msg.u.buf;
            T_MATTER_BLEMGR_GAP_DISCONNECT_CB_ARG gap_disconnect_cb_arg;
            gap_disconnect_cb_arg.conn_id = disconnected->conn_id;
            gap_disconnect_cb_arg.disc_cause = disconnected->disc_cause;
            if (matter_blemgr_callback_func) {
                matter_blemgr_callback_func(matter_blemgr_callback_data, MATTER_BLEMGR_GAP_DISCONNECT_CB, &gap_disconnect_cb_arg);
            }
            os_mem_free(callback_msg.u.buf);
            callback_msg.u.buf = NULL;
        }
        break;

	case BLE_MATTER_MSG_WRITE_CHAR_MULTI_ADV: {
            T_MATTER_CALLBACK_DATA *write_char_val = callback_msg.u.buf;
            T_MATTER_BLEMGR_RX_CHAR_WRITE_CB_ARG rx_char_write_cb_arg;
            rx_char_write_cb_arg.conn_id = write_char_val->conn_id;
            rx_char_write_cb_arg.p_value = write_char_val->msg_data.write_read.p_value;
            rx_char_write_cb_arg.len = write_char_val->msg_data.write_read.len;
            if (matter_blemgr_callback_func) {
                matter_blemgr_callback_func(matter_blemgr_callback_data, MATTER_BLEMGR_RX_CHAR_WRITE_CB, &rx_char_write_cb_arg);
            }
            if (write_char_val->msg_data.write_read.len != 0)
            {
                os_mem_free(write_char_val->msg_data.write_read.p_value);
                write_char_val->msg_data.write_read.p_value = NULL;
            }
            os_mem_free(callback_msg.u.buf);
            callback_msg.u.buf = NULL;
        }
        break;
    case BLE_MATTER_MSG_CCCD_RECV_ENABLE_MULTI_ADV: {
               matter_blemgr_callback_func(matter_blemgr_callback_data, MATTER_BLEMGR_TX_CHAR_CCCD_WRITE_CB, callback_msg.u.buf);
               os_mem_free(callback_msg.u.buf);
        }
        break;
    case BLE_MATTER_MSG_CCCD_RECV_DISABLE_MULTI_ADV:
    case BLE_MATTER_MSG_SEND_DATA_COMPLETE_MULTI_ADV: {
               matter_blemgr_callback_func(matter_blemgr_callback_data, MATTER_BLEMGR_TX_COMPLETE_CB, callback_msg.u.buf);
               os_mem_free(callback_msg.u.buf);
        }
        break;

    case BT_MATTER_SEND_CB_MSG_IND_NTF_ENABLE:
    case BT_MATTER_SEND_CB_MSG_IND_NTF_DISABLE:
        {
            T_MATTER_CALLBACK_DATA *indication_notification_enable = callback_msg.u.buf;
            T_MATTER_BLEMGR_TX_CHAR_CCCD_WRITE_CB_ARG tx_char_cccd_write_cb_arg;
            tx_char_cccd_write_cb_arg.conn_id = indication_notification_enable->conn_id;
            if (msg_type == BT_MATTER_SEND_CB_MSG_IND_NTF_DISABLE)
                tx_char_cccd_write_cb_arg.indicationsEnabled = 0;
            else if (msg_type == BT_MATTER_SEND_CB_MSG_IND_NTF_ENABLE)
                tx_char_cccd_write_cb_arg.indicationsEnabled = 1;
            tx_char_cccd_write_cb_arg.notificationsEnabled = 0;
            if (matter_blemgr_callback_func) {
                matter_blemgr_callback_func(matter_blemgr_callback_data, MATTER_BLEMGR_TX_CHAR_CCCD_WRITE_CB, &tx_char_cccd_write_cb_arg);
            }
            os_mem_free(callback_msg.u.buf);
            callback_msg.u.buf = NULL;
        }
        break;

	default:
		printf("[%s] unknow type(%d) callback msg\r\n", __func__, callback_msg.type);
		break;
	}
}

void ble_matter_adapter_app_handle_io_msg(T_IO_MSG io_msg)
{
    uint16_t msg_type = io_msg.type;
    switch (msg_type)
    {
    case IO_MSG_TYPE_BT_STATUS:
        {
            ble_matter_adapter_app_handle_gap_msg(&io_msg);
        }
        break;
    case IO_MSG_TYPE_UART:
        {
            /* We handle user command informations from Data UART in this branch. */
            //int8_t data = io_msg.subtype;
            //mesh_user_cmd_collect(&data, sizeof(data), device_cmd_table);
        }
        break;
    case IO_MSG_TYPE_AT_CMD:
        {
            //uint16_t subtype = io_msg.subtype;
            //void *arg = io_msg.u.buf;
            //if (ble_central_app_handle_at_cmd(subtype, arg) != 1) {
                //ble_peripheral_app_handle_at_cmd(subtype, arg);
            //}
        }
        break;
    case IO_MSG_TYPE_QDECODE:
        {
            if (io_msg.subtype == 0) {
			M_MULTI_ADV_PARAM *adv_param = io_msg.u.buf;
			uint8_t adv_stop_flag = 0;
			adv_stop_flag = ble_matter_adapter_judge_adv_stop(adv_param->adv_id);
			if (adv_stop_flag) {
				printf("stop adv flag[%d] is set, give sem and break\r\n", adv_param->adv_id);
				if(matter_multi_adapter.sem_handle)
					os_sem_give(matter_multi_adapter.sem_handle);
				break;
			}
			uint8_t cause = le_adv_update_param();
			if (cause != GAP_CAUSE_SUCCESS) {
				printf("le_adv_update_param fail! ret = 0x%x\r\n", cause);
			}
            } else if(io_msg.subtype == 2) {
                //gap_sched_scan(false);
            } else if (io_msg.subtype == 3) {
                //gap_sched_scan(true);
            } else if (io_msg.subtype == 4) {
                ble_matter_adapter_app_handle_upstream_msg(io_msg.subtype, io_msg.u.buf);
            } else if (io_msg.subtype == 5) {
                uint8_t conn_id = io_msg.u.buf;
                le_disconnect(conn_id);
            }
        }
        break;
    default:
        break;
    }
}

/**
 * @brief    Handle msg GAP_MSG_LE_DEV_STATE_CHANGE
 * @note     All the gap device state events are pre-handled in this function.
 *           Then the event handling function shall be called according to the new_state
 * @param[in] new_state  New gap device state
 * @param[in] cause GAP device state change cause
 * @return   void
 */
void ble_matter_adapter_app_handle_dev_state_evt(T_GAP_DEV_STATE new_state, uint16_t cause)
{
	APP_PRINT_INFO3("ble_matter_adapter_dev_state_evt: init state  %d, scan state %d, cause 0x%x",
					new_state.gap_init_state,
					new_state.gap_scan_state, cause);
	if (ble_matter_adapter_gap_dev_state.gap_init_state != new_state.gap_init_state) {
		if (new_state.gap_init_state == GAP_INIT_STATE_STACK_READY) {
			uint8_t bt_addr[6];
			APP_PRINT_INFO0("GAP stack ready");
			/*stack ready*/
			gap_get_param(GAP_PARAM_BD_ADDR, bt_addr);
			printf("local bd addr: %02x:%02x:%02x:%02x:%02x:%02x\r\n",
				   bt_addr[5],
				   bt_addr[4],
				   bt_addr[3],
				   bt_addr[2],
				   bt_addr[1],
				   bt_addr[0]);
#if CONFIG_BLE_MATTER_MULTI_ADV_ON
			memcpy(matter_local_public_addr, bt_addr, 6);
#endif
		}
	}

	if (ble_matter_adapter_gap_dev_state.gap_scan_state != new_state.gap_scan_state) {
		if (new_state.gap_scan_state == GAP_SCAN_STATE_IDLE) {
			APP_PRINT_INFO0("GAP scan stop");
			printf("GAP scan stop\r\n");
			
		} else if (new_state.gap_scan_state == GAP_SCAN_STATE_SCANNING) {
			APP_PRINT_INFO0("GAP scan start");
			printf("GAP scan start\r\n");
		}
	}

	if (ble_matter_adapter_gap_dev_state.gap_adv_state != new_state.gap_adv_state) {
		if (new_state.gap_adv_state == GAP_ADV_STATE_IDLE) {
			if (new_state.gap_adv_sub_state == GAP_ADV_TO_IDLE_CAUSE_CONN) {
				APP_PRINT_INFO0("GAP adv stoped: because connection created");
				printf("GAP adv stoped: because connection created\r\n");
			} else {
				APP_PRINT_INFO0("GAP adv stoped");
				printf("GAP adv stopped\r\n");
			}
			
		} else if (new_state.gap_adv_state == GAP_ADV_STATE_ADVERTISING) {
			APP_PRINT_INFO0("GAP adv start");
			printf("GAP adv start\r\n");
		}
	}
	ble_matter_adapter_gap_dev_state = new_state;
}

/**
 * @brief    Handle msg GAP_MSG_LE_CONN_STATE_CHANGE
 * @note     All the gap conn state events are pre-handled in this function.
 *           Then the event handling function shall be called according to the new_state
 * @param[in] conn_id Connection ID
 * @param[in] new_state  New gap connection state
 * @param[in] disc_cause Use this cause when new_state is GAP_CONN_STATE_DISCONNECTED
 * @return   void
 */
void ble_matter_adapter_app_handle_conn_state_evt(uint8_t conn_id, T_GAP_CONN_STATE new_state, uint16_t disc_cause)
{
	T_GAP_CONN_INFO conn_info;
	if (conn_id >= BLE_MATTER_ADAPTER_APP_MAX_LINKS) {
		return;
	}

	APP_PRINT_INFO4("ble_matter_adapter_app_handle_conn_state_evt: conn_id %d, conn_state(%d -> %d), disc_cause 0x%x",
					conn_id, ble_matter_adapter_app_link_table[conn_id].conn_state, new_state, disc_cause);
	ble_matter_adapter_app_link_table[conn_id].conn_state = new_state;
	switch (new_state) {
	case GAP_CONN_STATE_DISCONNECTED: {
		if ((disc_cause != (HCI_ERR | HCI_ERR_REMOTE_USER_TERMINATE))
			&& (disc_cause != (HCI_ERR | HCI_ERR_LOCAL_HOST_TERMINATE))) {
			APP_PRINT_ERROR2("ble_matter_adapter_app_handle_conn_state_evt: connection lost, conn_id %d, cause 0x%x", conn_id,
							 disc_cause);
		}
		printf("Disconnect conn_id %d, cause 0x%x\r\n", conn_id, disc_cause);

		if (ble_matter_adapter_app_link_table[conn_id].role == GAP_LINK_ROLE_MASTER) {
			ble_matter_adapter_central_app_max_links --;
		} else if (ble_matter_adapter_app_link_table[conn_id].role == GAP_LINK_ROLE_SLAVE) {
			ble_matter_adapter_peripheral_app_max_links --;
		}

		if (ble_matter_adapter_app_link_table[conn_id].role == GAP_LINK_ROLE_SLAVE) {
			printf("As peripheral,recieve disconncect,please start ADV\r\n");
		}

		memset(&ble_matter_adapter_app_link_table[conn_id], 0, sizeof(T_APP_LINK));

		if (link_customer == 1) { //matter
			matter_multi_adv_param_array[0].connect_flag = false;

			/* Matter ADV will be restarted from Matter upper layer */
			//matter_multi_adv_start_by_id(&matter_adv_id, matter_adv_data, matter_adv_data_length, NULL, 0, 1); // the last parameter: 1 for Matter; 2 for Customer

#if 1  //send data to matter
			T_MATTER_BLEMGR_GAP_DISCONNECT_CB_ARG *disconnected_msg_matter = (T_MATTER_BLEMGR_GAP_DISCONNECT_CB_ARG *)os_mem_alloc(0, sizeof(T_MATTER_BLEMGR_GAP_DISCONNECT_CB_ARG));
			memset(disconnected_msg_matter, 0, sizeof(T_MATTER_BLEMGR_GAP_DISCONNECT_CB_ARG));
			disconnected_msg_matter->conn_id = conn_id;
			disconnected_msg_matter->disc_cause = disc_cause;
			if (ble_matter_adapter_send_callback_msg(BLE_MATTER_MSG_DISCONNECTED_MULTI_ADV,NULL, disconnected_msg_matter) == false) {
				printf("[%s] send callback msg fail\r\n", __func__);
				os_mem_free(disconnected_msg_matter);
			}
#endif
#if CONFIG_BLE_MATTER_MULTI_ADV_ON
		} else if (link_customer == 2) { //customer
			matter_multi_adv_param_array[1].connect_flag = false;

			/* Customer ADV can be restarted on customer's application */
			matter_multi_adv_start_by_id(&customer_adv_id, customer_adv_data, customer_adv_data_length, customer_rsp_data, customer_rsp_data_length, 2); // the last parameter: 1 for Matter; 2 for Customer
#endif
		}


	}
	break;

	case GAP_CONN_STATE_CONNECTED: {
	    uint16_t conn_interval;
        uint16_t conn_latency;
        uint16_t conn_supervision_timeout;
        
		le_get_conn_addr(conn_id, ble_matter_adapter_app_link_table[conn_id].bd_addr,
						 (void *)&ble_matter_adapter_app_link_table[conn_id].bd_type);

		//get device role
		if (le_get_conn_info(conn_id, &conn_info)) {
		ble_matter_adapter_app_link_table[conn_id].role = conn_info.role;
		if (ble_matter_adapter_app_link_table[conn_id].role == GAP_LINK_ROLE_MASTER) {
			ble_matter_adapter_central_app_max_links ++;
		} else if (ble_matter_adapter_app_link_table[conn_id].role == GAP_LINK_ROLE_SLAVE) {
			ble_matter_adapter_peripheral_app_max_links ++;			}
		}
		printf("Connected success conn_id %d\r\n", conn_id);
		
        uint8_t local_bd_type;
        uint8_t remote_bd_type;
		le_get_conn_param(GAP_PARAM_CONN_LOCAL_BD_TYPE, &local_bd_type, conn_id);
		le_get_conn_param(GAP_PARAM_CONN_BD_ADDR_TYPE, &remote_bd_type, conn_id);
		APP_PRINT_INFO3("GAP_CONN_STATE_CONNECTED: conn_id %d, local_bd_type %d, remote_bd_type %d\n",
						conn_id, local_bd_type, remote_bd_type);
		printf("GAP_CONN_STATE_CONNECTED: conn_id %d, local_bd_type %d, remote_bd_type %d\n",
			   conn_id, local_bd_type, remote_bd_type);
			   
        le_get_conn_param(GAP_PARAM_CONN_INTERVAL, &conn_interval, conn_id);
        le_get_conn_param(GAP_PARAM_CONN_LATENCY, &conn_latency, conn_id);
        le_get_conn_param(GAP_PARAM_CONN_TIMEOUT, &conn_supervision_timeout, conn_id);            
		le_get_conn_addr(conn_id, ble_matter_adapter_app_link_table[conn_id].bd_addr,
                             &ble_matter_adapter_app_link_table[conn_id].bd_type);
        APP_PRINT_INFO5("GAP_CONN_STATE_CONNECTED:remote_bd %s, remote_addr_type %d, conn_interval 0x%x, conn_latency 0x%x, conn_supervision_timeout 0x%x",
                            TRACE_BDADDR(ble_matter_adapter_app_link_table[conn_id].bd_addr), ble_matter_adapter_app_link_table[conn_id].bd_type,
                            conn_interval, conn_latency, conn_supervision_timeout);
        printf("Connected success conn_id %d\r\n", conn_id);

		//get local adv bt type
		if (local_bd_type == GAP_LOCAL_ADDR_LE_RANDOM) { // matter
			link_customer = 1;
			T_MATTER_BLEMGR_GAP_CONNECT_CB_ARG *conn_msg_matter = (T_MATTER_BLEMGR_GAP_CONNECT_CB_ARG *)os_mem_alloc(0, sizeof(T_MATTER_BLEMGR_GAP_CONNECT_CB_ARG));
			memset(conn_msg_matter, 0, sizeof(T_MATTER_BLEMGR_GAP_CONNECT_CB_ARG));
			conn_msg_matter->conn_id = conn_id;
			if (ble_matter_adapter_send_callback_msg(BLE_MATTER_MSG_CONNECTED_MULTI_ADV, NULL, conn_msg_matter) == false) {
				printf("[%s] send callback msg fail\r\n", __func__);
				os_mem_free(conn_msg_matter);
			}
                matter_multi_adv_param_array[0].connect_flag == true;
                matter_multi_adv_stop_by_id(&matter_adv_id); // stop matter adv
#if CONFIG_BLE_MATTER_MULTI_ADV_ON
		} else if (local_bd_type == GAP_LOCAL_ADDR_LE_PUBLIC) { // customer
                link_customer = 2;
                matter_multi_adv_param_array[1].connect_flag == true;
		matter_multi_adv_stop_by_id(&customer_adv_id); // stop customer adv
#endif
		}

#if F_BT_LE_5_0_SET_PHY_SUPPORT
		{
			uint8_t tx_phy;
			uint8_t rx_phy;
			le_get_conn_param(GAP_PARAM_CONN_RX_PHY_TYPE, &rx_phy, conn_id);
			le_get_conn_param(GAP_PARAM_CONN_TX_PHY_TYPE, &tx_phy, conn_id);
			APP_PRINT_INFO2("GAP_CONN_STATE_CONNECTED: tx_phy %d, rx_phy %d\n", tx_phy, rx_phy);
			printf("GAP_CONN_STATE_CONNECTED: tx_phy %d, rx_phy %d\n", tx_phy, rx_phy);
		}
#endif	   
	}
	break;

	default:
		break;

	}
}

/**
 * @brief    Handle msg GAP_MSG_LE_AUTHEN_STATE_CHANGE
 * @note     All the gap authentication state events are pre-handled in this function.
 *           Then the event handling function shall be called according to the new_state
 * @param[in] conn_id Connection ID
 * @param[in] new_state  New authentication state
 * @param[in] cause Use this cause when new_state is GAP_AUTHEN_STATE_COMPLETE
 * @return   void
 */
void ble_matter_adapter_app_handle_authen_state_evt(uint8_t conn_id, uint8_t new_state, uint16_t cause)
{
	APP_PRINT_INFO2("ble_matter_adapter_app_handle_authen_state_evt:conn_id %d, cause 0x%x", conn_id, cause);
	switch (new_state) {
	case GAP_AUTHEN_STATE_STARTED: {
		APP_PRINT_INFO0("ble_matter_adapter_app_handle_authen_state_evt: GAP_AUTHEN_STATE_STARTED");
	}
	break;

	case GAP_AUTHEN_STATE_COMPLETE: {
		if (cause == GAP_SUCCESS) {
			printf("Pair success\r\n");
			APP_PRINT_INFO0("ble_matter_adapter_app_handle_authen_state_evt: GAP_AUTHEN_STATE_COMPLETE pair success");
		} else {
			printf("Pair failed: cause 0x%x\r\n", cause);
			APP_PRINT_INFO0("ble_matter_adapter_app_handle_authen_state_evt: GAP_AUTHEN_STATE_COMPLETE pair failed");
		}
	}
	break;

	default: {
		APP_PRINT_ERROR1("ble_matter_adapter_app_handle_authen_state_evt: unknown newstate %d", new_state);
	}
	break;
	}
}

/**
 * @brief    Handle msg GAP_MSG_LE_CONN_MTU_INFO
 * @note     This msg is used to inform APP that exchange mtu procedure is completed.
 * @param[in] conn_id Connection ID
 * @param[in] mtu_size  New mtu size
 * @return   void
 */
void ble_matter_adapter_app_handle_conn_mtu_info_evt(uint8_t conn_id, uint16_t mtu_size)
{
	APP_PRINT_INFO2("ble_matter_adapter_app_handle_conn_mtu_info_evt: conn_id %d, mtu_size %d", conn_id, mtu_size);
}

/**
 * @brief    Handle msg GAP_MSG_LE_CONN_PARAM_UPDATE
 * @note     All the connection parameter update change  events are pre-handled in this function.
 * @param[in] conn_id Connection ID
 * @param[in] status  New update state
 * @param[in] cause Use this cause when status is GAP_CONN_PARAM_UPDATE_STATUS_FAIL
 * @return   void
 */
void ble_matter_adapter_app_handle_conn_param_update_evt(uint8_t conn_id, uint8_t status, uint16_t cause)
{
	switch (status) {
	case GAP_CONN_PARAM_UPDATE_STATUS_SUCCESS: {
		uint16_t conn_interval;
		uint16_t conn_slave_latency;
		uint16_t conn_supervision_timeout;

		le_get_conn_param(GAP_PARAM_CONN_INTERVAL, &conn_interval, conn_id);
		le_get_conn_param(GAP_PARAM_CONN_LATENCY, &conn_slave_latency, conn_id);
		le_get_conn_param(GAP_PARAM_CONN_TIMEOUT, &conn_supervision_timeout, conn_id);
		APP_PRINT_INFO4("ble_matter_adapter_app_handle_conn_param_update_evt update success:conn_id %d, conn_interval 0x%x, conn_slave_latency 0x%x, conn_supervision_timeout 0x%x",
						conn_id, conn_interval, conn_slave_latency, conn_supervision_timeout);
	}
	break;

	case GAP_CONN_PARAM_UPDATE_STATUS_FAIL: {
		APP_PRINT_ERROR2("ble_matter_adapter_app_handle_conn_param_update_evt update failed: conn_id %d, cause 0x%x",
						 conn_id, cause);
	}
	break;

	case GAP_CONN_PARAM_UPDATE_STATUS_PENDING: {
		APP_PRINT_INFO1("ble_matter_adapter_app_handle_conn_param_update_evt update pending: conn_id %d", conn_id);
	}
	break;

	default:
		break;
	}
}

/**
 * @brief    All the BT GAP MSG are pre-handled in this function.
 * @note     Then the event handling function shall be called according to the
 *           subtype of T_IO_MSG
 * @param[in] p_gap_msg Pointer to GAP msg
 * @return   void
 */
void ble_matter_adapter_app_handle_gap_msg(T_IO_MSG *p_gap_msg)
{
	T_LE_GAP_MSG gap_msg;
	uint8_t conn_id;
	memcpy(&gap_msg, &p_gap_msg->u.param, sizeof(p_gap_msg->u.param));

	APP_PRINT_TRACE1("ble_matter_adapter_app_handle_gap_msg: subtype %d", p_gap_msg->subtype);
	switch (p_gap_msg->subtype) {
	case GAP_MSG_LE_DEV_STATE_CHANGE: {
		ble_matter_adapter_app_handle_dev_state_evt(gap_msg.msg_data.gap_dev_state_change.new_state,
												gap_msg.msg_data.gap_dev_state_change.cause);
	}
	break;

	case GAP_MSG_LE_CONN_STATE_CHANGE: {
		ble_matter_adapter_app_handle_conn_state_evt(gap_msg.msg_data.gap_conn_state_change.conn_id,
				(T_GAP_CONN_STATE)gap_msg.msg_data.gap_conn_state_change.new_state,
				gap_msg.msg_data.gap_conn_state_change.disc_cause);
	}
	break;

	case GAP_MSG_LE_CONN_MTU_INFO: {
		ble_matter_adapter_app_handle_conn_mtu_info_evt(gap_msg.msg_data.gap_conn_mtu_info.conn_id,
				gap_msg.msg_data.gap_conn_mtu_info.mtu_size);
	}
	break;

	case GAP_MSG_LE_CONN_PARAM_UPDATE: {
		ble_matter_adapter_app_handle_conn_param_update_evt(gap_msg.msg_data.gap_conn_param_update.conn_id,
				gap_msg.msg_data.gap_conn_param_update.status,
				gap_msg.msg_data.gap_conn_param_update.cause);
	}
	break;

	case GAP_MSG_LE_AUTHEN_STATE_CHANGE: {
		ble_matter_adapter_app_handle_authen_state_evt(gap_msg.msg_data.gap_authen_state.conn_id,
				gap_msg.msg_data.gap_authen_state.new_state,
				gap_msg.msg_data.gap_authen_state.status);
	}
	break;

	case GAP_MSG_LE_BOND_JUST_WORK: {
		conn_id = gap_msg.msg_data.gap_bond_just_work_conf.conn_id;
		le_bond_just_work_confirm(conn_id, GAP_CFM_CAUSE_ACCEPT);
		APP_PRINT_INFO0("GAP_MSG_LE_BOND_JUST_WORK");
	}
	break;

	case GAP_MSG_LE_BOND_PASSKEY_DISPLAY: {
		uint32_t display_value = 0;
		conn_id = gap_msg.msg_data.gap_bond_passkey_display.conn_id;
		le_bond_get_display_key(conn_id, &display_value);
		APP_PRINT_INFO2("GAP_MSG_LE_BOND_PASSKEY_DISPLAY: conn_id %d, passkey %d",
						conn_id, display_value);
		le_bond_passkey_display_confirm(conn_id, GAP_CFM_CAUSE_ACCEPT);
		printf("GAP_MSG_LE_BOND_PASSKEY_DISPLAY: conn_id %d, passkey %d\r\n",
			   conn_id,
			   display_value);
	}
	break;

	case GAP_MSG_LE_BOND_USER_CONFIRMATION: {
		uint32_t display_value = 0;
		conn_id = gap_msg.msg_data.gap_bond_user_conf.conn_id;
		le_bond_get_display_key(conn_id, &display_value);
		APP_PRINT_INFO2("GAP_MSG_LE_BOND_USER_CONFIRMATION: conn_id %d, passkey %d",
						conn_id, display_value);
		printf("GAP_MSG_LE_BOND_USER_CONFIRMATION: conn_id %d, passkey %d\r\n",
			   conn_id,
			   display_value);
		//le_bond_user_confirm(conn_id, GAP_CFM_CAUSE_ACCEPT);
	}
	break;

	case GAP_MSG_LE_BOND_PASSKEY_INPUT: {
		//uint32_t passkey = 888888;
		conn_id = gap_msg.msg_data.gap_bond_passkey_input.conn_id;
		APP_PRINT_INFO1("GAP_MSG_LE_BOND_PASSKEY_INPUT: conn_id %d", conn_id);
		printf("GAP_MSG_LE_BOND_PASSKEY_INPUT: conn_id %d\r\n", conn_id);
		//le_bond_passkey_input_confirm(conn_id, passkey, GAP_CFM_CAUSE_ACCEPT);
	}
	break;
#if F_BT_LE_SMP_OOB_SUPPORT
	case GAP_MSG_LE_BOND_OOB_INPUT: {
		uint8_t oob_data[GAP_OOB_LEN] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
		conn_id = gap_msg.msg_data.gap_bond_oob_input.conn_id;
		APP_PRINT_INFO1("GAP_MSG_LE_BOND_OOB_INPUT: conn_id %d", conn_id);
		le_bond_set_param(GAP_PARAM_BOND_OOB_DATA, GAP_OOB_LEN, oob_data);
		le_bond_oob_input_confirm(conn_id, GAP_CFM_CAUSE_ACCEPT);
	}
	break;
#endif
	default:
		APP_PRINT_ERROR1("ble_matter_adapter_app_handle_gap_msg: unknown subtype %d", p_gap_msg->subtype);
		break;
	}
}

/** @defgroup  CENTRAL_CLIENT_GAP_CALLBACK GAP Callback Event Handler
    * @brief Handle GAP callback event
    * @{
    */
/**
  * @brief Used to parse advertising data and scan response data
  * @param[in] scan_info point to scan information data.
  * @retval void
  */
void bt_matter_device_matter_app_parse_scan_info(T_LE_SCAN_INFO *scan_info)
{
    uint8_t buffer[32];
    uint8_t pos = 0;

    while (pos < scan_info->data_len)
    {
        /* Length of the AD structure. */
        uint8_t length = scan_info->data[pos++];
        uint8_t type;

        if ((length > 0x01) && ((pos + length) <= 31))
        {
            /* Copy the AD Data to buffer. */
            memcpy(buffer, scan_info->data + pos + 1, length - 1);
            /* AD Type, one octet. */
            type = scan_info->data[pos];

            APP_PRINT_TRACE2("bt_mesh_device_matter_app_parse_scan_info: AD Structure Info: AD type 0x%x, AD Data Length %d", type,
                             length - 1);

            switch (type)
            {
            case GAP_ADTYPE_FLAGS:
                {
                    /* (flags & 0x01) -- LE Limited Discoverable Mode */
                    /* (flags & 0x02) -- LE General Discoverable Mode */
                    /* (flags & 0x04) -- BR/EDR Not Supported */
                    /* (flags & 0x08) -- Simultaneous LE and BR/EDR to Same Device Capable (Controller) */
                    /* (flags & 0x10) -- Simultaneous LE and BR/EDR to Same Device Capable (Host) */
                    uint8_t flags = scan_info->data[pos + 1];
                    APP_PRINT_INFO1("GAP_ADTYPE_FLAGS: 0x%x", flags);
                    BLE_PRINT("GAP_ADTYPE_FLAGS: 0x%x\n\r", flags);

                }
                break;

            case GAP_ADTYPE_16BIT_MORE:
            case GAP_ADTYPE_16BIT_COMPLETE:
            case GAP_ADTYPE_SERVICES_LIST_16BIT:
                {
                    uint16_t *p_uuid = (uint16_t *)(buffer);
                    uint8_t i = length - 1;

                    while (i >= 2)
                    {
                        APP_PRINT_INFO1("GAP_ADTYPE_16BIT_XXX: 0x%x", *p_uuid);
                        BLE_PRINT("GAP_ADTYPE_16BIT_XXX: 0x%x\n\r", *p_uuid);
                        p_uuid ++;
                        i -= 2;
                    }
                }
                break;

            case GAP_ADTYPE_32BIT_MORE:
            case GAP_ADTYPE_32BIT_COMPLETE:
                {
                    uint32_t *p_uuid = (uint32_t *)(buffer);
                    uint8_t    i     = length - 1;

                    while (i >= 4)
                    {
                        APP_PRINT_INFO1("GAP_ADTYPE_32BIT_XXX: 0x%x", *p_uuid);
                        BLE_PRINT("GAP_ADTYPE_32BIT_XXX: 0x%x\n\r", (unsigned int)*p_uuid);
                        p_uuid ++;

                        i -= 4;
                    }
                }
                break;

            case GAP_ADTYPE_128BIT_MORE:
            case GAP_ADTYPE_128BIT_COMPLETE:
            case GAP_ADTYPE_SERVICES_LIST_128BIT:
                {
                    uint32_t *p_uuid = (uint32_t *)(buffer);
                    APP_PRINT_INFO4("GAP_ADTYPE_128BIT_XXX: 0x%8.8x%8.8x%8.8x%8.8x",
                                    p_uuid[3], p_uuid[2], p_uuid[1], p_uuid[0]);
                    BLE_PRINT("GAP_ADTYPE_128BIT_XXX: 0x%8.8x%8.8x%8.8x%8.8x\n\r",
                                    (unsigned int)p_uuid[3], (unsigned int)p_uuid[2], (unsigned int)p_uuid[1], (unsigned int)p_uuid[0]);

                }
                break;

            case GAP_ADTYPE_LOCAL_NAME_SHORT:
            case GAP_ADTYPE_LOCAL_NAME_COMPLETE:
                {
                    buffer[length - 1] = '\0';
                    APP_PRINT_INFO1("GAP_ADTYPE_LOCAL_NAME_XXX: %s", TRACE_STRING(buffer));
                    BLE_PRINT("GAP_ADTYPE_LOCAL_NAME_XXX: %s\n\r", buffer);

                }
                break;

            case GAP_ADTYPE_POWER_LEVEL:
                {
                    APP_PRINT_INFO1("GAP_ADTYPE_POWER_LEVEL: 0x%x", scan_info->data[pos + 1]);
                    BLE_PRINT("GAP_ADTYPE_POWER_LEVEL: 0x%x\n\r", scan_info->data[pos + 1]);

                }
                break;

            case GAP_ADTYPE_SLAVE_CONN_INTERVAL_RANGE:
                {
                    uint16_t *p_min = (uint16_t *)(buffer);
                    uint16_t *p_max = p_min + 1;
                    APP_PRINT_INFO2("GAP_ADTYPE_SLAVE_CONN_INTERVAL_RANGE: 0x%x - 0x%x", *p_min,
                                    *p_max);
                    BLE_PRINT("GAP_ADTYPE_SLAVE_CONN_INTERVAL_RANGE: 0x%x - 0x%x\n\r", *p_min, *p_max);

                }
                break;

            case GAP_ADTYPE_SERVICE_DATA:
                {
                    uint16_t *p_uuid = (uint16_t *)(buffer);
                    uint8_t data_len = length - 3;

                    APP_PRINT_INFO3("GAP_ADTYPE_SERVICE_DATA: UUID 0x%x, len %d, data %b", *p_uuid,
                                    data_len, TRACE_BINARY(data_len, &buffer[2]));
                    BLE_PRINT("GAP_ADTYPE_SERVICE_DATA: UUID 0x%x, len %d\n\r", *p_uuid, data_len);

                }
                break;
            case GAP_ADTYPE_APPEARANCE:
                {
                    uint16_t *p_appearance = (uint16_t *)(buffer);
                    APP_PRINT_INFO1("GAP_ADTYPE_APPEARANCE: %d", *p_appearance);
                    BLE_PRINT("GAP_ADTYPE_APPEARANCE: %d\n\r", *p_appearance);

                }
                break;

            case GAP_ADTYPE_MANUFACTURER_SPECIFIC:
                {
                    uint8_t data_len = length - 3;
                    uint16_t *p_company_id = (uint16_t *)(buffer);
                    APP_PRINT_INFO3("GAP_ADTYPE_MANUFACTURER_SPECIFIC: company_id 0x%x, len %d, data %b",
                                    *p_company_id, data_len, TRACE_BINARY(data_len, &buffer[2]));
                    BLE_PRINT("GAP_ADTYPE_MANUFACTURER_SPECIFIC: company_id 0x%x, len %d\n\r", *p_company_id, data_len);

                }
                break;

            default:
                {
                    uint8_t i = 0;

                    for (i = 0; i < (length - 1); i++)
                    {
                        APP_PRINT_INFO1("  AD Data: Unhandled Data = 0x%x", scan_info->data[pos + i]);
                    //BLE_PRINT("  AD Data: Unhandled Data = 0x%x\n\r", scan_info->data[pos + i]);

                    }
                }
                break;
            }
        }

        pos += length;
    }
}

/** @} */ /* End of group CENTRAL_CLIENT_GAP_CALLBACK */
/** @defgroup  GCS_CLIIENT_CALLBACK GCS Client Callback Event Handler
    * @brief Handle profile client callback event
    * @{
    */
void ble_matter_adapter_gcs_handle_discovery_result(uint8_t conn_id, T_GCS_DISCOVERY_RESULT discov_result)
{
	uint16_t i;
	T_GCS_DISCOV_RESULT *p_result_table;
	uint16_t    properties;
	switch (discov_result.discov_type) {
	case GCS_ALL_PRIMARY_SRV_DISCOV:
		APP_PRINT_INFO2("conn_id %d, GCS_ALL_PRIMARY_SRV_DISCOV, is_success %d",
						conn_id, discov_result.is_success);
		for (i = 0; i < discov_result.result_num; i++) {
			p_result_table = &(discov_result.p_result_table[i]);
			switch (p_result_table->result_type) {
			case DISC_RESULT_ALL_SRV_UUID16:
				APP_PRINT_INFO4("ALL SRV UUID16[%d]: service range: 0x%x-0x%x, uuid16 0x%x",
								i, p_result_table->result_data.srv_uuid16_disc_data.att_handle,
								p_result_table->result_data.srv_uuid16_disc_data.end_group_handle,
								p_result_table->result_data.srv_uuid16_disc_data.uuid16);
				printf("ALL SRV UUID16[%d]: service range: 0x%x-0x%x, uuid16 0x%x\r\n",
					   i, p_result_table->result_data.srv_uuid16_disc_data.att_handle,
					   p_result_table->result_data.srv_uuid16_disc_data.end_group_handle,
					   p_result_table->result_data.srv_uuid16_disc_data.uuid16);

				break;
			case DISC_RESULT_ALL_SRV_UUID128:
				APP_PRINT_INFO4("ALL SRV UUID128[%d]: service range: 0x%x-0x%x, service=<%b>",
								i, p_result_table->result_data.srv_uuid128_disc_data.att_handle,
								p_result_table->result_data.srv_uuid128_disc_data.end_group_handle,
								TRACE_BINARY(16, p_result_table->result_data.srv_uuid128_disc_data.uuid128));
				printf("ALL SRV UUID128[%d]: service range: 0x%x-0x%x, service="UUID_128_FORMAT"\n\r",
					   i, p_result_table->result_data.srv_uuid128_disc_data.att_handle,
					   p_result_table->result_data.srv_uuid128_disc_data.end_group_handle,
					   UUID_128(p_result_table->result_data.srv_uuid128_disc_data.uuid128));
				break;

			default:
				APP_PRINT_ERROR0("Invalid Discovery Result Type!");
				printf("Invalid Discovery Result Type!\n\r");
				break;
			}
		}
		break;

	case GCS_BY_UUID128_SRV_DISCOV:
		APP_PRINT_INFO2("conn_id %d, GCS_BY_UUID128_SRV_DISCOV, is_success %d",
						conn_id, discov_result.is_success);
		printf("conn_id %d, GCS_BY_UUID128_SRV_DISCOV, is_success %d\n\r",
			   conn_id, discov_result.is_success);

		for (i = 0; i < discov_result.result_num; i++) {
			p_result_table = &(discov_result.p_result_table[i]);
			switch (p_result_table->result_type) {
			case DISC_RESULT_SRV_DATA:
				APP_PRINT_INFO3("SRV DATA[%d]: service range: 0x%x-0x%x",
								i, p_result_table->result_data.srv_disc_data.att_handle,
								p_result_table->result_data.srv_disc_data.end_group_handle);
				printf("SRV DATA[%d]: service range: 0x%x-0x%x\n\r",
					   i, p_result_table->result_data.srv_disc_data.att_handle,
					   p_result_table->result_data.srv_disc_data.end_group_handle);

				break;

			default:
				APP_PRINT_ERROR0("Invalid Discovery Result Type!");
				printf("Invalid Discovery Result Type!\n\r");
				break;
			}
		}
		break;

	case GCS_BY_UUID_SRV_DISCOV:
		APP_PRINT_INFO2("conn_id %d, GCS_BY_UUID_SRV_DISCOV, is_success %d",
						conn_id, discov_result.is_success);
		printf("conn_id %d, GCS_BY_UUID_SRV_DISCOV, is_success %d\n\r",
			   conn_id, discov_result.is_success);

		for (i = 0; i < discov_result.result_num; i++) {
			p_result_table = &(discov_result.p_result_table[i]);
			switch (p_result_table->result_type) {
			case DISC_RESULT_SRV_DATA:
				APP_PRINT_INFO3("SRV DATA[%d]: service range: 0x%x-0x%x",
								i, p_result_table->result_data.srv_disc_data.att_handle,
								p_result_table->result_data.srv_disc_data.end_group_handle);
				printf("SRV DATA[%d]: service range: 0x%x-0x%x\n\r",
					   i, p_result_table->result_data.srv_disc_data.att_handle,
					   p_result_table->result_data.srv_disc_data.end_group_handle);

				break;

			default:
				APP_PRINT_ERROR0("Invalid Discovery Result Type!");
				printf("Invalid Discovery Result Type!\n\r");
				break;
			}
		}
		break;

	case GCS_ALL_CHAR_DISCOV:
		APP_PRINT_INFO2("conn_id %d, GCS_ALL_CHAR_DISCOV, is_success %d",
						conn_id, discov_result.is_success);
		printf("conn_id %d, GCS_ALL_CHAR_DISCOV, is_success %d\n\r",
			   conn_id, discov_result.is_success);

		for (i = 0; i < discov_result.result_num; i++) {
			p_result_table = &(discov_result.p_result_table[i]);
			switch (p_result_table->result_type) {
			case DISC_RESULT_CHAR_UUID16:
				properties = p_result_table->result_data.char_uuid16_disc_data.properties;
				APP_PRINT_INFO5("CHAR UUID16[%d]: decl_handle 0x%x, properties 0x%x, value_handle 0x%x, uuid16 0x%x",
								i, p_result_table->result_data.char_uuid16_disc_data.decl_handle,
								p_result_table->result_data.char_uuid16_disc_data.properties,
								p_result_table->result_data.char_uuid16_disc_data.value_handle,
								p_result_table->result_data.char_uuid16_disc_data.uuid16);
				APP_PRINT_INFO5("properties:indicate %d, read %d, write cmd %d, write %d, notify %d\r\n",
								properties & GATT_CHAR_PROP_INDICATE,
								properties & GATT_CHAR_PROP_READ,
								properties & GATT_CHAR_PROP_WRITE_NO_RSP,
								properties & GATT_CHAR_PROP_WRITE,
								properties & GATT_CHAR_PROP_NOTIFY);
				printf("CHAR UUID16[%d]: decl_handle 0x%x, properties 0x%x, value_handle 0x%x, uuid16 0x%x\n\r",
					   i, p_result_table->result_data.char_uuid16_disc_data.decl_handle,
					   p_result_table->result_data.char_uuid16_disc_data.properties,
					   p_result_table->result_data.char_uuid16_disc_data.value_handle,
					   p_result_table->result_data.char_uuid16_disc_data.uuid16);
				printf("properties:indicate %d, read %d, write cmd %d, write %d, notify %d\n\r",
					   properties & GATT_CHAR_PROP_INDICATE,
					   properties & GATT_CHAR_PROP_READ,
					   properties & GATT_CHAR_PROP_WRITE_NO_RSP,
					   properties & GATT_CHAR_PROP_WRITE,
					   properties & GATT_CHAR_PROP_NOTIFY);
				
				break;

			case DISC_RESULT_CHAR_UUID128:
				properties = p_result_table->result_data.char_uuid128_disc_data.properties;
				APP_PRINT_INFO5("CHAR UUID128[%d]:  decl hndl=0x%x, prop=0x%x, value hndl=0x%x, uuid128=<%b>",
								i, p_result_table->result_data.char_uuid128_disc_data.decl_handle,
								p_result_table->result_data.char_uuid128_disc_data.properties,
								p_result_table->result_data.char_uuid128_disc_data.value_handle,
								TRACE_BINARY(16, p_result_table->result_data.char_uuid128_disc_data.uuid128));
				APP_PRINT_INFO5("properties:indicate %d, read %d, write cmd %d, write %d, notify %d",
								properties & GATT_CHAR_PROP_INDICATE,
								properties & GATT_CHAR_PROP_READ,
								properties & GATT_CHAR_PROP_WRITE_NO_RSP,
								properties & GATT_CHAR_PROP_WRITE,
								properties & GATT_CHAR_PROP_NOTIFY
							   );
				printf("CHAR UUID128[%d]:  decl hndl=0x%x, prop=0x%x, value hndl=0x%x, uuid128="UUID_128_FORMAT"\n\r",
					   i, p_result_table->result_data.char_uuid128_disc_data.decl_handle,
					   p_result_table->result_data.char_uuid128_disc_data.properties,
					   p_result_table->result_data.char_uuid128_disc_data.value_handle,
					   UUID_128(p_result_table->result_data.char_uuid128_disc_data.uuid128));
				printf("properties:indicate %d, read %d, write cmd %d, write %d, notify %d\n\r",
					   properties & GATT_CHAR_PROP_INDICATE,
					   properties & GATT_CHAR_PROP_READ,
					   properties & GATT_CHAR_PROP_WRITE_NO_RSP,
					   properties & GATT_CHAR_PROP_WRITE,
					   properties & GATT_CHAR_PROP_NOTIFY
					  );
				break;
			default:
				APP_PRINT_ERROR0("Invalid Discovery Result Type!");
				printf("Invalid Discovery Result Type!\n\r");
				break;
			}
		}
		break;

	case GCS_BY_UUID_CHAR_DISCOV:
		APP_PRINT_INFO2("conn_id %d, GCS_BY_UUID_CHAR_DISCOV, is_success %d",
						conn_id, discov_result.is_success);
		printf("conn_id %d, GCS_BY_UUID_CHAR_DISCOV, is_success %d\n\r",
			   conn_id, discov_result.is_success);

		for (i = 0; i < discov_result.result_num; i++) {
			p_result_table = &(discov_result.p_result_table[i]);
			switch (p_result_table->result_type) {
			case DISC_RESULT_BY_UUID16_CHAR:
				properties = p_result_table->result_data.char_uuid16_disc_data.properties;
				APP_PRINT_INFO5("UUID16 CHAR[%d]: Characteristics by uuid16, decl hndl=0x%x, prop=0x%x, value hndl=0x%x, uuid16=<0x%x>",
								i, p_result_table->result_data.char_uuid16_disc_data.decl_handle,
								p_result_table->result_data.char_uuid16_disc_data.properties,
								p_result_table->result_data.char_uuid16_disc_data.value_handle,
								p_result_table->result_data.char_uuid16_disc_data.uuid16);
				APP_PRINT_INFO5("properties:indicate %d, read %d, write cmd %d, write %d, notify %d",
								properties & GATT_CHAR_PROP_INDICATE,
								properties & GATT_CHAR_PROP_READ,
								properties & GATT_CHAR_PROP_WRITE_NO_RSP,
								properties & GATT_CHAR_PROP_WRITE,
								properties & GATT_CHAR_PROP_NOTIFY
							   );
				printf("UUID16 CHAR[%d]: Characteristics by uuid16, decl hndl=0x%x, prop=0x%x, value hndl=0x%x, uuid16=<0x%x>\n\r",
					   i, p_result_table->result_data.char_uuid16_disc_data.decl_handle,
					   p_result_table->result_data.char_uuid16_disc_data.properties,
					   p_result_table->result_data.char_uuid16_disc_data.value_handle,
					   p_result_table->result_data.char_uuid16_disc_data.uuid16);
				printf("properties:indicate %d, read %d, write cmd %d, write %d, notify %d\n\r",
					   properties & GATT_CHAR_PROP_INDICATE,
					   properties & GATT_CHAR_PROP_READ,
					   properties & GATT_CHAR_PROP_WRITE_NO_RSP,
					   properties & GATT_CHAR_PROP_WRITE,
					   properties & GATT_CHAR_PROP_NOTIFY
					  );

				break;

			default:
				APP_PRINT_ERROR0("Invalid Discovery Result Type!");
				printf("Invalid Discovery Result Type!");
				break;
			}
		}
		break;

	case GCS_BY_UUID128_CHAR_DISCOV:
		APP_PRINT_INFO2("conn_id %d, GCS_BY_UUID128_CHAR_DISCOV, is_success %d",
						conn_id, discov_result.is_success);
		printf("conn_id %d, GCS_BY_UUID128_CHAR_DISCOV, is_success %d\n\r",
			   conn_id, discov_result.is_success);

		for (i = 0; i < discov_result.result_num; i++) {
			p_result_table = &(discov_result.p_result_table[i]);
			switch (p_result_table->result_type) {
			case DISC_RESULT_BY_UUID128_CHAR:
				properties = p_result_table->result_data.char_uuid128_disc_data.properties;
				APP_PRINT_INFO5("UUID128 CHAR[%d]: Characteristics by uuid128, decl hndl=0x%x, prop=0x%x, value hndl=0x%x, uuid128=<%b>",
								i, p_result_table->result_data.char_uuid128_disc_data.decl_handle,
								p_result_table->result_data.char_uuid128_disc_data.properties,
								p_result_table->result_data.char_uuid128_disc_data.value_handle,
								TRACE_BINARY(16, p_result_table->result_data.char_uuid128_disc_data.uuid128));
				APP_PRINT_INFO5("properties:indicate %d, read %d, write cmd %d, write %d, notify %d",
								properties & GATT_CHAR_PROP_INDICATE,
								properties & GATT_CHAR_PROP_READ,
								properties & GATT_CHAR_PROP_WRITE_NO_RSP,
								properties & GATT_CHAR_PROP_WRITE,
								properties & GATT_CHAR_PROP_NOTIFY
							   );
				printf("UUID128 CHAR[%d]: Characteristics by uuid128, decl hndl=0x%x, prop=0x%x, value hndl=0x%x, uuid128="UUID_128_FORMAT"\n\r",
					   i, p_result_table->result_data.char_uuid128_disc_data.decl_handle,
					   p_result_table->result_data.char_uuid128_disc_data.properties,
					   p_result_table->result_data.char_uuid128_disc_data.value_handle,
					   UUID_128(p_result_table->result_data.char_uuid128_disc_data.uuid128));
				printf("properties:indicate %d, read %d, write cmd %d, write %d, notify %d\n\r",
					   properties & GATT_CHAR_PROP_INDICATE,
					   properties & GATT_CHAR_PROP_READ,
					   properties & GATT_CHAR_PROP_WRITE_NO_RSP,
					   properties & GATT_CHAR_PROP_WRITE,
					   properties & GATT_CHAR_PROP_NOTIFY
					  );

				break;

			default:
				APP_PRINT_ERROR0("Invalid Discovery Result Type!");
				BLE_PRINT("Invalid Discovery Result Type!\n\r");
				break;
			}
		}
		break;

	case GCS_ALL_CHAR_DESC_DISCOV:
		APP_PRINT_INFO2("conn_id %d, GCS_ALL_CHAR_DESC_DISCOV, is_success %d\r\n",
						conn_id, discov_result.is_success);
		printf("conn_id %d, GCS_ALL_CHAR_DESC_DISCOV, is_success %d\n\r",
			   conn_id, discov_result.is_success);
		for (i = 0; i < discov_result.result_num; i++) {
			p_result_table = &(discov_result.p_result_table[i]);
			switch (p_result_table->result_type) {
			case DISC_RESULT_CHAR_DESC_UUID16:
				APP_PRINT_INFO3("DESC UUID16[%d]: Descriptors handle=0x%x, uuid16=<0x%x>",
								i, p_result_table->result_data.char_desc_uuid16_disc_data.handle,
								p_result_table->result_data.char_desc_uuid16_disc_data.uuid16);
				printf("DESC UUID16[%d]: Descriptors handle=0x%x, uuid16=<0x%x>\n\r",
					   i, p_result_table->result_data.char_desc_uuid16_disc_data.handle,
					   p_result_table->result_data.char_desc_uuid16_disc_data.uuid16);
				
				break;
			case DISC_RESULT_CHAR_DESC_UUID128:
				APP_PRINT_INFO3("DESC UUID128[%d]: Descriptors handle=0x%x, uuid128=<%b>",
								i, p_result_table->result_data.char_desc_uuid128_disc_data.handle,
								TRACE_BINARY(16, p_result_table->result_data.char_desc_uuid128_disc_data.uuid128));
				printf("DESC UUID128[%d]: Descriptors handle=0x%x, uuid128="UUID_128_FORMAT"\n\r",
					   i, p_result_table->result_data.char_desc_uuid128_disc_data.handle,
					   UUID_128(p_result_table->result_data.char_desc_uuid128_disc_data.uuid128));
				
				break;

			default:
				APP_PRINT_ERROR0("Invalid Discovery Result Type!");
				printf("Invalid Discovery Result Type!\n\r");
				break;
			}
		}
		break;

	default:
		APP_PRINT_ERROR2("Invalid disc type: conn_id %d, discov_type %d",
						 conn_id, discov_result.discov_type);
		printf("Invalid disc type: conn_id %d, discov_type %d\n\r",
			   conn_id, discov_result.discov_type);
		break;
	}
}
/**
 * @brief  Callback will be called when data sent from gcs client.
 * @param  client_id the ID distinguish which module sent the data.
 * @param  conn_id connection ID.
 * @param  p_data  pointer to data.
 * @retval   result @ref T_APP_RESULT
 */
T_APP_RESULT ble_matter_adapter_gcs_client_callback(T_CLIENT_ID client_id, uint8_t conn_id, void *p_data)
{
	T_APP_RESULT  result = APP_RESULT_SUCCESS;
	APP_PRINT_INFO2("ble_matter_adapter_gcs_client_callback: client_id %d, conn_id %d",
					client_id, conn_id);
	if (client_id == ble_matter_adapter_gcs_client_id) {
		T_GCS_CLIENT_CB_DATA *p_gcs_cb_data = (T_GCS_CLIENT_CB_DATA *)p_data;
		switch (p_gcs_cb_data->cb_type) {
		case GCS_CLIENT_CB_TYPE_DISC_RESULT:
			ble_matter_adapter_gcs_handle_discovery_result(conn_id, p_gcs_cb_data->cb_content.discov_result);
			break;
		case GCS_CLIENT_CB_TYPE_READ_RESULT:
			APP_PRINT_INFO3("READ RESULT: cause 0x%x, handle 0x%x, value_len %d",
							p_gcs_cb_data->cb_content.read_result.cause,
							p_gcs_cb_data->cb_content.read_result.handle,
							p_gcs_cb_data->cb_content.read_result.value_size);
			printf("READ RESULT: cause 0x%x, handle 0x%x, value_len %d\n\r",
				   p_gcs_cb_data->cb_content.read_result.cause,
				   p_gcs_cb_data->cb_content.read_result.handle,
				   p_gcs_cb_data->cb_content.read_result.value_size);

			if (p_gcs_cb_data->cb_content.read_result.cause == GAP_SUCCESS) {
				APP_PRINT_INFO1("READ VALUE: %b",
								TRACE_BINARY(p_gcs_cb_data->cb_content.read_result.value_size,
											 p_gcs_cb_data->cb_content.read_result.p_value));
				printf("READ VALUE: ");
				for (int i = 0; i < p_gcs_cb_data->cb_content.read_result.value_size; i++) {
					printf("0x%2x ", *(p_gcs_cb_data->cb_content.read_result.p_value + i));
				}
				printf("\n\r");
			}

			break;
		case GCS_CLIENT_CB_TYPE_WRITE_RESULT:
			APP_PRINT_INFO3("WRITE RESULT: cause 0x%x, handle 0x%x, type %d",
							p_gcs_cb_data->cb_content.write_result.cause,
							p_gcs_cb_data->cb_content.write_result.handle,
							p_gcs_cb_data->cb_content.write_result.type);
			printf("WRITE RESULT: cause 0x%x, handle 0x%x, type %d\n\r",
				   p_gcs_cb_data->cb_content.write_result.cause,
				   p_gcs_cb_data->cb_content.write_result.handle,
				   p_gcs_cb_data->cb_content.write_result.type);
			break;
		case GCS_CLIENT_CB_TYPE_NOTIF_IND:
			if (p_gcs_cb_data->cb_content.notif_ind.notify == false) {
				APP_PRINT_INFO2("INDICATION: handle 0x%x, value_size %d",
								p_gcs_cb_data->cb_content.notif_ind.handle,
								p_gcs_cb_data->cb_content.notif_ind.value_size);
				APP_PRINT_INFO1("INDICATION VALUE: %b",
								TRACE_BINARY(p_gcs_cb_data->cb_content.notif_ind.value_size,
											 p_gcs_cb_data->cb_content.notif_ind.p_value));
				printf("INDICATION: handle 0x%x, value_size %d\r\n",
					   p_gcs_cb_data->cb_content.notif_ind.handle,
					   p_gcs_cb_data->cb_content.notif_ind.value_size);
				printf("INDICATION VALUE: ");
				for (int i = 0; i < p_gcs_cb_data->cb_content.notif_ind.value_size; i++) {
					printf("0x%2x ", *(p_gcs_cb_data->cb_content.notif_ind.p_value + i));
				}
				printf("\n\r");
			} else {
				APP_PRINT_INFO2("NOTIFICATION: handle 0x%x, value_size %d",
								p_gcs_cb_data->cb_content.notif_ind.handle,
								p_gcs_cb_data->cb_content.notif_ind.value_size);
				APP_PRINT_INFO1("NOTIFICATION VALUE: %b",
								TRACE_BINARY(p_gcs_cb_data->cb_content.notif_ind.value_size,
											 p_gcs_cb_data->cb_content.notif_ind.p_value));
				printf("NOTIFICATION: handle 0x%x, value_size %d\r\n",
					   p_gcs_cb_data->cb_content.notif_ind.handle,
					   p_gcs_cb_data->cb_content.notif_ind.value_size);
				printf("NOTIFICATION VALUE: ");
				for (int i = 0; i < p_gcs_cb_data->cb_content.notif_ind.value_size; i++) {
					printf("0x%2x ", *(p_gcs_cb_data->cb_content.notif_ind.p_value + i));
				}
				printf("\n\r");
			}
			break;
		default:
			break;
		}
	}

	return result;
}

/**
  * @brief Callback for gap le to notify app
  * @param[in] cb_type callback msy type @ref GAP_LE_MSG_Types.
  * @param[in] p_cb_data point to callback data @ref T_LE_CB_DATA.
  * @retval result @ref T_APP_RESULT
  */
T_APP_RESULT ble_matter_adapter_app_gap_callback(uint8_t cb_type, void *p_cb_data)
{
	T_APP_RESULT result = APP_RESULT_SUCCESS;
	T_LE_CB_DATA *p_data = (T_LE_CB_DATA *)p_cb_data;
	char adv_type[20];
	char remote_addr_type[10];

	switch (cb_type) {
	    /* common msg*/
	    case GAP_MSG_LE_READ_RSSI:
		APP_PRINT_INFO3("GAP_MSG_LE_READ_RSSI:conn_id 0x%x cause 0x%x rssi %d",
		                p_data->p_le_read_rssi_rsp->conn_id,
		                p_data->p_le_read_rssi_rsp->cause,
		                p_data->p_le_read_rssi_rsp->rssi);
		break;

	    case GAP_MSG_LE_BOND_MODIFY_INFO:
		APP_PRINT_INFO1("GAP_MSG_LE_BOND_MODIFY_INFO: type 0x%x",
		                p_data->p_le_bond_modify_info->type);
		break;

	/* central reference msg*/
	    case GAP_MSG_LE_SCAN_INFO:
		APP_PRINT_INFO5("GAP_MSG_LE_SCAN_INFO:adv_type 0x%x, bd_addr %s, remote_addr_type %d, rssi %d, data_len %d",
		                p_data->p_le_scan_info->adv_type,
		                TRACE_BDADDR(p_data->p_le_scan_info->bd_addr),
		                p_data->p_le_scan_info->remote_addr_type,
		                p_data->p_le_scan_info->rssi,
		                p_data->p_le_scan_info->data_len);

		if (bt_matter_device_matter_scan_state) {
		    /* If you want to parse the scan info, please reference function ble_central_app_parse_scan_info. */
		    sprintf(adv_type,"%s",(p_data->p_le_scan_info->adv_type ==GAP_ADV_EVT_TYPE_UNDIRECTED)? "CON_UNDIRECT":
		                          (p_data->p_le_scan_info->adv_type ==GAP_ADV_EVT_TYPE_DIRECTED)? "CON_DIRECT":
		                          (p_data->p_le_scan_info->adv_type ==GAP_ADV_EVT_TYPE_SCANNABLE)? "SCANABLE_UNDIRCT":
		                          (p_data->p_le_scan_info->adv_type ==GAP_ADV_EVT_TYPE_NON_CONNECTABLE)? "NON_CONNECTABLE":
		                          (p_data->p_le_scan_info->adv_type ==GAP_ADV_EVT_TYPE_SCAN_RSP)? "SCAN_RSP":"unknown");
		    sprintf(remote_addr_type,"%s",(p_data->p_le_scan_info->remote_addr_type == GAP_REMOTE_ADDR_LE_PUBLIC)? "public":
		                          (p_data->p_le_scan_info->remote_addr_type == GAP_REMOTE_ADDR_LE_RANDOM)? "random":"unknown");

		    BLE_PRINT("ADVType\t\t\t| AddrType\t|%s\t\t\t|rssi\n\r","BT_Addr");
		    BLE_PRINT("%s\t\t%s\t"BD_ADDR_FMT"\t%d\n\r",adv_type,remote_addr_type,BD_ADDR_ARG(p_data->p_le_scan_info->bd_addr),
		                                            p_data->p_le_scan_info->rssi);

		    bt_matter_device_matter_app_parse_scan_info(p_data->p_le_scan_info);
		}
		//gap_sched_handle_adv_report(p_data->p_le_scan_info);
		break;


	#if F_BT_LE_4_2_DATA_LEN_EXT_SUPPORT
	    case GAP_MSG_LE_DATA_LEN_CHANGE_INFO:
		APP_PRINT_INFO3("GAP_MSG_LE_DATA_LEN_CHANGE_INFO: conn_id %d, tx octets 0x%x, max_tx_time 0x%x",
		                p_data->p_le_data_len_change_info->conn_id,
		                p_data->p_le_data_len_change_info->max_tx_octets,
		                p_data->p_le_data_len_change_info->max_tx_time);
		break;
	#endif

	#if F_BT_LE_GAP_CENTRAL_SUPPORT
	    case GAP_MSG_LE_CONN_UPDATE_IND:
		APP_PRINT_INFO5("GAP_MSG_LE_CONN_UPDATE_IND: conn_id %d, conn_interval_max 0x%x, conn_interval_min 0x%x, conn_latency 0x%x,supervision_timeout 0x%x",
		                p_data->p_le_conn_update_ind->conn_id,
		                p_data->p_le_conn_update_ind->conn_interval_max,
		                p_data->p_le_conn_update_ind->conn_interval_min,
		                p_data->p_le_conn_update_ind->conn_latency,
		                p_data->p_le_conn_update_ind->supervision_timeout);
		/* if reject the proposed connection parameter from peer device, use APP_RESULT_REJECT. */
		result = APP_RESULT_ACCEPT;
		break;

	    case GAP_MSG_LE_SET_HOST_CHANN_CLASSIF:
		APP_PRINT_INFO1("GAP_MSG_LE_SET_HOST_CHANN_CLASSIF: cause 0x%x",
		                p_data->p_le_set_host_chann_classif_rsp->cause);
		break;
	#endif
#if F_BT_LE_5_0_SET_PHY_SUPPORT
	case GAP_MSG_LE_PHY_UPDATE_INFO:
		APP_PRINT_INFO4("GAP_MSG_LE_PHY_UPDATE_INFO:conn_id %d, cause 0x%x, rx_phy %d, tx_phy %d",
						p_data->p_le_phy_update_info->conn_id,
						p_data->p_le_phy_update_info->cause,
						p_data->p_le_phy_update_info->rx_phy,
						p_data->p_le_phy_update_info->tx_phy);
		printf("GAP_MSG_LE_PHY_UPDATE_INFO:conn_id %d, cause 0x%x, rx_phy %d, tx_phy %d\n\r",
			   p_data->p_le_phy_update_info->conn_id,
			   p_data->p_le_phy_update_info->cause,
			   p_data->p_le_phy_update_info->rx_phy,
			   p_data->p_le_phy_update_info->tx_phy);

		break;

	case GAP_MSG_LE_REMOTE_FEATS_INFO: {
		uint8_t  remote_feats[8];
		APP_PRINT_INFO3("GAP_MSG_LE_REMOTE_FEATS_INFO: conn id %d, cause 0x%x, remote_feats %b",
						p_data->p_le_remote_feats_info->conn_id,
						p_data->p_le_remote_feats_info->cause,
						TRACE_BINARY(8, p_data->p_le_remote_feats_info->remote_feats));
		if (p_data->p_le_remote_feats_info->cause == GAP_SUCCESS) {
			memcpy(remote_feats, p_data->p_le_remote_feats_info->remote_feats, 8);
			if (remote_feats[LE_SUPPORT_FEATURES_MASK_ARRAY_INDEX1] & LE_SUPPORT_FEATURES_LE_2M_MASK_BIT) {
				APP_PRINT_INFO0("GAP_MSG_LE_REMOTE_FEATS_INFO: support 2M");
				printf("GAP_MSG_LE_REMOTE_FEATS_INFO: support 2M\n\r");
			}
			if (remote_feats[LE_SUPPORT_FEATURES_MASK_ARRAY_INDEX1] & LE_SUPPORT_FEATURES_LE_CODED_PHY_MASK_BIT) {
				APP_PRINT_INFO0("GAP_MSG_LE_REMOTE_FEATS_INFO: support CODED");
				printf("GAP_MSG_LE_REMOTE_FEATS_INFO: support CODED\n\r");
			}
		}
	}
	break;
#endif

	case GAP_MSG_LE_MODIFY_WHITE_LIST:
		APP_PRINT_INFO2("GAP_MSG_LE_MODIFY_WHITE_LIST: operation  0x%x, cause 0x%x",
						p_data->p_le_modify_white_list_rsp->operation,
						p_data->p_le_modify_white_list_rsp->cause);
		printf("GAP_MSG_LE_MODIFY_WHITE_LIST: operation  0x%x, cause 0x%x\r\n",
			   p_data->p_le_modify_white_list_rsp->operation,
			   p_data->p_le_modify_white_list_rsp->cause);
		break;

	case GAP_MSG_LE_ADV_UPDATE_PARAM:
		APP_PRINT_INFO1("GAP_MSG_LE_ADV_UPDATE_PARAM: cause 0x%x",
					  p_data->p_le_adv_update_param_rsp->cause);
		//printf("GAP_MSG_LE_ADV_UPDATE_PARAM: cause 0x%x\r\n",
					 // p_data->p_le_adv_update_param_rsp->cause);

		if (p_data->p_le_adv_update_param_rsp->cause == 0) {
			uint8_t adv_stop_flag = 0;
			adv_stop_flag = ble_matter_adapter_judge_adv_stop(matter_multi_adapter.adv_id);
			if (adv_stop_flag) {
				printf("[%s]stop adv flag[%d] is set, give sem and break\r\n", __func__, matter_multi_adapter.adv_id);
				matter_multi_adapter.adv_id = MAX_ADV_NUMBER;
				if(matter_multi_adapter.sem_handle)
					os_sem_give(matter_multi_adapter.sem_handle);
				break;
			}
#if BT_VENDOR_CMD_ONE_SHOT_SUPPORT
			T_GAP_CAUSE ret = GAP_CAUSE_SUCCESS;
			ret = le_vendor_one_shot_adv();
			if (ret != GAP_CAUSE_SUCCESS) {
				printf(" fail! ret = 0x%x\r\n", ret);
			}
#endif
		}

		APP_PRINT_INFO1("GAP_MSG_LE_ADV_UPDATE_PARAM: cause 0x%x",
		                p_data->p_le_adv_update_param_rsp->cause);
		//gap_sched_adv_params_set_done();
		break;

	default:
		APP_PRINT_ERROR1("ble_matter_adapter_app_gap_callback: unhandled cb_type 0x%x", cb_type);
		break;
	}
	return result;
}
/**
 * @brief  Callback will be called when data sent from profile client layer.
 * @param  client_id the ID distinguish which module sent the data.
 * @param  conn_id connection ID.
 * @param  p_data  pointer to data.
 * @retval   result @ref T_APP_RESULT
 */
T_APP_RESULT ble_matter_adapter_app_client_callback(T_CLIENT_ID client_id, uint8_t conn_id, void *p_data)
{
    T_APP_RESULT  result = APP_RESULT_SUCCESS;
    APP_PRINT_INFO2("bt_mesh_device_matter_app_client_callback: client_id %d, conn_id %d",
                    client_id, conn_id);
    if (client_id == CLIENT_PROFILE_GENERAL_ID)
    {
        T_CLIENT_APP_CB_DATA *p_client_app_cb_data = (T_CLIENT_APP_CB_DATA *)p_data;
        switch (p_client_app_cb_data->cb_type)
        {
        case CLIENT_APP_CB_TYPE_DISC_STATE:
            if (p_client_app_cb_data->cb_content.disc_state_data.disc_state == DISC_STATE_SRV_DONE)
            {
                APP_PRINT_INFO0("Discovery All Service Procedure Done.");
            }
            else
            {
                APP_PRINT_INFO0("Discovery state send to application directly.");
            }
            break;
        case CLIENT_APP_CB_TYPE_DISC_RESULT:
            if (p_client_app_cb_data->cb_content.disc_result_data.result_type == DISC_RESULT_ALL_SRV_UUID16)
            {
                APP_PRINT_INFO3("Discovery All Primary Service: UUID16 0x%x, start handle 0x%x, end handle 0x%x.",
                                p_client_app_cb_data->cb_content.disc_result_data.result_data.p_srv_uuid16_disc_data->uuid16,
                                p_client_app_cb_data->cb_content.disc_result_data.result_data.p_srv_uuid16_disc_data->att_handle,
                                p_client_app_cb_data->cb_content.disc_result_data.result_data.p_srv_uuid16_disc_data->end_group_handle);
            }
            else
            {
                APP_PRINT_INFO0("Discovery result send to application directly.");
            }
            break;
        default:
            break;
        }

    }
    return result;
}

/** @defgroup  PERIPH_SEVER_CALLBACK Profile Server Callback Event Handler
    * @brief Handle profile server callback event
    * @{
    */
/**
    * @brief    All the BT Profile service callback events are handled in this function
    * @note     Then the event handling function shall be called according to the
    *           service_id
    * @param    service_id  Profile service ID
    * @param    p_data      Pointer to callback data
    * @return   T_APP_RESULT, which indicates the function call is successful or not
    * @retval   APP_RESULT_SUCCESS  Function run successfully
    * @retval   others              Function run failed, and return number indicates the reason
    */
T_APP_RESULT ble_matter_adapter_app_profile_callback(T_SERVER_ID service_id, void *p_data)
{
	T_APP_RESULT app_result = APP_RESULT_SUCCESS;
	if (service_id == SERVICE_PROFILE_GENERAL_ID) {
		T_SERVER_APP_CB_DATA *p_param = (T_SERVER_APP_CB_DATA *)p_data;
		switch (p_param->eventId) {
		case PROFILE_EVT_SRV_REG_COMPLETE:// srv register result event.
			APP_PRINT_INFO1("PROFILE_EVT_SRV_REG_COMPLETE: result %d",
							p_param->event_data.service_reg_result);
			break;
		case PROFILE_EVT_SEND_DATA_COMPLETE:
			APP_PRINT_INFO5("PROFILE_EVT_SEND_DATA_COMPLETE: conn_id %d, cause 0x%x, service_id %d, attrib_idx 0x%x, credits %d",
							p_param->event_data.send_data_result.conn_id,
							p_param->event_data.send_data_result.cause,
							p_param->event_data.send_data_result.service_id,
							p_param->event_data.send_data_result.attrib_idx,
							p_param->event_data.send_data_result.credits);
			printf("PROFILE_EVT_SEND_DATA_COMPLETE: conn_id %d, cause 0x%x, service_id %d, attrib_idx 0x%x, credits %d\r\n",
				   p_param->event_data.send_data_result.conn_id,
				   p_param->event_data.send_data_result.cause,
				   p_param->event_data.send_data_result.service_id,
				   p_param->event_data.send_data_result.attrib_idx,
				   p_param->event_data.send_data_result.credits);
			if (p_param->event_data.send_data_result.cause == GAP_SUCCESS) {
				APP_PRINT_INFO0("PROFILE_EVT_SEND_DATA_COMPLETE success");
				printf("PROFILE_EVT_SEND_DATA_COMPLETE success\r\n");
//send msg to matter

				if (p_param->event_data.send_data_result.service_id == ble_matter_adapter_service_id) {
					T_MATTER_BLEMGR_TX_COMPLETE_CB_ARG *indication_complete_msg_matter = (T_MATTER_BLEMGR_TX_COMPLETE_CB_ARG *) os_mem_alloc(0, sizeof(T_MATTER_BLEMGR_TX_COMPLETE_CB_ARG));
					memset(indication_complete_msg_matter, 0, sizeof(T_MATTER_BLEMGR_TX_COMPLETE_CB_ARG));
					indication_complete_msg_matter->conn_id = p_param->event_data.send_data_result.conn_id;
					
					if (ble_matter_adapter_send_callback_msg(BLE_MATTER_MSG_SEND_DATA_COMPLETE_MULTI_ADV, NULL,indication_complete_msg_matter) == false) {
						printf("\n\r[%s] send callback msg fail\r\n", __func__);
						os_mem_free(indication_complete_msg_matter);
					}
				}
			} else {
				APP_PRINT_ERROR0("PROFILE_EVT_SEND_DATA_COMPLETE failed");
				printf("PROFILE_EVT_SEND_DATA_COMPLETE failed\r\n");
			}
			break;

		default:
			break;
		}
	} else if (service_id == ble_matter_adapter_service_id) {
		
        T_MATTER_CALLBACK_DATA *p_ms_cb_data = (T_MATTER_CALLBACK_DATA *)p_data;

        switch (p_ms_cb_data->msg_type)
        {
	
        case SERVICE_CALLBACK_TYPE_INDIFICATION_NOTIFICATION:
           {

                switch (p_ms_cb_data->msg_data.notification_indification_index)
                {
                case MATTER_NOTIFY_INDICATE_V3_ENABLE:
                    {
                        APP_PRINT_INFO0("MATTER_NOTIFY_INDICATE_V3_ENABLE");
                        printf("\n\rMATTER_NOTIFY_INDICATE_V3_ENABLE\r\n");
                        //send msg to matter
                        T_MATTER_CALLBACK_DATA *indication_notification_enable = os_mem_alloc(0, sizeof(T_MATTER_CALLBACK_DATA));

                        if(indication_notification_enable)
                        {
                            memcpy(indication_notification_enable, p_ms_cb_data, sizeof(T_MATTER_CALLBACK_DATA));
                            printf("L2055,BT_MATTER_SEND_CB_MSG_IND_NTF_ENABLE=%d\r\n",BT_MATTER_SEND_CB_MSG_IND_NTF_ENABLE);
                            //if(ble_matter_adapter_send_callback_msg(BMS_CALLBACK_MSG_CMP_CCCD_RECV_ENABLE_MATTER, service_id, indication_notification_enable)==false)
                            if(ble_matter_adapter_send_callback_msg(BLE_MATTER_MSG_CCCD_RECV_ENABLE_MULTI_ADV, service_id, indication_notification_enable)==false)
                            {
                                os_mem_free(indication_notification_enable);
                                indication_notification_enable = NULL;
                            }
                        }
                        else
                            printf("Malloc failed\r\n");
                    }
                    break;

                case MATTER_NOTIFY_INDICATE_V3_DISABLE:
                    {
                        APP_PRINT_INFO0("MATTER_NOTIFY_INDICATE_V3_DISABLE");
                        printf("\n\rMATTER_NOTIFY_INDICATE_V3_DISABLE\r\n");

                        //send msg to matter
                        T_MATTER_CALLBACK_DATA *indication_notification_disable = os_mem_alloc(0, sizeof(T_MATTER_CALLBACK_DATA));
                        if(indication_notification_disable)
                        {
                            memcpy(indication_notification_disable, p_ms_cb_data, sizeof(T_MATTER_CALLBACK_DATA));
                            //if(ble_matter_adapter_send_callback_msg(BMS_CALLBACK_MSG_CMP_CCCD_RECV_DISABLE_MATTER, service_id, indication_notification_disable)==false)
			    if(ble_matter_adapter_send_callback_msg(BLE_MATTER_MSG_CCCD_RECV_DISABLE_MULTI_ADV, service_id, indication_notification_disable)==false)
                            {
                                os_mem_free(indication_notification_disable);
                                indication_notification_disable = NULL;
                            }
                        }
                        else
                            printf("Malloc failed\r\n");
                    }
                    break;
                default:
                    break;
                }

            }
            break;
        case SERVICE_CALLBACK_TYPE_READ_CHAR_VALUE:
            {
                T_MATTER_BLEMGR_C3_CHAR_READ_CB_ARG c3_char_read_cb_arg;
                c3_char_read_cb_arg.pp_value = &p_ms_cb_data->msg_data.write_read.p_value;
                c3_char_read_cb_arg.p_len = &p_ms_cb_data->msg_data.write_read.len;
                if (matter_blemgr_callback_func) {
                    matter_blemgr_callback_func(matter_blemgr_callback_data, MATTER_BLEMGR_C3_CHAR_READ_CB, &c3_char_read_cb_arg);
                }
            }
            break;

        case SERVICE_CALLBACK_TYPE_WRITE_CHAR_VALUE:
            {
                //send msg to matter
                T_MATTER_CALLBACK_DATA *write_char_val = os_mem_alloc(0, sizeof(T_MATTER_CALLBACK_DATA));
                if(write_char_val)
                {
                    memcpy(write_char_val, p_ms_cb_data, sizeof(T_MATTER_CALLBACK_DATA));

                    //Make sure not malloc size of 0
                    if (write_char_val->msg_data.write_read.len !=0)
                    {
                        write_char_val->msg_data.write_read.p_value = os_mem_alloc(0, write_char_val->msg_data.write_read.len);
                        memcpy(write_char_val->msg_data.write_read.p_value, p_ms_cb_data->msg_data.write_read.p_value, p_ms_cb_data->msg_data.write_read.len);
                    }
                    if(ble_matter_adapter_send_callback_msg(BLE_MATTER_MSG_WRITE_CHAR_MULTI_ADV, service_id, write_char_val)==false)
                    {
                        if (write_char_val->msg_data.write_read.len !=0)
                        {
                            os_mem_free(write_char_val->msg_data.write_read.p_value);
                            write_char_val->msg_data.write_read.p_value = NULL;
                        }
                        os_mem_free(write_char_val);
                        write_char_val = NULL;
                    }
                }
                else
                    printf("Malloc failed\r\n");
            }
            break;
        default:
            break;
        }
    }

    return app_result;
}

void ble_matter_adapter_app_vendor_callback(uint8_t cb_type, void *p_cb_data)
{
	T_GAP_VENDOR_CB_DATA cb_data;
	memcpy(&cb_data, p_cb_data, sizeof(T_GAP_VENDOR_CB_DATA));
	APP_PRINT_INFO1("app_vendor_callback: command 0x%x", cb_data.p_gap_vendor_cmd_rsp->command);
	//printf();
	switch (cb_type)
	{
		case GAP_MSG_VENDOR_CMD_RSP:
			switch(cb_data.p_gap_vendor_cmd_rsp->command)
			{
#if BT_VENDOR_CMD_ONE_SHOT_SUPPORT
				case HCI_LE_VENDOR_EXTENSION_FEATURE2:
					//if(cb_data.p_gap_vendor_cmd_rsp->param[0] == HCI_EXT_SUB_ONE_SHOT_ADV)
					{
						APP_PRINT_ERROR1("One shot adv resp: cause 0x%x", cb_data.p_gap_vendor_cmd_rsp->cause);
						if (cb_data.p_gap_vendor_cmd_rsp->cause == 0) {
							if(matter_multi_adapter.sem_handle)
								os_sem_give(matter_multi_adapter.sem_handle);
						} else
							printf("One shot adv resp: cause 0x%x\r\n", cb_data.p_gap_vendor_cmd_rsp->cause);
					}
					break;
#endif
				default:
					break;
			}
			break;

		default:
			break;
	}

	return;
}

#if F_BT_GAPS_CHAR_WRITEABLE
/** @defgroup  SCATTERNET_GAPS_WRITE GAP Service Callback Handler
    * @brief Use @ref F_BT_GAPS_CHAR_WRITEABLE to open
    * @{
    */
/**
 * @brief    All the BT GAP service callback events are handled in this function
 * @param[in] service_id  Profile service ID
 * @param[in] p_para      Pointer to callback data
 * @return   Indicates the function call is successful or not
 * @retval   result @ref T_APP_RESULT
 */
T_APP_RESULT ble_matter_adapter_gap_service_callback(T_SERVER_ID service_id, void *p_para)
{
	(void) service_id;
	T_APP_RESULT  result = APP_RESULT_SUCCESS;
	T_GAPS_CALLBACK_DATA *p_gap_data = (T_GAPS_CALLBACK_DATA *)p_para;
	APP_PRINT_INFO2("gap_service_callback: conn_id = %d msg_type = %d\n", p_gap_data->conn_id,
					p_gap_data->msg_type);
	APP_PRINT_INFO2("gap_service_callback: len = 0x%x,opcode = %d\n", p_gap_data->msg_data.len,
					p_gap_data->msg_data.opcode);
	if (p_gap_data->msg_type == SERVICE_CALLBACK_TYPE_WRITE_CHAR_VALUE) {
		switch (p_gap_data->msg_data.opcode) {
		case GAPS_WRITE_DEVICE_NAME: {
			T_LOCAL_NAME device_name;
			memcpy(device_name.local_name, p_gap_data->msg_data.p_value, p_gap_data->msg_data.len);
			device_name.local_name[p_gap_data->msg_data.len] = 0;
			printf("GAPS_WRITE_DEVICE_NAME:device_name = %s\r\n", device_name.local_name);
			flash_save_local_name(&device_name);
		}
		break;

		case GAPS_WRITE_APPEARANCE: {
			uint16_t appearance_val;
			T_LOCAL_APPEARANCE appearance;
			LE_ARRAY_TO_UINT16(appearance_val, p_gap_data->msg_data.p_value);
			appearance.local_appearance = appearance_val;
			//printf("GAPS_WRITE_APPEARANCE:appearance = %s\r\n",appearance.local_appearance);
			flash_save_local_appearance(&appearance);
		}
		break;
		default:
			APP_PRINT_ERROR1("gap_service_callback: unhandled msg_data.opcode 0x%x", p_gap_data->msg_data.opcode);
			//printf("gap_service_callback: unhandled msg_data.opcode 0x%x\r\n", p_gap_data->msg_data.opcode);
			break;
		}
	}
	return result;
}
#endif
/** @} */ /* End of group PERIPH_SEVER_CALLBACK */
/** @} */ /* End of group PERIPH_APP */
#endif