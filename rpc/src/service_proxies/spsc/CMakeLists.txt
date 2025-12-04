#[[
   Copyright (c) 2025 Edward Boggis-Rolfe
   All rights reserved.
]]

cmake_minimum_required(VERSION 3.24)

# SPSC transport is host-only and requires coroutines
if(BUILD_COROUTINE)
  add_library(
    rpc_spsc_transport_host
    transport.cpp
  )

  target_compile_definitions(rpc_spsc_transport_host PRIVATE ${HOST_DEFINES})

  target_include_directories(
    rpc_spsc_transport_host
    PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../../include>"
    PRIVATE ${HOST_INCLUDES}
  )

  target_link_libraries(
    rpc_spsc_transport_host
    PRIVATE yas_common rpc_types_idl_host libcoro ${HOST_LIBRARIES}
  )

  target_compile_options(rpc_spsc_transport_host PRIVATE ${HOST_COMPILE_OPTIONS} ${WARN_OK})
  target_link_options(rpc_spsc_transport_host PRIVATE ${HOST_LINK_EXE_OPTIONS})
  set_property(TARGET rpc_spsc_transport_host PROPERTY COMPILE_PDB_NAME rpc_spsc_transport_host)

  if(ENABLE_CLANG_TIDY)
    set_target_properties(rpc_spsc_transport_host PROPERTIES CXX_CLANG_TIDY "${CLANG_TIDY_COMMAND}")
  endif()

  add_library(rpc::rpc_spsc_transport_host ALIAS rpc_spsc_transport_host)
endif()
