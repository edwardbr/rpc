#include "cpp_parser.h" // Your parser API header

#include "macro_parser.h"
#include "json_schema_generator.h" // The generator header
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <stack>

// Define the verboseStream and current_import stack if they are used globally by the parser
std::stringstream verboseStream;
extern std::stack<std::string> current_import;

int main(int argc, char* argv[])
{
    if(argc < 3)
    {
        std::cerr << "Usage: " << argv[0] << " <input_idl_file> <output_schema.json>" << std::endl;
        return 1;
    }

    std::string idl_filename = argv[1];
    std::string output_filename = argv[2];

    try
    {
        std::unique_ptr<macro_parser> parser = std::unique_ptr<macro_parser>(new macro_parser());

        // for (auto& define : defines)
        // {
        //     auto elems = split(define, '=');
        //     {
        //         macro_parser::definition def;
        //         std::string defName = elems[0];
        //         if (elems.size() > 1)
        //         {
        //             def.m_substitutionString = elems[1];
        //         }
        //         parser->AddDefine(defName, def);
        //     }
        // }

        std::string pre_parsed_data;

        {
            macro_parser::definition def;
            def.m_substitutionString = "1";
            parser->AddDefine("GENERATOR", def);
        }

        std::vector<std::filesystem::path> parsed_paths;
        // for (auto& path : include_paths)
        // {
        //     parsed_paths.emplace_back(std::filesystem::canonical(path, ec).make_preferred());
        // }

        std::vector<std::string> loaded_includes;

        std::stringstream output_stream;
        auto r = parser->load(output_stream, idl_filename.c_str(), parsed_paths, loaded_includes);
        if(!r)
        {
            std::cerr << "unable to load " << idl_filename << '\n';
            return -1;
        }
        pre_parsed_data = output_stream.str();

        // Create the root entity for parsing
        // Assuming the root represents the global scope or a library
        // Use 'header' spec initially, adjust if needed.
        class_entity root_entity(nullptr, interface_spec::header);
        root_entity.set_name("__global__"); // Give the root a name if helpful

        // Load and parse the IDL file
        // if(!root_entity.load(idl_filename.c_str(), false))
        // {
        //     std::cerr << "Error loading or parsing IDL file: " << idl_filename << std::endl;
        //     // Check verboseStream if your parser writes errors there
        //     // std::cerr << "Parser output:\n" << verboseStream.str() << std::endl;
        //     return 1;
        // }

        const auto* ppdata = pre_parsed_data.data();
        root_entity.parse_structure(ppdata, true, false);

        // Open the output file
        std::ofstream output_file(output_filename);
        if(!output_file)
        {
            std::cerr << "Error opening output file: " << output_filename << std::endl;
            return 1;
        }

        std::cout << "Generating JSON Schema to " << output_filename << "..." << std::endl;

        // Generate the JSON Schema
        json_schema_generator::write_json_schema(root_entity, output_file, idl_filename); // Use filename as title

        output_file.close();
        std::cout << "JSON Schema generation complete." << std::endl;
    }
    catch(const std::exception& e)
    {
        std::cerr << "An error occurred: " << e.what() << std::endl;
        return 1;
    }
    catch(...)
    {
        std::cerr << "An unknown error occurred." << std::endl;
        return 1;
    }

    return 0;
}