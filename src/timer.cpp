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
