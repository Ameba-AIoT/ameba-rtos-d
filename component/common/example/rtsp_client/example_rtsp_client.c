#include <FreeRTOS.h>
#include <task.h>
#include "platform_opts.h"
#include "platform_autoconf.h"

#include "sockets.h"
#include "wifi_conf.h"		// for wifi_is_ready_to_transceive
#include "wifi_util.h"		// for getting wifi mode info
#include "lwip/netif.h"		// for LwIP_GetIP
#include "lwip_netconf.h"	// for LwIP_GetMAC

// 
#include "rtsp_api.h"
#include "rtp_api.h"
#include "rtsp_client_api.h"
#include "example_rtsp_client.h"
#include "i2s_api.h"

extern struct netif xnetif[NET_IF_NUM];
extern uint8_t* LwIP_GetIP(struct netif *pnetif);
#define WLAN0_NAME "wlan0"

//server
#define REMOTE_IP 		"54.156.189.88"
#define REMOTE_PORT 	443
#define STREAM_NAME		"mediaStreaming/abcde"

//pcm handle
#define WRITE_SIZE		1440
#define RETRY_TIMES		3
extern int max_skb_buf_num;
extern int skbdata_used_num;

extern uint32_t rtp_drop_threshold;
extern uint8_t flag_show_ts_diff;

// i2s Pinmux
#define I2S_SCLK_PIN		PA_2
#define I2S_WS_PIN			PA_4
#define I2S_SD_TX_PIN		PB_26
#define I2S_SD_RX_PIN		PA_0
#define I2S_MCK_PIN			PB_23
i2s_t i2s_obj;
u32 count = 0;

static i2s_params_t i2s_params = {
	.sample_rate = SR_48KHZ,
	.word_length = WL_32b,
	.direction	 = I2S_DIR_RX,
	.channel_num = CH_STEREO,
	.slave_mode	 = I2S_SLAVE_MODE
};

static rtsp_client_params_t rtsp_client_params = {
	.type 			= AVMEDIA_TYPE_AUDIO,
	.codec_id 		= AV_CODEC_ID_PCMU,
	.channel 		= 1,
	.sample_rate 	= 24000,
	.cast_mode 		= UNICAST_UDP_MODE
};

static int sr_shrink;
static int pdata_len;

static RTSP_REQUEST_HANDLE rtsp_req_handle_table[] = {{REQUEST_OPTIONS, rtsp_options_request},
											{REQUEST_ANNOUNCE, rtsp_announce_request},
											{REQUEST_SETUP, rtsp_setup_request},
											{REQUEST_RECORD, rtsp_record_request},
											{REQUEST_GET_PARAM, rtsp_get_parameter_request},
											{REQUEST_TEARDOWN, rtsp_teardown_request},
											{RTSP_KEEPALIVE, rtsp_keep_alive_request}};

int rtsp_status_machine(struct rtsp_context *rtsp_ctx)
{
	int table_size = sizeof(rtsp_req_handle_table)/sizeof(RTSP_REQUEST_HANDLE);

	for (int i = 0; i < table_size; i++){\
		if (rtsp_ctx->request_type == rtsp_req_handle_table[i].req){
			rtsp_ctx->CSeq++;
			if (!rtsp_req_handle_table[i].handle(rtsp_ctx)){
				RTSP_DBG_ERROR("Error: Request is not supported.\n");
				return 0;
			}
		}
	}
	return 1;
}

