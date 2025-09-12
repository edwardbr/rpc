#include <gtest/gtest.h>
#include <iostream>
#include <string>
#include <vector>
#include <cstdint>
#include <map>
#include <algorithm>

// Conditional include for nlohmann/json if available
#include <nlohmann/json.hpp>
#include <nlohmann/json-schema.hpp>

// Generated IDL headers
#include <example/example.h>
#include <rpc/error_codes.h>

// Include test setup classes and required headers
#include "test_host.h"
#include "inproc_setup.h"
#include "test_globals.h"
#include "test_service_logger.h"
#include <common/foo_impl.h>
#include <common/tests.h>
#include <example/example.h>

// Test using inproc_setup to call standard_tests for schema validation
TEST(McpSchemaValidationTest, InprocSetupStandardTests)
{
    // Serialize a call to call_host_create_enclave_and_throw_away with parameter false
    std::vector<char> buffer;
    auto err
        = yyy::i_example::proxy_serialiser<rpc::serialiser::yas, rpc::encoding>::call_host_create_enclave_and_throw_away(
            false, buffer, rpc::encoding::yas_json);

    ASSERT_EQ(err, 0) << "Serialization should succeed";
    ASSERT_FALSE(buffer.empty()) << "Buffer should not be empty after serialization";

    // Get function info and find the matching function
    auto fi = yyy::i_example::get_function_info();
    ASSERT_FALSE(fi.empty()) << "Function info should not be empty";

    // Find the call_host_create_enclave_and_throw_away function in the real function info
    auto it = std::find_if(
        fi.begin(), fi.end(), [](const auto& func) { return func.name == "call_host_create_enclave_and_throw_away"; });

    ASSERT_NE(it, fi.end()) << "Should find call_host_create_enclave_and_throw_away function";

    // Convert buffer to string and parse as JSON
    std::string buffer_str(buffer.begin(), buffer.end());
    EXPECT_FALSE(buffer_str.empty()) << "Serialized buffer should contain data";

    // Parse the serialized data as JSON
    nlohmann::json payload_json;
    ASSERT_NO_THROW(payload_json = nlohmann::json::parse(buffer_str)) << "Buffer should contain valid JSON";

    // Parse the schema from the real function info
    const auto& schema_str = it->in_json_schema;
    EXPECT_FALSE(schema_str.empty()) << "Schema should not be empty";

    nlohmann::json schema_json;
    ASSERT_NO_THROW(schema_json = nlohmann::json::parse(schema_str)) << "Schema should be valid JSON";

    // Create validator and set the schema
    nlohmann::json_schema::json_validator validator;
    try
    {
        validator.set_root_schema(schema_json);
    }
    catch (const std::exception& e)
    {
        FAIL() << "Failed to set schema: " << e.what();
    }

    // Validate the payload against the schema
    try
    {
        validator.validate(payload_json);
        SUCCEED() << "Payload successfully validates against schema";
    }
    catch (const std::exception& e)
    {
        FAIL() << "Schema validation failed: " << e.what() << "\nPayload: " << payload_json.dump(2)
               << "\nSchema: " << schema_json.dump(2);
    }

    // Additional checks for the specific function
    EXPECT_TRUE(payload_json.contains("run_standard_tests")) << "Payload should contain run_standard_tests parameter";
    EXPECT_TRUE(payload_json["run_standard_tests"].is_boolean()) << "run_standard_tests should be a boolean";
    EXPECT_EQ(payload_json["run_standard_tests"].get<bool>(), false)
        << "run_standard_tests should be false as passed to serializer";

    SUCCEED() << "Schema validation test completed successfully";
}

// Test that input and output schemas are properly separated
TEST(McpSchemaValidationTest, InputOutputSchemaSeparation)
{
    // Get function info
    auto fi = yyy::i_example::get_function_info();
    ASSERT_FALSE(fi.empty()) << "Function info should not be empty";

    // Find a function with both input and output parameters
    auto it = std::find_if(fi.begin(), fi.end(), [](const auto& func) { 
        return func.name == "call_host_create_enclave"; 
    });

    ASSERT_NE(it, fi.end()) << "Should find call_host_create_enclave function";

    // Verify input schema exists and is valid
    EXPECT_FALSE(it->in_json_schema.empty()) << "Input schema should not be empty";
    
    nlohmann::json in_schema_json;
    ASSERT_NO_THROW(in_schema_json = nlohmann::json::parse(it->in_json_schema)) 
        << "Input schema should be valid JSON";
    
    EXPECT_EQ(in_schema_json["type"], "object") << "Input schema should be object type";
    EXPECT_TRUE(in_schema_json.contains("description")) << "Input schema should have description";
    EXPECT_TRUE(in_schema_json["description"].get<std::string>().find("Input parameters") != std::string::npos) 
        << "Input schema description should indicate input parameters";

    // Verify output schema exists and is valid
    EXPECT_FALSE(it->out_json_schema.empty()) << "Output schema should not be empty";
    
    nlohmann::json out_schema_json;
    ASSERT_NO_THROW(out_schema_json = nlohmann::json::parse(it->out_json_schema)) 
        << "Output schema should be valid JSON";
    
    EXPECT_EQ(out_schema_json["type"], "object") << "Output schema should be object type";
    EXPECT_TRUE(out_schema_json.contains("description")) << "Output schema should have description";
    EXPECT_TRUE(out_schema_json["description"].get<std::string>().find("Output parameters") != std::string::npos) 
        << "Output schema description should indicate output parameters";

    // Test that schemas are different (since this function has both input and output params)
    EXPECT_NE(it->in_json_schema, it->out_json_schema) << "Input and output schemas should be different";
}

// Test that functions with only input parameters have empty output schema
TEST(McpSchemaValidationTest, InputOnlyFunctionSchemas)
{
    // Get function info
    auto fi = yyy::i_example::get_function_info();
    ASSERT_FALSE(fi.empty()) << "Function info should not be empty";

    // Find a function with only input parameters (no [out] attributes)
    auto it = std::find_if(fi.begin(), fi.end(), [](const auto& func) { 
        return func.name == "call_host_create_enclave_and_throw_away"; 
    });

    ASSERT_NE(it, fi.end()) << "Should find call_host_create_enclave_and_throw_away function";

    // Verify input schema exists
    EXPECT_FALSE(it->in_json_schema.empty()) << "Input schema should not be empty for input-only function";
    
    // Verify output schema indicates no output parameters
    nlohmann::json out_schema_json;
    ASSERT_NO_THROW(out_schema_json = nlohmann::json::parse(it->out_json_schema)) 
        << "Output schema should be valid JSON";
    
    // For functions with no output parameters, the schema should still be valid but indicate no properties
    EXPECT_EQ(out_schema_json["type"], "object") << "Output schema should be object type";
    EXPECT_FALSE(out_schema_json.contains("properties") && !out_schema_json["properties"].empty()) 
        << "Output schema should not have properties for input-only function";
}

// Note: main() function now handled by GTest framework
// Tests are automatically discovered and run by gtest_main
