#include <host_telemetry_service.h>
#include <spdlog/spdlog.h>
#include <fmt/os.h>
#include <rpc/service.h>

#include "gtest/gtest.h"

extern "C"
{
    void rpc_log(const char* str, size_t sz)
    {
#ifdef USE_RPC_LOGGING        
        spdlog::info(std::string(str, sz));
#endif
    }
}

host_telemetry_service::host_telemetry_service(const std::string& test_suite_name, const std::string& name) : 
    test_suite_name_(test_suite_name),
    name_(name)
{
    output = fopen("C:/Dev/Secretarium/core1/build/test.pu", "w+");
    fmt::println(output, "@startuml");
    fmt::println(output, "title {}.{}", test_suite_name_, name_);
}

host_telemetry_service::~host_telemetry_service()
{
    fmt::println(output, "note left");
    fmt::println(output, "orphaned services {}", services.size());
    fmt::println(output, "orphaned impls {}", impls.size());
    fmt::println(output, "orphaned stubs {}", stubs.size());
    fmt::println(output, "orphaned service_proxies {}", service_proxies.size());
    fmt::println(output, "orphaned interface_proxies {}", interface_proxies.size());
    fmt::println(output, "orphaned object_proxies {}", object_proxies.size());

    std::for_each(services.begin(), services.end(), [this](std::pair<rpc::zone, name_count> const& it)
    {fmt::println(output, "error service zone_id {} service {} count {}", it.first.id, it.second.name, it.second.count);});
    std::for_each(impls.begin(), impls.end(), [this](std::pair<uint64_t, impl> const& it)
    {fmt::println(output, "error implementation {} zone_id {} count {}", it.second.name, it.second.zone_id.id, it.second.count);});
    std::for_each(stubs.begin(), stubs.end(), [this](std::pair<zone_object, uint64_t> const& it)
    {fmt::println(output, "error stub zone_id {} object_id {} count {}", it.first.zone_id.id, it.first.object_id.id, it.second);});
    std::for_each(service_proxies.begin(), service_proxies.end(), [this](std::pair<orig_zone, name_count> const& it)
    {fmt::println(output, "error service proxy zone_id {} destination_zone_id {} caller_id {} name {} count {}", it.first.zone_id.id, it.first.destination_zone_id.id, it.first.caller_zone_id.id, it.second.name, it.second.count);});
    std::for_each(object_proxies.begin(), object_proxies.end(), [this](std::pair<interface_proxy_id, uint64_t> const& it)
    {fmt::println(output, "error object_proxy zone_id {} destination_zone_id {} object_id {} count {}", it.first.zone_id.id, it.first.destination_zone_id.id, it.first.object_id.id, it.second);});
    std::for_each(interface_proxies.begin(), interface_proxies.end(), [this](std::pair<interface_proxy_id, name_count> const& it)
    {fmt::println(output, "error interface_proxy {} zone_id {} destination_zone_id {} object_id {} count {}", it.second.name, it.first.zone_id.id, it.first.destination_zone_id.id, it.first.object_id.id, it.second.count);});


    bool is_heathy = services.empty() && service_proxies.empty() && impls.empty() && stubs.empty() && interface_proxies.empty() && object_proxies.empty();
    if(is_heathy)
    {
        fmt::println(output, "system is healthy");
    }
    else
    {
        fmt::println(output, "error system is NOT healthy!");
    }
    fmt::println(output, "end note");
    fmt::println(output, "@enduml");
    fclose(output);
    EXPECT_TRUE(is_heathy);
    output = nullptr;
    historical_impls.clear();
}

std::string service_alias(rpc::zone zone_id)
{
    return fmt::format("s{}", zone_id.get_val());
}

uint64_t service_order(rpc::zone zone_id)
{
    return zone_id.get_val() * 1000000;
}

std::string object_stub_alias(rpc::zone zone_id, rpc::object object_id)
{
    return fmt::format("os_{}_{}", zone_id.get_val(), object_id.get_val());
}

uint64_t object_stub_order(rpc::zone zone_id, rpc::object object_id)
{
    return service_order(zone_id) + object_id.get_val();
}

