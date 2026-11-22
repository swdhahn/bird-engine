# Scene serialization, component reflection, prefab/archetype system, and save-game paths.
add_library(
  serialization OBJECT
  UUID.h
  UUID.cpp
  SerializedValue.h
  SerializedValue.cpp
  Json.h
  Json.cpp
  Archive.h
  JsonArchive.h
  JsonArchive.cpp
  ComponentRegistry.h
  ComponentRegistry.cpp
  DartScriptRecord.h
  DartScriptRecord.cpp
  BuiltinComponents.h
  BuiltinComponents.cpp
  PropertyOverride.h
  PropertyOverride.cpp
  PrefabInstanceComponent.h
  PrefabInstanceComponent.cpp
  Prefab.h
  Prefab.cpp
  PrefabManager.h
  PrefabManager.cpp
  SceneSerializer.h
  SceneSerializer.cpp
  SaveGame.h
  SaveGame.cpp)

target_link_libraries(serialization PRIVATE glm)
set_target_properties(serialization PROPERTIES POSITION_INDEPENDENT_CODE TRUE)
if(MSVC)
  set_target_properties(serialization PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS TRUE)
endif()
