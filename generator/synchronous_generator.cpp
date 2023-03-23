#include <type_traits>
#include <algorithm>
#include <tuple>
#include <type_traits>
#include "coreclasses.h"
#include "cpp_parser.h"
#include <filesystem>
#include <sstream>

#include "writer.h"

#include "synchronous_generator.h"

namespace enclave_marshaller
{
    namespace synchronous_generator
    {
        enum print_type
        {
            PROXY_PREPARE_IN,
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
                return fmt::format("  ,(\"{0}\", {0})", name);
            case PROXY_MARSHALL_OUT:
                return fmt::format("  ,(\"{0}\", {0})", name);
            case STUB_DEMARSHALL_DECLARATION:
                return fmt::format("{} {}_", object_type, name);
            case STUB_MARSHALL_IN:
                return fmt::format("  ,(\"{0}\", {0}_)", name);
            case STUB_PARAM_CAST:
                return fmt::format("{}_", name);
            case STUB_MARSHALL_OUT:
                return fmt::format("  ,(\"{0}\", {0}_)", name);
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
                return fmt::format("  ,(\"{0}\", {0})", name);
            case PROXY_MARSHALL_OUT:
                return fmt::format("  ,(\"{0}\", {0})", name);
            case STUB_DEMARSHALL_DECLARATION:
                return fmt::format("{} {}_{{}}", object_type, name);
            case STUB_MARSHALL_IN:
                return fmt::format("  ,(\"{0}\", {0}_)", name);
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
                return fmt::format("  ,(\"{0}\", {0})", name);
            case PROXY_MARSHALL_OUT:
                return fmt::format("  ,(\"{0}\", {0})", name);
            case STUB_DEMARSHALL_DECLARATION:
                return fmt::format("{} {}_", object_type, name);
            case STUB_MARSHALL_IN:
                return fmt::format("  ,(\"{0}\", {0}_)", name);
            case STUB_PARAM_CAST:
                return fmt::format("std::move({}_)", name);
            case STUB_MARSHALL_OUT:
                return fmt::format("  ,(\"{0}\", {0}_)", name);
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
                return fmt::format("  ,(\"{0}\", (uint64_t){0})", name);
            case PROXY_MARSHALL_OUT:
                return fmt::format("  ,(\"_{}\", (uint64_t) {})", count, name);
            case STUB_DEMARSHALL_DECLARATION:
                return fmt::format("uint64_t {}_", name);
            case STUB_MARSHALL_IN:
                return fmt::format("  ,(\"{0}\", {0}_)", name);
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
                return fmt::format("  ,(\"{0}\", {0}_)", name);
            case PROXY_MARSHALL_OUT:
                return fmt::format("  ,(\"{0}\", {0}_)", name);
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
                return fmt::format("  ,(\"{0}\", {0}_)", name);
            case PROXY_MARSHALL_OUT:
                return fmt::format("  ,(\"{0}\", {0}_)", name);
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
            case PROXY_PREPARE_IN:
                return fmt::format("rpc::shared_ptr<rpc::object_stub> {}_stub_;", name);
            case PROXY_MARSHALL_IN:
            {
                auto ret = fmt::format(
                    ",(\"_{1}\", proxy_bind_in_param({0}, {0}_stub_))",
                    name, count);
                count++;
                return ret;
            }
            case PROXY_MARSHALL_OUT:
                return fmt::format("  ,(\"{0}\", {0}_)", name);

            case PROXY_CLEAN_IN:
                return fmt::format("if({0}_stub_) {0}_stub_->release_from_service();", name);

            case STUB_DEMARSHALL_DECLARATION:
                return fmt::format(R"__(rpc::interface_descriptor {0}_object_;
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
				if(__rpc_ret == rpc::error::OK() && {1}_object_.destination_zone_id.is_set() && {1}_object_.object_id.is_set())
                {{
                    auto& zone_ = target_stub_.lock()->get_zone();
                    __rpc_ret = rpc::stub_bind_in_param(zone_, caller_channel_zone_id, caller_zone_id, {1}_object_, {1});
                }}
)__",
                                   object_type, name);
            case STUB_PARAM_CAST:
                return fmt::format("{}", name);
            case STUB_MARSHALL_OUT:
                return fmt::format("  ,(\"{0}\", (uint64_t){0})", name);
            case PROXY_VALUE_RETURN:
            case PROXY_OUT_DECLARATION:
                return fmt::format("  rpc::interface_descriptor {}_;", name);
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
            case PROXY_PREPARE_IN:
                return fmt::format("rpc::shared_ptr<rpc::object_stub> {}_stub_;", name);
            case PROXY_MARSHALL_IN:
                return fmt::format("  ,(\"{0}\", {0})", name);
            case PROXY_MARSHALL_OUT:
                return fmt::format("  ,(\"{0}\", {0}_)", name);

