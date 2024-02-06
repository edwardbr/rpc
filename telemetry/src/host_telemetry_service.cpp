#include <filesystem>
#include <algorithm>
#include <spdlog/spdlog.h>
#include <fmt/os.h>

#include <rpc/telemetry/host_telemetry_service.h>
#include <rpc/assert.h>
#include <rpc/service.h>
#include <sstream>

namespace rpc
{
    std::string get_thread_id()
    {
        std::stringstream ssr;
        ssr << std::this_thread::get_id();
        return ssr.str();
    }
    
    bool host_telemetry_service::create(std::shared_ptr<rpc::i_telemetry_service>& service, const std::string& test_suite_name, const std::string& name, const std::filesystem::path& directory) 
    { 
        std::string output_path_name("../../rpc_test_diagram/");
        
        std::filesystem::create_directory(directory);

        auto fixed_name = test_suite_name;
        for(auto& ch : fixed_name)
        {
            if(ch == '/')
                ch = '#';
        }

        auto file_name = directory / (fixed_name + "." + name + ".pu");
        std::string fn = file_name.string();
            
        auto output = ::fopen(fn.c_str(), "w+");
        if(!output)
            return false;

        fmt::println(output, "@startuml");
        fmt::println(output, "title {}.{}", test_suite_name, name);

        service = std::shared_ptr<host_telemetry_service>(new host_telemetry_service(output));
        return true;
    }

    host_telemetry_service::host_telemetry_service(FILE* output) : 
        output_(output)
    {}

    host_telemetry_service::~host_telemetry_service()
    {
        fmt::println(output_, "note left");
        fmt::println(output_, "orphaned services {}", services.size());
        fmt::println(output_, "orphaned impls {}", impls.size());
        fmt::println(output_, "orphaned stubs {}", stubs.size());
        fmt::println(output_, "orphaned service_proxies {}", service_proxies.size());
        fmt::println(output_, "orphaned interface_proxies {}", interface_proxies.size());
        fmt::println(output_, "orphaned object_proxies {}", object_proxies.size());

        std::for_each(services.begin(), services.end(), [this](std::pair<rpc::zone, name_count> const& it)
        {fmt::println(output_, "error service zone_id {} service {} count {}", it.first.id, it.second.name, it.second.count);});
        std::for_each(impls.begin(), impls.end(), [this](std::pair<uint64_t, impl> const& it)
        {fmt::println(output_, "error implementation {} zone_id {} count {}", it.second.name, it.second.zone_id.id, it.second.count);});
        std::for_each(stubs.begin(), stubs.end(), [this](std::pair<zone_object, uint64_t> const& it)
        {fmt::println(output_, "error stub zone_id {} object_id {} count {}", it.first.zone_id.id, it.first.object_id.id, it.second);});
        std::for_each(service_proxies.begin(), service_proxies.end(), [this](std::pair<orig_zone, name_count> const& it)
        {fmt::println(output_, "error service proxy zone_id {} destination_zone_id {} caller_id {} name {} count {}", it.first.zone_id.id, it.first.destination_zone_id.id, it.first.caller_zone_id.id, it.second.name, it.second.count);});
        std::for_each(object_proxies.begin(), object_proxies.end(), [this](std::pair<interface_proxy_id, uint64_t> const& it)
        {fmt::println(output_, "error object_proxy zone_id {} destination_zone_id {} object_id {} count {}", it.first.zone_id.id, it.first.destination_zone_id.id, it.first.object_id.id, it.second);});
        std::for_each(interface_proxies.begin(), interface_proxies.end(), [this](std::pair<interface_proxy_id, name_count> const& it)
        {fmt::println(output_, "error interface_proxy {} zone_id {} destination_zone_id {} object_id {} count {}", it.second.name, it.first.zone_id.id, it.first.destination_zone_id.id, it.first.object_id.id, it.second.count);});


        bool is_heathy = services.empty() && service_proxies.empty() && impls.empty() && stubs.empty() && interface_proxies.empty() && object_proxies.empty();
        if(is_heathy)
        {
            fmt::println(output_, "system is healthy");
        }
        else
        {
            fmt::println(output_, "error system is NOT healthy!");
        }
        fmt::println(output_, "end note");
        fmt::println(output_, "@enduml");
        fclose(output_);
        RPC_ASSERT(is_heathy);
        output_ = 0;
        historical_impls.clear();
    }

