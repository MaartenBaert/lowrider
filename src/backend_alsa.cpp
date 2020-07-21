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

#include "backend_alsa.h"

#include "aligned_memory.h"
#include "miscmath.h"
#include "string_helper.h"

#include <cassert>
#include <cerrno>

#include <algorithm>
#include <iostream>
#include <new>
#include <stdexcept>

#include <alsa/asoundlib.h>

struct lowrider_backend_alsa::Private {

	struct InputOutput {

		snd_pcm_t *m_pcm;
		snd_pcm_format_t m_sample_format;
		unsigned int m_channels, m_sample_rate;
		snd_pcm_uframes_t m_period_size, m_buffer_size;
		lowrider_aligned_memory<uint8_t> m_temp_data;
		bool m_running;

		InputOutput() {
			m_pcm = nullptr;
			m_sample_format = SND_PCM_FORMAT_UNKNOWN;
			m_channels = 0;
			m_sample_rate = 0;
			m_period_size = 0;
			m_buffer_size = 0;
			m_running = false;
		}

		void open(snd_pcm_stream_t direction, const std::string &name, lowrider_sample_format sample_format,
				  uint32_t channels, uint32_t sample_rate, uint32_t period_size, uint32_t buffer_size, bool wait) {
			assert(m_pcm == nullptr);

			snd_pcm_hw_params_t *hw_params = nullptr;
			snd_pcm_sw_params_t *sw_params = nullptr;

			try {

				// allocate parameter structures
				if(snd_pcm_hw_params_malloc(&hw_params) < 0) {
					throw std::bad_alloc();
				}
				if(snd_pcm_sw_params_malloc(&sw_params) < 0) {
					throw std::bad_alloc();
				}

				// open PCM device
				if(snd_pcm_open(&m_pcm, name.c_str(), direction, SND_PCM_NONBLOCK) < 0) {
					throw std::runtime_error(make_string("failed to open ALSA PCM '", name, "'"));
				}

				// get hardware parameters
				if(snd_pcm_hw_params_any(m_pcm, hw_params) < 0) {
					throw std::runtime_error(make_string("failed to get hardware parameters of ALSA PCM '", name, "'"));
				}

				// set access type
				if(snd_pcm_hw_params_set_access(m_pcm, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED) < 0) {
					throw std::runtime_error(make_string("failed to set access type of ALSA PCM '", name, "'"));
				}

				// set sample format
				switch(sample_format) {
					case lowrider_sample_format_any: {
						if(snd_pcm_hw_params_test_format(m_pcm, hw_params, SND_PCM_FORMAT_FLOAT) == 0) {
							m_sample_format = SND_PCM_FORMAT_FLOAT;
						} else if(snd_pcm_hw_params_test_format(m_pcm, hw_params, SND_PCM_FORMAT_S32) == 0) {
							m_sample_format = SND_PCM_FORMAT_S32;
						} else if(snd_pcm_hw_params_test_format(m_pcm, hw_params, SND_PCM_FORMAT_S24) == 0) {
							m_sample_format = SND_PCM_FORMAT_S24;
						} else if(snd_pcm_hw_params_test_format(m_pcm, hw_params, SND_PCM_FORMAT_S16) == 0) {
							m_sample_format = SND_PCM_FORMAT_S16;
						} else {
							throw std::runtime_error(make_string("failed to find a supported sample format for ALSA PCM '", name, "'"));
						}
						break;
					}
					case lowrider_sample_format_f32: m_sample_format = SND_PCM_FORMAT_FLOAT; break;
					case lowrider_sample_format_s32: m_sample_format = SND_PCM_FORMAT_S32; break;
					case lowrider_sample_format_s24: m_sample_format = SND_PCM_FORMAT_S24; break;
					case lowrider_sample_format_s16: m_sample_format = SND_PCM_FORMAT_S16; break;
				}
				if(snd_pcm_hw_params_set_format(m_pcm, hw_params, m_sample_format) < 0) {
					throw std::runtime_error(make_string("failed to set sample format of ALSA PCM '", name, "'"));
				}

				// set channel count
				m_channels = channels;
				if(snd_pcm_hw_params_set_channels_near(m_pcm, hw_params, &m_channels) < 0) {
					throw std::runtime_error(make_string("failed to set channel count of ALSA PCM '", name, "'"));
				}

				// disable resampling
				if(snd_pcm_hw_params_set_rate_resample(m_pcm, hw_params, 0) < 0) {
					throw std::runtime_error(make_string("failed to disable resampling of ALSA PCM '", name, "'"));
				}

				// set sample rate
				m_sample_rate = sample_rate;
				if(snd_pcm_hw_params_set_rate_near(m_pcm, hw_params, &m_sample_rate, nullptr) < 0) {
					throw std::runtime_error(make_string("failed to set sample rate of ALSA PCM '", name, "'"));
				}

				// set period size
				m_period_size = period_size;
				if(snd_pcm_hw_params_set_period_size_near(m_pcm, hw_params, &m_period_size, nullptr) < 0) {
					throw std::runtime_error(make_string("failed to set period size of ALSA PCM '", name, "'"));
				}

				// set buffer size
				m_buffer_size = buffer_size;
				if(snd_pcm_hw_params_set_buffer_size_near(m_pcm, hw_params, &m_buffer_size) < 0) {
					throw std::runtime_error(make_string("failed to set buffer size of ALSA PCM '", name, "'"));
				}

				// apply hardware parameters
				if(snd_pcm_hw_params(m_pcm, hw_params) < 0) {
					throw std::runtime_error(make_string("failed to apply hardware parameters of ALSA PCM '", name, "'"));
				}

				// get software parameters
				if(snd_pcm_sw_params_current(m_pcm, sw_params) < 0) {
					throw std::runtime_error(make_string("failed to get software parameters of ALSA PCM '", name, "'"));
				}

				// get boundary
				snd_pcm_uframes_t boundary;
				if(snd_pcm_sw_params_get_boundary(sw_params, &boundary) < 0) {
					throw std::runtime_error(make_string("failed to get boundary of ALSA PCM '", name, "'"));
				}

				// set start threshold
				if(snd_pcm_sw_params_set_start_threshold(m_pcm, sw_params, boundary) < 0) {
					throw std::runtime_error(make_string("failed to set start threshold of ALSA PCM '", name, "'"));
				}

				// set stop threshold
				if(snd_pcm_sw_params_set_stop_threshold(m_pcm, sw_params, boundary) < 0) {
					throw std::runtime_error(make_string("failed to set stop threshold of ALSA PCM '", name, "'"));
				}

				// set minimum available frames
				if(snd_pcm_sw_params_set_avail_min(m_pcm, sw_params, 1) < 0) {
					throw std::runtime_error(make_string("failed to set minimum available frames of ALSA PCM '", name, "'"));
				}

				// set period event
				if(snd_pcm_sw_params_set_period_event(m_pcm, sw_params, (wait)? 1 : 0) < 0) {
					throw std::runtime_error(make_string("failed to set period event of ALSA PCM '", name, "'"));
				}

				// set silence threshold
				if(snd_pcm_sw_params_set_silence_threshold(m_pcm, sw_params, 0) < 0) {
					throw std::runtime_error(make_string("failed to set silence threshold of ALSA PCM '", name, "'"));
				}

				// set silence size
				if(snd_pcm_sw_params_set_silence_size(m_pcm, sw_params, boundary) < 0) {
					throw std::runtime_error(make_string("failed to set silence size of ALSA PCM '", name, "'"));
				}

				// apply software parameters
				if(snd_pcm_sw_params(m_pcm, sw_params) < 0) {
					throw std::runtime_error(make_string("failed to apply software parameters of ALSA PCM '", name, "'"));
				}

				// prepare the PCM
				if(snd_pcm_prepare(m_pcm) < 0) {
					throw std::runtime_error(make_string("failed to prepare ALSA PCM '", name, "'"));
				}

				// allocate temp data
				switch(m_sample_format) {
					case SND_PCM_FORMAT_FLOAT: {
						m_temp_data.allocate(16, (m_channels * m_buffer_size * sizeof(float) + 15) / 16 * 16);
						break;
					}
					case SND_PCM_FORMAT_S32: {
						m_temp_data.allocate(16, (m_channels * m_buffer_size * sizeof(int32_t) + 15) / 16 * 16);
						break;
					}
					case SND_PCM_FORMAT_S24: {
						m_temp_data.allocate(16, (m_channels * m_buffer_size * sizeof(int32_t) + 15) / 16 * 16);
						break;
					}
					case SND_PCM_FORMAT_S16: {
						m_temp_data.allocate(16, (m_channels * m_buffer_size * sizeof(int16_t) + 15) / 16 * 16);
						break;
					}
					default: assert(false);
				}

				std::cerr << "Info: ALSA PCM '" << name << "'";
				std::cerr << " direction=" << snd_pcm_stream_name(direction);
				std::cerr << " format=" << snd_pcm_format_name(m_sample_format);
				std::cerr << " channels=" << m_channels;
				std::cerr << " rate=" << m_sample_rate;
				std::cerr << " period=" << m_period_size;
				std::cerr << " buffer=" << m_buffer_size;
				std::cerr << std::endl;

				// free parameter structures
				snd_pcm_sw_params_free(sw_params);
				sw_params = nullptr;
				snd_pcm_hw_params_free(hw_params);
				hw_params = nullptr;

			} catch(...) {
				if(m_pcm != nullptr) {
					snd_pcm_close(m_pcm);
					m_pcm = nullptr;
				}
				if(sw_params != nullptr) {
					snd_pcm_sw_params_free(sw_params);
					sw_params = nullptr;
				}
				if(hw_params != nullptr) {
					snd_pcm_hw_params_free(hw_params);
					hw_params = nullptr;
				}
				throw;
			}

		}

