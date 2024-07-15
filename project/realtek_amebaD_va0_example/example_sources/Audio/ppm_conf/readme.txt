Example Description

This example try to adjust ppm slightly.

How to build:
	1. copy the files in directory src to project\realtek_amebaD_va0_example\src\src_hp

Requirement Components:
    AmebaD_QFN88_EVB_V2

Set-up:
	1. Ensure J17 is shorted(PIN2 and PIN3 should be shorted) with jumpers.
	2. Ensure JP11 is shorted with jumpers.
	3. Ensure J19 and J23 are each shorted(PIN2 and PIN3 should be shorted) respectively with jumpers.
	4. Ensure J21 is shorted(PIN2 and PIN3 should be shorted) with jumpers.
	5. Ensure JP12 is shorted with jumpers.
	6. Plug in earphone/speaker into JACK(CTIA).
	7. Ensure J18 and J22 are each shorted(PIN1 and PIN2 should be shorted) respectively with jumpers.

Behavior:
	Listen to audio output from speaker jack, you may not hear the change.
	You can print in interrupt function to observe whether the audio becomes faster or slower.

Note:
	If you want to play PC music connect to board, please define AMIC2_ONLY, and change the jumpers.
		1. Ensure J19 and J23 are each shorted(PIN1 and PIN2 should be shorted) respectively with jumpers.
		2. Ensure J21 is shorted(PIN1 and PIN2 should be shorted) with jumpers.