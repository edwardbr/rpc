/*
 *   Copyright (c) 2025 Edward Boggis-Rolfe
 *   All rights reserved.
 */
#include <type_traits>
#include <algorithm>
#include <tuple>
#include <type_traits>
#include "coreclasses.h"
#include "cpp_parser.h"
#include "helpers.h"

#include "attributes.h"
#include "rpc_attributes.h"

extern "C"
{
#include "sha3.h"
}
#include <filesystem>
#include <sstream>

#include "writer.h"

#include "interface_declaration_generator.h"
#include "fingerprint_generator.h"
#include "synchronous_generator.h"
#include "json_schema/writer.h"
#include "json_schema/generator.h"
#include "json_schema/per_function_generator.h"
#include "type_utils.h"
#include <map>

namespace synchronous_generator
{

    struct protocol_version_descriptor
    {
        const char* macro;
        const char* symbol;
        uint64_t value;
    };

    constexpr protocol_version_descriptor protocol_versions[] = {
        {"RPC_V3", "rpc::VERSION_3", 3},
        {"RPC_V2", "rpc::VERSION_2", 2},
    };

    enum print_type
    {
        PROXY_PREPARE_IN,
        PROXY_PREPARE_IN_INTERFACE_ID,
        PROXY_MARSHALL_IN,
        PROXY_OUT_DECLARATION,
        PROXY_MARSHALL_OUT,
        PROXY_VALUE_RETURN,
        PROXY_CLEAN_IN,

        STUB_DEMARSHALL_DECLARATION,
        STUB_MARSHALL_IN,
        STUB_PARAM_WRAP,
        STUB_PARAM_CAST,
        STUB_ADD_REF_OUT_PREDECLARE,
        STUB_ADD_REF_OUT,
        STUB_MARSHALL_OUT
    };

    // Polymorphic renderer adapter that implements base_renderer interface
    class polymorphic_renderer : public rpc_generator::base_renderer
    {
    public:
        polymorphic_renderer() = default;

        // Implement pure virtual functions from base_renderer
        std::string render_by_value(int option,
            bool from_host,
            const class_entity& lib,
            const std::string& name,
            bool is_in,
            bool is_out,
            bool is_const,
            const std::string& type_name,
            uint64_t& count) override;

        std::string render_reference(int option,
            bool from_host,
            const class_entity& lib,
            const std::string& name,
            bool is_in,
            bool is_out,
            bool is_const,
            const std::string& type_name,
            uint64_t& count) override;

        std::string render_move(int option,
            bool from_host,
            const class_entity& lib,
            const std::string& name,
            bool is_in,
            bool is_out,
            bool is_const,
            const std::string& type_name,
            uint64_t& count) override;

        std::string render_pointer(int option,
            bool from_host,
            const class_entity& lib,
            const std::string& name,
            bool is_in,
            bool is_out,
            bool is_const,
            const std::string& type_name,
            uint64_t& count) override;

        std::string render_pointer_reference(int option,
            bool from_host,
            const class_entity& lib,
            const std::string& name,
            bool is_in,
            bool is_out,
            bool is_const,
            const std::string& type_name,
            uint64_t& count) override;

        std::string render_pointer_pointer(int option,
            bool from_host,
            const class_entity& lib,
            const std::string& name,
            bool is_in,
            bool is_out,
            bool is_const,
            const std::string& type_name,
            uint64_t& count) override;

        std::string render_interface(int option,
            bool from_host,
            const class_entity& lib,
            const std::string& name,
            bool is_in,
            bool is_out,
            bool is_const,
            const std::string& type_name,
            uint64_t& count) override;

        std::string render_interface_reference(int option,
            bool from_host,
            const class_entity& lib,
            const std::string& name,
            bool is_in,
            bool is_out,
            bool is_const,
            const std::string& type_name,
            uint64_t& count) override;
    };

    // Implementation functions for polymorphic_renderer
    std::string polymorphic_renderer::render_by_value(int option,
        bool from_host,
        const class_entity& lib,
        const std::string& name,
        bool is_in,
        bool is_out,
        bool is_const,
        const std::string& object_type,
        uint64_t& count)
    {
        std::ignore = from_host;
        std::ignore = lib;
        std::ignore = is_in;
        std::ignore = is_out;
        std::ignore = is_const;
        std::ignore = count;

        print_type pt = static_cast<print_type>(option);
        switch (pt)
        {
        case PROXY_MARSHALL_IN:
            return fmt::format("{0}, ", name);
        case PROXY_MARSHALL_OUT:
            return fmt::format("{0}, ", name);
        case STUB_DEMARSHALL_DECLARATION:
            return fmt::format("{} {}_{{}}", object_type, name);
        case STUB_MARSHALL_IN:
            return fmt::format("{}_, ", name);
        case STUB_PARAM_CAST:
            return fmt::format("{}_", name);
        case STUB_MARSHALL_OUT:
            return fmt::format("{0}_, ", name);
        default:
            return "";
        }
    }

    std::string polymorphic_renderer::render_reference(int option,
        bool from_host,
        const class_entity& lib,
        const std::string& name,
        bool is_in,
        bool is_out,
        bool is_const,
        const std::string& object_type,
        uint64_t& count)
    {
        std::ignore = from_host;
        std::ignore = lib;
        std::ignore = is_in;
        std::ignore = is_const;
        std::ignore = count;

        if (is_out)
        {
            throw std::runtime_error("REFERENCE does not support out vals");
        }

        print_type pt = static_cast<print_type>(option);
        switch (pt)
        {
        case PROXY_MARSHALL_IN:
            return fmt::format("{0}, ", name);
        case PROXY_MARSHALL_OUT:
            return fmt::format("{0}, ", name);
        case STUB_DEMARSHALL_DECLARATION:
            return fmt::format("{} {}_{{}}", object_type, name);
        case STUB_MARSHALL_IN:
            return fmt::format("{}_, ", name);
        case STUB_PARAM_CAST:
            return fmt::format("{}_", name);
        default:
            return "";
        }
    }

    std::string polymorphic_renderer::render_move(int option,
        bool from_host,
        const class_entity& lib,
        const std::string& name,
        bool is_in,
        bool is_out,
        bool is_const,
        const std::string& object_type,
        uint64_t& count)
    {
        std::ignore = from_host;
        std::ignore = lib;
        std::ignore = is_in;
        std::ignore = count;

        if (is_out)
        {
            throw std::runtime_error("MOVE does not support out vals");
        }
        if (is_const)
        {
            throw std::runtime_error("MOVE does not support const vals");
        }

        print_type pt = static_cast<print_type>(option);
        switch (pt)
        {
        case PROXY_MARSHALL_IN:
            return fmt::format("std::move({0}), ", name);
        case PROXY_MARSHALL_OUT:
            return fmt::format("{0}, ", name);
        case STUB_DEMARSHALL_DECLARATION:
            return fmt::format("{} {}_{{}}", object_type, name);
        case STUB_MARSHALL_IN:
            return fmt::format("{}_, ", name);
        case STUB_PARAM_CAST:
            return fmt::format("std::move({}_)", name);
        case STUB_MARSHALL_OUT:
            return fmt::format("{0}_, ", name);
        default:
            return "";
        }
    }

    std::string polymorphic_renderer::render_pointer(int option,
        bool from_host,
        const class_entity& lib,
        const std::string& name,
        bool is_in,
        bool is_out,
        bool is_const,
        const std::string& object_type,
        uint64_t& count)
    {
        std::ignore = from_host;
        std::ignore = lib;
        std::ignore = is_in;
        std::ignore = is_const;
        std::ignore = count;
        if (is_out)
        {
            throw std::runtime_error("POINTER does not support out vals");
        }

        print_type pt = static_cast<print_type>(option);
        switch (pt)
        {
        case PROXY_MARSHALL_IN:
            return fmt::format("(uint64_t){}, ", name);
        case PROXY_MARSHALL_OUT:
            return fmt::format("(uint64_t){}, ", count);
        case STUB_DEMARSHALL_DECLARATION:
            return fmt::format("uint64_t {}_{{}}", name);
        case STUB_MARSHALL_IN:
            return fmt::format("{}_, ", name);
        case STUB_PARAM_CAST:
            return fmt::format("({}*){}_", object_type, name);
        default:
            return "";
        }
    }

