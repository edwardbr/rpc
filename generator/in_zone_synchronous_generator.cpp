#include <type_traits>
#include <algorithm>
#include <tuple>
#include <type_traits>
#include "coreclasses.h"
#include "cpp_parser.h"

#include "writer.h"

#include "in_zone_synchronous_generator.h"

namespace enclave_marshaller
{
    namespace in_zone_synchronous_generator
    {
        void write_interface_predeclaration(const Library& lib, const ClassObject& m_ob, writer& header, writer& proxy,
                                            writer& stub)
        {
            proxy("class {}_proxy;", m_ob.name);
            stub("class {}_stub;", m_ob.name);
        };

        enum print_type
        {
            PROXY_MARSHALL_IN,
            PROXY_OUT_DECLARATION,
            PROXY_MARSHALL_OUT,
            PROXY_VALUE_RETURN,

            STUB_DEMARSHALL_DECLARATION,
            STUB_MARSHALL_IN,
            STUB_PARAM_CAST,
            STUB_MARSHALL_OUT
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
                INTERFACE_REFERENCE,
                INTERFACE_POINTER,
                INTERFACE_POINTER_REFERENCE,
                INTERFACE_POINTER_POINTER
            };

            template<param_type type>
            std::string render(print_type option, bool from_host, const Library& lib, const std::string& name,
                               bool is_in, bool is_out, bool is_const, const std::string& object_type,
                               uint64_t& count) const
            {
                assert(false);
            }
        };

        template<>
        std::string renderer::render<renderer::BY_VALUE>(print_type option, bool from_host, const Library& lib,
                                                         const std::string& name, bool is_in, bool is_out,
                                                         bool is_const, const std::string& object_type,
                                                         uint64_t& count) const
        {
            switch (option)
            {
            case PROXY_MARSHALL_IN:
                return fmt::format("  ,(\"_{}\", {})", count, name);
            case PROXY_MARSHALL_OUT:
                return fmt::format("  ,(\"_{}\", {})", count, name);
            case STUB_DEMARSHALL_DECLARATION:
                return fmt::format("{} {}_", object_type, name);
            case STUB_MARSHALL_IN:
                return fmt::format("{}_", name);
            case STUB_PARAM_CAST:
                return fmt::format("{}_", name);
            case STUB_MARSHALL_OUT:
                return fmt::format("  ,(\"_{}\", {}_)", count, name);
            default:
                return "";
            }
        };

        template<>
        std::string renderer::render<renderer::REFERANCE>(print_type option, bool from_host, const Library& lib,
                                                          const std::string& name, bool is_in, bool is_out,
                                                          bool is_const, const std::string& object_type,
                                                          uint64_t& count) const
        {
            if (is_out)
            {
                std::cerr << "REFERANCE does not support out vals\n";
                throw "REFERANCE does not support out vals\n";
            }

            switch (option)
            {
            case PROXY_MARSHALL_IN:
                return fmt::format("  ,(\"_{}\", (uint64_t)&{})", count, name);
            case PROXY_MARSHALL_OUT:
                return fmt::format("  ,(\"_{}\", (uint64_t)&{})", count, name);
            case STUB_DEMARSHALL_DECLARATION:
                return fmt::format("uint64_t {}_ = 0;", name);
            case STUB_MARSHALL_IN:
                return fmt::format("{}_", name);
            case STUB_PARAM_CAST:
                return fmt::format("*({}*){}_", object_type, name);
            default:
                return "";
            }
        };

        template<>
        std::string renderer::render<renderer::MOVE>(print_type option, bool from_host, const Library& lib,
                                                     const std::string& name, bool is_in, bool is_out, bool is_const,
                                                     const std::string& object_type, uint64_t& count) const
        {
            if (is_out)
            {
                std::cerr << "MOVE does not support out vals\n";
                throw "MOVE does not support out vals\n";
            }
            if (is_const)
            {
                std::cerr << "MOVE does not support const vals\n";
                throw "MOVE does not support const vals\n";
            }

            switch (option)
            {
            case PROXY_MARSHALL_IN:
                return fmt::format("  ,(\"_{}\", {})", count, name);
            case PROXY_MARSHALL_OUT:
                return fmt::format("  ,(\"_{}\", {})", count, name);
            case STUB_DEMARSHALL_DECLARATION:
                return fmt::format("{} {}_", object_type, name);
            case STUB_MARSHALL_IN:
                return fmt::format("{}_", name);
            case STUB_PARAM_CAST:
                return fmt::format("std::move({}_)", name);
            case STUB_MARSHALL_OUT:
                return fmt::format("  ,(\"_{}\", {}_)", count, name);
            default:
                return "";
            }
        };

