#ifndef _RTSP_API_H_
#define _RTSP_API_H_

#include "dlist.h"
#include "basic_types.h"
#include "osdep_service.h"
#if defined(CONFIG_PLATFORM_8195A)
#include "mmf_dbg.h"
#endif
#if defined(CONFIG_PLATFORM_8195BHP) || defined(CONFIG_PLATFORM_8721D)
#include "mmf2_dbg.h"
#endif
#include "rtsp/rtp_api.h"
#include "avcodec.h"
#include "rtsp/sdp.h"

//RTSPS
#define SUPPORT_RTSPS

//Trigger for softphone feature, when open it, run sip example and close rtsp example
//#define ENABLE_SIP_MMFV2
#define ENABLE_PROXY_SEVER 0

#if ENABLE_PROXY_SEVER
#define PROXY_SERVER_IP	"52.198.104.214"
#define PROXY_SERVER_PORT 5000
#endif

/* clock usage */
#define RTSP_DEPEND_CLK_HZ      configTICK_RATE_HZ      

#define RTSP_SERVICE_PRIORITY    3
#define RTSP_MAX_STREAM_NUM	2
#define RTSP_REQUEST_BUF_SIZE   MAX_SDP_SIZE	//512
#define RTSP_RESPONSE_BUF_SIZE 	MAX_SDP_SIZE //max size for response buffer  
#define RTSP_STREAM_NAME_SIZE	128
#define DEF_RTSP_PORT 554
#define DEF_HTTP_PORT 5008

#define RTSP_SELECT_SOCK 8 

/*rtsp request type list*/
#define REQUEST_OPTIONS 1
#define REQUEST_DESCRIBE 2
#define REQUEST_ANNOUNCE 3
#define REQUEST_SETUP 4
#define REQUEST_PLAY 5
#define REQUEST_RECORD 6
#define REQUEST_PAUSE 7
#define REQUEST_GET_PARAM 8
#define REQUEST_SET_PARAM 9
#define REQUEST_REDIRECT 10
#define REQUEST_TEARDOWN 11
#define RTSP_INTERLEAVED_FRAME 12
#define RTSP_KEEPALIVE 13
#define RTSP_QUIT 14

/*rtsp cast mode list*/
#define UNICAST_UDP_MODE 1
#define UNICAST_TCP_MODE 2
#define MULTICAST_MODE 3

#define BOUNDARY "amebaimagetest"

/*RTSP KEEPALIVE TIMEOUT ENABLE*/
//#define KEEPALIVE_TIMEOUT_ENABLE

#define RTSP_ERR_PREFIX         "[RTSP Err]"
#define RTSP_WARN_PREFIX        "[RTSP Wrn]"
#define RTSP_INFO_PREFIX        "[RTSP Inf]"

#define RTSP_DBG_MODE_INFO 0
#define RTSP_DBG_MODE_WARN 1
#define RTSP_DBG_MODE_ERR 2

extern int ConfigDebugRTSPErr;
extern int ConfigDebugRTSPWarn;
extern int ConfigDebugRTSPInfo;

#define RTSP_DBG_ERROR(...)													\
			do {															\
				if (ConfigDebugRTSPErr)										\
					printf("\n\r" RTSP_ERR_PREFIX __VA_ARGS__);				\
			}while(0)

#define RTSP_DBG_WARNING(...)												\
			do {															\
				if (ConfigDebugRTSPWarn)									\
					printf("\n\r" RTSP_WARN_PREFIX __VA_ARGS__);			\
			}while(0)

#define RTSP_DBG_INFO(...)													\
			do {															\
				if (ConfigDebugRTSPInfo)									\
					printf("\n\r" RTSP_INFO_PREFIX __VA_ARGS__);			\
			}while(0)

enum _rtsp_state {
	RTSP_INIT = 0,
	RTSP_READY = 1,
	RTSP_PLAYING = 2,
};
typedef enum _rtsp_state rtsp_state;

struct rtsp_session
{
	uint32_t id;
	uint8_t version;
	uint32_t start_time;
	uint32_t end_time;
	uint8_t *user;
	uint8_t *name;
};

struct rtsp_transport
{
	uint8_t isRtp; //transport protocol
	uint8_t isTcp; //lower transport protocol
	uint8_t castMode; //unicast UDP(1) or unicast TCP(2) or multicast(3)
	int port_low;
	int port_high;
	int clientport_low;
	int clientport_high;
	int serverport_low;
	int serverport_high;
	uint8_t ttl; //multicast time to live
	//to be added if necessary
	int interleaved_low;	// for RTSP over TCP
	int interleaved_high;	// for RTSP over TCP
};

struct __internal_payload{
	int codec_id;
	struct rtp_object payload;
};

