Example Description

This example describes how to use adc vabt channel to measure 5V voltage.

How to build:
	1. copy main.c to project\realtek_amebaD_va0_example\src\src_hp

Requirement Components:
    none

Set-up:
	Condition 1: On board, Connect VBAT_MEASURE pin to GND
	Condition 2: On board, Connect VBAT_MEASURE pin to 3.3V
	Condition 3: On board, Connect VBAT_MEASURE pin to 5V

Behavior:
	Condition 1: VBAT_MEASURE to GND -> vbat voltage value approches 0mv
	Condition 2: VBAT_MEASURE to 3.3V -> vbat voltage value approches 3300mv
	Condition 3: VBAT_MEASURE to 5V -> vbat voltage value approches 5000mv

 