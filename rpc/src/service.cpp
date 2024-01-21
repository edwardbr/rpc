#include <algorithm>

#include <yas/mem_streams.hpp>
#include <yas/std_types.hpp>


#include "rpc/service.h"
#include "rpc/stub.h"
#include "rpc/proxy.h"
#include "rpc/version.h"
#include "rpc/logger.h"

namespace rpc
{
    ////////////////////////////////////////////////////////////////////////////
    // service

    std::atomic<uint64_t> service::zone_id_generator = 0;
    zone service::generate_new_zone_id() 
    { 
        auto count = ++zone_id_generator;
        return {count}; 
    }

    object service::generate_new_object_id() const 
    { 
        auto count = ++object_id_generator;
        return {count}; 
    }

    service::service(zone zone_id, const i_telemetry_service* telemetry_service) : zone_id_(zone_id), telemetry_service_(telemetry_service)
    {
        if(telemetry_service_)
            telemetry_service_->on_service_creation("", zone_id);
#ifdef USE_RPC_LOGGING
        auto message = std::string("new service zone_id ") + std::to_string(zone_id.get_val());
        LOG_STR(message.data(),message.size());
#endif
    }

    service::~service() 
    {
        if(telemetry_service_)
            telemetry_service_->on_service_deletion("", zone_id_);        
#ifdef USE_RPC_LOGGING
        auto message = std::string("~service zone_id ") + std::to_string(zone_id_.get_val());
        LOG_STR(message.data(),message.size());
#endif

        object_id_generator = 0;
        // to do: assert that there are no more object_stubs in memory
        bool is_empty = check_is_empty();
        (void)is_empty;
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
#ifdef USE_RPC_LOGGING
                auto message = std::string("stub zone_id ") + std::to_string(get_zone_id())
                     + std::string(", object stub ") + std::to_string(item.first)
                     + std::string(" has been released but not deregisted in the service suspected unclean shutdown");
                LOG_STR(message.c_str(), message.size());
#endif
            }
            else
            {
#ifdef USE_RPC_LOGGING
                auto message = std::string("stub zone_id ") + std::to_string(get_zone_id()) 
                    + std::string(", object stub ") + std::to_string(item.first) 
                    + std::string(" has not been released, there is a strong pointer maintaining a positive reference count suspected unclean shutdown");
                LOG_STR(message.c_str(), message.size());
#endif
            }
            success = false;
        }
        for(auto item : wrapped_object_to_stub)
        {
            auto stub =  item.second.lock();
            if(!stub)
            {
#ifdef USE_RPC_LOGGING
                auto message = std::string("wrapped stub zone_id ") + std::to_string(get_zone_id()) 
                    + std::string(", wrapped_object has been released but not deregisted in the service suspected unclean shutdown");
                LOG_STR(message.c_str(), message.size());
#endif
            }
            else
            {
#ifdef USE_RPC_LOGGING
                auto message = std::string("wrapped stub zone_id ") + std::to_string(get_zone_id()) 
                    + std::string(", wrapped_object ") + std::to_string(stub->get_id()) 
                    + std::string(" has not been deregisted in the service suspected unclean shutdown");
                LOG_STR(message.c_str(), message.size());
#endif
            }
            success = false;
        }

        for(auto item : other_zones)
        {
            auto svcproxy =  item.second.lock();
            if(!svcproxy)
            {
#ifdef USE_RPC_LOGGING
                auto message = std::string("service proxy zone_id ") + std::to_string(get_zone_id()) 
                    + std::string(", caller_zone_id ") + std::to_string(item.first.source.id) 
                    + std::string(", destination_zone_id ") + std::to_string(item.first.dest.id) 
                    + std::string(", has been released but not deregisted in the service");
                LOG_STR(message.c_str(), message.size());
#endif
            }
            else
            {
#ifdef USE_RPC_LOGGING
                auto message = std::string("service proxy zone_id ") + std::to_string(get_zone_id()) 
                    + std::string(", caller_zone_id ") + std::to_string(item.first.source.id) 
                    + std::string(", destination_zone_id ") + std::to_string(svcproxy->get_destination_zone_id())
                    + std::string(", destination_channel_zone_id ") + std::to_string(svcproxy->get_destination_channel_zone_id()) 
                    + std::string(" has not been released in the service suspected unclean shutdown");
                LOG_STR(message.c_str(), message.size());
#endif

                for(auto proxy : svcproxy->get_proxies())
                {
                    auto op = proxy.second.lock();
                    if(op)
                    {
#ifdef USE_RPC_LOGGING
                        auto message = std::string("has object_proxy ") + std::to_string(op->get_object_id());
                        LOG_STR(message.c_str(), message.size());
#endif
                    }
                    else
                    {
#ifdef USE_RPC_LOGGING
                        auto message = std::string("has null object_proxy");
                        LOG_STR(message.c_str(), message.size());
#endif
                    }
                }
            }
            success = false;
        }
        return success;
    }

    int service::send(
        uint64_t protocol_version, 
        encoding encoding, 
        uint64_t tag, 
        caller_channel_zone caller_channel_zone_id, 
        caller_zone caller_zone_id, 
        destination_zone destination_zone_id, 
        object object_id, interface_ordinal 
        interface_id, 
        method method_id, 
        size_t in_size_,
        const char* in_buf_, 
        std::vector<char>& out_buf_
    )
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
                return rpc::error::ZONE_NOT_FOUND();
            }
            return other_zone->send(
                protocol_version, 
                encoding, 
                tag,
                get_zone_id().as_caller_channel(), 
                caller_zone_id, 
                destination_zone_id, 
                object_id, 
                interface_id, 
                method_id, 
                in_size_, 
                in_buf_, 
                out_buf_);
        }
        else
        {
#ifdef RPC_V2
            if(protocol_version == rpc::VERSION_2)
            ;
            else 
#endif
#ifdef RPC_V1
            if(protocol_version == rpc::VERSION_1)
            ;
            else
#endif
            {
                return rpc::error::INCOMPATIBLE_SERVICE();
            }
            rpc::weak_ptr<object_stub> weak_stub = get_object(object_id);
            auto stub = weak_stub.lock();
            if (stub == nullptr)
            {
                return rpc::error::INVALID_DATA();
            }
            std::for_each(service_loggers.begin(), service_loggers.end(), [&](const std::shared_ptr<service_logger>& logger){
                logger->before_send(caller_zone_id, object_id, interface_id, method_id, in_size_, in_buf_ ? in_buf_ : "");
            });

            auto ret = stub->call(protocol_version, encoding, caller_channel_zone_id, caller_zone_id, interface_id, method_id, in_size_, in_buf_, out_buf_);

            std::for_each(service_loggers.begin(), service_loggers.end(), [&](const std::shared_ptr<service_logger>& logger){
                logger->after_send(caller_zone_id, object_id, interface_id, method_id, ret, out_buf_);
            });

            return ret;
        }
    }

    interface_descriptor service::prepare_out_param(uint64_t protocol_version, caller_channel_zone caller_channel_zone_id, caller_zone caller_zone_id, rpc::proxy_base* base)
    {
        auto object_proxy = base->get_object_proxy();
        auto object_service_proxy = object_proxy->get_service_proxy();
        assert(object_service_proxy->get_zone_id() == zone_id_);
        auto destination_zone_id = object_service_proxy->get_destination_zone_id();
        auto destination_channel_zone_id = object_service_proxy->get_destination_channel_zone_id();
        auto object_id = object_proxy->get_object_id();

        assert(caller_zone_id.is_set());
        assert(destination_zone_id.is_set());

        uint64_t object_channel = caller_channel_zone_id.is_set() ? caller_channel_zone_id.id : caller_zone_id.id;
        assert(object_channel);

        uint64_t destination_channel = destination_channel_zone_id.is_set() ? destination_channel_zone_id.id : destination_zone_id.id;
        assert(destination_channel);

        if(object_channel == destination_channel)
        {
            //caller and destination are in the same channel let them fork where necessary
            //note the caller_channel_zone_id is 0 as both the caller and the destination are in from the same direction so any other value is wrong
            //Dont external_add_ref the local service proxy as we are return to source no channel is required
            if(telemetry_service_)
                telemetry_service_->on_service_add_ref(
                    "", 
                    get_zone_id(), 
                    {0}, 
                    destination_zone_id, 
                    object_id, 
                    {0}, 
                    caller_zone_id, 
                    rpc::add_ref_options::build_caller_route | rpc::add_ref_options::build_destination_route);   
            object_service_proxy->add_ref(
                protocol_version,
                {0}, 
                destination_zone_id, 
                object_id, 
                {0}, 
                caller_zone_id, 
                rpc::add_ref_options::build_caller_route | rpc::add_ref_options::build_destination_route, 
                false);
        }
        else
        {
            rpc::shared_ptr<service_proxy> destination_zone = object_service_proxy;
            rpc::shared_ptr<service_proxy> caller_zone;
            {
                std::lock_guard g(insert_control);
                auto found = other_zones.find({destination_zone_id, caller_zone_id});//we dont need to get caller id for this
                if(found != other_zones.end())
                {
                    destination_zone = found->second.lock();
                }
                else
                {
                    destination_zone = object_service_proxy->clone_for_zone(destination_zone_id, caller_zone_id);
                    other_zones[{destination_zone_id, caller_zone_id}] = destination_zone;
                }
                destination_zone->add_external_ref();
                //and the caller with destination info
                found = other_zones.find({{object_channel}, zone_id_.as_caller()});//we dont need to get caller id for this
                assert(found != other_zones.end());
                caller_zone = found->second.lock();
                assert(caller_zone);
            }

            if(telemetry_service_)
                telemetry_service_->on_service_add_ref(
                    "", 
                    get_zone_id(), 
                    destination_channel_zone_id, 
                    destination_zone_id, 
                    object_id, 
                    get_zone_id().as_caller_channel(), 
                    caller_zone_id, 
                    rpc::add_ref_options::build_destination_route);                

            //the fork is here so we need to add ref the destination normally with caller info
            //note the caller_channel_zone_id is is this zones id as the caller came from a route via this node
            destination_zone->add_ref(
                protocol_version,
                destination_channel_zone_id, 
                destination_zone_id, 
                object_id, 
                get_zone_id().as_caller_channel(), 
                caller_zone_id, 
                rpc::add_ref_options::build_destination_route, 
                false);
            
            
            if(telemetry_service_)
                telemetry_service_->on_service_add_ref(
                    "", 
                    get_zone_id(), 
                    zone_id_.as_destination_channel(), 
                    destination_zone_id, 
                    object_id, 
                    {0}, 
                    caller_zone_id, 
                    rpc::add_ref_options::build_caller_route);                
            
            //note the caller_channel_zone_id is 0 as the caller came from this route 
            caller_zone->add_ref(
                protocol_version,
                zone_id_.as_destination_channel(), 
                destination_zone_id, 
                object_id, 
                {0}, 
                caller_zone_id, 
                rpc::add_ref_options::build_caller_route, 
                false);
        }
 
        return {object_id, destination_zone_id};
    }    

    //this is a key function that returns an interface descriptor
    //for wrapping an implementation to a local object inside a stub where needed
    //or if the interface is a proxy to add ref it
    interface_descriptor service::get_proxy_stub_descriptor(uint64_t protocol_version, caller_channel_zone caller_channel_zone_id, caller_zone caller_zone_id, rpc::casting_interface* iface,
                                        std::function<rpc::shared_ptr<i_interface_stub>(rpc::shared_ptr<object_stub>)> fn,
                                        bool outcall,
                                        rpc::shared_ptr<object_stub>& stub)
    {
        if(outcall)
        {
            proxy_base* proxy_base = nullptr;
            if(caller_channel_zone_id.is_set() || caller_zone_id.is_set())
            {
                proxy_base = iface->query_proxy_base();
            }
            if(proxy_base)
            {
                return prepare_out_param(protocol_version, caller_channel_zone_id, caller_zone_id, proxy_base);
            }
        }
        
        //needed by the out call
        rpc::shared_ptr<service_proxy> caller_zone;

        auto* pointer = iface->get_address();
        {
            std::lock_guard g(insert_control);
            //find the stub by its address
            auto item = wrapped_object_to_stub.find(pointer);
            if (item != wrapped_object_to_stub.end())
            {
                stub = item->second.lock();
                stub->add_ref();
            }
            else
            {
                //else create a stub
                auto id = generate_new_object_id();
                stub = rpc::make_shared<object_stub>(id, *this, pointer, telemetry_service_);
                rpc::shared_ptr<i_interface_stub> interface_stub = fn(stub);
                stub->add_interface(interface_stub);
                wrapped_object_to_stub[pointer] = stub;
                stubs[id] = stub;
                stub->on_added_to_zone(stub);
                stub->add_ref();
            }

            if(outcall)
            {
                uint64_t object_channel = caller_channel_zone_id.is_set() ? caller_channel_zone_id.id : caller_zone_id.id;
                assert(object_channel);
                //and the caller with destination info
                auto found = other_zones.find({{object_channel}, zone_id_.as_caller()});//we dont need to get caller id for this
                if(found != other_zones.end())
                {
                    caller_zone = found->second.lock();
                }
                else
                {
                    //unexpected code path
                    assert(false);
                    //caller_zone = get_parent();
                }
                assert(caller_zone);
            }
        }
        if(outcall)
        {
            if(telemetry_service_)
                telemetry_service_->on_service_add_ref(
                    "", 
                    get_zone_id(), 
                    {0}, 
                    zone_id_.as_destination(), 
                    stub->get_id(), 
                    {0}, 
                    caller_zone_id, 
                    rpc::add_ref_options::build_caller_route);                    
                //note the caller_channel_zone_id is 0 as the caller came from this route 
            caller_zone->add_ref(
                protocol_version,
                {0}, 
                zone_id_.as_destination(), 
                stub->get_id(), 
                {0}, 
                caller_zone_id, 
                rpc::add_ref_options::build_caller_route, 
                false);        
        }        
        return {stub->get_id(), get_zone_id().as_destination()};
    }

    rpc::weak_ptr<object_stub> service::get_object(object object_id) const
    {
        std::lock_guard l(insert_control);
        auto item = stubs.find(object_id);
        if (item == stubs.end())
        {
            //we need a test if we get here
            assert(false);
            return rpc::weak_ptr<object_stub>();
        }

        return item->second;
    }
    int service::try_cast(
        uint64_t protocol_version, 
        destination_zone destination_zone_id, 
        object object_id, 
        interface_ordinal interface_id)
    {
        if(destination_zone_id != get_zone_id().as_destination())
        {
            rpc::shared_ptr<service_proxy> other_zone;
            {
                std::lock_guard g(insert_control);
                auto found = other_zones.lower_bound({destination_zone_id, {0}});
                if(found != other_zones.end() && found->first.dest == destination_zone_id)                
                {
                    other_zone = found->second.lock();
                }
            }
            if(!other_zone)
            {
                assert(false);
                return rpc::error::ZONE_NOT_FOUND();
            }
            if (telemetry_service_)
            {
                telemetry_service_->on_service_try_cast("service", get_zone_id(), destination_zone_id, {0}, object_id, interface_id);
            }
            return other_zone->try_cast(protocol_version, destination_zone_id, object_id, interface_id);
        }
        else
        {
#ifdef RPC_V2
            if(protocol_version == rpc::VERSION_2)
            ;
            else 
#endif
#ifdef RPC_V1
            if(protocol_version == rpc::VERSION_1)
            ;
            else
#endif
            {
                return rpc::error::INCOMPATIBLE_SERVICE();
            }
            rpc::weak_ptr<object_stub> weak_stub = get_object(object_id);
            auto stub = weak_stub.lock();
            if (!stub)
                return error::INVALID_DATA();
            return stub->try_cast(interface_id);
        }
    }

    uint64_t service::add_ref(
        uint64_t protocol_version, 
        destination_channel_zone destination_channel_zone_id, 
        destination_zone destination_zone_id, 
        object object_id, 
        caller_channel_zone caller_channel_zone_id, 
        caller_zone caller_zone_id, 
        add_ref_options build_out_param_channel, 
        bool proxy_add_ref
    )
{
        if(telemetry_service_)
            telemetry_service_->on_service_add_ref("service", zone_id_, destination_channel_zone_id, destination_zone_id, object_id, caller_channel_zone_id, caller_zone_id, build_out_param_channel);    
        
        auto dest_channel = destination_channel_zone_id.is_set() ? destination_channel_zone_id.get_val() : destination_zone_id.get_val();
        auto caller_channel = caller_channel_zone_id.is_set() ? caller_channel_zone_id.id : caller_zone_id.id;

        if(destination_zone_id != get_zone_id().as_destination())
        {
            auto build_channel = !!(build_out_param_channel & add_ref_options::build_destination_route) || !!(build_out_param_channel & add_ref_options::build_caller_route);
            if(dest_channel == caller_channel && build_channel)
            {
                //we are here as we are passing the buck to the zone that knows to either splits or terminates this zone has no refcount issues to deal with
                rpc::shared_ptr<rpc::service_proxy> dest_zone;
                do
                {
                    auto found = other_zones.find({destination_zone_id, caller_zone_id});
                    if(found != other_zones.end())
                    {
                        //untested section
                        assert(false);
                        // dest_zone = found->second.lock();
                        // dest_zone->add_external_ref();//update the local ref count the object refcount is done further down the stack
                        break;
                    }

                    found = other_zones.lower_bound({{dest_channel}, {0}});
                    if(found != other_zones.end() && found->first.dest.get_val() == dest_channel)
                    {
                        dest_zone = found->second.lock();
                        break;
                    }

                    dest_zone = get_parent();                        
                    assert(false);

                } while(false);
                
                if(telemetry_service_)
                    telemetry_service_->on_service_add_ref(
                        "", 
                        get_zone_id(), 
                        {0}, 
                        destination_zone_id, 
                        object_id, 
                        {0}, 
                        caller_zone_id, 
                        build_out_param_channel);                 
                return dest_zone->add_ref(
                    protocol_version, 
                    {0}, 
                    destination_zone_id, 
                    object_id, 
                    {0}, 
                    caller_zone_id, 
                    build_out_param_channel, 
                    false);                
            }
            else if(build_channel)
            {
                //we are here as this zone needs to send the destination addref and caller addref to different zones
                rpc::shared_ptr<rpc::service_proxy> dest_zone;
                {
                    std::lock_guard g(insert_control);
                    rpc::shared_ptr<rpc::service_proxy> caller_zone;

                    auto found = other_zones.find({destination_zone_id, caller_zone_id});
                    if(found != other_zones.end())
                    {
                        dest_zone = found->second.lock();
                        dest_zone->add_external_ref();
                    }
                    else
                    {
                        found = other_zones.lower_bound({{dest_channel}, {0}});
                        if(found != other_zones.end() && found->first.dest.get_val() == dest_channel)
                        {
                            auto tmp = found->second.lock();
                            dest_zone = tmp->clone_for_zone(destination_zone_id, caller_zone_id);
                        }
                        else
                        {
                            //get the parent to route it
                            dest_zone = get_parent()->clone_for_zone(destination_zone_id, caller_zone_id);
                        }
                        inner_add_zone_proxy(dest_zone);
                    }

                    if(caller_zone_id == zone_id_.as_caller())
                        build_out_param_channel = build_out_param_channel ^ add_ref_options::build_caller_route; //strip out this bit


                    if(!!(build_out_param_channel & add_ref_options::build_caller_route))
                    {
                        found = other_zones.lower_bound({{caller_channel}, {0}});
                        if(found != other_zones.end() && found->first.dest.get_val() == caller_channel)
                        {
                            caller_zone = found->second.lock();
                        }
                        else
                        {
                            //untested path
                            assert(false);
                            //caller_zone = get_parent();
                        }

                        assert(caller_zone);
                    }

                    do
                    {
                        //this more fiddly check is to route calls to a parent node that knows more about this
                        if(dest_zone && caller_zone && build_out_param_channel == (add_ref_options::build_caller_route | add_ref_options::build_destination_route))
                        {
                            auto dc = dest_zone->get_destination_channel_zone_id().is_set() ? dest_zone->get_destination_channel_zone_id().get_val() : dest_zone->get_destination_zone_id().get_val();
                            auto cc = caller_zone->get_destination_channel_zone_id().is_set() ? caller_zone->get_destination_channel_zone_id().get_val() : caller_zone->get_destination_zone_id().get_val();
                            if(dc == cc)
                            {
                                if(telemetry_service_)
                                    telemetry_service_->on_service_add_ref(
                                        "", 
                                        get_zone_id(), 
                                        {0}, 
                                        destination_zone_id, 
                                        object_id, 
                                        {0}, 
                                        caller_zone_id, 
                                        build_out_param_channel);

                                auto ret = dest_zone->add_ref(
                                    protocol_version, 
                                    {0}, 
                                    destination_zone_id, 
                                    object_id, 
                                    {0}, 
                                    caller_zone_id, 
                                    build_out_param_channel, 
                                    false);
                                dest_zone->release_external_ref();//perhaps this could be optimised
                                if(ret == std::numeric_limits<uint64_t>::max())
                                {
                                    return ret;
                                }
                                break;
                            }
                        }

                        //then call the add ref to the destination
                        if(!!(build_out_param_channel & add_ref_options::build_destination_route))
                        {
                            if(telemetry_service_)
                                telemetry_service_->on_service_add_ref(
                                    "", 
                                    get_zone_id(),
                                    {0},  
                                    destination_zone_id, 
                                    object_id, 
                                    get_zone_id().as_caller_channel(), 
                                    caller_zone_id, 
                                    add_ref_options::build_destination_route);
                            dest_zone->add_ref(
                                protocol_version, 
                                {0}, 
                                destination_zone_id, 
                                object_id, 
                                get_zone_id().as_caller_channel(), 
                                caller_zone_id, 
                                add_ref_options::build_destination_route, 
                                false);
                        }
                        //back fill the ref count to the caller
                        if(!!(build_out_param_channel & add_ref_options::build_caller_route))
                        {
                            if(telemetry_service_)
                                telemetry_service_->on_service_add_ref(
                                    "", 
                                    caller_zone->get_zone_id(), 
                                    zone_id_.as_destination_channel(), 
                                    destination_zone_id, 
                                    object_id, 
                                    caller_channel_zone_id, 
                                    caller_zone_id, 
                                    add_ref_options::build_caller_route);                                
                            caller_zone->add_ref(
                                protocol_version, 
                                zone_id_.as_destination_channel(), 
                                destination_zone_id, 
                                object_id, 
                                caller_channel_zone_id, 
                                caller_zone_id, 
                                add_ref_options::build_caller_route, 
                                false);
                        }
                    }while(false);
                }

                return 1;
            }
            else
            {
                rpc::shared_ptr<service_proxy> other_zone;
                {//brackets here as we are using a lock guard
                    std::lock_guard g(insert_control);
                    auto found = other_zones.find({destination_zone_id, caller_zone_id});
                    if(found != other_zones.end())
                    {
                        other_zone = found->second.lock();
                        other_zone->add_external_ref();
                    }

                    if(!other_zone)
                    {
                        auto found = other_zones.lower_bound({destination_zone_id, {0}});
                        if(found != other_zones.end() && found->first.dest == destination_zone_id)
                        {
                            auto tmp = found->second.lock();
                            other_zone = tmp->clone_for_zone(destination_zone_id, caller_zone_id);
                            inner_add_zone_proxy(other_zone);
                        }
                    }

                    //as we are a child we can consult the parent to see if it can do the job of proxying this                
                    if(!other_zone)
                    {
                        auto parent = get_parent();
                        assert(parent);
                        other_zone = parent->clone_for_zone(destination_zone_id, caller_zone_id);
                        inner_add_zone_proxy(other_zone);
                    }
                }
                if(telemetry_service_)
                    telemetry_service_->on_service_add_ref(
                        "", 
                        get_zone_id(), 
                        {0}, 
                        destination_zone_id, 
                        object_id, 
                        caller_channel_zone_id, 
                        caller_zone_id, 
                        build_out_param_channel);   
                return other_zone->add_ref(
                    protocol_version, 
                    {0}, 
                    destination_zone_id, 
                    object_id, 
                    caller_channel_zone_id, 
                    caller_zone_id, 
                    build_out_param_channel, 
                    false);
            }
        }
        else
        {
#ifdef RPC_V2
            if(protocol_version == rpc::VERSION_2)
            ;
            else 
#endif
#ifdef RPC_V1
            if(protocol_version == rpc::VERSION_1)
            ;
            else
#endif
            {
                return std::numeric_limits<uint64_t>::max();
            }

            //find the caller
            if(zone_id_.as_caller() != caller_zone_id && !!(build_out_param_channel & add_ref_options::build_caller_route))
            {
                rpc::shared_ptr<service_proxy> caller_zone;
                {
                    std::lock_guard g(insert_control);
                    //we swap the parameter types as this is from perspective of the caller and not the proxy that called this function
                    auto found = other_zones.find({caller_zone_id.as_destination(), destination_zone_id.as_caller()});
                    if(found != other_zones.end())
                    {
                        caller_zone = found->second.lock();
                    }                
                    else
                    {
                        //untested
                        assert(false);
                        //caller_zone = get_parent();                        
                    }
                    assert(caller_zone);
                }
                if(telemetry_service_)
                    telemetry_service_->on_service_add_ref(
                        "", 
                        get_zone_id(), 
                        {0},
                        destination_zone_id, 
                        object_id, 
                        {}, 
                        caller_zone_id, 
                        add_ref_options::build_caller_route);  
                caller_zone->add_ref(
                    protocol_version, 
                    {0},
                    destination_zone_id, 
                    object_id, 
                    {}, 
                    caller_zone_id, 
                    add_ref_options::build_caller_route, 
                    false);
            }
            if(object_id == dummy_object_id)
            {
                return 0;
            }

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

    uint64_t service::release_local_stub(const rpc::shared_ptr<rpc::object_stub>& stub)
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
        return count;
    }

    uint64_t service::release(
        uint64_t protocol_version, 
        destination_zone destination_zone_id, 
        object object_id, 
        caller_zone caller_zone_id
    )
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
            if(telemetry_service_)
                telemetry_service_->on_service_release("service", zone_id_, other_zone->get_destination_channel_zone_id(), destination_zone_id, object_id, caller_zone_id);    
            auto ret = other_zone->release(protocol_version, destination_zone_id, object_id, caller_zone_id);
            if(ret != std::numeric_limits<uint64_t>::max())
            {
                other_zone->release_external_ref();
            }
            return ret;
        }
        else
        {
            if(telemetry_service_)
                telemetry_service_->on_service_release("service", zone_id_, {0}, destination_zone_id, object_id, caller_zone_id);    

#ifdef RPC_V2
            if(protocol_version == rpc::VERSION_2)
            ;
            else 
#endif
#ifdef RPC_V1
            if(protocol_version == rpc::VERSION_1)
            ;
            else
#endif
            {
                return std::numeric_limits<uint64_t>::max();
            }

            bool reset_stub = false;
            rpc::shared_ptr<rpc::object_stub> stub;
            uint64_t count = 0;
            //these scope brackets are needed as otherwise there will be a recursive lock on a mutex in rare cases when the stub is reset
            {
                {
                    //a scoped lock
                    std::lock_guard l(insert_control);
                    auto item = stubs.find(object_id);
                    if (item == stubs.end())
                    {
                        assert(false);
                        return std::numeric_limits<uint64_t>::max();
                    }

                    stub = item->second.lock();
                }

                if (!stub)
                {
                    assert(false);
                    return std::numeric_limits<uint64_t>::max();
                }
                count = stub->release();
                if(!count)
                {
                    {
                        //a scoped lock
                        std::lock_guard l(insert_control);
                        {
                            stubs.erase(object_id);
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
        service_proxy->add_external_ref();
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
        caller_zone new_caller_zone_id, 
        bool& new_proxy_added)
    {
        new_proxy_added = false;
        std::lock_guard g(insert_control);

        //find if we have one
        auto item = other_zones.find({destination_zone_id, new_caller_zone_id});
        if (item != other_zones.end())
            return item->second.lock();

        item = other_zones.lower_bound({destination_zone_id, {0}});

        if(item != other_zones.end() && item->first.dest != destination_zone_id)
            item = other_zones.end();

        //if not we can make one from the proxy of the calling channel zone
        //this zone knows nothing about the destination zone however the caller channel zone will know how to connect to it
        if (item == other_zones.end() && caller_channel_zone_id.is_set())
        {
            item = other_zones.lower_bound({caller_channel_zone_id.as_destination(), {0}});
            if (item == other_zones.end() || item->first.dest != caller_channel_zone_id.as_destination())
            {
                assert(false); //something is wrong the caller channel should always be valid if specified
                return nullptr;
            }
        }
        //or if not we can make one from the proxy of the calling  zone
        if(item == other_zones.end() && caller_zone_id.is_set())
        {
            item = other_zones.lower_bound({caller_zone_id.as_destination(), {0}});
            if (item == other_zones.end() || item->first.dest != caller_zone_id.as_destination())
            {
                assert(false); //something is wrong the caller should always be valid if specified
                return nullptr;
            }
        }
        if (item == other_zones.end())
        {
            assert(false); //something is wrong we should not get here
            return nullptr;
        }

        auto calling_proxy = item->second.lock();
        if(!calling_proxy)
        {
            assert(!"Race condition"); //we have a race condition
            return nullptr;
        }

        auto proxy = calling_proxy->clone_for_zone(destination_zone_id, new_caller_zone_id);
        inner_add_zone_proxy(proxy);
        new_proxy_added = true;
        return proxy;
    }

    void service::remove_zone_proxy(destination_zone destination_zone_id, caller_zone caller_zone_id, destination_channel_zone destination_channel_zone_id)
    {
        {
            std::lock_guard g(insert_control);
            auto item = other_zones.find({destination_zone_id, caller_zone_id});
            if (item == other_zones.end())
            {
                assert(false);
            }
            else
            {
                auto sp = item->second.lock();
                other_zones.erase(item);
            }
        }
    }

    int service::create_interface_stub(rpc::interface_ordinal interface_id, std::function<interface_ordinal(uint8_t)> interface_getter, const rpc::shared_ptr<rpc::i_interface_stub>& original, rpc::shared_ptr<rpc::i_interface_stub>& new_stub)
    {
        //an identity check, send back the same pointer
        if(
#ifdef RPC_V2
                interface_getter(rpc::VERSION_2) == interface_id
#endif
#if defined(RPC_V1) && defined(RPC_V2)
                ||
#endif
#ifdef RPC_V1
                interface_getter(rpc::VERSION_1) == interface_id
#endif            
#if !defined(RPC_V1) && !defined(RPC_V2)
                false
#endif
        )
        {
            new_stub = rpc::static_pointer_cast<rpc::i_interface_stub>(original);
            return rpc::error::OK();
        }

        auto it = stub_factories.find(interface_id);
        if(it == stub_factories.end())
        {
            return rpc::error::INVALID_CAST();
        }

        new_stub = (*it->second)(original);
        if(!new_stub)
        {
            return rpc::error::INVALID_CAST();
        }
        //note a nullptr return value is a valid value, it indicates that this object does not implement that interface
        return rpc::error::OK();
    }

    //note this function is not thread safe!  Use it before using the service class for normal operation
    void service::add_interface_stub_factory(std::function<interface_ordinal (uint8_t)> id_getter, std::shared_ptr<std::function<rpc::shared_ptr<rpc::i_interface_stub>(const rpc::shared_ptr<rpc::i_interface_stub>&)>> factory)
    {
#ifdef RPC_V1
        auto interface_id = id_getter(rpc::VERSION_1);
        auto it = stub_factories.find({interface_id});
        if(it != stub_factories.end())
        {
            rpc::error::INVALID_DATA();
        }
        stub_factories[{interface_id}] = factory;
#endif

#ifdef RPC_V2
        interface_id = id_getter(rpc::VERSION_2);
        it = stub_factories.find({interface_id});
        if(it != stub_factories.end())
        {
            rpc::error::INVALID_DATA();
        }
        stub_factories[{interface_id}] = factory;
#endif
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

    child_service::~child_service()
    {
        std::vector<rpc::shared_ptr<rpc::service_proxy>> zones_to_be_explicitly_removed;
        
        for(auto sp : zones_to_be_explicitly_removed)
        {
            remove_zone_proxy(sp->get_destination_zone_id(), sp->get_caller_zone_id(), sp->get_destination_channel_zone_id());
        }
        zones_to_be_explicitly_removed.clear();

        if(parent_service_proxy_)
        {
            assert(parent_service_proxy_->get_caller_zone_id() == get_zone_id().as_caller());
            assert(parent_service_proxy_->get_destination_channel_zone_id().get_val() == 0);
            remove_zone_proxy(parent_service_proxy_->get_destination_zone_id(), get_zone_id().as_caller(), {0});
            parent_service_proxy_->set_parent_channel(false);
            parent_service_proxy_->release_external_ref();
            parent_service_proxy_ = nullptr;
        }    
    }

    void child_service::set_parent(const rpc::shared_ptr<rpc::service_proxy>& parent_service_proxy)
    {
        if(parent_service_proxy_ && parent_service_proxy_ != parent_service_proxy)
        {
            assert(false);
            parent_service_proxy_->release_external_ref();
        }
        if(parent_service_proxy)
        {
            assert(parent_zone_id_ == parent_service_proxy->get_destination_zone_id());
        }
        parent_service_proxy_ = parent_service_proxy;
    }
}
