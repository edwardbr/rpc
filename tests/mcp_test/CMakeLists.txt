#[[
   Copyright (c) 2024 Edward Boggis-Rolfe
   All rights reserved.
]]

# MCP Integration Tests - integrated with main CMake build system

# Simple MCP metadata test (no external dependencies)
add_executable(simple_mcp_metadata_test simple_mcp_metadata_test.cpp)

# Set C++ standard to match project standard
set_target_properties(simple_mcp_metadata_test PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

# Add include directories using project paths
target_include_directories(simple_mcp_metadata_test PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/rpc/include
)

# Set binary output directory to match project structure
set_target_properties(simple_mcp_metadata_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests/mcp_test
)

# Optional: MCP metadata test with nlohmann/json (if available)
find_package(nlohmann_json 3.2.0 QUIET)
if(nlohmann_json_FOUND)
    add_executable(mcp_metadata_test mcp_metadata_test.cpp)
    
    # Set C++ standard to match project standard
    set_target_properties(mcp_metadata_test PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED ON
    )
    
    target_link_libraries(mcp_metadata_test nlohmann_json::nlohmann_json)
    target_include_directories(mcp_metadata_test PRIVATE 
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${PROJECT_SOURCE_DIR}/rpc/include
    )
    
    # Set binary output directory to match project structure
    set_target_properties(mcp_metadata_test PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests/mcp_test
    )
    
    message(STATUS "Building advanced MCP test with nlohmann/json support")
else()
    message(STATUS "nlohmann/json not found, skipping advanced MCP test")
endif()

# Add to tests if CTest is enabled
if(BUILD_TEST AND COMMAND add_test)
    add_test(NAME simple_mcp_metadata_test COMMAND simple_mcp_metadata_test)
    if(TARGET mcp_metadata_test)
        add_test(NAME mcp_metadata_test COMMAND mcp_metadata_test)
    endif()
endif()