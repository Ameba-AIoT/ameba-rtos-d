##################################################################################
#                                                                                #
#                             example_cm_backtrace                               #
#                                                                                #
##################################################################################

Date: 2018-06-01

Table of Contents
~~~~~~~~~~~~~~~~~
 - Description
 - Setup Guide
 - Result description
 - Other Reference
 - Supported List

 
Description
~~~~~~~~~~~
        Cm_Backtrace(Cortex Microcontroller Backtrace) is an open source library for ARM Cortex-M MCUs to trace and locate the error codes, 
        it can also analyze the cause of errors automatically.

        The main features are as follows:
        1. The support errors include: assert failure, hard fault, Memory Management Fault, Bus Fault, Usage Fault, Debug Fault and other failure.
        2. Locate the cause of failure when fault occurs automatically and trace the location of the fault code, don¡¦t need to analyze the complicated fault registers manually.
        3. Output the function call stack in the error field (using addr2line tools for precise location).
        4. Support RT-Thread, UCOS, FreeRTOS operating system.
        5. Support Cortex-M0/M3/M4/M7 MCUs.
        6. Support IAR, Keil, GCC compiler.

Other Reference
~~~~~~~~~~~~~~~
        For more details, please refer to UM0163 Realtek Ameba Cm_Backtrace user guide.pdf

Supported List
~~~~~~~~~~~~~~
[Supported List]
	Supported :
	    Ameba-1, Ameba-z,Ameba-d.
	Source code not in project:
	    Ameba-pro