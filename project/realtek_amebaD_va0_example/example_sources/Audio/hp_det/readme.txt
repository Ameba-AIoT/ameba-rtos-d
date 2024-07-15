Example Description

This example playback bird sing after detect headphone inserted.

How to build:
	1. copy the files in directory src to project\realtek_amebaD_va0_example\src\src_hp

Requirement Components:
    AmebaD_QFN88_EVB_V2

Set-up:
	1. Connect PA22 to the PIN1 of J67.
	2. Ensure J18 and J22 are each shorted(PIN1 and PIN2 should be shorted) respectively with jumpers.

Behavior:
	1. Playback bird sing after plug in earphone into JACK(CTIA), and you can see the log "headphone inserted".
	2. After remove earphone, you can see the log "headphone removed".