            case PROXY_CLEAN_IN:
                return fmt::format("if({0}_stub_) {0}_stub_->release_from_service();", name);

            case STUB_DEMARSHALL_DECLARATION:
                return fmt::format("{} {}", object_type, name);
            case STUB_PARAM_CAST:
                return name;
            case PROXY_VALUE_RETURN:
                return fmt::format("rpc::proxy_bind_out_param(__rpc_sp, {0}_, __rpc_sp->get_zone_id().as_caller(), {0});", name);
            case PROXY_OUT_DECLARATION:
                return fmt::format("rpc::interface_descriptor {}_;", name);
            case STUB_ADD_REF_OUT_PREDECLARE:
                return fmt::format(
                    "rpc::interface_descriptor {0}_;", name);
            case STUB_ADD_REF_OUT:
                return fmt::format(
                    "{0}_ = zone_.stub_bind_out_param(caller_channel_zone_id, caller_zone_id, {0});", name);
            case STUB_MARSHALL_OUT:
                return fmt::format("  ,(\"{0}\", {0}_)", name);
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
            proxy("{{");
            proxy("auto __rpc_op = get_object_proxy();");
            proxy("auto __rpc_sp = __rpc_op->get_service_proxy();");
            proxy("   if(auto* telemetry_service = "
                  "__rpc_sp->get_telemetry_service();telemetry_service)");
            proxy("   {{");
            proxy("   telemetry_service->on_interface_proxy_creation(\"{0}_proxy\", "
                  "__rpc_sp->get_zone_id(), "
                  "__rpc_sp->get_destination_zone_id(), __rpc_op->get_object_id(), "
                  "{{{0}_proxy::id}});",
                  interface_name);
            proxy("}}");
            proxy("}}");
            proxy("mutable rpc::weak_ptr<{}_proxy> weak_this_;", interface_name);
            proxy("public:");
            proxy("");
            proxy("virtual ~{}_proxy()", interface_name);
            proxy("{{");
            proxy("auto __rpc_op = get_object_proxy();");
            proxy("auto __rpc_sp = __rpc_op->get_service_proxy();");
            proxy("if(auto* telemetry_service = "
                  "__rpc_sp->get_telemetry_service();telemetry_service)");
            proxy("{{");
            proxy("telemetry_service->on_interface_proxy_deletion(\"{0}_proxy\", "
                  "__rpc_sp->get_zone_id(), "
                  "__rpc_sp->get_destination_zone_id(), __rpc_op->get_object_id(), "
                  "{{{0}_proxy::id}});",
                  interface_name);
            proxy("}}");
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

            stub("int {0}_stub::call(rpc::caller_channel_zone caller_channel_zone_id, rpc::caller_zone caller_zone_id, rpc::method method_id, size_t in_size_, const char* in_buf_, std::vector<char>& "
                 "__rpc_out_buf)", interface_name);
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
                stub("switch(method_id.get_val())");
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

                    proxy("auto __rpc_op = get_object_proxy();");
                    proxy("auto __rpc_sp = __rpc_op->get_service_proxy();");
                    proxy("if(auto* telemetry_service = "
                          "__rpc_sp->get_telemetry_service();telemetry_service)");
                    proxy("{{");
                    proxy("telemetry_service->on_interface_proxy_send(\"{0}_proxy\", "
                          "__rpc_sp->get_zone_id(), "
                          "__rpc_sp->get_destination_zone_id(), "
                          "__rpc_op->get_object_id(), {{{0}_proxy::id}}, {{{1}}});",
                          interface_name, function_count);
                    proxy("}}");

