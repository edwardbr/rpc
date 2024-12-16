#include <sstream>
#include <unordered_set>

#include "interface_declaration_generator.h"
#include "fingerprint_generator.h"

namespace rpc_generator
{
    enum print_type
    {
        PROXY_PARAM_IN,
        PROXY_PARAM_OUT,

        STUB_PARAM_IN,
        STUB_PARAM_OUT,
    };

    struct renderer
    {
        enum param_type
        {
            BY_VALUE,
            REFERANCE,
            MOVE,
            POINTER,
            POINTER_REFERENCE,
            POINTER_POINTER,
            INTERFACE,
            INTERFACE_REFERENCE
        };

        template<param_type type>
        std::string render(print_type option, const class_entity& lib, const std::string& name, bool is_in, bool is_out,
                           bool is_const, const std::string& object_type, uint64_t& count) const
        {
            assert(false);
        }
    };

    template<>
    std::string renderer::render<renderer::BY_VALUE>(print_type option, const class_entity& lib,
                                                     const std::string& name, bool is_in, bool is_out, bool is_const,
                                                     const std::string& object_type, uint64_t& count) const
    {
        switch(option)
        {
        case PROXY_PARAM_IN:
            return fmt::format("{} {}", object_type, name);
        case STUB_PARAM_IN:
            return fmt::format("{}& {}", object_type, name);
        case STUB_PARAM_OUT:
            return fmt::format("{}& {}", object_type, name);
        case PROXY_PARAM_OUT:
            return fmt::format("{}& {}", object_type, name);
        default:
            return "";
        }
    };

    template<>
    std::string renderer::render<renderer::REFERANCE>(print_type option, const class_entity& lib,
                                                      const std::string& name, bool is_in, bool is_out, bool is_const,
                                                      const std::string& object_type, uint64_t& count) const
    {
        if(is_out)
        {
            throw std::runtime_error("REFERANCE does not support out vals");
        }

        switch(option)
        {
        case PROXY_PARAM_IN:
            return fmt::format("{}& {}", object_type, name);
        case STUB_PARAM_IN:
            return fmt::format("{}& {}", object_type, name);
        case STUB_PARAM_OUT:
            return fmt::format("{}& {}", object_type, name);
        case PROXY_PARAM_OUT:
            return fmt::format("{}& {}", object_type, name);
        default:
            return "";
        }
    };

    template<>
    std::string renderer::render<renderer::MOVE>(print_type option, const class_entity& lib, const std::string& name,
                                                 bool is_in, bool is_out, bool is_const, const std::string& object_type,
                                                 uint64_t& count) const
    {
        if(is_out)
        {
            throw std::runtime_error("MOVE does not support out vals");
        }
        if(is_const)
        {
            throw std::runtime_error("MOVE does not support const vals");
        }

        switch(option)
        {
        case PROXY_PARAM_IN:
            return fmt::format("{}&& {}", object_type, name);
        case STUB_PARAM_IN:
            return fmt::format("{}& {}", object_type, name);
        default:
            return "";
        }
    };

    template<>
    std::string renderer::render<renderer::POINTER>(print_type option, const class_entity& lib, const std::string& name,
                                                    bool is_in, bool is_out, bool is_const,
                                                    const std::string& object_type, uint64_t& count) const
    {
        if(is_out)
        {
            throw std::runtime_error("POINTER does not support out vals");
        }

        switch(option)
        {
        case PROXY_PARAM_IN:
            return fmt::format("uint64_t {}", name);
        case STUB_PARAM_IN:
            return fmt::format("uint64_t {}", name);
        default:
            return "";
        }
    };

    template<>
    std::string renderer::render<renderer::POINTER_REFERENCE>(print_type option, const class_entity& lib,
                                                              const std::string& name, bool is_in, bool is_out,
                                                              bool is_const, const std::string& object_type,
                                                              uint64_t& count) const
    {
        if(is_const && is_out)
        {
            throw std::runtime_error("POINTER_REFERENCE does not support const out vals");
        }
        switch(option)
        {
        case PROXY_PARAM_IN:
            return fmt::format("uint64_t& {}", name);
        case STUB_PARAM_IN:
            return fmt::format("{}*& {}", object_type, name);
        case STUB_PARAM_OUT:
            return fmt::format("uint64_t {}", name);
        case PROXY_PARAM_OUT:
            return fmt::format("uint64_t& {}", name);

        default:
            return "";
        }
    };

