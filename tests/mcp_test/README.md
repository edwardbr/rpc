# MCP Integration Tests

This directory contains tests and examples for the Model Context Protocol (MCP) integration with the RPC system.

## Files

### Test Programs
- **`simple_mcp_metadata_test.cpp`** - Self-contained test with no external dependencies
- **`mcp_metadata_test.cpp`** - Advanced test using nlohmann/json (optional dependency)

### Build Configuration
- **`CMakeLists.txt`** - Build configuration for MCP tests

## Building and Running

### Using CMakePresets (Recommended)
The MCP tests are integrated with the main CMake build system. Use the existing build presets:

```bash
cd /path/to/rpc

# Configure with your preferred preset
cmake --preset Debug  # or Release, Debug_SGX, etc.

# Build the MCP tests
cmake --build build --target simple_mcp_metadata_test
# Optional: cmake --build build --target mcp_metadata_test (if nlohmann/json available)

# Run the tests
./build/tests/mcp_test/simple_mcp_metadata_test
./build/tests/mcp_test/mcp_metadata_test  # if built
```

### Alternative: Build All Tests
```bash
cd /path/to/rpc
cmake --preset Debug
cmake --build build --target all
# All MCP tests will be built automatically
```

### Running with CTest
If your project has CTest enabled:
```bash
cd /path/to/rpc
cmake --build build
ctest --test-dir build -R mcp  # Run only MCP tests
```

## What These Tests Validate

### ✅ Core MCP Features
- **Function Descriptions**: Extraction from IDL `[description="..."]` attributes
- **JSON Schema Generation**: Automatic parameter schema creation
- **Metadata Querying**: Runtime access to function information
- **MCP Compatibility**: Generated metadata works with MCP tools

### ✅ RPC Integration
- **Transport Independence**: Works with any transport layer
- **YAS Compatibility**: JSON schemas work with embedded YAS parser
- **Backward Compatibility**: Existing RPC functionality unchanged

## Example Usage

After implementing MCP support, RPC interfaces decorated with descriptions:

```idl
[description="Adds two integers and returns the result"] 
error_code add(int a, int b, [out, by_value] int& c);
```

Will generate function_info with MCP-compatible metadata:

```cpp
{
    "yyy.i_example.add",                           // full_name
    "add",                                         // name
    {1},                                          // method id
    0,                                            // tag
    false,                                        // marshalls_interfaces
    "Adds two integers and returns the result",   // mcp_description
    "{\"type\":\"object\",\"properties\":{...}"   // json_schema
}
```

## Integration with MCP Services

The generated metadata can be directly used by MCP tools:

```json
{
  "name": "add",
  "description": "Adds two integers and returns the result",
  "inputSchema": {
    "type": "object",
    "properties": {
      "a": {"type": "integer"},
      "b": {"type": "integer"}
    },
    "required": ["a", "b"]
  }
}
```

## Related Files

- **`../idls/example/example.idl`** - Main example IDL file with MCP descriptions
- **`../../rpc/include/rpc/types.h`** - Extended function_info struct
- **`../../generator/src/synchronous_generator.cpp`** - Code generator with MCP support

## Example IDL Usage

The main example IDL file (`../idls/example/example.idl`) now includes MCP descriptions:

```idl
[description="Adds two integers and returns the result"] 
error_code add(int a, int b, [out, by_value] int& c);

[description="Creates a new foo object instance"] 
error_code create_foo([out] rpc::shared_ptr<xxx::i_foo>& target);

[description="Creates an enclave on the host and discards the result"] 
error_code call_host_create_enclave_and_throw_away(bool run_standard_tests);
```

When you generate code from this IDL using the RPC generator, it will automatically include MCP-compatible metadata in the generated function_info structures.