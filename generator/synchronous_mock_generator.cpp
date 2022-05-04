#include <type_traits>
#include <algorithm>
#include <tuple>
#include <type_traits>
#include "coreclasses.h"
#include "cpp_parser.h"
#include <filesystem>

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

        void write_marshalling_logic_nested(bool from_host, const class_entity& cls, int id, writer& header)
        {
            if (cls.get_type() == entity_type::INTERFACE)
                write_interface(from_host, cls, header, id);

            if (cls.get_type() == entity_type::LIBRARY)
                write_interface(from_host, cls, header, id);
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
            header("#include <string>");

            header("#include <gmock/gmock-actions.h>");
            header("#include <gmock/gmock.h>");
            header("#include <gmock/internal/gmock-port.h>");
            header("#include <gtest/gtest.h>");
            header("#include <gtest/gtest-spi.h>");
            header("#include <gmock/gmock-matchers.h>");

            header("#include <marshaller/marshaller.h>");
            header("#include <marshaller/service.h>");
            
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