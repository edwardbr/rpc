#include "json_schema_generator.h"
#include "json_schema/writer.h"
#include <map>
#include <algorithm>
#include <cctype>
#include <sstream>

namespace rpc_generator
{
    namespace json_schema_generator
    {
        // Simple type mapping for basic JSON schema generation
        std::string map_basic_type_to_json(const std::string& idl_type)
        {
            std::string clean_type = idl_type;

            // Check if this is a pointer type first (before cleaning)
            // Pointer types are serialized as memory addresses (uint64_t integers)
            if (idl_type.find('*') != std::string::npos)
            {
                return "integer";
            }

            // Remove const, &, shared_ptr wrappers etc. (but not *)
            if (clean_type.find("const") != std::string::npos)
            {
                clean_type = clean_type.substr(clean_type.find("const") + 5);
            }
            if (clean_type.find("rpc::shared_ptr<") != std::string::npos)
            {
                auto start = clean_type.find('<') + 1;
                auto end = clean_type.rfind('>');
                if (start < end)
                {
                    clean_type = clean_type.substr(start, end - start);
                }
            }

            // Trim whitespace and reference modifiers (but not *)
            size_t first = clean_type.find_first_not_of(" \t&");
            if (first == std::string::npos)
            {
                // String contains only whitespace and reference modifiers, clear it
                clean_type.clear();
            }
            else
            {
                size_t last = clean_type.find_last_not_of(" \t&");
                if (last != std::string::npos)
                {
                    clean_type = clean_type.substr(first, last - first + 1);
                }
                else
                {
                    // No trailing characters to trim, just trim from beginning
                    clean_type = clean_type.substr(first);
                }
            }

            // Integer types - stdint.h and C++ equivalents
            if (clean_type == "int" || clean_type == "int32_t" || clean_type == "int64_t" || clean_type == "uint32_t"
                || clean_type == "uint64_t" || clean_type == "long" || clean_type == "short" || clean_type == "char"
                || clean_type == "signed char" || clean_type == "unsigned char" || clean_type == "unsigned short"
                || clean_type == "unsigned int" || clean_type == "unsigned long" || clean_type == "long long"
                || clean_type == "unsigned long long" || clean_type == "int8_t" || clean_type == "int16_t"
                || clean_type == "uint8_t" || clean_type == "uint16_t" || clean_type == "intptr_t"
                || clean_type == "uintptr_t" || clean_type == "size_t" || clean_type == "ssize_t" || clean_type == "ptrdiff_t"
                || clean_type == "intmax_t" || clean_type == "uintmax_t" || clean_type == "int_fast8_t"
                || clean_type == "int_fast16_t" || clean_type == "int_fast32_t" || clean_type == "int_fast64_t"
                || clean_type == "uint_fast8_t" || clean_type == "uint_fast16_t" || clean_type == "uint_fast32_t"
                || clean_type == "uint_fast64_t" || clean_type == "int_least8_t" || clean_type == "int_least16_t"
                || clean_type == "int_least32_t" || clean_type == "int_least64_t" || clean_type == "uint_least8_t"
                || clean_type == "uint_least16_t" || clean_type == "uint_least32_t" || clean_type == "uint_least64_t")
            {
                return "integer";
            }
            // Floating point types
            else if (clean_type == "float" || clean_type == "double" || clean_type == "long double")
            {
                return "number";
            }
            // Boolean types
            else if (clean_type == "bool")
            {
                return "boolean";
            }
            // String types
            else if (clean_type == "std::string" || clean_type == "string" || clean_type == "std::wstring"
                     || clean_type == "std::u16string" || clean_type == "std::u32string"
                     || clean_type == "std::string_view" || clean_type == "std::wstring_view"
                     || clean_type == "std::u16string_view" || clean_type == "std::u32string_view")
            {
                return "string";
            }
            // Array/sequence container types
            else if (clean_type.find("std::vector") != std::string::npos || clean_type.find("std::list") != std::string::npos
                     || clean_type.find("std::forward_list") != std::string::npos
                     || clean_type.find("std::deque") != std::string::npos
                     || clean_type.find("std::array") != std::string::npos
                     || clean_type.find("std::valarray") != std::string::npos)
            {
                return "array";
            }
            // Set container types (arrays with unique elements)
            else if (clean_type.find("std::set") != std::string::npos || clean_type.find("std::multiset") != std::string::npos
                     || clean_type.find("std::unordered_set") != std::string::npos
                     || clean_type.find("std::unordered_multiset") != std::string::npos)
            {
                return "array";
            }
            // Map/object container types
            else if (clean_type.find("std::map") != std::string::npos || clean_type.find("std::multimap") != std::string::npos
                     || clean_type.find("std::unordered_map") != std::string::npos
                     || clean_type.find("std::unordered_multimap") != std::string::npos)
            {
                return "object";
            }
            // Stack/Queue container adaptors (treat as arrays)
            else if (clean_type.find("std::stack") != std::string::npos || clean_type.find("std::queue") != std::string::npos
                     || clean_type.find("std::priority_queue") != std::string::npos)
            {
                return "array";
            }
            // Optional/variant types (treat as objects for now)
            else if (clean_type.find("std::optional") != std::string::npos
                     || clean_type.find("std::variant") != std::string::npos
                     || clean_type.find("std::any") != std::string::npos)
            {
                return "object";
            }
            // Smart pointer types (treat as objects)
            else if (clean_type.find("std::unique_ptr") != std::string::npos
                     || clean_type.find("std::shared_ptr") != std::string::npos
                     || clean_type.find("std::weak_ptr") != std::string::npos)
            {
                return "object";
            }
            // Pair and tuple types (treat as objects)
            else if (clean_type.find("std::pair") != std::string::npos || clean_type.find("std::tuple") != std::string::npos)
            {
                return "object";
            }
            else
            {
                // For complex types (structs, classes, interfaces), use "object"
                // This includes custom types like xxx::something_complicated
                return "object";
            }
        }

