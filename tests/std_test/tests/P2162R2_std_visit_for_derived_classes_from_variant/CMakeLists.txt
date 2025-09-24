cmake_minimum_required(VERSION 3.24)

# Determine source files
add_executable(P2162R2_std_visit_for_derived_classes_from_variant
  test.cpp
)

target_include_directories(P2162R2_std_visit_for_derived_classes_from_variant
  PRIVATE
    ${PROJECT_SOURCE_DIR}/rpc/include
    ${PROJECT_SOURCE_DIR}/tests/common/include
    ${CMAKE_BINARY_DIR}/generated/include)

target_compile_definitions(P2162R2_std_visit_for_derived_classes_from_variant PRIVATE ${HOST_DEFINES})

target_link_libraries(P2162R2_std_visit_for_derived_classes_from_variant
  PRIVATE
    rpc::rpc_host
    yas_common
    fmt::fmt-header-only
    spdlog::spdlog)

target_compile_features(P2162R2_std_visit_for_derived_classes_from_variant PRIVATE cxx_std_17)

add_test(NAME P2162R2_std_visit_for_derived_classes_from_variant COMMAND P2162R2_std_visit_for_derived_classes_from_variant)
set_tests_properties(P2162R2_std_visit_for_derived_classes_from_variant PROPERTIES
  ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0;LSAN_OPTIONS=detect_leaks=0")
