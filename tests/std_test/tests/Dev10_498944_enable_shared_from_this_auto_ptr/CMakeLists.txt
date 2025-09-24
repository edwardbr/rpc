cmake_minimum_required(VERSION 3.24)

# Determine source files
add_executable(Dev10_498944_enable_shared_from_this_auto_ptr
  test.cpp
)

target_include_directories(Dev10_498944_enable_shared_from_this_auto_ptr
  PRIVATE
    ${PROJECT_SOURCE_DIR}/rpc/include
    ${PROJECT_SOURCE_DIR}/tests/common/include
    ${CMAKE_BINARY_DIR}/generated/include)

target_compile_definitions(Dev10_498944_enable_shared_from_this_auto_ptr PRIVATE ${HOST_DEFINES})

target_link_libraries(Dev10_498944_enable_shared_from_this_auto_ptr
  PRIVATE
    rpc::rpc_host
    yas_common
    fmt::fmt-header-only
    spdlog::spdlog)

target_compile_features(Dev10_498944_enable_shared_from_this_auto_ptr PRIVATE cxx_std_17)

add_test(NAME Dev10_498944_enable_shared_from_this_auto_ptr COMMAND Dev10_498944_enable_shared_from_this_auto_ptr)
set_tests_properties(Dev10_498944_enable_shared_from_this_auto_ptr PROPERTIES
  ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0;LSAN_OPTIONS=detect_leaks=0")
