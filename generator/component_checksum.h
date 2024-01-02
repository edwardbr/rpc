/*
 *   Copyright (c) 2024 Edward Boggis-Rolfe
 *   All rights reserved.
 */
#pragma once

#include "coreclasses.h"

namespace component_checksum
{
    void write_namespace(const class_entity& lib, std::filesystem::path& output_path);
}