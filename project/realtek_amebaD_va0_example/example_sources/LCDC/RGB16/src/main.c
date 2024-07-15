#include "ameba_soc.h"
#include "main.h"
#include "osdep_service.h"
#include "spi_api.h"
#include "spi_ex_api.h"

#define LCD_HSW				42
#define LCD_HFP				3
#define LCD_HBP				20
#define LCD_VSW				15
#define LCD_VFP				2
#define LCD_VBP				13
#define LCD_SyncMode		LCDC_RGB_DE_MODE
#define LCD_SampleEdge		LCDC_RGB_DCLK_RISING_EDGE_FETCH
#define LCD_RefreshFreq		50

#define RST 				_PA_16
#define SPI1_MOSI			_PA_12
#define SPI1_MISO			_PA_13
#define SPI1_SCLK			_PA_14
#define SPI1_CS				_PA_15

#define WIDTH		480
#define HEIGHT		480
#define MEM_SIZE	WIDTH * HEIGHT * 2		//rgb565

#define RED			0xF800
#define GREEN		0x07C0
#define BLUE		0x001F
#define BLACK		0x0000
#define WHITE		0xFFFF


PSRAM_BSS_SECTION
u8 PIX_DATA[MEM_SIZE] __attribute__((aligned(64)));

spi_t spi_lcd;

static void LCD_SPI_WR_REG(u16 data)
{
	spi_master_write(&spi_lcd, data);
}

static void LCD_SPI_WR_DATA(u16 data)
{
	spi_master_write(&spi_lcd, data | BIT8);
}

