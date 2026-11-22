# Diagnostics module: structured logging, profiling, stack traces, crash
# handling, and graceful fallback asset helpers.
add_library(
  diagnostics OBJECT
  LogSinks.h
  LogSinks.cpp
  Logger.h
  Logger.cpp
  Profiler.h
  Profiler.cpp
  StackTrace.h
  StackTrace.cpp
  CrashHandler.h
  CrashHandler.cpp
  FallbackAssets.h
  FallbackAssets.cpp
  FallbackAssetsGpu.cpp
  DiagnosticsCApi.h
  DiagnosticsCApi.cpp)

target_include_directories(diagnostics PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(diagnostics PRIVATE glm)

if(BIRD_ENABLE_PROFILING)
  target_compile_definitions(diagnostics PUBLIC BIRD_ENABLE_PROFILING=1)
else()
  target_compile_definitions(diagnostics PUBLIC BIRD_ENABLE_PROFILING=0)
endif()

if(BIRD_ENABLE_TRACY)
  target_compile_definitions(diagnostics PUBLIC BIRD_ENABLE_TRACY TRACY_ENABLE)
  target_link_libraries(diagnostics PRIVATE Tracy::TracyClient)
endif()

if(BIRD_ENABLE_CPPTRACE)
  target_compile_definitions(diagnostics PRIVATE BIRD_ENABLE_CPPTRACE)
  target_link_libraries(diagnostics PRIVATE cpptrace::cpptrace)
endif()

set_target_properties(diagnostics PROPERTIES POSITION_INDEPENDENT_CODE TRUE)
if(MSVC)
  set_target_properties(diagnostics PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS TRUE)
endif()
