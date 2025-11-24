/*
 *   Copyright (c) 2025 Edward Boggis-Rolfe
 *   All rights reserved.
 */

// Standard C++ headers
#include <algorithm>

// RPC headers
#include <rpc/rpc.h>

// Other headers
#include <yas/mem_streams.hpp>
#include <yas/std_types.hpp>

namespace rpc
{
    ////////////////////////////////////////////////////////////////////////////
    // service

    thread_local service* current_service_ = nullptr;
    service* service::get_current_service()
    {
        return current_service_;
    }
    void service::set_current_service(service* svc)
    {
        current_service_ = svc;
    }

    std::atomic<uint64_t> service::zone_id_generator = 0;
    zone service::generate_new_zone_id()
    {
        auto count = ++zone_id_generator;
        return {count};
    }

    object service::generate_new_object_id() const
    {
        auto count = ++object_id_generator;
        return {count};
    }

#ifdef BUILD_COROUTINE
    service::service(const char* name, zone zone_id, const std::shared_ptr<coro::io_scheduler>& scheduler)
        : zone_id_(zone_id)
        , name_(name)
        , io_scheduler_(scheduler)
    {
#ifdef USE_RPC_TELEMETRY
        if (auto telemetry_service = rpc::get_telemetry_service(); telemetry_service)
            telemetry_service->on_service_creation(name, zone_id, destination_zone{0});
#endif
    }
    service::service(const char* name, zone zone_id, const std::shared_ptr<coro::io_scheduler>& scheduler, child_service_tag)
        : zone_id_(zone_id)
        , name_(name)
        , io_scheduler_(scheduler)
    {
        // No telemetry call for child services
    }

#else
    service::service(const char* name, zone zone_id)
        : zone_id_(zone_id)
        , name_(name)
    {
#ifdef USE_RPC_TELEMETRY
        if (auto telemetry_service = rpc::get_telemetry_service(); telemetry_service)
            telemetry_service->on_service_creation(name, zone_id, destination_zone{0});
#endif
    }
    service::service(const char* name, zone zone_id, child_service_tag)
        : zone_id_(zone_id)
        , name_(name)
    {
        // No telemetry call for child services
    }

#endif

    service::~service()
    {
#ifdef USE_RPC_TELEMETRY
        if (auto telemetry_service = rpc::get_telemetry_service(); telemetry_service)
            telemetry_service->on_service_deletion(zone_id_);
#endif

        // Child services use reference counting through service proxies to manage proper cleanup ordering.
        // Parent services maintain references to child services to prevent premature destruction.
        // The cleanup mechanism in service_proxy handles the proper ordering.

        object_id_generator = 0;
        // to do: RPC_ASSERT that there are no more object_stubs in memory
        bool is_empty = check_is_empty();
        (void)is_empty;
        RPC_ASSERT(is_empty);

        {
            std::lock_guard l(stub_control);
            stubs.clear();
            wrapped_object_to_stub.clear();
        }
        other_zones.clear();
    }

    object service::get_object_id(const rpc::shared_ptr<casting_interface>& ptr) const
    {
        if (ptr == nullptr)
            return {};
        auto* addr = ptr->get_address();
        if (addr)
        {
            std::lock_guard g(stub_control);
            auto item = wrapped_object_to_stub.find(addr);
            if (item != wrapped_object_to_stub.end())
            {
                auto obj = item->second.lock();
                if (obj)
                    return obj->get_id();
            }
        }
        else
        {
            return casting_interface::get_object_id(*ptr);
        }
        return {};
    }

    bool service::check_is_empty() const
    {
        std::lock_guard l(stub_control);
        bool success = true;
        for (const auto& item : stubs)
        {
            auto stub = item.second.lock();
            if (!stub)
            {
                RPC_WARNING("stub zone_id {}, object stub {} has been released but not deregistered in the service "
                            "suspected unclean shutdown",
                    std::to_string(zone_id_),
                    std::to_string(item.first));
            }
            else
            {
                RPC_WARNING("stub zone_id {}, object stub {} has not been released, there is a strong pointer "
                            "maintaining a positive reference count suspected unclean shutdown",
                    std::to_string(zone_id_),
                    std::to_string(item.first));
            }
            success = false;
        }
        for (auto item : wrapped_object_to_stub)
        {
            auto stub = item.second.lock();
            if (!stub)
            {
                RPC_WARNING("wrapped stub zone_id {}, wrapped_object has been released but not deregistered in the "
                            "service suspected unclean shutdown",
                    std::to_string(zone_id_));
            }
            else
            {
                RPC_WARNING("wrapped stub zone_id {}, wrapped_object {} has not been deregistered in the service "
                            "suspected unclean shutdown",
                    std::to_string(zone_id_),
                    std::to_string(stub->get_id()));
            }
            success = false;
        }

        for (auto item : other_zones)
        {
            auto svcproxy = item.second.lock();
            if (!svcproxy)
            {
                RPC_WARNING("service proxy zone_id {}, destination_zone_id {}, has been released "
                            "but not deregistered in the service",
                    std::to_string(zone_id_),
                    std::to_string(item.first));
            }
            else
            {
                auto transport = svcproxy->get_transport();
                RPC_WARNING("service proxy zone_id {}, destination_zone_id {}, destination_channel_zone_id "
                            "{} has not been released in the service suspected unclean shutdown",
                    std::to_string(zone_id_),
                    std::to_string(item.first),
                    std::to_string(transport ? transport->get_adjacent_zone_id().as_destination_channel()
                                             : destination_channel_zone{0}));

                for (auto proxy : svcproxy->get_proxies())
                {
                    auto op = proxy.second.lock();
                    if (op)
                    {
                        RPC_WARNING(" has object_proxy {}", std::to_string(op->get_object_id()));
                    }
                    else
                    {
                        RPC_WARNING(" has null object_proxy");
                    }
                    success = false;
                }
            }
            success = false;
        }
        return success;
    }

    CORO_TASK(int)
    service::send(uint64_t protocol_version,
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
        if (destination_zone_id != zone_id_.as_destination())
        {
            RPC_ASSERT(false); // this should be going to the pass through
            CO_RETURN error::TRANSPORT_ERROR();
        }
        current_service_tracker tracker(this);
        if (protocol_version < rpc::LOWEST_SUPPORTED_VERSION || protocol_version > rpc::HIGHEST_SUPPORTED_VERSION)
        {
            RPC_ERROR("Unsupported service version {} in send", protocol_version);
            CO_RETURN rpc::error::INVALID_VERSION();
        }
        std::weak_ptr<object_stub> weak_stub = get_object(object_id);
        auto stub = weak_stub.lock();
        if (stub == nullptr)
        {
            RPC_INFO("Object gone - stub has already been released");
            CO_RETURN rpc::error::OBJECT_GONE();
        }

        auto ret = CO_AWAIT stub->call(
            protocol_version, encoding, caller_channel_zone_id, caller_zone_id, interface_id, method_id, in_size_, in_buf_, out_buf_);

        CO_RETURN ret;
    }

