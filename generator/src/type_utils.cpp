/*
 *   Copyright (c) 2025 Edward Boggis-Rolfe
 *   All rights reserved.
 */

#include "type_utils.h"
#include "helpers.h"
#include "attributes.h"
#include <algorithm>
#include <cctype>

namespace rpc_generator
{

    // Type checking utilities for basic types
    bool is_char_star(const std::string& type)
    {
        return type == "char*" || type == "const char*" || type == "char *" || type == "const char *";
    }

    bool is_int8(const std::string& type)
    {
        return type == "int8_t" || type == "signed char";
    }

    bool is_uint8(const std::string& type)
    {
        return type == "uint8_t" || type == "unsigned char";
    }

    bool is_int16(const std::string& type)
    {
        return type == "int16_t" || type == "short" || type == "short int" || type == "signed short"
               || type == "signed short int";
    }

    bool is_uint16(const std::string& type)
    {
        return type == "uint16_t" || type == "unsigned short" || type == "unsigned short int";
    }

    bool is_int32(const std::string& type)
    {
        return type == "int32_t" || type == "int" || type == "signed int" || type == "signed" || type == "long"
               || type == "signed long";
    }

    bool is_uint32(const std::string& type)
    {
        return type == "uint32_t" || type == "unsigned int" || type == "unsigned" || type == "unsigned long";
    }

    bool is_int64(const std::string& type)
    {
        return type == "int64_t" || type == "long long" || type == "signed long long" || type == "long long int"
               || type == "signed long long int";
    }

    bool is_uint64(const std::string& type)
    {
        return type == "uint64_t" || type == "unsigned long long" || type == "unsigned long long int";
    }

    bool is_long(const std::string& type)
    {
        return type == "long" || type == "signed long" || type == "long int" || type == "signed long int";
    }

    bool is_ulong(const std::string& type)
    {
        return type == "unsigned long" || type == "unsigned long int";
    }

    bool is_float(const std::string& type)
    {
        return type == "float";
    }

    bool is_double(const std::string& type)
    {
        return type == "double" || type == "long double";
    }

    bool is_bool(const std::string& type)
    {
        return type == "bool";
    }

    // Type cleaning utilities
    void strip_reference_modifiers(std::string& type, std::string& modifiers)
    {
        modifiers.clear();

        // Remove leading and trailing whitespace first
        size_t first = type.find_first_not_of(" \t\n\r");
        if (first == std::string::npos)
        {
            type.clear();
            return;
        }

        size_t last = type.find_last_not_of(" \t\n\r");
        type = type.substr(first, last - first + 1);

        // Strip reference and pointer modifiers from end
        while (!type.empty() && (type.back() == '&' || type.back() == '*' || type.back() == ' ' || type.back() == '\t'))
        {
            if (type.back() == '&')
            {
                modifiers = "&" + modifiers;
            }
            else if (type.back() == '*')
            {
                modifiers = "*" + modifiers;
            }
            type.pop_back();
        }

        // Remove trailing whitespace after stripping references and pointers
        last = type.find_last_not_of(" \t\n\r");
        if (last != std::string::npos)
        {
            type = type.substr(0, last + 1);
        }
    }

    std::string unconst(const std::string& type)
    {
        std::string result = type;

        // Remove "const" keyword from various positions
        size_t pos = 0;
        while ((pos = result.find("const", pos)) != std::string::npos)
        {
            // Check if "const" is a standalone word
            bool is_word_boundary_before = (pos == 0) || (!std::isalnum(result[pos - 1]) && result[pos - 1] != '_');
            bool is_word_boundary_after
                = (pos + 5 >= result.length()) || (!std::isalnum(result[pos + 5]) && result[pos + 5] != '_');

            if (is_word_boundary_before && is_word_boundary_after)
            {
                result.erase(pos, 5); // Remove "const"

                // Remove extra spaces that might be left
                while (pos < result.length() && result[pos] == ' ')
                {
                    result.erase(pos, 1);
                }
                if (pos > 0 && result[pos - 1] == ' ')
                {
                    result.erase(pos - 1, 1);
                    pos--;
                }
            }
            else
            {
                pos++;
            }
        }

        // Clean up any duplicate spaces
        size_t space_pos = 0;
        while ((space_pos = result.find("  ", space_pos)) != std::string::npos)
        {
            result.erase(space_pos, 1);
        }

        // Trim leading and trailing spaces
        size_t first = result.find_first_not_of(" \t\n\r");
        if (first == std::string::npos)
        {
            return "";
        }

        size_t last = result.find_last_not_of(" \t\n\r");
        return result.substr(first, last - first + 1);
    }

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

