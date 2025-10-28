#[[
   Copyright (c) 2025 Edward Boggis-Rolfe
   All rights reserved.
]]

cmake_minimum_required(VERSION 3.24)

# Enclave version
if(BUILD_ENCLAVE)
  add_library(
    rpc_local_transport_enclave
    transport.cpp
  )

  target_compile_definitions(rpc_local_transport_enclave PRIVATE ${ENCLAVE_DEFINES})
  target_compile_options(rpc_local_transport_enclave PRIVATE ${ENCLAVE_COMPILE_OPTIONS} ${WARN_OK})

  target_include_directories(
    rpc_local_transport_enclave
    PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../../include>"
    PRIVATE ${ENCLAVE_LIBCXX_INCLUDES}
  )

  target_link_libraries(
    rpc_local_transport_enclave
    PRIVATE yas_common rpc_types_idl_enclave
  )

  target_link_options(rpc_local_transport_enclave PRIVATE ${ENCLAVE_LINK_OPTIONS})

  set_property(TARGET rpc_local_transport_enclave PROPERTY COMPILE_PDB_NAME rpc_local_transport_enclave)

  if(ENABLE_CLANG_TIDY)
    set_target_properties(rpc_local_transport_enclave PROPERTIES CXX_CLANG_TIDY "${CLANG_TIDY_COMMAND}")
  endif()

  add_library(rpc::rpc_local_transport_enclave ALIAS rpc_local_transport_enclave)
endif()

# Host version
add_library(
  rpc_local_transport_host
  transport.cpp
)

target_compile_definitions(rpc_local_transport_host PRIVATE ${HOST_DEFINES})

target_include_directories(
  rpc_local_transport_host
  PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../../include>"
  PRIVATE ${HOST_INCLUDES}
)

if(BUILD_COROUTINE)
  set(CORO_RUNTIME libcoro)
endif()

target_link_libraries(
  rpc_local_transport_host
  PRIVATE yas_common rpc_types_idl_host ${HOST_LIBRARIES}
)

target_compile_options(rpc_local_transport_host PRIVATE ${HOST_COMPILE_OPTIONS} ${WARN_OK})
target_link_options(rpc_local_transport_host PRIVATE ${HOST_LINK_EXE_OPTIONS})
target_link_directories(rpc_local_transport_host PUBLIC ${SGX_LIBRARY_PATH})
set_property(TARGET rpc_local_transport_host PROPERTY COMPILE_PDB_NAME rpc_local_transport_host)

if(ENABLE_CLANG_TIDY)
  set_target_properties(rpc_local_transport_host PROPERTIES CXX_CLANG_TIDY "${CLANG_TIDY_COMMAND}")
endif()

add_library(rpc::rpc_local_transport_host ALIAS rpc_local_transport_host)
