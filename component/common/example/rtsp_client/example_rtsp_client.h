#include "rtsp_api.h"

#define I2S_DMA_PAGE_SIZE	3840	// 2 ~ 4096
#define I2S_DMA_PAGE_NUM	4		// Vaild number is 2~4
#define RECV_PAGE_NUM 		8

typedef struct i2s_param_s{
	uint32_t	sample_rate;
	uint32_t	word_length;
	uint32_t	direction;
	uint32_t	channel_num;
	uint32_t	slave_mode;
}i2s_params_t;

typedef struct rtsp_client_params_s{
	uint32_t type;
	uint32_t codec_id;
	uint32_t channel;
	uint32_t sample_rate;
	uint32_t cast_mode;
}rtsp_client_params_t;

typedef struct _RTSP_REQUEST_HANDLE{
    int req;
    int (*handle)(struct rtsp_context *);
}RTSP_REQUEST_HANDLE;

u8 i2s_tx_buf[I2S_DMA_PAGE_SIZE*I2S_DMA_PAGE_NUM]__attribute__((aligned(32)));
u8 i2s_rx_buf[I2S_DMA_PAGE_SIZE*I2S_DMA_PAGE_NUM]__attribute__((aligned(32)));

u8 recv_buf[I2S_DMA_PAGE_SIZE*RECV_PAGE_NUM]__attribute__((aligned(32)));