extern uint16_t _htons(uint16_t x);
void rtp_client_record(struct rtsp_context *, int);
void rtsp_client_service(struct rtsp_context *rtsp_ctx, char *remote_host, int remote_port, char *stream_name)
{
	struct sockaddr_in remote_addr;
	struct connect_context *connect_ctx = &rtsp_ctx->connect_ctx;
	int mode = 0;
	int opt = 1;

Redo:
	while(rtsp_is_service_enabled(rtsp_ctx))
	{
		vTaskDelay(1);
		if(!rtsp_is_service_enabled(rtsp_ctx))
		{
			RTSP_DBG_ERROR("rtsp service disabled while waiting for wifi Tx/Rx ready...");
			return;
		}
		if(rltk_wlan_running(0)>0){
			wext_get_mode(WLAN0_NAME, &mode);
			if(wifi_is_ready_to_transceive(RTW_STA_INTERFACE) >= 0 && (mode == RTW_MODE_INFRA)){
				RTSP_DBG_INFO("connect successful sta mode\r\n");
				break;
			}
			if(wifi_is_ready_to_transceive(RTW_AP_INTERFACE) >= 0 && (mode == RTW_MODE_MASTER)){
				RTSP_DBG_INFO("connect successful ap mode\r\n");
				break;
			}
		}
	}
	
	rtw_interface_t interface = (mode == RTW_MODE_INFRA)? RTW_STA_INTERFACE: RTW_AP_INTERFACE;

	// connect ctx init
	connect_ctx->client_ip = LwIP_GetIP(&xnetif[0]);
	connect_ctx->socket_id = socket(AF_INET, SOCK_STREAM, 0);
	if(connect_ctx->socket_id < 0)
	{
		RTSP_DBG_ERROR("rtsp server socket create failed\r\n");
		goto error;
	}

	if((setsockopt(connect_ctx->socket_id, SOL_SOCKET, SO_REUSEADDR, (const char *)&opt, sizeof(opt))) < 0){
		RTSP_DBG_ERROR("Error on setting socket option\r\n");
		goto error;
	}
	
	remote_addr.sin_family = AF_INET;
	remote_addr.sin_addr.s_addr = inet_addr(remote_host);
	remote_addr.sin_port = _htons(remote_port);

	*(connect_ctx->remote_ip + 3) = (unsigned char) (remote_addr.sin_addr.s_addr >> 24);
	*(connect_ctx->remote_ip + 2) = (unsigned char) (remote_addr.sin_addr.s_addr >> 16);
	*(connect_ctx->remote_ip + 1) = (unsigned char) (remote_addr.sin_addr.s_addr >> 8);
	*(connect_ctx->remote_ip) = (unsigned char) (remote_addr.sin_addr.s_addr );
	connect_ctx->remote_port = remote_port;

	if(rtsp_ctx->transport[rtsp_ctx->stream_ctx->index].castMode == UNICAST_UDP_MODE){
		memcpy(connect_ctx->server_ip, connect_ctx->remote_ip, 4);
		connect_ctx->server_port = remote_port;
	}
	
	RTSP_DBG_INFO("connect context inited\r\n");

	rtsp_ctx->client_socket = connect_ctx->socket_id;
	rtsp_ctx->stream_ctx->stream_name = stream_name;
		
	// connect
	if(connect(connect_ctx->socket_id, (struct sockaddr *)&remote_addr, sizeof(remote_addr)) < 0){
		RTSP_DBG_ERROR("Cannot connect stream socket");
		goto error;
	}
	
#ifdef SUPPORT_RTSPS
	if(rtsp_tls(rtsp_ctx) < 0){
		RTSP_DBG_ERROR("ssl create failed");
		goto error;
	}
#endif
	
	RTSP_DBG_INFO("rtsp stream enabled\r\n");

	// rtsp part
	do{
		if ((!rtsp_status_machine(rtsp_ctx)) || (rtsp_ctx->request_type == RTSP_QUIT)){
			goto out;
		}

		//here to start rtp/rtcp service
		if (rtsp_ctx->request_type == RTSP_KEEPALIVE){
			rtw_up_sema(&rtsp_ctx->start_rtp_sema);
#ifdef SUPPORT_RTCP
			rtw_up_sema(&rtsp_ctx->start_rtcp_sema);
#endif
			rtp_client_record(rtsp_ctx, sr_shrink);
			rtsp_ctx->request_type = REQUEST_TEARDOWN;
		}
	}while(1);

out:
	rtsp_ctx->state = RTSP_INIT;
	RTSP_DBG_ERROR("[%s] Socket closed\n\r",(rtsp_ctx->is_connected_to_proxy == 1)? "Proxy":"Local");
	rtsp_ctx->is_connected_to_proxy = 0;
	close(rtsp_ctx->client_socket);	
	//refresh number of streams been set up
	rtsp_ctx->nb_streams_setup = 0;
	for (int i=0; i < rtsp_ctx->nb_streams; i++) {
		rtsp_ctx->rtpseq[i] = 0;
		rtsp_ctx->stream_ctx[i].setup_done = 0;
	}

	if ((mode == RTW_MODE_INFRA) || (mode == RTW_MODE_MASTER)) {
		if (wifi_is_ready_to_transceive(interface) < 0) {
			RTSP_DBG_ERROR("wifi Tx/Rx broke! rtsp service cannot stream");
			close(connect_ctx->socket_id);
			RTSP_DBG_INFO("RTSP Reconnect!");
			goto Redo;
		}
	}
	else
		goto error;

error:
	rtsp_disable_stream(rtsp_ctx);
	close(connect_ctx->socket_id);
	RTSP_DBG_ERROR("rtsp service stop");

	return NULL;

}

