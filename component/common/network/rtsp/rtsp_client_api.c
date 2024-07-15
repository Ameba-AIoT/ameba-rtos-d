#include "rtsp_api.h"
#include "sockets.h"
#include "rtsp_client_api.h"

#ifdef SUPPORT_RTSPS
#define RTSP_PREFIX		"rtsps://"
#else
#define RTSP_PREFIX		"rtsp://"
#endif

static REQUEST_TABLE rtsp_req_table[]={{"OPTIONS", BIT(0)},
			                        {"DESCRIBE", BIT(1)},
			                        {"ANNOUNCE", BIT(2)},
			                        {"SETUP", BIT(3)},
			                        {"PLAY", BIT(4)},
			                        {"RECORD", BIT(5)},
			                        {"PAUSE", BIT(6)},
			                        {"GET_PARAMETER", BIT(7)},
			                        {"SET_PARAMETER", BIT(8)},
			                        {"REDIRECT", BIT(9)},
			                        {"TEARDOWN", BIT(10)},
			                        {"", 0}};

uint32_t keep_alive_time = 0;

int rtsp_write(struct rtsp_context *rtsp_ctx, u8 *data, int data_len)
{
#ifdef SUPPORT_RTSPS
	return mbedtls_ssl_write(&rtsp_ctx->ssl_ctx, data, data_len);
#else
	return write(rtsp_ctx->client_socket, data, data_len);
#endif
}

int rtsp_read(struct rtsp_context *rtsp_ctx, u8 *data, int data_len)
{
#ifdef SUPPORT_RTSPS
	return mbedtls_ssl_read(&rtsp_ctx->ssl_ctx, data, data_len);
#else
	return read(rtsp_ctx->client_socket, data, data_len);
#endif
}

#ifdef SUPPORT_RTSPS
static int rtsp_my_random(void *p_rng, unsigned char *output, size_t output_len)
{
	(void) p_rng;
	rtw_get_random_bytes(output, output_len);
	return 0;
}

static void *rtsp_my_calloc(size_t nelements, size_t elementSize)
{
	size_t size;
	void *ptr = NULL;

	size = nelements * elementSize;
	ptr = pvPortMalloc(size);

	if (ptr) {
		memset(ptr, 0, size);
	}

	return ptr;
}

int rtsp_tls(struct rtsp_context *rtsp_ctx)
{
	int ret = -1;
	mbedtls_ssl_context *ssl = &rtsp_ctx->ssl_ctx;
	mbedtls_ssl_config *conf = &rtsp_ctx->ssl_conf;
	char port_str[16];

	mbedtls_platform_set_calloc_free(rtsp_my_calloc, vPortFree);

	mbedtls_ssl_init(ssl);
	mbedtls_ssl_config_init(conf);

	mbedtls_ssl_set_bio(ssl, &rtsp_ctx->client_socket, mbedtls_net_send, mbedtls_net_recv, NULL);

	if ((ret = mbedtls_ssl_config_defaults(conf,
										   MBEDTLS_SSL_IS_CLIENT,
										   MBEDTLS_SSL_TRANSPORT_STREAM,
										   MBEDTLS_SSL_PRESET_DEFAULT)) != 0) {
		RTSP_DBG_ERROR("ERRPR: mbedtls_ssl_config_defaults ret(%d)\n", ret);
		goto exit;
	}

	mbedtls_ssl_conf_authmode(conf, MBEDTLS_SSL_VERIFY_NONE);
	mbedtls_ssl_conf_rng(conf, rtsp_my_random, NULL);

	if ((ret = mbedtls_ssl_setup(ssl, conf)) != 0) {
		RTSP_DBG_ERROR("ERRPR: mbedtls_ssl_setup ret(%d)\n", ret);
		goto exit;
	}

	if ((ret = mbedtls_ssl_handshake(ssl)) != 0) {
		RTSP_DBG_ERROR("ERROR: mbedtls_ssl_handshake ret(-0x%x)", -ret);
		goto exit;
	}

	RTSP_DBG_INFO("SSL ciphersuite %s\n", mbedtls_ssl_get_ciphersuite(ssl));
	
	return ret;
	
exit:
	mbedtls_ssl_free(ssl);
	mbedtls_ssl_config_free(conf);
	return ret;
}
#endif

