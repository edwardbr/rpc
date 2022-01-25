#include <type_traits>
#include <algorithm>
#include "coreclasses.h"
#include "cpp_parser.h"

#include "writer.h"

#include "in_zone_synchronous_generator.h"

namespace enclave_marshaller
{
    namespace in_zone_synchronous_generator
    {
        void write_interface_predeclaration(const Library& lib, const ClassObject& m_ob, writer& header, writer& proxy, writer& stub)
        {
            proxy("class {}_proxy;", 
                m_ob.name);
            stub("class {}_stub;", 
                m_ob.name);
        };

        bool is_in_call(
            bool from_host, 
            const Library& lib, 
            const std::string& name, 
            const std::string& type, 
            const std::list<std::string>& attributes, 
            bool& is_interface, 
            std::string& referenceModifiers,
            std::string& proxy_marshall_as, 
            std::string& stub_demarshall_declaration, 
            std::string& proxy_mapping, 
            std::string& stub_param_cast)
        {
            proxy_mapping = name;
            auto in = std::find(attributes.begin(), attributes.end(), "in") != attributes.end();
            auto out = std::find(attributes.begin(), attributes.end(), "out") != attributes.end();
            auto byval = std::find(attributes.begin(), attributes.end(), "by_value") != attributes.end();

            if(out && !in)
                return false;

            std::string type_name = type;
            stripReferenceModifiers(type_name, referenceModifiers);

            is_interface = false;
            for(auto it = lib.m_classes.begin(); it != lib.m_classes.end(); it++)
            {
                if((*it)->name == type_name)
                {
                    is_interface = true;
                    break;
                }
            }

            if(referenceModifiers.empty())
            {
                proxy_marshall_as = name;
                stub_demarshall_declaration = fmt::format("{} {}", type, name);
                stub_param_cast = name;
            }
            else if(referenceModifiers == "*&")
            {
                proxy_marshall_as = name;
                stub_demarshall_declaration = fmt::format("uint64_t {}", type_name, name);
                stub_param_cast = fmt::format("({}*){}", type_name, name);
            }
            else if(referenceModifiers == "*")
            {
                proxy_marshall_as = fmt::format("(uint64_t) {}", name);
                stub_demarshall_declaration = fmt::format("uint64_t {}", name);
                stub_param_cast = fmt::format("({}*){}", type_name, name);
            }
            else if(referenceModifiers == "&")
            {
                if(byval)
                {
                    proxy_marshall_as = name;
                    stub_demarshall_declaration = fmt::format("{} {}", type_name, name);
                    stub_param_cast = name;
                }
                else if(from_host == false)
                {
                    std::cerr << "passing data by reference from a non host zone is not allowed\n";
                    throw "passing data by reference from a non host zone is not allowed\n";
                }
                else
                {
                    proxy_marshall_as = fmt::format("(uint64_t)&{}", name);
                    stub_demarshall_declaration = fmt::format("uint64_t {} = 0;", name);
                    proxy_mapping = fmt::format("{}", name);
                    stub_param_cast = fmt::format("*({}*){}", type_name, name);
                }
            }
            else
            {
                std::cerr << fmt::format("passing data by {} as in {} {} is not supported", referenceModifiers, type, name);
                throw fmt::format("passing data by {} as in {} {} is not supported", referenceModifiers, type, name);
            }
            return true;
        }

