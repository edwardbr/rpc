#pragma once

#include <unordered_map>
#include <string>
#include <mutex>


#include <rpc/types.h>
#include <rpc/i_telemetry_service.h>


class host_telemetry_service : public rpc::i_telemetry_service
{
    struct name_count
    {
        std::string name;
        uint64_t count = 0;
    };

    struct zone_object
    {
        rpc::zone zone_id = {0};
        rpc::object object_id = {0};

        bool operator == (const zone_object& other) const
        {
            return zone_id == other.zone_id && object_id == other.object_id;
        }        
    };



    struct zone_object_hash
    {
        std::size_t operator()(zone_object const& s) const noexcept
        {
            std::size_t h1 = std::hash<uint64_t>{}(s.zone_id.id);
            std::size_t h2 = std::hash<uint64_t>{}(s.object_id.id);
            return h1 ^ (h2 << 1); // or use boost::hash_combine
        }
    };    

    struct orig_zone
    {
        rpc::zone operating_zone_id = {0};
        rpc::zone_proxy proxy_zone_id = {0};
        bool operator == (const orig_zone& other) const
        {
            return operating_zone_id == other.operating_zone_id && proxy_zone_id == other.proxy_zone_id;
        }  
    };

    struct orig_zone_hash
    {
        std::size_t operator()(orig_zone const& s) const noexcept
        {
            std::size_t h1 = std::hash<uint64_t>{}(s.operating_zone_id.id);
            std::size_t h2 = std::hash<uint64_t>{}(s.proxy_zone_id.id);
            return h1 ^ (h2 << 1); // or use boost::hash_combine
        }
    };

    struct interface_proxy_id
    {
        bool operator == (const interface_proxy_id& other) const
        {
        return  originating_zone_id == other.originating_zone_id &&
                zone_id == other.zone_id &&
                object_id == other.object_id &&
                interface_id == other.interface_id;
        }
        rpc::zone originating_zone_id = {0};
        rpc::zone_proxy zone_id = {0};
        rpc::object object_id = {0};
        rpc::interface_ordinal interface_id = {0};
    };

    struct interface_proxy_id_hash
    {
        std::size_t operator()(interface_proxy_id const& s) const noexcept
        {
            std::size_t h1 = std::hash<uint64_t>{}(s.originating_zone_id.id);
            std::size_t h2 = std::hash<uint64_t>{}(s.zone_id.id);
            std::size_t h3 = std::hash<uint64_t>{}(s.object_id.id);
            return h1 ^ (h2 << 1) ^ (h3 << 2); // or use boost::hash_combine
        }
    };

    struct impl
    {
        bool operator == (const impl& other) const
        {
        return  interface_id == other.interface_id &&
                interface_id == other.interface_id;
        }
        
        std::string name;
        rpc::interface_ordinal interface_id;
    };

    struct impl_hash
    {
        std::size_t operator()(impl const& s) const noexcept
        {
            std::size_t h1 = std::hash<std::string>{}(s.name);
            std::size_t h2 = std::hash<uint64_t>{}(s.interface_id.id);
            return h1 ^ (h2 << 1); // or use boost::hash_combine
        }
    };

    mutable std::mutex mux;
    mutable std::unordered_map<rpc::zone, name_count> services;
    mutable std::unordered_map<orig_zone, name_count, orig_zone_hash> service_proxies;
    mutable std::unordered_map<impl, uint64_t, impl_hash> impls;
    mutable std::unordered_map<zone_object, name_count, zone_object_hash> stubs;
    mutable std::unordered_map<interface_proxy_id, name_count, interface_proxy_id_hash> interface_proxies;
    mutable std::unordered_map<interface_proxy_id, uint64_t, interface_proxy_id_hash> object_proxies;


public:
    virtual ~host_telemetry_service();

    virtual void on_service_creation(const char* name, rpc::zone zone_id) const override;
    virtual void on_service_deletion(const char* name, rpc::zone zone_id) const override;
    virtual void on_service_proxy_creation(const char* name, rpc::zone originating_zone_id, rpc::zone_proxy zone_id) const override;
    virtual void on_service_proxy_deletion(const char* name, rpc::zone originating_zone_id, rpc::zone_proxy zone_id) const override;
    virtual void on_service_proxy_try_cast(const char* name, rpc::zone originating_zone_id, rpc::zone_proxy zone_id, rpc::object object_id, rpc::interface_ordinal interface_id) const override;
    virtual void on_service_proxy_add_ref(const char* name, rpc::zone originating_zone_id, rpc::zone_proxy zone_id, rpc::object object_id, rpc::caller caller_zone_id) const override;
    virtual void on_service_proxy_release(const char* name, rpc::zone originating_zone_id, rpc::zone_proxy zone_id, rpc::object object_id, rpc::caller caller_zone_id) const override;  
    virtual void on_service_proxy_add_external_ref(const char* name, rpc::zone originating_zone_id, rpc::zone_proxy zone_id, int ref_count, rpc::caller caller_zone_id) const override;
    virtual void on_service_proxy_release_external_ref(const char* name, rpc::zone originating_zone_id, rpc::zone_proxy zone_id, int ref_count, rpc::caller caller_zone_id) const override;  

    virtual void on_impl_creation(const char* name, rpc::interface_ordinal interface_id) const override;
    virtual void on_impl_deletion(const char* name, rpc::interface_ordinal interface_id) const override;

    virtual void on_stub_creation(const char* name, rpc::zone zone_id, rpc::object object_id, rpc::interface_ordinal interface_id) const override;    
    virtual void on_stub_deletion(const char* name, rpc::zone zone_id, rpc::object object_id, rpc::interface_ordinal interface_id) const override;
    virtual void on_stub_send(rpc::zone zone_id, rpc::object object_id, rpc::interface_ordinal interface_id, rpc::method method_id) const override;
    virtual void on_stub_add_ref(rpc::zone_proxy zone_id, rpc::object object_id, rpc::interface_ordinal interface_id, uint64_t count, rpc::caller caller_zone_id) const override;
    virtual void on_stub_release(rpc::zone_proxy zone_id, rpc::object object_id, rpc::interface_ordinal interface_id, uint64_t count, rpc::caller caller_zone_id) const override;

    virtual void on_object_proxy_creation(rpc::zone originating_zone_id, rpc::zone_proxy zone_id, rpc::object object_id) const override;
    virtual void on_object_proxy_deletion(rpc::zone originating_zone_id, rpc::zone_proxy zone_id, rpc::object object_id) const override;

    virtual void on_interface_proxy_creation(const char* name, rpc::zone originating_zone_id, rpc::zone_proxy zone_id, rpc::object object_id, rpc::interface_ordinal interface_id) const override;
    virtual void on_interface_proxy_deletion(const char* name, rpc::zone originating_zone_id, rpc::zone_proxy zone_id, rpc::object object_id, rpc::interface_ordinal interface_id) const override;
    virtual void on_interface_proxy_send(const char* name, rpc::zone originating_zone_id, rpc::zone_proxy zone_id, rpc::object object_id, rpc::interface_ordinal interface_id, rpc::method method_id) const override;

    void message(level_enum level, const char* message) const override;  

};
