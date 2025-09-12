/*
 *   Copyright (c) 2025 Edward Boggis-Rolfe
 *   All rights reserved.
 */
#pragma once

#include "coreclasses.h"
#include "writer.h"

namespace fingerprint
{
    uint64_t generate(const class_entity& cls, std::vector<const class_entity*> entity_stack, writer* comment);
}
