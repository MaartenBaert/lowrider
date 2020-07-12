#pragma once

#include <cassert>
#include <cmath>

#include <algorithm>

template<typename T>
inline T sqr(T x) {
	return x * x;
}

template<typename T, typename U>
inline T clamp(U v, T lo, T hi) {
	assert(lo <= hi);
	if(v < (U) lo)
		return lo;
	if(v > (U) hi)
		return hi;
	return (T) v;
}

template<> inline float clamp<float, float>(float v, float lo, float hi) {
	assert(!(lo > hi)); // nan ok
	return std::min(std::max(v, lo), hi);
}

template<> inline double clamp<double, double>(double v, double lo, double hi) {
	assert(!(lo > hi)); // nan ok
	return std::min(std::max(v, lo), hi);
}

template<typename F>
inline int32_t rint32(F x) {
	return (sizeof(long int) >= sizeof(int32_t))? (int32_t) std::lrint(x) : (int32_t) std::llrint(x);
}

template<typename F>
inline int64_t rint64(F x) {
	return (sizeof(long int) >= sizeof(int64_t))? (int64_t) std::lrint(x) : (int64_t) std::llrint(x);
}
