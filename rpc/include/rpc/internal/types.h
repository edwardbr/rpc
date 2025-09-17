/*
 *   Copyright (c) 2024 Edward Boggis-Rolfe
 *   All rights reserved.
 */
#pragma once
#include <string>
#include <stdint.h>
#include <functional>

#include "rpc/internal/coroutine_support.h"
#include <rpc/internal/serialiser.h>

//this class is to ensure type safty of parameters as it gets difficult guaranteeing parameter order
namespace rpc
{
    struct zone;
    struct destination_zone;
    struct destination_channel_zone;
    struct operating_zone;
    struct caller_zone;
    struct caller_channel_zone;
    struct known_direction_zone;

    struct zone
    {
    private:
        uint64_t id = 0;

    public:
        zone() = default;
        zone(const zone&) = default;
        zone(zone&&) noexcept = default;
        zone(uint64_t initial_id)
            : id(initial_id)
        {
        }
        zone& operator=(const zone&) = default;
        zone& operator=(zone&&) noexcept = default;
        zone& operator=(uint64_t other)
        {
            id = other;
            return *this;
        }

        uint64_t get_val() const { return id; }

        // compare
        constexpr bool operator==(const zone& val) const { return id == val.id; }
        template<typename T, std::enable_if_t<std::is_unsigned<T>::value, int> = 0>
        constexpr bool operator==(T val) const
        {
            return id == static_cast<uint64_t>(val);
        }
        constexpr bool operator!=(const zone& val) const { return id != val.id; }
        template<typename T, std::enable_if_t<std::is_unsigned<T>::value, int> = 0>
        constexpr bool operator!=(T val) const
        {
            return id != static_cast<uint64_t>(val);
        }

        // less
        constexpr bool operator<(uint64_t val) const { return id < val; }
        constexpr bool operator<(const zone& val) const { return id < val.id; }

        constexpr bool is_set() const noexcept { return id != 0; }

        inline destination_zone as_destination() const;
        inline destination_channel_zone as_destination_channel() const;
        inline caller_zone as_caller() const;
        inline caller_channel_zone as_caller_channel() const;

        template<typename Ar> void serialize(Ar& ar)
        {
            ar& YAS_OBJECT_NVP("id", ("id", id));
        }
    };

    struct destination_zone
    {
    private:
        uint64_t id = 0;

    public:
        destination_zone() = default;
        destination_zone(const destination_zone&) = default;
        destination_zone(destination_zone&&) noexcept = default;
        destination_zone(uint64_t initial_id)
            : id(initial_id)
        {
        }
        destination_zone& operator=(const destination_zone&) = default;
        destination_zone& operator=(destination_zone&&) noexcept = default;
        destination_zone& operator=(uint64_t other)
        {
            id = other;
            return *this;
        }

        uint64_t get_val() const { return id; }

        // compare
        constexpr bool operator==(const destination_zone& val) const { return id == val.id; }
        template<typename T, std::enable_if_t<std::is_unsigned<T>::value, int> = 0>
        constexpr bool operator==(T val) const
        {
            return id == static_cast<uint64_t>(val);
        }
        constexpr bool operator!=(const destination_zone& val) const { return id != val.id; }
        template<typename T, std::enable_if_t<std::is_unsigned<T>::value, int> = 0>
        constexpr bool operator!=(T val) const
        {
            return id != static_cast<uint64_t>(val);
        }

        constexpr bool operator<(uint64_t val) const { return id < val; }
        constexpr bool operator<(const destination_zone& val) const { return id < val.id; }

        constexpr bool is_set() const noexcept { return id != 0; }

        inline zone as_zone() const;
        inline destination_channel_zone as_destination_channel() const;
        inline caller_zone as_caller() const;
        inline caller_channel_zone as_caller_channel() const;

        template<typename Ar> void serialize(Ar& ar)
        {
            ar& YAS_OBJECT_NVP("id", ("id", id));
        }
    };

