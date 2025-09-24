cmake_minimum_required(VERSION 3.24)

# Determine source files
add_executable(VSO_0000000_has_static_rtti
  test.cpp
)

target_include_directories(VSO_0000000_has_static_rtti
  PRIVATE
    ${PROJECT_SOURCE_DIR}/rpc/include
    ${PROJECT_SOURCE_DIR}/tests/common/include
    ${CMAKE_BINARY_DIR}/generated/include)

target_compile_definitions(VSO_0000000_has_static_rtti PRIVATE ${HOST_DEFINES})

target_link_libraries(VSO_0000000_has_static_rtti
  PRIVATE
    rpc::rpc_host
    yas_common
    fmt::fmt-header-only
    spdlog::spdlog)

target_compile_features(VSO_0000000_has_static_rtti PRIVATE cxx_std_17)

add_test(NAME VSO_0000000_has_static_rtti COMMAND VSO_0000000_has_static_rtti)
set_tests_properties(VSO_0000000_has_static_rtti PROPERTIES
  ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0;LSAN_OPTIONS=detect_leaks=0")
