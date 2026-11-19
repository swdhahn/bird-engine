cmake_minimum_required(VERSION 3.23)

set(CMAKE_CXX_STANDARD 17)

set(CMAKE_PREFIX_PATH libs/install)
find_package(assimp CONFIG REQUIRED)

find_package(spirv_cross_core CONFIG REQUIRED)
find_package(spirv_cross_glsl CONFIG REQUIRED)
find_package(spirv_cross_cpp CONFIG REQUIRED)
find_package(SPIRV-Tools CONFIG REQUIRED)

# --- Bake library: everything that compiles .bird resources ---
# Linked by the BuildTools executable, and by any app that must re-bake at
# runtime (e.g. an editor). The engine runtime itself never links it.
add_library(BirdBake STATIC
      AssetLoader.cpp AssetLoader.h
      ShaderBaker.cpp ShaderBaker.h
      FormatHeaders.h
      ../renders/generic/ShaderBinary.cpp ../renders/generic/ShaderBinary.h
      ../util/stb_image.h ../util/stb_truetype.h)
target_include_directories(BirdBake PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(BirdBake PUBLIC ${ASSIMP_LIBRARIES}
      shaderc::shaderc_combined
      ${SPIRV_LIBRARIES}
      spirv-cross-core
      spirv-cross-glsl
      spirv-cross-cpp)

add_executable(BuildTools build-tools.cpp)
target_link_libraries(BuildTools PRIVATE BirdBake)

# --- NEW: Dart API Generator Target ---

# 1. Find LLVM and Clang (Requires libclang-dev / llvm-dev installed on host)
# Homebrew's llvm is keg-only (not symlinked into /opt/homebrew), so point
# CMake at it explicitly when LLVM_DIR/Clang_DIR aren't already set.
if(APPLE AND NOT LLVM_DIR)
  execute_process(COMMAND brew --prefix llvm
      OUTPUT_VARIABLE LLVM_BREW_PREFIX
      OUTPUT_STRIP_TRAILING_WHITESPACE
      ERROR_QUIET)
  if(LLVM_BREW_PREFIX)
    list(APPEND CMAKE_PREFIX_PATH "${LLVM_BREW_PREFIX}")
  endif()
endif()
find_package(LLVM REQUIRED CONFIG)
find_package(Clang REQUIRED CONFIG)

# 2. Define the generator executable
add_executable(DartApiGenerator DartApiGenerator.cpp)

# 3. Add necessary include and library directories for GCC to find Clang headers
target_include_directories(DartApiGenerator PRIVATE ${LLVM_INCLUDE_DIRS} ${CLANG_INCLUDE_DIRS})
target_link_directories(DartApiGenerator PRIVATE ${LLVM_LIBRARY_DIRS})

# 4. Link against the Clang C-API library
target_link_libraries(DartApiGenerator PRIVATE libclang)

# 5. Output the compiled tool to a predictable tools/ directory
set_target_properties(DartApiGenerator PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tools")
