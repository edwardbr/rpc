/*
 *   Copyright (c) 2025 Edward Boggis-Rolfe
 *   All rights reserved.
 */

#include <rpc/telemetry/i_telemetry_service.h>
#include <rpc/telemetry/multiplexing_telemetry_service.h>

namespace rpc
{
    // Global telemetry service definition for host builds
#ifndef _IN_ENCLAVE
    std::shared_ptr<i_telemetry_service> telemetry_service_ = nullptr;
#endif

}
