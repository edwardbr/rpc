/*
 *   Copyright (c) 2025 Edward Boggis-Rolfe
 *   All rights reserved.
 */
#include <rpc/rpc.h>
#include <cstdio>
#include <limits>

namespace rpc
{
    service_proxy::service_proxy(
        const char* name, destination_zone destination_zone_id, const std::shared_ptr<rpc::service>& svc)
        : zone_id_(svc->get_zone_id())
        , destination_zone_id_(destination_zone_id)
        , caller_zone_id_(svc->get_zone_id().as_caller())
        , service_(svc)
        , name_(name)
    {
#ifdef USE_RPC_TELEMETRY
        if (auto telemetry_service = rpc::get_telemetry_service(); telemetry_service)
        {
            telemetry_service->on_service_proxy_creation(
                svc->get_name().c_str(), name, get_zone_id(), get_destination_zone_id(), svc->get_zone_id().as_caller());
        }
#endif
        RPC_ASSERT(svc != nullptr);
    }

    service_proxy::service_proxy(const service_proxy& other)
        : std::enable_shared_from_this<rpc::service_proxy>(other)
        , zone_id_(other.zone_id_)
        , destination_zone_id_(other.destination_zone_id_)
        , destination_channel_zone_(other.destination_channel_zone_)
        , caller_zone_id_(other.caller_zone_id_)
        , service_(other.service_)
        , lifetime_lock_count_(0)
        , enc_(other.enc_)
        , name_(other.name_)
    {
        RPC_ASSERT(service_.lock() != nullptr);
    }

    service_proxy::~service_proxy()
    {
        if (!proxies_.empty())
        {
#ifdef USE_RPC_LOGGING
            RPC_WARNING("service_proxy destructor: {} proxies still in map for destination_zone={}, caller_zone={}",
                proxies_.size(),
                destination_zone_id_.get_val(),
                caller_zone_id_.get_val());

            // Log details of remaining proxies
            for (const auto& proxy_entry : proxies_)
            {
                auto proxy = proxy_entry.second.lock();
                RPC_WARNING(
                    "  Remaining proxy: object_id={}, valid={}", proxy_entry.first.get_val(), (proxy ? "true" : "false"));
            }
#endif
        }
        RPC_ASSERT(proxies_.empty());
        if (is_responsible_for_cleaning_up_service_)
        {
            auto svc = service_.lock();
            if (svc)
            {
                svc->remove_zone_proxy(destination_zone_id_, caller_zone_id_);
            }
        }
#ifdef USE_RPC_TELEMETRY
        if (auto telemetry_service = rpc::get_telemetry_service(); telemetry_service)
        {
            telemetry_service->on_service_proxy_deletion(get_zone_id(), destination_zone_id_, caller_zone_id_);
        }
#endif
    }

    void service_proxy::set_parent_channel(bool val)
    {
        is_parent_channel_ = val;

        if (lifetime_lock_count_ == 0 && is_parent_channel_ == false)
        {
            RPC_ASSERT(lifetime_lock_.get_nullable());
            lifetime_lock_.reset();
        }
    }

    void service_proxy::update_remote_rpc_version(uint64_t version)
    {
        const auto min_version = std::max<std::uint64_t>(rpc::LOWEST_SUPPORTED_VERSION, 1);
        const auto max_version = rpc::HIGHEST_SUPPORTED_VERSION;
        version_.store(std::clamp(version, min_version, max_version));
    }

    CORO_TASK(int)
    service_proxy::connect(rpc::interface_descriptor input_descr, rpc::interface_descriptor& output_descr)
    {
        std::ignore = input_descr;
        std::ignore = output_descr;
        RPC_ERROR("Zone not supported");
        CO_RETURN rpc::error::ZONE_NOT_SUPPORTED();
    }

    void service_proxy::add_external_ref()
    {
        std::lock_guard g(insert_control_);
        auto count = ++lifetime_lock_count_;
#ifdef USE_RPC_TELEMETRY
        if (auto telemetry_service = rpc::get_telemetry_service(); telemetry_service)
        {
            telemetry_service->on_service_proxy_add_external_ref(
                zone_id_, destination_channel_zone_, destination_zone_id_, caller_zone_id_, count);
        }
#endif
        RPC_ASSERT(count >= 1);
        if (count == 1)
        {
            // Cache lifetime_lock value for consistency throughout function
            RPC_ASSERT(!lifetime_lock_.get_nullable());
            lifetime_lock_ = stdex::member_ptr<service_proxy>(shared_from_this());
            // Use cached value to ensure consistency
            RPC_ASSERT(lifetime_lock_.get_nullable());
        }
    }

