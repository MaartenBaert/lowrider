#include "loopback.h"

#include "backend_alsa.h"
#include "miscmath.h"
#include "options.h"
#include "resampler.h"
#include "signals.h"
#include "timer.h"

#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <ctime>

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <new>
#include <stdexcept>
#include <utility>
#include <vector>

#include <unistd.h>

static void open_devices(lowrider_backend_alsa &backend_alsa) {

	backend_alsa.input_open(g_option_device_in, g_option_format_in, g_option_channels_in, g_option_rate_in, g_option_period_in, g_option_buffer_in);
	g_option_format_in = backend_alsa.input_get_sample_format();
	g_option_channels_in = backend_alsa.input_get_channels();
	g_option_rate_in = backend_alsa.input_get_sample_rate();
	g_option_period_in = backend_alsa.input_get_period_size();
	g_option_buffer_in = backend_alsa.input_get_buffer_size();

	backend_alsa.output_open(g_option_device_out, g_option_format_out, g_option_channels_out, g_option_rate_out, g_option_period_out, g_option_buffer_out);
	g_option_format_out = backend_alsa.input_get_sample_format();
	g_option_channels_out = backend_alsa.output_get_channels();
	g_option_rate_out = backend_alsa.output_get_sample_rate();
	g_option_period_out = backend_alsa.output_get_period_size();
	g_option_buffer_out = backend_alsa.output_get_buffer_size();

}

static uint64_t get_time_nano() {
	timespec ts;
	clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
	return (uint64_t) ts.tv_sec * (uint64_t) 1000000000 + (uint64_t) ts.tv_nsec;
}

void test_hardware() {

	lowrider_backend_alsa backend_alsa;
	open_devices(backend_alsa);

	// fill output buffer
	if(backend_alsa.output_write(nullptr, g_option_buffer_out) != g_option_buffer_out) {
		std::cerr << "Warning: could not fill output buffer" << std::endl;
	}

	// start input and output
	backend_alsa.input_start();
	backend_alsa.output_start();

	// start timer
	lowrider_timer timer;
	timer.start(g_option_timer_period);
	uint64_t last_time = get_time_nano();

	for( ; ; ) {

		uint32_t timer_expired = 0, timer_early = 0, timer_late = 0;
		uint32_t input_blocks = 0;
		uint32_t min_input_samples = 0, max_input_samples = 0;
		uint64_t sum_input_samples = 0, sumsqr_input_samples = 0;
		uint32_t output_blocks = 0;
		uint32_t min_output_samples = 0, max_output_samples = 0;
		uint64_t sum_output_samples = 0, sumsqr_output_samples = 0;
		int64_t input_offset_m1 = 0, input_offset_m2 = 0, input_offset_m3 = 0;
		int64_t output_offset_m1 = 0, output_offset_m2 = 0, output_offset_m3 = 0;

		uint64_t start_time = last_time;
		uint32_t loops = (uint32_t) ((uint64_t) 5000000000 / (uint64_t) g_option_timer_period);
		for(uint32_t loop = 0; loop < loops; ++loop) {

			// should we stop?
			if(g_sigint_flag) {
				return;
			}

			// wait for timer
			uint32_t expired = timer.wait();
			uint64_t current_time = get_time_nano();

			// check the time
			if(expired != 1) {
				++timer_expired;
			}
			if(current_time < last_time + (uint64_t) g_option_timer_period * 3 / 4) {
				++timer_early;
			}
			if(current_time > last_time + (uint64_t) g_option_timer_period * 5 / 4) {
				++timer_late;
			}
			last_time = current_time;

			// make sure that the input and output are still running
			if(!backend_alsa.input_running()) {
				throw std::runtime_error("input stopped unexpectedly");
			}
			if(!backend_alsa.output_running()) {
				throw std::runtime_error("output stopped unexpectedly");
			}

			// read from input
			uint32_t input_samples = backend_alsa.input_read(nullptr, g_option_buffer_in);
			if(input_samples != 0) {
				if(input_samples < min_input_samples || input_blocks == 0) {
					min_input_samples = input_samples;
				}
				if(input_samples > max_input_samples) {
					max_input_samples = input_samples;
				}
				sum_input_samples += (uint64_t) input_samples;
				sumsqr_input_samples += sqr((uint64_t) input_samples);
				++input_blocks;
			}

			// write to output
			uint32_t output_samples = backend_alsa.output_write(nullptr, g_option_buffer_out);
			if(output_samples != 0) {
				if(output_samples < min_output_samples || output_blocks == 0) {
					min_output_samples = output_samples;
				}
				if(output_samples > max_output_samples) {
					max_output_samples = output_samples;
				}
				sum_output_samples += (uint64_t) output_samples;
				sumsqr_output_samples += sqr((uint64_t) output_samples);
				++output_blocks;
			}

			// calculate offsets
			int64_t input_offset = (int64_t) sum_input_samples - (int64_t) ((current_time - start_time) * (uint64_t) g_option_rate_in / (uint64_t) 1000000000);
			input_offset_m1 += input_offset;
			input_offset_m2 += input_offset * loop;
			input_offset_m3 += sqr(input_offset);
			int64_t output_offset = (int64_t) sum_output_samples - (int64_t) ((current_time - start_time) * (uint64_t) g_option_rate_out / (uint64_t) 1000000000);
			output_offset_m1 += output_offset;
			output_offset_m2 += output_offset * loop;
			output_offset_m3 += sqr(output_offset);

		}

		// calculate statistics
		double avg_in = (double) sum_input_samples / (double) input_blocks;
		double std_in = std::sqrt((double) sumsqr_input_samples / (double) input_blocks - sqr(avg_in));
		double avg_out = (double) sum_output_samples / (double) output_blocks;
		double std_out = std::sqrt((double) sumsqr_output_samples / (double) output_blocks - sqr(avg_out));
		double input_m1 = (double) input_offset_m1 / (double) loops;
		double input_m2 = ((double) input_offset_m2 + 0.5 * (double) input_offset_m1) / sqr((double) loops);
		double input_m3 = (double) input_offset_m3 / (double) loops;
		double input_jitter = std::sqrt(input_m3 - 4.0 * sqr(input_m1) - 12.0 * sqr(input_m2) + 12.0 * input_m1 * input_m2);
		double output_m1 = (double) output_offset_m1 / (double) loops;
		double output_m2 = ((double) output_offset_m2 + 0.5 * (double) output_offset_m1) / sqr((double) loops);
		double output_m3 = (double) output_offset_m3 / (double) loops;
		double output_jitter = std::sqrt(output_m3 - 4.0 * sqr(output_m1) - 12.0 * sqr(output_m2) + 12.0 * output_m1 * output_m2);

		// print statistics
		std::ios_base::fmtflags flags(std::cout.flags());
		std::cout << std::fixed << std::setprecision(2);
		std::cout << "Stats:";
		std::cout << " expired=" << timer_expired;
		std::cout << " early=" << timer_early;
		std::cout << " late=" << timer_late;
		std::cout << " blocks_in=" << input_blocks;
		std::cout << " min_in=" << min_input_samples;
		std::cout << " max_in=" << max_input_samples;
		std::cout << " avg_in=" << avg_in;
		std::cout << " std_in=" << std_in;
		std::cout << " blocks_out=" << output_blocks;
		std::cout << " min_out=" << min_output_samples;
		std::cout << " max_out=" << max_output_samples;
		std::cout << " avg_out=" << avg_out;
		std::cout << " std_out=" << std_out;
		std::cout << " jitter_in=" << input_jitter;
		std::cout << " jitter_out=" << output_jitter;
		std::cout << std::endl;
		std::cout.flags(flags);

	}

}

