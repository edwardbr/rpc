#pragma once

#include <unordered_map>
#include <string>
#include <mutex>


#include <rpc/i_telemetry_service.h>


class host_telemetry_service : public rpc::i_telemetry_service
{
    struct name_count
    {
        std::string name;
        uint64_t count = 0;
    };

    using zone_object = std::pair<uint64_t,uint64_t>;
    using orig_zone = std::pair<uint64_t,uint64_t>;

    struct uint64_pair_hash
    {
        std::size_t operator()(zone_object const& s) const noexcept
        {
            std::size_t h1 = std::hash<uint64_t>{}(s.first);
            std::size_t h2 = std::hash<uint64_t>{}(s.second);
            return h1 ^ (h2 << 1); // or use boost::hash_combine
        }
    };

    struct interface_proxy_id
    {
        interface_proxy_id() = default;
        interface_proxy_id(const interface_proxy_id&) = default;
        interface_proxy_id(interface_proxy_id&&) = default;

        bool operator == (const interface_proxy_id& other) const
        {
        return  originating_zone_id == other.originating_zone_id &&
                zone_id == other.zone_id &&
                object_id == other.object_id;
        }
        uint64_t originating_zone_id = 0;
        uint64_t zone_id = 0;
        uint64_t object_id = 0;
    };

    struct interface_proxy_id_hash
    {
        std::size_t operator()(interface_proxy_id const& s) const noexcept
        {
            std::size_t h1 = std::hash<uint64_t>{}(s.originating_zone_id);
            std::size_t h2 = std::hash<uint64_t>{}(s.zone_id);
            std::size_t h3 = std::hash<uint64_t>{}(s.object_id);
            return h1 ^ (h2 << 1) ^ (h3 << 2); // or use boost::hash_combine
        }
    };

    struct impl
    {
        impl() = default;
        impl(const impl&) = default;
        impl(impl&&) = default;
        
        bool operator == (const impl& other) const
        {
        return  interface_id == other.interface_id &&
                interface_id == other.interface_id;
        }
        
        std::string name;
        uint64_t interface_id;
    };

    struct impl_hash
    {
        std::size_t operator()(impl const& s) const noexcept
        {
            std::size_t h1 = std::hash<std::string>{}(s.name);
            std::size_t h2 = std::hash<uint64_t>{}(s.interface_id);
            return h1 ^ (h2 << 1); // or use boost::hash_combine
        }
    };

    mutable std::mutex mux;
    mutable std::unordered_map<uint64_t, name_count> services;
    mutable std::unordered_map<orig_zone, name_count, uint64_pair_hash> service_proxies;
    mutable std::unordered_map<impl, uint64_t, impl_hash> impls;
    mutable std::unordered_map<zone_object, name_count, uint64_pair_hash> stubs;
    mutable std::unordered_map<interface_proxy_id, name_count, interface_proxy_id_hash> interface_proxies;
    mutable std::unordered_map<interface_proxy_id, uint64_t, interface_proxy_id_hash> object_proxies;


public:
    virtual ~host_telemetry_service();

    void on_service_creation(const char* name, uint64_t zone_id) const override;
    void on_service_deletion(const char* name, uint64_t zone_id) const override;
    void on_service_proxy_creation(const char* name, uint64_t originating_zone_id, uint64_t zone_id) const override;
    void on_service_proxy_deletion(const char* name, uint64_t originating_zone_id, uint64_t zone_id) const override;
    void on_service_proxy_try_cast(const char* name, uint64_t originating_zone_id, uint64_t zone_id, uint64_t object_id, uint64_t interface_id) const override;
    void on_service_proxy_add_ref(const char* name, uint64_t originating_zone_id, uint64_t zone_id, uint64_t object_id) const override;
    void on_service_proxy_release(const char* name, uint64_t originating_zone_id, uint64_t zone_id, uint64_t object_id) const override;  

    void on_impl_creation(const char* name, uint64_t interface_id) const override;
    void on_impl_deletion(const char* name, uint64_t interface_id) const override;

    void on_stub_creation(const char* name, uint64_t zone_id, uint64_t object_id, uint64_t interface_id) const override;    
    void on_stub_deletion(const char* name, uint64_t zone_id, uint64_t object_id, uint64_t interface_id) const override;
    void on_stub_send(uint64_t zone_id, uint64_t object_id, uint64_t interface_id, uint64_t method_id) const override;
    void on_stub_add_ref(uint64_t zone_id, uint64_t object_id, uint64_t interface_id, uint64_t count) const override;
    void on_stub_release(uint64_t zone_id, uint64_t object_id, uint64_t interface_id, uint64_t count) const override;

    void on_object_proxy_creation(uint64_t originating_zone_id, uint64_t zone_id, uint64_t object_id) const override;
    void on_object_proxy_deletion(uint64_t originating_zone_id, uint64_t zone_id, uint64_t object_id) const override;

    void on_interface_proxy_creation(const char* name, uint64_t originating_zone_id, uint64_t zone_id, uint64_t object_id, uint64_t interface_id) const override;
    void on_interface_proxy_deletion(const char* name, uint64_t originating_zone_id, uint64_t zone_id, uint64_t object_id, uint64_t interface_id) const override;
    void on_interface_proxy_send(const char* name, uint64_t originating_zone_id, uint64_t zone_id, uint64_t object_id, uint64_t interface_id, uint64_t method_id) const override;

    void message(level_enum level, const char* message) const override;  

};
