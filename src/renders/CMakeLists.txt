add_library(
  renders STATIC
  GraphicsPipeline.cpp
  GraphicsPipeline.h
  Window.cpp
  Window.h
  GraphicsConstants.h
  generic/Texture.cpp
  generic/Texture.h
  generic/Buffer.cpp
  generic/Buffer.h
  generic/Mesh.cpp
  generic/Mesh.h
  generic/Material.cpp
  generic/Material.h
  generic/Shader.cpp
  generic/Shader.h
  generic/FrameBuffer.h
  generic/FrameBuffer.cpp)

add_subdirectory(vulkan)

add_subdirectory(opengl)

target_link_libraries(opengl-renderer PRIVATE glm GLEW::GLEW)
target_link_libraries(vulkan-renderer PRIVATE glm VOLK::volk)
target_link_libraries(
  renders
  PRIVATE glm shaderc::shaderc_combined ${SPIRV_LIBRARIES} spirv-cross-core
          spirv-cross-glsl spirv-cross-cpp
  PUBLIC $<TARGET_OBJECTS:opengl-renderer> $<TARGET_OBJECTS:vulkan-renderer>
  PUBLIC GLEW::GLEW VOLK::volk)
target_include_directories(renders PRIVATE opengl vulkan)
set_target_properties(renders PROPERTIES POSITION_INDEPENDENT_CODE TRUE)