    int service_proxy::release_external_ref()
    {
        std::lock_guard g(insert_control_);
        return inner_release_external_ref();
    }

    int service_proxy::inner_release_external_ref()
    {
        auto count = --lifetime_lock_count_;
#ifdef USE_RPC_TELEMETRY
        if (auto telemetry_service = rpc::get_telemetry_service(); telemetry_service)
        {
            telemetry_service->on_service_proxy_release_external_ref(
                zone_id_, destination_channel_zone_, destination_zone_id_, caller_zone_id_, count);
        }
#endif
        RPC_ASSERT(count >= 0);
        if (count == 0 && is_parent_channel_ == false)
        {
            RPC_ASSERT(lifetime_lock_.get_nullable());
            lifetime_lock_.reset();
        }
        return count;
    }

    [[nodiscard]] CORO_TASK(int) service_proxy::send_from_this_zone(uint64_t protocol_version,
        rpc::encoding encoding,
        uint64_t tag,
        rpc::object object_id,
        rpc::interface_ordinal interface_id,
        rpc::method method_id,
        size_t in_size_,
        const char* in_buf_,
        std::vector<char>& out_buf_)
    {
        const auto min_version = std::max<std::uint64_t>(rpc::LOWEST_SUPPORTED_VERSION, 1);
        const auto max_version = rpc::HIGHEST_SUPPORTED_VERSION;
        if (protocol_version < min_version || protocol_version > max_version)
        {
            CO_RETURN rpc::error::INVALID_VERSION();
        }

        auto current_version = version_.load();
        if (protocol_version > current_version)
        {
            CO_RETURN rpc::error::INVALID_VERSION();
        }
        if (protocol_version < current_version)
        {
            version_.store(protocol_version);
        }

        CO_RETURN CO_AWAIT send(protocol_version,
            encoding,
            tag,
            get_zone_id().as_caller_channel(),
            caller_zone_id_,
            destination_zone_id_,
            object_id,
            interface_id,
            method_id,
            in_size_,
            in_buf_,
            out_buf_);
    }

    [[nodiscard]] CORO_TASK(int) service_proxy::sp_try_cast(
        destination_zone destination_zone_id, object object_id, std::function<interface_ordinal(uint64_t)> id_getter)
    {
        auto original_version = version_.load();
        auto version = original_version;
        const auto min_version = rpc::LOWEST_SUPPORTED_VERSION ? rpc::LOWEST_SUPPORTED_VERSION : 1;
        int last_error = rpc::error::INVALID_VERSION();
        while (version >= min_version)
        {
            auto if_id = id_getter(version);
#ifdef USE_RPC_TELEMETRY
            if (auto telemetry_service = rpc::get_telemetry_service(); telemetry_service)
            {
                telemetry_service->on_service_proxy_try_cast(
                    get_zone_id(), destination_zone_id, get_caller_zone_id(), object_id, if_id);
            }
#endif
            auto ret = CO_AWAIT try_cast(version, destination_zone_id, object_id, if_id);
            if (ret != rpc::error::INVALID_VERSION() && ret != rpc::error::INCOMPATIBLE_SERVICE())
            {
                if (original_version != version)
                {
                    version_.compare_exchange_strong(original_version, version);
                }
                CO_RETURN ret;
            }
            last_error = ret;
            if (version == min_version)
            {
                break;
            }
            version--;
        }
        RPC_ERROR("Incompatible service version in connect");
        CO_RETURN last_error;
    }