static void lcd_init_spi(void)
{
	GPIO_InitTypeDef ResetPin;

	/* Init SPI controller */
	Pinmux_Config(SPI1_MOSI, PINMUX_FUNCTION_SPIM);
	Pinmux_Config(SPI1_MISO, PINMUX_FUNCTION_SPIM);
	Pinmux_Config(SPI1_SCLK, PINMUX_FUNCTION_SPIM);
	Pinmux_Config(SPI1_CS, PINMUX_FUNCTION_SPIM);

	spi_lcd.spi_idx = MBED_SPI1;
	spi_init(&spi_lcd, SPI1_MOSI, SPI1_MISO, SPI1_SCLK, SPI1_CS);
	spi_frequency(&spi_lcd, 5000000);
	spi_format(&spi_lcd, 9, 3, 0);

	/* Reset LCD */
	ResetPin.GPIO_Pin = RST;
	ResetPin.GPIO_PuPd = GPIO_PuPd_NOPULL;
	ResetPin.GPIO_Mode = GPIO_Mode_OUT;
	GPIO_Init(&ResetPin);

	GPIO_WriteBit(RST, 1);
	DelayMs(100);

	GPIO_WriteBit(RST, 0);
	DelayMs(100);

	GPIO_WriteBit(RST, 1);
	DelayMs(120);

//************* Start Initial Sequence **********//
	LCD_SPI_WR_REG(0xFF);
	LCD_SPI_WR_DATA(0x77);
	LCD_SPI_WR_DATA(0x01);
	LCD_SPI_WR_DATA(0x00);
	LCD_SPI_WR_DATA(0x00);
	LCD_SPI_WR_DATA(0x13);

	LCD_SPI_WR_REG(0xEF);
	LCD_SPI_WR_DATA(0x08);

	LCD_SPI_WR_REG(0xFF);
	LCD_SPI_WR_DATA(0x77);
	LCD_SPI_WR_DATA(0x01);
	LCD_SPI_WR_DATA(0x00);
	LCD_SPI_WR_DATA(0x00);
	LCD_SPI_WR_DATA(0x10);

	LCD_SPI_WR_REG(0xC0);
	LCD_SPI_WR_DATA(0x3B);
	LCD_SPI_WR_DATA(0x00);

	LCD_SPI_WR_REG(0xC1);
	LCD_SPI_WR_DATA(0x0E);
	LCD_SPI_WR_DATA(0x0C);

	LCD_SPI_WR_REG(0xC2);
	LCD_SPI_WR_DATA(0x07);
	LCD_SPI_WR_DATA(0x0A);

	LCD_SPI_WR_REG(0xCC);
	LCD_SPI_WR_DATA(0x30);

	LCD_SPI_WR_REG(0xB0);
	LCD_SPI_WR_DATA(0x40);
	LCD_SPI_WR_DATA(0x07);
	LCD_SPI_WR_DATA(0x53);
	LCD_SPI_WR_DATA(0x0E);
	LCD_SPI_WR_DATA(0x12);
	LCD_SPI_WR_DATA(0x07);
	LCD_SPI_WR_DATA(0x0A);
	LCD_SPI_WR_DATA(0x09);
	LCD_SPI_WR_DATA(0x09);
	LCD_SPI_WR_DATA(0x26);
	LCD_SPI_WR_DATA(0x05);
	LCD_SPI_WR_DATA(0x10);
	LCD_SPI_WR_DATA(0x0D);
	LCD_SPI_WR_DATA(0x6E);
	LCD_SPI_WR_DATA(0x3B);
	LCD_SPI_WR_DATA(0xD6);

	LCD_SPI_WR_REG(0xB1);
	LCD_SPI_WR_DATA(0x40);
	LCD_SPI_WR_DATA(0x17);
	LCD_SPI_WR_DATA(0x5C);
	LCD_SPI_WR_DATA(0x0D);
	LCD_SPI_WR_DATA(0x11);
	LCD_SPI_WR_DATA(0x06);
	LCD_SPI_WR_DATA(0x08);
	LCD_SPI_WR_DATA(0x08);
	LCD_SPI_WR_DATA(0x08);
	LCD_SPI_WR_DATA(0x22);
	LCD_SPI_WR_DATA(0x03);
	LCD_SPI_WR_DATA(0x12);
	LCD_SPI_WR_DATA(0x11);
	LCD_SPI_WR_DATA(0x65);
	LCD_SPI_WR_DATA(0x28);
	LCD_SPI_WR_DATA(0xE8);

	LCD_SPI_WR_REG(0xFF);
	LCD_SPI_WR_DATA(0x77);
	LCD_SPI_WR_DATA(0x01);
	LCD_SPI_WR_DATA(0x00);
	LCD_SPI_WR_DATA(0x00);
	LCD_SPI_WR_DATA(0x11);

	LCD_SPI_WR_REG(0xB0);
	LCD_SPI_WR_DATA(0x4D);

	LCD_SPI_WR_REG(0xB1);
	LCD_SPI_WR_DATA(0x4C);

	LCD_SPI_WR_REG(0xB2);
	LCD_SPI_WR_DATA(0x81);

	LCD_SPI_WR_REG(0xB3);
	LCD_SPI_WR_DATA(0x80);

	LCD_SPI_WR_REG(0xB5);
	LCD_SPI_WR_DATA(0x4C);

	LCD_SPI_WR_REG(0xB7);
	LCD_SPI_WR_DATA(0x85);

	LCD_SPI_WR_REG(0xB8);
	LCD_SPI_WR_DATA(0x33);

	LCD_SPI_WR_REG(0xC1);
	LCD_SPI_WR_DATA(0x78);

	LCD_SPI_WR_REG(0xC2);
	LCD_SPI_WR_DATA(0x78);

	LCD_SPI_WR_REG(0xD0);
	LCD_SPI_WR_DATA(0x88);

	LCD_SPI_WR_REG(0xE0);
	LCD_SPI_WR_DATA(0x00);
	LCD_SPI_WR_DATA(0x00);
	LCD_SPI_WR_DATA(0x02);

	LCD_SPI_WR_REG(0xE1);
	LCD_SPI_WR_DATA(0x05);
	LCD_SPI_WR_DATA(0x30);
	LCD_SPI_WR_DATA(0x00);
	LCD_SPI_WR_DATA(0x00);
	LCD_SPI_WR_DATA(0x06);
	LCD_SPI_WR_DATA(0x30);
	LCD_SPI_WR_DATA(0x00);
	LCD_SPI_WR_DATA(0x00);
	LCD_SPI_WR_DATA(0x0E);
	LCD_SPI_WR_DATA(0x30);
	LCD_SPI_WR_DATA(0x30);

	LCD_SPI_WR_REG(0xE2);
	LCD_SPI_WR_DATA(0x10);
	LCD_SPI_WR_DATA(0x10);
	LCD_SPI_WR_DATA(0x30);
	LCD_SPI_WR_DATA(0x30);
	LCD_SPI_WR_DATA(0xF4);
	LCD_SPI_WR_DATA(0x00);
	LCD_SPI_WR_DATA(0x00);
	LCD_SPI_WR_DATA(0x00);
	LCD_SPI_WR_DATA(0xF4);
	LCD_SPI_WR_DATA(0x00);
	LCD_SPI_WR_DATA(0x00);
	LCD_SPI_WR_DATA(0x00);

	LCD_SPI_WR_REG(0xE3);
	LCD_SPI_WR_DATA(0x00);
	LCD_SPI_WR_DATA(0x00);
	LCD_SPI_WR_DATA(0x11);
	LCD_SPI_WR_DATA(0x11);

	LCD_SPI_WR_REG(0xE4);
	LCD_SPI_WR_DATA(0x44);
	LCD_SPI_WR_DATA(0x44);

	LCD_SPI_WR_REG(0xE5);
	LCD_SPI_WR_DATA(0x0A);
	LCD_SPI_WR_DATA(0xF4);
	LCD_SPI_WR_DATA(0x30);
	LCD_SPI_WR_DATA(0xF0);
	LCD_SPI_WR_DATA(0x0C);
	LCD_SPI_WR_DATA(0xF6);
	LCD_SPI_WR_DATA(0x30);
	LCD_SPI_WR_DATA(0xF0);
	LCD_SPI_WR_DATA(0x06);
	LCD_SPI_WR_DATA(0xF0);
	LCD_SPI_WR_DATA(0x30);
	LCD_SPI_WR_DATA(0xF0);
	LCD_SPI_WR_DATA(0x08);
	LCD_SPI_WR_DATA(0xF2);
	LCD_SPI_WR_DATA(0x30);
	LCD_SPI_WR_DATA(0xF0);

	LCD_SPI_WR_REG(0xE6);
	LCD_SPI_WR_DATA(0x00);
	LCD_SPI_WR_DATA(0x00);
	LCD_SPI_WR_DATA(0x11);
	LCD_SPI_WR_DATA(0x11);

	LCD_SPI_WR_REG(0xE7);
	LCD_SPI_WR_DATA(0x44);
	LCD_SPI_WR_DATA(0x44);

	LCD_SPI_WR_REG(0xE8);
	LCD_SPI_WR_DATA(0x0B);
	LCD_SPI_WR_DATA(0xF5);
	LCD_SPI_WR_DATA(0x30);
	LCD_SPI_WR_DATA(0xF0);
	LCD_SPI_WR_DATA(0x0D);
	LCD_SPI_WR_DATA(0xF7);
	LCD_SPI_WR_DATA(0x30);
	LCD_SPI_WR_DATA(0xF0);
	LCD_SPI_WR_DATA(0x07);
	LCD_SPI_WR_DATA(0xF1);
	LCD_SPI_WR_DATA(0x30);
	LCD_SPI_WR_DATA(0xF0);
	LCD_SPI_WR_DATA(0x09);
	LCD_SPI_WR_DATA(0xF3);
	LCD_SPI_WR_DATA(0x30);
	LCD_SPI_WR_DATA(0xF0);

	LCD_SPI_WR_REG(0xE9);
	LCD_SPI_WR_DATA(0x36);
	LCD_SPI_WR_DATA(0x01);

	LCD_SPI_WR_REG(0xEB);
	LCD_SPI_WR_DATA(0x00);
	LCD_SPI_WR_DATA(0x01);
	LCD_SPI_WR_DATA(0xE4);
	LCD_SPI_WR_DATA(0xE4);
	LCD_SPI_WR_DATA(0x44);
	LCD_SPI_WR_DATA(0x88);
	LCD_SPI_WR_DATA(0x33);

	LCD_SPI_WR_REG(0xED);
	LCD_SPI_WR_DATA(0x20);
	LCD_SPI_WR_DATA(0xFA);
	LCD_SPI_WR_DATA(0xB7);
	LCD_SPI_WR_DATA(0x76);
	LCD_SPI_WR_DATA(0x65);
	LCD_SPI_WR_DATA(0x54);
	LCD_SPI_WR_DATA(0x4F);
	LCD_SPI_WR_DATA(0xFF);
	LCD_SPI_WR_DATA(0xFF);
	LCD_SPI_WR_DATA(0xF4);
	LCD_SPI_WR_DATA(0x45);
	LCD_SPI_WR_DATA(0x56);
	LCD_SPI_WR_DATA(0x67);
	LCD_SPI_WR_DATA(0x7B);
	LCD_SPI_WR_DATA(0xAF);
	LCD_SPI_WR_DATA(0x02);

	LCD_SPI_WR_REG(0xEF);
	LCD_SPI_WR_DATA(0x10);
	LCD_SPI_WR_DATA(0x0D);
	LCD_SPI_WR_DATA(0x04);
	LCD_SPI_WR_DATA(0x08);
	LCD_SPI_WR_DATA(0x3F);
	LCD_SPI_WR_DATA(0x1F);

	LCD_SPI_WR_REG(0xFF);
	LCD_SPI_WR_DATA(0x77);
	LCD_SPI_WR_DATA(0x01);
	LCD_SPI_WR_DATA(0x00);
	LCD_SPI_WR_DATA(0x00);
	LCD_SPI_WR_DATA(0x10);

	LCD_SPI_WR_REG(0x11);

	DelayMs(120);

	LCD_SPI_WR_REG(0x29);
}