                    {
                        stub("//STUB_DEMARSHALL_DECLARATION");
                        stub("int __rpc_ret = rpc::error::OK();");
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

                    proxy("std::vector<char> __rpc_in_buf;");                    

                    if (has_inparams)
                    {
                        proxy("//PROXY_PREPARE_IN");
                        uint64_t count = 1;
                        for (auto& parameter : function.get_parameters())
                        {
                            std::string output;
                            {
                                if (!is_in_call(PROXY_PREPARE_IN, from_host, m_ob, parameter.get_name(),
                                                parameter.get_type(), parameter.get_attributes(), count, output))
                                    continue;

                                proxy(output);
                            }
                            count++;
                        }
                        
                        proxy("const auto __rpc_yas_mapping = YAS_OBJECT_NVP(");
                        proxy("  \"in\"");
                        proxy("  ,(\"__rpc_version\", rpc::get_version())");

                        stub("//STUB_MARSHALL_IN");
                        stub("uint8_t __rpc_version = 0;");
                        stub("yas::intrusive_buffer in(in_buf_, in_size_);");
                        stub("try");
                        stub("{{");
                        stub("yas::load<yas::mem|yas::binary|yas::no_header>(in, YAS_OBJECT_NVP(");
                        stub("  \"in\"");
                        stub("  ,(\"__rpc_version\", __rpc_version)");
					  

                        count = 1;
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
                        proxy("  );");
                        proxy("yas::count_ostream __rpc_counter;");
                        proxy("yas::binary_oarchive<yas::count_ostream, yas::mem|yas::binary|yas::no_header> __rpc_oa(__rpc_counter);");
                        proxy("__rpc_oa(__rpc_yas_mapping);");
                        proxy("__rpc_in_buf.resize(__rpc_counter.total_size);");
                        proxy("yas::mem_ostream __rpc_writer(__rpc_in_buf.data(), __rpc_counter.total_size);");
                        proxy("yas::save<yas::mem|yas::binary|yas::no_header>(__rpc_writer, __rpc_yas_mapping);");
                        stub("  ));");
                        stub("}}");
                        stub("catch(...)");
                        stub("{{");
                        stub("return rpc::error::STUB_DESERIALISATION_ERROR();");
                        stub("}}");
                        
                        stub("if(__rpc_version != rpc::get_version())");
                        stub("  return rpc::error::INVALID_VERSION();");
                    }

                    proxy("std::vector<char> __rpc_out_buf(24); //max size using short string optimisation");
                    proxy("{} __rpc_ret = __rpc_op->send({{{}::id}}, {{{}}}, __rpc_in_buf.size(), __rpc_in_buf.data(), __rpc_out_buf);",
                          function.get_return_type(), interface_name, function_count);
                    proxy("if(__rpc_ret >= rpc::error::MIN() && __rpc_ret <= rpc::error::MAX())");
                    proxy("{{");
                    proxy("return __rpc_ret;");
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
                    stub("if(__rpc_ret == rpc::error::OK())");
                    stub("{{");
					stub("try");
					stub("{{");

                    stub.print_tabs();
                    stub.raw("__rpc_ret = __rpc_target_->{}(", function.get_name());

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
					stub("catch(...)");
					stub("{{");
					stub("__rpc_ret = rpc::error::EXCEPTION();");
					stub("}}");
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
                        stub("if(__rpc_ret == rpc::error::OK())");
                        stub("{{");    
        
                        count = 1;
                        bool has_preamble = false;
                        for (auto& parameter : function.get_parameters())
                        {
                            count++;
                            std::string output;

                            if (!is_out_call(STUB_ADD_REF_OUT, from_host, m_ob, parameter.get_name(),
                                             parameter.get_type(), parameter.get_attributes(), count, output))
                                continue;

                            if(!has_preamble && !output.empty())
                            {
                                stub("auto& zone_ = target_stub_.lock()->get_zone();");//add a nice option
                                has_preamble = false;
                            }
                            stub(output);
                        }
                        stub("}}");                        
                    }
                    {
                        uint64_t count = 1;
                        proxy("//PROXY_MARSHALL_OUT");
                        proxy("try");
                        proxy("{{");
                        proxy("yas::load<yas::mem|yas::binary|yas::no_header>(yas::intrusive_buffer{{__rpc_out_buf.data(), "
                              "__rpc_out_buf.size()}}, YAS_OBJECT_NVP(");
                        proxy("  \"out\"");
                        proxy("  ,(\"__return_value\", __rpc_ret)");

                        stub("//STUB_MARSHALL_OUT");
                        stub("const auto __rpc_yas_mapping = YAS_OBJECT_NVP(");
                        stub("  \"out\"");
                        stub("  ,(\"__return_value\", __rpc_ret)");

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
                    proxy("}}");
                    proxy("catch(...)");
                    proxy("{{");
                    proxy("return rpc::error::PROXY_DESERIALISATION_ERROR();");
                    proxy("}}");
                        
                    stub("  );");

                    stub("yas::count_ostream __rpc_counter;");
                    stub("yas::binary_oarchive<yas::count_ostream, yas::mem|yas::binary|yas::no_header> __rpc_oa(__rpc_counter);");
                    stub("__rpc_oa(__rpc_yas_mapping);");
                    stub("__rpc_out_buf.resize(__rpc_counter.total_size);");
                    stub("yas::mem_ostream __rpc_writer(__rpc_out_buf.data(), __rpc_counter.total_size);");
                    stub("yas::save<yas::mem|yas::binary|yas::no_header>(__rpc_writer, __rpc_yas_mapping);");
                    stub("return __rpc_ret;");

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
                    proxy("//PROXY_CLEAN_IN");
                    {
                        uint64_t count = 1;
                        for (auto& parameter : function.get_parameters())
                        {
                            std::string output;
                            {
                                if (!is_in_call(PROXY_CLEAN_IN, from_host, m_ob, parameter.get_name(),
                                                parameter.get_type(), parameter.get_attributes(), count, output))
                                    continue;

                                proxy(output);
                            }
                            count++;
                        }
                    }

                    proxy("return __rpc_ret;");
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
            stub("auto* tmp = const_cast<{0}*>(static_cast<const {0}*>(original->get_target()->query_interface({{{0}::id}})));", ns);
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
            stub("int {}_stub::cast(rpc::interface_ordinal interface_id, rpc::shared_ptr<rpc::i_interface_stub>& new_stub)",
                 interface_name);
            stub("{{");
            stub("int __rpc_ret = stub_factory(interface_id, shared_from_this(), new_stub);");
            stub("return __rpc_ret;");
            stub("}}");
        }

