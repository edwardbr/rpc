#[[
   Copyright (c) 2024 Edward Boggis-Rolfe
   All rights reserved.
]]
cmake_minimum_required(VERSION 3.24)

add_executable(rpc_test main.cpp type_test_local_suite.cpp remote_type_test_suite.cpp type_test_with_host_suite.cpp ${COROUTINE_TESTS})

target_compile_definitions(rpc_test PRIVATE ${HOST_DEFINES})

target_include_directories(
  rpc_test
  PUBLIC "$<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/generated/include>"
  "$<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/generated/src>"
  PRIVATE ${HOST_INCLUDES})

if(BUILD_ENCLAVE)
  set(ENCLAVE_DEPENDANCIES marshal_test_edl_host rpc_telemetry_edl_header_host)
endif()

target_link_libraries(
  rpc_test
  PUBLIC common_host
  example_idl_host
  example_import_idl_host
  example_shared_idl_host
  -Wl,--whole-archive
  test_fixtures
  -Wl,--no-whole-archive
  yas_common
  rpc::rpc_host
  yas_common
  args::args
  fmt::fmt-header-only
  GTest::gtest_main
  GTest::gmock_main
  spdlog::spdlog
  ${CORO_RUNTIME}
  ${RPC_HOST_FMT_LIB}
  ${ENCLAVE_DEPENDANCIES}
  ${HOST_LIBRARIES})

target_compile_options(rpc_test PRIVATE ${HOST_COMPILE_OPTIONS} ${WARN_OK})
target_link_options(rpc_test PRIVATE ${HOST_LINK_EXE_OPTIONS})
target_link_directories(rpc_test PUBLIC ${SGX_LIBRARY_PATH})
set_property(TARGET rpc_test PROPERTY COMPILE_PDB_NAME rpc_test)

if(ENABLE_CLANG_TIDY)
  set_target_properties(rpc_test PROPERTIES CXX_CLANG_TIDY "${CLANG_TIDY_COMMAND}")
endif()

include(CTest)
gtest_discover_tests(rpc_test)
