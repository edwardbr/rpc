#[[
   Copyright (c) 2025 Edward Boggis-Rolfe
   All rights reserved.
]]

# RPC Shared Pointer Fuzz Test CMakeLists.txt
cmake_minimum_required(VERSION 3.24)

# Create fuzz test for RPC++ zone graphs and shared_ptr testing
add_executable(fuzz_test_main
    fuzz_test_main.cpp
)

# Link required libraries for fuzz test
target_link_libraries(fuzz_test_main
    PRIVATE
    rpc::rpc_host
    fuzz_test_idl_host  # Our independent fuzz test IDL
    test_fixtures  # For logging functions
    args::args  # Command line argument parsing
    ${LINK_LIBS}
)


# Include directories for fuzz test
target_include_directories(fuzz_test_main
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../fixtures/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/include
    ${CMAKE_BINARY_DIR}/generated/include
    ${CMAKE_BINARY_DIR}/generated/src
)


# Compiler options
target_compile_features(fuzz_test_main PRIVATE cxx_std_17)

# Apply the same compile options as other tests
target_compile_definitions(fuzz_test_main PRIVATE ${HOST_DEFINES})
target_compile_options(fuzz_test_main PRIVATE ${HOST_COMPILE_OPTIONS})
target_link_options(fuzz_test_main PRIVATE ${HOST_LINK_EXE_OPTIONS})
target_link_directories(fuzz_test_main PUBLIC ${SGX_LIBRARY_PATH})
set_property(TARGET fuzz_test_main PROPERTY COMPILE_PDB_NAME fuzz_test_main)

# # Copy test to output directory
# set_target_properties(fuzz_test_main PROPERTIES
#     RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/output/${CMAKE_BUILD_TYPE_LOWER}
#     OUTPUT_NAME fuzz_test_main
# )

# Enable telemetry for debugging
if(USE_RPC_TELEMETRY)
    target_compile_definitions(fuzz_test_main PRIVATE USE_RPC_TELEMETRY)
    target_link_libraries(fuzz_test_main PRIVATE rpc_telemetry_host)
endif()

# Add test to CTest
if(BUILD_TEST)
    add_test(NAME fuzz_test_main COMMAND fuzz_test_main 10)  # Run 10 iterations
endif()