        void write_interface_forward_declaration(const class_entity& m_ob, writer& header, writer& proxy, writer& stub)
        {
            header("class {};", m_ob.get_name());
            proxy("class {}_proxy;", m_ob.get_name());

            auto interface_name = std::string(m_ob.get_type() == entity_type::LIBRARY ? "i_" : "") + m_ob.get_name();

            stub("class {0}_stub : public rpc::i_interface_stub", interface_name);
            stub("{{");
            stub("rpc::shared_ptr<{}> __rpc_target_;", interface_name);
            stub("rpc::weak_ptr<rpc::object_stub> target_stub_;", interface_name);
            stub("");
            stub("{0}_stub(const rpc::shared_ptr<{0}>& __rpc_target, rpc::weak_ptr<rpc::object_stub> __rpc_target_stub) : ",
                 interface_name);
            stub("  __rpc_target_(__rpc_target),", interface_name);
            stub("  target_stub_(__rpc_target_stub)");
            stub("  {{}}");
            stub("mutable rpc::weak_ptr<{}_stub> weak_this_;", interface_name);
            stub("");
            stub("public:");
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
            stub("rpc::interface_ordinal get_interface_id() const override {{ return {{{}::id}}; }};", interface_name);
            stub("rpc::shared_ptr<{}> get_target() const {{ return __rpc_target_; }};", interface_name);
            stub("virtual rpc::shared_ptr<rpc::casting_interface> get_castable_interface() const override {{ return rpc::static_pointer_cast<rpc::casting_interface>(__rpc_target_); }}", interface_name);

            stub("rpc::weak_ptr<rpc::object_stub> get_object_stub() const override {{ return target_stub_;}}");
            stub("void* get_pointer() const override {{ return __rpc_target_.get();}}");
            stub("int call(rpc::caller_channel_zone caller_channel_zone_id, rpc::caller_zone caller_zone_id, rpc::method method_id, size_t in_size_, const char* in_buf_, std::vector<char>& "
                 "__rpc_out_buf) override;");
            stub("int cast(rpc::interface_ordinal interface_id, rpc::shared_ptr<rpc::i_interface_stub>& new_stub) override;");
            stub("}};");
            stub("");            
        }

