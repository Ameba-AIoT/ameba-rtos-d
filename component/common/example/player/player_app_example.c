/******************************************************************************
*
* Copyright(c) 2007 - 2018 Realtek Corporation. All rights reserved.
*
******************************************************************************/
#include "ameba_soc.h"
#include "FreeRTOS.h"

extern void example_player(u16 argc, u8 *argv[]);

#define CMD_TEST
#ifdef CMD_TEST
CMD_TABLE_DATA_SECTION
const COMMAND_TABLE player_test_cmd_table[] = {
	{
		(const u8 *)"player",  1, example_player, (const u8 *)"\t player\n"
	},
};
#else
void app_example(void)
{
	example_player();
}
#endif
