cmake_minimum_required(VERSION 3.23)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(Bird)

# OpenGL API profile. DESKTOP: OpenGL 4.1+ through GLEW. ES: OpenGL ES 3.0+,
# the profile shared by Android, iOS (deprecated GLES) and WebGL 2; on Linux
# it runs on Mesa for testing. Optional features are detected at run time.
if(ANDROID OR IOS OR EMSCRIPTEN)
  set(BIRD_DEFAULT_GL_PROFILE "ES")
else()
  set(BIRD_DEFAULT_GL_PROFILE "DESKTOP")
endif()
set(BIRD_GL_PROFILE "${BIRD_DEFAULT_GL_PROFILE}" CACHE STRING
    "OpenGL API profile: DESKTOP (OpenGL 4.1+) or ES (OpenGL ES 3.0+)")
set_property(CACHE BIRD_GL_PROFILE PROPERTY STRINGS DESKTOP ES)
if(NOT BIRD_GL_PROFILE MATCHES "^(DESKTOP|ES)$")
  message(FATAL_ERROR "BIRD_GL_PROFILE must be DESKTOP or ES, not '${BIRD_GL_PROFILE}'")
endif()
message(STATUS "Bird OpenGL profile: ${BIRD_GL_PROFILE}")
if(BIRD_GL_PROFILE STREQUAL "ES")
  add_compile_definitions(BIRD_GL_ES)
endif()

# Platform backend: window, input events, assets, logging.
#   DESKTOP   GLFW window (Linux, Windows, macOS)
#   HEADLESS  EGL pbuffer, no input; for CI rendering and captures (Linux)
#   ANDROID   NativeActivity, EGL on the activity's window, APK assets
# iOS and Web backends follow.
if(ANDROID)
  set(BIRD_DEFAULT_PLATFORM "ANDROID")
else()
  set(BIRD_DEFAULT_PLATFORM "DESKTOP")
endif()
set(BIRD_PLATFORM "${BIRD_DEFAULT_PLATFORM}" CACHE STRING
    "Platform backend: DESKTOP, HEADLESS or ANDROID")
set_property(CACHE BIRD_PLATFORM PROPERTY STRINGS DESKTOP HEADLESS ANDROID)
# Host platforms also build the asset compiler, the Bird launcher, Dart
# scripting, the tests and the install package; device platforms only
# build the runtime library and load assets baked on the host.
if(BIRD_PLATFORM MATCHES "^(DESKTOP|HEADLESS)$")
  set(BIRD_HOST_BUILD ON)
else()
  set(BIRD_HOST_BUILD OFF)
endif()
set(BIRD_PLATFORM_FILES
    src/platform/Platform.h
    src/platform/Clock.h
    src/platform/Clock.cpp
    src/platform/Log.h
    src/platform/Log.cpp
    src/platform/VFS.h
    src/platform/VFS.cpp)
if(BIRD_PLATFORM STREQUAL "DESKTOP")
  list(APPEND BIRD_PLATFORM_FILES
      src/platform/desktop/GlfwWindow.h
      src/platform/desktop/GlfwWindow.cpp
      src/platform/desktop/DesktopPlatform.cpp)
elseif(BIRD_PLATFORM STREQUAL "HEADLESS")
  list(APPEND BIRD_PLATFORM_FILES
      src/platform/headless/HeadlessWindow.h
      src/platform/headless/HeadlessWindow.cpp
      src/platform/headless/HeadlessPlatform.cpp)
elseif(BIRD_PLATFORM STREQUAL "ANDROID")
  if(NOT ANDROID)
    message(FATAL_ERROR "BIRD_PLATFORM=ANDROID needs the Android NDK toolchain")
  endif()
  list(APPEND BIRD_PLATFORM_FILES
      src/platform/android/AndroidApp.h
      src/platform/android/AndroidApp.cpp
      src/platform/android/AndroidKeyMap.h
      src/platform/android/AndroidKeyMap.cpp
      src/platform/android/AndroidLog.h
      src/platform/android/AndroidLog.cpp
      src/platform/android/AndroidWindow.h
      src/platform/android/AndroidWindow.cpp
      src/platform/android/ApkAssetVFS.h
      src/platform/android/ApkAssetVFS.cpp
      src/platform/android/AndroidMain.cpp)
else()
  message(FATAL_ERROR "BIRD_PLATFORM '${BIRD_PLATFORM}' is not implemented yet")
endif()

set(BIRD_FILES
    ${BIRD_PLATFORM_FILES}
    src/util/stb_image.h
    src/util/Constants.h
    src/util/Memory.hpp
    src/util/Math.h
    src/util/Math.cpp
    src/util/File.cpp
    src/util/File.h
    src/util/Input.cpp
    src/util/Input.h
    src/Application.cpp
    src/Application.h
    src/util/ResourceManager.cpp
    src/util/ResourceManager.h
    src/util/Resource.cpp
    src/util/Resource.h)
