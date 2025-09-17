/*
 *   Copyright (c) 2025 Edward Boggis-Rolfe
 *   All rights reserved.
 */

#include "coreclasses.h"
#include "cpp_parser.h"
#include "writer.h"

namespace interface_declaration_generator
{
    void build_scoped_name(const class_entity* entity, std::string& name);

    void write_constexpr(writer& header, const entity& constexpression);

    std::string write_proxy_send_declaration(const class_entity& m_ob,
        const std::string& scope,
        const std::shared_ptr<function_entity>& function,
        bool& has_inparams,
        std::string additional_params,
        bool include_variadics);

    std::string write_proxy_receive_declaration(const class_entity& m_ob,
        const std::string& scope,
        const std::shared_ptr<function_entity>& function,
        bool& has_inparams,
        std::string additional_params,
        bool include_variadics);

    std::string write_stub_receive_declaration(const class_entity& m_ob,
        const std::string& scope,
        const std::shared_ptr<function_entity>& function,
        bool& has_outparams,
        std::string additional_params,
        bool include_variadics);

    std::string write_stub_reply_declaration(const class_entity& m_ob,
        const std::string& scope,
        const std::shared_ptr<function_entity>& function,
        bool& has_outparams,
        std::string additional_params,
        bool include_variadics);

    void write_method(const class_entity& m_ob,
        writer& header,
        const std::string& interface_name,
        const std::shared_ptr<function_entity>& function,
        int& function_count);

    void write_interface(const class_entity& m_ob, writer& header);
}