static uint32_t rtsp_get_request_key(char *req)
{
    int size = sizeof(rtsp_req_table)/sizeof(REQUEST_TABLE);

    for (int i=0; i < size; i++){
        if (strncmp(rtsp_req_table[i].req, req, strlen(rtsp_req_table[i].req)) == 0){
            return rtsp_req_table[i].key;
        }
    }
    return 0;
}

void rtsp_parse_options(struct rtsp_context *rtsp_ctx)
{
	char *buf = rtsp_ctx->response;
    char *p = strstr(buf, OPTIONS_PUBLIC);
    if (NULL == p) {
        RTSP_DBG_ERROR("%s not found\n", OPTIONS_PUBLIC);
        return;	
    }
	
    p += strlen(OPTIONS_PUBLIC);
    char *ptr = p;
    char tmp[32];
    do{
        memset(tmp, 0, sizeof(tmp));
        if (*ptr == ','){
            strncpy(tmp, p, ptr-p);
            tmp[ptr-p]='\0';
            p = ptr+2;
            rtsp_ctx->reqstats |= rtsp_get_request_key(tmp);
        }else if (*ptr == '\r'){
            strncpy(tmp, p, ptr-p);
            tmp[ptr-p]='\0';
            break;
        }
        ptr++;
    }while(1);
    rtsp_ctx->reqstats |= rtsp_get_request_key(tmp);

    RTSP_DBG_INFO("req stats : %x\n", rtsp_ctx->reqstats);

    return;
}

void rtsp_g711_sdp(struct rtsp_context *rtsp_ctx, char* sdp_buf, int max_len)
{
	struct stream_context *stream = rtsp_ctx->stream_ctx;
	char attr_buf[MAX_SDP_SIZE];
	//sdp session level
	uint8_t *unicast_addr, *connection_addr;
	uint8_t nettype[] = "IN";
	uint8_t addrtype[] = "IP4";
	unicast_addr = rtsp_ctx->connect_ctx.client_ip;
	connection_addr = rtsp_ctx->connect_ctx.remote_ip;
	sprintf(sdp_buf, "v=0" CRLF);
	/* fill Protocol Version -- only have Version 0 for now*/
	sdp_fill_o_field(sdp_buf, max_len, rtsp_ctx->session.user, rtsp_ctx->session.id, rtsp_ctx->session.version, nettype, addrtype, unicast_addr);
	sdp_fill_s_field(sdp_buf, max_len, rtsp_ctx->session.name);
	sdp_fill_c_field(sdp_buf, max_len, nettype, addrtype, connection_addr, /*rtsp_ctx->transport.ttl*/0);//should change ttl if not unicast
	sdp_fill_t_field(sdp_buf, max_len, rtsp_ctx->session.start_time, rtsp_ctx->session.end_time);

	if(stream->stream_id >= 0){
		switch(stream->codec->codec_id){
			case(AV_CODEC_ID_PCMU):
				sprintf(attr_buf, "m=audio 0 RTP/AVP %d" CRLF \
								"a=rtpmap:%d PCMU/%d/1" CRLF \
								"a=control:streamid=%d" CRLF \
								,stream->codec->pt ,stream->codec->pt ,stream->samplerate, stream->stream_id); 
				break;
			case(AV_CODEC_ID_PCMA):
				sprintf(attr_buf, "m=audio 0 RTP/AVP %d" CRLF \
								"a=rtpmap:%d PCMA/%d/1" CRLF \
								"a=control:streamid=%d" CRLF \
								,stream->codec->pt ,stream->codec->pt ,stream->samplerate, stream->stream_id); 
				break;
			default:
				break;

		}
	}

	sdp_strcat(sdp_buf, max_len, attr_buf);
}