        bool is_out_call(
            bool from_host, 
            const Library& lib, 
            const std::string& name, 
            const std::string& type, 
            const std::list<std::string>& attributes, 
            bool& is_interface, 
            std::string& referenceModifiers,
            std::string& proxy_marshall_as, 
            std::string& proxy_out_declaration,
            std::string& proxy_out_cast,
            std::string& stub_demarshall_declaration, 
            std::string& stub_param_cast, 
            std::string& stub_out_serialise_cast,
            std::string& stub_value_return)
        {
            proxy_out_declaration.clear();

            auto in = std::find(attributes.begin(), attributes.end(), "in") != attributes.end();
            auto out = std::find(attributes.begin(), attributes.end(), "out") != attributes.end();
            auto by_pointer = std::find(attributes.begin(), attributes.end(), "by_pointer") != attributes.end();
            auto byval = std::find(attributes.begin(), attributes.end(), "by_value") != attributes.end();

            if(!out)
                return false;

            if(by_pointer && byval)
            {
                std::cerr << fmt::format("specifying by_pointer and by_value is not supported");
                throw fmt::format("specifying by_pointer and by_value is not supported");
            }
            if(!byval)
                by_pointer = true;

            std::string type_name = type;
            referenceModifiers.clear();
            stripReferenceModifiers(type_name, referenceModifiers);

            if(referenceModifiers.empty())
            {
                std::cerr << fmt::format("out parameters require data to be sent by pointer {} {} is not supported", type, name);
                throw fmt::format("out parameters require data to be sent by pointer {} {} is not supported", type, name);
            }

            is_interface = false;
            for(auto it = lib.m_classes.begin(); it != lib.m_classes.end(); it++)
            {
                if((*it)->name == type_name)
                {
                    is_interface = true;
                    break;
                }
            }

            if(referenceModifiers == "&") //implicitly by value
            {
                proxy_marshall_as = name;
                proxy_out_cast = name;
                stub_demarshall_declaration = fmt::format("{} {}", type_name, name);
                stub_param_cast = fmt::format("{}", name);
                stub_out_serialise_cast = name;
/*                std::cerr << fmt::format("out reference parameters are not supported {} {}", type, name);
                throw fmt::format("out reference parameters are not supported {} {} is not supported", type, name);*/
            }
            else if(byval && referenceModifiers == "*")
            {
                proxy_marshall_as = fmt::format("*{}", name);
                stub_demarshall_declaration = fmt::format("uint64_t {}", name);
                stub_param_cast = fmt::format("({}*){}", type_name, name);
                stub_out_serialise_cast = name;
            }

            else if(referenceModifiers == "*")
            {
                std::cerr << "passing [out] by_pointer data by * will not work use a ** or *&\n";
                throw "passing [out] by_pointer data by * will not work use a ** or *&\n";
            }


            //by pointer
            else if(by_pointer && referenceModifiers == "**")
            {
                proxy_marshall_as = fmt::format("{}_", name);
                proxy_out_declaration = fmt::format("uint64_t {}_;", name);
                stub_demarshall_declaration = fmt::format("{}* {} = nullptr", type_name, name);
                stub_param_cast = fmt::format("&{}", name);
                stub_out_serialise_cast = fmt::format("(uint64_t) {}", name);
                stub_value_return = fmt::format("*{} = ({}*){}_;", name, type_name, name);
            }
            else if(by_pointer && referenceModifiers == "*&")
            {
                proxy_marshall_as = fmt::format("{}_", name);
                proxy_out_declaration = fmt::format("uint64_t {}_;", name);
                stub_demarshall_declaration = fmt::format("uint64_t {}", name);
                stub_param_cast = fmt::format("({}*&){}", type_name, name);
                stub_out_serialise_cast = name;                
                stub_value_return = fmt::format("{} = ({}*){}_;", name, type_name, name);
            }
            else
            {
                std::cerr << fmt::format("passing data by {} as out parameter {} {} is not supported", referenceModifiers, type, name);
                throw fmt::format("passing data by {} as out parameter {} {} is not supported", referenceModifiers, type, name);
            }
            return true;
        }