		void close() {
			if(m_pcm != nullptr) {
				snd_pcm_close(m_pcm);
				m_pcm = nullptr;
			}
			m_running = false;
		}

		void input_start() {
			if(snd_pcm_start(m_pcm) < 0) {
				throw std::runtime_error("failed to start ALSA input");
			}
			m_running = true;
			std::cerr << "Info: input PCM started" << std::endl;
		}

		void output_start() {
			if(snd_pcm_start(m_pcm) < 0) {
				throw std::runtime_error("failed to start ALSA output");
			}
			m_running = true;
			std::cerr << "Info: output PCM started" << std::endl;
		}

		void input_recover() {
			assert(m_pcm != nullptr);
			m_running = false;
			std::cerr << "Warning: overrun in ALSA input" << std::endl;
			if(snd_pcm_prepare(m_pcm) < 0) {
				throw std::runtime_error("failed to recover ALSA input after overrun");
			}
		}

		void output_recover() {
			assert(m_pcm != nullptr);
			m_running = false;
			std::cerr << "Warning: underrun in ALSA output" << std::endl;
			if(snd_pcm_prepare(m_pcm) < 0) {
				throw std::runtime_error("failed to recover ALSA output after underrun");
			}
		}

		bool input_wait(uint32_t timeout) {
			assert(m_pcm != nullptr);
			int wait = snd_pcm_wait(m_pcm, timeout);
			if(wait < 0) {
				if(wait == -EPIPE) {
					input_recover();
					return false;
				} else {
					throw std::runtime_error("failed to wait on ALSA input");
				}
			}
			return (wait != 0);
		}

