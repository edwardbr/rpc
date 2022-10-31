#include <type_traits>
#include <algorithm>
#include <tuple>
#include <type_traits>
#include "coreclasses.h"
#include "cpp_parser.h"
#include <filesystem>
#include <sstream>

#include "writer.h"

#include "synchronous_mock_generator.h"

namespace enclave_marshaller
{
    namespace synchronous_mock_generator
    {

        void write_interface(bool from_host, const class_entity& m_ob, writer& header, int id)
        {
            auto interface_name = std::string(m_ob.get_type() == entity_type::LIBRARY ? "i_" : "") + m_ob.get_name();

            std::string base_class_declaration;
            auto bc = m_ob.get_base_classes();
            if (!bc.empty())
            {

                base_class_declaration = " : ";
                int i = 0;
                for (auto base_class : bc)
                {
                    if (i)
                        base_class_declaration += ", ";
                    base_class_declaration += base_class->get_name();
                    i++;
                }
            }
            header("class {0}_mock : public {0}", interface_name);
            header("{{");
            header("void* get_address() const override {{ return (void*)this; }}");
            header("const rpc::casting_interface* query_interface(uint64_t interface_id) const override ");
            header("{{");
            header("if({0}::id == interface_id)", interface_name);
            header("return static_cast<const {0}*>(this); ",interface_name);
            header("return nullptr;");
            header("}}");            
            header("public:");

            bool has_methods = false;
            for (auto& function : m_ob.get_functions())
            {
                if (function.get_type() != FunctionTypeMethod)
                    continue;
                has_methods = true;
            }

            if (has_methods)
            {
                int function_count = 1;
                for (auto& function : m_ob.get_functions())
                {
                    if (function.get_type() != FunctionTypeMethod)
                        continue;

                    header.print_tabs();
                    header.raw("MOCK_METHOD{}({}, {}(", function.get_parameters().size(), function.get_name(), function.get_return_type());
                    bool has_parameter = false;
                    for (auto& parameter : function.get_parameters())
                    {
                        if (has_parameter)
                        {
                            header.raw(", ");
                        }
                        has_parameter = true;
                        std::string modifier;
                        for (auto& item : parameter.get_attributes())
                        {
                            if (item == "const")
                                modifier = "const " + modifier;
                        }
                        header.raw("{}{} {}", modifier, parameter.get_type(), parameter.get_name());
                    }
                    header.raw("));\n");

                }
            }

            header("}};");
            header("");
        };


        void write_struct(bool from_host, const class_entity& m_ob, writer& header, int id)
        {
            auto interface_name = m_ob.get_name();

            //create a matcher for structures

            std::stringstream sstr;
            std::string obj_type(interface_name);
            {
                writer tmpl(sstr);
                tmpl.set_count(header.get_count());
                if (m_ob.get_is_template())
                {
                    if(m_ob.get_template_params().empty())
                        return; //too complicated for now
                    tmpl.print_tabs();
                    tmpl.raw("template<");
                    if(!m_ob.get_template_params().empty())
                        obj_type += "<";
                    bool first_pass = true;
                    for (const auto& param : m_ob.get_template_params())
                    {
                        if (!first_pass)
                        {
                            tmpl.raw(", ");
                            obj_type += ", ";
                        }
                        first_pass = false;
                        tmpl.raw("{} {}", param.type, param.name);
                        obj_type += param.name;
                    }
                    if(!m_ob.get_template_params().empty())
                        obj_type += ">";
                    tmpl.raw(">\n");
                }
            }
            header.raw(sstr.str());
            header("class {0}_matcher : public MatcherInterface<{1}>", interface_name, obj_type);
            header("{{");
            for (auto& function : m_ob.get_functions())
            {
                header("Matcher<{}> {}_matcher_;", function.get_return_type(), function.get_name());
            }
            header("public:");
            header("{0}_matcher(", interface_name);
            header.set_count(header.get_count() + 1);
            bool is_first = true;
            for (auto& function : m_ob.get_functions())
            {
                if(!is_first)
                    header.raw(",\n");
                is_first = false;
                header.print_tabs();
                header.raw("const Matcher<{}> {}_matcher", function.get_return_type(), function.get_name());
            }
            if(!m_ob.get_functions().empty())
                header.raw("\n");
            header(")");
            is_first = true;
            for (auto& function : m_ob.get_functions())
            {
                if(is_first)
                    header(": {0}_matcher_({0}_matcher)", function.get_name());
                else
                    header(", {0}_matcher_({0}_matcher)", function.get_name());
                is_first = false;
            }                
            header.set_count(header.get_count() - 1);
            header("{{}}");

            header("virtual bool MatchAndExplain({} request, MatchResultListener* listener) const", obj_type);
            header("{{");
            for (auto& function : m_ob.get_functions())
            {
                header("if (!{0}_matcher_.MatchAndExplain(request.{0}, listener))", function.get_name());
                header("\treturn false;");
            }
		    header("return true;");
        	header("}};");     
            header("virtual void DescribeTo(::std::ostream* os) const");
            header("{{");
            header("(*os) << \"is working \";");
            for (auto& function : m_ob.get_functions())
            {
                header("(*os) << \"{} \";", function.get_name());
		        header("{}_matcher_.DescribeTo(os);", function.get_name());
            }
        	header("}};");              
            header("virtual void DescribeNegationTo(::std::ostream* os) const");
            header("{{");   
            header("(*os) << \"is not working \";");
            for (auto& function : m_ob.get_functions())
            {
                header("(*os) << \"{} \";", function.get_name());
                header("{}_matcher_.DescribeTo(os);", function.get_name());           
            }
            header("}};");              
            header("}};");
            header("");
        }

