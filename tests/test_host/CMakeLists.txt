cmake_minimum_required(VERSION 3.24)

add_executable(rpc_test 
  main.cpp 
  rpc_log.cpp)

target_compile_definitions(rpc_test PRIVATE ${HOST_DEFINES})

target_include_directories(
  rpc_test
  PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
         "$<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/generated/include>"
         "$<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/generated/src>"
  PRIVATE ${HOST_INCLUDES})

target_link_libraries(
  rpc_test
  PUBLIC marshal_test_edl_host
        rpc_telemetry_edl_header_host
         common_host
         example_idl_host
         rpc::rpc_host
         yas_common
         clipp::clipp
         fmt::fmt-header-only
         GTest::gtest_main
         GTest::gmock_main
         spdlog::spdlog_header_only
         ${HOST_LIBRARIES})

target_compile_options(rpc_test PRIVATE ${HOST_COMPILE_OPTIONS})
target_link_options(rpc_test PRIVATE ${HOST_LINK_EXE_OPTIONS})
target_link_directories(rpc_test PUBLIC ${SGX_LIBRARY_PATH})
set_property(TARGET rpc_test PROPERTY COMPILE_PDB_NAME rpc_test)

if(ENABLE_CLANG_TIDY)
  set_target_properties(rpc_test PROPERTIES CXX_CLANG_TIDY "${CLANG_TIDY_COMMAND}")
endif()

gtest_discover_tests(rpc_test)
