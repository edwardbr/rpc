#include <sstream>
#include <unordered_set>

#include "interface_declaration_generator.h"
#include "fingerprint_generator.h"
#include "helpers.h"

#include "attributes.h"
#include "rpc_attributes.h"

namespace rpc_generator
{
    enum print_type
    {
        PROXY_PARAM_IN,
        PROXY_PARAM_OUT,

        STUB_PARAM_IN,
        STUB_PARAM_OUT,
        
        SEND_PARAM_IN
    };

    struct renderer
    {
        enum param_type
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

        template<param_type type>
        std::string render(print_type option, const class_entity& lib, const std::string& name, bool is_in, bool is_out,
                           bool is_const, const std::string& object_type, uint64_t& count) const
        {
            static_assert(false);
            std::ignore = option;
            std::ignore = lib;
            std::ignore = name;
            std::ignore = is_in;
            std::ignore = is_out;
            std::ignore = is_const;
            std::ignore = object_type;
            std::ignore = count;
            return {};
        }
    };

    template<>
    std::string renderer::render<renderer::BY_VALUE>(print_type option, const class_entity& lib,
                                                     const std::string& name, bool is_in, bool is_out, bool is_const,
                                                     const std::string& object_type, uint64_t& count) const
    {
        std::ignore = lib;
        std::ignore = is_in;
        std::ignore = is_out;
        std::ignore = is_const;
        std::ignore = count;

        switch(option)
        {
        case PROXY_PARAM_IN:
            return fmt::format("const {}& {}", object_type, name);
        case STUB_PARAM_IN:
            return fmt::format("{}& {}", object_type, name);
        case STUB_PARAM_OUT:
            return fmt::format("const {}& {}", object_type, name);
        case PROXY_PARAM_OUT:
            return fmt::format("{}& {}", object_type, name);
        case SEND_PARAM_IN:
            return fmt::format("{0}, ", name);
        default:
            return "";
        }
    };

    template<>
    std::string renderer::render<renderer::REFERENCE>(print_type option, const class_entity& lib,
                                                      const std::string& name, bool is_in, bool is_out, bool is_const,
                                                      const std::string& object_type, uint64_t& count) const
    {
        std::ignore = lib;
        std::ignore = is_in;
        std::ignore = is_out;
        std::ignore = is_const;
        std::ignore = count;

        if(is_out)
        {
            throw std::runtime_error("REFERENCE does not support out vals");
        }

        switch(option)
        {
        case PROXY_PARAM_IN:
            return fmt::format("const {}& {}", object_type, name);
        case STUB_PARAM_IN:
            return fmt::format("{}& {}", object_type, name);
        case STUB_PARAM_OUT:
            return fmt::format("const {}& {}", object_type, name);
        case PROXY_PARAM_OUT:
            return fmt::format("{}& {}", object_type, name);
        case SEND_PARAM_IN:
            return fmt::format("{0}, ", name);
        default:
            return "";
        }
    };