add_library(
  Bird_core SHARED
  ${BIRD_FILES} $<TARGET_OBJECTS:renders> $<TARGET_OBJECTS:opengl-renderer>
  $<TARGET_OBJECTS:scene> $<TARGET_OBJECTS:physics>
  $<TARGET_OBJECTS:animation>)
if(BIRD_HOST_BUILD)
  add_executable(Bird src/main.cpp)
  target_link_libraries(Bird Bird_core)
endif()

target_link_options(
  Bird_core PRIVATE
  # Only apply -Bsymbolic on Linux and Android (ELF systems)
  $<$<PLATFORM_ID:Linux,Android>:-Wl,-Bsymbolic>)
set_target_properties(Bird_core PROPERTIES
    POSITION_INDEPENDENT_CODE TRUE
    BUILD_WITH_INSTALL_RPATH TRUE
    INSTALL_RPATH "$ORIGIN"
)

if(APPLE)
  target_link_libraries(
    Bird_core
    PUBLIC "-framework Cocoa"
           "-framework IOKit"
           "-framework CoreVideo"
           "-framework OpenGL"
           "-framework AppKit"
           "-framework Metal"
           "-framework QuartzCore")
elseif(WIN32)
  if(MSVC)
    target_link_libraries(Bird_core PUBLIC user32 kernel32 opengl32)
    set_target_properties(Bird_core PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS TRUE)
  else()
    target_link_libraries(Bird_core PUBLIC -luser32 -lkernel32 -lopengl32)

  endif()
elseif(UNIX AND NOT APPLE AND NOT ANDROID)
  target_link_libraries(Bird_core PUBLIC -lpthread -ldl)
  if(BIRD_PLATFORM STREQUAL "DESKTOP")
    # GLFW's X11 backend.
    find_package(X11 REQUIRED)
    target_link_libraries(
      Bird_core
      PUBLIC -lX11
             -lXxf86vm
             -lXrandr
             -lXi
             -lXinerama
             -lXcursor)
  endif()
endif()

set(INSTALLED_LIB_PATH "${CMAKE_CURRENT_SOURCE_DIR}/libs/install")
set(CMAKE_PREFIX_PATH "${INSTALLED_LIB_PATH}")

if(BIRD_PLATFORM STREQUAL "DESKTOP")
  find_package(glfw3 CONFIG REQUIRED)
  set(BIRD_PLATFORM_LIBRARIES glfw)
elseif(BIRD_PLATFORM STREQUAL "HEADLESS")
  find_library(BIRD_HEADLESS_EGL_LIBRARY NAMES EGL REQUIRED)
  set(BIRD_PLATFORM_LIBRARIES ${BIRD_HEADLESS_EGL_LIBRARY})
