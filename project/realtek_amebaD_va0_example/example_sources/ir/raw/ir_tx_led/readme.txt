Example Description

This example describes how to generate specific WS2812C-2020 waveform needed by LED.in led application, to generate led waveform, carrier frequency should be set more than 10MHz, and duty cycle should be set

Procedure
(1) Copy this main.c to project\realtek_amebaD_va0_example\src\src_hp\main.c. If GCC/Linux build, please copy Makefile to project\realtek_amebaD_va0_example\src\src_hp together. 
(2) Check and update in component\soc\realtek\amebad\fwlib\include\rtl8721d_ir.h
	#define IS_IR_FREQUENCY(F) 							(((F) >= 5000) && ((F) <= 10000000))
(3) Connect TX pin: PB_23 to WS2812C-2020 input pin.

This example shows:
WS2812C-2020 LED signal will be sent from PB23.

[Supported List]
	Supported :
	    Ameba-D
