#include <iostream>

#include <fmt/format.h>

#include "helpers.h"
#include "coreclasses.h"
#include "attributes.h"

std::string get_encapsulated_shared_ptr_type(const std::string& type_name)
{
    const std::string template_pattern = "rpc::shared_ptr<";
    auto pos = type_name.find(template_pattern);
    if (pos != std::string::npos)
    {
        pos += template_pattern.length();
        while (type_name[pos] == ' ' || type_name[pos] == '\n' || type_name[pos] == '\r' || type_name[pos] == '\t')
            pos++;
        auto rpos = type_name.rfind(">");
        if (rpos == std::string::npos)
        {
            std::cerr << fmt::format("template parameter is malformed {}", type_name);
            throw fmt::format("template parameter is malformed {}", type_name);
        }
        while (type_name[rpos] == ' ' || type_name[rpos] == '\n' || type_name[rpos] == '\r' || type_name[rpos] == '\t')
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
    if (type == encapsulated_type)
        return false;

    std::shared_ptr<class_entity> obj;
    if (lib.find_class(encapsulated_type, obj))
    {
        if (obj->get_entity_type() == entity_type::INTERFACE)
        {
            return true;
        }
    }
    return false;
}

bool is_in_param(const std::list<std::string>& attributes)
{
    return std::find(attributes.begin(), attributes.end(), attribute_types::in_param) != attributes.end();
}

bool is_out_param(const std::list<std::string>& attributes)
{
    return std::find(attributes.begin(), attributes.end(), attribute_types::out_param) != attributes.end();
}

bool is_const_param(const std::list<std::string>& attributes)
{
    return std::find(attributes.begin(), attributes.end(), attribute_types::const_function) != attributes.end();
}

bool is_reference(std::string type_name)
{
    std::string reference_modifiers;
    strip_reference_modifiers(type_name, reference_modifiers);

    return reference_modifiers == "&";
}

bool is_rvalue(std::string type_name)
{
    std::string reference_modifiers;
    strip_reference_modifiers(type_name, reference_modifiers);

    return reference_modifiers == "&&";
}

bool is_pointer(std::string type_name)
{
    std::string reference_modifiers;
    strip_reference_modifiers(type_name, reference_modifiers);

    return reference_modifiers == "*";
}

bool is_pointer_reference(std::string type_name)
{
    std::string reference_modifiers;
    strip_reference_modifiers(type_name, reference_modifiers);

    return reference_modifiers == "*&";
}

bool is_pointer_to_pointer(std::string type_name)
{
    std::string reference_modifiers;
    strip_reference_modifiers(type_name, reference_modifiers);

    return reference_modifiers == "**";
}

bool is_type_and_parameter_the_same(std::string type, std::string name)
{
    if (type.empty() || type.size() < name.size())
        return false;
    if (*type.rbegin() == '&' || *type.rbegin() == '*')
    {
        type = type.substr(0, type.size() - 1);
    }
    auto template_pos = type.find('<');
    if (template_pos == 0)
    {
        return false;
    }

    if (template_pos != std::string::npos)
    {
        type = type.substr(0, template_pos);
    }
    return type == name;
}

void render_parameter(writer& wrtr, const class_entity& m_ob, const parameter_entity& parameter)
{
    std::string modifier;
    bool has_struct = false;
    for (auto& item : parameter.get_attributes())
    {
        if (item == "const")
            modifier = "const " + modifier;
        if (item == "struct")
            has_struct = true;
    }

    if (has_struct)
    {
        modifier = modifier + "struct ";
    }
    else if (is_type_and_parameter_the_same(parameter.get_type(), parameter.get_name()))
    {
        std::shared_ptr<class_entity> obj;
        if (!m_ob.get_owner()->find_class(parameter.get_name(), obj))
        {
            throw std::runtime_error(std::string("unable to identify type ") + parameter.get_name());
        }
        auto type = obj->get_entity_type();
        if (type == entity_type::STRUCT)
            modifier = modifier + "struct ";
        else if (type == entity_type::ENUM)
            modifier = modifier + "enum ";
    }

    wrtr.raw("{}{} {}", modifier, parameter.get_type(), parameter.get_name());
}

void render_function(writer& wrtr, const class_entity& m_ob, const function_entity& function)
{
    std::string modifier;
    if (function.is_static())
    {
        modifier += "inline static ";
    }
    bool has_struct = false;
    for (auto& item : function.get_attributes())
    {
        if (item == "struct")
            has_struct = true;
    }

    if (has_struct)
    {
        modifier = modifier + "struct ";
    }
    else if (is_type_and_parameter_the_same(function.get_return_type(), function.get_name()))
    {
        std::shared_ptr<class_entity> obj;
        if (!m_ob.get_owner()->find_class(function.get_name(), obj))
        {
            throw std::runtime_error(std::string("unable to identify type ") + function.get_name());
        }
        auto type = obj->get_entity_type();
        if (type == entity_type::STRUCT)
            modifier = modifier + "struct ";
        else if (type == entity_type::ENUM)
            modifier = modifier + "enum ";
    }

    wrtr.raw("{}{} {}", modifier, function.get_return_type(), function.get_name());
}
