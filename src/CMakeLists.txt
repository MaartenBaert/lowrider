set(CMAKE_INCLUDE_CURRENT_DIR TRUE)

if(WITH_ALSA)
	find_package(ALSA REQUIRED)
endif()
if(WITH_PULSEAUDIO)
	find_package(PulseAudio REQUIRED)
endif()
if(WITH_JACK)
	find_package(Jack REQUIRED)
endif()

set(sources
	aligned_memory.h
	analyze_resampler.cpp
	analyze_resampler.h
	backend_alsa.cpp
	backend_alsa.h
	bessel.cpp
	bessel.h
	loopback.cpp
	loopback.h
	main.cpp
	miscmath.h
	options.cpp
	options.h
	priority.cpp
	priority.h
	resampler.cpp
	resampler.h
	sample_format.h
	signals.cpp
	signals.h
	string_helper.h
	timer.cpp
	timer.h
)

if(ENABLE_ASM)

	#list(APPEND sources
	#	AV/FastResampler_FirFilter_SSE2.cpp
	#)

	#set_source_files_properties(
	#	AV/FastResampler_FirFilter_SSE2.cpp
	#	PROPERTIES COMPILE_FLAGS -msse2
	#)

endif()

add_executable(lowrider
	${sources}
)

target_include_directories(lowrider PRIVATE
	$<$<BOOL:${WITH_ALSA}>:${ALSA_INCLUDE_DIRS}>
	$<$<BOOL:${WITH_PULSEAUDIO}>:${PULSEAUDIO_INCLUDE_DIRS}>
	$<$<BOOL:${WITH_JACK}>:${JACK_INCLUDE_DIRS}>
)

target_link_libraries(lowrider PRIVATE
	$<$<BOOL:${WITH_ALSA}>:${ALSA_LIBRARIES}>
	$<$<BOOL:${WITH_PULSEAUDIO}>:${PULSEAUDIO_LIBRARIES}>
	$<$<BOOL:${WITH_JACK}>:${JACK_LIBRARIES}>
)

target_compile_definitions(lowrider PRIVATE
	-DLOWRIDER_ENABLE_ASM=$<BOOL:${ENABLE_ASM}>
	-DLOWRIDER_WITH_ALSA=$<BOOL:${WITH_ALSA}>
	-DLOWRIDER_WITH_PULSEAUDIO=$<BOOL:${WITH_PULSEAUDIO}>
	-DLOWRIDER_WITH_JACK=$<BOOL:${WITH_JACK}>
	-DLOWRIDER_VERSION="${PROJECT_VERSION}"
)
