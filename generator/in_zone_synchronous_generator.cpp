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

        bool is_in_call(bool from_host, const Library& lib, const std::string& name, const std::string& type, const std::list<std::string>& attributes, bool& is_interface, std::string& marshall_as, std::string& demarshall_declaration, std::string& param_cast)
        {
            auto in = std::find(attributes.begin(), attributes.end(), "in") != attributes.end();
            auto out = std::find(attributes.begin(), attributes.end(), "out") != attributes.end();
            auto byval = std::find(attributes.begin(), attributes.end(), "by_value") != attributes.end();

            if(out && !in)
                return false;

            std::string type_name = type;
            std::string referenceModifiers;
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
                marshall_as = name;
                demarshall_declaration = fmt::format("{} {}", type, name);
                param_cast = name;
            }
            else if(referenceModifiers == "*&")
            {
                marshall_as = name;
                demarshall_declaration = fmt::format("uint64_t {}", type_name, name);
                param_cast = fmt::format("({}*){}", type_name, name);
            }
            else if(referenceModifiers == "*")
            {
                marshall_as = fmt::format("(uint64_t) {}", name);
                demarshall_declaration = fmt::format("uint64_t {}", name);
                param_cast = fmt::format("({}*){}", type_name, name);
            }
            else if(referenceModifiers == "&")
            {
                if(byval)
                {
                    marshall_as = name;
                    demarshall_declaration = fmt::format("{} {}", type_name, name);
                    param_cast = name;
                }
                else if(from_host == false)
                {
                    std::cerr << "passing data by reference from a non host zone is not allowed\n";
                    throw "passing data by reference from a non host zone is not allowed\n";
                }
                else
                {
                    marshall_as = fmt::format("(uint64_t)&{}", name);
                    demarshall_declaration = fmt::format("uint64_t {}", name);
                    param_cast = fmt::format("*({}*){}", type_name, name);
                }
            }
            else
            {
                std::cerr << fmt::format("passing data by {} as in {} {} is not supported", referenceModifiers, type, name);
                throw fmt::format("passing data by {} as in {} {} is not supported", referenceModifiers, type, name);
            }
            return true;
        }

        bool is_out_call(bool from_host, const Library& lib, const std::string& name, const std::string& type, const std::list<std::string>& attributes, bool& is_interface, std::string& marshall_as, std::string& demarshall_declaration, std::string& param_cast, std::string& out_serialise_cast)
        {
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
            std::string referenceModifiers;
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
                std::cerr << fmt::format("out reference parameters are not supported {} {}", type, name);
                throw fmt::format("out reference parameters are not supported {} {} is not supported", type, name);
            }
            else if(byval && referenceModifiers == "*")
            {
                marshall_as = fmt::format("*{}", name);
                demarshall_declaration = fmt::format("uint64_t {}", name);
                param_cast = fmt::format("({}*){}", type_name, name);
                out_serialise_cast = name;
            }

            else if(referenceModifiers == "*")
            {
                std::cerr << "passing [out] by_pointer data by * will not work use a ** or *&\n";
                throw "passing [out] by_pointer data by * will not work use a ** or *&\n";
            }

            //by pointer
            else if(by_pointer && referenceModifiers == "**")
            {
                marshall_as = fmt::format("(uint64_t)*{}", name);
                demarshall_declaration = fmt::format("uint64_t {}", name);
                param_cast = fmt::format("({}*){}", type_name, name);
                out_serialise_cast = name;
            }
            else if(by_pointer && referenceModifiers == "*&")
            {
                marshall_as = fmt::format("(uint64_t){}", name);
                demarshall_declaration = fmt::format("{}* {}", type_name, name);
                param_cast = name;
                out_serialise_cast = fmt::format("(uint64_t) {}", name);
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
            header("class {}{}{} : public i_unknown", 
                m_ob.name, 
                m_ob.parentName.empty() ? "" : ":",
                m_ob.parentName);
			header("{{");
			header("public:");
			header("static constexpr uint64_t id = {};", id);

            proxy("class {}_proxy : public {}", 
                m_ob.name, 
                m_ob.name);
			proxy("{{");
            proxy("i_marshaller& marshaller_;");
            proxy("uint64_t object_id_;");
			proxy("public:");
			proxy("");
            proxy("{}_proxy(i_marshaller& stub, uint64_t object_id) : ", m_ob.name);
            proxy("  marshaller_(stub),", m_ob.name);
            proxy("  object_id_(object_id)", m_ob.name);
            proxy("  {{}}", m_ob.name);
			proxy("");


            stub("class {}_stub : public i_marshaller_impl", m_ob.name);
			stub("{{");
            stub("remote_shared_ptr<{}> target_;", m_ob.name);
			stub("public:");
			stub("");
            stub("{}_stub(remote_shared_ptr<{}>& target) : ", m_ob.name, m_ob.name);
            stub("  target_(target)", m_ob.name);
            stub("  {{}}");
			stub("");
            stub("error_code send(uint64_t object_id, uint64_t interface_id, uint64_t method_id, const yas::shared_buffer& in, yas::shared_buffer& out) override");
            stub("{{");

            stub("switch(interface_id)");
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
                proxy.raw("virtual {} {}_proxy::{} (", function.returnType, m_ob.name, function.name);
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

                for(auto& parameter : function.parameters)
                {
                    bool is_interface = false;
                    std::string marshall_as;
                    std::string demarshall_declaration;
                    std::string param_cast;
                    std::string out_serialise_cast;
                    if(is_in_call(from_host, lib, parameter.name, parameter.type, parameter.m_attributes, is_interface, marshall_as, demarshall_declaration, param_cast))
                        has_inparams = true;
                    else
                        is_out_call(from_host, lib, parameter.name, parameter.type, parameter.m_attributes, is_interface, marshall_as, demarshall_declaration, param_cast, out_serialise_cast);
                    stub("{};", demarshall_declaration);
                }

                if(has_inparams)
                {
                    proxy("const auto in_ = yas::save<yas::mem|yas::binary>(YAS_OBJECT_NVP(");
                    proxy("  \"in\"");

                    stub("yas::load<yas::mem|yas::binary>(in, YAS_OBJECT_NVP(");
                    stub("  \"in\"");

                    int count = 0;
                    for(auto& parameter : function.parameters)
                    {
                        bool is_interface = false;
                        std::string marshall_as;
                        std::string demarshall_declaration;
                        std::string param_cast;
                        if(!is_in_call(from_host, lib, parameter.name, parameter.type, parameter.m_attributes, is_interface, marshall_as, demarshall_declaration, param_cast))
                            continue;

                        proxy("  ,(\"_{}\", {})", count, marshall_as);
                        stub("  ,(\"_{}\", {})", count, parameter.name);
                        count++;
                    }
                    
                    proxy("  ));");
                    stub("  ));");
                }
                else
                {
                    proxy("const yas::shared_buffer in_;");
                }

                proxy("for(int i = 0;i < in_.size;i++){{std::cout << in_.data.get() + i;}} std::cout << \"\\n\";");
                
                proxy("yas::shared_buffer out_;");
                proxy("int ret = marshaller_.send(object_id_, {}::id, {}, in_, out_);", m_ob.name, function_count);
                proxy("if(ret)");
                proxy("{{");
                proxy("return ret;");
                proxy("}}");

                stub.print_tabs();
                stub.raw("error_code ret = target_->{}(", function.name);

                {
                    bool has_param = false;
                    for(auto& parameter : function.parameters)
                    {
                        bool is_interface = false;
                        std::string marshall_as;
                        std::string demarshall_declaration;
                        std::string param_cast;
                        std::string out_serialise_cast;
                        if(!is_in_call(from_host, lib, parameter.name, parameter.type, parameter.m_attributes, is_interface, marshall_as, demarshall_declaration, param_cast))                        
                            is_out_call(from_host, lib, parameter.name, parameter.type, parameter.m_attributes, is_interface, marshall_as, demarshall_declaration, param_cast, out_serialise_cast);
                        if(has_param)
                        {
                            stub.raw(",");
                        }
                        has_param = true;
                        stub.raw("{}", param_cast);
                    }
                }
                stub.raw(");\n");
                stub("if(ret)");
                stub("  return ret;");
                stub("");

                int count = 0;
                proxy("yas::load<yas::mem|yas::binary>(out_, YAS_OBJECT_NVP(");
                proxy("  \"out\"");
                proxy("  ,(\"_{}\", ret)", count);

                stub("out = yas::save<yas::mem|yas::binary>(YAS_OBJECT_NVP(");
                stub("  \"out\"");
                stub("  ,(\"_{}\", ret)", count);

                for(auto& parameter : function.parameters)
                {
                    count++;
                    bool is_interface = false;
                    std::string marshall_as;
                    std::string demarshall_declaration;
                    std::string param_cast;
                    std::string out_serialise_cast;
                    if(!is_out_call(from_host, lib, parameter.name, parameter.type, parameter.m_attributes, is_interface, marshall_as, demarshall_declaration, param_cast, out_serialise_cast))
                        continue;

                    proxy("  ,(\"_{}\", {})", count, marshall_as);
                    stub("  ,(\"_{}\", {})", count, out_serialise_cast);
                }
                proxy("  ));");
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

            header("//a marshalable interface for other zones");
            header("class i_{} : public i_zone", m_ob.name);
			header("{{");
            header("public:");
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
            header("");
            
            
            header("#ifndef _IN_ENCLAVE");
            header("//the class that encapsulates an environment or zone");
            header("//only host code can use this class directly other enclaves *may* have access to the i_zone derived interface");
            header("class {} : public i_{}", m_ob.name, m_ob.name);
			header("{{");
            header("std::unique_ptr<enclave_info> enclave_;");
            header("std::shared_ptr<i_marshaller> marshaller_;");

            header("public:");

            header("{}();", m_ob.name);
            header("~{}();", m_ob.name);
            header("");
            //header("error_code load(std::string& dll_file_name);");
            //header("");
            header("error_code assign_marshaller(const std::shared_ptr<i_marshaller>& marshaller)");
            header("{{");
            header("marshaller_ = marshaller;");
            header("}}");
            header("");

            /*proxy("error_code {}::load(std::string& dll_file_name);", m_ob.name);
            proxy("{{");
            proxy("}}");
            proxy("");*/

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
                    header("error_code query_interface(i_unknown& from, remote_shared_ptr<{}>& to) override;", obj->name);
                    proxy("error_code {}::query_interface(i_unknown& from, remote_shared_ptr<{}>& to)", m_ob.name, obj->name);
                    proxy("{{");
                    proxy("return marshaller_->try_cast(from, {}::id, to.as_i_unknown());", obj->name);
                    proxy("}}");
                }
            }	
            /*header("");
            header("//static functions passed to global functions in the target zone");
            for(auto& function : m_ob.functions)
            {
                if(function.type != FunctionTypeMethod)
                    continue;
                
                header.print_tabs();
                proxy.print_tabs();
                header.raw("{} {}(", function.returnType, function.name);
                proxy.raw("{} {}::{}(", function.returnType, m_ob.name, function.name);
                bool has_parameter = false;
                for(auto& parameter : function.parameters)
                {
                    if(has_parameter)
                    {
                        header.raw(", ");
                        proxy.raw(", ");
                    }
                    has_parameter = true;
                    header.raw("{} {}",parameter.type, parameter.name);
                    proxy.raw("{} {}",parameter.type, parameter.name);
                }
                header.raw(") override;\n");
                proxy.raw(")\n");
                proxy("{{");
                proxy("}}");
            }     */       
            header("}};");
            header("#endif //_IN_ENCLAVE");
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