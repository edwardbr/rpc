#pragma once
#include <stdint.h>
#include <functional>

//this class is to ensure type safty of parameters as it gets difficult guaranteeing parameter order
namespace rpc
{
    template<typename Type>
    struct type_id
    {
        //keep it public
        uint64_t id = 0;

        type_id() = default;
        type_id(uint64_t initial_id) : id(initial_id){}
        type_id(const type_id<Type>& initial_id) = default;

        //getter
        uint64_t operator()() const {return id;}
        uint64_t operator*() const {return id;}
        uint64_t& get_ref() {return id;}

        //setter
        void operator=(uint64_t val) {id = val;}
        void operator=(type_id<Type> val) {id = val.id;}

        //setter
        bool operator==(uint64_t val) const {return id == val;}
        bool operator==(type_id<Type> val) const {return id == val.id;}

        //setter
        bool operator!=(uint64_t val) const {return id != val;}
        bool operator!=(type_id<Type> val) const {return id != val.id;}

        //less
        bool operator<(uint64_t val) const {return id < val;}
        bool operator<(const uint64_t& val) const {return id < val;}
    };

    //the actual zone that a service is running on
    //or the zone that a service_proxy is pointing to
    struct ZoneId{};
    struct zone : public type_id<ZoneId>
    {};

    struct ServiceProxyId{};
    struct zone_proxy : public type_id<ServiceProxyId>
    {};

    //the zone that a service proxy was cloned from
    struct ClonedFromZoneId{};
    struct cloned_zone : public type_id<ClonedFromZoneId>
    {};

    //the zone that a service proxy is operating on
    struct OperatingZoneId{};
    struct operating_zone : public type_id<OperatingZoneId>
    {};

    //the zone that initiated the call
    struct CallerId{};
    struct caller : public type_id<CallerId>
    {};

    //the zone that passed that the call into this zone from the caller, you may have a chain of zones so the caller zone id may be different zone from the originator
    struct OriginatorId{};
    struct originator : public type_id<OriginatorId>
    {};

    //an id for objects unique to each zone
    struct ObjectId{};
    struct object : public type_id<ObjectId>
    {};

    //an id for interfaces
    struct InterfaceId{};
    struct interface_ordinal : public type_id<InterfaceId> //cant use the name interface
    {};

    //an id for method ordinals
    struct MethodId{};
    struct method : public type_id<MethodId>
    {};
}

template<>
struct std::hash<rpc::zone> 
{
    auto operator() (const rpc::zone& item) const noexcept
    {
        return (std::size_t)item.id;

    }
};

template<>
struct std::hash<rpc::cloned_zone> 
{
    auto operator() (const rpc::cloned_zone& item) const noexcept 
    {
        return (std::size_t)item.id;

    }
};

template<>
struct std::hash<rpc::operating_zone> 
{
    auto operator() (const rpc::operating_zone& item) const noexcept 
    {
        return (std::size_t)item.id;
    }
};

template<>
struct std::hash<rpc::caller> 
{
    auto operator() (const rpc::caller& item) const noexcept 
    {
        return (std::size_t)item.id;

    }
};

template<>
struct std::hash<rpc::originator> 
{
    auto operator() (const rpc::originator& item) const noexcept 
    {
        return (std::size_t)item.id;

    }
};

template<>
struct std::hash<rpc::interface_ordinal> 
{
    auto operator() (const rpc::interface_ordinal& item) const noexcept 
    {
        return (std::size_t)item.id;

    }
};

template<>
struct std::hash<rpc::object> 
{
    auto operator() (const rpc::object& item) const noexcept 
    {
        return (std::size_t)item.id;

    }
};

template<>
struct std::hash<rpc::method> 
{
    auto operator() (const rpc::method& item) const noexcept 
    {
        return (std::size_t)item.id;

    }
};