    CORO_TASK(void)
    service::post(uint64_t protocol_version,
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
        if (destination_zone_id != zone_id_.as_destination())
        {
            RPC_ASSERT(false); // this should be going to the pass through
            RPC_ERROR("Unsupported post destination");
            CO_RETURN;
        }

        current_service_tracker tracker(this);

        // Log that post was received
        RPC_INFO("service::post received for destination_zone={} object_id={}, options={}",
            destination_zone_id.get_val(),
            object_id.get_val(),
            static_cast<uint8_t>(options));

        if (protocol_version < rpc::LOWEST_SUPPORTED_VERSION || protocol_version > rpc::HIGHEST_SUPPORTED_VERSION)
        {
            RPC_ERROR("Unsupported service version {} in post", protocol_version);
            CO_RETURN;
        }
        std::weak_ptr<object_stub> weak_stub = get_object(object_id);
        auto stub = weak_stub.lock();
        if (stub == nullptr)
        {
            RPC_INFO("Object gone - stub has already been released");
            CO_RETURN;
        }

        // For local post, we just call the stub without waiting for a response
        // Note: back-channel is not applicable for post operations (fire-and-forget)
        // std::vector<char> out_buf_dummy;
        // CO_AWAIT stub->call(protocol_version,
        //     encoding,
        //     caller_channel_zone_id,
        //     caller_zone_id,
        //     interface_id,
        //     method_id,
        //     in_size_,
        //     in_buf_,
        //     out_buf_dummy);

        // Log that post was delivered to local stub
        RPC_INFO(
            "service::post delivered to local stub for object_id={} in zone={}", object_id.get_val(), zone_id_.get_val());

        CO_RETURN;
    }

    CORO_TASK(void)
    service::clean_up_on_failed_connection(const std::shared_ptr<rpc::service_proxy>& destination_zone,
        rpc::shared_ptr<rpc::casting_interface> input_interface)
    {
        if (destination_zone && input_interface)
        {
            auto object_id = casting_interface::get_object_id(*input_interface);
            uint64_t ref_count = 0;
            auto ret = CO_AWAIT destination_zone->sp_release(object_id, release_options::normal, ref_count);
            if (ret == error::OK())
            {
                // destination_zone->release_external_ref();
            }
            else
            {
                RPC_ERROR("destination_zone->sp_release failed with code {}", ret);
            }
        }
    }

    CORO_TASK(int)
    service::prepare_out_param(uint64_t protocol_version,
        caller_channel_zone caller_channel_zone_id,
        caller_zone caller_zone_id,
        rpc::casting_interface* base,
        interface_descriptor& descriptor)
    {
        auto object_proxy = base->get_object_proxy();
        auto object_service_proxy = object_proxy->get_service_proxy();
        RPC_ASSERT(object_service_proxy->zone_id_ == zone_id_);
        auto destination_zone_id = object_service_proxy->get_destination_zone_id();
        auto transport = object_service_proxy->get_transport();
        auto destination_channel = transport->get_adjacent_zone_id().as_destination_channel();
        auto object_id = object_proxy->get_object_id();

        RPC_ASSERT(caller_zone_id.is_set());
        RPC_ASSERT(destination_zone_id.is_set());

        uint64_t caller_channel
            = caller_channel_zone_id.is_set() ? caller_channel_zone_id.get_val() : caller_zone_id.get_val();

        RPC_ASSERT(caller_channel);
        RPC_ASSERT(destination_channel.is_set());

        if (caller_channel == destination_channel.get_val())
        {
            // caller and destination are in the same channel let them fork where necessary
            // note the caller_channel_zone_id is 0 as both the caller and the destination are in from the same direction so any other value is wrong
            // Dont external_add_ref the local service proxy as we are return to source no channel is required
#ifdef USE_RPC_TELEMETRY
            if (auto telemetry_service = rpc::get_telemetry_service(); telemetry_service)
            {
                telemetry_service->on_service_proxy_add_ref(zone_id_,
                    destination_zone_id,
                    {0},
                    caller_zone_id,
                    object_id,
                    rpc::add_ref_options::build_caller_route | rpc::add_ref_options::build_destination_route);
            }
#endif
            uint64_t temp_ref_count;
            std::vector<rpc::back_channel_entry> empty_in;
            std::vector<rpc::back_channel_entry> empty_out;
            auto transport = object_service_proxy->get_transport();
            if (!transport)
            {
                CO_RETURN error::TRANSPORT_ERROR();
            }

            uint64_t err = CO_AWAIT transport->add_ref(protocol_version,
                {0},
                destination_zone_id,
                object_id,
                {0},
                caller_zone_id,
                known_direction_zone(zone_id_),
                rpc::add_ref_options::build_caller_route | rpc::add_ref_options::build_destination_route,
                temp_ref_count,
                empty_in,
                empty_out);
            if (err != error::OK())
            {
                CO_RETURN err;
            }
            std::ignore = temp_ref_count;
        }
        else
        {
            std::shared_ptr<rpc::service_proxy> destination_service_proxy = object_service_proxy;
            std::shared_ptr<rpc::service_proxy> caller;
            // bool need_add_ref = false;
            std::unordered_map<destination_zone, std::weak_ptr<service_proxy>>::iterator caller_found_iter;
            bool need_caller_from_found = false;
            {
                std::lock_guard g(zone_control);
                {
                    auto found = other_zones.find(destination_zone_id); // we dont need to get caller id for this
                    if (found != other_zones.end())
                    {
                        destination_service_proxy = found->second.lock();
                        // need_add_ref = (destination_service_proxy != nullptr); // Mark that we need add_ref outside lock
                    }
                    else
                    {
                        destination_service_proxy = object_service_proxy->clone_for_zone(destination_zone_id);
                        inner_add_zone_proxy(destination_service_proxy);
                    }
                }
                // and the caller with destination info
                {
                    auto found = other_zones.find({caller_channel}); // we dont need to get caller id for this
                    if (found == other_zones.end())
                    {
                        // this is working on the premise that the caller_channel_zone_id is not known but caller_channel is
                        RPC_ASSERT(caller_channel == caller_channel_zone_id.get_val()
                                   && caller_channel != caller_zone_id.get_val());

                        auto alternative = other_zones.find(caller_zone_id.as_destination());
                        if (alternative == other_zones.end())
                        {
                            RPC_ASSERT(!!"alternative route to caller zone is not found");
                            CO_RETURN error::SERVICE_PROXY_LOST_CONNECTION();
                        }

                        auto alternative_caller_service_proxy = alternative->second.lock();
                        if (!alternative_caller_service_proxy)
                        {
                            RPC_ERROR("alternative_caller_service_proxy is null zone: {} destination_zone={}, "
                                      "caller_zone = {}",
                                std::to_string(zone_id_),
                                std::to_string(caller_zone_id.as_destination()),
                                std::to_string(0));
                            RPC_ASSERT(alternative_caller_service_proxy);
                            CO_RETURN error::SERVICE_PROXY_LOST_CONNECTION();
                        }

                        // now make a copy of the original as we need it back
                        caller = alternative_caller_service_proxy->clone_for_zone(
                            {caller_channel_zone_id.get_val()});
                        other_zones[caller_channel_zone_id.get_val()] = caller;
                        RPC_DEBUG("prepare_out_param service zone: {} destination_zone={}",
                            std::to_string(zone_id_),
                            std::to_string(caller->destination_zone_id_));
                    }
                    else
                    {
                        // Don't lock here - do it outside mutex to prevent TOCTOU race
                        caller_found_iter = found;
                        need_caller_from_found = true;
                    }
                }
            }

            // Lock caller outside mutex to prevent TOCTOU race
            if (need_caller_from_found)
            {
                caller = caller_found_iter->second.lock();
                if (!caller)
                {
                    // caller service_proxy was destroyed - this is a race condition
                    RPC_ERROR("caller service_proxy was destroyed during lookup");
                    // Return empty interface descriptor to indicate failure
                    CO_RETURN error::SERVICE_PROXY_LOST_CONNECTION();
                }
            }

            // Verify we have a valid caller
            if (!caller)
            {
                RPC_ERROR("Failed to obtain valid caller service_proxy");
                CO_RETURN error::SERVICE_PROXY_LOST_CONNECTION();
            }

            // // Call add_external_ref() outside the mutex to prevent race with service_proxy destruction
            // if (need_add_ref && destination_service_proxy)
            // {
            //     destination_service_proxy->add_external_ref();
            // }

#ifdef USE_RPC_TELEMETRY
            if (auto telemetry_service = rpc::get_telemetry_service(); telemetry_service)
            {
                telemetry_service->on_service_proxy_add_ref(
                    zone_id_, destination_zone_id, {0}, caller_zone_id, object_id, rpc::add_ref_options::build_destination_route);
            }
#endif

            // the fork is here so we need to add ref the destination normally with caller info
            // note the caller_channel_zone_id is is this zones id as the caller came from a route via
            // this node 
            int refcount = 0;
            if (destination_service_proxy)
            {
                uint64_t temp_ref_count;
                std::vector<rpc::back_channel_entry> empty_in;
                std::vector<rpc::back_channel_entry> empty_out;

                auto transport = destination_service_proxy->get_transport();
                if (!transport)
                {
                    CO_RETURN error::TRANSPORT_ERROR();
                }

                refcount = CO_AWAIT transport->add_ref(protocol_version,
                    {0},
                    destination_zone_id,
                    object_id,
                    zone_id_.as_caller_channel(),
                    caller_zone_id,
                    known_direction_zone(zone_id_),
                    rpc::add_ref_options::build_destination_route,
                    temp_ref_count,
                    empty_in,
                    empty_out);
            }
            else
            {
                RPC_ERROR("destination_zone service_proxy was destroyed during operation");
                CO_RETURN error::SERVICE_PROXY_LOST_CONNECTION();
            }
            std::ignore = refcount;

#ifdef USE_RPC_TELEMETRY
            if (auto telemetry_service = rpc::get_telemetry_service(); telemetry_service)
            {
                telemetry_service->on_service_proxy_add_ref(zone_id_,
                    destination_zone_id,
                    zone_id_.as_destination_channel(),
                    caller_zone_id,
                    object_id,
                    rpc::add_ref_options::build_caller_route);
            }
#endif

            {
                auto caller_transport = caller->get_transport();
                if (!caller_transport)
                {
                    CO_RETURN error::TRANSPORT_ERROR();
                }

                // note the caller_channel_zone_id is 0 as the caller came from this route
                uint64_t temp_ref_count;
                std::vector<rpc::back_channel_entry> empty_in;
                std::vector<rpc::back_channel_entry> empty_out;
                auto err_code = CO_AWAIT caller_transport->add_ref(protocol_version,
                    zone_id_.as_destination_channel(),
                    destination_zone_id,
                    object_id,
                    {0},
                    caller_zone_id,
                    known_direction_zone(zone_id_),
                    rpc::add_ref_options::build_caller_route,
                    temp_ref_count,
                    empty_in,
                    empty_out);
                if(err_code != error::OK())
                    CO_RETURN err_code;
            }
        }

        descriptor = {object_id, destination_zone_id};
        CO_RETURN error::OK();
    }