void run_loopback() {

	lowrider_backend_alsa backend_alsa;
	open_devices(backend_alsa);

	if(g_option_channels_in != g_option_channels_out) {
		throw std::runtime_error("different number of input and output channels, channel remapping is not supported yet");
	}

	if(g_option_target_level > g_option_buffer_out / 2) {
		g_option_target_level = g_option_buffer_out / 2;
		std::cerr << "Warning: target level reduced to " << g_option_target_level << " to avoid overrun" << std::endl;
	}

	// calculate loop filter parameters
	float max_loop_bandwidth = 0.02f / (1.0e-9f * (float) g_option_timer_period);
	if(g_option_loop_bandwidth > max_loop_bandwidth) {
		g_option_loop_bandwidth = max_loop_bandwidth;
		std::cerr << "Warning: loop bandwidth reduced to " << g_option_loop_bandwidth << " to ensure stability" << std::endl;
	}
	float loop_p = 2.0f * (float) M_PI * g_option_loop_bandwidth;
	float loop_i = 0.25f * sqr(loop_p) * 1.0e-9f * (float) g_option_timer_period;
	float loop_f1 = 6.0f * loop_p * 1.0e-9f * (float) g_option_timer_period;
	float loop_f2 = 10.0f * loop_p * 1.0e-9f * (float) g_option_timer_period;

	// initialize loop filter
	float nominal_ratio = (float) g_option_rate_in / (float) g_option_rate_out;
	float current_drift = 0.0f, current_filt1 = 0.0f, current_filt2 = 0.0f;

	// create resampler
	lowrider_resampler resampler(nominal_ratio, g_option_resampler_passband, g_option_resampler_stopband, g_option_resampler_beta, g_option_resampler_gain);

	// calculate memory size
	uint32_t filter_length = resampler.get_filter_length();
	uint32_t input_data_size = filter_length + g_option_period_in;
	uint32_t output_data_size = (uint32_t) ((uint64_t) g_option_period_in * (uint64_t) (3 * g_option_rate_out) / (uint64_t) (2 * g_option_rate_in)) + 4;
	uint32_t input_data_stride = (input_data_size + 3) / 4 * 4;
	uint32_t output_data_stride = (output_data_size + 3) / 4 * 4;
	float *input_memory = nullptr, *output_memory = nullptr;

	try {

		// allocate memory
		input_memory = (float*) aligned_alloc(4 * sizeof(float), g_option_channels_in * input_data_stride * sizeof(float));
		if(input_memory == nullptr) {
			throw std::bad_alloc();
		}
		output_memory = (float*) aligned_alloc(4 * sizeof(float), g_option_channels_out * output_data_stride * sizeof(float));
		if(output_memory == nullptr) {
			throw std::bad_alloc();
		}

		// initialize data pointers
		std::vector<float*> input_data(g_option_channels_in), output_data(g_option_channels_out);
		for(uint32_t i = 0; i < g_option_channels_in; ++i) {
			input_data[i] = input_memory + input_data_stride * i + filter_length;
		}
		for(uint32_t i = 0; i < g_option_channels_out; ++i) {
			output_data[i] = output_memory + output_data_stride * i + filter_length;
		}

		// initialize resampler buffer
		std::vector<float*> input_resampler(g_option_channels_in);
		uint32_t resampler_pos = 0;
		for(uint32_t i = 0; i < g_option_channels_in; ++i) {
			std::fill_n(input_data[i] - filter_length, filter_length, 0.0f);
		}

		// fill output buffer
		uint32_t warmup_target_level = g_option_target_level * 3 / 2;
		if(backend_alsa.output_write(nullptr, warmup_target_level) != warmup_target_level) {
			std::cerr << "Warning: could not fill output buffer" << std::endl;
		}

		// start input and output
		backend_alsa.input_start();
		backend_alsa.output_start();

		// start timer
		lowrider_timer timer;
		timer.start(g_option_timer_period);

		// warmup
		std::cerr << "Info: starting warmup" << std::endl;
		uint32_t input_samples_warmup = 0, output_samples_warmup = 0;
		while(input_samples_warmup < 4 * g_option_buffer_in && output_samples_warmup < 4 * g_option_buffer_out) {

			// should we stop?
			if(g_sigint_flag) {
				return;
			}

			// wait for timer
			timer.wait();

			// make sure that the input and output are still running
			if(!backend_alsa.input_running()) {
				throw std::runtime_error("input stopped unexpectedly");
			}
			if(!backend_alsa.output_running()) {
				throw std::runtime_error("output stopped unexpectedly");
			}

			// read from input
			input_samples_warmup += backend_alsa.input_read(nullptr, g_option_buffer_in);

			// write to output
			uint32_t buffer_used = backend_alsa.output_get_buffer_used();
			if(buffer_used < warmup_target_level) {
				output_samples_warmup += backend_alsa.output_write(nullptr, warmup_target_level - buffer_used);
			}

		}

		// loopback
		std::cerr << "Info: starting loopback" << std::endl;
		for( ; ; ) {

			// should we stop?
			if(g_sigint_flag) {
				return;
			}

			// wait for timer
			timer.wait();

			// make sure that the input and output are still running
			if(!backend_alsa.input_running()) {
				throw std::runtime_error("input stopped unexpectedly");
			}
			if(!backend_alsa.output_running()) {
				throw std::runtime_error("output stopped unexpectedly");
			}

			// read from input
			uint32_t input_samples = backend_alsa.input_read(input_data.data(), g_option_period_in);
			if(input_samples != 0) {

				// resample
				uint32_t output_samples = 0;
				if(resampler_pos < filter_length + input_samples) {
					for(uint32_t i = 0; i < g_option_channels_in; ++i) {
						input_resampler[i] = input_data[i] - filter_length + resampler_pos;
					}
					resampler.set_ratio(nominal_ratio / (1.0f + clamp(current_filt2, -0.5f, 0.5f)));
					auto p = resampler.resample(g_option_channels_in,
												input_resampler.data(), filter_length + input_samples - resampler_pos,
												output_data.data(), output_data_size);
					output_samples = p.second;
					resampler_pos += p.first;
				}
				for(uint32_t i = 0; i < g_option_channels_in; ++i) {
					std::copy(input_data[i] - filter_length + input_samples, input_data[i] + input_samples, input_data[i] - filter_length);
				}
				if(input_samples > resampler_pos) {
					std::cerr << "Warning: could not resample all samples" << std::endl;
					resampler_pos = 0;
				} else {
					resampler_pos -= input_samples;
				}

				// write to output
				if(backend_alsa.output_write(output_data.data(), output_samples) != output_samples) {
					std::cerr << "Warning: could not write all samples" << std::endl;
				}

			}

			// update loop filter
			uint32_t buffer_used = backend_alsa.output_get_buffer_used();
			float error = (float) ((int32_t) g_option_target_level - (int32_t) buffer_used) / (float) g_option_rate_out;
			current_drift = clamp(current_drift + error * loop_i, -g_option_max_drift, g_option_max_drift);
			current_filt1 += (error * loop_p + current_drift - current_filt1) * loop_f1;
			current_filt2 += (current_filt1 - current_filt2) * loop_f2;

			std::cout << buffer_used << " " << current_drift << " " << current_filt2 << std::endl;

		}

		// free memory
		free(output_memory);
		output_memory = nullptr;
		free(input_memory);
		input_memory = nullptr;

	} catch(...) {
		free(output_memory);
		free(input_memory);
		throw;
	}

}
