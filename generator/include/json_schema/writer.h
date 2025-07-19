#pragma once

#include <iostream>
#include <string>
#include <vector>

class json_writer
{
    std::ostream& os_;
    int indent_level_ = 0;
    std::string indent_string_ = "  "; // Two spaces for indentation
    bool needs_comma_ = false;         // Tracks if a comma is needed before the next element/property

    void print_indent()
    {
        for (int i = 0; i < indent_level_; ++i)
        {
            os_ << indent_string_;
        }
    }

    void handle_comma()
    {
        if (needs_comma_)
        {
            os_ << ",\n";
            needs_comma_ = false;
        }
        else
        {
            os_ << "\n";
        }
    }

public:
    json_writer(std::ostream& os)
        : os_(os)
    {
    }

    void open_object()
    {
        handle_comma();
        print_indent();
        os_ << "{";
        indent_level_++;
        needs_comma_ = false; // No comma needed after opening brace
        os_ << "\n";
    }

    void close_object()
    {
        indent_level_--;
        os_ << "\n"; // Newline before closing brace
        print_indent();
        os_ << "}";
        needs_comma_ = true; // Comma *might* be needed after this object
    }

    void open_array()
    {
        handle_comma();
        print_indent();
        os_ << "[";
        indent_level_++;
        needs_comma_ = false; // No comma needed after opening bracket
        os_ << "\n";
    }

    void close_array()
    {
        indent_level_--;
        os_ << "\n"; // Newline before closing bracket
        print_indent();
        os_ << "]";
        needs_comma_ = true; // Comma *might* be needed after this array
    }

    // Writes "key":
    void write_key(const std::string& key)
    {
        handle_comma();
        print_indent();
        os_ << "\"" << key << "\": ";
        needs_comma_ = false; // Value follows immediately, no comma yet
    }

    // Writes a string value like "value"
    void write_string_value(const std::string& value)
    {
        // Escape quotes and backslashes in the string value
        std::string escaped_value;
        escaped_value.reserve(value.length());
        for (char c : value)
        {
            switch (c)
            {
            case '"':
                escaped_value += "\\\"";
                break;
            case '\\':
                escaped_value += "\\\\";
                break;
            case '\b':
                escaped_value += "\\b";
                break;
            case '\f':
                escaped_value += "\\f";
                break;
            case '\n':
                escaped_value += "\\n";
                break;
            case '\r':
                escaped_value += "\\r";
                break;
            case '\t':
                escaped_value += "\\t";
                break;
            default:
                if ('\x00' <= c && c <= '\x1f')
                {
                    // Represent control characters using \uXXXX notation
                    char buf[7];
                    snprintf(buf, sizeof(buf), "\\u%04x", (int)c);
                    escaped_value += buf;
                }
                else
                {
                    escaped_value += c;
                }
            }
        }
        os_ << "\"" << escaped_value << "\"";
        needs_comma_ = true; // Comma needed before next key/value or element
    }

    // Writes a raw value (e.g., number, boolean, null, or a pre-formatted object/array)
    void write_raw_value(const std::string& raw_value)
    {
        os_ << raw_value;
        needs_comma_ = true; // Comma needed before next key/value or element
    }

    // Writes a full key-value pair with string value
    void write_string_property(const std::string& key, const std::string& value)
    {
        write_key(key);
        write_string_value(value);
    }

    // Writes a full key-value pair with raw value (number, boolean, null, object, array)
    void write_raw_property(const std::string& key, const std::string& raw_value)
    {
        write_key(key);
        write_raw_value(raw_value);
    }

    // Writes a single string element in an array
    void write_array_string_element(const std::string& value)
    {
        handle_comma();
        print_indent();
        write_string_value(value);
        // needs_comma_ is already true from write_string_value
    }

    // Writes a single raw element in an array
    void write_array_raw_element(const std::string& raw_value)
    {
        handle_comma();
        print_indent();
        write_raw_value(raw_value);
        // needs_comma_ is already true from write_raw_value
    }
};
