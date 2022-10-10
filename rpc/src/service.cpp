#include <algorithm>

#include "rpc/service.h"
#include "rpc/stub.h"
#include "rpc/proxy.h"

namespace rpc
{
    ////////////////////////////////////////////////////////////////////////////
    // service

    std::atomic<uint64_t> service::zone_count;

    service::~service() 
    {
        LOG("~service",100);
        cleanup();
    }

    void service::cleanup()
    {
        object_id_generator = 0;
        // to do: assert that there are no more object_stubs in memory
        assert(check_is_empty());

        std::for_each(stubs.begin(), stubs.end(), [](auto& item){
            auto stub = item.second.lock();
            assert(!stub);
        });
        stubs.clear();
        wrapped_object_to_stub.clear();
        other_zones.clear();
    }

    bool service::check_is_empty() const
    {
        if (!stubs.empty())
            return false;
        if (!wrapped_object_to_stub.empty())
            return false;
        return true;
    }

    int service::send(uint64_t zone_id, uint64_t object_id, uint64_t interface_id, uint64_t method_id, size_t in_size_,
                             const char* in_buf_, std::vector<char>& out_buf_)
    {
        if(zone_id != get_zone_id())
        {
            rpc::shared_ptr<service_proxy> other_zone;
            {
                std::lock_guard g(insert_control);
                auto found = other_zones.find(zone_id);
                if(found != other_zones.end())
                {
                    other_zone = found->second.lock();
                }
            }
            if(!other_zone)
            {
                return rpc::error::ZONE_NOT_SUPPORTED();
            }
            return other_zone->send(zone_id, object_id, interface_id, method_id, in_size_, in_buf_, out_buf_);
        }
        else
        {
            rpc::weak_ptr<object_stub> weak_stub = get_object(object_id);
            auto stub = weak_stub.lock();
            if (stub == nullptr)
            {
                return rpc::error::INVALID_DATA();
            }
            return stub->call(interface_id, method_id, in_size_, in_buf_, out_buf_);
        }
    }

    encapsulated_interface service::add_lookup_stub(rpc::casting_interface* iface,
                                      std::function<rpc::shared_ptr<i_interface_stub>(rpc::shared_ptr<object_stub>)> fn)
    {
        std::lock_guard g(insert_control);
        auto proxy_base = iface->query_proxy_base();
        if(proxy_base)
        {
            auto object_proxy = proxy_base->get_object_proxy();
            return {object_proxy->get_object_id(), object_proxy->get_zone_id()};
        }

        auto* pointer = iface->get_address();
        auto item = wrapped_object_to_stub.find(pointer);
        if (item == wrapped_object_to_stub.end())
        {
            auto id = generate_new_object_id();
            auto stub = rpc::shared_ptr<object_stub>(new object_stub(id, *this));
            rpc::shared_ptr<i_interface_stub> interface_stub = fn(stub);
            stub->add_interface(interface_stub);
            wrapped_object_to_stub[pointer] = stub;
            stubs[id] = stub;
            stub->on_added_to_zone(stub);
            return {id, get_zone_id()};
        }
        auto obj = item->second.lock();
        return {obj->get_id(), obj->get_zone().get_zone_id()};
    }

    int service::add_object(const rpc::shared_ptr<object_stub>& stub)
    {
        std::lock_guard g(insert_control);
        auto* pointer = stub->get_castable_interface()->get_address();
        assert(wrapped_object_to_stub.find(pointer) == wrapped_object_to_stub.end());
        auto stub_id = stub->get_id();
        assert(stubs.find(stub_id) == stubs.end());
        wrapped_object_to_stub[pointer] = stub;
        stubs[stub_id] = stub;
        stub->on_added_to_zone(stub);
        return error::OK();
    }

