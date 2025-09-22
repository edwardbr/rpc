/*
 *   Copyright (c) 2025 Edward Boggis-Rolfe
 *   All rights reserved.
 */

// Standard C++ headers
#include <algorithm>
#include <filesystem>
#include <sstream>
#include <tuple>
#include <type_traits>
#include <unordered_set>

// Other headers
extern "C"
{
#include "sha3.h"
}
#include "attributes.h"
#include "coreclasses.h"
#include "cpp_parser.h"
#include "fingerprint_generator.h"
#include "helpers.h"
#include "interface_declaration_generator.h"
#include "type_utils.h"
#include "writer.h"
#include "yas_generator.h"

namespace yas_generator
{
    enum print_type
    {
        PROXY_PARAM_IN,
        PROXY_MARSHALL_IN,
        PROXY_PARAM_OUT,
        PROXY_MARSHALL_OUT,

        STUB_PARAM_IN,
        STUB_MARSHALL_IN,
        STUB_PARAM_OUT,
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
            uint64_t& count) override
        {
            auto opt = static_cast<print_type>(option);
            std::ignore = from_host;
            std::ignore = lib;
            std::ignore = is_in;
            std::ignore = is_out;
            std::ignore = is_const;
            std::ignore = count;

            switch (opt)
            {
            case PROXY_PARAM_IN:
                return fmt::format("const {}& {}", type_name, name);
            case STUB_PARAM_IN:
                return fmt::format("{}& {}", type_name, name);
            case STUB_PARAM_OUT:
                return fmt::format("const {}& {}", type_name, name);
            case PROXY_PARAM_OUT:
                return fmt::format("{}& {}", type_name, name);
            case PROXY_MARSHALL_IN:
            case PROXY_MARSHALL_OUT:
            case STUB_MARSHALL_IN:
            case STUB_MARSHALL_OUT:
                return fmt::format("  ,(\"{0}\", {0})", name);
            default:
                return "";
            }
        }

        std::string render_reference(int option,
            bool from_host,
            const class_entity& lib,
            const std::string& name,
            bool is_in,
            bool is_out,
            bool is_const,
            const std::string& type_name,
            uint64_t& count) override
        {
            auto opt = static_cast<print_type>(option);
            std::ignore = from_host;
            std::ignore = lib;
            std::ignore = is_in;
            std::ignore = is_const;
            std::ignore = count;

            if (is_out)
            {
                throw std::runtime_error("REFERENCE does not support out vals");
            }

            switch (opt)
            {
            case PROXY_PARAM_IN:
                return fmt::format("const {}& {}", type_name, name);
            case STUB_PARAM_IN:
                return fmt::format("{}& {}", type_name, name);
            case STUB_PARAM_OUT:
                return fmt::format("const {}& {}", type_name, name);
            case PROXY_PARAM_OUT:
                return fmt::format("{}& {}", type_name, name);
            case PROXY_MARSHALL_IN:
            case PROXY_MARSHALL_OUT:
            case STUB_MARSHALL_IN:
            case STUB_MARSHALL_OUT:
                return fmt::format("  ,(\"{0}\", {0})", name);
            default:
                return "";
            }
        }

        std::string render_move(int option,
            bool from_host,
            const class_entity& lib,
            const std::string& name,
            bool is_in,
            bool is_out,
            bool is_const,
            const std::string& type_name,
            uint64_t& count) override
        {
            auto opt = static_cast<print_type>(option);
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

            switch (opt)
            {
            case PROXY_PARAM_IN:
                return fmt::format("{}&& {}", type_name, name);
            case STUB_PARAM_IN:
                return fmt::format("{}& {}", type_name, name);
            case PROXY_MARSHALL_IN:
            case PROXY_MARSHALL_OUT:
            case STUB_MARSHALL_IN:
            case STUB_MARSHALL_OUT:
                return fmt::format("  ,(\"{0}\", {0})", name);
            default:
                return "";
            }
        }

