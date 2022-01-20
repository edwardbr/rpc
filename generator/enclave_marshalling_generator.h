
namespace enclave_marshalling_generator
{
    namespace host_ecall
    {
        //entry point
        void write_files(const Library& lib, std::ostream& hos, std::ostream& cos, std::ostream& sos, const std::vector<std::string>& namespaces, const std::string& header_filename);
    }
}