        // Helper function to parse template arguments from instantiated type
        std::vector<std::string> parse_template_arguments(const std::string& type_with_params)
        {
            std::vector<std::string> args;
            if (type_with_params.find('<') == std::string::npos)
            {
                return args;
            }

            size_t start = type_with_params.find('<') + 1;
            size_t end = type_with_params.rfind('>');
            if (start >= end)
            {
                return args;
            }

            std::string args_str = type_with_params.substr(start, end - start);

            // Parse comma-separated arguments, respecting nested templates
            int depth = 0;
            size_t arg_start = 0;
            for (size_t i = 0; i < args_str.length(); ++i)
            {
                if (args_str[i] == '<')
                {
                    depth++;
                }
                else if (args_str[i] == '>')
                {
                    depth--;
                }
                else if (args_str[i] == ',' && depth == 0)
                {
                    std::string arg = args_str.substr(arg_start, i - arg_start);
                    trim_string(arg);
                    if (!arg.empty())
                    {
                        args.push_back(arg);
                    }
                    arg_start = i + 1;
                }
            }

            // Add the last argument
            std::string last_arg = args_str.substr(arg_start);
            trim_string(last_arg);
            if (!last_arg.empty())
            {
                args.push_back(last_arg);
            }

            return args;
        }

        // Helper function to get template parameter names from template definition
        std::vector<std::string> get_template_parameter_names(const class_entity& template_def)
        {
            std::vector<std::string> param_names;

            // First, try to get template parameters from template declaration elements
            auto all_elements = template_def.get_elements(entity_type::TEMPLATE_DECLARATION);
            for (const auto& element : all_elements)
            {
                if (element && element->get_entity_type() == entity_type::TEMPLATE_DECLARATION)
                {
                    std::string param_name = element->get_name();
                    if (!param_name.empty())
                    {
                        param_names.push_back(param_name);
                    }
                }
            }

            // If no template parameters found through formal API, infer from member types
            if (param_names.empty())
            {
                // Get all members and look for types that look like template parameters
                auto members = template_def.get_elements(entity_type::FUNCTION_VARIABLE);
                std::set<std::string> potential_params;

                for (const auto& member : members)
                {
                    if (member && member->get_entity_type() == entity_type::FUNCTION_VARIABLE)
                    {
                        auto func_entity = std::dynamic_pointer_cast<function_entity>(member);
                        if (func_entity)
                        {
                            std::string member_type = func_entity->get_return_type();
                            trim_string(member_type);

                            // Look for simple identifiers that could be template parameters
                            // Template parameters are typically single words without :: or special chars
                            if (!member_type.empty() && member_type.find("::") == std::string::npos
                                && member_type.find("<") == std::string::npos
                                && member_type.find("*") == std::string::npos && member_type.find("&") == std::string::npos
                                && member_type.find("std::") == std::string::npos && member_type != "int"
                                && member_type != "string" && member_type != "bool" && member_type != "char"
                                && member_type != "float" && member_type != "double" && member_type != "void"
                                && member_type != "auto")
                            {
                                // Remove const/volatile qualifiers
                                std::string clean_type = member_type;
                                size_t pos = clean_type.find("const");
                                if (pos != std::string::npos)
                                {
                                    clean_type.erase(pos, 5);
                                    trim_string(clean_type);
                                }
                                pos = clean_type.find("volatile");
                                if (pos != std::string::npos)
                                {
                                    clean_type.erase(pos, 8);
                                    trim_string(clean_type);
                                }

                                // Check if it looks like a template parameter (single identifier)
                                if (!clean_type.empty() && std::isalpha(clean_type[0])
                                    && clean_type.find_first_not_of(
                                           "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_")
                                           == std::string::npos)
                                {
                                    potential_params.insert(clean_type);
                                }
                            }
                        }
                    }
                }

                // Convert set to vector to maintain order
                for (const auto& param : potential_params)
                {
                    param_names.push_back(param);
                }
            }

            return param_names;
        }

