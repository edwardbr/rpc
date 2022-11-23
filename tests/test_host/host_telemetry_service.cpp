#include <host_telemetry_service.h>
#include <spdlog/spdlog.h>

host_telemetry_service::~host_telemetry_service()
{
    spdlog::info("orphaned services {}", services.size());
    spdlog::info("orphaned service_proxies {}", service_proxies.size());
    spdlog::info("orphaned impls {}", impls.size());
    spdlog::info("orphaned stubs {}", stubs.size());
    spdlog::info("orphaned interface_proxies {}", interface_proxies.size());
    spdlog::info("orphaned object_proxies {}", object_proxies.size());

    std::for_each(services.begin(), services.end(), [](std::pair<uint64_t, name_count> const& it){spdlog::warn("service {} zone {} count {}", it.second.name, it.first, it.second.count);});
    std::for_each(service_proxies.begin(), service_proxies.end(), [](std::pair<orig_zone, name_count> const& it){spdlog::warn("service_proxies {} originating_zone {} zone {} count {}", it.second.name, it.first.first, it.first.second, it.second.count);});
    std::for_each(impls.begin(), impls.end(), [](std::pair<impl, uint64_t> const& it){spdlog::warn("impls {} interface_id {} count {}", it.first.name, it.first.interface_id, it.second);});
    std::for_each(stubs.begin(), stubs.end(), [](std::pair<zone_object, name_count> const& it){spdlog::warn("stubs {} zone {} object_id {} count {}", it.second.name, it.first.first, it.first.second, it.second.count);});
    std::for_each(object_proxies.begin(), object_proxies.end(), [](std::pair<interface_proxy_id, uint64_t> const& it){spdlog::warn("object_proxies originating_zone {} zone {} object_id {} count {}", it.first.originating_zone_id, it.first.zone_id, it.first.object_id, it.second);});
    std::for_each(interface_proxies.begin(), interface_proxies.end(), [](std::pair<interface_proxy_id, name_count> const& it){spdlog::warn("interface_proxies {} originating_zone {} zone {} object_id {} count {}", it.second.name, it.first.originating_zone_id, it.first.zone_id, it.first.object_id, it.second.count);});


    bool is_heathy = services.empty() && service_proxies.empty() && impls.empty() && stubs.empty() && interface_proxies.empty() && object_proxies.empty();
    if(is_heathy)
    {
        spdlog::info("system is healthy");
    }
    else
    {
        spdlog::error("system is NOT healthy!");
    }
}

void host_telemetry_service::on_service_creation(const char* name, uint64_t zone_id) const
{
    std::lock_guard g(mux);
    services.emplace(zone_id, name_count{name, 1});
    spdlog::info("on_service_creation name {} zone_id {}", name, zone_id);
}

void host_telemetry_service::on_service_deletion(const char* name, uint64_t zone_id) const
{
    std::lock_guard g(mux);
    auto found = services.find(zone_id);
    if(found == services.end())
    {
        spdlog::error("service not found name {} zone_id {}", name, zone_id);
    }
    else if(found->second.count == 1)
    {
        services.erase(found);
        spdlog::info("on_service_deletion name {} zone_id {}", name, zone_id);
    }
    else
    {
        
        found->second.count--;
        spdlog::error("service still being used! name {} zone_id {}", name, zone_id);
        spdlog::info("on_service_deletion name {} zone_id {}", name, zone_id);
    }
}
void host_telemetry_service::on_service_proxy_creation(const char* name, uint64_t originating_zone_id, uint64_t zone_id) const
{
    std::lock_guard g(mux);
    service_proxies.emplace(orig_zone{originating_zone_id, zone_id}, name_count{name, 1});
    spdlog::info("on_service_proxy_creation name {} originating_zone_id {} zone_id {}", name, originating_zone_id, zone_id);
}
void host_telemetry_service::on_service_proxy_deletion(const char* name, uint64_t originating_zone_id, uint64_t zone_id) const
{
    std::lock_guard g(mux);
    auto found = service_proxies.find(orig_zone{originating_zone_id, zone_id});
    if(found == service_proxies.end())
    {
        spdlog::error("service_proxy not found name {} originating_zone_id {} zone_id {}", name, originating_zone_id, zone_id);
    }
    else if(found->second.count == 1)
    {
        service_proxies.erase(found);
        spdlog::info("on_service_proxy_deletion name {} originating_zone_id {} zone_id {}", name, originating_zone_id, zone_id);
    }
    else
    {
        
        found->second.count--;
        spdlog::error("service still being used! name {} originating_zone_id {} zone_id {}", name, originating_zone_id, zone_id);
        spdlog::info("on_service_proxy_deletion name {} originating_zone_id {} zone_id {}", name, originating_zone_id, zone_id);
    }        
}
void host_telemetry_service::on_service_proxy_try_cast(const char* name, uint64_t originating_zone_id, uint64_t zone_id, uint64_t object_id, uint64_t interface_id) const
{
    spdlog::info("on_service_proxy_try_cast name {} originating_zone_id {} zone_id {} object_id {} interface_id {}", name, originating_zone_id, zone_id, object_id, interface_id);
}
void host_telemetry_service::on_service_proxy_add_ref(const char* name, uint64_t originating_zone_id, uint64_t zone_id, uint64_t object_id) const
{
    spdlog::info("on_service_proxy_add_ref name {} originating_zone_id {} zone_id {} object_id {}", name, originating_zone_id, zone_id, object_id);
}
void host_telemetry_service::on_service_proxy_release(const char* name, uint64_t originating_zone_id, uint64_t zone_id, uint64_t object_id) const
{
    spdlog::info("on_service_proxy_release name {} originating_zone_id {} zone_id {} object_id {}", name, originating_zone_id, zone_id, object_id);
}

