##################################################################################
#                                                                                #
#                        Example example_spi_nand_littlefs                       #
#                                                                                #
##################################################################################


This example is for SPI NAND Flash test based on littlefs-2.50. Using littlefs-2.50, file/folder creation/read/write/delete related code is to be designed. For example, new file "hello.txt" is created, and can be successfully writen to Nand Flash through spi interface.

[Setup Guide]
~~~~~~~~~~~
		In order to run this application example successfully, the hardware setup should be confirm before moving on, and be familiar with spi basic knowledge.
		
/*SPIx pin location:
SPI0:
	S0: PB_18  (MOSI)
		PB_19  (MISO)
		PB_20  (SCLK)
		PB_21  (CS)
	S1: PA_16  (MOSI)
		PA_17  (MISO)
		PA_18  (SCLK)
		PA_19  (CS)
SPI1:
	S0: PA_12  (MOSI)
		PA_13  (MISO)
		PA_14  (SCLK)
		PA_15  (CS)
	S1: PB_4  (MOSI)
		PB_5  (MISO)
		PB_6  (SCLK)
		PB_7  (CS)
*/
		
		1. Hardware setup: 
			Default SPI group currently in use: SPI1_MOSI(PA_12), SPI1_MISO(PA_13), SPI1_SCLK(PA_14), SPI1_CS(PA_15); Connect these pins(the above four pin plus VCC/GND pin) to nand flash module(for example: Micron 8-pin nand flash soldered onto the base ).
			In addition, you can modify related SPI configuration(SPI1:S0->S1, SPI1->SPI0, ...) of nandflash_spi_init() function in \component\common\file_system\littlefs\spi_nand_adapter\littlefs_spi_nand.c, re-setup the hardware connection.
		
		2. Use command 'cd' to change directory to /project/realtek_amebaD_va0_example/GCC-RELEASE/project_hp, type command "make menuconfig" and choose CONFIG SPI NAND:
								[*] Enable SPI NAND
		Pressing <Y> or <space> to select;
		which will auto-generate following lines in platform_autoconf.h:
								#define CONFIG_SPI_NAND_EN 1

		3. Set the parameter CONFIG_EXAMPLE_SPI_NAND_LITTLEFS to 1 in platform_opts.h file.

		4. Rebuild the project and download images to Ameba.
		
		5. Launch any serial port tool (e.g. Realtek Trace Tool) on PC host to receive debugging information.

[Supported List]
~~~~~~~~~~
	    AmebaD