int32_t rtsp_parse_server_udp_ip(struct rtsp_context *rtsp_ctx)
{
	u32_t server_ip;
	char *buf = rtsp_ctx->response;
    char *ptr = strstr(buf, SETUP_SIP);
    if (!ptr) {
        RTSP_DBG_ERROR("SETUP: %s not found\n", SETUP_SIP);
        return 0;
    }
    ptr += strlen(SETUP_SIP);
    char tmp[16];
    char *p = ptr;

    memset(tmp, 0, sizeof(tmp));
    do{
        if (*p == ';' || *p == '\r'){
            break;
        }
        p++;
    }while(1);
    strncpy(tmp, ptr, p-ptr);

	server_ip = inet_addr(tmp);

	*(rtsp_ctx->connect_ctx.server_ip + 3) = (unsigned char) (server_ip >> 24);
	*(rtsp_ctx->connect_ctx.server_ip + 2) = (unsigned char) (server_ip >> 16);
	*(rtsp_ctx->connect_ctx.server_ip + 1) = (unsigned char) (server_ip >> 8);
	*(rtsp_ctx->connect_ctx.server_ip) = (unsigned char) (server_ip);

    RTSP_DBG_INFO("source ip: %d.%d.%d.%d", \
		(uint8_t)rtsp_ctx->connect_ctx.server_ip[0],(uint8_t)rtsp_ctx->connect_ctx.server_ip[1],(uint8_t)rtsp_ctx->connect_ctx.server_ip[2],(uint8_t)rtsp_ctx->connect_ctx.server_ip[3]);

    return 1;
}


int32_t rtsp_parse_server_udp_port(struct rtsp_context *rtsp_ctx)
{
	char *buf = rtsp_ctx->response;
    char *ptr = strstr(buf, SETUP_SPORT);
    if (!ptr) {
        RTSP_DBG_INFO("SETUP: %s not found\n", SETUP_SPORT);
        return 0;
    }
    ptr += strlen(SETUP_SPORT);
    char tmp[8];
    char *p = ptr;

    memset(tmp, 0, sizeof(tmp));
    do{
        if (*p == '-'){
            break;
        }
        p++;
    }while(1);
    strncpy(tmp, ptr, p-ptr);
	rtsp_ctx->transport[rtsp_ctx->nb_streams_setup].serverport_low = atol(tmp);
	rtsp_ctx->connect_ctx.server_port = atol(tmp);
    p++;
    ptr=p;
	
    memset(tmp, 0, sizeof(tmp));
    do{
        if (*p == ';' || *p == '\r'){
            break;
        }
        p++;
    }while(1);
    strncpy(tmp, ptr, p-ptr);
	rtsp_ctx->transport[rtsp_ctx->nb_streams_setup].serverport_high = atol(tmp);

    RTSP_DBG_INFO("client port from %d to %d", \
            rtsp_ctx->transport[rtsp_ctx->nb_streams_setup].clientport_low, \
            rtsp_ctx->transport[rtsp_ctx->nb_streams_setup].clientport_high);
    RTSP_DBG_INFO("server port from %d to %d", \
            rtsp_ctx->transport[rtsp_ctx->nb_streams_setup].serverport_low, \
            rtsp_ctx->transport[rtsp_ctx->nb_streams_setup].serverport_high);

    return 1;
}

int32_t rtsp_parse_sessionid(struct rtsp_context *rtsp_ctx)
{
    /* Session ID */
    char *buf = rtsp_ctx->response;
    char *ptr = strstr(buf, SETUP_SESSION);
    char tmp[32];
    if (!ptr) {
        RTSP_DBG_ERROR("SETUP: %s not found\n", SETUP_SESSION);
        return 0;
    }
    ptr += strlen(SETUP_SESSION);
    char *p = ptr;
    do{
        if (*p == ';' || *p == '\r'){
            break;
        }
        p++;
    }while(1);
    
    memcpy((void *)tmp, (const void *)ptr, p-ptr);
    tmp[p-ptr] = '\0';
    rtsp_ctx->session.id = atoi(tmp);

    RTSP_DBG_INFO("sessid : %u\n", rtsp_ctx->session.id);

    return 1;
}

