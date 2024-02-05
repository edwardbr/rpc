#pragma once

#include <unordered_map>
#include <string>
#include <mutex>
#include <fstream>
#include <filesystem>

#include <rpc/types.h>
#include <rpc/telemetry/i_telemetry_service.h>

namespace rpc
{
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
            rpc::zone zone_id = {0};
            rpc::destination_zone destination_zone_id = {0};
            rpc::caller_zone caller_zone_id = {0};
            bool operator == (const orig_zone& other) const
            {
                return zone_id == other.zone_id && destination_zone_id == other.destination_zone_id && caller_zone_id == other.caller_zone_id;
            }  
        };

        struct orig_zone_hash
        {
            std::size_t operator()(orig_zone const& s) const noexcept
            {
                std::size_t h1 = std::hash<uint64_t>{}(s.zone_id.id);
                std::size_t h2 = std::hash<uint64_t>{}(s.destination_zone_id.id);
                std::size_t h3 = std::hash<uint64_t>{}(s.caller_zone_id.id);
                return h1 ^ (h2 << 1) ^ (h3 << 2); // or use boost::hash_combine
            }
        };

        struct interface_proxy_id
        {
            bool operator == (const interface_proxy_id& other) const
            {
            return  zone_id == other.zone_id &&
                    destination_zone_id == other.destination_zone_id &&
                    object_id == other.object_id &&
                    interface_id == other.interface_id;
            }
            rpc::zone zone_id = {0};
            rpc::destination_zone destination_zone_id = {0};
            rpc::object object_id = {0};
            rpc::interface_ordinal interface_id = {0};
        };

        struct interface_proxy_id_hash
        {
            std::size_t operator()(interface_proxy_id const& s) const noexcept
            {
                std::size_t h1 = std::hash<uint64_t>{}(s.zone_id.id);
                std::size_t h2 = std::hash<uint64_t>{}(s.destination_zone_id.id);
                std::size_t h3 = std::hash<uint64_t>{}(s.object_id.id);
                return h1 ^ (h2 << 1) ^ (h3 << 2); // or use boost::hash_combine
            }
        };

        struct impl
        {
            rpc::zone zone_id;
            std::string name;
            uint_fast64_t count;
        };

        mutable std::mutex mux;
        mutable std::unordered_map<rpc::zone, name_count> services;
        mutable std::unordered_map<orig_zone, name_count, orig_zone_hash> service_proxies;
        mutable std::unordered_map<uint64_t, rpc::zone> historical_impls;
        mutable std::unordered_map<uint64_t, impl> impls;
        mutable std::unordered_map<zone_object, uint64_t, zone_object_hash> stubs;
        mutable std::unordered_map<interface_proxy_id, name_count, interface_proxy_id_hash> interface_proxies;
        mutable std::unordered_map<interface_proxy_id, uint64_t, interface_proxy_id_hash> object_proxies;

        FILE* output_ = nullptr;

        host_telemetry_service(FILE* output);

    public:
        static bool create(std::shared_ptr<rpc::i_telemetry_service>& service, const std::string& test_suite_name, const std::string& name, const std::filesystem::path& directory);

        virtual ~host_telemetry_service();

        void on_service_creation(const char* name, rpc::zone zone_id) const override;
        void on_service_deletion(const char* name, rpc::zone zone_id) const override;
        void on_service_try_cast(const char* name, rpc::zone zone_id, rpc::destination_zone destination_zone_id, rpc::caller_zone caller_zone_id, rpc::object object_id, rpc::interface_ordinal interface_id)  const override;
        void on_service_add_ref(const char* name, rpc::zone zone_id, rpc::destination_channel_zone destination_channel_zone_id, rpc::destination_zone destination_zone_id, rpc::object object_id, rpc::caller_channel_zone caller_channel_zone_id, rpc::caller_zone caller_zone_id, rpc::add_ref_options options)  const override;
        void on_service_release(const char* name, rpc::zone zone_id, rpc::destination_channel_zone destination_channel_zone_id, rpc::destination_zone destination_zone_id, rpc::object object_id, rpc::caller_zone caller_zone_id)  const override;

        void on_service_proxy_creation(const char* name, rpc::zone zone_id, rpc::destination_zone destination_zone_id, rpc::caller_zone caller_zone_id) const override;
        void on_service_proxy_deletion(const char* name, rpc::zone zone_id, rpc::destination_zone destination_zone_id, rpc::caller_zone caller_zone_id) const override;
        void on_service_proxy_try_cast(const char* name, rpc::zone zone_id, rpc::destination_zone destination_zone_id, rpc::caller_zone caller_zone_id, rpc::object object_id, rpc::interface_ordinal interface_id) const override;
        void on_service_proxy_add_ref(const char* name, rpc::zone zone_id, rpc::destination_zone destination_zone_id, rpc::destination_channel_zone destination_channel_zone_id, rpc::caller_zone caller_zone_id, rpc::object object_id) const override;
        void on_service_proxy_release(const char* name, rpc::zone zone_id, rpc::destination_zone destination_zone_id, rpc::destination_channel_zone destination_channel_zone_id, rpc::caller_zone caller_zone_id, rpc::object object_id) const override;  
        void on_service_proxy_add_external_ref(const char* name, rpc::zone zone_id, rpc::destination_channel_zone destination_channel_zone_id, rpc::destination_zone destination_zone_id, rpc::caller_zone caller_zone_id, int ref_count) const override;
        void on_service_proxy_release_external_ref(const char* name, rpc::zone zone_id, rpc::destination_channel_zone destination_channel_zone_id, rpc::destination_zone destination_zone_id, rpc::caller_zone caller_zone_id, int ref_count) const override;  

        void on_impl_creation(const char* name, uint64_t address, rpc::zone zone_id) const override;
        void on_impl_deletion(const char* name, uint64_t address, rpc::zone zone_id) const override;

        void on_stub_creation(rpc::zone zone_id, rpc::object object_id, uint64_t address) const override;    
        void on_stub_deletion(rpc::zone zone_id, rpc::object object_id) const override;
        void on_stub_send(rpc::zone zone_id, rpc::object object_id, rpc::interface_ordinal interface_id, rpc::method method_id) const override;
        void on_stub_add_ref(rpc::zone zone_id, rpc::object object_id, rpc::interface_ordinal interface_id, uint64_t count, rpc::caller_zone caller_zone_id) const override;
        void on_stub_release(rpc::zone zone_id, rpc::object object_id, rpc::interface_ordinal interface_id, uint64_t count, rpc::caller_zone caller_zone_id) const override;

        void on_object_proxy_creation(rpc::zone zone_id, rpc::destination_zone destination_zone_id, rpc::object object_id, bool add_ref_done) const override;
        void on_object_proxy_deletion(rpc::zone zone_id, rpc::destination_zone destination_zone_id, rpc::object object_id) const override;

        void on_interface_proxy_creation(const char* name, rpc::zone zone_id, rpc::destination_zone destination_zone_id, rpc::object object_id, rpc::interface_ordinal interface_id) const override;
        void on_interface_proxy_deletion(const char* name, rpc::zone zone_id, rpc::destination_zone destination_zone_id, rpc::object object_id, rpc::interface_ordinal interface_id) const override;
        void on_interface_proxy_send(const char* name, rpc::zone zone_id, rpc::destination_zone destination_zone_id, rpc::object object_id, rpc::interface_ordinal interface_id, rpc::method method_id) const override;

        void message(level_enum level, const char* message) const override;  
    };
}