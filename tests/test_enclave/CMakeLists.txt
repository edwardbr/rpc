cmake_minimum_required(VERSION 3.24)

message("marshal_test_enclave")

# ######################################################################################################################
# marshal_test_enclave

if(${BUILD_ENCLAVE})

  if(USE_RPC_TELEMETRY)
    set(TELEMETRY_ENCLAVE_EDL rpc_telemetry_edl_header_enclave)
    set(RPC_TELEMETRY_ENCLAVE rpc_telemetry_enclave)
  endif()

  add_enclave_library(
    marshal_test_enclave
    SRCS marshal_test_enclave.cpp
    HEADER_ONLY_LIBS
    TRUSTED_LIBS
      rpc_enclave
      marshal_test_edl_enclave
      ${RPC_TELEMETRY_ENCLAVE}
      ${TELEMETRY_ENCLAVE_EDL}
      common_enclave
      example_import_idl_enclave
      example_idl_enclave
      example_shared_idl_enclave
      yas_common
      ${SGX_ENCLAVE_LIBS})

  target_compile_definitions(marshal_test_enclave PRIVATE ${ENCLAVE_DEFINES})
  target_include_directories(
    marshal_test_enclave
    PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
           "$<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/generated/include>"
           "$<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/generated/src>" "$<INSTALL_INTERFACE:include/marshalled_tests>"
    PRIVATE ${ENCLAVE_LIBCXX_INCLUDES})

  if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    set(EXTRA_MARSHAL_TEST_ENCLAVE_COMPILE_OPTIONS -Wno-unused-parameter)
  endif()

  target_compile_options(marshal_test_enclave PRIVATE ${ENCLAVE_COMPILE_OPTIONS} ${WARN_PEDANTIC} ${EXTRA_MARSHAL_TEST_ENCLAVE_COMPILE_OPTIONS})
  target_link_options(marshal_test_enclave PRIVATE ${ENCLAVE_LINK_OPTIONS})

  set_property(TARGET marshal_test_enclave PROPERTY COMPILE_PDB_NAME marshal_test_enclave)
  if(ENABLE_CLANG_TIDY)
    set_target_properties(marshal_test_enclave PROPERTIES CXX_CLANG_TIDY "${CLANG_TIDY_COMMAND}")
  endif()
  enclave_sign(
    marshal_test_enclave
    KEY marshal_test_enclave_private.pem
    CONFIG marshal_test_enclave.config.${SGX_MODE}.xml)
endif()
