/*
 *   Copyright (c) 2025 Edward Boggis-Rolfe
 *   All rights reserved.
 */
#pragma once

#include <string>
#include <memory>
#include <unordered_map>
#include <mutex>

// types.h, error_codes.h, and serialiser.h are included by rpc.h

namespace rpc
{
    enum class add_ref_options : std::uint8_t
    {
        normal = 0,
        // when unidirectionally addreffing the destination
        build_destination_route = 1,
        // when unidirectionally addreffing the caller which prepares refcounts etc in the reverse direction
        build_caller_route = 2,
        // when the add_ref is from optimistic_ptr reference (assumed shared if not set)
        optimistic = 4
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

    enum class release_options : std::uint8_t
    {
        normal = 0,
        // when the release is from optimistic_ptr reference (assumed shared if not set)
        optimistic = 1
    };

    inline release_options operator|(release_options lhs, release_options rhs)
    {
        return static_cast<release_options>(static_cast<std::underlying_type<release_options>::type>(lhs)
                                            | static_cast<std::underlying_type<release_options>::type>(rhs));
    }
    inline release_options operator&(release_options lhs, release_options rhs)
    {
        return static_cast<release_options>(static_cast<std::underlying_type<release_options>::type>(lhs)
                                            & static_cast<std::underlying_type<release_options>::type>(rhs));
    }
    inline release_options operator^(release_options lhs, release_options rhs)
    {
        return static_cast<release_options>(static_cast<std::underlying_type<release_options>::type>(lhs)
                                            ^ static_cast<std::underlying_type<release_options>::type>(rhs));
    }

    inline release_options operator~(release_options lhs)
    {
        return static_cast<release_options>(~static_cast<std::underlying_type<release_options>::type>(lhs));
    }

    inline bool operator!(release_options e)
    {
        return e == static_cast<release_options>(0);
    }

    enum class post_options : std::uint8_t
    {
        normal = 0,
        // when posting a zone termination notification
        zone_terminating = 1,
        // when posting a release for optimistic_ptr
        release_optimistic = 2
    };

    inline post_options operator|(post_options lhs, post_options rhs)
    {
        return static_cast<post_options>(static_cast<std::underlying_type<post_options>::type>(lhs)
                                            | static_cast<std::underlying_type<post_options>::type>(rhs));
    }
    inline post_options operator&(post_options lhs, post_options rhs)
    {
        return static_cast<post_options>(static_cast<std::underlying_type<post_options>::type>(lhs)
                                            & static_cast<std::underlying_type<post_options>::type>(rhs));
    }
    inline post_options operator^(post_options lhs, post_options rhs)
    {
        return static_cast<post_options>(static_cast<std::underlying_type<post_options>::type>(lhs)
                                            ^ static_cast<std::underlying_type<post_options>::type>(rhs));
    }

    inline post_options operator~(post_options lhs)
    {
        return static_cast<post_options>(~static_cast<std::underlying_type<post_options>::type>(lhs));
    }

    inline bool operator!(post_options e)
    {
        return e == static_cast<post_options>(0);
    }

    // the used for marshalling data between zones
    class i_marshaller
    {
    public:
        virtual ~i_marshaller() = default;
        virtual CORO_TASK(int) send(uint64_t protocol_version,
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
            std::vector<char>& out_buf_,
            const std::vector<back_channel_entry>& in_back_channel,
            std::vector<back_channel_entry>& out_back_channel)
            = 0;
        virtual CORO_TASK(void) post(uint64_t protocol_version,
            encoding encoding,
            uint64_t tag,
            caller_channel_zone caller_channel_zone_id,
            caller_zone caller_zone_id,
            destination_zone destination_zone_id,
            object object_id,
            interface_ordinal interface_id,
            method method_id,
            post_options options,
            size_t in_size_,
            const char* in_buf_,
            const std::vector<back_channel_entry>& in_back_channel)
            = 0;
        virtual CORO_TASK(int) try_cast(
            uint64_t protocol_version, destination_zone destination_zone_id, object object_id, interface_ordinal interface_id,
            const std::vector<back_channel_entry>& in_back_channel,
            std::vector<back_channel_entry>& out_back_channel)
            = 0;
        virtual CORO_TASK(int) add_ref(uint64_t protocol_version,
            destination_channel_zone destination_channel_zone_id,
            destination_zone destination_zone_id,
            object object_id,
            caller_channel_zone caller_channel_zone_id,
            caller_zone caller_zone_id,
            known_direction_zone known_direction_zone_id,
            add_ref_options build_out_param_channel,
            uint64_t& reference_count,
            const std::vector<back_channel_entry>& in_back_channel,
            std::vector<back_channel_entry>& out_back_channel)
            = 0;
        virtual CORO_TASK(int) release(uint64_t protocol_version,
            destination_zone destination_zone_id,
            object object_id,
            caller_zone caller_zone_id,
            release_options options,
            uint64_t& reference_count,
            const std::vector<back_channel_entry>& in_back_channel,
            std::vector<back_channel_entry>& out_back_channel)
            = 0;
    };

    struct retry_buffer
    {
        std::vector<char> data;
        int return_value;
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
            ar& YAS_OBJECT_NVP("interface_descriptor", ("object_id", object_id), ("zone_id", destination_zone_id));
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