    std::string service_alias(rpc::zone zone_id)
    {
        return fmt::format("s{}", zone_id.get_val());
    }

    uint64_t service_order(rpc::zone zone_id)
    {
        return std::min((uint32_t)(zone_id.get_val() * 100000), (uint32_t)999999);
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
        auto entry = services.find(zone_id);
        if(entry == services.end())
        {
            services.emplace(zone_id, name_count{name, 1});
            fmt::println(output_, "participant \"{} zone {}\" as {} order {} #Moccasin", name, zone_id.get_val(), service_alias(zone_id), service_order(zone_id));
            fmt::println(output_, "activate {} #Moccasin", service_alias(zone_id));
        }
        fflush(output_);
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
            auto count = --found->second.count;
            if(count == 0)
            {
                services.erase(found);
                fmt::println(output_, "deactivate {}", service_alias(zone_id));
            }
            else
            {        
                spdlog::error("service still being used! name {} zone_id {}", name, service_alias(zone_id));
                fmt::println(output_, "deactivate {}", service_alias(zone_id));
                fmt::println(output_, "hnote over s{} #red : (still being used!)", service_alias(zone_id));    
            }
        }
        fflush(output_);
    }

    void host_telemetry_service::on_service_try_cast(const char* name, rpc::zone zone_id, rpc::destination_zone destination_zone_id, rpc::caller_zone caller_zone_id, rpc::object object_id, rpc::interface_ordinal interface_id) const
    {
        if(zone_id.as_destination() == destination_zone_id)
            fmt::println(output_, "{} -> {} : try_cast {}", service_alias(zone_id), object_proxy_alias({zone_id}, destination_zone_id, object_id), interface_id.get_val());
        else
            fmt::println(output_, "{} -> {} : try_cast {}", service_alias(zone_id), service_proxy_alias({zone_id}, destination_zone_id, caller_zone_id), interface_id.get_val());
    }

    void host_telemetry_service::on_service_add_ref(const char* name, rpc::zone zone_id, rpc::destination_channel_zone destination_channel_zone_id, rpc::destination_zone destination_zone_id, rpc::object object_id, rpc::caller_channel_zone caller_channel_zone_id, rpc::caller_zone caller_zone_id, rpc::add_ref_options options) const
    {
        auto dest = destination_channel_zone_id.get_val() ? rpc::zone(destination_channel_zone_id.id) : destination_zone_id.as_zone();

        if(rpc::add_ref_options::normal == options)
        {
            if(zone_id != dest)
            {
                fmt::println(output_, "{} -> {} : add_ref", service_alias(zone_id), service_proxy_alias(zone_id, {dest.get_val()}, caller_zone_id));
            }
            else if(object_id != rpc::dummy_object_id)
            {
            //     fmt::println(output_, "{} -> {} : dummy add_ref", service_alias({caller_zone_id.get_val()}), object_stub_alias(zone_id, object_id));
            // }
            // else
            // {
                fmt::println(output_, "{} -> {} : add_ref", service_alias(zone_id), object_stub_alias(zone_id, object_id));
            }
        }
        else if(!!(options & rpc::add_ref_options::build_caller_route) && !!(options & rpc::add_ref_options::build_destination_route))
        {
            fmt::println(output_, "{} -->x {} : add_ref delegate linking", service_alias({caller_zone_id.get_val()}), service_alias(zone_id));
        }
        else
        {
            if(!!(options & rpc::add_ref_options::build_destination_route))
            {
                fmt::println(output_, "{} -[#green]>o {} : add_ref build destination", service_alias(zone_id), service_alias(destination_zone_id.as_zone()));
            }
            if(!!(options & rpc::add_ref_options::build_caller_route))
            {
                fmt::println(output_, "{} o-[#magenta]> {} : add_ref build caller", service_alias({caller_zone_id.get_val()}), service_alias(zone_id));
            }
        }
    }

    void host_telemetry_service::on_service_release(const char* name, rpc::zone zone_id, rpc::destination_channel_zone destination_channel_zone_id, rpc::destination_zone destination_zone_id, rpc::object object_id, rpc::caller_zone caller_zone_id) const
    {
        auto dest = destination_channel_zone_id.get_val() ? rpc::zone(destination_channel_zone_id.id) : destination_zone_id.as_zone();

        if(zone_id != dest)
        {
            fmt::println(output_, "{} -> {} : release", service_alias(zone_id), service_proxy_alias(zone_id, {dest.get_val()}, caller_zone_id));
        }
        else if(object_id != rpc::dummy_object_id)
        {
        //     fmt::println(output_, "hnote over {} : dummy release", service_alias(zone_id));
        // }
        // else
        // {
            fmt::println(output_, "{} -> {} : release", service_alias(zone_id), object_stub_alias(zone_id, object_id));
        }
    }  


    void host_telemetry_service::on_service_proxy_creation(const char* name, rpc::zone zone_id, rpc::destination_zone destination_zone_id, rpc::caller_zone caller_zone_id) const
    {    
        fmt::println(output_, "participant \"{} \\nzone {} \\ndestination {} \\ncaller {}\" as {} order {} #cyan"
            , name
            , zone_id.get_val()
            , destination_zone_id.get_val()
            , caller_zone_id.get_val()
            , service_proxy_alias(zone_id, destination_zone_id, caller_zone_id)
            , service_proxy_order(zone_id, destination_zone_id));
        //fmt::println(output_, "{} --> {} : links to", service_proxy_alias(zone_id, destination_zone_id, caller_zone_id), service_alias(destination_zone_id.as_zone()));
        fmt::println(output_, "activate {} #cyan", service_proxy_alias(zone_id, destination_zone_id, caller_zone_id));
        std::lock_guard g(mux);
        auto found = service_proxies.find(orig_zone{zone_id, destination_zone_id, caller_zone_id});
        if(found == service_proxies.end())
        {
            service_proxies.emplace(orig_zone{zone_id, destination_zone_id, caller_zone_id}, name_count{name, 0});
        }
        fflush(output_);
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
                fmt::println(output_, "deactivate {}", service_proxy_alias(zone_id, destination_zone_id, caller_zone_id));
                fmt::println(output_, "hnote over {} : deleted", service_proxy_alias(zone_id, destination_zone_id, caller_zone_id));
            }
            else
            {
                
                spdlog::error("service still being used! name {} zone_id {} destination_zone_id {} caller_zone_id {}", name, zone_id.get_val(), destination_zone_id.get_val(), caller_zone_id.get_val());
                fmt::println(output_, "deactivate {}", service_proxy_alias(zone_id, destination_zone_id, caller_zone_id));
                fmt::println(output_, "hnote over {} #red : deleted (still being used!)", service_proxy_alias(zone_id, destination_zone_id, caller_zone_id));    
            }        
        }
        fflush(output_);
    }

    void host_telemetry_service::on_service_proxy_try_cast(const char* name, rpc::zone zone_id, rpc::destination_zone destination_zone_id, rpc::caller_zone caller_zone_id, rpc::object object_id, rpc::interface_ordinal interface_id) const
    {
        fmt::println(output_, "{} -> {} : try_cast", service_proxy_alias(zone_id, destination_zone_id, caller_zone_id), service_alias(zone_id));    
        fflush(output_);
    }

    void host_telemetry_service::on_service_proxy_add_ref(const char* name, rpc::zone zone_id, rpc::destination_zone destination_zone_id, rpc::destination_channel_zone destination_channel_zone_id, rpc::caller_zone caller_zone_id, rpc::object object_id, rpc::add_ref_options options) const
    {
        std::string type;
        if(!!(options & rpc::add_ref_options::build_caller_route) && !!(options & rpc::add_ref_options::build_destination_route))
        {
            type = "delegate linking";
        }
        else
        {
            if(!!(options & rpc::add_ref_options::build_destination_route))
            {
                type = "build destination";
            }
            if(!!(options & rpc::add_ref_options::build_caller_route))
            {
                type = "build caller";
            }
        }        
        
        // std::lock_guard g(mux);
        // auto found = service_proxies.find(orig_zone{zone_id, destination_zone_id, caller_zone_id});
        // if(found == service_proxies.end())
        // {
        //     //service_proxies.emplace(orig_zone{zone_id, destination_zone_id, caller_zone_id}, name_count{name, 1});
        //     fmt::println(output_, "object add_ref name with proxy added {} zone_id {} destination_zone_id {} caller_zone_id {}", name, zone_id.get_val(), destination_zone_id.get_val(), caller_zone_id.get_val());
        // }
        // else
        // {
        //     fmt::println(output_, "object add_ref name {} zone_id {} destination_zone_id {} caller_zone_id {} object_id {}", name, zone_id.get_val(), destination_zone_id.get_val(), caller_zone_id.get_val(), object_id.get_val());
        // }
        auto dest = destination_channel_zone_id.get_val() ? rpc::zone(destination_channel_zone_id.id) : destination_zone_id.as_zone();

        if(object_id == rpc::dummy_object_id)
        {
            fmt::println(output_, "{} -> {} : dummy add_ref {} {}", service_proxy_alias(zone_id, destination_zone_id, caller_zone_id), service_alias(dest), type, get_thread_id());
        }
        else
        {
            fmt::println(output_, "{} -> {} : add_ref {} {}", service_proxy_alias(zone_id, destination_zone_id, caller_zone_id), service_alias(dest), type, get_thread_id());    
        }
        fflush(output_);
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
        //     fmt::println(output_, "object release name {} zone_id {} destination_zone_id {} caller_zone_id {} object_id {}", name, zone_id.get_val(), destination_zone_id.get_val(), caller_zone_id.get_val(), object_id.get_val());
        // }
        auto dest = destination_channel_zone_id.get_val() ? rpc::zone(destination_channel_zone_id.id) : destination_zone_id.as_zone();

        if(object_id == rpc::dummy_object_id)
        {
            fmt::println(output_, "{} -> {} : dummy release {}", service_proxy_alias(zone_id, destination_zone_id, caller_zone_id), service_alias(dest), get_thread_id());
        }
        else
        {
            fmt::println(output_, "{} -> {} : release {}", service_proxy_alias(zone_id, destination_zone_id, caller_zone_id), service_alias(dest), get_thread_id());    
        }
        fflush(output_);
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
            auto count = ++found->second.count;
            if(ref_count != count)
            {
                message(level_enum::warn, fmt::format("add_external_ref count does not match zone_id {} destination_zone_id {} caller_zone_id {} {} - {}", zone_id.id, destination_zone_id.id, caller_zone_id.id, ref_count, count).c_str());            
            }
        }

        rpc::zone destination_channel_zone = {};
        if(destination_channel_zone_id == 0 || destination_channel_zone_id == destination_zone_id.as_destination_channel())
        {
            destination_channel_zone = destination_zone_id.as_zone();
        }
        else
        {
            destination_channel_zone = {destination_channel_zone_id.get_val()};
        }
        auto entry = services.find(destination_channel_zone);
        if(entry == services.end())
        {
            services.emplace(destination_channel_zone, name_count{"", 1});
            fmt::println(output_, "participant \"zone {}\" as {} order {} #Moccasin", destination_channel_zone.get_val(), service_alias(destination_channel_zone), service_order(destination_channel_zone));
            fmt::println(output_, "activate {} #Moccasin", service_alias(destination_channel_zone));
        }
        fmt::println(output_, "hnote over {} : add_external_ref {} {}", service_proxy_alias(zone_id, destination_zone_id, caller_zone_id), get_thread_id(), ref_count);
        fflush(output_);
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
            auto count  = --found->second.count;
            if(ref_count != count)
            {
                message(level_enum::warn, fmt::format("release_external_ref count does not match zone_id {} destination_zone_id {} caller_zone_id {} {} - {}", zone_id.id, destination_zone_id.id, caller_zone_id.id, ref_count, count).c_str());
            }
        }
        auto dest = destination_channel_zone_id.get_val();
        if(destination_channel_zone_id == 0 || destination_channel_zone_id == destination_zone_id.as_destination_channel())
        {
            dest = destination_zone_id.get_val();
        }
        
        fmt::println(output_, "hnote over {} : release_external_ref {} {}", service_proxy_alias(zone_id, destination_zone_id, caller_zone_id), get_thread_id(), ref_count);
        fflush(output_);
    }


    void host_telemetry_service::on_impl_creation(const char* name, uint64_t address, rpc::zone zone_id) const
    {
        std::lock_guard g(mux);
        if(historical_impls.find(address) != historical_impls.end())
        {
            RPC_ASSERT(!"historical address reused");
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
                RPC_ASSERT(!"object being registered in two zones");
            else
                RPC_ASSERT(!"new object registered twice");
        }
        
        fmt::println(output_, "participant \"{}\" as {} order {}", name, object_alias(address), object_order(zone_id, address)); 
        fmt::println(output_, "activate {}", object_alias(address));
        fflush(output_);
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
        fmt::println(output_, "deactivate {}", object_alias(address));
        fmt::println(output_, "hnote over {} : deleted", object_alias(address));
        fflush(output_);
    }

    void host_telemetry_service::on_stub_creation(rpc::zone zone_id, rpc::object object_id, uint64_t address) const
    {
        std::lock_guard g(mux);
        auto hi = historical_impls.find(address);
        if(hi != historical_impls.end() && hi->second != zone_id)
        {
            if(hi->second == 0)
                hi->second = zone_id;
            else if(services.find(hi->second) != services.end())
                RPC_ASSERT(!"object already assigned to a different zone");
            else
                hi->second = zone_id;
        }
        else
        {
            historical_impls[address] = zone_id;
        }

        fmt::println(output_, "participant \"{}\" as {} order {}", object_alias(address), object_alias(address), object_order(zone_id, address)); 
        fmt::println(output_, "activate {}", object_alias(address));
        
        stubs.emplace(zone_object{zone_id, object_id}, 0);
        fmt::println(output_, "participant \"object stub\\nzone {}\\nobject {}\" as {} order {} #lime"
            , zone_id.get_val()
            , object_id.get_val()
            , object_stub_alias(zone_id, object_id)
            , object_stub_order(zone_id, object_id)
            );
        fmt::println(output_, "activate {} #lime", object_stub_alias(zone_id, object_id));
        fmt::println(output_, "{} --> {} : links to", object_stub_alias(zone_id, object_id), object_alias(address));
        fflush(output_);
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
                fmt::println(output_, "deactivate {}", object_stub_alias(zone_id, object_id));
            }
            else
            {            
                spdlog::error("stub still being used! zone_id {}", zone_id.get_val());
                fmt::println(output_, "deactivate {}", object_stub_alias(zone_id, object_id));
            }
            fmt::println(output_, "hnote over {} : deleted", object_stub_alias(zone_id, object_id));        
        }
        fflush(output_);
    }

    void host_telemetry_service::on_stub_send(rpc::zone zone_id, rpc::object object_id, rpc::interface_ordinal interface_id, rpc::method method_id) const
    {
        fmt::println(output_, "note over {} : send", object_stub_alias(zone_id, object_id));
        fflush(output_);
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
            fmt::println(output_, "hnote over {} : begin add_ref count {} ", object_stub_alias(zone, object_id), count);
        }
        fflush(output_);
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
            auto new_count = --found->second;
            if(new_count == 0)
            {
                fmt::println(output_, "hnote over {} : release count {}", object_stub_alias(zone, object_id), count);
            }
            else
            {            
                fmt::println(output_, "hnote over {} : release count {}", object_stub_alias(zone, object_id), count);
            }
        }
        fflush(output_);
    }

    void host_telemetry_service::on_object_proxy_creation(rpc::zone zone_id, rpc::destination_zone destination_zone_id, rpc::object object_id, bool add_ref_done) const
    {
        std::lock_guard g(mux);
        object_proxies.emplace(interface_proxy_id{zone_id, destination_zone_id, object_id, {0}}, 1);
        fmt::println(output_, "participant \"object_proxy\\nzone {}\\ndestination {}\\nobject {}\" as {} order {} #pink"
            , zone_id.get_val()
            , destination_zone_id.get_val()
            , object_id.get_val()
            , object_proxy_alias(zone_id, destination_zone_id, object_id)
            , object_proxy_order(zone_id, destination_zone_id, object_id));
        fmt::println(output_, "activate {} #pink", object_proxy_alias(zone_id, destination_zone_id, object_id));
        fmt::println(output_, "{} --> {} : links to", object_proxy_alias(zone_id, destination_zone_id, object_id), object_stub_alias(destination_zone_id.as_zone(), object_id));
        if(add_ref_done)
        {
            fmt::println(output_, "{} -> {} : complete_add_ref", object_proxy_alias(zone_id, destination_zone_id, object_id), service_proxy_alias(zone_id, destination_zone_id, zone_id.as_caller()));    
        }
        else
        {
            fmt::println(output_, "{} -> {} : add_ref", object_proxy_alias(zone_id, destination_zone_id, object_id), service_proxy_alias(zone_id, destination_zone_id, zone_id.as_caller()));    
        }
        fflush(output_);
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
            auto count = --found->second;
            if(count == 0)
            {
                object_proxies.erase(found);
                fmt::println(output_, "deactivate {}", object_proxy_alias(zone_id, destination_zone_id, object_id));
                fmt::println(output_, "{} -> {} : release", object_proxy_alias(zone_id, destination_zone_id, object_id), service_proxy_alias(zone_id, destination_zone_id, zone_id.as_caller()));    
            }
            else
            {            
                spdlog::error("rpc::object proxy still being used! zone_id {} destination_zone_id {} object_id {}", zone_id.get_val(), destination_zone_id.get_val(), object_id.get_val());
                fmt::println(output_, "deactivate {}", object_proxy_alias(zone_id, destination_zone_id, object_id));
            }
        }
        fflush(output_);
    }

    void host_telemetry_service::on_interface_proxy_creation(const char* name, rpc::zone zone_id, rpc::destination_zone destination_zone_id, rpc::object object_id, rpc::interface_ordinal interface_id) const
    {
        std::lock_guard g(mux);
        interface_proxies.emplace(interface_proxy_id{zone_id, destination_zone_id, object_id, interface_id}, name_count{name, 1});
        fmt::println(output_, "hnote over {} : new interface proxy \\n {}", object_proxy_alias(zone_id, destination_zone_id, object_id), name);
        fflush(output_);
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
            auto count = --found->second.count;
            if(count == 0)
            {
                interface_proxies.erase(found);
                fmt::println(output_, "hnote over {} : deleted \\n {} ", object_proxy_alias(zone_id, destination_zone_id, object_id), name);
            }
            else
            {            
                spdlog::error("interface proxy still being used! name {} zone_id {} destination_zone_id {} object_id {}", name, zone_id.get_val(), destination_zone_id.get_val(), object_id.get_val());
                fmt::println(output_, "hnote over {} : deleted \\n {}", object_proxy_alias(zone_id, destination_zone_id, object_id), name);
            }
        }
        fflush(output_);
    }

    void host_telemetry_service::on_interface_proxy_send(const char* name, rpc::zone zone_id, rpc::destination_zone destination_zone_id, rpc::object object_id, rpc::interface_ordinal interface_id, rpc::method method_id) const
    {
        fmt::println(output_, "{} -> {} : {}", object_proxy_alias(zone_id, destination_zone_id, object_id), object_stub_alias(destination_zone_id.as_zone(), object_id), name);
        fflush(output_);
    }

    void host_telemetry_service::message(level_enum level, const char* message) const
    {
        std::string colour;
        switch(level)
        {
        case debug:
            colour = "green";
            break;
        case trace:
            colour = "blue";
            break;
        case info:
            colour = "honeydew";
            break;
        case warn:
            colour = "orangered";
            break;
        case err:
            colour = "red";
            break;
        case off:
            return;
        case critical:
        default:
            colour = "red";
            break;
        }
        fmt::println(output_, "note left #{}: {}", colour, message);
        fflush(output_);
    }

}