    [[nodiscard]] CORO_TASK(int) service_proxy::sp_add_ref(object object_id,
        caller_channel_zone caller_channel_zone_id,
        add_ref_options build_out_param_channel,
        known_direction_zone known_direction_zone_id,
        uint64_t& ref_count)
    {
#ifdef USE_RPC_TELEMETRY
        if (auto telemetry_service = rpc::get_telemetry_service(); telemetry_service)
        {
            telemetry_service->on_service_proxy_add_ref(get_zone_id(),
                destination_zone_id_,
                destination_channel_zone_,
                get_caller_zone_id(),
                object_id,
                build_out_param_channel);
        }
#endif

        auto original_version = version_.load();
        auto version = original_version;
        const auto min_version = rpc::LOWEST_SUPPORTED_VERSION ? rpc::LOWEST_SUPPORTED_VERSION : 1;
        int last_error = rpc::error::INVALID_VERSION();
        while (version >= min_version)
        {
            auto attempt = CO_AWAIT add_ref(version,
                destination_channel_zone_,
                destination_zone_id_,
                object_id,
                caller_channel_zone_id,
                caller_zone_id_,
                known_direction_zone_id,
                build_out_param_channel,
                ref_count);
            if (attempt != rpc::error::INVALID_VERSION() && attempt != rpc::error::INCOMPATIBLE_SERVICE())
            {
                if (original_version != version)
                {
                    version_.compare_exchange_strong(original_version, version);
                }
                CO_RETURN attempt;
            }
            last_error = attempt;
            if (version == min_version)
            {
                break;
            }
            version--;
        }
        RPC_ERROR("Incompatible service version in sp_add_ref");
        CO_RETURN last_error;
    }

    CORO_TASK(int) service_proxy::sp_release(object object_id, uint64_t& ref_count)
    {
#ifdef USE_RPC_TELEMETRY
        if (auto telemetry_service = rpc::get_telemetry_service(); telemetry_service)
        {
            telemetry_service->on_service_proxy_release(
                get_zone_id(), destination_zone_id_, destination_channel_zone_, get_caller_zone_id(), object_id);
        }
#endif

        auto original_version = version_.load();
        auto version = original_version;
        const auto min_version = rpc::LOWEST_SUPPORTED_VERSION ? rpc::LOWEST_SUPPORTED_VERSION : 1;
        int last_error = rpc::error::INVALID_VERSION();
        while (version >= min_version)
        {
            auto ret = CO_AWAIT release(version, destination_zone_id_, object_id, caller_zone_id_, release_options::normal, ref_count);
            if (ret != rpc::error::INVALID_VERSION() && ret != rpc::error::INCOMPATIBLE_SERVICE())
            {
                if (original_version != version)
                {
                    version_.compare_exchange_strong(original_version, version);
                }
                CO_RETURN ret;
            }
            last_error = ret;
            if (version == min_version)
            {
                break;
            }
            version--;
        }
        RPC_ERROR("Incompatible service version in sp_release");
        CO_RETURN last_error;
    }

    CORO_TASK(void)
    service_proxy::cleanup_after_object(std::shared_ptr<rpc::service> svc,
        std::shared_ptr<rpc::service_proxy> self,
        object object_id,
        int inherited_shared_reference_count,
        int inherited_optimistic_reference_count)
    {
        // self is needed to keep the service proxy alive in the async destruction of the service_proxy
        RPC_DEBUG("cleanup_after_object service zone: {} destination_zone={}, caller_zone={}, object_id = {} "
                  "(inherited: shared={}, optimistic={})",
            get_zone_id().get_val(),
            destination_zone_id_.get_val(),
            caller_zone_id_.get_val(),
            object_id.get_val(),
            inherited_shared_reference_count,
            inherited_optimistic_reference_count);

        auto caller_zone_id = get_zone_id().as_caller();

        // Handle normal reference release
        uint64_t ref_count = 0;
        auto ret = CO_AWAIT release(version_.load(), destination_zone_id_, object_id, caller_zone_id, release_options::normal, ref_count);
        if (ret != rpc::error::OK())
        {
            RPC_ERROR("cleanup_after_object release failed");
            RPC_ASSERT(false);
            CO_RETURN;
        }
        inner_release_external_ref();

        // Handle inherited optimistic references first (as recommended)
        for (int i = 0; i < inherited_optimistic_reference_count; i++)
        {
            RPC_DEBUG("Releasing inherited optimistic reference {}/{} for object {}",
                (i + 1),
                inherited_optimistic_reference_count,
                object_id.get_val());

            auto ret = CO_AWAIT release(version_.load(), destination_zone_id_, object_id, caller_zone_id, release_options::optimistic, ref_count);
            if (ret != rpc::error::OK())
            {
                RPC_ERROR("cleanup_after_object optimistic release failed");
                RPC_ASSERT(false);
                CO_RETURN;
            }
            inner_release_external_ref();
        }

        // Handle inherited shared references from race conditions
        for (int i = 0; i < inherited_shared_reference_count; i++)
        {
            RPC_DEBUG("Releasing inherited shared reference {}/{} for object {}",
                (i + 1),
                inherited_shared_reference_count,
                object_id.get_val());

            auto ret = CO_AWAIT release(version_.load(), destination_zone_id_, object_id, caller_zone_id, release_options::normal, ref_count);
            if (ret != rpc::error::OK())
            {
                RPC_ERROR("cleanup_after_object shared release failed");
                RPC_ASSERT(false);
                CO_RETURN;
            }
            inner_release_external_ref();
        }
        self = nullptr; // just so it does not get optimised out
        svc = nullptr;  // this needs to hang around a bit longer than the service proxy
        CO_RETURN;
    }

