
namespace enclave_marshaller
{
    namespace in_zone_synchronous_generator
    {
        //entry point
        void write_files(bool from_host, const class_entity& lib, std::ostream& hos, std::ostream& cos, std::ostream& sos, const std::vector<std::string>& namespaces, const std::string& header_filename);
    }
}