cmake_minimum_required(VERSION 3.24)

# Determine source files
add_executable(Dev10_561430_list_and_tree_leaks
  test.cpp
)

target_include_directories(Dev10_561430_list_and_tree_leaks
  PRIVATE
    ${PROJECT_SOURCE_DIR}/rpc/include
    ${PROJECT_SOURCE_DIR}/tests/common/include
    ${CMAKE_BINARY_DIR}/generated/include)

target_compile_definitions(Dev10_561430_list_and_tree_leaks PRIVATE ${HOST_DEFINES})

target_link_libraries(Dev10_561430_list_and_tree_leaks
  PRIVATE
    rpc::rpc_host
    yas_common
    fmt::fmt-header-only
    spdlog::spdlog)

target_compile_features(Dev10_561430_list_and_tree_leaks PRIVATE cxx_std_17)

add_test(NAME Dev10_561430_list_and_tree_leaks COMMAND Dev10_561430_list_and_tree_leaks)
set_tests_properties(Dev10_561430_list_and_tree_leaks PROPERTIES
  ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0;LSAN_OPTIONS=detect_leaks=0")
