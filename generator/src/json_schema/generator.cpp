#include "coreclasses.h" // Your parser API header
#include "cpp_parser.h"  // Your parser API header
#include "json_schema/generator.h"
#include "json_schema/writer.h"
#include <iostream>
#include <string>
#include <vector>
#include <set>
#include <map>
#include <sstream>
#include <algorithm>   // For std::find_if_not, std::reverse, std::find_if
#include <cctype>      // For std::isspace, std::iscntrl
#include <type_traits> // For underlying_type used with ALL_POSSIBLE_MEMBERS
#include <typeinfo>    // For dynamic_cast
#include <stdexcept>   // For std::stoll exceptions, std::stoi
#include <memory>      // For std::shared_ptr
#include <variant>     // For storing different definition info types


// Declare verboseStream if used globally by the generator/parser
extern std::stringstream verboseStream;

namespace json_schema_generator
{

    // --- Forward Declarations ---
    struct SyntheticMethodInfo;
    using DefinitionInfoVariant = std::variant<const class_entity*, SyntheticMethodInfo>;
    using OrderedDefinitionItem = std::pair<std::string, DefinitionInfoVariant>;

    void map_idl_type_to_json_schema(const class_entity& root, const class_entity* current_context,
                                     const std::string& idl_type_name, const attributes& attribs, json_writer& writer,
                                     std::set<std::string>& definitions_needed,
                                     std::set<std::string>& definitions_written,
                                     const std::set<std::string>& currently_processing,
                                     const std::map<std::string, DefinitionInfoVariant>& definition_info_map);
    void write_schema_definition(const class_entity& root, const class_entity& ent, json_writer& writer,
                                 std::set<std::string>& definitions_needed, std::set<std::string>& definitions_written,
                                 const std::set<std::string>& currently_processing,
                                 const std::map<std::string, DefinitionInfoVariant>& definition_info_map);
    void
    write_synthetic_method_struct_definition(const class_entity& root, const SyntheticMethodInfo& info,
                                             json_writer& writer, std::set<std::string>& definitions_needed,
                                             std::set<std::string>& definitions_written,
                                             const std::set<std::string>& currently_processing,
                                             const std::map<std::string, DefinitionInfoVariant>& definition_info_map);
    void find_definable_entities(const class_entity& current_entity, std::vector<OrderedDefinitionItem>& ordered_defs);

    // --- Struct to hold info for synthetic method structs ---
    struct SyntheticMethodInfo
    {
        const class_entity* interface_entity = nullptr;
        const function_entity* method_entity = nullptr;
        bool is_send_struct = true;
    };

