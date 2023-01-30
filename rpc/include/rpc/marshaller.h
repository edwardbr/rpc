#pragma once

#include <string>
#include <memory>
#include <unordered_map>
#include <mutex>

#include <yas/count_streams.hpp>
#include <yas/serialize.hpp>

#include <rpc/types.h>
#include <rpc/error_codes.h>

namespace rpc
{
    // the used for marshalling data between zones
    class i_marshaller
    {
    public:
        virtual int send(caller_channel_zone originating_zone_id, caller_zone caller_zone_id, destination_zone destination_zone_id, object object_id, interface_ordinal interface_id, method method_id, size_t in_size_,
                                const char* in_buf_, std::vector<char>& out_buf_)
            = 0;
        virtual int try_cast(destination_zone destination_zone_id, object object_id, interface_ordinal interface_id) = 0;
        virtual uint64_t add_ref(destination_zone destination_zone_id, object object_id, caller_zone caller_zone_id) = 0;
        virtual uint64_t release(destination_zone destination_zone_id, object object_id, caller_zone caller_zone_id) = 0;
    };

    struct interface_descriptor
    {
        interface_descriptor() :
            object_id{0},
            destination_zone_id{0}
        {}
        
        interface_descriptor(object obj_id, destination_zone dest_zone_id) : 
            object_id{obj_id},
            destination_zone_id{dest_zone_id}
        {}
        interface_descriptor(const interface_descriptor& other) = default;
        interface_descriptor(interface_descriptor&& other) = default;
        
        interface_descriptor& operator= (const interface_descriptor& other) = default;
        interface_descriptor& operator= (interface_descriptor&& other) = default;

        object object_id;
        destination_zone destination_zone_id;
        
        template<typename Ar>
		void serialize(Ar &ar)
		{
			ar & YAS_OBJECT_NVP("interface_descriptor"
			  ,("object_id", object_id.id)
			  ,("zone_id", destination_zone_id.id)
			);
		}
    };
}