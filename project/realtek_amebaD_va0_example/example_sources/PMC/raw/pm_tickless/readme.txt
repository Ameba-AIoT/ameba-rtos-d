Example Description

This example describes how to use freertos tickless with uart interruptable interface.

Configuration
(1) To realize loguart wake event wake up KM0, .km0_osc2m_close of struct ps_config in rtl8721dlp_sleepcfg.c should be set as "FALSE".
(2) As the power of KM4 is controlled by KM0, to realize loguart wake event wake up KM4, BIT_LP_WEVT_UART_STS of hs_wakeevent_config[] in rtl8721dlp_sleepcfg.c should be set as "ON".

Requirement Components:
	USBtoTTL adapter

Connect to PC
 - Connect Ground: connect to GND pin via USBtoTTL adapter
 - Use KM0 LOGUART
	_PA_8 as LOGUART_RX connect to TX of USBtoTTL adapter
	_PA_7 as LOGUART_TX connect to RX of USBtoTTL adapter

Procedure
(1) Freertos will enter tickless automatically.
(2) User can press "Enter" key in log uart to wake KM0 and KM4.
(3) KM4 executes app_loguart_resume() function.
(4) KM4 keeps awake 5s.
(5) KM4 executes app_loguart_suspend() function then enter sleep mode.
(6) KM0 keeps alive 13s then enter sleep mode.
(7) Repeat Steps (2)-(6) to realize chip enter/leave sleep mode.

Notes:
(1) Freertos enter tickless if the wakelock bit map is 0.
	It means there is no function requires system to keep awake.
	By default there is PMU_OS keeping system awake.
	So we need to release this PMU_OS to enable tickless mode.
(2) KM4 keep alive time is adjustable by parameter of pmu_set_sysactive_time(), unit is ms.