    // --- Helper functions ---
    bool has_attribute(const attributes& attribs, const std::string& name)
    {
        for(const auto& attr : attribs)
        {
            if(attr == name || attr.rfind(name + "=", 0) == 0)
                return true;
        }
        return false;
    }
    std::string find_attribute_value(const attributes& attribs, const std::string& name)
    {
        for(const auto& attr : attribs)
        {
            if(attr.rfind(name + "=", 0) == 0)
            {
                if(attr.length() > name.length() + 1)
                {
                    std::string value = attr.substr(name.length() + 1);
                    if(value.length() >= 2 && value.front() == '"' && value.back() == '"')
                    {
                        value = value.substr(1, value.length() - 2);
                    }
                    else if(value.length() >= 2 && value.front() == '\'' && value.back() == '\'')
                    {
                        value = value.substr(1, value.length() - 2);
                    }
                    return value;
                }
                else
                {
                    return "";
                }
            }
        }
        return "";
    }
    std::string clean_type_name(const std::string& raw_type)
    {
        std::string cleaned = raw_type;
        auto first_good_char = std::find_if_not(cleaned.begin(), cleaned.end(),
                                                [](unsigned char c) { return std::isspace(c) || std::iscntrl(c); });
        cleaned.erase(cleaned.begin(), first_good_char);
        auto last_good_char = std::find_if_not(cleaned.rbegin(), cleaned.rend(),
                                               [](unsigned char c) { return std::isspace(c) || std::iscntrl(c); });
        cleaned.erase(last_good_char.base(), cleaned.end());
        return cleaned;
    }
    bool parse_template_args(const std::string& type_name, std::string& container_name, std::vector<std::string>& args)
    {
        args.clear();
        size_t open_bracket = type_name.find('<');
        size_t close_bracket = type_name.rfind('>');
        if(open_bracket == std::string::npos || close_bracket == std::string::npos || close_bracket <= open_bracket)
            return false;
        container_name = clean_type_name(type_name.substr(0, open_bracket));
        if(container_name.empty())
            return false;
        std::string args_str = type_name.substr(open_bracket + 1, close_bracket - open_bracket - 1);
        if(args_str.empty())
            return false;
        int bracket_level = 0;
        size_t current_arg_start = 0;
        for(size_t i = 0; i < args_str.length(); ++i)
        {
            if(args_str[i] == '<')
                bracket_level++;
            else if(args_str[i] == '>')
            {
                if(bracket_level > 0)
                    bracket_level--;
                else
                    return false;
            }
            else if(args_str[i] == ',' && bracket_level == 0)
            {
                std::string arg = clean_type_name(args_str.substr(current_arg_start, i - current_arg_start));
                if(arg.empty())
                    return false;
                args.push_back(arg);
                current_arg_start = i + 1;
            }
        }
        if(bracket_level != 0)
            return false;
        std::string last_arg = clean_type_name(args_str.substr(current_arg_start));
        if(last_arg.empty())
            return false;
        args.push_back(last_arg);
        return !args.empty();
    }
    std::string get_qualified_name(const entity& ent)
    {
        std::string qualified_name;
        std::vector<std::string> parts;
        const entity* current_entity_ptr = &ent;
        while(current_entity_ptr != nullptr && !current_entity_ptr->get_name().empty()
              && current_entity_ptr->get_name() != "__global__")
        {
            parts.push_back(current_entity_ptr->get_name());
            const class_entity* current_class_ptr = dynamic_cast<const class_entity*>(current_entity_ptr);
            if(current_class_ptr == nullptr)
                break;
            const class_entity* owner = current_class_ptr->get_owner();
            if(owner == nullptr || owner->get_name().empty() || owner->get_name() == "__global__"
               || (owner->get_entity_type() != entity_type::NAMESPACE && owner->get_entity_type() != entity_type::CLASS
                   && owner->get_entity_type() != entity_type::STRUCT
                   && owner->get_entity_type() != entity_type::INTERFACE))
            {
                break;
            }
            current_entity_ptr = owner;
        }
        std::reverse(parts.begin(), parts.end());
        for(size_t i = 0; i < parts.size(); ++i)
        {
            qualified_name += parts[i];
            if(i < parts.size() - 1)
                qualified_name += "_";
        }
        if(qualified_name.empty() && !ent.get_name().empty() && ent.get_name() != "__global__")
        {
            qualified_name = ent.get_name();
        }
        return qualified_name;
    }
    const class_entity* find_type_entity_upwards(const class_entity* start_scope, const std::string& type_name_cleaned)
    {
        const class_entity* current_scope = start_scope;
        while(current_scope != nullptr)
        {
            entity_type relevant_types = entity_type::NAMESPACE_MEMBERS | entity_type::STRUCTURE_MEMBERS;
            for(const auto& element_ptr : current_scope->get_elements(relevant_types))
            {
                if(!element_ptr)
                    continue;
                if(clean_type_name(element_ptr->get_name()) == type_name_cleaned)
                {
                    entity_type et = element_ptr->get_entity_type();
                    if(et == entity_type::TYPEDEF || et == entity_type::STRUCT || et == entity_type::ENUM
                       || et == entity_type::CLASS || et == entity_type::SEQUENCE)
                    {
                        const class_entity* found_direct_entity = dynamic_cast<const class_entity*>(element_ptr.get());
                        if(found_direct_entity)
                            return found_direct_entity;
                    }
                }
            }
            current_scope = current_scope->get_owner();
        }
        return nullptr;
    }
    bool find_class_in_map(const std::string& type_name_cleaned,
                           const std::map<std::string, DefinitionInfoVariant>& definition_info_map,
                           const class_entity*& found_entity)
    {
        auto it = definition_info_map.find(type_name_cleaned);
        if(it != definition_info_map.end() && std::holds_alternative<const class_entity*>(it->second))
        {
            found_entity = std::get<const class_entity*>(it->second);
            return (found_entity != nullptr);
        }
        size_t last_sep = type_name_cleaned.rfind('_');
        if(last_sep != std::string::npos)
        {
            std::string base_name = type_name_cleaned.substr(last_sep + 1);
            it = definition_info_map.find(base_name);
            if(it != definition_info_map.end() && std::holds_alternative<const class_entity*>(it->second))
            {
                found_entity = std::get<const class_entity*>(it->second);
                return (found_entity != nullptr);
            }
        }
        return false;
    }

