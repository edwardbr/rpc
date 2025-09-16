/*
 *   Copyright (c) 2025 Edward Boggis-Rolfe
 *   All rights reserved.
 */
#include <filesystem>
#include <fstream>
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

#include "writer.h"
#include "fingerprint_generator.h"
#include "component_checksum.h"

namespace component_checksum
{
    constexpr uint64_t latest_protocol_version = 3;

    std::string get_namespace(const class_entity* entity)
    {
        if (!entity || entity->get_name().empty())
            return "";

        std::string ns;
        auto* tmp = entity->get_owner();
        if (tmp)
            ns = get_namespace(tmp);

#ifdef WIN32
        ns += entity->get_name() + ".";
#else
        ns += entity->get_name() + "::";
#endif
        return ns;
    }

    void write_interface(const class_entity& m_ob, std::filesystem::path& output_path)
    {
        if (m_ob.is_in_import())
            return;
        auto interface_name = std::string(m_ob.get_entity_type() == entity_type::LIBRARY ? "i_" : "") + m_ob.get_name();

        auto name = get_namespace(m_ob.get_owner()) + interface_name;

        auto status = m_ob.get_value("status");
        if (status.empty())
            status = "not_specified";

        auto file = output_path / status / name;

        std::filesystem::create_directories(file.parent_path());

        std::ofstream out(file);
        out << fingerprint::generate(m_ob, {}, nullptr, latest_protocol_version);
    };

    void write_struct(const class_entity& m_ob, std::filesystem::path& output_path)
    {
        if (m_ob.is_in_import())
            return;

        auto name = get_namespace(m_ob.get_owner()) + m_ob.get_name();

        auto status = m_ob.get_value("status");
        if (status.empty())
            status = "not_specified";

        auto file = output_path / status / name;

        std::filesystem::create_directories(file.parent_path());

        std::ofstream out(file);
        out << fingerprint::generate(m_ob, {}, nullptr, latest_protocol_version);
    };

    // entry point
    void write_namespace(const class_entity& lib, std::filesystem::path& output_path)
    {
        for (auto& elem : lib.get_elements(entity_type::NAMESPACE_MEMBERS))
        {
            if (elem->is_in_import())
                continue;
            else if (elem->get_entity_type() == entity_type::NAMESPACE)
            {
                auto& ent = static_cast<const class_entity&>(*elem);
                write_namespace(ent, output_path);
            }
            else if (elem->get_entity_type() == entity_type::STRUCT)
            {
                auto& ent = static_cast<const class_entity&>(*elem);
                write_struct(ent, output_path);
            }
            else if (elem->get_entity_type() == entity_type::INTERFACE || elem->get_entity_type() == entity_type::LIBRARY)
            {
                auto& ent = static_cast<const class_entity&>(*elem);
                write_interface(ent, output_path);
            }
        }
    }
}
