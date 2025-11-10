add_library(
  opengl-renderer OBJECT
  GLPipeline.cpp
  GLPipeline.h
  GLTexture.cpp
  GLTexture.h
  GLBuffer.cpp
  GLBuffer.h
  GLMesh.cpp
  GLMesh.h
  GLShader.cpp
  GLShader.h)
set_target_properties(opengl-renderer PROPERTIES POSITION_INDEPENDENT_CODE TRUE)
