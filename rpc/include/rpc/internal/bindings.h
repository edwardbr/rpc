/*
 *   Copyright (c) 2025 Edward Boggis-Rolfe
 *   All rights reserved.
 */
#pragma once

#include <rpc/internal/service.h>
#include <rpc/internal/service_proxy.h>

namespace rpc
{
    template<class T>
    CORO_TASK(interface_descriptor)
    proxy_bind_in_param(std::shared_ptr<rpc::object_proxy> object_p,
        uint64_t protocol_version,
        const rpc::shared_ptr<T>& iface,
        std::shared_ptr<rpc::object_stub>& stub)
    {
        if (!iface)
            CO_RETURN interface_descriptor();

        RPC_ASSERT(object_p);
        if (!object_p)
            CO_RETURN interface_descriptor();
        auto operating_service = object_p->get_service_proxy()->get_operating_zone_service();

        // this is to check that an interface is belonging to another zone and not the operating zone
        if (!iface->is_local()
            && casting_interface::get_destination_zone(*iface) != operating_service->get_zone_id().as_destination())
        {
            CO_RETURN{casting_interface::get_object_id(*iface), casting_interface::get_destination_zone(*iface)};
        }

        // else encapsulate away
        CO_RETURN CO_AWAIT operating_service->bind_in_proxy(protocol_version, iface, stub);
    }
    
    template<class T>
    CORO_TASK(interface_descriptor)
    stub_bind_out_param(rpc::service& zone,
        uint64_t protocol_version,
        caller_channel_zone caller_channel_zone_id,
        caller_zone caller_zone_id,
        const shared_ptr<T>& iface)
    {
        CO_RETURN CO_AWAIT zone.bind_out_stub(protocol_version, caller_channel_zone_id, caller_zone_id, iface);
    }

    // do not use directly it is for the interface generator use rpc::create_interface_proxy if you want to get a proxied pointer to a remote implementation
    template<class T>
    CORO_TASK(int)
    stub_bind_in_param(uint64_t protocol_version,
        rpc::service& serv,
        caller_channel_zone caller_channel_zone_id,
        caller_zone caller_zone_id,
        const rpc::interface_descriptor& encap,
        rpc::shared_ptr<T>& iface)
    {
        // if we have a null object id then return a null ptr
        if (encap == rpc::interface_descriptor())
        {
            CO_RETURN rpc::error::OK();
        }
        // if it is local to this service then just get the relevant stub
        else if (serv.get_zone_id().as_destination() == encap.destination_zone_id)
        {
            iface = rpc::static_pointer_cast<T>(serv.get_castable_interface(encap.object_id, T::get_id(protocol_version)));
            if (!iface)
            {
                RPC_ERROR("Object not found in local interface lookup");
                CO_RETURN rpc::error::OBJECT_NOT_FOUND();
            }
            CO_RETURN rpc::error::OK();
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
            {
                RPC_ERROR("Object not found - service proxy is null");
                CO_RETURN rpc::error::OBJECT_NOT_FOUND();
            }

            std::shared_ptr<rpc::object_proxy> op = CO_AWAIT service_proxy->get_or_create_object_proxy(encap.object_id,
                service_proxy::object_proxy_creation_rule::ADD_REF_IF_NEW,
                new_proxy_added,
                caller_zone_id.as_known_direction_zone(),
                false);
            RPC_ASSERT(op != nullptr);
            if (!op)
            {
                RPC_ERROR("Object not found - object proxy is null");
                CO_RETURN rpc::error::OBJECT_NOT_FOUND();
            }
            auto ret = CO_AWAIT op->query_interface(iface, false);
            CO_RETURN ret;
        }
    }

