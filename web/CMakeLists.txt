# Browser build of the sample game in test/: the engine (BIRD_PLATFORM
# defaults to WEB under Emscripten) and the game linked into one WebAssembly
# module, with the baked .bird assets preloaded into the virtual file system.
#
#   emcmake cmake -S web -B build-web -DCMAKE_BUILD_TYPE=Release
#   cmake --build build-web
#   python3 -m http.server -d build-web   # then open http://localhost:8000
cmake_minimum_required(VERSION 3.24)
project(BirdWeb C CXX)

if(NOT EMSCRIPTEN)
  message(FATAL_ERROR "Configure web/ with emcmake (the Emscripten toolchain)")
endif()

set(BIRD_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/..")
add_subdirectory("${BIRD_ROOT}" bird)

add_executable(bird_web
    "${BIRD_ROOT}/src/platform/web/WebMain.cpp"
    "${BIRD_ROOT}/test/main.cpp")
target_include_directories(bird_web PRIVATE "${BIRD_ROOT}/src")
# Whole archive: keep every engine object, as a shared library would.
target_link_libraries(bird_web PRIVATE "$<LINK_LIBRARY:WHOLE_ARCHIVE,Bird_core>")
set_target_properties(bird_web PROPERTIES OUTPUT_NAME index SUFFIX ".html")

# Stage the baked assets at their repo paths; the runtime reads only .bird
# files, which BuildTools bakes on a desktop.
file(GLOB_RECURSE BIRD_BAKED_ASSETS RELATIVE "${BIRD_ROOT}" CONFIGURE_DEPENDS
     "${BIRD_ROOT}/assets/*.bird")
# Not used by the sample game; keeps the download about 57 MB smaller.
list(FILTER BIRD_BAKED_ASSETS EXCLUDE REGEX "^assets/models/bugatti/")
set(BIRD_WEB_STAGE "${CMAKE_CURRENT_BINARY_DIR}/staged")
foreach(asset IN LISTS BIRD_BAKED_ASSETS)
  configure_file("${BIRD_ROOT}/${asset}" "${BIRD_WEB_STAGE}/${asset}" COPYONLY)
endforeach()

target_link_options(bird_web PRIVATE
    "SHELL:--preload-file ${BIRD_WEB_STAGE}/assets@/assets"
    "SHELL:--shell-file ${CMAKE_CURRENT_SOURCE_DIR}/shell.html"
    -sENVIRONMENT=web
    -sALLOW_MEMORY_GROWTH=1
    -sINITIAL_MEMORY=256MB
    # Scene loading and physics recurse deeper than the 64 KB default.
    -sSTACK_SIZE=4MB)
set_property(TARGET bird_web APPEND PROPERTY LINK_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/shell.html" ${BIRD_BAKED_ASSETS})
