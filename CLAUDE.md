# RPC++ Codebase Analysis for Claude Code

## Overview
RPC++ is a Remote Procedure Call library for modern C++ that enables type-safe communication across different execution contexts (in-process, inter-process, remote machines, embedded devices, SGX enclaves). The system uses Interface Definition Language (IDL) files to generate C++ proxy/stub code with full MCP (Model Context Protocol) integration.

## Project Structure

### Core Directories
- **`/rpc/`** - Core RPC library implementation
- **`/generator/`** - IDL code generator (creates C++ from .idl files)
- **`/submodules/idlparser/`** - IDL parsing engine with recent attributes refactoring
- **`/tests/`** - Test suite including MCP integration tests
- **`/telemetry/`** - Optional telemetry/logging subsystem

### Key Files
- **`CMakeLists.txt`** - Main build configuration (version 2.2.0)
- **`README.md`** - Project documentation and build instructions
- **`MCP_IMPLEMENTATION_GUIDE.md`** - Complete MCP integration documentation
- **`CMakePresets.json`** - CMake preset configurations for different build modes
- **`.vscode/settings.json`** - VSCode configuration (file associations only)

## Build System

### CMake Configuration
- **Generator**: Ninja
- **C++ Standard**: C++17
- **Presets**: Defined in `CMakePresets.json`

### Available CMake Presets
- **`Debug`** - Standard debug build
- **`Debug_SGX`** - Debug build with SGX hardware enclave support
- **`Debug_SGX_Sim`** - Debug build with SGX simulation mode
- **`Release`** - Optimized release build
- **`Release_SGX`** - Release build with SGX hardware support

### Key Base Configuration (from CMakePresets.json)
```cmake
RPC_STANDALONE=ON              # Standalone build mode
BUILD_TEST=ON                  # Enable test building
BUILD_ENCLAVE=OFF              # SGX enclave support (base: disabled)
BUILD_COROUTINE=ON             # Coroutine support enabled
DEBUG_RPC_GEN=ON               # Debug code generation
CMAKE_EXPORT_COMPILE_COMMANDS=ON # Export compile commands for tooling
```

### Build Commands
```bash
# Configure
cmake --preset Debug

# Build core library
cmake --build build --target rpc

# Build and run tests
cmake --build build --target <test_name>
./build/tests/<test_name>
```

## Recent Major Changes

### 1. JSON Schema Generation Enhancements (COMPLETED)
**Location**: `/generator/src/synchronous_generator.cpp`

**Improvements**:
- **Generic Template Parameter Detection**: Replaced hardcoded template parameter names ('T', 'U') with first-principles analysis of template definitions
- **Universal Namespace Resolution**: Dynamic discovery of all available namespaces instead of hardcoded namespace prefixes
- **Enhanced Type Resolution**: Proper handling of nested complex types in containers and template instantiations
- **Robust Substitution**: Template parameter substitution works with any parameter names and multiple parameters

**Impact**:
- Eliminated "Unknown complex type" errors for complex nested types
- Works with any namespace naming conventions (not just "xxx")
- Supports template parameters with any names (not just "T")  
- Future-proof for new IDL patterns without code changes

### 2. Attributes System Refactoring (COMPLETED)
**Location**: `/submodules/idlparser/parsers/ast_parser/coreclasses.h`

**Change**: Converted `typedef std::list<std::string> attributes;` to a full class:
```cpp
class attributes {
private:
    std::vector<std::pair<std::string, std::string>> data_;
public:
    void push_back(const std::string& str);
    void push_back(const std::pair<std::string, std::string>& pair);
    // ... iterator and utility methods
};
```

**Impact**: 
- Enables name=value attribute parsing (e.g., `[description="text"]`)
- Maintains backward compatibility for simple string attributes
- Updated all search operations to use lambdas with std::find_if
- Enhanced GetAttributes function in library_loader.cpp for value extraction

### 3. MCP Integration (COMPLETED)
**Documentation**: See `MCP_IMPLEMENTATION_GUIDE.md` for complete details.

