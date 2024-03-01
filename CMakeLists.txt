cmake_minimum_required(VERSION 3.23)

set(CMAKE_CXX_STANDARD 17)

project(Bird)

add_custom_target(GenerateDependencies COMMAND
        cd ${CMAKE_SOURCE_DIR}/libs/bin && ${CMAKE_COMMAND} -G ${CMAKE_GENERATOR} .. -DCMAKE_BUILD_TYPE=Release "-DCMAKE_MAKE_PROGRAM=${CMAKE_MAKE_PROGRAM}" -G ${CMAKE_GENERATOR} && ${CMAKE_COMMAND} --build .
        VERBATIM)

link_directories(libs/GLFW/lib libs/ASSIMP/lib libs/GLEW/lib libs/SHADERC/lib libs/SPIRV_CROSS/lib)

# We have to get around VOLK doesn't exist at startup
set(VOLK_SOURCE_FILE_PATH "")
set(VOLK_LIB )
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/libs/VOLK/src/VOLK")
    set(VOLK_SOURCE_FILE_PATH "libs/VOLK/src/VOLK/volk.c")
    set(VOLK_LIB volk::volk)
    add_subdirectory(libs/VOLK/src/VOLK)
else()
    message(STATUS "VOLK directory not found, skipping.")
endif()

add_executable(Bird src/main.cpp ${VOLK_SOURCE_FILE_PATH} src/ApplicationVK.cpp src/ApplicationVK.h src/util/stb_image.h src/util/Constants.h src/util/Math.h src/util/Math.cpp src/util/File.cpp src/util/File.h src/util/Input.cpp src/util/Input.h src/Application.cpp src/Application.h src/util/Assets.cpp src/util/Assets.h)
add_dependencies(Bird GenerateDependencies)

if(APPLE)
    target_link_libraries(Bird "-framework Cocoa" "-framework IOKit" "-framework CoreVideo" "-framework OpenGL" "-framework AppKit" "-framework Metal" "-framework QuartzCore")
elseif(WIN32)
    target_link_libraries(Bird -luser32 -lkernel32 -lopengl32)
elseif(UNIX AND NOT APPLE)
    target_link_libraries(Bird -lX11 -lXxf86vm -lXrandr -lpthread -lXi -ldl -lXinerama -lXcursor -lGL)
endif()



include_directories(
        libs/GLFW/include libs/ASSIMP/include libs/GLEW/include
        libs/GLM/src/GLM libs/VULKAN/src/VULKAN/include libs/VOLK/src/VOLK
        libs/SHADERC/include libs/SPIRV_CROSS/include
)
add_subdirectory(src/renders)
include_directories(src/renders)
target_link_libraries(Bird renders)
add_subdirectory(src/scene)
include_directories(src/scene)
target_link_libraries(Bird scene)

target_link_libraries(Bird stdc++ -lglfw3 -lassimp -lglew ${VOLK_LIB} -lshaderc_combined -lspirv-cross-core -lspirv-cross-glsl)
add_definitions(-Wno-deprecated-volatile)

# Build Tools
set (CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
add_subdirectory(src/build-tools)