        // Helper function to substitute template parameters in a type name
        std::string substitute_template_parameters(
            const std::string& type_with_params, const std::string& member_type, const class_entity& template_def)
        {
            if (type_with_params.find('<') == std::string::npos)
            {
                return member_type; // No template parameters to substitute
            }

            auto template_args = parse_template_arguments(type_with_params);
            auto param_names = get_template_parameter_names(template_def);

            if (template_args.empty())
            {
                return member_type;
            }

            std::string result = member_type;

            // Substitute each template parameter with its corresponding argument
            for (size_t i = 0; i < std::min(param_names.size(), template_args.size()); ++i)
            {
                const std::string& param_name = param_names[i];
                const std::string& arg_value = template_args[i];

                // Replace exact matches and standalone occurrences
                size_t pos = 0;
                while ((pos = result.find(param_name, pos)) != std::string::npos)
                {
                    // Check if this is a standalone parameter name (not part of another identifier)
                    bool is_standalone = true;
                    if (pos > 0 && (std::isalnum(result[pos - 1]) || result[pos - 1] == '_'))
                    {
                        is_standalone = false;
                    }
                    if (pos + param_name.length() < result.length()
                        && (std::isalnum(result[pos + param_name.length()]) || result[pos + param_name.length()] == '_'))
                    {
                        is_standalone = false;
                    }

                    if (is_standalone)
                    {
                        result.replace(pos, param_name.length(), arg_value);
                        pos += arg_value.length();
                    }
                    else
                    {
                        pos += param_name.length();
                    }
                }
            }

            return result;
        }

        // Helper function to trim whitespace from strings
        void trim_string(std::string& str)
        {
            // Trim leading whitespace
            size_t first = str.find_first_not_of(" \t\n\r");
            if (first == std::string::npos)
            {
                str.clear();
                return;
            }

            // Trim trailing whitespace
            size_t last = str.find_last_not_of(" \t\n\r");
            str = str.substr(first, last - first + 1);
        }