        std::string render_pointer(int option,
            bool from_host,
            const class_entity& lib,
            const std::string& name,
            bool is_in,
            bool is_out,
            bool is_const,
            const std::string& type_name,
            uint64_t& count) override
        {
            auto opt = static_cast<print_type>(option);
            std::ignore = from_host;
            std::ignore = lib;
            std::ignore = is_in;
            std::ignore = is_const;
            std::ignore = type_name;
            std::ignore = count;

            if (is_out)
            {
                throw std::runtime_error("POINTER does not support out vals");
            }

            switch (opt)
            {
            case PROXY_PARAM_IN:
                return fmt::format("uint64_t {}", name);
            case STUB_PARAM_IN:
                return fmt::format("uint64_t {}", name);
            case PROXY_MARSHALL_IN:
            case PROXY_MARSHALL_OUT:
            case STUB_MARSHALL_IN:
            case STUB_MARSHALL_OUT:
                return fmt::format("  ,(\"{0}\", {0})", name);
            default:
                return "";
            }
        }

        std::string render_pointer_reference(int option,
            bool from_host,
            const class_entity& lib,
            const std::string& name,
            bool is_in,
            bool is_out,
            bool is_const,
            const std::string& type_name,
            uint64_t& count) override
        {
            auto opt = static_cast<print_type>(option);
            std::ignore = from_host;
            std::ignore = lib;
            std::ignore = is_in;
            std::ignore = type_name;
            std::ignore = count;

            if (is_const && is_out)
            {
                throw std::runtime_error("POINTER_REFERENCE does not support const out vals");
            }
            switch (opt)
            {
            case PROXY_PARAM_IN:
                return fmt::format("uint64_t {}", name);
            case STUB_PARAM_IN:
                return fmt::format("uint64_t& {}", name);
            case STUB_PARAM_OUT:
                return fmt::format("uint64_t {}", name);
            case PROXY_PARAM_OUT:
                return fmt::format("uint64_t& {}", name);
            case PROXY_MARSHALL_IN:
            case PROXY_MARSHALL_OUT:
            case STUB_MARSHALL_IN:
            case STUB_MARSHALL_OUT:
                return fmt::format("  ,(\"{0}\", {0})", name);
            default:
                return "";
            }
        }

        std::string render_pointer_pointer(int option,
            bool from_host,
            const class_entity& lib,
            const std::string& name,
            bool is_in,
            bool is_out,
            bool is_const,
            const std::string& type_name,
            uint64_t& count) override
        {
            auto opt = static_cast<print_type>(option);
            std::ignore = from_host;
            std::ignore = lib;
            std::ignore = is_in;
            std::ignore = is_out;
            std::ignore = is_const;
            std::ignore = type_name;
            std::ignore = count;

            switch (opt)
            {
            case PROXY_PARAM_IN:
                return fmt::format("uint64_t {}", name);
            case STUB_PARAM_IN:
                return fmt::format("uint64_t& {}", name);
            case STUB_PARAM_OUT:
                return fmt::format("uint64_t {}", name);
            case PROXY_PARAM_OUT:
                return fmt::format("uint64_t& {}", name);
            case PROXY_MARSHALL_IN:
            case PROXY_MARSHALL_OUT:
            case STUB_MARSHALL_IN:
            case STUB_MARSHALL_OUT:
                return fmt::format("  ,(\"{0}\", {0})", name);
            default:
                return "";
            }
        }

        std::string render_interface(int option,
            bool from_host,
            const class_entity& lib,
            const std::string& name,
            bool is_in,
            bool is_out,
            bool is_const,
            const std::string& type_name,
            uint64_t& count) override
        {
            auto opt = static_cast<print_type>(option);
            std::ignore = from_host;
            std::ignore = lib;
            std::ignore = is_in;
            std::ignore = is_const;
            std::ignore = type_name;
            std::ignore = count;

            if (is_out)
            {
                throw std::runtime_error("INTERFACE does not support out vals");
            }

            switch (opt)
            {
            case PROXY_PARAM_IN:
                return fmt::format("const rpc::interface_descriptor& {}", name);
            case STUB_PARAM_IN:
                return fmt::format("rpc::interface_descriptor& {}", name);
            case PROXY_PARAM_OUT:
                return fmt::format("const rpc::interface_descriptor& {}", name);
            case STUB_PARAM_OUT:
                return fmt::format("rpc::interface_descriptor& {}", name);
            case PROXY_MARSHALL_IN:
            case PROXY_MARSHALL_OUT:
            case STUB_MARSHALL_IN:
            case STUB_MARSHALL_OUT:
                return fmt::format("  ,(\"{0}\", {0})", name);
            default:
                return "";
            }
        }