    // the zone that a service proxy was cloned from
    struct destination_channel_zone
    {
    private:
        uint64_t id = 0;

    public:
        destination_channel_zone() = default;
        destination_channel_zone(const destination_channel_zone&) = default;
        destination_channel_zone(destination_channel_zone&&) noexcept = default;
        destination_channel_zone(uint64_t initial_id)
            : id(initial_id)
        {
        }
        destination_channel_zone& operator=(const destination_channel_zone&) = default;
        destination_channel_zone& operator=(destination_channel_zone&&) noexcept = default;
        destination_channel_zone& operator=(uint64_t other)
        {
            id = other;
            return *this;
        }

        uint64_t get_val() const { return id; }

        // compare
        constexpr bool operator==(const destination_channel_zone& val) const { return id == val.id; }
        template<typename T, std::enable_if_t<std::is_unsigned<T>::value, int> = 0>
        constexpr bool operator==(T val) const
        {
            return id == static_cast<uint64_t>(val);
        }
        constexpr bool operator!=(const destination_channel_zone& val) const { return id != val.id; }
        template<typename T, std::enable_if_t<std::is_unsigned<T>::value, int> = 0>
        constexpr bool operator!=(T val) const
        {
            return id != static_cast<uint64_t>(val);
        }

        constexpr bool operator<(uint64_t val) const { return id < val; }
        constexpr bool operator<(const destination_channel_zone& val) const { return id < val.id; }

        constexpr bool is_set() const noexcept { return id != 0; }

        inline destination_zone as_destination() const;
        inline caller_channel_zone as_caller_channel() const;

        template<typename Ar> void serialize(Ar& ar)
        {
            ar& YAS_OBJECT_NVP("id", ("id", id));
        }
    };

    // the zone that initiated the call
    struct caller_zone
    {
    private:
        uint64_t id = 0;

    public:
        caller_zone() = default;
        caller_zone(const caller_zone&) = default;
        caller_zone(caller_zone&&) noexcept = default;
        caller_zone(uint64_t initial_id)
            : id(initial_id)
        {
        }
        caller_zone& operator=(const caller_zone&) = default;
        caller_zone& operator=(caller_zone&&) noexcept = default;
        caller_zone& operator=(uint64_t other)
        {
            id = other;
            return *this;
        }

        uint64_t get_val() const { return id; }

        // compare
        constexpr bool operator==(const caller_zone& val) const { return id == val.id; }
        template<typename T, std::enable_if_t<std::is_unsigned<T>::value, int> = 0>
        constexpr bool operator==(T val) const
        {
            return id == static_cast<uint64_t>(val);
        }
        constexpr bool operator!=(const caller_zone& val) const { return id != val.id; }
        template<typename T, std::enable_if_t<std::is_unsigned<T>::value, int> = 0>
        constexpr bool operator!=(T val) const
        {
            return id != static_cast<uint64_t>(val);
        }

        // less
        constexpr bool operator<(uint64_t val) const { return id < val; }
        constexpr bool operator<(const caller_zone& val) const { return id < val.id; }

        constexpr bool is_set() const noexcept { return id != 0; }

        inline caller_channel_zone as_caller_channel() const;
        inline destination_zone as_destination() const;
        inline destination_channel_zone as_destination_channel() const;
        inline known_direction_zone as_known_direction_zone() const;

        template<typename Ar> void serialize(Ar& ar)
        {
            ar& YAS_OBJECT_NVP("id", ("id", id));
        }
    };

    // the zone that initiated the call
    struct caller_channel_zone
    {
    private:
        uint64_t id = 0;

    public:
        caller_channel_zone() = default;
        caller_channel_zone(const caller_channel_zone&) = default;
        caller_channel_zone(caller_channel_zone&&) noexcept = default;
        caller_channel_zone(uint64_t initial_id)
            : id(initial_id)
        {
        }
        caller_channel_zone& operator=(const caller_channel_zone&) = default;
        caller_channel_zone& operator=(caller_channel_zone&&) noexcept = default;
        caller_channel_zone& operator=(uint64_t other)
        {
            id = other;
            return *this;
        }

