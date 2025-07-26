#include "json_schema/type_utils.h"
#include <algorithm>
#include <cctype>

namespace rpc_generator
{
    namespace json_schema
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

            // Strip reference modifiers from end
            while (!type.empty() && (type.back() == '&' || type.back() == ' ' || type.back() == '\t'))
            {
                if (type.back() == '&')
                {
                    modifiers = "&" + modifiers;
                }
                type.pop_back();
            }

            // Remove trailing whitespace after stripping references
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

    } // namespace json_schema
} // namespace rpc_generator
