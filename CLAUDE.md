<!--
Copyright (c) 2025 Edward Boggis-Rolfe
All rights reserved.
-->

# RPC++ Codebase Analysis for Claude Code

## Overview
RPC++ is a Remote Procedure Call library for modern C++ that enables type-safe communication across different execution contexts (in-process, inter-process, remote machines, embedded devices, SGX enclaves). The system uses Interface Definition Language (IDL) files to generate C++ proxy/stub code with full JSON schema generation capabilities.

## Project Structure

### Core Directories
- **`/rpc/`** - Core RPC library implementation
- **`/generator/`** - IDL code generator (creates C++ from .idl files)
- **`/submodules/idlparser/`** - IDL parsing engine with recent attributes refactoring
- **`/tests/`** - Test suite including JSON schema generation tests
- **`/telemetry/`** - Optional telemetry/logging subsystem

### Key Files
- **`CMakeLists.txt`** - Main build configuration (version 2.2.0)
- **`README.md`** - Project documentation and build instructions
- **`JSON_SCHEMA_IMPLEMENTATION_GUIDE.md`** - Complete JSON schema generation documentation
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

### 3. JSON Schema Generation Integration (COMPLETED)
**Documentation**: See `JSON_SCHEMA_IMPLEMENTATION_GUIDE.md` for complete details.

**Key Changes**:
- Extended `rpc::function_info` with `description`, `in_json_schema`, and `out_json_schema` fields
- Enhanced code generator with comprehensive JSON schema generation:
  - Template parameter substitution using first-principles detection
  - Generic namespace resolution for complex types
  - Support for nested complex types in containers (vectors, maps)
  - Template instantiations with proper parameter mapping
- Added description attribute support in IDL files
- Created comprehensive test suite in `/tests/json_schema_test/`

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

### 4. Logging System Refactoring (COMPLETED - Sept 2024)
**Locations**: 
- `/rpc/include/rpc/logger.h` - Core logging macros and fmt integration
- `/generator/src/synchronous_generator.cpp` - Generator logging cleanup  
- `/generator/src/yas_generator.cpp` - YAS generator logging cleanup
- `/tests/common/include/common/foo_impl.h` - Test code logging migration

**Major Changes**:
- **Legacy Macro Elimination**: Completely removed `LOG_STR` and `LOG_CSTR` macros across entire codebase
- **fmt::format Integration**: All logging now uses modern fmt::format with structured parameter substitution
- **New Structured Logging Macros**:
  ```cpp
  #define RPC_DEBUG(format_str, ...)    // Level 0 - Debug information
  #define RPC_TRACE(format_str, ...)    // Level 1 - Detailed tracing  
  #define RPC_INFO(format_str, ...)     // Level 2 - General information
  #define RPC_WARNING(format_str, ...)  // Level 3 - Warning conditions
  #define RPC_ERROR(format_str, ...)    // Level 4 - Error conditions
  #define RPC_CRITICAL(format_str, ...) // Level 5 - Critical failures
  ```
- **Temporary String Variable Elimination**: Removed all unnecessary string concatenation and temporary variables
- **Generator Code Cleanup**: Fixed generators to eliminate temporaries in generated logging code
- **Conditional Compilation Cleanup**: Removed unnecessary `#ifdef USE_RPC_LOGGING` blocks around single macro calls

**Migration Example**:
```cpp
// OLD STYLE (removed)
std::string debug_msg = "service::send zone: " + std::to_string(zone_id);
LOG_CSTR(debug_msg.c_str());

// NEW STYLE (current)  
RPC_DEBUG("service::send zone: {} destination_zone={}, caller_zone={}", 
          zone_id, destination_zone_id.get_val(), caller_zone_id.get_val());
```

**Performance Benefits**:
- **Zero String Copying**: Direct fmt::format eliminates intermediate string objects
- **Reduced Memory Allocations**: Fewer temporary objects means better memory efficiency  
- **Better Code Generation**: Generators produce cleaner, more efficient logging code
- **Compile-time Format Validation**: fmt::format provides compile-time format string checking

**Files Affected**: 28+ files across the codebase including core service files, generators, and test implementations

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
- **JSON Schema Tests**: Metadata extraction and schema validation
- **Build Integration**: CMake-based test execution

### Code Generation Process
1. Parse .idl files using idlparser
2. Extract attributes (including descriptions)
3. Generate C++ headers with proxy/stub code
4. Include JSON schema metadata in function_info structures
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

### Run JSON Schema Tests
```bash
cmake --build build --target simple_json_schema_metadata_test
./build/tests/json_schema_test/simple_json_schema_metadata_test
```

### Run Hierarchical Zone Fuzz Tests
```bash
cmake --build build --target fuzz_test_main
./build/output/debug/fuzz_test_main 3  # Run 3 iterations of hierarchical tests
```

**What this tests**:
- Creates 3-level zone hierarchies where child zones create their own children
- Tests cross-zone `rpc::shared_ptr` marshalling 
- Validates `place_shared_object` functionality with [in] parameters
- Property-based testing with randomized parameters
- Telemetry visualization showing zone topology graphs

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
- Enhanced JSON schema features (improved output schemas, additional format support)

## Major Update: Hierarchical Zone Graph Testing (September 2024)

**Location**: `/tests/fuzz_test/fuzz_test_main.cpp`

**Achievement**: Successfully implemented and tested true hierarchical zone creation where child zones create their own children, demonstrating complex multi-level RPC++ zone architectures.

**Key Features Implemented**:
- **Deep Hierarchical Zones**: 3+ level zone hierarchies where child zones autonomously create their own children
- **Proper Zone Lifecycle Management**: Zones kept alive through `shared_ptr` to objects, not service storage
- **Cross-Zone shared_ptr Marshalling**: Objects created in one zone and successfully passed/manipulated across multiple zone levels
- **place_shared_object Implementation**: Working [in] parameter testing with newly created objects across zones
- **Autonomous Node Architecture**: Nodes can create child zones using `rpc::service::get_current_service()`

**Technical Implementation**:
```cpp
// Child zones create their own children using local service context
auto current_service = rpc::service::get_current_service();
auto err_code = current_service->connect_to_zone<rpc::local_child_service_proxy<...>>(
    child_zone_name.c_str(), {zone_id}, parent_interface, child_node, setup_callback);
```

**Architecture Compliance**: 
- **No Service Storage**: Removed improper `root_service_` and `child_services_` storage
- **Hidden Service Principle**: Each object only interacts with current service via `get_current_service()`
- **Object-Based Zone Lifecycle**: Zones stay alive through references to objects within them

**Test Results**: 
- **25+ zones created** in various test patterns
- **3-level hierarchies verified** with telemetry visualization showing true parent-child relationships
- **Cross-zone marshalling working** with factories, caches, and autonomous nodes
- **Property-based testing** with randomized parameters demonstrating robustness

**Telemetry Output Example**:
```
Zone 2 fuzz_root
  ├─ Zone 5000 level1_0  
    ├─ Zone 6000 child_5000_6000  ← Child zone created by parent
  ├─ Zone 5001 level1_1
    ├─ Zone 6001 child_5001_6001
```

This demonstrates RPC++ can handle complex distributed zone topologies where zones act as autonomous entities creating and managing their own sub-hierarchies.

---

This codebase represents a mature RPC system with recent enhancements for JSON schema generation, improved IDL attribute handling, and comprehensive hierarchical zone testing. The build system is well-integrated and the code generation pipeline is robust and extensible.
- when running tests with:
- Always load all the .md files in the root directory and in the docs folder