#include "rtsp_api.h"

#define RTSP_RESPONSE	"RTSP/1.0 "
#define OPTIONS_PUBLIC	"Public: "
#define SETUP_SESSION	"Session: "
#define SETUP_SIP		"source="
#define SETUP_CPORT 	"client_port="
#define SETUP_SPORT 	"server_port="

typedef struct _REQUEST_TABLE{
    char req[64];
    int32_t key;
}REQUEST_TABLE;

int rtsp_write(struct rtsp_context *rtsp_ctx, u8 *data, int data_len);
#ifdef SUPPORT_RTSPS
int rtsp_tls(struct rtsp_context *rtsp_ctx);
#endif
int rtsp_options_request(struct rtsp_context *rtsp_ctx);
int rtsp_announce_request(struct rtsp_context *rtsp_ctx);
int rtsp_setup_request(struct rtsp_context *rtsp_ctx);
int rtsp_record_request(struct rtsp_context *rtsp_ctx);
int rtsp_keep_alive_request(struct rtsp_context *rtsp_ctx);
int rtsp_get_parameter_request(struct rtsp_context *rtsp_ctx);
int rtsp_teardown_request(struct rtsp_context *rtsp_ctx);