		uint32_t input_read(float * const *data, uint32_t size) {
			assert(m_pcm != nullptr);

			// limit read size
			if(size > m_buffer_size) {
				size = m_buffer_size;
			}

			// read the samples
			snd_pcm_sframes_t samples_read = snd_pcm_readi(m_pcm, m_temp_data.data(), size);
			if(samples_read < 0) {
				if(samples_read == -EPIPE) {
					input_recover();
					return 0;
				} else if(samples_read == -EAGAIN) {
					return 0;
				} else {
					throw std::runtime_error("failed to read from ALSA input");
				}
			} else if(samples_read == 0) {
				return 0;
			}

			// convert the samples
			if(data != nullptr) {
				switch(m_sample_format) {
					case SND_PCM_FORMAT_FLOAT: {
						float *temp = (float*) m_temp_data.data();
						for(uint32_t i = 0; i < (uint32_t) samples_read; ++i) {
							for(uint32_t c = 0; c < m_channels; ++c) {
								data[c][i] = *(temp++);
							}
						}
						break;
					}
					case SND_PCM_FORMAT_S32: {
						int32_t *temp = (int32_t*) m_temp_data.data();
						for(uint32_t i = 0; i < (uint32_t) samples_read; ++i) {
							for(uint32_t c = 0; c < m_channels; ++c) {
								data[c][i] = (float) *(temp++) * (float) (1.0 / 2147483648.0);
							}
						}
						break;
					}
					case SND_PCM_FORMAT_S24: {
						int32_t *temp = (int32_t*) m_temp_data.data();
						for(uint32_t i = 0; i < (uint32_t) samples_read; ++i) {
							for(uint32_t c = 0; c < m_channels; ++c) {
								data[c][i] = (float) *(temp++) * (float) (1.0 / 8388608.0);
							}
						}
						break;
					}
					case SND_PCM_FORMAT_S16: {
						int16_t *temp = (int16_t*) m_temp_data.data();
						for(uint32_t i = 0; i < (uint32_t) samples_read; ++i) {
							for(uint32_t c = 0; c < m_channels; ++c) {
								data[c][i] = (float) *(temp++) * (float) (1.0 / 32768.0);
							}
						}
						break;
					}
					default: assert(false);
				}
			}

			return (uint32_t) samples_read;
		}