        template<>
        std::string renderer::render<renderer::POINTER>(print_type option, bool from_host, const Library& lib,
                                                        const std::string& name, bool is_in, bool is_out, bool is_const,
                                                        const std::string& object_type, uint64_t& count) const
        {
            if (is_out)
            {
                std::cerr << "POINTER does not support out vals\n";
                throw "POINTER does not support out vals\n";
            }

            switch (option)
            {
            case PROXY_MARSHALL_IN:
                return fmt::format("  ,(\"_{}\", (uint64_t){})", count, name);
            case PROXY_MARSHALL_OUT:
                return fmt::format("  ,(\"_{}\", (uint64_t) {})", count, name);
            case STUB_DEMARSHALL_DECLARATION:
                return fmt::format("uint64_t {}_", name);
            case STUB_MARSHALL_IN:
                return fmt::format("{}_", name);
            case STUB_PARAM_CAST:
                return fmt::format("({}*){}_", object_type, name);
            default:
                return "";
            }
        };

        template<>
        std::string renderer::render<renderer::POINTER_REFERENCE>(print_type option, bool from_host, const Library& lib,
                                                                  const std::string& name, bool is_in, bool is_out,
                                                                  bool is_const, const std::string& object_type,
                                                                  uint64_t& count) const
        {
            if (is_const && is_out)
            {
                std::cerr << "POINTER_REFERENCE does not support const out vals\n";
                throw "POINTER_REFERENCE does not support const out vals\n";
            }
            switch (option)
            {
            case PROXY_MARSHALL_IN:
                return fmt::format("  ,(\"_{}\", {}_)", count, name);
            case PROXY_MARSHALL_OUT:
                return fmt::format("  ,(\"_{}\", {}_)", count, name);
            case STUB_DEMARSHALL_DECLARATION:
                return fmt::format("{}* {}_ = nullptr", object_type, name);
            case STUB_PARAM_CAST:
                return fmt::format("{}_", name);
            case PROXY_OUT_DECLARATION:
                return fmt::format("uint64_t {}_ = 0;", name);
            case STUB_MARSHALL_OUT:
                return fmt::format("  ,(\"_{}\", (uint64_t){}_)", count, name);
            case PROXY_VALUE_RETURN:
                return fmt::format("{} = ({}*){}_;", name, object_type, name);

            default:
                return "";
            }
        };

        template<>
        std::string renderer::render<renderer::POINTER_POINTER>(print_type option, bool from_host, const Library& lib,
                                                                const std::string& name, bool is_in, bool is_out,
                                                                bool is_const, const std::string& object_type,
                                                                uint64_t& count) const
        {
            switch (option)
            {
            case PROXY_MARSHALL_IN:
                return fmt::format("  ,(\"_{}\", {}_)", count, name);
            case PROXY_MARSHALL_OUT:
                return fmt::format("  ,(\"_{}\", {}_)", count, name);
            case STUB_DEMARSHALL_DECLARATION:
                return fmt::format("{}* {}_ = nullptr", object_type, name);
            case STUB_PARAM_CAST:
                return fmt::format("&{}_", name);
            case PROXY_VALUE_RETURN:
                return fmt::format("*{} = ({}*){}_;", name, object_type, name);
            case PROXY_OUT_DECLARATION:
                return fmt::format("uint64_t {}_ = 0;", name);
            case STUB_MARSHALL_OUT:
                return fmt::format("  ,(\"_{}\", (uint64_t){}_)", count, name);
            default:
                return "";
            }
        };

        template<>
        std::string
        renderer::render<renderer::INTERFACE_REFERENCE>(print_type option, bool from_host, const Library& lib,
                                                        const std::string& name, bool is_in, bool is_out, bool is_const,
                                                        const std::string& object_type, uint64_t& count) const
        {
            if (is_out)
            {
                std::cerr << "INTERFACE_REFERENCE does not support out vals\n";
                throw "INTERFACE_REFERENCE does not support out vals\n";
            }

            switch (option)
            {
            case PROXY_MARSHALL_IN:
                return fmt::format("  ,(\"_{}\", {})", count, name);
            case PROXY_MARSHALL_OUT:
                return fmt::format("  ,(\"_{}\", {})", count, name);
            case STUB_DEMARSHALL_DECLARATION:
                return "";
            case STUB_PARAM_CAST:
                return "";
            case PROXY_VALUE_RETURN:
            case PROXY_OUT_DECLARATION:
            case STUB_MARSHALL_OUT:
                return fmt::format("  ,(\"_{}\", (uint64_t){})", count, name);
            default:
                return "";
            }
        };

        template<>
        std::string renderer::render<renderer::INTERFACE_POINTER>(print_type option, bool from_host, const Library& lib,
                                                                  const std::string& name, bool is_in, bool is_out,
                                                                  bool is_const, const std::string& object_type,
                                                                  uint64_t& count) const
        {
            if (is_out)
            {
                std::cerr << "INTERFACE_POINTER does not support out vals\n";
                throw "INTERFACE_POINTER does not support out vals\n";
            }

            switch (option)
            {
            case PROXY_MARSHALL_IN:
                return fmt::format("  ,(\"_{}\", {}_)", count, name);
            case PROXY_MARSHALL_OUT:
                return fmt::format("  ,(\"_{}\", {}_)", count, name);
            case STUB_DEMARSHALL_DECLARATION:
                return "";
            case STUB_PARAM_CAST:
                return "";
            case PROXY_VALUE_RETURN:
            case PROXY_OUT_DECLARATION:
            case STUB_MARSHALL_OUT:
                return fmt::format("  ,(\"_{}\", (uint64_t){}_)", count, name);
            default:
                return "";
            }
        };