    void service_proxy::on_object_proxy_released(object object_id, int inherited_shared_reference_count, int inherited_optimistic_reference_count)
    {
        RPC_DEBUG("on_object_proxy_released service zone: {} destination_zone={}, caller_zone={}, object_id = {} "
                  "(inherited: shared={}, optimistic={})",
            get_zone_id().get_val(),
            destination_zone_id_.get_val(),
            caller_zone_id_.get_val(),
            object_id.get_val(),
            inherited_shared_reference_count,
            inherited_optimistic_reference_count);

        // this keeps the underlying service alive while the service proxy is released
        auto current_service = get_operating_zone_service();

        auto caller_zone_id = get_zone_id().as_caller();
        RPC_ASSERT(caller_zone_id == get_caller_zone_id());

#ifdef USE_RPC_TELEMETRY
        if (auto telemetry_service = rpc::get_telemetry_service(); telemetry_service)
        {
            telemetry_service->on_service_proxy_release(
                get_zone_id(), destination_zone_id_, destination_channel_zone_, caller_zone_id, object_id);
        }
#endif

        // Handle proxy map cleanup - use only insert_control_ to avoid deadlocks
        {
            std::lock_guard proxy_lock(insert_control_);
            auto item = proxies_.find(object_id);
            if (item != proxies_.end())
            {
                auto existing_proxy = item->second.lock();
                if (existing_proxy != nullptr)
                {
                    // Check if there are other proxies that need to handle the cleanup
                    int total_inherited = inherited_shared_reference_count + inherited_optimistic_reference_count;
                    if (total_inherited > 0)
                    {
                        // There are other proxies - we need to transfer references to the existing proxy
                        RPC_DEBUG("Race condition avoided - transferring {} inherited references (shared={}, optimistic={}) "
                                  "for object {}, skipping remote release calls",
                            total_inherited,
                            inherited_shared_reference_count,
                            inherited_optimistic_reference_count,
                            object_id.get_val());

                        // Transfer all inherited references to the existing proxy
                        for (int i = 0; i < inherited_shared_reference_count; i++)
                        {
                            existing_proxy->inherit_shared_reference();
                        }
                        for (int i = 0; i < inherited_optimistic_reference_count; i++)
                        {
                            existing_proxy->inherit_optimistic_reference();
                        }
                        return;
                    }
                }
                // Always remove this entry from the map since this object proxy is being released
                proxies_.erase(item);
            }
        }

        // Schedule the coroutine work asynchronously
#ifdef BUILD_COROUTINE
        current_service->schedule(
            cleanup_after_object(current_service, shared_from_this(), object_id, inherited_shared_reference_count, inherited_optimistic_reference_count));
#else
        cleanup_after_object(current_service, shared_from_this(), object_id, inherited_shared_reference_count, inherited_optimistic_reference_count);
#endif
    }

