add_library(
  renders OBJECT
  GraphicsPipeline.cpp
  GraphicsPipeline.h
  Window.cpp
  Window.h
  GraphicsConstants.h
  generic/Texture.cpp
  generic/Texture.h
  generic/Buffer.h
  generic/RawBuffer.h
  generic/RawBuffer.cpp
  generic/Mesh.cpp
  generic/Mesh.h
  generic/Material.cpp
  generic/Material.h
  generic/Shader.cpp
  generic/Shader.h
  generic/FrameBuffer.h
  generic/FrameBuffer.cpp)

# add_subdirectory(vulkan)

add_subdirectory(opengl)

target_link_libraries(opengl-renderer PRIVATE glm libglew_static)
# target_link_libraries(vulkan-renderer PRIVATE glm VOLK::volk)
target_link_libraries(
  renders
  PRIVATE 
      glm shaderc::shaderc_combined
      libglew_static 
      ${SPIRV_LIBRARIES} spirv-cross-core
      spirv-cross-glsl spirv-cross-cpp 
      glfw opengl-renderer
)

    
    #$<TARGET_OBJECTS:opengl-renderer> # $<TARGET_OBJECTS:vulkan-renderer> vulkan-renderer
    #PUBLIC libglew_static VOLK::volk)


target_include_directories(renders PRIVATE opengl) # vulkan)
set_target_properties(renders PROPERTIES POSITION_INDEPENDENT_CODE TRUE)

if(MSVC)
    set_target_properties(renders PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS TRUE)
endif()
