cmake_minimum_required(VERSION 3.24)

message("marshal_test_enclave")

# ######################################################################################################################
# marshal_test_enclave

if(${BUILD_ENCLAVE})
  add_enclave_library(
    marshal_test_enclave
    SRCS marshal_test_enclave.cpp
    HEADER_ONLY_LIBS
    TRUSTED_LIBS
      rpc_enclave
      marshal_test_edl_enclave
      rpc_telemetry_edl_header_enclave
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

  target_compile_options(marshal_test_enclave PRIVATE ${ENCLAVE_COMPILE_OPTIONS})
  target_link_options(marshal_test_enclave PRIVATE ${ENCLAVE_LINK_OPTIONS})

  set_property(TARGET marshal_test_enclave PROPERTY COMPILE_PDB_NAME marshal_test_enclave)
  if(ENABLE_CLANG_TIDY)
    set_target_properties(marshal_test_enclave PROPERTIES CXX_CLANG_TIDY "${CLANG_TIDY_COMMAND}")
  endif()
  enclave_sign(
    marshal_test_enclave
    KEY marshal_test_enclave_private.pem
    CONFIG marshal_test_enclave.config.${SGX_MODE}.xml)
  # ####################################################################################################################
  add_enclave_library(
    marshal_test_enclave_v1
    SRCS marshal_test_enclave.cpp
    HEADER_ONLY_LIBS
    TRUSTED_LIBS
      rpc_enclave_v1
      marshal_test_edl_enclave
      rpc_telemetry_edl_header_enclave
      common_enclave_v1
      example_import_idl_enclave
      example_idl_enclave
      example_shared_idl_enclave
      yas_common
      ${SGX_ENCLAVE_LIBS})

  target_compile_definitions(
    marshal_test_enclave_v1
    PUBLIC NO_RPC_V2
    PRIVATE ${ENCLAVE_DEFINES})

  target_include_directories(
    marshal_test_enclave_v1
    PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
           "$<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/generated/include>"
           "$<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/generated/src>" "$<INSTALL_INTERFACE:include/marshalled_tests>"
    PRIVATE ${ENCLAVE_LIBCXX_INCLUDES})

  target_compile_options(marshal_test_enclave_v1 PRIVATE ${ENCLAVE_COMPILE_OPTIONS})
  target_link_options(marshal_test_enclave_v1 PRIVATE ${ENCLAVE_LINK_OPTIONS})

  set_property(TARGET marshal_test_enclave_v1 PROPERTY COMPILE_PDB_NAME marshal_test_enclave_v1)
  if(ENABLE_CLANG_TIDY)
    set_target_properties(marshal_test_enclave_v1 PROPERTIES CXX_CLANG_TIDY "${CLANG_TIDY_COMMAND}")
  endif()
  enclave_sign(
    marshal_test_enclave_v1
    KEY marshal_test_enclave_private.pem
    CONFIG marshal_test_enclave.config.${SGX_MODE}.xml)

endif()
