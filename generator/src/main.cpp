#include <iostream>
#include <string>
#include <sstream>
#include <filesystem>
#include <fstream>

#include <args.hxx>

#include "commonfuncs.h"
#include "macro_parser.h"

#include <coreclasses.h>

#include "synchronous_generator.h"
#include "synchronous_mock_generator.h"
#include "yas_generator.h"
#include "component_checksum.h"

#include "json_schema/generator.h"

using namespace std;

std::stringstream verboseStream;

namespace javascript_json
{
    namespace json
    {
        extern string namespace_name;
    }
}

void get_imports(const class_entity& object, std::list<std::string>& imports, std::set<std::string>& imports_cache)
{
    for (auto& cls : object.get_classes())
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

bool is_different(const std::stringstream& stream, const std::string& data)
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
        args::ArgumentParser args_parser("Generate C++ headers and source from idl files");
        args::HelpFlag h(args_parser, "help", "help", {"help"});

        args::ValueFlag<std::string> root_idl_arg(
            args_parser, "path", "the idl to be parsed", {'i', "idl"}, args::Options::Required);
        args::ValueFlag<std::string> output_path_arg(
            args_parser, "path", "the base output path", {'p', "output_path"}, args::Options::Required);
        args::ValueFlag<std::string> header_path_arg(
            args_parser, "path", "the generated header relative filename", {'h', "header"}, args::Options::Required);
        args::ValueFlag<std::string> proxy_path_arg(
            args_parser, "path", "the generated proxy relative filename", {'x', "proxy"}, args::Options::Required);
        args::ValueFlag<std::string> stub_path_arg(
            args_parser, "path", "the generated stub relative filename", {'s', "stub"}, args::Options::Required);
        args::ValueFlag<std::string> stub_header_path_arg(
            args_parser, "path", "the generated stub header relative filename", {'t', "stub_header"}, args::Options::Required);
        args::ValueFlag<std::string> mock_path_arg(
            args_parser, "path", "the generated mock relative filename", {'m', "mock"});
        args::Flag suppress_catch_stub_exceptions_arg(
            args_parser, "suppress_catch_stub_exceptions", "catch stub exceptions", {'c', "suppress_catch_stub_exceptions"});
        args::ValueFlag<std::string> module_name_arg(
            args_parser, "name", "the name given to the stub_factory", {'M', "module_name"});
        args::ValueFlagList<std::string> include_paths_arg(
            args_parser, "name", "locations of include files used by the idl", {'P', "path"});
        args::ValueFlagList<std::string> namespaces_arg(
            args_parser, "namespace", "namespace of the generated interface", {'n', "namespace"});
        args::Flag dump_preprocessor_output_and_die_arg(args_parser,
            "dump_preprocessor_output_and_die",
            "dump preprocessor output and die",
            {'d', "dump_preprocessor_output_and_die"});
        args::ValueFlagList<std::string> defines_arg(args_parser, "define", "macro define", {'D'});
        args::ValueFlagList<std::string> additional_headers_arg(
            args_parser, "header", "additional header to be added to the idl generated header", {'H', "additional_headers"});
        args::ValueFlagList<std::string> rethrow_exceptions_arg(
            args_parser, "exception", "exceptions that should be rethrown", {'r', "rethrow_stub_exception"});
        args::ValueFlagList<std::string> additional_stub_headers_arg(
            args_parser, "header", "additional stub headers", {'A', "additional_stub_header"});

        try
        {
            args_parser.ParseCLI(argc, argv);
        }
        catch (const args::Help&)
        {
            std::cout << args_parser;
            return 0;
        }
        catch (const args::ParseError& e)
        {
            std::cerr << e.what() << std::endl;
            std::cerr << args_parser;
            return 1;
        }

        std::string module_name = args::get(module_name_arg);
        string root_idl = args::get(root_idl_arg);
        string header_path = args::get(header_path_arg);
        string proxy_path = args::get(proxy_path_arg);
        string stub_path = args::get(stub_path_arg);
        string stub_header_path = args::get(stub_header_path_arg);
        string mock_path = args::get(mock_path_arg);
        string output_path = args::get(output_path_arg);
        std::vector<std::string> namespaces = args::get(namespaces_arg);
        std::vector<std::string> include_paths = args::get(include_paths_arg);
        std::vector<std::string> defines = args::get(defines_arg);
        bool suppress_catch_stub_exceptions = args::get(suppress_catch_stub_exceptions_arg);
        std::vector<std::string> rethrow_exceptions = args::get(rethrow_exceptions_arg);
        std::vector<std::string> additional_headers = args::get(additional_headers_arg);
        std::vector<std::string> additional_stub_headers = args::get(additional_stub_headers_arg);
        bool dump_preprocessor_output_and_die = args::get(dump_preprocessor_output_and_die_arg);
        std::replace(header_path.begin(), header_path.end(), '\\', '/');
        std::replace(proxy_path.begin(), proxy_path.end(), '\\', '/');
        std::replace(stub_path.begin(), stub_path.end(), '\\', '/');
        std::replace(mock_path.begin(), mock_path.end(), '\\', '/');
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

        auto idl = std::filesystem::absolute(root_idl, ec);
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
        auto r = parser->load(output_stream, root_idl, parsed_paths, loaded_includes);
        if (!r)
        {
            std::cerr << "unable to load " << root_idl << '\n';
            return -1;
        }
        pre_parsed_data = output_stream.str();
        if (dump_preprocessor_output_and_die)
        {
            std::cout << pre_parsed_data << '\n';
            return 0;
        }

        // load the idl file
        auto objects = std::make_shared<class_entity>(nullptr);
        const auto* ppdata = pre_parsed_data.data();
        objects->parse_structure(ppdata, true, false);

        std::list<std::string> imports;
        {
            std::set<std::string> imports_cache;
            if (!objects->get_import_lib().empty())
            {
                std::cout << "root object has a non empty import lib\n";
                return -1;
            }

            get_imports(*objects, imports, imports_cache);
        }

        stub_header_path = stub_header_path.size() ? stub_header_path : (stub_path + ".h");

        // do the generation of the checksums, in a directory that matches the main header one
        auto checksums_path
            = std::filesystem::path(output_path) / "check_sums" / std::filesystem::path(header_path).parent_path();
        std::filesystem::create_directories(checksums_path);
        component_checksum::write_namespace(*objects, checksums_path);

        // do the generation of the proxy and stubs
        {
            auto header_fs_path = std::filesystem::path(output_path) / "include" / header_path;
            auto proxy_fs_path = std::filesystem::path(output_path) / "src" / proxy_path;
            auto stub_fs_path = std::filesystem::path(output_path) / "src" / stub_path;
            auto stub_header_fs_path = std::filesystem::path(output_path) / "include" / stub_header_path;
            auto mock_fs_path = std::filesystem::path(output_path) / "include" / mock_path;

            std::filesystem::create_directories(header_fs_path.parent_path());
            std::filesystem::create_directories(proxy_fs_path.parent_path());
            std::filesystem::create_directories(stub_fs_path.parent_path());
            std::filesystem::create_directories(stub_header_fs_path.parent_path());
            if (mock_path.length())
                std::filesystem::create_directories(mock_fs_path.parent_path());

            // read the original data and close the files afterwards
            string interfaces_h_data;
            string interfaces_proxy_data;
            string interfaces_proxy_header_data;
            string interfaces_stub_data;
            string interfaces_stub_header_data;
            string interfaces_mock_data;

            {
                ifstream hfs(header_fs_path);
                std::getline(hfs, interfaces_h_data, '\0');

                ifstream proxy_fs(proxy_fs_path);
                std::getline(proxy_fs, interfaces_proxy_data, '\0');

                ifstream stub_fs(stub_fs_path);
                std::getline(stub_fs, interfaces_stub_data, '\0');

                ifstream stub_header_fs(stub_header_fs_path);
                std::getline(stub_header_fs, interfaces_stub_header_data, '\0');

                if (mock_path.length())
                {
                    ifstream mock_fs(mock_fs_path);
                    std::getline(mock_fs, interfaces_mock_data, '\0');
                }
            }

            std::stringstream header_stream;
            std::stringstream proxy_stream;
            std::stringstream stub_stream;
            std::stringstream stub_header_stream;
            std::stringstream mock_stream;

            rpc_generator::synchronous_generator::write_files(module_name,
                true,
                *objects,
                header_stream,
                proxy_stream,
                stub_stream,
                stub_header_stream,
                namespaces,
                header_path,
                stub_header_path,
                imports,
                additional_headers,
                !suppress_catch_stub_exceptions,
                rethrow_exceptions,
                additional_stub_headers);

            header_stream << ends;
            proxy_stream << ends;
            stub_stream << ends;
            stub_header_stream << ends;
            if (mock_path.length())
            {
                rpc_generator::synchronous_mock_generator::write_files(
                    true, *objects, mock_stream, namespaces, header_path);
                mock_stream << ends;
            }

            // compare and write if different
            if (is_different(header_stream, interfaces_h_data))
            {
                ofstream file(header_fs_path);
                file << header_stream.str();
            }
            if (is_different(proxy_stream, interfaces_proxy_data))
            {
                ofstream file(proxy_fs_path);
                file << proxy_stream.str();
            }
            if (is_different(stub_stream, interfaces_stub_data))
            {
                ofstream file(stub_fs_path);
                file << stub_stream.str();
            }
            if (is_different(stub_header_stream, interfaces_stub_header_data))
            {
                ofstream file(stub_header_fs_path);
                file << stub_header_stream.str();
            }
            if (mock_path.length())
            {
                if (interfaces_mock_data != mock_stream.str())
                {
                    ofstream file(mock_fs_path);
                    file << mock_stream.str();
                }
            }
        }

        // do the generation of the yas serialisation
        {
            // get default paths
            auto pos = header_path.rfind(".h");
            if (pos == std::string::npos)
            {
                std::cerr << "failed looking for a .h suffix " << header_path << '\n';
                return -1;
            }

            auto file_path = header_path.substr(0, pos) + ".cpp";
            auto tmp_header_path = std::filesystem::path(output_path) / "src" / file_path;

            // then generate yas subdirectories
            auto header_fs_path = tmp_header_path.parent_path() / "yas" / tmp_header_path.filename();

            std::filesystem::create_directories(header_fs_path.parent_path());

            // read the original data and close the files afterwards
            string interfaces_h_data;

            {
                std::error_code ec;
                ifstream hfs(header_fs_path);
                std::getline(hfs, interfaces_h_data, '\0');
            }

            std::stringstream header_stream;

            rpc_generator::yas_generator::write_files(true,
                *objects,
                header_stream,
                namespaces,
                header_path,
                !suppress_catch_stub_exceptions,
                rethrow_exceptions,
                additional_stub_headers);

            header_stream << ends;

            // compare and write if different
            if (is_different(header_stream, interfaces_h_data))
            {
                ofstream file(header_fs_path);
                file << header_stream.str();
            }
        }

        {
            auto pos = header_path.rfind(".h");
            if (pos == std::string::npos)
            {
                std::cerr << "failed looking for a .h suffix " << header_path << '\n';
                return -1;
            }

            auto file_path = header_path.substr(0, pos) + ".json";

            auto json_schema_fs_path = std::filesystem::path(output_path) / "json_schema" / file_path;

            std::filesystem::create_directories(json_schema_fs_path.parent_path());

            // read the original data and close the files afterwards
            string json_schema_data;

            {
                ifstream hfs(json_schema_fs_path);
                std::getline(hfs, json_schema_data, '\0');
            }

            std::stringstream json_schema_stream;

            // Generate the JSON Schema
            rpc_generator::json_schema::write_json_schema(*objects,
                json_schema_stream,
                module_name); // Use filename as title

            if (is_different(json_schema_stream, json_schema_data))
            {
                ofstream file(json_schema_fs_path);
                file << json_schema_stream.str();
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
