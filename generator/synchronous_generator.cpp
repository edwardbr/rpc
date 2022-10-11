#include <type_traits>
#include <algorithm>
#include <tuple>
#include <type_traits>
#include "coreclasses.h"
#include "cpp_parser.h"
#include <filesystem>

#include "writer.h"

#include "synchronous_generator.h"

namespace enclave_marshaller
{
    namespace synchronous_generator
    {
        enum print_type
        {
            PROXY_MARSHALL_IN,
            PROXY_OUT_DECLARATION,
            PROXY_MARSHALL_OUT,
            PROXY_VALUE_RETURN,

            STUB_DEMARSHALL_DECLARATION,
            STUB_MARSHALL_IN,
            STUB_PARAM_WRAP,
            STUB_PARAM_CAST,
            STUB_ADD_REF_OUT_PREDECLARE,
            STUB_ADD_REF_OUT,
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
                INTERFACE,
                INTERFACE_REFERENCE
            };

            template<param_type type>
            std::string render(print_type option, bool from_host, const class_entity& lib, const std::string& name,
                               bool is_in, bool is_out, bool is_const, const std::string& object_type,
                               uint64_t& count) const
            {
                assert(false);
            }
        };

        template<>
        std::string renderer::render<renderer::BY_VALUE>(print_type option, bool from_host, const class_entity& lib,
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
                return fmt::format("  ,(\"_{}\", {}_)", count, name);
            case STUB_PARAM_CAST:
                return fmt::format("{}_", name);
            case STUB_MARSHALL_OUT:
                return fmt::format("  ,(\"_{}\", {}_)", count, name);
            default:
                return "";
            }
        };

        template<>
        std::string renderer::render<renderer::REFERANCE>(print_type option, bool from_host, const class_entity& lib,
                                                          const std::string& name, bool is_in, bool is_out,
                                                          bool is_const, const std::string& object_type,
                                                          uint64_t& count) const
        {
            if (is_out)
            {
                throw std::runtime_error("REFERANCE does not support out vals");
            }

            switch (option)
            {
            case PROXY_MARSHALL_IN:
                return fmt::format("  ,(\"_{}\", {})", count, name);
            case PROXY_MARSHALL_OUT:
                return fmt::format("  ,(\"_{}\", {})", count, name);
            case STUB_DEMARSHALL_DECLARATION:
                return fmt::format("{} {}_{{}}", object_type, name);
            case STUB_MARSHALL_IN:
                return fmt::format("  ,(\"_{}\", {}_)", count, name);
            case STUB_PARAM_CAST:
                return fmt::format("{}_", name);
            default:
                return "";
            }
        };

        template<>
        std::string renderer::render<renderer::MOVE>(print_type option, bool from_host, const class_entity& lib,
                                                     const std::string& name, bool is_in, bool is_out, bool is_const,
                                                     const std::string& object_type, uint64_t& count) const
        {
            if (is_out)
            {
                throw std::runtime_error("MOVE does not support out vals");
            }
            if (is_const)
            {
                throw std::runtime_error("MOVE does not support const vals");
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
                return fmt::format("  ,(\"_{}\", {}_)", count, name);
            case STUB_PARAM_CAST:
                return fmt::format("std::move({}_)", name);
            case STUB_MARSHALL_OUT:
                return fmt::format("  ,(\"_{}\", {}_)", count, name);
            default:
                return "";
            }
        };

        template<>
        std::string renderer::render<renderer::POINTER>(print_type option, bool from_host, const class_entity& lib,
                                                        const std::string& name, bool is_in, bool is_out, bool is_const,
                                                        const std::string& object_type, uint64_t& count) const
        {
            if (is_out)
            {
                throw std::runtime_error("POINTER does not support out vals");
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
                return fmt::format("  ,(\"_{}\", {}_)", count, name);
            case STUB_PARAM_CAST:
                return fmt::format("({}*){}_", object_type, name);
            default:
                return "";
            }
        };

        template<>
        std::string renderer::render<renderer::POINTER_REFERENCE>(print_type option, bool from_host,
                                                                  const class_entity& lib, const std::string& name,
                                                                  bool is_in, bool is_out, bool is_const,
                                                                  const std::string& object_type, uint64_t& count) const
        {
            if (is_const && is_out)
            {
                throw std::runtime_error("POINTER_REFERENCE does not support const out vals");
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
        std::string renderer::render<renderer::POINTER_POINTER>(print_type option, bool from_host,
                                                                const class_entity& lib, const std::string& name,
                                                                bool is_in, bool is_out, bool is_const,
                                                                const std::string& object_type, uint64_t& count) const
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
        std::string renderer::render<renderer::INTERFACE>(print_type option, bool from_host, const class_entity& lib,
                                                          const std::string& name, bool is_in, bool is_out,
                                                          bool is_const, const std::string& object_type,
                                                          uint64_t& count) const
        {
            if (is_out)
            {
                throw std::runtime_error("INTERFACE does not support out vals");
            }

            switch (option)
            {
            case PROXY_MARSHALL_IN:
            {
                auto ret = fmt::format(
                    ",(\"_{1}\", encapsulate_outbound_interfaces({0}, false))",
                    name, count);
                count++;
                return ret;
            }
            case PROXY_MARSHALL_OUT:
                return fmt::format("  ,(\"_{}\", {}_)", count, name);
            case STUB_DEMARSHALL_DECLARATION:
                return fmt::format(R"__(rpc::encapsulated_interface {0}_object_;
                    uint64_t {0}_zone_ = 0)__",
                                   name);
            case STUB_MARSHALL_IN:
            {
                auto ret = fmt::format("  ,(\"_{1}\", {0}_object_)", name, count);
                count++;
                return ret;
            }
            case STUB_PARAM_WRAP:
                return fmt::format(R"__(
                {0} {1};
				if(ret == rpc::error::OK() && {1}_object_.zone_id && {1}_object_.object_id)
                {{
                    auto {1}_service_proxy_ = target_stub_.lock()->get_zone().get_zone_proxy({1}_object_.zone_id);
                    if({1}_service_proxy_)
                        {1}_service_proxy_->create_proxy({1}_object_, {1});
                    else
                        ret = rpc::error::ZONE_NOT_FOUND();
                }}
)__",
                                   object_type, name);
            case STUB_PARAM_CAST:
                return fmt::format("{}", name);
            case STUB_MARSHALL_OUT:
                return fmt::format("  ,(\"_{}\", (uint64_t){})", count, name);
            case PROXY_VALUE_RETURN:
            case PROXY_OUT_DECLARATION:
                return fmt::format("  rpc::encapsulated_interface {}_;", name);
            default:
                return "";
            }
        };

        template<>
        std::string
        renderer::render<renderer::INTERFACE_REFERENCE>(print_type option, bool from_host, const class_entity& lib,
                                                        const std::string& name, bool is_in, bool is_out, bool is_const,
                                                        const std::string& object_type, uint64_t& count) const
        {
            switch (option)
            {
            case PROXY_MARSHALL_IN:
                return fmt::format("  ,(\"_{}\", {})", count, name);
            case PROXY_MARSHALL_OUT:
                return fmt::format("  ,(\"_{}\", {}_)", count, name);
            case STUB_DEMARSHALL_DECLARATION:
                return fmt::format("{} {}", object_type, name);
            case STUB_PARAM_CAST:
                return name;
            case PROXY_VALUE_RETURN:
                return fmt::format("get_object_proxy()->get_service_proxy()->create_proxy({0}_, {0});", name);
            case PROXY_OUT_DECLARATION:
                return fmt::format("rpc::encapsulated_interface {}_;", name);
            case STUB_ADD_REF_OUT_PREDECLARE:
                return fmt::format(
                    "rpc::encapsulated_interface {0}_;", name);
            case STUB_ADD_REF_OUT:
                return fmt::format(
                    "{0}_ = target_stub_.lock()->get_zone().encapsulate_outbound_interfaces({0}, true);", name);
            case STUB_MARSHALL_OUT:
                return fmt::format("  ,(\"_{}\", {}_)", count, name);
            default:
                return "";
            }
        };

        std::string get_encapsulated_shared_ptr_type(const std::string& type_name)
        {
            const std::string template_pattern = "rpc::shared_ptr<";
            auto pos = type_name.find(template_pattern);
            if (pos != std::string::npos)
            {
                pos += template_pattern.length();
                while (type_name[pos] == ' ' || type_name[pos] == '\n' || type_name[pos] == '\r'
                       || type_name[pos] == '\t')
                    pos++;
                auto rpos = type_name.rfind(">");
                if (rpos == std::string::npos)
                {
                    std::cerr << fmt::format("template parameter is malformed {}", type_name);
                    throw fmt::format("template parameter is malformed {}", type_name);
                }
                while (type_name[rpos] == ' ' || type_name[rpos] == '\n' || type_name[rpos] == '\r'
                       || type_name[rpos] == '\t')
                    rpos++;
                return type_name.substr(pos, rpos - pos);
            }
            return type_name;
        }

        bool is_in_call(print_type option, bool from_host, const class_entity& lib, const std::string& name,
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
            strip_reference_modifiers(type_name, referenceModifiers);

            std::string encapsulated_type = get_encapsulated_shared_ptr_type(type_name);

            bool is_interface = false;
            std::shared_ptr<class_entity> obj;
            if (lib.find_class(encapsulated_type, obj))
            {
                if (obj->get_type() == entity_type::INTERFACE)
                {
                    is_interface = true;
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
                        throw std::runtime_error("passing data by reference from a non host zone is not allowed");
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
                if (referenceModifiers.empty() || (referenceModifiers == "&" && (is_const || !out)))
                {
                    output = renderer().render<renderer::INTERFACE>(option, from_host, lib, name, in, out, is_const,
                                                                    type_name, count);
                }
                else if (referenceModifiers == "&")
                {
                    output = renderer().render<renderer::INTERFACE_REFERENCE>(option, from_host, lib, name, in, out,
                                                                              is_const, type_name, count);
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

        bool is_out_call(print_type option, bool from_host, const class_entity& lib, const std::string& name,
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
            strip_reference_modifiers(type_name, referenceModifiers);

            std::string encapsulated_type = get_encapsulated_shared_ptr_type(type_name);

            bool is_interface = false;
            std::shared_ptr<class_entity> obj;
            if (lib.find_class(encapsulated_type, obj))
            {
                if (obj->get_type() == entity_type::INTERFACE)
                {
                    is_interface = true;
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
                    output = renderer().render<renderer::BY_VALUE>(option, from_host, lib, name, in, out, is_const,
                                                                    type_name, count);
                }
                else if (referenceModifiers == "&&")
                {
                    throw std::runtime_error("out call rvalue references is not possible");
                }
                else if (referenceModifiers == "*")
                {
                    throw std::runtime_error("passing [out] by_pointer data by * will not work use a ** or *&");
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
                if (referenceModifiers == "&")
                {
                    output = renderer().render<renderer::INTERFACE_REFERENCE>(option, from_host, lib, name, in, out,
                                                                              is_const, type_name, count);
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

        void write_interface(bool from_host, const class_entity& m_ob, writer& header, writer& proxy, writer& stub,
                             size_t id)
        {
            auto interface_name = std::string(m_ob.get_type() == entity_type::LIBRARY ? "i_" : "") + m_ob.get_name();

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
            header("class {}{} : public rpc::casting_interface", interface_name, base_class_declaration);
            header("{{");
            header("public:");
            header("static constexpr uint64_t id = {}ull;", id);
            header("virtual ~{}() = default;", interface_name);

            proxy("class {0}_proxy : public rpc::proxy_impl<{0}>", interface_name);
            proxy("{{");
            proxy("{}_proxy(rpc::shared_ptr<rpc::object_proxy> object_proxy) : ", interface_name);
            proxy("  rpc::proxy_impl<{}>(object_proxy)", interface_name);
            proxy("  {{");
            proxy("     if(auto* telemetry_service = "
                  "get_object_proxy()->get_service_proxy()->get_telemetry_service();telemetry_service)");
            proxy("     {{");
            proxy("     telemetry_service->on_interface_proxy_creation(\"{0}_proxy\", "
                  "get_object_proxy()->get_service_proxy()->get_operating_zone_id(), "
                  "get_object_proxy()->get_service_proxy()->get_zone_id(), get_object_proxy()->get_object_id(), "
                  "{0}_proxy::id);",
                  interface_name);
            proxy("     }}");
            proxy("  }}");
            proxy("mutable rpc::weak_ptr<{}_proxy> weak_this_;", interface_name);
            proxy("public:");
            proxy("");
            proxy("virtual ~{}_proxy()", interface_name);
            proxy("  {{");
            proxy("     if(auto* telemetry_service = "
                  "get_object_proxy()->get_service_proxy()->get_telemetry_service();telemetry_service)");
            proxy("     {{");
            proxy("     telemetry_service->on_interface_proxy_deletion(\"{0}_proxy\", "
                  "get_object_proxy()->get_service_proxy()->get_operating_zone_id(), "
                  "get_object_proxy()->get_service_proxy()->get_zone_id(), get_object_proxy()->get_object_id(), "
                  "{0}_proxy::id);",
                  interface_name);
            proxy("     }}");
            proxy("  }}");
            proxy("[[nodiscard]] static rpc::shared_ptr<{}> create(const rpc::shared_ptr<rpc::object_proxy>& "
                  "object_proxy)",
                  interface_name);
            proxy("{{");
            proxy("auto ret = rpc::shared_ptr<{0}_proxy>(new {0}_proxy(object_proxy));", interface_name);
            proxy("ret->weak_this_ = ret;", interface_name);
            proxy("return rpc::static_pointer_cast<{}>(ret);", interface_name);
            proxy("}}");
            proxy("rpc::shared_ptr<{0}_proxy> shared_from_this(){{return "
                  "rpc::shared_ptr<{0}_proxy>(weak_this_);}}",
                  interface_name);
            proxy("");

            stub("int {0}_stub::call(uint64_t method_id, size_t in_size_, const char* in_buf_, std::vector<char>& "
                 "out_buf_)", interface_name);
            stub("{{");

            bool has_methods = false;
            for (auto& function : m_ob.get_functions())
            {
                if (function.get_type() != FunctionTypeMethod)
                    continue;
                has_methods = true;
            }

            if (has_methods)
            {
                stub("switch(method_id)");
                stub("{{");

                int function_count = 1;
                for (auto& function : m_ob.get_functions())
                {
                    if (function.get_type() != FunctionTypeMethod)
                        continue;

                    stub("case {}:", function_count);
                    stub("{{");

                    header.print_tabs();
                    proxy.print_tabs();
                    header.raw("virtual {} {}(", function.get_return_type(), function.get_name());
                    // proxy.raw("virtual {} {}_proxy::{} (", function.get_return_type(), interface_name,
                    //           function.get_name());
                    proxy.raw("virtual {} {}(", function.get_return_type(), function.get_name());
                    bool has_parameter = false;
                    for (auto& parameter : function.get_parameters())
                    {
                        if (has_parameter)
                        {
                            header.raw(", ");
                            proxy.raw(", ");
                        }
                        has_parameter = true;
                        std::string modifier;
                        for (auto& item : parameter.get_attributes())
                        {
                            if (item == "const")
                                modifier = "const " + modifier;
                        }
                        header.raw("{}{} {}", modifier, parameter.get_type(), parameter.get_name());
                        proxy.raw("{}{} {}", modifier, parameter.get_type(), parameter.get_name());
                    }
                    header.raw(") = 0;\n");
                    proxy.raw(") override\n");
                    proxy("{{");

                    bool has_inparams = false;

                    proxy("if(auto* telemetry_service = "
                          "get_object_proxy()->get_service_proxy()->get_telemetry_service();telemetry_service)");
                    proxy("{{");
                    proxy("telemetry_service->on_interface_proxy_send(\"{0}_proxy\", "
                          "get_object_proxy()->get_service_proxy()->get_operating_zone_id(), "
                          "get_object_proxy()->get_service_proxy()->get_zone_id(), "
                          "get_object_proxy()->get_object_id(), {0}_proxy::id, {1});",
                          interface_name, function_count);
                    proxy("}}");

                    {
                        stub("//STUB_DEMARSHALL_DECLARATION");
                        stub("int ret = rpc::error::OK();");
                        uint64_t count = 1;
                        for (auto& parameter : function.get_parameters())
                        {
                            bool is_interface = false;
                            std::string output;
                            if (is_in_call(STUB_DEMARSHALL_DECLARATION, from_host, m_ob, parameter.get_name(),
                                           parameter.get_type(), parameter.get_attributes(), count, output))
                                has_inparams = true;
                            else
                                is_out_call(STUB_DEMARSHALL_DECLARATION, from_host, m_ob, parameter.get_name(),
                                            parameter.get_type(), parameter.get_attributes(), count, output);
                            stub("{};", output);
                        }
                    }

                    if (has_inparams)
                    {
                        proxy("//PROXY_MARSHALL_IN");
                        proxy("const auto in_ = yas::save<yas::mem|yas::binary|yas::no_header>(YAS_OBJECT_NVP(");
                        proxy("  \"in\"");

                        stub("//STUB_MARSHALL_IN");
                        stub("yas::intrusive_buffer in(in_buf_, in_size_);");
                        stub("yas::load<yas::mem|yas::binary|yas::no_header>(in, YAS_OBJECT_NVP(");
                        stub("  \"in\"");

                        uint64_t count = 1;
                        for (auto& parameter : function.get_parameters())
                        {
                            std::string output;
                            {
                                if (!is_in_call(PROXY_MARSHALL_IN, from_host, m_ob, parameter.get_name(),
                                                parameter.get_type(), parameter.get_attributes(), count, output))
                                    continue;

                                proxy(output);
                            }
                            count++;
                        }

                        count = 1;
                        for (auto& parameter : function.get_parameters())
                        {
                            std::string output;
                            {
                                if (!is_in_call(STUB_MARSHALL_IN, from_host, m_ob, parameter.get_name(),
                                                parameter.get_type(), parameter.get_attributes(), count, output))
                                    continue;

                                stub(output);
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

                    proxy("std::vector<char> out_buf_(24); //max size using short string optimisation");
                    proxy("{} ret = get_object_proxy()->send({}::id, {}, in_.size, in_.data.get(), out_buf_);",
                          function.get_return_type(), interface_name, function_count);
                    proxy("if(ret != rpc::error::OK())");
                    proxy("{{");
                    proxy("return ret;");
                    proxy("}}");

                    stub("//STUB_PARAM_WRAP");

                    {
                        uint64_t count = 1;
                        for (auto& parameter : function.get_parameters())
                        {
                            std::string output;
                            if (!is_in_call(STUB_PARAM_WRAP, from_host, m_ob, parameter.get_name(),
                                            parameter.get_type(), parameter.get_attributes(), count, output))
                                is_out_call(STUB_PARAM_WRAP, from_host, m_ob, parameter.get_name(),
                                            parameter.get_type(), parameter.get_attributes(), count, output);
                            stub.raw("{}", output);
                        }
                    }

                    stub("//STUB_PARAM_CAST");
                    stub("if(ret == rpc::error::OK())");
                    stub("{{");
                    stub.print_tabs();
                    stub.raw("ret = target_->{}(", function.get_name());

                    {
                        bool has_param = false;
                        uint64_t count = 1;
                        for (auto& parameter : function.get_parameters())
                        {
                            std::string output;
                            if (!is_in_call(STUB_PARAM_CAST, from_host, m_ob, parameter.get_name(),
                                            parameter.get_type(), parameter.get_attributes(), count, output))
                                is_out_call(STUB_PARAM_CAST, from_host, m_ob, parameter.get_name(),
                                            parameter.get_type(), parameter.get_attributes(), count, output);
                            if (has_param)
                            {
                                stub.raw(",");
                            }
                            has_param = true;
                            stub.raw("{}", output);
                        }
                    }
                    stub.raw(");\n");
                    stub("}}");

                    {
                        uint64_t count = 1;
                        proxy("//PROXY_OUT_DECLARATION");
                        for (auto& parameter : function.get_parameters())
                        {
                            count++;
                            std::string output;
                            if (is_in_call(PROXY_OUT_DECLARATION, from_host, m_ob, parameter.get_name(),
                                           parameter.get_type(), parameter.get_attributes(), count, output))
                                continue;
                            if (!is_out_call(PROXY_OUT_DECLARATION, from_host, m_ob, parameter.get_name(),
                                             parameter.get_type(), parameter.get_attributes(), count, output))
                                continue;

                            proxy(output);
                        }
                    }
                    {
                        stub("//STUB_ADD_REF_OUT_PREDECLARE");
                        uint64_t count = 1;
                        for (auto& parameter : function.get_parameters())
                        {
                            count++;
                            std::string output;

                            if (!is_out_call(STUB_ADD_REF_OUT_PREDECLARE, from_host, m_ob, parameter.get_name(),
                                             parameter.get_type(), parameter.get_attributes(), count, output))
                                continue;

                            stub(output);
                        }                        

                        stub("//STUB_ADD_REF_OUT");
                        stub("if(ret == rpc::error::OK())");
                        stub("{{");      
        
                        count = 1;
                        for (auto& parameter : function.get_parameters())
                        {
                            count++;
                            std::string output;

                            if (!is_out_call(STUB_ADD_REF_OUT, from_host, m_ob, parameter.get_name(),
                                             parameter.get_type(), parameter.get_attributes(), count, output))
                                continue;

                            stub(output);
                        }
                        stub("}}");                        
                    }
                    {
                        uint64_t count = 1;
                        proxy("//PROXY_MARSHALL_OUT");
                        proxy("yas::load<yas::mem|yas::binary|yas::no_header>(yas::intrusive_buffer{{out_buf_.data(), "
                              "out_buf_.size()}}, YAS_OBJECT_NVP(");
                        proxy("  \"out\"");
                        proxy("  ,(\"_{}\", ret)", count);

                        stub("//STUB_MARSHALL_OUT");
                        stub("const auto yas_mapping_ = YAS_OBJECT_NVP(");
                        stub("  \"out\"");
                        stub("  ,(\"_{}\", ret)", count);

                        for (auto& parameter : function.get_parameters())
                        {
                            count++;
                            std::string output;
                            if (!is_out_call(PROXY_MARSHALL_OUT, from_host, m_ob, parameter.get_name(),
                                             parameter.get_type(), parameter.get_attributes(), count, output))
                                continue;
                            proxy(output);

                            if (!is_out_call(STUB_MARSHALL_OUT, from_host, m_ob, parameter.get_name(),
                                             parameter.get_type(), parameter.get_attributes(), count, output))
                                continue;

                            stub(output);
                        }
                    }
                    proxy("  ));");

                    stub("  );");

                    stub("yas::count_ostream counter_;");
                    stub("yas::binary_oarchive<yas::count_ostream, yas::mem|yas::binary|yas::no_header> oa(counter_);");
                    stub("oa(yas_mapping_);");
                    stub("out_buf_.resize(counter_.total_size);");
                    stub("yas::mem_ostream writer_(out_buf_.data(), counter_.total_size);");
                    stub("yas::save<yas::mem|yas::binary|yas::no_header>(writer_, yas_mapping_);");
                    stub("return ret;");

                    proxy("//PROXY_VALUE_RETURN");
                    {
                        uint64_t count = 1;
                        for (auto& parameter : function.get_parameters())
                        {
                            count++;
                            std::string output;
                            if (is_in_call(PROXY_VALUE_RETURN, from_host, m_ob, parameter.get_name(),
                                           parameter.get_type(), parameter.get_attributes(), count, output))
                                continue;
                            if (!is_out_call(PROXY_VALUE_RETURN, from_host, m_ob, parameter.get_name(),
                                             parameter.get_type(), parameter.get_attributes(), count, output))
                                continue;

                            proxy(output);
                        }
                    }

                    proxy("return ret;");
                    proxy("}}");
                    proxy("");

                    function_count++;
                    stub("}}");
                    stub("break;");
                }

                stub("default:");
                stub("return rpc::error::INVALID_METHOD_ID();");
                stub("}};");
            }

            header("}};");
            header("");
            proxy("}};");
            proxy("");

            stub("return rpc::error::INVALID_METHOD_ID();");
            stub("}}");
            stub("");
        };

        void write_stub_factory(const class_entity& lib, const class_entity& m_ob, writer& stub, std::set<std::string>& done)
        {
            auto interface_name = std::string(m_ob.get_type() == entity_type::LIBRARY ? "i_" : "") + m_ob.get_name();
            auto owner = m_ob.get_owner();
            std::string ns = interface_name;
            while(!owner->get_name().empty())
            {
                ns = owner->get_name() + "::" + ns;
                owner = owner->get_owner();
            }
            if(done.find(ns) != done.end())
                return;
            done.insert(ns);

            stub("if(interface_id == {}::id)", ns);
            stub("{{");
            stub("auto* tmp = static_cast<{0}*>(original->get_target()->query_interface({0}::id));", ns);
            stub("if(tmp != nullptr)");
            stub("{{");
            stub("rpc::shared_ptr<{}> tmp_ptr(original->get_target(), tmp);", ns);
            stub("new_stub = rpc::static_pointer_cast<rpc::i_interface_stub>({0}_stub::create(tmp_ptr, "
                 "original->get_object_stub()));",
                 ns);
            stub("return rpc::error::OK();");
            stub("}}");
            stub("return rpc::error::INVALID_CAST();");

            stub("}}");
        }

        void write_stub_cast_factory(const class_entity& lib, const class_entity& m_ob, writer& stub)
        {
            auto interface_name = std::string(m_ob.get_type() == entity_type::LIBRARY ? "i_" : "") + m_ob.get_name();
            stub("int {}_stub::cast(uint64_t interface_id, rpc::shared_ptr<rpc::i_interface_stub>& new_stub)",
                 interface_name);
            stub("{{");
            stub("int ret = stub_factory(interface_id, shared_from_this(), new_stub);");
            stub("return ret;");
            stub("}}");
        }

        void write_interface_forward_declaration(const class_entity& m_ob, writer& header, writer& proxy, writer& stub)
        {
            header("class {};", m_ob.get_name());
            proxy("class {}_proxy;", m_ob.get_name());

            auto interface_name = std::string(m_ob.get_type() == entity_type::LIBRARY ? "i_" : "") + m_ob.get_name();

            stub("class {0}_stub : public rpc::i_interface_stub", interface_name);
            stub("{{");
            stub("rpc::shared_ptr<{}> target_;", interface_name);
            stub("rpc::weak_ptr<rpc::object_stub> target_stub_;", interface_name);
            stub("");
            stub("{0}_stub(const rpc::shared_ptr<{0}>& target, rpc::weak_ptr<rpc::object_stub> target_stub) : ",
                 interface_name);
            stub("  target_(target),", interface_name);
            stub("  target_stub_(target_stub)");
            stub("  {{}}");
            stub("mutable rpc::weak_ptr<{}_stub> weak_this_;", interface_name);
            stub("");
            stub("public:");
            stub("static rpc::shared_ptr<{0}_stub> create(const rpc::shared_ptr<{0}>& target, "
                 "rpc::weak_ptr<rpc::object_stub> target_stub)",
                 interface_name);
            stub("{{");
            stub("auto ret = rpc::shared_ptr<{0}_stub>(new {0}_stub(target, target_stub));", interface_name);
            stub("ret->weak_this_ = ret;", interface_name);
            stub("return ret;", interface_name);
            stub("}}");
            stub("rpc::shared_ptr<{0}_stub> shared_from_this(){{return rpc::shared_ptr<{0}_stub>(weak_this_);}}",
                 interface_name);
            stub("");
            stub("uint64_t get_interface_id() const override {{ return {}::id; }};", interface_name);
            stub("rpc::shared_ptr<{}> get_target() const {{ return target_; }};", interface_name);
            stub("virtual rpc::shared_ptr<rpc::casting_interface> get_castable_interface() const override {{ return rpc::static_pointer_cast<rpc::casting_interface>(target_); }}", interface_name);

            stub("rpc::weak_ptr<rpc::object_stub> get_object_stub() const override {{ return target_stub_;}}");
            stub("void* get_pointer() const override {{ return target_.get();}}");
            stub("int call(uint64_t method_id, size_t in_size_, const char* in_buf_, std::vector<char>& "
                 "out_buf_) override;");
            stub("int cast(uint64_t interface_id, rpc::shared_ptr<rpc::i_interface_stub>& new_stub) override;");
            stub("}};");
            stub("");            
        }

        void write_struct_forward_declaration(const class_entity& m_ob, writer& header)
        {
            if (!m_ob.get_template_params().empty())
            {
                header.print_tabs();
                header.raw("template<");
                bool first_pass = true;
                for (const auto& param : m_ob.get_template_params())
                {
                    if (!first_pass)
                        header.raw(", ");
                    first_pass = false;
                    header.raw("{} {}", param.type, param.name);
                }
                header.raw(">\n");
            }
            header("struct {};", m_ob.get_name());
        }

        void write_enum_forward_declaration(const class_entity& m_ob, writer& header)
        {
            header("enum class {}", m_ob.get_name());
            header("{{");
            auto enum_vals = m_ob.get_functions();
            for (auto& enum_val : enum_vals)
            {
                header("{},", enum_val.get_name());
            }
            header("}};");
        }

        void write_typedef_forward_declaration(const class_entity& m_ob, writer& header)
        {
            header("using {} = {};", m_ob.get_name(), m_ob.get_alias_name());
        }

        void write_struct(const class_entity& m_ob, writer& header)
        {
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
            if (!m_ob.get_template_params().empty())
            {
                header.print_tabs();
                header.raw("template<");
                bool first_pass = true;
                for (const auto& param : m_ob.get_template_params())
                {
                    if (!first_pass)
                        header.raw(", ");
                    first_pass = false;
                    header.raw("{} {}", param.type, param.name);
                }
                header.raw(">\n");
            }
            header("struct {}{}", m_ob.get_name(), base_class_declaration);
            header("{{");

            for (auto& field : m_ob.get_functions())
            {
                if (field.get_type() != FunctionTypeVariable)
                    continue;

                header.print_tabs();
                header.raw("{} {}", field.get_return_type(), field.get_name());
                if (field.get_array_size())
                    header.raw("[{}]", field.get_array_size());
                if (!field.get_default_value().empty())
                {
                    header.raw(" = {}::{};\n", field.get_return_type(), field.get_default_value());
                }
                else
                {
                    header.raw("{{}};\n");
                }
            }

            header("");
            header("// one member-function for save/load");
            header("template<typename Ar>");
            header("void serialize(Ar &ar)");
            header("{{");
            header("ar & YAS_OBJECT_NVP(\"{}\"", m_ob.get_name());

            for (auto& field : m_ob.get_functions())
            {
                header("  ,(\"{0}\", {0})", field.get_name());
            }
            header(");");

            header("}}");

            header("}};");
        };

        void write_encapsulate_outbound_interfaces(const class_entity& lib, const class_entity& obj,
                                                   writer& header, writer& proxy, writer& stub,
                                                   const std::vector<std::string>& namespaces)
        {
            auto interface_name = std::string(obj.get_type() == entity_type::LIBRARY ? "i_" : "") + obj.get_name();
            std::string ns;

            for (auto& name : namespaces)
            {
                ns += name + "::";
            }

            auto owner = obj.get_owner();
            while (owner && !owner->get_name().empty())
            {
                ns += owner->get_name() + "::";
                owner = owner->get_owner();
            }

            int id = 1;
            header("template<> rpc::encapsulated_interface "
                   "rpc::service::encapsulate_outbound_interfaces(const rpc::shared_ptr<{}{}>& "
                   "iface, bool add_ref);",
                   ns, interface_name);
        }

        void write_library_proxy_factory(writer& proxy, writer& stub, const class_entity& obj,
                                         const std::vector<std::string>& namespaces)
        {
            auto interface_name = std::string(obj.get_type() == entity_type::LIBRARY ? "i_" : "") + obj.get_name();
            std::string ns;

            for (auto& name : namespaces)
            {
                ns += name + "::";
            }
            auto owner = obj.get_owner();
            while (owner && !owner->get_name().empty())
            {
                ns += owner->get_name() + "::";
                owner = owner->get_owner();
            }

            proxy("template<> void rpc::object_proxy::create_interface_proxy(rpc::shared_ptr<{}{}>& "
                  "inface)",
                  ns, interface_name);
            proxy("{{");
            proxy("inface = {1}{0}_proxy::create(shared_from_this());", interface_name, ns);
            proxy("}}");
            proxy("");

            stub("template<> rpc::encapsulated_interface rpc::service::encapsulate_outbound_interfaces(const rpc::shared_ptr<{}{}>& "
                 "iface, bool add_ref)",
                 ns, interface_name);
            stub("{{");

            stub("return find_or_create_stub(iface.get(), [&](const rpc::shared_ptr<rpc::object_stub>& stub) -> "
                 "rpc::shared_ptr<rpc::i_interface_stub>{{");
            stub("return rpc::static_pointer_cast<rpc::i_interface_stub>({}{}_stub::create(iface, stub));", ns,
                 interface_name);
            stub("}}, add_ref);");
            stub("}}");
        }

        void write_marshalling_logic_nested(bool from_host, const class_entity& cls, std::string prefix, writer& header,
                                            writer& proxy, writer& stub)
        {
            if (cls.get_type() == entity_type::STRUCT)
                write_struct(cls, header);

            header("");

            std::size_t hash = std::hash<std::string> {}(prefix + "::" + cls.get_name());

            if (cls.get_type() == entity_type::INTERFACE)
                write_interface(from_host, cls, header, proxy, stub, hash);

            if (cls.get_type() == entity_type::LIBRARY)
                write_interface(from_host, cls, header, proxy, stub, hash);
        }

        void write_marshalling_logic(const class_entity& lib, std::string prefix, writer& header,
                                     writer& proxy, writer& stub)
        {
            {
                int id = 1;
                for (auto& cls : lib.get_classes())
                {
                    if (!cls->get_import_lib().empty())
                        continue;
                    if (cls->get_type() == entity_type::INTERFACE)
                        write_stub_cast_factory(lib, *cls, stub);
                }

                for (auto& cls : lib.get_classes())
                {
                    if (!cls->get_import_lib().empty())
                        continue;
                    if (cls->get_type() == entity_type::LIBRARY)
                        write_stub_cast_factory(lib, *cls, stub);
                }
            }
        }

        // entry point
        void write_namespace_predeclaration(const class_entity& lib, writer& header, writer& proxy,
                                            writer& stub)
        {
            for (auto cls : lib.get_classes())
            {
                if (!cls->get_import_lib().empty())
                    continue;
                if (cls->get_type() == entity_type::INTERFACE || cls->get_type() == entity_type::LIBRARY)
                    write_interface_forward_declaration(*cls, header, proxy, stub);
                if (cls->get_type() == entity_type::STRUCT)
                    write_struct_forward_declaration(*cls, header);
                if (cls->get_type() == entity_type::ENUM)
                    write_enum_forward_declaration(*cls, header);
                if (cls->get_type() == entity_type::TYPEDEF)
                    write_typedef_forward_declaration(*cls, header);
            }

            for (auto cls : lib.get_classes())
            {
                if (!cls->get_import_lib().empty())
                    continue;
                if (cls->get_type() == entity_type::NAMESPACE)
                {

                    header("namespace {}", cls->get_name());
                    header("{{");
                    proxy("namespace {}", cls->get_name());
                    proxy("{{");
                    stub("namespace {}", cls->get_name());
                    stub("{{");

                    write_namespace_predeclaration(*cls, header, proxy, stub);

                    header("}}");
                    proxy("}}");
                    stub("}}");
                }
            }
        }

        // entry point
        void write_namespace(bool from_host, const class_entity& lib, std::string prefix, writer& header, writer& proxy,
                             writer& stub)
        {
            for (auto cls : lib.get_classes())
            {
                if (!cls->get_import_lib().empty())
                    continue;
                if (cls->get_type() == entity_type::NAMESPACE)
                {

                    header("namespace {}", cls->get_name());
                    header("{{");
                    proxy("namespace {}", cls->get_name());
                    proxy("{{");
                    stub("namespace {}", cls->get_name());
                    stub("{{");

                    write_namespace(from_host, *cls, prefix + cls->get_name() + "::", header, proxy, stub);

                    header("}}");
                    proxy("}}");
                    stub("}}");
                }
                else
                {
                    write_marshalling_logic_nested(from_host, *cls, prefix, header, proxy, stub);
                }
            }
            write_marshalling_logic(lib, prefix, header, proxy, stub);
        }

        void write_epilog(bool from_host, const class_entity& lib, writer& header, writer& proxy, writer& stub,
                          const std::vector<std::string>& namespaces)
        {
            for (auto cls : lib.get_classes())
            {
                if (!cls->get_import_lib().empty())
                    continue;
                if (cls->get_type() == entity_type::NAMESPACE)
                {

                    write_epilog(from_host, *cls, header, proxy, stub, namespaces);
                }
                else
                {
                    if (cls->get_type() == entity_type::LIBRARY || cls->get_type() == entity_type::INTERFACE)
                        write_encapsulate_outbound_interfaces(lib, *cls, header, proxy, stub, namespaces);

                    if (cls->get_type() == entity_type::LIBRARY || cls->get_type() == entity_type::INTERFACE)
                        write_library_proxy_factory(proxy, stub, *cls, namespaces);
                }
            }
        }

        void write_stub_factory_lookup_items(const class_entity& lib, std::string prefix, writer& header, writer& proxy,
                                writer& stub, std::set<std::string>& done)
        {
            for (auto cls : lib.get_classes())
            {
                if (!cls->get_import_lib().empty())
                    continue;
                if (cls->get_type() == entity_type::NAMESPACE)
                {
                    write_stub_factory_lookup_items(*cls, prefix + cls->get_name() + "::", header, proxy, stub, done);
                }
                else
                {
                    for (auto& cls : lib.get_classes())
                    {
                        if (!cls->get_import_lib().empty())
                            continue;
                        if (cls->get_type() == entity_type::INTERFACE)
                            write_stub_factory(lib, *cls, stub, done);
                    }

                    for (auto& cls : lib.get_classes())
                    {
                        if (!cls->get_import_lib().empty())
                            continue;
                        if (cls->get_type() == entity_type::LIBRARY)
                            write_stub_factory(lib, *cls, stub, done);
                    }
                }
            }
        }
        
        void write_stub_factory_lookup(const class_entity& lib, std::string prefix, writer& header, writer& proxy,
                             writer& stub)
        {
            if (lib.get_owner() == nullptr)
            {
                stub("#ifndef STUB_FACTORY");
                stub("#define STUB_FACTORY");
            }

            

            {
                stub("template<class T>");
                stub("int stub_factory(uint64_t interface_id, rpc::shared_ptr<T> original, "
                     "rpc::shared_ptr<rpc::i_interface_stub>& new_stub)");
                stub("{{");
                stub("if(interface_id == original->get_interface_id())");
                stub("{{");
                stub("new_stub = rpc::static_pointer_cast<rpc::i_interface_stub>(original);");
                stub("return rpc::error::OK();");
                stub("}}");

                std::set<std::string> done;

                write_stub_factory_lookup_items(lib, prefix, header, proxy, stub, done);

                stub("return rpc::error::INVALID_DATA();");
                stub("}}");
            }
            
            if (lib.get_owner() == nullptr)
            {
                stub("#endif");
            }
        }

        // entry point
        void write_files(bool from_host, const class_entity& lib, std::ostream& hos, std::ostream& pos,
                         std::ostream& sos, const std::vector<std::string>& namespaces,
                         const std::string& header_filename, const std::list<std::string>& imports)
        {
            writer header(hos);
            writer proxy(pos);
            writer stub(sos);

            header("#pragma once");
            header("");
            header("#include <memory>");
            header("#include <vector>");
            header("#include <map>");
            header("#include <set>");
            header("#include <string>");
            header("#include <array>");

            header("#include <rpc/marshaller.h>");
            header("#include <rpc/service.h>");
            header("#include <rpc/error_codes.h>");
            header("#include <rpc/casting_interface.h>");
            

            for (const auto& import : imports)
            {
                std::filesystem::path p(import);
                auto import_header = p.root_name() / p.parent_path() / p.stem();
                auto path = import_header.string();
                std::replace(path.begin(), path.end(), '\\', '/');
                header("#include \"{}.h\"", path);
            }

            header("");

            proxy("#pragma once");
            proxy("#include <yas/mem_streams.hpp>");
            proxy("#include <yas/binary_iarchive.hpp>");
            proxy("#include <yas/binary_oarchive.hpp>");
            proxy("#include <yas/std_types.hpp>");
            proxy("#include <yas/count_streams.hpp>");
            proxy("#include <rpc/proxy.h>");
            proxy("#include <rpc/service.h>");
            proxy("#include \"{}\"", header_filename);
            proxy("");

            stub("#pragma once");
            stub("#include <yas/mem_streams.hpp>");
            stub("#include <yas/binary_iarchive.hpp>");
            stub("#include <yas/binary_oarchive.hpp>");
            stub("#include <yas/count_streams.hpp>");
            stub("#include <yas/std_types.hpp>");
            stub("#include <rpc/stub.h>");
            stub("#include <rpc/proxy.h>");
            stub("#include <rpc/service.h>");
            stub("#include \"{}\"", header_filename);
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

                prefix += ns + "::";
            }

            write_namespace_predeclaration(lib, header, proxy, stub);
            
            write_stub_factory_lookup(lib, prefix, header, proxy, stub);

            write_namespace(from_host, lib, prefix, header, proxy, stub);

            for (auto& ns : namespaces)
            {
                header("}}");
                proxy("}}");
                stub("}}");
            }

            write_epilog(from_host, lib, header, proxy, stub, namespaces);
        }
    }
}