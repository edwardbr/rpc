/*
 *   Copyright (c) 2025 Edward Boggis-Rolfe
 *   All rights reserved.
 */
#include "fingerprint_generator.h"
#include "cpp_parser.h"
#include <filesystem>
#include <sstream>

#include "attributes.h"
#include "rpc_attributes.h"

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
        if (type_name.size() >= 2 && type_name[0] == ':' && type_name[1] == ':')
        {
            current_namespace = &get_root(cls);
        }

        do
        {
            auto candidate_namespace = current_namespace;

            while (candidate_namespace)
            {
                for (const auto& ns : type_namespace)
                {
                    std::shared_ptr<class_entity> subcls = nullptr;
                    for (auto tmp1 : candidate_namespace->get_classes())
                    {
                        if (tmp1->get_name() == ns)
                        {
                            subcls = tmp1;
                            break;
                        }
                    }
                    if (subcls)
                    {
                        candidate_namespace = subcls.get();
                    }
                    else
                    {
                        candidate_namespace = nullptr;
                        break;
                    }
                }
                if (candidate_namespace)
                {
                    return candidate_namespace;
                }
            }
            current_namespace = current_namespace->get_owner();
        } while (current_namespace);

        return nullptr;
    };

    std::string extract_subsituted_templates(
        const std::string& source, const class_entity& cls, std::vector<const class_entity*> entity_stack)
    {
        std::stringstream sstr;
        std::stringstream temp;
        for (auto char_data : source)
        {
            if (isalpha(char_data) || isdigit(char_data) || char_data == '_' || char_data == ':')
            {
                temp << char_data;
            }
            else
            {
                auto name = temp.str();
                std::stringstream tmp;
                temp.swap(tmp); // the clear function does not do what you would think that it would do so we swap them
                if (!name.empty())
                {
                    auto* type = find_type(name, cls);
                    if (type && type != &cls)
                    {
                        auto id = fingerprint::generate(*type, entity_stack, nullptr);
                        if (id == 0)
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
        if (!name.empty())
        {
            auto* type = find_type(name, cls);
            if (type && type != &cls)
            {
                auto id = fingerprint::generate(*type, entity_stack, nullptr);
                if (id == 0)
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

    // this is too simplistic, it only supports one template parameter without nested templates
    std::string substitute_template_params(const std::string& type, const std::string& alternative)
    {
        std::string output;
        int inBrackets = 0;
        for (size_t i = 0; i < type.length(); i++)
        {
            if (type[i] == '<')
            {
                inBrackets++;
                if (inBrackets == 1)
                {
                    output += type[i];
                    // splice in the alternative
                    output += alternative;
                    continue;
                }
            }
            else if (type[i] == '>')
            {
                inBrackets--;
            }

            if (!inBrackets)
            {
                output += type[i];
            }
        }
        if (inBrackets != 0)
        {
            throw std::runtime_error("template type missing '>'");
        }
        return output;
    }

    uint64_t generate(const class_entity& cls, std::vector<const class_entity*> entity_stack, writer* comment)
    {
        for (const auto* tmp : entity_stack)
        {
            if (tmp == &cls)
            {
                // we have a problem in that we are recursing back to the orginating interface that needs the id of
                // itself even though it is in the middle of working it out
                return 0;
            }
        }
        entity_stack.push_back(&cls);

        std::string status_attr = "status";

        std::string seed;
        if (cls.get_entity_type() == entity_type::INTERFACE || cls.get_entity_type() == entity_type::LIBRARY)
        {
            auto* owner = cls.get_owner();
            while (owner)
            {
                seed = owner->get_name() + "::" + seed;
                owner = owner->get_owner();
            }

            if (cls.get_entity_type() == entity_type::LIBRARY)
                seed += "i_";

            seed += cls.get_name();

            seed += "{";
            for (auto& func : cls.get_functions())
            {
                seed += "[";
                for (auto& item : func->get_data())
                {
                    // this keyword should not contaminate the interface fingerprint, so that we can deprecate a method
                    // and warn developers that this method is for the chop unfortunately for legacy reasons
                    // "deprecated" does contaminate the fingerprint we need to flush through all prior interface
                    // versions before we can rehabilitate this as "deprecated"
                    if (item.first == rpc_attribute_types::deprecated_function)
                        continue;

                    // this is to deal with the issue that a deprecated function attribute contaminated the fingerprint
                    // making it impossible to change without changing the interface id which when deprecating a feature
                    // should not do
                    if (item.first == rpc_attribute_types::fingerprint_contaminating_deprecated_function)
                    {
                        seed += "deprecated";
                        continue;
                    }

                    // to handle some idl parser yuckiness
                    if (item.first == attribute_types::tolerate_struct_or_enum)
                    {
                        continue;
                    }

                    // not interested in contaminating the seed with the description, happy to debate this descision
                    if (item.first == attribute_types::description)
                    {
                        continue;
                    }

                    seed += item.first;
                    if (!item.second.empty())
                    {
                        seed += "=";
                        seed += item.second;
                    }
                }
                seed += "]";

                if (func->get_entity_type() == entity_type::CPPQUOTE)
                {
                    if (func->is_in_import())
                        continue;
                    sha3_context c;
                    sha3_Init256(&c);
                    sha3_Update(&c, func->get_name().data(), func->get_name().length());
                    const auto* hash = sha3_Finalize(&c);
                    seed += "#cpp_quote";
                    seed += std::to_string(*(uint64_t*)hash);
                    continue;
                }
                if (func->get_entity_type() == entity_type::FUNCTION_PUBLIC)
                {
                    seed += "public:";
                    continue;
                }
                if (func->get_entity_type() == entity_type::FUNCTION_PRIVATE)
                {
                    seed += "private:";
                    continue;
                }

                seed += func->get_name();
                seed += "(";
                for (auto& param : func->get_parameters())
                {
                    seed += "[";
                    for (auto& item : param)
                    {
                        seed += item.first;
                        if (!item.second.empty())
                        {
                            seed += "=";
                            seed += item.second;
                        }
                    }
                    seed += "]";

                    auto param_type = param.get_type();
                    std::string reference_modifiers;
                    strip_reference_modifiers(param_type, reference_modifiers);
                    auto template_params = get_template_param(param_type);
                    if (!template_params.empty())
                    {
                        auto substituted_template_param = extract_subsituted_templates(template_params, cls, entity_stack);
                        seed += substitute_template_params(param_type, substituted_template_param);
                    }
                    else
                    {
                        auto* type = find_type(param_type, cls);
                        if (type && type != &cls)
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
        else if (cls.get_entity_type() == entity_type::STRUCT)
        {
            if (cls.get_is_template()
                && cls.get_value(rpc_attribute_types::use_legacy_empty_template_struct_id_attr) == "true")
            {
                // this is a bad bug that needs to be preserved for legacy template structures
                std::ignore = cls;
            }
            else
            {
                // template classes cannot know what type they are until the template parameters are specified
                if (cls.get_is_template() && cls.get_value(rpc_attribute_types::use_template_param_in_id_attr) != "false")
                {
                    seed += "template<";
                    bool first_pass = true;
                    for (const auto& param : cls.get_template_params())
                    {
                        if (!first_pass)
                            seed += ",";
                        first_pass = false;
                        seed += param.type;
                        seed += " ";
                        seed += param.get_name();
                    }
                    seed += ">";
                }
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
                        uint64_t type_id = generate(*base_class, {}, nullptr);
                        seed += std::to_string(type_id) + " ";
                        i++;
                    }
                }
                seed += "{";

                int i = 0;
                for (auto& field : cls.get_functions())
                {
                    if (field->get_entity_type() != entity_type::FUNCTION_VARIABLE)
                        continue;

                    if (i)
                        seed += ", ";

                    auto param_type = field->get_return_type();
                    std::string reference_modifiers;
                    strip_reference_modifiers(param_type, reference_modifiers);
                    auto template_params = get_template_param(param_type);
                    if (!template_params.empty())
                    {
                        auto substituted_template_param = extract_subsituted_templates(template_params, cls, entity_stack);
                        seed += substitute_template_params(param_type, substituted_template_param);
                    }
                    else
                    {
                        const class_entity* type_info = find_type(param_type, cls);
                        if (type_info && type_info != &cls)
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
                    if (field->get_array_string().size())
                        seed += "[" + field->get_array_string() + "]";
                    i++;
                }
                seed += "}";
            }
        }

        if (comment)
        {
            if (seed.empty())
                (*comment)("//EMPTY_SEED!");
            else
                (*comment)("//{}", seed);
        }

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
