#include "options.h"

#include "resampler.h"
#include "string_helper.h"

#include <cstddef>

#include <iostream>
#include <stdexcept>

bool g_option_help = false;
bool g_option_version = false;
bool g_option_analyze_resampler = false;
bool g_option_test_hardware = false;

std::string g_option_device_in;
std::string g_option_device_out;
lowrider_sample_format g_option_format_in = lowrider_sample_format_any;
lowrider_sample_format g_option_format_out = lowrider_sample_format_any;
uint32_t g_option_channels_in = 2;
uint32_t g_option_channels_out = 2;
uint32_t g_option_rate_in = 48000;
uint32_t g_option_rate_out = 48000;
uint32_t g_option_period_in = 256;
uint32_t g_option_period_out = 256;
uint32_t g_option_buffer_in = 1024;
uint32_t g_option_buffer_out = 1024;

uint32_t g_option_target_level = 128;
uint32_t g_option_timer_period = 620000;

float g_option_loop_bandwidth = 0.1f;
float g_option_max_drift = 0.002f;

float g_option_resampler_passband = 0.42f;
float g_option_resampler_stopband = 0.50f;
float g_option_resampler_beta = 8.0f;
float g_option_resampler_gain = 1.0f;

void print_help() {
	std::cout << "Usage: lowrider [OPTION]" << std::endl;
	std::cout << std::endl;
	std::cout << "Options:" << std::endl;
	std::cout << "  --help                      Show this help message." << std::endl;
	std::cout << "  --version                   Show version information." << std::endl;
	std::cout << "  --analyze-resampler         Analyze the frequency response and accuracy of the" << std::endl;
	std::cout << "                              resampler using the specified resampler parameters." << std::endl;
	std::cout << "  --device-in=NAME            Set the input device." << std::endl;
	std::cout << "  --device-out=NAME           Set the output device." << std::endl;
	std::cout << "  --format-in=FORMAT          Set the input sample format (default 'any')." << std::endl;
	std::cout << "  --format-out=FORMAT         Set the output sample format (default 'any')." << std::endl;
	std::cout << "  --channels-in=NUM           Set the number of input channels (default 2)." << std::endl;
	std::cout << "  --channels-out=NUM          Set the number of output channels (default 2)." << std::endl;
	std::cout << "  --rate-in=RATE              Set the input sample rate (default 48000 Hz)." << std::endl;
	std::cout << "  --rate-out=RATE             Set the output sample rate (default 48000 Hz)." << std::endl;
	std::cout << "  --period-in=SIZE            Set the input period size (default 256)." << std::endl;
	std::cout << "  --period-out=SIZE           Set the output period size (default 256)." << std::endl;
	std::cout << "  --buffer-in=SIZE            Set the input buffer size (default 1024)." << std::endl;
	std::cout << "  --buffer-out=SIZE           Set the output buffer size (default 1024)." << std::endl;
	std::cout << "  --target-level=LEVEL        Set the targeted buffer fill level (default 128)." << std::endl;
	std::cout << "  --timer-period=NANOSECONDS  Set the timer period (default 620000 ns)." << std::endl;
	std::cout << "  --loop-bandwidth=FREQUENCY  Set the bandwidth of the feedback loop (default 0.1 Hz)." << std::endl;
	std::cout << "  --max-drift=DRIFT           Set the maximum allowed clock drift (default 0.002)." << std::endl;
	std::cout << "  --resampler-passband=VALUE  Set the resampler passband parameter (default 0.42)." << std::endl;
	std::cout << "  --resampler-stopband=VALUE  Set the resampler stopband parameter (default 0.50)." << std::endl;
	std::cout << "  --resampler-beta=VALUE      Set the resampler beta parameter (default 8.0)." << std::endl;
	std::cout << "  --resampler-gain=VALUE      Set the resampler gain parameter (default 1.0)." << std::endl;
}

void print_version() {
	std::cout << "lowrider " << LOWRIDER_VERSION << std::endl;
}

static void parse_option_novalue(bool has_value, const std::string &option, bool &result) {
	if(has_value) {
		throw std::runtime_error(make_string("option '", option, "' does not accept a value"));
	}
	result = true;
}

template<typename T>
static void parse_option_value(bool has_value, const std::string &option, const std::string &value, T &result, T min, T max) {
	if(!has_value) {
		throw std::runtime_error(make_string("option '", option, "' requires a value"));
	}
	std::istringstream ss(value);
	ss >> result;
	if(ss.fail()) {
		throw std::runtime_error(make_string("invalid value '", value, "' for option '", option, "'"));
	}
	if(!(result >= min && result <= max)) {
		throw std::runtime_error(make_string("value for option '", option, "' must be between ", min, " and ", max));
	}
}

static void parse_option_value(bool has_value, const std::string &option, const std::string &value, std::string &result) {
	if(!has_value) {
		throw std::runtime_error(make_string("option '", option, "' requires a value"));
	}
	result = value;
}

