
namespace enclave_marshaller
{
    namespace synchronous_generator
    {
        //entry point
        void write_files(std::string module_name, bool from_host, const class_entity& lib, std::ostream& hos, std::ostream& pos,
                         std::ostream& sos, std::ostream& shos,
                         const std::vector<std::string>& namespaces, const std::string& header_filename,
                         const std::string& stub_header_filename,
                         const std::list<std::string>& imports, std::vector<std::string> additional_headers);
    }
}