int rtsp_receive_response(struct rtsp_context *rtsp_ctx)
{
	char *buf = rtsp_ctx->response;
	int read_len;

	memset(buf, 0, RTSP_RESPONSE_BUF_SIZE);
	rtw_mutex_get(&rtsp_ctx->socket_lock);
	read_len = rtsp_read(rtsp_ctx, buf, RTSP_RESPONSE_BUF_SIZE);
	rtw_mutex_put(&rtsp_ctx->socket_lock);
	
    if (read_len <= 0) {
        return -1;
    }

	return read_len;
}

int rtsp_check_response_status(struct rtsp_context *rtsp_ctx)
{
    int offset = strlen(RTSP_RESPONSE);
    char buf[8], *sep = NULL;
	char *response = rtsp_ctx->response;

    if (strncmp((const char*)response, (const char*)RTSP_RESPONSE, offset) != 0) {
		return 0;
    }

    sep = strchr((const char *)response+offset, ' ');
    if (!sep) {
        return 0;
    }
	
    memset(buf, 0, sizeof(buf));
    strncpy((char *)buf, (const char *)(response+offset), sep-response-offset);

    if (atoi(buf) != 200){
        return 0;
    }

    return 1;
}

int32_t rtsp_request_is_supported(int32_t key, struct rtsp_context *rtsp_ctx)
{
    RTSP_DBG_INFO("cmd stats : %x, key : %x\n", rtsp_ctx->reqstats, key);

    if (((rtsp_ctx->reqstats&0x01) > 0x01) || (0x01 == (key&0x01)))
        return 0;

    if ((key & rtsp_ctx->reqstats) > 0x01)
        return 1;
	
    return 0;
}

static int rtsp_send_options_request(struct rtsp_context *rtsp_ctx)
{
    char *buf = rtsp_ctx->request;
	int ok;

    memset(buf, 0, RTSP_REQUEST_BUF_SIZE);
	sprintf(buf, "OPTIONS %s%d.%d.%d.%d:%d/%s RTSP/1.0" CRLF \
				"CSeq: %d" CRLF \
				"User-Agent: " CRLF \
				CRLF \
				,RTSP_PREFIX , (uint8_t)rtsp_ctx->connect_ctx.remote_ip[0], (uint8_t)rtsp_ctx->connect_ctx.remote_ip[1], (uint8_t)rtsp_ctx->connect_ctx.remote_ip[2], (uint8_t)rtsp_ctx->connect_ctx.remote_ip[3], rtsp_ctx->connect_ctx.remote_port, rtsp_ctx->stream_ctx->stream_name, rtsp_ctx->CSeq);

	rtw_mutex_get(&rtsp_ctx->socket_lock);
	ok = rtsp_write(rtsp_ctx, buf, strlen(buf));
	rtw_mutex_put(&rtsp_ctx->socket_lock);

	if (ok < 0){
		RTSP_DBG_ERROR("send OPTIONS request failed!\n");
		return 0;
	}

    return 1;
}

int rtsp_options_request(struct rtsp_context *rtsp_ctx)
{
    if (!rtsp_send_options_request(rtsp_ctx))
		return 0;

    RTSP_DBG_INFO("++++++++++++ OPTIONS Request: ++++++++++++\n%s\n", rtsp_ctx->request);

    if (rtsp_receive_response(rtsp_ctx) <= 0){
        RTSP_DBG_ERROR("Error: Server did not respond properly, closing...\n");
        return 0;
    }

    RTSP_DBG_INFO("++++++++++++ OPTIONS Reply: ++++++++++++\n%s\n", rtsp_ctx->response);

    if (!rtsp_check_response_status(rtsp_ctx))	//200 ok
        return 0;
	
    rtsp_parse_options(rtsp_ctx);
    rtsp_ctx->request_type = REQUEST_ANNOUNCE ;
    return 1;
}

