#include <iostream>
#include <string>
#include <sstream>
#include <filesystem>
#include <fstream>

#include <clipp.h>

#include "commonfuncs.h"
#include "macro_parser.h"

#include <coreclasses.h>

#include "synchronous_generator.h"
#include "synchronous_mock_generator.h"

using namespace std;

std::stringstream verboseStream;

namespace javascript_json
{
    namespace json
    {
        extern string namespace_name;
    }
}

void get_imports(const std::shared_ptr<class_entity>& object, std::list<std::string>& imports,
                 std::set<std::string>& imports_cache)
{
    for (auto& cls : object->get_classes())
    {
        if (!cls->get_import_lib().empty())
        {
            if (imports_cache.find(cls->get_import_lib()) == imports_cache.end())
            {
                imports_cache.insert(cls->get_import_lib());
                imports.push_back(cls->get_import_lib());
            }
        }
    }
}

bool is_dfferent(const std::stringstream& stream, const std::string& data)
{
    auto stream_str = stream.str();
    if (stream_str.empty())
    {
        if (data.empty())
            return true;
        return false;
    }
    stream_str = stream_str.substr(0, stream_str.length() - 1);
    return stream_str != data;
}

int main(const int argc, char* argv[])
{
    try
    {
        std::string module_name;
        string rootIdl;
        string headerPath;
        string proxyPath;
        string proxyHeaderPath;
        string stubPath;
        string stubHeaderPath;
        string mockPath;
        string output_path;
        std::vector<std::string> namespaces;
        std::vector<std::string> include_paths;
        std::vector<std::string> defines;
        std::vector<std::string> wrong_elements;
        bool dump_preprocessor_output_and_die = false;

        auto cli = (
			clipp::required("-i", "--idl").doc("the idl to be parsed") & clipp::value("idl",rootIdl),
			clipp::required("-p", "--output_path").doc("base output path") & clipp::value("output_path",output_path),
			clipp::required("-h", "--header").doc("the generated header relative filename") & clipp::value("header",headerPath),
			clipp::required("-x", "--proxy").doc("the generated proxy relative filename") & clipp::value("proxy",proxyPath),
            clipp::parameter("-y", "--proxy_header").doc("the generated proxy header relative filename") & clipp::value("proxy_header",proxyHeaderPath),
			clipp::required("-s", "--stub").doc("the generated stub relative filename") & clipp::value("stub",stubPath),
            clipp::parameter("-t", "--stub_header").doc("the generated stub header relative filename") & clipp::value("stub_header",stubHeaderPath),
			clipp::option("-m", "--mock").doc("the generated mock relative filename") & clipp::value("mock",mockPath),
			clipp::option("-M", "--module_name").doc("the name given to the stub_factory") & clipp::value("module_name",module_name),
			clipp::repeatable(clipp::option("-p", "--path") & clipp::value("path",include_paths)).doc("locations of include files used by the idl"),
			clipp::option("-n","--namespace").doc("namespace of the generated interface") & clipp::value("namespace",namespaces),
			clipp::option("-d","--dump_preprocessor_output_and_die").set(dump_preprocessor_output_and_die).doc("dump preprocessor output and die"),
			clipp::repeatable(clipp::option("-D") & clipp::value("define",defines)).doc("macro define"),
			clipp::any_other(wrong_elements)
		);

        clipp::parsing_result res = clipp::parse(argc, argv, cli);
        if (!wrong_elements.empty())
        {
            std::cout << "Error unrecognised parameters\n";
            for (auto& element : wrong_elements)
            {
                std::cout << element << "\n";
            }

            std::cout << "Please read documentation\n" << clipp::make_man_page(cli, argv[0]);
            return -1;
        }
        if (!res)
        {
            cout << clipp::make_man_page(cli, argv[0]);
            return -1;
        }

        std::replace(headerPath.begin(), headerPath.end(), '\\', '/');
        std::replace(proxyPath.begin(), proxyPath.end(), '\\', '/');
        std::replace(stubPath.begin(), stubPath.end(), '\\', '/');
        if (mockPath.length())
            std::replace(mockPath.begin(), mockPath.end(), '\\', '/');
        std::replace(output_path.begin(), output_path.end(), '\\', '/');

        std::unique_ptr<macro_parser> parser = std::unique_ptr<macro_parser>(new macro_parser());

        for (auto& define : defines)
        {
            auto elems = split(define, '=');
            {
                macro_parser::definition def;
                std::string defName = elems[0];
                if (elems.size() > 1)
                {
                    def.m_substitutionString = elems[1];
                }
                parser->AddDefine(defName, def);
            }
        }

        std::string pre_parsed_data;

        {
            macro_parser::definition def;
            def.m_substitutionString = "1";
            parser->AddDefine("GENERATOR", def);
        }
		
        std::error_code ec;

        auto idl = std::filesystem::absolute(rootIdl, ec);
        if (!std::filesystem::exists(idl))
        {
            cout << "Error file " << idl << " does not exist";
            return -1;
        }

        std::vector<std::filesystem::path> parsed_paths;
        for (auto& path : include_paths)
        {
            parsed_paths.emplace_back(std::filesystem::canonical(path, ec).make_preferred());
        }

        std::vector<std::string> loaded_includes;

        std::stringstream output_stream;
        auto r = parser->load(output_stream, rootIdl, parsed_paths, loaded_includes);
        pre_parsed_data = output_stream.str();
        if (dump_preprocessor_output_and_die)
        {
            std::cout << pre_parsed_data << '\n';
            return 0;
        }

        // load the idl file
        auto objects = std::make_shared<class_entity>(nullptr);
        const auto* ppdata = pre_parsed_data.data();
        objects->parse_structure(ppdata, true);

        std::list<std::string> imports;
        {
            std::set<std::string> imports_cache;
            if (!objects->get_import_lib().empty())
            {
                std::cout << "root object has a non empty import lib\n";
                return -1;
            }

            get_imports(objects, imports, imports_cache);
        }

        string interfaces_h_data;
        string interfaces_proxy_data;
        string interfaces_proxy_header_data;
        string interfaces_stub_data;
        string interfaces_stub_header_data;
        string interfaces_mock_data;

        proxyHeaderPath = proxyHeaderPath.size() ? proxyHeaderPath : (proxyPath + ".h");
        stubHeaderPath = stubHeaderPath.size() ? stubHeaderPath : (stubPath + ".h");

        auto header_path = std::filesystem::path(output_path) / "include" / headerPath;
        auto proxy_path = std::filesystem::path(output_path) / "src" / proxyPath;
        auto proxy_header_path = std::filesystem::path(output_path) / "src" / proxyHeaderPath;
        auto stub_path = std::filesystem::path(output_path) / "src" / stubPath;
        auto stub_header_path = std::filesystem::path(output_path) / "src" / stubHeaderPath;
        auto mock_path = std::filesystem::path(output_path) / "include" / mockPath;

        std::filesystem::create_directories(header_path.parent_path());
        std::filesystem::create_directories(proxy_path.parent_path());
        std::filesystem::create_directories(proxy_header_path.parent_path());
        std::filesystem::create_directories(stub_path.parent_path());
        std::filesystem::create_directories(stub_header_path.parent_path());
        if (mockPath.length())
            std::filesystem::create_directories(mock_path.parent_path());

        // read the original data and close the files afterwards
        {
            std::error_code ec;
            ifstream hfs(header_path);
            std::getline(hfs, interfaces_h_data, '\0');

            ifstream proxy_fs(proxy_path);
            std::getline(proxy_fs, interfaces_proxy_data, '\0');

            ifstream proxy_header_fs(proxy_header_path);
            std::getline(proxy_fs, interfaces_proxy_header_data, '\0');

            ifstream stub_fs(stub_path);
            std::getline(stub_fs, interfaces_stub_data, '\0');

            ifstream stub_header_fs(stub_header_path);
            std::getline(stub_header_fs, interfaces_stub_header_data, '\0');

            if (mockPath.length())
            {
                ifstream mock_fs(mock_path);
                std::getline(mock_fs, interfaces_mock_data, '\0');
            }
        }

        std::stringstream header_stream;
        std::stringstream proxy_stream;
        std::stringstream proxy_header_stream;
        std::stringstream stub_stream;
        std::stringstream stub_header_stream;
        std::stringstream mock_stream;

        // do the generation to the ostrstreams
        {
            enclave_marshaller::synchronous_generator::write_files(module_name, true, *objects, header_stream, proxy_stream,
                                                                   proxy_header_stream, stub_stream, stub_header_stream, 
                                                                   namespaces, headerPath, proxyHeaderPath, stubHeaderPath, imports);

            header_stream << ends;
            proxy_stream << ends;
            proxy_header_stream << ends;
            stub_stream << ends;
            stub_header_stream << ends;
            if (mockPath.length())
            {
                enclave_marshaller::synchronous_mock_generator::write_files(true, *objects, mock_stream, namespaces,
                                                                            headerPath, imports);
                mock_stream << ends;
            }
        }

        // compare and write if different
        if (is_dfferent(header_stream, interfaces_h_data))
        {
            ofstream file(header_path);
            file << header_stream.str();
        }
        if (is_dfferent(proxy_stream, interfaces_proxy_data))
        {
            ofstream file(proxy_path);
            file << proxy_stream.str();
        }
        if (is_dfferent(proxy_header_stream, interfaces_proxy_header_data))
        {
            ofstream file(proxy_header_path);
            file << proxy_header_stream.str();
        }
        if (is_dfferent(stub_stream, interfaces_stub_data))
        {
            ofstream file(stub_path);
            file << stub_stream.str();
        }
        if (is_dfferent(stub_header_stream, interfaces_stub_header_data))
        {
            ofstream file(stub_header_path);
            file << stub_header_stream.str();
        }
        if (mockPath.length())
        {
            if (interfaces_mock_data != mock_stream.str())
            {
                ofstream file(mock_path);
                file << mock_stream.str();
            }
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << std::endl;
        return 1;
    }

    return 0;
}