		uint32_t output_write(const float * const *data, uint32_t size) {
			assert(m_pcm != nullptr);

			// limit write size
			if(size > m_buffer_size) {
				size = m_buffer_size;
			}

			// convert the samples
			if(data == nullptr) {
				switch(m_sample_format) {
					case SND_PCM_FORMAT_FLOAT: {
						std::fill_n((float*) m_temp_data.data(), m_channels * size, 0.0f);
						break;
					}
					case SND_PCM_FORMAT_S32:
					case SND_PCM_FORMAT_S24: {
						std::fill_n((int32_t*) m_temp_data.data(), m_channels * size, 0);
						break;
					}
					case SND_PCM_FORMAT_S16: {
						std::fill_n((int16_t*) m_temp_data.data(), m_channels * size, 0);
						break;
					}
					default: assert(false);
				}
			} else {
				switch(m_sample_format) {
					case SND_PCM_FORMAT_FLOAT: {
						float *temp = (float*) m_temp_data.data();
						for(uint32_t i = 0; i < (uint32_t) size; ++i) {
							for(uint32_t c = 0; c < m_channels; ++c) {
								*(temp++) = data[c][i];
							}
						}
						break;
					}
					case SND_PCM_FORMAT_S32: {
						int32_t *temp = (int32_t*) m_temp_data.data();
						for(uint32_t i = 0; i < (uint32_t) size; ++i) {
							for(uint32_t c = 0; c < m_channels; ++c) {
								*(temp++) = (int32_t) rint32(clamp(data[c][i] * 2147483648.0f, -2147483648.0f, 2147483647.0f));
							}
						}
						break;
					}
					case SND_PCM_FORMAT_S24: {
						int32_t *temp = (int32_t*) m_temp_data.data();
						for(uint32_t i = 0; i < (uint32_t) size; ++i) {
							for(uint32_t c = 0; c < m_channels; ++c) {
								*(temp++) = (int32_t) rint32(clamp(data[c][i] * 8388608.0f, -8388608.0f, 8388607.0f));
							}
						}
						break;
					}
					case SND_PCM_FORMAT_S16: {
						int16_t *temp = (int16_t*) m_temp_data.data();
						for(uint32_t i = 0; i < (uint32_t) size; ++i) {
							for(uint32_t c = 0; c < m_channels; ++c) {
								*(temp++) = (int16_t) rint32(clamp(data[c][i] * 32768.0f, -32768.0f, 32767.0f));
							}
						}
						break;
					}
					default: assert(false);
				}
			}

			// write the samples
			snd_pcm_sframes_t samples_written = snd_pcm_writei(m_pcm, m_temp_data.data(), size);
			if(samples_written < 0) {
				if(samples_written == -EPIPE) {
					output_recover();
					return 0;
				} else if(samples_written == -EAGAIN) {
					return 0;
				} else {
					throw std::runtime_error("failed to write to ALSA output");
				}
			}

			return samples_written;

		}

		uint32_t input_avail() {
			assert(m_pcm != nullptr);
			snd_pcm_sframes_t avail = snd_pcm_avail(m_pcm);
			if(avail < 0) {
				if(avail == -EPIPE) {
					input_recover();
					return 0;
				} else {
					throw std::runtime_error("failed to get available samples of ALSA input");
				}
			}
			return (uint32_t) avail;
		}

		uint32_t output_avail() {
			assert(m_pcm != nullptr);
			snd_pcm_sframes_t avail = snd_pcm_avail(m_pcm);
			if(avail < 0) {
				if(avail == -EPIPE) {
					output_recover();
					return 0;
				} else {
					throw std::runtime_error("failed to get available samples of ALSA output");
				}
			}
			return (uint32_t) avail;
		}