    template<>
    std::string renderer::render<renderer::POINTER_POINTER>(print_type option, const class_entity& lib,
                                                            const std::string& name, bool is_in, bool is_out,
                                                            bool is_const, const std::string& object_type,
                                                            uint64_t& count) const
    {
        switch(option)
        {
        case PROXY_PARAM_IN:
            return fmt::format("{}** {}", object_type, name);
        case STUB_PARAM_IN:
            return fmt::format("{}** {}", object_type, name);
        case STUB_PARAM_OUT:
            return fmt::format("uint64_t {}", name);
        case PROXY_PARAM_OUT:
            return fmt::format("uint64_t& {}", name);
        default:
            return "";
        }
    };

    template<>
    std::string renderer::render<renderer::INTERFACE>(print_type option, const class_entity& lib,
                                                      const std::string& name, bool is_in, bool is_out, bool is_const,
                                                      const std::string& object_type, uint64_t& count) const
    {
        if(is_out)
        {
            throw std::runtime_error("INTERFACE does not support out vals");
        }

        switch(option)
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
    };

    template<>
    std::string renderer::render<renderer::INTERFACE_REFERENCE>(print_type option, const class_entity& lib,
                                                                const std::string& name, bool is_in, bool is_out,
                                                                bool is_const, const std::string& object_type,
                                                                uint64_t& count) const
    {
        switch(option)
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
    };

    std::string get_encapsulated_shared_ptr_type(const std::string& type_name)
    {
        const std::string template_pattern = "rpc::shared_ptr<";
        auto pos = type_name.find(template_pattern);
        if(pos != std::string::npos)
        {
            pos += template_pattern.length();
            while(type_name[pos] == ' ' || type_name[pos] == '\n' || type_name[pos] == '\r' || type_name[pos] == '\t')
                pos++;
            auto rpos = type_name.rfind(">");
            if(rpos == std::string::npos)
            {
                std::cerr << fmt::format("template parameter is malformed {}", type_name);
                throw fmt::format("template parameter is malformed {}", type_name);
            }
            while(type_name[rpos] == ' ' || type_name[rpos] == '\n' || type_name[rpos] == '\r'
                  || type_name[rpos] == '\t')
                rpos++;
            return type_name.substr(pos, rpos - pos);
        }
        return type_name;
    }

    bool is_in_call(print_type option, const class_entity& lib, const std::string& name, const std::string& type,
                    const std::list<std::string>& attributes, uint64_t& count, std::string& output)
    {
        auto in = std::find(attributes.begin(), attributes.end(), "in") != attributes.end();
        auto out = std::find(attributes.begin(), attributes.end(), "out") != attributes.end();
        auto is_const = std::find(attributes.begin(), attributes.end(), "const") != attributes.end();
        auto by_value = std::find(attributes.begin(), attributes.end(), "by_value") != attributes.end();

        if(out && !in)
            return false;

        std::string type_name = type;
        std::string reference_modifiers;
        strip_reference_modifiers(type_name, reference_modifiers);

        std::string encapsulated_type = get_encapsulated_shared_ptr_type(type_name);

        bool is_interface = false;
        std::shared_ptr<class_entity> obj;
        if(lib.find_class(encapsulated_type, obj))
        {
            if(obj->get_entity_type() == entity_type::INTERFACE)
            {
                is_interface = true;
            }
        }

        if(!is_interface)
        {
            if(reference_modifiers.empty())
            {
                output = renderer().render<renderer::BY_VALUE>(option, lib, name, in, out, is_const, type_name, count);
            }
            else if(reference_modifiers == "&")
            {
                if(by_value)
                {
                    output
                        = renderer().render<renderer::BY_VALUE>(option, lib, name, in, out, is_const, type_name, count);
                }
                else
                {
                    output = renderer().render<renderer::REFERANCE>(option, lib, name, in, out, is_const, type_name,
                                                                    count);
                }
            }
            else if(reference_modifiers == "&&")
            {
                output = renderer().render<renderer::MOVE>(option, lib, name, in, out, is_const, type_name, count);
            }
            else if(reference_modifiers == "*")
            {
                output = renderer().render<renderer::POINTER>(option, lib, name, in, out, is_const, type_name, count);
            }
            else if(reference_modifiers == "*&")
            {
                output = renderer().render<renderer::POINTER_REFERENCE>(option, lib, name, in, out, is_const, type_name,
                                                                        count);
            }
            else if(reference_modifiers == "**")
            {
                output = renderer().render<renderer::POINTER_POINTER>(option, lib, name, in, out, is_const, type_name,
                                                                      count);
            }
            else
            {

                std::cerr << fmt::format("passing data by {} as in {} {} is not supported", reference_modifiers, type,
                                         name);
                throw fmt::format("passing data by {} as in {} {} is not supported", reference_modifiers, type, name);
            }
        }
        else
        {
            if(reference_modifiers.empty() || (reference_modifiers == "&" && (is_const || !out)))
            {
                output = renderer().render<renderer::INTERFACE>(option, lib, name, in, out, is_const, type_name, count);
            }
            else if(reference_modifiers == "&")
            {
                output = renderer().render<renderer::INTERFACE_REFERENCE>(option, lib, name, in, out, is_const,
                                                                          type_name, count);
            }
            else
            {
                std::cerr << fmt::format("passing interface by {} as in {} {} is not supported", reference_modifiers,
                                         type, name);
                throw fmt::format("passing interface by {} as in {} {} is not supported", reference_modifiers, type,
                                  name);
            }
        }
        return true;
    }

