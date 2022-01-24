cmake_minimum_required(VERSION 3.0.0)

add_subdirectory(fmt)
add_subdirectory(clipp)
add_subdirectory(googletest)

add_library(yas_common INTERFACE)

target_include_directories(yas_common 
  INTERFACE
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/yas/include>"
  )

  
add_subdirectory(idlparser)