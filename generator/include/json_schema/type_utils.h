#pragma once

#include <string>
#include <algorithm>
#include <cctype>

namespace rpc_generator
{
    namespace json_schema
    {

        // Type checking utilities for basic types
        bool is_char_star(const std::string& type);
        bool is_int8(const std::string& type);
        bool is_uint8(const std::string& type);
        bool is_int16(const std::string& type);
        bool is_uint16(const std::string& type);
        bool is_int32(const std::string& type);
        bool is_uint32(const std::string& type);
        bool is_int64(const std::string& type);
        bool is_uint64(const std::string& type);
        bool is_long(const std::string& type);
        bool is_ulong(const std::string& type);
        bool is_float(const std::string& type);
        bool is_double(const std::string& type);
        bool is_bool(const std::string& type);

        // Type cleaning utilities
        void strip_reference_modifiers(std::string& type, std::string& modifiers);
        std::string unconst(const std::string& type);
        void trim_string(std::string& str);
        std::string clean_type_name(const std::string& raw_type);

    } // namespace json_schema
} // namespace rpc_generator