    bool is_out_call(print_type option, const class_entity& lib, const std::string& name, const std::string& type,
                     const std::list<std::string>& attributes, uint64_t& count, std::string& output)
    {
        auto in = std::find(attributes.begin(), attributes.end(), "in") != attributes.end();
        auto out = std::find(attributes.begin(), attributes.end(), "out") != attributes.end();
        auto is_const = std::find(attributes.begin(), attributes.end(), "const") != attributes.end();

        if(!out)
            return false;

        if(is_const)
        {
            std::cerr << fmt::format("out parameters cannot be null");
            throw fmt::format("out parameters cannot be null");
        }

        std::string type_name = type;
        std::string reference_modifiers;
        strip_reference_modifiers(type_name, reference_modifiers);

        std::string encapsulated_type = get_encapsulated_shared_ptr_type(type_name);

        bool is_interface = false;
        std::shared_ptr<class_entity> obj;
        if(lib.find_class(encapsulated_type, obj))
        {
            if(obj->get_entity_type() == entity_type::INTERFACE)
            {
                is_interface = true;
            }
        }

        if(reference_modifiers.empty())
        {
            std::cerr << fmt::format("out parameters require data to be sent by pointer or reference {} {} ", type,
                                     name);
            throw fmt::format("out parameters require data to be sent by pointeror reference {} {} ", type, name);
        }

        if(!is_interface)
        {
            if(reference_modifiers == "&")
            {
                output = renderer().render<renderer::BY_VALUE>(option, lib, name, in, out, is_const, type_name, count);
            }
            else if(reference_modifiers == "&&")
            {
                throw std::runtime_error("out call rvalue references is not possible");
            }
            else if(reference_modifiers == "*")
            {
                throw std::runtime_error("passing [out] by_pointer data by * will not work use a ** or *&");
            }
            else if(reference_modifiers == "*&")
            {
                output = renderer().render<renderer::POINTER_REFERENCE>(option, lib, name, in, out, is_const, type_name,
                                                                        count);
            }
            else if(reference_modifiers == "**")
            {
                output = renderer().render<renderer::POINTER_POINTER>(option, lib, name, in, out, is_const, type_name,
                                                                      count);
            }
            else
            {

                std::cerr << fmt::format("passing data by {} as in {} {} is not supported", reference_modifiers, type,
                                         name);
                throw fmt::format("passing data by {} as in {} {} is not supported", reference_modifiers, type, name);
            }
        }
        else
        {
            if(reference_modifiers == "&")
            {
                output = renderer().render<renderer::INTERFACE_REFERENCE>(option, lib, name, in, out, is_const,
                                                                          type_name, count);
            }
            else
            {
                std::cerr << fmt::format("passing interface by {} as in {} {} is not supported", reference_modifiers,
                                         type, name);
                throw fmt::format("passing interface by {} as in {} {} is not supported", reference_modifiers, type,
                                  name);
            }
        }
        return true;
    }