extern uint8_t encodeA(short pcm_val);
extern uint8_t encodeU(short pcm_val);
void rtp_client_record(struct rtsp_context *rtsp_ctx, int sr_shrink)
{
	struct stream_context *stream_ctx = rtsp_ctx->stream_ctx;
	struct rtp_object *payload;
	int data_size = I2S_DMA_PAGE_SIZE / (sr_shrink * 8);
	uint8_t pdata[data_size];
	uint32_t poutdata[I2S_DMA_PAGE_SIZE/4];
	int last_tick = 0;
	uint32_t frame_timer_period = 0;
	int num = 0;
	uint8_t (*g711_encode)(short pcm_val);

	if(stream_ctx->codec->codec_id == AV_CODEC_ID_PCMA)
		g711_encode = encodeA;
	else
		g711_encode = encodeU;

	frame_timer_period =(int) (1000/(stream_ctx->samplerate/(data_size)));

	while(1){
		/* wait output not empty and get one payload from rtp stream queue */
		do{
			payload = rtp_object_out_stream_queue(stream_ctx);
			if(payload==NULL)	vTaskDelay(1);
		}while(payload == NULL);

		/*fill payload*/
		memcpy((void*)poutdata, (void*)&recv_buf[I2S_DMA_PAGE_SIZE*num],I2S_DMA_PAGE_SIZE);
		for (int i=0; i<data_size; i++){
			pdata[i] = g711_encode((short)(poutdata[i*2*sr_shrink]>>16));
		}
		num = num + 1;

		if(payload->data)
			free(payload->data);
		payload->data = malloc(data_size);
		memcpy(payload->data, pdata, data_size);
		payload->len = data_size;
		payload->timestamp = xTaskGetTickCount(); // will convert tick count to rtp timestamp in driver
		if (num >= RECV_PAGE_NUM)
			num = 0;

		/* because we will fill & send a complete frame in a single rtp object, set both fs & fe to 1 and fd to 0*/
		rtp_object_set_fs(payload, 1);
		rtp_object_set_fe(payload, 1);
		rtp_object_set_fd(payload, 0);
		/* set payload state to READY to enable rtp task process this rtp object;*/
		payload->state = RTP_OBJECT_READY;

		/* calculate the timing to send frame */
		if(last_tick != 0){
			int delta = xTaskGetTickCount() - last_tick ;
			if(delta < (int)frame_timer_period)
				vTaskDelay(frame_timer_period-delta);
		}
		last_tick = xTaskGetTickCount();

		/* insert payload to rtsp_ctx stream */
		rtp_object_in_stream_queue(payload, stream_ctx);
	}

}

