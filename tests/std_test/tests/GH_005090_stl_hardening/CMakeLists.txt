cmake_minimum_required(VERSION 3.24)

# Determine source files
add_executable(GH_005090_stl_hardening
  test.cpp
)

target_include_directories(GH_005090_stl_hardening
  PRIVATE
    ${PROJECT_SOURCE_DIR}/rpc/include
    ${PROJECT_SOURCE_DIR}/tests/common/include
    ${CMAKE_BINARY_DIR}/generated/include)

target_compile_definitions(GH_005090_stl_hardening PRIVATE ${HOST_DEFINES})

target_link_libraries(GH_005090_stl_hardening
  PRIVATE
    rpc::rpc_host
    yas_common
    fmt::fmt-header-only
    spdlog::spdlog)

target_compile_features(GH_005090_stl_hardening PRIVATE cxx_std_17)

add_test(NAME GH_005090_stl_hardening COMMAND GH_005090_stl_hardening)
set_tests_properties(GH_005090_stl_hardening PROPERTIES
  ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0;LSAN_OPTIONS=detect_leaks=0")
