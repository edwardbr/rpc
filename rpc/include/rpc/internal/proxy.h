/*
 *   Copyright (c) 2024 Edward Boggis-Rolfe
 *   All rights reserved.
 */
#pragma once

#include <string>
#include <memory>
#include <unordered_map>
#include <mutex>


namespace rpc
{
    NAMESPACE_INLINE_BEGIN
    
    namespace proxy_factory
	{
		template<class T> void create(const std::shared_ptr<object_proxy>& ob, shared_ptr<T>& inface);
	}
    
    // declared here as object_proxy and service_proxy is not fully defined in the body of proxy_base
    template<class T>
    interface_descriptor proxy_base::proxy_bind_in_param(
        uint64_t protocol_version, const rpc::shared_ptr<T>& iface, std::shared_ptr<rpc::object_stub>& stub)
    {
        if (!iface)
            return interface_descriptor();

        auto operating_service = object_proxy_->get_service_proxy()->get_operating_zone_service();

        // this is to check that an interface is belonging to another zone and not the operating zone
        auto proxy = iface->query_proxy_base();
        if (proxy
            && proxy->get_object_proxy()->get_destination_zone_id() != operating_service->get_zone_id().as_destination())
        {
            return {proxy->get_object_proxy()->get_object_id(), proxy->get_object_proxy()->get_destination_zone_id()};
        }

        // else encapsulate away
        return operating_service->proxy_bind_in_param(protocol_version, iface, stub);
    }

    // do not use directly it is for the interface generator use rpc::create_interface_proxy if you want to get a
    // proxied pointer to a remote implementation
    template<class T>
    int stub_bind_in_param(uint64_t protocol_version,
        rpc::service& serv,
        caller_channel_zone caller_channel_zone_id,
        caller_zone caller_zone_id,
        const interface_descriptor& encap,
        rpc::shared_ptr<T>& iface)
    {
        // if we have a null object id then return a null ptr
        if (encap == interface_descriptor())
        {
            return rpc::error::OK();
        }
        // if it is local to this service then just get the relevant stub
        else if (serv.get_zone_id().as_destination() == encap.destination_zone_id)
        {
            iface = serv.get_local_interface<T>(protocol_version, encap.object_id);
            if (!iface)
                return rpc::error::OBJECT_NOT_FOUND();
            return rpc::error::OK();
        }
        else
        {
            auto zone_id = serv.get_zone_id();
            // get the right  service proxy
            // if the zone is different lookup or clone the right proxy
            bool new_proxy_added = false;
            auto service_proxy = serv.get_zone_proxy(
                caller_channel_zone_id, caller_zone_id, encap.destination_zone_id, zone_id.as_caller(), new_proxy_added);
            if (!service_proxy)
                return rpc::error::OBJECT_NOT_FOUND();

            bool is_new = false;
            std::shared_ptr<object_proxy> op = service_proxy->get_object_proxy(encap.object_id, is_new);
            if (is_new)
            {
                auto ret = service_proxy->sp_add_ref(encap.object_id, {0}, rpc::add_ref_options::normal);
                if (ret == std::numeric_limits<uint64_t>::max())
                    return -1;
                if (!new_proxy_added)
                {
                    service_proxy->add_external_ref();
                }
            }
            auto ret = op->query_interface(iface, false);
            return ret;
        }
    }

    // declared here as object_proxy and service_proxy is not fully defined in the body of proxy_base
    template<class T>
    interface_descriptor proxy_base::stub_bind_out_param(uint64_t protocol_version,
        caller_channel_zone caller_channel_zone_id,
        caller_zone caller_zone_id,
        const rpc::shared_ptr<T>& iface)
    {
        if (!iface)
            return {{0}, {0}};

        auto operating_service = object_proxy_->get_service_proxy()->get_operating_zone_service();

        // this is to check that an interface is belonging to another zone and not the operating zone
        auto proxy = iface->query_proxy_base();
        if (proxy && proxy->get_object_proxy()->get_zone_id() != operating_service->get_zone_id())
        {
            return {proxy->get_object_proxy()->get_object_id(), proxy->get_object_proxy()->get_zone_id()};
        }

        // else encapsulate away
        return operating_service->stub_bind_out_param(protocol_version, caller_channel_zone_id, caller_zone_id, iface);
    }

    // do not use directly it is for the interface generator use rpc::create_interface_proxy if you want to get a
    // proxied pointer to a remote implementation
    template<class T>
    int proxy_bind_out_param(const std::shared_ptr<rpc::service_proxy>& sp,
        const interface_descriptor& encap,
        caller_zone caller_zone_id,
        rpc::shared_ptr<T>& val)
    {
        // if we have a null object id then return a null ptr
        if (!encap.object_id.is_set() || !encap.destination_zone_id.is_set())
            return rpc::error::OK();

        auto serv = sp->get_operating_zone_service();

        // if it is local to this service then just get the relevant stub
        if (encap.destination_zone_id == serv->get_zone_id().as_destination())
        {
            auto ob = serv->get_object(encap.object_id).lock();
            if (!ob)
                return rpc::error::OBJECT_NOT_FOUND();

            auto count = serv->release_local_stub(ob);
            RPC_ASSERT(count);
            if (!count || count == std::numeric_limits<uint64_t>::max())
            {
                return rpc::error::REFERENCE_COUNT_ERROR();
            }

#ifdef RPC_V2
            auto interface_stub = ob->get_interface(T::get_id(rpc::VERSION_2));
            if (!interface_stub)
            {
                if (!interface_stub)
                {
                    return rpc::error::INVALID_INTERFACE_ID();
                }
            }
#endif

            val = rpc::static_pointer_cast<T>(interface_stub->get_castable_interface());
            return rpc::error::OK();
        }

        // get the right  service proxy
        bool new_proxy_added = false;
        auto service_proxy = sp;

        if (sp->get_destination_zone_id() != encap.destination_zone_id)
        {
            // if the zone is different lookup or clone the right proxy
            // the service proxy is where the object came from so it should be used as the new caller channel for this
            // returned object
            auto caller_channel_zone_id = sp->get_destination_zone_id().as_caller_channel();
            service_proxy = serv->get_zone_proxy(caller_channel_zone_id,
                caller_zone_id,
                {encap.destination_zone_id},
                sp->get_zone_id().as_caller(),
                new_proxy_added);
        }

        bool is_new = false;
        std::shared_ptr<object_proxy> op = service_proxy->get_object_proxy(encap.object_id, is_new);
        if (!is_new)
        {
            // as this is an out parameter the callee will be doing an add ref if the object proxy is already found we
            // can do a release
            RPC_ASSERT(!new_proxy_added);
            auto ret = service_proxy->sp_release(encap.object_id);
            if (ret != std::numeric_limits<uint64_t>::max())
            {
                service_proxy->release_external_ref();
            }
        }
        return op->query_interface(val, false);
    }