    void build_scoped_name(const class_entity* entity, std::string& name)
    {
        auto* owner = entity->get_owner();
        if(owner && !owner->get_name().empty())
        {
            build_scoped_name(owner, name);
        }
        auto entity_name
            = std::string(entity->get_entity_type() == entity_type::LIBRARY ? "i_" : "") + entity->get_name();
        name += entity_name + "::";
    }

    void write_constexpr(writer& header, const entity& constexpression)
    {
        if(constexpression.is_in_import())
            return;
        auto& function = static_cast<const function_entity&>(constexpression);
        header.print_tabs();
        header.raw("static constexpr {} {}", function.get_return_type(), function.get_name());
        if(!function.get_default_value().empty())
        {
            header.raw(" = {};\n", function.get_default_value());
        }
        else
        {
            header.raw("{{}};\n");
        }
    }

    std::string write_proxy_send_declaration(const class_entity& m_ob, const std::string& scope,
                                             const std::shared_ptr<function_entity>& function, int function_count,
                                             bool& has_inparams, std::string additional_params, bool include_variadics)
    {
        std::stringstream stream;
        writer header(stream);

        header.raw("int {}{}(", scope, function->get_name());
        has_inparams = false;
        {
            uint64_t count = 1;
            for(auto& parameter : function->get_parameters())
            {
                auto& attributes = parameter.get_attributes();
                auto in = std::find(attributes.begin(), attributes.end(), "in") != attributes.end();
                auto out = std::find(attributes.begin(), attributes.end(), "out") != attributes.end();

                if(out && !in)
                    continue;

                has_inparams = true;

                std::string modifier;
                for(auto& item : parameter.get_attributes())
                {
                    if(item == "const")
                        modifier = "const " + modifier;
                }

                std::string output;
                if(!is_in_call(PROXY_PARAM_IN, m_ob, parameter.get_name(), modifier + parameter.get_type(),
                               parameter.get_attributes(), count, output))
                    continue;

                header.raw(output);
                header.raw(", ");
                count++;
            }
        }
        header.raw("std::vector<char>& __buffer");
        header.raw(additional_params);
        if(include_variadics)
            header.raw(", __Args... __args");
        header.raw(")");            
        return stream.str();
    }

    std::string write_proxy_receive_declaration(const class_entity& m_ob, const std::string& scope,
                                                const std::shared_ptr<function_entity>& function, int& function_count,
                                                bool& has_inparams, std::string additional_params, bool include_variadics)
    {
        std::stringstream stream;
        writer header(stream);

        header.raw("int {}{}(", scope,
                   function->get_name());
        has_inparams = false;

        uint64_t count = 1;
        for(auto& parameter : function->get_parameters())
        {
            auto& attributes = parameter.get_attributes();
            auto out = std::find(attributes.begin(), attributes.end(), "out") != attributes.end();

            if(!out)
                continue;
            has_inparams = true;

            std::string modifier;
            for(auto& item : parameter.get_attributes())
            {
                if(item == "const")
                    modifier = "const " + modifier;
            }

            std::string output;
            if(!is_out_call(PROXY_PARAM_OUT, m_ob, parameter.get_name(), modifier + parameter.get_type(),
                            parameter.get_attributes(), count, output))
                continue;
            header.raw(output);
            header.raw(", ");
            count++;
        }
        header.raw("const char* __rpc_buf, size_t __rpc_buf_size");
        header.raw(additional_params);
        if(include_variadics)
            header.raw(", __Args... __args");
        header.raw(")");            
        return stream.str();
    }

    std::string write_stub_receive_declaration(const class_entity& m_ob, const std::string& scope,
                                               const std::shared_ptr<function_entity>& function, int function_count,
                                               bool& has_outparams, std::string additional_params, bool include_variadics)
    {
        std::stringstream stream;
        writer header(stream);

        has_outparams = false;
        header.raw("int {}{}(", scope,
                   function->get_name());

        uint64_t count = 1;
        for(auto& parameter : function->get_parameters())
        {
            auto& attributes = parameter.get_attributes();
            auto out = std::find(attributes.begin(), attributes.end(), "out") != attributes.end();
            auto in = std::find(attributes.begin(), attributes.end(), "in") != attributes.end();

            if(!in && out)
                continue;            
            has_outparams = true;

            std::string modifier;
            for(auto& item : parameter.get_attributes())
            {
                // if(item == "const") // we dont do const here as we need to load a temporary
                //     modifier = "const " + modifier;
            }

            std::string output;
            if(!is_in_call(STUB_PARAM_IN, m_ob, parameter.get_name(), modifier + parameter.get_type(),
                           parameter.get_attributes(), count, output))
                continue;
            header.raw(output);
            header.raw(", ");
            count++;
        }
        header.raw("const char* __rpc_buf, size_t __rpc_buf_size");
        header.raw(additional_params);
        if(include_variadics)
            header.raw(", __Args... __args");
        header.raw(")");            
        return stream.str();
    }

