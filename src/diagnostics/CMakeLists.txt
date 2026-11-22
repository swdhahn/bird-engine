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
  DiagnosticsCApi.h
  DiagnosticsCApi.cpp)

target_include_directories(diagnostics PRIVATE ${CMAKE_SOURCE_DIR}/src)
set_target_properties(diagnostics PROPERTIES POSITION_INDEPENDENT_CODE TRUE)
if(MSVC)
  set_target_properties(diagnostics PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS TRUE)
endif()