        uint64_t get_val() const { return id; }

        // compare
        constexpr bool operator==(const caller_channel_zone& val) const { return id == val.id; }
        template<typename T, std::enable_if_t<std::is_unsigned<T>::value, int> = 0>
        constexpr bool operator==(T val) const
        {
            return id == static_cast<uint64_t>(val);
        }
        constexpr bool operator!=(const caller_channel_zone& val) const { return id != val.id; }
        template<typename T, std::enable_if_t<std::is_unsigned<T>::value, int> = 0>
        constexpr bool operator!=(T val) const
        {
            return id != static_cast<uint64_t>(val);
        }

        // less
        constexpr bool operator<(uint64_t val) const { return id < val; }
        constexpr bool operator<(const caller_channel_zone& val) const { return id < val.id; }

        constexpr bool is_set() const noexcept { return id != 0; }

        inline destination_zone as_destination() const;
        inline destination_channel_zone as_destination_channel() const;

        template<typename Ar> void serialize(Ar& ar)
        {
            ar& YAS_OBJECT_NVP("id", ("id", id));
        }
    };

    struct known_direction_zone
    {
    private:
        uint64_t id = 0;

    public:
        known_direction_zone() = default;
        known_direction_zone(const known_direction_zone&) = default;
        known_direction_zone(known_direction_zone&&) noexcept = default;
        known_direction_zone(zone z) noexcept
            : id(z.get_val())
        {
        }
        known_direction_zone(uint64_t initial_id)
            : id(initial_id)
        {
        }
        known_direction_zone& operator=(const known_direction_zone&) = default;
        known_direction_zone& operator=(known_direction_zone&&) noexcept = default;
        known_direction_zone& operator=(uint64_t other)
        {
            id = other;
            return *this;
        }

        uint64_t get_val() const { return id; }

        // compare
        constexpr bool operator==(const known_direction_zone& val) const { return id == val.id; }
        template<typename T, std::enable_if_t<std::is_unsigned<T>::value, int> = 0>
        constexpr bool operator==(T val) const
        {
            return id == static_cast<uint64_t>(val);
        }
        constexpr bool operator!=(const known_direction_zone& val) const { return id != val.id; }
        template<typename T, std::enable_if_t<std::is_unsigned<T>::value, int> = 0>
        constexpr bool operator!=(T val) const
        {
            return id != static_cast<uint64_t>(val);
        }

        // less
        constexpr bool operator<(uint64_t val) const { return id < val; }
        constexpr bool operator<(const known_direction_zone& val) const { return id < val.id; }

        constexpr bool is_set() const noexcept { return id != 0; }

        inline destination_zone as_destination() const;
        // inline destination_channel_zone as_destination_channel() const;

        template<typename Ar> void serialize(Ar& ar)
        {
            ar& YAS_OBJECT_NVP("id", ("id", id));
        }
    };

    // an id for objects unique to each zone
    struct object
    {
    private:
        uint64_t id = 0;

    public:
        object() = default;
        object(const object&) = default;
        object(object&&) noexcept = default;
        object(uint64_t initial_id)
            : id(initial_id)
        {
        }
        object& operator=(const object&) = default;
        object& operator=(object&&) noexcept = default;
        object& operator=(uint64_t other)
        {
            id = other;
            return *this;
        }

        uint64_t get_val() const { return id; }

        // compare
        constexpr bool operator==(const object& val) const { return id == val.id; }
        template<typename T, std::enable_if_t<std::is_unsigned<T>::value, int> = 0>
        constexpr bool operator==(T val) const
        {
            return id == static_cast<uint64_t>(val);
        }
        constexpr bool operator!=(const object& val) const { return id != val.id; }
        template<typename T, std::enable_if_t<std::is_unsigned<T>::value, int> = 0>
        constexpr bool operator!=(T val) const
        {
            return id != static_cast<uint64_t>(val);
        }

