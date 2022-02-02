#pragma once

#include <string>
#include <memory>
#include <unordered_map>
#include <mutex>
#include <assert.h>

#include <marshaller/marshaller.h>


class object_stub
{
    // stubs have stong pointers
    std::unordered_map<uint64_t, std::shared_ptr<i_interface_marshaller>> proxy_map;
    std::mutex insert_control;

public:
    object_stub(std::shared_ptr<i_interface_marshaller> initial_interface);
    error_code call(uint64_t interface_id, uint64_t method_id, size_t in_size_, const char* in_buf_, size_t out_size_,
                    char* out_buf_);
    error_code try_cast(uint64_t interface_id);
};


//responsible for all object lifetimes created within the zone
class zone_stub : public i_marshaller
{
    inline static zone_stub* the_zone_stub = 0;

    std::unordered_map<uint64_t, rpc_cpp::weak_ptr<object_stub>> stubs;
    std::mutex insert_control;


public:

    zone_stub()
    {
        if(the_zone_stub)
        {
            assert(false);
        }
        the_zone_stub = this;
    }

    ~zone_stub()
    {
        the_zone_stub = nullptr;
    }

    template<class T>
    uint64_t encapsulate_outbound_interfaces(rpc_cpp::shared_ptr<T> object)
    {
        return 0;
    }

    static zone_stub* get_zone_stub()
    {
        return the_zone_stub;
    }


};