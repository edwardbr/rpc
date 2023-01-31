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
        constexpr uint64_t get_val() const {return id;}//only for c calls
        uint64_t& get_ref() {return id;}//for c calls

        //setter
        void operator=(uint64_t val) {id = val;}
        void operator=(const type_id<Type>& val) {id = val.id;}

        //setter
        constexpr bool operator==(const type_id<Type>& val) const {return id == val.id;}

        //setter
        constexpr bool operator!=(const type_id<Type>& val) const {return id != val.id;}

        //less
        constexpr bool operator<(uint64_t val) const {return id < val;}
        constexpr bool operator<(const uint64_t& val) const {return id < val;}

        constexpr bool is_set() const noexcept
        {
            return id != 0;
        }
    };

    //the actual zone that a service is running on
    struct ZoneId{};
    //the zone that is the ultimate zone for this call
    struct DestinationZoneId{};
    //a zone that calls are made to get through to the destincation zone
    struct DestinationChannelZoneId{};

    //the zone that initiates a call
    struct CallerZoneId{};
    //a zone that channels calls from a caller
    struct CallerChannelZoneId{};

    struct zone;
    struct destination_zone;
    struct destination_channel_zone;
    struct operating_zone;
    struct caller_zone;
    struct caller_channel_zone;
    struct caller_channel_zone;


    struct zone : public type_id<ZoneId>
    {
        zone() = default;
        zone(const type_id<ZoneId>& other) : type_id<ZoneId>(other){}

        type_id<DestinationZoneId> as_destination() const {return {id};}        
        type_id<CallerZoneId> as_caller() const {return {id};}              
        type_id<CallerChannelZoneId> as_caller_channel() const {return {id};}        
    };

    struct destination_zone : public type_id<DestinationZoneId>
    {
        destination_zone() = default;
        destination_zone(const type_id<DestinationZoneId>& other) : type_id<DestinationZoneId>(other){}

        type_id<DestinationChannelZoneId> as_destination_channel() const {return {id};}        
        type_id<CallerZoneId> as_caller() const {return {id};}        
        type_id<CallerChannelZoneId> as_caller_channel() const {return {id};}   
    };

    //the zone that a service proxy was cloned from
    struct destination_channel_zone : public type_id<DestinationChannelZoneId>
    {
        destination_channel_zone() = default;
        destination_channel_zone(const type_id<DestinationChannelZoneId>& other) : type_id<DestinationChannelZoneId>(other){}

    };

    //the zone that initiated the call
    struct caller_zone : public type_id<CallerZoneId>
    {
        caller_zone() = default;
        caller_zone(const type_id<CallerZoneId>& other) : type_id<CallerZoneId>(other){}
        
        type_id<CallerChannelZoneId> as_caller_channel() const {return {id};}   
        type_id<DestinationChannelZoneId> as_destination_channel() const {return {id};}   
    };

    //the zone that initiated the call
    struct caller_channel_zone : public type_id<CallerChannelZoneId>
    {
        caller_channel_zone() = default;
        caller_channel_zone(const type_id<CallerChannelZoneId>& other) : type_id<CallerChannelZoneId>(other){}

        type_id<DestinationZoneId> as_destination() const {return {id};}     //this one is wierd, its for cloning service proxies
        type_id<DestinationChannelZoneId> as_destination_channel() const {return {id};}  
    };


    //an id for objects unique to each zone
    struct ObjectId{};
    struct object : public type_id<ObjectId>
    {
        object() = default;
        object(const type_id<ObjectId>& other) : type_id<ObjectId>(other){}
    };

    //an id for interfaces
    struct InterfaceId{};
    struct interface_ordinal : public type_id<InterfaceId> //cant use the name interface
    {
        constexpr interface_ordinal() = default;
        constexpr interface_ordinal(const type_id<InterfaceId>& other) : type_id<InterfaceId>(other){}
    };

    //an id for method ordinals
    struct MethodId{};
    struct method : public type_id<MethodId>
    {
        method() = default;
        method(const type_id<MethodId>& other) : type_id<MethodId>(other){}
    };
}

namespace std
{
    template<typename Type>
    inline std::string to_string(rpc::type_id<Type> _Val) {
        return std::to_string(_Val.get_val());
    }
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
struct std::hash<rpc::destination_zone> 
{
    auto operator() (const rpc::destination_zone& item) const noexcept 
    {
        return (std::size_t)item.id;

    }
};

template<>
struct std::hash<rpc::destination_channel_zone> 
{
    auto operator() (const rpc::destination_channel_zone& item) const noexcept 
    {
        return (std::size_t)item.id;

    }
};

template<>
struct std::hash<rpc::caller_zone> 
{
    auto operator() (const rpc::caller_zone& item) const noexcept 
    {
        return (std::size_t)item.id;

    }
};

template<>
struct std::hash<rpc::caller_channel_zone> 
{
    auto operator() (const rpc::caller_channel_zone& item) const noexcept 
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