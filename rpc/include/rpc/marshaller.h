#pragma once

#include <string>
#include <memory>
#include <unordered_map>
#include <mutex>

#include <yas/count_streams.hpp>
#include <yas/serialize.hpp>
#include <rpc/error_codes.h>

namespace rpc
{
    // the used for marshalling data between zones
    class i_marshaller
    {
    public:
        virtual int send(uint64_t originating_zone_id, uint64_t zone_id, uint64_t object_id, uint64_t interface_id, uint64_t method_id, size_t in_size_,
                                const char* in_buf_, std::vector<char>& out_buf_)
            = 0;
        virtual int try_cast(uint64_t zone_id, uint64_t object_id, uint64_t interface_id) = 0;
        virtual uint64_t add_ref(uint64_t zone_id, uint64_t object_id) = 0;
        virtual uint64_t release(uint64_t zone_id, uint64_t object_id) = 0;
    };

    struct interface_descriptor
    {
        interface_descriptor() :
            object_id{0},
            zone_id{0}
        {}
        
        interface_descriptor(uint64_t obj_id, uint64_t zn_id) : 
            object_id{obj_id},
            zone_id{zn_id}
        {}
        interface_descriptor(const interface_descriptor& other) = default;
        interface_descriptor(interface_descriptor&& other) = default;
        
        interface_descriptor& operator= (const interface_descriptor& other) = default;
        interface_descriptor& operator= (interface_descriptor&& other) = default;

        uint64_t object_id;
        uint64_t zone_id;
        
        template<typename Ar>
		void serialize(Ar &ar)
		{
			ar & YAS_OBJECT_NVP("interface_descriptor"
			  ,("object_id", object_id)
			  ,("zone_id", zone_id)
			);
		}
    };
}