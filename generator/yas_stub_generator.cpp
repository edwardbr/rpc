#include <type_traits>
#include <algorithm>
#include <tuple>
#include <type_traits>
#include "coreclasses.h"
#include "cpp_parser.h"
extern "C"
{
#include "sha3.h"
}
#include <filesystem>
#include <sstream>

#include "writer.h"

#include "fingerprint_generator.h"
#include "yas_generator.h"

namespace rpc_generator
{
    namespace yas_generator
    {
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
            switch(option)
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
            if(is_out)
            {
                throw std::runtime_error("REFERANCE does not support out vals");
            }

            switch(option)
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
            if(is_out)
            {
                throw std::runtime_error("POINTER does not support out vals");
            }

            switch(option)
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
            if(is_const && is_out)
            {
                throw std::runtime_error("POINTER_REFERENCE does not support const out vals");
            }
            switch(option)
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
            switch(option)
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
            if(is_out)
            {
                throw std::runtime_error("INTERFACE does not support out vals");
            }

            switch(option)
            {
            case PROXY_PREPARE_IN:
                return fmt::format("rpc::shared_ptr<rpc::object_stub> {}_stub_;", name);
            case PROXY_PREPARE_IN_INTERFACE_ID:
                return fmt::format("RPC_ASSERT(rpc::are_in_same_zone(this, {0}.get()));\n"
                                   "\t\t\tauto {0}_stub_id_ = proxy_bind_in_param(__rpc_sp->get_remote_rpc_version(), "
                                   "{0}, {0}_stub_);",
                                   name);
            case PROXY_MARSHALL_IN:
            {
                auto ret = fmt::format(",(\"_{1}\", {0}_stub_id_)", name, count);
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
                    auto target_stub_strong = target_stub_.lock();
                    if (target_stub_strong)
                    {{
                        auto& zone_ = target_stub_strong->get_zone();
                        __rpc_ret = rpc::stub_bind_in_param(protocol_version, zone_, caller_channel_zone_id, caller_zone_id, {1}_object_, {1});
                    }}
                    else
                    {{
                        assert(false);
                        __rpc_ret = rpc::error::ZONE_NOT_FOUND();
                    }}
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
            switch(option)
            {
            case PROXY_PREPARE_IN:
                return fmt::format("rpc::shared_ptr<rpc::object_stub> {}_stub_;", name);
            case PROXY_PREPARE_IN_INTERFACE_ID:
                return fmt::format("RPC_ASSERT(rpc::are_in_same_zone(this, {0}.get()));\n"
                                   "\t\t\tauto {0}_stub_id_ = proxy_bind_in_param(__rpc_sp->get_remote_rpc_version(), "
                                   "{0}, {0}_stub_);",
                                   name);
            case PROXY_MARSHALL_IN:
            {
                auto ret = fmt::format(",(\"_{1}\", {0}_stub_id_)", name, count);
                count++;
                return ret;
            }
            case PROXY_MARSHALL_OUT:
                return fmt::format("  ,(\"{0}\", {0}_)", name);

            case PROXY_CLEAN_IN:
                return fmt::format("if({0}_stub_) {0}_stub_->release_from_service();", name);

            case STUB_DEMARSHALL_DECLARATION:
                return fmt::format("{} {}", object_type, name);
            case STUB_PARAM_CAST:
                return name;
            case PROXY_VALUE_RETURN:
                return fmt::format(
                    "rpc::proxy_bind_out_param(__rpc_sp, {0}_, __rpc_sp->get_zone_id().as_caller(), {0});", name);
            case PROXY_OUT_DECLARATION:
                return fmt::format("rpc::interface_descriptor {}_;", name);
            case STUB_ADD_REF_OUT_PREDECLARE:
                return fmt::format("rpc::interface_descriptor {0}_;", name);
            case STUB_ADD_REF_OUT:
                return fmt::format(
                    "{0}_ = stub_bind_out_param(zone_, protocol_version, caller_channel_zone_id, caller_zone_id, {0});",
                    name);
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
            if(pos != std::string::npos)
            {
                pos += template_pattern.length();
                while(type_name[pos] == ' ' || type_name[pos] == '\n' || type_name[pos] == '\r'
                      || type_name[pos] == '\t')
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

        bool is_in_call(print_type option, bool from_host, const class_entity& lib, const std::string& name,
                        const std::string& type, const std::list<std::string>& attributes, uint64_t& count,
                        std::string& output)
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
                    output = renderer().render<renderer::BY_VALUE>(option, from_host, lib, name, in, out, is_const,
                                                                   type_name, count);
                }
                else if(reference_modifiers == "&")
                {
                    if(by_value)
                    {
                        output = renderer().render<renderer::BY_VALUE>(option, from_host, lib, name, in, out, is_const,
                                                                       type_name, count);
                    }
                    else if(from_host == false)
                    {
                        throw std::runtime_error("passing data by reference from a non host zone is not allowed");
                    }
                    else
                    {
                        output = renderer().render<renderer::REFERANCE>(option, from_host, lib, name, in, out, is_const,
                                                                        type_name, count);
                    }
                }
                else if(reference_modifiers == "&&")
                {
                    output = renderer().render<renderer::MOVE>(option, from_host, lib, name, in, out, is_const,
                                                               type_name, count);
                }
                else if(reference_modifiers == "*")
                {
                    output = renderer().render<renderer::POINTER>(option, from_host, lib, name, in, out, is_const,
                                                                  type_name, count);
                }
                else if(reference_modifiers == "*&")
                {
                    output = renderer().render<renderer::POINTER_REFERENCE>(option, from_host, lib, name, in, out,
                                                                            is_const, type_name, count);
                }
                else if(reference_modifiers == "**")
                {
                    output = renderer().render<renderer::POINTER_POINTER>(option, from_host, lib, name, in, out,
                                                                          is_const, type_name, count);
                }
                else
                {

                    std::cerr << fmt::format("passing data by {} as in {} {} is not supported", reference_modifiers,
                                             type, name);
                    throw fmt::format("passing data by {} as in {} {} is not supported", reference_modifiers, type,
                                      name);
                }
            }
            else
            {
                if(reference_modifiers.empty() || (reference_modifiers == "&" && (is_const || !out)))
                {
                    output = renderer().render<renderer::INTERFACE>(option, from_host, lib, name, in, out, is_const,
                                                                    type_name, count);
                }
                else if(reference_modifiers == "&")
                {
                    output = renderer().render<renderer::INTERFACE_REFERENCE>(option, from_host, lib, name, in, out,
                                                                              is_const, type_name, count);
                }
                else
                {
                    std::cerr << fmt::format("passing interface by {} as in {} {} is not supported",
                                             reference_modifiers, type, name);
                    throw fmt::format("passing interface by {} as in {} {} is not supported", reference_modifiers, type,
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
                    output = renderer().render<renderer::BY_VALUE>(option, from_host, lib, name, in, out, is_const,
                                                                   type_name, count);
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
                    output = renderer().render<renderer::POINTER_REFERENCE>(option, from_host, lib, name, in, out,
                                                                            is_const, type_name, count);
                }
                else if(reference_modifiers == "**")
                {
                    output = renderer().render<renderer::POINTER_POINTER>(option, from_host, lib, name, in, out,
                                                                          is_const, type_name, count);
                }
                else
                {

                    std::cerr << fmt::format("passing data by {} as in {} {} is not supported", reference_modifiers,
                                             type, name);
                    throw fmt::format("passing data by {} as in {} {} is not supported", reference_modifiers, type,
                                      name);
                }
            }
            else
            {
                if(reference_modifiers == "&")
                {
                    output = renderer().render<renderer::INTERFACE_REFERENCE>(option, from_host, lib, name, in, out,
                                                                              is_const, type_name, count);
                }
                else
                {
                    std::cerr << fmt::format("passing interface by {} as in {} {} is not supported",
                                             reference_modifiers, type, name);
                    throw fmt::format("passing interface by {} as in {} {} is not supported", reference_modifiers, type,
                                      name);
                }
            }
            return true;
        }

        void write_method(bool from_host, const class_entity& m_ob, writer& proxy, writer& stub,
                          const std::string& interface_name, const std::shared_ptr<function_entity>& function,
                          int& function_count, bool catch_stub_exceptions,
                          const std::vector<std::string>& rethrow_exceptions)
        {
            if(function->get_entity_type() == entity_type::FUNCTION_METHOD)
            {
                stub("case {}:", function_count);
                stub("{{");

                proxy.print_tabs();
                proxy.raw("virtual {} {}(", function->get_return_type(), function->get_name());
                bool has_parameter = false;
                for(auto& parameter : function->get_parameters())
                {
                    if(has_parameter)
                    {
                        proxy.raw(", ");
                    }
                    has_parameter = true;
                    std::string modifier;
                    for(auto& item : parameter.get_attributes())
                    {
                        if(item == "const")
                            modifier = "const " + modifier;
                    }
                    proxy.raw("{}{} {}", modifier, parameter.get_type(), parameter.get_name());
                }
                bool function_is_const = false;
                for(auto& item : function->get_attributes())
                {
                    if(item == "const")
                        function_is_const = true;
                }
                if(function_is_const)
                {
                    proxy.raw(") const override\n");
                }
                else
                {
                    proxy.raw(") override\n");
                }
                proxy("{{");

                bool has_inparams = false;

                proxy("auto __rpc_op = get_object_proxy();");
                proxy("auto __rpc_sp = __rpc_op->get_service_proxy();");
                proxy("if (auto telemetry_service = rpc::telemetry_service_manager::get(); telemetry_service)");
                proxy("{{");
                proxy("telemetry_service->on_interface_proxy_send(\"{0}::{1}\", "
                      "__rpc_sp->get_zone_id(), "
                      "__rpc_sp->get_destination_zone_id(), "
                      "__rpc_op->get_object_id(), {{{0}_proxy::get_id(rpc::get_version())}}, {{{2}}});",
                      interface_name, function->get_name(), function_count);
                proxy("}}");

                {
                    stub("//STUB_DEMARSHALL_DECLARATION");
                    stub("#if defined(__clang__) || defined(__GNUC__) || defined(__GNUG__)");
                    stub("#pragma GCC diagnostic push");
                    stub("#pragma GCC diagnostic ignored \"-Wunused-variable\"");
                    stub("#endif");
                    stub("int __rpc_ret = rpc::error::OK();");
                    uint64_t count = 1;
                    for(auto& parameter : function->get_parameters())
                    {
                        std::string output;
                        if(is_in_call(STUB_DEMARSHALL_DECLARATION, from_host, m_ob, parameter.get_name(),
                                      parameter.get_type(), parameter.get_attributes(), count, output))
                            has_inparams = true;
                        else
                            is_out_call(STUB_DEMARSHALL_DECLARATION, from_host, m_ob, parameter.get_name(),
                                        parameter.get_type(), parameter.get_attributes(), count, output);
                        stub("{};", output);
                    }
                    stub("#if defined(__clang__) || defined(__GNUC__) || defined(__GNUG__)");
                    stub("#pragma GCC diagnostic pop");
                    stub("#endif");
                }

                proxy("std::vector<char> __rpc_in_buf;");
                proxy("auto __rpc_ret = rpc::error::OK();");
                proxy("std::vector<char> __rpc_out_buf(24); //max size using short string optimisation");

                proxy("//PROXY_PREPARE_IN");
                uint64_t count = 1;
                for(auto& parameter : function->get_parameters())
                {
                    std::string output;
                    {
                        if(!is_in_call(PROXY_PREPARE_IN, from_host, m_ob, parameter.get_name(), parameter.get_type(),
                                       parameter.get_attributes(), count, output))
                            continue;
                        proxy(output);

                        if(!is_in_call(PROXY_PREPARE_IN_INTERFACE_ID, from_host, m_ob, parameter.get_name(),
                                       parameter.get_type(), parameter.get_attributes(), count, output))
                            continue;

                        proxy(output);
                    }
                    count++;
                }
                proxy("//////////////////////////send here");
                stub("#ifdef RPC_V2");
                stub("if(protocol_version == rpc::VERSION_2)");
                stub("{{");
                if(has_inparams)
                {
                    stub("//PROXY_PREPARE_IN");
                    proxy("auto __rpc_in_yas_mapping = YAS_OBJECT_NVP(");
                    proxy("  \"in\"");

                    stub("//STUB_MARSHALL_IN");
                    stub("yas::intrusive_buffer in(in_buf_, in_size_);");
                    stub("try");
                    stub("{{");
                    stub("auto __rpc_in_yas_mapping = YAS_OBJECT_NVP(");
                    stub("  \"in\"");

                    count = 1;
                    for(auto& parameter : function->get_parameters())
                    {
                        std::string output;
                        {
                            if(!is_in_call(PROXY_MARSHALL_IN, from_host, m_ob, parameter.get_name(),
                                           parameter.get_type(), parameter.get_attributes(), count, output))
                                continue;

                            proxy(output);
                        }
                        count++;
                    }

                    count = 1;
                    for(auto& parameter : function->get_parameters())
                    {
                        std::string output;
                        {
                            if(!is_in_call(STUB_MARSHALL_IN, from_host, m_ob, parameter.get_name(),
                                           parameter.get_type(), parameter.get_attributes(), count, output))
                                continue;

                            stub(output);
                        }
                        count++;
                    }
                    proxy("  );");

                    proxy("yas::mem_ostream __rpc_writer(4096);");
                    proxy("{{");
                    proxy("yas::save<yas::mem|yas::binary|yas::no_header>(__rpc_writer, __rpc_in_yas_mapping);");
                    proxy("auto __rpc_writer_buf = __rpc_writer.get_intrusive_buffer();");
                    proxy("__rpc_in_buf = std::vector<char>(__rpc_writer_buf.data, __rpc_writer_buf.data + "
                          "__rpc_writer_buf.size);");
                    proxy("}}");
                    stub("  );");
                    stub("{{");

                    stub("switch(enc)");
                    stub("{{");
                    stub("case rpc::encoding::yas_compressed_binary:");
                    stub("yas::load<yas::mem|yas::binary|yas::compacted|yas::no_header>(in, __rpc_in_yas_mapping);");
                    stub("break;");
                    stub("case rpc::encoding::yas_text:");
                    stub("yas::load<yas::mem|yas::text|yas::no_header>(in, __rpc_in_yas_mapping);");
                    stub("break;");
                    stub("case rpc::encoding::yas_json:");
                    stub("yas::load<yas::mem|yas::json|yas::no_header>(in, __rpc_in_yas_mapping);");
                    stub("break;");
                    stub("case rpc::encoding::enc_default:");
                    stub("case rpc::encoding::yas_binary:");
                    stub("yas::load<yas::mem|yas::binary|yas::no_header>(in, __rpc_in_yas_mapping);");
                    stub("break;");
                    stub("default:");
                    stub("#ifdef USE_RPC_LOGGING");
                    stub("{{");
                    stub("auto error_message = std::string(\"An invalid rpc encoding has been specified when trying to "
                         "call {} in an implementation of {} \");",
                         function->get_name(), interface_name);
                    stub("LOG_STR(error_message.data(), error_message.length());");
                    stub("}}");
                    stub("#endif");
                    stub("return rpc::error::STUB_DESERIALISATION_ERROR();");
                    stub("}}");
                    stub("}}");
                    stub("}}");
                    stub("#ifdef USE_RPC_LOGGING");
                    stub("catch(std::exception& ex)");
                    stub("{{");
                    stub("auto error_message = std::string(\"A stub deserialisation error has occurred in an {} "
                         "implementation in function {} \") + ex.what();",
                         interface_name, function->get_name());
                    stub("LOG_STR(error_message.data(), error_message.length());");
                    stub("return rpc::error::STUB_DESERIALISATION_ERROR();");
                    stub("}}");
                    stub("#endif");
                    stub("catch(...)");
                    stub("{{");
                    stub("#ifdef USE_RPC_LOGGING");
                    stub("auto error_message = std::string(\"exception has occurred in an {} implementation in "
                         "function {}\");",
                         interface_name, function->get_name());
                    stub("LOG_STR(error_message.data(), error_message.length());");
                    stub("#endif");
                    stub("return rpc::error::STUB_DESERIALISATION_ERROR();");
                    stub("}}");
                }

                stub("}}");

                stub("#endif");
                stub("//STUB_PARAM_WRAP");

                {
                    uint64_t count = 1;
                    for(auto& parameter : function->get_parameters())
                    {
                        std::string output;
                        if(!is_in_call(STUB_PARAM_WRAP, from_host, m_ob, parameter.get_name(), parameter.get_type(),
                                       parameter.get_attributes(), count, output))
                            is_out_call(STUB_PARAM_WRAP, from_host, m_ob, parameter.get_name(), parameter.get_type(),
                                        parameter.get_attributes(), count, output);
                        stub.raw("{}", output);
                    }
                }

                stub("//STUB_PARAM_CAST");
                stub("if(__rpc_ret == rpc::error::OK())");
                stub("{{");
                if(catch_stub_exceptions)
                {
                    stub("try");
                    stub("{{");
                }

                stub.print_tabs();
                stub.raw("__rpc_ret = __rpc_target_->{}(", function->get_name());

                {
                    bool has_param = false;
                    uint64_t count = 1;
                    for(auto& parameter : function->get_parameters())
                    {
                        std::string output;
                        if(!is_in_call(STUB_PARAM_CAST, from_host, m_ob, parameter.get_name(), parameter.get_type(),
                                       parameter.get_attributes(), count, output))
                            is_out_call(STUB_PARAM_CAST, from_host, m_ob, parameter.get_name(), parameter.get_type(),
                                        parameter.get_attributes(), count, output);
                        if(has_param)
                        {
                            stub.raw(",");
                        }
                        has_param = true;
                        stub.raw("{}", output);
                    }
                }
                stub.raw(");\n");
                if(catch_stub_exceptions)
                {
                    stub("}}");

                    for(auto& rethrow_stub_exception : rethrow_exceptions)
                    {
                        stub("catch({}& __ex)", rethrow_stub_exception);
                        stub("{{");
                        stub("throw __ex;");
                        stub("}}");
                    }

                    stub("#ifdef USE_RPC_LOGGING");
                    stub("catch(std::exception ex)");
                    stub("{{");
                    stub("auto error_message = std::string(\"exception has occurred in an {} implementation in "
                         "function {} "
                         "\") + ex.what();",
                         interface_name, function->get_name());
                    stub("LOG_STR(error_message.data(), error_message.length());");
                    stub("__rpc_ret = rpc::error::EXCEPTION();");
                    stub("}}");
                    stub("#endif");
                    stub("catch(...)");
                    stub("{{");
                    stub("#ifdef USE_RPC_LOGGING");
                    stub(
                        "auto error_message = std::string(\"exception has occurred in an {} implementation in function "
                        "{}\");",
                        interface_name, function->get_name());
                    stub("LOG_STR(error_message.data(), error_message.length());");
                    stub("#endif");
                    stub("__rpc_ret = rpc::error::EXCEPTION();");
                    stub("}}");
                }

                stub("}}");

                {
                    uint64_t count = 1;
                    proxy("//PROXY_OUT_DECLARATION");
                    for(auto& parameter : function->get_parameters())
                    {
                        count++;
                        std::string output;
                        if(is_in_call(PROXY_OUT_DECLARATION, from_host, m_ob, parameter.get_name(),
                                      parameter.get_type(), parameter.get_attributes(), count, output))
                            continue;
                        if(!is_out_call(PROXY_OUT_DECLARATION, from_host, m_ob, parameter.get_name(),
                                        parameter.get_type(), parameter.get_attributes(), count, output))
                            continue;

                        proxy(output);
                    }
                }
                {
                    stub("//STUB_ADD_REF_OUT_PREDECLARE");
                    uint64_t count = 1;
                    for(auto& parameter : function->get_parameters())
                    {
                        count++;
                        std::string output;

                        if(!is_out_call(STUB_ADD_REF_OUT_PREDECLARE, from_host, m_ob, parameter.get_name(),
                                        parameter.get_type(), parameter.get_attributes(), count, output))
                            continue;

                        stub(output);
                    }

                    stub("//STUB_ADD_REF_OUT");
                    stub("if(__rpc_ret == rpc::error::OK())");
                    stub("{{");

                    count = 1;
                    bool has_preamble = false;
                    for(auto& parameter : function->get_parameters())
                    {
                        count++;
                        std::string output;

                        if(!is_out_call(STUB_ADD_REF_OUT, from_host, m_ob, parameter.get_name(), parameter.get_type(),
                                        parameter.get_attributes(), count, output))
                            continue;

                        if(!has_preamble && !output.empty())
                        {
                            stub("auto target_stub_strong = target_stub_.lock();");
                            stub("if (target_stub_strong)");
                            stub("{{");
                            stub("auto& zone_ = target_stub_strong->get_zone();");
                            has_preamble = true;
                        }
                        stub(output);
                    }
                    if(has_preamble)
                    {
                        stub("}}");
                        stub("else");
                        stub("{{");
                        stub("assert(false);");
                        stub("}}");
                    }
                    stub("}}");
                }
                bool has_out_parameter = false;
                for(auto& parameter : function->get_parameters())
                {
                    std::string output;
                    if(is_out_call(PROXY_MARSHALL_OUT, from_host, m_ob, parameter.get_name(), parameter.get_type(),
                                   parameter.get_attributes(), count, output))
                    {
                        has_out_parameter = true;
                        break;
                    }
                }
                if(has_out_parameter)
                {
                    proxy("#ifdef RPC_V2");
                    proxy("if(__rpc_sp->get_remote_rpc_version() == rpc::VERSION_2)");
                    proxy("{{");
                    uint64_t count = 1;
                    proxy("//PROXY_MARSHALL_OUT");
                    proxy("try");
                    proxy("{{");
                    proxy("auto __rpc_out_yas_mapping = YAS_OBJECT_NVP(");
                    proxy("  \"out\"");

                    stub("#ifdef RPC_V2");
                    stub("if(protocol_version == rpc::VERSION_2)");
                    stub("{{");
                    stub("//STUB_MARSHALL_OUT");
                    stub("auto __rpc_out_yas_mapping = YAS_OBJECT_NVP(");
                    stub("  \"out\"");

                    for(auto& parameter : function->get_parameters())
                    {
                        count++;
                        std::string output;
                        if(!is_out_call(PROXY_MARSHALL_OUT, from_host, m_ob, parameter.get_name(), parameter.get_type(),
                                        parameter.get_attributes(), count, output))
                            continue;
                        proxy(output);

                        if(!is_out_call(STUB_MARSHALL_OUT, from_host, m_ob, parameter.get_name(), parameter.get_type(),
                                        parameter.get_attributes(), count, output))
                            continue;

                        stub(output);
                    }
                    proxy("  );");
                    proxy("{{");
                    proxy("yas::load<yas::mem|yas::binary|yas::no_header>(yas::intrusive_buffer{{__rpc_out_buf.data(), "
                          "__rpc_out_buf.size()}}, __rpc_out_yas_mapping);");
                    proxy("}}");
                    proxy("}}");
                    proxy("#ifdef USE_RPC_LOGGING");
                    proxy("catch(std::exception ex)");
                    proxy("{{");
                    proxy("auto error_message = std::string(\"A proxy deserialisation error has occurred while calling "
                          "{} in aimplementation of {} \") + ex.what();",
                          function->get_name(), interface_name);
                    proxy("LOG_STR(error_message.data(), error_message.length());");
                    proxy("return rpc::error::PROXY_DESERIALISATION_ERROR();");
                    proxy("}}");
                    proxy("#endif");
                    proxy("catch(...)");
                    proxy("{{");
                    proxy("#ifdef USE_RPC_LOGGING");
                    proxy("auto error_message = std::string(\"A proxy deserialisation error has occurred while calling "
                          "{} in aimplementation of {} \");",
                          function->get_name(), interface_name);
                    proxy("LOG_STR(error_message.data(), error_message.length());");
                    proxy("#endif");
                    proxy("return rpc::error::PROXY_DESERIALISATION_ERROR();");
                    proxy("}}");

                    stub("  );");

                    stub("yas::mem_ostream __rpc_writer(4096);");
                    stub("switch(enc)");
                    stub("{{");
                    stub("case rpc::encoding::yas_compressed_binary:");
                    stub("yas::save<yas::mem|yas::binary|yas::compacted|yas::no_header>(__rpc_writer, "
                         "__rpc_out_yas_mapping);");
                    stub("break;");
                    stub("case rpc::encoding::yas_text:");
                    stub("yas::save<yas::mem|yas::text|yas::no_header>(__rpc_writer, __rpc_out_yas_mapping);");
                    stub("break;");
                    stub("case rpc::encoding::yas_json:");
                    stub("yas::save<yas::mem|yas::json|yas::no_header>(__rpc_writer, __rpc_out_yas_mapping);");
                    stub("break;");
                    stub("case rpc::encoding::enc_default:");
                    stub("case rpc::encoding::yas_binary:");
                    stub("yas::save<yas::mem|yas::binary|yas::no_header>(__rpc_writer, __rpc_out_yas_mapping);");
                    stub("break;");
                    stub("default:");
                    stub("#ifdef USE_RPC_LOGGING");
                    stub("{{");
                    stub("auto error_message = std::string(\"An invalid rpc encoding has been specified when trying to "
                         "call {} in an implementation of {} \");",
                         function->get_name(), interface_name);
                    stub("LOG_STR(error_message.data(), error_message.length());");
                    stub("}}");
                    stub("#endif");
                    stub("return rpc::error::STUB_DESERIALISATION_ERROR();");
                    stub("}}");
                    stub("auto __rpc_writer_buf = __rpc_writer.get_intrusive_buffer();");
                    stub("__rpc_out_buf = std::vector<char>(__rpc_writer_buf.data, __rpc_writer_buf.data + "
                         "__rpc_writer_buf.size);");
                    stub("return __rpc_ret;");

                    proxy("}}");
                    proxy("#endif");
                    stub("}}");
                    stub("#endif");
                }
                else
                {
                    stub("#ifdef RPC_V2");
                    stub("if(protocol_version == rpc::VERSION_2)");
                    stub("{{");

                    stub("if(enc == rpc::encoding::yas_json)");
                    stub("{{");
                    stub("__rpc_out_buf.resize(2);");
                    stub("__rpc_out_buf[0] = '{{';");
                    stub("__rpc_out_buf[1] = '}}';");
                    stub("}}");
                    stub("return __rpc_ret;");
                    stub("}}");
                    stub("#endif");
                }

                proxy("//PROXY_VALUE_RETURN");
                {
                    uint64_t count = 1;
                    for(auto& parameter : function->get_parameters())
                    {
                        count++;
                        std::string output;
                        if(is_in_call(PROXY_VALUE_RETURN, from_host, m_ob, parameter.get_name(), parameter.get_type(),
                                      parameter.get_attributes(), count, output))
                            continue;
                        if(!is_out_call(PROXY_VALUE_RETURN, from_host, m_ob, parameter.get_name(), parameter.get_type(),
                                        parameter.get_attributes(), count, output))
                            continue;

                        proxy(output);
                    }
                }
                proxy("//PROXY_CLEAN_IN");
                {
                    uint64_t count = 1;
                    for(auto& parameter : function->get_parameters())
                    {
                        std::string output;
                        {
                            if(!is_in_call(PROXY_CLEAN_IN, from_host, m_ob, parameter.get_name(), parameter.get_type(),
                                           parameter.get_attributes(), count, output))
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
        }
        void write_interface(bool from_host, const class_entity& m_ob, writer& proxy, writer& stub, size_t id,
                             bool catch_stub_exceptions, const std::vector<std::string>& rethrow_exceptions)
        {
            if(m_ob.is_in_import())
                return;

            auto interface_name
                = std::string(m_ob.get_entity_type() == entity_type::LIBRARY ? "i_" : "") + m_ob.get_name();

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

            proxy("class {}{} : public rpc::casting_interface", interface_name, base_class_declaration);

            proxy("{{");

            stub("class {}{} : public rpc::casting_interface", interface_name, base_class_declaration);
            stub("{{");

            bool has_methods = false;
            for(auto& function : m_ob.get_functions())
            {
                if(function->get_entity_type() != entity_type::FUNCTION_METHOD)
                    continue;
                has_methods = true;
            }

            if(has_methods)
            {
                stub("switch(method_id.get_val())");
                stub("{{");

                int function_count = 1;
                for(auto& function : m_ob.get_functions())
                {
                    if(function->get_entity_type() == entity_type::FUNCTION_METHOD)
                        write_method(from_host, m_ob, proxy, stub, interface_name, function, function_count,
                                     catch_stub_exceptions, rethrow_exceptions);
                }

                stub("default:");
                stub("return rpc::error::INVALID_METHOD_ID();");
                stub("}};");
            }

            proxy("}};");
            proxy("");

            stub("return rpc::error::INVALID_METHOD_ID();");
            stub("}}");
            stub("");
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

        // entry point
        void write_namespace(bool from_host, const class_entity& lib, std::string prefix, writer& proxy, writer& stub,
                             bool catch_stub_exceptions, const std::vector<std::string>& rethrow_exceptions)
        {
            for(auto& elem : lib.get_elements(entity_type::NAMESPACE_MEMBERS))
            {
                // this is deprecated and only to be used with rpc v1, delete when no longer needed
                std::size_t hash = std::hash<std::string> {}(prefix + "::" + elem->get_name());

                if(elem->is_in_import())
                    continue;
                else if(elem->get_entity_type() == entity_type::NAMESPACE)
                {
                    bool is_inline = elem->get_attribute("inline") == "inline";

                    if(is_inline)
                    {
                        proxy("inline namespace {}", elem->get_name());
                        stub("inline namespace {}", elem->get_name());
                    }
                    else
                    {
                        proxy("namespace {}", elem->get_name());
                        stub("namespace {}", elem->get_name());
                    }
                    proxy("{{");
                    stub("{{");
                    auto& ent = static_cast<const class_entity&>(*elem);
                    write_namespace(from_host, ent, prefix + elem->get_name() + "::", proxy, stub,
                                    catch_stub_exceptions, rethrow_exceptions);
                    proxy("}}");
                    stub("}}");
                }
                else if(elem->get_entity_type() == entity_type::INTERFACE
                        || elem->get_entity_type() == entity_type::LIBRARY)
                {
                    auto& ent = static_cast<const class_entity&>(*elem);
                    write_interface(from_host, ent, proxy, stub, hash, catch_stub_exceptions, rethrow_exceptions);
                }
            }
        }

        // entry point
        void write_files(std::string module_name, bool from_host, const class_entity& lib, std::ostream& header_stream,
                         const std::vector<std::string>& namespaces, const std::string& header_filename,
                         const std::list<std::string>& imports, const std::vector<std::string>& additional_headers,
                         bool catch_stub_exceptions, const std::vector<std::string>& rethrow_exceptions,
                         const std::vector<std::string>& additional_stub_headers)
        {
            std::stringstream tmpstr;
            std::ostream& t = tmpstr;
            writer tmp(t);
            writer header(header_stream);

            std::for_each(additional_stub_headers.begin(), additional_stub_headers.end(),
                          [&](const std::string& additional_stub_header)
                          { impl("#include <{}>", additional_stub_header); });

            header("#include <yas/mem_streams.hpp>");
            header("#include <yas/binary_iarchive.hpp>");
            header("#include <yas/binary_oarchive.hpp>");
            header("#include <yas/std_types.hpp>");
            header("#include <rpc/impl.h>");
            header("#include <rpc/stub.h>");
            header("#include <rpc/service.h>");
            header("#include \"{}\"", header_filename);
            header("");

            std::string prefix;
            for(auto& ns : namespaces)
            {
                header("namespace {}", ns);
                header("{{");

                prefix += ns + "::";
            }

            write_namespace(from_host, lib, prefix, header, tmp, catch_stub_exceptions, rethrow_exceptions);
            write_namespace(from_host, lib, prefix, tmp, header, catch_stub_exceptions, rethrow_exceptions);

            for(auto& ns : namespaces)
            {
                (void)ns;
                header("}}");
            }
        }
    }
}
