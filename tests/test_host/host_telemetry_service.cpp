#include <host_telemetry_service.h>
#include <spdlog/spdlog.h>

#include "gtest/gtest.h"

host_telemetry_service::~host_telemetry_service()
{
    spdlog::info("orphaned services {}", services.size());
    spdlog::info("orphaned service_proxies {}", service_proxies.size());
    spdlog::info("orphaned impls {}", impls.size());
    spdlog::info("orphaned stubs {}", stubs.size());
    spdlog::info("orphaned interface_proxies {}", interface_proxies.size());
    spdlog::info("orphaned object_proxies {}", object_proxies.size());

    std::for_each(services.begin(), services.end(), [](std::pair<rpc::zone, name_count> const& it){spdlog::warn("service {} rpc::zone {} count {}", it.second.name, it.first.id, it.second.count);});
    std::for_each(service_proxies.begin(), service_proxies.end(), [](std::pair<orig_zone, name_count> const& it){spdlog::warn("service_proxies {} originating_zone {} rpc::zone {} count {}", it.second.name, it.first.operating_zone_id.id, it.first.proxy_zone_id.id, it.second.count);});
    std::for_each(impls.begin(), impls.end(), [](std::pair<impl, uint64_t> const& it){spdlog::warn("impls {} interface_id {} count {}", it.first.name, it.first.interface_id.id, it.second);});
    std::for_each(stubs.begin(), stubs.end(), [](std::pair<zone_object, name_count> const& it){spdlog::warn("stubs {} rpc::zone {} object_id {} count {}", it.second.name, it.first.zone_id.id, it.first.object_id.id, it.second.count);});
    std::for_each(object_proxies.begin(), object_proxies.end(), [](std::pair<interface_proxy_id, uint64_t> const& it){spdlog::warn("object_proxies originating_zone {} rpc::zone {} object_id {} count {}", it.first.originating_zone_id.id, it.first.zone_id.id, it.first.object_id.id, it.second);});
    std::for_each(interface_proxies.begin(), interface_proxies.end(), [](std::pair<interface_proxy_id, name_count> const& it){spdlog::warn("interface_proxies {} originating_zone {} rpc::zone {} object_id {} count {}", it.second.name, it.first.originating_zone_id.id, it.first.zone_id.id, it.first.object_id.id, it.second.count);});


    bool is_heathy = services.empty() && service_proxies.empty() && impls.empty() && stubs.empty() && interface_proxies.empty() && object_proxies.empty();
    if(is_heathy)
    {
        spdlog::info("system is healthy");
    }
    else
    {
        spdlog::error("system is NOT healthy!");
    }
    EXPECT_TRUE(is_heathy);
}

void host_telemetry_service::on_service_creation(const char* name, rpc::zone zone_id) const
{
    std::lock_guard g(mux);
    services.emplace(zone_id, name_count{name, 1});
    spdlog::info("new service name {} zone_id {}", name, *zone_id);
}

void host_telemetry_service::on_service_deletion(const char* name, rpc::zone zone_id) const
{
    std::lock_guard g(mux);
    auto found = services.find(zone_id);
    if(found == services.end())
    {
        spdlog::error("service not found name {} zone_id {}", name, *zone_id);
    }
    else if(found->second.count == 1)
    {
        services.erase(found);
        spdlog::info("service deleted name {} zone_id {}", name, *zone_id);
    }
    else
    {
        
        found->second.count--;
        spdlog::error("service still being used! name {} zone_id {}", name, *zone_id);
        spdlog::info("on_service_deletion name {} zone_id {}", name, *zone_id);
    }
}
void host_telemetry_service::on_service_proxy_creation(const char* name, rpc::zone originating_zone_id, rpc::destination_zone zone_id) const
{
    std::lock_guard g(mux);
    service_proxies.emplace(orig_zone{originating_zone_id, *zone_id}, name_count{name, 1});
    spdlog::info("new service_proxy name {} originating_zone_id {} zone_id {}", name, *originating_zone_id, *zone_id);
}
void host_telemetry_service::on_service_proxy_deletion(const char* name, rpc::zone originating_zone_id, rpc::destination_zone zone_id) const
{
    std::lock_guard g(mux);
    auto found = service_proxies.find(orig_zone{originating_zone_id, *zone_id});
    if(found == service_proxies.end())
    {
        spdlog::error("service_proxy not found name {} originating_zone_id {} zone_id {}", name, *originating_zone_id, *zone_id);
    }
    else if(found->second.count == 1)
    {
        service_proxies.erase(found);
        spdlog::info("service_proxy deleted name {} originating_zone_id {} zone_id {}", name, *originating_zone_id, *zone_id);
    }
    else
    {
        
        found->second.count--;
        spdlog::error("service still being used! name {} originating_zone_id {} zone_id {}", name, *originating_zone_id, *zone_id);
        spdlog::info("on_service_proxy_deletion name {} originating_zone_id {} zone_id {}", name, *originating_zone_id, *zone_id);
    }        
}
void host_telemetry_service::on_service_proxy_try_cast(const char* name, rpc::zone originating_zone_id, rpc::destination_zone zone_id, rpc::object object_id, rpc::interface_ordinal interface_id) const
{
    spdlog::info("service_proxy cast name {} originating_zone_id {} zone_id {} object_id {} interface_id {}", name, *originating_zone_id, *zone_id, *object_id, *interface_id);
}
void host_telemetry_service::on_service_proxy_add_ref(const char* name, rpc::zone originating_zone_id, rpc::destination_zone zone_id, rpc::object object_id, rpc::caller_zone caller_zone_id) const
{
    spdlog::info("service_proxy add_ref name {} originating_zone_id {} zone_id {} object_id {}", name, *originating_zone_id, *zone_id, *object_id, *caller_zone_id);
}
void host_telemetry_service::on_service_proxy_release(const char* name, rpc::zone originating_zone_id, rpc::destination_zone zone_id, rpc::object object_id, rpc::caller_zone caller_zone_id) const
{
    spdlog::info("service_proxy release name {} originating_zone_id {} zone_id {} object_id {}", name, *originating_zone_id, *zone_id, *object_id, *caller_zone_id);
}

