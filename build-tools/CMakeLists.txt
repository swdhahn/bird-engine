cmake_minimum_required(VERSION 3.23)
set(CMAKE_CXX_STANDARD 17)

find_package(Vulkan REQUIRED)

project(BuildTools)

add_executable(BuildTools src/Main.cpp)


