cmake_minimum_required(VERSION 3.12)

message("marshal_test_enclave")

# ##############################################################################
# marshal_test_enclave

if(${BUILD_ENCLAVE})
  add_enclave_library(marshal_test_enclave
  SRCS
      marshal_test_enclave.cpp

  HEADER_ONLY_LIBS

  TRUSTED_LIBS
    marshaller_enclave
    marshal_test_edl_enclave    
    common_enclave
    example_interface

    yas_common
    ${SGX_ENCLAVE_LIBS}
    )
  
  target_compile_definitions(marshal_test_enclave PRIVATE ${ENCLAVE_DEFINES})
  target_include_directories(marshal_test_enclave 
  PUBLIC 
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
    "$<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/generated/include>"
    "$<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/generated/src>"
    "$<INSTALL_INTERFACE:include/marshalled_tests>"
  PRIVATE 
    ${ENCLAVE_LIBCXX_INCLUDES})
    
  target_compile_options(marshal_test_enclave PRIVATE ${ENCLAVE_COMPILE_OPTIONS})
  target_link_options(marshal_test_enclave PRIVATE ${ENCLAVE_LINK_OPTIONS})

  set_property(TARGET marshal_test_enclave PROPERTY COMPILE_PDB_NAME marshal_test_enclave)

  enclave_sign(marshal_test_enclave KEY marshal_test_enclave_private.pem CONFIG marshal_test_enclave.config.${SGX_MODE}.xml)

 endif()