cmake_minimum_required(VERSION 3.24)

# Determine source files
add_executable(VSO_0157762_feature_test_macros
  test.compile.pass.cpp
)

target_include_directories(VSO_0157762_feature_test_macros
  PRIVATE
    ${PROJECT_SOURCE_DIR}/rpc/include
    ${PROJECT_SOURCE_DIR}/tests/common/include
    ${CMAKE_BINARY_DIR}/generated/include)

target_compile_definitions(VSO_0157762_feature_test_macros PRIVATE ${HOST_DEFINES})

target_link_libraries(VSO_0157762_feature_test_macros
  PRIVATE
    rpc::rpc_host
    yas_common
    fmt::fmt-header-only
    spdlog::spdlog)

target_compile_features(VSO_0157762_feature_test_macros PRIVATE cxx_std_17)

add_test(NAME VSO_0157762_feature_test_macros COMMAND VSO_0157762_feature_test_macros)
set_tests_properties(VSO_0157762_feature_test_macros PROPERTIES
  ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0;LSAN_OPTIONS=detect_leaks=0")
