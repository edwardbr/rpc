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
        virtual int send(uint64_t zone_id, uint64_t object_id, uint64_t interface_id, uint64_t method_id, size_t in_size_,
                                const char* in_buf_, std::vector<char>& out_buf_)
            = 0;
        virtual int try_cast(uint64_t zone_id, uint64_t object_id, uint64_t interface_id) = 0;
        virtual uint64_t add_ref(uint64_t zone_id, uint64_t object_id) = 0;
        virtual uint64_t release(uint64_t zone_id, uint64_t object_id) = 0;
    };

    struct encapsulated_interface
    {
        encapsulated_interface() :
            object_id{0},
            zone_id{0}
        {}
        
        encapsulated_interface(uint64_t obj_id, uint64_t zn_id) : 
            object_id{obj_id},
            zone_id{zn_id}
        {}
        encapsulated_interface(const encapsulated_interface& other) = default;
        encapsulated_interface(encapsulated_interface&& other) = default;
        
        encapsulated_interface& operator= (const encapsulated_interface& other) = default;
        encapsulated_interface& operator= (encapsulated_interface&& other) = default;

        uint64_t object_id;
        uint64_t zone_id;
        
        template<typename Ar>
		void serialize(Ar &ar)
		{
			ar & YAS_OBJECT_NVP("encapsulated_interface"
			  ,("object_id", object_id)
			  ,("zone_id", zone_id)
			);
		}
    };
}