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

# DartApiGenerator only uses the libclang C API (clang-c/Index.h), so it only
# needs the merged libclang shared library plus that one header. We locate
# those directly rather than via find_package(LLVM/Clang CONFIG): that path
# pulls in the full internal component graph (clangBasic, clangAST, ...), and
# several distro packages (notably Ubuntu's libclang-dev) export cmake
# targets for those components without actually shipping the .a archives
# they reference, which makes find_package() fail outright even though the
# one thing we need is present.
set(LIBCLANG_SEARCH_LIB_HINTS "")
set(LIBCLANG_SEARCH_INCLUDE_HINTS "")

if(APPLE)
  execute_process(COMMAND brew --prefix llvm
      OUTPUT_VARIABLE LLVM_BREW_PREFIX
      OUTPUT_STRIP_TRAILING_WHITESPACE
      ERROR_QUIET)
  if(LLVM_BREW_PREFIX)
    list(APPEND LIBCLANG_SEARCH_LIB_HINTS "${LLVM_BREW_PREFIX}/lib")
    list(APPEND LIBCLANG_SEARCH_INCLUDE_HINTS "${LLVM_BREW_PREFIX}/include")
  endif()
elseif(WIN32)
  # Default install locations for both the official LLVM.org installer and
  # `choco install llvm`.
  list(APPEND LIBCLANG_SEARCH_LIB_HINTS "C:/Program Files/LLVM/lib")
  list(APPEND LIBCLANG_SEARCH_INCLUDE_HINTS "C:/Program Files/LLVM/include")
endif()

# Opportunistically widen the search using llvm-config, when present
# (apt's llvm-dev always ships it; the Windows/macOS installs may not).
find_program(LLVM_CONFIG_EXECUTABLE NAMES llvm-config llvm-config.exe)
if(LLVM_CONFIG_EXECUTABLE)
  execute_process(COMMAND ${LLVM_CONFIG_EXECUTABLE} --libdir
      OUTPUT_VARIABLE LLVM_CONFIG_LIBDIR OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
  execute_process(COMMAND ${LLVM_CONFIG_EXECUTABLE} --includedir
      OUTPUT_VARIABLE LLVM_CONFIG_INCLUDEDIR OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
  list(APPEND LIBCLANG_SEARCH_LIB_HINTS "${LLVM_CONFIG_LIBDIR}")
  list(APPEND LIBCLANG_SEARCH_INCLUDE_HINTS "${LLVM_CONFIG_INCLUDEDIR}")
endif()

find_library(LIBCLANG_LIBRARY NAMES clang libclang HINTS ${LIBCLANG_SEARCH_LIB_HINTS})
find_path(LIBCLANG_INCLUDE_DIR NAMES clang-c/Index.h HINTS ${LIBCLANG_SEARCH_INCLUDE_HINTS})

if(NOT LIBCLANG_LIBRARY OR NOT LIBCLANG_INCLUDE_DIR)
  message(FATAL_ERROR "libclang (and its clang-c/Index.h header) not found. "
                       "Install LLVM/Clang -- see docs/getting_started/building_from_source.md.")
endif()

add_executable(DartApiGenerator DartApiGenerator.cpp)
target_include_directories(DartApiGenerator PRIVATE ${LIBCLANG_INCLUDE_DIR})
target_link_libraries(DartApiGenerator PRIVATE ${LIBCLANG_LIBRARY})

# Output the compiled tool to a predictable tools/ directory
set_target_properties(DartApiGenerator PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tools")
