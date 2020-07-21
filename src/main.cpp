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
#include "loopback.h"
#include "options.h"
#include "priority.h"
#include "signals.h"

#include <cstdlib>

#include <iostream>
#include <exception>

int main(int argc, char *argv[]) {

	try {

		// register signals
		register_signals();

		// parse options
		parse_options(argc, argv);

		// initialization
		set_realtime_priority();
		set_memory_lock();

		// run the program
		if(g_option_help) {
			print_help();
		} else if(g_option_version) {
			print_version();
		} else if(g_option_analyze_resampler) {
			analyze_resampler();
		} else if(g_option_test_hardware) {
			test_hardware();
		} else {
			run_loopback();
		}

	} catch(const std::exception &e) {
		std::cerr << "Error: " << e.what() << std::endl;
		return EXIT_FAILURE;
	} catch(...) {
		std::cerr << "Error: unknown exception" << std::endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;

}
