#include <algorithm>

#include "rpc/service.h"
#include "rpc/stub.h"
#include "rpc/proxy.h"

#ifndef LOG_STR_DEFINED
# ifdef USE_RPC_LOGGING
#  define LOG_STR(str, sz) log_str(str, sz)
   extern "C"
   {
       void log_str(const char* str, size_t sz);
   }
# else
#  define LOG_STR(str, sz)
# endif
#define LOG_STR_DEFINED
#endif
namespace rpc
{
    ////////////////////////////////////////////////////////////////////////////
    // service

    std::atomic<uint64_t> service::zone_count = 0;

    service::~service() 
    {
        LOG("~service",100);
        object_id_generator = 0;
        // to do: assert that there are no more object_stubs in memory
        //assert(check_is_empty());

        stubs.clear();
        wrapped_object_to_stub.clear();
        other_zones.clear();
    }

    bool service::check_is_empty() const
    {
        bool success = true;
        for(auto item : stubs)
        {
            auto stub =  item.second.lock();
            if(!stub)
            {
                auto message = std::string("service ") + std::to_string(get_zone_id()) + std::string(", object stub ") + std::to_string(item.first) + std::string(" has been released but not deregisted in the service suspected unclean shutdown");
                LOG_STR(message.c_str(), message.size());
            }
            else
            {
                auto message = std::string("service ") + std::to_string(get_zone_id()) + std::string(", object stub ") + std::to_string(item.first) + std::string(" has not been released, there is a strong pointer maintaining a positive reference count suspected unclean shutdown");
                LOG_STR(message.c_str(), message.size());
            }
            success = false;
        }
        for(auto item : wrapped_object_to_stub)
        {
            auto stub =  item.second.lock();
            if(!stub)
            {
                auto message = std::string("service ") + std::to_string(get_zone_id()) + std::string(", wrapped_object has been released but not deregisted in the service suspected unclean shutdown");
                LOG_STR(message.c_str(), message.size());
            }
            else
            {
                auto message = std::string("service ") + std::to_string(get_zone_id()) + std::string(", wrapped_object ") + std::to_string(stub->get_id()) + std::string(" has not been deregisted in the service suspected unclean shutdown");
                LOG_STR(message.c_str(), message.size());
            }
            success = false;
        }

        for(auto item : other_zones)
        {
            auto svcproxy =  item.second.lock();
            if(!svcproxy)
            {
                auto message = std::string("service ") + std::to_string(get_zone_id()) + std::string(", proxy ") + std::to_string(item.first) + std::string(", has been released but not deregisted in the service");
                LOG_STR(message.c_str(), message.size());
            }
            else
            {
                auto message = std::string("service ") + std::to_string(get_zone_id()) 
                + std::string(", proxy ") + std::to_string(svcproxy->get_operating_zone_id())
                + std::string(", cloned from ") + std::to_string(svcproxy->get_cloned_from_zone_id()) 
                + std::string(" has not been released in the service suspected unclean shutdown");
                LOG_STR(message.c_str(), message.size());

                for(auto proxy : svcproxy->get_proxies())
                {
                    auto op = proxy.second.lock();
                    if(op)
                    {
                        auto message = std::string("has object_proxy ") + std::to_string(op->get_object_id());
                        LOG_STR(message.c_str(), message.size());
                    }
                    else
                    {
                        auto message = std::string("has null object_proxy");
                        LOG_STR(message.c_str(), message.size());
                    }
                }
            }
            success = false;
        }
        return success;
    }