        template<>
        std::string renderer::render<renderer::INTERFACE_POINTER_REFERENCE>(print_type option, bool from_host,
                                                                            const Library& lib, const std::string& name,
                                                                            bool is_in, bool is_out, bool is_const,
                                                                            const std::string& object_type,
                                                                            uint64_t& count) const
        {
            switch (option)
            {
            case PROXY_MARSHALL_IN:
                return fmt::format("  ,(\"_{}\", {}_)", count, name);
            case PROXY_MARSHALL_OUT:
                return fmt::format("  ,(\"_{}\", {}_)", count, name);
            case STUB_DEMARSHALL_DECLARATION:
                return "";
            case STUB_PARAM_CAST:
                return "";
            case PROXY_VALUE_RETURN:
            case PROXY_OUT_DECLARATION:
            case STUB_MARSHALL_OUT:
                return fmt::format("  ,(\"_{}\", (uint64_t){}_)", count, name);
            default:
                return "";
            }
        };

        template<>
        std::string renderer::render<renderer::INTERFACE_POINTER_POINTER>(print_type option, bool from_host,
                                                                          const Library& lib, const std::string& name,
                                                                          bool is_in, bool is_out, bool is_const,
                                                                          const std::string& object_type,
                                                                          uint64_t& count) const
        {
            switch (option)
            {
            case PROXY_MARSHALL_IN:
                return fmt::format("  ,(\"_{}\", {}_)", count, name);
            case PROXY_MARSHALL_OUT:
                return fmt::format("  ,(\"_{}\", {}_)", count, name);
            case STUB_DEMARSHALL_DECLARATION:
                return "";
            case STUB_PARAM_CAST:
                return "";
            case PROXY_VALUE_RETURN:
            case PROXY_OUT_DECLARATION:
            case STUB_MARSHALL_OUT:
                return fmt::format("  ,(\"_{}\", (uint64_t){}_)", count, name);
            default:
                return "";
            }
        }

        bool is_in_call(print_type option, bool from_host, const Library& lib, const std::string& name,
                        const std::string& type, const std::list<std::string>& attributes, uint64_t& count,
                        std::string& output)
        {
            auto in = std::find(attributes.begin(), attributes.end(), "in") != attributes.end();
            auto out = std::find(attributes.begin(), attributes.end(), "out") != attributes.end();
            auto is_const = std::find(attributes.begin(), attributes.end(), "const") != attributes.end();
            auto by_value = std::find(attributes.begin(), attributes.end(), "by_value") != attributes.end();

            if (out && !in)
                return false;

            std::string type_name = type;
            std::string referenceModifiers;
            stripReferenceModifiers(type_name, referenceModifiers);

            bool is_interface = false;
            for (auto it = lib.m_classes.begin(); it != lib.m_classes.end(); it++)
            {
                if ((*it)->name == type_name && (*it)->type == ObjectTypeInterface)
                {
                    is_interface = true;
                    break;
                }
            }

            if (!is_interface)
            {
                if (referenceModifiers.empty())
                {
                    output = renderer().render<renderer::BY_VALUE>(option, from_host, lib, name, in, out, is_const,
                                                                   type_name, count);
                }
                else if (referenceModifiers == "&")
                {
                    if (by_value)
                    {
                        output = renderer().render<renderer::BY_VALUE>(option, from_host, lib, name, in, out, is_const,
                                                                       type_name, count);
                    }
                    else if (from_host == false)
                    {
                        std::cerr << "passing data by reference from a non host zone is not allowed\n";
                        throw "passing data by reference from a non host zone is not allowed\n";
                    }
                    else
                    {
                        output = renderer().render<renderer::REFERANCE>(option, from_host, lib, name, in, out, is_const,
                                                                        type_name, count);
                    }
                }
                else if (referenceModifiers == "&&")
                {
                    output = renderer().render<renderer::MOVE>(option, from_host, lib, name, in, out, is_const,
                                                               type_name, count);
                }
                else if (referenceModifiers == "*")
                {
                    output = renderer().render<renderer::POINTER>(option, from_host, lib, name, in, out, is_const,
                                                                  type_name, count);
                }
                else if (referenceModifiers == "*&")
                {
                    output = renderer().render<renderer::POINTER_REFERENCE>(option, from_host, lib, name, in, out,
                                                                            is_const, type_name, count);
                }
                else if (referenceModifiers == "**")
                {
                    output = renderer().render<renderer::POINTER_POINTER>(option, from_host, lib, name, in, out,
                                                                          is_const, type_name, count);
                }
                else
                {

                    std::cerr << fmt::format("passing data by {} as in {} {} is not supported", referenceModifiers,
                                             type, name);
                    throw fmt::format("passing data by {} as in {} {} is not supported", referenceModifiers, type,
                                      name);
                }
            }
            else
            {
                if (referenceModifiers.empty())
                {
                    std::cerr << "passing interfaces by value is not possible\n";
                    throw "passing interfaces by value is not possible\n";
                }
                if (referenceModifiers == "&")
                {
                    if (from_host == false)
                    {
                        std::cerr << "passing data by reference from a non host zone is not allowed\n";
                        throw "passing data by reference from a non host zone is not allowed\n";
                    }
                    else
                    {
                        output = renderer().render<renderer::INTERFACE_REFERENCE>(option, from_host, lib, name, in, out,
                                                                                  is_const, type_name, count);
                    }
                }
                else if (referenceModifiers == "*")
                {
                    output = renderer().render<renderer::INTERFACE_POINTER>(option, from_host, lib, name, in, out,
                                                                            is_const, type_name, count);
                }
                else if (referenceModifiers == "*&")
                {
                    output = renderer().render<renderer::INTERFACE_POINTER_REFERENCE>(option, from_host, lib, name, in,
                                                                                      out, is_const, type_name, count);
                }
                else if (referenceModifiers == "**")
                {
                    output = renderer().render<renderer::INTERFACE_POINTER_POINTER>(option, from_host, lib, name, in,
                                                                                    out, is_const, type_name, count);
                }
                else
                {
                    std::cerr << fmt::format("passing interface by {} as in {} {} is not supported", referenceModifiers,
                                             type, name);
                    throw fmt::format("passing interface by {} as in {} {} is not supported", referenceModifiers, type,
                                      name);
                }
            }
            return true;
        }