    template<>
    std::string renderer::render<renderer::MOVE>(print_type option, const class_entity& lib, const std::string& name,
                                                 bool is_in, bool is_out, bool is_const, const std::string& object_type,
                                                 uint64_t& count) const
    {
        std::ignore = lib;
        std::ignore = is_in;
        std::ignore = count;

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
        case SEND_PARAM_IN:
            return fmt::format("std::move({0}), ", name);
        default:
            return "";
        }
    };

    template<>
    std::string renderer::render<renderer::POINTER>(print_type option, const class_entity& lib, const std::string& name,
                                                    bool is_in, bool is_out, bool is_const,
                                                    const std::string& object_type, uint64_t& count) const
    {
        std::ignore = lib;
        std::ignore = is_in;
        std::ignore = is_out;
        std::ignore = is_const;
        std::ignore = object_type;
        std::ignore = count;

        if(is_out)
        {
            throw std::runtime_error("POINTER does not support out vals");
        }

        switch(option)
        {
        case PROXY_PARAM_IN:
            return fmt::format("uint64_t {}", name);
        case STUB_PARAM_IN:
            return fmt::format("uint64_t& {}", name);
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
        std::ignore = lib;
        std::ignore = option;
        std::ignore = name;
        std::ignore = is_in;
        std::ignore = object_type;
        std::ignore = count;        

        if(is_const && is_out)
        {
            throw std::runtime_error("POINTER_REFERENCE does not support const out vals");
        }
        switch(option)
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
    };

    template<>
    std::string renderer::render<renderer::POINTER_POINTER>(print_type option, const class_entity& lib,
                                                            const std::string& name, bool is_in, bool is_out,
                                                            bool is_const, const std::string& object_type,
                                                            uint64_t& count) const
    {
        std::ignore = lib;
        std::ignore = is_in;
        std::ignore = is_out;
        std::ignore = is_const;
        std::ignore = object_type;
        std::ignore = count;        

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
        std::ignore = lib;
        std::ignore = is_in;
        std::ignore = is_const;
        std::ignore = object_type;
        std::ignore = count;        

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
        std::ignore = lib;
        std::ignore = is_in;
        std::ignore = is_out;
        std::ignore = is_const;
        std::ignore = object_type;
        std::ignore = count;

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

    bool do_in_param(print_type option, const class_entity& lib, const std::string& name, const std::string& type,
                    const std::list<std::string>& attributes, uint64_t& count, std::string& output)
    {
        auto in = is_in_param(attributes);
        auto out = is_out_param(attributes);
        auto is_const = is_const_param(attributes);
        auto by_value = std::find(attributes.begin(), attributes.end(), attribute_types::by_value_param) != attributes.end();

        if(out && !in)
            return false;

        std::string type_name = type;
        std::string reference_modifiers;
        strip_reference_modifiers(type_name, reference_modifiers);

        bool is_interface = is_interface_param(lib, type);

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
                    output = renderer().render<renderer::REFERENCE>(option, lib, name, in, out, is_const, type_name,
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

    bool do_out_param(print_type option, const class_entity& lib, const std::string& name, const std::string& type,
                     const std::list<std::string>& attributes, uint64_t& count, std::string& output)
    {
        auto in = is_in_param(attributes);
        auto out = is_out_param(attributes);
        auto is_const = is_const_param(attributes);

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

        bool is_interface = is_interface_param(lib, type);

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
                                             const std::shared_ptr<function_entity>& function,
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
                auto in = is_in_param(attributes);
                auto out = is_out_param(attributes);

                if(out && !in)
                    continue;

                has_inparams = true;

                std::string output;
                if(!do_in_param(PROXY_PARAM_IN, m_ob, parameter.get_name(), parameter.get_type(),
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
                                                const std::shared_ptr<function_entity>& function,
                                                bool& has_inparams, std::string additional_params,
                                                bool include_variadics)
    {
        std::stringstream stream;
        writer header(stream);

        header.raw("int {}{}(", scope, function->get_name());
        has_inparams = false;

        uint64_t count = 1;
        for(auto& parameter : function->get_parameters())
        {
            auto& attributes = parameter.get_attributes();
            auto out = is_out_param(attributes);

            if(!out)
                continue;
            has_inparams = true;

            std::string output;
            if(!do_out_param(PROXY_PARAM_OUT, m_ob, parameter.get_name(), parameter.get_type(),
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
                                               const std::shared_ptr<function_entity>& function,
                                               bool& has_outparams, std::string additional_params,
                                               bool include_variadics)
    {
        std::stringstream stream;
        writer header(stream);

        has_outparams = false;
        header.raw("int {}{}(", scope, function->get_name());

        uint64_t count = 1;
        for(auto& parameter : function->get_parameters())
        {
            auto& attributes = parameter.get_attributes();
            auto out = is_out_param(attributes);
            auto in = is_in_param(attributes);

            if(!in && out)
                continue;
            has_outparams = true;

            std::string output;
            if(!do_in_param(STUB_PARAM_IN, m_ob, parameter.get_name(), parameter.get_type(),
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
                                             const std::shared_ptr<function_entity>& function,
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
                auto out = is_out_param(attributes);

                if(!out)
                    continue;

                has_outparams = true;
                std::string output;
                if(!do_out_param(STUB_PARAM_OUT, m_ob, parameter.get_name(), parameter.get_type(),
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

    std::string client_sender_declaration(const class_entity& m_ob, const std::shared_ptr<function_entity>& function, bool& is_suitable)
    {
        std::stringstream stream;
        writer header(stream);

        header.raw("ReturnType {}(", function->get_name());
        is_suitable = true;
        {
            uint64_t count = 1;
            for(auto& parameter : function->get_parameters())
            {
                if(is_out_param(parameter.get_attributes()))
                {
                    is_suitable = false;
                    //this function is not suitable as it is an out parameter
                    break;
                }
                if(is_interface_param(m_ob, parameter.get_type()))
                {
                    is_suitable = false;
                    //this function is not suitable as it is an interface parameter
                    break;
                }
                
                if(is_pointer(parameter.get_type()) || is_pointer_to_pointer(parameter.get_type()))
                {
                    is_suitable = false;
                    //this function is not suitable as it is an interface parameter
                    break;
                }

                if(count > 1)
                    header.raw(", ");       
                    
                render_parameter(header, m_ob, parameter);

                count++;
            }
        }
        header.raw(")");
        
        if(is_const_param(function->get_attributes()))
            header.raw(" const");
        return stream.str();
    }

    void write_method(const class_entity& m_ob, writer& header, const std::shared_ptr<function_entity>& function)
    {
        if(function->get_entity_type() == entity_type::FUNCTION_METHOD)
        {
            std::string scoped_namespace;
            build_scoped_name(&m_ob, scoped_namespace);

            header.print_tabs();
            for(auto& item : function->get_attributes())
            {
                if(item == rpc_attribute_types::deprecated_function || item == rpc_attribute_types::fingerprint_contaminating_deprecated_function)
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
                render_parameter(header, m_ob, parameter);
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
            for(auto& function : m_ob.get_functions())
            {
                if(function->get_entity_type() == entity_type::FUNCTION_METHOD)
                {
                    bool has_params = false;
                    auto key = ::rpc_generator::write_proxy_send_declaration(m_ob, "", function, 
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
        header("// template pure static class for deserialising data from a proxy or some other target into a stub");
        header("template<typename __Serialiser, typename... __Args>");
        header("struct stub_deserialiser");
        header("{{");
        {
            std::unordered_set<std::string> unique_signatures;
            for(auto& function : m_ob.get_functions())
            {
                if(function->get_entity_type() == entity_type::FUNCTION_METHOD)
                {
                    bool has_params = false;
                    auto key = ::rpc_generator::write_stub_receive_declaration(m_ob, "", function,
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
        header("// template pure static class for serialising reply data from a stub");
        header("template<typename __Serialiser, typename... __Args>");
        header("struct stub_serialiser");
        header("{{");
        {
            std::unordered_set<std::string> unique_signatures;
            for(auto& function : m_ob.get_functions())
            {
                if(function->get_entity_type() == entity_type::FUNCTION_METHOD)
                {
                    bool has_params = false;
                    auto key = ::rpc_generator::write_stub_reply_declaration(m_ob, "", function,
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
        header("// template pure static class for a proxy deserialising reply data from a stub");
        header("template<typename __Serialiser, typename... __Args>");
        header("struct proxy_deserialiser");
        header("{{");
        {
            std::unordered_set<std::string> unique_signatures;
            for(auto& function : m_ob.get_functions())
            {
                if(function->get_entity_type() == entity_type::FUNCTION_METHOD)
                {
                    bool has_params = false;
                    auto key = ::rpc_generator::write_proxy_receive_declaration(m_ob, "", function,
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
                for(auto& function : m_ob.get_functions())
                {
                    if(function->get_entity_type() == entity_type::FUNCTION_METHOD)
                    {
                        function_count++;

                        bool is_suitable = true;
                        auto key
                            = ::rpc_generator::client_sender_declaration(m_ob, function, is_suitable);
                        if(!is_suitable)
                            continue;
                            
                        has_usable_functions = true;

                        if(unique_signatures.emplace(key).second)
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
                                for(auto& parameter : function->get_parameters())
                                {
                                    std::string mshl_val;
                                    {
                                        if(!do_in_param(SEND_PARAM_IN, m_ob, parameter.get_name(),
                                                    parameter.get_type(), parameter.get_attributes(), count, mshl_val))
                                            continue;

                                        output.raw(mshl_val);
                                    }
                                    count++;
                                }

                                output.raw("__buffer, __this->get_encoding());\n");
                               
                                std::string tag = function->get_attribute_value("tag");
                                if(tag.empty())
                                    tag = "0";
                                
                                output("return __this->register_call(__err, \"{}.{}\", {{{}}}, {}, __buffer);\n", class_alias, function->get_name(), function_count, tag); // get the method id later
                            }

                            output("}}");                        
                        }
                    }
                }
            }
            output("}};");
            output("");
            
            if(has_usable_functions)
                header.write_buffer(stream.str());
            else
                header("// no usable functions for a buffered_proxy_serialiser class");
        }
        header("friend {}_stub;", interface_name);
        header("}};");
        header("");
    };
}