std::string object_alias(uint64_t address)
{
    return fmt::format("o_{}", address);
}

uint64_t object_order(rpc::zone zone_id, uint64_t address)
{
    return service_order(zone_id) + address % 100;
}

std::string object_proxy_alias(rpc::zone zone_id, rpc::destination_zone destination_zone_id, rpc::object object_id)
{
    return fmt::format("op_{}_{}_{}", zone_id.get_val(), destination_zone_id.get_val(), object_id.get_val());
}

uint64_t object_proxy_order(rpc::zone zone_id, rpc::destination_zone destination_zone_id, rpc::object object_id)
{
    return service_order(zone_id) + destination_zone_id.get_val() * 1000 + object_id.get_val();
}

std::string service_proxy_alias(rpc::zone zone_id, rpc::destination_zone destination_zone_id, rpc::caller_zone caller_zone_id)
{
    return fmt::format("sp{}_{}_{}", zone_id.get_val(), destination_zone_id.get_val(), caller_zone_id.get_val());
}

uint64_t service_proxy_order(rpc::zone zone_id, rpc::destination_zone destination_zone_id)
{
    return service_order(zone_id) + destination_zone_id.get_val() * 10000;
}

void host_telemetry_service::on_service_creation(const char* name, rpc::zone zone_id) const
{
    std::lock_guard g(mux);
    services.emplace(zone_id, name_count{name, 1});
    fmt::println(output, "participant \"{} zone {}\" as {} order {} #Moccasin", name, zone_id.get_val(), service_alias(zone_id), service_order(zone_id));
    fmt::println(output, "activate {} #Moccasin", service_alias(zone_id));
    fflush(output);
}

void host_telemetry_service::on_service_deletion(const char* name, rpc::zone zone_id) const
{
    std::lock_guard g(mux);
    auto found = services.find(zone_id);
    if(found == services.end())
    {
        spdlog::error("service not found name {} zone_id {}", name, zone_id.get_val());
    }
    else 
    {
        found->second.count--;
        if(found->second.count == 0)
        {
            services.erase(found);
            fmt::println(output, "deactivate {}", service_alias(zone_id));
        }
        else
        {        
            spdlog::error("service still being used! name {} zone_id {}", name, service_alias(zone_id));
            fmt::println(output, "deactivate {}", service_alias(zone_id));
            fmt::println(output, "hnote over s{} #red : (still being used!)", service_alias(zone_id));    
        }
    }
    fflush(output);
}

void host_telemetry_service::on_service_try_cast(const char* name, rpc::zone zone_id, rpc::destination_zone destination_zone_id, rpc::caller_zone caller_zone_id, rpc::object object_id, rpc::interface_ordinal interface_id) const
{
    if(zone_id.as_destination() == destination_zone_id)
        fmt::println(output, "{} -> {} : try_cast {}", service_alias(zone_id), object_proxy_alias({zone_id}, destination_zone_id, object_id), interface_id.get_val());
    else
        fmt::println(output, "{} -> {} : try_cast {}", service_alias(zone_id), service_proxy_alias({zone_id}, destination_zone_id, caller_zone_id), interface_id.get_val());
}

void host_telemetry_service::on_service_add_ref(const char* name, rpc::zone zone_id, rpc::destination_channel_zone destination_channel_zone_id, rpc::destination_zone destination_zone_id, rpc::object object_id, rpc::caller_channel_zone caller_channel_zone_id, rpc::caller_zone caller_zone_id) const
{
    auto dest = destination_channel_zone_id.get_val() ? rpc::zone(destination_channel_zone_id.id) : destination_zone_id.as_zone();

    if(zone_id != dest)
    {
        fmt::println(output, "{} -> {} : add_ref", service_alias(zone_id), service_alias(dest));
    }
    else if(object_id != rpc::dummy_object_id)
    {
    //     fmt::println(output, "{} -> {} : dummy add_ref", service_alias({caller_zone_id.get_val()}), object_stub_alias(zone_id, object_id));
    // }
    // else
    // {
        fmt::println(output, "{} -> {} : add_ref", service_alias(zone_id), object_stub_alias(destination_zone_id.as_zone(), object_id));
    }
}

