/*
 *   Copyright (c) 2025 Edward Boggis-Rolfe
 *   All rights reserved.
 */

#include "test_host.h"
#include <rpc/error_codes.h>

host::host()
{
#ifdef USE_RPC_TELEMETRY
    if (auto telemetry_service = rpc::telemetry_service_manager::get(); telemetry_service)
        telemetry_service->on_impl_creation("host", (uint64_t)this, rpc::service::get_current_service()->get_zone_id());
#endif
}

host::~host()
{
#ifdef USE_RPC_TELEMETRY
    if (auto telemetry_service = rpc::telemetry_service_manager::get(); telemetry_service)
        telemetry_service->on_impl_deletion((uint64_t)this, rpc::service::get_current_service()->get_zone_id());
#endif
}

CORO_TASK(error_code) host::create_enclave(rpc::shared_ptr<yyy::i_example>& target)
{
#ifdef BUILD_ENCLAVE
    rpc::shared_ptr<yyy::i_host> host = shared_from_this();
    auto serv = current_host_service.lock();
    auto new_zone_id = ++(*zone_gen);
    auto err_code
        = serv->connect_to_zone<rpc::enclave_service_proxy>("an enclave", {new_zone_id}, host, target, enclave_path);

    CO_RETURN err_code;
#endif
    RPC_ERROR("Incompatible service - enclave not built");
    CO_RETURN rpc::error::INCOMPATIBLE_SERVICE();
};

// live app registry, it should have sole responsibility for the long term storage of app shared ptrs
CORO_TASK(error_code) host::look_up_app(const std::string& app_name, rpc::shared_ptr<yyy::i_example>& app)
{
    {
        std::scoped_lock lock(cached_apps_mux_);
        auto it = cached_apps_.find(app_name);
        if (it != cached_apps_.end())
        {
            app = it->second;
        }
    }
    CO_RETURN rpc::error::OK();
}

CORO_TASK(error_code) host::set_app(const std::string& name, const rpc::shared_ptr<yyy::i_example>& app)
{
    {
        std::scoped_lock lock(cached_apps_mux_);
        cached_apps_[name] = app;
    }
    CO_RETURN rpc::error::OK();
}

CORO_TASK(error_code) host::unload_app(const std::string& name)
{
    {
        std::scoped_lock lock(cached_apps_mux_);
        cached_apps_.erase(name);
    }
    CO_RETURN rpc::error::OK();
}
