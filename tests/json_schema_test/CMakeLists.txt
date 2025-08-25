#[[
   Copyright (c) 2024 Edward Boggis-Rolfe
   All rights reserved.
]]

# JSON Schema Generation Tests - consolidated into single test executable

# Single consolidated JSON schema metadata test
add_executable(json_schema_metadata_test json_schema_metadata_test.cpp i_foo_schema_validation_test.cpp ../fixtures/src/rpc_log.cpp)

# Add compile definitions like test_host
target_compile_definitions(json_schema_metadata_test PRIVATE ${HOST_DEFINES})

# Add include directories to match test_host structure
target_include_directories(json_schema_metadata_test
    PUBLIC "$<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/generated/include>"
    "$<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/generated/src>"
    PRIVATE ${HOST_INCLUDES}
)

if(BUILD_ENCLAVE)
    set(ENCLAVE_DEPENDENCIES marshal_test_edl_host rpc_telemetry_edl_header_host)
endif()

# Link with libraries similar to test_host
target_link_libraries(json_schema_metadata_test
    PUBLIC
    common_host
    example_idl_host
    example_import_idl_host
    example_shared_idl_host
    rpc::rpc_host
    yas_common
    fmt::fmt
    GTest::gtest_main
    GTest::gmock_main
    spdlog::spdlog
    nlohmann_json::nlohmann_json
    nlohmann_json_schema_validator
    ${HOST_LIBRARIES}
    ${ENCLAVE_DEPENDENCIES}
    test_fixtures
)

# Add compile options and link options like test_host
target_compile_options(json_schema_metadata_test PRIVATE ${HOST_COMPILE_OPTIONS} ${WARN_OK})
target_link_options(json_schema_metadata_test PRIVATE ${HOST_LINK_EXE_OPTIONS})
target_link_directories(json_schema_metadata_test PUBLIC ${SGX_LIBRARY_PATH})
set_property(TARGET json_schema_metadata_test PROPERTY COMPILE_PDB_NAME json_schema_metadata_test)

if(ENABLE_CLANG_TIDY)
    set_target_properties(json_schema_metadata_test PROPERTIES CXX_CLANG_TIDY "${CLANG_TIDY_COMMAND}")
endif()

# Add to tests if CTest is enabled - using gtest_discover_tests
if(BUILD_TEST)
    include(GoogleTest)
    gtest_discover_tests(json_schema_metadata_test)
endif()