int rtp_clinet_g711_send_unicast(struct stream_context *stream_ctx, struct rtp_object *pObject, int data_len)
{
	struct rtsp_context *rtsp_ctx = stream_ctx->parent;
	int cast_mode = rtsp_ctx->transport[stream_ctx->index].castMode;
	
	if (stream_ctx->periodic_report.timer1 == 0){
		stream_ctx->periodic_report.timer1 = rtw_get_current_time();
	}
	
	rtp_hdr_t *rtphdr;
	int socket;
	int ret;
	uint8_t packet_buf[WRITE_SIZE];
	uint8_t *ptr, *rtp_hdr_pos;
	//int bytes_left = pObject->len;
	pObject->rtphdr->seq = htons(rtsp_ctx->rtpseq[stream_ctx->index]);
	int j, retry_cnt, header_len;
	uint32_t _write_size;
	( void ) _write_size; //To avoid gcc warnings
	
	uint8_t *data_entry = pObject->data;
	
	// for UDP
	struct sockaddr_in adr_cs;
	int len_cs;
	
	socket = pObject->connect_ctx.socket_id;
	
	if (cast_mode == UNICAST_UDP_MODE) {
		//remote socket info
		adr_cs.sin_family = AF_INET;
		adr_cs.sin_addr.s_addr = *(uint32_t *)pObject->connect_ctx.remote_ip;
		adr_cs.sin_port = htons(pObject->connect_ctx.remote_port);
		len_cs = sizeof(adr_cs);
		rtp_hdr_pos = packet_buf;
		_write_size = WRITE_SIZE;
	}
	else { // UNICAST_TCP_MODE
		packet_buf[0] = '$';
		packet_buf[1] = rtsp_ctx->transport[stream_ctx->index].interleaved_low; // channel ID in SETUP
		rtp_hdr_pos = packet_buf + 4;
		_write_size = WRITE_SIZE - 4; // 4 bytes for interleaved header
	}
	
	ptr = (cast_mode == UNICAST_TCP_MODE)? (packet_buf+4) : packet_buf;
	
	if(pObject->rtphdr->cc > 0) { //has CSRC
		memcpy(ptr, pObject->rtphdr, RTP_HDR_SZ + 4*pObject->rtphdr->cc);
		rtphdr = (rtp_hdr_t *)ptr;
		ptr += RTP_HDR_SZ + 4*pObject->rtphdr->cc;
	}
	else { //no CSRC
		memcpy(ptr, pObject->rtphdr, RTP_HDR_SZ); //prepare header
		rtphdr = (rtp_hdr_t *)ptr;
		ptr += RTP_HDR_SZ;
	}
	
	header_len = ptr - rtp_hdr_pos;
	memcpy(ptr, data_entry, data_len);
	free(data_entry);
	
	if (cast_mode == UNICAST_TCP_MODE) {
		// fill lenght in interleaved header
		uint16_t rtp_len = header_len + data_len;
		uint16_t* len_ptr = (uint16_t*)(packet_buf+2); 
		*len_ptr = htons(rtp_len);
	}
	
	// send
	if (cast_mode == UNICAST_TCP_MODE) {
		rtw_mutex_get(&rtsp_ctx->socket_lock);
		ret = rtsp_write(rtsp_ctx, packet_buf, 4 + header_len + data_len);	// 4 bytes for interleaved header
		rtw_mutex_put(&rtsp_ctx->socket_lock);
	}
	else { //cast_mode == UNICAST_UDP_MODE
check_skb:
		if(skbdata_used_num > (max_skb_buf_num - 3)) {
			vTaskDelay(1);
			goto check_skb;
		}else{
			rtsp_keep_alive_request(rtsp_ctx);
			retry_cnt = 3;
			ret = sendto(socket, packet_buf, header_len + data_len, 0, (struct sockaddr *)&adr_cs, len_cs);
			if (ret < 0)
			{
				j = 1;
				do{
					rtw_mdelay_os(j);
					//retry
					ret = sendto(socket, packet_buf, header_len + data_len, 0, (struct sockaddr *)&adr_cs, len_cs);
					j++;
					retry_cnt--;
				}while((ret < 0)&&(retry_cnt>0));
			}
		}
	}
	if(ret < 0) {
		RTP_DBG_ERROR("RTP send fail\n\r");
		return -EAGAIN;
	}
	stream_ctx->periodic_report.bytes += (header_len + data_len);
	rtsp_ctx->rtpseq[stream_ctx->index]++;
	rtphdr->seq = htons(rtsp_ctx->rtpseq[stream_ctx->index]);
	stream_ctx->periodic_report.timer2 = rtw_get_current_time();
	stream_ctx->periodic_report.send_frame++;
	if((stream_ctx->periodic_report.timer2 - stream_ctx->periodic_report.timer1) >= stream_ctx->periodic_report.period) {
		rtp_report(stream_ctx);
	}
	return 0;
}

