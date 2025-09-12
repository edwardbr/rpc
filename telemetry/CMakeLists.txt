# Copyright 2024 Secretarium Ltd <contact@secretarium.org>

cmake_minimum_required(VERSION 3.24)

if(USE_RPC_TELEMETRY)
  message("rpc_telemetry_interface")
  add_library(rpc_telemetry_interface INTERFACE)
  target_include_directories(rpc_telemetry_interface INTERFACE "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>")

  add_library(rpc::rpc_telemetry_interface ALIAS rpc_telemetry_interface)

  if(BUILD_ENCLAVE)
    message("rpc_telemetry_enclave")
    add_library(rpc_telemetry_enclave STATIC src/enclave_telemetry_service.cpp)

    target_compile_definitions(rpc_telemetry_enclave PRIVATE ${ENCLAVE_DEFINES})
    target_include_directories(
      rpc_telemetry_enclave
      PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
      PRIVATE ${ENCLAVE_LIBCXX_INCLUDES})
    target_compile_options(rpc_telemetry_enclave PRIVATE ${ENCLAVE_COMPILE_OPTIONS} ${WARN_OK})
    target_link_directories(rpc_telemetry_enclave PUBLIC ${SGX_LIBRARY_PATH})

    if(ENABLE_CLANG_TIDY)
      set_target_properties(rpc_telemetry_enclave PROPERTIES CXX_CLANG_TIDY "${CLANG_TIDY_COMMAND}")
    endif()

    target_link_libraries(rpc_telemetry_enclave PRIVATE rpc_telemetry_edl_header_enclave yas_common rpc::rpc_enclave ${RPC_ENCLAVE_FMT_LIB})

    set_property(TARGET rpc_telemetry_enclave PROPERTY COMPILE_PDB_NAME rpc_telemetry_enclave)

    add_library(rpc::rpc_telemetry_enclave ALIAS rpc_telemetry_enclave)
  endif()

endif()
if(${BUILD_HOST})

  message("rpc_telemetry_host")
  add_library(rpc_telemetry_host STATIC src/sequence_diagram_telemetry_service.cpp src/console_telemetry_service.cpp src/telemetry_handler.cpp src/telemetry_service_factory.cpp)

  target_include_directories(rpc_telemetry_host PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>")
  target_compile_options(rpc_telemetry_host PRIVATE ${HOST_COMPILE_OPTIONS} ${WARN_OK})
  target_link_options(rpc_telemetry_host PRIVATE ${HOST_LINK_OPTIONS})
  target_link_directories(rpc_telemetry_host PUBLIC ${SGX_LIBRARY_PATH})
  target_compile_definitions(rpc_telemetry_host PRIVATE ${HOST_DEFINES})
  target_link_libraries(rpc_telemetry_host PRIVATE rpc::rpc_host fmt::fmt-header-only yas_common spdlog::spdlog ${RPC_HOST_FMT_LIB})

  add_library(rpc::rpc_telemetry_host ALIAS rpc_telemetry_host)
  set_property(TARGET rpc_telemetry_host PROPERTY COMPILE_PDB_NAME rpc_telemetry_host)
endif()

install(
  DIRECTORY include/
  DESTINATION include/rpc
  FILES_MATCHING
  PATTERN "*.h*")
