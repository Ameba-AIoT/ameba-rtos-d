#include "ameba_soc.h"


static void Secure_ReadEfuse(u32 addr, u8 size, u8* key)
{
	u32 Idx = 0;
	u32 CtrlSetting = HAL_READ32(SYSTEM_CTRL_BASE, REG_HS_EFUSE_CTRL1_S);
	
	for (Idx = 0; Idx < size; Idx++) {
		EFUSERead8(CtrlSetting, addr + Idx, &key[Idx], L25EOUTVOLTAGE);
	}
}

static void Secure_WriteEfuse(u32 addr, u8 size, u8* key)
{
	u32 Idx = 0;
	u32 CtrlSetting = HAL_READ32(SYSTEM_CTRL_BASE, REG_HS_EFUSE_CTRL1_S);

	for (Idx = 0; Idx < size; Idx++) {
		EFUSE_PMAP_WRITE8(CtrlSetting, addr + Idx, key[Idx], L25EOUTVOLTAGE);
	}
}

#ifdef CONFIG_MP_INCLUDED

IMAGE3_ENTRY_SECTION
NS_ENTRY bool Secure_PGKey(u32 addr, u8 size, u8* key)
{
	u8* ReadBuf = pvPortMalloc(size);
	u8 res;

	if(ReadBuf == NULL)
		return FALSE;

	Secure_WriteEfuse(addr, size, key);
	Secure_ReadEfuse(addr, size, ReadBuf);

	if(_memcmp(key, ReadBuf, size) == 0)
		res = TRUE;
	else
		res = FALSE;

	if (ReadBuf != NULL)
		vPortFree(ReadBuf);

	return res;
}

#endif

