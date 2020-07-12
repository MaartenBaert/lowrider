#pragma once

#include "sample_format.h"

#include <cstdint>

#include <string>

extern bool g_option_help;
extern bool g_option_version;
extern bool g_option_analyze_resampler;
extern bool g_option_test_hardware;

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
extern uint32_t g_option_timer_period;

extern float g_option_loop_bandwidth;
extern float g_option_max_drift;

extern float g_option_resampler_passband;
extern float g_option_resampler_stopband;
extern float g_option_resampler_beta;
extern float g_option_resampler_gain;

void print_help();
void print_version();
void parse_options(int argc, char *argv[]);