        bool is_out_call(print_type option, bool from_host, const Library& lib, const std::string& name,
                         const std::string& type, const std::list<std::string>& attributes, uint64_t& count,
                         std::string& output)
        {
            auto in = std::find(attributes.begin(), attributes.end(), "in") != attributes.end();
            auto out = std::find(attributes.begin(), attributes.end(), "out") != attributes.end();
            auto by_value = std::find(attributes.begin(), attributes.end(), "by_value") != attributes.end();
            auto is_const = std::find(attributes.begin(), attributes.end(), "const") != attributes.end();

            if (!out)
                return false;

            if (is_const)
            {
                std::cerr << fmt::format("out parameters cannot be null");
                throw fmt::format("out parameters cannot be null");
            }

            std::string type_name = type;
            std::string referenceModifiers;
            stripReferenceModifiers(type_name, referenceModifiers);

            bool is_interface = false;
            for (auto it = lib.m_classes.begin(); it != lib.m_classes.end(); it++)
            {
                if ((*it)->name == type_name && (*it)->type == ObjectTypeInterface)
                {
                    is_interface = true;
                    break;
                }
            }

            if (referenceModifiers.empty())
            {
                std::cerr << fmt::format("out parameters require data to be sent by pointer or reference {} {} ", type,
                                         name);
                throw fmt::format("out parameters require data to be sent by pointeror reference {} {} ", type, name);
            }

            if (!is_interface)
            {
                if (referenceModifiers == "&")
                {
                    if (by_value)
                    {
                        output = renderer().render<renderer::BY_VALUE>(option, from_host, lib, name, in, out, is_const,
                                                                       type_name, count);
                    }
                    else
                    {
                        std::cerr << "passing data by reference as an out call is not possible\n";
                        throw "passing data by reference as an out call is not possible\n";
                    }
                }
                else if (referenceModifiers == "&&")
                {
                    std::cerr << "out call rvalue references is not possible\n";
                    throw "out call rvalue references is not possible\n";
                }
                else if (referenceModifiers == "*")
                {
                    std::cerr << "passing [out] by_pointer data by * will not work use a ** or *&\n";
                    throw "passing [out] by_pointer data by * will not work use a ** or *&\n";
                }
                else if (referenceModifiers == "*&")
                {
                    output = renderer().render<renderer::POINTER_REFERENCE>(option, from_host, lib, name, in, out,
                                                                            is_const, type_name, count);
                }
                else if (referenceModifiers == "**")
                {
                    output = renderer().render<renderer::POINTER_POINTER>(option, from_host, lib, name, in, out,
                                                                          is_const, type_name, count);
                }
                else
                {

                    std::cerr << fmt::format("passing data by {} as in {} {} is not supported", referenceModifiers,
                                             type, name);
                    throw fmt::format("passing data by {} as in {} {} is not supported", referenceModifiers, type,
                                      name);
                }
            }
            else
            {
                if (referenceModifiers.empty())
                {
                    std::cerr << "passing interfaces by value is not possible\n";
                    throw "passing interfaces by value is not possible\n";
                }
                if (referenceModifiers == "&")
                {
                    if (from_host == false)
                    {
                        std::cerr << "passing data by reference from a non host zone is not allowed\n";
                        throw "passing data by reference from a non host zone is not allowed\n";
                    }
                    else
                    {
                        output = renderer().render<renderer::INTERFACE_REFERENCE>(option, from_host, lib, name, in, out,
                                                                                  is_const, type_name, count);
                    }
                }
                else if (referenceModifiers == "*")
                {
                    output = renderer().render<renderer::INTERFACE_POINTER>(option, from_host, lib, name, in, out,
                                                                            is_const, type_name, count);
                }
                else if (referenceModifiers == "*&")
                {
                    output = renderer().render<renderer::INTERFACE_POINTER_REFERENCE>(option, from_host, lib, name, in,
                                                                                      out, is_const, type_name, count);
                }
                else if (referenceModifiers == "**")
                {
                    output = renderer().render<renderer::INTERFACE_POINTER_POINTER>(option, from_host, lib, name, in,
                                                                                    out, is_const, type_name, count);
                }
                else
                {
                    std::cerr << fmt::format("passing interface by {} as in {} {} is not supported", referenceModifiers,
                                             type, name);
                    throw fmt::format("passing interface by {} as in {} {} is not supported", referenceModifiers, type,
                                      name);
                }
            }
            return true;
        }

