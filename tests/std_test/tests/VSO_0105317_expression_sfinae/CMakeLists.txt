cmake_minimum_required(VERSION 3.24)

# Determine source files
add_executable(VSO_0105317_expression_sfinae
  test.cpp
)

target_include_directories(VSO_0105317_expression_sfinae
  PRIVATE
    ${PROJECT_SOURCE_DIR}/rpc/include
    ${PROJECT_SOURCE_DIR}/tests/common/include
    ${CMAKE_BINARY_DIR}/generated/include)

target_compile_definitions(VSO_0105317_expression_sfinae PRIVATE ${HOST_DEFINES})

target_link_libraries(VSO_0105317_expression_sfinae
  PRIVATE
    rpc::rpc_host
    yas_common
    fmt::fmt-header-only
    spdlog::spdlog)

target_compile_features(VSO_0105317_expression_sfinae PRIVATE cxx_std_17)

add_test(NAME VSO_0105317_expression_sfinae COMMAND VSO_0105317_expression_sfinae)
set_tests_properties(VSO_0105317_expression_sfinae PROPERTIES
  ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0;LSAN_OPTIONS=detect_leaks=0")
