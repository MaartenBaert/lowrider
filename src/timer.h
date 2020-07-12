#pragma once

#include <cstdint>

class lowrider_timer {

private:
	int m_timer;

public:
	// Creates a timer with a period specified in microseconds.
	lowrider_timer();

	// Destroys the timer
	~lowrider_timer();

	// Start the timer with the given period (in nanoseconds).
	void start(uint64_t period);

	// Stops the timer.
	void stop();

	// Waits for the timer to expire. Returns the number of times the timer has expired since the last call.
	uint32_t wait();

};
