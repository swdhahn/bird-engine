add_library(
  scene STATIC
  Scene.cpp
  Scene.h
  WorldObject.cpp
  WorldObject.h
  Entity.cpp
  Entity.h
  Camera.cpp
  Camera.h
  Light.cpp
  Light.h
  DefaultCamera.cpp
  DefaultCamera.h)

add_subdirectory(components)

target_link_libraries(scene PRIVATE glm)
set_target_properties(scene PROPERTIES POSITION_INDEPENDENT_CODE TRUE)
