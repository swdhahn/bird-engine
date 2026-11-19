# All Jolt-dependent code lives in this object library, compiled with Jolt's
# interface definitions. Public headers stay Jolt-free (see PhysicsInternal.h).
add_library(
  physics OBJECT
  PhysicsLayers.h
  ContactEvent.h
  FixedStepClock.h
  PhysicsInternal.h
  CollisionShape.h
  CollisionShape.cpp
  PhysicsServer.h
  PhysicsServer.cpp
  PhysicsWorld.h
  PhysicsWorld.cpp
  RigidBodyComponent.h
  RigidBodyComponent.cpp)

target_link_libraries(physics PRIVATE glm Jolt::Jolt)
set_target_properties(physics PROPERTIES POSITION_INDEPENDENT_CODE TRUE)
if(MSVC)
  set_target_properties(physics PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS TRUE)
endif()
