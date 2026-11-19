cmake_minimum_required(VERSION 3.23)

set(CMAKE_CXX_STANDARD 17)

set(CMAKE_PREFIX_PATH libs/install)
find_package(assimp CONFIG REQUIRED)

find_package(spirv_cross_core CONFIG REQUIRED)
find_package(spirv_cross_glsl CONFIG REQUIRED)
find_package(spirv_cross_cpp CONFIG REQUIRED)
find_package(SPIRV-Tools CONFIG REQUIRED)

# --- Existing BuildTools Target ---
add_executable(BuildTools build-tools.cpp AssetLoader.cpp AssetLoader.h
                         ../util/stb_image.h)
target_link_libraries(BuildTools ${ASSIMP_LIBRARIES}
      shaderc::shaderc_combined
      ${SPIRV_LIBRARIES}
      spirv-cross-core
      spirv-cross-glsl
      spirv-cross-cpp)

# --- NEW: Dart API Generator Target ---

# 1. Find LLVM and Clang (Requires libclang-dev / llvm-dev installed on host)
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