        void write_interface(bool from_host, const Library& lib, const ClassObject& m_ob, writer& header, writer& proxy,
                             writer& stub, int id)
        {
            auto interface_name = std::string(m_ob.type == ObjectLibrary ? "i_" : "") + m_ob.name;
            header("class {}{}{}", interface_name, m_ob.parentName.empty() ? "" : ":", m_ob.parentName);
            header("{{");
            header("public:");
            header("static constexpr uint64_t id = {};", id);
            header("virtual ~{}() = default;", interface_name);

            proxy("class {0}_proxy : public i_proxy<{0}>", interface_name);
            proxy("{{");
            proxy("public:");
            proxy("");
            proxy("{}_proxy(std::shared_ptr<object_proxy> object_proxy) : ", interface_name);
            proxy("  i_proxy<{}>(object_proxy)", interface_name);
            proxy("  {{}}", interface_name);
            proxy("virtual ~{}_proxy(){{}}", interface_name);
            proxy("");

            stub("class {0}_stub : public i_interface_marshaller, public std::enable_shared_from_this<{0}_stub>", interface_name);
            stub("{{");
            stub("std::shared_ptr<{}> target_;", interface_name);
            stub("public:");
            stub("");
            stub("{0}_stub(std::shared_ptr<{0}>& target) : ", interface_name);
            stub("  target_(target)", interface_name);
            stub("  {{}}");
            stub("");
            stub("uint64_t get_interface_id() override {{ return {}::id; }};", interface_name);
            stub("std::shared_ptr<{}> get_target() {{ return target_; }};", interface_name);
            stub("error_code call(uint64_t method_id, size_t in_size_, const char* in_buf_, size_t out_size_, char* "
                 "out_buf_) override");
            stub("{{");

            bool has_methods = false;
            for (auto& function : m_ob.functions)
            {
                if (function.type != FunctionTypeMethod)
                    continue;
                has_methods = true;
            }

            if (has_methods)
            {
                stub("switch(method_id)");
                stub("{{");

                int function_count = 1;
                for (auto& function : m_ob.functions)
                {
                    if (function.type != FunctionTypeMethod)
                        continue;

                    stub("case {}:", function_count);
                    stub("{{");

                    header.print_tabs();
                    proxy.print_tabs();
                    header.raw("virtual {} {}(", function.returnType, function.name);
                    proxy.raw("virtual {} {}_proxy::{} (", function.returnType, interface_name, function.name);
                    bool has_parameter = false;
                    for (auto& parameter : function.parameters)
                    {
                        if (has_parameter)
                        {
                            header.raw(", ");
                            proxy.raw(", ");
                        }
                        has_parameter = true;
                        std::string modifier;
                        for (auto it = parameter.m_attributes.begin(); it != parameter.m_attributes.end(); it++)
                        {
                            if (*it == "const")
                                modifier = "const " + modifier;
                        }
                        header.raw("{}{} {}", modifier, parameter.type, parameter.name);
                        proxy.raw("{}{} {}", modifier, parameter.type, parameter.name);
                    }
                    header.raw(") = 0;\n");
                    proxy.raw(") override\n");
                    proxy("{{");
                    proxy("if(!object_proxy_)");
                    proxy("{{");
                    proxy("return -1;");
                    proxy("}}");

                    bool has_inparams = false;

                    {
                        stub("//STUB_DEMARSHALL_DECLARATION");
                        uint64_t count = 1;
                        for (auto& parameter : function.parameters)
                        {
                            bool is_interface = false;
                            std::string output;
                            if (is_in_call(STUB_DEMARSHALL_DECLARATION, from_host, lib, parameter.name, parameter.type,
                                           parameter.m_attributes, count, output))
                                has_inparams = true;
                            else
                                is_out_call(STUB_DEMARSHALL_DECLARATION, from_host, lib, parameter.name, parameter.type,
                                            parameter.m_attributes, count, output);
                            stub("{};", output);
                        }
                    }

                    if (has_inparams)
                    {
                        proxy("//PROXY_MARSHALL_IN");
                        proxy("const auto in_ = yas::save<yas::mem|yas::binary>(YAS_OBJECT_NVP(");
                        proxy("  \"in\"");

                        stub("//STUB_MARSHALL_IN");
                        stub("yas::intrusive_buffer in(in_buf_, in_size_);");
                        stub("yas::load<yas::mem|yas::binary>(in, YAS_OBJECT_NVP(");
                        stub("  \"in\"");

                        uint64_t count = 1;
                        for (auto& parameter : function.parameters)
                        {
                            std::string output;
                            {
                                if (!is_in_call(PROXY_MARSHALL_IN, from_host, lib, parameter.name, parameter.type,
                                                parameter.m_attributes, count, output))
                                    continue;

                                proxy(output);
                            }
                            {
                                if (!is_in_call(STUB_MARSHALL_IN, from_host, lib, parameter.name, parameter.type,
                                                parameter.m_attributes, count, output))
                                    continue;

                                stub("  ,(\"_{}\", {})", count, output);
                            }
                            count++;
                        }

                        proxy("  ));");
                        stub("  ));");
                    }
                    else
                    {
                        proxy("const yas::shared_buffer in_;");
                    }

                    // proxy("for(int i = 0;i < in_.size;i++){{std::cout << in_.data.get() + i;}} std::cout <<
                    // \"\\n\";");

                    proxy("char out_buf[10000];");
                    proxy("error_code ret = object_proxy_->send({}::id, {}, in_.size, in_.data.get(), 10000, out_buf);",
                          interface_name, function_count);
                    proxy("if(ret)");
                    proxy("{{");
                    proxy("return ret;");
                    proxy("}}");

                    stub("//STUB_PARAM_CAST");
                    stub.print_tabs();
                    stub.raw("error_code ret = target_->{}(", function.name);

                    {
                        bool has_param = false;
                        uint64_t count = 1;
                        for (auto& parameter : function.parameters)
                        {
                            std::string output;
                            if (!is_in_call(STUB_PARAM_CAST, from_host, lib, parameter.name, parameter.type,
                                            parameter.m_attributes, count, output))
                                is_out_call(STUB_PARAM_CAST, from_host, lib, parameter.name, parameter.type,
                                            parameter.m_attributes, count, output);
                            if (has_param)
                            {
                                stub.raw(",");
                            }
                            has_param = true;
                            stub.raw("{}", output);
                        }
                    }
                    stub.raw(");\n");
                    stub("if(ret)");
                    stub("  return ret;");
                    stub("");

                    {
                        uint64_t count = 1;
                        proxy("//PROXY_OUT_DECLARATION");
                        for (auto& parameter : function.parameters)
                        {
                            count++;
                            std::string output;
                            if (is_in_call(PROXY_OUT_DECLARATION, from_host, lib, parameter.name, parameter.type,
                                           parameter.m_attributes, count, output))
                                continue;
                            if (!is_out_call(PROXY_OUT_DECLARATION, from_host, lib, parameter.name, parameter.type,
                                             parameter.m_attributes, count, output))
                                continue;

                            proxy(output);
                        }
                    }

                    {
                        uint64_t count = 1;
                        proxy("//PROXY_MARSHALL_OUT");
                        proxy(
                            "yas::load<yas::mem|yas::binary>(yas::intrusive_buffer{{out_buf, 10000}}, YAS_OBJECT_NVP(");
                        proxy("  \"out\"");
                        proxy("  ,(\"_{}\", ret)", count);

                        stub("//STUB_MARSHALL_OUT");
                        stub("yas::mem_ostream os(out_buf_, out_size_);");
                        stub("yas::save<yas::mem|yas::binary>(os, YAS_OBJECT_NVP(");
                        stub("  \"out\"");
                        stub("  ,(\"_{}\", ret)", count);

                        for (auto& parameter : function.parameters)
                        {
                            count++;
                            std::string output;
                            if (!is_out_call(PROXY_MARSHALL_OUT, from_host, lib, parameter.name, parameter.type,
                                             parameter.m_attributes, count, output))
                                continue;
                            proxy(output);

                            if (!is_out_call(STUB_MARSHALL_OUT, from_host, lib, parameter.name, parameter.type,
                                             parameter.m_attributes, count, output))
                                continue;

                            stub(output);
                        }
                    }
                    proxy("  ));");

                    proxy("//PROXY_VALUE_RETURN");
                    {
                        uint64_t count = 1;
                        for (auto& parameter : function.parameters)
                        {
                            count++;
                            std::string output;
                            if (is_in_call(PROXY_VALUE_RETURN, from_host, lib, parameter.name, parameter.type,
                                           parameter.m_attributes, count, output))
                                continue;
                            if (!is_out_call(PROXY_VALUE_RETURN, from_host, lib, parameter.name, parameter.type,
                                             parameter.m_attributes, count, output))
                                continue;

                            proxy(output);
                        }
                    }

                    proxy("return ret;");
                    proxy("}}");
                    proxy("");

                    stub("  ));");
                    stub("return ret;");

                    function_count++;
                    stub("}}");
                    stub("break;");
                }

                stub("default:");
                stub("return -1;");
                stub("}};");
            }

            header("}};");
            header("");
            proxy("}};");
            proxy("");

            stub("return -1;");
            stub("}}");
            stub("error_code cast(uint64_t interface_id, std::shared_ptr<i_interface_marshaller>& new_stub) override;");
            stub("}};");
            stub("");
        };