void host_telemetry_service::on_service_proxy_add_external_ref(const char* name, rpc::zone originating_zone_id, rpc::destination_zone zone_id, int ref_count, rpc::caller_zone caller_zone_id) const
{
    spdlog::info("service_proxy add_external_ref name {} originating_zone_id {} zone_id {} ref_count {}", name, *originating_zone_id, *zone_id, ref_count, *caller_zone_id);
}

void host_telemetry_service::on_service_proxy_release_external_ref(const char* name, rpc::zone originating_zone_id, rpc::destination_zone zone_id, int ref_count, rpc::caller_zone caller_zone_id) const
{
    spdlog::info("service_proxy release_external_ref name {} originating_zone_id {} zone_id {} ref_count {}", name, *originating_zone_id, *zone_id, ref_count, *caller_zone_id);
}


void host_telemetry_service::on_impl_creation(const char* name, rpc::interface_ordinal interface_id) const
{
    std::lock_guard g(mux);
    auto found = impls.find(impl{name, *interface_id});
    if(found == impls.end())
    {
        impls.emplace(impl{name, *interface_id}, 1);
        spdlog::info("new impl name {} interface_id {} impl {}", name, *interface_id, 1);
    }
    else
    {
        found->second++;
        spdlog::info("impl addref name {} interface_id {} impl {}", name, *interface_id, found->second);
    }
}
void host_telemetry_service::on_impl_deletion(const char* name, rpc::interface_ordinal interface_id) const
{
    std::lock_guard g(mux);
    auto found = impls.find(impl{name, *interface_id});
    if(found == impls.end())
    {
        spdlog::error("impl not found name {} interface_id {}", name, *interface_id);
    }
    else
    {
        found->second--;
        spdlog::info("impl release name {} interface_id {} impl {}", name, *interface_id, found->second);
        if(!found->second)
        {
            spdlog::info("impl deleted name {} interface_id {} impl {}", name, *interface_id, found->second);
            impls.erase(found);
        }
    }
}

void host_telemetry_service::on_stub_creation(const char* name, rpc::zone zone_id, rpc::object object_id, rpc::interface_ordinal interface_id) const
{
    std::lock_guard g(mux);
    stubs.emplace(zone_object{zone_id, *object_id}, name_count{name, 1});
    spdlog::info("new stub name {} zone_id {} object_id {} interface_id {}", name, *zone_id, *object_id, *interface_id);
}