        // less
        constexpr bool operator<(uint64_t val) const { return id < val; }
        constexpr bool operator<(object val) const { return id < val.id; }

        constexpr bool is_set() const noexcept { return id != 0; }

        template<typename Ar> void serialize(Ar& ar)
        {
            ar& YAS_OBJECT_NVP("id", ("id", id));
        }
    };

    // an id for interfaces
    struct interface_ordinal // cant use the name interface
    {
    private:
        uint64_t id = 0;

    public:
        interface_ordinal() = default;
        interface_ordinal(const interface_ordinal&) = default;
        interface_ordinal(interface_ordinal&&) noexcept = default;
        interface_ordinal(uint64_t initial_id)
            : id(initial_id)
        {
        }
        interface_ordinal& operator=(const interface_ordinal&) = default;
        interface_ordinal& operator=(interface_ordinal&&) noexcept = default;
        interface_ordinal& operator=(uint64_t other)
        {
            id = other;
            return *this;
        }

        uint64_t get_val() const { return id; }

        // compare
        constexpr bool operator==(const interface_ordinal& val) const { return id == val.id; }
        template<typename T, std::enable_if_t<std::is_unsigned<T>::value, int> = 0>
        constexpr bool operator==(T val) const
        {
            return id == static_cast<uint64_t>(val);
        }
        constexpr bool operator!=(const interface_ordinal& val) const { return id != val.id; }
        template<typename T, std::enable_if_t<std::is_unsigned<T>::value, int> = 0>
        constexpr bool operator!=(T val) const
        {
            return id != static_cast<uint64_t>(val);
        }

        // less
        constexpr bool operator<(uint64_t val) const { return id < val; }
        constexpr bool operator<(interface_ordinal val) const { return id < val.id; }

        constexpr bool is_set() const noexcept { return id != 0; }

        template<typename Ar> void serialize(Ar& ar)
        {
            ar& YAS_OBJECT_NVP("id", ("id", id));
        }
    };

    // an id for method ordinals
    struct method
    {
    private:
        uint64_t id = 0;

    public:
        method() = default;
        method(const method&) = default;
        method(method&&) noexcept = default;
        method(uint64_t initial_id)
            : id(initial_id)
        {
        }
        method& operator=(const method&) = default;
        method& operator=(method&&) noexcept = default;
        method& operator=(uint64_t other)
        {
            id = other;
            return *this;
        }

        uint64_t get_val() const { return id; }

        // compare
        constexpr bool operator==(const method& val) const { return id == val.id; }
        template<typename T, std::enable_if_t<std::is_unsigned<T>::value, int> = 0>
        constexpr bool operator==(T val) const
        {
            return id == static_cast<uint64_t>(val);
        }
        constexpr bool operator!=(const method& val) const { return id != val.id; }
        template<typename T, std::enable_if_t<std::is_unsigned<T>::value, int> = 0>
        constexpr bool operator!=(T val) const
        {
            return id != static_cast<uint64_t>(val);
        }

        // less
        constexpr bool operator<(uint64_t val) const { return id < val; }
        constexpr bool operator<(method val) const { return id < val.id; }

        constexpr bool is_set() const noexcept { return id != 0; }

        template<typename Ar> void serialize(Ar& ar)
        {
            ar& YAS_OBJECT_NVP("id", ("id", id));
        }
    };

    struct function_info
    {
        std::string full_name;
        std::string name;
        method id;
        uint64_t tag;
        bool marshalls_interfaces;
        std::string description;
        std::string in_json_schema;
        std::string out_json_schema;
    };

    // converters
    destination_zone zone::as_destination() const
    {
        return destination_zone(id);
    }
    destination_channel_zone zone::as_destination_channel() const
    {
        return destination_channel_zone(id);
    }
    caller_zone zone::as_caller() const
    {
        return caller_zone(id);
    }
    caller_channel_zone zone::as_caller_channel() const
    {
        return caller_channel_zone(id);
    }