        void write_stub_factory(bool from_host, const Library& lib, const ClassObject& m_ob, writer& stub, int id)
        {
            auto interface_name = std::string(m_ob.type == ObjectLibrary ? "i_" : "") + m_ob.name;
            stub("if(interface_id == {}::id)", interface_name);
            stub("{{");
            stub("auto* tmp = dynamic_cast<{0}*>(original->get_target().get());", interface_name);
            stub("if(tmp != nullptr)");
            stub("{{");
            stub("std::shared_ptr<{}> tmp_ptr(original->get_target(), tmp);", interface_name);
            stub("new_stub = std::shared_ptr<i_interface_marshaller>(new {0}_stub(tmp_ptr));",interface_name);
            stub("return 0;");
            stub("}}");
            stub("return -1;");

            stub("}}");
        }

        void write_stub_cast_factory(bool from_host, const Library& lib, const ClassObject& m_ob, writer& stub, int id)
        {
            auto interface_name = std::string(m_ob.type == ObjectLibrary ? "i_" : "") + m_ob.name;
            stub("error_code {}_stub::cast(uint64_t interface_id, std::shared_ptr<i_interface_marshaller>& new_stub)", interface_name);
            stub("{{");
            stub("error_code ret = stub_factory(interface_id, shared_from_this(), new_stub);");
            stub("return ret;");
            stub("}}");
        }

