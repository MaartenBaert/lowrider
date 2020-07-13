#pragma once

#include <cassert>
#include <cstddef>
#include <cstdlib>

#include <new>

template<typename T>
class lowrider_aligned_memory {

private:
	T *m_data;

public:
	lowrider_aligned_memory() noexcept {
		m_data = nullptr;
	}

	lowrider_aligned_memory(size_t alignment, size_t size) {
		assert(size % alignment == 0);
		m_data = (T*) aligned_alloc(alignment * sizeof(T), size * sizeof(T));
		if(m_data == nullptr) {
			throw std::bad_alloc();
		}
	}

	~lowrider_aligned_memory() noexcept {
		::free(m_data);
	}

	lowrider_aligned_memory(const lowrider_aligned_memory&) = delete;

	lowrider_aligned_memory(lowrider_aligned_memory &&other) noexcept {
		m_data = other.m_data;
		other.m_data = nullptr;
	}

	lowrider_aligned_memory& operator=(const lowrider_aligned_memory&) = delete;

	lowrider_aligned_memory& operator=(lowrider_aligned_memory &&other) noexcept {
		::free(m_data);
		m_data = other.m_data;
		other.m_data = nullptr;
	}

	void allocate(size_t alignment, size_t size) {
		assert(size % alignment == 0);
		::free(m_data);
		m_data = (T*) aligned_alloc(alignment * sizeof(T), size * sizeof(T));
		if(m_data == nullptr) {
			throw std::bad_alloc();
		}
	}

	void free() noexcept {
		::free(m_data);
		m_data = nullptr;
	}

	T* data() noexcept {
		return m_data;
	}

	const T* data() const noexcept {
		return m_data;
	}

};
