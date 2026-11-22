add_library(
  editor OBJECT
  UndoStack.h
  UndoStack.cpp
  EditorCamera.h
  EditorCamera.cpp)

target_link_libraries(editor PRIVATE glm)
set_target_properties(editor PROPERTIES POSITION_INDEPENDENT_CODE TRUE)
if(MSVC)
    set_target_properties(editor PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS TRUE)
endif()