    std::string write_stub_reply_declaration(const class_entity& m_ob, const std::string& scope,
                                             const std::shared_ptr<function_entity>& function, int function_count,
                                             bool& has_outparams, std::string additional_params, bool include_variadics)
    {
        std::stringstream stream;
        writer header(stream);

        header.raw("int {}{}(", scope, function->get_name());
        has_outparams = false;
        {
            uint64_t count = 1;
            for(auto& parameter : function->get_parameters())
            {
                auto& attributes = parameter.get_attributes();
                auto out = std::find(attributes.begin(), attributes.end(), "out") != attributes.end();

                if(!out)
                    continue;

                has_outparams = true;
                std::string modifier;
                for(auto& item : parameter.get_attributes())
                {
                    if(item == "const")
                        modifier = "const " + modifier;
                }

                std::string output;
                if(!is_out_call(STUB_PARAM_OUT, m_ob, parameter.get_name(), modifier + parameter.get_type(),
                                parameter.get_attributes(), count, output))
                    continue;

                header.raw(output);
                header.raw(", ");
                count++;
            }
        }
        header.raw("std::vector<char>& __buffer");
        header.raw(additional_params);
        if(include_variadics)
            header.raw(", __Args... __args");
        header.raw(")");            
        return stream.str();
    }

    void write_method(const class_entity& m_ob, writer& header, const std::string& interface_name,
                      const std::shared_ptr<function_entity>& function, int& function_count)
    {
        if(function->get_entity_type() == entity_type::FUNCTION_METHOD)
        {
            std::string scoped_namespace;
            build_scoped_name(&m_ob, scoped_namespace);

            header.print_tabs();
            for(auto& item : function->get_attributes())
            {
                if(item == "deprecated" || item == "_deprecated")
                    header.raw("[[deprecated]] ");
            }
            header.raw("virtual {} {}(", function->get_return_type(), function->get_name());
            bool has_parameter = false;
            for(auto& parameter : function->get_parameters())
            {
                if(has_parameter)
                {
                    header.raw(", ");
                }
                has_parameter = true;
                std::string modifier;
                for(auto& item : parameter.get_attributes())
                {
                    if(item == "const")
                        modifier = "const " + modifier;
                }
                header.raw("{}{} {}", modifier, parameter.get_type(), parameter.get_name());
            }
            bool function_is_const = false;
            for(auto& item : function->get_attributes())
            {
                if(item == "const")
                    function_is_const = true;
            }
            if(function_is_const)
            {
                header.raw(") const = 0;\n");
            }
            else
            {
                header.raw(") = 0;\n");
            }
        }
        else if(function->get_entity_type() == entity_type::FUNCTION_PRIVATE)
        {
            header("private:");
        }
        else if(function->get_entity_type() == entity_type::FUNCTION_PUBLIC)
        {
            header("public:");
        }
    }