static void parse_option_format(bool has_value, const std::string &option, const std::string &value, lowrider_sample_format &result) {
	if(!has_value) {
		throw std::runtime_error(make_string("option '", option, "' requires a value"));
	}
	std::string lower = to_lower(value);
	if(lower == "any") {
		result = lowrider_sample_format_any;
	} else if(lower == "f32") {
		result = lowrider_sample_format_f32;
	} else if(lower == "s32") {
		result = lowrider_sample_format_s32;
	} else if(lower == "s24") {
		result = lowrider_sample_format_s24;
	} else if(lower == "s16") {
		result = lowrider_sample_format_s16;
	} else {
		throw std::runtime_error(make_string("unknown sample format '", value, "'"));
	}
}

void parse_options(int argc, char *argv[]) {

	// parse options
	for(int i = 1; i < argc; ++i) {

		// split into option and value
		std::string arg = argv[i];
		size_t p = arg.find('=');
		bool has_value;
		std::string option, value;
		if(p == std::string::npos) {
			has_value = false;
			option = arg;
		} else {
			has_value = true;
			option = arg.substr(0, p);
			value = arg.substr(p + 1);
		}

		// handle options
		if(option == "--help") {
			parse_option_novalue(has_value, option, g_option_help);
		} else if(option == "--version") {
			parse_option_novalue(has_value, option, g_option_version);
		} else if(option == "--analyze-resampler") {
			parse_option_novalue(has_value, option, g_option_analyze_resampler);
		} else if(option == "--test-hardware") {
			parse_option_novalue(has_value, option, g_option_test_hardware);
		} else if(option == "--device-in") {
			parse_option_value(has_value, option, value, g_option_device_in);
		} else if(option == "--device-out") {
			parse_option_value(has_value, option, value, g_option_device_out);
		} else if(option == "--format-in") {
			parse_option_format(has_value, option, value, g_option_format_in);
		} else if(option == "--format-out") {
			parse_option_format(has_value, option, value, g_option_format_out);
		} else if(option == "--channels-in") {
			parse_option_value(has_value, option, value, g_option_channels_in, (uint32_t) 1, (uint32_t) 100);
		} else if(option == "--channels-out") {
			parse_option_value(has_value, option, value, g_option_channels_out, (uint32_t) 1, (uint32_t) 100);
		} else if(option == "--rate-in") {
			parse_option_value(has_value, option, value, g_option_rate_in, (uint32_t) 1, (uint32_t) 1000000);
		} else if(option == "--rate-out") {
			parse_option_value(has_value, option, value, g_option_rate_out, (uint32_t) 1, (uint32_t) 1000000);
		} else if(option == "--period-in") {
			parse_option_value(has_value, option, value, g_option_period_in, (uint32_t) 1, (uint32_t) 1000000);
		} else if(option == "--period-out") {
			parse_option_value(has_value, option, value, g_option_period_out, (uint32_t) 1, (uint32_t) 1000000);
		} else if(option == "--buffer-in") {
			parse_option_value(has_value, option, value, g_option_buffer_in, (uint32_t) 1, (uint32_t) 1000000);
		} else if(option == "--buffer-out") {
			parse_option_value(has_value, option, value, g_option_buffer_out, (uint32_t) 1, (uint32_t) 1000000);
		} else if(option == "--target-level") {
			parse_option_value(has_value, option, value, g_option_target_level, (uint32_t) 1, (uint32_t) 1000000);
		} else if(option == "--timer-period") {
			parse_option_value(has_value, option, value, g_option_timer_period, (uint32_t) 1000, (uint32_t) 100000000);
		} else if(option == "--loop-bandwidth") {
			parse_option_value(has_value, option, value, g_option_loop_bandwidth, 0.001f, 10.0f);
		} else if(option == "--max-drift") {
			parse_option_value(has_value, option, value, g_option_max_drift, 0.0f, 0.1f);
		} else if(option == "--resampler-passband") {
			parse_option_value(has_value, option, value, g_option_resampler_passband, lowrider_resampler::PASSBAND_MIN, lowrider_resampler::PASSBAND_MAX);
		} else if(option == "--resampler-stopband") {
			parse_option_value(has_value, option, value, g_option_resampler_stopband, lowrider_resampler::STOPBAND_MIN, lowrider_resampler::STOPBAND_MAX);
		} else if(option == "--resampler-beta") {
			parse_option_value(has_value, option, value, g_option_resampler_beta, lowrider_resampler::BETA_MIN, lowrider_resampler::BETA_MAX);
		} else if(option == "--resampler-gain") {
			parse_option_value(has_value, option, value, g_option_resampler_gain, 0.0f, 1000000.0f);
		} else {
			throw std::runtime_error(make_string("invalid command-line option '", arg, "'"));
		}

	}

	// check for incompatible options
	if((uint32_t) g_option_help + (uint32_t) g_option_version + (uint32_t) g_option_analyze_resampler + (uint32_t) g_option_test_hardware > 1) {
		std::ostringstream ss;
		ss << "incompatible options:";
		if(g_option_help)
			ss << " --help";
		if(g_option_version)
			ss << " --version";
		if(g_option_analyze_resampler)
			ss << " --analyze-resampler";
		if(g_option_test_hardware)
			ss << " --test-hardware";
		throw std::runtime_error(ss.str());
	}

	// check for missing options
	if(!g_option_help && !g_option_version && !g_option_analyze_resampler) {
		if(g_option_device_in.empty()) {
			throw std::runtime_error("missing option: --device-in");
		}
		if(g_option_device_out.empty()) {
			throw std::runtime_error("missing option: --device-out");
		}
	}

}
