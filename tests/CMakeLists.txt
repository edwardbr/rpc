#[[
   Copyright (c) 2024 Edward Boggis-Rolfe
   All rights reserved.
]]
cmake_minimum_required(VERSION 3.24)

if(BUILD_ENCLAVE)
  set(HOST_DEFINES ${HOST_DEFINES} BUILD_ENCLAVE)
endif()

if(BUILD_ENCLAVE)
  add_subdirectory(edl)
endif()

add_subdirectory(idls)
add_subdirectory(common)
add_subdirectory(test_host)
add_subdirectory(mcp_test)

if(BUILD_ENCLAVE)
  add_subdirectory(test_enclave)
endif()
