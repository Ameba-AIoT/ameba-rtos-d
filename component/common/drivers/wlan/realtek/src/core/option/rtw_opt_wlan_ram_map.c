#include <rom_wlan_ram_map.h>

extern struct _rom_wlan_ram_map rom_wlan_ram_map;

extern void *sram0_reserve_malloc(int size);
extern void sram0_reserve_free(void *mem);

static unsigned char *malloc_func(unsigned int sz)
{
	return sram0_reserve_malloc(sz);
}

static void mfree_func(unsigned char *pbuf, unsigned int sz)
{
	sram0_reserve_free(pbuf);
}

void init_rom_wlan_ram_map(void)
{
	rom_wlan_ram_map.rtw_malloc = malloc_func;
	rom_wlan_ram_map.rtw_mfree = mfree_func;
}
