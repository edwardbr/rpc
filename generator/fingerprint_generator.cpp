#include "fingerprint_generator.h"
#include "cpp_parser.h"
#include <filesystem>
#include <sstream>

extern "C"
{
    #include "sha3.h"
}


namespace fingerprint
{

    
    const class_entity* find_type(std::string type_name, const class_entity& cls)
    {
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
                for(const auto& ns : type_namespace)
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
        } while(current_namespace);

        return nullptr;
    };

    std::string extract_subsituted_templates(const std::string& source, const class_entity& cls,
                                                std::vector<const class_entity*> entity_stack)
    {
        std::stringstream sstr;
        std::stringstream temp;
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
                temp.swap(tmp); // the clear function does not do what you would think that it would do so we swap them
                if(!name.empty())
                {
                    auto* type = find_type(name, cls);
                    if(type && type != &cls)
                    {
                        auto id = fingerprint::generate(*type, entity_stack, nullptr);
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
                auto id = fingerprint::generate(*type, entity_stack, nullptr);
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
        for(size_t i = 0; i < type.length(); i++)
        {
            if(type[i] == '<')
            {
                inBrackets++;
                if(inBrackets == 1)
                {
                    output += type[i];
                    // splice in the alternative
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

                                                 
    uint64_t generate(const class_entity& cls, std::vector<const class_entity*> entity_stack,
                                    writer* comment)
    {
        for(const auto* tmp : entity_stack)
        {
            if(tmp == &cls)
            {
                // we have a problem in that we are recursing back to the orginating interface that needs the id of
                // itself even though it is in the middle of working it out
                return 0;
            }
        }
        entity_stack.push_back(&cls);

        std::string seed;
        for(auto& item : cls.get_attributes())
        {
            seed += item;
        }
        if(cls.get_entity_type() == entity_type::INTERFACE || cls.get_entity_type() == entity_type::LIBRARY)
        {
            auto* owner = cls.get_owner();
            while(owner)
            {
                seed = owner->get_name() + "::" + seed;
                owner = owner->get_owner();
            }

            if(cls.get_entity_type() == entity_type::LIBRARY)
                seed += "i_";

            seed += cls.get_name();

            seed += "{";
            for(auto& func : cls.get_functions())
            {
                {
                    //this is to tell the fingerprinter that an element does not want to be added to the fingerprint
                    bool no_fingerprint = false;
                    for(auto& item : func->get_attributes())
                    {
                        if(item == "no_fingerprint")
                        {
                            no_fingerprint = true;
                            continue;
                        }
                    }
                    if(no_fingerprint)
                        continue;
                }
                
                seed += "[";
                for(auto& item : func->get_attributes())
                {
                    //this keyword should not contaminate the interface fingerprint, so that we can deprecate a method and warn developers that this method is for the chop
                    //unfortunately for legacy reasons "deprecated" does contaminate the fingerprint we need to flush through all prior interface versions before we can rehabilitate this as "deprecated"
                    if(item == "_deprecated")
                        continue;
                    seed += item;
                }
                seed += "]";
                
                if(func->get_entity_type() == entity_type::CPPQUOTE)
                {
                    if(func->is_in_import())
                        continue;
                    sha3_context c;
                    sha3_Init256(&c);
                    sha3_Update(&c, func->get_name().data(), func->get_name().length());
                    const auto* hash = sha3_Finalize(&c);
                    seed += "#cpp_quote";
                    seed += std::to_string(*(uint64_t*)hash);
                    continue;
                }
                if(func->get_entity_type() == entity_type::FUNCTION_PUBLIC)
                {
                    seed += "public:";
                    continue;
                }
                if(func->get_entity_type() == entity_type::FUNCTION_PRIVATE)
                {
                    seed += "private:";
                    continue;
                }

                seed += func->get_name();
                seed += "(";
                for(auto& param : func->get_parameters())
                {
                    seed += "[";
                    for(auto& item : param.get_attributes())
                    {
                        seed += item;
                    }
                    seed += "]";

                    auto param_type = param.get_type();
                    std::string reference_modifiers;
                    strip_reference_modifiers(param_type, reference_modifiers);
                    auto template_params = get_template_param(param_type);
                    if(!template_params.empty())
                    {
                        auto substituted_template_param
                            = extract_subsituted_templates(template_params, cls, entity_stack);
                        seed += substitute_template_params(param_type, substituted_template_param);
                    }
                    else
                    {
                        auto* type = find_type(param_type, cls);
                        if(type && type != &cls)
                        {
                            uint64_t type_id = generate(*type, entity_stack, nullptr);
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
        if(!cls.get_is_template() && cls.get_entity_type() == entity_type::STRUCT)
        {
            // template classes cannot know what type they are until the template parameters are specified
            seed += "struct";
            seed += get_full_name(cls);
            auto bc = cls.get_base_classes();
            if(!bc.empty())
            {
                seed += " : ";
                int i = 0;
                for(auto base_class : bc)
                {
                    if(i)
                        seed += ", ";
                    uint64_t type_id = generate(*base_class, {}, nullptr);
                    seed += std::to_string(type_id) + " ";
                    i++;
                }
            }
            seed += "{";

            int i = 0;
            for(auto& field : cls.get_functions())
            {
                if(field->get_entity_type() != entity_type::FUNCTION_VARIABLE)
                    continue;

                if(i)
                    seed += ", ";

                auto param_type = field->get_return_type();
                std::string reference_modifiers;
                strip_reference_modifiers(param_type, reference_modifiers);
                auto template_params = get_template_param(param_type);
                if(!template_params.empty())
                {
                    auto substituted_template_param
                        = extract_subsituted_templates(template_params, cls, entity_stack);
                    seed += substitute_template_params(param_type, substituted_template_param);
                }
                else
                {
                    const class_entity* type_info = find_type(param_type, cls);
                    if(type_info && type_info != &cls)
                    {
                        uint64_t type_id = generate(*type_info, entity_stack, nullptr);
                        seed += std::to_string(type_id);
                    }
                    else
                    {
                        seed += param_type;
                    }
                }
                seed += reference_modifiers + " ";
                seed += field->get_name();
                if(field->get_array_string().size())
                    seed += "[" + field->get_array_string() + "]";
                i++;
            }
            seed += "}";
        }

        if(comment)
            (*comment)("//{}", seed);

        entity_stack.pop_back();

        // convert to sha3 hash
        sha3_context c;
        sha3_Init256(&c);
        sha3_Update(&c, seed.data(), seed.length());
        const auto* hash = sha3_Finalize(&c);
        // truncate and return generated hash
        return *(uint64_t*)hash;
    }
}