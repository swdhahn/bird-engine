cmake_minimum_required(VERSION 3.23)

project(GenerateLibraries)
set(CMAKE_CXX_STANDARD 20)
find_package(Vulkan REQUIRED)

cmake_policy(SET CMP0135 NEW)

if(NOT ${Vulkan_FOUND})
    message(FATAL_ERROR "Please make sure the Vulkan SDK is setup properly.")
endif()

include(ExternalProject)
ExternalProject_Add(GLM
        URL "https://github.com/g-truc/glm/archive/refs/tags/0.9.9.8.tar.gz"
        PREFIX ${CMAKE_CURRENT_SOURCE_DIR}/GLM
        BUILD_COMMAND ""
        INSTALL_COMMAND ""
        )
ExternalProject_Add(GLFW
        URL "https://github.com/glfw/glfw/archive/refs/tags/3.3.8.tar.gz"
        PREFIX ${CMAKE_CURRENT_SOURCE_DIR}/GLFW
        CMAKE_ARGS
                -DGLFW_BUILD_EXAMPLES=OFF
                -DGLFW_BUILD_DOCS=OFF
                -DGLFW_BUILD_TESTS=OFF
                "-DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>"
        )
ExternalProject_Add(GLEW
        URL "https://github.com/Perlmint/glew-cmake/archive/refs/tags/glew-cmake-2.2.0.tar.gz"
        PREFIX ${CMAKE_CURRENT_SOURCE_DIR}/GLEW
        CMAKE_ARGS
            -DONLY_LIBS=ON
            "-DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>"
        )
ExternalProject_Add(ASSIMP
        URL "https://github.com/assimp/assimp/archive/refs/tags/v5.2.5.tar.gz"
        PREFIX ${CMAKE_CURRENT_SOURCE_DIR}/ASSIMP
        CMAKE_ARGS
                -DASSIMP_BUILD_TESTS=OFF
                "-DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>"
        )