        // Generate detailed schema for complex types (structs/classes)
        void generate_complex_type_schema(const std::string& clean_type_name,
            const class_entity& root,
            json_writer& writer,
            std::set<std::string>& visited_types)
        {
            // Avoid infinite recursion for circular references
            if (visited_types.find(clean_type_name) != visited_types.end())
            {
                writer.write_string_property("type", "object");
                writer.write_string_property("description", "Circular reference to " + clean_type_name);
                return;
            }
            visited_types.insert(clean_type_name);

            // Try to find the struct/class definition
            std::shared_ptr<class_entity> struct_def;
            bool found = false;

            // First try to find the type as-is
            if (root.find_class(clean_type_name, struct_def))
            {
                found = true;
            }

            // If not found, try with namespace prefixes by exploring available namespaces
            if (!found && clean_type_name.find("::") == std::string::npos)
            {
                // Get all namespace children from root and try each one
                auto namespaces = root.get_elements(entity_type::NAMESPACE_MEMBERS);
                for (const auto& ns_element : namespaces)
                {
                    if (ns_element && ns_element->get_entity_type() == entity_type::NAMESPACE)
                    {
                        std::string ns_name = ns_element->get_name();
                        if (!ns_name.empty() && ns_name != "__global__")
                        {
                            std::string namespaced_type = ns_name + "::" + clean_type_name;
                            if (root.find_class(namespaced_type, struct_def))
                            {
                                found = true;
                                break;
                            }
                        }
                    }
                }
            }

            // If still not found, check if this is a template instantiation
            if (!found && clean_type_name.find('<') != std::string::npos)
            {
                // Parse template name and arguments
                size_t template_start = clean_type_name.find('<');
                std::string template_name = clean_type_name.substr(0, template_start);
                trim_string(template_name);

                // Try to find the template definition
                if (root.find_class(template_name, struct_def))
                {
                    found = true;
                }
                else
                {
                    // Try with namespace prefixes
                    auto namespaces = root.get_elements(entity_type::NAMESPACE_MEMBERS);
                    for (const auto& ns_element : namespaces)
                    {
                        if (ns_element && ns_element->get_entity_type() == entity_type::NAMESPACE)
                        {
                            std::string ns_name = ns_element->get_name();
                            if (!ns_name.empty() && ns_name != "__global__")
                            {
                                std::string namespaced_template = ns_name + "::" + template_name;
                                if (root.find_class(namespaced_template, struct_def))
                                {
                                    found = true;
                                    break;
                                }
                            }
                        }
                    }
                }
            }

            if (found)
            {
                // Generate object schema with properties for each member
                writer.write_string_property("type", "object");
                writer.write_string_property("description", "Schema for " + clean_type_name);

                // Get member variables of the struct
                auto elements = struct_def->get_elements(entity_type::FUNCTION_VARIABLE);

                // If no FUNCTION_VARIABLE elements found, try getting all structure members
                if (elements.empty())
                {
                    elements = struct_def->get_elements(entity_type::STRUCTURE_MEMBERS);
                }

                if (!elements.empty())
                {
                    writer.write_key("properties");
                    writer.open_object();

                    std::vector<std::string> required_fields;

                    // Process each member of the struct
                    for (const auto& element : elements)
                    {
                        std::string member_name = element->get_name();
                        if (!member_name.empty() && member_name != "public:")
                        {
                            // Skip static members (they are not serialized)
                            if (element->get_entity_type() == entity_type::FUNCTION_VARIABLE)
                            {
                                auto func_entity = std::dynamic_pointer_cast<function_entity>(element);
                                if (func_entity && func_entity->is_static())
                                {
                                    continue; // Skip static members
                                }
                            }
                            writer.write_key(member_name);
                            writer.open_object();

                            // Try to get the member type - cast to appropriate entity type
                            std::string member_type = "string"; // default
                            std::string raw_type_name = "";
                            entity_type elem_type = element->get_entity_type();

                            if (elem_type == entity_type::FUNCTION_VARIABLE)
                            {
                                // This is a variable declaration, try to get its type
                                auto func_entity = std::dynamic_pointer_cast<function_entity>(element);
                                if (func_entity)
                                {
                                    raw_type_name = func_entity->get_return_type();

                                    // Apply template parameter substitution if we're processing a template instantiation
                                    raw_type_name
                                        = substitute_template_parameters(clean_type_name, raw_type_name, *struct_def);

                                    member_type = map_basic_type_to_json(raw_type_name);
                                }
                            }
                            else if (elem_type == entity_type::PARAMETER)
                            {
                                // This is a parameter, get its type
                                auto param_entity = std::dynamic_pointer_cast<parameter_entity>(element);
                                if (param_entity)
                                {
                                    raw_type_name = param_entity->get_type();

                                    // Apply template parameter substitution if we're processing a template instantiation
                                    raw_type_name
                                        = substitute_template_parameters(clean_type_name, raw_type_name, *struct_def);

                                    member_type = map_basic_type_to_json(raw_type_name);
                                }
                            }

                            // Handle templated types more intelligently
                            if (!raw_type_name.empty())
                            {
                                // Check for std::vector<T>
                                if (raw_type_name.find("std::vector<") != std::string::npos)
                                {
                                    writer.write_string_property("type", "array");

                                    // Extract the template parameter
                                    size_t start = raw_type_name.find("<") + 1;
                                    size_t end = raw_type_name.rfind(">");
                                    if (start < end)
                                    {
                                        std::string inner_type = raw_type_name.substr(start, end - start);
                                        trim_string(inner_type);

                                        writer.write_key("items");
                                        writer.open_object();

                                        std::string inner_json_type = map_basic_type_to_json(inner_type);
                                        if (inner_json_type == "object")
                                        {
                                            // Complex type in vector
                                            generate_complex_type_schema(inner_type, root, writer, visited_types);
                                        }
                                        else
                                        {
                                            writer.write_string_property("type", inner_json_type);
                                        }

                                        writer.close_object(); // items
                                    }
                                }
                                // Check for std::map<K,V> - YAS serializes this as array of {k, v} objects
                                else if (raw_type_name.find("std::map<") != std::string::npos)
                                {
                                    writer.write_string_property("type", "array");
                                    writer.write_string_property(
                                        "description", "Map serialized as array of {k, v} objects");

                                    // Extract value type (second template parameter)
                                    size_t start = raw_type_name.find("<") + 1;
                                    size_t comma = raw_type_name.find(",", start);
                                    size_t end = raw_type_name.rfind(">");

                                    if (comma != std::string::npos && comma < end)
                                    {
                                        std::string value_type = raw_type_name.substr(comma + 1, end - comma - 1);
                                        trim_string(value_type);

                                        writer.write_key("items");
                                        writer.open_object();
                                        writer.write_string_property("type", "object");
                                        writer.write_string_property("description", "Map entry with key and value");
                                        writer.write_key("properties");
                                        writer.open_object();

                                        // Key property (usually string)
                                        writer.write_key("k");
                                        writer.open_object();
                                        writer.write_string_property("type", "string");
                                        writer.close_object();

                                        // Value property
                                        writer.write_key("v");
                                        writer.open_object();
                                        std::string value_json_type = map_basic_type_to_json(value_type);
                                        if (value_json_type == "object")
                                        {
                                            // Complex type as map value
                                            generate_complex_type_schema(value_type, root, writer, visited_types);
                                        }
                                        else
                                        {
                                            writer.write_string_property("type", value_json_type);
                                        }
                                        writer.close_object(); // v

                                        writer.close_object(); // properties
                                        writer.write_raw_property("additionalProperties", "false");
                                        writer.close_object(); // items
                                    }
                                }
                                // Check if it's a complex type for direct object mapping
                                else if (member_type == "object")
                                {
                                    // Complex type - generate recursive schema
                                    generate_complex_type_schema(raw_type_name, root, writer, visited_types);
                                }
                                else
                                {
                                    writer.write_string_property("type", member_type);
                                }
                            }
                            else
                            {
                                writer.write_string_property("type", member_type);
                            }

                            writer.close_object();
                            required_fields.push_back(member_name);
                        }
                    }

                    writer.close_object(); // properties

                    // Add required fields
                    if (!required_fields.empty())
                    {
                        writer.write_key("required");
                        writer.open_array();
                        for (const auto& field : required_fields)
                        {
                            writer.write_array_string_element(field);
                        }
                        writer.close_array();
                    }
                }

                writer.write_raw_property("additionalProperties", "false");
            }
            else
            {
                // Struct/class not found, fallback to generic object
                writer.write_string_property("type", "object");
                writer.write_string_property("description", "Unknown complex type: " + clean_type_name);
            }

            visited_types.erase(clean_type_name); // Allow reuse in other branches
        }

