cmake_minimum_required(VERSION 3.24)

# Determine source files
add_executable(Dev09_196243_tr1_enable_shared_from_this_ops
  test.cpp
)

target_include_directories(Dev09_196243_tr1_enable_shared_from_this_ops
  PRIVATE
    ${PROJECT_SOURCE_DIR}/rpc/include
    ${PROJECT_SOURCE_DIR}/tests/common/include
    ${CMAKE_BINARY_DIR}/generated/include)

target_compile_definitions(Dev09_196243_tr1_enable_shared_from_this_ops PRIVATE ${HOST_DEFINES})

target_link_libraries(Dev09_196243_tr1_enable_shared_from_this_ops
  PRIVATE
    rpc::rpc_host
    yas_common
    fmt::fmt-header-only
    spdlog::spdlog)

target_compile_features(Dev09_196243_tr1_enable_shared_from_this_ops PRIVATE cxx_std_17)

add_test(NAME Dev09_196243_tr1_enable_shared_from_this_ops COMMAND Dev09_196243_tr1_enable_shared_from_this_ops)
set_tests_properties(Dev09_196243_tr1_enable_shared_from_this_ops PROPERTIES
  ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0;LSAN_OPTIONS=detect_leaks=0")