static int rtsp_send_announce_request(struct rtsp_context *rtsp_ctx)
{
	if(rtsp_ctx->state != RTSP_INIT)
		return 0;

    int sock = rtsp_ctx->client_socket;
    char *buf = rtsp_ctx->request;
	int ok;
	char sdp_buffer[MAX_SDP_SIZE] = {0};
	
    memset(buf, 0, RTSP_REQUEST_BUF_SIZE);

	int sdp_content_len = 0;
	rtsp_g711_sdp(rtsp_ctx, sdp_buffer, MAX_SDP_SIZE - 1);
	sdp_content_len = strlen(sdp_buffer);
	sprintf(buf, "ANNOUNCE %s%d.%d.%d.%d:%d/%s RTSP/1.0" CRLF \
				"Content-Type: application/sdp" CRLF \
				"CSeq: %d" CRLF \
				"User-Agent: " CRLF \
				"Content-Length: %d" CRLF \
				CRLF \
				"%s" \
				,RTSP_PREFIX , (uint8_t)rtsp_ctx->connect_ctx.remote_ip[0], (uint8_t)rtsp_ctx->connect_ctx.remote_ip[1], (uint8_t)rtsp_ctx->connect_ctx.remote_ip[2], (uint8_t)rtsp_ctx->connect_ctx.remote_ip[3], rtsp_ctx->connect_ctx.remote_port, rtsp_ctx->stream_ctx->stream_name, rtsp_ctx->CSeq, sdp_content_len, sdp_buffer);

	rtw_mutex_get(&rtsp_ctx->socket_lock);
	ok = rtsp_write(rtsp_ctx, buf, strlen(buf));
	rtw_mutex_put(&rtsp_ctx->socket_lock);
 
	if (ok < 0){
		RTSP_DBG_ERROR("send ANNOUNCE request failed!\n");
		return 0;
	}

    return 1;
}

int rtsp_announce_request(struct rtsp_context *rtsp_ctx)
{
    if (!rtsp_send_announce_request(rtsp_ctx))
        return 0;

    RTSP_DBG_INFO("++++++++++++ ANNOUNCE Request: ++++++++++++\n%s\n", rtsp_ctx->request);

    if (rtsp_receive_response(rtsp_ctx) <= 0) {
        RTSP_DBG_ERROR("Error: Server did not respond properly, closing...\n");
        return 0;
    }

    RTSP_DBG_INFO("++++++++++++ ANNOUNCE Reply: ++++++++++++\n%s\n", rtsp_ctx->response);

    if (!rtsp_check_response_status(rtsp_ctx))	//200 ok
        return 0;
	
    rtsp_ctx->request_type = REQUEST_SETUP;

    return 1;
}