void host_telemetry_service::on_stub_deletion(const char* name, rpc::zone zone_id, rpc::object object_id, rpc::interface_ordinal interface_id) const
{
    std::lock_guard g(mux);
    auto found = stubs.find(zone_object{zone_id, *object_id});
    if(found == stubs.end())
    {
        spdlog::error("stub not found name {} zone_id {}", name, *zone_id);
    }
    else if(found->second.count == 1)
    {
        stubs.erase(found);
        spdlog::info("stub deleted name {} zone_id {}", name, *zone_id);
    }
    else
    {            
        found->second.count--;
        spdlog::error("stub still being used! name {} zone_id {}", name, *zone_id);
        spdlog::info("on_stub_deletion name {} zone_id {}", name, *zone_id);
    }
}
void host_telemetry_service::on_stub_send(rpc::zone zone_id, rpc::object object_id, rpc::interface_ordinal interface_id, rpc::method method_id) const
{
    spdlog::info("stub send zone_id {} object_id {} interface_id {} method_id {}", *zone_id, *object_id, *interface_id, *method_id);
}
void host_telemetry_service::on_stub_add_ref(rpc::destination_zone zone_id, rpc::object object_id, rpc::interface_ordinal interface_id, uint64_t count, rpc::caller_zone caller_zone_id) const
{
    spdlog::info("stub addref zone_id {} object_id {} interface_id {} count {}", *zone_id, *object_id, *interface_id, count);
}
void host_telemetry_service::on_stub_release(rpc::destination_zone zone_id, rpc::object object_id, rpc::interface_ordinal interface_id, uint64_t count, rpc::caller_zone caller_zone_id) const
{
    spdlog::info("stub release zone_id {} object_id {} interface_id {} count {}", *zone_id, *object_id, *interface_id, count);
}

void host_telemetry_service::on_object_proxy_creation(rpc::zone originating_zone_id, rpc::destination_zone zone_id, rpc::object object_id) const
{
    std::lock_guard g(mux);
    object_proxies.emplace(interface_proxy_id{originating_zone_id, *zone_id, *object_id, 0}, 1);
    spdlog::info("new object_proxy zone_id {} object_id {}", *zone_id, *object_id);
}
void host_telemetry_service::on_object_proxy_deletion(rpc::zone originating_zone_id, rpc::destination_zone zone_id, rpc::object object_id) const
{
    std::lock_guard g(mux);
    auto found = object_proxies.find(interface_proxy_id{originating_zone_id, *zone_id, *object_id, 0});
    if(found == object_proxies.end())
    {
        spdlog::error("rpc::object proxy not found object_id {} zone_id {}", *object_id, *zone_id);
    }
    else if(found->second == 1)
    {
        object_proxies.erase(found);
        spdlog::info("object_proxy deleted object_id {} zone_id {}", *object_id, *zone_id);
    }
    else
    {            
        found->second--;
        spdlog::error("rpc::object proxy still being used! object_id {} zone_id {}", *object_id, *zone_id);
        spdlog::info("on_object_proxy_deletion object_id {} zone_id {}", *object_id, *zone_id);
    }
}

void host_telemetry_service::on_interface_proxy_creation(const char* name, rpc::zone originating_zone_id, rpc::destination_zone zone_id, rpc::object object_id, rpc::interface_ordinal interface_id) const
{
    std::lock_guard g(mux);
    interface_proxies.emplace(interface_proxy_id{originating_zone_id, *zone_id, *object_id, *interface_id}, name_count{name, 1});
    spdlog::info("new interface_proxy name {} originating_zone_id {} zone_id {} object_id {} interface_id {}", name, *originating_zone_id, *zone_id, *object_id, *interface_id);
}
void host_telemetry_service::on_interface_proxy_deletion(const char* name, rpc::zone originating_zone_id, rpc::destination_zone zone_id, rpc::object object_id, rpc::interface_ordinal interface_id) const
{
    std::lock_guard g(mux);
    auto found = interface_proxies.find(interface_proxy_id{originating_zone_id, *zone_id, *object_id, *interface_id});
    if(found == interface_proxies.end())
    {
        spdlog::error("interface proxy not found name {} originating_zone_id {} zone_id {} object_id {}", name, *originating_zone_id, *zone_id, *object_id);
    }
    else if(found->second.count == 1)
    {
        interface_proxies.erase(found);
        spdlog::info("interface_proxy deleted name {} originating_zone_id {} zone_id {} object_id {}", name, *originating_zone_id, *zone_id, *object_id);
    }
    else
    {            
        found->second.count--;
        spdlog::error("interface proxy still being used! name {} originating_zone_id {} zone_id {} object_id {}", name, *originating_zone_id, *zone_id, *object_id);
        spdlog::info("on_interface_proxy_deletion name {} originating_zone_id {} zone_id {} object_id {}", name, *originating_zone_id, *zone_id, *object_id);
    }
}
void host_telemetry_service::on_interface_proxy_send(const char* name, rpc::zone originating_zone_id, rpc::destination_zone zone_id, rpc::object object_id, rpc::interface_ordinal interface_id, rpc::method method_id) const
{
    spdlog::info("interface_proxy send name {} originating_zone_id {} zone_id {} object_id {} interface_id {} method_id {}", name, *originating_zone_id, *zone_id, *object_id, *interface_id, *method_id);
}

void host_telemetry_service::message(level_enum level, const char* message) const
{
    spdlog::log((spdlog::level::level_enum)level, "{}", message);
}