void lcd_init(void)
{
	LCDC_RGBInitTypeDef LCDC_RGBInitStruct;

	RCC_PeriphClockCmd(APBPeriph_LCDC, APBPeriph_LCDC_CLOCK, ENABLE);

	/* Enable LCDC pinmux, default 16bit I/F */
	Pinmux_Config(_PA_19, PINMUX_FUNCTION_LCD);  /*D0*/
	Pinmux_Config(_PA_20, PINMUX_FUNCTION_LCD);  /*D1*/
	Pinmux_Config(_PA_23, PINMUX_FUNCTION_LCD);  /*D2*/
	Pinmux_Config(_PA_24, PINMUX_FUNCTION_LCD);  /*D3*/
	Pinmux_Config(_PA_31, PINMUX_FUNCTION_LCD);  /*D4*/
	Pinmux_Config(_PB_0, PINMUX_FUNCTION_LCD);  /*D5*/
	Pinmux_Config(_PA_30, PINMUX_FUNCTION_LCD);  /*D6*/
	Pinmux_Config(_PA_28, PINMUX_FUNCTION_LCD);  /*D7*/
	Pinmux_Config(_PA_26, PINMUX_FUNCTION_LCD);  /*D8*/
	Pinmux_Config(_PA_25, PINMUX_FUNCTION_LCD);  /*D9*/
	Pinmux_Config(_PB_8, PINMUX_FUNCTION_LCD);  /*D10*/
	Pinmux_Config(_PB_9, PINMUX_FUNCTION_LCD);  /*D11*/
	Pinmux_Config(_PB_10, PINMUX_FUNCTION_LCD);  /*D12*/
	Pinmux_Config(_PB_11, PINMUX_FUNCTION_LCD);  /*D13*/
	Pinmux_Config(_PB_18, PINMUX_FUNCTION_LCD);  /*D14*/
	Pinmux_Config(_PB_19, PINMUX_FUNCTION_LCD);  /*D15*/
	Pinmux_Config(_PB_20, PINMUX_FUNCTION_LCD);  /*VSYNC, TE*/
	Pinmux_Config(_PB_22, PINMUX_FUNCTION_LCD);  /*RD, HSYNC, LATCH*/
	Pinmux_Config(_PB_23, PINMUX_FUNCTION_LCD);  /*WR, DCLK, LED_CLK*/
	Pinmux_Config(_PB_28, PINMUX_FUNCTION_LCD);  /*CS, ENABLE, OE*/
	PAD_PullCtrl(_PB_28, GPIO_PuPd_DOWN);
	PAD_PullCtrl(_PB_20, GPIO_PuPd_UP);
	PAD_PullCtrl(_PB_22, GPIO_PuPd_UP);
	PAD_PullCtrl(_PB_23, GPIO_PuPd_UP);

	/*Init LCD through 3SPI*/
	lcd_init_spi();

	/* init RGB 16bit interface */
	LCDC_RGBStructInit(&LCDC_RGBInitStruct);
	LCDC_RGBInitStruct.LCDC_RGBImgWidth = WIDTH;
	LCDC_RGBInitStruct.LCDC_RGBImgHeight = HEIGHT;
	LCDC_RGBInitStruct.LCDC_RGBHSW = LCD_HSW;
	LCDC_RGBInitStruct.LCDC_RGBHFP = LCD_HFP;
	LCDC_RGBInitStruct.LCDC_RGBHBP = LCD_HBP;
	LCDC_RGBInitStruct.LCDC_RGBVSW = LCD_VSW;
	LCDC_RGBInitStruct.LCDC_RGBVFP = LCD_VFP;
	LCDC_RGBInitStruct.LCDC_RGBVBP = LCD_VBP;
	LCDC_RGBInitStruct.LCDC_RGBSyncMode = LCDC_RGB_DE_MODE;
	LCDC_RGBInitStruct.LCDC_RGBDclkActvEdge = LCDC_RGB_DCLK_FALLING_EDGE_FETCH;
	LCDC_RGBInitStruct.LCDC_RGBIfMode = LCDC_RGB_IF_16_BIT;
	LCDC_RGBInitStruct.LCDC_RGBRefreshFreq = 50;
	LCDC_RGBInit(LCDC, &LCDC_RGBInitStruct);

	/* configure DMA burst size */
	LCDC_DMAModeConfig(LCDC, 2);

	/* configure base address */
	LCDC_DMAImageBaseAddrConfig(LCDC, (u32)PIX_DATA);

	/* enable LCDC, start transfer data */
	LCDC_Cmd(LCDC, ENABLE);
}