    // this is a key function that returns an interface descriptor
    // for wrapping an implementation to a local object inside a stub where needed
    // or if the interface is a proxy to add ref it
    CORO_TASK(int)
    service::get_proxy_stub_descriptor(uint64_t protocol_version,
        caller_channel_zone caller_channel_zone_id,
        caller_zone caller_zone_id,
        rpc::casting_interface* iface,
        std::function<std::shared_ptr<rpc::i_interface_stub>(std::shared_ptr<rpc::object_stub>)> fn,
        bool outcall,
        std::shared_ptr<rpc::object_stub>& stub,
        interface_descriptor& descriptor)
    {
        if (outcall)
        {
            rpc::casting_interface* casting_interface = nullptr;
            if ((caller_channel_zone_id.is_set() || caller_zone_id.is_set()) && !iface->is_local())
            {
                CO_RETURN CO_AWAIT prepare_out_param(
                    protocol_version, caller_channel_zone_id, caller_zone_id, iface, descriptor);
            }
        }

        {
            auto* pointer = iface->get_address();
            // find the stub by its address
            {
                std::lock_guard g(stub_control);
                auto item = wrapped_object_to_stub.find(pointer);
                if (item != wrapped_object_to_stub.end())
                {
                    stub = item->second.lock();
                    // Don't mask the race condition - if stub is null here, we have a serious problem
                    RPC_ASSERT(stub != nullptr);
                    stub->add_ref(false);
                }
                else
                {
                    // else create a stub
                    auto id = generate_new_object_id();
                    stub = std::make_shared<object_stub>(id, shared_from_this(), pointer);
                    std::shared_ptr<rpc::i_interface_stub> interface_stub = fn(stub);
                    stub->add_interface(interface_stub);
                    wrapped_object_to_stub[pointer] = stub;
                    stubs[id] = stub;
                    stub->on_added_to_zone(stub);
                    stub->add_ref(false);
                }
            }
        }
        /*if (outcall)
        {
            // needed by the out call
            std::shared_ptr<rpc::service_proxy> caller;
            {
                std::lock_guard g(zone_control);
                uint64_t caller_channel
                    = caller_channel_zone_id.is_set() ? caller_channel_zone_id.get_val() : caller_zone_id.get_val();
                RPC_ASSERT(caller_channel);
                // and the caller with destination info
                auto found = other_zones.find(
                    {{caller_channel}, zone_id_.as_caller()}); // we dont need to get caller id for this
                if (found != other_zones.end())
                {
                    caller = found->second.lock();
                }
                else
                {
                    // unexpected code path
                    RPC_ASSERT(false);
                }
                RPC_ASSERT(caller);
            }

#ifdef USE_RPC_TELEMETRY
            if (auto telemetry_service = rpc::get_telemetry_service(); telemetry_service)
            {
                telemetry_service->on_service_proxy_add_ref(zone_id_,
                    zone_id_.as_destination(),
                    {0},
                    caller_zone_id,
                    stub->get_id(),
                    rpc::add_ref_options::build_caller_route);
            }
#endif
            // note the caller_channel_zone_id is 0 as the caller came from this route
            uint64_t temp_ref_count;
            std::vector<rpc::back_channel_entry> empty_in;
            std::vector<rpc::back_channel_entry> empty_out;
            auto transport = caller->get_transport();
            if(!transport)
            {
                CO_RETURN error::TRANSPORT_ERROR();
            }
            auto err = CO_AWAIT transport->add_ref(protocol_version,
                {0},
                zone_id_.as_destination(),
                stub->get_id(),
                {0},
                caller_zone_id,
                known_direction_zone(zone_id_),
                rpc::add_ref_options::build_caller_route,
                temp_ref_count,
                empty_in,
                empty_out);
            if(err != error::OK())
            {
                CO_RETURN err;
            }
        }*/
        descriptor = {stub->get_id(), zone_id_.as_destination()};
        CO_RETURN error::OK();
    }

