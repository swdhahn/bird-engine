cmake_minimum_required(VERSION 3.23)

set(CMAKE_CXX_STANDARD 20)
find_package(Vulkan REQUIRED)

project(Bird)

add_custom_target(GenerateDependencies COMMAND
        cd ${CMAKE_SOURCE_DIR}/libs/bin && ${CMAKE_COMMAND} -G ${CMAKE_GENERATOR} .. -DCMAKE_BUILD_TYPE=Release "-DCMAKE_MAKE_PROGRAM=${CMAKE_MAKE_PROGRAM}" -G ${CMAKE_GENERATOR} && ${CMAKE_COMMAND} --build .
        VERBATIM)

# Compile shaders!
file(GLOB_RECURSE shader_sources CONFIGURE_DEPENDS "shaders/*.vert" "shaders/*.frag")
set(shader_compilation_cmd "cd" "${CMAKE_BINARY_DIR}")
foreach(file_path ${shader_sources})
    string(REPLACE "${CMAKE_SOURCE_DIR}" "" file_path_new ${file_path})
    get_filename_component(new_file_directory "${CMAKE_BINARY_DIR}${file_path_new}" DIRECTORY)
    message(${new_file_directory})
    set(shader_compilation_cmd ${shader_compilation_cmd} "&&" "mkdir" "-p" "${new_file_directory}" "&&" "${Vulkan_GLSLC_EXECUTABLE}" "${file_path}" "-o" "${CMAKE_BINARY_DIR}${file_path_new}.spv")
endforeach()

add_custom_target(CompileShaders COMMAND
        ${shader_compilation_cmd}
        VERBATIM)

link_directories(libs/GLFW/lib libs/ASSIMP/lib libs/GLEW/lib)

add_executable(Bird src/main.cpp src/Application.cpp src/Application.h src/util/stb_image.h src/util/Constants.h src/util/Math.h src/util/Math.cpp src/util/File.cpp src/util/File.h)
add_dependencies(Bird CompileShaders GenerateDependencies)

if(APPLE)
    target_link_libraries(Bird "-framework Cocoa" "-framework IOKit" "-framework CoreVideo" "-framework OpenGL" "-framework AppKit" "-framework Metal" "-framework QuartzCore")
elseif(WIN32)
    target_link_libraries(Bird -luser32 -lkernel32 -lopengl32)
elseif(UNIX AND NOT APPLE)
    target_link_libraries(Bird -lX11 -lXxf86vm -lXrandr -lpthread -lXi -ldl -lXinerama -lXcursor -lGL)
endif()

include_directories(
        libs/GLFW/include libs/ASSIMP/include libs/GLEW/include
        libs/GLM/src/GLM ${Vulkan_INCLUDE_DIRS}
)
add_subdirectory(src/renders)
include_directories(src/renders)
target_link_libraries(Bird renders)
add_subdirectory(src/scene)
include_directories(src/scene)
target_link_libraries(Bird scene)

target_link_libraries(Bird stdc++ -lglfw3 -lassimp -lglew Vulkan::Vulkan)
add_definitions(-Wno-deprecated-volatile)


