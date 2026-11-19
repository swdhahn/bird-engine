add_library(
  scene OBJECT
  Object.h
  Scene.cpp
  Scene.h
  WorldObject.cpp
  WorldObject.h
  Camera.cpp
  Camera.h
  Light.cpp
  Light.h
  DefaultCamera.cpp
  DefaultCamera.h)

add_subdirectory(components)
add_subdirectory(2D)

target_link_libraries(scene PRIVATE glm)
set_target_properties(scene PROPERTIES POSITION_INDEPENDENT_CODE TRUE)
if(MSVC)
    set_target_properties(scene PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS TRUE)
endif()
