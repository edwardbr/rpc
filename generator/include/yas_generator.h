/*
 *   Copyright (c) 2025 Edward Boggis-Rolfe
 *   All rights reserved.
 */

namespace yas_generator
{
    // entry point
    void write_files(bool from_host,
        const class_entity& lib,
        std::ostream& header_stream,
        const std::vector<std::string>& namespaces,
        const std::string& header_filename,
        bool catch_stub_exceptions,
        const std::vector<std::string>& rethrow_exceptions,
        const std::vector<std::string>& additional_stub_headers);
}