        void write_struct_forward_declaration(const class_entity& m_ob, writer& header)
        {
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
                if (field.get_array_string().size())
                    header.raw("[{}]", field.get_array_string());
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

            std::stringstream sstr;
            std::string obj_type(m_ob.get_name());
            {
                writer tmpl(sstr);
                tmpl.set_count(header.get_count());
                if (m_ob.get_is_template())
                {
                    tmpl.print_tabs();
                    tmpl.raw("template<");
                    if(!m_ob.get_template_params().empty())
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
                        tmpl.raw("{} {}", param.type, param.name);
                        obj_type += param.name;
                    }
                    if(!m_ob.get_template_params().empty())
                        obj_type += ">";
                    tmpl.raw(">\n");
                }
            }
            header.raw(sstr.str());
            header("inline bool operator != (const {0}& lhs, const {0}& rhs)", obj_type);
            header("{{");
            header.print_tabs();
            header.raw("return ");
            bool first_pass = true;
            for (auto& field : m_ob.get_functions())
            {
                header.raw("\n");
                header.print_tabs();
                header.raw("{1}lhs.{0} != rhs.{0}", field.get_name(), first_pass ? "" : "|| ");
                first_pass = false;
            }
            header.raw(";\n");
            header("}}");

            header.raw(sstr.str());
            header("inline bool operator == (const {0}& lhs, const {0}& rhs)", obj_type);
            header("{{");
            header("return !(lhs != rhs);");
            header("}}");
        };

        void build_scoped_name(const class_entity* entity, std::string& name)
        {
            auto* owner = entity->get_owner();
            if(owner && !owner->get_name().empty())
            {
                build_scoped_name(owner, name);
            }
            name += entity->get_name() + "::";
        }

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
            if(owner && !owner->get_name().empty())
            {
                build_scoped_name(owner, ns);
            }

