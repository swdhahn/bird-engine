cmake_minimum_required(VERSION 3.23)

project(GenerateLibraries)
set(CMAKE_CXX_STANDARD 17)

cmake_policy(SET CMP0135 NEW)

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
        URL "https://github.com/assimp/assimp/archive/refs/tags/v5.3.1.tar.gz"
        PREFIX ${CMAKE_CURRENT_SOURCE_DIR}/ASSIMP
        CMAKE_ARGS
                -DASSIMP_BUILD_TESTS=OFF
                "-DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>"
                -Werror
                -Wdeprecated-declarations
        )
ExternalProject_Add(VOLK
        URL "https://github.com/zeux/volk/archive/refs/tags/1.3.250.tar.gz"
        PREFIX ${CMAKE_CURRENT_SOURCE_DIR}/VOLK
        BUILD_COMMAND ""
        INSTALL_COMMAND ""
        )
ExternalProject_Add(VULKAN
        URL "https://github.com/KhronosGroup/Vulkan-Headers/archive/refs/tags/v1.3.266.tar.gz"
        PREFIX ${CMAKE_CURRENT_SOURCE_DIR}/VULKAN
        BUILD_COMMAND ""
        INSTALL_COMMAND ""
        )

ExternalProject_Add(SPIRV_CROSS
        URL "https://github.com/KhronosGroup/SPIRV-Cross/archive/refs/tags/MoltenVK-1.1.5.tar.gz"
        PREFIX ${CMAKE_CURRENT_SOURCE_DIR}/SPIRV_CROSS
        CMAKE_ARGS
        "-DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>"
        )

#https://github.com/KhronosGroup/SPIRV-Cross/archive/refs/tags/MoltenVK-1.1.5.tar.gz

ExternalProject_Add(SHADERC
        URL "https://github.com/google/shaderc/archive/refs/tags/v2023.6.tar.gz"
        PREFIX ${CMAKE_CURRENT_SOURCE_DIR}/SHADERC
        CMAKE_ARGS
                "-DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>"
                -DSHADERC_SKIP_TESTS=TRUE
                -DSHADERC_SKIP_EXAMPLES=TRUE
                -SHADERC_SKIP_COPYRIGHT_CHECK=TRUE
        )

ExternalProject_Add_Step(SHADERC git-deps
        DEPENDERS build
        DEPENDEES patch
        COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/SHADERC/src/SHADERC/utils/git-sync-deps
        COMMENT "Downloading Dependencies"
)

