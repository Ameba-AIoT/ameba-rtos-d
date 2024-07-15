#include "platform_autoconf.h"
#ifdef CONFIG_GHD_EN
#include "FreeRTOS.h"
#include "task.h"
//#include "gh_dongle_wifi.h"

void example_game_hdst_dongle(void)
{
	ghd_init_task();
	 
	ghd_usbd_audio_task();
	
   	return;
}

#endif