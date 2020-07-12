#pragma once

#include <cstdint>

#include <utility>

/*
This is a simple variable-rate resampler based on a polyphase filter bank with linear interpolation.
It uses a sinc filter windowed with a Kaiser window. The algorithm is described in more detail here:
https://ccrma.stanford.edu/~jos/resample/resample.html

- The resampling ratio is defined as the input rate divided by the output rate.
- The passband and stopband frequencies are specified relative to the lowest sample rate.
  The 6dB point of the filter is located exactly in the center of the transition band.
- The beta parameter controls the stopband attenuation of the filter.
- The gain parameter can be used to rescale the input data, which can be useful to avoid clipping due to ringing.

The stopband attenuation can be estimated using the following empirical formulas:
	min attenuation = (beta * 8.7 + 6) dB
	avg attenuation = (beta * 8.7 + 24) dB
In practice, beta values above 14 are usually pointless because regular single-precision floating point rounding errors
become the dominant source of errors at that point.

Some sensible default values:
- high quality:   passband=0.45, stopband=0.50, beta=10.0 => latency ~ 68 samples
- medium quality: passband=0.42, stopband=0.50, beta=8.0  => latency ~ 36 samples
- low quality:    passband=0.40, stopband=0.54, beta=7.0  => latency ~ 18 samples

The latency function can be used to accurately convert timestamps. The correct formula for this is:
	output_timestamp = input_timestamp - buffered_samples + get_latency_in()

In order to minimize unneccesary copying, the resampler does not do any buffering. When data is processed in blocks,
the user of this class must be able to store at least one filter length of input data for the next invocation.
A possible implementation might look like this:

	uint32_t buffer_size = resampler.get_filter_length(), block_size = 1024;
	std::vector<float> data_in(buffer_size + block_size);
	std::vector<float> data_out((size_t) std::ceil((float) block_size / resampler.get_ratio()) + 1);
	uint32_t pos = buffer_size;
	while(...) {
		read_data(data_in.data() + buffer_size, block_size);
		if(pos < data_in.size()) {
			float *ptr_in[1] = {data_in.data() + pos}, *ptr_out[1] = {data_out.data()};
			auto p = resampler.resample(1, ptr_in, data_in.size() - pos, ptr_out, data_out.size());
			write_data(data_out.data(), p.second)
			pos += p.first;
		}
		std::copy(data_in.data() + block_size, data_in.data() + block_size + buffer_size, data_in.data());
		pos -= block_size;
	}

In this implementation, the number of buffered samples is equal to buffer_size - pos. This value can be negative.
*/

class lowrider_resampler {

private:
	uint64_t m_ratio;
	uint32_t m_offset;
	uint32_t m_filter_length, m_filter_rows;
	float *m_filter_bank;

private:
	static constexpr uint64_t RATIO_ONE = (uint64_t) 1 << 32;

public:
	// Lower and upper bounds for parameters.
	static constexpr float RATIO_MIN = 1.0e-3f;
	static constexpr float RATIO_MAX = 1.0e+3f;
	static constexpr float PASSBAND_MIN = 0.001f;
	static constexpr float PASSBAND_MAX = 0.499f;
	static constexpr float STOPBAND_MIN = 0.500f;
	static constexpr float STOPBAND_MAX = 0.999f;
	static constexpr float BETA_MIN = 1.0f;
	static constexpr float BETA_MAX = 20.0f;

public:
	// Initializes the resampler and generates a filter bank based on the provided filter parameters.
	// The parameters must be within the bounds defined above.
	lowrider_resampler(float ratio, float passband, float stopband, float beta, float gain);

	// Frees the memory used by the filter bank and destroys the resampler.
	~lowrider_resampler();

	// Resets the state of the resampler, while reusing the existing filter bank.
	void reset();

	// Resamples as much data as possible without exceeding the provided input or output size.
	// Returns the number of processed input and output samples.
	// Note that this function generally won't process all input samples, since it needs at least one filter length of
	// input in order to produce any output at all. Input samples which have not been processed yet should be kept for
	// the next invocation. Also note that extreme ratio changes can cause the reported number of processed input
	// samples to exceed the number of input samples that were actually provided. In this case, the corresponding
	// number of input samples should be skipped on the next invocation.
	std::pair<uint32_t, uint32_t> resample(uint32_t channels, const float * const *data_in, uint32_t size_in,
										   float * const *data_out, uint32_t size_out);

	// Calculates the required input size to produce the requested number of output samples.
	uint32_t calculate_size_in(uint32_t size_out);

	// Calculates the required output size to process the provided number of input samples.
	uint32_t calculate_size_out(uint32_t size_in);

	// Returns the current resampler latency expressed in input samples.
	float get_latency_in();

	// Returns the current resampler latency expressed in output samples.
	float get_latency_out();

	// Returns the current resampling ratio (rate_in/rate_out).
	float get_ratio();

	// Changes the resampling ratio. The filter bank is not regenerated, so large changes are not recommended.
	void set_ratio(float ratio);

	// Returns the filter length (in input samples).
	uint32_t get_filter_length();

	// Returns the number of filter rows in the filter bank.
	uint32_t get_filter_rows();

};