static int rtsp_send_setup_request(struct rtsp_context *rtsp_ctx)
{
	char *buf = rtsp_ctx->request;
	int ok;
	char* castmode;

	memset(buf, 0, RTSP_REQUEST_BUF_SIZE);

	switch(rtsp_ctx->transport[rtsp_ctx->nb_streams_setup].castMode)
	{
		case(UNICAST_TCP_MODE):
		case(UNICAST_UDP_MODE):
			castmode = "unicast";
			break;
		case(MULTICAST_MODE):
			castmode = "multicast";
			break;
		default:
			castmode = "unknown";
	}
	if(rtsp_ctx->transport[rtsp_ctx->nb_streams_setup].castMode == UNICAST_UDP_MODE)
	{
		sprintf(rtsp_ctx->request, "SETUP %s%d.%d.%d.%d:%d/%s/streamid=%d RTSP/1.0" CRLF \
									"Transport: RTP/AVP/UDP;%s;client_port=%d-%d;mode=record" CRLF \
									"CSeq: %d" CRLF \
									"User-Agent: " CRLF \
									CRLF \
									,RTSP_PREFIX ,(uint8_t)rtsp_ctx->connect_ctx.remote_ip[0], (uint8_t)rtsp_ctx->connect_ctx.remote_ip[1], (uint8_t)rtsp_ctx->connect_ctx.remote_ip[2], (uint8_t)rtsp_ctx->connect_ctx.remote_ip[3], rtsp_ctx->connect_ctx.remote_port, rtsp_ctx->stream_ctx->stream_name, rtsp_ctx->nb_streams_setup, castmode, rtsp_ctx->transport[rtsp_ctx->nb_streams_setup].clientport_low, rtsp_ctx->transport[rtsp_ctx->nb_streams_setup].clientport_high, rtsp_ctx->CSeq);
	}else if(rtsp_ctx->transport[rtsp_ctx->nb_streams_setup].castMode == UNICAST_TCP_MODE)
	{
		sprintf(rtsp_ctx->request, "SETUP %s%d.%d.%d.%d:%d/%s/streamid=%d RTSP/1.0" CRLF \
									"Transport: RTP/AVP/TCP;%s;interleaved=%d-%d;mode=record" CRLF \
									"CSeq: %d" CRLF \
									"User-Agent: " CRLF \
									CRLF \
									,RTSP_PREFIX ,(uint8_t)rtsp_ctx->connect_ctx.remote_ip[0], (uint8_t)rtsp_ctx->connect_ctx.remote_ip[1], (uint8_t)rtsp_ctx->connect_ctx.remote_ip[2], (uint8_t)rtsp_ctx->connect_ctx.remote_ip[3], rtsp_ctx->connect_ctx.remote_port, rtsp_ctx->stream_ctx->stream_name, rtsp_ctx->nb_streams_setup, castmode, rtsp_ctx->transport[rtsp_ctx->nb_streams_setup].interleaved_low, rtsp_ctx->transport[rtsp_ctx->nb_streams_setup].interleaved_high, rtsp_ctx->CSeq);	
	}else if(rtsp_ctx->transport[rtsp_ctx->nb_streams_setup].castMode == MULTICAST_MODE)
	{
//		sprintf(rtsp_ctx->request, "SETUP %s%d.%d.%d.%d:%d/%s RTSP/1.0" CRLF \
//									"Transport: RTP/AVP/UDP;%s;port=%d-%d;ttl=%d" CRLF \
//									"CSeq: %d" CRLF \
//									"User-Agent: " CRLF \
//									CRLF \
//									,RTSP_PREFIX ,(uint8_t)rtsp_ctx->connect_ctx.remote_ip[0], (uint8_t)rtsp_ctx->connect_ctx.remote_ip[1], (uint8_t)rtsp_ctx->connect_ctx.remote_ip[2], (uint8_t)rtsp_ctx->connect_ctx.remote_ip[3], rtsp_ctx->connect_ctx.remote_port, rtsp_ctx->stream_ctx->stream_name, castmode, rtsp_ctx->transport[rtsp_ctx->nb_streams_setup].port_low, rtsp_ctx->transport[rtsp_ctx->nb_streams_setup].port_high, rtsp_ctx->transport[rtsp_ctx->nb_streams_setup].ttl, rtsp_ctx->CSeq);
	}

	rtw_mutex_get(&rtsp_ctx->socket_lock);
	ok = rtsp_write(rtsp_ctx, buf, strlen(buf));
	rtw_mutex_put(&rtsp_ctx->socket_lock);
	if (ok < 0){
		RTSP_DBG_ERROR("send SETUP request failed!\n");
	return 0;
	}
	
	return 1;
}

int rtsp_setup_request(struct rtsp_context *rtsp_ctx)
{
	if((rtsp_ctx->state != RTSP_INIT) && (rtsp_ctx->nb_streams_setup >= rtsp_ctx->nb_streams))
		return 0;
	
	if (!rtsp_send_setup_request(rtsp_ctx))
		return 0;

    RTSP_DBG_INFO("++++++++++++ SETUP Request: ++++++++++++\n%s\n", rtsp_ctx->request);

	if (rtsp_receive_response(rtsp_ctx) <= 0){
		RTSP_DBG_ERROR("Error: Server did not respond properly, closing...\n");
		return 0;
	}

    RTSP_DBG_INFO("++++++++++++ SETUP Reply: ++++++++++++\n%s\n", rtsp_ctx->response);

	if (!rtsp_check_response_status(rtsp_ctx))	//200 ok
		return 0;

	if (rtsp_ctx->transport[rtsp_ctx->nb_streams_setup].castMode == UNICAST_UDP_MODE){
		rtsp_parse_server_udp_ip(rtsp_ctx);
		if(!rtsp_parse_server_udp_port(rtsp_ctx))
			return 0;
	}

	if(!rtsp_parse_sessionid(rtsp_ctx))
		return 0;

	rtsp_ctx->state = RTSP_READY;
	rtsp_ctx->stream_ctx[rtsp_ctx->setup_stream_index].setup_done = 1;
	rtsp_ctx->nb_streams_setup++;
	RTSP_DBG_INFO("state changed from RTSP_INIT to RTSP_READY\n");
	
    rtsp_ctx->request_type = REQUEST_RECORD;
    return 1;
}

