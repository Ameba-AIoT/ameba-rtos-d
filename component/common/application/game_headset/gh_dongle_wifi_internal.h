#ifndef _GH_DONGLE_WIFI_INTERNAL_H
#define _GH_DONGLE_WIFI_INTERNAL_H

#include <drv_types.h>
#include "gpio_api.h"

#define VENDER "realtek"
#define VENDER_SIZE 7
#define SEQ_SIZE 2
#define ENCRYP_PARA_SIZE 8
#define WLAN_FC_TYPE_DATA		2
#define WLAN_FC_STYPE_DATA		0
#define WLAN_FC_STYPE_QOS_DATA		8
#define PACKET_SIZE 576 * 2
#define MAX_PACKET_QUEUE_SIZE 5
#define PACKET_NUM 3
#define HID_SIZE 20
#define DATA_Q_SIZE 20
#define BURST_PKT_NUM 2
#define PAIR_MODE_TIME 600

#define PAIR_PROFILE_ADDR 0x100000
#define SSID "sample_dongle"/*"Realtek 5G Gaming Headset"*/
#define PASSWORD "12345678"	
#define CHANNEL "149"

#define WIFI_TO_UAC 1
#define MUSIC_DATA_BURST 1

#define IEEE80211_FC(type, stype) host_to_le16((type << 2) | (stype << 4))

#ifdef __CHECKER__
#define __force __attribute__((force))
#define __bitwise __attribute__((bitwise))
#else
#define __force
#define __bitwise
#endif

enum {
	UAC_DATA = 0,
	HID_CMD,
	VENDER_DATA,
	VENDER_CMD,
	MIC_DATA,
	NULL_DATA,
};

enum {
	PLUG = 10,
	UNPLUG = 11,
};

enum {
	GH_ERROR = 0,
	GH_WARNING,
	GH_INFO,
	GH_DEBUG,
	GH_NONE,
};

enum {
	POWER_ON = 0,
	PAIR_MODE,
	BIND_MODE,
};

typedef struct s_buf_t{
	u8 *txbuf;
	u32 txlen;
	u32 tsf;
	char type;
}s_buf;

typedef struct cmd_buf_t{
	u8 txbuf[HID_SIZE];
	char type;
}cmd_buf;

typedef struct data_buf_t{
	u8 txbuf[PACKET_SIZE + SEQ_SIZE];
	u32 tsf;
	char type;
}data_buf;

typedef struct gh_pair_profile_t{
	char gh_ssid[20];
	char gh_password[12];
	char gh_channel[4];
	char gh_dongle_mac[6];
	char gh_hdst_mac[6];
}gh_pair_profile;

typedef struct gh_dongle_adapter {
	_xqueue music_ready_queue;
	_xqueue music_pre_queue;
	char music_queue_cnt;
	char music_pre_queue_cnt;

	u8 pair_mode;
	u16 uac_tx_count;
	u16 cmd_tx_count;
	u16 wifi_rx_index;
	u8 music_stop_flag;
	u8 music_start_cnt;
	u8 start_music_timer_flag;
	u8 connect_sync_flag;
	u32 music_stream;
	u32 cmd_queue_cnt;
	u32 cmd_pre_queue_cnt;
	u16 encrypt_pn;
	u8 music_plug_flag;
	u8 music_chann_mode;
	u8 music_sf;
	u8 music_bit_res;
	int current_channel;

	xSemaphoreHandle uac_tx_sema;
	xSemaphoreHandle mic_rx_sema;
	xSemaphoreHandle cmd_tx_sema;
	xSemaphoreHandle connect_sync_sema;

	_xqueue cmd_queue;
	_xqueue cmd_pre_queue;

	_mutex wifi_tx_mux;
	_mutex cmd_mux;
	_mutex music_mux;
}ghd_adapter;


#define GH_DEBUG_ON	1
#if GH_DEBUG_ON
#define GH_DEBUGF(level, fmt, arg...)     \
do {\
	if (level <= GH_DEBUG) {\
		if (level <= GH_ERROR) {\
			printf("\n\rerror %s, " fmt "\n\r", __func__, ##arg);\
		} \
		else if(level == GH_WARNING){\
			printf("warning %s, " fmt "\n", __func__, ##arg);\
		} \
		else if(level == GH_INFO){\
			printf("info %s, " fmt "\n", __func__, ##arg);\
		} \
		else if(level == GH_DEBUG){\
			printf("debug %s, " fmt "\n", __func__, ##arg);\
		} \
	}\
}while(0)
#else
#define GH_DEBUGF(level, fmt, arg...)
#endif

extern u8 g_dongle_uac_tx_buf[PACKET_SIZE * PACKET_NUM];
extern u8 g_dongle_mic_rx_buf[PACKET_SIZE * PACKET_NUM];
extern ghd_adapter g_ghd_adapter;

#endif /* _UTIL_H */
