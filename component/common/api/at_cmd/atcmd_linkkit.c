#include <stdio.h>
#include "log_service.h"
#include "cmsis_os.h"
#include <platform/platform_stdlib.h>

#if CONFIG_LINKKIT_AWSS
extern void linkkit_erase_wifi_config();
extern void linkkit_erase_fw_config();
extern int linkkit_write_device_info(int argc, unsigned char **argv);

void fATCA(void *arg)
{
	int argc;
	int ret = -1;
	unsigned char *argv[MAX_ARGC] = {0};
	
	if(!arg) { 
		goto USAGE;
	}
	
	argv[0] = "pk/ps/dn/ds/l/e/fw";
	argc = parse_param(arg, argv);
	if (argc == 3) {
		/*Set firmware: ATCA=fw,xxx, trace/debug/info/warn/fatal/none*/
            
		/* Write device key/device secret */
			ret = linkkit_write_device_info(argc, argv);
			if (ret) {
				goto USAGE;
			} 
			return;
		
	} else if (argc == 2)	{
	/* Erase wifi profile or reset user account binding : "ATCA=e" or "ATCA=r" */
		if (!strcmp(argv[1], "ew")) {
			linkkit_erase_wifi_config();
		}
		if (!strcmp(argv[1], "ef")) {
			linkkit_erase_fw_config();
		}
	}

USAGE:	
	printf("\r\n[ATCA] Write ORDERLY device's key/secret into flash, set log level or erase wifi profile");
	printf("\r\n[ATCA] Usage: ATCA=pk,xxxxxxxx\t(set product key, less than 20-Byte long)");
	printf("\r\n[ATCA] Usage: ATCA=ps,xxxxxxxx\t(set product secret, less than 64-Byte long)");
	printf("\r\n[ATCA] Usage: ATCA=dn,xxxxxxxx\t(set device name, less than 32-Byte long)");
	printf("\r\n[ATCA] Usage: ATCA=ds,xxxxxxxx\t(set device secret, less than 64-Byte long)");
	printf("\r\n[ATCA] Usage: ATCA=fw,xxxxxxxx\t(set firmware version, less than 32-Byte long)");
	printf("\r\n[ATCA] Usage: ATCA=ew\t(erase AP  profile)");
	printf("\r\n[ATCA] Usage: ATCA=ef\t(erase fwVersion  profile)");
	return;	
}
#endif

void fATCx(void *arg)
{	
}

log_item_t at_cloud_items[ ] = {	
#if CONFIG_LINKKIT_AWSS
	{"ATCA", fATCA,},
#endif
    {"ATC?", fATCx,},
};
void at_cloud_init(void)
{
	log_service_add_table(at_cloud_items, sizeof(at_cloud_items)/sizeof(at_cloud_items[0]));
}