void host_telemetry_service::on_service_release(const char* name, rpc::zone zone_id, rpc::destination_channel_zone destination_channel_zone_id, rpc::destination_zone destination_zone_id, rpc::object object_id, rpc::caller_zone caller_zone_id) const
{
    auto dest = destination_channel_zone_id.get_val() ? rpc::zone(destination_channel_zone_id.id) : destination_zone_id.as_zone();

    if(zone_id != dest)
    {
        fmt::println(output, "{} -> {} : release", service_alias(zone_id), service_alias(dest));
    }
    else if(object_id != rpc::dummy_object_id)
    {
    //     fmt::println(output, "hnote over {} : dummy release", service_alias(zone_id));
    // }
    // else
    // {
        fmt::println(output, "{} -> {} : release", service_alias(zone_id), object_stub_alias(destination_zone_id.as_zone(), object_id));
    }
}  


void host_telemetry_service::on_service_proxy_creation(const char* name, rpc::zone zone_id, rpc::destination_zone destination_zone_id, rpc::caller_zone caller_zone_id) const
{    
    fmt::println(output, "participant \"{} \\nzone {} \\ndestination {} \\ncaller {}\" as {} order {} #cyan"
        , name
        , zone_id.get_val()
        , destination_zone_id.get_val()
        , caller_zone_id.get_val()
        , service_proxy_alias(zone_id, destination_zone_id, caller_zone_id)
        , service_proxy_order(zone_id, destination_zone_id));
    //fmt::println(output, "{} --> {} : links to", service_proxy_alias(zone_id, destination_zone_id, caller_zone_id), service_alias(destination_zone_id.as_zone()));
    fmt::println(output, "activate {} #cyan", service_proxy_alias(zone_id, destination_zone_id, caller_zone_id));
    std::lock_guard g(mux);
    auto found = service_proxies.find(orig_zone{zone_id, destination_zone_id, caller_zone_id});
    if(found == service_proxies.end())
    {
        service_proxies.emplace(orig_zone{zone_id, destination_zone_id, caller_zone_id}, name_count{name, 0});
    }
    fflush(output);
}

void host_telemetry_service::on_service_proxy_deletion(const char* name, rpc::zone zone_id, rpc::destination_zone destination_zone_id, rpc::caller_zone caller_zone_id) const
{
    std::lock_guard g(mux);
    auto found = service_proxies.find(orig_zone{zone_id, destination_zone_id, caller_zone_id});
    if(found == service_proxies.end())
    {
        spdlog::error("service_proxy not found name {} zone_id {} destination_zone_id {} caller_zone_id {}", name, zone_id.get_val(), destination_zone_id.get_val(), caller_zone_id.get_val());
    }
    else 
    {
        if(found->second.count == 0)
        {
            service_proxies.erase(found);
            fmt::println(output, "deactivate {}", service_proxy_alias(zone_id, destination_zone_id, caller_zone_id));
            fmt::println(output, "hnote over {} : deleted", service_proxy_alias(zone_id, destination_zone_id, caller_zone_id));
        }
        else
        {
            
            spdlog::error("service still being used! name {} zone_id {} destination_zone_id {} caller_zone_id {}", name, zone_id.get_val(), destination_zone_id.get_val(), caller_zone_id.get_val());
            fmt::println(output, "deactivate {}", service_proxy_alias(zone_id, destination_zone_id, caller_zone_id));
            fmt::println(output, "hnote over {} #red : deleted (still being used!)", service_proxy_alias(zone_id, destination_zone_id, caller_zone_id));    
        }        
    }
    fflush(output);
}

void host_telemetry_service::on_service_proxy_try_cast(const char* name, rpc::zone zone_id, rpc::destination_zone destination_zone_id, rpc::caller_zone caller_zone_id, rpc::object object_id, rpc::interface_ordinal interface_id) const
{
    fmt::println(output, "{} -> {} : try_cast", service_proxy_alias(zone_id, destination_zone_id, caller_zone_id), service_alias(zone_id));    
    fflush(output);
}