        void write_struct(const ClassObject& m_ob, writer& header)
        {
            header("struct {}{}{}", m_ob.name, m_ob.parentName.empty() ? "" : ":", m_ob.parentName);
            header("{{");

            for (auto& field : m_ob.functions)
            {
                if (field.type != FunctionTypeVariable)
                    continue;

                header.print_tabs();
                header.raw("{} {};\n", field.returnType, field.name);
            }

            header("");
            header("// one member-function for save/load");
            header("template<typename Ar>");
            header("void serialize(Ar &ar)");
            header("{{");
            header("ar & YAS_OBJECT(\"{}\"", m_ob.name);

            int count = 0;
            for (auto& field : m_ob.functions)
            {
                header("  ,(\"_{}\", {})", count++, field.name);
            }
            header(");");

            header("}}");

            header("}};");
        };

        void write_library(bool from_host, const Library& lib, const ClassObject& m_ob, writer& header, writer& proxy,
                           writer& stub)
        {
            for (auto& name : m_ob.m_ownedClasses)
            {
                const ClassObject* obj = nullptr;
                if (!lib.FindClassObject(name, obj))
                {
                    continue;
                }
                if (obj->type == ObjectTypeInterface)
                    write_interface_predeclaration(lib, *obj, header, proxy, stub);
            }

            proxy("");

            {
                int id = 1;
                for (auto& name : m_ob.m_ownedClasses)
                {
                    const ClassObject* obj = nullptr;
                    if (!lib.FindClassObject(name, obj))
                    {
                        continue;
                    }
                    if (obj->type == ObjectTypeInterface)
                        write_interface(from_host, lib, *obj, header, proxy, stub, id++);
                }
            }

            write_interface(from_host, lib, m_ob, header, proxy, stub, 0);

            {
                stub("template<class T>");
                stub("error_code stub_factory(uint64_t interface_id, std::shared_ptr<T> original, "
                     "std::shared_ptr<i_interface_marshaller>& new_stub)");
                stub("{{");
                stub("error_code ret = -1;");
                stub("if(interface_id == original->get_interface_id())");
                stub("{{");
                stub("new_stub = original;");
                stub("return 0;;");
                stub("}}");
                int id = 1;
                for (auto& name : m_ob.m_ownedClasses)
                {
                    const ClassObject* obj = nullptr;
                    if (!lib.FindClassObject(name, obj))
                    {
                        continue;
                    }
                    if (obj->type == ObjectTypeInterface)
                        write_stub_factory(from_host, lib, *obj, stub, id++);
                }
                write_stub_factory(from_host, lib, m_ob, stub, 0);
                stub("return ret;");
                stub("}}");
            }

            {
                int id = 1;
                for (auto& name : m_ob.m_ownedClasses)
                {
                    const ClassObject* obj = nullptr;
                    if (!lib.FindClassObject(name, obj))
                    {
                        continue;
                    }
                    if (obj->type == ObjectTypeInterface)
                        write_stub_cast_factory(from_host, lib, *obj, stub, id++);
                }
                write_stub_cast_factory(from_host, lib, m_ob, stub, 0);
            }            

            proxy("#ifndef _IN_ENCLAVE");
            proxy("//the class that encapsulates an environment or zone");
            proxy("//only host code can use this class directly other enclaves *may* have access to the i_zone derived "
                  "interface");
            proxy("class {} : public i_marshaller, public i_{}_proxy", m_ob.name, m_ob.name);
            proxy("{{");
            proxy("uint64_t eid_ = 0;");
            proxy("zone_config config = {{}};");
            proxy("std::string filename_;");

            proxy("public:");

            proxy("{0}(std::string filename) : i_{0}_proxy(nullptr), filename_(filename){{}}", m_ob.name);
            proxy("~{}()", m_ob.name, m_ob.name);
            proxy("{{");
            proxy("enclave_marshal_test_destroy(eid_);");
            proxy("sgx_destroy_enclave(eid_);");
            proxy("}}");
            proxy("error_code load()", m_ob.name);
            proxy("{{");
            proxy("sgx_launch_token_t token = {{ 0 }};");
            proxy("int updated = 0;");
            proxy("sgx_status_t status = sgx_create_enclavea(filename_.data(), 1, &token, &updated, &eid_, NULL);");
            proxy("if(status)");
            proxy("  return -1;");
            proxy("error_code err_code = 0;");
            proxy("enclave_marshal_test_init(eid_, &err_code, &config);");
            proxy("return err_code;");
            proxy("}}");
            proxy("");

            proxy("error_code send(uint64_t object_id, uint64_t interface_id, uint64_t method_id, size_t in_size_, "
                  "const char* in_buf_, size_t out_size_, char* out_buf_)");
            proxy("{{");
            proxy("error_code err_code = 0;");
            proxy("sgx_status_t status = ::call(eid_, &err_code, object_id, interface_id, method_id, in_size_, "
                  "in_buf_, out_size_, out_buf_);");
            proxy("if(status)");
            proxy("  err_code = -1;");
            proxy("return err_code;");
            proxy("}}");
            proxy("error_code try_cast(uint64_t zone_id_, uint64_t object_id, uint64_t interface_id)");
            proxy("{{");
            proxy("error_code err_code = 0;");
            proxy("sgx_status_t status = ::try_cast(eid_, &err_code, zone_id_, object_id, interface_id);");
            proxy("if(status)");
            proxy("  err_code = -1;");
            proxy("return err_code;");
            proxy("}}");

            proxy("}};");
            proxy("#endif");
        };