        void write_interface(bool from_host, const Library& lib, const ClassObject& m_ob, writer& header, writer& proxy, writer& stub, int id)
        {
            auto interface_name = std::string(m_ob.type == ObjectLibrary ? "i_" : "") + m_ob.name;
            header("class {}{}{} : public i_unknown", 
                interface_name, 
                m_ob.parentName.empty() ? "" : ":",
                m_ob.parentName);
			header("{{");
			header("public:");
			header("static constexpr uint64_t id = {};", id);

            proxy("class {}_proxy : public {}", 
                interface_name, 
                interface_name);
			proxy("{{");
            proxy("i_marshaller& marshaller_;");
            proxy("uint64_t object_id_;");
			proxy("public:");
			proxy("");
            proxy("{}_proxy(i_marshaller& stub, uint64_t object_id) : ", interface_name);
            proxy("  marshaller_(stub),", interface_name);
            proxy("  object_id_(object_id)", interface_name);
            proxy("  {{}}", interface_name);
			proxy("");


            stub("class {}_stub : public i_marshaller_impl", interface_name);
			stub("{{");
            stub("remote_shared_ptr<{}> target_;", interface_name);
			stub("public:");
			stub("");
            stub("{}_stub(remote_shared_ptr<{}>& target) : ", interface_name, interface_name);
            stub("  target_(target)", interface_name);
            stub("  {{}}");
			stub("");
            stub("error_code send(uint64_t object_id, uint64_t interface_id, uint64_t method_id, size_t in_size_, const char* in_buf_, size_t out_size_, char* out_buf_) override");
            stub("{{");

            stub("switch(method_id)");
            stub("{{");

            int function_count = 1;
            for(auto& function : m_ob.functions)
            {
                if(function.type != FunctionTypeMethod)
                    continue;
                
                stub("case {}:", function_count);
                stub("{{");

                header.print_tabs();
                proxy.print_tabs();
                header.raw("virtual {} {}(", function.returnType, function.name);
                proxy.raw("virtual {} {}_proxy::{} (", function.returnType, interface_name, function.name);
                bool has_parameter = false;
                for(auto& parameter : function.parameters)
                {
                    if(has_parameter)
                    {
                        header.raw(", ");
                        proxy.raw(", ");
                    }
                    has_parameter = true;
                    std::string modifier;
                    for(auto it = parameter.m_attributes.begin(); it != parameter.m_attributes.end(); it++)
                    {
                        if(*it == "const")
                            modifier = "const " + modifier;
                    }
                    header.raw("{}{} {}",modifier, parameter.type, parameter.name);
                    proxy.raw("{}{} {}",modifier, parameter.type, parameter.name);
                }
                header.raw(") = 0;\n");
                proxy.raw(") override\n");
                proxy("{{");

                bool has_inparams = false;

                stub("//stub_demarshall_declaration");
                for(auto& parameter : function.parameters)
                {
                    bool is_interface = false;
                    std::string referenceModifiers;
                    std::string proxy_marshall_as;
                    std::string proxy_mapping;
                    std::string proxy_out_declaration;
                    std::string proxy_out_cast;
                    std::string stub_demarshall_declaration;
                    std::string stub_param_cast;
                    std::string stub_out_serialise_cast;
                    std::string stub_value_return;
                    if(is_in_call(from_host, lib, parameter.name, parameter.type, parameter.m_attributes, is_interface, referenceModifiers, proxy_marshall_as, stub_demarshall_declaration, proxy_mapping, stub_param_cast))
                        has_inparams = true;
                    else
                        is_out_call(from_host, lib, parameter.name, parameter.type, parameter.m_attributes, is_interface, referenceModifiers, proxy_marshall_as, proxy_out_declaration, proxy_out_cast, stub_demarshall_declaration, stub_param_cast, stub_out_serialise_cast, stub_value_return);
                    stub("{};", stub_demarshall_declaration);
                }

                if(has_inparams)
                {
                    proxy("//proxy_marshall_as");
                    proxy("const auto in_ = yas::save<yas::mem|yas::binary>(YAS_OBJECT_NVP(");
                    proxy("  \"in\"");

                    stub("//parameter.name");
                    stub("yas::intrusive_buffer in(in_buf_, in_size_);");
                    stub("yas::load<yas::mem|yas::binary>(in, YAS_OBJECT_NVP(");
                    stub("  \"in\"");

                    int count = 0;
                    for(auto& parameter : function.parameters)
                    {
                        bool is_interface = false;
                        std::string referenceModifiers;
                        std::string proxy_marshall_as;
                        std::string proxy_mapping;
                        std::string proxy_out_declaration;
                        std::string proxy_out_cast;
                        std::string stub_demarshall_declaration;
                        std::string stub_param_cast;
                        if(!is_in_call(from_host, lib, parameter.name, parameter.type, parameter.m_attributes, is_interface, referenceModifiers, proxy_marshall_as, stub_demarshall_declaration, proxy_mapping, stub_param_cast))
                            continue;

                        proxy("  ,(\"_{}\", {})", count, proxy_marshall_as);
                        stub("  ,(\"_{}\", {})", count, proxy_mapping);
                        count++;
                    }
                    
                    proxy("  ));");
                    stub("  ));");
                }
                else
                {
                    proxy("const yas::shared_buffer in_;");
                }

                //proxy("for(int i = 0;i < in_.size;i++){{std::cout << in_.data.get() + i;}} std::cout << \"\\n\";");
                
                proxy("char out_buf[10000];");
                proxy("int ret = marshaller_.send(object_id_, {}::id, {}, in_.size, in_.data.get(), 10000, out_buf);", interface_name, function_count);
                proxy("if(ret)");
                proxy("{{");
                proxy("return ret;");
                proxy("}}");

                stub("//stub_param_cast");
                stub.print_tabs();
                stub.raw("error_code ret = target_->{}(", function.name);

                {
                    bool has_param = false;
                    for(auto& parameter : function.parameters)
                    {
                        bool is_interface = false;
                        std::string referenceModifiers;
                        std::string proxy_marshall_as;
                        std::string proxy_mapping;
                        std::string proxy_out_declaration;
                        std::string proxy_out_cast;
                        std::string stub_demarshall_declaration;
                        std::string stub_param_cast;
                        std::string stub_out_serialise_cast;
                        std::string stub_value_return;
                        if(!is_in_call(from_host, lib, parameter.name, parameter.type, parameter.m_attributes, is_interface, referenceModifiers, proxy_marshall_as, stub_demarshall_declaration, proxy_mapping, stub_param_cast))                        
                            is_out_call(from_host, lib, parameter.name, parameter.type, parameter.m_attributes, is_interface, referenceModifiers, proxy_marshall_as, proxy_out_declaration, proxy_out_cast, stub_demarshall_declaration, stub_param_cast, stub_out_serialise_cast, stub_value_return);
                        if(has_param)
                        {
                            stub.raw(",");
                        }
                        has_param = true;
                        stub.raw("{}", stub_param_cast);
                    }
                }
                stub.raw(");\n");
                stub("if(ret)");
                stub("  return ret;");
                stub("");

                int count = 0;

                proxy("//parameter.name");
                for(auto& parameter : function.parameters)
                {
                    count++;
                    bool is_interface = false;
                    std::string referenceModifiers;
                    std::string proxy_marshall_as;
                    std::string proxy_mapping;
                    std::string proxy_out_declaration;
                    std::string proxy_out_cast;
                    std::string stub_demarshall_declaration;
                    std::string stub_param_cast;
                    std::string stub_out_serialise_cast;
                    std::string stub_value_return;
                    if(is_in_call(from_host, lib, parameter.name, parameter.type, parameter.m_attributes, is_interface, referenceModifiers, proxy_marshall_as, stub_demarshall_declaration, proxy_mapping, stub_param_cast))
                        continue;
                    if(!is_out_call(from_host, lib, parameter.name, parameter.type, parameter.m_attributes, is_interface, referenceModifiers, proxy_marshall_as, proxy_out_declaration, proxy_out_cast, stub_demarshall_declaration, stub_param_cast, stub_out_serialise_cast, stub_value_return))
                        continue;

                    proxy(proxy_out_declaration);
                }

                proxy("//proxy_marshall_as");
                proxy("yas::load<yas::mem|yas::binary>(yas::intrusive_buffer{{out_buf, 10000}}, YAS_OBJECT_NVP(");
                proxy("  \"out\"");
                proxy("  ,(\"_{}\", ret)", count);

                stub("//stub_out_serialise_cast");
                stub("yas::mem_ostream os(out_buf_, out_size_);");
                stub("yas::save<yas::mem|yas::binary>(os, YAS_OBJECT_NVP(");
                stub("  \"out\"");
                stub("  ,(\"_{}\", ret)", count);

                for(auto& parameter : function.parameters)
                {
                    count++;
                    bool is_interface = false;
                    std::string referenceModifiers;
                    std::string proxy_marshall_as;
                    std::string proxy_mapping;
                    std::string proxy_out_declaration;
                    std::string proxy_out_cast;
                    std::string stub_demarshall_declaration;
                    std::string stub_param_cast;
                    std::string stub_out_serialise_cast;
                    std::string stub_value_return;
                    if(!is_out_call(from_host, lib, parameter.name, parameter.type, parameter.m_attributes, is_interface, referenceModifiers, proxy_marshall_as, proxy_out_declaration, proxy_out_cast, stub_demarshall_declaration, stub_param_cast, stub_out_serialise_cast, stub_value_return))
                        continue;

                    proxy("  ,(\"_{}\", {})", count, proxy_marshall_as);
                    stub("  ,(\"_{}\", {})", count, stub_out_serialise_cast);
                }
                proxy("  ));");


                proxy("//stub_param_cast");
                for(auto& parameter : function.parameters)
                {
                    count++;
                    bool is_interface = false;
                    std::string referenceModifiers;
                    std::string proxy_marshall_as;
                    std::string proxy_mapping;
                    std::string proxy_out_declaration;
                    std::string proxy_out_cast;
                    std::string stub_demarshall_declaration;
                    std::string stub_param_cast;
                    std::string stub_out_serialise_cast;
                    std::string stub_value_return;
                    if(is_in_call(from_host, lib, parameter.name, parameter.type, parameter.m_attributes, is_interface, referenceModifiers, proxy_marshall_as, stub_demarshall_declaration, proxy_mapping, stub_param_cast))
                        continue;
                    if(!is_out_call(from_host, lib, parameter.name, parameter.type, parameter.m_attributes, is_interface, referenceModifiers, proxy_marshall_as, proxy_out_declaration, proxy_out_cast, stub_demarshall_declaration, stub_param_cast, stub_out_serialise_cast, stub_value_return))
                        continue;

                    proxy(stub_value_return);
                }

                proxy("return ret;");
                proxy("}}");
                proxy("");
                
                stub("  ));");
                stub("return ret;");

                function_count++;
                stub("}}");
                stub("break;");
            }
            
            stub("}};");

            header("}};");            
            header("");

            proxy("}};");            
            proxy("");

            stub("return 0;"); 
            stub("}}");
            stub("}};");                
            stub("");
        };

