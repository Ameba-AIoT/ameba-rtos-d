/*
 * Copyright (c) 2023 Realtek, LLC.
 * All rights reserved.
 *
 * Licensed under the Realtek License, Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License from Realtek
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "ameba_soc.h"
#include "audio_hw_config.h"

uint32_t k_audio_hw_amplifier_pin = _PB_28;
uint32_t k_audio_hw_amplifier_enable_time = 55;
uint32_t k_audio_hw_amplifier_disable_time = 180;
bool k_audio_hw_amplifier_pin_initial_state = true;
bool k_audio_hw_use_amplifier = true;
bool k_audio_hw_use_headset = false;
bool k_audio_hw_time_debug = false;