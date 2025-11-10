cmake_minimum_required(VERSION 3.23)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(Bird_test)

add_library(Bird_test SHARED main.cpp)

set(Bird_Engine_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../build/cmake/Bird_Engine")
find_package(Bird_Engine CONFIG REQUIRED)

target_link_libraries(Bird_test PRIVATE Bird::Bird_core)
# target_link_libraries(Bird_test PRIVATE Bird::Bird_core)
# target_link_directories(Bird_test PUBLIC -L../)
# set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/../)
