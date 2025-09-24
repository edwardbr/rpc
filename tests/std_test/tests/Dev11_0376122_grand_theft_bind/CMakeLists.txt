cmake_minimum_required(VERSION 3.24)

# Determine source files
add_executable(Dev11_0376122_grand_theft_bind
  test.cpp
)

target_include_directories(Dev11_0376122_grand_theft_bind
  PRIVATE
    ${PROJECT_SOURCE_DIR}/rpc/include
    ${PROJECT_SOURCE_DIR}/tests/common/include
    ${CMAKE_BINARY_DIR}/generated/include)

target_compile_definitions(Dev11_0376122_grand_theft_bind PRIVATE ${HOST_DEFINES})

target_link_libraries(Dev11_0376122_grand_theft_bind
  PRIVATE
    rpc::rpc_host
    yas_common
    fmt::fmt-header-only
    spdlog::spdlog)

target_compile_features(Dev11_0376122_grand_theft_bind PRIVATE cxx_std_17)

add_test(NAME Dev11_0376122_grand_theft_bind COMMAND Dev11_0376122_grand_theft_bind)
set_tests_properties(Dev11_0376122_grand_theft_bind PROPERTIES
  ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0;LSAN_OPTIONS=detect_leaks=0")
