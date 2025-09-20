/*
 *   Copyright (c) 2025 Edward Boggis-Rolfe
 *   All rights reserved.
 */
#pragma once

// types.h and version.h are included by rpc.h
#include <string>
#include <vector>
#include <type_traits>
#include <stdint.h>

#include <rpc/internal/marshaller.h>
#include <rpc/internal/stub.h>

namespace rpc
{
    class casting_interface;
    class object_proxy;
    class service_proxy;
    class service;
    class object;
    template<class T> class shared_ptr;

    // this do nothing class is for static pointer casting
    class casting_interface
    {
    public:
        virtual ~casting_interface() = default;
        virtual void* get_address() const = 0;
        virtual const rpc::casting_interface* query_interface(rpc::interface_ordinal interface_id) const = 0;
        virtual std::shared_ptr<rpc::object_proxy> get_object_proxy() const { return nullptr; }

        static std::shared_ptr<rpc::object_proxy> get_object_proxy(const casting_interface& iface);
        static object get_object_id(const casting_interface& iface);
        static std::shared_ptr<rpc::service_proxy> get_service_proxy(const casting_interface& iface);
        static std::shared_ptr<rpc::service> get_service(const casting_interface& iface);
        static zone get_zone(const casting_interface& iface);
        static destination_zone get_destination_zone(const casting_interface& iface);
        static destination_channel_zone get_channel_zone(const casting_interface& iface);
    };

    template<class T> class casting_interface_t : public casting_interface, public T
    {
        stdex::member_ptr<object_proxy> object_proxy_;

    public:
        virtual ~casting_interface_t() = default;
        std::shared_ptr<rpc::object_proxy> get_object_proxy() const { return object_proxy_.get_nullable(); }

        virtual CORO_TASK(interface_descriptor) proxy_bind_in_param(
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
            if (casting_interface::get_destination_zone(*iface) != operating_service->get_zone_id().as_destination())
            {
                CO_RETURN{casting_interface::get_object_id(*iface), casting_interface::get_destination_zone(*iface)};
            }

            // else encapsulate away
            CO_RETURN CO_AWAIT operating_service->proxy_bind_in_param(protocol_version, iface, stub);
        }

        virtual CORO_TASK(interface_descriptor) stub_bind_out_param(uint64_t protocol_version,
            caller_channel_zone caller_channel_zone_id,
            caller_zone caller_zone_id,
            const rpc::shared_ptr<T>& iface)

        {
            if (!iface)
                CO_RETURN{{0}, {0}};

            auto object_proxy = object_proxy_.get_nullable();
            RPC_ASSERT(object_proxy);
            if (!object_proxy)
                CO_RETURN{{0}, {0}};
            auto operating_service = object_proxy->get_service_proxy()->get_operating_zone_service();

            // this is to check that an interface is belonging to another zone and not the operating zone
            auto proxy = iface->query_casting_interface();
            if (proxy && casting_interface::get_zone(*proxy) != operating_service->get_zone_id())
            {
                CO_RETURN{proxy->get_object_proxy()->get_object_id(), casting_interface::get_zone(*proxy)};
            }

            // else encapsulate away
            CO_RETURN operating_service->stub_bind_out_param(
                protocol_version, caller_channel_zone_id, caller_zone_id, iface);
        }
    };
    

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
    
    bool are_in_same_zone(const casting_interface* first, const casting_interface* second);

    // the route to all fingerprinting
    template<typename T> class id
    {
        // not implemented here! only in the concrete derivations
        // static constexpr uint64_t get(uint64_t)
    };

    // this is a nice helper function to match an interface id to a interface in a version independent way
    template<class T> bool match(rpc::interface_ordinal interface_id)
    {
        return T::get_id(rpc::VERSION_2) == interface_id;
    }

    // these will soon be superfluous
    template<typename T> class has_get_id_member
    {
        typedef char one;
        struct two
        {
            char x[2];
        };

        template<typename C> static one test(decltype(&C::get_id));
        template<typename C> static two test(...);

    public:
        enum
        {
            value = sizeof(test<T>(0)) == sizeof(char)
        };
    };

    template<typename T> class has_id_get_member
    {
        typedef char one;
        struct two
        {
            char x[2];
        };

        template<typename C> static one test(decltype(&id<C>::get));
        template<typename C> static two test(...);

    public:
        enum
        {
            value = sizeof(test<T>(0)) == sizeof(char)
        };
    };

    constexpr uint64_t STD_VECTOR_UINT_8_ID = 0x71FC1FAC5CD5E6FA;
    constexpr uint64_t STD_STRING_ID = 0x71FC1FAC5CD5E6F9;
    constexpr uint64_t UINT_8_ID = 0x71FC1FAC5CD5E6F8;
    constexpr uint64_t UINT_16_ID = 0x71FC1FAC5CD5E6F7;
    constexpr uint64_t UINT_32_ID = 0x71FC1FAC5CD5E6F6;
    constexpr uint64_t UINT_64_ID = 0x71FC1FAC5CD5E6F5;
    constexpr uint64_t INT_8_ID = 0x71FC1FAC5CD5E6F4;
    constexpr uint64_t INT_16_ID = 0x71FC1FAC5CD5E6F3;
    constexpr uint64_t INT_32_ID = 0x71FC1FAC5CD5E6F2;
    constexpr uint64_t INT_64_ID = 0x71FC1FAC5CD5E6F1;
    constexpr uint64_t FLOAT_32_ID = 0x71FC1FAC5CD5E6F0;
    constexpr uint64_t FLOAT_64_ID = 0x71FC1FAC5CD5E6EF;

    // declarations more can be added but these are the most commonly used

    template<> class id<std::string>
    {
    public:
        static constexpr uint64_t get(uint64_t) { return STD_STRING_ID; }
    };

    template<> class id<std::vector<uint8_t>>
    {
    public:
        static constexpr uint64_t get(uint64_t) { return STD_VECTOR_UINT_8_ID; }
    };

    template<> class id<uint8_t>
    {
    public:
        static constexpr uint64_t get(uint64_t) { return UINT_8_ID; }
    };

    template<> class id<uint16_t>
    {
    public:
        static constexpr uint64_t get(uint64_t) { return UINT_16_ID; }
    };

    template<> class id<uint32_t>
    {
    public:
        static constexpr uint64_t get(uint64_t) { return UINT_32_ID; }
    };

    template<> class id<uint64_t>
    {
    public:
        static constexpr uint64_t get(uint64_t) { return UINT_64_ID; }
    };

    template<> class id<int8_t>
    {
    public:
        static constexpr uint64_t get(uint64_t) { return INT_8_ID; }
    };

    template<> class id<int16_t>
    {
    public:
        static constexpr uint64_t get(uint64_t) { return INT_16_ID; }
    };

    template<> class id<int32_t>
    {
    public:
        static constexpr uint64_t get(uint64_t) { return INT_32_ID; }
    };

    template<> class id<int64_t>
    {
    public:
        static constexpr uint64_t get(uint64_t) { return INT_64_ID; }
    };

    template<> class id<float>
    {
    public:
        static constexpr uint64_t get(uint64_t) { return FLOAT_32_ID; }
    };

    template<> class id<double>
    {
    public:
        static constexpr uint64_t get(uint64_t) { return FLOAT_64_ID; }
    };
}
