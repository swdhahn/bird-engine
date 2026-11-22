# All Recast/Detour-dependent code lives in this object library.
# Public headers stay Recast/Detour-free (see NavInternal.h).
add_library(
  navigation OBJECT
  NavTypes.h
  NavInternal.h
  NavMesh.h
  NavMesh.cpp
  NavServer.h
  NavServer.cpp
  NavWorld.h
  NavWorld.cpp
  NavAgent.h
  NavAgent.cpp)

target_link_libraries(
  navigation
  PRIVATE glm
          RecastNavigation::Recast
          RecastNavigation::Detour
          RecastNavigation::DetourCrowd)
set_target_properties(navigation PROPERTIES POSITION_INDEPENDENT_CODE TRUE)
if(MSVC)
  set_target_properties(navigation PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS TRUE)
endif()