        void write_struct(const ClassObject& m_ob, writer& header)
        {
            header("struct {}{}{}", 
                m_ob.name, 
                m_ob.parentName.empty() ? "" : ":",
                m_ob.parentName);
			header("{{");

            for(auto& field : m_ob.functions)
            {
                if(field.type != FunctionTypeVariable)
                    continue;
                
                header.print_tabs();
                header.raw("{} {};\n", field.returnType, field.name);
            }

            header("");
            header("// one member-function for save/load");
            header("template<typename Ar>");
            header("void serialize(Ar &ar)");
            header("{{");
            header("ar & YAS_OBJECT(\"{}\"",m_ob.name);

            int count = 0;
            for(auto& field : m_ob.functions)
            {
                header("  ,(\"_{}\", {})", count++, field.name);
            }
            header(");");

            header("}}");
			

            header("}};");
        };

        void write_library(bool from_host, const Library& lib, const ClassObject& m_ob, writer& header, writer& proxy, writer& stub)
        {
            for(auto& name : m_ob.m_ownedClasses)
            {
                const ClassObject* obj = nullptr;
                if(!lib.FindClassObject(name, obj))
                {
                    continue;
                }
                if(obj->type == ObjectTypeInterface)
                    write_interface_predeclaration(lib, *obj, header, proxy, stub);
            }
            
            proxy("");

            {  
                int id = 1;
                for(auto& name : m_ob.m_ownedClasses)
                {
                    const ClassObject* obj = nullptr;
                    if(!lib.FindClassObject(name, obj))
                    {
                        continue;
                    }
                    if(obj->type == ObjectTypeInterface)
                        write_interface(from_host, lib, *obj, header, proxy, stub, id++);
                }
            }

            write_interface(from_host, lib, m_ob, header, proxy, stub, 0);

            /*header("//a marshalable interface for other zones");
            header("class i_{} : public i_zone", m_ob.name);
			header("{{");
            header("public:");
            header("static constexpr uint64_t id = 0;");
            header("");


            header("//polymorphic helper functions");
            for(auto& name : m_ob.m_ownedClasses)
            {
                const ClassObject* obj = nullptr;
                if(!lib.FindClassObject(name, obj))
                {
                    continue;
                }
                if(obj->type == ObjectTypeInterface)
                {
                    header("virtual error_code query_interface(i_unknown& from, remote_shared_ptr<{0}>& to) = 0;", obj->name);
                }
            }	

            header("");
            header("//static functions passed to global functions in the target zone");
            for(auto& function : m_ob.functions)
            {
                if(function.type != FunctionTypeMethod)
                    continue;
                
                header.print_tabs();
                header.raw("virtual {} {}(", function.returnType, function.name);
                bool has_parameter = false;
                for(auto& parameter : function.parameters)
                {
                    if(has_parameter)
                    {
                        header.raw(", ");
                    }
                    has_parameter = true;
                    header.raw("{} {}",parameter.type, parameter.name);
                }
                header.raw(") = 0;\n");
            }
            
            header("}};");
            header("");*/
            
            
            proxy("#ifndef _IN_ENCLAVE");
            proxy("//the class that encapsulates an environment or zone");
            proxy("//only host code can use this class directly other enclaves *may* have access to the i_zone derived interface");
            proxy("class {} : public i_marshaller_impl, public i_{}_proxy", m_ob.name, m_ob.name);
			proxy("{{");
            proxy("zone_config config = {{}};");
            proxy("std::string filename_;");

            proxy("public:");

            proxy("{}(std::string filename) : i_{}_proxy(*this, 0), filename_(filename){{}}", m_ob.name, m_ob.name, m_ob.name);
            proxy("~{}()", m_ob.name, m_ob.name);
            proxy("{{");
            proxy("enclave_marshal_test_destroy(eid_);");
            proxy("sgx_destroy_enclave(eid_);");
            proxy("}}");
            proxy("error_code load()", m_ob.name);
            proxy("{{");
            proxy("sgx_launch_token_t token = {{ 0 }};");
            proxy("int updated = 0;");
            proxy("sgx_status_t status = sgx_create_enclavea(filename_.data(), 1, &token, &updated, &eid_, NULL);");
            proxy("if(status)");
            proxy("  return -1;");
            proxy("error_code err_code = 0;");
            proxy("enclave_marshal_test_init(eid_, &err_code, &config);");
            proxy("return err_code;");
            proxy("}}");
            proxy("");

            /*header("//polymorphic helper functions");
            for(auto& name : m_ob.m_ownedClasses)
            {
                const ClassObject* obj = nullptr;
                if(!lib.FindClassObject(name, obj))
                {
                    continue;
                }
                if(obj->type == ObjectTypeInterface)
                {
                    header("error_code query_interface(i_unknown& from, remote_shared_ptr<{}>& to) override;", obj->name);
                    proxy("error_code {}::query_interface(i_unknown& from, remote_shared_ptr<{}>& to)", m_ob.name, obj->name);
                    proxy("{{");
                    proxy("return marshaller_->try_cast(from, {}::id, to.as_i_unknown());", obj->name);
                    proxy("}}");
                }
            }	
            proxy("");
            proxy("//static functions passed to global functions in the target zone");
            for(auto& function : m_ob.functions)
            {
                if(function.type != FunctionTypeMethod)
                    continue;
                
                proxy.print_tabs();
                proxy.print_tabs();
                proxy.raw("{} {}(", function.returnType, function.name);
                proxy.raw("{} {}::{}(", function.returnType, m_ob.name, function.name);
                bool has_parameter = false;
                for(auto& parameter : function.parameters)
                {
                    if(has_parameter)
                    {
                        proxy.raw(", ");
                        proxy.raw(", ");
                    }
                    has_parameter = true;
                    proxy.raw("{} {}",parameter.type, parameter.name);
                    proxy.raw("{} {}",parameter.type, parameter.name);
                }
                proxy.raw(") override;\n");
                proxy.raw(")\n");
                proxy("{{");
                proxy("}}");
            }         */
            proxy("}};");
            proxy("#endif");
        };

