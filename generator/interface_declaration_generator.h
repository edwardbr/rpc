#include "coreclasses.h"
#include "cpp_parser.h"
#include "writer.h"

namespace rpc_generator
{
    void build_scoped_name(const class_entity* entity, std::string& name);

    void write_constexpr(writer& header, const entity& constexpression);

    std::string write_proxy_send_declaration(const class_entity& m_ob, const std::string& interface_name,
                                      const std::shared_ptr<function_entity>& function, int function_count,
                                      bool& has_inparams);

    std::string write_proxy_receive_declaration(const class_entity& m_ob, const std::string& interface_name,
                                         const std::shared_ptr<function_entity>& function, int& function_count,
                                         bool& has_inparams);

    std::string write_stub_receive_declaration(const class_entity& m_ob, const std::string& interface_name,
                                        const std::shared_ptr<function_entity>& function, int function_count,
                                        bool& has_outparams);

    std::string write_stub_reply_declaration(const class_entity& m_ob, const std::string& interface_name,
                                      const std::shared_ptr<function_entity>& function, int function_count,
                                      bool& has_outparams);

    // void write_send_method(writer& header, const std::shared_ptr<function_entity>& function);

    // void write_receive_method(writer& header, const std::shared_ptr<function_entity>& function);

    void write_method(const class_entity& m_ob, writer& header, const std::string& interface_name,
                      const std::shared_ptr<function_entity>& function, int& function_count);

    void write_interface(const class_entity& m_ob, writer& header);
}