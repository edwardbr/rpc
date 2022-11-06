#include "rpc/stub.h"

namespace rpc
{
    object_stub::~object_stub()
    {
        LOG("~object_stub",100);
    }

    rpc::shared_ptr<rpc::casting_interface> object_stub::get_castable_interface() const
    {
        assert(!stub_map.empty());
        auto& iface = stub_map.begin()->second;
        return iface->get_castable_interface();
    }

    void object_stub::add_interface(const rpc::shared_ptr<i_interface_stub>& iface)
    {
        stub_map[iface->get_interface_id()] = iface;
    }

    rpc::shared_ptr<i_interface_stub> object_stub::get_interface(uint64_t interface_id)
    {
        auto res = stub_map.find(interface_id);
        if(res == stub_map.end())
            return nullptr;
        return res->second;
    }

    int object_stub::call(uint64_t originating_zone_id, uint64_t interface_id, uint64_t method_id, size_t in_size_, const char* in_buf_,
                                 std::vector<char>& out_buf_)
    {
        rpc::shared_ptr<i_interface_stub> stub;
        {
            auto item = stub_map.find(interface_id);
            if (item != stub_map.end())
            {
                stub = item->second;
            }
        }
        if(stub)
        {
            return stub->call(originating_zone_id, method_id, in_size_, in_buf_, out_buf_);
        }        
        return rpc::error::INVALID_DATA();
    }

    int object_stub::try_cast(uint64_t interface_id)
    {
        int ret = rpc::error::OK();
        auto item = stub_map.find(interface_id);
        if (item == stub_map.end())
        {
            rpc::shared_ptr<i_interface_stub> new_stub;
            rpc::shared_ptr<i_interface_stub> stub = stub_map.begin()->second;
            ret = stub->cast(interface_id, new_stub);
            if (ret == rpc::error::OK())
            {
                add_interface(new_stub);
            }
        }
        return ret;
    }

    uint64_t object_stub::add_ref()
    {
        uint64_t ret = reference_count++;
        return ret;
    }

    uint64_t object_stub::release()
    {
        uint64_t count = reference_count--;
        return count;
    }

}