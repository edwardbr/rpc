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

// Include the separated proxy classes
#include <rpc/internal/object_proxy.h>
#include <rpc/internal/service_proxy.h>

// All dependencies are included by rpc.h

#ifdef USE_RPC_TELEMETRY
#include <rpc/telemetry/i_telemetry_service.h>
#endif
// stub.h is included by rpc.h

namespace rpc
{
    class service;
    class child_service;
    template<class T> class proxy_impl;

    // Base class for interface proxies with virtual destructor for proper inheritance
    class proxy_base
    {
        stdex::member_ptr<object_proxy> object_proxy_;

    protected:
        proxy_base(std::shared_ptr<rpc::object_proxy> object_proxy)
            : object_proxy_(object_proxy)
        {
        }
        virtual ~proxy_base() { }

        template<class T>
        CORO_TASK(interface_descriptor)
        proxy_bind_in_param(
            uint64_t protocol_version, const rpc::shared_ptr<T>& iface, std::shared_ptr<rpc::object_stub>& stub);
        template<class T>
        CORO_TASK(interface_descriptor)
        stub_bind_out_param(uint64_t protocol_version,
            caller_channel_zone caller_channel_zone_id,
            caller_zone caller_zone_id,
            const rpc::shared_ptr<T>& iface);

        template<class T1, class T2>
        friend CORO_TASK(rpc::shared_ptr<T1>) dynamic_pointer_cast(const shared_ptr<T2>& from) noexcept;
        friend service;

    public:
        std::shared_ptr<rpc::object_proxy> get_object_proxy() const { return object_proxy_.get_nullable(); }
    };

    template<class T> class proxy_impl : public proxy_base, public T
    {
    public:
        proxy_impl(std::shared_ptr<rpc::object_proxy> object_proxy)
            : proxy_base(object_proxy)
        {
        }

        virtual void* get_address() const override
        {
            RPC_ASSERT(false);
            return (T*)get_object_proxy().get();
        }
        rpc::proxy_base* query_proxy_base() const override
        {
            return static_cast<proxy_base*>(const_cast<proxy_impl*>(this));
        }
        const rpc::casting_interface* query_interface(rpc::interface_ordinal interface_id) const override
        {
            if (T::get_id(rpc::VERSION_2) == interface_id)
                return static_cast<const T*>(this);
            return nullptr;
        }
        virtual ~proxy_impl() = default;
    };


    // declared here as object_proxy and service_proxy is not fully defined in the body of proxy_base
    template<class T>
    CORO_TASK(interface_descriptor)
    proxy_base::proxy_bind_in_param(
        uint64_t protocol_version, const rpc::shared_ptr<T>& iface, std::shared_ptr<rpc::object_stub>& stub)
    {
        if (!iface)
            CO_RETURN interface_descriptor();

        auto object_proxy = object_proxy_.get_nullable();
        RPC_ASSERT(object_proxy);
        if (!object_proxy)
            CO_RETURN interface_descriptor();
        auto operating_service = object_proxy->get_service_proxy()->get_operating_zone_service();

        // this is to check that an interface is belonging to another zone and not the operating zone
        auto proxy = iface->query_proxy_base();
        if (proxy && casting_interface::get_destination_zone(*iface) != operating_service->get_zone_id().as_destination())
        {
            CO_RETURN{casting_interface::get_object_id(*iface), casting_interface::get_destination_zone(*iface)};
        }

        // else encapsulate away
        CO_RETURN CO_AWAIT operating_service->proxy_bind_in_param(protocol_version, iface, stub);
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
            iface = serv.get_local_interface<T>(protocol_version, encap.object_id);
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

            std::shared_ptr<rpc::object_proxy> op = CO_AWAIT service_proxy->get_or_create_object_proxy(
                encap.object_id, service_proxy::object_proxy_creation_rule::ADD_REF_IF_NEW, new_proxy_added, caller_zone_id.as_known_direction_zone());
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

    // declared here as object_proxy and service_proxy is not fully defined in the body of proxy_base
    template<class T>
    CORO_TASK(interface_descriptor)
    proxy_base::stub_bind_out_param(uint64_t protocol_version,
        caller_channel_zone caller_channel_zone_id,
        caller_zone caller_zone_id,
        const rpc::shared_ptr<T>& iface)
    {
        if (!iface)
            CO_RETURN{{0}, {0}};

        auto object_proxy = object_proxy_.get_nullable();
        RPC_ASSERT(object_proxy);
        if (!object_proxy)
            CO_RETURN {{0}, {0}};
        auto operating_service = object_proxy->get_service_proxy()->get_operating_zone_service();

        // this is to check that an interface is belonging to another zone and not the operating zone
        auto proxy = iface->query_proxy_base();
        if (proxy && casting_interface::get_zone(*proxy) != operating_service->get_zone_id())
        {
            CO_RETURN{proxy->get_object_proxy()->get_object_id(), casting_interface::get_zone(*proxy)};
        }

        // else encapsulate away
        CO_RETURN operating_service->stub_bind_out_param(protocol_version, caller_channel_zone_id, caller_zone_id, iface);
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

            auto count = serv->release_local_stub(ob);
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
            encap.object_id, service_proxy::object_proxy_creation_rule::RELEASE_IF_NOT_NEW, false, {});
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
            RPC_ERROR("Invalid data in demarshall_interface_proxy");
            CO_RETURN rpc::error::INVALID_DATA();

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

        std::shared_ptr<rpc::object_proxy> op = CO_AWAIT service_proxy->get_or_create_object_proxy(
            encap.object_id, service_proxy::object_proxy_creation_rule::DO_NOTHING, false, {});
        if (!op)
        {
            RPC_ERROR("Object not found in demarshall_interface_proxy");
            CO_RETURN rpc::error::OBJECT_NOT_FOUND();
        }
        RPC_ASSERT(op != nullptr);
        CO_RETURN CO_AWAIT op->query_interface(val, false);
    }
}
