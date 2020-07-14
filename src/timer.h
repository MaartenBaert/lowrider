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
