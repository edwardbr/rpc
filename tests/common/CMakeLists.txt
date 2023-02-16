cmake_minimum_required(VERSION 3.12)

message("common_enclave")

# ##############################################################################
# common_enclave
if(${RPC_BUILD_SGX})
  add_library(common_enclave STATIC
    src/host_service_proxy.cpp
    include/common/enclave_service_proxy.h
    include/common/foo_impl.h
    include/common/host_service_proxy.h
    include/common/tests.h
  )

  target_compile_definitions(common_enclave PRIVATE ${ENCLAVE_DEFINES})
  target_include_directories(common_enclave
    PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
    "$<INSTALL_INTERFACE:include/marshalled_tests>"
    PRIVATE
    ${ENCLAVE_LIBCXX_INCLUDES})

  target_compile_options(common_enclave PRIVATE ${ENCLAVE_COMPILE_OPTIONS})
  target_link_options(common_enclave PRIVATE ${ENCLAVE_LINK_OPTIONS})

  target_link_libraries(common_enclave
    PUBLIC
    marshaller_enclave
    marshal_test_edl_enclave
    example_interface

    yas_common
  )

  set_property(TARGET common_enclave PROPERTY COMPILE_PDB_NAME common_enclave)

  set(ENCLAVE_SERVICE_PROXY src/enclave_service_proxy.cpp)
endif()

if(${RPC_BUILD_SGX})
add_library(common_host STATIC
  ${ENCLAVE_SERVICE_PROXY}
  include/common/enclave_service_proxy.h
  include/common/foo_impl.h
  include/common/host_service_proxy.h
  include/common/tests.h
)

target_compile_definitions(common_host PRIVATE ${HOST_DEFINES})
target_include_directories(common_host
  PUBLIC
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
  "$<INSTALL_INTERFACE:include/marshalled_tests>"
  PRIVATE
  ${HOST_INCLUDES})

target_compile_options(common_host PRIVATE ${HOST_COMPILE_OPTIONS})
target_link_options(common_host PRIVATE ${HOST_LINK_OPTIONS})

if(${RPC_BUILD_SGX})
  set(MARSHAL_TEST_EDL_HOST marshal_test_edl_host)
endif()

target_link_libraries(common_host
  PUBLIC
  marshal_test_edl_host
  example_interface

  rpc::marshaller_host
  yas_common
)
else()
add_library(common_host INTERFACE
  include/common/enclave_service_proxy.h
  include/common/foo_impl.h
  include/common/host_service_proxy.h
  include/common/tests.h
)

target_compile_definitions(common_host INTERFACE ${HOST_DEFINES})
target_include_directories(common_host
  INTERFACE
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
  "$<INSTALL_INTERFACE:include/marshalled_tests>"
  ${HOST_INCLUDES})

target_compile_options(common_host INTERFACE ${HOST_COMPILE_OPTIONS})
target_link_options(common_host INTERFACE ${HOST_LINK_OPTIONS})

target_link_libraries(common_host
INTERFACE
  example_interface

  rpc::marshaller_host
  yas_common
)

endif()



set_property(TARGET common_host PROPERTY COMPILE_PDB_NAME common_host)
