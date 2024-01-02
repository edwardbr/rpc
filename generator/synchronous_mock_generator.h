/*
 *   Copyright (c) 2024 Edward Boggis-Rolfe
 *   All rights reserved.
 */

namespace enclave_marshaller
{
    namespace synchronous_mock_generator
    {
        //entry point
        void write_files(bool from_host, const class_entity& lib, std::ostream& moc, const std::vector<std::string>& namespaces, const std::string& header_filename, const std::list<std::string>& imports);
    }
}