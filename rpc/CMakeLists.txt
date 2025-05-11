#[[
   Copyright (c) 2024 Edward Boggis-Rolfe
   All rights reserved.
]]

cmake_minimum_required(VERSION 3.24)

if(USE_RPC_TELEMETRY)
  set(ENCLAVE_TELEMETRY_SERVICE rpc::rpc_telemetry_enclave)
  set(HOST_TELEMETRY_SERVICE rpc::rpc_telemetry_host)
  set(TELEMETRY_INTERFACE rpc::rpc_telemetry_interface)
else()
  set(ENCLAVE_TELEMETRY_SERVICE)
  set(HOST_TELEMETRY_SERVICE)
  set(TELEMETRY_INTERFACE)
endif()

if(BUILD_ENCLAVE)
  add_library(
    rpc_enclave
    include/rpc/internal/version.h
    include/rpc/internal/assert.h
    include/rpc/internal/error_codes.h
    include/rpc/internal/types.h
    include/rpc/internal/logger.h
    include/rpc/internal/type_id.h
    include/rpc/internal/marshaller.h
    include/rpc/internal/forward_declarations.h
    include/rpc/internal/casting_interface.h
    include/rpc/internal/proxy_base.h
    include/rpc/internal/object_proxy.h
    include/rpc/internal/service_proxy.h
    include/rpc/internal/proxy_impl.h
    include/rpc/internal/smart_pointers.h
    include/rpc/internal/service.h
    include/rpc/internal/stub.h
    include/rpc/internal/proxy.h
    include/rpc/internal/serialiser.h
    include/rpc/service_proxies/basic_service_proxies.h
    src/object_proxy.cpp
    src/service_proxy.cpp
    src/casting_interface.cpp
    src/service.cpp
    src/stub.cpp
    src/error_codes.cpp
    src/version.cpp)

  target_compile_definitions(rpc_enclave PRIVATE ${ENCLAVE_DEFINES})
  target_compile_options(rpc_enclave PRIVATE ${ENCLAVE_COMPILE_OPTIONS} ${WARN_OK})
  target_include_directories(
    rpc_enclave
    PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>" "$<INSTALL_INTERFACE:include/rpc>"
    PRIVATE ${ENCLAVE_LIBCXX_INCLUDES})
  target_link_libraries(
    rpc_enclave
    PUBLIC ${TELEMETRY_INTERFACE} ${ENCLAVE_TELEMETRY_SERVICE}
    PRIVATE yas_common)
  target_link_options(rpc_enclave PRIVATE ${ENCLAVE_LINK_OPTIONS})

  set_property(TARGET rpc_enclave PROPERTY COMPILE_PDB_NAME rpc_enclave)
  if(ENABLE_CLANG_TIDY)
    set_target_properties(rpc_enclave PROPERTIES CXX_CLANG_TIDY "${CLANG_TIDY_COMMAND}")
  endif()
  add_library(rpc::rpc_enclave ALIAS rpc_enclave)
endif()

# ######################################################################################################################
add_library(
  rpc_host
  include/rpc/internal/version.h
  include/rpc/internal/assert.h
  include/rpc/internal/error_codes.h
  include/rpc/internal/types.h
  include/rpc/internal/logger.h
  include/rpc/internal/type_id.h
  include/rpc/internal/marshaller.h
  include/rpc/internal/forward_declarations.h
  include/rpc/internal/casting_interface.h
  include/rpc/internal/proxy_base.h
  include/rpc/internal/object_proxy.h
  include/rpc/internal/service_proxy.h
  include/rpc/internal/proxy_impl.h
  include/rpc/internal/smart_pointers.h
  include/rpc/internal/service.h
  include/rpc/internal/stub.h
  include/rpc/internal/proxy.h
  include/rpc/internal/serialiser.h
  include/rpc/service_proxies/basic_service_proxies.h
  src/object_proxy.cpp
  src/service_proxy.cpp
  src/casting_interface.cpp
  src/service.cpp
  src/stub.cpp
  src/error_codes.cpp
  src/version.cpp)

target_compile_definitions(rpc_host PRIVATE ${HOST_DEFINES})

target_include_directories(
  rpc_host
  PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>" "$<INSTALL_INTERFACE:include/rpc>"
  PRIVATE ${HOST_INCLUDES})

target_link_libraries(
  rpc_host
  PUBLIC ${TELEMETRY_INTERFACE} ${HOST_TELEMETRY_SERVICE}
  PRIVATE yas_common ${HOST_LIBRARIES})

target_compile_options(rpc_host PRIVATE ${HOST_COMPILE_OPTIONS} ${WARN_OK})
target_link_options(rpc_host PRIVATE ${HOST_LINK_EXE_OPTIONS})
target_link_directories(rpc_host PUBLIC ${SGX_LIBRARY_PATH})
set_property(TARGET rpc_host PROPERTY COMPILE_PDB_NAME rpc_host)

if(ENABLE_CLANG_TIDY)
  set_target_properties(rpc_host PROPERTIES CXX_CLANG_TIDY "${CLANG_TIDY_COMMAND}")
endif()

add_library(rpc::rpc_host ALIAS rpc_host)

install(
  DIRECTORY include/
  DESTINATION include/rpc
  FILES_MATCHING
  PATTERN "*.h*"
  # PATTERN "private" EXCLUDE
)
