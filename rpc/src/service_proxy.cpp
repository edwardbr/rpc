/*
 *   Copyright (c) 2024 Edward Boggis-Rolfe
 *   All rights reserved.
 */
#include <rpc/rpc.h>

#ifdef USE_RPC_TELEMETRY
#include <rpc/telemetry/i_telemetry_service.h>
#endif

namespace rpc
{
    NAMESPACE_INLINE_BEGIN
    
    service_proxy::service_proxy(const char* name, destination_zone destination_zone_id, const std::shared_ptr<service>& svc)
        : zone_id_(svc->get_zone_id())
        , destination_zone_id_(destination_zone_id)
        , caller_zone_id_(svc->get_zone_id().as_caller())
        , service_(svc)
        , name_(name)
    {
#ifdef USE_RPC_TELEMETRY
        if (auto telemetry_service = rpc::telemetry_service_manager::get(); telemetry_service)
        {
            telemetry_service->on_service_proxy_creation(
                name, get_zone_id(), get_destination_zone_id(), svc->get_zone_id().as_caller());
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

    // not thread safe
    void service_proxy::set_parent_channel(bool val)
    {
        is_parent_channel_ = val;

        if (lifetime_lock_count_ == 0 && is_parent_channel_ == false)
        {
            RPC_ASSERT(lifetime_lock_);
            lifetime_lock_ = nullptr;
        }
    }

    service_proxy::~service_proxy()
    {
        RPC_ASSERT(proxies_.empty());
        auto svc = service_.lock();
        if (svc)
        {
            svc->remove_zone_proxy(destination_zone_id_, caller_zone_id_);
        }
#ifdef USE_RPC_TELEMETRY
        if (auto telemetry_service = rpc::telemetry_service_manager::get(); telemetry_service)
        {
            telemetry_service->on_service_proxy_deletion(get_zone_id(), get_destination_zone_id(), get_caller_zone_id());
        }
#endif
    }
    int service_proxy::connect(interface_descriptor input_descr, interface_descriptor& output_descr)
    {
        std::ignore = input_descr;
        std::ignore = output_descr;
        return rpc::error::ZONE_NOT_SUPPORTED();
    }

    void service_proxy::add_external_ref()
    {
        std::lock_guard g(insert_control_);
        auto count = ++lifetime_lock_count_;
#ifdef USE_RPC_TELEMETRY
        if (auto telemetry_service = rpc::telemetry_service_manager::get(); telemetry_service)
        {
            telemetry_service->on_service_proxy_add_external_ref(
                zone_id_, destination_channel_zone_, destination_zone_id_, caller_zone_id_, count);
        }
#endif
        RPC_ASSERT(count >= 1);
        if (count == 1)
        {
            RPC_ASSERT(!lifetime_lock_);
            lifetime_lock_ = shared_from_this();
            RPC_ASSERT(lifetime_lock_);
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
        if (auto telemetry_service = rpc::telemetry_service_manager::get(); telemetry_service)
        {
            telemetry_service->on_service_proxy_release_external_ref(
                zone_id_, destination_channel_zone_, destination_zone_id_, caller_zone_id_, count);
        }
#endif
        RPC_ASSERT(count >= 0);
        if (count == 0 && is_parent_channel_ == false)
        {
            RPC_ASSERT(lifetime_lock_);
            lifetime_lock_ = nullptr;
        }
        return count;
    }

    [[nodiscard]] int service_proxy::send_from_this_zone(uint64_t protocol_version,
        rpc::encoding encoding,
        uint64_t tag,
        rpc::object object_id,
        rpc::interface_ordinal interface_id,
        rpc::method method_id,
        size_t in_size_,
        const char* in_buf_,
        std::vector<char>& out_buf_)
    {
        return send(protocol_version,
            encoding,
            tag,
            caller_channel_zone{},
            caller_zone_id_,
            destination_zone_id_,
            object_id,
            interface_id,
            method_id,
            in_size_,
            in_buf_,
            out_buf_);
    }

    [[nodiscard]] int service_proxy::send_from_this_zone(encoding enc,
        uint64_t tag,
        object object_id,
        std::function<interface_ordinal(uint64_t)> id_getter,
        method method_id,
        size_t in_size_,
        const char* in_buf_,
        std::vector<char>& out_buf_)
    {
        // force a lowest common denominator
        if (enc != encoding::enc_default && enc != encoding::yas_binary && enc != encoding::yas_compressed_binary
            && enc != encoding::yas_json)
        {
            return error::INCOMPATIBLE_SERIALISATION();
        }

        auto version = version_.load();
        auto ret = send_from_this_zone(
            version, enc_, tag, object_id, id_getter(version), method_id, in_size_, in_buf_, out_buf_);
        if (ret == rpc::error::INVALID_VERSION())
        {
            version_.compare_exchange_strong(version, version - 1);
        }
        return ret;
    }

    [[nodiscard]] int service_proxy::sp_try_cast(
        destination_zone destination_zone_id, object object_id, std::function<interface_ordinal(uint64_t)> id_getter)
    {
        auto original_version = version_.load();
        auto version = original_version;
        while (version)
        {
            auto if_id = id_getter(version);
#ifdef USE_RPC_TELEMETRY
            if (auto telemetry_service = rpc::telemetry_service_manager::get(); telemetry_service)
            {
                telemetry_service->on_service_proxy_try_cast(
                    get_zone_id(), destination_zone_id, get_caller_zone_id(), object_id, if_id);
            }
#endif
            auto ret = try_cast(version, destination_zone_id, object_id, if_id);
            if (ret != rpc::error::INVALID_VERSION())
            {
                if (original_version != version)
                {
                    version_.compare_exchange_strong(original_version, version);
                }
                return ret;
            }
            version--;
        }
        return rpc::error::INCOMPATIBLE_SERVICE();
    }

    [[nodiscard]] uint64_t service_proxy::sp_add_ref(
        object object_id, caller_channel_zone caller_channel_zone_id, add_ref_options build_out_param_channel)
    {
#ifdef USE_RPC_TELEMETRY
        if (auto telemetry_service = rpc::telemetry_service_manager::get(); telemetry_service)
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
        while (version)
        {
            auto ret = add_ref(version,
                destination_channel_zone_,
                destination_zone_id_,
                object_id,
                caller_channel_zone_id,
                caller_zone_id_,
                build_out_param_channel);
            if (ret != std::numeric_limits<uint64_t>::max())
            {
                if (original_version != version)
                {
                    version_.compare_exchange_strong(original_version, version);
                }
                return ret;
            }
            version--;
        }
        return rpc::error::INCOMPATIBLE_SERVICE();
    }

    uint64_t service_proxy::sp_release(object object_id)
    {
#ifdef USE_RPC_TELEMETRY
        if (auto telemetry_service = rpc::telemetry_service_manager::get(); telemetry_service)
        {
            telemetry_service->on_service_proxy_release(
                get_zone_id(), destination_zone_id_, destination_channel_zone_, get_caller_zone_id(), object_id);
        }
#endif

        auto original_version = version_.load();
        auto version = original_version;
        while (version)
        {
            auto ret = release(version, destination_zone_id_, object_id, caller_zone_id_);
            if (ret != std::numeric_limits<uint64_t>::max())
            {
                if (original_version != version)
                {
                    version_.compare_exchange_strong(original_version, version);
                }
                return ret;
            }
            version--;
        }
        return rpc::error::INCOMPATIBLE_SERVICE();
    }

    void service_proxy::on_object_proxy_released(object object_id)
    {
        auto caller_zone_id = get_zone_id().as_caller();
        RPC_ASSERT(caller_zone_id == get_caller_zone_id());

        {
            std::lock_guard l(insert_control_);
            auto item = proxies_.find(object_id);
            RPC_ASSERT(item != proxies_.end());
            if (item->second.lock() == nullptr)
            {
                // the reason for this if statement is between an object weak pointer dying and this attempted entry
                // being removed another object pointer object may have sneaked in and set to a non null value
                proxies_.erase(item);
            }
        }

#ifdef USE_RPC_TELEMETRY
        if (auto telemetry_service = rpc::telemetry_service_manager::get(); telemetry_service)
        {
            telemetry_service->on_service_proxy_release(
                get_zone_id(), destination_zone_id_, destination_channel_zone_, caller_zone_id, object_id);
        }
#endif

        auto original_version = version_.load();
        auto version = original_version;
        while (version)
        {
            auto ret = release(version, destination_zone_id_, object_id, caller_zone_id);
            if (ret != std::numeric_limits<uint64_t>::max())
            {
                inner_release_external_ref();
                if (original_version != version)
                {
                    version_.compare_exchange_strong(original_version, version);
                }
                return;
            }
            version--;
        }
        {
            std::string message("unable to release on service");
            LOG_STR(message.c_str(), message.size());
            RPC_ASSERT(false);
        }
        return;
    }

    void service_proxy::clone_completed()
    {
#ifdef USE_RPC_TELEMETRY
        if (auto telemetry_service = rpc::telemetry_service_manager::get(); telemetry_service)
        {
            telemetry_service->on_service_proxy_creation(
                name_.c_str(), get_zone_id(), get_destination_zone_id(), get_caller_zone_id());
        }
#endif
    }
    std::shared_ptr<service_proxy> service_proxy::clone_for_zone(
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

        ret->clone_completed();
        return ret;
    }

    std::shared_ptr<object_proxy> service_proxy::get_object_proxy(object object_id, bool& is_new)
    {
        RPC_ASSERT(get_caller_zone_id() == get_zone_id().as_caller());
        std::lock_guard l(insert_control_);
        auto item = proxies_.find(object_id);
        std::shared_ptr<object_proxy> op;
        if (item != proxies_.end())
            op = item->second.lock();
        if (op == nullptr)
        {
            op = std::shared_ptr<object_proxy>(new object_proxy(object_id, shared_from_this()));
#ifdef USE_RPC_TELEMETRY
            if (auto telemetry_service = rpc::telemetry_service_manager::get(); telemetry_service)
            {
                telemetry_service->on_object_proxy_creation(get_zone_id(), get_destination_zone_id(), object_id, true);
            }
#endif
            proxies_[object_id] = op;
            is_new = true;
            return op;
        }
        is_new = false;
        return op;
    }

    NAMESPACE_INLINE_END
}