int rtp_client_o_g711_handler(struct stream_context *stream_ctx, struct rtp_object *payload){
	struct rtsp_context *rtsp_ctx = stream_ctx->parent;
	uint32_t temp_timer = 0;
	int i, stream_nb = 0;
	int ret;
	
	uint32_t frame_tick_cnt = payload->timestamp;
	// convert xTaskGetTickCount to timestamp here
	payload->timestamp = rtsp_get_timestamp(stream_ctx, frame_tick_cnt);
	if (flag_show_ts_diff)
		RTSP_DBG_INFO("[G][%d] ts += %d\n\r",rtsp_ctx->id,payload->timestamp - stream_ctx->periodic_report.last_timestamp);
	stream_ctx->periodic_report.last_timestamp = payload->timestamp;
	
	if(stream_ctx->statistics.do_start_check){
		for(i = 0; i < rtsp_ctx->nb_streams; i ++){
			if(rtsp_ctx->stream_ctx[i].stream_id >= 0)
				stream_nb ++;
		}
		/* initialize rtp object header part */
		if(payload->rtphdr != NULL)
			free(payload->rtphdr);
		payload->rtphdr = malloc(RTP_HDR_SZ + sizeof(uint32_t)*stream_nb);
		if(payload->rtphdr == NULL){
			RTSP_DBG_ERROR("\n\rallocate rtp header failed");
			return -ENOMEM;
		}
	}

	rtp_fill_header(payload->rtphdr, 2, 0, 0, 0, 0, stream_ctx->codec->pt, rtsp_ctx->rtpseq[stream_ctx->index], payload->timestamp, stream_ctx->stream_id);
	if(stream_ctx->statistics.do_start_check)
	{
		stream_ctx->statistics.delta_timer = 0;
		stream_ctx->statistics.timer2 = rtw_get_current_time();
	}
	stream_ctx->statistics.timer1 = rtw_get_current_time();
	uint32_t current_tick = xTaskGetTickCount();
	int tick_diff = current_tick - frame_tick_cnt;
	if (((stream_ctx->statistics.timer1-stream_ctx->statistics.timer2) < stream_ctx->statistics.delay_threshold * 2)
			&& (tick_diff <= (int)rtp_drop_threshold))
	{
		/* sending rtp payload */
		switch(rtsp_ctx->transport[stream_ctx->index].castMode){
			case(UNICAST_UDP_MODE):
			case(UNICAST_TCP_MODE):
				ret = rtp_clinet_g711_send_unicast(stream_ctx, payload, pdata_len);
				break;
			case(MULTICAST_MODE):
				//to add...
				break;
			default:
				return -EINVAL;
		}
		/* END of sending rtp payload */
		temp_timer = rtw_get_current_time();
		stream_ctx->statistics.delta_timer = temp_timer - stream_ctx->statistics.timer2;
		stream_ctx->statistics.timer2 = rtw_get_current_time();
	}
	else{
		stream_ctx->periodic_report.drop_frame++;
		temp_timer = rtw_get_current_time();
		stream_ctx->statistics.delta_timer = temp_timer - stream_ctx->statistics.timer2;
		stream_ctx->statistics.timer2 = temp_timer;
		//stream_ctx->statistics.do_start_check = 1;
		rtp_object_set_fs(payload, 0);
		rtp_object_set_fe(payload, 0);
		rtp_object_set_fk(payload, 0);
		rtp_object_set_fd(payload, 0);
		payload->data = NULL;
		payload->len = 0;
		return -EAGAIN;
	}
	//successfully sent packet out, set do start check to 0
	stream_ctx->statistics.do_start_check = 0;
	payload->data = NULL;
	payload->len = 0; 
	return ret;
}

static void rtp_client_service_unicast(struct rtsp_context *rtsp_ctx)
{
	int cast_mode = rtsp_ctx->transport[0].castMode;
	struct rtp_object *payload;
	struct stream_context *stream;
	int i, ret;
	unsigned int filter_count = 0;
	( void ) filter_count; //To avoid gcc warnings
	int rtp_socket;
	if (cast_mode == UNICAST_TCP_MODE) {
		/* reuse RTSP socket (TCP) to send RTP packet */
		rtp_socket = rtsp_ctx->client_socket;
	}
	else {	// UNICAST_UDP_MODE
		/* init rtp socket (UDP) */
		rtp_socket = socket(AF_INET, SOCK_DGRAM, 0);
		int rtp_port = rtsp_ctx->transport[0].clientport_low;
		struct sockaddr_in rtp_addr;
		socklen_t addrlen = sizeof(struct sockaddr_in);
		memset(&rtp_addr, 0, addrlen);
		rtp_addr.sin_family = AF_INET;
		rtp_addr.sin_addr.s_addr = *(uint32_t *)(rtsp_ctx->connect_ctx.client_ip);
		rtp_addr.sin_port = _htons((u16)rtp_port);
		if (bind(rtp_socket,(struct sockaddr *)&rtp_addr, addrlen)<0) {
			RTSP_DBG_ERROR("bind failed\r\n");
			goto exit;
		}
	}

	rtsp_ctx->is_rtp_start = 1;
	printf("\n\r\n\rrtp started (%s)\n\r",(cast_mode == UNICAST_TCP_MODE)? "TCP":"UDP");
restart:
	while((rtsp_ctx->state == RTSP_PLAYING)&&(rtsp_is_stream_enabled(rtsp_ctx)))
	{
		if(rtw_down_timeout_sema(&rtsp_ctx->rtp_input_sema,100))
		{
			for(i = 0; i < rtsp_ctx->nb_streams; i++)
			{
				stream = &rtsp_ctx->stream_ctx[i];
				if (!list_empty(&stream->input_queue))
				{
					rtw_mutex_get(&stream->input_lock);
					payload = list_first_entry(&stream->input_queue, struct rtp_object, rtp_list);
					list_del_init(&payload->rtp_list);
					rtw_mutex_put(&stream->input_lock);
					
					if (stream->setup_done) {
						if(payload->state == RTP_OBJECT_READY)
						{
							payload->state = RTP_OBJECT_INUSE;
							payload->connect_ctx.socket_id = rtp_socket;
							if (cast_mode == UNICAST_UDP_MODE) {
								payload->connect_ctx.remote_port = (u16)rtsp_ctx->transport[i].serverport_low;
								payload->connect_ctx.client_ip = rtsp_ctx->connect_ctx.client_ip;
								payload->connect_ctx.remote_ip = rtsp_ctx->connect_ctx.server_ip;
							}
							ret = 0;
							ret = payload->rtp_object_handler(stream, payload);

							if(ret < 0){
								if(ret == -EAGAIN)//means packet drop
									stream->statistics.drop_packet++;
								else {
									RTSP_DBG_ERROR("payload->rtp_object_handler(stream, payload) ret=%d\n\r",ret);
								}
							}else{
								if(stream->statistics.base_timestamp == 0) {
									stream->statistics.base_timestamp = payload->timestamp;
									RTSP_DBG_INFO("base ts:%d", stream->statistics.base_timestamp);
								}
								stream->statistics.sent_packet++;
							}
							payload->state = RTP_OBJECT_USED;
						}
						else{
							payload->state = RTP_OBJECT_IDLE;
						}
					}
					rtw_mutex_get(&stream->output_lock);
					list_add_tail(&payload->rtp_list, &stream->output_queue);
					rtw_mutex_put(&stream->output_lock);
					rtw_up_sema(&rtsp_ctx->rtp_output_sema);
				}
			}
		}
	}
	vTaskDelay(10);
	if(rtsp_ctx->state == RTSP_READY) {
		filter_count = 0;//for pause
		goto restart;
	}
exit:
	rtsp_ctx->is_rtp_start = 0;
	if (cast_mode == UNICAST_UDP_MODE)
		close(rtp_socket);
	printf("\n\rrtp stopped (%s)\n\r",(cast_mode == UNICAST_TCP_MODE)? "TCP":"UDP");
}