void host_telemetry_service::on_service_proxy_add_external_ref(const char* name, uint64_t originating_zone_id, uint64_t zone_id, int ref_count) const
{
    spdlog::info("on_service_proxy_add_external_ref name {} originating_zone_id {} zone_id {} ref_count {}", name, originating_zone_id, zone_id, ref_count);
}

void host_telemetry_service::on_service_proxy_release_external_ref(const char* name, uint64_t originating_zone_id, uint64_t zone_id, int ref_count) const
{
    spdlog::info("on_service_proxy_release_external_ref name {} originating_zone_id {} zone_id {} ref_count {}", name, originating_zone_id, zone_id, ref_count);
}


void host_telemetry_service::on_impl_creation(const char* name, uint64_t interface_id) const
{
    std::lock_guard g(mux);
    auto found = impls.find(impl{name, interface_id});
    if(found == impls.end())
    {
        impls.emplace(impl{name, interface_id}, 1);
        spdlog::info("on_impl_creation name {} interface_id {} impl {}", name, interface_id, 1);
    }
    else
    {
        found->second++;
        spdlog::info("on_impl_creation name {} interface_id {} impl {}", name, interface_id, found->second);
    }
}
void host_telemetry_service::on_impl_deletion(const char* name, uint64_t interface_id) const
{
    std::lock_guard g(mux);
    auto found = impls.find(impl{name, interface_id});
    if(found == impls.end())
    {
        spdlog::error("impl not found name {} interface_id {}", name, interface_id);
    }
    else
    {
        found->second--;
        spdlog::info("on_impl_deletion name {} interface_id {} impl {}", name, interface_id, found->second);
        if(!found->second)
            impls.erase(found);
    }
}

void host_telemetry_service::on_stub_creation(const char* name, uint64_t zone_id, uint64_t object_id, uint64_t interface_id) const
{
    std::lock_guard g(mux);
    stubs.emplace(zone_object{zone_id, object_id}, name_count{name, 1});
    spdlog::info("on_stub_creation name {} zone_id {} object_id {} interface_id {}", name, zone_id, object_id, interface_id);
}

