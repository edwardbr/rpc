cmake_minimum_required(VERSION 3.24)

# Determine source files
add_executable(P1020R1_smart_pointer_for_overwrite
  test.cpp
)

target_include_directories(P1020R1_smart_pointer_for_overwrite
  PRIVATE
    ${PROJECT_SOURCE_DIR}/rpc/include
    ${PROJECT_SOURCE_DIR}/tests/common/include
    ${CMAKE_BINARY_DIR}/generated/include)

target_compile_definitions(P1020R1_smart_pointer_for_overwrite PRIVATE ${HOST_DEFINES})

target_link_libraries(P1020R1_smart_pointer_for_overwrite
  PRIVATE
    rpc::rpc_host
    yas_common
    fmt::fmt-header-only
    spdlog::spdlog)

target_compile_features(P1020R1_smart_pointer_for_overwrite PRIVATE cxx_std_17)

add_test(NAME P1020R1_smart_pointer_for_overwrite COMMAND P1020R1_smart_pointer_for_overwrite)
set_tests_properties(P1020R1_smart_pointer_for_overwrite PROPERTIES
  ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0;LSAN_OPTIONS=detect_leaks=0")
