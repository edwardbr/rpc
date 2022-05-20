#include "marshaller/service.h"
#include "marshaller/stub.h"

namespace rpc
{
    ////////////////////////////////////////////////////////////////////////////
    // service

    service::~service() { cleanup(); }

    void service::cleanup()
    {
        // clean up the zone root pointer
        if (root_stub)
        {
            release(zone_id_, root_stub->get_object_stub().lock()->get_id());
            root_stub.reset();
        }

        object_id_generator = 0;
        // to do: assert that there are no more object_stubs in memory
        assert(check_is_empty());

        stubs.clear();
        wrapped_object_to_stub.clear();
        other_zones.clear();
    }

    bool service::check_is_empty()
    {
        if (root_stub)
            return false; // already initialised
        if (!stubs.empty())
            return false;
        if (!wrapped_object_to_stub.empty())
            return false;
        if (!other_zones.empty())
            return false;
        return true;
    }

    uint64_t service::get_root_object_id()
    {
        if (!root_stub)
            return 0;
        auto stub = root_stub->get_object_stub().lock();
        if (!stub)
            return 0;

        return stub->get_id();
    }

    error_code service::send(uint64_t object_id, uint64_t interface_id, uint64_t method_id, size_t in_size_,
                             const char* in_buf_, std::vector<char>& out_buf_)
    {
        error_code ret = -1;

        rpc::weak_ptr<object_stub> weak_stub = get_object(object_id);
        auto stub = weak_stub.lock();
        if (stub == nullptr)
        {
            return -1;
        }
        ret = stub->call(interface_id, method_id, in_size_, in_buf_, out_buf_);
        return ret;
    }

    uint64_t service::add_lookup_stub(void* pointer,
                                      std::function<rpc::shared_ptr<i_interface_stub>(rpc::shared_ptr<object_stub>)> fn)
    {
        std::lock_guard g(insert_control);
        auto item = wrapped_object_to_stub.find(pointer);
        if (item == wrapped_object_to_stub.end())
        {
            auto id = get_new_object_id();
            auto stub = rpc::shared_ptr<object_stub>(new object_stub(id, *this));
            rpc::shared_ptr<i_interface_stub> wrapped_interface = fn(stub);
            stub->add_interface(wrapped_interface);
            wrapped_object_to_stub[pointer] = stub;
            stubs[id] = stub;
            stub->on_added_to_zone(stub);
            return id;
        }
        return item->second.lock()->get_id();
    }

    error_code service::add_object(void* pointer, const rpc::shared_ptr<object_stub>& stub)
    {
        std::lock_guard g(insert_control);
        assert(wrapped_object_to_stub.find(pointer) == wrapped_object_to_stub.end());
        assert(stubs.find(stub->get_id()) == stubs.end());
        wrapped_object_to_stub[pointer] = stub;
        stubs[stub->get_id()] = stub;
        stub->on_added_to_zone(stub);
        return 0;
    }

    rpc::weak_ptr<object_stub> service::get_object(uint64_t object_id)
    {
        std::lock_guard l(insert_control);
        auto item = stubs.find(object_id);
        if (item == stubs.end())
        {
            return rpc::weak_ptr<object_stub>();
        }

        return item->second;
    }
    error_code service::try_cast(uint64_t zone_id, uint64_t object_id, uint64_t interface_id)
    {
        error_code ret = -1;
        rpc::weak_ptr<object_stub> weak_stub = get_object(object_id);
        auto stub = weak_stub.lock();
        if (!stub)
            return -1;
        return stub->try_cast(interface_id);
    }

    uint64_t service::add_ref(uint64_t zone_id, uint64_t object_id)
    {
        error_code ret = -1;
        rpc::weak_ptr<object_stub> weak_stub = get_object(object_id);
        auto stub = weak_stub.lock();
        if (!stub)
            return -1;
        return stub->add_ref();
    }

    uint64_t service::release(uint64_t zone_id, uint64_t object_id)
    {
        rpc::weak_ptr<object_stub> weak_stub = get_object(object_id);
        auto stub = weak_stub.lock();
        if (!stub)
            return -1;
        uint64_t ret = stub->release([&]() {
            std::lock_guard l(insert_control);
            stubs.erase(stub->get_id());
            wrapped_object_to_stub.erase(stub->get_pointer());
        });

        return ret;
    }

    void service::add_zone(uint64_t zone_id, rpc::weak_ptr<service_proxy> zone)
    {
        std::lock_guard g(insert_control);
        other_zones[zone_id] = zone;
    }
    rpc::weak_ptr<service_proxy> service::get_zone(uint64_t zone_id)
    {
        std::lock_guard g(insert_control);
        auto item = other_zones.find(zone_id);
        if (item != other_zones.end())
            return item->second;

        return rpc::weak_ptr<service_proxy>();
    }
    void service::remove_zone(uint64_t zone_id)
    {
        std::lock_guard g(insert_control);
        other_zones.erase(zone_id);
    }

}