        //entry point
        void write_files(bool from_host, const Library& lib, std::ostream& hos, std::ostream& pos, std::ostream& sos, const std::vector<std::string>& namespaces, const std::string& header_filename)
        {
            writer header(hos);
            writer proxy(pos);
            writer stub(sos);

            header("#pragma once");
            header("");
            header("#include <marshaller/marshaller.h>");
            header("#include <memory>");
            header("#include <vector>");
            header("#include <map>");
            header("#include <string>");
            header("");

  			proxy("#include <yas/mem_streams.hpp>");
  			proxy("#include <yas/binary_iarchive.hpp>");
  			proxy("#include <yas/binary_oarchive.hpp>");
  			proxy("#include <yas/std_types.hpp>");
  			proxy("#include \"{}\"", header_filename);
            proxy("");

  			stub("#include <yas/mem_streams.hpp>");
  			stub("#include <yas/binary_iarchive.hpp>");
  			stub("#include <yas/binary_oarchive.hpp>");
  			proxy("#include <yas/std_types.hpp>");
  			stub("#include \"{}\"", header_filename);
            stub("");


            for(auto& ns : namespaces)
            {
                header("namespace {}", ns);
                header("{{");
                proxy("namespace {}", ns);
                proxy("{{");
                stub("namespace {}", ns);
                stub("{{");
            }

            for(auto& name : lib.m_ownedClasses)
            {
                const ClassObject* obj = nullptr;
                if(!lib.FindClassObject(name, obj))
                {
                    continue;
                }
                if(obj->type == ObjectStruct)
                    write_struct(*obj, header);
            }
            header("");

            for(auto& name : lib.m_ownedClasses)
            {
                const ClassObject* obj = nullptr;
                if(!lib.FindClassObject(name, obj))
                {
                    continue;
                }
                if(obj->type == ObjectLibrary)
                    write_library(from_host, lib, *obj, header, proxy, stub);
            }

            for(auto& ns : namespaces)
            {
                header("}}");
                proxy("}}");
                stub("}}");
            }
        }
    }
}