void color_display_st7701s(void *param)
{
	(void)(param);
	u16 *pframe = (u16 *)PIX_DATA;
	int len = MEM_SIZE / 10;
	int i;
	u32 temp = 0;

	/* prepare color data */
	for (i = 0; i < len; i++) {
		pframe[i] = RED;
		pframe[i + len * 1] = GREEN;
		pframe[i + len * 2] = BLUE;
		pframe[i + len * 3] = WHITE;
		pframe[i + len * 4] = BLACK;
	}
	DCache_Clean((u32)PIX_DATA, MEM_SIZE);

	/* init controller */
	lcd_init();

	/* display red green blue Cyclically */
	while (1) {
		DelayMs(1000);

		for (i = 0; i < MEM_SIZE / 2; i++) {
			if (temp % 3 == 0) {
				pframe[i] = RED;
			} else if (temp % 3 == 1) {
				pframe[i] = GREEN;
			} else {
				pframe[i] = BLUE;
			}
		}

		DCache_Clean((u32)PIX_DATA, MEM_SIZE);
		temp++;
	}
}


int main(void)
{
	if (xTaskCreate(color_display_st7701s, NULL, 1024, NULL, 1, NULL)) {
		printf("Fail to create task color_display_st7701s\n");
	}

	vTaskStartScheduler();
}

