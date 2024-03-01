cmake_minimum_required(VERSION 3.23)

set(CMAKE_CXX_STANDARD 17)

add_definitions(-DBIRD_BUILD_TOOLS_FILE_PATH="src/build-tools")

add_executable(BuildTools build-tools.cpp AssetLoader.cpp AssetLoader.h)
include_directories(../../libs/XXHASH/src/XXHASH)
target_link_libraries(BuildTools stdc++ -lassimp)
