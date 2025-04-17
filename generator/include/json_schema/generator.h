#pragma once

#include "coreclasses.h" // Your parser API header
#include <iostream>
#include <string>

namespace json_schema_generator
{

    void write_json_schema(const class_entity& root_entity, std::ostream& os,
                           const std::string& schema_title = "Generated Schema");
} // namespace json_schema_generator
