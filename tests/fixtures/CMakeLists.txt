#[[
   Copyright (c) 2024 Edward Boggis-Rolfe
   All rights reserved.
]]

# Test fixtures static library
add_library(test_fixtures STATIC
    src/test_host.cpp
    src/test_service_logger.cpp
    src/test_globals.cpp
    src/rpc_log.cpp
    src/rpc_global_logger.cpp
)

# Add compile definitions
target_compile_definitions(test_fixtures PRIVATE ${HOST_DEFINES})

# Add include directories
target_include_directories(test_fixtures
    PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
    "$<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/generated/include>"
    "$<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/generated/src>"
    PRIVATE ${HOST_INCLUDES}
)

# Add enclave dependencies if needed
if(BUILD_ENCLAVE)
    set(ENCLAVE_DEPENDENCIES marshal_test_edl_host rpc_telemetry_edl_header_host)
endif()

# Link with required libraries
target_link_libraries(test_fixtures
    PUBLIC common_host
    example_idl_host
    example_import_idl_host
    example_shared_idl_host
    rpc::rpc_host
    yas_common
    fmt::fmt
    spdlog::spdlog
    GTest::gtest
    GTest::gmock
    ${HOST_LIBRARIES}
    ${ENCLAVE_DEPENDENCIES}
)

# Add compile options and link options
target_compile_options(test_fixtures PRIVATE ${HOST_COMPILE_OPTIONS} ${WARN_OK})
target_link_directories(test_fixtures PUBLIC ${SGX_LIBRARY_PATH})
target_link_options(test_fixtures PRIVATE ${HOST_LINK_EXE_OPTIONS})

# Set properties
set_property(TARGET test_fixtures PROPERTY COMPILE_PDB_NAME test_fixtures)

if(ENABLE_CLANG_TIDY)
    set_target_properties(test_fixtures PROPERTIES CXX_CLANG_TIDY "${CLANG_TIDY_COMMAND}")
endif()