    int service::send(uint64_t originating_zone_id, uint64_t zone_id, uint64_t object_id, uint64_t interface_id, uint64_t method_id, size_t in_size_,
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
            return other_zone->send(get_zone_id(), zone_id, object_id, interface_id, method_id, in_size_, in_buf_, out_buf_);
        }
        else
        {
            rpc::weak_ptr<object_stub> weak_stub = get_object(object_id);
            auto stub = weak_stub.lock();
            if (stub == nullptr)
            {
                return rpc::error::INVALID_DATA();
            }
            return stub->call(originating_zone_id, interface_id, method_id, in_size_, in_buf_, out_buf_);
        }
    }

    //this is a key function that returns an interface descriptor
    //for wrapping an implementation to a local object inside a stub where needed
    //or if the interface is a proxy to add ref it
    interface_descriptor service::get_proxy_stub_descriptor(uint64_t originating_zone_id, rpc::casting_interface* iface,
                                        std::function<rpc::shared_ptr<i_interface_stub>(rpc::shared_ptr<object_stub>)> fn,
                                        bool add_ref,
                                        rpc::shared_ptr<object_stub>& stub)
    {
        proxy_base* proxy_base = nullptr;
        if(originating_zone_id)//a proxy with no zone id makes no sense
        {
            proxy_base = iface->query_proxy_base();
        }
        if(proxy_base)
        {
            auto object_proxy = proxy_base->get_object_proxy();
            auto object_zone_id = object_proxy->get_zone_id();
            auto object_id = object_proxy->get_object_id();
            if(add_ref && originating_zone_id != object_zone_id)
            {
                auto zone_proxy = object_proxy->get_service_proxy();
                auto cloned_from_zone_id = zone_proxy->get_cloned_from_zone_id();
                zone_proxy->add_ref(object_zone_id, object_id, originating_zone_id == cloned_from_zone_id);
            }
            return {object_id, object_zone_id};
        }

        std::lock_guard g(insert_control);
        auto* pointer = iface->get_address();
        {
            //find the stub by its address
            auto item = wrapped_object_to_stub.find(pointer);
            if (item != wrapped_object_to_stub.end())
            {
                stub = item->second.lock();
                stub->add_ref();
                return {stub->get_id(), stub->get_zone().get_zone_id()};
            }
        }
        //else create a stub
        auto id = generate_new_object_id();
        stub = rpc::shared_ptr<object_stub>(new object_stub(id, *this));
        rpc::shared_ptr<i_interface_stub> interface_stub = fn(stub);
        stub->add_interface(interface_stub);
        wrapped_object_to_stub[pointer] = stub;
        stubs[id] = stub;
        stub->on_added_to_zone(stub);
        stub->add_ref();
        return {id, get_zone_id()};
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

    uint64_t service::add_ref(uint64_t zone_id, uint64_t object_id, bool out_param)
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
                return std::numeric_limits<uint64_t>::max();
            }
            return other_zone->add_ref(zone_id, object_id, out_param);
        }
        else
        {
            rpc::weak_ptr<object_stub> weak_stub = get_object(object_id);
            auto stub = weak_stub.lock();
            if (!stub)
                return std::numeric_limits<uint64_t>::max();
            return stub->add_ref();
        }
    }

    void service::release_local_stub(const rpc::shared_ptr<rpc::object_stub>& stub)
    {
        std::lock_guard l(insert_control);
        uint64_t count = stub->release();
        if(!count)
        {
            {
                stubs.erase(stub->get_id());
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
    }

    uint64_t service::release(uint64_t zone_id, uint64_t object_id)
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
                return std::numeric_limits<uint64_t>::max();
            }
            return other_zone->release(zone_id, object_id);
        }
        else
        {

            bool reset_stub = false;
            rpc::shared_ptr<rpc::object_stub> stub;
            uint64_t count = 0;
            //these scope brackets are needed as otherwise there will be a recursive lock on a mutex in rare cases when the stub is reset
            {
                std::lock_guard l(insert_control);
                auto item = stubs.find(object_id);
                if (item == stubs.end())
                {
                    return std::numeric_limits<uint64_t>::max();
                }

                stub = item->second.lock();

                if (!stub)
                    return std::numeric_limits<uint64_t>::max();
                count = stub->release();
                if(!count)
                {
                    {
                        stubs.erase(item);
                    }
                    {
                        auto* pointer = stub->get_castable_interface()->get_address();
                        auto it = wrapped_object_to_stub.find(pointer);
                        if(it != wrapped_object_to_stub.end())
                        {
                            wrapped_object_to_stub.erase(it);
                        }
                        else
                        {
                            assert(false);
                            return std::numeric_limits<uint64_t>::max();
                        }
                    }
                    stub->reset();        
                    reset_stub = true;
                }
            }
            
            if(reset_stub)
                stub->reset();        

            return count;
        }
    }
    

    void service::inner_add_zone_proxy(const rpc::shared_ptr<service_proxy>& service_proxy)
    {
        assert(service_proxy->get_zone_id() != zone_id_);
        assert(other_zones.find(service_proxy->get_zone_id()) == other_zones.end());
        other_zones[service_proxy->get_zone_id()] = service_proxy;
    }

    void service::add_zone_proxy(const rpc::shared_ptr<service_proxy>& service_proxy)
    {
        assert(service_proxy->get_zone_id() != zone_id_);
        std::lock_guard g(insert_control);
        inner_add_zone_proxy(service_proxy);
    }

    rpc::shared_ptr<service_proxy> service::get_zone_proxy(uint64_t originating_zone_id, uint64_t zone_id, bool& new_proxy_added)
    {
        new_proxy_added = false;
        std::lock_guard g(insert_control);

        //find if we have one
        auto item = other_zones.find(zone_id);
        if (item != other_zones.end())
            return item->second.lock();

        //if not perhaps we can make one from the proxy of the originating call    
        if(originating_zone_id == 0)
            return nullptr;
        item = other_zones.find(originating_zone_id);
        if (item == other_zones.end())
            return nullptr;
        auto originating_proxy = item->second.lock();
        if(!originating_proxy)
            return nullptr;

        auto proxy = originating_proxy->clone_for_zone(zone_id);
        new_proxy_added = true;
        return proxy;
    }

    void service::remove_zone_proxy(uint64_t zone_id)
    {
        std::lock_guard g(insert_control);
        auto item = other_zones.find(zone_id);
        if (item == other_zones.end())
        {
            assert(false);
        }
        else
        {
            other_zones.erase(item);
        }
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

    void child_service::set_parent(const rpc::shared_ptr<rpc::service_proxy>& parent_service, bool child_does_not_use_parents_interface)
    {
        if(parent_service_ && child_does_not_use_parents_interface_)
            parent_service_->release_external_ref();
        parent_service_ = parent_service;
        child_does_not_use_parents_interface_ = child_does_not_use_parents_interface;
    }

    child_service::~child_service()
    {
        if(parent_service_)
        {
        #ifdef _DEBUG
            parent_service_->set_operating_zone_service_released();
        #endif
            remove_zone_proxy(parent_service_->get_zone_id());
            set_parent(nullptr, false);
        }

        // clean up the zone root pointer
        if (root_stub_)
        {
            auto stub = root_stub_->get_object_stub().lock();
            if(stub)
                release(zone_id_, stub->get_id());
            root_stub_.reset();
        }        
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

    rpc::shared_ptr<service_proxy> child_service::get_zone_proxy(uint64_t originating_zone_id, uint64_t zone_id, bool& new_proxy_added)
    {
        auto proxy = service::get_zone_proxy(originating_zone_id, zone_id, new_proxy_added);
        if(proxy)
            return proxy;

        {
            std::lock_guard g(insert_control);
            proxy = parent_service_->clone_for_zone(zone_id);
            new_proxy_added = true;
            return proxy;
        }
    }
}