    std::weak_ptr<object_stub> service::get_object(object object_id) const
    {
        std::lock_guard l(stub_control);
        auto item = stubs.find(object_id);
        if (item == stubs.end())
        {
            // Stub has been deleted - can happen with optimistic_ptr when shared_ptr is released
            return std::weak_ptr<object_stub>();
        }

        return item->second;
    }
    CORO_TASK(int)
    service::try_cast(uint64_t protocol_version,
        destination_zone destination_zone_id,
        object object_id,
        interface_ordinal interface_id,
        const std::vector<rpc::back_channel_entry>& in_back_channel,
        std::vector<rpc::back_channel_entry>& out_back_channel)
    {
        if (destination_zone_id != zone_id_.as_destination())
        {
            RPC_ASSERT(false); // this should be going to the pass through
            CO_RETURN error::TRANSPORT_ERROR();
        }
        current_service_tracker tracker(this);

        if (protocol_version < rpc::LOWEST_SUPPORTED_VERSION || protocol_version > rpc::HIGHEST_SUPPORTED_VERSION)
        {
            RPC_ERROR("Unsupported service version {} in try_cast", protocol_version);
            CO_RETURN rpc::error::INVALID_VERSION();
        }
        std::weak_ptr<object_stub> weak_stub = get_object(object_id);
        auto stub = weak_stub.lock();
        if (!stub)
        {
            RPC_ERROR("Invalid data - stub is null in try_cast");
            CO_RETURN error::INVALID_DATA();
        }
        CO_RETURN stub->try_cast(interface_id);
    }

