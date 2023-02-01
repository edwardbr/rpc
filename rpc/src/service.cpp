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
        bool is_empty = check_is_empty();
        assert(is_empty);

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
                auto message = std::string("stub zone ") + std::to_string(get_zone_id()) + std::string(", object stub ") + std::to_string(item.first) + std::string(" has been released but not deregisted in the service suspected unclean shutdown");
                LOG_STR(message.c_str(), message.size());
            }
            else
            {
                auto message = std::string("stub zone ") + std::to_string(get_zone_id()) + std::string(", object stub ") + std::to_string(item.first) + std::string(" has not been released, there is a strong pointer maintaining a positive reference count suspected unclean shutdown");
                LOG_STR(message.c_str(), message.size());
            }
            success = false;
        }
        for(auto item : wrapped_object_to_stub)
        {
            auto stub =  item.second.lock();
            if(!stub)
            {
                auto message = std::string("wrapped stub zone ") + std::to_string(get_zone_id()) + std::string(", wrapped_object has been released but not deregisted in the service suspected unclean shutdown");
                LOG_STR(message.c_str(), message.size());
            }
            else
            {
                auto message = std::string("wrapped stub zone ") + std::to_string(get_zone_id()) + std::string(", wrapped_object ") + std::to_string(stub->get_id()) + std::string(" has not been deregisted in the service suspected unclean shutdown");
                LOG_STR(message.c_str(), message.size());
            }
            success = false;
        }

        for(auto item : other_zones)
        {
            auto svcproxy =  item.second.lock();
            if(!svcproxy)
            {
                auto message = std::string("service proxy zone ") + std::to_string(get_zone_id()) + std::string(", proxy ") + std::to_string(item.first.dest.id) + std::string(", has been released but not deregisted in the service");
                LOG_STR(message.c_str(), message.size());
            }
            else
            {
                auto message = std::string("service proxy zone ") + std::to_string(get_zone_id()) 
                + std::string(", destination_zone ") + std::to_string(svcproxy->get_destination_zone_id())
                + std::string(", destination_channel_zone ") + std::to_string(svcproxy->get_destination_channel_zone_id()) 
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

    int service::send(caller_channel_zone caller_channel_zone_id, caller_zone caller_zone_id, destination_zone destination_zone_id, object object_id, interface_ordinal interface_id, method method_id, size_t in_size_,
                             const char* in_buf_, std::vector<char>& out_buf_)
    {
        if(destination_zone_id != get_zone_id().as_destination())
        {
            rpc::shared_ptr<service_proxy> other_zone;
            {
                std::lock_guard g(insert_control);
                auto found = other_zones.find({destination_zone_id, caller_zone_id});
                if(found != other_zones.end())
                {
                    other_zone = found->second.lock();
                }
            }
            if(!other_zone)
            {
                assert(false);
                return rpc::error::ZONE_NOT_SUPPORTED();
            }
            return other_zone->send(get_zone_id().as_caller_channel(), caller_zone_id, destination_zone_id, object_id, interface_id, method_id, in_size_, in_buf_, out_buf_);
        }
        else
        {
            rpc::weak_ptr<object_stub> weak_stub = get_object(object_id);
            auto stub = weak_stub.lock();
            if (stub == nullptr)
            {
                return rpc::error::INVALID_DATA();
            }
            return stub->call(caller_channel_zone_id, caller_zone_id, interface_id, method_id, in_size_, in_buf_, out_buf_);
        }
    }

    interface_descriptor service::get_proxy_descriptor(caller_channel_zone caller_channel_zone_id, caller_zone caller_zone_id, rpc::proxy_base* base)
    {
        auto object_proxy = base->get_object_proxy();
        auto destination_zone = object_proxy->get_service_proxy();
        auto destination_zone_id = destination_zone->get_destination_zone_id();
        auto destination_channel_zone_id = destination_zone->get_destination_channel_zone_id();
        auto object_id = object_proxy->get_object_id();
        bool needs_external_add_ref = true;
        //check to see if the source and destination come from the same direction
        if(    caller_zone_id.is_set() 
            && destination_zone_id.is_set() 
            && caller_zone_id != destination_zone_id.as_caller())
        {
            if(caller_zone_id.is_set() 
                && destination_channel_zone_id.is_set() 
                && caller_zone_id.as_destination_channel() == destination_channel_zone_id)
                needs_external_add_ref = false;
            else if(caller_channel_zone_id.is_set() 
                && destination_zone_id.is_set() 
                && caller_channel_zone_id.as_destination() == destination_zone_id)
                needs_external_add_ref = false;
            else if(caller_channel_zone_id.is_set() 
                && destination_channel_zone_id.is_set() 
                && caller_channel_zone_id.as_destination_channel() == destination_channel_zone_id)
                needs_external_add_ref = false;
    
            rpc::shared_ptr<service_proxy> other_zone = destination_zone;
            if(destination_zone->get_caller_zone_id() != caller_zone_id)
            {
                auto found = other_zones.find({destination_zone_id, caller_zone_id});//we dont need to get caller id for this
                if(found != other_zones.end())
                {
                    other_zone = found->second.lock();
                }
                else
                {
                    other_zone = destination_zone->clone_for_zone(destination_zone_id, caller_zone_id, caller_channel_zone_id);
                    needs_external_add_ref = false;
                }
            }
            other_zone->add_ref(destination_zone_id, object_id, caller_zone_id, needs_external_add_ref);
        }
 
        return {object_id, destination_zone_id};
    }    

    //this is a key function that returns an interface descriptor
    //for wrapping an implementation to a local object inside a stub where needed
    //or if the interface is a proxy to add ref it
    interface_descriptor service::get_proxy_stub_descriptor(caller_channel_zone caller_channel_zone_id, caller_zone caller_zone_id, rpc::casting_interface* iface,
                                        std::function<rpc::shared_ptr<i_interface_stub>(rpc::shared_ptr<object_stub>)> fn,
                                        bool add_ref,
                                        rpc::shared_ptr<object_stub>& stub)
    {
        if(add_ref)
        {
            proxy_base* proxy_base = nullptr;
            if(caller_channel_zone_id.is_set() || caller_zone_id.is_set())
            {
                proxy_base = iface->query_proxy_base();
            }
            if(proxy_base)
            {
                return get_proxy_descriptor(caller_channel_zone_id, caller_zone_id, proxy_base);
            }
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
                return {stub->get_id(), get_zone_id().as_destination()};
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
        return {id, get_zone_id().as_destination()};
    }

    rpc::weak_ptr<object_stub> service::get_object(object object_id) const
    {
        std::lock_guard l(insert_control);
        auto item = stubs.find(object_id);
        if (item == stubs.end())
        {
            return rpc::weak_ptr<object_stub>();
        }

        return item->second;
    }
    int service::try_cast(destination_zone destination_zone_id, object object_id, interface_ordinal interface_id)
    {
        if(destination_zone_id != get_zone_id().as_destination())
        {
            rpc::shared_ptr<service_proxy> other_zone;
            {
                std::lock_guard g(insert_control);
                auto found = other_zones.find({destination_zone_id, get_zone_id().as_caller()});//we dont need to get caller id for this
                if(found != other_zones.end())
                {
                    other_zone = found->second.lock();
                }
            }
            if(!other_zone)
            {
                assert(false);
                return rpc::error::ZONE_NOT_SUPPORTED();
            }
            return other_zone->try_cast(destination_zone_id, object_id, interface_id);
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

    uint64_t service::add_ref(destination_zone destination_zone_id, object object_id, caller_zone caller_zone_id, bool proxy_add_ref)
    {
        if(destination_zone_id != get_zone_id().as_destination())
        {
            rpc::shared_ptr<service_proxy> other_zone;
            bool zone_cloned = false;
            {
                std::lock_guard g(insert_control);
                auto found = other_zones.find({destination_zone_id, caller_zone_id});
                if(found != other_zones.end())
                {
                    other_zone = found->second.lock();
                }
                
                if(!other_zone)
                {
                    found = other_zones.find({destination_zone_id, get_zone_id().as_caller()});
                    if(found != other_zones.end())
                    {
                        auto tmp = found->second.lock();
                        other_zone = tmp->clone_for_zone(destination_zone_id, caller_zone_id, {0});
                        zone_cloned = true;
                    }
                }

                if(!other_zone)
                {
                    auto found = other_zones.lower_bound({destination_zone_id, {0}});
                    if(found != other_zones.end() && found->first.dest == destination_zone_id)
                    {
                        auto tmp = found->second.lock();
                        other_zone = tmp->clone_for_zone(destination_zone_id, caller_zone_id, {});
                        zone_cloned = true;
                    }
                }
            }
            if(!other_zone)
            {
                assert(false);
                return std::numeric_limits<uint64_t>::max();
            }
            return other_zone->add_ref(destination_zone_id, object_id, caller_zone_id, !zone_cloned);
        }
        else
        {
            rpc::weak_ptr<object_stub> weak_stub = get_object(object_id);
            auto stub = weak_stub.lock();
            if (!stub)
            {
                assert(false);
                return std::numeric_limits<uint64_t>::max();
            }
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
                {
                    //if you get here make sure that get_address is defined in the most derived class
                    assert(false);
                }
            }
            stub->reset();        
        }
    }

    uint64_t service::release(destination_zone destination_zone_id, object object_id, caller_zone caller_zone_id)
    {
        if(destination_zone_id != get_zone_id().as_destination())
        {
            rpc::shared_ptr<service_proxy> other_zone;
            {
                std::lock_guard g(insert_control);
                auto found = other_zones.find({destination_zone_id, caller_zone_id});
                if(found != other_zones.end())
                {
                    other_zone = found->second.lock();
                }
            }
            if(!other_zone)
            {
                assert(false);
                return std::numeric_limits<uint64_t>::max();
            }
            return other_zone->release(destination_zone_id, object_id, caller_zone_id);
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
                    assert(false);
                    return std::numeric_limits<uint64_t>::max();
                }

                stub = item->second.lock();

                if (!stub)
                {
                    assert(false);
                    return std::numeric_limits<uint64_t>::max();
                }
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
        auto destination_zone_id = service_proxy->get_destination_zone_id();
        auto caller_zone_id = service_proxy->get_caller_zone_id();
        assert(destination_zone_id != zone_id_.as_destination());
        assert(other_zones.find({destination_zone_id, caller_zone_id}) == other_zones.end());
        other_zones[{destination_zone_id, caller_zone_id}] = service_proxy;
    }

    void service::add_zone_proxy(const rpc::shared_ptr<service_proxy>& service_proxy)
    {
        assert(service_proxy->get_destination_zone_id() != zone_id_.as_destination());
        std::lock_guard g(insert_control);
        inner_add_zone_proxy(service_proxy);
    }

    rpc::shared_ptr<service_proxy> service::get_zone_proxy(
        caller_channel_zone caller_channel_zone_id, 
        caller_zone caller_zone_id, 
        destination_zone destination_zone_id, 
        bool& new_proxy_added)
    {
        new_proxy_added = false;
        std::lock_guard g(insert_control);

        //find if we have one
        auto item = other_zones.find({destination_zone_id, caller_zone_id});
        if (item != other_zones.end())
            return item->second.lock();

        //if not we can make one from the proxy of the calling zone
        if(caller_zone_id.is_set())
        {
            item = other_zones.find({caller_zone_id.as_destination(), get_zone_id().as_caller()});
        }
        //or if not we can make one from the proxy of the calling channel zone
        if (item == other_zones.end())
        {
            if(!caller_channel_zone_id.is_set())
                return nullptr;
            item = other_zones.find({caller_channel_zone_id.as_destination(), get_zone_id().as_caller()});
        }
        if (item == other_zones.end())
            return nullptr;

        auto calling_proxy = item->second.lock();
        if(!calling_proxy)
            return nullptr;

        auto proxy = calling_proxy->clone_for_zone(destination_zone_id, caller_zone_id, caller_channel_zone_id);
        new_proxy_added = true;
        return proxy;
    }

    void service::remove_zone_proxy(destination_zone destination_zone_id, caller_zone caller_zone_id)
    {
        std::lock_guard g(insert_control);
        auto item = other_zones.find({destination_zone_id, caller_zone_id});
        if (item == other_zones.end())
        {
            assert(false);
        }
        else
        {
            other_zones.erase(item);
        }
    }

    rpc::shared_ptr<casting_interface> service::get_castable_interface(object object_id, interface_ordinal interface_id)
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
            remove_zone_proxy(parent_service_->get_destination_zone_id(), get_zone_id().as_caller());
            set_parent(nullptr, false);
        }

        // clean up the zone root pointer
        if (root_stub_)
        {
            auto stub = root_stub_->get_object_stub().lock();
            if(stub)
                release(zone_id_.as_destination(), stub->get_id(), zone_id_.as_caller());
            root_stub_.reset();
        }        
    }

    bool child_service::check_is_empty() const
    {
        if (root_stub_)
            return false; // already initialised
        return service::check_is_empty();
    }

    object child_service::get_root_object_id() const
    {
        if (!root_stub_)
            return {0};
        auto stub = root_stub_->get_object_stub().lock();
        if (!stub)
            return {0};

        return stub->get_id();
    }

    rpc::shared_ptr<service_proxy> child_service::get_zone_proxy(caller_channel_zone caller_channel_zone_id, caller_zone caller_zone_id, destination_zone destination_zone_id, bool& new_proxy_added)
    {
        auto proxy = service::get_zone_proxy(caller_channel_zone_id, caller_zone_id, destination_zone_id, new_proxy_added);
        if(proxy)
            return proxy;

        if(parent_service_)
        {
            std::lock_guard g(insert_control);
            proxy = parent_service_->clone_for_zone(destination_zone_id, caller_zone_id, caller_channel_zone_id);
            new_proxy_added = true;
            return proxy;
        }
        return nullptr;
    }
}
