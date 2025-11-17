/*
 *   Copyright (c) 2025 Edward Boggis-Rolfe
 *   All rights reserved.
 */
#include <rpc/rpc.h>
#include <cstdio>
#include <limits>

namespace rpc
{
    
    service_proxy::service_proxy(const std::string& name, const std::shared_ptr<transport>& transport, const std::shared_ptr<rpc::service>& svc)
    : zone_id_(svc->get_zone_id())
        , destination_zone_id_(transport->get_adjacent_zone_id().as_destination())
        , service_(svc)
        , transport_(transport)
        , name_(name)        
    {
#ifdef USE_RPC_TELEMETRY
        if (auto telemetry_service = rpc::get_telemetry_service(); telemetry_service)
        {
            telemetry_service->on_service_proxy_creation(
                name, name_, get_zone_id(), get_destination_zone_id(), svc->get_zone_id().as_caller());
        }
#endif
        RPC_ASSERT(svc != nullptr);
    }

/*    service_proxy::service_proxy(
        const std::string& name, destination_zone destination_zone_id, const std::shared_ptr<rpc::service>& svc)
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
                svc->get_name(), name, get_zone_id(), get_destination_zone_id(), svc->get_zone_id().as_caller());
        }
#endif
        RPC_ASSERT(svc != nullptr);
    }
  */  
    service_proxy::service_proxy(const std::string& name, destination_zone destination_zone_id, const std::shared_ptr<rpc::service_proxy>& other)
        : zone_id_(other->zone_id_)
        , destination_zone_id_(destination_zone_id)
        , service_(other->service_)
        , transport_(other->transport_)
        , version_(rpc::get_version())
        , enc_(encoding::enc_default)
        , name_(name)
    {
        
    }

    // service_proxy::service_proxy(const service_proxy& other)
    //     : std::enable_shared_from_this<rpc::service_proxy>(other)
    //     , zone_id_(other.zone_id_)
    //     , destination_zone_id_(other.destination_zone_id_)
    //     , destination_channel_zone_(other.destination_channel_zone_)
    //     , caller_zone_id_(other.caller_zone_id_)
    //     , service_(other.service_)
    //     , lifetime_lock_count_(0)
    //     , enc_(other.enc_)
    //     , name_(other.name_)
    // {
    //     RPC_ASSERT(service_.lock() != nullptr);
    // }

