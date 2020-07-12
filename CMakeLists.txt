cmake_minimum_required(VERSION 3.1)
cmake_policy(VERSION 3.1)
set(CMAKE_CXX_STANDARD 11)

project(lowrider VERSION 0.0.0)

option(ENABLE_ASM "Allow architecture-specific assembly instructions or intrinsics for better performance." ON)

option(WITH_ALSA "Build with ALSA support." ON)
option(WITH_PULSEAUDIO "Build with PulseAudio support." OFF)
option(WITH_JACK "Build with JACK support." OFF)

set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)

include(FeatureSummary)

add_subdirectory(src)

feature_summary(WHAT ALL FATAL_ON_MISSING_REQUIRED_PACKAGES)