    CORO_TASK(int)
    service::add_ref(uint64_t protocol_version,
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
        auto build_channel = !!(build_out_param_channel & add_ref_options::build_destination_route)
                             || !!(build_out_param_channel & add_ref_options::build_caller_route);

        // note if known_direction_zone_id is always 0 test_y_topology_and_set_host_with_prong_object will fail.
        // known_direction_zone_id.get_val() = 0;
        current_service_tracker tracker(this);
#ifdef USE_RPC_TELEMETRY
        if (auto telemetry_service = rpc::get_telemetry_service(); telemetry_service)
        {
            telemetry_service->on_service_add_ref(zone_id_,
                destination_channel_zone_id,
                destination_zone_id,
                object_id,
                caller_channel_zone_id,
                caller_zone_id,
                build_out_param_channel);
        }
#endif

        auto dest_channel = destination_zone_id.get_val();
        if (destination_channel_zone_id != zone_id_.as_destination_channel() && destination_channel_zone_id.get_val() != 0)
            dest_channel = destination_channel_zone_id.get_val();
        auto caller_channel
            = caller_channel_zone_id.is_set() ? caller_channel_zone_id.get_val() : caller_zone_id.get_val();

        if (destination_zone_id != zone_id_.as_destination())
        {
            // service is being used as a bridge to other zones

            auto build_channel = !!(build_out_param_channel & add_ref_options::build_destination_route)
                                 || !!(build_out_param_channel & add_ref_options::build_caller_route);

            if (dest_channel == caller_channel && build_channel)
            {
                RPC_ASSERT(false); // this should be going to the pass through
                CO_RETURN error::TRANSPORT_ERROR();
            }
            else if (build_channel)
            {
                //                 // we are here as this zone needs to send the destination addref and caller addref to
                //                 different zones a fork is in progress std::shared_ptr<rpc::service_proxy>
                //                 destination; std::shared_ptr<rpc::service_proxy> caller;
                //                 {
                //                     // bool has_called_inner_add_zone_proxy = false;
                //                     {
                //                         std::lock_guard g(zone_control);

                //                         auto found = other_zones.find(destination_zone_id);
                //                         if (found != other_zones.end())
                //                         {
                //                             destination = found->second.lock();
                //                             // Move add_external_ref() outside the lock to prevent TOCTOU race
                //                         }
                //                         else
                //                         {
                //                             found = other_zones.lower_bound({dest_channel});
                //                             if (found != other_zones.end() && found->first.dest.get_val() == dest_channel)
                //                             {
                //                                 auto tmp = found->second.lock();
                //                                 if (!tmp)
                //                                 {
                //                                     RPC_ERROR("tmp is null zone: {} destination_zone={}, caller_zone={}",
                //                                         std::to_string(zone_id_),
                //                                         std::to_string(dest_channel),
                //                                         std::to_string(0));
                //                                     RPC_ASSERT(tmp);
                //                                     CO_RETURN rpc::error::OBJECT_NOT_FOUND();
                //                                 }
                //                                 destination = tmp->clone_for_zone(destination_zone_id);
                //                             }
                //                             else
                //                             {
                //                                 // with the Y bug fix we should not get here under normal operation as the destination is always valid
                //                                 RPC_ASSERT(false);
                //                                 CO_RETURN error::ZONE_NOT_FOUND();
                //                                 // // get the parent to route it
                //                                 // RPC_ASSERT(get_parent() != nullptr);
                //                                 // destination = get_parent()->clone_for_zone(destination_zone_id, caller_zone_id);
                //                             }
                //                             inner_add_zone_proxy(destination);
                //                             // has_called_inner_add_zone_proxy = true;
                //                         }

                //                         // detect if the caller is this zone, if so dont try and find a service proxy for it
                //                         if (caller_zone_id == zone_id_.as_caller())
                //                         {
                //                             build_out_param_channel
                //                                 = build_out_param_channel ^ add_ref_options::build_caller_route; // strip out this bit
                //                         }
                //                         else
                //                         {
                //                             // connect the remote sender to the destinaton
                //                             found = other_zones.lower_bound({{caller_channel}, {0}});
                //                             if (found != other_zones.end() && found->first.dest.get_val() == caller_channel)
                //                             {
                //                                 caller = found->second.lock();
                //                                 if (!caller)
                //                                 {
                //                                     RPC_ERROR("caller is null zone: {} destination_zone={}, caller_zone={}",
                //                                         std::to_string(zone_id_),
                //                                         std::to_string(caller_channel),
                //                                         std::to_string(0));
                //                                     RPC_ASSERT(caller);
                //                                     CO_RETURN rpc::error::OBJECT_NOT_FOUND();
                //                                 }
                //                             }
                //                             else
                //                             {
                //                                 // with the Y bug fix we should not get here under normal operation as the destination is always valid
                //                                 RPC_ASSERT(false);
                //                                 CO_RETURN error::ZONE_NOT_FOUND();

                //                                 // caller = get_parent();

                //                                 // if (!caller)
                //                                 // {
                //                                 //     RPC_ERROR("get_parent() returned: nullptr - THIS IS A PROBLEM!");
                //                                 //     RPC_ASSERT(caller);
                //                                 // }
                //                             }
                //                         }
                //                     }

                //                     // Call add_external_ref() outside mutex to prevent TOCTOU race
                //                     // if (destination && !has_called_inner_add_zone_proxy)
                //                     // {
                //                     //     destination->add_external_ref();
                //                     // }

                //                     /*do
                //                     {
                //                         // this more fiddly check is to route calls to a parent node that knows more about this
                //                         if (destination && caller
                //                             && build_out_param_channel
                //                                    == (add_ref_options::build_caller_route | add_ref_options::build_destination_route))
                //                         {
                //                             auto dc = destination->get_destination_channel_zone_id().is_set()
                //                                           ? destination->get_destination_channel_zone_id().get_val()
                //                                           : destination->get_destination_zone_id().get_val();
                //                             auto cc = caller->get_destination_channel_zone_id().is_set()
                //                                           ? caller->get_destination_channel_zone_id().get_val()
                //                                           : caller->get_destination_zone_id().get_val();
                //                             if (dc == cc)
                //                             {
                // #ifdef USE_RPC_TELEMETRY
                //                                 if (auto telemetry_service = rpc::get_telemetry_service(); telemetry_service)
                //                                 {
                //                                     telemetry_service->on_service_proxy_add_ref(
                //                                         zone_id_, destination_zone_id, {0}, caller_zone_id, object_id, build_out_param_channel);
                //                                 }
                // #endif

                //                                 std::vector<rpc::back_channel_entry> empty_in;
                //                                 std::vector<rpc::back_channel_entry> empty_out;
                //                                 auto ret = CO_AWAIT destination->add_ref(protocol_version,
                //                                     {0},
                //                                     destination_zone_id,
                //                                     object_id,
                //                                     {0},
                //                                     caller_zone_id,
                //                                     known_direction_zone_id,
                //                                     build_out_param_channel,
                //                                     reference_count,
                //                                     empty_in,
                //                                     empty_out);
                //                                 destination->release_external_ref(); // perhaps this could be
                //                                 optimised if (ret != rpc::error::OK())
                //                                 {
                //                                     reference_count = 0;
                //                                     CO_RETURN ret;
                //                                 }
                //                                 break;
                //                             }
                //                         }

                //                         // then call the add ref to the destination
                //                         if (!!(build_out_param_channel & add_ref_options::build_destination_route))
                //                         {
                // #ifdef USE_RPC_TELEMETRY
                //                             if (auto telemetry_service = rpc::get_telemetry_service(); telemetry_service)
                //                             {
                //                                 telemetry_service->on_service_proxy_add_ref(zone_id_,
                //                                     destination_zone_id,
                //                                     {0},
                //                                     caller_zone_id,
                //                                     object_id,
                //                                     add_ref_options::build_destination_route);
                //                             }
                // #endif
                //                             uint64_t temp_ref_count;
                //                             std::vector<rpc::back_channel_entry> empty_in;
                //                             std::vector<rpc::back_channel_entry> empty_out;
                //                             auto err = CO_AWAIT destination->add_ref(protocol_version,
                //                                 {0},
                //                                 destination_zone_id,
                //                                 object_id,
                //                                 zone_id_.as_caller_channel(),
                //                                 caller_zone_id,
                //                                 known_direction_zone_id,
                //                                 add_ref_options::build_destination_route,
                //                                 temp_ref_count,
                //                                 empty_in,
                //                                 empty_out);
                //                             RPC_ASSERT(err == error::OK());
                //                         }
                //                         // back fill the ref count to the caller
                //                         if (!!(build_out_param_channel & add_ref_options::build_caller_route))
                //                         {
                // #ifdef USE_RPC_TELEMETRY
                //                             if (auto telemetry_service = rpc::get_telemetry_service(); telemetry_service)
                //                             {
                //                                 telemetry_service->on_service_proxy_add_ref(caller->zone_id_,
                //                                     destination_zone_id,
                //                                     zone_id_.as_destination_channel(),
                //                                     caller_zone_id,
                //                                     object_id,
                //                                     add_ref_options::build_caller_route);
                //                             }
                // #endif

                //                             uint64_t temp_ref_count;
                //                             std::vector<rpc::back_channel_entry> empty_in;
                //                             std::vector<rpc::back_channel_entry> empty_out;
                //                             auto err = CO_AWAIT caller->add_ref(protocol_version,
                //                                 zone_id_.as_destination_channel(),
                //                                 destination_zone_id,
                //                                 object_id,
                //                                 caller_channel_zone_id,
                //                                 caller_zone_id,
                //                                 known_direction_zone_id,
                //                                 add_ref_options::build_caller_route,
                //                                 temp_ref_count,
                //                                 empty_in,
                //                                 empty_out);
                //                             std::ignore = err;
                //                         }
                //                     } while (false);
                //                     */
                //                 }

                /*{
                    std::lock_guard g(zone_control);
                    auto destination_copy = destination;
                    destination = nullptr;
                    auto caller_copy = caller;
                    caller = nullptr;

                    if(destination_copy && destination_copy->is_unused())
                    {
                        auto found = other_zones.find({destination_copy->get_destination_zone_id(),
                destination_copy->get_caller_zone_id()}); // we dont need to get caller id for this if (found !=
                other_zones.end())
                        {
                            other_zones.erase(found);
                        }
                        else
                        {

                            RPC_ASSERT(false);
                            return error::REFERENCE_COUNT_ERROR();
                        }
                    }
                    destination_copy = nullptr;
                    if(caller_copy && caller_copy->is_unused())
                    {
                        auto found = other_zones.find({caller_copy->get_destination_zone_id(),
                caller_copy->get_caller_zone_id()}); // we dont need to get caller id for this if (found !=
                other_zones.end())
                        {
                            other_zones.erase(found);
                        }
                        else
                        {

                            RPC_ASSERT(false);
                            return error::REFERENCE_COUNT_ERROR();
                        }
                    }
                    caller_copy = nullptr;
                }*/

                CO_RETURN error::OK();
            }
            else
            {
                RPC_ASSERT(false); // this should be going to the pass through
                CO_RETURN error::TRANSPORT_ERROR();
            }
        }
        else
        {
            // service has the implementation
            if (protocol_version < rpc::LOWEST_SUPPORTED_VERSION || protocol_version > rpc::HIGHEST_SUPPORTED_VERSION)
            {
                reference_count = 0;
                RPC_ERROR("Unsupported service version {} in add_ref", protocol_version);
                CO_RETURN rpc::error::INVALID_VERSION();
            }

            // find the caller
            //             if (zone_id_.as_caller() != caller_zone_id && !!(build_out_param_channel & add_ref_options::build_caller_route))
            //             {
            //                 std::shared_ptr<rpc::service_proxy> caller;
            //                 {
            //                     std::lock_guard g(zone_control);
            //                     // we swap the parameter types as this is from perspective of the caller and not the proxy that called this function
            //                     // there should always be a destination service_proxy in
            //                     // other_zones as the requester has a positive ref count to it through this service

            //                     auto found = other_zones.lower_bound({caller_zone_id.as_destination(), {0}});
            //                     if (found != other_zones.end() && found->first.dest == caller_zone_id.as_destination())
            //                     {
            //                         caller = found->second.lock();
            //                         if (!caller)
            //                         {
            //                             RPC_ERROR("caller is null zone: {} destination_zone={}, caller_zone={}",
            //                                 std::to_string(zone_id_),
            //                                 std::to_string(caller_zone_id),
            //                                 std::to_string(0));
            //                             RPC_ASSERT(caller);
            //                             CO_RETURN rpc::error::OBJECT_NOT_FOUND();
            //                         }
            //                     }
            //                     else
            //                     {
            //                         RPC_ERROR("Unable to build add_ref_options::build_caller_route");
            //                         RPC_ASSERT(false);
            //                         CO_RETURN rpc::error::OBJECT_NOT_FOUND();
            //                     }
            //                 }
            // #ifdef USE_RPC_TELEMETRY
            //                 if (auto telemetry_service = rpc::get_telemetry_service(); telemetry_service)
            //                 {
            //                     telemetry_service->on_service_proxy_add_ref(
            //                         zone_id_, destination_zone_id, {0}, caller_zone_id, object_id, add_ref_options::build_caller_route);
            //                 }
            // #endif
            //                 uint64_t temp_ref_count;
            //                 std::vector<rpc::back_channel_entry> empty_in;
            //                 std::vector<rpc::back_channel_entry> empty_out;
            //                 auto caller_transport = caller->get_transport();
            //                 auto err = CO_AWAIT caller_transport->add_ref(protocol_version,
            //                     {0},
            //                     destination_zone_id,
            //                     object_id,
            //                     {},
            //                     caller_zone_id,
            //                     known_direction_zone_id,
            //                     add_ref_options::build_caller_route,
            //                     temp_ref_count,
            //                     empty_in,
            //                     empty_out);
            //                 std::ignore = err;
            //             }
            if (object_id == dummy_object_id)
            {
                reference_count = 0;
                CO_RETURN rpc::error::OK();
            }

            std::weak_ptr<object_stub> weak_stub = get_object(object_id);
            auto stub = weak_stub.lock();
            if (!stub)
            {
                RPC_ASSERT(false);
                reference_count = 0;
                CO_RETURN rpc::error::OBJECT_NOT_FOUND();
            }
            reference_count = stub->add_ref(!!(build_out_param_channel & add_ref_options::optimistic));
            CO_RETURN rpc::error::OK();
        }
    }

