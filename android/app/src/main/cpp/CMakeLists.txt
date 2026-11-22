# Native build of the Android app: the engine (BIRD_PLATFORM defaults to
# ANDROID under the NDK toolchain) and the sample game as a separate library,
# mirroring the desktop plugin model.
cmake_minimum_required(VERSION 3.23)
project(BirdAndroid C CXX)

set(BIRD_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../../..")
add_subdirectory("${BIRD_ROOT}" bird)

add_library(Bird_test SHARED "${BIRD_ROOT}/test/main.cpp")
target_include_directories(Bird_test PRIVATE "${BIRD_ROOT}/src")
target_link_libraries(Bird_test PRIVATE Bird_core)