        void write_marshalling_logic_nested(bool from_host, const class_entity& cls, int id, writer& header)
        {
            if (cls.get_type() == entity_type::INTERFACE)
                write_interface(from_host, cls, header, id);

            if (cls.get_type() == entity_type::LIBRARY)
                write_interface(from_host, cls, header, id);

            if (cls.get_type() == entity_type::STRUCT)
                write_struct(from_host, cls, header, id);
        }

        // entry point
        void write_namespace(bool from_host, const class_entity& lib, int& id, writer& header)
        {
            for (auto cls : lib.get_classes())
            {
                if(!cls->get_import_lib().empty())
                    continue;
                if (cls->get_type() == entity_type::NAMESPACE)
                {

                    header("namespace {}", cls->get_name());
                    header("{{");

                    write_namespace(from_host, *cls, id, header);

                    header("}}");
                }
                else
                {
                    write_marshalling_logic_nested(from_host, *cls, id++, header);
                }
            }
        }

        // entry point
        void write_files(bool from_host, const class_entity& lib, std::ostream& hos, const std::vector<std::string>& namespaces, const std::string& header_filename, const std::list<std::string>& imports)
        {
            writer header(hos);

            header("#pragma once");
            header("");
            header("#include <memory>");
            header("#include <vector>");
            header("#include <map>");
            header("#include <set>");
            header("#include <string>");
            header("#include <array>");

            header("#include <gmock/gmock-actions.h>");
            header("#include <gmock/gmock.h>");
            header("#include <gmock/internal/gmock-port.h>");
            header("#include <gtest/gtest.h>");
            header("#include <gtest/gtest-spi.h>");
            header("#include <gmock/gmock-matchers.h>");

            header("#include <rpc/marshaller.h>");
            header("#include <rpc/service.h>");
            
            header("#include \"{}\"", header_filename);

            //header("namespace mocks");
            //header("{{");
            // This list should be kept sorted.;
            header("using testing::Action;");
            header("using testing::ActionInterface;");
            header("using testing::Assign;");
            header("using testing::ByMove;");
            header("using testing::ByRef;");
            header("using testing::DoDefault;");
            header("using testing::IgnoreResult;");
            header("using testing::Invoke;");
            header("using testing::InvokeWithoutArgs;");
            header("using testing::MakePolymorphicAction;");
            header("using testing::Ne;");
            header("using testing::PolymorphicAction;");
            header("using testing::Return;");
            header("using testing::ReturnNull;");
            header("using testing::ReturnRef;");
            header("using testing::ReturnRefOfCopy;");
            header("using testing::SetArgPointee;");
            header("using testing::SetArgumentPointee;");
            header("using testing::_;");
            header("using testing::get;");
            header("using testing::make_tuple;");
            header("using testing::tuple;");
            header("using testing::tuple_element;");

            header("using ::testing::Field;");
            header("using ::testing::AllOf;");
            header("using ::testing::StrEq;");
            header("using ::testing::Eq;");
            header("using ::testing::AnyOf;");
            header("using ::testing::Not;");
            header("using ::testing::NotNull;");

            header("using ::testing::MatcherInterface;");
            header("using ::testing::Matcher;");
            header("using ::testing::MatchResultListener;");

            header("");


            for (auto& ns : namespaces)
            {
                header("namespace {}", ns);
                header("{{");
            }

            int id = 1;
            write_namespace(from_host, lib, id, header);

            for (auto& ns : namespaces)
            {
                header("}}");
            }
            //header("}}");
        }
    }
}