    uint64_t service::release_local_stub(const std::shared_ptr<rpc::object_stub>& stub, bool is_optimistic)
    {
        std::lock_guard l(stub_control);
        uint64_t count = stub->release(is_optimistic);
        if (!is_optimistic && !count)
        {
            {
                stubs.erase(stub->get_id());
            }
            {
                auto* pointer = stub->get_castable_interface()->get_address();
                auto it = wrapped_object_to_stub.find(pointer);
                if (it != wrapped_object_to_stub.end())
                {
                    wrapped_object_to_stub.erase(it);
                }
                else
                {
                    // if you get here make sure that get_address is defined in the most derived class
                    RPC_ASSERT(false);
                }
            }
            stub->reset();
        }
        return count;
    }

    void service::cleanup_service_proxy(const std::shared_ptr<rpc::service_proxy>& other_zone)
    {
        /*bool should_cleanup = !other_zone->release_external_ref();
        if (should_cleanup)
        {
            RPC_DEBUG("service::release cleaning up unused routing service_proxy destination_zone={}, caller_zone={}",
                std::to_string(other_zone->get_destination_zone_id()),
                std::to_string(other_zone->get_caller_zone_id()));

            std::lock_guard g(zone_control);

            // Routing service_proxies should NEVER have object_proxies - this is a bug
            if (!other_zone->proxies_.empty())
            {
#ifdef USE_RPC_LOGGING
                RPC_ERROR("BUG: Routing service_proxy (destination_zone={} != zone={}) has {} object_proxies - routing "
                          "proxies should never host objects",
                    other_zone->get_destination_zone_id().get_val(),
                    zone_id_.get_val(),
                    other_zone->proxies_.size());

                // Log details of the problematic object_proxies for debugging
                for (const auto& proxy_pair : other_zone->proxies_)
                {
                    auto object_proxy_ptr = proxy_pair.second.lock();
                    RPC_ERROR("  BUG: object_proxy object_id={} in routing service_proxy, alive={}",
                        proxy_pair.first.get_val(),
                        (object_proxy_ptr ? "yes" : "no"));
                }
#endif

                // This should not happen - routing service_proxies should not have object_proxies
                RPC_ERROR("Routing service_proxy should not have object_proxies destination_zone={}, caller_zone={}",
                    std::to_string(other_zone->get_destination_zone_id()),
                    std::to_string(other_zone->get_caller_zone_id()));
                RPC_ASSERT(false);
            }

            auto found_again = other_zones.find({other_zone->get_destination_zone_id(),
other_zone->get_caller_zone_id()}); if (found_again != other_zones.end())
            {
                auto sp_check = found_again->second.lock();
                RPC_ASSERT(other_zone == sp_check);
                if (!sp_check || sp_check->is_unused())
                {
                    other_zones.erase(found_again);
                }
            }
            else
            {
                RPC_ERROR("dying service proxy not found destination_zone={}, caller_zone={}",
                    std::to_string(other_zone->get_destination_zone_id()),
                    std::to_string(other_zone->get_caller_zone_id()));
                RPC_ASSERT(!"dying proxy not found");
            }
        }*/
    }

