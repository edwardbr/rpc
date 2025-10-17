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
#include <memory>
#include <unordered_map>

namespace rpc
{
    class object_proxy;
    class object_stub;
    class service_proxy;
    class service;
    class object;
    template<class T> class shared_ptr;

    // this is a nice helper function to match an interface id to a interface in a version independent way
    template<class T> bool match(rpc::interface_ordinal interface_id)
    {
        return T::get_id(rpc::VERSION_2) == interface_id;
    }

    // this is the base class to all interfaces
    class casting_interface
    {
    public:
        virtual ~casting_interface() = default;
        virtual void* get_address() const = 0;
        virtual const rpc::casting_interface* query_interface(rpc::interface_ordinal interface_id) const = 0;

        virtual bool is_local() const { return true; }
        virtual std::shared_ptr<rpc::object_proxy> get_object_proxy() const { return nullptr; }


        static object get_object_id(const casting_interface& iface);
        static std::shared_ptr<rpc::service_proxy> get_service_proxy(const casting_interface& iface);
        static std::shared_ptr<rpc::service> get_service(const casting_interface& iface);
        static zone get_zone(const casting_interface& iface);
        static destination_zone get_destination_zone(const casting_interface& iface);
        static destination_channel_zone get_channel_zone(const casting_interface& iface);
    };

    bool are_in_same_zone(const casting_interface* first, const casting_interface* second);

    // T is a class derived from casting_interface its role is to provide access to the object proxy to the remote zone
    template<class T> class interface_proxy : public T
    {
    protected:
        stdex::member_ptr<object_proxy> object_proxy_;

    public:
        interface_proxy(std::shared_ptr<rpc::object_proxy> object_proxy)
            : object_proxy_(object_proxy)
        {
        }
        virtual ~interface_proxy() = default;

        void* get_address() const override { return (void*)this; }
        const rpc::casting_interface* query_interface(rpc::interface_ordinal interface_id) const override
        {
            if (rpc::match<T>(interface_id))
                return static_cast<const T*>(this);
            return nullptr;
        }

        bool is_local() const override { return false; }
        std::shared_ptr<rpc::object_proxy> get_object_proxy() const override { return object_proxy_.get_nullable(); }
    };

    // do not use directly it is for the interface generator use rpc::create_interface_proxy if you want to get a proxied pointer to a remote implementation
    template<class T>
    CORO_TASK(int)
    stub_bind_in_param(uint64_t protocol_version,
        rpc::service& serv,
        caller_channel_zone caller_channel_zone_id,
        caller_zone caller_zone_id,
        const rpc::interface_descriptor& encap,
        rpc::shared_ptr<T>& iface);

    // do not use directly it is for the interface generator use rpc::create_interface_proxy if you want to get a proxied pointer to a remote implementation
    template<class T>
    CORO_TASK(int)
    proxy_bind_out_param(const std::shared_ptr<rpc::service_proxy>& sp,
        const rpc::interface_descriptor& encap,
        caller_zone caller_zone_id,
        rpc::shared_ptr<T>& val);

    template<class T>
    CORO_TASK(int)
    demarshall_interface_proxy(uint64_t protocol_version,
        const std::shared_ptr<rpc::service_proxy>& sp,
        const rpc::interface_descriptor& encap,
        caller_zone caller_zone_id,
        rpc::shared_ptr<T>& val);

    // the route to all fingerprinting
    template<typename T> class id
    {
        // not implemented here! only in the concrete derivations
        // static constexpr uint64_t get(uint64_t)
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