    service_proxy::~service_proxy()
    {
        if (!proxies_.empty())
        {
#ifdef USE_RPC_LOGGING
            RPC_WARNING("service_proxy destructor: {} proxies still in map for destination_zone={}",
                proxies_.size(),
                destination_zone_id_.get_val()
                // , caller_zone_id_.get_val()
                );

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
        // if (is_responsible_for_cleaning_up_service_)
        // {
            auto svc = service_.lock();
        //     if (get_zone_id().as_caller() != caller_zone_id_)
        //     {
                svc->remove_zone_proxy(destination_zone_id_, zone_id_.as_caller());
        //     }
        // }
        
#ifdef USE_RPC_TELEMETRY
        if (auto telemetry_service = rpc::get_telemetry_service(); telemetry_service)
        {
            telemetry_service->on_service_proxy_deletion(get_zone_id(), destination_zone_id_, zone_id_.as_caller());
        }
#endif
        RPC_ASSERT(proxies_.empty());
    }

    // void service_proxy::set_parent_channel(bool val)
    // {
    //     is_parent_channel_ = val;

    //     if (lifetime_lock_count_ == 0 && is_parent_channel_ == false)
    //     {
    //         RPC_ASSERT(lifetime_lock_.get_nullable());
    //         lifetime_lock_.reset();
    //     }
    // }

    void service_proxy::update_remote_rpc_version(uint64_t version)
    {
        const auto min_version = std::max<std::uint64_t>(rpc::LOWEST_SUPPORTED_VERSION, 1);
        const auto max_version = rpc::HIGHEST_SUPPORTED_VERSION;
        version_.store(std::clamp(version, min_version, max_version));
    }
    
//     void service_proxy::add_external_ref()
//     {
//         std::lock_guard g(insert_control_);
//         auto count = ++lifetime_lock_count_;
// #ifdef USE_RPC_TELEMETRY
//         if (auto telemetry_service = rpc::get_telemetry_service(); telemetry_service)
//         {
//             auto transport = transport_.get_nullable();
//             RPC_ASSERT(transport);
//             telemetry_service->on_service_proxy_add_external_ref(
//                 zone_id_, transport ? transport->get_adjacent_zone_id().as_destination_channel() : {0}, destination_zone_id_, zone_id_.as_caller(), count);
//         }
// #endif
//         // RPC_ASSERT(count >= 1);
//         // if (count == 1)
//         // {
//         //     // Cache lifetime_lock value for consistency throughout function
//         //     RPC_ASSERT(!lifetime_lock_.get_nullable());
//         //     lifetime_lock_ = stdex::member_ptr<service_proxy>(shared_from_this());
//         //     // Use cached value to ensure consistency
//         //     RPC_ASSERT(lifetime_lock_.get_nullable());
//         // }
//     }

//     int service_proxy::release_external_ref()
//     {
//         std::lock_guard g(insert_control_);
//         return inner_release_external_ref();
//     }

//     int service_proxy::inner_release_external_ref()
//     {
//         auto count = --lifetime_lock_count_;
// #ifdef USE_RPC_TELEMETRY
//         if (auto telemetry_service = rpc::get_telemetry_service(); telemetry_service)
//         {
//             auto transport = transport_.get_nullable();
//             RPC_ASSERT(transport);
//             telemetry_service->on_service_proxy_release_external_ref(
//                 zone_id_, transport ? transport->get_adjacent_zone_id().as_destination_channel() : {0}, destination_zone_id_, zone_id_.as_caller(), count);
//         }
// #endif
//         RPC_ASSERT(count >= 0);
//         // if (count == 0 && is_parent_channel_ == false)
//         // {
//         //     RPC_ASSERT(lifetime_lock_.get_nullable());
//         //     lifetime_lock_.reset();
//         // }
//         return count;
//     }

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
        
        auto transport = transport_.get_nullable();
        if(!transport)
        {
            CO_RETURN rpc::error::TRANSPORT_ERROR();
        }

        std::vector<rpc::back_channel_entry> empty_in;
        std::vector<rpc::back_channel_entry> empty_out;
        CO_RETURN CO_AWAIT transport->send(protocol_version,
            encoding,
            tag,
            get_zone_id().as_caller_channel(),
            get_zone_id().as_caller(),
            destination_zone_id_,
            object_id,
            interface_id,
            method_id,
            in_size_,
            in_buf_,
            out_buf_,
            empty_in,
            empty_out);
    }