    CORO_TASK(int)
    service::release(uint64_t protocol_version,
        destination_zone destination_zone_id,
        object object_id,
        caller_zone caller_zone_id,
        release_options options,
        uint64_t& reference_count,
        const std::vector<rpc::back_channel_entry>& in_back_channel,
        std::vector<rpc::back_channel_entry>& out_back_channel)
    {
        current_service_tracker tracker(this);

        //         if (destination_zone_id != zone_id_.as_destination())
        //         {
        //             std::shared_ptr<rpc::service_proxy> other_zone;
        //             {
        //                 std::lock_guard g(zone_control);
        //                 auto found = other_zones.find({destination_zone_id, caller_zone_id});
        //                 if (found != other_zones.end())
        //                 {
        //                     other_zone = found->second.lock();
        //                 }
        //             }
        //             if (!other_zone)
        //             {
        //                 RPC_ERROR("service::release other_zone is null destination_zone={}, caller_zone={}",
        //                     std::to_string(destination_zone_id),
        //                     std::to_string(caller_zone_id));
        //                 RPC_ASSERT(false);
        //                 reference_count = 0;
        //                 CO_RETURN rpc::error::ZONE_NOT_FOUND();
        //             }
        // #ifdef USE_RPC_TELEMETRY
        //             if (auto telemetry_service = rpc::get_telemetry_service(); telemetry_service)
        //                 telemetry_service->on_service_release(
        //                     zone_id_, other_zone->get_destination_channel_zone_id(), destination_zone_id, object_id, caller_zone_id);
        // #endif
        //             auto ret = CO_AWAIT other_zone->sp_release(object_id, options, reference_count);
        //             if (ret == error::OK())
        //             {
        //                 cleanup_service_proxy(other_zone);
        //                 CO_RETURN rpc::error::OK();
        //             }
        //             else
        //             {
        //                 reference_count = 0;
        //                 CO_RETURN rpc::error::OBJECT_NOT_FOUND();
        //             }
        //         }
        //         else
        {
#ifdef USE_RPC_TELEMETRY
            if (auto telemetry_service = rpc::get_telemetry_service(); telemetry_service)
                telemetry_service->on_service_release(zone_id_, {0}, destination_zone_id, object_id, caller_zone_id);
#endif

            if (protocol_version < rpc::LOWEST_SUPPORTED_VERSION || protocol_version > rpc::HIGHEST_SUPPORTED_VERSION)
            {
                reference_count = 0;
                RPC_ERROR("Unsupported service version {} in release", protocol_version);
                CO_RETURN rpc::error::INVALID_VERSION();
            }

            bool reset_stub = false;
            std::shared_ptr<rpc::object_stub> stub;
            uint64_t count = 0;
            // these scope brackets are needed as otherwise there will be a recursive lock on a mutex in rare cases when the stub is reset
            {
                {
                    // a scoped lock
                    std::lock_guard l(stub_control);
                    auto item = stubs.find(object_id);
                    if (item == stubs.end())
                    {
                        // Stub has been deleted - can happen with optimistic_ptr when shared_ptr is released
                        reference_count = 0;
                        CO_RETURN rpc::error::OBJECT_NOT_FOUND();
                    }

                    stub = item->second.lock();
                }

                if (!stub)
                {
                    RPC_ASSERT(false);
                    reference_count = 0;
                    CO_RETURN rpc::error::OBJECT_NOT_FOUND();
                }
                // this guy needs to live outside of the mutex or deadlocks may happen
                count = stub->release(!!(release_options::optimistic & options));
                if (!count && !(release_options::optimistic & options))
                {
                    {
                        // a scoped lock
                        std::lock_guard l(stub_control);
                        {
                            stubs.erase(object_id);
                        }
                        {
                            auto* pointer = stub->get_castable_interface()->get_address();
                            auto it = wrapped_object_to_stub.find(pointer);
                            if (it != wrapped_object_to_stub.end())
                            {
                                wrapped_object_to_stub.erase(it);
                            }
                            else
                            {
                                RPC_ASSERT(false);
                                reference_count = 0;
                                CO_RETURN rpc::error::OBJECT_NOT_FOUND();
                            }
                        }
                    }
                    stub->reset();
                    reset_stub = true;
                }
            }

            if (reset_stub)
                stub->reset();

            reference_count = count;
            CO_RETURN rpc::error::OK();
        }
    }

    void service::inner_add_zone_proxy(const std::shared_ptr<rpc::service_proxy>& service_proxy)
    {
        // this is for internal use only has no lock
        // service_proxy->add_external_ref();
        auto destination_zone_id = service_proxy->get_destination_zone_id();
        // auto caller_zone_id = service_proxy->get_caller_zone_id();
        RPC_ASSERT(destination_zone_id != zone_id_.as_destination());
        RPC_ASSERT(other_zones.find(destination_zone_id) == other_zones.end());
        other_zones[destination_zone_id] = service_proxy;
        transports[destination_zone_id] = service_proxy->get_transport();
        RPC_DEBUG("inner_add_zone_proxy service zone: {} destination_zone={}",
            std::to_string(zone_id_),
            std::to_string(service_proxy->destination_zone_id_));
    }

    void service::add_zone_proxy(const std::shared_ptr<rpc::service_proxy>& service_proxy)
    {
        RPC_ASSERT(service_proxy->get_destination_zone_id() != zone_id_.as_destination());
        std::lock_guard g(zone_control);
        inner_add_zone_proxy(service_proxy);
    }

    void service::add_transport(destination_zone adjacent_zone_id, const std::shared_ptr<transport>& transport_ptr)
    {
        std::lock_guard g(zone_control);
        transports[adjacent_zone_id] = transport_ptr;
        RPC_DEBUG("add_transport service zone: {} adjacent_zone_id={}",
            std::to_string(zone_id_),
            std::to_string(adjacent_zone_id));
    }

    void service::remove_transport(destination_zone adjacent_zone_id)
    {
        std::lock_guard g(zone_control);
        auto it = transports.find(adjacent_zone_id);
        if (it != transports.end())
        {
            transports.erase(it);
            RPC_DEBUG("remove_transport service zone: {} adjacent_zone_id={}",
                std::to_string(zone_id_),
                std::to_string(adjacent_zone_id));
        }
    }

    std::shared_ptr<rpc::transport> service::get_transport(destination_zone destination_zone_id) const
    {
        std::lock_guard g(zone_control);

        // Try to find a direct transport to the destination zone
        auto item = transports.find(destination_zone_id);
        if (item != transports.end())
        {
            auto transport = item->second.lock();
            if (transport)
            {
                return transport;
            }
        }
        return nullptr;
    }