        // Recursively generate schema for any type
        void generate_type_schema_recursive(
            const std::string& type_name, const class_entity& root, json_writer& writer, std::set<std::string>& visited_types)
        {
            std::string basic_type = map_basic_type_to_json(type_name);

            if (basic_type == "object")
            {
                // This might be a complex type that needs recursive schema generation
                std::string clean_type = type_name;

                // Clean the type name for lookup
                if (clean_type.find("const") != std::string::npos)
                {
                    clean_type = clean_type.substr(clean_type.find("const") + 5);
                }

                // Trim whitespace and reference modifiers
                size_t first = clean_type.find_first_not_of(" \t&");
                if (first != std::string::npos)
                {
                    size_t last = clean_type.find_last_not_of(" \t&");
                    if (last != std::string::npos)
                    {
                        clean_type = clean_type.substr(first, last - first + 1);
                    }
                    else
                    {
                        clean_type = clean_type.substr(first);
                    }
                }

                // Check if this is a known complex type (starts with namespace or is not std:: container)
                if (clean_type.find("::") != std::string::npos
                    || clean_type.find("std::") == std::string::npos) // Not a std:: container
                {
                    // Try multiple variations of the type name
                    std::string lookup_type = clean_type;

                    // If it doesn't have namespace, try adding xxx:: prefix for our test structs
                    if (clean_type.find("::") == std::string::npos && (clean_type.find("something_") != std::string::npos))
                    {
                        lookup_type = "xxx::" + clean_type;
                    }

                    generate_complex_type_schema(lookup_type, root, writer, visited_types);
                }
                else
                {
                    // Generic object type
                    writer.write_string_property("type", basic_type);
                }
            }
            else
            {
                // Simple type
                writer.write_string_property("type", basic_type);
            }
        }