static int rtsp_send_record_request(struct rtsp_context *rtsp_ctx)
{
    char *buf = rtsp_ctx->request;
	int ok;

	memset(buf, 0, RTSP_REQUEST_BUF_SIZE);
	sprintf(buf, "RECORD %s%d.%d.%d.%d:%d/%s RTSP/1.0" CRLF \
				"Range: npt=0.000-" CRLF \
				"CSeq: %d" CRLF \
				"User-Agent: " CRLF \
				"Session: %u" CRLF \
				CRLF \
				,RTSP_PREFIX ,(uint8_t)rtsp_ctx->connect_ctx.remote_ip[0], (uint8_t)rtsp_ctx->connect_ctx.remote_ip[1], (uint8_t)rtsp_ctx->connect_ctx.remote_ip[2], (uint8_t)rtsp_ctx->connect_ctx.remote_ip[3], rtsp_ctx->connect_ctx.remote_port, rtsp_ctx->stream_ctx->stream_name, rtsp_ctx->CSeq, rtsp_ctx->session.id);

	rtw_mutex_get(&rtsp_ctx->socket_lock);
	ok = rtsp_write(rtsp_ctx, buf, strlen(buf));
	rtw_mutex_put(&rtsp_ctx->socket_lock);

	if (ok < 0){
		RTSP_DBG_ERROR("send RECORD request failed!\n");
		return 0;
	}

    return 1;
}

int rtsp_record_request(struct rtsp_context *rtsp_ctx)
{
	if (!rtsp_send_record_request(rtsp_ctx))
		return 0;

	RTSP_DBG_INFO("++++++++++++ RECORD Request: ++++++++++++\n%s\n", rtsp_ctx->request);

	if (rtsp_receive_response(rtsp_ctx) <= 0) {
		RTSP_DBG_ERROR("Error: Server did not respond properly, closing...\n");
		return 0;
	}

	RTSP_DBG_INFO("++++++++++++ RECORD Reply: ++++++++++++\n%s\n", rtsp_ctx->response);

	if (!rtsp_check_response_status(rtsp_ctx))	//200 ok
		return 0;

	rtsp_ctx->state = RTSP_PLAYING;
	rtsp_ctx->request_type = RTSP_KEEPALIVE;
	keep_alive_time = rtw_get_current_time();

	rtsp_ctx->CSeq++;	
	rtsp_get_parameter_request(rtsp_ctx);
	
	return 1;
}

static int rtsp_send_keep_alive_request(struct rtsp_context *rtsp_ctx)
{
	if (rtsp_request_is_supported(rtsp_req_table[7].key, rtsp_ctx)){
		rtsp_get_parameter_request(rtsp_ctx);
	}else{
		rtsp_options_request(rtsp_ctx);
		rtsp_ctx->request_type = RTSP_KEEPALIVE;
	}

	return 1;
}


int rtsp_keep_alive_request(struct rtsp_context *rtsp_ctx)
{
	if(keep_alive_time !=0 ) {
		if(rtw_get_current_time() - keep_alive_time > 50000){
			RTSP_DBG_INFO("+++++++++++++++++++  Keep alive: request  ++++++++++++++++++++++++++\n");
			keep_alive_time = rtw_get_current_time();
			rtsp_send_keep_alive_request(rtsp_ctx);
		}
	}	
	
    return 1;
}

