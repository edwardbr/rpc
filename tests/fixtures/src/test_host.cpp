/*
 *   Copyright (c) 2024 Edward Boggis-Rolfe
 *   All rights reserved.
 */

#include "test_host.h"
#include <rpc/error_codes.h>
#include <atomic>
#include <csignal>
#include <iostream>
#include <execinfo.h>
#include <cstring>

// Global atomic counters to track host::look_up_app calls
std::atomic<int> lookup_enter_count{0};
std::atomic<int> lookup_exit_count{0};

void print_call_stack() {
    void *array[20];
    size_t size;
    char **strings;
    
    size = backtrace(array, 20);
    strings = backtrace_symbols(array, size);
    
    std::cout << "Call stack (" << size << " frames):" << std::endl;
    for (size_t i = 0; i < size; i++) {
        std::cout << "  " << i << ": " << strings[i] << std::endl;
        // Look for key patterns to identify the crash type
        if (strstr(strings[i], "on_object_proxy_released") != nullptr) {
            std::cout << "  >> PROXY CLEANUP CRASH DETECTED" << std::endl;
        }
        if (strstr(strings[i], "unable to find object") != nullptr) {
            std::cout << "  >> OBJECT NOT FOUND CRASH DETECTED" << std::endl;
        }
    }
    free(strings);
}

void segfault_handler(int sig) {
    std::cout << "CRASH DETECTED! Signal: " << sig << std::endl;
    std::cout << "lookup_enter_count: " << lookup_enter_count.load() 
              << ", lookup_exit_count: " << lookup_exit_count.load() << std::endl;
    std::cout << "Calls in progress: " << (lookup_enter_count.load() - lookup_exit_count.load()) << std::endl;
    
    print_call_stack();
    
    signal(sig, SIG_DFL);
    raise(sig);
}

host::host(rpc::zone zone_id)
    : zone_id_(zone_id)
{
#ifdef USE_RPC_TELEMETRY
    if (auto telemetry_service = rpc::telemetry_service_manager::get(); telemetry_service)
        telemetry_service->on_impl_creation("host", (uint64_t)this, zone_id_);
#endif
    // Install signal handler for segfaults
    signal(SIGSEGV, segfault_handler);
    signal(SIGABRT, segfault_handler);
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
    lookup_enter_count.fetch_add(1);
    std::scoped_lock lock(cached_apps_mux_);
    auto it = cached_apps_.find(app_name);
    if (it == cached_apps_.end())
    {
        lookup_exit_count.fetch_add(1);
        return rpc::error::OK();
    }
    app = it->second;
    lookup_exit_count.fetch_add(1);
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