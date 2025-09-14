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
add_subdirectory(fixtures)
add_subdirectory(test_host)
# this is a way to detect if we have nlohman that we can use for testing json schemas
if(DEFINED NLOHMANN_JSON_CONFIG_INSTALL_DIR)
  add_subdirectory(json_schema_test)
endif()

if(BUILD_ENCLAVE)
  add_subdirectory(test_enclave)
endif()

# Add fuzz test subdirectory
if(NOT BUILD_COROUTINE)
  add_subdirectory(fuzz_test)
endif()
