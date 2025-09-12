#[[
   Copyright (c) 2024 Edward Boggis-Rolfe
   All rights reserved.
]]

cmake_minimum_required(VERSION 3.24)

project(
  rpc
  VERSION 2.2.0
  LANGUAGES CXX)

message("configuring RPC")

if("${CMAKE_CURRENT_SOURCE_DIR}" PATH_EQUAL "${CMAKE_SOURCE_DIR}")
  set(RPC_STANDALONE ON)
else()
  set(RPC_STANDALONE OFF)
endif()

if(RPC_STANDALONE)
  message("RPC standalone")
  set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/output)
  set(CMAKE_PDB_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/output)
  set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/output)
  set(CMAKE_COMPILE_PDB_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/output)
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/output)

  set(CMAKE_CXX_STANDARD 17)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)

  project(rpc_tests VERSION 2.2.0)

  list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmakeconfigs")
  include(DependencyPrimer)

  add_subdirectory(submodules)
endif()

if(USE_RPC_TELEMETRY)
  set(RPC_ENCLAVE_TELEMETRY rpc::rpc_telemetry_enclave)
  set(RPC_TELEMETRY_ENCLAVE_EDL rpc_telemetry_edl_header_enclave)
  set(RPC_HOST_TELEMETRY rpc::rpc_telemetry_host)
  set(RPC_TELEMETRY_INTERFACE rpc::rpc_telemetry_interface)
else()
  set(RPC_ENCLAVE_TELEMETRY)
  set(RPC_TELEMETRY_ENCLAVE_EDL)
  set(RPC_HOST_TELEMETRY)
  set(RPC_TELEMETRY_INTERFACE)
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
