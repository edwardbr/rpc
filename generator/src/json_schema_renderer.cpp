#include "json_schema_renderer.h"
#include "type_utils.h"

namespace rpc_generator
{
    std::string json_schema_renderer::get_json_schema_type(const std::string& cpp_type) const
    {
        // Centralized type mapping - replaces scattered if/else chains
        if (is_integer_type(cpp_type))
        {
            return "integer";
        }
        else if (is_numeric_type(cpp_type))
        {
            return "number";
        }
        else if (is_string_type(cpp_type))
        {
            return "string";
        }
        else if (is_boolean_type(cpp_type))
        {
            return "boolean";
        }
        else if (cpp_type.find("std::vector") == 0 || 
                 cpp_type.find("std::array") == 0 ||
                 cpp_type.find("std::list") == 0)
        {
            return "array";
        }
        else if (cpp_type.find("std::map") == 0)
        {
            return "object";
        }
        else
        {
            // Complex user-defined types
            return "object";
        }
    }

} // namespace rpc_generator