    // Main function to write the definition for a specific NON-SYNTHETIC entity
    void write_schema_definition(const class_entity& root, const class_entity& ent, json_writer& writer,
                                 std::set<std::string>& definitions_needed, std::set<std::string>& definitions_written,
                                 const std::set<std::string>& currently_processing,
                                 const std::map<std::string, DefinitionInfoVariant>& definition_info_map)
    {
        if(ent.get_is_template())
        {
            writer.open_object();
            writer.write_string_property("type", "null");
            writer.write_string_property("description", "Note: Schema generation skipped for template definition.");
            writer.close_object();
            return;
        }
        writer.open_object();
        const attributes& definition_attribs = ent.get_attributes();
        std::string attr_description = find_attribute_value(definition_attribs, "description");
        if(has_attribute(definition_attribs, "deprecated"))
        {
            writer.write_raw_property("deprecated", "true");
        }
        switch(ent.get_entity_type())
        {
        case entity_type::STRUCT:
        case entity_type::CLASS:
        {
            if(!attr_description.empty())
            {
                writer.write_string_property("description", attr_description);
            }
            writer.write_string_property("type", "object");
            std::vector<std::string> required_fields;
            std::map<std::string, std::pair<std::string, attributes>> properties;
            for(const auto& element_ptr : ent.get_elements(entity_type::FUNCTION_VARIABLE))
            {
                if(!element_ptr || element_ptr->get_entity_type() != entity_type::FUNCTION_VARIABLE)
                    continue;
                const auto* var = dynamic_cast<const function_entity*>(element_ptr.get());
                if(!var)
                    continue;
                std::string member_name = clean_type_name(var->get_name());
                std::string raw_member_type = var->get_return_type();
                if(member_name.empty() || raw_member_type.empty())
                    continue;
                std::string cleaned_member_type = clean_type_name(raw_member_type);
                if(cleaned_member_type.empty())
                    continue;
                properties[member_name] = {cleaned_member_type, var->get_attributes()};
                if(has_attribute(var->get_attributes(), "required"))
                {
                    required_fields.push_back(member_name);
                }
            }
            if(!properties.empty())
            {
                writer.write_key("properties");
                writer.open_object();
                for(const auto& pair : properties)
                {
                    writer.write_key(pair.first);
                    map_idl_type_to_json_schema(root, &ent, pair.second.first, pair.second.second, writer,
                                                definitions_needed, definitions_written, currently_processing,
                                                definition_info_map);
                }
                writer.close_object();
            }
            else
            {
                writer.write_key("properties");
                writer.open_object();
                writer.close_object();
            }
            if(!required_fields.empty())
            {
                writer.write_key("required");
                writer.open_array();
                for(const auto& field : required_fields)
                {
                    writer.write_array_string_element(field);
                }
                writer.close_array();
            }
            writer.write_raw_property("additionalProperties", "false");
            break;
        }
        case entity_type::ENUM:
        {
            writer.write_string_property("type", "integer");
            writer.write_key("enum");
            writer.open_array();
            int next_value = 0;
            std::vector<std::string> value_descriptions;
            using underlying = std::underlying_type<entity_type>::type;
            const entity_type ALL_POSSIBLE_MEMBERS
                = static_cast<entity_type>(static_cast<underlying>(entity_type::NAMESPACE_MEMBERS)
                                           | static_cast<underlying>(entity_type::STRUCTURE_MEMBERS));
            for(const auto& element_ptr : ent.get_elements(ALL_POSSIBLE_MEMBERS))
            {
                if(!element_ptr)
                    continue;
                std::string enum_value_name = clean_type_name(element_ptr->get_name());
                if(!enum_value_name.empty() && enum_value_name.find_first_of("{}[]() \t\n\r") == std::string::npos)
                {
                    int assigned_value = next_value;
                    const function_entity* value_entity = dynamic_cast<const function_entity*>(element_ptr.get());
                    if(value_entity)
                    {
                        std::string explicit_value_str = clean_type_name(value_entity->get_return_type());
                        if(!explicit_value_str.empty())
                        {
                            try
                            {
                                assigned_value = std::stoi(explicit_value_str);
                            }
                            catch(const std::exception& e)
                            { 
                                std::cerr << "exception has occured explicit_value_str has value " << explicit_value_str << " resulting in this error: " << e.what() << "\n";
                            }
                        }
                    }
                    writer.write_array_raw_element(std::to_string(assigned_value));
                    value_descriptions.push_back(enum_value_name + " = " + std::to_string(assigned_value));
                    next_value = assigned_value + 1;
                }
            }
            writer.close_array();
            if(!value_descriptions.empty())
            {
                std::string final_description = attr_description;
                if(!final_description.empty())
                {
                    final_description += ". ";
                }
                final_description += "Possible values: ";
                for(size_t i = 0; i < value_descriptions.size(); ++i)
                {
                    final_description += value_descriptions[i];
                    if(i < value_descriptions.size() - 1)
                    {
                        final_description += "; ";
                    }
                }
                writer.write_string_property("description", final_description);
            }
            else if(!attr_description.empty())
            {
                writer.write_string_property("description", attr_description);
            }
            break;
        }
        case entity_type::SEQUENCE:
        {
            if(!attr_description.empty())
            {
                writer.write_string_property("description", attr_description);
            }
            writer.write_string_property("type", "array");
            std::string element_type = clean_type_name(ent.get_alias_name());
            if(!element_type.empty())
            {
                writer.write_key("items");
                map_idl_type_to_json_schema(root, &ent, element_type, {}, writer, definitions_needed,
                                            definitions_written, currently_processing, definition_info_map);
            }
            else
            {
                writer.write_key("items");
                writer.open_object();
                writer.write_string_property("description", "Warning: Sequence element type not determined.");
                writer.close_object();
            }
            break;
        }
        default:
        {
            if(!attr_description.empty())
            {
                writer.write_string_property("description", attr_description);
            }
            writer.write_string_property("type", "null");
            writer.write_string_property("description", "Error: Unexpected entity type in write_schema_definition: "
                                                            + std::to_string(static_cast<int>(ent.get_entity_type())));
            break;
        }
        }
        writer.close_object();
    }

    // Writes definition for synthetic _send or _receive structs
    // ** RESTORED **
    void
    write_synthetic_method_struct_definition(const class_entity& root, const SyntheticMethodInfo& info,
                                             json_writer& writer, std::set<std::string>& definitions_needed,
                                             std::set<std::string>& definitions_written,
                                             const std::set<std::string>& currently_processing,
                                             const std::map<std::string, DefinitionInfoVariant>& definition_info_map)
    {
        if(!info.interface_entity || !info.method_entity)
        {
            writer.open_object();
            writer.write_string_property("type", "null");
            writer.write_string_property("description", "Error: Invalid info for synthetic struct generation.");
            writer.close_object();
            return;
        }
        writer.open_object();
        writer.write_string_property("type", "object");
        std::string struct_type = info.is_send_struct ? "_send" : "_receive";
        std::string description = "Parameters for " + info.method_entity->get_name() + struct_type + " from interface "
                                  + info.interface_entity->get_name();
        writer.write_string_property("description", description);
        std::vector<std::string> required_fields;
        std::map<std::string, std::pair<std::string, attributes>> properties;
        for(const auto& param : info.method_entity->get_parameters())
        {
            bool is_in = has_attribute(param.get_attributes(), "in");
            bool is_out = has_attribute(param.get_attributes(), "out");
            bool implicitly_in = !is_in && !is_out;
            bool include_param = info.is_send_struct ? (is_in || implicitly_in) : is_out;
            if(include_param)
            {
                std::string param_name = clean_type_name(param.get_name());
                std::string raw_param_type = param.get_type();
                if(param_name.empty() || raw_param_type.empty())
                    continue;
                std::string cleaned_param_type = clean_type_name(raw_param_type);
                if(cleaned_param_type.empty())
                    continue;
                properties[param_name] = {cleaned_param_type, param.get_attributes()};
                if(!has_attribute(param.get_attributes(), "optional"))
                {
                    required_fields.push_back(param_name);
                }
            }
        }
        if(!properties.empty())
        {
            writer.write_key("properties");
            writer.open_object();
            for(const auto& pair : properties)
            {
                writer.write_key(pair.first);
                map_idl_type_to_json_schema(root, info.interface_entity, pair.second.first, pair.second.second, writer,
                                            definitions_needed, definitions_written, currently_processing,
                                            definition_info_map);
            }
            writer.close_object();
        }
        else
        {
            writer.write_key("properties");
            writer.open_object();
            writer.close_object();
        }
        if(!required_fields.empty())
        {
            writer.write_key("required");
            writer.open_array();
            for(const auto& field : required_fields)
            {
                writer.write_array_string_element(field);
            }
            writer.close_array();
        }
        writer.write_raw_property("additionalProperties", "false");
        writer.close_object();
    }

    // Maps an IDL type name to its JSON schema representation
    // ** RESTORED **
    void map_idl_type_to_json_schema(const class_entity& root, const class_entity* current_context,
                                     const std::string& idl_type_name_in, const attributes& attribs,
                                     json_writer& writer, std::set<std::string>& definitions_needed,
                                     std::set<std::string>& definitions_written,
                                     const std::set<std::string>& currently_processing,
                                     const std::map<std::string, DefinitionInfoVariant>& definition_info_map)
    {
        std::string idl_type_name_cleaned = clean_type_name(idl_type_name_in);
        if(idl_type_name_cleaned.empty())
        {
            writer.open_object();
            writer.write_string_property("type", "null");
            writer.write_string_property("description",
                                         "Error: Invalid or empty type name encountered ('" + idl_type_name_in + "').");
            writer.close_object();
            return;
        }
        if(is_char_star(idl_type_name_cleaned) || idl_type_name_cleaned == "char*")
        {
            writer.open_object();
            std::string description = find_attribute_value(attribs, "description");
            if(!description.empty())
                writer.write_string_property("description", description);
            if(has_attribute(attribs, "deprecated"))
                writer.write_raw_property("deprecated", "true");
            writer.write_string_property("type", "string");
            writer.close_object();
            return;
        }
        std::string ignored_modifiers;
        strip_reference_modifiers(idl_type_name_cleaned, ignored_modifiers);
        idl_type_name_cleaned = unconst(idl_type_name_cleaned);
        std::string container_name;
        std::vector<std::string> template_args;
        if(parse_template_args(idl_type_name_cleaned, container_name, template_args))
        {
            if((container_name == "std::vector" || container_name == "std::list" || container_name == "std::set"
                || container_name == "std::unordered_set" || container_name == "std::deque"
                || container_name == "std::queue" || container_name == "std::stack")
               && template_args.size() >= 1)
            {
                writer.open_object();
                writer.write_string_property("type", "array");
                std::string description = find_attribute_value(attribs, "description");
                if(!description.empty())
                    writer.write_string_property("description", description);
                if(has_attribute(attribs, "deprecated"))
                    writer.write_raw_property("deprecated", "true");
                writer.write_key("items");
                map_idl_type_to_json_schema(root, current_context, template_args[0], {}, writer, definitions_needed,
                                            definitions_written, currently_processing, definition_info_map);
                writer.close_object();
                return;
            }
            else if(container_name == "std::array" && template_args.size() == 2)
            {
                writer.open_object();
                writer.write_string_property("type", "array");
                std::string description = find_attribute_value(attribs, "description");
                if(!description.empty())
                    writer.write_string_property("description", description);
                if(has_attribute(attribs, "deprecated"))
                    writer.write_raw_property("deprecated", "true");
                try
                {
                    long long array_size = std::stoll(template_args[1]);
                    if(array_size >= 0)
                    {
                        writer.write_raw_property("minItems", std::to_string(array_size));
                        writer.write_raw_property("maxItems", std::to_string(array_size));
                    }
                }
                catch(const std::exception& e)
                {
                    std::cerr << "exception has occured std::stoll(template_args[1]) has value " << template_args[1] << " resulting in this error: " << e.what() << "\n";
                    std::string current_desc = find_attribute_value(attribs, "description");
                    std::string size_note = "[Note: Array size is non-literal: " + template_args[1] + "]";
                    writer.write_string_property("description",
                                                 current_desc.empty() ? size_note : (current_desc + " " + size_note));
                }
                writer.write_key("items");
                map_idl_type_to_json_schema(root, current_context, template_args[0], {}, writer, definitions_needed,
                                            definitions_written, currently_processing, definition_info_map);
                writer.close_object();
                return;
            }
            else if((container_name == "std::map" || container_name == "std::unordered_map")
                    && template_args.size() == 2)
            {
                writer.open_object();
                writer.write_string_property("type", "array");
                std::string description = find_attribute_value(attribs, "description");
                if(!description.empty())
                    writer.write_string_property("description", description);
                if(has_attribute(attribs, "deprecated"))
                    writer.write_raw_property("deprecated", "true");
                writer.write_key("items");
                writer.open_object();
                writer.write_string_property("type", "object");
                writer.write_key("properties");
                writer.open_object();
                writer.write_key("k");
                map_idl_type_to_json_schema(root, current_context, template_args[0], {}, writer, definitions_needed,
                                            definitions_written, currently_processing, definition_info_map);
                writer.write_key("v");
                map_idl_type_to_json_schema(root, current_context, template_args[1], {}, writer, definitions_needed,
                                            definitions_written, currently_processing, definition_info_map);
                writer.close_object();
                writer.write_key("required");
                writer.open_array();
                writer.write_array_string_element("k");
                writer.write_array_string_element("v");
                writer.close_array();
                writer.write_raw_property("additionalProperties", "false");
                writer.close_object();
                writer.close_object();
                return;
            }
        }
        const class_entity* found_entity_ptr = nullptr;
        bool found = false;
        if(current_context != nullptr)
        {
            found_entity_ptr = find_type_entity_upwards(current_context, idl_type_name_cleaned);
            if(found_entity_ptr)
            {
                found = true;
            }
        }
        if(!found)
        {
            if(find_class_in_map(idl_type_name_cleaned, definition_info_map, found_entity_ptr))
            {
                found = true;
            }
        }
        if(found && found_entity_ptr != nullptr)
        {
            const class_entity& found_entity = *found_entity_ptr;
            entity_type et = found_entity.get_entity_type();
            if(et == entity_type::TYPEDEF)
            {
                std::string underlying_type = clean_type_name(found_entity.get_alias_name());
                if(!underlying_type.empty())
                {
                    map_idl_type_to_json_schema(root, current_context, underlying_type, attribs, writer,
                                                definitions_needed, definitions_written, currently_processing,
                                                definition_info_map);
                }
                else
                {
                    writer.open_object();
                    writer.write_string_property("type", "null");
                    writer.write_string_property("description", "Error: Typedef underlying type invalid.");
                    writer.close_object();
                }
                return;
            }
            else if(et == entity_type::STRUCT || et == entity_type::ENUM || et == entity_type::CLASS
                    || et == entity_type::SEQUENCE)
            {
                std::string qualified_name = get_qualified_name(found_entity);
                if(!qualified_name.empty())
                {
                    writer.open_object();
                    std::string description = find_attribute_value(attribs, "description");
                    if(!description.empty())
                        writer.write_string_property("description", description);
                    if(has_attribute(attribs, "deprecated"))
                        writer.write_raw_property("deprecated", "true");
                    writer.write_string_property("$ref", "#/definitions/" + qualified_name);
                    writer.close_object();
                    if(definitions_written.find(qualified_name) == definitions_written.end()
                       && currently_processing.find(qualified_name) == currently_processing.end())
                    {
                        definitions_needed.insert(qualified_name);
                    }
                }
                else
                {
                    writer.open_object();
                    writer.write_string_property("type", "null");
                    writer.write_string_property("description", "Error: Failed get qualified name for $ref.");
                    writer.close_object();
                }
                return;
            }
        }
        std::string idl_type_name = idl_type_name_cleaned;
        writer.open_object();
        std::string description = find_attribute_value(attribs, "description");
        if(!description.empty())
            writer.write_string_property("description", description);
        if(has_attribute(attribs, "deprecated"))
            writer.write_raw_property("deprecated", "true");
        if(is_int8(idl_type_name) || is_uint8(idl_type_name) || is_int16(idl_type_name) || is_uint16(idl_type_name)
           || is_int32(idl_type_name) || is_uint32(idl_type_name) || is_int64(idl_type_name) || is_uint64(idl_type_name)
           || is_long(idl_type_name) || is_ulong(idl_type_name) || idl_type_name == "int" || idl_type_name == "char")
        {
            writer.write_string_property("type", "integer");
        }
        else if(is_float(idl_type_name) || is_double(idl_type_name))
        {
            writer.write_string_property("type", "number");
        }
        else if(is_bool(idl_type_name))
        {
            writer.write_string_property("type", "boolean");
        }
        else if(idl_type_name == "string" || idl_type_name == "std::string")
        {
            writer.write_string_property("type", "string");
            std::string format = find_attribute_value(attribs, "format");
            if(!format.empty())
                writer.write_string_property("format", format);
        }
        else
        {
            writer.write_string_property("type", "null");
            std::string error_msg = "Error: Could not resolve IDL type '" + idl_type_name_in + "'";
            if(current_context != nullptr)
            {
                std::string context_name = get_qualified_name(*current_context);
                if(context_name.empty())
                    context_name = current_context->get_name();
                error_msg += " used within scope '" + context_name + "'";
            }
            error_msg += " (Searched scope and global definitions). Stripped type checked: '" + idl_type_name + "'.";
            writer.write_string_property("description", error_msg);
        }
        writer.close_object();
    }

    // Finds interfaces and adds synthetic struct info, populates ordered list
    // ** RESTORED **
    void find_definable_entities(const class_entity& current_entity, std::vector<OrderedDefinitionItem>& ordered_defs)
    {
        entity_type et = current_entity.get_entity_type();
        if(current_entity.is_in_import())
            return;
        bool is_template_definition = current_entity.get_is_template();
        std::string qualified_name = get_qualified_name(current_entity);
        if(!qualified_name.empty() && !is_template_definition)
        {
            if(et == entity_type::STRUCT || et == entity_type::ENUM || et == entity_type::CLASS
               || et == entity_type::SEQUENCE)
            {
                bool found = false;
                for(const auto& item : ordered_defs)
                {
                    if(item.first == qualified_name)
                    {
                        found = true;
                        break;
                    }
                }
                if(!found)
                {
                    ordered_defs.push_back({qualified_name, &current_entity});
                }
            }
            else if(et == entity_type::INTERFACE)
            {
                for(const auto& element_ptr : current_entity.get_elements(entity_type::FUNCTION_METHOD))
                {
                    if(!element_ptr || element_ptr->get_entity_type() != entity_type::FUNCTION_METHOD)
                        continue;
                    const function_entity* method = dynamic_cast<const function_entity*>(element_ptr.get());
                    if(!method)
                        continue;
                    std::string method_name = clean_type_name(method->get_name());
                    if(method_name.empty())
                        continue;
                    std::string send_struct_name = qualified_name + "_" + method_name + "_send";
                    std::string receive_struct_name = qualified_name + "_" + method_name + "_receive";
                    ordered_defs.push_back({send_struct_name, SyntheticMethodInfo {&current_entity, method, true}});
                    ordered_defs.push_back({receive_struct_name, SyntheticMethodInfo {&current_entity, method, false}});
                }
            }
        }
        entity_type members_to_get = entity_type::TYPE_NULL;
        if(!is_template_definition)
        {
            if(et == entity_type::NAMESPACE || current_entity.get_owner() == nullptr
               || current_entity.get_name() == "__global__")
            {
                members_to_get = entity_type::NAMESPACE_MEMBERS;
            }
            else if(et == entity_type::STRUCT || et == entity_type::CLASS || et == entity_type::INTERFACE)
            {
                members_to_get = entity_type::STRUCTURE_MEMBERS | entity_type::NAMESPACE_MEMBERS;
            }
        }
        if(members_to_get != entity_type::TYPE_NULL)
        {
            for(const auto& element_ptr : current_entity.get_elements(members_to_get))
            {
                if(!element_ptr || element_ptr->is_in_import())
                    continue;
                const class_entity* child_class = dynamic_cast<const class_entity*>(element_ptr.get());
                if(!child_class)
                    continue;
                find_definable_entities(*child_class, ordered_defs);
            }
        }
    }

    // Entry point function
    // ** RESTORED **
    void write_json_schema(const class_entity& root_entity, std::ostream& os,
                           const std::string& schema_title)
    {
        json_writer writer(os);
        std::set<std::string> definitions_needed;
        std::set<std::string> definitions_written;
        std::vector<OrderedDefinitionItem> ordered_defs;
        std::map<std::string, DefinitionInfoVariant> definition_info_map;
        find_definable_entities(root_entity, ordered_defs);
        for(const auto& item : ordered_defs)
        {
            definition_info_map[item.first] = item.second;
            definitions_needed.insert(item.first);
        }
        writer.open_object();
        writer.write_string_property("$schema", "http://json-schema.org/draft-07/schema#");
        writer.write_string_property("title", schema_title);
        writer.write_key("definitions");
        writer.open_object();
        size_t processed_count = 0;
        const size_t max_processed = (definition_info_map.size()) * 3 + 20;
        std::set<std::string> currently_processing;
        while(!definitions_needed.empty() && processed_count++ < max_processed)
        {
            std::string current_name = *definitions_needed.begin();
            definitions_needed.erase(definitions_needed.begin());
            if(definitions_written.count(current_name) || currently_processing.count(current_name))
                continue;
            auto it_info = definition_info_map.find(current_name);
            if(it_info != definition_info_map.end())
            {
                currently_processing.insert(current_name);
                writer.write_key(current_name);
                std::visit(
                    [&](auto&& arg)
                    {
                        using T = std::decay_t<decltype(arg)>;
                        if constexpr(std::is_same_v<T, const class_entity*>)
                        {
                            write_schema_definition(root_entity, *arg, writer, definitions_needed, definitions_written,
                                                    currently_processing, definition_info_map);
                        }
                        else if constexpr(std::is_same_v<T, SyntheticMethodInfo>)
                        {
                            write_synthetic_method_struct_definition(root_entity, arg, writer, definitions_needed,
                                                                     definitions_written, currently_processing,
                                                                     definition_info_map);
                        }
                    },
                    it_info->second);
                currently_processing.erase(current_name);
                definitions_written.insert(current_name);
            }
            else
            {
                writer.write_key(current_name);
                writer.open_object();
                writer.write_string_property("type", "null");
                writer.write_string_property("description",
                                             "Error: Definition info not found for '" + current_name + "'.");
                writer.close_object();
                definitions_written.insert(current_name);
            }
        }
        if(processed_count >= max_processed && !definitions_needed.empty())
        {
            writer.write_key("__GENERATION_ERROR__");
            writer.open_object();
            writer.write_string_property("description", "Max processing limit reached.");
            writer.write_key("remaining_definitions");
            writer.open_array();
            for(const auto& remaining_name : definitions_needed)
            {
                writer.write_array_string_element(remaining_name);
            }
            writer.close_array();
            writer.close_object();
        }
        writer.close_object();
        writer.close_object();
        os << "\n";
    }

} // namespace json_schema_generator