            int id = 1;
            header("template<> rpc::interface_descriptor "
                   "rpc::service::proxy_bind_in_param(const rpc::shared_ptr<{}{}>& "
                   "iface, rpc::shared_ptr<rpc::object_stub>& stub);",
                   ns, interface_name);
            header("template<> rpc::interface_descriptor "
                   "rpc::service::stub_bind_out_param(caller_channel_zone caller_channel_zone_id, caller_zone caller_zone_id, const rpc::shared_ptr<{}{}>& "
                   "iface);",
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
            if (owner && !owner->get_name().empty())
            {
                build_scoped_name(owner, ns);
            }

            proxy("template<> void rpc::object_proxy::create_interface_proxy(rpc::shared_ptr<{}{}>& "
                  "inface)",
                  ns, interface_name);
            proxy("{{");
            proxy("inface = {1}{0}_proxy::create(shared_from_this());", interface_name, ns);
            proxy("}}");
            proxy("");

            
            stub("template<> std::function<rpc::shared_ptr<rpc::i_interface_stub>(const rpc::shared_ptr<rpc::object_stub>& stub)> rpc::service::create_interface_stub(const rpc::shared_ptr<{}{}>& iface)",
                 ns, interface_name);
            stub("{{");
            stub("return [&](const rpc::shared_ptr<rpc::object_stub>& stub) -> "
                 "rpc::shared_ptr<rpc::i_interface_stub>{{");
            stub("return rpc::static_pointer_cast<rpc::i_interface_stub>({}{}_stub::create(iface, stub));", ns,
                 interface_name);
            stub("}};");
            stub("}}");

            stub("template<> rpc::interface_descriptor rpc::service::proxy_bind_in_param(const rpc::shared_ptr<{}{}>& iface, rpc::shared_ptr<rpc::object_stub>& stub)",
                 ns, interface_name);
            stub("{{");
            stub("if(!iface)");
            stub("{{");
            stub("return {{{{0}},{{0}}}};");
            stub("}}");

            stub("auto factory = create_interface_stub(iface);");
            stub("return get_proxy_stub_descriptor({{0}}, {{0}}, iface.get(), factory, false, stub);");
            stub("}}");
            
            stub("template<> rpc::interface_descriptor rpc::service::stub_bind_out_param(caller_channel_zone caller_channel_zone_id, caller_zone caller_zone_id, const rpc::shared_ptr<{}{}>& iface)",
                 ns, interface_name);
            stub("{{");
            stub("if(!iface)");
            stub("{{");
            stub("return {{{{0}},{{0}}}};");
            stub("}}");

            stub("rpc::shared_ptr<rpc::object_stub> stub;");

            stub("auto factory = create_interface_stub(iface);");
            stub("return get_proxy_stub_descriptor(caller_channel_zone_id, caller_zone_id, iface.get(), factory, true, stub);");
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
                    bool is_inline = cls->get_attribute("inline") == "inline";
                    if(is_inline)
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
        void write_namespace(bool from_host, const class_entity& lib, std::string prefix, writer& header, writer& proxy,
                             writer& stub)
        {
            for (auto cls : lib.get_classes())
            {
                if (!cls->get_import_lib().empty())
                    continue;
                if (cls->get_type() == entity_type::NAMESPACE)
                {
                    bool is_inline = cls->get_attribute("inline") == "inline";

                    if(is_inline)
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
                stub("int stub_factory(rpc::interface_ordinal interface_id, rpc::shared_ptr<T> original, "
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
        void write_files(bool from_host, const class_entity& lib, std::ostream& hos, std::ostream& pos, std::ostream& phos,
                         std::ostream& sos, std::ostream& shos, const std::vector<std::string>& namespaces,
                         const std::string& header_filename, const std::string& proxy_header_filename, 
                         const std::string& stub_header_filename, const std::list<std::string>& imports)
        {
            writer header(hos);
            writer proxy(pos);
            writer proxy_header(phos);
            writer stub(sos);
            writer stub_header(shos);

            header("#pragma once");
            header("");
            header("#include <memory>");
            header("#include <vector>");
            header("#include <list>");
            header("#include <map>");
            header("#include <set>");
            header("#include <string>");
            header("#include <array>");

            header("#include <rpc/version.h>");
            header("#include <rpc/marshaller.h>");
            header("#include <rpc/service.h>");
            header("#include <rpc/error_codes.h>");
            header("#include <rpc/types.h>");
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

            proxy_header("#pragma once");
            proxy_header("#include <yas/mem_streams.hpp>");
            proxy_header("#include <yas/binary_iarchive.hpp>");
            proxy_header("#include <yas/binary_oarchive.hpp>");
            proxy_header("#include <yas/std_types.hpp>");
            proxy_header("#include <yas/count_streams.hpp>");
            proxy_header("#include <rpc/proxy.h>");
            proxy_header("#include <rpc/stub.h>");
            proxy_header("#include <rpc/service.h>");
            proxy_header("#include \"{}\"", header_filename);
            proxy_header("");

            proxy("#include \"{}\"", proxy_header_filename);
            proxy("");

            stub_header("#pragma once");
            stub_header("#include <yas/mem_streams.hpp>");
            stub_header("#include <yas/binary_iarchive.hpp>");
            stub_header("#include <yas/binary_oarchive.hpp>");
            stub_header("#include <yas/count_streams.hpp>");
            stub_header("#include <yas/std_types.hpp>");
            stub_header("#include <rpc/stub.h>");
            stub_header("#include <rpc/proxy.h>");
            stub_header("#include <rpc/service.h>");
            stub_header("#include \"{}\"", header_filename);
            stub_header("");

            stub("#include \"{}\"", stub_header_filename);
            stub("");

            std::string prefix;
            for (auto& ns : namespaces)
            {
                header("namespace {}", ns);
                header("{{");
                proxy("namespace {}", ns);
                proxy("{{");
                proxy_header("namespace {}", ns);
                proxy_header("{{");
                stub("namespace {}", ns);
                stub("{{");
                stub_header("namespace {}", ns);
                stub_header("{{");

                prefix += ns + "::";
            }

            write_namespace_predeclaration(lib, header, proxy_header, stub_header);
            
            write_stub_factory_lookup(lib, prefix, header, proxy, stub);

            write_namespace(from_host, lib, prefix, header, proxy, stub);

            for (auto& ns : namespaces)
            {
                header("}}");
                proxy("}}");
                proxy_header("}}");
                stub("}}");
                stub_header("}}");
            }

            write_epilog(from_host, lib, header, proxy, stub, namespaces);
        }
    }
}