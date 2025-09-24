cmake_minimum_required(VERSION 3.24)

# Determine source files
add_executable(P1135R6_atomic_wait
  test.cpp
)

target_include_directories(P1135R6_atomic_wait
  PRIVATE
    ${PROJECT_SOURCE_DIR}/rpc/include
    ${PROJECT_SOURCE_DIR}/tests/common/include
    ${CMAKE_BINARY_DIR}/generated/include)

target_compile_definitions(P1135R6_atomic_wait PRIVATE ${HOST_DEFINES})

target_link_libraries(P1135R6_atomic_wait
  PRIVATE
    rpc::rpc_host
    yas_common
    fmt::fmt-header-only
    spdlog::spdlog)

target_compile_features(P1135R6_atomic_wait PRIVATE cxx_std_17)

add_test(NAME P1135R6_atomic_wait COMMAND P1135R6_atomic_wait)
set_tests_properties(P1135R6_atomic_wait PROPERTIES
  ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0;LSAN_OPTIONS=detect_leaks=0")
