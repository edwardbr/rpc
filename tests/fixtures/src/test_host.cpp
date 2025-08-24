/*
 *   Copyright (c) 2024 Edward Boggis-Rolfe
 *   All rights reserved.
 */

#include "test_host.h"
#include <rpc/error_codes.h>

host::host(rpc::zone zone_id)
    : zone_id_(zone_id)
{
#ifdef USE_RPC_TELEMETRY
    if (auto telemetry_service = rpc::telemetry_service_manager::get(); telemetry_service)
        telemetry_service->on_impl_creation("host", (uint64_t)this, zone_id_);
#endif
}

host::~host()
{
#ifdef USE_RPC_TELEMETRY
    if (auto telemetry_service = rpc::telemetry_service_manager::get(); telemetry_service)
        telemetry_service->on_impl_deletion((uint64_t)this, zone_id_);
#endif
}

error_code host::create_enclave(rpc::shared_ptr<yyy::i_example>& target)
{
#ifdef BUILD_ENCLAVE
    rpc::shared_ptr<yyy::i_host> host = shared_from_this();
    auto serv = current_host_service.lock();
    auto err_code = serv->connect_to_zone<rpc::enclave_service_proxy>(
        "an enclave", {++(*zone_gen)}, host, target, enclave_path);

    return err_code;
#endif
    LOG_CSTR("ERROR: Incompatible service - enclave not built");
    return rpc::error::INCOMPATIBLE_SERVICE();
}

error_code host::look_up_app(const std::string& app_name, rpc::shared_ptr<yyy::i_example>& app)
{
    std::scoped_lock lock(cached_apps_mux_);
    auto it = cached_apps_.find(app_name);
    if (it == cached_apps_.end())
    {
        return rpc::error::OK();
    }
    app = it->second;
    return rpc::error::OK();
}

error_code host::set_app(const std::string& name, const rpc::shared_ptr<yyy::i_example>& app)
{
    std::scoped_lock lock(cached_apps_mux_);

    cached_apps_[name] = app;
    return rpc::error::OK();
}

error_code host::unload_app(const std::string& name)
{
    std::scoped_lock lock(cached_apps_mux_);
    cached_apps_.erase(name);
    return rpc::error::OK();
}