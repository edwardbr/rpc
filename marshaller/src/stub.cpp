#include "marshaller/stub.h"

namespace rpc
{
    object_stub::~object_stub()
    {
        LOG("~object_stub",100);
    }

    void* object_stub::get_pointer() const
    {
        assert(!stub_map.empty());
        return stub_map.begin()->second->get_pointer();
    }

    void object_stub::add_interface(const rpc::shared_ptr<i_interface_stub>& iface)
    {
        std::lock_guard l(insert_control);
        stub_map[iface->get_interface_id()] = iface;
    }

    int object_stub::call(uint64_t interface_id, uint64_t method_id, size_t in_size_, const char* in_buf_,
                                 std::vector<char>& out_buf_)
    {
        rpc::shared_ptr<i_interface_stub> stub;
        {
            std::lock_guard l(insert_control);
            auto item = stub_map.find(interface_id);
            if (item != stub_map.end())
            {
                stub = item->second;
            }
        }
        if(stub)
        {
            return stub->call(method_id, in_size_, in_buf_, out_buf_);
        }        
        return rpc::error::INVALID_DATA();
    }

    int object_stub::try_cast(uint64_t interface_id)
    {
        int ret = rpc::error::OK();
        std::lock_guard l(insert_control);
        auto item = stub_map.find(interface_id);
        if (item == stub_map.end())
        {
            rpc::shared_ptr<i_interface_stub> new_stub;
            rpc::shared_ptr<i_interface_stub> stub = stub_map.begin()->second;
            ret = stub->cast(interface_id, new_stub);
            if (ret == rpc::error::OK())
            {
                stub_map.emplace(interface_id, std::move(new_stub));
            }
        }
        return ret;
    }

    uint64_t object_stub::add_ref()
    {
        std::lock_guard l(insert_control);
        uint64_t ret = reference_count++;
        return ret;
    }

    uint64_t object_stub::release(std::function<void()> on_delete)
    {
        std::lock_guard l(insert_control);
        uint64_t count = reference_count--;
        if (count == 0)
        {
            on_delete();
            p_this.reset();
        }
        return count;
    }

}