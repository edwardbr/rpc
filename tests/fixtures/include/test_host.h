/*
 *   Copyright (c) 2025 Edward Boggis-Rolfe
 *   All rights reserved.
 */

#pragma once

// Standard C++ headers
#include <atomic>
#include <map>
#include <mutex>
#include <string>

// RPC headers
#include <rpc/rpc.h>
#ifdef USE_RPC_TELEMETRY
#include <rpc/telemetry/i_telemetry_service.h>
#endif

// Other headers
#ifdef BUILD_ENCLAVE
#include "untrusted/enclave_marshal_test_u.h"
#include <common/enclave_service_proxy.h>
#endif
#include <example/example.h>
#include "test_globals.h"

class host : public yyy::i_host, public rpc::enable_shared_from_this<host>
{
    // perhaps this should be an unsorted list but map is easier to debug for now
    std::map<std::string, rpc::shared_ptr<yyy::i_example>> cached_apps_;
    std::mutex cached_apps_mux_;

    void* get_address() const override { return (void*)this; }
    const rpc::casting_interface* query_interface(rpc::interface_ordinal interface_id) const override
    {
        if (rpc::match<yyy::i_host>(interface_id))
            return static_cast<const yyy::i_host*>(this);
        return nullptr;
    }

public:
    host();
    virtual ~host();
    CORO_TASK(error_code) create_enclave(rpc::shared_ptr<yyy::i_example>& target) override;
    CORO_TASK(error_code) look_up_app(const std::string& app_name, rpc::shared_ptr<yyy::i_example>& app) override;
    CORO_TASK(error_code) set_app(const std::string& name, const rpc::shared_ptr<yyy::i_example>& app) override;
    CORO_TASK(error_code) unload_app(const std::string& name) override;
};