void rtp_client_service_init(void *param)
{
	struct rtsp_context *rtsp_ctx = (struct rtsp_context *) param;
	struct stream_context *stream;
	while(rtsp_is_service_enabled(rtsp_ctx))
	{
		if(rtw_down_timeout_sema(&rtsp_ctx->start_rtp_sema,100))
		{
			for(int i = 0; i < rtsp_ctx->nb_streams; i++) {
				stream = &rtsp_ctx->stream_ctx[i];
				if (stream->setup_done)
					rtp_stream_statistics_sync(stream);
			}
			
			switch(rtsp_ctx->transport[0].castMode)
			{
				case(UNICAST_UDP_MODE):
				case(UNICAST_TCP_MODE):
					rtsp_ctx->rtp_service_handle = rtp_client_service_unicast;
					rtsp_ctx->rtp_service_handle(rtsp_ctx);
					break;
				case(MULTICAST_MODE):
					//to do
					break;
				default:
					break;
			}
		}
	}
	rtsp_ctx->is_rtp_start = 0;
	vTaskDelete(NULL);
}

void rtsp_client_service_init(void *param)
{
	struct rtsp_context *rtsp_ctx = (struct rtsp_context *) param;
	
	rtsp_enable_service(rtsp_ctx);
	while(rtsp_is_service_enabled(rtsp_ctx))
	{
		if(rtw_down_timeout_sema(&rtsp_ctx->start_rtsp_sema,100))
		{
			//rtsp start stream
			rtsp_client_service(rtsp_ctx, REMOTE_IP, REMOTE_PORT, STREAM_NAME);
		}
	}	
	RTSP_DBG_ERROR("rtsp service closed");
	vTaskDelete(NULL);
}

struct rtsp_context *rtsp_client_context_create(uint8_t nb_streams, uint8_t cast_mode)
{
	struct rtsp_context *rtsp_ctx = rtsp_context_create(nb_streams);
	if(!rtsp_ctx)	return NULL;
	
	for(int t=0; t<rtsp_ctx->nb_streams; t++){
		rtsp_ctx->transport[t].castMode = cast_mode;
		if(cast_mode == UNICAST_TCP_MODE){
			rtsp_ctx->transport[t].interleaved_low = 0;
			rtsp_ctx->transport[t].interleaved_high = 1;
		}
	}
	
