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

#include "timer.h"

#include <cassert>
#include <cerrno>
#include <ctime>

#include <stdexcept>

#include <sys/timerfd.h>
#include <unistd.h>

lowrider_timer::lowrider_timer() {
	m_timer = timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC);
	if(m_timer == -1) {
		throw std::runtime_error("failed to create timer");
	}
}

lowrider_timer::~lowrider_timer() {
	int res;
	do {
		res = close(m_timer);
	} while(res == -1 && errno == EINTR);
	assert(res == 0);
}

void lowrider_timer::start(uint64_t period) {
	itimerspec spec;
	spec.it_interval.tv_sec = period / 1000000000;
	spec.it_interval.tv_nsec = period % 1000000000;
	spec.it_value.tv_sec = period / 1000000000;
	spec.it_value.tv_nsec = period % 1000000000;
	if(timerfd_settime(m_timer, 0, &spec, nullptr) != 0) {
		throw std::runtime_error("failed to start timer");
	}
}

void lowrider_timer::stop() {
	itimerspec spec;
	spec.it_interval.tv_sec = 0;
	spec.it_interval.tv_nsec = 0;
	spec.it_value.tv_sec = 0;
	spec.it_value.tv_nsec = 0;
	if(timerfd_settime(m_timer, 0, &spec, nullptr) != 0) {
		throw std::runtime_error("failed to stop timer");
	}
}

uint32_t lowrider_timer::wait() {
	uint64_t expired;
	ssize_t res;
	do {
		res = read(m_timer, &expired, sizeof(expired));
	} while(res == -1 && errno == EINTR);
	if(res != sizeof(expired)) {
		throw std::runtime_error("failed to wait for timer");
	}
	return (uint32_t) expired;
}
