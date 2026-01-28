cmake_minimum_required(VERSION 3.23)

project(GenerateLibraries)
set(CMAKE_CXX_STANDARD 17)

cmake_policy(SET CMP0135 NEW)

include(ExternalProject)

set(DEPS_INSTALL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/install")

ExternalProject_Add(
  GLM
  URL "https://github.com/g-truc/glm/archive/refs/tags/0.9.9.8.tar.gz"
  PREFIX ${CMAKE_CURRENT_SOURCE_DIR}/GLM
  BUILD_COMMAND ""
  INSTALL_COMMAND ${CMAKE_COMMAND} -E copy_directory <SOURCE_DIR>/glm
                  ${DEPS_INSTALL_DIR}/include/glm)
ExternalProject_Add(
  GLFW
  URL "https://github.com/glfw/glfw/archive/refs/tags/3.3.8.tar.gz"
  PREFIX ${CMAKE_CURRENT_SOURCE_DIR}/GLFW
  CMAKE_ARGS -DGLFW_BUILD_EXAMPLES=OFF -DGLFW_BUILD_DOCS=OFF
             -DGLFW_BUILD_TESTS=OFF -DCMAKE_C_FLAGS=-fblocks
             "-DCMAKE_INSTALL_PREFIX=${DEPS_INSTALL_DIR}")
ExternalProject_Add(
  GLEW
  URL "https://github.com/Perlmint/glew-cmake/archive/refs/tags/glew-cmake-2.2.0.tar.gz"
  PREFIX ${CMAKE_CURRENT_SOURCE_DIR}/GLEW
  CMAKE_ARGS -DONLY_LIBS=ON -Dglew-cmake_BUILD_SHARED=OFF "-DCMAKE_INSTALL_PREFIX=${DEPS_INSTALL_DIR}")
ExternalProject_Add(
  ASSIMP
  URL "https://github.com/assimp/assimp/archive/refs/tags/v5.3.1.tar.gz"
  PREFIX ${CMAKE_CURRENT_SOURCE_DIR}/ASSIMP
  CMAKE_ARGS -DASSIMP_BUILD_TESTS=OFF -DBUILD_SHARED_LIBS=OFF -DASSIMP_BUILD_ZLIB=OFF
             "-DCMAKE_INSTALL_PREFIX=${DEPS_INSTALL_DIR}" -Werror
             -Wdeprecated-declarations)
ExternalProject_Add(
        VULKAN
        URL "https://github.com/KhronosGroup/Vulkan-Headers/archive/refs/tags/v1.3.266.tar.gz"
        PREFIX ${CMAKE_CURRENT_SOURCE_DIR}/VULKAN
        BUILD_COMMAND ""
        INSTALL_COMMAND
        ${CMAKE_COMMAND} -E copy_directory <SOURCE_DIR>/include
        ${DEPS_INSTALL_DIR}/include)
ExternalProject_Add(
  VOLK
  URL "https://github.com/zeux/volk/archive/refs/tags/1.3.250.tar.gz"
  PREFIX ${CMAKE_CURRENT_SOURCE_DIR}/VOLK
  CMAKE_ARGS "-DCMAKE_INSTALL_PREFIX=${DEPS_INSTALL_DIR}" -DVOLK_INSTALL=ON "-DCMAKE_C_FLAGS=-I${DEPS_INSTALL_DIR}/include"
  INSTALL_COMMAND
    # Copy the static library from its build location to the install lib folder
    ${CMAKE_COMMAND} -E copy <BINARY_DIR>/libvolk.a
    ${DEPS_INSTALL_DIR}/lib/libvolk.a
    # Copy the volk.h header to the install include folder
  COMMAND ${CMAKE_COMMAND} -E copy <SOURCE_DIR>/volk.h
          ${DEPS_INSTALL_DIR}/include/volk.h)
ExternalProject_Add(
  SPIRV_CROSS
  URL "https://github.com/KhronosGroup/SPIRV-Cross/archive/refs/tags/MoltenVK-1.1.5.tar.gz"
  PREFIX ${CMAKE_CURRENT_SOURCE_DIR}/SPIRV_CROSS
  CMAKE_ARGS "-DCMAKE_INSTALL_PREFIX=${DEPS_INSTALL_DIR}"
             -DSPIRV_CROSS_ENABLE_TESTS=OFF -DSPIRV_CROSS_CLI=OFF)

# https://github.com/KhronosGroup/SPIRV-Cross/archive/refs/tags/MoltenVK-1.1.5.tar.gz

ExternalProject_Add(
  SHADERC
  URL "https://github.com/google/shaderc/archive/refs/tags/v2025.4.tar.gz"
  PREFIX ${CMAKE_CURRENT_SOURCE_DIR}/SHADERC
  CMAKE_ARGS "-DCMAKE_INSTALL_PREFIX=${DEPS_INSTALL_DIR}"
             -DSHADERC_SKIP_TESTS=TRUE
             -DSHADERC_SKIP_EXAMPLES=TRUE
             -DSHADERC_SKIP_COPYRIGHT_CHECK=TRUE
             -DSHADERC_ENABLE_GLSLANG_COMBINED=ON
             -DSHADERC_ENABLE_SPIRV_TOOLS_COMBINED=ON)

ExternalProject_Add_Step(
  SHADERC git-deps
  DEPENDERS build
  DEPENDEES patch
  COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/SHADERC/src/SHADERC/utils/git-sync-deps
  COMMENT "Downloading Dependencies")


ExternalProject_Add(
  JOLT
  URL "https://github.com/jrouwe/JoltPhysics/archive/refs/tags/v5.4.0.tar.gz"
  PREFIX ${CMAKE_CURRENT_SOURCE_DIR}/JOLT
  SOURCE_SUBDIR Build
  CMAKE_ARGS -DCROSS_PLATFORM_DETERMINISTIC=ON -DTARGET_UNIT_TESTS=OFF -DTARGET_HELLO_WORLD=OFF -DTARGET_PERFORMANCE_TEST=OFF -DTARGET_SAMPLES=OFF -DTARGET_VIEWER=OFF
             "-DCMAKE_INSTALL_PREFIX=${DEPS_INSTALL_DIR}")




if(APPLE)
    # 1. Identify the target name (Standard GLEW uses 'libglew_static' or 'glew_s')
    set(TARGET_TO_FIX "libglew_static")

    # 2. Check if target exists (safety check)
    if(TARGET ${TARGET_TO_FIX})
        # 3. Get the current list of interface libraries
        get_target_property(CURRENT_LIBS ${TARGET_TO_FIX} INTERFACE_LINK_LIBRARIES)

        if(CURRENT_LIBS)
            # 4. Remove AGL framework from the list
            list(FILTER CURRENT_LIBS EXCLUDE REGEX "AGL\\.framework")

            # 5. Write the cleaned list back to the target
            set_target_properties(${TARGET_TO_FIX} PROPERTIES
                    INTERFACE_LINK_LIBRARIES "${CURRENT_LIBS}"
            )
            message(STATUS "Fixed GLEW: Removed deprecated AGL dependency from ${TARGET_TO_FIX}")
        endif()
    endif()
endif()