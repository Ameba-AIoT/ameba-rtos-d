/*
 *  Routines to access hardware
 *
 *  Copyright (c) 2013 Realtek Semiconductor Corp.
 *
 *  This module is a confidential and proprietary property of RealTek and
 *  possession or use of this module requires written permission of RealTek.
 */


#include "osdep_service.h"
#include "rtl8721dlp_rcc.h"


static u32 I2CCLK_TABLE[] = {10000000, 100000000, 100000000};

#define I2C_MTR_ID 			0x0
#define I2C_MTR_SDA    		_PA_25
#define I2C_MTR_SCL    		_PA_26
#define I2C_DATA_LENGTH     127

uint8_t	i2cdatasrc[I2C_DATA_LENGTH];
u32 datalength = I2C_DATA_LENGTH;
u8 *pdatabuf;

SemaphoreHandle_t txSemaphore = NULL;
SemaphoreHandle_t rxSemaphore = NULL;

static void i2c_give_sema(u32 IsWrite)
{
	portBASE_TYPE taskWoken = pdFALSE;

	if (IsWrite) {
		xSemaphoreGiveFromISR(txSemaphore, &taskWoken);
	} else {
		xSemaphoreGiveFromISR(rxSemaphore, &taskWoken);
	}
	portEND_SWITCHING_ISR(taskWoken);
}

static void i2c_take_sema(u32 IsWrite)
{
	if (IsWrite) {
		xSemaphoreTake(txSemaphore, RTW_MAX_DELAY);
	} else {
		xSemaphoreTake(rxSemaphore, RTW_MAX_DELAY);
	}
}

void i2c_int_task(void)
{
	int  i2clocalcnt;

	I2C_InitTypeDef  i2C_master;
	I2C_IntModeCtrl i2c_intctrl;
	i2c_intctrl.I2Cx = I2C_DEV_TABLE[I2C_MTR_ID].I2Cx;

	RCC_PeriphClockCmd(APBPeriph_I2C0, APBPeriph_I2C0_CLOCK, ENABLE);

	// prepare for transmission
	_memset(&i2cdatasrc[0], 0x00, I2C_DATA_LENGTH);

	for (i2clocalcnt = 0; i2clocalcnt < I2C_DATA_LENGTH; i2clocalcnt++) {
		i2cdatasrc[i2clocalcnt] = i2clocalcnt + 0x2;
	}

	vSemaphoreCreateBinary(txSemaphore);
	xSemaphoreTake(txSemaphore, 1 / portTICK_RATE_MS);

	vSemaphoreCreateBinary(rxSemaphore);
	xSemaphoreTake(rxSemaphore, 1 / portTICK_RATE_MS);

	I2C_StructInit(&i2C_master);

	i2C_master.I2CIdx = I2C_MTR_ID;
	i2C_master.I2CAckAddr = 0x23;
	i2C_master.I2CSpdMod = I2C_SS_MODE;
	i2C_master.I2CClk = 100;
	i2C_master.I2CIPClk = I2CCLK_TABLE[I2C_MTR_ID];

	i2C_master.I2CTXTL = I2C_TRX_BUFFER_DEPTH / 3;

	i2c_intctrl.I2CSendSem =  i2c_give_sema;
	i2c_intctrl.I2CWaitSem = i2c_take_sema;

	/* I2C Pin Mux Initialization */
	Pinmux_Config(I2C_MTR_SDA, PINMUX_FUNCTION_I2C);
	Pinmux_Config(I2C_MTR_SCL, PINMUX_FUNCTION_I2C);

	PAD_PullCtrl(I2C_MTR_SDA, GPIO_PuPd_UP);
	PAD_PullCtrl(I2C_MTR_SCL, GPIO_PuPd_UP);

	/* I2C HAL Initialization */
	I2C_Init(i2c_intctrl.I2Cx, &i2C_master);

	InterruptRegister((IRQ_FUN)I2C_ISRHandle, I2C_DEV_TABLE[I2C_MTR_ID].IrqNum, (uint32_t)&i2c_intctrl, 7);
	InterruptEn(I2C_DEV_TABLE[I2C_MTR_ID].IrqNum, 7);

	/* I2C Enable Module */
	I2C_Cmd(i2c_intctrl.I2Cx, ENABLE);

	pdatabuf = i2cdatasrc;


	I2C_MasterWriteInt(i2c_intctrl.I2Cx, &i2c_intctrl, pdatabuf, I2C_DATA_LENGTH);
	DelayMs(50);
	I2C_MasterReadInt(i2c_intctrl.I2Cx, &i2c_intctrl, pdatabuf, I2C_DATA_LENGTH);
	DelayMs(50);

	I2C_MasterWriteInt(i2c_intctrl.I2Cx, &i2c_intctrl, pdatabuf, I2C_DATA_LENGTH);
	DelayMs(50);
	I2C_MasterReadInt(i2c_intctrl.I2Cx, &i2c_intctrl, pdatabuf, I2C_DATA_LENGTH);
	DelayMs(50);

	I2C_MasterWriteInt(i2c_intctrl.I2Cx, &i2c_intctrl, pdatabuf, I2C_DATA_LENGTH);
	DelayMs(50);
	I2C_MasterReadInt(i2c_intctrl.I2Cx, &i2c_intctrl, pdatabuf, I2C_DATA_LENGTH);


	while (1);
}

void main(void)
{
	if (xTaskCreate((TaskFunction_t)i2c_int_task, "i2c_task DEMO", (2048 / 4), NULL, (tskIDLE_PRIORITY + 1), NULL) != pdPASS) {
		DBG_8195A("Cannot create i2c_int_task demo task\n\r");
	}

	vTaskStartScheduler();
}