void host_telemetry_service::on_service_proxy_add_ref(const char* name, rpc::zone zone_id, rpc::destination_zone destination_zone_id, rpc::destination_channel_zone destination_channel_zone_id, rpc::caller_zone caller_zone_id, rpc::object object_id) const
{
    // std::lock_guard g(mux);
    // auto found = service_proxies.find(orig_zone{zone_id, destination_zone_id, caller_zone_id});
    // if(found == service_proxies.end())
    // {
    //     //service_proxies.emplace(orig_zone{zone_id, destination_zone_id, caller_zone_id}, name_count{name, 1});
    //     fmt::println(output, "object add_ref name with proxy added {} zone_id {} destination_zone_id {} caller_zone_id {}", name, zone_id.get_val(), destination_zone_id.get_val(), caller_zone_id.get_val());
    // }
    // else
    // {
    //     fmt::println(output, "object add_ref name {} zone_id {} destination_zone_id {} caller_zone_id {} object_id {}", name, zone_id.get_val(), destination_zone_id.get_val(), caller_zone_id.get_val(), object_id.get_val());
    // }
    auto dest = destination_channel_zone_id.get_val() ? rpc::zone(destination_channel_zone_id.id) : destination_zone_id.as_zone();

    if(object_id == rpc::dummy_object_id)
    {
        fmt::println(output, "{} -> {} : dummy add_ref", service_proxy_alias(zone_id, destination_zone_id, caller_zone_id), service_alias(dest));
    }
    else
    {
        fmt::println(output, "{} -> {} : add_ref", service_proxy_alias(zone_id, destination_zone_id, caller_zone_id), service_alias(dest));    
    }
    fflush(output);
}

void host_telemetry_service::on_service_proxy_release(const char* name, rpc::zone zone_id, rpc::destination_zone destination_zone_id, rpc::destination_channel_zone destination_channel_zone_id, rpc::caller_zone caller_zone_id, rpc::object object_id) const
{
    // std::lock_guard g(mux);
    // auto found = service_proxies.find(orig_zone{zone_id, destination_zone_id, caller_zone_id});
    // if(found == service_proxies.end())
    // {
    //     spdlog::error("object release not found name {} zone_id {} destination_zone_id {} caller_zone_id {}", name, zone_id.get_val(), destination_zone_id.get_val(), caller_zone_id.get_val());
    // }
    // else
    // {
    //     fmt::println(output, "object release name {} zone_id {} destination_zone_id {} caller_zone_id {} object_id {}", name, zone_id.get_val(), destination_zone_id.get_val(), caller_zone_id.get_val(), object_id.get_val());
    // }
    auto dest = destination_channel_zone_id.get_val() ? rpc::zone(destination_channel_zone_id.id) : destination_zone_id.as_zone();

    if(object_id == rpc::dummy_object_id)
    {
        fmt::println(output, "{} -> {} : dummy release", service_proxy_alias(zone_id, destination_zone_id, caller_zone_id), service_alias(dest));
    }
    else
    {
        fmt::println(output, "{} -> {} : release", service_proxy_alias(zone_id, destination_zone_id, caller_zone_id), service_alias(dest));    
    }
    fflush(output);
}

void host_telemetry_service::on_service_proxy_add_external_ref(const char* name, rpc::zone zone_id, rpc::destination_channel_zone destination_channel_zone_id, rpc::destination_zone destination_zone_id, rpc::caller_zone caller_zone_id, int ref_count) const
{
    std::lock_guard g(mux);
    auto found = service_proxies.find(orig_zone{zone_id, destination_zone_id, caller_zone_id});
    if(found == service_proxies.end())
    {
        spdlog::error("service_proxy add_external_ref not found name {} zone_id {} destination_zone_id {} caller_zone_id {}", name, zone_id.get_val(), destination_zone_id.get_val(), caller_zone_id.get_val());
    }
    else
    {
        found->second.count++;
        assert(ref_count == found->second.count);
        //fmt::println(output, "service_proxy add_external_ref name {} zone_id {} destination_zone_id {} caller_zone_id {} ref_count {}", name, zone_id.get_val(), destination_zone_id.get_val(), ref_count, caller_zone_id.get_val(), ref_count);
    }

    if(destination_channel_zone_id == 0 || destination_channel_zone_id == destination_zone_id.as_destination_channel())
    {
        fmt::println(output, "{} -> {} : add_external_ref {}", service_proxy_alias(zone_id, destination_zone_id, caller_zone_id), service_alias({destination_zone_id.get_val()}), ref_count);  
    }
    else
    {
        fmt::println(output, "{} -> {} : add_external_ref {}", service_proxy_alias(zone_id, destination_zone_id, caller_zone_id), service_alias({destination_channel_zone_id.get_val()}), ref_count);  
    }
    fflush(output);
}