void host_telemetry_service::on_stub_deletion(const char* name, uint64_t zone_id, uint64_t object_id, uint64_t interface_id) const
{
    std::lock_guard g(mux);
    auto found = stubs.find(zone_object{zone_id, object_id});
    if(found == stubs.end())
    {
        spdlog::error("stub not found name {} zone_id {}", name, zone_id);
    }
    else if(found->second.count == 1)
    {
        stubs.erase(found);
        spdlog::info("on_stub_deletion name {} zone_id {}", name, zone_id);
    }
    else
    {            
        found->second.count--;
        spdlog::error("stub still being used! name {} zone_id {}", name, zone_id);
        spdlog::info("on_stub_deletion name {} zone_id {}", name, zone_id);
    }
}
void host_telemetry_service::on_stub_send(uint64_t zone_id, uint64_t object_id, uint64_t interface_id, uint64_t method_id) const
{
    spdlog::info("on_stub_send zone_id {} object_id {} interface_id {} method_id {}", zone_id, object_id, interface_id, method_id);
}
void host_telemetry_service::on_stub_add_ref(uint64_t zone_id, uint64_t object_id, uint64_t interface_id, uint64_t count) const
{
    spdlog::info("on_stub_add_ref zone_id {} object_id {} interface_id {} count {}", zone_id, object_id, interface_id, count);
}
void host_telemetry_service::on_stub_release(uint64_t zone_id, uint64_t object_id, uint64_t interface_id, uint64_t count) const
{
    spdlog::info("on_stub_release zone_id {} object_id {} interface_id {} count {}", zone_id, object_id, interface_id, count);
}

void host_telemetry_service::on_object_proxy_creation(uint64_t originating_zone_id, uint64_t zone_id, uint64_t object_id) const
{
    std::lock_guard g(mux);
    object_proxies.emplace(interface_proxy_id{originating_zone_id, zone_id, object_id, 0}, 1);
    spdlog::info("on_object_proxy_creation zone_id {} object_id {}", zone_id, object_id);
}
void host_telemetry_service::on_object_proxy_deletion(uint64_t originating_zone_id, uint64_t zone_id, uint64_t object_id) const
{
    std::lock_guard g(mux);
    auto found = object_proxies.find(interface_proxy_id{originating_zone_id, zone_id, object_id, 0});
    if(found == object_proxies.end())
    {
        spdlog::error("object proxy not found object_id {} zone_id {}", object_id, zone_id);
    }
    else if(found->second == 1)
    {
        object_proxies.erase(found);
        spdlog::info("on_object_proxy_deletion object_id {} zone_id {}", object_id, zone_id);
    }
    else
    {            
        found->second--;
        spdlog::error("object proxy still being used! object_id {} zone_id {}", object_id, zone_id);
        spdlog::info("on_object_proxy_deletion object_id {} zone_id {}", object_id, zone_id);
    }
}

void host_telemetry_service::on_interface_proxy_creation(const char* name, uint64_t originating_zone_id, uint64_t zone_id, uint64_t object_id, uint64_t interface_id) const
{
    std::lock_guard g(mux);
    interface_proxies.emplace(interface_proxy_id{originating_zone_id, zone_id, object_id, interface_id}, name_count{name, 1});
    spdlog::info("on_interface_proxy_creation name {} originating_zone_id {} zone_id {} object_id {} interface_id {}", name, originating_zone_id, zone_id, object_id, interface_id);
}
void host_telemetry_service::on_interface_proxy_deletion(const char* name, uint64_t originating_zone_id, uint64_t zone_id, uint64_t object_id, uint64_t interface_id) const
{
    std::lock_guard g(mux);
    auto found = interface_proxies.find(interface_proxy_id{originating_zone_id, zone_id, object_id, interface_id});
    if(found == interface_proxies.end())
    {
        spdlog::error("interface proxy not found name {} originating_zone_id {} zone_id {} object_id {}", name, originating_zone_id, zone_id, object_id);
    }
    else if(found->second.count == 1)
    {
        interface_proxies.erase(found);
        spdlog::info("on_interface_proxy_deletion name {} originating_zone_id {} zone_id {} object_id {}", name, originating_zone_id, zone_id, object_id);
    }
    else
    {            
        found->second.count--;
        spdlog::error("interface proxy still being used! name {} originating_zone_id {} zone_id {} object_id {}", name, originating_zone_id, zone_id, object_id);
        spdlog::info("on_interface_proxy_deletion name {} originating_zone_id {} zone_id {} object_id {}", name, originating_zone_id, zone_id, object_id);
    }
}
void host_telemetry_service::on_interface_proxy_send(const char* name, uint64_t originating_zone_id, uint64_t zone_id, uint64_t object_id, uint64_t interface_id, uint64_t method_id) const
{
    spdlog::info("on_interface_proxy_send name {} originating_zone_id {} zone_id {} object_id {} interface_id {} method_id {}", name, originating_zone_id, zone_id, object_id, interface_id, method_id);
}

void host_telemetry_service::message(level_enum level, const char* message) const
{
    spdlog::log((spdlog::level::level_enum)level, "message {}", message);
}