    void write_interface(const class_entity& m_ob, writer& header)
    {
        if(m_ob.is_in_import())
            return;

        header("");
        header("/****************************************************************************/");

        auto interface_name = std::string(m_ob.get_entity_type() == entity_type::LIBRARY ? "i_" : "") + m_ob.get_name();

        std::string base_class_declaration;
        auto bc = m_ob.get_base_classes();
        if(!bc.empty())
        {

            base_class_declaration = " : ";
            int i = 0;
            for(auto base_class : bc)
            {
                if(i)
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
        for(auto& function : m_ob.get_functions())
        {
            if(function->get_entity_type() != entity_type::FUNCTION_METHOD)
                continue;
            has_methods = true;
        }

        if(has_methods)
        {
            int function_count = 1;
            for(auto& function : m_ob.get_functions())
            {
                if(function->get_entity_type() == entity_type::CPPQUOTE)
                {
                    if(function->is_in_import())
                        continue;
                    auto text = function->get_name();
                    header.write_buffer(text);
                    continue;
                }
                if(function->get_entity_type() == entity_type::FUNCTION_PUBLIC)
                {
                    header("public:");
                    continue;
                }
                if(function->get_entity_type() == entity_type::FUNCTION_PRIVATE)
                {
                    header("private:");
                    continue;
                }
                if(function->get_entity_type() == entity_type::CONSTEXPR)
                {
                    write_constexpr(header, *function);
                    continue;
                }
                else if(function->get_entity_type() == entity_type::CPPQUOTE)
                {
                    if(function->is_in_import())
                        continue;
                    auto text = function->get_name();
                    header.write_buffer(text);
                    continue;
                }
                if(function->get_entity_type() == entity_type::FUNCTION_METHOD)
                    write_method(m_ob, header, interface_name, function, function_count);
            }
        }

        header("");
        header("public:");
        header("// ********************* compile time polymorphic serialisers *********************");
        header("// template class for serialising request data to a stub or some other target");
        header("template<typename __Serialiser, typename... __Args>");
        header("struct proxy_sender");
        header("{{");
        {
            int function_count = 0;
            std::unordered_set<std::string> unique_signatures;
            bool has_inparams = false;
            for(auto& function : m_ob.get_functions())
            {
                if(function->get_entity_type() == entity_type::FUNCTION_METHOD)
                {
                    function_count++;
                    bool has_params = false;
                    auto key
                        = ::rpc_generator::write_proxy_send_declaration(m_ob, "", function, function_count, has_params, "", true);
                    if(unique_signatures.emplace(key).second)
                    {
                        header("static {};", key);
                    }
                }
            }
        }
        header("}};");
        header("");
        header("// template class for serialising reply data to a proxy or some other target");
        header("template<typename __Serialiser, typename... __Args>");
        header("struct stub_receiver");
        header("{{");
        {
            int function_count = 0;
            std::unordered_set<std::string> unique_signatures;
            bool has_outparams = false;
            for(auto& function : m_ob.get_functions())
            {
                if(function->get_entity_type() == entity_type::FUNCTION_METHOD)
                {
                    function_count++;
                    bool has_params = false;
                    auto key = ::rpc_generator::write_stub_receive_declaration(m_ob, "", function, function_count,
                                                                               has_params, "", true);
                    if(unique_signatures.emplace(key).second)
                    {
                        header("static {};", key);
                    }
                }
            }
        }
        header("}};");
        header("");
        header("// template class for deserialising request data from a proxy or some other target");
        header("template<typename __Serialiser, typename... __Args>");
        header("struct stub_sender");
        header("{{");
        {
            int function_count = 0;
            std::unordered_set<std::string> unique_signatures;
            bool has_outparams = false;
            for(auto& function : m_ob.get_functions())
            {
                if(function->get_entity_type() == entity_type::FUNCTION_METHOD)
                {
                    function_count++;
                    bool has_params = false;
                    auto key
                        = ::rpc_generator::write_stub_reply_declaration(m_ob, "", function, function_count, has_params,"",  true);
                    if(unique_signatures.emplace(key).second)
                    {
                        header("static {};", key);
                    }
                }
            }
        }
        header("}};");
        header("");
        header("// template class for deserialising reply data from a stub or some other target");
        header("template<typename __Serialiser, typename... __Args>");
        header("struct proxy_receiver");
        header("{{");
        {
            int function_count = 0;
            std::unordered_set<std::string> unique_signatures;
            bool has_inparams = false;
            for(auto& function : m_ob.get_functions())
            {
                if(function->get_entity_type() == entity_type::FUNCTION_METHOD)
                {
                    function_count++;

                    bool has_params = false;
                    auto key = ::rpc_generator::write_proxy_receive_declaration(m_ob, "", function, function_count,
                                                                                has_params, "", true);
                    if(unique_signatures.emplace(key).second)
                    {
                        header("static {};", key);
                    }
                }
            }
        }
        header("}};");
        header("");
        header("friend {}_stub;", interface_name);
        header("}};");
        header("");
    };
}