void host_telemetry_service::on_service_proxy_release_external_ref(const char* name, rpc::zone zone_id, rpc::destination_channel_zone destination_channel_zone_id, rpc::destination_zone destination_zone_id, rpc::caller_zone caller_zone_id, int ref_count) const
{
    std::lock_guard g(mux);
    auto found = service_proxies.find(orig_zone{zone_id, destination_zone_id, caller_zone_id});
    if(found == service_proxies.end())
    {
        spdlog::error("service_proxy release_external_ref not found name {} zone_id {} destination_zone_id {} caller_zone_id {}", name, zone_id.get_val(), destination_zone_id.get_val(), caller_zone_id.get_val());
    }
    else
    {
        found->second.count--;
        assert(ref_count == found->second.count);
        //fmt::println(output, "service_proxy release_external_ref name {} zone_id {} destination_zone_id {} caller_zone_id {} ref_count {}", name, zone_id.get_val(), destination_zone_id.get_val(), ref_count, caller_zone_id.get_val(), ref_count);
    }
    if(destination_channel_zone_id == 0 || destination_channel_zone_id == destination_zone_id.as_destination_channel())
    {
        fmt::println(output, "{} -> {} : release_external_ref {}", service_proxy_alias(zone_id, destination_zone_id, caller_zone_id), service_alias({destination_zone_id.get_val()}), ref_count);  
    }
    else
    {
        fmt::println(output, "{} -> {} : release_external_ref {}", service_proxy_alias(zone_id, destination_zone_id, caller_zone_id), service_alias({destination_channel_zone_id.get_val()}), ref_count);  
    }
    fflush(output);
}


void host_telemetry_service::on_impl_creation(const char* name, uint64_t address, rpc::zone zone_id) const
{
    //assert(zone_id.get_val());
    std::lock_guard g(mux);
    if(historical_impls.find(address) != historical_impls.end())
    {
        assert(!"historical address reused");
    }
    auto found = impls.find(address);
    if(found == impls.end())
    {
        impls.emplace(address, impl{zone_id, name, 1});
        historical_impls[address] = zone_id;
    }
    else
    {
        if(found->second.zone_id != zone_id)
            assert(!"object being registered in two zones");
        else
            assert(!"new object registered twice");
    }
    
    fmt::println(output, "participant \"{}\" as {} order {}", name, object_alias(address), object_order(zone_id, address)); 
    fmt::println(output, "activate {}", object_alias(address));
    fflush(output);
}

void host_telemetry_service::on_impl_deletion(const char* name, uint64_t address, rpc::zone zone_id) const
{
    std::lock_guard g(mux);
    auto found = impls.find(address);
    if(found == impls.end())
    {
        spdlog::error("impl not found name {} interface_id {}", address, name);
    }
    else
    {
        impls.erase(found);
    }
    historical_impls.erase(address);
    fmt::println(output, "deactivate {}", object_alias(address));
    fmt::println(output, "hnote over {} : deleted", object_alias(address));
    fflush(output);
}

void host_telemetry_service::on_stub_creation(rpc::zone zone_id, rpc::object object_id, uint64_t address) const
{
    std::lock_guard g(mux);
    auto hi = historical_impls.find(address);
    if(hi != historical_impls.end() && hi->second != zone_id)
    {
        if(hi->second == 0)
            hi->second = zone_id;
        else
            assert(!"object already assigned to a different zone");
    }
    else
    {
        historical_impls[address] = zone_id;
    }
    
    stubs.emplace(zone_object{zone_id, object_id}, 0);
    fmt::println(output, "participant \"object stub\\nzone {}\\nobject {}\" as {} order {} #lime"
        , zone_id.get_val()
        , object_id.get_val()
        , object_stub_alias(zone_id, object_id)
        , object_stub_order(zone_id, object_id)
        );
    fmt::println(output, "activate {} #lime", object_stub_alias(zone_id, object_id));
    fmt::println(output, "{} --> {} : links to", object_stub_alias(zone_id, object_id), object_alias(address));
    fflush(output);
}