struct stream_context
{
	struct rtsp_context *parent;
	int stream_id; //sync with stream_flow id
	char *stream_name;
	struct list_head input_queue;
	_mutex input_lock;
	struct list_head output_queue;
	_mutex output_lock;
	struct codec_info *codec;
	uint8_t media_type;
	uint8_t framerate;
	uint32_t bitrate;
	uint32_t samplerate;
	uint32_t channel;
	uint32_t old_depend_clock_tick;
	uint32_t rtp_timestamp;
	uint8_t use_rtp_tick_inc;
	uint8_t index;	// index in rtsp_context
	uint8_t setup_done;
	struct rtp_statistics statistics;
	struct rtp_periodic_report_s periodic_report;
	struct __internal_payload rtpobj;
};

struct rtsp_context
{
	int id;
	uint8_t allow_stream;
	rtsp_state state;
	uint8_t request_type;
	uint8_t request_incomplete;
	uint32_t CSeq;
	char *response;
	char *request;
	int32_t reqstats;
	struct connect_context connect_ctx;
	int proxy_socket;
	int proxy_port;
	uint8_t is_connected_to_proxy;
	struct rtsp_transport transport[RTSP_MAX_STREAM_NUM];
	struct rtsp_session session;
	u16 rtpseq[RTSP_MAX_STREAM_NUM];
	uint8_t is_rtsp_start;
	_sema start_rtsp_sema;
	uint8_t is_rtp_start;
	_sema start_rtp_sema;
	void (* rtp_service_handle) (struct rtsp_context* rtsp_ctx);
	_sema start_proxy_connect_sema;
	_sema rtp_input_sema;
	_sema rtp_output_sema;
#ifdef SUPPORT_RTCP
	uint8_t is_rtcp_start;
	_sema start_rtcp_sema;
	void (* rtcp_service_handle) (struct rtsp_context* rtsp_ctx);
#endif
#ifdef SUPPORT_HTTP
	//to be added
#endif
	uint8_t nb_streams_setup;
	int8_t setup_stream_index;
	uint8_t nb_streams;
	struct stream_context *stream_ctx;
	uint32_t pre_filter_packet;
	int client_socket;
	_mutex socket_lock;
#ifdef SUPPORT_RTSPS
	mbedtls_ssl_context ssl_ctx;
	mbedtls_ssl_config ssl_conf;
#endif

	// callback
	int (*cb_start)(void*);		// play
	int (*cb_stop)(void*);		// teardown
	int (*cb_pause)(void*);		// pause
	int (*cb_custom)(void*);	// setparam
};

uint32_t rtsp_get_timestamp(struct stream_context *stream_ctx, uint32_t current_clock_tick);
int rtsp_get_number(int number_base, uint32_t *number_bitmap, _mutex *bitmap_lock);
void rtsp_put_number(int number, int number_base, uint32_t *number_bitmap, _mutex *bitmap_lock);
void rtp_stream_statistics_sync(struct stream_context *stream_ctx);
struct rtsp_context *rtsp_context_create(uint8_t nb_streams);
void rtsp_context_free(struct rtsp_context *rtsp_ctx);
void rtsp_context_free(struct rtsp_context *rtsp_ctx);
void rtsp_enable_stream(struct rtsp_context *rtsp_ctx);
void rtsp_disable_stream(struct rtsp_context *rtsp_ctx);
int rtsp_is_stream_enabled(struct rtsp_context *rtsp_ctx);
void rtsp_enable_service(struct rtsp_context *rtsp_ctx);
int rtsp_is_service_enabled(struct rtsp_context *rtsp_ctx);
void rtsp_close_service(struct rtsp_context *rtsp_ctx);
int rtsp_open(struct rtsp_context *rtsp_ctx);
void rtsp_close(struct rtsp_context *rtsp_ctx);
void rtsp_start(struct rtsp_context *rtsp_ctx);
void rtsp_stop(struct rtsp_context *rtsp_ctx);
void rtp_object_in_stream_queue(struct rtp_object *payload, struct stream_context *stream_ctx);
struct rtp_object *rtp_object_out_stream_queue(struct stream_context *stream_ctx);

void set_profile_lv_string(char * plid);
void set_sps_string(char * sps);
void set_pps_string(char * pps);
int rtsp_parse_stream_media_type(struct codec_info *codec);
void rtsp_set_debug_mode(int debug_mode, int enable);

struct stream_context* rtp_server_stream_contexts_create(uint8_t nb_streams);
void rtp_server_stream_contexts_free(struct stream_context* stream_ctx);
struct stream_context* rtp_server_stream_param_init(struct stream_context *stream_ctxs, int streaem_id, int audio_samplerate, int video_framerate, int codec_id, struct connect_context* connect_ctx);
void rtp_server_start(struct stream_context *stream);

#endif
