cmake_minimum_required(VERSION 3.24)

# Determine source files
add_executable(P0674R1_make_shared_for_arrays
  test.cpp
)

target_include_directories(P0674R1_make_shared_for_arrays
  PRIVATE
    ${PROJECT_SOURCE_DIR}/rpc/include
    ${PROJECT_SOURCE_DIR}/tests/common/include
    ${CMAKE_BINARY_DIR}/generated/include)

target_compile_definitions(P0674R1_make_shared_for_arrays PRIVATE ${HOST_DEFINES})

target_link_libraries(P0674R1_make_shared_for_arrays
  PRIVATE
    rpc::rpc_host
    yas_common
    fmt::fmt-header-only
    spdlog::spdlog)

target_compile_features(P0674R1_make_shared_for_arrays PRIVATE cxx_std_17)

add_test(NAME P0674R1_make_shared_for_arrays COMMAND P0674R1_make_shared_for_arrays)
set_tests_properties(P0674R1_make_shared_for_arrays PROPERTIES
  ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0;LSAN_OPTIONS=detect_leaks=0")