void host_telemetry_service::on_stub_deletion(rpc::zone zone_id, rpc::object object_id) const
{
    std::lock_guard g(mux);
    auto found = stubs.find(zone_object{zone_id, object_id});
    if(found == stubs.end())
    {
        spdlog::error("stub not found zone_id {}", zone_id.get_val());
    }
    else
    {
        if(found->second == 0)
        {
            stubs.erase(found);
            fmt::println(output, "deactivate {}", object_stub_alias(zone_id, object_id));
        }
        else
        {            
            spdlog::error("stub still being used! zone_id {}", zone_id.get_val());
            fmt::println(output, "deactivate {}", object_stub_alias(zone_id, object_id));
        }
        fmt::println(output, "hnote over {} : deleted", object_stub_alias(zone_id, object_id));        
    }
    fflush(output);
}

void host_telemetry_service::on_stub_send(rpc::zone zone_id, rpc::object object_id, rpc::interface_ordinal interface_id, rpc::method method_id) const
{
    fmt::println(output, "note over {} : send", object_stub_alias(zone_id, object_id));
    fflush(output);
}

void host_telemetry_service::on_stub_add_ref(rpc::zone zone, rpc::object object_id, rpc::interface_ordinal interface_id, uint64_t count, rpc::caller_zone caller_zone_id) const
{
    std::lock_guard g(mux);
    auto found = stubs.find(zone_object{zone, object_id});
    if(found == stubs.end())
    {
        spdlog::error("stub not found zone_id {} caller_zone_id {} object_id {}", zone.get_val(), caller_zone_id.get_val(), object_id.get_val());
    }
    else
    {            
        found->second++;
        assert(count == found->second);        
        fmt::println(output, "hnote over {} : begin add_ref count {} ", object_stub_alias(zone, object_id), count);
    }
    fflush(output);
}

void host_telemetry_service::on_stub_release(rpc::zone zone, rpc::object object_id, rpc::interface_ordinal interface_id, uint64_t count, rpc::caller_zone caller_zone_id) const
{
    std::lock_guard g(mux);
    auto found = stubs.find(zone_object{zone, object_id});
    if(found == stubs.end())
    {
        spdlog::error("stub not found zone_id {} caller_zone_id {} object_id {}", zone.get_val(), caller_zone_id.get_val(), object_id.get_val());
    }
    {
        found->second--;
        assert(count == found->second);
        if(found->second == 0)
        {
            fmt::println(output, "hnote over {} : release count {}", object_stub_alias(zone, object_id), count);
        }
        else
        {            
            fmt::println(output, "hnote over {} : release count {}", object_stub_alias(zone, object_id), count);
        }
    }
    fflush(output);
}

void host_telemetry_service::on_object_proxy_creation(rpc::zone zone_id, rpc::destination_zone destination_zone_id, rpc::object object_id, bool add_ref_done) const
{
    std::lock_guard g(mux);
    object_proxies.emplace(interface_proxy_id{zone_id, destination_zone_id, object_id, {0}}, 1);
    fmt::println(output, "participant \"object_proxy\\nzone {}\\ndestination {}\\nobject {}\" as {} order {} #pink"
        , zone_id.get_val()
        , destination_zone_id.get_val()
        , object_id.get_val()
        , object_proxy_alias(zone_id, destination_zone_id, object_id)
        , object_proxy_order(zone_id, destination_zone_id, object_id));
    fmt::println(output, "activate {} #pink", object_proxy_alias(zone_id, destination_zone_id, object_id));
    fmt::println(output, "{} --> {} : links to", object_proxy_alias(zone_id, destination_zone_id, object_id), object_stub_alias(destination_zone_id.as_zone(), object_id));
    if(add_ref_done)
    {
        fmt::println(output, "{} -> {} : complete_add_ref", object_proxy_alias(zone_id, destination_zone_id, object_id), service_proxy_alias(zone_id, destination_zone_id, zone_id.as_caller()));    
    }
    else
    {
        fmt::println(output, "{} -> {} : add_ref", object_proxy_alias(zone_id, destination_zone_id, object_id), service_proxy_alias(zone_id, destination_zone_id, zone_id.as_caller()));    
    }
    fflush(output);
}

