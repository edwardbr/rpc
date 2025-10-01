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

    // the class that encapsulates an environment or zone
    // only host code can use this class directly other enclaves *may* have access to the i_service_proxy derived interface
    class service_proxy : public i_marshaller, public std::enable_shared_from_this<rpc::service_proxy>
    {
        std::unordered_map<object, std::weak_ptr<object_proxy>> proxies_;
        std::mutex insert_control_;

        const zone zone_id_;
        destination_zone destination_zone_id_ = {0};
        destination_channel_zone destination_channel_zone_ = {0};
        caller_zone caller_zone_id_ = {0};
        std::weak_ptr<service> service_;
        // if this service proxy represents a child service, hold a strong reference to the parent service to prevent premature destruction
        stdex::member_ptr<service> parent_service_ref_;
        bool is_responsible_for_cleaning_up_service_ = true;

        stdex::member_ptr<service_proxy> lifetime_lock_;
        std::atomic<int> lifetime_lock_count_ = 0;
        std::atomic<uint64_t> version_ = rpc::get_version();
        encoding enc_ = encoding::enc_default;
        // if a service proxy is pointing to the zones parent zone then it needs to stay alive even if there are no active references going through it
        bool is_parent_channel_ = false;
        std::string name_;

    protected:
        service_proxy(const char* name, destination_zone destination_zone_id, const std::shared_ptr<rpc::service>& svc);

        service_proxy(const service_proxy& other);

        // not thread safe
        bool is_parent_channel() const { return is_parent_channel_; }
        void set_parent_channel(bool val);

    public:
        virtual ~service_proxy();

        std::string get_name() const { return name_; }

        uint64_t get_remote_rpc_version() const { return version_.load(); }
        void update_remote_rpc_version(uint64_t version);
        bool is_unused() const { return lifetime_lock_count_ == 0; }

        encoding get_encoding() const { return enc_; }

        uint64_t set_encoding(encoding enc)
        {
            enc_ = enc;
            return error::OK();
        }

        virtual CORO_TASK(int) connect(rpc::interface_descriptor input_descr, rpc::interface_descriptor& output_descr);

        void add_external_ref();

        int release_external_ref();

        int inner_release_external_ref();

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

        CORO_TASK(void)
        cleanup_after_object(std::shared_ptr<rpc::service> svc,
            std::shared_ptr<rpc::service_proxy> self,
            object object_id,
            int inherited_shared_reference_count,
            int inherited_optimistic_reference_count);

        void on_object_proxy_released(object object_id, int inherited_shared_reference_count, int inherited_optimistic_reference_count);

        std::unordered_map<object, std::weak_ptr<object_proxy>> get_proxies() { return proxies_; }
        int get_lifetime_lock_count() const { return lifetime_lock_count_.load(); }

        // Set parent service reference for child service proxies to prevent premature parent destruction
        void set_parent_service_reference(const std::shared_ptr<rpc::service>& parent_service)
        {
            parent_service_ref_ = parent_service;
        }

        virtual std::shared_ptr<rpc::service_proxy> clone() = 0;
        std::shared_ptr<rpc::service_proxy> clone_for_zone(
            destination_zone destination_zone_id, caller_zone caller_zone_id);

        // the zone where this proxy is created
        zone get_zone_id() const { return zone_id_; }
        // the ultimate zone where this proxy is calling
        destination_zone get_destination_zone_id() const { return destination_zone_id_; }
        // the intermediate zone where this proxy is calling
        destination_channel_zone get_destination_channel_zone_id() const { return destination_channel_zone_; }
        caller_zone get_caller_zone_id() const { return caller_zone_id_; }

        // the service that this proxy lives in
        std::shared_ptr<rpc::service> get_operating_zone_service() const { return service_.lock(); }

        enum class object_proxy_creation_rule
        {
            DO_NOTHING,
            ADD_REF_IF_NEW,
            RELEASE_IF_NOT_NEW,
        };

        CORO_TASK(std::shared_ptr<rpc::object_proxy>)
        get_or_create_object_proxy(object object_id,
            object_proxy_creation_rule rule,
            bool new_proxy_added,
            known_direction_zone known_direction_zone_id,
            bool is_optimistic);

        friend service;
        friend child_service;
    };
}