**Key Changes**:
- Extended `rpc::function_info` with `mcp_description` and `json_schema` fields
- Enhanced code generator with comprehensive JSON schema generation:
  - Template parameter substitution using first-principles detection
  - Generic namespace resolution for complex types
  - Support for nested complex types in containers (vectors, maps)
  - Template instantiations with proper parameter mapping
- Added description attribute support in IDL files
- Created comprehensive test suite in `/tests/mcp_test/`

**Usage Example**:
```idl
[description="Adds two integers and returns the result"]
error_code add(int a, int b, [out, by_value] int& c);
```

**Supported Complex Types**:
- User-defined structs/classes in any namespace
- Template instantiations (e.g., `MyTemplate<int>`)
- Nested containers (e.g., `std::vector<MyStruct>`, `std::map<string, MyClass>`)
- Multi-parameter templates with any parameter names

## IDL System

### Parser Architecture
- **Core Parser**: `/submodules/idlparser/` 
- **AST Classes**: `coreclasses.h` contains entity, interface, and attributes classes
- **String Extraction**: `extract_string_literal()` and `extract_multiline_string_literal()` functions
- **Code Generation**: `/generator/src/synchronous_generator.cpp`

### Supported IDL Features
- Interfaces (pure virtual base classes)
- Structures with C++ STL types
- Attributes with name=value pairs
- Namespaces
- Basic types: int, string, vector, map, optional, variant

## Development Workflow

### Adding New IDL Features
1. Update parser in `/submodules/idlparser/`
2. Modify code generator in `/generator/src/`
3. Add test IDL files in `/tests/idls/`
4. Create corresponding tests
5. Update documentation

### Testing Strategy
- **Unit Tests**: Individual component testing
- **Integration Tests**: Full IDL→C++ generation pipeline
- **MCP Tests**: Metadata extraction and schema validation
- **Build Integration**: CMake-based test execution

### Code Generation Process
1. Parse .idl files using idlparser
2. Extract attributes (including descriptions)
3. Generate C++ headers with proxy/stub code
4. Include MCP metadata in function_info structures
5. Compile generated code with main project

## Transport Layer
- **In-Memory**: Direct function calls
- **Arena-based**: Different memory spaces
- **SGX Enclaves**: Secure execution environments
- **Network**: Planned for future releases
- **Serialization**: YAS library for data marshalling

## Key Dependencies
- **YAS**: Serialization framework
- **libcoro**: Coroutine support (when BUILD_COROUTINE=ON)
- **c-ares**: DNS resolution (configured in submodules)
- **range-v3**: Range library support
- **CMake 3.24+**: Build system requirement

## Development Environment
- **Recommended IDE**: VSCode with CMake Tools extension
- **Debugger**: GDB/LLDB support with debug info enabled
- **Code Style**: Modern C++17 patterns
- **Extensions**: "Microsoft MIDL and Mozilla WebIDL syntax highlighting" for .idl files

## Common Tasks

### Generate Code from IDL
```bash
cmake --build build --target <interface_name>_idl
```

### Run MCP Tests
```bash
cmake --build build --target simple_mcp_metadata_test
./build/tests/mcp_test/simple_mcp_metadata_test
```

### Debug Code Generation
Set `DEBUG_RPC_GEN=ON` in CMake to enable generator debugging output.

## Architecture Notes
- **Type Safety**: Full C++ type system support
- **Transport Independence**: Protocol-agnostic design
- **Modern C++**: Leverages C++17 features extensively
- **Performance**: Zero-copy serialization where possible
- **Extensibility**: Plugin-based transport system

## Future Considerations
- Coroutine-based async calls (in development)
- Exception-based error handling (planned)
- Additional serialization backends
- Network transport implementations
- Enhanced MCP features (output schemas, SSE support)

This codebase represents a mature RPC system with recent enhancements for MCP integration and improved IDL attribute handling. The build system is well-integrated and the code generation pipeline is robust and extensible.
- when running tests with: