#pragma once

#include "coreclasses.h" // Your parser API header
#include "cpp_parser.h"  // Your parser API header
#include "writer.h"      // The JSON writer helper
#include <iostream>
#include <string>
#include <vector>
#include <set>
#include <map>
#include <sstream>
#include <algorithm>   // For std::find_if_not, std::reverse
#include <cctype>      // For std::isspace, std::iscntrl
#include <type_traits> // For underlying_type used with ALL_POSSIBLE_MEMBERS
#include <typeinfo>    // For dynamic_cast
#include <stdexcept>   // For std::stoll exceptions

// Declare verboseStream if used globally by the generator/parser
extern std::stringstream verboseStream;

namespace json_schema_generator
{

    // Forward declarations
    void
    map_idl_type_to_json_schema(const class_entity& root,
                                const class_entity* current_context, // Added context
                                const std::string& idl_type_name, const attributes& attribs, json_writer& writer,
                                std::set<std::string>& definitions_needed,
                                std::set<std::string>& definitions_written,       // Pass written set to avoid re-adding
                                const std::set<std::string>& currently_processing // Pass processing set for cycle check
    );

    void write_schema_definition(const class_entity& root, const class_entity& ent, json_writer& writer,
                                 std::set<std::string>& definitions_needed,
                                 std::set<std::string>& definitions_written, // Pass needed sets for recursive calls
                                 const std::set<std::string>& currently_processing);