static int rtsp_send_get_parameter_request(struct rtsp_context *rtsp_ctx)
{
    char *buf = rtsp_ctx->request;
	int ok;

    memset(buf, 0, RTSP_REQUEST_BUF_SIZE);
	sprintf(buf, "GET_PARAMETER %s%d.%d.%d.%d:%d/%s RTSP/1.0" CRLF \
				"CSeq: %d" CRLF \
				"User-Agent: " CRLF \
				"Session: %u" CRLF \
				CRLF \
				,RTSP_PREFIX , (uint8_t)rtsp_ctx->connect_ctx.remote_ip[0], (uint8_t)rtsp_ctx->connect_ctx.remote_ip[1], (uint8_t)rtsp_ctx->connect_ctx.remote_ip[2], (uint8_t)rtsp_ctx->connect_ctx.remote_ip[3], rtsp_ctx->connect_ctx.remote_port, rtsp_ctx->stream_ctx->stream_name, rtsp_ctx->CSeq, rtsp_ctx->session.id);

	rtw_mutex_get(&rtsp_ctx->socket_lock);
	ok = rtsp_write(rtsp_ctx, buf, strlen(buf));
	rtw_mutex_put(&rtsp_ctx->socket_lock);

	if (ok < 0){
		RTSP_DBG_ERROR("send GET_PARAMETER request failed!\n");
		return 0;
	}

    return 1;
}

int rtsp_get_parameter_request(struct rtsp_context *rtsp_ctx)
{
	if (!rtsp_send_get_parameter_request(rtsp_ctx))
        return 0;

    RTSP_DBG_INFO("++++++++++++ GET_PARAMETER Request: ++++++++++++\n%s\n", rtsp_ctx->request);
    	
	if (rtsp_receive_response(rtsp_ctx) <= 0){
		RTSP_DBG_ERROR("Error: Server did not respond properly, closing...\n");
		return 0;
	}

	RTSP_DBG_INFO("++++++++++++ GET_PARAMETER Reply: ++++++++++++\n%s\n", rtsp_ctx->response);

	if (!rtsp_check_response_status(rtsp_ctx))	//200 ok
		return 0;

    return 1;
}


static int rtsp_send_teardown_request(struct rtsp_context *rtsp_ctx)
{
	char *buf = rtsp_ctx->request;
	int ok;

    memset(buf, 0, RTSP_REQUEST_BUF_SIZE);
	sprintf(buf, "TEARDOWN %s%d.%d.%d.%d:%d/%s RTSP/1.0" CRLF \
				"CSeq: %d" CRLF \
				"User-Agent: " CRLF \
				"Session: %u" CRLF \
				CRLF \
				,RTSP_PREFIX , (uint8_t)rtsp_ctx->connect_ctx.remote_ip[0], (uint8_t)rtsp_ctx->connect_ctx.remote_ip[1], (uint8_t)rtsp_ctx->connect_ctx.remote_ip[2], (uint8_t)rtsp_ctx->connect_ctx.remote_ip[3], rtsp_ctx->connect_ctx.remote_port, rtsp_ctx->stream_ctx->stream_name, rtsp_ctx->CSeq, rtsp_ctx->session.id);

	rtw_mutex_get(&rtsp_ctx->socket_lock);
	ok = rtsp_write(rtsp_ctx, buf, strlen(buf));
	rtw_mutex_put(&rtsp_ctx->socket_lock);

	if (ok < 0){
		RTSP_DBG_ERROR("send TEARDOWN request failed!\n");
		return 0;
	}

    return 1;
}

int rtsp_teardown_request(struct rtsp_context *rtsp_ctx)
{
    if (!rtsp_send_teardown_request(rtsp_ctx))
        return 0;

    RTSP_DBG_INFO("++++++++++++ TEARDOWN Request: ++++++++++++\n%s\n", rtsp_ctx->request);
			
	if (rtsp_receive_response(rtsp_ctx) <= 0){
		RTSP_DBG_ERROR("Error: Server did not respond properly, closing...\n");
		return 0;
	}

	RTSP_DBG_INFO("++++++++++++ TEARDOWN Reply: ++++++++++++\n%s\n", rtsp_ctx->response);

	if (!rtsp_check_response_status(rtsp_ctx))	//200 ok
		return 0;

	rtsp_ctx->state = RTSP_INIT;
	rtsp_ctx->request_type = RTSP_QUIT;

    return 1;
}



