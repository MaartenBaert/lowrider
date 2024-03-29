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

#include "resampler.h"

#include "bessel.h"
#include "miscmath.h"

#include <cassert>
#include <cmath>

#include <algorithm>

inline double sinc(double x) {
	return (std::abs(x) < 1.0e-9)? 1.0 : std::sin(x * (double) M_PI) / (x * (double) M_PI);
}

inline double kaiser(double x, double beta) {
	return bessel_i0(beta * std::sqrt(std::max(0.0, 1.0 - sqr(x)))) / bessel_i0(beta);
}

static void firfilter(uint32_t channels, uint32_t filter_length,
					  const float *coef1, const float *coef2, float frac,
					  const float * const *data_in, uint32_t pos_in, float * const *data_out, uint32_t pos_out) {
	assert(filter_length % 4 == 0);
	for(uint32_t c = 0; c < channels; ++c) {
		const float *data = data_in[c] + pos_in;
		float sum = 0.0f;
		for(uint32_t i = 0; i < filter_length / 4 * 4; ++i) {
			float coef = coef1[i] + (coef2[i] - coef1[i]) * frac;
			sum += data[i] * coef;
		}
		data_out[c][pos_out] = sum;
	}
}

lowrider_resampler::lowrider_resampler(float ratio, float passband, float stopband, float beta, float gain) {
	assert(std::isfinite(ratio) && ratio >= RATIO_MIN && ratio <= RATIO_MAX);
	assert(std::isfinite(passband) && passband >= PASSBAND_MIN && passband <= PASSBAND_MAX);
	assert(std::isfinite(stopband) && stopband >= STOPBAND_MIN && stopband <= STOPBAND_MAX);
	assert(std::isfinite(beta) && beta >= BETA_MIN && beta <= BETA_MAX);

	m_ratio = rint64((float) RATIO_ONE * ratio);
	m_offset = 0;

	// calculate the filter bank size
	float sinc_lobes = std::max(2.0f, beta / ((float) M_PI * 0.5f * (stopband - passband)));
	float sinc_freq = (passband + stopband) / std::max(1.0f, ratio);
	float base_rows = clamp(3.0f * std::exp(0.5f * beta), 16.0f, 4096.0f);
	m_filter_length = (uint32_t) std::ceil(clamp(sinc_lobes / sinc_freq * 0.25f, 1.0f, 4096.0f)) * 4;
	m_filter_rows = (uint32_t) std::ceil(clamp(base_rows * sinc_freq, 1.0f, 16384.0f));

	// allocate filter bank
	m_filter_bank.allocate(4, (m_filter_rows + 1) * m_filter_length);

	// generate filters
	double window_scale = 1.0f / (double) (m_filter_length / 2);
	for(unsigned int j = 0; j <= m_filter_rows; ++j) {
		float *coef = m_filter_bank.data() + j * m_filter_length;
		double shift = 1.0f - (double) j / (double) m_filter_rows - (double) (m_filter_length / 2);
		for(unsigned int i = 0; i < m_filter_length; ++i) {
			double x = (double) i + shift;
			coef[i] = kaiser(x * window_scale, (double) beta) * sinc(x * (double) sinc_freq) * (double) sinc_freq * (double) gain;
		}
	}

}

void lowrider_resampler::reset() {
	m_offset = 0;
}

std::pair<uint32_t, uint32_t> lowrider_resampler::resample(uint32_t channels, const float * const *data_in, uint32_t size_in,
												  float * const *data_out, uint32_t size_out) {
	float frac_scale = 1.0f / (float) RATIO_ONE;
	uint32_t pos_in = 0, pos_out = 0;
	while(pos_in + m_filter_length <= size_in && pos_out < size_out) {

		// select the required filter
		uint64_t sel = (uint64_t) m_offset * m_filter_rows;
		uint32_t row = (uint32_t) (sel >> 32);
		float *coef1 = m_filter_bank.data() + row * m_filter_length;
		float *coef2 = coef1 + m_filter_length;
		float frac = (float) (uint32_t) sel * frac_scale;

		// calculate the next sample
		firfilter(channels, m_filter_length, coef1, coef2, frac, data_in, pos_in, data_out, pos_out);

		// increase the position
		uint64_t new_offset = (uint64_t) m_offset + m_ratio;
		m_offset = (uint32_t) new_offset;
		pos_in += (uint32_t) (new_offset >> 32);
		++pos_out;

	}
	return std::make_pair(pos_in, pos_out);
}

uint32_t lowrider_resampler::calculate_size_in(uint32_t size_out) {
	return (uint32_t) (((uint64_t) m_offset + m_ratio * size_out) >> 32) + (m_filter_length - 1);
}

uint32_t lowrider_resampler::calculate_size_out(uint32_t size_in) {
	return (size_in < m_filter_length)? 0 : (((uint64_t) (size_in - (m_filter_length - 1)) << 32) - (uint64_t) m_offset - 1) / m_ratio + 1;
}

float lowrider_resampler::get_latency_in() {
	return (float) (m_filter_length / 2 - 1) + (float) m_offset / (float) RATIO_ONE;
}

float lowrider_resampler::get_latency_out() {
	return get_latency_in() * (float) RATIO_ONE / (float) m_ratio;
}

float lowrider_resampler::get_ratio() {
	return (float) m_ratio / (float) RATIO_ONE;
}

void lowrider_resampler::set_ratio(float ratio) {
	m_ratio = rint64((float) RATIO_ONE * ratio);
}

uint32_t lowrider_resampler::get_filter_length() {
	return m_filter_length;
}

uint32_t lowrider_resampler::get_filter_rows() {
	return m_filter_rows;
}