    // --- Helper functions to check attributes ---
    bool has_attribute(const attributes& attribs, const std::string& name)
    {
        for(const auto& attr : attribs)
        {
            if(attr == name)
                return true;
            if(attr.rfind(name + "=", 0) == 0)
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
                    // Handle potential quotes around attribute values
                    if(value.length() >= 2 && value.front() == '"' && value.back() == '"')
                    {
                        value = value.substr(1, value.length() - 2);
                    }
                    // Handle potential single quotes
                    else if(value.length() >= 2 && value.front() == '\'' && value.back() == '\'')
                    {
                        value = value.substr(1, value.length() - 2);
                    }
                    return value;
                }
                else
                {
                    return ""; // Attribute name found, but no value after '='
                }
            }
        }
        return ""; // Attribute name not found
    }
    // --- End Attribute Helpers ---

    // Helper to clean type names
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

    // Helper to parse template arguments
    bool parse_template_args(const std::string& type_name, std::string& container_name, std::vector<std::string>& args)
    {
        args.clear();
        size_t open_bracket = type_name.find('<');
        size_t close_bracket = type_name.rfind('>');
        if(open_bracket == std::string::npos || close_bracket == std::string::npos || close_bracket < open_bracket)
            return false;
        container_name = clean_type_name(type_name.substr(0, open_bracket));
        if(container_name.empty())
            return false;
        std::string args_str = type_name.substr(open_bracket + 1, close_bracket - open_bracket - 1);
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
                args.push_back(clean_type_name(args_str.substr(current_arg_start, i - current_arg_start)));
                current_arg_start = i + 1;
            }
        }
        if(bracket_level != 0)
            return false;
        args.push_back(clean_type_name(args_str.substr(current_arg_start)));
        if(args.empty() || (args.size() == 1 && args[0].empty()))
            return false;
        return true;
    }

    // Helper to get a qualified name suitable for JSON definitions
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
                   && owner->get_entity_type() != entity_type::STRUCT))
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

    // Helper: Tries to find a definition within a specific scope and its parents
    bool find_type_upwards(const class_entity* start_scope, const std::string& type_name_cleaned,
                           std::shared_ptr<class_entity>& found_entity_sp)
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
                        found_entity_sp = std::dynamic_pointer_cast<class_entity>(element_ptr);
                        if(found_entity_sp)
                            return true;
                    }
                }
            }
            current_scope = current_scope->get_owner();
        }
        return false;
    }

    // Main function to write the definition for a specific entity
    void write_schema_definition(const class_entity& root,
                                 const class_entity& ent, // The entity being defined
                                 json_writer& writer, std::set<std::string>& definitions_needed,
                                 std::set<std::string>& definitions_written,
                                 const std::set<std::string>& currently_processing)
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
        std::string description = find_attribute_value(definition_attribs, "description");
        if(!description.empty())
        {
            writer.write_string_property("description", description);
        }
        if(has_attribute(definition_attribs, "deprecated"))
        {
            writer.write_raw_property("deprecated", "true");
        }

        switch(ent.get_entity_type())
        {
        case entity_type::STRUCT:
        case entity_type::CLASS:
        {
            writer.write_string_property("type", "object");
            std::vector<std::string> required_fields;
            std::map<std::string, std::pair<std::string, attributes>> properties;
            for(const auto& element_ptr : ent.get_elements(entity_type::FUNCTION_VARIABLE))
            {
                if(!element_ptr)
                    continue;
                if(element_ptr->get_entity_type() == entity_type::FUNCTION_VARIABLE)
                {
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
            }
            if(!properties.empty())
            {
                writer.write_key("properties");
                writer.open_object();
                for(const auto& pair : properties)
                {
                    writer.write_key(pair.first);
                    map_idl_type_to_json_schema(root, &ent, pair.second.first, pair.second.second, writer,
                                                definitions_needed, definitions_written, currently_processing);
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
            writer.write_string_property("type", "string");
            writer.write_key("enum");
            writer.open_array();
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
                    writer.write_array_string_element(enum_value_name);
                }
            }
            writer.close_array();
            break;
        }
        case entity_type::SEQUENCE:
        {
            writer.write_string_property("type", "array");
            std::string element_type = clean_type_name(ent.get_alias_name());
            if(!element_type.empty())
            {
                writer.write_key("items");
                map_idl_type_to_json_schema(root, &ent, element_type, {}, writer, definitions_needed,
                                            definitions_written, currently_processing);
            }
            else
            { /* handle missing element type */
            }
            break;
        }
        default:
            writer.write_string_property("type", "null");
            writer.write_string_property("description", "Error: Unexpected entity type in write_schema_definition: "
                                                            + std::to_string(static_cast<int>(ent.get_entity_type())));
            break;
        }
        writer.close_object();
    }

    // Maps an IDL type name to its JSON schema representation
    void map_idl_type_to_json_schema(const class_entity& root,
                                     const class_entity* current_context, // Context where the type name is used
                                     const std::string& idl_type_name_in,
                                     const attributes& attribs, // Attributes from the usage site
                                     json_writer& writer, std::set<std::string>& definitions_needed,
                                     std::set<std::string>& definitions_written,
                                     const std::set<std::string>& currently_processing)
    {
        std::string idl_type_name_cleaned = clean_type_name(idl_type_name_in);
        if(idl_type_name_cleaned.empty())
        { /* handle error */
            return;
        }

        // --- 1. Check for standard containers ---
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
                                            definitions_written, currently_processing);
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
                    std::string current_desc = find_attribute_value(attribs, "description");
                    std::string size_note = "[Note: Array size is non-literal: " + template_args[1] + "]";
                    writer.write_string_property("description",
                                                 current_desc.empty() ? size_note : (current_desc + " " + size_note));
                }
                writer.write_key("items");
                map_idl_type_to_json_schema(root, current_context, template_args[0], {}, writer, definitions_needed,
                                            definitions_written, currently_processing);
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
                                            definitions_written, currently_processing);
                writer.write_key("v");
                map_idl_type_to_json_schema(root, current_context, template_args[1], {}, writer, definitions_needed,
                                            definitions_written, currently_processing);
                writer.close_object(); // properties
                writer.write_key("required");
                writer.open_array();
                writer.write_array_string_element("k");
                writer.write_array_string_element("v");
                writer.close_array();
                writer.write_raw_property("additionalProperties", "false");
                writer.close_object(); // items
                writer.close_object();
                return;
            }
        }

        // --- 2. If not a container, try resolving type (upward search + global) ---
        std::shared_ptr<class_entity> found_entity_sp;
        bool found = false;
        if(current_context != nullptr)
        {
            found = find_type_upwards(current_context, idl_type_name_cleaned, found_entity_sp);
        }
        if(!found)
        {
            if(root.find_class(idl_type_name_cleaned, found_entity_sp) && found_entity_sp != nullptr)
            {
                found = true;
            }
        }

        // --- 3. Process found entity or fallback to basic types / error ---
        if(found && found_entity_sp != nullptr)
        {
            const class_entity& found_entity = *found_entity_sp;
            entity_type et = found_entity.get_entity_type();
            if(et == entity_type::TYPEDEF)
            {
                std::string underlying_type = clean_type_name(found_entity.get_alias_name());
                if(!underlying_type.empty())
                {
                    map_idl_type_to_json_schema(root, current_context, underlying_type, attribs, writer,
                                                definitions_needed, definitions_written, currently_processing);
                }
                else
                { /* Error */
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
                { /* Error */
                    writer.open_object();
                    writer.write_string_property("type", "null");
                    writer.write_string_property("description", "Error: Failed get qualified name for $ref.");
                    writer.close_object();
                }
                return;
            }
            // If found type isn't one we handle (e.g. INTERFACE), fall through
        }

        // --- 4. If not found or not handled complex, check basic types ---
        std::string idl_type_name = idl_type_name_cleaned;
        std::string ref_mods;
        strip_reference_modifiers(idl_type_name, ref_mods); // Modifies idl_type_name
        idl_type_name = unconst(idl_type_name);             // Returns new string

        writer.open_object();
        std::string description = find_attribute_value(attribs, "description");
        if(!description.empty())
            writer.write_string_property("description", description);
        if(has_attribute(attribs, "deprecated"))
            writer.write_raw_property("deprecated", "true");

        // ** RESTORED Explicit Checks **
        if(is_int8(idl_type_name) || is_uint8(idl_type_name) || is_int16(idl_type_name) || is_uint16(idl_type_name)
           || is_int32(idl_type_name) || is_uint32(idl_type_name) || is_int64(idl_type_name) || is_uint64(idl_type_name)
           || is_long(idl_type_name) || is_ulong(idl_type_name) || idl_type_name == "int") // Restored plain int check
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
        else if(idl_type_name == "string" || idl_type_name == "std::string" || is_char_star(idl_type_name))
        {
            writer.write_string_property("type", "string");
            std::string format = find_attribute_value(attribs, "format");
            if(!format.empty())
                writer.write_string_property("format", format);
        }
        else
        {
            // --- 5. Final fallback: Unknown type ---
            writer.write_string_property("type", "null");
            std::string error_msg = "Error: Could not resolve IDL type '" + idl_type_name_cleaned + "'";
            if(current_context != nullptr)
            {
                std::string context_name = get_qualified_name(*current_context);
                if(context_name.empty())
                    context_name = current_context->get_name();
                error_msg += " used within scope '" + context_name + "'";
            }
            error_msg += " (Searched scope and global definitions).";
            writer.write_string_property("description", error_msg);
        }
        writer.close_object();
    }

    // ** MODIFIED: Skips template definitions **
    void find_definable_entities(const class_entity& current_entity,
                                 std::map<std::string, const class_entity*>& definables)
    {
        entity_type et = current_entity.get_entity_type();
        if(current_entity.is_in_import())
            return;
        bool is_template_definition = current_entity.get_is_template();
        std::string qualified_name = get_qualified_name(current_entity);
        if(!qualified_name.empty())
        {
            if((et == entity_type::STRUCT || et == entity_type::ENUM || et == entity_type::CLASS
                || et == entity_type::SEQUENCE)
               && !is_template_definition)
            {
                if(definables.find(qualified_name) == definables.end())
                {
                    definables[qualified_name] = &current_entity;
                }
            }
            else if(is_template_definition)
            { /* verboseStream << "Skipping template definition: " << qualified_name << std::endl; */
            }
        }
        entity_type members_to_get = entity_type::TYPE_NULL;
        if(!is_template_definition)
        { // Don't recurse into template members
            if(et == entity_type::NAMESPACE || current_entity.get_owner() == nullptr
               || current_entity.get_name() == "__global__")
            {
                members_to_get = entity_type::NAMESPACE_MEMBERS;
            }
            else if(et == entity_type::STRUCT || et == entity_type::CLASS)
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
                const entity* element = element_ptr.get();
                entity_type child_et = element->get_entity_type();
                const class_entity* child_class = dynamic_cast<const class_entity*>(element);
                if(!child_class)
                    continue;
                bool child_is_template = child_class->get_is_template();
                if((child_et == entity_type::STRUCT || child_et == entity_type::ENUM || child_et == entity_type::CLASS
                    || child_et == entity_type::SEQUENCE)
                   && !child_is_template)
                {
                    std::string child_qualified_name = get_qualified_name(*child_class);
                    if(!child_qualified_name.empty() && definables.find(child_qualified_name) == definables.end())
                    {
                        definables[child_qualified_name] = child_class;
                    }
                }
                if((child_et == entity_type::NAMESPACE || child_et == entity_type::STRUCT
                    || child_et == entity_type::CLASS)
                   && !child_is_template)
                {
                    find_definable_entities(*child_class, definables);
                }
            }
        }
    }

    // Entry point function
    void write_json_schema(const class_entity& root_entity, std::ostream& os,
                           const std::string& schema_title = "Generated Schema")
    {
        json_writer writer(os);
        std::set<std::string> definitions_needed;
        std::set<std::string> definitions_written;
        std::map<std::string, const class_entity*> definable_entities;
        find_definable_entities(root_entity, definable_entities);
        for(const auto& pair : definable_entities)
        {
            definitions_needed.insert(pair.first);
        }
        writer.open_object();
        writer.write_string_property("$schema", "http://json-schema.org/draft-07/schema#");
        writer.write_string_property("title", schema_title);
        writer.write_key("definitions");
        writer.open_object();
        int processed_count = 0;
        const int max_processed = definable_entities.size() * 3 + 20;
        std::set<std::string> currently_processing;
        while(!definitions_needed.empty() && processed_count++ < max_processed)
        {
            std::string current_name = *definitions_needed.begin();
            definitions_needed.erase(definitions_needed.begin());
            if(definitions_written.count(current_name))
                continue;
            if(currently_processing.count(current_name))
                continue;
            auto it = definable_entities.find(current_name);
            if(it != definable_entities.end() && it->second != nullptr)
            {
                currently_processing.insert(current_name);
                writer.write_key(current_name);
                write_schema_definition(root_entity, *(it->second), writer, definitions_needed, definitions_written,
                                        currently_processing);
                currently_processing.erase(current_name);
                definitions_written.insert(current_name);
            }
            else
            { /* handle definition not found error */
            }
        }
        if(processed_count >= max_processed && !definitions_needed.empty())
        { /* handle max processing error */
        }
        writer.close_object(); // Close definitions
        writer.close_object(); // Close main schema object
        os << std::endl;
    }

} // namespace json_schema_generator
