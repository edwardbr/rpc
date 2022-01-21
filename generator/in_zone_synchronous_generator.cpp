#include <type_traits>
#include <algorithm>
#include "coreclasses.h"
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

        void write_interface(const Library& lib, const ClassObject& m_ob, writer& header, writer& proxy, writer& stub, int id)
        {
            header("class {}{}{} : public i_unknown", 
                m_ob.name, 
                m_ob.parentName.empty() ? "" : ":",
                m_ob.parentName);
			header("{{");
			header("public:");
			header("static constexpr int id = {};", id);

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


            stub("class {}_stub : public i_marshaller", m_ob.name);
			stub("{{");
            stub("remote_shared_ptr<{}> target_;", m_ob.name);
			stub("public:");
			stub("");
            stub("{}_stub(remote_shared_ptr<{}>& target) : ", m_ob.name, m_ob.name);
            stub("  target_(target),", m_ob.name);
            stub("  {{}}");
			stub("");
            stub("error_code send(int object_id, int interface_id, int method_id, const yas::shared_buffer& in, yas::shared_buffer& out) override");
            stub("{{");

            int function_count = 1;
            for(auto& function : m_ob.functions)
            {
                if(function.type != FunctionTypeMethod)
                    continue;
                
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
                    header.raw("{} {}",parameter.type, parameter.name);
                    proxy.raw("{} {}",parameter.type, parameter.name);
                }
                header.raw(") = 0;\n");
                proxy.raw(") override\n");
                proxy("{{");

                bool has_inparams = false;
                for(auto& parameter : function.parameters)
                {
                    auto in_it = std::find(parameter.m_attributes.begin(), parameter.m_attributes.end(), "in");
                    auto out_it = std::find(parameter.m_attributes.begin(), parameter.m_attributes.end(), "out");
                    auto byval_it = std::find(parameter.m_attributes.begin(), parameter.m_attributes.end(), "by_value");
                    if(out_it != parameter.m_attributes.end() && in_it == parameter.m_attributes.end())
                        continue;

                    has_inparams = true;
                    break;
                }

                if(has_inparams)
                {
                    proxy("const auto in_ = yas::save<yas::mem|yas::json>(YAS_OBJECT_NVP(");
                    proxy("  \"in\"");

                    int count = 0;
                    for(auto& parameter : function.parameters)
                    {
                        auto in_it = std::find(parameter.m_attributes.begin(), parameter.m_attributes.end(), "in");
                        auto out_it = std::find(parameter.m_attributes.begin(), parameter.m_attributes.end(), "out");
                        auto byval_it = std::find(parameter.m_attributes.begin(), parameter.m_attributes.end(), "by_value");
                        if(out_it != parameter.m_attributes.end() && in_it == parameter.m_attributes.end())
                            continue;

                        proxy("  ,(\"_{}\", {})", count++, parameter.name);
                    }
                    
                    proxy("  ));");
                }
                else
                {
                    proxy("const yas::shared_buffer in_;");
                }
                
                proxy("yas::shared_buffer out_;");
                proxy("int ret = marshaller_.send(object_id_, {}::id, {}, in_, out_);", m_ob.name, function_count);
                proxy("if(ret)");
                proxy("{{");
                proxy("return ret;");
                proxy("}}");

                bool has_out_params = false;
                for(auto& parameter : function.parameters)
                {
                    auto in_it = std::find(parameter.m_attributes.begin(), parameter.m_attributes.end(), "in");
                    auto out_it = std::find(parameter.m_attributes.begin(), parameter.m_attributes.end(), "out");
                    if(out_it == parameter.m_attributes.end() && in_it != parameter.m_attributes.end())
                        continue;

                    has_out_params = true;
                    break;
                }

                if(has_out_params)
                {
                    proxy("yas::load<yas::mem|yas::json>(out_, YAS_OBJECT_NVP(");
                    proxy("  \"out\"");

                    int count = 0;
                    for(auto& parameter : function.parameters)
                    {
                        auto in_it = std::find(parameter.m_attributes.begin(), parameter.m_attributes.end(), "in");
                        auto out_it = std::find(parameter.m_attributes.begin(), parameter.m_attributes.end(), "out");
                        if(out_it == parameter.m_attributes.end() && in_it != parameter.m_attributes.end())
                            continue;

                        proxy("  ,(\"_{}\", {})", count++, parameter.name);
                    }
                    proxy("  ));");
                }
                proxy("return ret;");
                proxy("}}");
                proxy("");

                function_count++;
            }

            header("}};");            
            header("");

            proxy("}};");            
            proxy("");

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
            header(")");

            header("}}");
			

            header("}};");
        };

        void write_library(const Library& lib, const ClassObject& m_ob, writer& header, writer& proxy, writer& stub)
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
                        write_interface(lib, *obj, header, proxy, stub, id++);
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
            header("");
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
            }            
            header("}};");
            header("#endif //_IN_ENCLAVE");
        };

        //entry point
        void write_files(const Library& lib, std::ostream& hos, std::ostream& pos, std::ostream& sos, const std::vector<std::string>& namespaces, const std::string& header_filename)
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

  			proxy("#include <yas/serialize.hpp>");
  			proxy("#include \"{}\"", header_filename);
            proxy("");

  			stub("#include <yas/serialize.hpp>");
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
                    write_library(lib, *obj, header, proxy, stub);
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