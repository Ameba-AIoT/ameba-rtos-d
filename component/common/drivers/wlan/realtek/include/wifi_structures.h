/******************************************************************************
 * Copyright (c) 2013-2016 Realtek Semiconductor Corp.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
  ******************************************************************************
  * @file    wifi_structures.h
  * @author
  * @version
  * @brief   This file provides the data structures used for wlan API.
  ******************************************************************************
  */

#ifndef _WIFI_STRUCTURES_H
#define _WIFI_STRUCTURES_H

/** @addtogroup nic NIC
 *  @ingroup    wlan
 *  @brief      NIC functions
 *  @{
 */

//#include <freertos/freertos_service.h>
#include "wifi_constants.h"
#include "dlist.h"
#include "autoconf.h"
#ifdef	__cplusplus
extern "C" {
#endif

#if defined(__IAR_SYSTEMS_ICC__) || defined (__GNUC__) || defined(__CC_ARM) || (defined(__ARMCC_VERSION) && (__ARMCC_VERSION >= 6010050))
#pragma pack(1)
#endif

/**
  * @brief  The structure is used to describe the SSID.
  */
typedef struct rtw_ssid {
    unsigned char len;     /**< SSID length */
    unsigned char val[33]; /**< SSID name (AP name)  */
} rtw_ssid_t;
#if defined(__IAR_SYSTEMS_ICC__) || defined (__GNUC__) || defined(__CC_ARM) || (defined(__ARMCC_VERSION) && (__ARMCC_VERSION >= 6010050))
#pragma pack()
#endif

#if defined(__IAR_SYSTEMS_ICC__) || defined (__GNUC__) || defined(__CC_ARM) || (defined(__ARMCC_VERSION) && (__ARMCC_VERSION >= 6010050))
#pragma pack(1)
#endif

/**
  * @brief  The structure is used to describe the unique 6-byte MAC address.
  */
typedef struct rtw_mac {
    unsigned char octet[6]; /**< Unique 6-byte MAC address */
} rtw_mac_t;
#if defined(__IAR_SYSTEMS_ICC__) || defined (__GNUC__) || defined(__CC_ARM) || (defined(__ARMCC_VERSION) && (__ARMCC_VERSION >= 6010050))
#pragma pack()
#endif

/**
  * @brief  The structure is used to describe the setting about SSID,
  *			security type, password and default channel, used to start AP mode.
  * @note  The data length of string pointed by ssid should not exceed 32, 
  *        and the data length of string pointed by password should not exceed 64.
  */
typedef struct rtw_ap_info {
	rtw_ssid_t 			ssid;
	rtw_security_t		security_type;
	unsigned char 		*password;
	int 				password_len;
	int					channel;
}rtw_ap_info_t;

/**
  * @brief  The structure is used to describe the station mode setting about SSID, 
  *			security type and password, used when connecting to an AP.
  * @note  The data length of string pointed by ssid should not exceed 32, 
  *        and the data length of string pointed by password should not exceed 64.
  */
typedef struct rtw_network_info {
	rtw_ssid_t 			ssid;
	rtw_mac_t			bssid;
	rtw_security_t		security_type;
	unsigned char 		*password;
	int 				password_len;
	int					key_id;
}rtw_network_info_t;

#if defined(__IAR_SYSTEMS_ICC__) || defined(__GNUC__) || defined(__CC_ARM) || (defined(__ARMCC_VERSION) && (__ARMCC_VERSION >= 6010050))
#pragma pack(1)
#endif

/**
  * @brief  The structure is used to describe the scan result of the AP.
  */
typedef struct rtw_scan_result {
    rtw_ssid_t              SSID;             /**< Service Set Identification (i.e. Name of Access Point)                    */
    rtw_mac_t               BSSID;            /**< Basic Service Set Identification (i.e. MAC address of Access Point)       */
    signed short		                  signal_strength;  /**< Receive Signal Strength Indication in dBm. <-90=Very poor, >-30=Excellent */
    rtw_bss_type_t          bss_type;         /**< Network type                                                              */
    rtw_security_t          security;         /**< Security type                                                             */
    rtw_wps_type_t          wps_type;         /**< WPS type                                                                  */
    unsigned int                      channel;          /**< Radio channel that the AP beacon was received on                          */
    rtw_802_11_band_t       band;             /**< Radio band                                                                */                                        
} rtw_scan_result_t;
#if defined(__IAR_SYSTEMS_ICC__) || defined(__GNUC__) || defined(__CC_ARM) || (defined(__ARMCC_VERSION) && (__ARMCC_VERSION >= 6010050))
#pragma pack()
#endif

/**
  * @brief  The structure is used to describe the data needed by scan result handler function.
  */
typedef struct rtw_scan_handler_result {
    rtw_scan_result_t ap_details;
    rtw_bool_t        scan_complete;
    void*               user_data;

} rtw_scan_handler_result_t;

#if defined(__IAR_SYSTEMS_ICC__) || defined(__GNUC__) || defined(__CC_ARM) || (defined(__ARMCC_VERSION) && (__ARMCC_VERSION >= 6010050))
#pragma pack(1)
#endif

/**
  * @brief  The structure is used to store the WIFI setting gotten from WIFI driver.
  */
typedef struct rtw_wifi_setting {
	rtw_mode_t		mode;
	unsigned char 		ssid[33];
	unsigned char		channel;
	rtw_security_t		security_type;
	unsigned char 		password[RTW_MAX_PSK_LEN+1];
	unsigned char		key_idx;

}rtw_wifi_setting_t;
#if defined(__IAR_SYSTEMS_ICC__) || defined(__GNUC__) || defined(__CC_ARM) || (defined(__ARMCC_VERSION) && (__ARMCC_VERSION >= 6010050))
#pragma pack()
#endif

/**
  * @brief  The structure is used to describe the setting when configure the network.
  */
typedef struct rtw_wifi_config {
	unsigned int		boot_mode;
	unsigned char 		ssid[32];
	unsigned char		ssid_len;
	unsigned char		security_type;
	unsigned char		password[RTW_MAX_PSK_LEN+1];
	unsigned char		password_len;
	unsigned char		channel;
} rtw_wifi_config_t;

/**
  * @brief  The structure is used to describe the maclist.
  */
typedef struct
{
    unsigned int    count;         /**< Number of MAC addresses in the list    */
    rtw_mac_t mac_list[1];   /**< Variable length array of MAC addresses */
} rtw_maclist_t;

/**
  * @brief  The structure is used to describe the ap client information.
  */
typedef struct
{
    unsigned int    count;         /**< Number of MAC addresses in the list    */
    rtw_mac_t mac_list[AP_STA_NUM];     /**< MAC addresses array */
    signed char rssi_list[AP_STA_NUM];    /**< RSSI array */
} rtw_ap_client_info_t;

/**
  * @brief  The structure is used to describe the bss info of the network.\n
  *			It include the version, BSSID, beacon_period, capability, SSID,
  *			channel, atm_window, dtim_period, RSSI e.g.
  */
typedef struct {
    unsigned int      version;                /**< version field */
    unsigned int      length;                 /**< byte length of data in this record,   */
                                          /* starting at version and including IEs */
    rtw_mac_t     BSSID;
    unsigned short      beacon_period;          /**< units are Kusec */
    unsigned short      capability;             /**< Capability information */
    unsigned char       SSID_len;
    unsigned char       SSID[32];
    unsigned char       channel;
//    struct {
//        uint32_t    count;                /* # rates in this set */
//        uint8_t   rates[16];              /* rates in 500kbps units w/hi bit set if basic */
//    } rateset;                            /* supported rates */
//    rtw_chanspec_t  chanspec;              /* chanspec for bss */
    unsigned short      atim_window;            /**< units are Kusec */
    unsigned char       dtim_period;            /**< DTIM period */
    signed short       RSSI;                   /**< receive signal strength (in dBm) */

    unsigned char       n_cap;                  /**< BSS is 802.11N Capable */
    unsigned int      nbss_cap;               /**< 802.11N BSS Capabilities (based on HT_CAP_*) */
    unsigned char       basic_mcs[MCSSET_LEN];  /**< 802.11N BSS required MCS set */

    unsigned short      ie_offset;              /**< offset at which IEs start, from beginning */
    unsigned int      ie_length;             /**< byte length of Information Elements */
} rtw_bss_info_t;

/**
  * @brief  The structure is used to set WIFI packet filter pattern.
  */
typedef struct {
	unsigned short	offset;     /**< Offset in bytes to start filtering (referenced to the start of the ethernet packet) */
	unsigned short	mask_size;  /**< Size of the mask in bytes */
	unsigned char*	mask;       /**< Pattern mask bytes to be ANDed with the pattern eg. "\xff00" (must be in network byte order) */
	unsigned char*	pattern;    /**< Pattern bytes used to filter eg. "\x0800"  (must be in network byte order) */
} rtw_packet_filter_pattern_t;

typedef struct ieee80211_frame_info{
	unsigned short i_fc;
	unsigned short i_dur;
	unsigned char i_addr1[6];
	unsigned char i_addr2[6];
	unsigned char i_addr3[6];
	unsigned short i_seq;
	unsigned char bssid[6];
	unsigned char encrypt;
	signed char rssi;
#if defined(CONFIG_UNSUPPORT_PLCPHDR_RPT) && CONFIG_UNSUPPORT_PLCPHDR_RPT
	rtw_rx_type_t type;
#endif
	unsigned char data_rate;
}ieee80211_frame_info_t;

#if defined(CONFIG_UNSUPPORT_PLCPHDR_RPT) && CONFIG_UNSUPPORT_PLCPHDR_RPT
typedef struct rtw_rx_info {
	unsigned short length;	// length without FCS
	unsigned char filter;		// 1: HT-20 2T and not LDPC pkt; 2: HT-40 2T and not LDPC pkt; 3: LDPC pkt
	signed char rssi;	// -128~-1
	unsigned short channel;	// channel which this pkt in
	unsigned char agg:1;		// aggregation pkt or not. If an AMPDU contains only one MPDU then above 'length' is the antual pkt length without FCS, buuut if it contains multiple MPDUs then above 'length' is useless because it cannot tell how many MPDUs are contained and how long is each MPDU.
	unsigned char mcs:7;		// mcs index
}rtw_rx_info_t;


struct rtw_plcp_info {
	struct rtw_plcp_info *prev;
	struct rtw_plcp_info *next;
	rtw_rx_info_t rtw_plcp_info;

};

struct rtw_rx_buffer {
	struct rtw_plcp_info *head;
	struct rtw_plcp_info *tail;
};

#endif

typedef struct {
	char filter_id;
	rtw_packet_filter_pattern_t patt;
	rtw_packet_filter_rule_t rule;
	unsigned char enable;
}rtw_packet_filter_info_t;

typedef struct rtw_mac_filter_list{
	struct list_head node;
	unsigned char mac_addr[6];
}rtw_mac_filter_list_t;

typedef struct wowlan_pattern {
	unsigned char eth_da[6];
	unsigned char eth_sa[6];
	unsigned char eth_proto_type[2];
	unsigned char header_len[1];
	//unsigned char header_content[8];
	unsigned char ip_proto[1];
	//unsigned char checksum[2];
	unsigned char ip_sa[4];
	unsigned char ip_da[4];
    unsigned char src_port[2];
    unsigned char dest_port[2];
	unsigned char mask[5];
} wowlan_pattern_t;

typedef struct rtw_fw_txrpt_stats {
  unsigned long tx_rty_cnt;             // tx retry count (due to HW limit, the retry times >=4 is calculated as 4)
  int txrpt_rty_cnt_ready;              // used to sync txrpt retry count ready
  unsigned long tx_ok_cnt;
  unsigned long tx_drop_cnt;
}rtw_fw_txrpt_stats_t;

/**
  * @brief This structure is used to describe the connection status and status code for management frames only
  */
struct rtw_connection_info {
	unsigned char auth_alg;             // authentication algorithm
	unsigned char auth_code;            // authentication status code
	unsigned char assoc_code;           // association status code
	unsigned char disassoc_code;        // deauthentication/disassociation status code
};

#ifdef	__cplusplus
}
#endif

/*\@}*/

#endif /* _WIFI_STRUCTURES_H */
