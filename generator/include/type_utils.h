/*
 *   Copyright (c) 2025 Edward Boggis-Rolfe
 *   All rights reserved.
 */

#pragma once

#include <string>
#include <algorithm>
#include <cctype>
#include <iostream>
#include <fmt/format.h>

// Forward declarations
class class_entity;
class attributes;

namespace rpc_generator
{
    // Unified parameter type classification (shared across all generators)
    enum class param_type
    {
        BY_VALUE,
        REFERENCE,
        MOVE,
        POINTER,
        POINTER_REFERENCE,
        POINTER_POINTER,
        INTERFACE,
        INTERFACE_REFERENCE
    };

    // Generation contexts for different generators
    enum class generation_context
    {
        JSON_SCHEMA,
        PROXY_PARAM_IN,
        PROXY_PARAM_OUT,
        STUB_PARAM_IN,
        STUB_PARAM_OUT,
        INTERFACE_DECL,
        YAS_SERIAL,
        SEND_PARAM_IN
    };

    // Unified parameter analysis result
    struct parameter_info
    {
        param_type type;
        std::string clean_type_name;
        std::string reference_modifiers;
        bool is_in;
        bool is_out;
        bool is_const;
        bool is_interface;
        bool by_value;
    };

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

    // Unified type classification for JSON Schema
    bool is_integer_type(const std::string& type);
    bool is_numeric_type(const std::string& type);
    bool is_string_type(const std::string& type);
    bool is_boolean_type(const std::string& type);

    // Type cleaning utilities
    void strip_reference_modifiers(std::string& type, std::string& modifiers);
    std::string unconst(const std::string& type);
    void trim_string(std::string& str);
    std::string clean_type_name(const std::string& raw_type);

    // Unified parameter analysis (new)
    parameter_info analyze_parameter(const class_entity& lib, const std::string& type, const attributes& attribs);

    // Determine parameter type from analysis
    param_type classify_parameter_type(const std::string& type_name,
        const std::string& reference_modifiers,
        bool is_interface,
        bool is_out,
        bool is_const,
        bool by_value);

    // Simplified parameter processing helper (replaces duplicate analysis in generators)
    struct param_analysis_result
    {
        parameter_info info;
        bool should_process_as_input;
        bool should_process_as_output;
    };

    // Unified parameter analysis and filtering
    param_analysis_result analyze_parameter_with_context(
        const class_entity& lib, const std::string& type, const attributes& attribs);

    // Base renderer interface - pure virtual base class for all generators
    // This provides a common polymorphic interface to replace template-based rendering
    class base_renderer
    {
    public:
        virtual ~base_renderer() = default;

        // Core rendering functions - implemented by derived classes with generator-specific behavior
        // Each function corresponds to a param_type enum value
        // option parameter controls marshalling behavior between proxy and host
        // from_host parameter indicates direction for generators that need it (ignored by generators that don't)
        virtual std::string render_by_value(int option,
            bool from_host,
            const class_entity& lib,
            const std::string& name,
            bool is_in,
            bool is_out,
            bool is_const,
            const std::string& type_name,
            uint64_t& count)
            = 0;

        virtual std::string render_reference(int option,
            bool from_host,
            const class_entity& lib,
            const std::string& name,
            bool is_in,
            bool is_out,
            bool is_const,
            const std::string& type_name,
            uint64_t& count)
            = 0;

        virtual std::string render_move(int option,
            bool from_host,
            const class_entity& lib,
            const std::string& name,
            bool is_in,
            bool is_out,
            bool is_const,
            const std::string& type_name,
            uint64_t& count)
            = 0;

        virtual std::string render_pointer(int option,
            bool from_host,
            const class_entity& lib,
            const std::string& name,
            bool is_in,
            bool is_out,
            bool is_const,
            const std::string& type_name,
            uint64_t& count)
            = 0;

        virtual std::string render_pointer_reference(int option,
            bool from_host,
            const class_entity& lib,
            const std::string& name,
            bool is_in,
            bool is_out,
            bool is_const,
            const std::string& type_name,
            uint64_t& count)
            = 0;

        virtual std::string render_pointer_pointer(int option,
            bool from_host,
            const class_entity& lib,
            const std::string& name,
            bool is_in,
            bool is_out,
            bool is_const,
            const std::string& type_name,
            uint64_t& count)
            = 0;

        virtual std::string render_interface(int option,
            bool from_host,
            const class_entity& lib,
            const std::string& name,
            bool is_in,
            bool is_out,
            bool is_const,
            const std::string& type_name,
            uint64_t& count)
            = 0;

        virtual std::string render_interface_reference(int option,
            bool from_host,
            const class_entity& lib,
            const std::string& name,
            bool is_in,
            bool is_out,
            bool is_const,
            const std::string& type_name,
            uint64_t& count)
            = 0;

        // Dispatch function that maps param_type enum to specific render function
        std::string render_param_type(param_type type,
            int option,
            bool from_host,
            const class_entity& lib,
            const std::string& name,
            bool is_in,
            bool is_out,
            bool is_const,
            const std::string& type_name,
            uint64_t& count);
    };

    // Unified do_in_param function using polymorphic base_renderer
    bool do_in_param_unified(base_renderer& renderer,
        int option,
        bool from_host,
        const class_entity& lib,
        const std::string& name,
        const std::string& type,
        const attributes& attribs,
        uint64_t& count,
        std::string& output);

    // Unified do_out_param function using polymorphic base_renderer
    bool do_out_param_unified(base_renderer& renderer,
        int option,
        bool from_host,
        const class_entity& lib,
        const std::string& name,
        const std::string& type,
        const attributes& attribs,
        uint64_t& count,
        std::string& output);

} // namespace rpc_generator
