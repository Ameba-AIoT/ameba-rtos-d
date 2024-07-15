# Ameba Audio

## Table of Contents

- [Ameba Audio](#ameba-audio)
	- [Table of Contents](#table-of-contents)
	- [About ](#about-)
	- [Menuconfig ](#menuconfig-)
	- [Configurations ](#configurations-)
	- [How to run ](#how-to-run-)

## About <a name = "about"></a>

Ameba audio project can achieve:
1. audio playback.
2. before using this example, please check the component/audio/frameworks/media/audio/interfaces/audio/audio_track.h to see how to use audio interfaces.

## Menuconfig <a name = "menuconfig"></a>
Please setup menuconfig first to enable audio framework:
```
cd project/realtek_amebaD_va0_example/GCC-RELEASE/project_hp
make menuconfig
---->Audio Config
     ---->Enable Audio Framework
          ---->Select Audio Framework
               ---->Passthrough
```

## Configurations <a name = "configurations"></a>

Please see discriptions in component/soc/realtek/amebad/app/hal/config/audio_hw_config.h.
component/soc/realtek/amebad/app/hal/config/audio_hw_config.c is for audio Hardware configurations.

```
//for example, if user wants to control amp himself, k_audio_hw_use_amplifier should be set as false in audio_hw_config.c:
bool k_audio_hw_use_amplifier = false;
//if user controls audio amplifier by PB10, please set:
uint32_t k_audio_hw_amplifier_pin = _PB_10;
```

## How to run <a name = "How to run"></a>
1. enter project/realtek_amebaD_va0_example/GCC-RELEASE/project_hp
2. Use CMD `make all EXAMPLE=audio_track` to compile this example.
2. For playing run command and parameters after system boot up, please refer to audio_track_app_example.c.
