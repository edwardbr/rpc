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


        uint64_t generate_fingerprint(const class_entity& cls, std::vector<const class_entity*> entity_stack, writer* comment);
        const class_entity& get_root(const class_entity& cls);
        std::string get_full_name(const class_entity& cls);
        const class_entity* find_type(std::string type_name, const class_entity& cls);
        std::string extract_subsituted_templates(const std::string& source, const class_entity& cls, std::vector<const class_entity*> entity_stack);

        const class_entity& get_root(const class_entity& cls)
        {
            auto* tmp = cls.get_owner();
            while (tmp)
            {
                auto tmp1 = tmp->get_owner();                
                if(!tmp1)
                    break;
                tmp = tmp1;
            }   
            if(tmp)
                return *tmp;
            return cls;
        }      

        std::string get_full_name(const class_entity& cls)
        {
            auto name = cls.get_name();
            auto* tmp = cls.get_owner();
            while (tmp)
            {
                name = tmp->get_name() + "::" + name;
                auto tmp1 = tmp->get_owner();                
                if(!tmp1)
                    break;
                tmp = tmp1;
            }   
            name = "::" + name;
            return name;
        }    

        const class_entity* find_type(std::string type_name, const class_entity& cls)
        {
            auto* tmp = &get_root(cls);

            auto type_namespace = split_namespaces(type_name);

            const auto* current_namespace = &cls;
            if(type_name.size() >= 2 && type_name[0] == ':' && type_name[1] == ':')
            {
                current_namespace = &get_root(cls);
            }

            do
            {
                auto candidate_namespace = current_namespace;
            
                while(candidate_namespace)
                {
                    size_t pos = 0;
                    for (const auto& ns : type_namespace) 
                    {
                        std::shared_ptr<class_entity> subcls = nullptr;
                        for(auto tmp1 : candidate_namespace->get_classes())
                        {
                            if(tmp1->get_name() == ns)
                            {
                                subcls = tmp1;
                                break;
                            }
                        }
                        if(subcls)
                        {
                            candidate_namespace = subcls.get();
                        }
                        else
                        {
                            candidate_namespace = nullptr;
                            break;
                        }
                    }      
                    if(candidate_namespace)
                    {
                        return candidate_namespace;
                    }
                }
                current_namespace = current_namespace->get_owner();
            } while (current_namespace);
            
            return nullptr;
        };

        std::string extract_subsituted_templates(const std::string& source, const class_entity& cls, std::vector<const class_entity*> entity_stack)
        {
            std::stringstream sstr;
            std::stringstream temp;
            bool is_in_name = false;
            for(auto char_data : source)
            {
                if(isalpha(char_data) || isdigit(char_data) || char_data == '_' || char_data == ':')
                {
                    temp << char_data;
                }
                else 
                {
                    auto name = temp.str();
                    std::stringstream tmp;
                    temp.swap(tmp);//the clear function does not do what you would think that it would do so we swap them
                    if(!name.empty())
                    {
                        auto* type = find_type(name, cls);
                        if(type && type != &cls)
                        {
                            auto id = generate_fingerprint(*type, entity_stack, nullptr);
                            if(id == 0)
                                sstr << get_full_name(*type);
                            else
                                sstr << id;                            
                        }
                        else
                            sstr << name;
                    }
                    sstr << char_data;
                }
                char_data++;
            }
            auto name = temp.str();
            if(!name.empty())
            {
                auto* type = find_type(name, cls);
                if(type && type != &cls)
                {
                    auto id = generate_fingerprint(*type, entity_stack, nullptr);
                    if(id == 0)
                        sstr << get_full_name(*type);
                    else
                        sstr << id;
                }
                else
                    sstr << name;
            }  
            auto output = sstr.str();          
            return output;
        }

        std::string substitute_template_params(const std::string& type, const std::string& alternative)
        {
            std::string output;
            int inBrackets = 0;
            for(size_t i = 0;i < type.length();i++)
            {
                if(type[i] == '<')
                {
                    inBrackets++;
                    if(inBrackets == 1)
                    {
                        output += type[i];
                        //splice in the alternative
                        output += alternative;
                        continue;
                    }
                }
                else if(type[i] == '>')
                {
                    inBrackets--;
                }
                
                if(!inBrackets)
                {
                    output += type[i];
                }
            }
            if(inBrackets != 0)
            {
                throw std::runtime_error("template type missing '>'");
            }
            return output;
        }

        uint64_t generate_fingerprint(const class_entity& cls, std::vector<const class_entity*> entity_stack, writer* comment)
        {
            for(const auto* tmp : entity_stack)
            {
                if(tmp == &cls)
                {
                    //we have a problem in that we are recursing back to the orginating interface that needs the id of itself even though it is in the middle of working it out
                    return 0;
                }
            }
            entity_stack.push_back(&cls);

            std::string seed;
            for(auto& item : cls.get_attributes())
            {
                //if(item == "")
            }
            if(cls.get_type() == entity_type::INTERFACE || cls.get_type() == entity_type::LIBRARY)
            {
                auto* owner = cls.get_owner();
                while (owner)
                {
                    seed = owner->get_name() + "::" + seed;
                    owner = owner->get_owner();                
                } 

                if(cls.get_type() == entity_type::LIBRARY)
                    seed += "i_";

                seed += cls.get_name();

                seed += "{";
                for(auto& func : cls.get_functions())
                {
                    seed += "[";
                    for(auto& item : cls.get_attributes())
                    {
                        if(item == "noexcept")
                            seed += item + ",";
                        else if(item == "const")
                            seed += item + ",";
                        else if(item == "tag")
                            seed += item + ",";
                    }
                    seed += "]";
                    seed += func.get_name();
                    seed += "(";
                    for(auto& param : func.get_parameters())
                    {
                        seed += "[";
                        for(auto& item : param.get_attributes())
                        {
                            if(item == "in")
                                seed += item + ",";
                            else if(item == "out")
                                seed += item + ",";
                            else if(item == "inout")
                                seed += item + ",";
                            else if(item == "const")
                                seed += item + ",";
                        }
                        seed += "]";

                        auto param_type = param.get_type();
                        if(param_type == "something_complicated")
                            param_type=param_type;
                        std::string reference_modifiers;
                        strip_reference_modifiers(param_type, reference_modifiers);
                        auto template_params = get_template_param(param_type);
                        if(!template_params.empty())
                        {
                            auto substituted_template_param = extract_subsituted_templates(template_params, cls, entity_stack);
                            seed += substitute_template_params(param_type, substituted_template_param);
                        }
                        else
                        {                        
                            auto* type = find_type(param_type, cls);
                            if(type && type != &cls)
                            {
                                uint64_t type_id = generate_fingerprint(*type, entity_stack, nullptr);
                                seed += std::to_string(type_id);
                            }
                            else
                            {
                                seed += param_type;
                            }
                        }
                        
                        seed += reference_modifiers + " ";
                        seed += param.get_name() + ",";
                        
                    }
                    seed += ")";                    
                }
                seed += "}";
            }
            if(!cls.get_is_template() && cls.get_type() == entity_type::STRUCT)
            {
                //template classes cannot know what type they are until the template parameters are specified
                seed += "struct";
                seed += get_full_name(cls);
                auto bc = cls.get_base_classes();
                if (!bc.empty())
                {
                    seed += " : ";
                    int i = 0;
                    for (auto base_class : bc)
                    {
                        if (i)
                            seed += ", ";
                        uint64_t type_id = generate_fingerprint(*base_class, {}, nullptr);
                        seed += std::to_string(type_id) + " ";
                        i++;
                    }
                }
                seed += "{";

                int i = 0;
                for (auto& field : cls.get_functions())
                {
                    if (field.get_type() != FunctionTypeVariable)
                        continue;

                    if (i)
                        seed += ", ";
                    
                    auto param_type = field.get_return_type();
                    std::string reference_modifiers;
                    strip_reference_modifiers(param_type, reference_modifiers);
                    auto template_params = get_template_param(param_type);
                    if(!template_params.empty())
                    {
                        auto substituted_template_param = extract_subsituted_templates(template_params, cls, entity_stack);
                        seed += substitute_template_params(param_type, substituted_template_param);
                    }
                    else
                    {
                        const class_entity* type_info = find_type(param_type, cls);
                        if(type_info && type_info != &cls)
                        {
                            uint64_t type_id = generate_fingerprint(*type_info, entity_stack, nullptr);
                            seed += std::to_string(type_id);                        
                        }
                        else
                        {
                            seed += param_type;    
                        }
                    }
                    seed += reference_modifiers + " ";
                    seed += field.get_name();
                    if (field.get_array_string().size())
                        seed += "[" + field.get_array_string() + "]";
                    i++;
                }
                seed += "}";
            }

            if(comment)
                (*comment)("//{}", seed);
            
            entity_stack.pop_back();

            //convert to sha3 hash
            sha3_context c;
            sha3_Init256(&c);
            sha3_Update(&c, seed.data(), seed.length());
            const auto* hash = sha3_Finalize(&c);
            //truncate and return generated hash
            return *(uint64_t*)hash;
        }

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
            std::string reference_modifiers;
            strip_reference_modifiers(type_name, reference_modifiers);

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
                if (reference_modifiers.empty())
                {
                    output = renderer().render<renderer::BY_VALUE>(option, from_host, lib, name, in, out, is_const,
                                                                   type_name, count);
                }
                else if (reference_modifiers == "&")
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
                else if (reference_modifiers == "&&")
                {
                    output = renderer().render<renderer::MOVE>(option, from_host, lib, name, in, out, is_const,
                                                               type_name, count);
                }
                else if (reference_modifiers == "*")
                {
                    output = renderer().render<renderer::POINTER>(option, from_host, lib, name, in, out, is_const,
                                                                  type_name, count);
                }
                else if (reference_modifiers == "*&")
                {
                    output = renderer().render<renderer::POINTER_REFERENCE>(option, from_host, lib, name, in, out,
                                                                            is_const, type_name, count);
                }
                else if (reference_modifiers == "**")
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
                if (reference_modifiers.empty() || (reference_modifiers == "&" && (is_const || !out)))
                {
                    output = renderer().render<renderer::INTERFACE>(option, from_host, lib, name, in, out, is_const,
                                                                    type_name, count);
                }
                else if (reference_modifiers == "&")
                {
                    output = renderer().render<renderer::INTERFACE_REFERENCE>(option, from_host, lib, name, in, out,
                                                                              is_const, type_name, count);
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
            std::string reference_modifiers;
            strip_reference_modifiers(type_name, reference_modifiers);

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

            if (reference_modifiers.empty())
            {
                std::cerr << fmt::format("out parameters require data to be sent by pointer or reference {} {} ", type,
                                         name);
                throw fmt::format("out parameters require data to be sent by pointeror reference {} {} ", type, name);
            }

            if (!is_interface)
            {
                if (reference_modifiers == "&")
                {
                    output = renderer().render<renderer::BY_VALUE>(option, from_host, lib, name, in, out, is_const,
                                                                    type_name, count);
                }
                else if (reference_modifiers == "&&")
                {
                    throw std::runtime_error("out call rvalue references is not possible");
                }
                else if (reference_modifiers == "*")
                {
                    throw std::runtime_error("passing [out] by_pointer data by * will not work use a ** or *&");
                }
                else if (reference_modifiers == "*&")
                {
                    output = renderer().render<renderer::POINTER_REFERENCE>(option, from_host, lib, name, in, out,
                                                                            is_const, type_name, count);
                }
                else if (reference_modifiers == "**")
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
                if (reference_modifiers == "&")
                {
                    output = renderer().render<renderer::INTERFACE_REFERENCE>(option, from_host, lib, name, in, out,
                                                                              is_const, type_name, count);
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
            header("static rpc::interface_ordinal get_id(uint8_t rpc_version)");
            header("{{");
            header("#ifndef NO_RPC_V2");
            header("if(rpc_version == rpc::VERSION_2)");
            header("{{");
            header("return {{{}ull}};", generate_fingerprint(m_ob, {}, &header));
            header("}}");
            header("#endif");
            header("#ifndef NO_RPC_V1");
            header("if(rpc_version == rpc::VERSION_1)");
            header("{{");
            header("return {{{}ull}};", id);
            header("}}");
            header("#endif");
            header("return {{0}};");
            header("}}");
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
            proxy("{{");
            proxy("telemetry_service->on_interface_proxy_creation(\"{0}_proxy\", "
                  "__rpc_sp->get_zone_id(), "
                  "__rpc_sp->get_destination_zone_id(), __rpc_op->get_object_id(), "
                  "{{{0}_proxy::get_id(rpc::get_version())}});",
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
                  "{{{0}_proxy::get_id(rpc::get_version())}});",
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
                          "__rpc_op->get_object_id(), {{{0}_proxy::get_id(rpc::get_version())}}, {{{1}}});",
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
                        
                        proxy("auto __rpc_in_yas_mapping = YAS_OBJECT_NVP(");
                        proxy("  \"in\"");
                        proxy("  ,(\"__rpc_version\", rpc::get_version())");

                        stub("//STUB_MARSHALL_IN");
                        stub("uint8_t __rpc_version = 0;");
                        stub("yas::intrusive_buffer in(in_buf_, in_size_);");
                        stub("try");
                        stub("{{");
                        stub("auto __rpc_in_yas_mapping = YAS_OBJECT_NVP(");
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
                        
                        proxy("{{");
                        proxy("yas::binary_oarchive<yas::count_ostream, yas::mem|yas::binary|yas::no_header> __rpc_oa(__rpc_counter);");
                        proxy("__rpc_oa(__rpc_in_yas_mapping);");
                        proxy("}}");
                        
                        proxy("__rpc_in_buf.resize(__rpc_counter.total_size);");
                        proxy("yas::mem_ostream __rpc_writer(__rpc_in_buf.data(), __rpc_counter.total_size);");
                        proxy("{{");
                        proxy("yas::save<yas::mem|yas::binary|yas::no_header>(__rpc_writer, __rpc_in_yas_mapping);");
                        proxy("}}");
                        stub("  );");
                        stub("{{");
                        stub("yas::load<yas::mem|yas::binary|yas::no_header>(in, __rpc_in_yas_mapping);");
                        stub("}}");
                        stub("}}");
                        stub("catch(...)");
                        stub("{{");
                        stub("return rpc::error::STUB_DESERIALISATION_ERROR();");
                        stub("}}");
                        
                        stub("if(__rpc_version != rpc::get_version())");
                        stub("  return rpc::error::INVALID_VERSION();");
                    }

                    proxy("std::vector<char> __rpc_out_buf(24); //max size using short string optimisation");
                    proxy("{} __rpc_ret = __rpc_op->send({}::get_id, {{{}}}, __rpc_in_buf.size(), __rpc_in_buf.data(), __rpc_out_buf);",
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
                        proxy("auto __rpc_out_yas_mapping = YAS_OBJECT_NVP(");
                        proxy("  \"out\"");
                        proxy("  ,(\"__return_value\", __rpc_ret)");

                        stub("//STUB_MARSHALL_OUT");
                        stub("auto __rpc_out_yas_mapping = YAS_OBJECT_NVP(");
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
                    proxy("  );");
                    proxy("{{");
                    proxy("yas::load<yas::mem|yas::binary|yas::no_header>(yas::intrusive_buffer{{__rpc_out_buf.data(), __rpc_out_buf.size()}}, __rpc_out_yas_mapping);");
                    proxy("}}");
                    proxy("}}");
                    proxy("catch(...)");
                    proxy("{{");
                    proxy("return rpc::error::PROXY_DESERIALISATION_ERROR();");
                    proxy("}}");
                        
                    stub("  );");

                    stub("yas::count_ostream __rpc_counter;");
                    stub("{{");
                    stub("yas::binary_oarchive<yas::count_ostream, yas::mem|yas::binary|yas::no_header> __rpc_oa(__rpc_counter);");
                    stub("__rpc_oa(__rpc_out_yas_mapping);");
                    stub("}}");
                    stub("__rpc_out_buf.resize(__rpc_counter.total_size);");
                    stub("yas::mem_ostream __rpc_writer(__rpc_out_buf.data(), __rpc_counter.total_size);");
                    stub("{{");
                    stub("yas::save<yas::mem|yas::binary|yas::no_header>(__rpc_writer, __rpc_out_yas_mapping);");
                    stub("}}");
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

            stub("srv->add_interface_stub_factory({0}::get_id, std::make_shared<std::function<rpc::shared_ptr<rpc::i_interface_stub>(const rpc::shared_ptr<rpc::i_interface_stub>&)>>([](const rpc::shared_ptr<rpc::i_interface_stub>& original) -> rpc::shared_ptr<rpc::i_interface_stub>", ns);
            stub("{{");
            stub("auto ci = original->get_castable_interface();");
            stub("const {0}* tmp = nullptr;", ns);
            stub("#ifndef NO_RPC_V2");
            stub("{{");
            stub("auto* tmp = const_cast<{0}*>(static_cast<const {0}*>(ci->query_interface({0}::get_id(rpc::VERSION_2))));", ns);
            stub("if(tmp != nullptr)");
            stub("{{");
            stub("rpc::shared_ptr<{0}> tmp_ptr(ci, tmp);", ns);
            stub("return rpc::static_pointer_cast<rpc::i_interface_stub>({}_stub::create(tmp_ptr, original->get_object_stub()));", ns);
            stub("}}");
            stub("}}");
            stub("#endif");            
            stub("#ifndef NO_RPC_V1");
            stub("{{");
            stub("auto* tmp = const_cast<{0}*>(static_cast<const {0}*>(ci->query_interface({{{0}::get_id(rpc::VERSION_1)}})));", ns);
            stub("if(tmp != nullptr)");
            stub("{{");
            stub("rpc::shared_ptr<{0}> tmp_ptr(ci, tmp);", ns);
            stub("return rpc::static_pointer_cast<rpc::i_interface_stub>({}_stub::create(tmp_ptr, original->get_object_stub()));", ns);
            stub("}}");
            stub("}}");
            stub("#endif");            
            stub("return nullptr;");
            stub("}}));");
        }

        void write_stub_cast_factory(const class_entity& lib, const class_entity& m_ob, writer& stub)
        {
            auto interface_name = std::string(m_ob.get_type() == entity_type::LIBRARY ? "i_" : "") + m_ob.get_name();
            stub("int {}_stub::cast(rpc::interface_ordinal interface_id, rpc::shared_ptr<rpc::i_interface_stub>& new_stub)",
                 interface_name);
            stub("{{");
            stub("auto& service = get_object_stub().lock()->get_zone();");
            stub("int __rpc_ret = service.create_interface_stub(interface_id, {}::get_id, shared_from_this(), new_stub);", interface_name);
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
            stub("rpc::interface_ordinal get_interface_id(uint8_t rpc_version) const override");
            stub("{{");
            stub("return {{{}::get_id(rpc_version)}};", interface_name);
            stub("}}");            
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

            header("static uint64_t get_id(uint8_t rpc_version)");
            header("{{");
            header("//if(rpc_version == rpc::VERSION_1) not implemented");
            header("#ifndef NO_RPC_V2");
            header("if(rpc_version == rpc::VERSION_2)");
            header("{{");
            header("return {}ull;", generate_fingerprint(m_ob, {}, &header));
            header("}}");
            header("#endif");
            header("return 0;");
            header("}}");

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

            proxy("template<> void object_proxy::create_interface_proxy(shared_ptr<{}{}>& "
                  "inface)",
                  ns, interface_name);
            proxy("{{");
            proxy("inface = {1}{0}_proxy::create(shared_from_this());", interface_name, ns);
            proxy("}}");
            proxy("");

            
            stub("template<> std::function<shared_ptr<i_interface_stub>(const shared_ptr<object_stub>& stub)> service::create_interface_stub(const shared_ptr<{}{}>& iface)",
                 ns, interface_name);
            stub("{{");
            stub("return [&](const shared_ptr<object_stub>& stub) -> "
                 "shared_ptr<i_interface_stub>{{");
            stub("return static_pointer_cast<i_interface_stub>({}{}_stub::create(iface, stub));", ns,
                 interface_name);
            stub("}};");
            stub("}}");

            stub("template<> interface_descriptor service::proxy_bind_in_param(const shared_ptr<{}{}>& iface, shared_ptr<object_stub>& stub)",
                 ns, interface_name);
            stub("{{");
            stub("if(!iface)");
            stub("{{");
            stub("return {{{{0}},{{0}}}};");
            stub("}}");

            stub("auto factory = create_interface_stub(iface);");
            stub("return get_proxy_stub_descriptor({{0}}, {{0}}, iface.get(), factory, false, stub);");
            stub("}}");
            
            stub("template<> interface_descriptor service::stub_bind_out_param(caller_channel_zone caller_channel_zone_id, caller_zone caller_zone_id, const shared_ptr<{}{}>& iface)",
                 ns, interface_name);
            stub("{{");
            stub("if(!iface)");
            stub("{{");
            stub("return {{{{0}},{{0}}}};");
            stub("}}");

            stub("shared_ptr<object_stub> stub;");

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

            //this is deprecated and only to be used with rpc v1, delete when no longer needed
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

        void write_stub_factory_lookup_items(const class_entity& lib, std::string prefix, writer& proxy,
                                writer& stub, std::set<std::string>& done)
        {
            for (auto cls : lib.get_classes())
            {
                if (!cls->get_import_lib().empty())
                    continue;
                if (cls->get_type() == entity_type::NAMESPACE)
                {
                    write_stub_factory_lookup_items(*cls, prefix + cls->get_name() + "::", proxy, stub, done);
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
        
        void write_stub_factory_lookup(const std::string module_name, const class_entity& lib, std::string prefix, writer& stub_header, writer& proxy,
                             writer& stub)
        {
            stub_header("int {}_register_stubs(const rpc::shared_ptr<rpc::service>& srv);", module_name);
            stub("int {}_register_stubs(const rpc::shared_ptr<rpc::service>& srv)", module_name);
                stub("{{");

                std::set<std::string> done;

            write_stub_factory_lookup_items(lib, prefix, proxy, stub, done);

            stub("return rpc::error::OK();");
                stub("}}");
            }
            
        // entry point
        void write_files(std::string module_name, bool from_host, const class_entity& lib, std::ostream& hos, std::ostream& pos, 
                         std::ostream& sos, std::ostream& shos, const std::vector<std::string>& namespaces,
                         const std::string& header_filename, 
                         const std::string& stub_header_filename, const std::list<std::string>& imports, std::vector<std::string> additional_headers)
        {
            writer header(hos);
            writer proxy(pos);
            writer stub(sos);
            writer stub_header(shos);

            header("#pragma once");
            header("");

            std::for_each(additional_headers.begin(), additional_headers.end(), [&](const std::string& additional_header){
                header("#include <{}>", additional_header);
            });

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
            
            write_stub_factory_lookup(module_name, lib, prefix, stub_header, proxy, stub);

            write_namespace(from_host, lib, prefix, header, proxy, stub);

            for (auto& ns : namespaces)
            {
                header("}}");
                proxy("}}");
                stub("}}");
                stub_header("}}");
            }

            stub("namespace rpc");
            stub("{{");
            proxy("namespace rpc");
            proxy("{{");
            write_epilog(from_host, lib, header, proxy, stub, namespaces);
            proxy("}}");
            stub("}}");
        }
    }
}