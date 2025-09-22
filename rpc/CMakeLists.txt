#[[
   Copyright (c) 2024 Edward Boggis-Rolfe
   All rights reserved.
]]

cmake_minimum_required(VERSION 3.24)

# if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC" OR CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  message("using legacy")
  set(REMOTE_PTR_CPP src/remote_pointer.cpp)
# else()
#   message("using clang12")
#   set(REMOTE_PTR_CPP)
# endif()

add_subdirectory(interfaces)

if(USE_RPC_TELEMETRY)
  set(RPC_ENCLAVE_TELEMETRY_SERVICE rpc::rpc_telemetry_enclave)
  set(RPC_HOST_TELEMETRY_SERVICE rpc::rpc_telemetry_host)
  set(RPC_TELEMETRY_INTERFACE rpc::rpc_telemetry_interface)
else()
  set(RPC_ENCLAVE_TELEMETRY_SERVICE)
  set(RPC_HOST_TELEMETRY_SERVICE)
  set(RPC_TELEMETRY_INTERFACE)
endif()

if(BUILD_ENCLAVE)
  add_library(
    rpc_enclave
    include/rpc/internal/version.h
    include/rpc/internal/assert.h
    include/rpc/internal/error_codes.h
    include/rpc/internal/types.h
    include/rpc/internal/logger.h
      include/rpc/internal/marshaller.h
    include/rpc/internal/casting_interface.h
    include/rpc/internal/service.h
    include/rpc/internal/stub.h
    include/rpc/internal/object_proxy.h
    include/rpc/internal/service_proxy.h
    include/rpc/internal/serialiser.h
    include/rpc/service_proxies/basic_service_proxies.h
    src/casting_interface.cpp
    src/error_codes.cpp
    src/object_proxy.cpp
    src/service_proxy.cpp
    src/service.cpp
    src/stub.cpp
    src/version.cpp
    ${REMOTE_PTR_CPP})

  target_compile_definitions(rpc_enclave PRIVATE ${ENCLAVE_DEFINES})
  target_compile_options(rpc_enclave PRIVATE ${ENCLAVE_COMPILE_OPTIONS} ${WARN_OK})
  target_include_directories(
    rpc_enclave
    PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>" "$<INSTALL_INTERFACE:include/rpc>"
    PRIVATE ${ENCLAVE_LIBCXX_INCLUDES})
  target_link_libraries(
    rpc_enclave
    PUBLIC ${RPC_TELEMETRY_INTERFACE} ${RPC_ENCLAVE_TELEMETRY_SERVICE} ${RPC_ENCLAVE_FMT_LIB} rpc_types_idl_enclave
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
  include/rpc/internal/marshaller.h
  include/rpc/internal/casting_interface.h
  include/rpc/internal/service.h
  include/rpc/internal/stub.h
  include/rpc/internal/object_proxy.h
  include/rpc/internal/service_proxy.h
  include/rpc/internal/serialiser.h
  include/rpc/service_proxies/basic_service_proxies.h
  src/casting_interface.cpp
  src/error_codes.cpp
  src/object_proxy.cpp
  src/service_proxy.cpp
  src/service.cpp
  src/stub.cpp
  src/version.cpp
  ${REMOTE_PTR_CPP})

# Conditionally add thread-local logger implementation
if(USE_THREAD_LOCAL_LOGGING)
  target_sources(rpc_host PRIVATE src/thread_local_logger.cpp)
endif()

target_compile_definitions(rpc_host PRIVATE ${HOST_DEFINES})

target_include_directories(
  rpc_host
  PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>" "$<INSTALL_INTERFACE:include/rpc>"
  PRIVATE ${HOST_INCLUDES})

if(BUILD_COROUTINE)
  set(CORO_RUNTIME libcoro)
endif()

target_link_libraries(
  rpc_host
  PUBLIC ${RPC_TELEMETRY_INTERFACE} ${RPC_HOST_TELEMETRY_SERVICE} 
  ${CORO_RUNTIME}
  rpc_types_idl_host
  PRIVATE yas_common ${HOST_LIBRARIES} ${RPC_HOST_FMT_LIB})

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
