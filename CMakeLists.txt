cmake_minimum_required(VERSION 3.23)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(Bird)

set(BIRD_FILES
    src/util/stb_image.h
    src/util/Constants.h
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
add_library(Bird_core SHARED ${BIRD_FILES} 
    $<TARGET_OBJECTS:renders>
    $<TARGET_OBJECTS:opengl-renderer>
    $<TARGET_OBJECTS:scene>)
add_executable(Bird src/main.cpp)
target_link_libraries(Bird Bird_core)

target_link_options(
  Bird_core PRIVATE
  # Only apply -Bsymbolic on Linux and Android (ELF systems)
  $<$<PLATFORM_ID:Linux,Android>:-Wl,-Bsymbolic>)
set_target_properties(Bird_core PROPERTIES POSITION_INDEPENDENT_CODE TRUE)

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
elseif(UNIX AND NOT APPLE)
  find_package(X11 REQUIRED)
  target_link_libraries(
    Bird_core
    PUBLIC -lX11
           -lXxf86vm
           -lXrandr
           -lpthread
           -lXi
           -ldl
           -lXinerama
           -lXcursor
           -lGL)
endif()

set(INSTALLED_LIB_PATH "${CMAKE_CURRENT_SOURCE_DIR}/libs/install")
set(CMAKE_PREFIX_PATH "${INSTALLED_LIB_PATH}")

find_package(OpenGL REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(assimp CONFIG REQUIRED)
find_package(glew CONFIG REQUIRED)
find_package(spirv_cross_core CONFIG REQUIRED)
find_package(spirv_cross_glsl CONFIG REQUIRED)
find_package(spirv_cross_cpp CONFIG REQUIRED)
find_package(SPIRV-Tools CONFIG REQUIRED)
find_package(Jolt CONFIG REQUIRED)

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
    set(LIB_PREFIX "")      # MSVC often outputs "volk.lib"
    set(BUILD_SUBDIR "/$<CONFIG>") # Handles /Debug or /Release subfolders
    message(STATUS "Configuring for MSVC (Windows)")
else()
    # Linux / Mac / Unix / MinGW
    set(LIB_EXT ".a")
    set(LIB_PREFIX "lib")
    set(BUILD_SUBDIR "")
endif()

add_library(shaderc::shaderc_combined STATIC IMPORTED)
set_target_properties(
  shaderc::shaderc_combined
  PROPERTIES IMPORTED_LOCATION
             "${INSTALLED_LIB_PATH}/lib/${LIB_PREFIX}shaderc_combined${LIB_EXT}")
set_target_properties(
  shaderc::shaderc_combined PROPERTIES INTERFACE_INCLUDE_DIRECTORIES
                                       "${INSTALLED_LIB_PATH}/include")
add_library(VOLK::volk STATIC IMPORTED)
set_target_properties(
  VOLK::volk PROPERTIES IMPORTED_LOCATION "${INSTALLED_LIB_PATH}/lib/${LIB_PREFIX}volk${LIB_EXT}")
set_target_properties(VOLK::volk PROPERTIES INTERFACE_INCLUDE_DIRECTORIES
                                            "${INSTALLED_LIB_PATH}/include")
message(STATUS "${CMAKE_CURRENT_SOURCE_DIR}")
add_library(glm INTERFACE)
target_include_directories(
  glm INTERFACE $<BUILD_INTERFACE:${INSTALLED_LIB_PATH}/include>
                $<INSTALL_INTERFACE:include>)
add_subdirectory(src/renders)
include_directories(src/renders)
add_subdirectory(src/scene)
include_directories(src/scene)

target_link_libraries(
  Bird_core
  PUBLIC  glm 
  PRIVATE glfw
          libglew_static
          assimp::assimp
          VOLK::volk
          Jolt::Jolt
          shaderc::shaderc_combined
          ${SPIRV_LIBRARIES}
          spirv-cross-core
          spirv-cross-glsl
          spirv-cross-cpp)

target_compile_definitions(Bird_core PRIVATE BIRD_CORE_EXPORTS)
if(NOT MSVC)
  add_definitions(-Wno-deprecated-volatile)
endif()

# Build Tools
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
add_subdirectory(src/build-tools)

install(
  DIRECTORY src/
  DESTINATION include
  FILES_MATCHING
  PATTERN "*.h"
  PATTERN "*.hpp")
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
  DESTINATION cmake/Bird_Engine)
install(DIRECTORY libs/install/include/glm DESTINATION include)