	rtsp_ctx->request = malloc(RTSP_REQUEST_BUF_SIZE);
	if(rtsp_ctx->request == NULL)
	{
		RTSP_DBG_ERROR("allocate rtsp response failed");
		free(rtsp_ctx->connect_ctx.remote_ip);
		free(rtsp_ctx->response);
		free(rtsp_ctx->stream_ctx);
		free(rtsp_ctx);
		return NULL;
	}
	rtsp_ctx->connect_ctx.server_ip = malloc(4);
	if(rtsp_ctx->connect_ctx.server_ip == NULL)
	{
		RTSP_DBG_ERROR("allocate server ip memory failed");
		free(rtsp_ctx->connect_ctx.remote_ip);
		free(rtsp_ctx->response);
		free(rtsp_ctx->request);
		free(rtsp_ctx->stream_ctx);
		free(rtsp_ctx);
		return NULL;
	}	
	rtsp_ctx->request_type = REQUEST_OPTIONS;
	
	return rtsp_ctx;
}

int rtsp_client_set_params(struct rtsp_context *rtsp_ctx, rtsp_client_params_t *params)
{
	struct stream_context *stream_ctx = &rtsp_ctx->stream_ctx[0];
	
	// init payload object
	rtp_object_init(&stream_ctx->rtpobj.payload);

	stream_ctx->codec = malloc(sizeof(struct codec_info));
	if(!stream_ctx->codec)	return -1;

	stream_ctx->media_type = AVMEDIA_TYPE_AUDIO;
	stream_ctx->samplerate = params->sample_rate;
	stream_ctx->channel = params->channel;
	
	if((int)params->codec_id != AV_CODEC_ID_UNKNOWN){
		get_codec_by_id(stream_ctx->codec, params->codec_id);
		stream_ctx->codec->pt = RTP_PT_DYN_BASE;
		stream_ctx->codec->clock_rate = params->sample_rate;
		stream_ctx->rtpobj.payload.rtp_object_handler = rtp_client_o_g711_handler;
		if(stream_ctx->rtpobj.payload.rtp_object_handler == NULL)
			return -EINVAL;
		stream_ctx->rtp_timestamp = xTaskGetTickCount();
	}
	
	if(stream_ctx->stream_id < 0)
		stream_ctx->stream_id = 0;//rtsp_get_number(STREAM_FLOW_ID_BASE, &stream_flow_id_bitmap, &stream_flow_id_bitmap_lock);
	rtp_stream_statistics_sync(stream_ctx);
	stream_ctx->rtpobj.codec_id = stream_ctx->codec->codec_id;

	return 1;
}

void rtsp_client_start(struct rtsp_context *rtsp_ctx)
{
	for(int i = 0; i < rtsp_ctx->nb_streams; i++)
	{
		INIT_LIST_HEAD(&rtsp_ctx->stream_ctx[i].input_queue);
		INIT_LIST_HEAD(&rtsp_ctx->stream_ctx[i].output_queue);
	}
	if (rtsp_ctx->connect_ctx.client_port == 0)
		rtsp_ctx->connect_ctx.client_port = rtsp_ctx->id + DEF_RTSP_PORT;
	RTP_DBG_INFO("\n\rRTSP[%d] port: %d\n\r", rtsp_ctx->id, rtsp_ctx->connect_ctx.client_port);
	rtsp_ctx->state = RTSP_INIT;
	rtsp_ctx->nb_streams_setup = 0;
	rtsp_enable_stream(rtsp_ctx);
	rtw_up_sema(&rtsp_ctx->start_rtsp_sema);
	
	// insert to input queue
	for(int i=0;i<rtsp_ctx->nb_streams;i++){
		rtp_object_in_stream_queue(&rtsp_ctx->stream_ctx[i].rtpobj.payload, &rtsp_ctx->stream_ctx[i]); 
	}
}

void rtsp_client_destroy(struct rtsp_context *rtsp_ctx)
{
	if(rtsp_ctx->stream_ctx->codec)
		free(rtsp_ctx->stream_ctx->codec);

	rtp_object_deinit(&rtsp_ctx->stream_ctx->rtpobj.payload);
	
	rtsp_close_service(rtsp_ctx);
	free(rtsp_ctx->request);
	free(rtsp_ctx->connect_ctx.server_ip);
	rtsp_context_free(rtsp_ctx);

	return NULL;
}

void i2s_tx_complete(void *data, char *pbuf)
{   
	/* To avoid gcc warnings */
	( void ) data;
	( void ) pbuf;

}

