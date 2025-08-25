/*
 *   Copyright (c) 2024 Edward Boggis-Rolfe
 *   All rights reserved.
 */
#include <algorithm>

#include <yas/mem_streams.hpp>
#include <yas/std_types.hpp>

#include "rpc/service.h"
#include "rpc/stub.h"
#include "rpc/proxy.h"
#include "rpc/version.h"
#include "rpc/logger.h"

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

    thread_local caller_zone current_caller_ = {};
    caller_zone service::get_current_caller()
    {
        return current_caller_;
    }

    class current_caller_manager
    {
        caller_zone previous_caller_;

    public:
        current_caller_manager(caller_zone new_caller)
            : previous_caller_(current_caller_)
        {
            current_caller_ = new_caller;
        }
        ~current_caller_manager() { current_caller_ = previous_caller_; }
    };

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

    service::service(const char* name, zone zone_id)
        : zone_id_(zone_id)
        , name_(name)
    {
#ifdef USE_RPC_TELEMETRY
        if (auto telemetry_service = rpc::telemetry_service_manager::get(); telemetry_service)
            telemetry_service->on_service_creation(name, zone_id, destination_zone{0});
#endif
    }

    service::service(const char* name, zone zone_id, child_service_tag)
        : zone_id_(zone_id)
        , name_(name)
    {
        // No telemetry call for child services
    }

    service::~service()
    {
#ifdef USE_RPC_TELEMETRY
        if (auto telemetry_service = rpc::telemetry_service_manager::get(); telemetry_service)
            telemetry_service->on_service_deletion(zone_id_);
#endif

        // For child services, the destructor should only be called when the parent has released it.
        // For parent services, reference counting via child service proxies prevents premature destruction.
        // The cleanup_after_object mechanism in service_proxy handles the proper ordering.

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

    object service::get_object_id(shared_ptr<casting_interface> ptr) const
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
            return ptr->query_proxy_base()->get_object_proxy()->get_object_id();
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
#ifdef USE_RPC_LOGGING
                auto message = std::string("stub zone_id ") + std::to_string(zone_id_) + std::string(", object stub ")
                               + std::to_string(item.first)
                               + std::string(
                                   " has been released but not deregistered in the service suspected unclean shutdown");
                LOG_STR(message.c_str(), message.size());
#endif
            }
            else
            {
#ifdef USE_RPC_LOGGING
                auto message = std::string("stub zone_id ") + std::to_string(zone_id_) + std::string(", object stub ")
                               + std::to_string(item.first)
                               + std::string(" has not been released, there is a strong pointer maintaining a positive "
                                             "reference count suspected unclean shutdown");
                LOG_STR(message.c_str(), message.size());
#endif
            }
            success = false;
        }
        for (auto item : wrapped_object_to_stub)
        {
            auto stub = item.second.lock();
            if (!stub)
            {
#ifdef USE_RPC_LOGGING
                auto message = std::string("wrapped stub zone_id ") + std::to_string(zone_id_)
                               + std::string(", wrapped_object has been released but not deregistered in the service "
                                             "suspected unclean shutdown");
                LOG_STR(message.c_str(), message.size());
#endif
            }
            else
            {
#ifdef USE_RPC_LOGGING
                auto message = std::string("wrapped stub zone_id ") + std::to_string(zone_id_)
                               + std::string(", wrapped_object ") + std::to_string(stub->get_id())
                               + std::string(" has not been deregisted in the service suspected unclean shutdown");
                LOG_STR(message.c_str(), message.size());
#endif
            }
            success = false;
        }

        for (auto item : other_zones)
        {
            auto svcproxy = item.second.lock();
            if (!svcproxy)
            {
#ifdef USE_RPC_LOGGING
                auto message = std::string("service proxy zone_id ") + std::to_string(zone_id_)
                               + std::string(", caller_zone_id ") + std::to_string(item.first.source.id)
                               + std::string(", destination_zone_id ") + std::to_string(item.first.dest.id)
                               + std::string(", has been released but not deregistered in the service");
                LOG_STR(message.c_str(), message.size());
#endif
            }
            else
            {
#ifdef USE_RPC_LOGGING
                auto message = std::string("service proxy zone_id ") + std::to_string(zone_id_)
                               + std::string(", caller_zone_id ") + std::to_string(item.first.source.id)
                               + std::string(", destination_zone_id ")
                               + std::to_string(svcproxy->get_destination_zone_id())
                               + std::string(", destination_channel_zone_id ")
                               + std::to_string(svcproxy->get_destination_channel_zone_id())
                               + std::string(" has not been released in the service suspected unclean shutdown");
                LOG_STR(message.c_str(), message.size());
#endif

                for (auto proxy : svcproxy->get_proxies())
                {
                    auto op = proxy.second.lock();
                    if (op)
                    {
#ifdef USE_RPC_LOGGING
                        auto message = std::string("has object_proxy ") + std::to_string(op->get_object_id());
                        LOG_STR(message.c_str(), message.size());
#endif
                    }
                    else
                    {
#ifdef USE_RPC_LOGGING
                        auto message = std::string("has null object_proxy");
                        LOG_STR(message.c_str(), message.size());
#endif
                    }
                    success = false;
                }
            }
            success = false;
        }
        return success;
    }

    int service::send(uint64_t protocol_version,
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
        std::vector<char>& out_buf_)
    {
        current_service_tracker tracker(this);
        current_caller_manager cc(caller_zone_id);

        if (destination_zone_id != zone_id_.as_destination())
        {
            rpc::shared_ptr<service_proxy> other_zone;
            {
                std::lock_guard g(zone_control);
                auto found = other_zones.find({destination_zone_id, caller_zone_id});
                if (found != other_zones.end())
                {
                    other_zone = found->second.lock();
                }
            }
            if (!other_zone)
            {
#ifdef USE_RPC_LOGGING
                auto debug_msg = "service::send zone: " + std::to_string(zone_id_.id)
                            + " destination_zone=" + std::to_string(destination_zone_id.get_val())
                            + ", caller_zone=" + std::to_string(caller_zone_id.get_val());                LOG_CSTR(debug_msg.c_str());
#endif
                LOG_CSTR("ERROR: Zone not found in send operation");
                // RPC_ASSERT(false);
                return rpc::error::ZONE_NOT_FOUND();
            }
            auto result = other_zone->send(protocol_version,
                encoding,
                tag,
                zone_id_.as_caller_channel(),
                caller_zone_id,
                destination_zone_id,
                object_id,
                interface_id,
                method_id,
                in_size_,
                in_buf_,
                out_buf_);
            return result;
        }
        else
        {
#ifdef RPC_V2
            if (protocol_version == rpc::VERSION_2)
                ;
            else
#endif
            {
                LOG_CSTR("ERROR: Incompatible service version in send");
                return rpc::error::INCOMPATIBLE_SERVICE();
            }
            rpc::weak_ptr<object_stub> weak_stub = get_object(object_id);
            auto stub = weak_stub.lock();
            if (stub == nullptr)
            {
                LOG_CSTR("ERROR: Invalid data - stub is null in send");
                return rpc::error::INVALID_DATA();
            }

            auto ret = stub->call(protocol_version,
                encoding,
                caller_channel_zone_id,
                caller_zone_id,
                interface_id,
                method_id,
                in_size_,
                in_buf_,
                out_buf_);

            return ret;
        }
    }

    void service::clean_up_on_failed_connection(
        const rpc::shared_ptr<service_proxy>& destination_zone, rpc::shared_ptr<rpc::casting_interface> input_interface)
    {
        if (destination_zone && input_interface)
        {
            auto object_id = input_interface->query_proxy_base()->get_object_proxy()->get_object_id();
            auto ret = destination_zone->sp_release(object_id);
            if (ret != std::numeric_limits<uint64_t>::max())
            {
                destination_zone->release_external_ref();
            }
        }
    }

    interface_descriptor service::prepare_remote_input_interface(caller_channel_zone caller_channel_zone_id,
        caller_zone caller_zone_id,
        rpc::proxy_base* base,
        rpc::shared_ptr<service_proxy>& destination_zone)
    {
        auto object_proxy = base->get_object_proxy();
        auto object_service_proxy = object_proxy->get_service_proxy();
        RPC_ASSERT(object_service_proxy->zone_id_ == zone_id_);
        auto destination_zone_id = object_service_proxy->get_destination_zone_id();
        auto destination_channel_zone_id = object_service_proxy->get_destination_channel_zone_id();
        auto object_id = object_proxy->get_object_id();

        RPC_ASSERT(caller_zone_id.is_set());
        RPC_ASSERT(destination_zone_id.is_set());

        uint64_t object_channel = caller_channel_zone_id.is_set() ? caller_channel_zone_id.id : caller_zone_id.id;
        RPC_ASSERT(object_channel);

        uint64_t destination_channel
            = destination_channel_zone_id.is_set() ? destination_channel_zone_id.id : destination_zone_id.id;
        RPC_ASSERT(destination_channel);

        destination_zone = object_service_proxy;
        {
            std::lock_guard g(zone_control);
            auto found = other_zones.find({destination_zone_id, caller_zone_id}); // we dont need to get caller id for this
            if (found != other_zones.end())
            {
                destination_zone = found->second.lock();
                destination_zone->add_external_ref();
            }
            else
            {
                destination_zone = object_service_proxy->clone_for_zone(destination_zone_id, caller_zone_id);
                inner_add_zone_proxy(destination_zone);
            }
        }

#ifdef USE_RPC_TELEMETRY
        if (auto telemetry_service = rpc::telemetry_service_manager::get(); telemetry_service)
        {
            telemetry_service->on_service_proxy_add_ref(zone_id_,
                destination_zone_id,
                destination_channel_zone_id,
                caller_zone_id,
                object_id,
                add_ref_options::build_destination_route);
        }
#endif

        // the fork is here so we need to add ref the destination normally with caller info
        // note the caller_channel_zone_id is is this zones id as the caller came from a route via this node
        destination_zone->add_ref(rpc::get_version(),
            destination_channel_zone_id,
            destination_zone_id,
            object_id,
            zone_id_.as_caller_channel(),
            caller_zone_id,
            rpc::add_ref_options::build_destination_route);

        return {object_id, destination_zone_id};
    }

    interface_descriptor service::prepare_out_param(uint64_t protocol_version,
        caller_channel_zone caller_channel_zone_id,
        caller_zone caller_zone_id,
        rpc::proxy_base* base)
    {
        auto object_proxy = base->get_object_proxy();
        auto object_service_proxy = object_proxy->get_service_proxy();
        RPC_ASSERT(object_service_proxy->zone_id_ == zone_id_);
        auto destination_zone_id = object_service_proxy->get_destination_zone_id();
        auto destination_channel_zone_id = object_service_proxy->get_destination_channel_zone_id();
        auto object_id = object_proxy->get_object_id();

        RPC_ASSERT(caller_zone_id.is_set());
        RPC_ASSERT(destination_zone_id.is_set());

        uint64_t object_channel = caller_channel_zone_id.is_set() ? caller_channel_zone_id.id : caller_zone_id.id;
        RPC_ASSERT(object_channel);

        uint64_t destination_channel
            = destination_channel_zone_id.is_set() ? destination_channel_zone_id.id : destination_zone_id.id;
        RPC_ASSERT(destination_channel);

        if (object_channel == destination_channel)
        {
            // caller and destination are in the same channel let them fork where necessary
            // note the caller_channel_zone_id is 0 as both the caller and the destination are in from the same
            // direction so any other value is wrong Dont external_add_ref the local service proxy as we are return to
            // source no channel is required
#ifdef USE_RPC_TELEMETRY
            if (auto telemetry_service = rpc::telemetry_service_manager::get(); telemetry_service)
            {
                telemetry_service->on_service_proxy_add_ref(zone_id_,
                    destination_zone_id,
                    {0},
                    caller_zone_id,
                    object_id,
                    rpc::add_ref_options::build_caller_route | rpc::add_ref_options::build_destination_route);
            }
#endif
            object_service_proxy->add_ref(protocol_version,
                {0},
                destination_zone_id,
                object_id,
                {0},
                caller_zone_id,
                rpc::add_ref_options::build_caller_route | rpc::add_ref_options::build_destination_route);
        }
        else
        {
            rpc::shared_ptr<service_proxy> destination_zone = object_service_proxy;
            rpc::shared_ptr<service_proxy> caller;
            bool need_add_ref = false;
            std::map<zone_route, rpc::weak_ptr<service_proxy>>::iterator caller_found_iter;
            bool need_caller_from_found = false;
            {
                std::lock_guard g(zone_control);
                {
                    auto found = other_zones.find(
                        {destination_zone_id, caller_zone_id}); // we dont need to get caller id for this
                    if (found != other_zones.end())
                    {
                        destination_zone = found->second.lock();
                        need_add_ref = (destination_zone != nullptr); // Mark that we need add_ref outside lock
                    }
                    else
                    {
                        destination_zone = object_service_proxy->clone_for_zone(destination_zone_id, caller_zone_id);
                        inner_add_zone_proxy(destination_zone);
                    }
                }
                // and the caller with destination info
                {
                    auto found = other_zones.find(
                        {{object_channel}, zone_id_.as_caller()}); // we dont need to get caller id for this
                    if (found == other_zones.end())
                    {
                        // this is working on the premise that the caller_channel_zone_id is not known but
                        // object_channel is
                        RPC_ASSERT(object_channel == caller_channel_zone_id.get_val()
                                   && object_channel != caller_zone_id.get_val());

                        auto alternative = other_zones.lower_bound({caller_zone_id.as_destination(), {0}});
                        if (alternative == other_zones.end() || alternative->first.dest != caller_zone_id.as_destination())
                        {
                            RPC_ASSERT(!!"alternative route to caller zone is not found");
                            return {};
                        }

                        auto alternative_caller_service_proxy = alternative->second.lock();
                        RPC_ASSERT(alternative_caller_service_proxy != nullptr || !!"alternative caller proxy not found");

                        // now make a copy of the original as we need it back
                        caller = alternative_caller_service_proxy->clone_for_zone(
                            {caller_channel_zone_id.get_val()}, zone_id_.as_caller());
                        other_zones[{{caller_channel_zone_id.get_val()}, zone_id_.as_caller()}] = caller;
#ifdef USE_RPC_LOGGING
                        auto debug_msg = "prepare_out_param service zone: " + std::to_string(zone_id_.id)
                                         + " destination_zone=" + std::to_string(caller->destination_zone_id_.get_val())
                                         + ", caller_zone=" + std::to_string(caller->caller_zone_id_.get_val());
                        LOG_CSTR(debug_msg.c_str());
#endif
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
                    LOG_CSTR("ERROR: caller service_proxy was destroyed during lookup");
                    return {}; // Return empty interface descriptor to indicate failure
                }
            }

            // Verify we have a valid caller
            if (!caller)
            {
                LOG_CSTR("ERROR: Failed to obtain valid caller service_proxy");
                return {};
            }

            // Call add_external_ref() outside the mutex to prevent race with service_proxy destruction
            if (need_add_ref && destination_zone)
            {
                destination_zone->add_external_ref();
            }

#ifdef USE_RPC_TELEMETRY
            if (auto telemetry_service = rpc::telemetry_service_manager::get(); telemetry_service)
            {
                telemetry_service->on_service_proxy_add_ref(
                    zone_id_, destination_zone_id, {0}, caller_zone_id, object_id, rpc::add_ref_options::build_destination_route);
            }
#endif

            // the fork is here so we need to add ref the destination normally with caller info
            // note the caller_channel_zone_id is is this zones id as the caller came from a route via this node
            if (destination_zone)
            {
                destination_zone->add_ref(protocol_version,
                    {0},
                    destination_zone_id,
                    object_id,
                    zone_id_.as_caller_channel(),
                    caller_zone_id,
                    rpc::add_ref_options::build_destination_route);
            }
            else
            {
                LOG_CSTR("ERROR: destination_zone service_proxy was destroyed during operation");
                return {};
            }

#ifdef USE_RPC_TELEMETRY
            if (auto telemetry_service = rpc::telemetry_service_manager::get(); telemetry_service)
            {
                telemetry_service->on_service_proxy_add_ref(zone_id_,
                    destination_zone_id,
                    zone_id_.as_destination_channel(),
                    caller_zone_id,
                    object_id,
                    rpc::add_ref_options::build_caller_route);
            }
#endif

            // note the caller_channel_zone_id is 0 as the caller came from this route
            caller->add_ref(protocol_version,
                zone_id_.as_destination_channel(),
                destination_zone_id,
                object_id,
                {0},
                caller_zone_id,
                rpc::add_ref_options::build_caller_route);
        }

        return {object_id, destination_zone_id};
    }

    // this is a key function that returns an interface descriptor
    // for wrapping an implementation to a local object inside a stub where needed
    // or if the interface is a proxy to add ref it
    interface_descriptor service::get_proxy_stub_descriptor(uint64_t protocol_version,
        caller_channel_zone caller_channel_zone_id,
        caller_zone caller_zone_id,
        rpc::casting_interface* iface,
        std::function<rpc::shared_ptr<i_interface_stub>(rpc::shared_ptr<object_stub>)> fn,
        bool outcall,
        rpc::shared_ptr<object_stub>& stub)
    {
        if (outcall)
        {
            proxy_base* proxy_base = nullptr;
            if (caller_channel_zone_id.is_set() || caller_zone_id.is_set())
            {
                proxy_base = iface->query_proxy_base();
            }
            if (proxy_base)
            {
                return prepare_out_param(protocol_version, caller_channel_zone_id, caller_zone_id, proxy_base);
            }
        }

        // needed by the out call
        rpc::shared_ptr<service_proxy> caller;

        auto* pointer = iface->get_address();
        {
            // find the stub by its address
            {
                std::lock_guard g(stub_control);
                auto item = wrapped_object_to_stub.find(pointer);
                if (item != wrapped_object_to_stub.end())
                {
                    stub = item->second.lock();
                    // Don't mask the race condition - if stub is null here, we have a serious problem
                    RPC_ASSERT(stub != nullptr);
                    stub->add_ref();
                }
                else
                {
                    // else create a stub
                    auto id = generate_new_object_id();
                    stub = rpc::make_shared<object_stub>(id, *this, pointer);
                    rpc::shared_ptr<i_interface_stub> interface_stub = fn(stub);
                    stub->add_interface(interface_stub);
                    wrapped_object_to_stub[pointer] = stub;
                    stubs[id] = stub;
                    stub->on_added_to_zone(stub);
                    stub->add_ref();
                }
            }

            if (outcall)
            {
                std::lock_guard g(zone_control);
                uint64_t object_channel = caller_channel_zone_id.is_set() ? caller_channel_zone_id.id : caller_zone_id.id;
                RPC_ASSERT(object_channel);
                // and the caller with destination info
                auto found = other_zones.find(
                    {{object_channel}, zone_id_.as_caller()}); // we dont need to get caller id for this
                if (found != other_zones.end())
                {
                    caller = found->second.lock();
                }
                else
                {
                    // unexpected code path
                    RPC_ASSERT(false);
                    // caller = get_parent();
                }
                RPC_ASSERT(caller);
            }
        }
        if (outcall)
        {
#ifdef USE_RPC_TELEMETRY
            if (auto telemetry_service = rpc::telemetry_service_manager::get(); telemetry_service)
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
            caller->add_ref(protocol_version,
                {0},
                zone_id_.as_destination(),
                stub->get_id(),
                {0},
                caller_zone_id,
                rpc::add_ref_options::build_caller_route);
        }
        return {stub->get_id(), zone_id_.as_destination()};
    }

    rpc::weak_ptr<object_stub> service::get_object(object object_id) const
    {
        std::lock_guard l(stub_control);
        auto item = stubs.find(object_id);
        if (item == stubs.end())
        {
            // we need a test if we get here
            RPC_ASSERT(false);
            return rpc::weak_ptr<object_stub>();
        }

        return item->second;
    }
    int service::try_cast(
        uint64_t protocol_version, destination_zone destination_zone_id, object object_id, interface_ordinal interface_id)
    {
        current_service_tracker tracker(this);
        if (destination_zone_id != zone_id_.as_destination())
        {
            rpc::shared_ptr<service_proxy> other_zone;
            {
                std::lock_guard g(zone_control);
                auto found = other_zones.lower_bound({destination_zone_id, {0}});
                if (found != other_zones.end() && found->first.dest == destination_zone_id)
                {
                    other_zone = found->second.lock();
                }
            }
            if (!other_zone)
            {
                RPC_ASSERT(false);
                LOG_CSTR("ERROR: Zone not found in try_cast operation");
                return rpc::error::ZONE_NOT_FOUND();
            }
#ifdef USE_RPC_TELEMETRY
            if (auto telemetry_service = rpc::telemetry_service_manager::get(); telemetry_service)
            {
                telemetry_service->on_service_try_cast(zone_id_, destination_zone_id, {0}, object_id, interface_id);
            }
#endif
            other_zone->add_external_ref();
            auto result = other_zone->try_cast(protocol_version, destination_zone_id, object_id, interface_id);
            // Balance external ref taken when building route; then drop unused proxies
            other_zone->release_external_ref();
            return result;
        }
        else
        {
#ifdef RPC_V2
            if (protocol_version == rpc::VERSION_2)
                ;
            else
#endif
            {
                LOG_CSTR("ERROR: Incompatible service version in try_cast");
                return rpc::error::INCOMPATIBLE_SERVICE();
            }
            rpc::weak_ptr<object_stub> weak_stub = get_object(object_id);
            auto stub = weak_stub.lock();
            if (!stub)
            {
                LOG_CSTR("ERROR: Invalid data - stub is null in try_cast");
                return error::INVALID_DATA();
            }
            return stub->try_cast(interface_id);
        }
    }

    uint64_t service::add_ref(uint64_t protocol_version,
        destination_channel_zone destination_channel_zone_id,
        destination_zone destination_zone_id,
        object object_id,
        caller_channel_zone caller_channel_zone_id,
        caller_zone caller_zone_id,
        add_ref_options build_out_param_channel)
    {
        current_service_tracker tracker(this);
        current_caller_manager cc(caller_zone_id);

#ifdef USE_RPC_TELEMETRY
        if (auto telemetry_service = rpc::telemetry_service_manager::get(); telemetry_service)
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
        if (destination_channel_zone_id != zone_id_.as_destination_channel() && destination_channel_zone_id.id != 0)
            dest_channel = destination_channel_zone_id.get_val();
        auto caller_channel = caller_channel_zone_id.is_set() ? caller_channel_zone_id.id : caller_zone_id.id;

        if (destination_zone_id != zone_id_.as_destination())
        {
            auto build_channel = !!(build_out_param_channel & add_ref_options::build_destination_route)
                                 || !!(build_out_param_channel & add_ref_options::build_caller_route);
            if (dest_channel == caller_channel && build_channel)
            {
                // we are here as we are passing the buck to the zone that knows to either splits or terminates this
                // zone has no refcount issues to deal with
                rpc::shared_ptr<rpc::service_proxy> destination;
                do
                {
                    std::lock_guard g(zone_control);
                    auto found = other_zones.find({destination_zone_id, caller_zone_id});
                    if (found != other_zones.end())
                    {
                        // Previously untested section - now logging to verify if it's called
                        LOG_CSTR("*** EDGE CASE PATH HIT AT LINE 792 ***");
                        LOG_CSTR("dest_channel == caller_channel && build_channel condition met!");
                        LOG_CSTR("*** END EDGE CASE PATH 792 ***");
                        RPC_ASSERT(false);
                        // destination = found->second.lock();
                        // destination->add_external_ref();//update the local ref count the object refcount is done
                        // further down the stack
                        break;
                    }

                    found = other_zones.lower_bound({{dest_channel}, {0}});
                    if (found != other_zones.end() && found->first.dest.get_val() == dest_channel)
                    {
                        destination = found->second.lock();
                        break;
                    }

                    RPC_ASSERT(get_parent() != nullptr);
                    destination = get_parent();
                    RPC_ASSERT(false);

                } while (false);

#ifdef USE_RPC_TELEMETRY
                if (auto telemetry_service = rpc::telemetry_service_manager::get(); telemetry_service)
                {
                    telemetry_service->on_service_proxy_add_ref(
                        zone_id_, destination_zone_id, {0}, caller_zone_id, object_id, build_out_param_channel);
                }
#endif
                return destination->add_ref(
                    protocol_version, {0}, destination_zone_id, object_id, {0}, caller_zone_id, build_out_param_channel);
            }
            else if (build_channel)
            {
                // we are here as this zone needs to send the destination addref and caller addref to different zones
                rpc::shared_ptr<rpc::service_proxy> destination;
                rpc::shared_ptr<rpc::service_proxy> caller;
                {
                    bool has_called_inner_add_zone_proxy = false;
                    {
                        std::lock_guard g(zone_control);

                        auto found = other_zones.find({destination_zone_id, caller_zone_id});
                        if (found != other_zones.end())
                        {
                            destination = found->second.lock();
                            // Move add_external_ref() outside the lock to prevent TOCTOU race
                        }
                        else
                        {
                            found = other_zones.lower_bound({{dest_channel}, {0}});
                            if (found != other_zones.end() && found->first.dest.get_val() == dest_channel)
                            {
                                auto tmp = found->second.lock();
                                RPC_ASSERT(tmp != nullptr);
                                destination = tmp->clone_for_zone(destination_zone_id, caller_zone_id);
                            }
                            else
                            {
                                // get the parent to route it
                                RPC_ASSERT(get_parent() != nullptr);
                                destination = get_parent()->clone_for_zone(destination_zone_id, caller_zone_id);
                            }
                            inner_add_zone_proxy(destination);
                            has_called_inner_add_zone_proxy = true;
                        }

                        if (caller_zone_id == zone_id_.as_caller())
                            build_out_param_channel
                                = build_out_param_channel ^ add_ref_options::build_caller_route; // strip out this bit

                        if (!!(build_out_param_channel & add_ref_options::build_caller_route))
                        {
                            found = other_zones.lower_bound({{caller_channel}, {0}});
                            if (found != other_zones.end() && found->first.dest.get_val() == caller_channel)
                            {
                                caller = found->second.lock();
                            }
                            else
                            {
                                // PREVIOUSLY UNTESTED PATH!!! - now logging to verify if it's called
                                LOG_CSTR("*** EDGE CASE PATH HIT AT LINE 870+ ***");
                                LOG_CSTR("Unknown zone reference path - zone doesn't know of caller existence!");
                                LOG_CSTR("Falling back to get_parent() - this assumes parent knows about the zone");
                                LOG_CSTR("*** POTENTIAL ISSUE: parent may not know about zones in other branches ***");

                                // It has been worked out that this happens when a reference to an zone is passed to a
                                // zone that does not know of its existence.
                                // SOLUTION:
                                // Create a temporary "snail trail" of service proxies if not present from the caller to
                                // the called this would result in the i_marshaller::send method having an additional
                                // list of zones that are referred to in any parameter that is passing an interface.
                                // This list needs to be not encrypted along the chain of services in a function call so
                                // that they can maintain that snail trail. All service proxies whether already present
                                // or ephemeral will need to be protected with a shared pointer for the lifetime of the
                                // call. This will require a change to the code generator to populate the list of zones
                                // in the send method for the receiving service to process.
                                // TEMPORARY FIX:
                                // This fix below assumes that the bottom most parent knows about the zone in question.
                                // However one branch may have a zone with a child that the bottom most node does not
                                // know about so this will break.  With the proposed snail trail fix this logic branch
                                // should assert false as it should then be impossible to get to this position.
                                caller = get_parent();

                                if (caller)
                                {
                                    LOG_CSTR("get_parent() returned: valid caller");
                                }
                                else
                                {
                                    LOG_CSTR("get_parent() returned: nullptr - THIS IS A PROBLEM!");
                                }
                                LOG_CSTR("*** END EDGE CASE PATH 870+ ***");
                            }

                            RPC_ASSERT(caller);
                        }
                    }

                    // Call add_external_ref() outside mutex to prevent TOCTOU race
                    if (destination && !has_called_inner_add_zone_proxy)
                    {
                        destination->add_external_ref();
                    }

                    do
                    {
                        // this more fiddly check is to route calls to a parent node that knows more about this
                        if (destination && caller
                            && build_out_param_channel
                                   == (add_ref_options::build_caller_route | add_ref_options::build_destination_route))
                        {
                            auto dc = destination->get_destination_channel_zone_id().is_set()
                                          ? destination->get_destination_channel_zone_id().get_val()
                                          : destination->get_destination_zone_id().get_val();
                            auto cc = caller->get_destination_channel_zone_id().is_set()
                                          ? caller->get_destination_channel_zone_id().get_val()
                                          : caller->get_destination_zone_id().get_val();
                            if (dc == cc)
                            {
#ifdef USE_RPC_TELEMETRY
                                if (auto telemetry_service = rpc::telemetry_service_manager::get(); telemetry_service)
                                {
                                    telemetry_service->on_service_proxy_add_ref(
                                        zone_id_, destination_zone_id, {0}, caller_zone_id, object_id, build_out_param_channel);
                                }
#endif

                                auto ret = destination->add_ref(protocol_version,
                                    {0},
                                    destination_zone_id,
                                    object_id,
                                    {0},
                                    caller_zone_id,
                                    build_out_param_channel);
                                destination->release_external_ref(); // perhaps this could be optimised
                                if (ret == std::numeric_limits<uint64_t>::max())
                                {
                                    return ret;
                                }
                                break;
                            }
                        }

                        // then call the add ref to the destination
                        if (!!(build_out_param_channel & add_ref_options::build_destination_route))
                        {
#ifdef USE_RPC_TELEMETRY
                            if (auto telemetry_service = rpc::telemetry_service_manager::get(); telemetry_service)
                            {
                                telemetry_service->on_service_proxy_add_ref(zone_id_,
                                    destination_zone_id,
                                    {0},
                                    caller_zone_id,
                                    object_id,
                                    add_ref_options::build_destination_route);
                            }
#endif
                            destination->add_ref(protocol_version,
                                {0},
                                destination_zone_id,
                                object_id,
                                zone_id_.as_caller_channel(),
                                caller_zone_id,
                                add_ref_options::build_destination_route);
                        }
                        // back fill the ref count to the caller
                        if (!!(build_out_param_channel & add_ref_options::build_caller_route))
                        {
#ifdef USE_RPC_TELEMETRY
                            if (auto telemetry_service = rpc::telemetry_service_manager::get(); telemetry_service)
                            {
                                telemetry_service->on_service_proxy_add_ref(caller->zone_id_,
                                    destination_zone_id,
                                    zone_id_.as_destination_channel(),
                                    caller_zone_id,
                                    object_id,
                                    add_ref_options::build_caller_route);
                            }
#endif

                            caller->add_ref(protocol_version,
                                zone_id_.as_destination_channel(),
                                destination_zone_id,
                                object_id,
                                caller_channel_zone_id,
                                caller_zone_id,
                                add_ref_options::build_caller_route);
                        }
                    } while (false);
                }

                return 1;
            }
            else
            {
                rpc::shared_ptr<service_proxy> other_zone;
                { // brackets here as we are using a lock guard
                    std::lock_guard g(zone_control);
                    auto found = other_zones.find({destination_zone_id, caller_zone_id});
                    if (found != other_zones.end())
                    {
                        other_zone = found->second.lock();
                        if (other_zone)
                        {
                            other_zone->add_external_ref();
                        }
                    }

                    if (!other_zone)
                    {
                        auto found = other_zones.lower_bound({destination_zone_id, {0}});
                        if (found != other_zones.end() && found->first.dest == destination_zone_id)
                        {
                            auto tmp = found->second.lock();
                            RPC_ASSERT(tmp != nullptr);
                            other_zone = tmp->clone_for_zone(destination_zone_id, caller_zone_id);
                        }
                        else
                        {
                            auto parent = get_parent();
                            RPC_ASSERT(parent);
                            other_zone = parent->clone_for_zone(destination_zone_id, caller_zone_id);
                        }
                        inner_add_zone_proxy(other_zone);
                    }
                }

#ifdef USE_RPC_TELEMETRY
                if (auto telemetry_service = rpc::telemetry_service_manager::get(); telemetry_service)
                {
                    telemetry_service->on_service_proxy_add_ref(
                        zone_id_, destination_zone_id, {0}, caller_zone_id, object_id, build_out_param_channel);
                }
#endif

                return other_zone->add_ref(protocol_version,
                    {0},
                    destination_zone_id,
                    object_id,
                    caller_channel_zone_id,
                    caller_zone_id,
                    build_out_param_channel);
            }
        }
        else
        {
#ifdef RPC_V2
            if (protocol_version == rpc::VERSION_2)
                ;
            else
#endif
            {
                return std::numeric_limits<uint64_t>::max();
            }

            // find the caller
            if (zone_id_.as_caller() != caller_zone_id && !!(build_out_param_channel & add_ref_options::build_caller_route))
            {
                rpc::shared_ptr<service_proxy> caller;
                {
                    std::lock_guard g(zone_control);
                    // we swap the parameter types as this is from perspective of the caller and not the proxy that
                    // called this function
                    auto found = other_zones.find({caller_zone_id.as_destination(), destination_zone_id.as_caller()});
                    if (found != other_zones.end())
                    {
                        caller = found->second.lock();
                    }
                    else
                    {
                        // untested
                        RPC_ASSERT(false);
                        // caller = get_parent();
                    }
                    RPC_ASSERT(caller);
                }
#ifdef USE_RPC_TELEMETRY
                if (auto telemetry_service = rpc::telemetry_service_manager::get(); telemetry_service)
                {
                    telemetry_service->on_service_proxy_add_ref(
                        zone_id_, destination_zone_id, {0}, caller_zone_id, object_id, add_ref_options::build_caller_route);
                }
#endif
                caller->add_ref(
                    protocol_version, {0}, destination_zone_id, object_id, {}, caller_zone_id, add_ref_options::build_caller_route);
            }
            if (object_id == dummy_object_id)
            {
                return 0;
            }

            rpc::weak_ptr<object_stub> weak_stub = get_object(object_id);
            auto stub = weak_stub.lock();
            if (!stub)
            {
                RPC_ASSERT(false);
                return std::numeric_limits<uint64_t>::max();
            }
            return stub->add_ref();
        }
    }

    uint64_t service::release_local_stub(const rpc::shared_ptr<rpc::object_stub>& stub)
    {
        std::lock_guard l(stub_control);
        uint64_t count = stub->release();
        if (!count)
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

    uint64_t service::release(
        uint64_t protocol_version, destination_zone destination_zone_id, object object_id, caller_zone caller_zone_id)
    {
        current_service_tracker tracker(this);
        current_caller_manager cc(caller_zone_id);

        if (destination_zone_id != zone_id_.as_destination())
        {
            rpc::shared_ptr<service_proxy> other_zone;
            {
                std::lock_guard g(zone_control);
                auto found = other_zones.find({destination_zone_id, caller_zone_id});
                if (found != other_zones.end())
                {
                    other_zone = found->second.lock();
                }
            }
            if (!other_zone)
            {
                RPC_ASSERT(false);
                return std::numeric_limits<uint64_t>::max();
            }
#ifdef USE_RPC_TELEMETRY
            if (auto telemetry_service = rpc::telemetry_service_manager::get(); telemetry_service)
                telemetry_service->on_service_release(
                    zone_id_, other_zone->get_destination_channel_zone_id(), destination_zone_id, object_id, caller_zone_id);
#endif
            auto ret = other_zone->sp_release(object_id);
            if (ret != std::numeric_limits<uint64_t>::max())
            {
                bool should_cleanup = !other_zone->release_external_ref();
                if (should_cleanup)
                {
#ifdef USE_RPC_LOGGING
                    auto debug_msg = "service::release cleaning up unused routing service_proxy destination_zone="
                                     + std::to_string(destination_zone_id.get_val())
                                     + ", caller_zone=" + std::to_string(caller_zone_id.get_val());
                    LOG_CSTR(debug_msg.c_str());
#endif

                    std::lock_guard g(zone_control);

                    // Routing service_proxies should NEVER have object_proxies - this is a bug
                    if (!other_zone->proxies_.empty())
                    {
#ifdef USE_RPC_LOGGING
                        auto bug_msg = "BUG: Routing service_proxy (destination_zone="
                                       + std::to_string(destination_zone_id.get_val())
                                       + " != zone=" + std::to_string(zone_id_.get_val()) + ") has "
                                       + std::to_string(other_zone->proxies_.size())
                                       + " object_proxies - routing proxies should never host objects";
                        LOG_CSTR(bug_msg.c_str());

                        // Log details of the problematic object_proxies for debugging
                        for (const auto& proxy_pair : other_zone->proxies_)
                        {
                            auto object_proxy_ptr = proxy_pair.second.lock();
                            auto detail_msg = "  BUG: object_proxy object_id=" + std::to_string(proxy_pair.first.get_val())
                                              + " in routing service_proxy, alive=" + (object_proxy_ptr ? "yes" : "no");
                            LOG_CSTR(detail_msg.c_str());
                        }
#endif

                        // This should not happen - routing service_proxies should not have object_proxies
                        RPC_ASSERT(other_zone->proxies_.empty() && "Routing service_proxy should not have object_proxies");
                    }

                    other_zone->is_responsible_for_cleaning_up_service_ = false;
                    auto found_again = other_zones.find({destination_zone_id, caller_zone_id});
                    if (found_again != other_zones.end())
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
                        RPC_ASSERT("dying proxy not found");
                    }
                }
            }
            return ret;
        }
        else
        {
#ifdef USE_RPC_TELEMETRY
            if (auto telemetry_service = rpc::telemetry_service_manager::get(); telemetry_service)
                telemetry_service->on_service_release(zone_id_, {0}, destination_zone_id, object_id, caller_zone_id);
#endif

#ifdef RPC_V2
            if (protocol_version == rpc::VERSION_2)
                ;
            else
#endif
            {
                return std::numeric_limits<uint64_t>::max();
            }

            bool reset_stub = false;
            rpc::shared_ptr<rpc::object_stub> stub;
            uint64_t count = 0;
            // these scope brackets are needed as otherwise there will be a recursive lock on a mutex in rare cases when
            // the stub is reset
            {
                {
                    // a scoped lock
                    std::lock_guard l(stub_control);
                    auto item = stubs.find(object_id);
                    if (item == stubs.end())
                    {
                        RPC_ASSERT(false);
                        return std::numeric_limits<uint64_t>::max();
                    }

                    stub = item->second.lock();
                }

                if (!stub)
                {
                    RPC_ASSERT(false);
                    return std::numeric_limits<uint64_t>::max();
                }
                // this guy needs to live outside of the mutex or deadlocks may happen
                count = stub->release();
                if (!count)
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
                                return std::numeric_limits<uint64_t>::max();
                            }
                        }
                    }
                    stub->reset();
                    reset_stub = true;
                }
            }

            if (reset_stub)
                stub->reset();

            return count;
        }
    }

    void service::inner_add_zone_proxy(const rpc::shared_ptr<service_proxy>& service_proxy)
    {
        // this is for internal use only has no lock
        service_proxy->add_external_ref();
        auto destination_zone_id = service_proxy->get_destination_zone_id();
        auto caller_zone_id = service_proxy->get_caller_zone_id();
        RPC_ASSERT(destination_zone_id != zone_id_.as_destination());
        RPC_ASSERT(other_zones.find({destination_zone_id, caller_zone_id}) == other_zones.end());
        other_zones[{destination_zone_id, caller_zone_id}] = service_proxy;
#ifdef USE_RPC_LOGGING
        auto debug_msg = "inner_add_zone_proxy service zone: " + std::to_string(zone_id_.id)
                         + " destination_zone=" + std::to_string(service_proxy->destination_zone_id_.get_val())
                         + ", caller_zone=" + std::to_string(service_proxy->caller_zone_id_.get_val());
        LOG_CSTR(debug_msg.c_str());
#endif
    }

    void service::add_zone_proxy(const rpc::shared_ptr<service_proxy>& service_proxy)
    {
        RPC_ASSERT(service_proxy->get_destination_zone_id() != zone_id_.as_destination());
        std::lock_guard g(zone_control);
        inner_add_zone_proxy(service_proxy);
    }

    rpc::shared_ptr<service_proxy> service::get_zone_proxy(caller_channel_zone caller_channel_zone_id,
        caller_zone caller_zone_id,
        destination_zone destination_zone_id,
        caller_zone new_caller_zone_id,
        bool& new_proxy_added)
    {
        new_proxy_added = false;
        std::lock_guard g(zone_control);

        // find if we have one
        auto item = other_zones.find({destination_zone_id, new_caller_zone_id});
        if (item != other_zones.end())
            return item->second.lock();

        item = other_zones.lower_bound({destination_zone_id, {0}});

        if (item != other_zones.end() && item->first.dest != destination_zone_id)
            item = other_zones.end();

        // if not we can make one from the proxy of the calling channel zone
        // this zone knows nothing about the destination zone however the caller channel zone will know how to connect
        // to it
        if (item == other_zones.end() && caller_channel_zone_id.is_set())
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

        auto proxy = calling_proxy->clone_for_zone(destination_zone_id, new_caller_zone_id);
        inner_add_zone_proxy(proxy);
        new_proxy_added = true;
        return proxy;
    }

    void service::remove_zone_proxy(destination_zone destination_zone_id, caller_zone caller_zone_id)
    {
        {
            std::lock_guard g(zone_control);
            auto item = other_zones.find({destination_zone_id, caller_zone_id});
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

    void service::remove_zone_proxy_if_not_used(destination_zone destination_zone_id, caller_zone caller_zone_id)
    {
        {
            std::lock_guard g(zone_control);
            auto item = other_zones.find({destination_zone_id, caller_zone_id});
            if (item == other_zones.end())
            {
                RPC_ASSERT(false);
            }
            else
            {
                auto sp = item->second.lock();
                if (!sp || sp->is_unused())
                {
                    other_zones.erase(item);
                }
            }
        }
    }

    int service::create_interface_stub(rpc::interface_ordinal interface_id,
        std::function<interface_ordinal(uint8_t)> interface_getter,
        const rpc::shared_ptr<rpc::i_interface_stub>& original,
        rpc::shared_ptr<rpc::i_interface_stub>& new_stub)
    {
        // an identity check, send back the same pointer
        if (
#ifdef RPC_V2
            interface_getter(rpc::VERSION_2)
            == interface_id
#endif
#if !defined(RPC_V2)
            false
#endif
        )
        {
            new_stub = rpc::static_pointer_cast<rpc::i_interface_stub>(original);
            return rpc::error::OK();
        }

        auto it = stub_factories.find(interface_id);
        if (it == stub_factories.end())
        {
            LOG_CSTR("stub factory does not have a record of this interface this not an error in the rpc stack");
            return rpc::error::INVALID_CAST();
        }

        new_stub = (*it->second)(original);
        if (!new_stub)
        {
            LOG_CSTR("Object does not support the interface this not an error in the rpc stack");
            return rpc::error::INVALID_CAST();
        }
        // note a nullptr return value is a valid value, it indicates that this object does not implement that interface
        return rpc::error::OK();
    }

    // note this function is not thread safe!  Use it before using the service class for normal operation
    void service::add_interface_stub_factory(std::function<interface_ordinal(uint8_t)> id_getter,
        std::shared_ptr<std::function<rpc::shared_ptr<rpc::i_interface_stub>(const rpc::shared_ptr<rpc::i_interface_stub>&)>> factory)
    {
#ifdef RPC_V2
        auto interface_id = id_getter(rpc::VERSION_2);
        auto it = stub_factories.find({interface_id});
        if (it != stub_factories.end())
        {
            LOG_CSTR("ERROR: Invalid data - add_interface_stub_factory failed");
            rpc::error::INVALID_DATA();
        }
        stub_factories[{interface_id}] = factory;
#endif
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
    
    child_service::child_service(const char* name, zone zone_id, destination_zone parent_zone_id)
            : service(name, zone_id, child_service_tag{})
            , parent_zone_id_(parent_zone_id)
        {
#ifdef USE_RPC_TELEMETRY
            if (auto telemetry_service = rpc::telemetry_service_manager::get(); telemetry_service)
                telemetry_service->on_service_creation(name, zone_id, parent_zone_id);
#endif            
        }    

    child_service::~child_service()
    {
        if (parent_service_proxy_)
        {
            RPC_ASSERT(parent_service_proxy_->get_caller_zone_id() == zone_id_.as_caller());
            RPC_ASSERT(parent_service_proxy_->get_destination_channel_zone_id().get_val() == 0);
            // remove_zone_proxy is not callable by the proxy as the service is dying and locking on the weak pointer is
            // now no longer possible
            other_zones.erase({parent_service_proxy_->get_destination_zone_id(), zone_id_.as_caller()});
            parent_service_proxy_->set_parent_channel(false);
            parent_service_proxy_->release_external_ref();
            parent_service_proxy_ = nullptr;
        }
    }

    bool child_service::set_parent_proxy(const rpc::shared_ptr<rpc::service_proxy>& parent_service_proxy)
    {
        std::lock_guard l(parent_protect);
        if (parent_service_proxy_)
        {
            RPC_ASSERT(false);
            return false;
        }
        parent_service_proxy_ = parent_service_proxy;
        return true;
    }
}