elseif(BIRD_PLATFORM STREQUAL "ANDROID")
  # NativeActivity glue from the NDK; ANativeActivity_onCreate must survive
  # linking because the system looks it up by name.
  add_library(bird_native_app_glue STATIC
      ${ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c)
  target_include_directories(bird_native_app_glue
      PUBLIC ${ANDROID_NDK}/sources/android/native_app_glue)
  target_link_libraries(bird_native_app_glue PUBLIC android log)
  target_include_directories(Bird_core
      PRIVATE ${ANDROID_NDK}/sources/android/native_app_glue)
  target_link_options(Bird_core PRIVATE -u ANativeActivity_onCreate)
  # The game's shared library inside the APK, loaded by AndroidMain.
  set(BIRD_ANDROID_GAME_LIBRARY "libgame.so" CACHE STRING
      "File name of the game library the Android launcher loads")
  target_compile_definitions(Bird_core PRIVATE
      BIRD_ANDROID_GAME_LIBRARY="${BIRD_ANDROID_GAME_LIBRARY}")
  set(BIRD_PLATFORM_LIBRARIES bird_native_app_glue android log dl)
endif()
if(BIRD_HOST_BUILD)
  find_package(assimp CONFIG REQUIRED)
endif()

# GL libraries for the selected profile, linked by every GL-using target.
if(BIRD_GL_PROFILE STREQUAL "DESKTOP")
  find_package(OpenGL REQUIRED)
  find_package(glew CONFIG REQUIRED)
  set(BIRD_GL_LIBRARIES libglew_static)
  if(UNIX AND NOT APPLE)
    list(APPEND BIRD_GL_LIBRARIES -lGL)
  endif()
elseif(ANDROID)
  set(BIRD_GL_LIBRARIES GLESv3 EGL)
elseif(IOS)
  set(BIRD_GL_LIBRARIES "-framework OpenGLES")
elseif(EMSCRIPTEN)
  # Emscripten maps the ES 3.0 API onto WebGL 2.
  set(BIRD_GL_LIBRARIES "-sMAX_WEBGL_VERSION=2" "-sMIN_WEBGL_VERSION=2")
else()
  # Desktop testing of the ES profile (Mesa, ANGLE): GLES plus EGL.
  find_library(BIRD_GLES_LIBRARY NAMES GLESv2 libGLESv2 REQUIRED)
  find_library(BIRD_EGL_LIBRARY NAMES EGL libEGL REQUIRED)
  set(BIRD_GL_LIBRARIES ${BIRD_GLES_LIBRARY} ${BIRD_EGL_LIBRARY})
endif()
if(BIRD_HOST_BUILD)
  find_package(spirv_cross_core CONFIG REQUIRED)
  find_package(spirv_cross_glsl CONFIG REQUIRED)
  find_package(spirv_cross_cpp CONFIG REQUIRED)
  find_package(SPIRV-Tools CONFIG REQUIRED)
  find_package(Jolt CONFIG REQUIRED)
  set(BIRD_GLM_INCLUDE_DIR "${INSTALLED_LIB_PATH}/include")
else()
  # Device builds fetch their two runtime dependencies from source, with
  # the versions and Jolt options libs/CMakeLists.txt uses on the host.
  include(FetchContent)
  FetchContent_Declare(bird_glm
      GIT_REPOSITORY https://github.com/g-truc/glm.git
      GIT_TAG 0.9.9.8
      GIT_SHALLOW TRUE
      SOURCE_SUBDIR do-not-configure)  # headers only
  set(CROSS_PLATFORM_DETERMINISTIC ON CACHE BOOL "" FORCE)
  set(CPP_RTTI_ENABLED ON CACHE BOOL "" FORCE)
  set(TARGET_UNIT_TESTS OFF CACHE BOOL "" FORCE)
  set(TARGET_HELLO_WORLD OFF CACHE BOOL "" FORCE)
  set(TARGET_PERFORMANCE_TEST OFF CACHE BOOL "" FORCE)
  set(TARGET_SAMPLES OFF CACHE BOOL "" FORCE)
  set(TARGET_VIEWER OFF CACHE BOOL "" FORCE)
  set(ENABLE_ALL_WARNINGS OFF CACHE BOOL "" FORCE)
  set(INTERPROCEDURAL_OPTIMIZATION OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(bird_jolt
      GIT_REPOSITORY https://github.com/jrouwe/JoltPhysics.git
      GIT_TAG v5.4.0
      GIT_SHALLOW TRUE
      SOURCE_SUBDIR Build)
  FetchContent_MakeAvailable(bird_glm bird_jolt)
  set_target_properties(Jolt PROPERTIES POSITION_INDEPENDENT_CODE ON)
  if(NOT TARGET Jolt::Jolt)
    add_library(Jolt::Jolt ALIAS Jolt)
  endif()
  set(BIRD_GLM_INCLUDE_DIR "${bird_glm_SOURCE_DIR}")
endif()

if(TARGET libglew_static AND APPLE)
  # 1. Get the list of libraries GLEW thinks it needs
  get_target_property(_glew_libs libglew_static INTERFACE_LINK_LIBRARIES)

  if(_glew_libs)
    message(STATUS "Patching GLEW: Removing deprecated AGL dependency...")

    # 1. Remove the specific path to AGL.framework We use Regex to catch it
    #   regardless of how it's formatted (start, middle, or end of list)
    string(REGEX REPLACE "/System/Library/Frameworks/AGL\\.framework;?" ""
                         _glew_libs "${_glew_libs}")
    string(REGEX REPLACE ";/System/Library/Frameworks/AGL\\.framework" ""
                         _glew_libs "${_glew_libs}")

    # 1. Update the target with the clean list
    set_target_properties(libglew_static PROPERTIES INTERFACE_LINK_LIBRARIES
                                                    "${_glew_libs}")
  endif()
endif()

if(MSVC)
  # Visual Studio / MSVC specific
  set(LIB_EXT ".lib")
  set(LIB_PREFIX "") # MSVC often outputs "volk.lib"
  set(BUILD_SUBDIR "/$<CONFIG>") # Handles /Debug or /Release subfolders
  message(STATUS "Configuring for MSVC (Windows)")
else()
  # Linux / Mac / Unix / MinGW
  set(LIB_EXT ".a")
  set(LIB_PREFIX "lib")
  set(BUILD_SUBDIR "")
endif()

if(BIRD_HOST_BUILD)
add_library(shaderc::shaderc_combined STATIC IMPORTED)
set_target_properties(
  shaderc::shaderc_combined
  PROPERTIES IMPORTED_LOCATION
             "${INSTALLED_LIB_PATH}/lib/${LIB_PREFIX}shaderc_combined${LIB_EXT}"
)
set_target_properties(
  shaderc::shaderc_combined PROPERTIES INTERFACE_INCLUDE_DIRECTORIES
                                       "${INSTALLED_LIB_PATH}/include")
add_library(VOLK::volk STATIC IMPORTED)
set_target_properties(
  VOLK::volk PROPERTIES IMPORTED_LOCATION
                        "${INSTALLED_LIB_PATH}/lib/${LIB_PREFIX}volk${LIB_EXT}")
set_target_properties(VOLK::volk PROPERTIES INTERFACE_INCLUDE_DIRECTORIES
                                            "${INSTALLED_LIB_PATH}/include")
set(BIRD_HOST_ONLY_LIBRARIES assimp::assimp VOLK::volk)
endif()
message(STATUS "${CMAKE_CURRENT_SOURCE_DIR}")
add_library(glm INTERFACE)
target_include_directories(
  glm INTERFACE $<BUILD_INTERFACE:${BIRD_GLM_INCLUDE_DIR}>
                $<INSTALL_INTERFACE:include>)
add_subdirectory(src/renders)
include_directories(src/renders)
add_subdirectory(src/scene)
include_directories(src/scene)
add_subdirectory(src/physics)
include_directories(src/physics)
add_subdirectory(src/animation)

target_link_libraries(
  Bird_core
  PUBLIC glm
  PRIVATE ${BIRD_PLATFORM_LIBRARIES}
          ${BIRD_GL_LIBRARIES}
          ${BIRD_HOST_ONLY_LIBRARIES}
          Jolt::Jolt)
# Shader compilation (shaderc, SPIRV-Cross) lives in BirdBake/BuildTools;
# the runtime only loads baked .bird shaders.

target_compile_definitions(Bird_core PRIVATE BIRD_CORE_EXPORTS)
if(NOT MSVC)
  add_definitions(-Wno-deprecated-volatile)
endif()

if(NOT BIRD_HOST_BUILD)
  # Device build: the runtime library is the whole product.
  return()
endif()

# Build Tools
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
add_subdirectory(src/build-tools)

# Embedded Dart VM & FFI Binding Generation
option(BIRD_ENABLE_DART "Enable embedded Dart VM and FFI binding generation" ON)
if(BIRD_ENABLE_DART)
  add_subdirectory(src/dart)
  target_sources(Bird_core PRIVATE $<TARGET_OBJECTS:bird_dart>)
  target_link_libraries(Bird_core PRIVATE dart_dll)
  target_compile_definitions(Bird_core PUBLIC BIRD_ENABLE_DART)
  target_include_directories(Bird_core PUBLIC $<BUILD_INTERFACE:${BIRD_DART_GEN_C_DIR}>)
  if(UNIX AND NOT APPLE)
    target_link_options(Bird PRIVATE -Wl,--export-dynamic)
  endif()
  add_custom_command(TARGET Bird POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${BIRD_DART_DLL_PATH}"
      "$<TARGET_FILE_DIR:Bird>"
    COMMENT "Copying libdart_dll alongside Bird executable"
  )
  if(WIN32)
    install(FILES "${BIRD_DART_DLL_PATH}" DESTINATION bin)
  else()
    install(FILES "${BIRD_DART_DLL_PATH}" DESTINATION lib)
  endif()
endif()

# Unit tests (optional; requires GoogleTest)
option(BIRD_BUILD_TESTS "Build Bird engine unit tests" ON)
if(BIRD_BUILD_TESTS)
  find_package(GTest CONFIG QUIET)
  if(GTest_FOUND)
    enable_testing()
    add_subdirectory(tests)
  else()
    message(STATUS "GoogleTest not found; skipping unit tests")
  endif()
endif()

if(APPLE)
  set_target_properties(Bird PROPERTIES
    BUILD_WITH_INSTALL_RPATH TRUE
    INSTALL_RPATH "@executable_path;@executable_path/../lib"
  )
else()
  set_target_properties(Bird PROPERTIES
    BUILD_WITH_INSTALL_RPATH TRUE
    INSTALL_RPATH "$ORIGIN:$ORIGIN/../lib"
  )
endif()

install(
  DIRECTORY src/
  DESTINATION include
  FILES_MATCHING
  PATTERN "*.h"
  PATTERN "*.hpp")
install(
	DIRECTORY assets/shaders/
	DESTINATION assets/shaders
	FILES_MATCHING
	PATTERN "*.vert"
	PATTERN "*.frag"
	PATTERN "*.bird"
)
install(
  TARGETS Bird_core Bird BuildTools glm
  EXPORT Bird_Engine
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
  INCLUDES
  DESTINATION include)

install(
  EXPORT Bird_Engine
  FILE Bird_EngineConfig.cmake
  NAMESPACE Bird::
  DESTINATION lib/cmake/Bird_Engine)
install(DIRECTORY libs/install/include/glm DESTINATION include)
