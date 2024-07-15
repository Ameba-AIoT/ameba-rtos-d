#ifndef __FREERTOS_RT_MEMORY_H
#define __FREERTOS_RT_MEMORY_H

#include <stddef.h>
#include "FreeRTOS.h"

#ifdef __cplusplus
extern "C" {
#endif


void* RtSRAMMalloc(size_t size);
void* RtSRAMCalloc(size_t num, size_t size);
void RtSRAMFree(void *p);
void* RtSRAMReAlloc(void *p, size_t size);
size_t RtSRAMGetFreeHeapSize(void);
size_t RtSRAMGetMinimumEverFreeHeapSize(void);


#ifdef __cplusplus
}
#endif

#endif /* __FREERTOS_RT_MEMORY_H */