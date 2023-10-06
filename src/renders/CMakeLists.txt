
add_library(renders STATIC GraphicsPipeline.cpp GraphicsPipeline.h Window.cpp Window.h GraphicsConstants.h generic/Texture.cpp generic/Texture.h generic/Buffer.cpp generic/Buffer.h generic/Mesh.cpp generic/Mesh.h generic/Material.cpp generic/Material.h generic/Shader.cpp generic/Shader.h)

add_subdirectory(vulkan)
include_directories(vulkan)

add_subdirectory(opengl)
include_directories(opengl)
target_link_libraries(renders opengl-renderer vulkan-renderer)