    template<class T>
    int demarshall_interface_proxy(uint64_t protocol_version,
        const std::shared_ptr<rpc::service_proxy>& sp,
        const interface_descriptor& encap,
        caller_zone caller_zone_id,
        rpc::shared_ptr<T>& val)
    {
        if (protocol_version > rpc::get_version())
            return rpc::error::INCOMPATIBLE_SERVICE();

        // if we have a null object id then return a null ptr
        if (encap.object_id == 0 || encap.destination_zone_id == 0)
            return rpc::error::OK();

        if (encap.destination_zone_id != sp->get_destination_zone_id())
        {
            return rpc::proxy_bind_out_param(sp, encap, caller_zone_id, val);
        }

        auto service_proxy = sp;
        auto serv = service_proxy->get_operating_zone_service();

        // if it is local to this service then just get the relevant stub
        if (serv->get_zone_id().as_destination() == encap.destination_zone_id)
        {
            // if we get here then we need to invent a test for this
            RPC_ASSERT(false);
            return rpc::error::INVALID_DATA();
            // val = serv->get_local_interface<T>(protocol_version, encap.object_id);
            // if(!val)
            //     return rpc::error::OBJECT_NOT_FOUND();
            // return rpc::error::OK();
        }

        // get the right  service proxy
        // bool new_proxy_added = false;
        if (service_proxy->get_destination_zone_id() != encap.destination_zone_id)
        {
            // if we get here then we need to invent a test for this
            RPC_ASSERT(false);
            return rpc::error::INVALID_DATA();

            // //if the zone is different lookup or clone the right proxy
            // service_proxy = serv->get_zone_proxy(
            //     {0},
            //     caller_zone_id,
            //     encap.destination_zone_id,
            //     caller_zone_id,
            //     new_proxy_added);
        }

        if (serv->get_parent_zone_id() == service_proxy->get_destination_zone_id())
            service_proxy->add_external_ref();

        bool is_new = false;
        std::shared_ptr<object_proxy> op = service_proxy->get_object_proxy(encap.object_id, is_new);
        return op->query_interface(val, false);
    }
    
    template<class T> int object_proxy::query_interface(rpc::shared_ptr<T>& iface, bool do_remote_check)
    {
        auto create = [&](std::unordered_map<interface_ordinal, rpc::weak_ptr<proxy_base>>::iterator item) -> int
        {
            rpc::shared_ptr<proxy_base> proxy = item->second.lock();
            if (!proxy)
            {
                // weak pointer needs refreshing
                proxy_factory::create<T>(shared_from_this(), iface);
                item->second = rpc::reinterpret_pointer_cast<proxy_base>(iface);
                return rpc::error::OK();
            }
            iface = rpc::reinterpret_pointer_cast<T>(proxy);
            return rpc::error::OK();
        };

        { // scope for the lock
            std::lock_guard guard(insert_control_);
#ifdef RPC_V2
            if (T::get_id(rpc::VERSION_2) == 0)
            {
                return rpc::error::OK();
            }
            {
                auto item = proxy_map.find(T::get_id(rpc::VERSION_2));
                if (item != proxy_map.end())
                {
                    return create(item);
                }
            }
#endif
            if (!do_remote_check)
            {
                proxy_factory::create<T>(shared_from_this(),iface);
#ifdef RPC_V2
                proxy_map[T::get_id(rpc::VERSION_2)] = rpc::reinterpret_pointer_cast<proxy_base>(iface);
#endif
                return rpc::error::OK();
            }
        }

        // release the lock and then check for casting
        if (do_remote_check)
        {
            // see if object_id can implement interface
            int ret = try_cast(T::get_id);
            if (ret != rpc::error::OK())
            {
                return ret;
            }
        }
        { // another scope for the lock
            std::lock_guard guard(insert_control_);

            // check again...
#ifdef RPC_V2
            {
                auto item = proxy_map.find(T::get_id(rpc::VERSION_2));
                if (item != proxy_map.end())
                {
                    return create(item);
                }
            }
#endif
            proxy_factory::create<T>(shared_from_this(), iface);
#ifdef RPC_V2
            proxy_map[T::get_id(rpc::VERSION_2)] = rpc::reinterpret_pointer_cast<proxy_base>(iface);
#endif
            return rpc::error::OK();
        }
    }
    
    NAMESPACE_INLINE_END
}
