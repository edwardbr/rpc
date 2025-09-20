cmake_minimum_required(VERSION 3.24)

add_executable(rpc_memory_tests
  shared_ptr_tests.cpp)

# The shared pointer implementation lives entirely in headers, so we only need include paths.
target_include_directories(rpc_memory_tests
  PRIVATE
    ${PROJECT_SOURCE_DIR}/rpc/include
    ${PROJECT_SOURCE_DIR}/tests/common/include
    ${CMAKE_BINARY_DIR}/generated/include)

target_compile_definitions(rpc_memory_tests PRIVATE ${HOST_DEFINES})

target_link_libraries(rpc_memory_tests
  PRIVATE
    rpc::rpc_host
    yas_common
    fmt::fmt-header-only
    spdlog::spdlog
    GTest::gtest_main)

target_compile_features(rpc_memory_tests PRIVATE cxx_std_17)


add_test(NAME rpc_memory_tests COMMAND rpc_memory_tests)
set_tests_properties(rpc_memory_tests PROPERTIES
  ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0;LSAN_OPTIONS=detect_leaks=0")