    zone destination_zone::as_zone() const
    {
        return zone(id);
    }
    destination_channel_zone destination_zone::as_destination_channel() const
    {
        return destination_channel_zone(id);
    }
    caller_zone destination_zone::as_caller() const
    {
        return caller_zone(id);
    }
    caller_channel_zone destination_zone::as_caller_channel() const
    {
        return caller_channel_zone(id);
    }

    destination_zone destination_channel_zone::as_destination() const
    {
        return destination_zone(id);
    }
    caller_channel_zone destination_channel_zone::as_caller_channel() const
    {
        return caller_channel_zone(id);
    }

    caller_channel_zone caller_zone::as_caller_channel() const
    {
        return caller_channel_zone(id);
    }
    destination_zone caller_zone::as_destination() const
    {
        return destination_zone(id);
    }
    destination_channel_zone caller_zone::as_destination_channel() const
    {
        return destination_channel_zone(id);
    }
    
    known_direction_zone caller_zone::as_known_direction_zone() const
    {
        return known_direction_zone(id);
    }

    destination_zone caller_channel_zone::as_destination() const
    {
        return destination_zone(id);
    }
    destination_channel_zone caller_channel_zone::as_destination_channel() const
    {
        return destination_channel_zone(id);
    }    
    
    destination_zone known_direction_zone::as_destination() const
    {
        return destination_zone(id);        
    }
}

namespace std
{
    // Remove the old template for type_id, and provide overloads for each affected class:
    inline std::string to_string(const rpc::zone& val)
    {
        return std::to_string(val.get_val());
    }
    inline std::string to_string(const rpc::destination_zone& val)
    {
        return std::to_string(val.get_val());
    }
    inline std::string to_string(const rpc::destination_channel_zone& val)
    {
        return std::to_string(val.get_val());
    }
    inline std::string to_string(const rpc::caller_zone& val)
    {
        return std::to_string(val.get_val());
    }
    inline std::string to_string(const rpc::caller_channel_zone& val)
    {
        return std::to_string(val.get_val());
    }
    inline std::string to_string(const rpc::known_direction_zone& val)
    {
        return std::to_string(val.get_val());
    }
    inline std::string to_string(const rpc::object& val)
    {
        return std::to_string(val.get_val());
    }
    inline std::string to_string(const rpc::interface_ordinal& val)
    {
        return std::to_string(val.get_val());
    }
    inline std::string to_string(const rpc::method& val)
    {
        return std::to_string(val.get_val());
    }

    template<> struct hash<rpc::zone>
    {
        auto operator()(const rpc::zone& item) const noexcept { return (std::size_t)item.get_val(); }
    };

    template<> struct hash<rpc::destination_zone>
    {
        auto operator()(const rpc::destination_zone& item) const noexcept { return (std::size_t)item.get_val(); }
    };

    template<> struct hash<rpc::destination_channel_zone>
    {
        auto operator()(const rpc::destination_channel_zone& item) const noexcept { return (std::size_t)item.get_val(); }
    };

    template<> struct hash<rpc::caller_zone>
    {
        auto operator()(const rpc::caller_zone& item) const noexcept { return (std::size_t)item.get_val(); }
    };

    template<> struct hash<rpc::caller_channel_zone>
    {
        auto operator()(const rpc::caller_channel_zone& item) const noexcept { return (std::size_t)item.get_val(); }
    };

    template<> struct hash<rpc::known_direction_zone>
    {
        auto operator()(const rpc::known_direction_zone& item) const noexcept { return (std::size_t)item.get_val(); }
    };

    template<> struct hash<rpc::interface_ordinal>
    {
        auto operator()(const rpc::interface_ordinal& item) const noexcept { return (std::size_t)item.get_val(); }
    };

    template<> struct hash<rpc::object>
    {
        auto operator()(const rpc::object& item) const noexcept { return (std::size_t)item.get_val(); }
    };

    template<> struct hash<rpc::method>
    {
        auto operator()(const rpc::method& item) const noexcept { return (std::size_t)item.get_val(); }
    };
}