    std::string polymorphic_renderer::render_pointer_reference(int option,
        bool from_host,
        const class_entity& lib,
        const std::string& name,
        bool is_in,
        bool is_out,
        bool is_const,
        const std::string& object_type,
        uint64_t& count)
    {
        std::ignore = from_host;
        std::ignore = lib;
        std::ignore = is_in;
        std::ignore = count;

        if (is_const && is_out)
        {
            throw std::runtime_error("POINTER_REFERENCE does not support const out vals");
        }
        print_type pt = static_cast<print_type>(option);
        switch (pt)
        {
        case PROXY_MARSHALL_IN:
            return fmt::format("{0}_, ", name);
        case PROXY_MARSHALL_OUT:
            return fmt::format("{0}_, ", name);
        case STUB_DEMARSHALL_DECLARATION:
            return fmt::format("{}* {}_ = nullptr", object_type, name);
        case STUB_PARAM_CAST:
            return fmt::format("{}_", name);
        case PROXY_OUT_DECLARATION:
            return fmt::format("uint64_t {}_ = 0;", name);
        case STUB_MARSHALL_OUT:
            return fmt::format("(uint64_t){}_, ", name);
        case PROXY_VALUE_RETURN:
            return fmt::format("{} = ({}*){}_;", name, object_type, name);
        default:
            return "";
        }
    }

    std::string polymorphic_renderer::render_pointer_pointer(int option,
        bool from_host,
        const class_entity& lib,
        const std::string& name,
        bool is_in,
        bool is_out,
        bool is_const,
        const std::string& object_type,
        uint64_t& count)
    {
        std::ignore = from_host;
        std::ignore = lib;
        std::ignore = is_in;
        std::ignore = is_out;
        std::ignore = is_const;
        std::ignore = count;

        print_type pt = static_cast<print_type>(option);
        switch (pt)
        {
        case PROXY_MARSHALL_IN:
            return fmt::format("{0}_, ", name);
        case PROXY_MARSHALL_OUT:
            return fmt::format("{0}_, ", name);
        case STUB_DEMARSHALL_DECLARATION:
            return fmt::format("{}* {}_ = nullptr", object_type, name);
        case STUB_PARAM_CAST:
            return fmt::format("&{}_", name);
        case PROXY_VALUE_RETURN:
            return fmt::format("*{} = ({}*){}_;", name, object_type, name);
        case PROXY_OUT_DECLARATION:
            return fmt::format("uint64_t {}_ = 0;", name);
        case STUB_MARSHALL_OUT:
            return fmt::format("(uint64_t){}_, ", name);
        default:
            return "";
        }
    }

