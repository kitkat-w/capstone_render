#----------------------------------------------------------------
# Generated CMake target import file for configuration "Debug".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "PNG::png_static" for configuration "Debug"
set_property(TARGET PNG::png_static APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
set_target_properties(PNG::png_static PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_DEBUG "C"
  IMPORTED_LOCATION_DEBUG "${_IMPORT_PREFIX}/debug/lib/libpng16d.a"
  )

list(APPEND _cmake_import_check_targets PNG::png_static )
list(APPEND _cmake_import_check_files_for_PNG::png_static "${_IMPORT_PREFIX}/debug/lib/libpng16d.a" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
