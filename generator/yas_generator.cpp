#include <type_traits>
#include <algorithm>
#include <tuple>
#include <type_traits>
#include <unordered_set>

#include "coreclasses.h"
#include "cpp_parser.h"
#include "helpers.h"
extern "C"
{
#include "sha3.h"
}
#include <filesystem>
#include <sstream>

#include "writer.h"

#include "interface_declaration_generator.h"
#include "fingerprint_generator.h"
#include "yas_generator.h"

namespace rpc_generator
{
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
                std::ignore = option;
                std::ignore = from_host;
                std::ignore = lib;
                std::ignore = name;
                std::ignore = is_in;
                std::ignore = is_out;
                std::ignore = is_const;
                std::ignore = object_type;
                std::ignore = count;
                
                assert(false);
            }
        };

        template<>
        std::string renderer::render<renderer::BY_VALUE>(print_type option, bool from_host, const class_entity& lib,
                                                         const std::string& name, bool is_in, bool is_out,
                                                         bool is_const, const std::string& object_type,
                                                         uint64_t& count) const
        {
            std::ignore = from_host;
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

            case PROXY_MARSHALL_IN:
            case PROXY_MARSHALL_OUT:
            case STUB_MARSHALL_IN:
            case STUB_MARSHALL_OUT:
                return fmt::format("  ,(\"{0}\", {0})", name);

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
            std::ignore = from_host;
            std::ignore = lib;
            std::ignore = is_in;
            std::ignore = is_const;
            std::ignore = count;
            
            if(is_out)
            {
                throw std::runtime_error("REFERANCE does not support out vals");
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

            case PROXY_MARSHALL_IN:
            case PROXY_MARSHALL_OUT:
            case STUB_MARSHALL_IN:
            case STUB_MARSHALL_OUT:
                return fmt::format("  ,(\"{0}\", {0})", name);
            default:
                return "";
            }
        };

        template<>
        std::string renderer::render<renderer::MOVE>(print_type option, bool from_host, const class_entity& lib,
                                                     const std::string& name, bool is_in, bool is_out, bool is_const,
                                                     const std::string& object_type, uint64_t& count) const
        {
            std::ignore = from_host;
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
            case PROXY_MARSHALL_IN:
            case PROXY_MARSHALL_OUT:
            case STUB_MARSHALL_IN:
            case STUB_MARSHALL_OUT:
                return fmt::format("  ,(\"{0}\", {0})", name);
            default:
                return "";
            }
        };

        template<>
        std::string renderer::render<renderer::POINTER>(print_type option, bool from_host, const class_entity& lib,
                                                        const std::string& name, bool is_in, bool is_out, bool is_const,
                                                        const std::string& object_type, uint64_t& count) const
        {
            std::ignore = from_host;
            std::ignore = lib;
            std::ignore = is_in;
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
                return fmt::format("uint64_t {}", name);
            case PROXY_MARSHALL_IN:
            case PROXY_MARSHALL_OUT:
            case STUB_MARSHALL_IN:
            case STUB_MARSHALL_OUT:
                return fmt::format("  ,(\"{0}\", {0})", name);
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
            std::ignore = from_host;
            std::ignore = lib;
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
        };

        template<>
        std::string renderer::render<renderer::POINTER_POINTER>(print_type option, bool from_host,
                                                                const class_entity& lib, const std::string& name,
                                                                bool is_in, bool is_out, bool is_const,
                                                                const std::string& object_type, uint64_t& count) const
        {
            std::ignore = from_host;
            std::ignore = lib;
            std::ignore = is_in;
            std::ignore = is_out;
            std::ignore = is_const;
            std::ignore = object_type;
            std::ignore = count;
            
            switch(option)
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
        };

        template<>
        std::string renderer::render<renderer::INTERFACE>(print_type option, bool from_host, const class_entity& lib,
                                                          const std::string& name, bool is_in, bool is_out,
                                                          bool is_const, const std::string& object_type,
                                                          uint64_t& count) const
        {
            std::ignore = from_host;
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
        };

        template<>
        std::string
        renderer::render<renderer::INTERFACE_REFERENCE>(print_type option, bool from_host, const class_entity& lib,
                                                        const std::string& name, bool is_in, bool is_out, bool is_const,
                                                        const std::string& object_type, uint64_t& count) const
        {
            std::ignore = from_host;
            std::ignore = lib;
            std::ignore = is_in;
            std::ignore = is_out;
            std::ignore = is_const;
            std::ignore = object_type;
            std::ignore = count;
            
            switch(option)
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
        };

        bool do_in_param(print_type option, bool from_host, const class_entity& lib, const std::string& name,
                        const std::string& type, const std::list<std::string>& attributes, uint64_t& count,
                        std::string& output)
        {
            auto in = is_in_param(attributes);
            auto out = is_out_param(attributes);
            auto is_const = is_const_param(attributes);
            auto by_value = std::find(attributes.begin(), attributes.end(), "by_value") != attributes.end();

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

        bool do_out_param(print_type option, bool from_host, const class_entity& lib, const std::string& name,
                         const std::string& type, const std::list<std::string>& attributes, uint64_t& count,
                         std::string& output)
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

        void build_fully_scoped_name(const class_entity* entity, std::string& name)
        {
            auto* owner = entity->get_owner();
            if(owner && !owner->get_name().empty())
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
            strip_reference_modifiers(ret, reference_modifiers);

            auto template_start = ret.find('<');
            if(template_start != std::string::npos)
            {
                int template_count = 1;
                auto pData = ret.data() + template_start + 1;
                while(*pData != 0 && template_count > 0)
                {
                    if(*pData == '<')
                        template_count++;
                    else if(*pData == '>')
                        template_count--;
                    pData++;
                }
                template_modifier = ret.substr(template_start, pData - (ret.data() + template_start));
                ret = ret.substr(0, template_start);
            }

            std::shared_ptr<class_entity> param_type;
            m_ob.find_class(ret, param_type);

            if(param_type.get())
            {
                ret.clear();
                build_fully_scoped_name(param_type.get(), ret);
            }
            return ret + template_modifier + reference_modifiers;
        }

        void write_proxy_send_method(bool from_host, const class_entity& m_ob, writer& proxy,
                                     const std::string& interface_name,
                                     const std::shared_ptr<function_entity>& function, int& function_count)
        {
            bool has_inparams = false;
            proxy("template<>");
            proxy("{}", ::rpc_generator::write_proxy_send_declaration(
                            m_ob, interface_name + "::proxy_serialiser<rpc::serialiser::yas, rpc::encoding>::", function,
                            has_inparams, ", rpc::encoding __rpc_enc", false));
            proxy("{{");

            if(has_inparams)
            {
                proxy("auto __yas_mapping = YAS_OBJECT_NVP(");
                proxy("  \"in\"");

                uint64_t count = 1;
                for(auto& parameter : function->get_parameters())
                {
                    std::string output;
                    {
                        if(!do_in_param(PROXY_MARSHALL_IN, from_host, m_ob, parameter.get_name(), parameter.get_type(),
                                       parameter.get_attributes(), count, output))
                            continue;

                        proxy(output);
                    }
                    count++;
                }

                proxy("  );");

                proxy("__buffer.clear(); // this does not change the capacity of the vector so this is a low cost reset to the buffer");
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

        void write_proxy_receive_method(bool from_host, const class_entity& m_ob, writer& proxy,
                                        const std::string& interface_name,
                                        const std::shared_ptr<function_entity>& function, int& function_count)
        {
            bool has_inparams = false;
            proxy("template<>");
            proxy("{}", ::rpc_generator::write_proxy_receive_declaration(
                            m_ob, interface_name + "::proxy_deserialiser<rpc::serialiser::yas, rpc::encoding>::", function, 
                            has_inparams, ", rpc::encoding __rpc_enc", false));
            proxy("{{");

            if(has_inparams)
            {
                uint64_t count = 1;
                proxy("try");
                proxy("{{");
                proxy("auto __yas_mapping = YAS_OBJECT_NVP(");
                proxy("  \"out\"");

                for(auto& parameter : function->get_parameters())
                {
                    count++;
                    std::string output;
                    if(!do_out_param(PROXY_MARSHALL_OUT, from_host, m_ob, parameter.get_name(), parameter.get_type(),
                                    parameter.get_attributes(), count, output))
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
                proxy("return rpc::error::PROXY_DESERIALISATION_ERROR();");
                proxy("}}");
                proxy("}}");
                proxy("#ifdef USE_RPC_LOGGING");
                proxy("catch(std::exception& ex)");
                proxy("{{");
                proxy("auto error_message = std::string(\"A proxy deserialisation error has occurred in an {} "
                      "implementation in function {} \") + ex.what();",
                      interface_name, function->get_name());
                proxy("LOG_STR(error_message.data(), error_message.length());");
                proxy("return rpc::error::PROXY_DESERIALISATION_ERROR();");
                proxy("}}");
                proxy("#endif");
                proxy("catch(...)");
                proxy("{{");
                proxy("#ifdef USE_RPC_LOGGING");
                proxy("auto error_message = std::string(\"exception has occurred in an {} implementation in "
                      "function {}\");",
                      interface_name, function->get_name());
                proxy("LOG_STR(error_message.data(), error_message.length());");
                proxy("#endif");
                proxy("return rpc::error::STUB_DESERIALISATION_ERROR();");
                proxy("}}");
            }
            proxy("return rpc::error::OK();");
            proxy("}}");
            proxy("");

            function_count++;
        }

        void write_stub_receive_method(bool from_host, const class_entity& m_ob, writer& proxy,
                                       const std::string& interface_name,
                                       const std::shared_ptr<function_entity>& function, int& function_count)
        {
            bool has_outparams = false;
            proxy("template<>");
            proxy("{}", ::rpc_generator::write_stub_receive_declaration(
                            m_ob, interface_name + "::stub_deserialiser<rpc::serialiser::yas, rpc::encoding>::", function,
                            has_outparams, ", rpc::encoding __rpc_enc", false));
            proxy("{{");

            if(has_outparams)
            {
                uint64_t count = 1;
                proxy("try");
                proxy("{{");
                proxy("auto __yas_mapping = YAS_OBJECT_NVP(");
                proxy("  \"out\"");

                for(auto& parameter : function->get_parameters())
                {
                    count++;
                    std::string output;
                    if(!do_in_param(STUB_MARSHALL_IN, from_host, m_ob, parameter.get_name(), parameter.get_type(),
                                   parameter.get_attributes(), count, output))
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
                proxy("return rpc::error::PROXY_DESERIALISATION_ERROR();");
                proxy("}}");
                proxy("}}");
                proxy("#ifdef USE_RPC_LOGGING");
                proxy("catch(std::exception& ex)");
                proxy("{{");
                proxy("auto error_message = std::string(\"A proxy deserialisation error has occurred in an {} "
                      "implementation in function {} \") + ex.what();",
                      interface_name, function->get_name());
                proxy("LOG_STR(error_message.data(), error_message.length());");
                proxy("return rpc::error::PROXY_DESERIALISATION_ERROR();");
                proxy("}}");
                proxy("#endif");
                proxy("catch(...)");
                proxy("{{");
                proxy("#ifdef USE_RPC_LOGGING");
                proxy("auto error_message = std::string(\"exception has occurred in an {} implementation in "
                      "function {}\");",
                      interface_name, function->get_name());
                proxy("LOG_STR(error_message.data(), error_message.length());");
                proxy("#endif");
                proxy("return rpc::error::STUB_DESERIALISATION_ERROR();");
                proxy("}}");
            }
            proxy("return rpc::error::OK();");
            proxy("}}");
            proxy("");

            function_count++;
        }

        void write_stub_reply_method(bool from_host, const class_entity& m_ob, writer& proxy,
                                     const std::string& interface_name,
                                     const std::shared_ptr<function_entity>& function, int& function_count)
        {
            bool has_outparams = false;
            proxy("template<>");
            proxy("{}", ::rpc_generator::write_stub_reply_declaration(
                            m_ob, interface_name + "::stub_serialiser<rpc::serialiser::yas, rpc::encoding>::", function,
                            has_outparams, ", rpc::encoding __rpc_enc", false));
            proxy("{{");

            if(has_outparams)
            {
                proxy("auto __yas_mapping = YAS_OBJECT_NVP(");
                proxy("  \"out\"");

                uint64_t count = 1;
                for(auto& parameter : function->get_parameters())
                {
                    std::string output;
                    {
                        if(!do_out_param(STUB_MARSHALL_OUT, from_host, m_ob, parameter.get_name(), parameter.get_type(),
                                        parameter.get_attributes(), count, output))
                            continue;

                        proxy(output);
                    }
                    count++;
                }

                proxy("  );");

                proxy("__buffer.clear(); // this does not change the capacity of the vector so this is a low cost reset to the buffer");
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

        void write_interface(bool from_host, const class_entity& m_ob, writer& proxy)
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

            {
                bool has_methods = false;
                for(auto& function : m_ob.get_functions())
                {
                    if(function->get_entity_type() != entity_type::FUNCTION_METHOD)
                        continue;
                    has_methods = true;
                }

                if(has_methods)
                {
                    int function_count = 0;
                    std::unordered_set<std::string> unique_signatures;
                    for(auto& function : m_ob.get_functions())
                    {
                        if(function->get_entity_type() == entity_type::FUNCTION_METHOD)
                        {
                            ++function_count;
                            bool has_params = false;
                            if(unique_signatures
                                   .emplace(::rpc_generator::write_proxy_send_declaration(
                                       m_ob, "", function, has_params, ", rpc::encoding __rpc_enc",
                                       false))
                                   .second)
                            {
                                write_proxy_send_method(from_host, m_ob, proxy, interface_name, function,
                                                        function_count);
                            }
                        }
                    }
                }
            }

            {
                bool has_methods = false;
                for(auto& function : m_ob.get_functions())
                {
                    if(function->get_entity_type() != entity_type::FUNCTION_METHOD)
                        continue;
                    has_methods = true;
                }

                if(has_methods)
                {
                    int function_count = 0;
                    std::unordered_set<std::string> unique_signatures;
                    for(auto& function : m_ob.get_functions())
                    {
                        if(function->get_entity_type() == entity_type::FUNCTION_METHOD)
                        {
                            ++function_count;
                            bool has_params = false;
                            if(unique_signatures
                                   .emplace(::rpc_generator::write_proxy_receive_declaration(
                                       m_ob, "", function, has_params, ", rpc::encoding __rpc_enc",
                                       false))
                                   .second)
                            {
                                write_proxy_receive_method(from_host, m_ob, proxy, interface_name, function,
                                                           function_count);
                            }
                        }
                    }
                }
            }

            {
                bool has_methods = false;
                for(auto& function : m_ob.get_functions())
                {
                    if(function->get_entity_type() != entity_type::FUNCTION_METHOD)
                        continue;
                    has_methods = true;
                }

                if(has_methods)
                {
                    int function_count = 0;
                    std::unordered_set<std::string> unique_signatures;
                    for(auto& function : m_ob.get_functions())
                    {
                        if(function->get_entity_type() == entity_type::FUNCTION_METHOD)
                        {
                            ++function_count;
                            bool has_params = false;
                            if(unique_signatures
                                   .emplace(::rpc_generator::write_stub_receive_declaration(
                                       m_ob, "", function, has_params, ", rpc::encoding __rpc_enc",
                                       false))
                                   .second)
                            {
                                write_stub_receive_method(from_host, m_ob, proxy, interface_name, function,
                                                          function_count);
                            }
                        }
                    }
                }
            }

            {
                bool has_methods = false;
                for(auto& function : m_ob.get_functions())
                {
                    if(function->get_entity_type() != entity_type::FUNCTION_METHOD)
                        continue;
                    has_methods = true;
                }

                if(has_methods)
                {
                    int function_count = 0;
                    std::unordered_set<std::string> unique_signatures;
                    for(auto& function : m_ob.get_functions())
                    {
                        if(function->get_entity_type() == entity_type::FUNCTION_METHOD)
                        {
                            ++function_count;
                            bool has_params = false;
                            if(unique_signatures
                                   .emplace(::rpc_generator::write_stub_reply_declaration(
                                       m_ob, "", function, has_params, ", rpc::encoding __rpc_enc",
                                       false))
                                   .second)
                            {
                                write_stub_reply_method(from_host, m_ob, proxy, interface_name, function,
                                                        function_count);
                            }
                        }
                    }
                }
            }
        };

        // entry point
        void write_namespace(bool from_host, const class_entity& lib, std::string prefix, writer& proxy,
                             bool catch_stub_exceptions, const std::vector<std::string>& rethrow_exceptions)
        {
            for(auto& elem : lib.get_elements(entity_type::NAMESPACE_MEMBERS))
            {
                if(elem->is_in_import())
                    continue;
                else if(elem->get_entity_type() == entity_type::NAMESPACE)
                {
                    bool is_inline = elem->get_attribute("inline") == "inline";

                    if(is_inline)
                    {
                        proxy("inline namespace {}", elem->get_name());
                    }
                    else
                    {
                        proxy("namespace {}", elem->get_name());
                    }
                    proxy("{{");
                    auto& ent = static_cast<const class_entity&>(*elem);
                    write_namespace(from_host, ent, prefix + elem->get_name() + "::", proxy, catch_stub_exceptions,
                                    rethrow_exceptions);
                    proxy("}}");
                }
                else if(elem->get_entity_type() == entity_type::INTERFACE
                        || elem->get_entity_type() == entity_type::LIBRARY)
                {
                    auto& ent = static_cast<const class_entity&>(*elem);
                    write_interface(from_host, ent, proxy);
                }
            }
        }

        // entry point
        void write_files(bool from_host, const class_entity& lib, std::ostream& header_stream,
                         const std::vector<std::string>& namespaces, const std::string& header_filename,
                         bool catch_stub_exceptions, const std::vector<std::string>& rethrow_exceptions,
                         const std::vector<std::string>& additional_stub_headers)
        {
            std::stringstream tmpstr;
            std::ostream& t = tmpstr;
            writer tmp(t);
            writer header(header_stream);

            std::for_each(additional_stub_headers.begin(), additional_stub_headers.end(),
                          [&](const std::string& additional_stub_header)
                          { header("#include <{}>", additional_stub_header); });

            header("#include <yas/mem_streams.hpp>");
            header("#include <yas/binary_iarchive.hpp>");
            header("#include <yas/binary_oarchive.hpp>");
            header("#include <yas/serialize.hpp>");
            header("#include <yas/std_types.hpp>");
            header("#include <rpc/proxy.h>");
            header("#include <rpc/stub.h>");
            header("#include <rpc/error_codes.h>");
            header("#include <rpc/marshaller.h>");
            header("#include <rpc/serialiser.h>");
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

            write_namespace(from_host, lib, prefix, header, catch_stub_exceptions, rethrow_exceptions);

            for(auto& ns : namespaces)
            {
                (void)ns;
                header("}}");
            }
        }
    }
}
