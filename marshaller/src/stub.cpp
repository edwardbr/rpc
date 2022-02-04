#include "marshaller/stub.h"

/*object_stub::object_stub(rpc_cpp::shared_ptr<i_interface_stub> initial_interface)
{
    //proxy_map[initial_interface->get_interface_id()] = initial_interface;
}*/

void* object_stub::get_pointer()
{
    assert(!proxy_map.empty());
    return proxy_map.begin()->second->get_pointer();
}

void object_stub::add_interface(rpc_cpp::shared_ptr<i_interface_stub> iface)
{
    std::lock_guard l(insert_control);
    proxy_map[iface->get_interface_id()] = iface;
}

error_code object_stub::call(uint64_t interface_id, uint64_t method_id, size_t in_size_, const char* in_buf_,
                             size_t out_size_, char* out_buf_)
{
    error_code ret = -1;
    std::lock_guard l(insert_control);
    auto item = proxy_map.find(interface_id);
    if (item != proxy_map.end())
    {
        ret = item->second->call(method_id, in_size_, in_buf_, out_size_, out_buf_);
    }
    return ret;
}
error_code object_stub::try_cast(uint64_t interface_id)
{
    error_code ret = 0;
    std::lock_guard l(insert_control);
    auto item = proxy_map.find(interface_id);
    if (item == proxy_map.end())
    {
        rpc_cpp::shared_ptr<i_interface_stub> new_stub;
        ret = proxy_map.begin()->second->cast(interface_id, new_stub);
        if (!ret)
        {
            proxy_map.emplace(interface_id, std::move(new_stub));
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
        p_this = nullptr;
    }
    return ret;
}

////////////////////////////////////////////////////////////////////////////
// zone_stub

zone_stub::~zone_stub()
{
    shutdown();
}

void zone_stub::shutdown()
{
    // clean up the zone root pointer
    release(zone_id, root_stub->get_target_stub().lock()->get_id());
    root_stub = nullptr;

    // to do: assert that there are no more object_stubs in memory
    assert(check_is_empty());
}

bool zone_stub::check_is_empty()
{
    if (root_stub)
        return false; // already initialised
    if (!stubs.empty())
        return false;
    if (!wrapped_object_to_stub.empty())
        return false;
    return true;
}

uint64_t zone_stub::get_root_object_id()
{
    if (!root_stub)
        return 0;
    auto stub = root_stub->get_target_stub().lock();
    if (!stub)
        return 0;

    return stub->get_id();
}

error_code zone_stub::send(uint64_t object_id, uint64_t interface_id, uint64_t method_id, size_t in_size_,
                           const char* in_buf_, size_t out_size_, char* out_buf_)
{
    error_code ret = -1;

    rpc_cpp::weak_ptr<object_stub> weak_stub = get_object(object_id);
    auto stub = weak_stub.lock();
    if (stub == nullptr)
    {
        return -1;
    }
    ret = stub->call(interface_id, method_id, in_size_, in_buf_, out_size_, out_buf_);
    return ret;
}

uint64_t
zone_stub::add_lookup_stub(void* pointer,
                           std::function<rpc_cpp::shared_ptr<i_interface_stub>(rpc_cpp::shared_ptr<object_stub>)> fn)
{
    std::lock_guard g(insert_control);
    auto item = wrapped_object_to_stub.find(pointer);
    if (item == wrapped_object_to_stub.end())
    {
        auto id = object_id_generator++;
        auto stub = rpc_cpp::shared_ptr<object_stub>(new object_stub(id, *this));
        rpc_cpp::shared_ptr<i_interface_stub> wrapped_interface = fn(stub);
        stub->add_interface(wrapped_interface);
        wrapped_object_to_stub[pointer] = stub;
        stubs[id] = stub;
        stub->on_added_to_zone(stub);
        return id;
    }
    return item->second.lock()->get_id();
}

error_code zone_stub::add_object(void* pointer, rpc_cpp::shared_ptr<object_stub> stub)
{
    std::lock_guard g(insert_control);
    assert(wrapped_object_to_stub.find(pointer) == wrapped_object_to_stub.end());
    assert(stubs.find(stub->get_id()) == stubs.end());
    wrapped_object_to_stub[pointer] = stub;
    stubs[stub->get_id()] = stub;
    stub->on_added_to_zone(stub);
    return 0;
}

rpc_cpp::weak_ptr<object_stub> zone_stub::get_object(uint64_t object_id)
{
    std::lock_guard l(insert_control);
    auto item = stubs.find(object_id);
    if (item == stubs.end())
    {
        return rpc_cpp::weak_ptr<object_stub>();
    }

    return item->second;
}
error_code zone_stub::try_cast(uint64_t zone_id, uint64_t object_id, uint64_t interface_id)
{
    error_code ret = -1;
    rpc_cpp::weak_ptr<object_stub> weak_stub = get_object(object_id);
    auto stub = weak_stub.lock();
    if (!stub)
        return -1;
    return stub->try_cast(interface_id);
}

uint64_t zone_stub::add_ref(uint64_t zone_id, uint64_t object_id)
{
    error_code ret = -1;
    rpc_cpp::weak_ptr<object_stub> weak_stub = get_object(object_id);
    auto stub = weak_stub.lock();
    if (!stub)
        return -1;
    return stub->add_ref();
}

uint64_t zone_stub::release(uint64_t zone_id, uint64_t object_id)
{
    rpc_cpp::weak_ptr<object_stub> weak_stub = get_object(object_id);
    auto stub = weak_stub.lock();
    if (!stub)
        return -1;
    uint64_t ret = stub->release([&](){
        std::lock_guard l(insert_control);
        stubs.erase(stub->get_id());
        wrapped_object_to_stub.erase(stub->get_pointer());
    });

    return ret;
}