    std::shared_ptr<rpc::service_proxy> service_proxy::clone_for_zone(
        destination_zone destination_zone_id, caller_zone caller_zone_id)
    {
        RPC_ASSERT(!(caller_zone_id_ == caller_zone_id && destination_zone_id_ == destination_zone_id));
        auto ret = clone();
        ret->is_parent_channel_ = false;
        ret->caller_zone_id_ = caller_zone_id;
        if (destination_zone_id_ != destination_zone_id)
        {
            ret->destination_zone_id_ = destination_zone_id;
            if (!ret->destination_channel_zone_.is_set())
                ret->destination_channel_zone_ = destination_zone_id_.as_destination_channel();
        }

#ifdef USE_RPC_TELEMETRY
        if (auto telemetry_service = rpc::get_telemetry_service(); telemetry_service)
        {
            telemetry_service->on_cloned_service_proxy_creation(ret->service_.lock()->get_name().c_str(),
                ret->name_.c_str(),
                ret->get_zone_id(),
                ret->get_destination_zone_id(),
                ret->get_caller_zone_id());
        }
#endif
        return ret;
    }

    CORO_TASK(std::shared_ptr<rpc::object_proxy>)
    service_proxy::get_or_create_object_proxy(
        object object_id, object_proxy_creation_rule rule, bool new_proxy_added, known_direction_zone known_direction_zone_id)
    {
        RPC_DEBUG("get_or_create_object_proxy service zone: {} destination_zone={}, caller_zone={}, object_id = {}",
            zone_id_.get_val(),
            destination_zone_id_.get_val(),
            caller_zone_id_.get_val(),
            object_id.get_val());

        std::shared_ptr<rpc::object_proxy> op;
        bool is_new = false;
        std::shared_ptr<rpc::service_proxy> self_ref; // Hold reference to prevent destruction

        {
            RPC_ASSERT(get_caller_zone_id() == get_zone_id().as_caller());
            std::lock_guard l(insert_control_);

            // Capture strong reference to self while in critical section

            auto item = proxies_.find(object_id);
            if (item != proxies_.end())
            {
                op = item->second.lock();
            }

            if (!op)
            {
                // Either no entry exists, or the weak_ptr is expired - create new object_proxy
                op = std::shared_ptr<rpc::object_proxy>(new object_proxy(object_id, shared_from_this()));
#ifdef USE_RPC_TELEMETRY
                if (auto telemetry_service = rpc::get_telemetry_service(); telemetry_service)
                {
                    telemetry_service->on_object_proxy_creation(get_zone_id(), get_destination_zone_id(), object_id, true);
                }
#endif
                proxies_[object_id] = op;
                is_new = true;
            }
        }

        // Perform remote operations OUTSIDE the mutex lock
        // self_ref keeps this service_proxy alive during these operations
        if (is_new && rule == object_proxy_creation_rule::ADD_REF_IF_NEW)
        {
#ifdef USE_RPC_TELEMETRY
            if (auto telemetry_service = rpc::get_telemetry_service(); telemetry_service)
            {
                telemetry_service->message(rpc::i_telemetry_service::level_enum::info,
                    "get_or_create_object_proxy calling sp_add_ref with normal options for new object_proxy");
            }
#endif
            uint64_t ref_count = 0;
            auto ret
                = CO_AWAIT sp_add_ref(object_id, {0}, rpc::add_ref_options::normal, known_direction_zone_id, ref_count);
            if (ret != error::OK())
            {
                RPC_ERROR("sp_add_ref failed");
                RPC_ASSERT(false);
                CO_RETURN nullptr;
            }
            if (!new_proxy_added)
            {
                // This is now safe because self_ref keeps us alive
                add_external_ref();
            }
        }
        if (!is_new && rule == object_proxy_creation_rule::RELEASE_IF_NOT_NEW)
        {
            RPC_DEBUG(
                "get_or_create_object_proxy calling sp_release due to object_proxy_creation_rule::RELEASE_IF_NOT_NEW");

            // as this is an out parameter the callee will be doing an add ref if the object proxy is already
            // found we can do a release
            RPC_ASSERT(!new_proxy_added);
            uint64_t ref_count = 0;
            auto ret = CO_AWAIT sp_release(object_id, ref_count);
            if (ret == error::OK())
            {
                // This is now safe because self_ref keeps us alive
                release_external_ref();
            }
            else
            {
                RPC_ERROR("sp_release failed");
            }
        }

        // self_ref goes out of scope here, allowing normal destruction if needed
        CO_RETURN op;
    }
}
