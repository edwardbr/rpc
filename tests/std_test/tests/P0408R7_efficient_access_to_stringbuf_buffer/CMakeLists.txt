cmake_minimum_required(VERSION 3.24)

# Determine source files
add_executable(P0408R7_efficient_access_to_stringbuf_buffer
  test.cpp
)

target_include_directories(P0408R7_efficient_access_to_stringbuf_buffer
  PRIVATE
    ${PROJECT_SOURCE_DIR}/rpc/include
    ${PROJECT_SOURCE_DIR}/tests/common/include
    ${CMAKE_BINARY_DIR}/generated/include)

target_compile_definitions(P0408R7_efficient_access_to_stringbuf_buffer PRIVATE ${HOST_DEFINES})

target_link_libraries(P0408R7_efficient_access_to_stringbuf_buffer
  PRIVATE
    rpc::rpc_host
    yas_common
    fmt::fmt-header-only
    spdlog::spdlog)

target_compile_features(P0408R7_efficient_access_to_stringbuf_buffer PRIVATE cxx_std_17)

add_test(NAME P0408R7_efficient_access_to_stringbuf_buffer COMMAND P0408R7_efficient_access_to_stringbuf_buffer)
set_tests_properties(P0408R7_efficient_access_to_stringbuf_buffer PROPERTIES
  ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0;LSAN_OPTIONS=detect_leaks=0")
