ENTRY(Reset_Handler)

INCLUDE "rlx8721d_layout.ld"

SECTIONS
{
	.rom.text : { } > IROM
	.rom.rodata : { } > DROM
	.hal.rom.bss : { } > ROMBSS_RAM

	/* image1 entry, this section should in RAM and fixed address for ROM */
	.ram_boot2nd.entry :
	{
		__ram_boot2nd_text_start__ = .;
		__image2_entry_func__ = .;
		KEEP(*(SORT(.image1.entry.data*)))
		KEEP(*(.image1.validate.rodata*))
		KEEP(*(.image1.rodata*))
	} > BOOTLOADER2_RAM

	/*  Add . to assign the start address of the section */ 
	/*  to prevent the change of the start address by ld doing section alignment */  
	.ram_boot2nd.text . :
	{
		/* image1 text */
		*(.boot.ram.text*)    
		*(.boot.rodata*)
		*(.rodata*)
	} > BOOTLOADER2_RAM

	.ram_boot2nd.data . :
	{
		__ram_boot2nd_data_start__ = .;
		KEEP(*(.boot.ram.data*))
		__ram_boot2nd_data_end__ = .;
	
		__ram_boot2nd_text_end__ = .;
	} > BOOTLOADER2_RAM

	.ram_boot2nd.bss . :
	{
		__image1_bss_start__ = .;
		KEEP(*(.boot.ram.bss*))
		KEEP(*(.boot.ram.end.bss*))
		*(.bss*)
		__image1_bss_end__ = .;
	} > BOOTLOADER2_RAM

	.xip_boot2nd.text :
	{
		__flash_boot2nd_text_start__ = .;

		*(.flashboot.text*)
		*(.text*)
		*(.rodata*)
		

		__flash_boot2nd_text_end__ = .;

		. = ALIGN(16);
	} > KM0_BOOT2

	.ram_image2.entry :
	{
		__ram_image2_text_start__ = .;
		__image2_entry_func__ = .;

	} > BD_RAM

	__ls_flash_text_start__ = ORIGIN(KM0_IMG2);
	__hs_flash_text_start__ = ORIGIN(KM4_IMG2);
}