    std::string clean_type_name(const std::string& raw_type)
    {
        std::string cleaned = raw_type;
        auto first_good_char = std::find_if_not(
            cleaned.begin(), cleaned.end(), [](unsigned char c) { return std::isspace(c) || std::iscntrl(c); });
        cleaned.erase(cleaned.begin(), first_good_char);

        auto last_good_char = std::find_if_not(
            cleaned.rbegin(), cleaned.rend(), [](unsigned char c) { return std::isspace(c) || std::iscntrl(c); });
        cleaned.erase(last_good_char.base(), cleaned.end());

        return cleaned;
    }

    // Unified type classification for JSON Schema
    bool is_integer_type(const std::string& type)
    {
        return is_int8(type) || is_uint8(type) || is_int16(type) || is_uint16(type) || is_int32(type) || is_uint32(type)
               || is_int64(type) || is_uint64(type) || is_long(type) || is_ulong(type) || type == "size_t"
               || type == "ptrdiff_t";
    }

    bool is_numeric_type(const std::string& type)
    {
        return is_integer_type(type) || is_float(type) || is_double(type);
    }

    bool is_string_type(const std::string& type)
    {
        return type == "std::string" || type == "string" || type == "std::wstring" || type == "wstring"
               || is_char_star(type);
    }

    bool is_boolean_type(const std::string& type)
    {
        return is_bool(type);
    }

    // Unified parameter analysis (replaces scattered logic across generators)
    parameter_info analyze_parameter(const class_entity& lib, const std::string& type, const attributes& attribs)
    {
        parameter_info info;

        // Extract attributes
        info.is_in = is_in_param(attribs);
        info.is_out = is_out_param(attribs);
        info.is_const = is_const_param(attribs);
        info.by_value = attribs.has_value(attribute_types::by_value_param);

        // Parse type and modifiers
        std::string type_name = type;
        strip_reference_modifiers(type_name, info.reference_modifiers);
        info.clean_type_name = type_name;

        // Check if interface
        info.is_interface = is_interface_param(lib, type);

        // Classify parameter type
        info.type = classify_parameter_type(
            type_name, info.reference_modifiers, info.is_interface, info.is_out, info.is_const, info.by_value);

        return info;
    }

    // Determine parameter type from analysis
    param_type classify_parameter_type(const std::string& /* type_name */,
        const std::string& reference_modifiers,
        bool is_interface,
        bool is_out,
        bool is_const,
        bool by_value)
    {
        if (is_interface)
        {
            if (reference_modifiers.empty() || (reference_modifiers == "&" && (is_const || !is_out)))
            {
                return param_type::INTERFACE;
            }
            else if (reference_modifiers == "&")
            {
                return param_type::INTERFACE_REFERENCE;
            }
        }
        else
        {
            if (reference_modifiers.empty())
            {
                return param_type::BY_VALUE;
            }
            else if (reference_modifiers == "&")
            {
                return by_value ? param_type::BY_VALUE : param_type::REFERENCE;
            }
            else if (reference_modifiers == "&&")
            {
                return param_type::MOVE;
            }
            else if (reference_modifiers == "*")
            {
                return param_type::POINTER;
            }
            else if (reference_modifiers == "*&")
            {
                return param_type::POINTER_REFERENCE;
            }
            else if (reference_modifiers == "**")
            {
                return param_type::POINTER_POINTER;
            }
        }

        // Default fallback
        return param_type::BY_VALUE;
    }

    // Unified parameter analysis and filtering (replaces duplicate logic)
    param_analysis_result analyze_parameter_with_context(
        const class_entity& lib, const std::string& type, const attributes& attribs)
    {
        param_analysis_result result;

        // Perform unified parameter analysis
        result.info = analyze_parameter(lib, type, attribs);

        // Determine processing context
        result.should_process_as_input = !(result.info.is_out && !result.info.is_in);
        result.should_process_as_output = result.info.is_out;

        return result;
    }

    // Implementation of base_renderer::render_param_type
    std::string base_renderer::render_param_type(param_type type,
        int option,
        bool from_host,
        const class_entity& lib,
        const std::string& name,
        bool is_in,
        bool is_out,
        bool is_const,
        const std::string& type_name,
        uint64_t& count)
    {
        switch (type)
        {
        case param_type::BY_VALUE:
            return render_by_value(option, from_host, lib, name, is_in, is_out, is_const, type_name, count);
        case param_type::REFERENCE:
            return render_reference(option, from_host, lib, name, is_in, is_out, is_const, type_name, count);
        case param_type::MOVE:
            return render_move(option, from_host, lib, name, is_in, is_out, is_const, type_name, count);
        case param_type::POINTER:
            return render_pointer(option, from_host, lib, name, is_in, is_out, is_const, type_name, count);
        case param_type::POINTER_REFERENCE:
            return render_pointer_reference(option, from_host, lib, name, is_in, is_out, is_const, type_name, count);
        case param_type::POINTER_POINTER:
            return render_pointer_pointer(option, from_host, lib, name, is_in, is_out, is_const, type_name, count);
        case param_type::INTERFACE:
            return render_interface(option, from_host, lib, name, is_in, is_out, is_const, type_name, count);
        case param_type::INTERFACE_REFERENCE:
            return render_interface_reference(option, from_host, lib, name, is_in, is_out, is_const, type_name, count);
        default:
            throw std::runtime_error("Unsupported parameter type in base_renderer");
        }
    }

