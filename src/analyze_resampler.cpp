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

#include "analyze_resampler.h"

#include "miscmath.h"
#include "options.h"
#include "resampler.h"

#include <cassert>
#include <cmath>
#include <cstdint>

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <utility>
#include <vector>

void analyze_resampler() {

	// create resampler
	float ratio = (float) g_option_rate_in / (float) g_option_rate_out * 0.999f;
	lowrider_resampler resampler(ratio, g_option_resampler_passband, g_option_resampler_stopband, g_option_resampler_beta, g_option_resampler_gain);
	double actual_rate_out = (double) g_option_rate_in / (double) resampler.get_ratio();
	/*double actual_latency = (double) (resampler.get_filter_length() / 2 - 1) / (double) resampler.get_ratio();*/

	float passband = g_option_resampler_passband * (float) std::min(g_option_rate_in, g_option_rate_out);
	float stopband = g_option_resampler_stopband * (float) std::min(g_option_rate_in, g_option_rate_out);

	// print header
	std::cout << "Freq (Hz)   Gain (dB)   Error (dB)" << std::endl;

	uint32_t freqs = 480;
	double average_error = 0.0;
	uint32_t average_error_count = 0;
	for(uint32_t f = 0; f < freqs; ++f) {

		uint32_t samples_in = 10000;
		double test_freq = 0.5 * (double) g_option_rate_in * ((double) f + 0.5) / (float) freqs;

		// generate input
		std::vector<float> data_in(samples_in);
		for(uint32_t i = 0; i < samples_in; ++i) {
			data_in[i] = std::cos(2.0 * M_PI * test_freq * (double) i / (double) g_option_rate_in);
		}

		// resample the data in blocks
		std::vector<float> data_out;
		uint32_t pos_in = 0, pos_out = 0;
		resampler.reset();
		while(pos_in <= samples_in - resampler.get_filter_length()) {

			uint32_t block_in = std::min(samples_in - pos_in, 1234 + resampler.get_filter_length());
			uint32_t block_out = resampler.calculate_size_out(block_in);

			data_out.resize(pos_out + block_out);

			float *ptr_in[1] = {data_in.data() + pos_in};
			float *ptr_out[1] = {data_out.data() + pos_out};
			auto p = resampler.resample(1, ptr_in, block_in, ptr_out, block_out);
			assert(p.first > block_in - resampler.get_filter_length());
			assert(p.second == block_out);

			pos_in += p.first;
			pos_out += p.second;

		}
		uint32_t samples_out = pos_out;

		// analyze output
		double dot_sin_data = 0.0, dot_cos_data = 0.0, dot_sin_cos = 0.0;
		double norm_sin = 0.0, norm_cos = 0.0;
		for(uint32_t i = 0; i < samples_out; ++i) {
			double vec_sin = std::sin(2.0 * M_PI * test_freq * (double) i / actual_rate_out);
			double vec_cos = std::cos(2.0 * M_PI * test_freq * (double) i / actual_rate_out);
			dot_sin_data += vec_sin * (double) data_out[i];
			dot_cos_data += vec_cos * (double) data_out[i];
			dot_sin_cos += vec_sin * vec_cos;
			norm_sin += sqr(vec_sin);
			norm_cos += sqr(vec_cos);
		}
		double det = norm_sin * norm_cos - sqr(dot_sin_cos);
		double ampl_sin = (norm_cos * dot_sin_data - dot_sin_cos * dot_cos_data) / det;
		double ampl_cos = (norm_sin * dot_cos_data - dot_sin_cos * dot_sin_data) / det;
		double gain = sqr(ampl_sin) + sqr(ampl_cos);
		double error = 0.0;
		for(uint32_t i = 0; i < samples_out; ++i) {
			double vec_sin = std::sin(2.0 * M_PI * test_freq * (double) i / actual_rate_out);
			double vec_cos = std::cos(2.0 * M_PI * test_freq * (double) i / actual_rate_out);
			error += sqr(ampl_sin * vec_sin + ampl_cos * vec_cos - (double) data_out[i]);
		}
		error /= (double) samples_out;
		if(test_freq <= passband) {
			average_error += error;
			++average_error_count;
		}

		/*double error2 = 0.0;
		for(uint32_t i = 0; i < samples_out; ++i) {
			double expected = std::cos(2.0 * M_PI * test_freq * ((double) i + actual_latency) / actual_rate_out);
			error2 += sqr(expected - data_out[i]);
		}
		error2 /= (double) samples_out;*/

		// print data
		std::ios_base::fmtflags flags(std::cout.flags());
		std::cout << std::fixed << std::setw(9) << std::setprecision(2) << test_freq;
		std::cout << std::fixed << std::setw(12) << std::setprecision(2) << (10.0 * std::log10(gain));
		std::cout << std::fixed << std::setw(13) << std::setprecision(2) << (10.0 * std::log10(2.0 * error));
		/*std::cout << std::fixed << std::setw(13) << std::setprecision(2) << (10.0 * std::log10(2.0 * error2));
		std::cout << std::fixed << std::setw(13) << std::setprecision(2) << dot_sin_cos;*/
		std::cout << std::endl;
		std::cout.flags(flags);

	}
	average_error /= (double) average_error_count;

	double average_snr = 0.5 * sqr(g_option_resampler_gain) / average_error;
	double average_latency = ((double) (resampler.get_filter_length() / 2) - 0.5) / (double) g_option_rate_in;

	std::cout << std::endl;
	std::cout << "Input Rate:      " << std::fixed << std::setw(14) << std::setprecision(2) << g_option_rate_in << " Hz" << std::endl;
	std::cout << "Output Rate:     " << std::fixed << std::setw(14) << std::setprecision(2) << g_option_rate_out << " Hz" << std::endl;
	std::cout << "Passband:        " << std::fixed << std::setw(14) << std::setprecision(2) << passband << " Hz" << std::endl;
	std::cout << "Stopband:        " << std::fixed << std::setw(14) << std::setprecision(2) << stopband << " Hz" << std::endl;
	std::cout << "Beta:            " << std::fixed << std::setw(14) << std::setprecision(4) << g_option_resampler_beta << std::endl;
	std::cout << "Gain:            " << std::fixed << std::setw(14) << std::setprecision(2) << (20.0f * std::log10(g_option_resampler_gain)) << " dB" << std::endl;
	std::cout << "Filter Length:   " << std::setw(14) << resampler.get_filter_length() << std::endl;
	std::cout << "Filter Rows:     " << std::setw(14) << resampler.get_filter_rows() << std::endl;
	std::cout << "Average SNR:     " << std::fixed << std::setw(14) << std::setprecision(2) << (10.0f * std::log10(average_snr)) << " dB" << std::endl;
	std::cout << "Average latency: " << std::fixed << std::setw(14) << std::setprecision(2) << (average_latency * 1e3) << " ms" << std::endl;

}
