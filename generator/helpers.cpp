#include <iostream>

#include <fmt/format.h>

#include "helpers.h"
#include "coreclasses.h"

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

bool is_interface_param(const class_entity& lib, const std::string& type)
{
    std::string reference_modifiers;
    std::string type_name = type;
    strip_reference_modifiers(type_name, reference_modifiers);

    std::string encapsulated_type = get_encapsulated_shared_ptr_type(type_name);
    if(type == encapsulated_type)
        return false;

    bool is_interface = false;
    std::shared_ptr<class_entity> obj;
    if(lib.find_class(encapsulated_type, obj))
    {
        if(obj->get_entity_type() == entity_type::INTERFACE)
        {
            return true;
        }
    }
    return false;
}


bool is_in_param(const std::list<std::string>& attributes)
{
    return std::find(attributes.begin(), attributes.end(), "in") != attributes.end();
}

bool is_out_param(const std::list<std::string>& attributes)
{
    return std::find(attributes.begin(), attributes.end(), "out") != attributes.end();
}

bool is_const_param(const std::list<std::string>& attributes)
{
    return std::find(attributes.begin(), attributes.end(), "const") != attributes.end();
}


bool is_reference(std::string type_name)
{
    std::string reference_modifiers;
    strip_reference_modifiers(type_name, reference_modifiers);

    return type_name == "&";
}

bool is_rvalue(std::string type_name)
{
    std::string reference_modifiers;
    strip_reference_modifiers(type_name, reference_modifiers);

    return type_name == "&&";
}

bool is_pointer(std::string type_name)
{
    std::string reference_modifiers;
    strip_reference_modifiers(type_name, reference_modifiers);

    return type_name == "*";
}

bool is_pointer_reference(std::string type_name)
{
    std::string reference_modifiers;
    strip_reference_modifiers(type_name, reference_modifiers);

    return type_name == "*&";
}

bool is_pointer_to_pointer(std::string type_name)
{
    std::string reference_modifiers;
    strip_reference_modifiers(type_name, reference_modifiers);

    return type_name == "**";
}