    rpc::weak_ptr<object_stub> service::get_object(uint64_t object_id) const
    {
        std::lock_guard l(insert_control);
        auto item = stubs.find(object_id);
        if (item == stubs.end())
        {
            return rpc::weak_ptr<object_stub>();
        }

        return item->second;
    }
    int service::try_cast(uint64_t zone_id, uint64_t object_id, uint64_t interface_id)
    {
        if(zone_id != get_zone_id())
        {
            rpc::shared_ptr<service_proxy> other_zone;
            {
                std::lock_guard g(insert_control);
                auto found = other_zones.find(zone_id);
                if(found != other_zones.end())
                {
                    other_zone = found->second.lock();
                }
            }
            if(!other_zone)
            {
                return rpc::error::ZONE_NOT_SUPPORTED();
            }
            return other_zone->try_cast(zone_id, object_id, interface_id);
        }
        else
        {
            rpc::weak_ptr<object_stub> weak_stub = get_object(object_id);
            auto stub = weak_stub.lock();
            if (!stub)
                return error::INVALID_DATA();
            return stub->try_cast(interface_id);
        }
    }

    uint64_t service::add_ref(uint64_t zone_id, uint64_t object_id)
    {
        rpc::weak_ptr<object_stub> weak_stub = get_object(object_id);
        auto stub = weak_stub.lock();
        if (!stub)
            return -1;
        return stub->add_ref();
    }

    uint64_t service::release(uint64_t zone_id, uint64_t object_id)
    {
        std::lock_guard l(insert_control);
        auto item = stubs.find(object_id);
        if (item == stubs.end())
        {
            return -1;
        }

        auto stub = item->second.lock();

        if (!stub)
            return -1;
        uint64_t count = stub->release();
        if(!count)
        {
            {
                auto it = stubs.find(stub->get_id());
                if(it != stubs.end())
                    stubs.erase(it);
                else
                    assert(false);
            }
            {
                auto* pointer = stub->get_castable_interface()->get_address();
                auto it = wrapped_object_to_stub.find(pointer);
                if(it != wrapped_object_to_stub.end())
                {
                    wrapped_object_to_stub.erase(it);
                }
                else
                    assert(false);
            }
            stub->reset();        
        }

        return count;
    }

    void service::add_zone_proxy(const rpc::shared_ptr<service_proxy>& service_proxy)
    {
        assert(service_proxy->get_zone_id() != zone_id_);
        std::lock_guard g(insert_control);
        other_zones[service_proxy->get_zone_id()] = service_proxy;
    }
    /*void service::add_zone_proxy(const rpc::shared_ptr<service_proxy>& service_proxy, uint64_t zone_id)
    {
        assert(zone_id != zone_id_);
        std::lock_guard g(insert_control);
        other_zones[zone_id] = service_proxy;
    }*/
    rpc::shared_ptr<service_proxy> service::get_zone_proxy(uint64_t zone_id)
    {
        std::lock_guard g(insert_control);
        auto item = other_zones.find(zone_id);
        if (item != other_zones.end())
            return item->second.lock();

        return nullptr;
    }
    void service::remove_zone_proxy(uint64_t zone_id)
    {
        std::lock_guard g(insert_control);
        other_zones.erase(zone_id);
    }

    rpc::shared_ptr<casting_interface> service::get_castable_interface(uint64_t object_id, uint64_t interface_id)
    {
        auto ob = get_object(object_id).lock();
        if(!ob)
            return nullptr;
        auto interface_stub = ob->get_interface(interface_id);
        if(!interface_stub)
            return nullptr;
        return interface_stub->get_castable_interface();
    }

    child_service::~child_service()
    {
        if(parent_service_)
        {
            remove_zone_proxy(parent_service_->get_zone_id());
            parent_service_ = nullptr;
        }
        cleanup();
    }

    void child_service::cleanup()
    {
        // clean up the zone root pointer
        if (root_stub_)
        {
            auto stub = root_stub_->get_object_stub().lock();
            if(stub)
                release(zone_id_, stub->get_id());
            root_stub_.reset();
        }

        service::cleanup();
    }

    bool child_service::check_is_empty() const
    {
        if (root_stub_)
            return false; // already initialised
        return service::check_is_empty();
    }

    uint64_t child_service::get_root_object_id() const
    {
        if (!root_stub_)
            return 0;
        auto stub = root_stub_->get_object_stub().lock();
        if (!stub)
            return 0;

        return stub->get_id();
    }

    rpc::shared_ptr<service_proxy> child_service::get_zone_proxy(uint64_t zone_id)
    {
        auto proxy = service::get_zone_proxy(zone_id);
        if(!proxy)
        {
            proxy = parent_service_->clone_for_zone(zone_id);
            add_zone_proxy(proxy);
        }
        return proxy;
    }
}
