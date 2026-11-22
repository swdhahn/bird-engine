# Isolated audio subsystem built on single-header miniaudio.
# All miniaudio types and includes stay private to this module (AudioInternal.h
# and MiniaudioImpl.cpp); public headers remain completely backend-free.
add_library(
  audio OBJECT
  AudioTypes.h
  AudioInternal.h
  AudioSystem.h
  AudioSystem.cpp
  AudioSourceComponent.h
  AudioSourceComponent.cpp
  AudioListenerComponent.h
  AudioListenerComponent.cpp
  MiniaudioImpl.cpp)

target_link_libraries(audio PRIVATE glm)
set_target_properties(audio PROPERTIES POSITION_INDEPENDENT_CODE TRUE)
if(MSVC)
  set_target_properties(audio PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS TRUE)
endif()
