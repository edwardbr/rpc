#include "marshaller/stub.h"

namespace rpc
{
    void* object_stub::get_pointer()
    {
        assert(!stub_map.empty());
        return stub_map.begin()->second->get_pointer();
    }

    void object_stub::add_interface(rpc::shared_ptr<i_interface_stub> iface)
    {
        std::lock_guard l(insert_control);
        stub_map[iface->get_interface_id()] = iface;
    }

    error_code object_stub::call(uint64_t interface_id, uint64_t method_id, size_t in_size_, const char* in_buf_,
                                 std::vector<char>& out_buf_)
    {
        error_code ret = -1;
        std::lock_guard l(insert_control);
        auto item = stub_map.find(interface_id);
        if (item != stub_map.end())
        {
            ret = item->second->call(method_id, in_size_, in_buf_, out_buf_);
        }
        return ret;
    }
    error_code object_stub::try_cast(uint64_t interface_id)
    {
        error_code ret = 0;
        std::lock_guard l(insert_control);
        auto item = stub_map.find(interface_id);
        if (item == stub_map.end())
        {
            rpc::shared_ptr<i_interface_stub> new_stub;
            rpc::shared_ptr<i_interface_stub> stub = stub_map.begin()->second;
            ret = stub->cast(interface_id, new_stub);
            if (!ret)
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
        uint64_t ret = reference_count--;
        if (!ret)
        {
            on_delete();
            p_this.reset();
        }
        return ret;
    }

}