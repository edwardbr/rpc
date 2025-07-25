#pragma once

#include <string>
#include <set>
#include <vector>
#include "../../submodules/idlparser/parsers/ast_parser/coreclasses.h"
#include "json_schema/writer.h"

namespace rpc_generator
{
    namespace json_schema_generator
    {
        // Simple type mapping for basic JSON schema generation
        std::string map_basic_type_to_json(const std::string& idl_type);

        // Helper function to parse template arguments from instantiated type
        std::vector<std::string> parse_template_arguments(const std::string& type_with_params);

        // Helper function to get template parameter names from template definition
        std::vector<std::string> get_template_parameter_names(const class_entity& template_def);

        // Helper function to substitute template parameters in a type name
        std::string substitute_template_parameters(
            const std::string& type_with_params, const std::string& member_type, const class_entity& template_def);

        // Helper function to trim whitespace from strings
        void trim_string(std::string& str);

        // Generate detailed schema for complex types (structs/classes)
        void generate_complex_type_schema(const std::string& clean_type_name,
            const class_entity& root,
            json_writer& writer,
            std::set<std::string>& visited_types);

        // Recursively generate schema for any type
        void generate_type_schema_recursive(
            const std::string& type_name, const class_entity& root, json_writer& writer, std::set<std::string>& visited_types);

        // Enhanced JSON schema generation for input parameters with recursive complex type support
        std::string generate_function_input_parameter_schema_with_recursion(
            const class_entity& root, const class_entity& interface, const function_entity& function);

        // Enhanced JSON schema generation for output parameters with recursive complex type support
        std::string generate_function_output_parameter_schema_with_recursion(
            const class_entity& root, const class_entity& interface, const function_entity& function);
    }
}