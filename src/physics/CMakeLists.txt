add_library(
  physics OBJECT JoltIncludes.h PhysicsLayers.h PhysicsPipeline.h
                 PhysicsPipeline.cpp ColliderShape.h ColliderShape.cpp)

target_link_libraries(physics PRIVATE glm Jolt::Jolt)
set_target_properties(physics PROPERTIES POSITION_INDEPENDENT_CODE TRUE)
if(MSVC)
  set_target_properties(physics PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS TRUE)
endif()