    [[nodiscard]] CORO_TASK(int) service_proxy::sp_try_cast(
        destination_zone destination_zone_id, object object_id, std::function<interface_ordinal(uint64_t)> id_getter)
    {
        auto original_version = version_.load();
        auto version = original_version;
        const auto min_version = rpc::LOWEST_SUPPORTED_VERSION ? rpc::LOWEST_SUPPORTED_VERSION : 1;
        int last_error = rpc::error::INVALID_VERSION();
        
        auto transport = transport_.get_nullable();
        if(!transport)
        {
            CO_RETURN rpc::error::TRANSPORT_ERROR();
        }
        
        while (version >= min_version)
        {
            auto if_id = id_getter(version);
#ifdef USE_RPC_TELEMETRY
            if (auto telemetry_service = rpc::get_telemetry_service(); telemetry_service)
            {
                telemetry_service->on_service_proxy_try_cast(
                    get_zone_id(), destination_zone_id, zone_id_.as_caller(), object_id, if_id);
            }
#endif
            std::vector<rpc::back_channel_entry> empty_in;
            std::vector<rpc::back_channel_entry> empty_out;
            auto ret = CO_AWAIT transport->try_cast(version, destination_zone_id, object_id, if_id, empty_in, empty_out);
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
        RPC_ERROR("Incompatible service version in try_cast");
        CO_RETURN last_error;
    }

    [[nodiscard]] CORO_TASK(int) service_proxy::sp_add_ref(object object_id,
        caller_channel_zone caller_channel_zone_id,
        add_ref_options build_out_param_channel,
        known_direction_zone known_direction_zone_id,
        uint64_t& ref_count)
    {
        auto transport = transport_.get_nullable();
        if(!transport)
        {
            CO_RETURN rpc::error::TRANSPORT_ERROR();
        }
        
#ifdef USE_RPC_TELEMETRY
        if (auto telemetry_service = rpc::get_telemetry_service(); telemetry_service)
        {
            telemetry_service->on_service_proxy_add_ref(get_zone_id(),
                destination_zone_id_,
                transport->get_adjacent_zone_id().as_destination_channel(),
                zone_id_.as_caller(),
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
            std::vector<rpc::back_channel_entry> empty_in;
            std::vector<rpc::back_channel_entry> empty_out;
            auto attempt = CO_AWAIT transport->add_ref(version,
                transport->get_adjacent_zone_id().as_destination_channel(),
                destination_zone_id_,
                object_id,
                caller_channel_zone_id,
                zone_id_.as_caller(),
                known_direction_zone_id,
                build_out_param_channel,
                ref_count,
                empty_in,
                empty_out);
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

    CORO_TASK(int) service_proxy::sp_release(object object_id, release_options options, uint64_t& ref_count)
    {
        auto transport = transport_.get_nullable();
        if(!transport)
        {
            CO_RETURN rpc::error::TRANSPORT_ERROR();
        }

#ifdef USE_RPC_TELEMETRY
        if (auto telemetry_service = rpc::get_telemetry_service(); telemetry_service)
        {
            telemetry_service->on_service_proxy_release(
                get_zone_id(), destination_zone_id_, transport->get_adjacent_zone_id().as_destination_channel(), zone_id_.as_caller(), object_id);
        }
#endif

        auto original_version = version_.load();
        auto version = original_version;
        const auto min_version = rpc::LOWEST_SUPPORTED_VERSION ? rpc::LOWEST_SUPPORTED_VERSION : 1;
        int last_error = rpc::error::INVALID_VERSION();
        
        while (version >= min_version)
        {
            std::vector<rpc::back_channel_entry> empty_in;
            std::vector<rpc::back_channel_entry> empty_out;
            auto ret = CO_AWAIT transport->release(version, destination_zone_id_, object_id, zone_id_.as_caller(), options, ref_count, empty_in, empty_out);
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

    void service_proxy::cleanup_after_object(std::shared_ptr<object_proxy> op,bool is_optimistic)
    {
        std::shared_ptr<rpc::service_proxy> self = op->get_service_proxy();
        std::shared_ptr<rpc::service> svc = self->get_operating_zone_service();
        
        auto object_id = op->get_object_id();
        // self is needed to keep the service proxy alive in the async destruction of the service_proxy
        RPC_DEBUG("cleanup_after_object service zone: {} destination_zone={}, object_id = {} "
                  "decrement={}",
            get_zone_id().get_val(),
            destination_zone_id_.get_val(),
            object_id.get_val(),
            is_optimistic ? "optimistic" : "shared");

        auto transport = transport_.get_nullable();
        if(!transport)
        {
            RPC_ERROR("transport_ is null unable to release");
            RPC_ASSERT(false);
            CO_RETURN;
        }

        op.reset();
        self.reset(); // just so it does not get optimised out
        
        auto version = version_.load();
        auto destination_zone_id = destination_zone_id_;

#ifdef BUILD_COROUTINE
        svc->schedule([svc, object_id, transport, version, destination_zone_id, is_optimistic]()->CORO_TASK(void){
#endif            
            uint64_t ref_count = 0;
            std::vector<rpc::back_channel_entry> empty_in;
            std::vector<rpc::back_channel_entry> empty_out;

            auto ret = CO_AWAIT transport->release(version, destination_zone_id, object_id, svc->get_zone_id().as_caller(), is_optimistic ? release_options::optimistic : release_options::normal, ref_count, empty_in, empty_out);
            // inner_count = inner_release_external_ref();
            // RPC_DEBUG("inner count = {} for object {}", inner_count, object_id.get_val());
            // if(inner_count == 0 && is_responsible_for_cleaning_up_service_)
            // {
            //     svc->remove_zone_proxy(destination_zone_id_, zone_id_.as_caller());
            // }

            // Notify that object is gone after all cleanup is complete
            CO_AWAIT svc->notify_object_gone_event(object_id, destination_zone_id);
            
            //error handling here as the cleanup needs to happen anyway
            if (ret == rpc::error::OK())
            {
                RPC_DEBUG("Remote {} count = {} for object {}", is_optimistic ? "optimistic" : "shared", ref_count, object_id.get_val());
            }
            else if (is_optimistic && ret == rpc::error::OBJECT_NOT_FOUND())
            {
                RPC_DEBUG("Object {} not found - stub already deleted (normal for optimistic_ptr)", object_id.get_val());
            }
            else 
            {
                RPC_ERROR("cleanup_after_object optimistic release failed: {}", rpc::error::to_string(ret));
                RPC_ASSERT(false);
            }
#ifdef BUILD_COROUTINE
        }());
#endif
    }

    void service_proxy::on_object_proxy_released(const std::shared_ptr<object_proxy>& op, bool is_optimistic)
    {
        auto object_id = op->get_object_id();
        
        RPC_DEBUG("on_object_proxy_released service zone: {} destination_zone={}, object_id = {} "
                  "decrement={})",
            get_zone_id().get_val(),
            destination_zone_id_.get_val(),
            object_id.get_val(),
            is_optimistic ? "optimistic" : "shared");


#ifdef USE_RPC_TELEMETRY
        if (auto telemetry_service = rpc::get_telemetry_service(); telemetry_service)
        {
            auto transport = transport_.get_nullable();
            RPC_ASSERT(transport);
            telemetry_service->on_service_proxy_release(
                get_zone_id(), destination_zone_id_, transport->get_adjacent_zone_id().as_destination_channel(), zone_id_.as_caller(), object_id.get_val());
        }
#endif

        {
            // as there are no more refcounts on the object proxy remove it from the proxies_ map now
            // it is not possible to know if it is time to remove the proxies_ map deterministically once we 
            // schedule the call to cleanup_after_object
            std::lock_guard proxy_lock(insert_control_);
            if(op->get_shared_count() == 0 && op->get_optimistic_count() == 0)
            {
                auto item = proxies_.find(object_id);
                RPC_ASSERT(item != proxies_.end());
                if (item != proxies_.end())
                {
                    // Remove from map since object_proxy is being destroyed
                    RPC_DEBUG("Removing object_id={} from proxy map (object_proxy being destroyed)", object_id.get_val());
                    proxies_.erase(item);
                }
            }
        }

        // Schedule the coroutine work asynchronously
        cleanup_after_object(op, is_optimistic);
    }

    std::shared_ptr<rpc::service_proxy> service_proxy::clone_for_zone(
        destination_zone destination_zone_id)
    {
        RPC_ASSERT(destination_zone_id_ != destination_zone_id);
        auto ret = std::make_shared<service_proxy>(name_, destination_zone_id, shared_from_this());

#ifdef USE_RPC_TELEMETRY
        if (auto telemetry_service = rpc::get_telemetry_service(); telemetry_service)
        {
            telemetry_service->on_cloned_service_proxy_creation(ret->service_.lock()->get_name(),
                ret->name_,
                ret->get_zone_id(),
                ret->get_destination_zone_id(),
                ret->get_zone_id().as_caller());
        }
#endif
        return ret;
    }

    CORO_TASK(int)
    service_proxy::get_or_create_object_proxy(
        object object_id, object_proxy_creation_rule rule, bool new_proxy_added, known_direction_zone known_direction_zone_id,
            bool is_optimistic, std::shared_ptr<rpc::object_proxy>& op)
    {
        RPC_DEBUG("get_or_create_object_proxy service zone: {} destination_zone={}, caller_zone={}, object_id = {}",
            zone_id_.get_val(),
            destination_zone_id_.get_val(),
            zone_id_.as_caller().get_val(),
            object_id.get_val());

        std::shared_ptr<rpc::object_proxy> tmp;
        bool is_new = false;
        std::shared_ptr<rpc::service_proxy> self_ref; // Hold reference to prevent destruction

        {
            std::lock_guard l(insert_control_);

            // Capture strong reference to self while in critical section

            auto item = proxies_.find(object_id);
            if (item != proxies_.end())
            {
                tmp = item->second.lock();
            }

            if (!tmp)
            {
                // Either no entry exists, or the weak_ptr is expired - create new object_proxy
                tmp = std::shared_ptr<rpc::object_proxy>(new object_proxy(object_id, shared_from_this()));
#ifdef USE_RPC_TELEMETRY
                if (auto telemetry_service = rpc::get_telemetry_service(); telemetry_service)
                {
                    telemetry_service->on_object_proxy_creation(get_zone_id(), get_destination_zone_id(), object_id, true);
                }
#endif
                proxies_[object_id] = tmp;
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
                = CO_AWAIT sp_add_ref(object_id, {0}, is_optimistic ? rpc::add_ref_options::optimistic : rpc::add_ref_options::normal, known_direction_zone_id, ref_count);
            if (ret != error::OK())
            {
                RPC_ERROR("sp_add_ref failed");
                std::lock_guard l(insert_control_);
                proxies_.erase(object_id);
                RPC_ASSERT(false);
                CO_RETURN ret;
            }
            // if (!new_proxy_added)
            // {
            //     // This is now safe because self_ref keeps us alive
            //     add_external_ref();
            // }
        }
        if (!is_new && rule == object_proxy_creation_rule::RELEASE_IF_NOT_NEW)
        {
            RPC_DEBUG(
                "get_or_create_object_proxy calling sp_release due to object_proxy_creation_rule::RELEASE_IF_NOT_NEW");

            // as this is an out parameter the callee will be doing an add ref if the object proxy is already
            // found we can do a release
            RPC_ASSERT(!new_proxy_added);
            uint64_t ref_count = 0;
            auto ret = CO_AWAIT sp_release(object_id, is_optimistic ? rpc::release_options::optimistic : rpc::release_options::normal, ref_count);
            if (ret == error::OK())
            {
                // // This is now safe because self_ref keeps us alive
                // release_external_ref();
            }
            else
            {
                RPC_ERROR("sp_release failed");
                return ret;
            }
        }
        
        op = tmp;

        // self_ref goes out of scope here, allowing normal destruction if needed
        CO_RETURN error::OK();
    }

/*    // i_marshaller interface implementation
    // Routes to transport_ for remote zones or service_ for local zone

    CORO_TASK(int) service_proxy::send(uint64_t protocol_version,
        encoding encoding,
        uint64_t tag,
        caller_channel_zone caller_channel_zone_id,
        caller_zone caller_zone_id,
        destination_zone destination_zone_id,
        object object_id,
        interface_ordinal interface_id,
        method method_id,
        size_t in_size_,
        const char* in_buf_,
        std::vector<char>& out_buf_,
        const std::vector<rpc::back_channel_entry>& in_back_channel,
        std::vector<rpc::back_channel_entry>& out_back_channel)
    {
        // Route based on destination: local service or remote transport
        if (destination_zone_id.get_val() == zone_id_.get_val())
        {
            // Local zone - route to service
            auto svc = service_.lock();
            if (!svc)
            {
                CO_RETURN error::ZONE_NOT_FOUND();
            }

            CO_RETURN CO_AWAIT svc->send(
                protocol_version, encoding, tag,
                caller_channel_zone_id, caller_zone_id, destination_zone_id,
                object_id, interface_id, method_id,
                in_size_, in_buf_, out_buf_,
                in_back_channel, out_back_channel);
        }
        else
        {
            // Remote zone - route to transport
            auto transport = transport_.get_nullable();
            if (!transport)
            {
                CO_RETURN error::TRANSPORT_ERROR();
            }

            auto result = CO_AWAIT transport->send(
                protocol_version, encoding, tag,
                caller_channel_zone_id, caller_zone_id, destination_zone_id,
                object_id, interface_id, method_id,
                in_size_, in_buf_, out_buf_,
                in_back_channel, out_back_channel);

            // If zone not found or transport error, cleanup transport and parent service ref
            if (result == error::ZONE_NOT_FOUND() || result == error::TRANSPORT_ERROR())
            {
                transport_.reset();
                parent_service_ref_.reset();
            }

            CO_RETURN result;
        }
    }

    CORO_TASK(void) service_proxy::post(uint64_t protocol_version,
        encoding encoding,
        uint64_t tag,
        caller_channel_zone caller_channel_zone_id,
        caller_zone caller_zone_id,
        destination_zone destination_zone_id,
        object object_id,
        interface_ordinal interface_id,
        method method_id,
        post_options options,
        size_t in_size_,
        const char* in_buf_,
        const std::vector<rpc::back_channel_entry>& in_back_channel)
    {
        // Route based on destination: local service or remote transport
        if (destination_zone_id.get_val() == zone_id_.get_val())
        {
            // Local zone - route to service
            auto svc = service_.lock();
            if (!svc)
            {
                CO_RETURN;
            }

            CO_AWAIT svc->post(
                protocol_version, encoding, tag,
                caller_channel_zone_id, caller_zone_id, destination_zone_id,
                object_id, interface_id, method_id, options,
                in_size_, in_buf_, in_back_channel);
        }
        else
        {
            // Remote zone - route to transport
            auto transport = transport_.get_nullable();
            if (!transport)
            {
                CO_RETURN;
            }

            CO_AWAIT transport->post(
                protocol_version, encoding, tag,
                caller_channel_zone_id, caller_zone_id, destination_zone_id,
                object_id, interface_id, method_id, options,
                in_size_, in_buf_, in_back_channel);
        }
    }

    CORO_TASK(int) service_proxy::try_cast(
        uint64_t protocol_version,
        destination_zone destination_zone_id,
        object object_id,
        interface_ordinal interface_id,
        const std::vector<rpc::back_channel_entry>& in_back_channel,
        std::vector<rpc::back_channel_entry>& out_back_channel)
    {
        // Route based on destination: local service or remote transport
        if (destination_zone_id.get_val() == zone_id_.get_val())
        {
            // Local zone - route to service
            auto svc = service_.lock();
            if (!svc)
            {
                CO_RETURN error::ZONE_NOT_FOUND();
            }

            CO_RETURN CO_AWAIT svc->try_cast(
                protocol_version, destination_zone_id,
                object_id, interface_id,
                in_back_channel, out_back_channel);
        }
        else
        {
            // Remote zone - route to transport
            auto transport = transport_.get_nullable();
            if (!transport)
            {
                CO_RETURN error::TRANSPORT_ERROR();
            }

            auto result = CO_AWAIT transport->try_cast(
                protocol_version, destination_zone_id,
                object_id, interface_id,
                in_back_channel, out_back_channel);

            // If zone not found or transport error, cleanup transport and parent service ref
            if (result == error::ZONE_NOT_FOUND() || result == error::TRANSPORT_ERROR())
            {
                transport_.reset();
                parent_service_ref_.reset();
            }

            CO_RETURN result;
        }
    }

    CORO_TASK(int) service_proxy::add_ref(uint64_t protocol_version,
        destination_channel_zone destination_channel_zone_id,
        destination_zone destination_zone_id,
        object object_id,
        caller_channel_zone caller_channel_zone_id,
        caller_zone caller_zone_id,
        known_direction_zone known_direction_zone_id,
        add_ref_options build_out_param_channel,
        uint64_t& reference_count,
        const std::vector<rpc::back_channel_entry>& in_back_channel,
        std::vector<rpc::back_channel_entry>& out_back_channel)
    {
        // Route based on destination: local service or remote transport
        if (destination_zone_id.get_val() == zone_id_.get_val())
        {
            // Local zone - route to service
            auto svc = service_.lock();
            if (!svc)
            {
                CO_RETURN error::ZONE_NOT_FOUND();
            }

            CO_RETURN CO_AWAIT svc->add_ref(
                protocol_version,
                destination_channel_zone_id, destination_zone_id, object_id,
                caller_channel_zone_id, caller_zone_id, known_direction_zone_id,
                build_out_param_channel, reference_count,
                in_back_channel, out_back_channel);
        }
        else
        {
            // Remote zone - route to transport
            auto transport = transport_.get_nullable();
            if (!transport)
            {
                CO_RETURN error::TRANSPORT_ERROR();
            }

            auto result = CO_AWAIT transport->add_ref(
                protocol_version,
                destination_channel_zone_id, destination_zone_id, object_id,
                caller_channel_zone_id, caller_zone_id, known_direction_zone_id,
                build_out_param_channel, reference_count,
                in_back_channel, out_back_channel);

            // If zone not found or transport error, cleanup transport and parent service ref
            if (result == error::ZONE_NOT_FOUND() || result == error::TRANSPORT_ERROR())
            {
                transport_.reset();
                parent_service_ref_.reset();
            }

            CO_RETURN result;
        }
    }

    CORO_TASK(int) service_proxy::release(uint64_t protocol_version,
        destination_zone destination_zone_id,
        object object_id,
        caller_zone caller_zone_id,
        release_options options,
        uint64_t& reference_count,
        const std::vector<rpc::back_channel_entry>& in_back_channel,
        std::vector<rpc::back_channel_entry>& out_back_channel)
    {
        // Route based on destination: local service or remote transport
        if (destination_zone_id.get_val() == zone_id_.get_val())
        {
            // Local zone - route to service
            auto svc = service_.lock();
            if (!svc)
            {
                CO_RETURN error::ZONE_NOT_FOUND();
            }

            CO_RETURN CO_AWAIT svc->release(
                protocol_version, destination_zone_id, object_id,
                caller_zone_id, options, reference_count,
                in_back_channel, out_back_channel);
        }
        else
        {
            // Remote zone - route to transport
            auto transport = transport_.get_nullable();
            if (!transport)
            {
                CO_RETURN error::TRANSPORT_ERROR();
            }

            auto result = CO_AWAIT transport->release(
                protocol_version, destination_zone_id, object_id,
                caller_zone_id, options, reference_count,
                in_back_channel, out_back_channel);

            // If zone not found or transport error, cleanup transport and parent service ref
            if (result == error::ZONE_NOT_FOUND() || result == error::TRANSPORT_ERROR())
            {
                transport_.reset();
                parent_service_ref_.reset();
            }

            CO_RETURN result;
        }
    }*/
}
