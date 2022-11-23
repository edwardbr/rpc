
namespace enclave_marshaller
{
    namespace synchronous_generator
    {
        //entry point
        void write_files(bool from_host, const class_entity& lib, std::ostream& hos, std::ostream& pos,
                         std::ostream& phos, std::ostream& sos, std::ostream& shos,
                         const std::vector<std::string>& namespaces, const std::string& header_filename,
                         const std::string& proxy_header_filename, const std::string& stub_header_filename,
                         const std::list<std::string>& imports);
    }
}