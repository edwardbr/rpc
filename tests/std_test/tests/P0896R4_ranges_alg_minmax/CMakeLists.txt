cmake_minimum_required(VERSION 3.24)

# Determine source files
add_executable(P0896R4_ranges_alg_minmax
  test.cpp
)

target_include_directories(P0896R4_ranges_alg_minmax
  PRIVATE
    ${PROJECT_SOURCE_DIR}/rpc/include
    ${PROJECT_SOURCE_DIR}/tests/common/include
    ${CMAKE_BINARY_DIR}/generated/include)

target_compile_definitions(P0896R4_ranges_alg_minmax PRIVATE ${HOST_DEFINES})

target_link_libraries(P0896R4_ranges_alg_minmax
  PRIVATE
    rpc::rpc_host
    yas_common
    fmt::fmt-header-only
    spdlog::spdlog)

target_compile_features(P0896R4_ranges_alg_minmax PRIVATE cxx_std_17)

add_test(NAME P0896R4_ranges_alg_minmax COMMAND P0896R4_ranges_alg_minmax)
set_tests_properties(P0896R4_ranges_alg_minmax PROPERTIES
  ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0;LSAN_OPTIONS=detect_leaks=0")