    // Unified do_in_param function using polymorphic base_renderer
    bool do_in_param_unified(base_renderer& renderer,
        int option,
        bool from_host,
        const class_entity& lib,
        const std::string& name,
        const std::string& type,
        const attributes& attribs,
        uint64_t& count,
        std::string& output)
    {
        // UNIFIED: Same logic across all generators using polymorphic renderer
        auto analysis = analyze_parameter_with_context(lib, type, attribs);

        if (!analysis.should_process_as_input)
            return false;

        const auto& info = analysis.info;

        // SIMPLIFIED: Use polymorphic dispatch instead of complex template logic
        try
        {
            output = renderer.render_param_type(
                info.type, option, from_host, lib, name, info.is_in, info.is_out, info.is_const, info.clean_type_name, count);
            return true;
        }
        catch (const std::exception& e)
        {
            std::cerr << fmt::format("Error rendering parameter {} {}: {}", type, name, e.what());
            throw;
        }
    }

    // Unified do_out_param function using polymorphic base_renderer
    bool do_out_param_unified(base_renderer& renderer,
        int option,
        bool from_host,
        const class_entity& lib,
        const std::string& name,
        const std::string& type,
        const attributes& attribs,
        uint64_t& count,
        std::string& output)
    {
        // UNIFIED: Same logic across all generators using polymorphic renderer
        auto analysis = analyze_parameter_with_context(lib, type, attribs);

        if (!analysis.should_process_as_output)
            return false;

        const auto& info = analysis.info;

        // UNIFIED: Same validation across all generators
        if (info.is_const)
        {
            std::cerr << fmt::format("out parameters cannot be const");
            throw fmt::format("out parameters cannot be const");
        }

        if (info.reference_modifiers.empty())
        {
            std::cerr << fmt::format("out parameters require data to be sent by pointer or reference {} {} ", type, name);
            throw fmt::format("out parameters require data to be sent by pointer or reference {} {} ", type, name);
        }

        // UNIFIED: Handle output-specific validation and rendering
        try
        {
            switch (info.type)
            {
            case param_type::BY_VALUE:
                output = renderer.render_by_value(
                    option, from_host, lib, name, info.is_in, info.is_out, info.is_const, info.clean_type_name, count);
                break;
            case param_type::REFERENCE:
                // For output parameters, references are rendered as BY_VALUE (original behavior)
                output = renderer.render_by_value(
                    option, from_host, lib, name, info.is_in, info.is_out, info.is_const, info.clean_type_name, count);
                break;
            case param_type::MOVE:
                throw std::runtime_error("out call rvalue references is not possible");
            case param_type::POINTER:
                throw std::runtime_error("passing [out] by_pointer data by * will not work use a ** or *&");
            case param_type::POINTER_REFERENCE:
                output = renderer.render_pointer_reference(
                    option, from_host, lib, name, info.is_in, info.is_out, info.is_const, info.clean_type_name, count);
                break;
            case param_type::POINTER_POINTER:
                output = renderer.render_pointer_pointer(
                    option, from_host, lib, name, info.is_in, info.is_out, info.is_const, info.clean_type_name, count);
                break;
            case param_type::INTERFACE:
                throw std::runtime_error("INTERFACE does not support out vals");
            case param_type::INTERFACE_REFERENCE:
                output = renderer.render_interface_reference(
                    option, from_host, lib, name, info.is_in, info.is_out, info.is_const, info.clean_type_name, count);
                break;
            default:
                std::cerr << fmt::format(
                    "Unsupported output parameter type for {} {} with modifiers {}", type, name, info.reference_modifiers);
                throw fmt::format(
                    "Unsupported output parameter type for {} {} with modifiers {}", type, name, info.reference_modifiers);
            }
            return true;
        }
        catch (const std::exception& e)
        {
            std::cerr << fmt::format("Error rendering output parameter {} {}: {}", type, name, e.what());
            throw;
        }
    }

} // namespace rpc_generator
