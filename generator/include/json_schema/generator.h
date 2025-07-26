#pragma once

#include "coreclasses.h" // Your parser API header
#include <iostream>
#include <string>

namespace rpc_generator
{
    namespace json_schema
    {

        void write_json_schema(
            const class_entity& root_entity, std::ostream& os, const std::string& schema_title = "Generated Schema");

    } // namespace json_schema
} // namespace rpc_generator
