# Engine-wide decoupled event and messaging system.
add_library(
  events OBJECT
  EventTypes.h
  EventPayload.h
  EventPayload.cpp
  EventBus.h
  EventBus.cpp)

target_link_libraries(events PRIVATE glm)
set_target_properties(events PROPERTIES POSITION_INDEPENDENT_CODE TRUE)
if(MSVC)
  set_target_properties(events PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS TRUE)
endif()
