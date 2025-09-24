cmake_minimum_required(VERSION 3.24)

# Determine source files
add_executable(P0718R2_atomic_smart_ptrs
  test.cpp
)

target_include_directories(P0718R2_atomic_smart_ptrs
  PRIVATE
    ${PROJECT_SOURCE_DIR}/rpc/include
    ${PROJECT_SOURCE_DIR}/tests/common/include
    ${CMAKE_BINARY_DIR}/generated/include)

target_compile_definitions(P0718R2_atomic_smart_ptrs PRIVATE ${HOST_DEFINES})

target_link_libraries(P0718R2_atomic_smart_ptrs
  PRIVATE
    rpc::rpc_host
    yas_common
    fmt::fmt-header-only
    spdlog::spdlog)

target_compile_features(P0718R2_atomic_smart_ptrs PRIVATE cxx_std_17)

add_test(NAME P0718R2_atomic_smart_ptrs COMMAND P0718R2_atomic_smart_ptrs)
set_tests_properties(P0718R2_atomic_smart_ptrs PROPERTIES
  ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0;LSAN_OPTIONS=detect_leaks=0")