    std::shared_ptr<rpc::service_proxy> service::get_zone_proxy(caller_channel_zone caller_channel_zone_id,
        caller_zone caller_zone_id,
        destination_zone destination_zone_id,
        caller_zone new_caller_zone_id,
        bool& new_proxy_added)
    {
        new_proxy_added = false;
        std::lock_guard g(zone_control);

        // find if we have one
        {
            auto item = other_zones.find(destination_zone_id);
            if (item != other_zones.end())
                return item->second.lock();
        }

        // Try to find a direct transport to the destination zone
        {
            auto item = transports.find(destination_zone_id);
            if (item != transports.end())
            {
                auto transport = item->second.lock();
                if (transport)
                {
                    auto proxy = std::make_shared<service_proxy>(transport, destination_zone_id);
                    inner_add_zone_proxy(proxy);
                    new_proxy_added = true;
                    return proxy;
                }
            }
        }

        // If no direct transport, fall back to transport via caller channel
        {
            auto item = transports.find(caller_channel_zone_id.as_destination());
            if (item != transports.end())
            {
                auto transport = item->second.lock();
                if (transport)
                {
                    auto proxy = std::make_shared<service_proxy>(transport, destination_zone_id);
                    inner_add_zone_proxy(proxy);
                    new_proxy_added = true;
                    return proxy;
                }
            }
            return nullptr;
        }

        /*item = other_zones.lower_bound(destination_zone_id});

        if (item != other_zones.end() && item->first.dest != destination_zone_id)
            item = other_zones.end();

        // if not we can make one from the proxy of the calling channel zone
        // this zone knows nothing about the destination zone however the caller channel zone will know how to connect
        to it if (item == other_zones.end() && caller_channel_zone_id.is_set())
        {
            item = other_zones.lower_bound({caller_channel_zone_id.as_destination(), {0}});
            if (item == other_zones.end() || item->first.dest != caller_channel_zone_id.as_destination())
            {
                RPC_ASSERT(false); // something is wrong the caller channel should always be valid if specified
                return nullptr;
            }
        }
        // or if not we can make one from the proxy of the calling  zone
        if (item == other_zones.end() && caller_zone_id.is_set())
        {
            item = other_zones.lower_bound({caller_zone_id.as_destination(), {0}});
            if (item == other_zones.end() || item->first.dest != caller_zone_id.as_destination())
            {
                RPC_ASSERT(false); // something is wrong the caller should always be valid if specified
                return nullptr;
            }
        }
        if (item == other_zones.end())
        {
            RPC_ASSERT(false); // something is wrong we should not get here
            return nullptr;
        }

        auto calling_proxy = item->second.lock();
        if (!calling_proxy)
        {
            RPC_ASSERT(!"Race condition"); // we have a race condition
            return nullptr;
        }

        auto proxy = calling_proxy->clone_for_zone(destination_zone_id);
        inner_add_zone_proxy(proxy);
        new_proxy_added = true;
        return proxy;*/
        return nullptr;
    }

    void service::remove_zone_proxy(destination_zone destination_zone_id, caller_zone caller_zone_id)
    {
        {
            std::lock_guard g(zone_control);
            auto item = other_zones.find(destination_zone_id);
            if (item == other_zones.end())
            {
                RPC_ASSERT(false);
            }
            else
            {
                other_zones.erase(item);
            }
        }
    }

    // void service::remove_zone_proxy_if_not_used(destination_zone destination_zone_id, caller_zone caller_zone_id)
    // {
    //     {
    //         std::lock_guard g(zone_control);
    //         auto item = other_zones.find({destination_zone_id, caller_zone_id});
    //         if (item == other_zones.end())
    //         {
    //             RPC_ASSERT(false);
    //         }
    //         else
    //         {
    //             auto sp = item->second.lock();
    //             if (!sp || sp->is_unused())
    //             {
    //                 other_zones.erase(item);
    //             }
    //         }
    //     }
    // }

    int service::create_interface_stub(rpc::interface_ordinal interface_id,
        std::function<interface_ordinal(uint8_t)> interface_getter,
        const std::shared_ptr<rpc::i_interface_stub>& original,
        std::shared_ptr<rpc::i_interface_stub>& new_stub)
    {
        // an identity check, send back the same pointer
        if (interface_getter(rpc::VERSION_2) == interface_id)
        {
            new_stub = std::static_pointer_cast<rpc::i_interface_stub>(original);
            return rpc::error::OK();
        }

        auto it = stub_factories.find(interface_id);
        if (it == stub_factories.end())
        {
            RPC_INFO("stub factory does not have a record of this interface this not an error in the rpc stack");
            return rpc::error::INVALID_CAST();
        }

        new_stub = (*it->second)(original);
        if (!new_stub)
        {
            RPC_INFO("Object does not support the interface this not an error in the rpc stack");
            return rpc::error::INVALID_CAST();
        }
        // note a nullptr return value is a valid value, it indicates that this object does not implement that interface
        return rpc::error::OK();
    }

    // note this function is not thread safe!  Use it before using the service class for normal operation
    void service::add_interface_stub_factory(std::function<interface_ordinal(uint8_t)> id_getter,
        std::shared_ptr<std::function<std::shared_ptr<rpc::i_interface_stub>(const std::shared_ptr<rpc::i_interface_stub>&)>> factory)
    {
        auto interface_id = id_getter(rpc::VERSION_2);
        auto it = stub_factories.find({interface_id});
        if (it != stub_factories.end())
        {
            RPC_ERROR("Invalid data - add_interface_stub_factory failed");
            rpc::error::INVALID_DATA();
        }
        stub_factories[{interface_id}] = factory;
    }

    rpc::shared_ptr<casting_interface> service::get_castable_interface(object object_id, interface_ordinal interface_id)
    {
        auto ob = get_object(object_id).lock();
        if (!ob)
            return nullptr;
        auto interface_stub = ob->get_interface(interface_id);
        if (!interface_stub)
            return nullptr;
        return interface_stub->get_castable_interface();
    }

    void service::add_service_event(const std::weak_ptr<service_event>& event)
    {
        std::lock_guard g(service_events_control);
        service_events_.insert(event);
    }
    void service::remove_service_event(const std::weak_ptr<service_event>& event)
    {
        std::lock_guard g(service_events_control);
        service_events_.erase(event);
    }
    CORO_TASK(void) service::notify_object_gone_event(object object_id, destination_zone destination)
    {
        if (!service_events_.empty())
        {
            auto service_events_copy = service_events_;
            for (auto se : service_events_copy)
            {
                auto se_handler = se.lock();
                if (se_handler)
                    CO_AWAIT se_handler->on_object_released(object_id, destination);
            }
        }
        CO_RETURN;
    }

    child_service::~child_service()
    {
        // if (parent_service_proxy_)
        // {
        //     RPC_ASSERT(parent_service_proxy_->get_caller_zone_id() == zone_id_.as_caller());
        //     RPC_ASSERT(parent_service_proxy_->get_destination_channel_zone_id().get_val() == 0);
        //     // remove_zone_proxy is not callable by the proxy as the service is dying and locking on the weak pointer
        //     is now no longer possible other_zones.erase({parent_service_proxy_->get_destination_zone_id(),
        //     zone_id_.as_caller()}); parent_service_proxy_->set_parent_channel(false);
        //     parent_service_proxy_->release_external_ref();
        //     parent_service_proxy_ = nullptr;
        // }
    }
}