        std::string render_interface_reference(int option,
            bool from_host,
            const class_entity& lib,
            const std::string& name,
            bool is_in,
            bool is_out,
            bool is_const,
            const std::string& type_name,
            uint64_t& count) override
        {
            auto opt = static_cast<print_type>(option);
            std::ignore = from_host;
            std::ignore = lib;
            std::ignore = is_in;
            std::ignore = is_out;
            std::ignore = is_const;
            std::ignore = type_name;
            std::ignore = count;

            switch (opt)
            {
            case PROXY_PARAM_IN:
                return fmt::format("const rpc::interface_descriptor& {}", name);
            case STUB_PARAM_IN:
                return fmt::format("rpc::interface_descriptor& {}", name);
            case PROXY_PARAM_OUT:
                return fmt::format("const rpc::interface_descriptor& {}", name);
            case STUB_PARAM_OUT:
                return fmt::format("rpc::interface_descriptor& {}", name);
            case PROXY_MARSHALL_IN:
            case PROXY_MARSHALL_OUT:
            case STUB_MARSHALL_IN:
            case STUB_MARSHALL_OUT:
                return fmt::format("  ,(\"{0}\", {0})", name);
            default:
                return "";
            }
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

    void build_fully_scoped_name(const class_entity* entity, std::string& name)
    {
        auto* owner = entity->get_owner();
        if (owner && !owner->get_name().empty())
        {
            build_fully_scoped_name(owner, name);
        }
        name += "::" + entity->get_name();
    }

    std::string deduct_parameter_type_name(const class_entity& m_ob, const std::string& type_name)
    {
        auto ret = type_name;
        std::string reference_modifiers;
        std::string template_modifier;
        rpc_generator::strip_reference_modifiers(ret, reference_modifiers);

        auto template_start = ret.find('<');
        if (template_start != std::string::npos)
        {
            int template_count = 1;
            auto pData = ret.data() + template_start + 1;
            while (*pData != 0 && template_count > 0)
            {
                if (*pData == '<')
                    template_count++;
                else if (*pData == '>')
                    template_count--;
                pData++;
            }
            template_modifier = ret.substr(template_start, pData - (ret.data() + template_start));
            ret = ret.substr(0, template_start);
        }

        std::shared_ptr<class_entity> param_type;
        m_ob.find_class(ret, param_type);

        if (param_type.get())
        {
            ret.clear();
            build_fully_scoped_name(param_type.get(), ret);
        }
        return ret + template_modifier + reference_modifiers;
    }

    void write_proxy_send_method(bool from_host,
        const class_entity& m_ob,
        writer& proxy,
        const std::string& interface_name,
        const std::shared_ptr<function_entity>& function,
        int& function_count)
    {
        bool has_inparams = false;
        proxy("template<>");
        proxy("{}",
            interface_declaration_generator::write_proxy_send_declaration(m_ob,
                interface_name + "::proxy_serialiser<rpc::serialiser::yas, rpc::encoding>::",
                function,
                has_inparams,
                ", rpc::encoding __rpc_enc",
                false));
        proxy("{{");

        if (has_inparams)
        {
            proxy("auto __yas_mapping = YAS_OBJECT_NVP(");
            proxy("  \"in\"");

            uint64_t count = 1;
            for (auto& parameter : function->get_parameters())
            {
                std::string output;
                {
                    if (!do_in_param(
                            PROXY_MARSHALL_IN, from_host, m_ob, parameter.get_name(), parameter.get_type(), parameter, count, output))
                        continue;

                    proxy(output);
                }
                count++;
            }

            proxy("  );");

            proxy("__buffer.clear(); // this does not change the capacity of the vector so this is a low cost "
                  "reset to the buffer");
            proxy("switch(__rpc_enc)");
            proxy("{{");
            proxy("case rpc::encoding::yas_compressed_binary:");
            proxy("::yas::save<::yas::mem|::yas::binary|::yas::compacted|::yas::no_header>(::yas::vector_"
                  "ostream(__buffer), "
                  "__yas_mapping);");
            proxy("break;");
            // proxy("case rpc::encoding::yas_text:");
            // proxy("::yas::save<::yas::mem|::yas::text|::yas::no_header>(::yas::vector_ostream(__buffer), "
            //       "__yas_mapping);");
            // proxy("break;");
            proxy("case rpc::encoding::yas_json:");
            proxy("::yas::save<::yas::mem|::yas::json|::yas::no_header>(::yas::vector_ostream(__buffer), "
                  "__yas_mapping);");
            proxy("break;");
            proxy("case rpc::encoding::enc_default:");
            proxy("case rpc::encoding::yas_binary:");
            proxy("::yas::save<::yas::mem|::yas::binary|::yas::no_header>(::yas::vector_ostream(__buffer), "
                  "__yas_mapping);");
            proxy("break;");
            proxy("default:");
            proxy("return rpc::error::PROXY_DESERIALISATION_ERROR();");
            proxy("break;");
            proxy("}}");
        }
        else
        {
            proxy("if(__rpc_enc == rpc::encoding::yas_json)");
            proxy("  __buffer = {{'{{','}}'}};");
        }
        proxy("return rpc::error::OK();");
        proxy("}}");
        proxy("");

        function_count++;
    }

    void write_proxy_receive_method(bool from_host,
        const class_entity& m_ob,
        writer& proxy,
        const std::string& interface_name,
        const std::shared_ptr<function_entity>& function,
        int& function_count)
    {
        bool has_inparams = false;
        proxy("template<>");
        proxy("{}",
            interface_declaration_generator::write_proxy_receive_declaration(m_ob,
                interface_name + "::proxy_deserialiser<rpc::serialiser::yas, rpc::encoding>::",
                function,
                has_inparams,
                ", rpc::encoding __rpc_enc",
                false));
        proxy("{{");

        if (has_inparams)
        {
            proxy("// no hope of reading anything from an empty buffer");
            proxy("if (__rpc_buf_size == 0)");
            proxy("{{");
            proxy("    RPC_ERROR(\"Proxy deserialisation error - empty buffer\");");
            proxy("    return rpc::error::PROXY_DESERIALISATION_ERROR();");
            proxy("}}");
            proxy("try");
            proxy("{{");
            proxy("auto __yas_mapping = YAS_OBJECT_NVP(");
            proxy("  \"out\"");

            uint64_t count = 1;
            for (auto& parameter : function->get_parameters())
            {
                count++;
                std::string output;
                if (!do_out_param(
                        PROXY_MARSHALL_OUT, from_host, m_ob, parameter.get_name(), parameter.get_type(), parameter, count, output))
                    continue;
                proxy(output);
            }
            proxy("  );");
            proxy("switch(__rpc_enc)");
            proxy("{{");
            proxy("case rpc::encoding::yas_compressed_binary:");
            proxy("::yas::load<::yas::mem|::yas::binary|::yas::compacted|::yas::no_header>(::yas::intrusive_"
                  "buffer(__rpc_buf,__rpc_buf_size), "
                  "__yas_mapping);");
            proxy("break;");
            // proxy("case rpc::encoding::yas_text:");
            // proxy("::yas::load<::yas::mem|::yas::text|::yas::no_header>(::yas::intrusive_buffer(__rpc_buf,__"
            //       "rpc_buf_size), __yas_mapping);");
            // proxy("break;");
            proxy("case rpc::encoding::yas_json:");
            proxy("::yas::load<::yas::mem|::yas::json|::yas::no_header>(::yas::intrusive_buffer(__rpc_buf,__"
                  "rpc_buf_size), __yas_mapping);");
            proxy("break;");
            proxy("case rpc::encoding::enc_default:");
            proxy("case rpc::encoding::yas_binary:");
            proxy("::yas::load<::yas::mem|::yas::binary|::yas::no_header>(::yas::intrusive_buffer(__rpc_buf,__"
                  "rpc_buf_size), __yas_mapping);");
            proxy("break;");
            proxy("default:");
            proxy("RPC_ERROR(\"Proxy deserialisation error - unknown encoding\");");
            proxy("return rpc::error::PROXY_DESERIALISATION_ERROR();");
            proxy("}}");
            proxy("}}");
            proxy("#ifdef USE_RPC_LOGGING");
            proxy("catch(std::exception& ex)");
            proxy("{{");
            proxy("RPC_ERROR(\"A proxy deserialisation error has occurred in an {} implementation in function {} "
                  "{{}}\", ex.what());",
                interface_name,
                function->get_name());
            proxy("return rpc::error::PROXY_DESERIALISATION_ERROR();");
            proxy("}}");
            proxy("#endif");
            proxy("catch(...)");
            proxy("{{");
            proxy("RPC_ERROR(\"Exception has occurred in an {} implementation in function {}\");",
                interface_name,
                function->get_name());
            proxy("return rpc::error::PROXY_DESERIALISATION_ERROR();");
            proxy("}}");
        }
        proxy("return rpc::error::OK();");
        proxy("}}");
        proxy("");

        function_count++;
    }

    void write_stub_receive_method(bool from_host,
        const class_entity& m_ob,
        writer& stub,
        const std::string& interface_name,
        const std::shared_ptr<function_entity>& function,
        int& function_count)
    {
        bool has_outparams = false;
        stub("template<>");
        stub("{}",
            interface_declaration_generator::write_stub_receive_declaration(m_ob,
                interface_name + "::stub_deserialiser<rpc::serialiser::yas, rpc::encoding>::",
                function,
                has_outparams,
                ", rpc::encoding __rpc_enc",
                false));
        stub("{{");

        if (has_outparams)
        {
            stub("// no hope of reading anything from an empty buffer");
            stub("if (__rpc_buf_size == 0)");
            stub("    return rpc::error::STUB_DESERIALISATION_ERROR();");
            stub("try");
            stub("{{");
            stub("auto __yas_mapping = YAS_OBJECT_NVP(");
            stub("  \"out\"");

            uint64_t count = 1;
            for (auto& parameter : function->get_parameters())
            {
                count++;
                std::string output;
                if (!do_in_param(
                        STUB_MARSHALL_IN, from_host, m_ob, parameter.get_name(), parameter.get_type(), parameter, count, output))
                    continue;
                stub(output);
            }
            stub("  );");

            stub("switch(__rpc_enc)");
            stub("{{");
            stub("case rpc::encoding::yas_compressed_binary:");
            stub("::yas::load<::yas::mem|::yas::binary|::yas::compacted|::yas::no_header>(::yas::intrusive_"
                 "buffer(__rpc_buf,__rpc_buf_size), "
                 "__yas_mapping);");
            stub("break;");
            // stub("case rpc::encoding::yas_text:");
            // stub("::yas::load<::yas::mem|::yas::text|::yas::no_header>(::yas::intrusive_buffer(__rpc_buf,__"
            //       "rpc_buf_size), __yas_mapping);");
            // stub("break;");
            stub("case rpc::encoding::yas_json:");
            stub("::yas::load<::yas::mem|::yas::json|::yas::no_header>(::yas::intrusive_buffer(__rpc_buf,__"
                 "rpc_buf_size), __yas_mapping);");
            stub("break;");
            stub("case rpc::encoding::enc_default:");
            stub("case rpc::encoding::yas_binary:");
            stub("::yas::load<::yas::mem|::yas::binary|::yas::no_header>(::yas::intrusive_buffer(__rpc_buf,__"
                 "rpc_buf_size), __yas_mapping);");
            stub("break;");
            stub("default:");
            stub("return rpc::error::STUB_DESERIALISATION_ERROR();");
            stub("}}");
            stub("}}");
            stub("#ifdef USE_RPC_LOGGING");
            stub("catch(std::exception& ex)");
            stub("{{");
            stub("RPC_ERROR(\"A stub deserialisation error has occurred in an {} implementation in function {} {{}}\", "
                 "ex.what());",
                interface_name,
                function->get_name());
            stub("return rpc::error::STUB_DESERIALISATION_ERROR();");
            stub("}}");
            stub("#endif");
            stub("catch(...)");
            stub("{{");
            stub("RPC_ERROR(\"Exception has occurred in an {} implementation in function {}\");",
                interface_name,
                function->get_name());
            stub("return rpc::error::STUB_DESERIALISATION_ERROR();");
            stub("}}");
        }
        stub("return rpc::error::OK();");
        stub("}}");
        stub("");

        function_count++;
    }

    void write_stub_reply_method(bool from_host,
        const class_entity& m_ob,
        writer& stub,
        const std::string& interface_name,
        const std::shared_ptr<function_entity>& function,
        int& function_count)
    {
        bool has_outparams = false;
        stub("template<>");
        stub("{}",
            interface_declaration_generator::write_stub_reply_declaration(m_ob,
                interface_name + "::stub_serialiser<rpc::serialiser::yas, rpc::encoding>::",
                function,
                has_outparams,
                ", rpc::encoding __rpc_enc",
                false));
        stub("{{");

        if (has_outparams)
        {
            stub("auto __yas_mapping = YAS_OBJECT_NVP(");
            stub("  \"out\"");

            uint64_t count = 1;
            for (auto& parameter : function->get_parameters())
            {
                std::string output;
                {
                    if (!do_out_param(
                            STUB_MARSHALL_OUT, from_host, m_ob, parameter.get_name(), parameter.get_type(), parameter, count, output))
                        continue;

                    stub(output);
                }
                count++;
            }

            stub("  );");

            stub("__buffer.clear(); // this does not change the capacity of the vector so this is a low cost reset "
                 "to the buffer");
            stub("switch(__rpc_enc)");
            stub("{{");
            stub("case rpc::encoding::yas_compressed_binary:");
            stub("::yas::save<::yas::mem|::yas::binary|::yas::compacted|::yas::no_header>(::yas::vector_"
                 "ostream(__buffer), "
                 "__yas_mapping);");
            stub("break;");
            // stub("case rpc::encoding::yas_text:");
            // stub("::yas::save<::yas::mem|::yas::text|::yas::no_header>(::yas::vector_ostream(__buffer), "
            //       "__yas_mapping);");
            // stub("break;");
            stub("case rpc::encoding::yas_json:");
            stub("::yas::save<::yas::mem|::yas::json|::yas::no_header>(::yas::vector_ostream(__buffer), "
                 "__yas_mapping);");
            stub("break;");
            stub("case rpc::encoding::enc_default:");
            stub("case rpc::encoding::yas_binary:");
            stub("::yas::save<::yas::mem|::yas::binary|::yas::no_header>(::yas::vector_ostream(__buffer), "
                 "__yas_mapping);");
            stub("break;");
            stub("}}");
        }
        else
        {
            stub("if(__rpc_enc == rpc::encoding::yas_json)");
            stub("  __buffer = {{'{{','}}'}};");
        }
        stub("return rpc::error::OK();");
        stub("}}");
        stub("");

        function_count++;
    }

    void write_interface(bool from_host, const class_entity& m_ob, writer& proxy)
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

        {
            bool has_methods = false;
            for (auto& function : m_ob.get_functions())
            {
                if (function->get_entity_type() != entity_type::FUNCTION_METHOD)
                    continue;
                has_methods = true;
            }

            if (has_methods)
            {
                int function_count = 0;
                std::unordered_set<std::string> unique_signatures;
                for (auto& function : m_ob.get_functions())
                {
                    if (function->get_entity_type() == entity_type::FUNCTION_METHOD)
                    {
                        ++function_count;
                        bool has_params = false;
                        if (unique_signatures
                                .emplace(interface_declaration_generator::write_proxy_send_declaration(
                                    m_ob, "", function, has_params, ", rpc::encoding __rpc_enc", false))
                                .second)
                        {
                            write_proxy_send_method(from_host, m_ob, proxy, interface_name, function, function_count);
                        }
                    }
                }
            }
        }

        {
            bool has_methods = false;
            for (auto& function : m_ob.get_functions())
            {
                if (function->get_entity_type() != entity_type::FUNCTION_METHOD)
                    continue;
                has_methods = true;
            }

            if (has_methods)
            {
                int function_count = 0;
                std::unordered_set<std::string> unique_signatures;
                for (auto& function : m_ob.get_functions())
                {
                    if (function->get_entity_type() == entity_type::FUNCTION_METHOD)
                    {
                        ++function_count;
                        bool has_params = false;
                        if (unique_signatures
                                .emplace(interface_declaration_generator::write_proxy_receive_declaration(
                                    m_ob, "", function, has_params, ", rpc::encoding __rpc_enc", false))
                                .second)
                        {
                            write_proxy_receive_method(from_host, m_ob, proxy, interface_name, function, function_count);
                        }
                    }
                }
            }
        }

        {
            bool has_methods = false;
            for (auto& function : m_ob.get_functions())
            {
                if (function->get_entity_type() != entity_type::FUNCTION_METHOD)
                    continue;
                has_methods = true;
            }

            if (has_methods)
            {
                int function_count = 0;
                std::unordered_set<std::string> unique_signatures;
                for (auto& function : m_ob.get_functions())
                {
                    if (function->get_entity_type() == entity_type::FUNCTION_METHOD)
                    {
                        ++function_count;
                        bool has_params = false;
                        if (unique_signatures
                                .emplace(interface_declaration_generator::write_stub_receive_declaration(
                                    m_ob, "", function, has_params, ", rpc::encoding __rpc_enc", false))
                                .second)
                        {
                            write_stub_receive_method(from_host, m_ob, proxy, interface_name, function, function_count);
                        }
                    }
                }
            }
        }

        {
            bool has_methods = false;
            for (auto& function : m_ob.get_functions())
            {
                if (function->get_entity_type() != entity_type::FUNCTION_METHOD)
                    continue;
                has_methods = true;
            }

            if (has_methods)
            {
                int function_count = 0;
                std::unordered_set<std::string> unique_signatures;
                for (auto& function : m_ob.get_functions())
                {
                    if (function->get_entity_type() == entity_type::FUNCTION_METHOD)
                    {
                        ++function_count;
                        bool has_params = false;
                        if (unique_signatures
                                .emplace(interface_declaration_generator::write_stub_reply_declaration(
                                    m_ob, "", function, has_params, ", rpc::encoding __rpc_enc", false))
                                .second)
                        {
                            write_stub_reply_method(from_host, m_ob, proxy, interface_name, function, function_count);
                        }
                    }
                }
            }
        }
    };

