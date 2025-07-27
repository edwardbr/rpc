#include <sstream>
#include <unordered_set>

#include "interface_declaration_generator.h"
#include "fingerprint_generator.h"
#include "helpers.h"
#include "type_utils.h"

#include "attributes.h"
#include "rpc_attributes.h"

namespace interface_declaration_generator
{
    enum print_type
    {
        PROXY_PARAM_IN,
        PROXY_PARAM_OUT,

        STUB_PARAM_IN,
        STUB_PARAM_OUT,

        SEND_PARAM_IN
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
            std::ignore = from_host;
            std::ignore = lib;
            std::ignore = is_in;
            std::ignore = is_out;
            std::ignore = is_const;
            std::ignore = count;

            print_type pt = static_cast<print_type>(option);
            switch (pt)
            {
            case PROXY_PARAM_IN:
                return fmt::format("const {}& {}", type_name, name);
            case STUB_PARAM_IN:
                return fmt::format("{}& {}", type_name, name);
            case STUB_PARAM_OUT:
                return fmt::format("const {}& {}", type_name, name);
            case PROXY_PARAM_OUT:
                return fmt::format("{}& {}", type_name, name);
            case SEND_PARAM_IN:
                return fmt::format("{0}, ", name);
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
            std::ignore = from_host;
            std::ignore = lib;
            std::ignore = is_in;
            std::ignore = is_out;
            std::ignore = is_const;
            std::ignore = count;

            if (is_out)
            {
                throw std::runtime_error("REFERENCE does not support out vals");
            }

            print_type pt = static_cast<print_type>(option);
            switch (pt)
            {
            case PROXY_PARAM_IN:
                return fmt::format("const {}& {}", type_name, name);
            case STUB_PARAM_IN:
                return fmt::format("{}& {}", type_name, name);
            case STUB_PARAM_OUT:
                return fmt::format("const {}& {}", type_name, name);
            case PROXY_PARAM_OUT:
                return fmt::format("{}& {}", type_name, name);
            case SEND_PARAM_IN:
                return fmt::format("{0}, ", name);
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
            case PROXY_PARAM_IN:
                return fmt::format("{}&& {}", type_name, name);
            case STUB_PARAM_IN:
                return fmt::format("{}& {}", type_name, name);
            case SEND_PARAM_IN:
                return fmt::format("std::move({0}), ", name);
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
            std::ignore = from_host;
            std::ignore = lib;
            std::ignore = is_in;
            std::ignore = is_out;
            std::ignore = is_const;
            std::ignore = type_name;
            std::ignore = count;

            if (is_out)
            {
                throw std::runtime_error("POINTER does not support out vals");
            }

            print_type pt = static_cast<print_type>(option);
            switch (pt)
            {
            case PROXY_PARAM_IN:
                return fmt::format("uint64_t {}", name);
            case STUB_PARAM_IN:
                return fmt::format("uint64_t& {}", name);
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
            std::ignore = from_host;
            std::ignore = lib;
            std::ignore = is_in;
            std::ignore = type_name;
            std::ignore = count;

            if (is_const && is_out)
            {
                throw std::runtime_error("POINTER_REFERENCE does not support const out vals");
            }
            print_type pt = static_cast<print_type>(option);
            switch (pt)
            {
            case PROXY_PARAM_IN:
                return fmt::format("uint64_t& {}", name);
            case STUB_PARAM_IN:
                return fmt::format("uint64_t& {}", name);
            case STUB_PARAM_OUT:
                return fmt::format("uint64_t {}", name);
            case PROXY_PARAM_OUT:
                return fmt::format("uint64_t& {}", name);
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
            std::ignore = from_host;
            std::ignore = lib;
            std::ignore = is_in;
            std::ignore = is_out;
            std::ignore = is_const;
            std::ignore = count;

            print_type pt = static_cast<print_type>(option);
            switch (pt)
            {
            case PROXY_PARAM_IN:
                return fmt::format("{}** {}", type_name, name);
            case STUB_PARAM_IN:
                return fmt::format("{}** {}", type_name, name);
            case STUB_PARAM_OUT:
                return fmt::format("uint64_t {}", name);
            case PROXY_PARAM_OUT:
                return fmt::format("uint64_t& {}", name);
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

            print_type pt = static_cast<print_type>(option);
            switch (pt)
            {
            case PROXY_PARAM_IN:
                return fmt::format("const rpc::interface_descriptor& {}", name);
            case STUB_PARAM_IN:
                return fmt::format("rpc::interface_descriptor& {}", name);
            case PROXY_PARAM_OUT:
                return fmt::format("rpc::interface_descriptor& {}", name);
            case STUB_PARAM_OUT:
                return fmt::format("rpc::interface_descriptor& {}", name);
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
            std::ignore = from_host;
            std::ignore = lib;
            std::ignore = is_in;
            std::ignore = is_out;
            std::ignore = is_const;
            std::ignore = type_name;
            std::ignore = count;

            print_type pt = static_cast<print_type>(option);
            switch (pt)
            {
            case PROXY_PARAM_IN:
                return fmt::format("rpc::interface_descriptor& {}", name);
            case STUB_PARAM_IN:
                return fmt::format("rpc::interface_descriptor& {}", name);
            case PROXY_PARAM_OUT:
                return fmt::format("rpc::interface_descriptor& {}", name);
            case STUB_PARAM_OUT:
                return fmt::format("rpc::interface_descriptor& {}", name);
            default:
                return "";
            }
        }
    };

    bool do_in_param(print_type option,
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
            r, static_cast<int>(option), false, lib, name, type, attribs, count, output);
    }

    bool do_out_param(print_type option,
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
            r, static_cast<int>(option), false, lib, name, type, attribs, count, output);
    }

    void build_scoped_name(const class_entity* entity, std::string& name)
    {
        auto* owner = entity->get_owner();
        if (owner && !owner->get_name().empty())
        {
            build_scoped_name(owner, name);
        }
        auto entity_name = std::string(entity->get_entity_type() == entity_type::LIBRARY ? "i_" : "") + entity->get_name();
        name += entity_name + "::";
    }

    void write_constexpr(writer& header, const entity& constexpression)
    {
        if (constexpression.is_in_import())
            return;
        auto& function = static_cast<const function_entity&>(constexpression);
        header.print_tabs();
        header.raw("static constexpr {} {}", function.get_return_type(), function.get_name());
        if (!function.get_default_value().empty())
        {
            header.raw(" = {};\n", function.get_default_value());
        }
        else
        {
            header.raw("{{}};\n");
        }
    }

    std::string write_proxy_send_declaration(const class_entity& m_ob,
        const std::string& scope,
        const std::shared_ptr<function_entity>& function,
        bool& has_inparams,
        std::string additional_params,
        bool include_variadics)
    {
        std::stringstream stream;
        writer header(stream);

        header.raw("int {}{}(", scope, function->get_name());
        has_inparams = false;
        {
            uint64_t count = 1;
            for (auto& parameter : function->get_parameters())
            {
                auto in = is_in_param(parameter);
                auto out = is_out_param(parameter);

                if (out && !in)
                    continue;

                has_inparams = true;

                std::string output;
                if (!do_in_param(PROXY_PARAM_IN, m_ob, parameter.get_name(), parameter.get_type(), parameter, count, output))
                    continue;

                header.raw(output);
                header.raw(", ");
                count++;
            }
        }
        header.raw("std::vector<char>& __buffer");
        header.raw(additional_params);
        if (include_variadics)
            header.raw(", __Args... __args");
        header.raw(")");
        return stream.str();
    }

    std::string write_proxy_receive_declaration(const class_entity& m_ob,
        const std::string& scope,
        const std::shared_ptr<function_entity>& function,
        bool& has_inparams,
        std::string additional_params,
        bool include_variadics)
    {
        std::stringstream stream;
        writer header(stream);

        header.raw("int {}{}(", scope, function->get_name());
        has_inparams = false;

        uint64_t count = 1;
        for (auto& parameter : function->get_parameters())
        {
            auto out = is_out_param(parameter);

            if (!out)
                continue;
            has_inparams = true;

            std::string output;
            if (!do_out_param(PROXY_PARAM_OUT, m_ob, parameter.get_name(), parameter.get_type(), parameter, count, output))
                continue;
            header.raw(output);
            header.raw(", ");
            count++;
        }
        header.raw("const char* __rpc_buf, size_t __rpc_buf_size");
        header.raw(additional_params);
        if (include_variadics)
            header.raw(", __Args... __args");
        header.raw(")");
        return stream.str();
    }

    std::string write_stub_receive_declaration(const class_entity& m_ob,
        const std::string& scope,
        const std::shared_ptr<function_entity>& function,
        bool& has_outparams,
        std::string additional_params,
        bool include_variadics)
    {
        std::stringstream stream;
        writer header(stream);

        has_outparams = false;
        header.raw("int {}{}(", scope, function->get_name());

        uint64_t count = 1;
        for (auto& parameter : function->get_parameters())
        {
            auto out = is_out_param(parameter);
            auto in = is_in_param(parameter);

            if (!in && out)
                continue;
            has_outparams = true;

            std::string output;
            if (!do_in_param(STUB_PARAM_IN, m_ob, parameter.get_name(), parameter.get_type(), parameter, count, output))
                continue;
            header.raw(output);
            header.raw(", ");
            count++;
        }
        header.raw("const char* __rpc_buf, size_t __rpc_buf_size");
        header.raw(additional_params);
        if (include_variadics)
            header.raw(", __Args... __args");
        header.raw(")");
        return stream.str();
    }

    std::string write_stub_reply_declaration(const class_entity& m_ob,
        const std::string& scope,
        const std::shared_ptr<function_entity>& function,
        bool& has_outparams,
        std::string additional_params,
        bool include_variadics)
    {
        std::stringstream stream;
        writer header(stream);

        header.raw("int {}{}(", scope, function->get_name());
        has_outparams = false;
        {
            uint64_t count = 1;
            for (auto& parameter : function->get_parameters())
            {
                auto out = is_out_param(parameter);

                if (!out)
                    continue;

                has_outparams = true;
                std::string output;
                if (!do_out_param(STUB_PARAM_OUT, m_ob, parameter.get_name(), parameter.get_type(), parameter, count, output))
                    continue;

                header.raw(output);
                header.raw(", ");
                count++;
            }
        }
        header.raw("std::vector<char>& __buffer");
        header.raw(additional_params);
        if (include_variadics)
            header.raw(", __Args... __args");
        header.raw(")");
        return stream.str();
    }

    std::string client_sender_declaration(
        const class_entity& m_ob, const std::shared_ptr<function_entity>& function, bool& is_suitable)
    {
        std::stringstream stream;
        writer header(stream);

        header.raw("ReturnType {}(", function->get_name());
        is_suitable = true;
        {
            uint64_t count = 1;
            for (auto& parameter : function->get_parameters())
            {
                if (is_out_param(parameter))
                {
                    is_suitable = false;
                    // this function is not suitable as it is an out parameter
                    break;
                }
                if (is_interface_param(m_ob, parameter.get_type()))
                {
                    is_suitable = false;
                    // this function is not suitable as it is an interface parameter
                    break;
                }

                if (is_pointer(parameter.get_type()) || is_pointer_to_pointer(parameter.get_type()))
                {
                    is_suitable = false;
                    // this function is not suitable as it is an interface parameter
                    break;
                }

                if (count > 1)
                    header.raw(", ");

                render_parameter(header, m_ob, parameter);

                count++;
            }
        }
        header.raw(")");

        if (is_const_param(*function))
            header.raw(" const");
        return stream.str();
    }

    void write_method(const class_entity& m_ob, writer& header, const std::shared_ptr<function_entity>& function)
    {
        if (function->get_entity_type() == entity_type::FUNCTION_METHOD)
        {
            std::string scoped_namespace;
            build_scoped_name(&m_ob, scoped_namespace);

            header.print_tabs();
            if (function->has_value(rpc_attribute_types::deprecated_function)
                || function->has_value(rpc_attribute_types::fingerprint_contaminating_deprecated_function))
                header.raw("[[deprecated]] ");
            header.raw("virtual CORO_TASK({}) {}(", function->get_return_type(), function->get_name());
            bool has_parameter = false;
            for (auto& parameter : function->get_parameters())
            {
                if (has_parameter)
                {
                    header.raw(", ");
                }
                has_parameter = true;
                render_parameter(header, m_ob, parameter);
            }
            bool function_is_const = false;
            if (function->has_value("const"))
                function_is_const = true;
            if (function_is_const)
            {
                header.raw(") const = 0;\n");
            }
            else
            {
                header.raw(") = 0;\n");
            }
        }
        else if (function->get_entity_type() == entity_type::FUNCTION_PRIVATE)
        {
            header("private:");
        }
        else if (function->get_entity_type() == entity_type::FUNCTION_PUBLIC)
        {
            header("public:");
        }
    }

    void write_interface(const class_entity& m_ob, writer& header)
    {
        if (m_ob.is_in_import())
            return;

        header("");
        header("/****************************************************************************/");

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
        header("class {}_stub;", interface_name);
        header("class {}{} : public rpc::casting_interface", interface_name, base_class_declaration);
        header("{{");
        header("public:");
        header("static rpc::interface_ordinal get_id(uint64_t rpc_version)");
        header("{{");
        header("#ifdef RPC_V2");
        header("if(rpc_version == rpc::VERSION_2)");
        header("{{");
        header("return {{{}ull}};", fingerprint::generate(m_ob, {}, &header));
        header("}}");
        header("#endif");
        header("return {{0}};");
        header("}}");
        header("");
        header("static std::vector<rpc::function_info> get_function_info();");
        header("");
        header("virtual ~{}() = default;", interface_name);
        header("");
        header("// ********************* interface methods *********************");

        bool has_methods = false;
        for (auto& function : m_ob.get_functions())
        {
            if (function->get_entity_type() != entity_type::FUNCTION_METHOD)
                continue;
            has_methods = true;
        }

        if (has_methods)
        {
            for (auto& function : m_ob.get_functions())
            {
                if (function->get_entity_type() == entity_type::CPPQUOTE)
                {
                    if (function->is_in_import())
                        continue;
                    auto text = function->get_name();
                    header.write_buffer(text);
                    continue;
                }
                if (function->get_entity_type() == entity_type::FUNCTION_PUBLIC)
                {
                    header("public:");
                    continue;
                }
                if (function->get_entity_type() == entity_type::FUNCTION_PRIVATE)
                {
                    header("private:");
                    continue;
                }
                if (function->get_entity_type() == entity_type::CONSTEXPR)
                {
                    write_constexpr(header, *function);
                    continue;
                }
                else if (function->get_entity_type() == entity_type::CPPQUOTE)
                {
                    if (function->is_in_import())
                        continue;
                    auto text = function->get_name();
                    header.write_buffer(text);
                    continue;
                }
                if (function->get_entity_type() == entity_type::FUNCTION_METHOD)
                    write_method(m_ob, header, function);
            }
        }

        header("");
        header("public:");
        header("// ********************* compile time polymorphic serialisers *********************");
        header("// template pure static class for serialising proxy request data to a stub or some other target");
        header("template<typename __Serialiser, typename... __Args>");
        header("struct proxy_serialiser");
        header("{{");
        {
            std::unordered_set<std::string> unique_signatures;
            for (auto& function : m_ob.get_functions())
            {
                if (function->get_entity_type() == entity_type::FUNCTION_METHOD)
                {
                    bool has_params = false;
                    auto key = write_proxy_send_declaration(m_ob, "", function, has_params, "", true);
                    if (unique_signatures.emplace(key).second)
                    {
                        header("static {};", key);
                    }
                }
            }
        }
        header("}};");
        header("");
        header("// template pure static class for deserialising data from a proxy or some other target into a stub");
        header("template<typename __Serialiser, typename... __Args>");
        header("struct stub_deserialiser");
        header("{{");
        {
            std::unordered_set<std::string> unique_signatures;
            for (auto& function : m_ob.get_functions())
            {
                if (function->get_entity_type() == entity_type::FUNCTION_METHOD)
                {
                    bool has_params = false;
                    auto key = write_stub_receive_declaration(m_ob, "", function, has_params, "", true);
                    if (unique_signatures.emplace(key).second)
                    {
                        header("static {};", key);
                    }
                }
            }
        }
        header("}};");
        header("");
        header("// template pure static class for serialising reply data from a stub");
        header("template<typename __Serialiser, typename... __Args>");
        header("struct stub_serialiser");
        header("{{");
        {
            std::unordered_set<std::string> unique_signatures;
            for (auto& function : m_ob.get_functions())
            {
                if (function->get_entity_type() == entity_type::FUNCTION_METHOD)
                {
                    bool has_params = false;
                    auto key = write_stub_reply_declaration(m_ob, "", function, has_params, "", true);
                    if (unique_signatures.emplace(key).second)
                    {
                        header("static {};", key);
                    }
                }
            }
        }
        header("}};");
        header("");
        header("// template pure static class for a proxy deserialising reply data from a stub");
        header("template<typename __Serialiser, typename... __Args>");
        header("struct proxy_deserialiser");
        header("{{");
        {
            std::unordered_set<std::string> unique_signatures;
            for (auto& function : m_ob.get_functions())
            {
                if (function->get_entity_type() == entity_type::FUNCTION_METHOD)
                {
                    bool has_params = false;
                    auto key = write_proxy_receive_declaration(m_ob, "", function, has_params, "", true);
                    if (unique_signatures.emplace(key).second)
                    {
                        header("static {};", key);
                    }
                }
            }
        }
        header("}};");
        header("");

        {
            std::stringstream stream;
            writer output(stream, header.get_tab_count());
            bool has_usable_functions = false;

            output("// proxy class for serialising requests into a buffer for optional dispatch at a future time");
            output("template<class Parent, typename ReturnType>");
            output("class buffered_proxy_serialiser");
            output("{{");
            output("public:");
            output("using subclass = Parent;");
            {

                auto class_alias = get_full_name(m_ob, true, false, ".");
                int function_count = 0;
                std::unordered_set<std::string> unique_signatures;
                for (auto& function : m_ob.get_functions())
                {
                    if (function->get_entity_type() == entity_type::FUNCTION_METHOD)
                    {
                        function_count++;

                        bool is_suitable = true;
                        auto key = client_sender_declaration(m_ob, function, is_suitable);
                        if (!is_suitable)
                            continue;

                        has_usable_functions = true;

                        if (unique_signatures.emplace(key).second)
                        {
                            output("{}", key);
                            output("{{");

                            uint64_t count = 1;
                            {
                                output("std::vector<char> __buffer;");
                                output("auto __this = static_cast<subclass*>(this);");
                                output.print_tabs();
                                output.raw("auto __err = proxy_serialiser<rpc::serialiser::yas, rpc::encoding>::{}(",
                                    function->get_name());
                                for (auto& parameter : function->get_parameters())
                                {
                                    std::string mshl_val;
                                    {
                                        if (!do_in_param(SEND_PARAM_IN,
                                                m_ob,
                                                parameter.get_name(),
                                                parameter.get_type(),
                                                parameter,
                                                count,
                                                mshl_val))
                                            continue;

                                        output.raw(mshl_val);
                                    }
                                    count++;
                                }

                                output.raw("__buffer, __this->get_encoding());\n");

                                std::string tag = function->get_value("tag");
                                if (tag.empty())
                                    tag = "0";

                                output("return __this->register_call(__err, \"{}.{}\", {{{}}}, {}, __buffer);\n",
                                    class_alias,
                                    function->get_name(),
                                    function_count,
                                    tag); // get the method id later
                            }

                            output("}}");
                        }
                    }
                }
            }
            output("}};");
            output("");

            if (has_usable_functions)
                header.write_buffer(stream.str());
            else
                header("// no usable functions for a buffered_proxy_serialiser class");
        }
        header("friend {}_stub;", interface_name);
        header("}};");
        header("");
    };
}
