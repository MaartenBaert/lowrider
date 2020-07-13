#include "resampler.h"

#include "miscmath.h"

#include <cassert>
#include <cmath>
#include <cstdlib>

#include <algorithm>
#include <new>

inline float sinc(float x) {
	return (std::abs(x) < 1.0e-4f)? 1.0f : std::sin(x * (float) M_PI) / (x * (float) M_PI);
}

inline double sinc(double x) {
	return (std::abs(x) < 1.0e-9)? 1.0 : std::sin(x * (double) M_PI) / (x * (double) M_PI);
}

inline float bessel_i0(float x) {
	assert(x >= 0.0f);
	if(x < 4.0f) {
		float p = 0.0f, t = x / 4.0f;
		p = p * t + 2.976967e-02f;
		p = p * t - 8.944015e-02f;
		p = p * t + 2.472905e-01f;
		p = p * t - 2.102603e-01f;
		p = p * t + 6.092336e-01f;
		p = p * t - 8.754104e-02f;
		p = p * t + 1.809474e+00f;
		p = p * t - 7.709405e-03f;
		p = p * t + 4.001216e+00f;
		p = p * t - 1.164405e-04f;
		p = p * t + 4.000006e+00f;
		p = p * t - 1.359807e-07f;
		p = p * t + 1.000000e+00f;
		return p;
	} else {
		float p = 0.0f, t = 4.0f / x;
		p = p * t + 9.256256e-03f;
		p = p * t - 6.370068e-02f;
		p = p * t + 1.894629e-01f;
		p = p * t - 3.180811e-01f;
		p = p * t + 3.316216e-01f;
		p = p * t - 2.245450e-01f;
		p = p * t + 1.012653e-01f;
		p = p * t - 3.044211e-02f;
		p = p * t + 6.270247e-03f;
		p = p * t - 3.237180e-04f;
		p = p * t + 1.813388e-03f;
		p = p * t + 1.246446e-02f;
		p = p * t + 3.989423e-01f;
		return p * std::exp(x) / std::sqrt(x);
	}
}

inline double bessel_i0(double x) {
	if(x < 5.0) {
		double p = 0.0, t = x / 5.0 * 2.0 - 1.0;
		p = p * t + 7.74576540800919304e-11;
		p = p * t + 3.43260953673822710e-10;
		p = p * t + 2.29943069093013127e-09;
		p = p * t + 1.75512397221419985e-08;
		p = p * t + 1.25474839385967770e-07;
		p = p * t + 8.11843452726015733e-07;
		p = p * t + 5.09192335749760076e-06;
		p = p * t + 2.89363192499752772e-05;
		p = p * t + 1.58012403747176931e-04;
		p = p * t + 7.72712220051228809e-04;
		p = p * t + 3.59015934014472781e-03;
		p = p * t + 1.47067692366668178e-02;
		p = p * t + 5.63355063502382486e-02;
		p = p * t + 1.85962516759611229e-01;
		p = p * t + 5.60335052556681257e-01;
		p = p * t + 1.40237721617911415e+00;
		p = p * t + 3.07482079878055803e+00;
		p = p * t + 5.22429631812787676e+00;
		p = p * t + 7.13485201854592876e+00;
		p = p * t + 6.29179061322175492e+00;
		p = p * t + 3.28983914405012179e+00;
		return p;
	} else {
		double p = 0.0, t = 5.0 / x * 2.0 - 1.0;
		p = p * t - 7.86791617247035831e-09;
		p = p * t - 1.00530110761183567e-07;
		p = p * t + 2.00992037029333895e-07;
		p = p * t + 4.34160662975923923e-07;
		p = p * t - 9.06281731216786455e-07;
		p = p * t - 8.04515875527666453e-07;
		p = p * t + 2.08812613689774853e-06;
		p = p * t + 8.33820323156281170e-07;
		p = p * t - 3.28778426349098506e-06;
		p = p * t - 6.36789761595481956e-07;
		p = p * t + 4.30979371981555338e-06;
		p = p * t + 1.16177045793733166e-06;
		p = p * t - 5.16510588300618181e-06;
		p = p * t - 4.53780572965321393e-06;
		p = p * t + 2.35346418052302895e-06;
		p = p * t + 1.00552431959749950e-05;
		p = p * t + 2.13388450957766881e-05;
		p = p * t + 6.71944990037508168e-05;
		p = p * t + 4.10576613336662065e-04;
		p = p * t + 5.66012665634273420e-03;
		p = p * t + 4.04244506336700837e-01;
		return p * std::exp(x) / std::sqrt(x);
	}
}

inline float kaiser(float x, float beta) {
	return bessel_i0(beta * std::sqrt(std::max(0.0f, 1.0f - sqr(x)))) / bessel_i0(beta);
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
