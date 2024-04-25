#[[
   Copyright (c) 2024 Edward Boggis-Rolfe
   All rights reserved.
]]

cmake_minimum_required(VERSION 3.24)

project(
  rpc
  VERSION 2.1.0
  LANGUAGES CXX)

if(RPC_STANDALONE)
  set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/output)
  set(CMAKE_PDB_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/output)
  set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/output)
  set(CMAKE_COMPILE_PDB_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/output)
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/output)

  set(CMAKE_CXX_STANDARD 17)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
  
  find_package(Git QUIET)
  if(GIT_FOUND AND EXISTS "${CMAKE_CURRENT_LIST_DIR}/.git")
  # Update submodules as needed
      option(GIT_SUBMODULE "Check submodules during build" ON)
      if(GIT_SUBMODULE)
          message(STATUS "Submodule init")
          execute_process(COMMAND ${GIT_EXECUTABLE} submodule init
                          WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                          RESULT_VARIABLE GIT_SUBMOD_INIT_RESULT)
          if(NOT GIT_SUBMOD_INIT_RESULT EQUAL "0")
              message(FATAL_ERROR "submodule init")
          endif()
          message(STATUS "Submodule update")
          execute_process(COMMAND ${GIT_EXECUTABLE} submodule update
                          WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                          RESULT_VARIABLE GIT_SUBMOD_RESULT)
          if(NOT GIT_SUBMOD_RESULT EQUAL "0")
              message(FATAL_ERROR "git submodule update --init failed with ${GIT_SUBMOD_RESULT}, please checkout submodules")
          endif()
      endif()
  endif()  

  project(enclave_marshaller VERSION 0.1.0)

  list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmakeconfigs")
  include(DependancyPrimer)

  add_subdirectory(submodules)
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")
include(FindRPCGenerator)

add_subdirectory(generator)
add_subdirectory(telemetry)
add_subdirectory(rpc)
if(${BUILD_TEST})
  add_subdirectory(tests)
endif()

install(FILES "${CMAKE_CURRENT_LIST_DIR}/cmake/FindRPCGenerator.cmake" DESTINATION ${CMAKE_INSTALL_PREFIX}/cmake/rpc)
