Example Description

This example describes how to use ADC normal channel.

How to build:
	1. copy main.c to project\realtek_amebaD_va0_example\src\src_hp

Requirement Components:
    none

Set-up:
	Condition 1: On board, Connect AD0(PB5) pin to GND
	Condition 2: On board, Connect AD0(PB5) pin to 3.3V
	Condition 3: On board, Connect AD2(PB7) pin to GND
	Condition 4: On board, Connect AD2(PB7) pin to 3.3V

Behavior:
	Condition 1: PB5 to GND -> AD0 voltage value approches 0mv
	Condition 2: PB5 to 3.3V -> AD0 voltage value approches 3300mv
	Condition 3: PB7 to GND -> AD2 voltage value approches 0mv
	Condition 4: PB7 to 3.3V -> AD2 voltage value approches 3300mv



