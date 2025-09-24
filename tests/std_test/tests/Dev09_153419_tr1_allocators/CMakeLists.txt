cmake_minimum_required(VERSION 3.24)

# Determine source files
add_executable(Dev09_153419_tr1_allocators
  test.cpp
)

target_include_directories(Dev09_153419_tr1_allocators
  PRIVATE
    ${PROJECT_SOURCE_DIR}/rpc/include
    ${PROJECT_SOURCE_DIR}/tests/common/include
    ${CMAKE_BINARY_DIR}/generated/include)

target_compile_definitions(Dev09_153419_tr1_allocators PRIVATE ${HOST_DEFINES})

target_link_libraries(Dev09_153419_tr1_allocators
  PRIVATE
    rpc::rpc_host
    yas_common
    fmt::fmt-header-only
    spdlog::spdlog)

target_compile_features(Dev09_153419_tr1_allocators PRIVATE cxx_std_17)

add_test(NAME Dev09_153419_tr1_allocators COMMAND Dev09_153419_tr1_allocators)
set_tests_properties(Dev09_153419_tr1_allocators PROPERTIES
  ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0;LSAN_OPTIONS=detect_leaks=0")
