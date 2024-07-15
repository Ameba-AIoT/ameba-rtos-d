#include "basic_types.h"
#include "ameba_soc.h"
#include "PinNames.h"
#include "diag.h"
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include <example_entry.h>

#if configENABLE_TRUSTZONE == 0
	#error User should refer to AN for how to enable RDP.
#endif

u32 no_protection_func(u32 data)
{
	u32 result;
	
	result = data/5+3;
	result *=2;
	result +=8;

	return result;
}

void rdp_demo(void)
{
	int i = 0;
	u32 rdp_result;
	u32 no_rdp_result;


	/* Tasks are not created with a secure context. Any task that is going to call secure functions must call 
		portALLOCATE_SECURE_CONTEXT() to allocate itself a secure context before it calls any secure function. */
	portALLOCATE_SECURE_CONTEXT(configMINIMAL_SECURE_STACK_SIZE);
	
	for(i = 0; i < 32; i++){
		rdp_result = rdp_protection_entry(i);
		no_rdp_result = rdp_no_protection_call(no_protection_func, i);

		if(rdp_result != no_rdp_result) {
			DBG_8195A("rdp call fail!");
			DBG_8195A("rdp_result = 0x%x, no_rdp_result=0x%x\n", rdp_result, no_rdp_result);
			goto end;
		}
	}
	
	DBG_8195A("rdp call succeed!");

end:
	vTaskDelete(NULL);
}

void main(void)
{
	DBG_8195A("RDP demo main \n\r");
	
	// create demo Task
	if(xTaskCreate( (TaskFunction_t)rdp_demo, "RDP DEMO", (2048/4), NULL, (tskIDLE_PRIORITY + 1), NULL)!= pdPASS) {
		DBG_8195A("Cannot create demo task\n\r");
		goto end_demo;
	}

#ifdef PLATFORM_FREERTOS
	vTaskStartScheduler();
#endif

end_demo:	
	while(1);
}