		lowrider_sample_format get_sample_format() {
			switch(m_sample_format) {
				case SND_PCM_FORMAT_FLOAT: return lowrider_sample_format_f32;
				case SND_PCM_FORMAT_S32: return lowrider_sample_format_s32;
				case SND_PCM_FORMAT_S24: return lowrider_sample_format_s24;
				case SND_PCM_FORMAT_S16: return lowrider_sample_format_s16;
				default: assert(false);
			}
			return lowrider_sample_format_any;
		}

	};

	InputOutput m_input, m_output;

};

lowrider_backend_alsa::lowrider_backend_alsa() {
	m_private = new Private;
}

lowrider_backend_alsa::~lowrider_backend_alsa() {
	output_close();
	input_close();
	delete m_private;
}

void lowrider_backend_alsa::input_open(const std::string &name, lowrider_sample_format sample_format, uint32_t channels,
									   uint32_t sample_rate, uint32_t period_size, uint32_t buffer_size, bool wait) {
	m_private->m_input.open(SND_PCM_STREAM_CAPTURE, name, sample_format, channels, sample_rate, period_size, buffer_size, wait);
}

void lowrider_backend_alsa::input_close() {
	m_private->m_input.close();
}

void lowrider_backend_alsa::input_start() {
	m_private->m_input.input_start();
}

bool lowrider_backend_alsa::input_running() {
	return m_private->m_input.m_running;
}

bool lowrider_backend_alsa::input_wait(uint32_t timeout) {
	return m_private->m_input.input_wait(timeout);
}

uint32_t lowrider_backend_alsa::input_read(float * const *data, uint32_t size) {
	return m_private->m_input.input_read(data, size);
}

lowrider_sample_format lowrider_backend_alsa::input_get_sample_format() {
	return m_private->m_input.get_sample_format();
}

uint32_t lowrider_backend_alsa::input_get_channels() {
	return m_private->m_input.m_channels;
}

uint32_t lowrider_backend_alsa::input_get_sample_rate() {
	return m_private->m_input.m_sample_rate;
}

uint32_t lowrider_backend_alsa::input_get_period_size() {
	return m_private->m_input.m_period_size;
}

uint32_t lowrider_backend_alsa::input_get_buffer_size() {
	return m_private->m_input.m_buffer_size;
}

uint32_t lowrider_backend_alsa::input_get_buffer_used() {
	return std::min(m_private->m_input.input_avail(), (uint32_t) m_private->m_input.m_buffer_size);
}

uint32_t lowrider_backend_alsa::input_get_buffer_free() {
	uint32_t avail = std::min(m_private->m_input.input_avail(), (uint32_t) m_private->m_input.m_buffer_size);
	return m_private->m_input.m_buffer_size - avail;
}

void lowrider_backend_alsa::output_open(const std::string &name, lowrider_sample_format sample_format, uint32_t channels,
										uint32_t sample_rate, uint32_t period_size, uint32_t buffer_size, bool wait) {
	m_private->m_output.open(SND_PCM_STREAM_PLAYBACK, name, sample_format, channels, sample_rate, period_size, buffer_size, wait);
}

void lowrider_backend_alsa::output_close() {
	m_private->m_output.close();
}

void lowrider_backend_alsa::output_start() {
	m_private->m_output.output_start();
}

bool lowrider_backend_alsa::output_running() {
	return m_private->m_output.m_running;
}

uint32_t lowrider_backend_alsa::output_write(const float * const *data, uint32_t size) {
	return m_private->m_output.output_write(data, size);
}

lowrider_sample_format lowrider_backend_alsa::output_get_sample_format() {
	return m_private->m_output.get_sample_format();
}

uint32_t lowrider_backend_alsa::output_get_channels() {
	return m_private->m_output.m_channels;
}

uint32_t lowrider_backend_alsa::output_get_sample_rate() {
	return m_private->m_output.m_sample_rate;
}

uint32_t lowrider_backend_alsa::output_get_period_size() {
	return m_private->m_output.m_period_size;
}

uint32_t lowrider_backend_alsa::output_get_buffer_size() {
	return m_private->m_output.m_buffer_size;
}

uint32_t lowrider_backend_alsa::output_get_buffer_used() {
	uint32_t avail = std::min(m_private->m_output.output_avail(), (uint32_t) m_private->m_output.m_buffer_size);
	return m_private->m_output.m_buffer_size - avail;
}

uint32_t lowrider_backend_alsa::output_get_buffer_free() {
	return std::min(m_private->m_output.output_avail(), (uint32_t) m_private->m_output.m_buffer_size);
}
