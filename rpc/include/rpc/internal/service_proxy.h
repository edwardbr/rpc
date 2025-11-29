/*
 *   Copyright (c) 2025 Edward Boggis-Rolfe
 *   All rights reserved.
 */
#pragma once

#include <string>
#include <memory>
#include <unordered_map>
#include <mutex>
#include <atomic>
#include <functional>
#include <vector>

#include <rpc/internal/types.h>
#include <rpc/internal/coroutine_support.h>
#include <rpc/internal/serialiser.h>
#include <rpc/internal/marshaller.h>
#include <rpc/internal/version.h>

#ifdef USE_RPC_TELEMETRY
#include <rpc/telemetry/i_telemetry_service.h>
#endif

// Forward declarations
namespace rpc
{
    class service;
    class child_service;
    class object_proxy;
    class i_marshaller;
    class transport;

    // the class that encapsulates an environment or zone
    // only host code can use this class directly other enclaves *may* have access to the i_service_proxy derived interface
    class service_proxy : 
        public std::enable_shared_from_this<rpc::service_proxy>
    {
        std::unordered_map<object, std::weak_ptr<object_proxy>> proxies_;
        std::mutex insert_control_;

        const zone zone_id_;
        destination_zone destination_zone_id_ = {0};
        std::weak_ptr<service> service_;

        // Transport for routing calls to remote zones
        stdex::member_ptr<transport> transport_;

        std::atomic<uint64_t> version_ = rpc::get_version();
        encoding enc_ = encoding::enc_default;
        std::string name_;

    public:
        service_proxy(const std::string& name, const std::shared_ptr<transport>& transport, const std::shared_ptr<rpc::service>& svc);
        service_proxy(const std::shared_ptr<transport>& transport, destination_zone destination_zone_id);
        service_proxy(const std::string& name, destination_zone destination_zone_id, const std::shared_ptr<rpc::service_proxy>& other);

        virtual ~service_proxy();

        std::string get_name() const { return name_; }

        uint64_t get_remote_rpc_version() const { return version_.load(); }
        void update_remote_rpc_version(uint64_t version);

        encoding get_encoding() const { return enc_; }

        uint64_t set_encoding(encoding enc)
        {
            enc_ = enc;
            return error::OK();
        }

        // Set transport for this service_proxy
        void set_transport(std::shared_ptr<transport> transport)
        {
            transport_ = transport;
        }

        std::shared_ptr<transport> get_transport() const
        {
            return transport_.get_nullable();
        }

        [[nodiscard]] CORO_TASK(int) send_from_this_zone(uint64_t protocol_version,
            rpc::encoding encoding,
            uint64_t tag,
            rpc::object object_id,
            rpc::interface_ordinal interface_id,
            rpc::method method_id,
            size_t in_size_,
            const char* in_buf_,
            std::vector<char>& out_buf_);

        [[nodiscard]] CORO_TASK(int) sp_try_cast(
            destination_zone destination_zone_id, object object_id, std::function<interface_ordinal(uint64_t)> id_getter);

        [[nodiscard]] CORO_TASK(int) sp_add_ref(object object_id,
            caller_channel_zone caller_channel_zone_id,
            add_ref_options build_out_param_channel,
            known_direction_zone known_direction_zone_id,
            uint64_t& ref_count);

        CORO_TASK(int) sp_release(object object_id, release_options options, uint64_t& ref_count);

        void cleanup_after_object(std::shared_ptr<object_proxy> op,bool is_optimistic);

        void on_object_proxy_released(const std::shared_ptr<object_proxy>& op, bool optimistic);

        std::unordered_map<object, std::weak_ptr<object_proxy>> get_proxies() { return proxies_; }

        std::shared_ptr<rpc::service_proxy> clone_for_zone(destination_zone destination_zone_id);

        // the zone where this proxy is created
        zone get_zone_id() const { return zone_id_; }
        // the ultimate zone where this proxy is calling
        destination_zone get_destination_zone_id() const { return destination_zone_id_; }

        // the service that this proxy lives in
        std::shared_ptr<rpc::service> get_operating_zone_service() const { return service_.lock(); }

        enum class object_proxy_creation_rule
        {
            DO_NOTHING,
            ADD_REF_IF_NEW,
            RELEASE_IF_NOT_NEW,
        };

        CORO_TASK(int)
        get_or_create_object_proxy(object object_id,
            object_proxy_creation_rule rule,
            bool new_proxy_added,
            known_direction_zone known_direction_zone_id,
            bool is_optimistic, std::shared_ptr<rpc::object_proxy>& op);

        friend service;
        friend child_service;
    };
}