    // entry point
    void write_namespace(bool from_host,
        const class_entity& lib,
        std::string prefix,
        writer& proxy,
        bool catch_stub_exceptions,
        const std::vector<std::string>& rethrow_exceptions)
    {
        for (auto& elem : lib.get_elements(entity_type::NAMESPACE_MEMBERS))
        {
            if (elem->is_in_import())
                continue;
            else if (elem->get_entity_type() == entity_type::NAMESPACE)
            {
                bool is_inline = elem->has_value("inline");

                if (is_inline)
                {
                    proxy("inline namespace {}", elem->get_name());
                }
                else
                {
                    proxy("namespace {}", elem->get_name());
                }
                proxy("{{");
                auto& ent = static_cast<const class_entity&>(*elem);
                write_namespace(
                    from_host, ent, prefix + elem->get_name() + "::", proxy, catch_stub_exceptions, rethrow_exceptions);
                proxy("}}");
            }
            else if (elem->get_entity_type() == entity_type::INTERFACE || elem->get_entity_type() == entity_type::LIBRARY)
            {
                auto& ent = static_cast<const class_entity&>(*elem);
                write_interface(from_host, ent, proxy);
            }
        }
    }

    // entry point
    void write_files(bool from_host,
        const class_entity& lib,
        std::ostream& header_stream,
        const std::vector<std::string>& namespaces,
        const std::string& header_filename,
        bool catch_stub_exceptions,
        const std::vector<std::string>& rethrow_exceptions,
        const std::vector<std::string>& additional_stub_headers)
    {
        std::stringstream tmpstr;
        std::ostream& t = tmpstr;
        writer tmp(t);
        writer header(header_stream);

        std::for_each(additional_stub_headers.begin(),
            additional_stub_headers.end(),
            [&](const std::string& additional_stub_header) { header("#include <{}>", additional_stub_header); });

        header("#include <yas/mem_streams.hpp>");
        header("#include <yas/binary_iarchive.hpp>");
        header("#include <yas/binary_oarchive.hpp>");
        header("#include <yas/serialize.hpp>");
        header("#include <yas/std_types.hpp>");
        header("#include <rpc/rpc.h>");
        header("#include \"{}\"", header_filename);
        header("");

        std::string prefix;
        for (auto& ns : namespaces)
        {
            header("namespace {}", ns);
            header("{{");

            prefix += ns + "::";
        }

        write_namespace(from_host, lib, prefix, header, catch_stub_exceptions, rethrow_exceptions);

        for (auto& ns : namespaces)
        {
            (void)ns;
            header("}}");
        }
    }
}
