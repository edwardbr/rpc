#include <iostream>
#include <string>
#include <sstream>
#include <filesystem>
#include <fstream>

#include <clipp.h>

#include "commonfuncs.h"
#include "macro_parser.h"

#include <coreclasses.h>

#include "in_zone_synchronous_generator.h"

using namespace std;


std::stringstream verboseStream;

namespace javascript_json{	namespace json	{		extern string namespace_name;	}}

int main(const int argc, char* argv[])
{
	string rootIdl;
	string headerPath;
	string proxyPath;
	string stubPath;
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
        clipp::required("-s", "--stub").doc("the generated stub relative filename") & clipp::value("stub",stubPath),
		clipp::repeatable(clipp::option("-p", "--path") & clipp::value("path",include_paths)).doc("locations of include files used by the idl"),
		clipp::option("-n","--namespace").doc("namespace of the generated interface") & clipp::value("namespace",namespaces),
		clipp::option("-d","--dump_preprocessor_output_and_die").set(dump_preprocessor_output_and_die).doc("dump preprocessor output and die"),
		clipp::repeatable(clipp::option("-D") & clipp::value("define",defines)).doc("macro define"),
		clipp::any_other(wrong_elements)
    );

	clipp::parsing_result res = clipp::parse(argc, argv, cli);
	if(!wrong_elements.empty())
	{
		std::cout << "Error unrecognised parameters\n";
		for(auto& element : wrong_elements)
		{
			std::cout << element << "\n";
		}		

		std::cout << "Please read documentation\n" << clipp::make_man_page(cli, argv[0]);
		return -1;
	}
    if(!res) 
	{
		cout << clipp::make_man_page(cli, argv[0]);
		return -1;
	}

	std::replace( headerPath.begin(), headerPath.end(), '\\', '/'); 
	std::replace( proxyPath.begin(), proxyPath.end(), '\\', '/'); 
	std::replace( stubPath.begin(), stubPath.end(), '\\', '/'); 
	std::replace( output_path.begin(), output_path.end(), '\\', '/'); 

    std::unique_ptr<macro_parser> parser = std::unique_ptr<macro_parser>(new macro_parser());

	for(auto& define : defines)
	{
		auto elems = split(define, '=');
		{
			macro_parser::definition def;
			std::string defName = elems[0];
			if(elems.size() > 1)
			{
				def.m_substitutionString = elems[1];
			}
			//std::cout << "#define: " << defName << " " << def.m_substitutionString << "\n";
			parser->AddDefine(defName, def);
		}
	}

	std::string pre_parsed_data;
	
	try
	{
		{
			macro_parser::definition def;
			def.m_substitutionString = "1";
			parser->AddDefine("GENERATOR", def);

		}

		/*{
			macro_parser::definition def;
			def.m_substitutionString = "unsigned int ";
			parser->AddDefine("size_t", def);
		}*/
		std::error_code ec;

		auto idl = std::filesystem::absolute(rootIdl, ec);
		if(!std::filesystem::exists(idl))
		{
			cout << "Error file " << idl << " does not exist";
			return -1;
		}

		std::vector<std::filesystem::path> parsed_paths;
		for(auto& path : include_paths)
		{
			parsed_paths.emplace_back(std::filesystem::canonical(path, ec).make_preferred());
		}

        std::vector<std::string> loaded_includes;

		std::stringstream output_stream;
		auto r = parser->load(output_stream, rootIdl, parsed_paths, loaded_includes);
		pre_parsed_data = output_stream.str();
		if(dump_preprocessor_output_and_die)
		{
			std::cout << pre_parsed_data << '\n';
			return 0;
		}
	}
	catch(std::exception ex)
	{
		std::cout << ex.what() << '\n';
		return -1;
	}

	//load the idl file
	auto objects = std::make_shared<class_entity>(nullptr);
	const auto* ppdata = pre_parsed_data.data();
	objects->parse_structure(ppdata,true);

	try
	{
		string interfaces_h_data;
		string interfaces_proxy_data;
		string interfaces_stub_data;

		auto header_path = std::filesystem::path(output_path) / "include" / headerPath;
		auto proxy_path = std::filesystem::path(output_path) / "src" / proxyPath;
		auto stub_path = std::filesystem::path(output_path) / "src" / stubPath;

		std::filesystem::create_directories(header_path.parent_path());
		std::filesystem::create_directories(proxy_path.parent_path());
		std::filesystem::create_directories(stub_path.parent_path());

		//read the original data and close the files afterwards
		{
			std::error_code ec;
			ifstream hfs(header_path);
			std::getline(hfs, interfaces_h_data, '\0');

			ifstream proxy_fs(proxy_path);
			std::getline(proxy_fs, interfaces_proxy_data, '\0');

			ifstream stub_fs(stub_path);
			std::getline(stub_fs, interfaces_stub_data, '\0');
		}

		std::stringstream header_stream;
		std::stringstream proxy_stream;
		std::stringstream stub_stream;

		//do the generation to the ostrstreams
		{
			enclave_marshaller::in_zone_synchronous_generator::write_files(true, *objects, header_stream, proxy_stream, stub_stream, namespaces, headerPath);

			header_stream << ends;
			proxy_stream << ends;
			stub_stream << ends;
		}

		//compare and write if different
		if(interfaces_h_data != header_stream.str())
		{
			ofstream file(header_path);
			file << header_stream.str();
		}
		if(interfaces_proxy_data != proxy_stream.str())
		{
			ofstream file(proxy_path);
			file << proxy_stream.str();
		}
		if(interfaces_stub_data != stub_stream.str())
		{
			ofstream file(stub_path);
			file << stub_stream.str();
		}

	}
	catch (std::string msg)
	{
		std::cerr << msg << std::endl;
		return 1;
	}

	return 0;
}