void i2s_rx_complete(void *data, char* pbuf)
{
    i2s_t *obj = (i2s_t *)data;

	DCache_Invalidate((u32)(pbuf), I2S_DMA_PAGE_SIZE);
	_memcpy((void*)&recv_buf[I2S_DMA_PAGE_SIZE*count], (void*)pbuf, I2S_DMA_PAGE_SIZE);

	count++;
	if(count>=RECV_PAGE_NUM)
		count = 0;

	i2s_recv_page(obj);
}

extern void i2s_slavemode(i2s_t *obj, uint32_t slave_mode);
void i2s_stream_create(i2s_params_t *params)
{
	// I2S init
	i2s_obj.channel_num = params->channel_num;
	i2s_obj.sampling_rate = params->sample_rate;
	i2s_obj.word_length = params->word_length;
	i2s_obj.direction = params->direction;
	i2s_init(&i2s_obj, I2S_SCLK_PIN, I2S_WS_PIN, I2S_SD_TX_PIN, I2S_SD_RX_PIN, I2S_MCK_PIN);
	i2s_slavemode(&i2s_obj, params->slave_mode);
	i2s_set_dma_buffer(&i2s_obj, (char*)i2s_tx_buf, (char*)i2s_rx_buf, I2S_DMA_PAGE_NUM, I2S_DMA_PAGE_SIZE);
	i2s_tx_irq_handler(&i2s_obj, (i2s_irq_handler)i2s_tx_complete, (uint32_t)&i2s_obj);
	i2s_rx_irq_handler(&i2s_obj, (i2s_irq_handler)i2s_rx_complete, (uint32_t)&i2s_obj);
	
	i2s_recv_page(&i2s_obj);

	return;
}

int i2s_sr_shrink(i2s_params_t *i2s_params, rtsp_client_params_t *rtsp_client_params){

	uint32_t i2s_sample_rate = 0;
	
	if (i2s_params->sample_rate == SR_8KHZ)
		i2s_sample_rate = 8000;
	else if (i2s_params->sample_rate == SR_16KHZ)
		i2s_sample_rate = 16000;
	else if (i2s_params->sample_rate == SR_32KHZ)
		i2s_sample_rate = 32000;
	else if (i2s_params->sample_rate == SR_44p1KHZ)
		i2s_sample_rate = 44100;
	else if (i2s_params->sample_rate == SR_48KHZ)
		i2s_sample_rate = 48000;
	else if (i2s_params->sample_rate == SR_88p2KHZ)
		i2s_sample_rate = 88200;
	else if (i2s_params->sample_rate == SR_96KHZ)
		i2s_sample_rate = 96000;
	else{
		RTSP_DBG_ERROR("There is no proper definition of i2s sample rate.\n\r");
		return -1;
	}

	if (i2s_sample_rate%rtsp_client_params->sample_rate){
		RTSP_DBG_ERROR("is2 sample rate must can be divied by rtsp sample rate\n\r");
		return -1;
	}
	else
		return i2s_sample_rate/rtsp_client_params->sample_rate;
}

int example_rtsp_client(void)
{
	struct rtsp_context *rtsp_ctx = rtsp_client_context_create(1, rtsp_client_params.cast_mode);
	if(!rtsp_ctx)	
		goto error;

	sr_shrink = i2s_sr_shrink(&i2s_params, &rtsp_client_params);
	if(sr_shrink <= 0)
		goto error;
	pdata_len = I2S_DMA_PAGE_SIZE / (sr_shrink * 8);

	if(rtsp_client_set_params(rtsp_ctx, &rtsp_client_params) < 0)
		goto error;

	// init i2s
	i2s_stream_create(&i2s_params);

	if(xTaskCreate(rtsp_client_service_init, ((const char*)"rtsp_client_service_init"), 2048, (void *)rtsp_ctx, RTSP_SERVICE_PRIORITY, NULL) != pdPASS) {
		RTSP_DBG_ERROR("rtsp_client_service_init: Create Task Error\n");
		goto error;
	}

	//rtsp_service_priority = uxTaskPriorityGet(NULL);
	if(xTaskCreate(rtp_client_service_init, ((const char*)"rtp_service_init"), 1024, (void *)rtsp_ctx, RTSP_SERVICE_PRIORITY, NULL) != pdPASS) {
		RTSP_DBG_ERROR("rtp_start_service: Create Task Error\n");
		goto error;
	}

	// start
	rtsp_client_start(rtsp_ctx);

	return 0;

error:
	rtsp_client_destroy(rtsp_ctx);
	return -1;
}