void host_telemetry_service::on_object_proxy_deletion(rpc::zone zone_id, rpc::destination_zone destination_zone_id, rpc::object object_id) const
{
    std::lock_guard g(mux);
    auto found = object_proxies.find(interface_proxy_id{zone_id, destination_zone_id, object_id, {0}});
    if(found == object_proxies.end())
    {
        spdlog::error("rpc::object proxy not found zone_id {} destination_zone_id {} object_id {}", zone_id.get_val(), destination_zone_id.get_val(), object_id.get_val());
    }
    else
    {
        found->second--;
        if(found->second == 0)
        {
            object_proxies.erase(found);
            fmt::println(output, "deactivate {}", object_proxy_alias(zone_id, destination_zone_id, object_id));
            fmt::println(output, "{} -> {} : release", object_proxy_alias(zone_id, destination_zone_id, object_id), service_proxy_alias(zone_id, destination_zone_id, zone_id.as_caller()));    
        }
        else
        {            
            spdlog::error("rpc::object proxy still being used! zone_id {} destination_zone_id {} object_id {}", zone_id.get_val(), destination_zone_id.get_val(), object_id.get_val());
            fmt::println(output, "deactivate {}", object_proxy_alias(zone_id, destination_zone_id, object_id));
        }
    }
    fflush(output);
}

void host_telemetry_service::on_interface_proxy_creation(const char* name, rpc::zone zone_id, rpc::destination_zone destination_zone_id, rpc::object object_id, rpc::interface_ordinal interface_id) const
{
    std::lock_guard g(mux);
    interface_proxies.emplace(interface_proxy_id{zone_id, destination_zone_id, object_id, interface_id}, name_count{name, 1});
    fmt::println(output, "hnote over {} : new interface proxy \\n {}", object_proxy_alias(zone_id, destination_zone_id, object_id), name);
    fflush(output);
}

void host_telemetry_service::on_interface_proxy_deletion(const char* name, rpc::zone zone_id, rpc::destination_zone destination_zone_id, rpc::object object_id, rpc::interface_ordinal interface_id) const
{
    std::lock_guard g(mux);
    auto found = interface_proxies.find(interface_proxy_id{zone_id, destination_zone_id, object_id, interface_id});
    if(found == interface_proxies.end())
    {
        spdlog::error("interface proxy not found name {} zone_id {} destination_zone_id {} object_id {}", name, zone_id.get_val(), destination_zone_id.get_val(), object_id.get_val());
    }
    else
    {
        found->second.count--;
        if(found->second.count == 0)
        {
            interface_proxies.erase(found);
            fmt::println(output, "hnote over {} : deleted \\n {} ", object_proxy_alias(zone_id, destination_zone_id, object_id), name);
        }
        else
        {            
            spdlog::error("interface proxy still being used! name {} zone_id {} destination_zone_id {} object_id {}", name, zone_id.get_val(), destination_zone_id.get_val(), object_id.get_val());
            fmt::println(output, "hnote over {} : deleted \\n {}", object_proxy_alias(zone_id, destination_zone_id, object_id), name);
        }
    }
    fflush(output);
}

void host_telemetry_service::on_interface_proxy_send(const char* name, rpc::zone zone_id, rpc::destination_zone destination_zone_id, rpc::object object_id, rpc::interface_ordinal interface_id, rpc::method method_id) const
{
    fmt::println(output, "{} -> {} : {}", object_proxy_alias(zone_id, destination_zone_id, object_id), object_stub_alias(destination_zone_id.as_zone(), object_id), name);
    fflush(output);
}

void host_telemetry_service::message(level_enum level, const char* message) const
{
    fmt::println(output, "note left : {}", message);
    fflush(output);
}