    std::string polymorphic_renderer::render_interface(int option,
        bool from_host,
        const class_entity& lib,
        const std::string& name,
        bool is_in,
        bool is_out,
        bool is_const,
        const std::string& object_type,
        uint64_t& count)
    {
        std::ignore = from_host;
        std::ignore = lib;
        std::ignore = is_in;
        std::ignore = is_const;
        std::ignore = count;

        if (is_out)
        {
            throw std::runtime_error("INTERFACE does not support out vals");
        }

        print_type pt = static_cast<print_type>(option);
        switch (pt)
        {
        case PROXY_PREPARE_IN:
            return fmt::format("rpc::shared_ptr<rpc::object_stub> {}_stub_;", name);
        case PROXY_PREPARE_IN_INTERFACE_ID:
            return fmt::format(
                "RPC_ASSERT(rpc::are_in_same_zone(this, {0}.get()));\n"
                "\t\t\tauto {0}_stub_id_ = CO_AWAIT proxy_bind_in_param(__rpc_sp->get_remote_rpc_version(), "
                "{0}, {0}_stub_);",
                name);
        case PROXY_MARSHALL_IN:
        {
            auto ret = fmt::format("{0}_stub_id_, ", name);
            count++;
            return ret;
        }
        case PROXY_MARSHALL_OUT:
            return fmt::format("{0}_, ", name);

        case PROXY_CLEAN_IN:
            return fmt::format("if({0}_stub_) {0}_stub_->release_from_service();", name);

        case STUB_DEMARSHALL_DECLARATION:
            return fmt::format(R"__(rpc::interface_descriptor {0}_object_{{}};
                    uint64_t {0}_zone_ = 0)__",
                name);
        case STUB_MARSHALL_IN:
        {
            auto ret = fmt::format("{}_object_, ", name);
            count++;
            return ret;
        }
        case STUB_PARAM_WRAP:
            return fmt::format(R"__(
                {0} {1};
                if(__rpc_ret == rpc::error::OK() && {1}_object_.destination_zone_id.is_set() && {1}_object_.object_id.is_set())
                {{
                    auto target_stub_strong = target_stub_.lock();
                    if (target_stub_strong)
                    {{
                        auto& zone_ = target_stub_strong->get_zone();
                        __rpc_ret = CO_AWAIT rpc::stub_bind_in_param(protocol_version, zone_, caller_channel_zone_id, caller_zone_id, {1}_object_, {1});
                    }}
                    else
                    {{
                        assert(false);
                        __rpc_ret = rpc::error::ZONE_NOT_FOUND();
                    }}
                }}
)__",
                object_type,
                name);
        case STUB_PARAM_CAST:
            return fmt::format("{}", name);
        case STUB_MARSHALL_OUT:
            return fmt::format("(uint64_t){}, ", name);
        case PROXY_VALUE_RETURN:
        case PROXY_OUT_DECLARATION:
            return fmt::format("  rpc::interface_descriptor {}_;", name);
        default:
            return "";
        }
    }

    std::string polymorphic_renderer::render_interface_reference(int option,
        bool from_host,
        const class_entity& lib,
        const std::string& name,
        bool is_in,
        bool is_out,
        bool is_const,
        const std::string& object_type,
        uint64_t& count)
    {
        std::ignore = from_host;
        std::ignore = lib;
        std::ignore = is_in;
        std::ignore = is_out;
        std::ignore = is_const;
        std::ignore = count;

        switch (option)
        {
        case PROXY_PREPARE_IN:
            return fmt::format("rpc::shared_ptr<rpc::object_stub> {}_stub_;", name);
        case PROXY_PREPARE_IN_INTERFACE_ID:
            return fmt::format(
                "RPC_ASSERT(rpc::are_in_same_zone(this, {0}.get()));\n"
                "\t\t\tauto {0}_stub_id_ = CO_AWAIT proxy_bind_in_param(__rpc_sp->get_remote_rpc_version(), "
                "{0}, {0}_stub_);",
                name);
        case PROXY_MARSHALL_IN:
        {
            auto ret = fmt::format("{0}_stub_id_, ", name, count);
            count++;
            return ret;
        }
        case PROXY_MARSHALL_OUT:
            return fmt::format("{0}_, ", name);

        case PROXY_CLEAN_IN:
            return fmt::format("if({0}_stub_) {0}_stub_->release_from_service();", name);

        case STUB_DEMARSHALL_DECLARATION:
            return fmt::format("{} {}", object_type, name);
        case STUB_PARAM_CAST:
            return name;
        case PROXY_VALUE_RETURN:
            return fmt::format("auto {0}_ret = CO_AWAIT rpc::proxy_bind_out_param(__rpc_sp, {0}_, "
                               "__rpc_sp->get_zone_id().as_caller(), {0}); std::ignore = {0}_ret;",
                name);
        case PROXY_OUT_DECLARATION:
            return fmt::format("rpc::interface_descriptor {}_;", name);
        case STUB_ADD_REF_OUT_PREDECLARE:
            return fmt::format("rpc::interface_descriptor {0}_;", name);
        case STUB_ADD_REF_OUT:
            return fmt::format("{0}_ = CO_AWAIT stub_bind_out_param(zone_, protocol_version, "
                               "caller_channel_zone_id, caller_zone_id, {0});",
                name);
        case STUB_MARSHALL_OUT:
            return fmt::format("{}_, ", name);
        default:
            return "";
        }
    };

    bool do_in_param(print_type option,
        bool from_host,
        const class_entity& lib,
        const std::string& name,
        const std::string& type,
        const attributes& attribs,
        uint64_t& count,
        std::string& output)
    {
        // UNIFIED: Use polymorphic renderer with print_type option
        polymorphic_renderer r;
        return rpc_generator::do_in_param_unified(
            r, static_cast<int>(option), from_host, lib, name, type, attribs, count, output);
    }

    bool do_out_param(print_type option,
        bool from_host,
        const class_entity& lib,
        const std::string& name,
        const std::string& type,
        const attributes& attribs,
        uint64_t& count,
        std::string& output)
    {
        // UNIFIED: Use polymorphic renderer with print_type option
        polymorphic_renderer r;
        return rpc_generator::do_out_param_unified(
            r, static_cast<int>(option), from_host, lib, name, type, attribs, count, output);
    }

    void write_method(bool from_host,
        const class_entity& m_ob,
        writer& proxy,
        writer& stub,
        const std::string& interface_name,
        const std::shared_ptr<function_entity>& function,
        int& function_count,
        bool catch_stub_exceptions,
        const std::vector<std::string>& rethrow_exceptions)
    {
        if (function->get_entity_type() == entity_type::FUNCTION_METHOD)
        {
            std::string scoped_namespace;
            interface_declaration_generator::build_scoped_name(&m_ob, scoped_namespace);

            stub("case {}:", function_count);
            stub("{{");
            stub("// Validate encoding format support");
            stub("if (enc != rpc::encoding::yas_binary &&");
            stub("    enc != rpc::encoding::yas_compressed_binary &&");
            stub("    enc != rpc::encoding::yas_json &&");
            stub("    enc != rpc::encoding::enc_default)");
            stub("{{");
            stub("    CO_RETURN rpc::error::INCOMPATIBLE_SERIALISATION();");
            stub("}}");

            proxy.print_tabs();
            proxy.raw("virtual CORO_TASK({}) {}(", function->get_return_type(), function->get_name());
            bool has_parameter = false;
            for (auto& parameter : function->get_parameters())
            {
                if (has_parameter)
                {
                    proxy.raw(", ");
                }
                has_parameter = true;
                render_parameter(proxy, m_ob, parameter);
            }
            bool function_is_const = function->has_value("const");
            if (function_is_const)
            {
                proxy.raw(") const override\n");
            }
            else
            {
                proxy.raw(") override\n");
            }
            proxy("{{");

            proxy("auto __rpc_op = casting_interface::get_object_proxy(*this);");
            proxy("auto __rpc_sp = __rpc_op->get_service_proxy();");
            proxy("auto __rpc_encoding = __rpc_sp->get_encoding();");
            proxy("auto __rpc_version = __rpc_sp->get_remote_rpc_version();");
            proxy("const auto __rpc_min_version = std::max<std::uint64_t>(rpc::LOWEST_SUPPORTED_VERSION, 1);");
            proxy("#ifdef USE_RPC_TELEMETRY");
            proxy("if (auto telemetry_service = rpc::telemetry_service_manager::get(); telemetry_service)");
            proxy("{{");
            proxy("telemetry_service->on_interface_proxy_send(\"{0}::{1}\", "
                  "__rpc_sp->get_zone_id(), "
                  "__rpc_sp->get_destination_zone_id(), "
                  "__rpc_op->get_object_id(), {{{0}_proxy::get_id(__rpc_version)}}, {{{2}}});",
                interface_name,
                function->get_name(),
                function_count);
            proxy("}}");
            proxy("#endif");

            {
                stub("//STUB_DEMARSHALL_DECLARATION");
                uint64_t count = 1;
                for (auto& parameter : function->get_parameters())
                {
                    std::string output;
                    if (do_in_param(STUB_DEMARSHALL_DECLARATION,
                            from_host,
                            m_ob,
                            parameter.get_name(),
                            parameter.get_type(),
                            parameter,
                            count,
                            output))
                        ;
                    else
                        do_out_param(STUB_DEMARSHALL_DECLARATION,
                            from_host,
                            m_ob,
                            parameter.get_name(),
                            parameter.get_type(),
                            parameter,
                            count,
                            output);
                    stub("{};", output);
                }
            }

            proxy("std::vector<char> __rpc_out_buf(RPC_OUT_BUFFER_SIZE); //max size using short string optimisation");
            proxy("auto __rpc_ret = rpc::error::OK();");
            
            proxy("//PROXY_PREPARE_IN");
    
            proxy("if (__rpc_version < __rpc_min_version)");
            proxy("{{");
            proxy("CO_RETURN rpc::error::INVALID_VERSION();");
            proxy("}}");
            uint64_t count = 1;
            for (auto& parameter : function->get_parameters())
            {
                std::string output;
                {
                    if (!do_in_param(
                            PROXY_PREPARE_IN, from_host, m_ob, parameter.get_name(), parameter.get_type(), parameter, count, output))
                        continue;
                    proxy(output);

                    if (!do_in_param(PROXY_PREPARE_IN_INTERFACE_ID,
                            from_host,
                            m_ob,
                            parameter.get_name(),
                            parameter.get_type(),
                            parameter,
                            count,
                            output))
                        continue;

                    proxy(output);
                }
                count++;
            }            
            
            proxy("while (__rpc_version >= __rpc_min_version)");
            proxy("{{");
            proxy("std::vector<char> __rpc_in_buf;");

            {
                proxy.print_tabs();
                proxy.raw("__rpc_ret = {}proxy_serialiser<rpc::serialiser::yas, rpc::encoding>::{}(",
                    scoped_namespace,
                    function->get_name());
                stub.print_tabs();
                stub.raw("auto __rpc_ret = {}stub_deserialiser<rpc::serialiser::yas, rpc::encoding>::{}(",
                    scoped_namespace,
                    function->get_name());
                count = 1;
                for (auto& parameter : function->get_parameters())
                {
                    std::string output;
                    {
                        if (!do_in_param(
                                PROXY_MARSHALL_IN, from_host, m_ob, parameter.get_name(), parameter.get_type(), parameter, count, output))
                            continue;

                        proxy.raw(output);
                    }
                    count++;
                }

                count = 1;
                for (auto& parameter : function->get_parameters())
                {
                    std::string output;
                    {
                        if (!do_in_param(
                                STUB_MARSHALL_IN, from_host, m_ob, parameter.get_name(), parameter.get_type(), parameter, count, output))
                            continue;

                        stub.raw(output);
                    }
                    count++;
                }
                proxy.raw("__rpc_in_buf, __rpc_sp->get_encoding());\n");
                proxy("if(__rpc_ret != rpc::error::OK())");
                proxy("  CO_RETURN __rpc_ret;");
                stub.raw("in_buf_, in_size_, enc);\n");
                stub("if(__rpc_ret != rpc::error::OK())");
                stub("  CO_RETURN __rpc_ret;");
            }

            std::string tag = function->get_value("tag");
            if (tag.empty())
                tag = "0";

            proxy("__rpc_ret = CO_AWAIT __rpc_op->send(__rpc_version, __rpc_encoding, (uint64_t){}, {}::get_id(__rpc_version), {{{}}}, __rpc_in_buf.size(), "
                  "__rpc_in_buf.data(), __rpc_out_buf);",
                tag,
                interface_name,
                function_count);


            proxy("if(__rpc_ret == rpc::error::INVALID_VERSION())");
            proxy("{{");
            proxy("if(__rpc_version == __rpc_min_version)");
            proxy("{{");
            proxy("__rpc_out_buf.clear();");
            proxy("CO_RETURN __rpc_ret;");
            proxy("}}");
            proxy("--__rpc_version;");
            proxy("__rpc_sp->update_remote_rpc_version(__rpc_version);");
            proxy("__rpc_out_buf = std::vector<char>(RPC_OUT_BUFFER_SIZE);");
            proxy("continue;");
            proxy("}}");

            proxy("if(__rpc_ret == rpc::error::INCOMPATIBLE_SERIALISATION())");
            proxy("{{");
            proxy("// Try fallback to yas_json if current encoding is not supported");
            proxy("if(__rpc_encoding != rpc::encoding::yas_json)");
            proxy("{{");
            proxy("__rpc_sp->set_encoding(rpc::encoding::yas_json);");
            proxy("__rpc_encoding = rpc::encoding::yas_json;");
            proxy("__rpc_out_buf = std::vector<char>(RPC_OUT_BUFFER_SIZE);");
            proxy("continue;");
            proxy("}}");
            proxy("else");
            proxy("{{");
            proxy("// Already using yas_json, no more fallback options");
            proxy("CO_RETURN __rpc_ret;");
            proxy("}}");
            proxy("}}");

            proxy("if(__rpc_ret >= rpc::error::MIN() && __rpc_ret <= rpc::error::MAX())");
            proxy("{{");
            proxy("//if you fall into this rabbit hole ensure that you have added any error offsets compatible with your error code system to the rpc library");
            proxy("//this is only here to handle rpc generated errors and not application errors");
            proxy("//clean up any input stubs, this code has to assume that the destination is behaving correctly");
            proxy("RPC_ERROR(\"failed in {}\");", function->get_name());
            proxy("__rpc_out_buf.clear();");
            proxy("CO_RETURN __rpc_ret;");
            proxy("}}");

            proxy("break;");
            proxy("}}");

            stub("//STUB_PARAM_WRAP");

            {
                uint64_t count = 1;
                for (auto& parameter : function->get_parameters())
                {
                    std::string output;
                    if (!do_in_param(
                            STUB_PARAM_WRAP, from_host, m_ob, parameter.get_name(), parameter.get_type(), parameter, count, output))
                        do_out_param(
                            STUB_PARAM_WRAP, from_host, m_ob, parameter.get_name(), parameter.get_type(), parameter, count, output);
                    stub.raw("{}", output);
                }
            }

            stub("//STUB_PARAM_CAST");
            stub("if(__rpc_ret == rpc::error::OK())");
            stub("{{");
            if (catch_stub_exceptions)
            {
                stub("try");
                stub("{{");
            }

            stub.print_tabs();
            stub.raw("__rpc_ret = CO_AWAIT __rpc_target_->{}(", function->get_name());

            {
                bool has_param = false;
                uint64_t count = 1;
                for (auto& parameter : function->get_parameters())
                {
                    std::string output;
                    if (!do_in_param(
                            STUB_PARAM_CAST, from_host, m_ob, parameter.get_name(), parameter.get_type(), parameter, count, output))
                        do_out_param(
                            STUB_PARAM_CAST, from_host, m_ob, parameter.get_name(), parameter.get_type(), parameter, count, output);
                    if (has_param)
                    {
                        stub.raw(",");
                    }
                    has_param = true;
                    stub.raw("{}", output);
                }
            }
            stub.raw(");\n");
            if (catch_stub_exceptions)
            {
                stub("}}");

                for (auto& rethrow_stub_exception : rethrow_exceptions)
                {
                    stub("catch({}& __ex)", rethrow_stub_exception);
                    stub("{{");
                    stub("throw __ex;");
                    stub("}}");
                }

                stub("#ifdef USE_RPC_LOGGING");
                stub("catch(const std::exception& ex)");
                stub("{{");
                stub("RPC_ERROR(\"Exception has occurred in an {} implementation in function {} {{}}\", ex.what());",
                     interface_name,
                     function->get_name());
                stub("__rpc_ret = rpc::error::EXCEPTION();");
                stub("}}");
                stub("#endif");
                stub("catch(...)");
                stub("{{");
                stub("RPC_ERROR(\"Exception has occurred in an {} implementation in function {}\");",
                     interface_name,
                     function->get_name());
                stub("__rpc_ret = rpc::error::EXCEPTION();");
                stub("}}");
            }

            stub("}}");

            {
                uint64_t count = 1;
                proxy("//PROXY_OUT_DECLARATION");
                for (auto& parameter : function->get_parameters())
                {
                    count++;
                    std::string output;
                    if (do_in_param(
                            PROXY_OUT_DECLARATION, from_host, m_ob, parameter.get_name(), parameter.get_type(), parameter, count, output))
                        continue;
                    if (!do_out_param(
                            PROXY_OUT_DECLARATION, from_host, m_ob, parameter.get_name(), parameter.get_type(), parameter, count, output))
                        continue;

                    proxy(output);
                }
            }
            {
                stub("//STUB_ADD_REF_OUT_PREDECLARE");
                uint64_t count = 1;
                for (auto& parameter : function->get_parameters())
                {
                    count++;
                    std::string output;

                    if (!do_out_param(STUB_ADD_REF_OUT_PREDECLARE,
                            from_host,
                            m_ob,
                            parameter.get_name(),
                            parameter.get_type(),
                            parameter,
                            count,
                            output))
                        continue;

                    stub(output);
                }

                count = 1;
                bool has_preamble = false;
                for (auto& parameter : function->get_parameters())
                {
                    count++;
                    std::string output;

                    if (!do_out_param(
                            STUB_ADD_REF_OUT, from_host, m_ob, parameter.get_name(), parameter.get_type(), parameter, count, output))
                        continue;

                    if (!has_preamble && !output.empty())
                    {
                        stub("//STUB_ADD_REF_OUT");
                        stub("if(__rpc_ret < rpc::error::MIN() || __rpc_ret > rpc::error::MAX())");
                        stub("{{");
                        stub("auto target_stub_strong = target_stub_.lock();");
                        stub("if (target_stub_strong)");
                        stub("{{");
                        stub("auto& zone_ = target_stub_strong->get_zone();");
                        has_preamble = true;
                    }
                    stub(output);
                }
                if (has_preamble)
                {
                    stub("}}");
                    stub("else");
                    stub("{{");
                    stub("assert(false);");
                    stub("}}");
                    stub("}}");
                }
            }
            {
                uint64_t count = 1;
                proxy.print_tabs();
                proxy.raw("auto __receiver_result = {}proxy_deserialiser<rpc::serialiser::yas, rpc::encoding>::{}(",
                    scoped_namespace,
                    function->get_name());

                stub.print_tabs();
                stub.raw(
                    "{}stub_serialiser<rpc::serialiser::yas, rpc::encoding>::{}(", scoped_namespace, function->get_name());

                for (auto& parameter : function->get_parameters())
                {
                    count++;
                    std::string output;
                    if (!do_out_param(
                            PROXY_MARSHALL_OUT, from_host, m_ob, parameter.get_name(), parameter.get_type(), parameter, count, output))
                        continue;
                    proxy.raw(output);

                    if (!do_out_param(
                            STUB_MARSHALL_OUT, from_host, m_ob, parameter.get_name(), parameter.get_type(), parameter, count, output))
                        continue;

                    stub.raw(output);
                }
                proxy.raw("__rpc_out_buf.data(), __rpc_out_buf.size(), __rpc_sp->get_encoding());\n");
                proxy("if(__receiver_result != rpc::error::OK())");
                proxy("  __rpc_ret = __receiver_result;");

                stub.raw("__rpc_out_buf, enc);\n");
            }
            stub("CO_RETURN __rpc_ret;");

            proxy("//PROXY_VALUE_RETURN");
            {
                uint64_t count = 1;
                for (auto& parameter : function->get_parameters())
                {
                    count++;
                    std::string output;
                    if (do_in_param(
                            PROXY_VALUE_RETURN, from_host, m_ob, parameter.get_name(), parameter.get_type(), parameter, count, output))
                        continue;
                    if (!do_out_param(
                            PROXY_VALUE_RETURN, from_host, m_ob, parameter.get_name(), parameter.get_type(), parameter, count, output))
                        continue;

                    proxy(output);
                }
            }
            proxy("//PROXY_CLEAN_IN");
            {
                uint64_t count = 1;
                for (auto& parameter : function->get_parameters())
                {
                    std::string output;
                    {
                        if (!do_in_param(
                                PROXY_CLEAN_IN, from_host, m_ob, parameter.get_name(), parameter.get_type(), parameter, count, output))
                            continue;

                        proxy(output);
                    }
                    count++;
                }
            }

            proxy("CO_RETURN __rpc_ret;");
            proxy("}}");
            proxy("");

            function_count++;
            stub("}}");
            stub("break;");
        }
    }

    void write_interface(bool from_host,
        const class_entity& m_ob,
        writer& proxy,
        writer& stub,
        bool catch_stub_exceptions,
        const std::vector<std::string>& rethrow_exceptions)
    {
        if (m_ob.is_in_import())
            return;

        auto interface_name = std::string(m_ob.get_entity_type() == entity_type::LIBRARY ? "i_" : "") + m_ob.get_name();

        std::string base_class_declaration;
        auto bc = m_ob.get_base_classes();
        if (!bc.empty())
        {

            base_class_declaration = " : ";
            int i = 0;
            for (auto base_class : bc)
            {
                if (i)
                    base_class_declaration += ", ";
                base_class_declaration += base_class->get_name();
                i++;
            }
        }

        // generate the get_function_info function for the interface
        {
            proxy("std::vector<rpc::function_info> {0}::get_function_info()", interface_name);
            proxy("{{");
            proxy("std::vector<rpc::function_info> functions;");

            // generate unambiguous alias
            auto full_name = get_full_name(m_ob, true, false, ".");

            const auto& library = get_root(m_ob);
            int function_count = 1;
            for (auto& function : m_ob.get_functions())
            {
                if (function->get_entity_type() != entity_type::FUNCTION_METHOD)
                    continue;

                std::string tag = function->get_value("tag");
                if (tag.empty())
                    tag = "0";

                bool marshalls_interfaces = false;

                for (auto parameter : function->get_parameters())
                {
                    std::string type_name = parameter.get_type();
                    std::string reference_modifiers;
                    rpc_generator::strip_reference_modifiers(type_name, reference_modifiers);

                    marshalls_interfaces = is_interface_param(library, parameter.get_type());
                }

                // Get description attribute
                std::string description = function->get_value("description");
                if (!description.empty() && description.front() == '"' && description.back() == '"')
                {
                    description = description.substr(1, description.length() - 2);
                }

                // Generate JSON schemas for function parameters
                std::string in_json_schema;
                std::string out_json_schema;
                if (!marshalls_interfaces)
                {
                    in_json_schema
                        = json_schema::generate_function_input_parameter_schema_with_recursion(library, m_ob, *function);
                    out_json_schema
                        = json_schema::generate_function_output_parameter_schema_with_recursion(library, m_ob, *function);
                }

                proxy("functions.emplace_back(rpc::function_info{{\"{0}.{1}\", \"{1}\", {{{2}}}, (uint64_t){3}, "
                      "{4}, R\"__({5})__\", R\"__({6})__\", R\"__({7})__\"}});",
                    full_name,
                    function->get_name(),
                    function_count,
                    tag,
                    marshalls_interfaces,
                    description,
                    in_json_schema,
                    out_json_schema);
                function_count++;
            }
            proxy("return functions;");
            proxy("}}");
        }

        proxy("class {0}_proxy : public rpc::proxy_impl<{0}>", interface_name);
        proxy("{{");
        proxy("{}_proxy(rpc::shared_ptr<rpc::object_proxy> object_proxy) : ", interface_name);
        proxy("  rpc::proxy_impl<{}>(object_proxy)", interface_name);
        proxy("{{");
        proxy("#ifdef USE_RPC_TELEMETRY");
        proxy("auto __rpc_op = casting_interface::get_object_proxy(*this);");
        proxy("auto __rpc_sp = __rpc_op->get_service_proxy();");
        proxy("if (auto telemetry_service = rpc::telemetry_service_manager::get(); telemetry_service)");
        proxy("{{");
        proxy("telemetry_service->on_interface_proxy_creation(\"{0}\", "
              "__rpc_sp->get_zone_id(), "
              "__rpc_sp->get_destination_zone_id(), __rpc_op->get_object_id(), "
              "{{{0}_proxy::get_id(__rpc_version)}});",
            interface_name);
        proxy("}}");
        proxy("#endif");
        proxy("}}");
        proxy("mutable rpc::weak_ptr<{}_proxy> weak_this_;", interface_name);
        proxy("public:");
        proxy("");
        proxy("virtual ~{}_proxy()", interface_name);
        proxy("{{");
        proxy("#ifdef USE_RPC_TELEMETRY");
        proxy("if (auto telemetry_service = rpc::telemetry_service_manager::get(); telemetry_service)");
        proxy("{{");
        proxy("auto __rpc_op = casting_interface::get_object_proxy(*this);");
        proxy("auto __rpc_sp = __rpc_op->get_service_proxy();");
        proxy("telemetry_service->on_interface_proxy_deletion("
              "__rpc_sp->get_zone_id(), "
              "__rpc_sp->get_destination_zone_id(), __rpc_op->get_object_id(), "
              "{{{0}_proxy::get_id(__rpc_version)}});",
            interface_name);
        proxy("}}");
        proxy("#endif");
        proxy("}}");
        proxy("[[nodiscard]] static rpc::shared_ptr<{}> create(const rpc::shared_ptr<rpc::object_proxy>& "
              "object_proxy)",
            interface_name);
        proxy("{{");
        proxy("auto __rpc_ret = rpc::shared_ptr<{0}_proxy>(new {0}_proxy(object_proxy));", interface_name);
        proxy("__rpc_ret->weak_this_ = __rpc_ret;", interface_name);
        proxy("return rpc::static_pointer_cast<{}>(__rpc_ret);", interface_name);
        proxy("}}");
        proxy("rpc::shared_ptr<{0}_proxy> shared_from_this(){{return "
              "rpc::shared_ptr<{0}_proxy>(weak_this_);}}",
            interface_name);
        proxy("");

        stub("CORO_TASK(int) {0}_stub::call(uint64_t protocol_version, rpc::encoding enc, rpc::caller_channel_zone "
             "caller_channel_zone_id, rpc::caller_zone caller_zone_id, rpc::method method_id, size_t in_size_, "
             "const char* in_buf_, std::vector<char>& "
             "__rpc_out_buf)",
            interface_name);
        stub("{{");

        bool has_methods = false;
        for (auto& function : m_ob.get_functions())
        {
            if (function->get_entity_type() != entity_type::FUNCTION_METHOD)
                continue;
            has_methods = true;
        }

        if (has_methods)
        {
            stub("switch(method_id.get_val())");
            stub("{{");

            int function_count = 1;
            for (auto& function : m_ob.get_functions())
            {
                if (function->get_entity_type() == entity_type::FUNCTION_METHOD)
                    write_method(
                        from_host, m_ob, proxy, stub, interface_name, function, function_count, catch_stub_exceptions, rethrow_exceptions);
            }

            stub("default:");
            stub("RPC_ERROR(\"Invalid method ID - unknown method in stub\");");
            stub("CO_RETURN rpc::error::INVALID_METHOD_ID();");
            stub("}};");
        }
        proxy("}};");
        proxy("");

        stub("RPC_ERROR(\"Invalid method ID - no methods found\");");
        stub("CO_RETURN rpc::error::INVALID_METHOD_ID();");
        stub("}}");
        stub("");
    };

    void write_stub_factory(const class_entity& m_ob, writer& stub, std::set<std::string>& done)
    {
        auto interface_name = std::string(m_ob.get_entity_type() == entity_type::LIBRARY ? "i_" : "") + m_ob.get_name();
        auto owner = m_ob.get_owner();
        std::string ns = interface_name;
        while (!owner->get_name().empty())
        {
            ns = owner->get_name() + "::" + ns;
            owner = owner->get_owner();
        }
        if (done.find(ns) != done.end())
            return;
        done.insert(ns);

        stub("srv->add_interface_stub_factory(::{0}::get_id, "
             "std::make_shared<std::function<rpc::shared_ptr<rpc::i_interface_stub>(const "
             "rpc::shared_ptr<rpc::i_interface_stub>&)>>([](const rpc::shared_ptr<rpc::i_interface_stub>& "
             "original) -> rpc::shared_ptr<rpc::i_interface_stub>",
            ns);
        stub("{{");
        stub("auto ci = original->get_castable_interface();");
        stub("{{");
        stub("auto* tmp = const_cast<::{0}*>(static_cast<const "
             "::{0}*>(ci->query_interface(::{0}::get_id(rpc::get_version()))));",
            ns);
        stub("if(tmp != nullptr)");
        stub("{{");
        stub("rpc::shared_ptr<::{0}> tmp_ptr(ci, tmp);", ns);
        stub("return rpc::static_pointer_cast<rpc::i_interface_stub>(::{}_stub::create(tmp_ptr, "
             "original->get_object_stub()));",
            ns);
        stub("}}");
        stub("}}");
        stub("return nullptr;");
        stub("}}));");
    }

    void write_stub_cast_factory(const class_entity& m_ob, writer& stub)
    {
        auto interface_name = std::string(m_ob.get_entity_type() == entity_type::LIBRARY ? "i_" : "") + m_ob.get_name();
        stub("int {}_stub::cast(rpc::interface_ordinal interface_id, rpc::shared_ptr<rpc::i_interface_stub>& "
             "new_stub)",
            interface_name);
        stub("{{");
        stub("auto& service = get_object_stub().lock()->get_zone();");
        stub("int __rpc_ret = service.create_interface_stub(interface_id, {}::get_id, shared_from_this(), "
             "new_stub);",
            interface_name);
        stub("return __rpc_ret;");
        stub("}}");
    }

    void write_interface_forward_declaration(const class_entity& m_ob, writer& header, writer& proxy, writer& stub)
    {
        header("class {};", m_ob.get_name());
        proxy("class {}_proxy;", m_ob.get_name());

        auto interface_name = std::string(m_ob.get_entity_type() == entity_type::LIBRARY ? "i_" : "") + m_ob.get_name();

        stub("class {0}_stub : public rpc::i_interface_stub", interface_name);
        stub("{{");
        stub("rpc::shared_ptr<{}> __rpc_target_;", interface_name);
        stub("rpc::weak_ptr<rpc::object_stub> target_stub_;", interface_name);
        stub("");
        stub("{0}_stub(const rpc::shared_ptr<{0}>& __rpc_target, rpc::weak_ptr<rpc::object_stub> "
             "__rpc_target_stub) : ",
            interface_name);
        stub("  __rpc_target_(__rpc_target),", interface_name);
        stub("  target_stub_(__rpc_target_stub)");
        stub("  {{}}");
        stub("mutable rpc::weak_ptr<{}_stub> weak_this_;", interface_name);
        stub("");
        stub("public:");
        stub("virtual ~{0}_stub() = default;", interface_name);
        stub("static rpc::shared_ptr<{0}_stub> create(const rpc::shared_ptr<{0}>& __rpc_target, "
             "rpc::weak_ptr<rpc::object_stub> __rpc_target_stub)",
            interface_name);
        stub("{{");
        stub("auto __rpc_ret = rpc::shared_ptr<{0}_stub>(new {0}_stub(__rpc_target, __rpc_target_stub));", interface_name);
        stub("__rpc_ret->weak_this_ = __rpc_ret;", interface_name);
        stub("return __rpc_ret;", interface_name);
        stub("}}");
        stub("rpc::shared_ptr<{0}_stub> shared_from_this(){{return rpc::shared_ptr<{0}_stub>(weak_this_);}}",
            interface_name);
        stub("");
        stub("rpc::interface_ordinal get_interface_id(uint64_t rpc_version) const override");
        stub("{{");
        stub("return {{{}::get_id(rpc_version)}};", interface_name);
        stub("}}");
        stub("virtual rpc::shared_ptr<rpc::casting_interface> get_castable_interface() const override {{ return "
             "rpc::static_pointer_cast<rpc::casting_interface>(__rpc_target_); }}",
            interface_name);

        stub("rpc::weak_ptr<rpc::object_stub> get_object_stub() const override {{ return target_stub_;}}");
        stub("void* get_pointer() const override {{ return __rpc_target_.get();}}");
        stub("CORO_TASK(int) call(uint64_t protocol_version, rpc::encoding enc, rpc::caller_channel_zone "
             "caller_channel_zone_id, rpc::caller_zone caller_zone_id, rpc::method method_id, size_t in_size_, "
             "const char* in_buf_, std::vector<char>& "
             "__rpc_out_buf) override;");
        stub("int cast(rpc::interface_ordinal interface_id, rpc::shared_ptr<rpc::i_interface_stub>& new_stub) "
             "override;");
        stub("}};");
        stub("");
    }

    void write_enum_forward_declaration(const entity& ent, writer& header)
    {
        if (!ent.is_in_import())
        {
            auto& enum_entity = static_cast<const class_entity&>(ent);
            if (enum_entity.get_base_classes().empty())
                header("enum class {}", enum_entity.get_name());
            else
                header("enum class {} : {}", enum_entity.get_name(), enum_entity.get_base_classes().front()->get_name());
            header("{{");
            auto enum_vals = enum_entity.get_functions();
            for (auto& enum_val : enum_vals)
            {
                if (enum_val->get_return_type().empty())
                    header("{},", enum_val->get_name());
                else
                    header("{} = {},", enum_val->get_name(), enum_val->get_return_type());
            }
            header("}};");
        }
    }

    void write_typedef_forward_declaration(const entity& ent, writer& header)
    {
        if (!ent.is_in_import())
        {
            auto& cls = static_cast<const class_entity&>(ent);
            header("using {} = {};", cls.get_name(), cls.get_alias_name());
        }
    }

    void write_struct_id(const class_entity& m_ob, writer& header)
    {
        if (m_ob.is_in_import())
            return;

        header("");
        header("/****************************************************************************/");
        if (!m_ob.get_is_template())
            header("template<>");
        else
        {
            header.print_tabs();
            header.raw("template<");
            bool first_pass = true;
            for (const auto& param : m_ob.get_template_params())
            {
                if (!first_pass)
                    header.raw(", ");
                first_pass = false;

                template_deduction deduction;
                m_ob.deduct_template_type(param, deduction);

                if (deduction.type == template_deduction_type::OTHER && deduction.identified_type)
                {
                    auto full_name = get_full_name(*deduction.identified_type, true);
                    header.raw("{} {}", full_name, param.get_name());
                }
                else
                {
                    header.raw("{} {}", param.type, param.get_name());
                }
            }
            header.raw(">\n");
        }

        header.print_tabs();
        header.raw("class id<{}", get_full_name(m_ob, true));
        if (m_ob.get_is_template() && !m_ob.get_template_params().empty())
        {
            header.raw("<");
            bool first_pass = true;
            for (const auto& param : m_ob.get_template_params())
            {
                if (!first_pass)
                    header.raw(", ");
                first_pass = false;
                header.raw("{}", param.get_name());
            }
            header.raw(">");
        }
        header.raw(">\n");

        header("{{");
        header("public:");
        header("static constexpr uint64_t get(uint64_t rpc_version)");
        header("{{");
        auto val = m_ob.get_value(rpc_attribute_types::use_template_param_in_id_attr);
        for (const auto& version : protocol_versions)
        {
            header("#ifdef {}", version.macro);
            header("if(rpc_version >= {})", version.symbol);
            header("{{");
            header("auto id = {}ull;", fingerprint::generate(m_ob, {}, &header, version.value));
            if (val != "false")
            {
                for (const auto& param : m_ob.get_template_params())
                {
                    template_deduction deduction;
                    m_ob.deduct_template_type(param, deduction);
                    if (deduction.type == template_deduction_type::CLASS || deduction.type == template_deduction_type::TYPENAME)
                    {
                        header("id ^= rpc::id<{}>::get({});", param.get_name(), version.symbol);
                        header("id = (id << 1)|(id >> (sizeof(id) - 1));//rotl");
                    }
                    else if (deduction.identified_type)
                    {
                        if (deduction.identified_type->get_entity_type() == entity_type::ENUM)
                        {
                            header("id ^= static_cast<uint64_t>({});", param.get_name());
                            header("id = (id << 1)|(id >> (sizeof(id) - 1));//rotl");
                            break;
                        }
                        else if (param.get_name() == "size_t")
                        {
                            header("id ^= static_cast<uint64_t>({});", param.get_name());
                            header("id = (id << 1)|(id >> (sizeof(id) - 1));//rotl");
                            break;
                        }
                        else
                        {
                            header("static_assert(!\"not supported\"));//rotl");
                        }
                    }
                    else
                    {
                        header("id ^= static_cast<uint64_t>({});", param.get_name());
                        header("id = (id << 1)|(id >> (sizeof(id) - 1));//rotl");
                    }
                }
            }
            header("return id;");
            header("}}");
            header("#endif");
        }
        header("return 0;");
        header("}}");
        header("}};");
        header("");
    }

    void write_struct(const class_entity& m_ob, writer& header)
    {
        if (m_ob.is_in_import())
            return;

        header("");
        header("/****************************************************************************/");

        std::string base_class_declaration;
        auto bc = m_ob.get_base_classes();
        if (!bc.empty())
        {

            base_class_declaration = " : ";
            int i = 0;
            for (auto base_class : bc)
            {
                if (i)
                    base_class_declaration += ", ";
                base_class_declaration += base_class->get_name();
                i++;
            }
        }
        if (m_ob.get_is_template())
        {
            header.print_tabs();
            header.raw("template<");
            bool first_pass = true;
            for (const auto& param : m_ob.get_template_params())
            {
                if (!first_pass)
                    header.raw(", ");
                first_pass = false;
                header.raw("{} {}", param.type, param.get_name());
                if (!param.default_value.empty())
                    header.raw(" = {}", param.default_value);
            }
            header.raw(">\n");
        }
        header("struct {}{}", m_ob.get_name(), base_class_declaration);
        header("{{");

        for (auto& field : m_ob.get_elements(entity_type::STRUCTURE_MEMBERS))
        {
            if (field->get_entity_type() == entity_type::FUNCTION_VARIABLE)
            {
                auto* function_variable = static_cast<const function_entity*>(field.get());
                header.print_tabs();
                render_function(header, m_ob, *function_variable);
                if (function_variable->get_array_string().size())
                    header.raw("[{}]", function_variable->get_array_string());
                if (!function_variable->get_default_value().empty())
                {
                    header.raw(" = {};\n", function_variable->get_default_value());
                }
                else
                {
                    header.raw("{{}};\n");
                }
            }
            else if (field->get_entity_type() == entity_type::CPPQUOTE)
            {
                if (field->is_in_import())
                    continue;
                auto text = field->get_name();
                header.write_buffer(text);
                continue;
            }
            else if (field->get_entity_type() == entity_type::FUNCTION_PRIVATE)
            {
                header("private:");
            }
            else if (field->get_entity_type() == entity_type::FUNCTION_PUBLIC)
            {
                header("public:");
            }
            else if (field->get_entity_type() == entity_type::CONSTEXPR)
            {
                interface_declaration_generator::write_constexpr(header, *field);
            }
        }

        header("");
        header("// one member-function for save/load");
        header("template<typename Ar>");
        header("void serialize(Ar &ar)");
        header("{{");
        header("std::ignore = ar;");
        bool has_fields = false;
        for (auto& field : m_ob.get_functions())
        {
            auto type = field->get_entity_type();
            if (type != entity_type::CPPQUOTE && type != entity_type::FUNCTION_PUBLIC
                && type != entity_type::FUNCTION_PRIVATE && type != entity_type::CONSTEXPR)
            {
                if (field->get_entity_type() == entity_type::FUNCTION_VARIABLE)
                {
                    auto* function_variable = static_cast<const function_entity*>(field.get());
                    if (function_variable->is_static())
                    {
                        continue;
                    }
                }

                has_fields = true;
                break;
            }
        }
        if (has_fields)
        {
            header("ar & YAS_OBJECT_NVP(\"{}\"", m_ob.get_name());

            for (auto& field : m_ob.get_functions())
            {
                auto type = field->get_entity_type();
                if (type != entity_type::CPPQUOTE && type != entity_type::FUNCTION_PUBLIC
                    && type != entity_type::FUNCTION_PRIVATE && type != entity_type::CONSTEXPR)
                {
                    if (field->get_entity_type() == entity_type::FUNCTION_VARIABLE)
                    {
                        auto* function_variable = static_cast<const function_entity*>(field.get());
                        if (function_variable->is_static())
                        {
                            continue;
                        }
                    }
                    header("  ,(\"{0}\", {0})", field->get_name());
                }
            }
            header(");");
        }

        header("}}");

        header("}};");

        std::stringstream sstr;
        std::string obj_type(m_ob.get_name());
        {
            writer tmpl(sstr, header.get_tab_count());
            if (m_ob.get_is_template())
            {
                tmpl.print_tabs();
                tmpl.raw("template<");
                if (!m_ob.get_template_params().empty())
                {
                    obj_type += "<";
                    bool first_pass = true;
                    for (const auto& param : m_ob.get_template_params())
                    {
                        if (!first_pass)
                        {
                            tmpl.raw(", ");
                            obj_type += ", ";
                        }
                        first_pass = false;
                        tmpl.raw("{} {}", param.type, param.get_name());
                        if (!param.default_value.empty())
                            tmpl.raw(" = {}", param.default_value);
                        obj_type += param.get_name();
                    }
                    obj_type += ">";
                }
                tmpl.raw(">\n");
            }
        }
        header.raw(sstr.str());
        header("inline bool operator != (const {0}& lhs, const {0}& rhs)", obj_type);
        header("{{");
        bool has_params = true;
        {
            bool first_pass = true;
            for (auto& field : m_ob.get_functions())
            {
                auto type = field->get_entity_type();
                if (type != entity_type::CPPQUOTE && type != entity_type::FUNCTION_PUBLIC
                    && type != entity_type::FUNCTION_PRIVATE && type != entity_type::CONSTEXPR)
                {
                    if (field->get_entity_type() == entity_type::FUNCTION_VARIABLE)
                    {
                        auto* function_variable = static_cast<const function_entity*>(field.get());
                        if (function_variable->is_static())
                        {
                            continue;
                        }
                    }
                    first_pass = false;
                }
            }
            has_params = !first_pass;
        }
        if (has_params)
        {
            header.print_tabs();
            header.raw("return ");

            bool first_pass = true;
            for (auto& field : m_ob.get_functions())
            {
                auto type = field->get_entity_type();
                if (type != entity_type::CPPQUOTE && type != entity_type::FUNCTION_PUBLIC
                    && type != entity_type::FUNCTION_PRIVATE && type != entity_type::CONSTEXPR)
                {
                    if (field->get_entity_type() == entity_type::FUNCTION_VARIABLE)
                    {
                        auto* function_variable = static_cast<const function_entity*>(field.get());
                        if (function_variable->is_static())
                        {
                            continue;
                        }
                    }

                    header.raw("\n");
                    header.print_tabs();
                    header.raw("{1}lhs.{0} != rhs.{0}", field->get_name(), first_pass ? "" : "|| ");
                    first_pass = false;
                }
            }
            header.raw(";\n");
        }
        else
        {
            header("std::ignore = lhs;");
            header("std::ignore = rhs;");
            header("return false;");
        }
        header("}}\n");

        header.raw(sstr.str());
        header("inline bool operator == (const {0}& lhs, const {0}& rhs)", obj_type);
        header("{{");
        header("return !(lhs != rhs);");
        header("}}");
    };

    void write_encapsulate_outbound_interfaces(
        const class_entity& obj, writer& header, const std::vector<std::string>& namespaces)
    {
        auto interface_name = std::string(obj.get_entity_type() == entity_type::LIBRARY ? "i_" : "") + obj.get_name();
        std::string ns;

        for (auto& name : namespaces)
        {
            ns += name + "::";
        }

        auto owner = obj.get_owner();
        if (owner && !owner->get_name().empty())
        {
            interface_declaration_generator::build_scoped_name(owner, ns);
        }

        header("template<> CORO_TASK(rpc::interface_descriptor) "
               "rpc::service::proxy_bind_in_param(uint64_t protocol_version, const rpc::shared_ptr<::{}{}>& "
               "iface, rpc::shared_ptr<rpc::object_stub>& stub);",
            ns,
            interface_name);
        header("template<> CORO_TASK(rpc::interface_descriptor) "
               "rpc::service::stub_bind_out_param(uint64_t protocol_version, caller_channel_zone "
               "caller_channel_zone_id, caller_zone caller_zone_id, const rpc::shared_ptr<::{}{}>& "
               "iface);",
            ns,
            interface_name);
    }

    void write_library_proxy_factory(
        writer& proxy, writer& stub, const class_entity& obj, const std::vector<std::string>& namespaces)
    {
        auto interface_name = std::string(obj.get_entity_type() == entity_type::LIBRARY ? "i_" : "") + obj.get_name();
        std::string ns;

        for (auto& name : namespaces)
        {
            ns += name + "::";
        }
        auto owner = obj.get_owner();
        if (owner && !owner->get_name().empty())
        {
            interface_declaration_generator::build_scoped_name(owner, ns);
        }

        proxy("template<> void object_proxy::create_interface_proxy(shared_ptr<::{}{}>& "
              "inface)",
            ns,
            interface_name);
        proxy("{{");
        proxy("inface = ::{1}{0}_proxy::create(shared_from_this());", interface_name, ns);
        proxy("}}");
        proxy("");

        stub("template<> std::function<shared_ptr<i_interface_stub>(const shared_ptr<object_stub>& stub)> "
             "service::create_interface_stub(const shared_ptr<::{}{}>& iface)",
            ns,
            interface_name);
        stub("{{");
        stub("return [&](const shared_ptr<object_stub>& stub) -> "
             "shared_ptr<i_interface_stub>{{");
        stub("return static_pointer_cast<i_interface_stub>(::{}{}_stub::create(iface, stub));", ns, interface_name);
        stub("}};");
        stub("}}");

        stub("template<> CORO_TASK(interface_descriptor) service::proxy_bind_in_param(uint64_t protocol_version, "
             "const "
             "shared_ptr<::{}{}>& iface, shared_ptr<object_stub>& stub)",
            ns,
            interface_name);
        stub("{{");
        stub("if(!iface)");
        stub("{{");
        stub("CO_RETURN {{{{0}},{{0}}}};");
        stub("}}");

        stub("auto factory = create_interface_stub(iface);");
        stub("CO_RETURN CO_AWAIT get_proxy_stub_descriptor(protocol_version, {{0}}, {{0}}, iface.get(), factory, "
             "false, stub);");
        stub("}}");

        stub("template<> CORO_TASK(interface_descriptor) service::stub_bind_out_param(uint64_t protocol_version, "
             "caller_channel_zone caller_channel_zone_id, caller_zone caller_zone_id, const shared_ptr<::{}{}>& "
             "iface)",
            ns,
            interface_name);
        stub("{{");
        stub("if(!iface)");
        stub("{{");
        stub("CO_RETURN {{{{0}},{{0}}}};");
        stub("}}");

        stub("shared_ptr<object_stub> stub;");

        stub("auto factory = create_interface_stub(iface);");
        stub("CO_RETURN CO_AWAIT get_proxy_stub_descriptor(protocol_version, caller_channel_zone_id, "
             "caller_zone_id, "
             "iface.get(), factory, true, stub);");
        stub("}}");
    }

    void write_marshalling_logic(const class_entity& lib, writer& stub)
    {
        {
            for (auto& cls : lib.get_classes())
            {
                if (!cls->get_import_lib().empty())
                    continue;
                if (cls->get_entity_type() == entity_type::INTERFACE)
                    write_stub_cast_factory(*cls, stub);
            }

            for (auto& cls : lib.get_classes())
            {
                if (!cls->get_import_lib().empty())
                    continue;
                if (cls->get_entity_type() == entity_type::LIBRARY)
                    write_stub_cast_factory(*cls, stub);
            }
        }
    }

    // entry point
    void write_namespace_predeclaration(const class_entity& lib, writer& header, writer& proxy, writer& stub)
    {
        for (auto cls : lib.get_classes())
        {
            if (!cls->get_import_lib().empty())
                continue;
            if (cls->get_entity_type() == entity_type::INTERFACE || cls->get_entity_type() == entity_type::LIBRARY)
                write_interface_forward_declaration(*cls, header, proxy, stub);
        }

        for (auto cls : lib.get_classes())
        {
            if (!cls->get_import_lib().empty())
                continue;
            if (cls->get_entity_type() == entity_type::NAMESPACE)
            {
                bool is_inline = cls->has_value("inline");
                if (is_inline)
                {
                    header("inline namespace {}", cls->get_name());
                    proxy("inline namespace {}", cls->get_name());
                    stub("inline namespace {}", cls->get_name());
                }
                else
                {
                    header("namespace {}", cls->get_name());
                    proxy("namespace {}", cls->get_name());
                    stub("namespace {}", cls->get_name());
                }

                header("{{");
                proxy("{{");
                stub("{{");

                write_namespace_predeclaration(*cls, header, proxy, stub);

                header("}}");
                proxy("}}");
                stub("}}");
            }
        }
    }

    // entry point
    void write_namespace(bool from_host,
        const class_entity& lib,
        std::string prefix,
        writer& header,
        writer& proxy,
        writer& stub,
        bool catch_stub_exceptions,
        const std::vector<std::string>& rethrow_exceptions)
    {
        for (auto& elem : lib.get_elements(entity_type::NAMESPACE_MEMBERS))
        {
            if (elem->is_in_import())
                continue;
            else if (elem->get_entity_type() == entity_type::ENUM)
                write_enum_forward_declaration(*elem, header);
            else if (elem->get_entity_type() == entity_type::TYPEDEF)
                write_typedef_forward_declaration(*elem, header);
            else if (elem->get_entity_type() == entity_type::NAMESPACE)
            {
                bool is_inline = elem->has_value("inline");

                if (is_inline)
                {
                    header("inline namespace {}", elem->get_name());
                    proxy("inline namespace {}", elem->get_name());
                    stub("inline namespace {}", elem->get_name());
                }
                else
                {
                    header("namespace {}", elem->get_name());
                    proxy("namespace {}", elem->get_name());
                    stub("namespace {}", elem->get_name());
                }
                header("{{");
                proxy("{{");
                stub("{{");
                auto& ent = static_cast<const class_entity&>(*elem);
                write_namespace(
                    from_host, ent, prefix + elem->get_name() + "::", header, proxy, stub, catch_stub_exceptions, rethrow_exceptions);
                header("}}");
                proxy("}}");
                stub("}}");
            }
            else if (elem->get_entity_type() == entity_type::STRUCT)
            {
                auto& ent = static_cast<const class_entity&>(*elem);
                write_struct(ent, header);
            }

            else if (elem->get_entity_type() == entity_type::INTERFACE || elem->get_entity_type() == entity_type::LIBRARY)
            {
                auto& ent = static_cast<const class_entity&>(*elem);
                interface_declaration_generator::write_interface(ent, header);
                write_interface(from_host, ent, proxy, stub, catch_stub_exceptions, rethrow_exceptions);
            }
            else if (elem->get_entity_type() == entity_type::CONSTEXPR)
            {
                interface_declaration_generator::write_constexpr(header, *elem);
            }
            else if (elem->get_entity_type() == entity_type::CPPQUOTE)
            {
                if (!elem->is_in_import())
                {
                    auto text = elem->get_name();
                    header.write_buffer(text);
                }
            }
        }
        write_marshalling_logic(lib, stub);
    }

    void write_epilog(bool from_host,
        const class_entity& lib,
        writer& header,
        writer& proxy,
        writer& stub,
        const std::vector<std::string>& namespaces)
    {
        for (auto cls : lib.get_classes())
        {
            if (!cls->get_import_lib().empty())
                continue;
            if (cls->get_entity_type() == entity_type::NAMESPACE)
            {

                write_epilog(from_host, *cls, header, proxy, stub, namespaces);
            }
            else if (cls->get_entity_type() == entity_type::STRUCT)
            {
                auto& ent = static_cast<const class_entity&>(*cls);
                write_struct_id(ent, header);
            }
            else
            {
                if (cls->get_entity_type() == entity_type::LIBRARY || cls->get_entity_type() == entity_type::INTERFACE)
                    write_encapsulate_outbound_interfaces(*cls, header, namespaces);

                if (cls->get_entity_type() == entity_type::LIBRARY || cls->get_entity_type() == entity_type::INTERFACE)
                    write_library_proxy_factory(proxy, stub, *cls, namespaces);
            }
        }
    }

    void write_stub_factory_lookup_items(
        const class_entity& lib, std::string prefix, writer& stub, std::set<std::string>& done)
    {
        for (auto cls : lib.get_classes())
        {
            if (!cls->get_import_lib().empty())
                continue;
            if (cls->get_entity_type() == entity_type::NAMESPACE)
            {
                write_stub_factory_lookup_items(*cls, prefix + cls->get_name() + "::", stub, done);
            }
            else
            {
                for (auto& cls : lib.get_classes())
                {
                    if (!cls->get_import_lib().empty())
                        continue;
                    if (cls->get_entity_type() == entity_type::INTERFACE)
                        write_stub_factory(*cls, stub, done);
                }

                for (auto& cls : lib.get_classes())
                {
                    if (!cls->get_import_lib().empty())
                        continue;
                    if (cls->get_entity_type() == entity_type::LIBRARY)
                        write_stub_factory(*cls, stub, done);
                }
            }
        }
    }

    void write_stub_factory_lookup(
        const std::string module_name, const class_entity& lib, std::string prefix, writer& stub_header, writer& stub)
    {
        stub_header("void {}_register_stubs(const rpc::shared_ptr<rpc::service>& srv);", module_name);
        stub("void {}_register_stubs(const rpc::shared_ptr<rpc::service>& srv)", module_name);
        stub("{{");

        std::set<std::string> done;

        write_stub_factory_lookup_items(lib, prefix, stub, done);

        stub("}}");
    }

    // entry point
    void write_files(std::string module_name,
        bool from_host,
        const class_entity& lib,
        std::ostream& hos,
        std::ostream& pos,
        std::ostream& sos,
        std::ostream& shos,
        const std::vector<std::string>& namespaces,
        const std::string& header_filename,
        const std::string& stub_header_filename,
        const std::list<std::string>& imports,
        const std::vector<std::string>& additional_headers,
        bool catch_stub_exceptions,
        const std::vector<std::string>& rethrow_exceptions,
        const std::vector<std::string>& additional_stub_headers,
        bool include_rpc_headers)
    {
        writer header(hos);
        writer proxy(pos);
        writer stub(sos);
        writer stub_header(shos);

        header("#pragma once");
        header("");

        std::for_each(additional_headers.begin(),
            additional_headers.end(),
            [&](const std::string& additional_header) { header("#include <{}>", additional_header); });

        std::for_each(additional_stub_headers.begin(),
            additional_stub_headers.end(),
            [&](const std::string& additional_stub_header) { stub("#include <{}>", additional_stub_header); });

        header("#include <memory>");
        header("#include <vector>");
        header("#include <list>");
        header("#include <map>");
        header("#include <unordered_map>");
        header("#include <set>");
        header("#include <unordered_set>");
        header("#include <string>");
        header("#include <array>");

        if (include_rpc_headers)
        {
            header("#include <rpc/version.h>");
            header("#include <rpc/marshaller.h>");
            header("#include <rpc/serialiser.h>");
            header("#include <rpc/service.h>");
            header("#include <rpc/error_codes.h>");
            header("#include <rpc/types.h>");
            header("#include <rpc/casting_interface.h>");
        }

        for (const auto& import : imports)
        {
            std::filesystem::path p(import);
            auto import_header = p.root_name() / p.parent_path() / p.stem();
            auto path = import_header.string();
            std::replace(path.begin(), path.end(), '\\', '/');
            header("#include \"{}.h\"", path);
        }

        header("");

        proxy("#include <yas/mem_streams.hpp>");
        proxy("#include <yas/binary_iarchive.hpp>");
        proxy("#include <yas/binary_oarchive.hpp>");
        proxy("#include <yas/json_iarchive.hpp>");
        proxy("#include <yas/json_oarchive.hpp>");
        proxy("#include <yas/text_iarchive.hpp>");
        proxy("#include <yas/text_oarchive.hpp>");
        proxy("#include <yas/std_types.hpp>");
        proxy("#include <yas/count_streams.hpp>");
        proxy("#include <rpc/proxy.h>");
        proxy("#include <rpc/stub.h>");
        proxy("#include <rpc/service.h>");
        proxy("#include <rpc/logger.h>");  // For RPC_ERROR in error handling
        proxy("#include \"{}\"", header_filename);

        proxy("");

        stub_header("#pragma once");
        stub_header("#include <rpc/service.h>");
        stub_header("");

        stub("#include <yas/mem_streams.hpp>");
        stub("#include <yas/binary_iarchive.hpp>");
        stub("#include <yas/binary_oarchive.hpp>");
        stub("#include <yas/count_streams.hpp>");
        stub("#include <yas/std_types.hpp>");
        stub("#include <rpc/stub.h>");
        stub("#include <rpc/proxy.h>");
        stub("#include \"{}\"", header_filename);
        // stub("#include \"{}\"", yas_header_filename);
        stub("#include \"{}\"", stub_header_filename);
        stub("");

        std::string prefix;
        for (auto& ns : namespaces)
        {
            header("namespace {}", ns);
            header("{{");
            proxy("namespace {}", ns);
            proxy("{{");
            stub("namespace {}", ns);
            stub("{{");
            stub_header("namespace {}", ns);
            stub_header("{{");

            prefix += ns + "::";
        }

        write_namespace_predeclaration(lib, header, proxy, stub);

        write_namespace(from_host, lib, prefix, header, proxy, stub, catch_stub_exceptions, rethrow_exceptions);

        for (auto& ns : namespaces)
        {
            (void)ns;
            header("}}");
            proxy("}}");
            stub("}}");
            stub_header("}}");
        }

        header("");
        header("/****************************************************************************/");
        header("namespace rpc");
        header("{{");
        stub("namespace rpc");
        stub("{{");
        proxy("namespace rpc");
        proxy("{{");
        write_epilog(from_host, lib, header, proxy, stub, namespaces);
        header("}}");
        proxy("}}");
        stub("}}");

        write_stub_factory_lookup(module_name, lib, prefix, stub_header, stub);
    }
}
