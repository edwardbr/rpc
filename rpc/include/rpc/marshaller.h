/*
 *   Copyright (c) 2025 Edward Boggis-Rolfe
 *   All rights reserved.
 */
#pragma once

#include <string>
#include <memory>
#include <unordered_map>
#include <mutex>

#include <rpc/types.h>
#include <rpc/serialiser.h>
#include <rpc/error_codes.h>

namespace rpc
{
    enum class add_ref_options : std::uint8_t
    {
        normal = 1,
        // when unidirectionally addreffing the destination
        build_destination_route = 2,
        // when unidirectionally addreffing the caller which prepares refcounts etc in the reverse direction
        build_caller_route = 4
    };

    inline add_ref_options operator|(add_ref_options lhs, add_ref_options rhs)
    {
        return static_cast<add_ref_options>(static_cast<std::underlying_type<add_ref_options>::type>(lhs)
                                            | static_cast<std::underlying_type<add_ref_options>::type>(rhs));
    }
    inline add_ref_options operator&(add_ref_options lhs, add_ref_options rhs)
    {
        return static_cast<add_ref_options>(static_cast<std::underlying_type<add_ref_options>::type>(lhs)
                                            & static_cast<std::underlying_type<add_ref_options>::type>(rhs));
    }
    inline add_ref_options operator^(add_ref_options lhs, add_ref_options rhs)
    {
        return static_cast<add_ref_options>(static_cast<std::underlying_type<add_ref_options>::type>(lhs)
                                            ^ static_cast<std::underlying_type<add_ref_options>::type>(rhs));
    }

    inline add_ref_options operator~(add_ref_options lhs)
    {
        return static_cast<add_ref_options>(~static_cast<std::underlying_type<add_ref_options>::type>(lhs));
    }

    inline bool operator!(add_ref_options e)
    {
        return e == static_cast<add_ref_options>(0);
    }

    // the used for marshalling data between zones
    class i_marshaller
    {
    public:
        virtual ~i_marshaller() = default;
        virtual int send(uint64_t protocol_version,
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
            = 0;
        virtual int try_cast(
            uint64_t protocol_version, destination_zone destination_zone_id, object object_id, interface_ordinal interface_id)
            = 0;
        virtual int add_ref(uint64_t protocol_version,
            destination_channel_zone destination_channel_zone_id,
            destination_zone destination_zone_id,
            object object_id,
            caller_channel_zone caller_channel_zone_id,
            caller_zone caller_zone_id,
            known_direction_zone known_direction_zone_id,
            add_ref_options build_out_param_channel,
            uint64_t& reference_count)
            = 0;
        virtual int release(
            uint64_t protocol_version, destination_zone destination_zone_id, object object_id, caller_zone caller_zone_id,
            uint64_t& reference_count)
            = 0;
    };

    struct interface_descriptor
    {
        interface_descriptor()
            : object_id{0}
            , destination_zone_id{0}
        {
        }

        interface_descriptor(object obj_id, destination_zone dest_zone_id)
            : object_id{obj_id}
            , destination_zone_id{dest_zone_id}
        {
        }
        interface_descriptor(const interface_descriptor& other) = default;
        interface_descriptor(interface_descriptor&& other) = default;

        interface_descriptor& operator=(const interface_descriptor& other) = default;
        interface_descriptor& operator=(interface_descriptor&& other) = default;

        object object_id;
        destination_zone destination_zone_id;

        template<typename Ar> void serialize(Ar& ar)
        {
            ar& YAS_OBJECT_NVP("interface_descriptor", ("object_id", object_id.id), ("zone_id", destination_zone_id.id));
        }
    };

    inline bool operator==(const interface_descriptor& first, const interface_descriptor& second)
    {
        return first.destination_zone_id == second.destination_zone_id && first.object_id == second.object_id;
    }
    inline bool operator!=(const interface_descriptor& first, const interface_descriptor& second)
    {
        return !(first == second);
    }
}
