cmake_minimum_required(VERSION 3.24)

# Determine source files
add_executable(Dev11_1066589_shared_ptr_atomic_deadlock
  test.cpp
)

target_include_directories(Dev11_1066589_shared_ptr_atomic_deadlock
  PRIVATE
    ${PROJECT_SOURCE_DIR}/rpc/include
    ${PROJECT_SOURCE_DIR}/tests/common/include
    ${CMAKE_BINARY_DIR}/generated/include)

target_compile_definitions(Dev11_1066589_shared_ptr_atomic_deadlock PRIVATE ${HOST_DEFINES})

target_link_libraries(Dev11_1066589_shared_ptr_atomic_deadlock
  PRIVATE
    rpc::rpc_host
    yas_common
    fmt::fmt-header-only
    spdlog::spdlog)

target_compile_features(Dev11_1066589_shared_ptr_atomic_deadlock PRIVATE cxx_std_17)

add_test(NAME Dev11_1066589_shared_ptr_atomic_deadlock COMMAND Dev11_1066589_shared_ptr_atomic_deadlock)
set_tests_properties(Dev11_1066589_shared_ptr_atomic_deadlock PROPERTIES
  ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0;LSAN_OPTIONS=detect_leaks=0")
