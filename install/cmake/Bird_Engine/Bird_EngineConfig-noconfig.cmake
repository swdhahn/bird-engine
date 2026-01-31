#----------------------------------------------------------------
# Generated CMake target import file.
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "Bird::Bird_core" for configuration ""
set_property(TARGET Bird::Bird_core APPEND PROPERTY IMPORTED_CONFIGURATIONS NOCONFIG)
set_target_properties(Bird::Bird_core PROPERTIES
  IMPORTED_LOCATION_NOCONFIG "${_IMPORT_PREFIX}/lib/libBird_core.so"
  IMPORTED_SONAME_NOCONFIG "libBird_core.so"
  )

list(APPEND _cmake_import_check_targets Bird::Bird_core )
list(APPEND _cmake_import_check_files_for_Bird::Bird_core "${_IMPORT_PREFIX}/lib/libBird_core.so" )

# Import target "Bird::Bird" for configuration ""
set_property(TARGET Bird::Bird APPEND PROPERTY IMPORTED_CONFIGURATIONS NOCONFIG)
set_target_properties(Bird::Bird PROPERTIES
  IMPORTED_LOCATION_NOCONFIG "${_IMPORT_PREFIX}/bin/Bird"
  )

list(APPEND _cmake_import_check_targets Bird::Bird )
list(APPEND _cmake_import_check_files_for_Bird::Bird "${_IMPORT_PREFIX}/bin/Bird" )

# Import target "Bird::BuildTools" for configuration ""
set_property(TARGET Bird::BuildTools APPEND PROPERTY IMPORTED_CONFIGURATIONS NOCONFIG)
set_target_properties(Bird::BuildTools PROPERTIES
  IMPORTED_LOCATION_NOCONFIG "${_IMPORT_PREFIX}/bin/BuildTools"
  )

list(APPEND _cmake_import_check_targets Bird::BuildTools )
list(APPEND _cmake_import_check_files_for_Bird::BuildTools "${_IMPORT_PREFIX}/bin/BuildTools" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
