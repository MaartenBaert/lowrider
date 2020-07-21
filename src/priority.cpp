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

#include "priority.h"

#include "options.h"

#include <iostream>

#include <sched.h>
#include <sys/mman.h>
#include <sys/resource.h>

void set_realtime_priority() {

	// should we enable real-time priority?
	if(g_option_realtime_priority != 0) {

		// get real-time priority limit
		rlimit limit;
		getrlimit(RLIMIT_RTPRIO, &limit);
		if(limit.rlim_cur <= 0) {
			g_option_realtime_priority = 0;
			std::cerr << "Warning: system does not allow real-time priority" << std::endl;
			return;
		}
		if((rlim_t) g_option_realtime_priority > limit.rlim_cur) {
			g_option_realtime_priority = limit.rlim_cur;
			std::cerr << "Warning: system limits real-time priority to " << g_option_realtime_priority << std::endl;
		}

		// set realtime priority
		int priority = (int) g_option_realtime_priority;
		if(sched_setscheduler(0, SCHED_RR, (sched_param*) &priority) != 0) {
			g_option_realtime_priority = 0;
			std::cerr << "Warning: failed to set real-time priority" << std::endl;
		}

	}

}

void set_memory_lock() {

	// should we enable memory locking?
	if(g_option_memory_lock) {

		// lock all memory
		if(mlockall(MCL_CURRENT | MCL_FUTURE) != 0) {
			g_option_memory_lock = false;
			std::cerr << "Warning: failed to lock process memory" << std::endl;
		}

	}

}
