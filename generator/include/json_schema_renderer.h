#pragma once

#include "type_utils.h"
#include <string>

namespace rpc_generator
{
    // Demonstration of unified JSON Schema renderer using SAX-style pattern
    class json_schema_renderer
    {
    public:
        // Template-based rendering for different parameter types
        template<param_type PT>
        std::string render_json_type(const parameter_info& info) const;
        
        // Centralized mapping from C++ types to JSON Schema types
        std::string get_json_schema_type(const std::string& cpp_type) const;
    };

    // Specializations for each parameter type
    template<>
    inline std::string json_schema_renderer::render_json_type<param_type::BY_VALUE>(const parameter_info& info) const
    {
        return get_json_schema_type(info.clean_type_name);
    }

    template<>
    inline std::string json_schema_renderer::render_json_type<param_type::REFERENCE>(const parameter_info& info) const
    {
        return get_json_schema_type(info.clean_type_name);
    }

    template<>
    inline std::string json_schema_renderer::render_json_type<param_type::POINTER>(const parameter_info& info) const
    {
        // All pointer types serialize as memory addresses (integers)
        return "integer";
    }

    template<>
    inline std::string json_schema_renderer::render_json_type<param_type::POINTER_REFERENCE>(const parameter_info& info) const
    {
        return "integer";
    }

    template<>
    inline std::string json_schema_renderer::render_json_type<param_type::POINTER_POINTER>(const parameter_info& info) const
    {
        return "integer";
    }

    template<>
    inline std::string json_schema_renderer::render_json_type<param_type::INTERFACE>(const parameter_info& info) const
    {
        return "object"; // Interface descriptors are objects
    }

    template<>
    inline std::string json_schema_renderer::render_json_type<param_type::INTERFACE_REFERENCE>(const parameter_info& info) const
    {
        return "object";
    }

    template<>
    inline std::string json_schema_renderer::render_json_type<param_type::MOVE>(const parameter_info& info) const
    {
        return get_json_schema_type(info.clean_type_name);
    }

} // namespace rpc_generator