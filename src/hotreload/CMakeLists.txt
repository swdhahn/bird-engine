# Isolated asset hot-reloading module.
# EntityHandleRegistry is always compiled so ID-based Dart FFI entity handles
# remain safe in all builds. The background FileWatcher, BuildToolsRunner, and
# HotReloadManager are compiled out when BIRD_ENABLE_HOT_RELOAD is OFF
# (Release/shipping and device builds).
set(BIRD_HOTRELOAD_SOURCES
    HotReloadTypes.h
    ReloadQueue.h
    EntityHandleRegistry.h
    EntityHandleRegistry.cpp)

if(BIRD_ENABLE_HOT_RELOAD)
  list(APPEND BIRD_HOTRELOAD_SOURCES
       FileWatcher.h
       FileWatcher.cpp
       BuildToolsRunner.h
       BuildToolsRunner.cpp
       HotReloadManager.h
       HotReloadManager.cpp)
endif()

add_library(hotreload OBJECT ${BIRD_HOTRELOAD_SOURCES})
set_target_properties(hotreload PROPERTIES POSITION_INDEPENDENT_CODE TRUE)
if(MSVC)
  set_target_properties(hotreload PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS TRUE)
endif()
