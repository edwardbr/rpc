cmake_minimum_required(VERSION 3.24)

# Determine source files
add_executable(LWG3018_shared_ptr_function
  test.cpp
)

target_include_directories(LWG3018_shared_ptr_function
  PRIVATE
    ${PROJECT_SOURCE_DIR}/rpc/include
    ${PROJECT_SOURCE_DIR}/tests/common/include
    ${CMAKE_BINARY_DIR}/generated/include)

target_compile_definitions(LWG3018_shared_ptr_function PRIVATE ${HOST_DEFINES})

target_link_libraries(LWG3018_shared_ptr_function
  PRIVATE
    rpc::rpc_host
    yas_common
    fmt::fmt-header-only
    spdlog::spdlog)

target_compile_features(LWG3018_shared_ptr_function PRIVATE cxx_std_17)

add_test(NAME LWG3018_shared_ptr_function COMMAND LWG3018_shared_ptr_function)
set_tests_properties(LWG3018_shared_ptr_function PROPERTIES
  ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0;LSAN_OPTIONS=detect_leaks=0")
