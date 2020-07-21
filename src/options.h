/*
Copyright (c) 2020 Maarten Baert <info@maartenbaert.be>

This file is part of lowrider.

lowrider is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

lowrider is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with lowrider.  If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once

#include "sample_format.h"

#include <cstdint>

#include <string>

enum lowrider_wakeup_mode {
	lowrider_wakeup_mode_timer,
	lowrider_wakeup_mode_wait,
};

extern bool g_option_help;
extern bool g_option_version;
extern bool g_option_analyze_resampler;
extern bool g_option_test_hardware;

extern bool g_option_trace_loopback;

extern std::string g_option_device_in;
extern std::string g_option_device_out;
extern lowrider_sample_format g_option_format_in;
extern lowrider_sample_format g_option_format_out;
extern uint32_t g_option_channels_in;
extern uint32_t g_option_channels_out;
extern uint32_t g_option_rate_in;
extern uint32_t g_option_rate_out;
extern uint32_t g_option_period_in;
extern uint32_t g_option_period_out;
extern uint32_t g_option_buffer_in;
extern uint32_t g_option_buffer_out;

extern uint32_t g_option_target_level;

extern lowrider_wakeup_mode g_option_wakeup_mode;
extern uint32_t g_option_timer_period;

extern uint32_t g_option_realtime_priority;
extern bool g_option_memory_lock;

extern float g_option_loop_bandwidth;
extern float g_option_initial_drift;
extern float g_option_max_drift;

extern float g_option_resampler_passband;
extern float g_option_resampler_stopband;
extern float g_option_resampler_beta;
extern float g_option_resampler_gain;

void print_help();
void print_version();
void parse_options(int argc, char *argv[]);
