#ifndef __MMF_DBG_H_
#define __MMF_DBG_H_

#include <stdint.h>
#include "rtsp/rtsp_api.h"

// Define debug group
#define	_MMF_DBG_RTSP_          	0x00000001
#define	_MMF_DBG_RTP_          		0x00000002
#define	_MMF_DBG_ISP_			    0x00000004
#define	_MMF_DBG_ENCODER_			0x00000008

extern uint32_t ConfigDebugMmfErr;
extern uint32_t ConfigDebugMmfWarn;
extern uint32_t ConfigDebugMmfInfo;

#define DBG_MMF_ERR_MSG_ON(x)       (ConfigDebugMmfErr |= (x)\
                                     if (x == _MMF_DBG_RTSP_) rtsp_set_debug_mode(RTSP_DBG_MODE_ERR, 1)\
                                     if (x == _MMF_DBG_RTP_) rtsp_set_debug_mode(RTP_DBG_MODE_ERR, 1))
#define DBG_MMF_WARN_MSG_ON(x)      (ConfigDebugMmfWarn |= (x)\
                                     if (x == _MMF_DBG_RTSP_) rtsp_set_debug_mode(RTSP_DBG_MODE_WARN, 1)\
                                     if (x == _MMF_DBG_RTP_) rtsp_set_debug_mode(RTP_DBG_MODE_WARN, 1))
#define DBG_MMF_INFO_MSG_ON(x)      (ConfigDebugMmfInfo |= (x)\
                                     if (x == _MMF_DBG_RTSP_) rtsp_set_debug_mode(RTSP_DBG_MODE_INFO, 1)\
                                     if (x == _MMF_DBG_RTP_) rtsp_set_debug_mode(RTP_DBG_MODE_INFO, 1))

#define DBG_MMF_ERR_MSG_OFF(x)      (ConfigDebugMmfErr &= ~(x)\
                                     if (x == _MMF_DBG_RTSP_) rtsp_set_debug_mode(RTSP_DBG_MODE_ERR, 0)\
                                     if (x == _MMF_DBG_RTP_) rtsp_set_debug_mode(RTP_DBG_MODE_ERR, 0))
#define DBG_MMF_WARN_MSG_OFF(x)     (ConfigDebugMmfWarn &= ~(x)\
                                     if (x == _MMF_DBG_RTSP_) rtsp_set_debug_mode(RTSP_DBG_MODE_WARN, 0)\
                                     if (x == _MMF_DBG_RTP_) rtsp_set_debug_mode(RTP_DBG_MODE_WARN, 0))
#define DBG_MMF_INFO_MSG_OFF(x)     (ConfigDebugMmfInfo &= ~(x)\
                                     if (x == _MMF_DBG_RTSP_) rtsp_set_debug_mode(RTSP_DBG_MODE_INFO, 0)\
                                     if (x == _MMF_DBG_RTP_) rtsp_set_debug_mode(RTP_DBG_MODE_INFO, 0))

#define ISP_ERR_PREFIX         "[ISP Err]"
#define ISP_WARN_PREFIX        "[ISP Wrn]"
#define ISP_INFO_PREFIX        "[ISP Inf]"

#define ENCODER_ERR_PREFIX         "[Enc Err]"
#define ENCODER_WARN_PREFIX        "[Enc Wrn]"
#define ENCODER_INFO_PREFIX        "[Enc Inf]"


#define MMF_PRINTK      printf

#ifdef ISP_DBG_ERROR
#undef ISP_DBG_ERROR                          
#define ISP_DBG_ERROR(...)													\
			do {															\
				if (ConfigDebugMmfErr&_MMF_DBG_ISP_)						\
					MMF_PRINTK("\n\r" ISP_ERR_PREFIX __VA_ARGS__);			\
			}while(0)
#endif
#define ENCODER_DBG_ERROR(...)												\
			do {															\
				if (ConfigDebugMmfErr&_MMF_DBG_ENCODER_)					\
					MMF_PRINTK("\n\r" ENCODER_ERR_PREFIX __VA_ARGS__);		\
			}while(0)

// DBG_WARNING
#define ISP_DBG_WARNING(...)												\
			do {															\
				if (ConfigDebugMmfWarn&_MMF_DBG_ISP_)						\
					MMF_PRINTK("\n\r" ISP_WARN_PREFIX __VA_ARGS__);			\
			}while(0)
#define ENCODER_DBG_WARNING(...)											\
			do {															\
				if (ConfigDebugMmfWarn&_MMF_DBG_ENCODER_)					\
					MMF_PRINTK("\n\r" ENCODER_WARN_PREFIX __VA_ARGS__);		\
			}while(0)
				
// DBG_INFO
#define ISP_DBG_INFO(...)													\
			do {															\
				if (ConfigDebugMmfInfo&_MMF_DBG_ISP_)						\
					MMF_PRINTK("\n\r" ISP_INFO_PREFIX __VA_ARGS__);			\
			}while(0)
#define ENCODER_DBG_INFO(...)												\
			do {															\
				if (ConfigDebugMmfInfo&_MMF_DBG_ENCODER_)					\
					MMF_PRINTK("\n\r" ENCODER_INFO_PREFIX __VA_ARGS__);		\
			}while(0)
#endif

