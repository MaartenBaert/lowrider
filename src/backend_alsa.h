#pragma once

#include "sample_format.h"

#include <cstdint>

#include <string>

class lowrider_backend_alsa {

	struct Private;

private:
	Private *m_private;

public:
	lowrider_backend_alsa();
	~lowrider_backend_alsa();

	void input_open(const std::string &name, lowrider_sample_format sample_format, uint32_t channels, uint32_t sample_rate, uint32_t period_size, uint32_t buffer_size);
	void input_close();
	void input_start();
	bool input_running();

	// Waits until data is available or until timeout (in milliseconds). A negative value means no timeout.
	// Returns true if data is available, or false if a timeout occurred.
	bool input_wait(int32_t timeout);

	// Reads as much data as possible without waiting. If 'data' is nullptr, the data is discarded.
	// Returns the actual number of samples read.
	uint32_t input_read(float * const *data, uint32_t size);

	lowrider_sample_format input_get_sample_format();
	uint32_t input_get_channels();
	uint32_t input_get_sample_rate();
	uint32_t input_get_period_size();
	uint32_t input_get_buffer_size();
	uint32_t input_get_buffer_used();
	uint32_t input_get_buffer_free();

	void output_open(const std::string &name, lowrider_sample_format sample_format, uint32_t channels, uint32_t sample_rate, uint32_t period_size, uint32_t buffer_size);
	void output_close();
	void output_start();
	bool output_running();

	// Writes as much data as possible without waiting. If 'data' is nullptr, zeros are written.
	// Returns the actual number of samples written.
	uint32_t output_write(const float * const *data, uint32_t size);

	lowrider_sample_format output_get_sample_format();
	uint32_t output_get_channels();
	uint32_t output_get_sample_rate();
	uint32_t output_get_period_size();
	uint32_t output_get_buffer_size();
	uint32_t output_get_buffer_used();
	uint32_t output_get_buffer_free();

};
