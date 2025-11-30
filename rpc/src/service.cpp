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
                RPC_WARNING("service proxy zone_id {}, destination_zone_id {} "
                            "has not been released in the service suspected unclean shutdown",
                    std::to_string(zone_id_),
                    std::to_string(item.first));

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
    service::prepare_out_param(
        uint64_t protocol_version, caller_zone caller_zone_id, rpc::casting_interface* base, interface_descriptor& descriptor)
    {
        auto object_proxy = base->get_object_proxy();
        RPC_ASSERT(object_proxy != nullptr);
        auto object_service_proxy = object_proxy->get_service_proxy();
        RPC_ASSERT(object_service_proxy->zone_id_ == zone_id_);
        auto destination_zone_id = object_service_proxy->get_destination_zone_id();
        auto transport = object_service_proxy->get_transport();
        auto object_id = object_proxy->get_object_id();

        RPC_ASSERT(caller_zone_id.is_set());
        RPC_ASSERT(destination_zone_id.is_set());

        uint64_t temp_ref_count = 0;
        std::vector<rpc::back_channel_entry> empty_in;
        std::vector<rpc::back_channel_entry> empty_out;
        uint64_t err_code = 0;
        std::shared_ptr<rpc::i_marshaller> marshaller;

        if (caller_zone_id == destination_zone_id.as_caller())
        {
            marshaller = transport;
            if (!marshaller)
            {
                CO_RETURN error::TRANSPORT_ERROR();
            }
        }
        else
        {
            auto dest_tp = object_service_proxy->get_transport();
            marshaller = dest_tp->get_destination_handler(destination_zone_id, caller_zone_id.as_destination());
            if (!marshaller)
            {
                std::shared_ptr<rpc::transport> caller_transport;
                {
                    std::lock_guard g(zone_control);
                    // get the caller with destination info
                    auto found
                        = transports.find(caller_zone_id.as_destination()); // we dont need to get caller id for this
                    if (found == transports.end())
                    {
                        RPC_ERROR("No service proxy found for caller zone {}", caller_zone_id.get_val());
                        CO_RETURN error::ZONE_NOT_FOUND();
                    }
                    else
                    {
                        caller_transport = found->second.lock();
                        // Verify we have a valid caller
                        if (!caller_transport)
                        {
                            RPC_ERROR("Failed to obtain valid caller service_proxy");
                            CO_RETURN error::SERVICE_PROXY_LOST_CONNECTION();
                        }
                    }
                }

                // the fork is here so we need to add ref the destination normally with caller info
                auto object_transport = object_service_proxy->get_transport();
                if (object_transport == caller_transport)
                {
                    marshaller = object_transport;
                }
                else
                {
                    marshaller = transport::create_pass_through(object_service_proxy->get_transport(),
                        caller_transport,
                        shared_from_this(),
                        destination_zone_id,
                        caller_zone_id.as_destination());
                }
            }
        }

#ifdef USE_RPC_TELEMETRY
        if (auto telemetry_service = rpc::get_telemetry_service(); telemetry_service)
        {
            telemetry_service->on_service_proxy_add_ref(
                zone_id_, destination_zone_id, caller_zone_id, object_id, rpc::add_ref_options::build_destination_route);
        }
#endif

        err_code = CO_AWAIT marshaller->add_ref(protocol_version,
            destination_zone_id,
            object_id,
            zone_id_.as_caller_channel(),
            caller_zone_id,
            known_direction_zone(zone_id_),
            rpc::add_ref_options::build_destination_route | rpc::add_ref_options::build_caller_route,
            temp_ref_count,
            empty_in,
            empty_out);
        if (err_code != rpc::error::OK())
        {
            RPC_ERROR("prepare_out_param add_ref failed with code {}", err_code);
            CO_RETURN err_code;
        }

#ifdef USE_RPC_TELEMETRY
        if (auto telemetry_service = rpc::get_telemetry_service(); telemetry_service)
        {
            telemetry_service->on_service_proxy_add_ref(
                zone_id_, destination_zone_id, caller_zone_id, object_id, rpc::add_ref_options::build_caller_route);
        }
#endif

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
            if (caller_zone_id.is_set() && !iface->is_local())
            {
                CO_RETURN CO_AWAIT prepare_out_param(protocol_version, caller_zone_id, iface, descriptor);
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
        bool build_caller_channel = !!(build_out_param_channel & add_ref_options::build_caller_route);
        bool build_dest_channel = !!(build_out_param_channel & add_ref_options::build_destination_route)
                                  || build_out_param_channel == add_ref_options::normal
                                  || build_out_param_channel == add_ref_options::optimistic;

        RPC_ASSERT(!!build_caller_channel || !!build_dest_channel);

        current_service_tracker tracker(this);
#ifdef USE_RPC_TELEMETRY
        if (auto telemetry_service = rpc::get_telemetry_service(); telemetry_service)
        {
            telemetry_service->on_service_add_ref(
                zone_id_, destination_zone_id, object_id, caller_zone_id, build_out_param_channel);
        }
#endif
        if (build_caller_channel)
        {
            if (zone_id_.as_caller() != caller_zone_id)
            {
                auto caller_transport = get_transport(caller_zone_id.as_destination());
                auto error = caller_transport->add_ref(protocol_version,
                    destination_zone_id,
                    object_id,
                    zone_id_.as_caller_channel(),
                    caller_zone_id,
                    known_direction_zone_id,
                    build_out_param_channel & add_ref_options::build_caller_route,
                    reference_count,
                    in_back_channel,
                    out_back_channel);
                if (error != rpc::error::OK())
                {
                    RPC_ERROR("Caller channel add_ref failed with code {}", error);
                    CO_RETURN error;
                }
            }
        }

        if (build_dest_channel)
        {
            if (zone_id_.as_destination() != destination_zone_id)
            {
                auto dest_transport = get_transport(destination_zone_id);
                CO_RETURN dest_transport->add_ref(protocol_version,
                    destination_zone_id,
                    object_id,
                    zone_id_.as_caller_channel(),
                    caller_zone_id,
                    known_direction_zone_id,
                    build_out_param_channel,
                    reference_count,
                    in_back_channel,
                    out_back_channel);
            }

            // service has the implementation
            if (protocol_version < rpc::LOWEST_SUPPORTED_VERSION || protocol_version > rpc::HIGHEST_SUPPORTED_VERSION)
            {
                reference_count = 0;
                RPC_ERROR("Unsupported service version {} in add_ref", protocol_version);
                CO_RETURN rpc::error::INVALID_VERSION();
            }

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
        }
        CO_RETURN rpc::error::OK();
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

    void service::cleanup_service_proxy(const std::shared_ptr<rpc::service_proxy>& other_zone) { }

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

        {
#ifdef USE_RPC_TELEMETRY
            if (auto telemetry_service = rpc::get_telemetry_service(); telemetry_service)
                telemetry_service->on_service_release(zone_id_, destination_zone_id, object_id, caller_zone_id);
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

    std::shared_ptr<rpc::service_proxy> service::get_zone_proxy(
        std::shared_ptr<rpc::service_proxy> caller_sp, // when you know where you are calling to
        caller_channel_zone caller_channel_zone_id,
        caller_zone caller_zone_id, // when you know who is calling you
        destination_zone destination_zone_id,
        bool& new_proxy_added)
    {
        assert(!caller_sp ^ !caller_zone_id.is_set()); // only one must be set
        new_proxy_added = false;
        std::lock_guard g(zone_control);

        // find if we have one
        {
            auto item = other_zones.find(destination_zone_id);
            if (item != other_zones.end())
                return item->second.lock();
        }

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

        // Try to find a direct transport to the destination zone
        if (caller_zone_id.is_set())
        {
            auto item = transports.find(caller_zone_id.as_destination());
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

        // Try to find a direct transport to the destination zone
        if (caller_channel_zone_id.is_set())
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
        }

        // If no direct transport, fall back to transport via caller channel
        {
            auto transport = caller_sp->get_transport();
            if (transport)
            {
                RPC_WARNING("get_zone_proxy: Creating service_proxy with indirect transport! service_zone={}, "
                            "transport_to={}, destination_zone={}",
                    zone_id_.get_val(),
                    transport->get_adjacent_zone_id().get_val(),
                    destination_zone_id.get_val());

                auto proxy = std::make_shared<service_proxy>(transport, destination_zone_id);
                inner_add_zone_proxy(proxy);
                new_proxy_added = true;
                return proxy;
            }
        }

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

    child_service::~child_service() { }
}
