#pragma once

#include <string>
#include <memory>
#include <unordered_map>
#include <mutex>

#include <yas/serialize.hpp>
#include <rpc/error_codes.h>

namespace rpc
{
    // the used for marshalling data between zones
    class i_marshaller
    {
    public:
        virtual int send(uint64_t object_id, uint64_t interface_id, uint64_t method_id, size_t in_size_,
                                const char* in_buf_, std::vector<char>& out_buf_)
            = 0;
        virtual int try_cast(uint64_t zone_id, uint64_t object_id, uint64_t interface_id) = 0;
        virtual uint64_t add_ref(uint64_t zone_id, uint64_t object_id) = 0;
        virtual uint64_t release(uint64_t zone_id, uint64_t object_id) = 0;
    };
}