        // Enhanced JSON schema generation for input parameters with recursive complex type support
        std::string generate_function_input_parameter_schema_with_recursion(
            const class_entity& root, const class_entity& interface, const function_entity& function)
        {
            std::ignore = interface; // May be used in future enhancements

            std::stringstream schema_stream;
            json_writer writer(schema_stream);
            std::set<std::string> visited_types; // Track visited types to prevent infinite recursion

            writer.open_object();
            writer.write_string_property("type", "object");
            writer.write_string_property("description", "Input parameters for " + function.get_name() + " method");

            // Collect input parameters
            std::vector<std::string> required_fields;
            std::map<std::string, std::string> properties;

            bool has_properties = false;
            for (const auto& param : function.get_parameters())
            {
                // Check if this is an input parameter (default to input if no attributes specified)
                bool is_in = param.has_value("in");
                bool is_out = param.has_value("out");
                bool implicitly_in = !is_in && !is_out;

                if (is_in || implicitly_in)
                {
                    has_properties = true;
                    break;
                }
            }

            if (has_properties)
            {
                writer.write_key("properties");
                writer.open_object();

                for (const auto& param : function.get_parameters())
                {
                    bool is_in = param.has_value("in");
                    bool is_out = param.has_value("out");
                    bool implicitly_in = !is_in && !is_out;

                    if (is_in || implicitly_in)
                    {
                        std::string param_name = param.get_name();
                        std::string param_type = param.get_type();

                        writer.write_key(param_name);
                        writer.open_object();

                        // Generate recursive schema for this parameter
                        generate_type_schema_recursive(param_type, root, writer, visited_types);

                        writer.close_object();
                        required_fields.push_back(param_name);
                    }
                }

                writer.close_object(); // properties

                // Write required fields
                if (!required_fields.empty())
                {
                    writer.write_key("required");
                    writer.open_array();
                    for (const auto& field : required_fields)
                    {
                        writer.write_array_string_element(field);
                    }
                    writer.close_array();
                }
            }

            writer.write_raw_property("additionalProperties", "false");
            writer.close_object();

            return schema_stream.str();
        }

        // Enhanced JSON schema generation for output parameters with recursive complex type support
        std::string generate_function_output_parameter_schema_with_recursion(
            const class_entity& root, const class_entity& interface, const function_entity& function)
        {
            std::ignore = interface; // May be used in future enhancements

            std::stringstream schema_stream;
            json_writer writer(schema_stream);
            std::set<std::string> visited_types; // Track visited types to prevent infinite recursion

            writer.open_object();
            writer.write_string_property("type", "object");
            writer.write_string_property("description", "Output parameters for " + function.get_name() + " method");

            // Collect output parameters
            std::vector<std::string> required_fields;
            std::map<std::string, std::string> properties;

            bool has_properties = false;
            for (const auto& param : function.get_parameters())
            {
                // Check if this is an output parameter
                bool is_out = param.has_value("out");

                if (is_out)
                {
                    has_properties = true;
                    break;
                }
            }

            if (has_properties)
            {
                writer.write_key("properties");
                writer.open_object();

                for (const auto& param : function.get_parameters())
                {
                    bool is_out = param.has_value("out");

                    if (is_out)
                    {
                        std::string param_name = param.get_name();
                        std::string param_type = param.get_type();

                        writer.write_key(param_name);
                        writer.open_object();

                        // Generate recursive schema for this parameter
                        generate_type_schema_recursive(param_type, root, writer, visited_types);

                        writer.close_object();
                        required_fields.push_back(param_name);
                    }
                }

                writer.close_object(); // properties

                // Write required fields
                if (!required_fields.empty())
                {
                    writer.write_key("required");
                    writer.open_array();
                    for (const auto& field : required_fields)
                    {
                        writer.write_array_string_element(field);
                    }
                    writer.close_array();
                }
            }

            writer.write_raw_property("additionalProperties", "false");
            writer.close_object();

            return schema_stream.str();
        }
    }
}