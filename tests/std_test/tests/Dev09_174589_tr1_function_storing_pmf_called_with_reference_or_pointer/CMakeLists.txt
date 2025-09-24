cmake_minimum_required(VERSION 3.24)

# Determine source files
add_executable(Dev09_174589_tr1_function_storing_pmf_called_with_reference_or_pointer
  test.cpp
)

target_include_directories(Dev09_174589_tr1_function_storing_pmf_called_with_reference_or_pointer
  PRIVATE
    ${PROJECT_SOURCE_DIR}/rpc/include
    ${PROJECT_SOURCE_DIR}/tests/common/include
    ${CMAKE_BINARY_DIR}/generated/include)

target_compile_definitions(Dev09_174589_tr1_function_storing_pmf_called_with_reference_or_pointer PRIVATE ${HOST_DEFINES})

target_link_libraries(Dev09_174589_tr1_function_storing_pmf_called_with_reference_or_pointer
  PRIVATE
    rpc::rpc_host
    yas_common
    fmt::fmt-header-only
    spdlog::spdlog)

target_compile_features(Dev09_174589_tr1_function_storing_pmf_called_with_reference_or_pointer PRIVATE cxx_std_17)

add_test(NAME Dev09_174589_tr1_function_storing_pmf_called_with_reference_or_pointer COMMAND Dev09_174589_tr1_function_storing_pmf_called_with_reference_or_pointer)
set_tests_properties(Dev09_174589_tr1_function_storing_pmf_called_with_reference_or_pointer PROPERTIES
  ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0;LSAN_OPTIONS=detect_leaks=0")
