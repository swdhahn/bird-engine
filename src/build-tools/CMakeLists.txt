cmake_minimum_required(VERSION 3.23)

set(CMAKE_CXX_STANDARD 17)

add_executable(BuildTools build-tools.cpp AssetLoader.cpp AssetLoader.h ../util/stb_image.h)
target_link_libraries(BuildTools stdc++ -lassimp)