        void write_library_proxy_factories(bool from_host, const Library& lib, const ClassObject& m_ob, writer& header,
                                           writer& proxy, writer& stub, const std::vector<std::string>& namespaces)
        {
            std::string ns;

            for (auto& name : namespaces)
            {
                ns += name + "::";
            }
            {
                int id = 1;
                for (auto& name : m_ob.m_ownedClasses)
                {
                    const ClassObject* obj = nullptr;
                    if (!lib.FindClassObject(name, obj))
                    {
                        continue;
                    }
                    if (obj->type == ObjectTypeInterface)
                    {

                        proxy("template<> void object_proxy::create_interface_proxy(rpc_cpp::shared_ptr<{}{}>& "
                              "inface)",
                              ns, obj->name);
                        proxy("{{");
                        proxy("    inface = rpc_cpp::shared_ptr<{1}{0}>(new {1}{0}_proxy(shared_from_this()));",
                              obj->name, ns);
                        proxy("}}");
                        proxy("");
                    }
                }
            }
        }

        // entry point
        void write_files(bool from_host, const Library& lib, std::ostream& hos, std::ostream& pos, std::ostream& sos,
                         const std::vector<std::string>& namespaces, const std::string& header_filename)
        {
            writer header(hos);
            writer proxy(pos);
            writer stub(sos);

            header("#pragma once");
            header("");
            header("#include <marshaller/marshaller.h>");
            header("#include <memory>");
            header("#include <vector>");
            header("#include <map>");
            header("#include <string>");
            header("");

            proxy("#include <yas/mem_streams.hpp>");
            proxy("#include <yas/binary_iarchive.hpp>");
            proxy("#include <yas/binary_oarchive.hpp>");
            proxy("#include <yas/std_types.hpp>");
            proxy("#include \"{}\"", header_filename);
            proxy("");

            stub("#include <yas/mem_streams.hpp>");
            stub("#include <yas/binary_iarchive.hpp>");
            stub("#include <yas/binary_oarchive.hpp>");
            proxy("#include <yas/std_types.hpp>");
            stub("#include \"{}\"", header_filename);
            stub("");

            for (auto& ns : namespaces)
            {
                header("namespace {}", ns);
                header("{{");
                proxy("namespace {}", ns);
                proxy("{{");
                stub("namespace {}", ns);
                stub("{{");
            }

            for (auto& name : lib.m_ownedClasses)
            {
                const ClassObject* obj = nullptr;
                if (!lib.FindClassObject(name, obj))
                {
                    continue;
                }
                if (obj->type == ObjectStruct)
                    write_struct(*obj, header);
            }
            header("");

            for (auto& name : lib.m_ownedClasses)
            {
                const ClassObject* obj = nullptr;
                if (!lib.FindClassObject(name, obj))
                {
                    continue;
                }
                if (obj->type == ObjectLibrary)
                    write_library(from_host, lib, *obj, header, proxy, stub);
            }

            for (auto& ns : namespaces)
            {
                header("}}");
                proxy("}}");
                stub("}}");
            }

            for (auto& name : lib.m_ownedClasses)
            {
                const ClassObject* obj = nullptr;
                if (!lib.FindClassObject(name, obj))
                {
                    continue;
                }
                if (obj->type == ObjectLibrary)
                    write_library_proxy_factories(from_host, lib, *obj, header, proxy, stub, namespaces);
            }
        }
    }
}