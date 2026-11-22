# Isolated asset hot-reloading module.
# The file watcher, async BuildTools runner, and reload manager are only
# compiled when BIRD_ENABLE_HOT_RELOAD is ON (disabled in Release/shipping and
# device builds).
set(BIRD_HOTRELOAD_SOURCES
    HotReloadTypes.h
    ReloadQueue.h)

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