    // do not use directly it is for the interface generator use rpc::create_interface_proxy if you want to get a proxied pointer to a remote implementation
    template<class T>
    CORO_TASK(int)
    proxy_bind_out_param(const std::shared_ptr<rpc::service_proxy>& sp,
        const rpc::interface_descriptor& encap,
        caller_zone caller_zone_id,
        rpc::shared_ptr<T>& val)
    {
        // if we have a null object id then return a null ptr
        if (!encap.object_id.is_set() || !encap.destination_zone_id.is_set())
            CO_RETURN rpc::error::OK();

        auto serv = sp->get_operating_zone_service();

        // if it is local to this service then just get the relevant stub
        if (encap.destination_zone_id == serv->get_zone_id().as_destination())
        {
            auto ob = serv->get_object(encap.object_id).lock();
            if (!ob)
            {
                RPC_ERROR("Object not found - object is null in release");
                CO_RETURN rpc::error::OBJECT_NOT_FOUND();
            }

            auto count = serv->release_local_stub(ob, false);
            RPC_ASSERT(count);
            if (!count || count == std::numeric_limits<uint64_t>::max())
            {
                RPC_ERROR("Reference count error in release");
                CO_RETURN rpc::error::REFERENCE_COUNT_ERROR();
            }

            auto interface_stub = ob->get_interface(T::get_id(rpc::VERSION_2));
            if (!interface_stub)
            {
                if (!interface_stub)
                {
                    RPC_ERROR("Invalid interface ID in proxy release");
                    CO_RETURN rpc::error::INVALID_INTERFACE_ID();
                }
            }

            val = rpc::static_pointer_cast<T>(interface_stub->get_castable_interface());
            CO_RETURN rpc::error::OK();
        }

        // get the right  service proxy
        bool new_proxy_added = false;
        auto service_proxy = sp;

        if (sp->get_destination_zone_id() != encap.destination_zone_id)
        {
            // if the zone is different lookup or clone the right proxy
            // the service proxy is where the object came from so it should be used as the new caller channel for this returned object
            auto caller_channel_zone_id = sp->get_destination_zone_id().as_caller_channel();
            service_proxy = serv->get_zone_proxy(caller_channel_zone_id,
                caller_zone_id,
                {encap.destination_zone_id},
                sp->get_zone_id().as_caller(),
                new_proxy_added);
        }

        std::shared_ptr<rpc::object_proxy> op = CO_AWAIT service_proxy->get_or_create_object_proxy(
            encap.object_id, service_proxy::object_proxy_creation_rule::RELEASE_IF_NOT_NEW, false, {}, false);
        if (!op)
        {
            RPC_ERROR("Object not found in proxy_bind_out_param");
            CO_RETURN rpc::error::OBJECT_NOT_FOUND();
        }
        RPC_ASSERT(op != nullptr);
        CO_RETURN CO_AWAIT op->query_interface(val, false);
    }

    template<class T>
    CORO_TASK(int)
    demarshall_interface_proxy(uint64_t protocol_version,
        const std::shared_ptr<rpc::service_proxy>& sp,
        const rpc::interface_descriptor& encap,
        caller_zone caller_zone_id,
        rpc::shared_ptr<T>& val)
    {
        if (protocol_version > rpc::get_version())
        {
            RPC_ERROR("Incompatible service in demarshall_interface_proxy");
            CO_RETURN rpc::error::INCOMPATIBLE_SERVICE();
        }

        // if we have a null object id then return a null ptr
        if (encap.object_id == 0 || encap.destination_zone_id == 0)
            CO_RETURN rpc::error::OK();

        if (encap.destination_zone_id != sp->get_destination_zone_id())
        {
            CO_RETURN CO_AWAIT rpc::proxy_bind_out_param(sp, encap, caller_zone_id, val);
        }

        auto service_proxy = sp;
        auto serv = service_proxy->get_operating_zone_service();

        // if it is local to this service then just get the relevant stub
        if (serv->get_zone_id().as_destination() == encap.destination_zone_id)
        {
            // if we get here then we need to invent a test for this
            RPC_ASSERT(false);
            RPC_ERROR("Invalid data in demarshall_interface_proxy");
            CO_RETURN rpc::error::INVALID_DATA();
        }

        // get the right  service proxy
        // bool new_proxy_added = false;
        if (service_proxy->get_destination_zone_id() != encap.destination_zone_id)
        {
            // if we get here then we need to invent a test for this
            RPC_ASSERT(false);
            RPC_ERROR("Invalid data in demarshall_interface_proxy");
            CO_RETURN rpc::error::INVALID_DATA();
        }

        if (serv->get_parent_zone_id() == service_proxy->get_destination_zone_id())
            service_proxy->add_external_ref();

        std::shared_ptr<rpc::object_proxy> op = CO_AWAIT service_proxy->get_or_create_object_proxy(
            encap.object_id, service_proxy::object_proxy_creation_rule::DO_NOTHING, false, {}, false);
        if (!op)
        {
            RPC_ERROR("Object not found in demarshall_interface_proxy");
            CO_RETURN rpc::error::OBJECT_NOT_FOUND();
        }
        RPC_ASSERT(op != nullptr);
        CO_RETURN CO_AWAIT op->query_interface(val, false);
    }

}
