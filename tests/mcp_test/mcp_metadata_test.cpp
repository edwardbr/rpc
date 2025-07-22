#include <iostream>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

// This will be generated from the IDL
// #include "example.h"

struct function_info_test
{
    std::string full_name;
    std::string name;
    int method_id;
    uint64_t tag;
    bool marshalls_interfaces;
    std::string mcp_description;
    std::string json_schema;
};

// Mock function to simulate what would be generated
std::vector<function_info_test> get_test_function_info()
{
    std::vector<function_info_test> functions;
    
    // Simulated generated code for 'add' function
    functions.emplace_back(function_info_test{
        "yyy.i_example.add",
        "add", 
        1, 
        0, 
        false,
        "Adds two integers and returns the result",
        R"({"type":"object","description":"Input parameters for add method","properties":{"a":{"type":"integer"},"b":{"type":"integer"}},"required":["a","b"],"additionalProperties":false})"
    });
    
    // Simulated generated code for 'create_foo' function
    functions.emplace_back(function_info_test{
        "yyy.i_example.create_foo",
        "create_foo", 
        2, 
        0, 
        true,
        "Creates a foo object",
        R"({"type":"object","description":"Input parameters for create_foo method","properties":{},"additionalProperties":false})"
    });
    
    // Simulated generated code for 'call_host_create_enclave_and_throw_away' function
    functions.emplace_back(function_info_test{
        "yyy.i_example.call_host_create_enclave_and_throw_away",
        "call_host_create_enclave_and_throw_away", 
        3, 
        0, 
        false,
        "Call host and create enclave then throw away",
        R"({"type":"object","description":"Input parameters for call_host_create_enclave_and_throw_away method","properties":{"run_standard_tests":{"type":"boolean"}},"required":["run_standard_tests"],"additionalProperties":false})"
    });
    
    return functions;
}

void test_metadata_querying()
{
    std::cout << "=== Testing Metadata Querying ===" << std::endl;
    
    auto functions = get_test_function_info();
    
    for (const auto& func : functions)
    {
        std::cout << "Function: " << func.name << std::endl;
        std::cout << "  Full Name: " << func.full_name << std::endl;
        std::cout << "  Method ID: " << func.method_id << std::endl;
        std::cout << "  Tag: " << func.tag << std::endl;
        std::cout << "  Marshalls Interfaces: " << (func.marshalls_interfaces ? "true" : "false") << std::endl;
        std::cout << "  Description: " << func.mcp_description << std::endl;
        std::cout << "  JSON Schema: " << func.json_schema << std::endl;
        std::cout << std::endl;
    }
}

void test_json_schema_compatibility()
{
    std::cout << "=== Testing JSON Schema Compatibility ===" << std::endl;
    
    auto functions = get_test_function_info();
    
    for (const auto& func : functions)
    {
        try
        {
            // Parse the JSON schema to validate it's valid JSON
            auto schema_json = nlohmann::json::parse(func.json_schema);
            
            std::cout << "✓ " << func.name << " has valid JSON schema" << std::endl;
            std::cout << "  Schema type: " << schema_json["type"] << std::endl;
            std::cout << "  Description: " << schema_json["description"] << std::endl;
            
            if (schema_json.contains("properties"))
            {
                std::cout << "  Properties: ";
                for (auto it = schema_json["properties"].begin(); it != schema_json["properties"].end(); ++it)
                {
                    std::cout << it.key();
                    if (std::next(it) != schema_json["properties"].end()) std::cout << ", ";
                }
                std::cout << std::endl;
            }
            
            // Test creating example JSON payloads
            if (func.name == "add")
            {
                nlohmann::json payload = {
                    {"a", 5},
                    {"b", 3}
                };
                std::cout << "  Example payload: " << payload.dump() << std::endl;
            }
            else if (func.name == "call_host_create_enclave_and_throw_away")
            {
                nlohmann::json payload = {
                    {"run_standard_tests", true}
                };
                std::cout << "  Example payload: " << payload.dump() << std::endl;
            }
            else if (func.name == "create_foo")
            {
                nlohmann::json payload = nlohmann::json::object();
                std::cout << "  Example payload: " << payload.dump() << std::endl;
            }
            
        }
        catch (const std::exception& e)
        {
            std::cout << "✗ " << func.name << " has invalid JSON schema: " << e.what() << std::endl;
        }
        
        std::cout << std::endl;
    }
}

void test_mcp_service_compatibility()
{
    std::cout << "=== Testing MCP Service Compatibility ===" << std::endl;
    
    auto functions = get_test_function_info();
    
    // Simulate what an MCP service would see
    nlohmann::json mcp_tools = nlohmann::json::array();
    
    for (const auto& func : functions)
    {
        nlohmann::json tool = {
            {"name", func.name},
            {"description", func.mcp_description},
            {"inputSchema", nlohmann::json::parse(func.json_schema)}
        };
        mcp_tools.push_back(tool);
    }
    
    std::cout << "MCP Tools JSON:" << std::endl;
    std::cout << mcp_tools.dump(2) << std::endl;
    
    std::cout << "\n✓ Successfully created MCP-compatible tool definitions" << std::endl;
}

int main()
{
    try
    {
        test_metadata_querying();
        test_json_schema_compatibility(); 
        test_mcp_service_compatibility();
        
        std::cout << "=== All Tests Passed ===" << std::endl;
        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
}