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
      SkeletalImport.cpp SkeletalImport.h
      ../animation/skeletal/Skeleton.cpp
      ../animation/skeletal/SkeletalClip.cpp
      ../animation/skeletal/SkeletalAsset.cpp
      ../renders/generic/ShaderBinary.cpp ../renders/generic/ShaderBinary.h
      ../util/stb_image.h ../util/stb_truetype.h)
target_include_directories(BirdBake PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(BirdBake PUBLIC ${ASSIMP_LIBRARIES} glm
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

# libclang parses headers like a compiler, so it needs the builtin headers
# (stddef.h, stdarg.h, ...) of its own version. Pointing -resource-dir at
# the copy installed beside the library keeps them in the driver's usual
# place in the search order; an -isystem override would shadow the C++
# standard library's #include_next chains.
get_filename_component(_LIBCLANG_REAL "${LIBCLANG_LIBRARY}" REALPATH)
get_filename_component(_LIBCLANG_DIR "${_LIBCLANG_REAL}" DIRECTORY)
# Debian/Ubuntu keep the library in /usr/lib/<triple> but the headers under
# /usr/lib/llvm-<major>/lib/clang/<major>, so search those roots too.
get_filename_component(_LIBCLANG_NAME "${_LIBCLANG_REAL}" NAME)
set(_LIBCLANG_MAJOR "")
if(_LIBCLANG_NAME MATCHES "^libclang(-([0-9]+))?\\.so\\.([0-9]+)")
  set(_LIBCLANG_MAJOR "${CMAKE_MATCH_3}")
endif()
set(_CLANG_RESOURCE_ROOTS "${_LIBCLANG_DIR}" "${_LIBCLANG_DIR}/../lib")
if(LLVM_CONFIG_LIBDIR)
  list(APPEND _CLANG_RESOURCE_ROOTS "${LLVM_CONFIG_LIBDIR}")
endif()
if(_LIBCLANG_MAJOR)
  list(APPEND _CLANG_RESOURCE_ROOTS "/usr/lib/llvm-${_LIBCLANG_MAJOR}/lib")
endif()
list(APPEND _CLANG_RESOURCE_ROOTS "/usr/lib")
set(BIRD_CLANG_RESOURCE_DIR "")
set(_CLANG_RESOURCE_FALLBACK "")
foreach(_root IN LISTS _CLANG_RESOURCE_ROOTS)
  file(GLOB _CLANG_RESOURCE_CANDIDATES LIST_DIRECTORIES true "${_root}/clang/*")
  foreach(_candidate IN LISTS _CLANG_RESOURCE_CANDIDATES)
    if(NOT EXISTS "${_candidate}/include/stddef.h")
      continue()
    endif()
    get_filename_component(_candidate_name "${_candidate}" NAME)
    if(NOT BIRD_CLANG_RESOURCE_DIR AND _LIBCLANG_MAJOR
       AND _candidate_name MATCHES "^${_LIBCLANG_MAJOR}(\\.|$)")
      get_filename_component(BIRD_CLANG_RESOURCE_DIR "${_candidate}" REALPATH)
    elseif(NOT _CLANG_RESOURCE_FALLBACK)
      get_filename_component(_CLANG_RESOURCE_FALLBACK "${_candidate}" REALPATH)
    endif()
  endforeach()
endforeach()
# Unversioned library names (macOS, Windows) take the copy beside them.
if(NOT BIRD_CLANG_RESOURCE_DIR)
  set(BIRD_CLANG_RESOURCE_DIR "${_CLANG_RESOURCE_FALLBACK}")
endif()
if(NOT BIRD_CLANG_RESOURCE_DIR)
  message(FATAL_ERROR "Found libclang at ${_LIBCLANG_REAL}, but not its builtin "
                      "headers (clang/<version>/include/stddef.h beside it). "
                      "Install the matching clang resource headers.")
endif()

set(_GENERATOR_PARSE_FLAGS "-resource-dir=${BIRD_CLANG_RESOURCE_DIR}")
if(APPLE)
  # libclang does not ask xcrun for the SDK the way the clang driver does.
  execute_process(COMMAND xcrun --show-sdk-path
      OUTPUT_VARIABLE _MACOS_SDK OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
  if(NOT _MACOS_SDK)
    set(_MACOS_SDK "${CMAKE_OSX_SYSROOT}")
  endif()
  if(_MACOS_SDK)
    list(APPEND _GENERATOR_PARSE_FLAGS "-isysroot" "${_MACOS_SDK}")
  endif()
endif()
set(BIRD_DART_GENERATOR_PARSE_FLAGS "${_GENERATOR_PARSE_FLAGS}" CACHE INTERNAL
    "Flags DartApiGenerator passes to libclang")
message(STATUS "DartApiGenerator: libclang ${_LIBCLANG_REAL}, resource dir ${BIRD_CLANG_RESOURCE_DIR}")

add_executable(DartApiGenerator DartApiGenerator.cpp)
target_include_directories(DartApiGenerator PRIVATE ${LIBCLANG_INCLUDE_DIR})
target_link_libraries(DartApiGenerator PRIVATE ${LIBCLANG_LIBRARY})

# Output the compiled tool to a predictable tools/ directory
set_target_properties(DartApiGenerator PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tools")
