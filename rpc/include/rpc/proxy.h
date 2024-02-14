#pragma once

#include <string>
#include <memory>
#include <unordered_map>
#include <mutex>

#include <rpc/logger.h>
#include <rpc/version.h>
#include <rpc/types.h>
#include <rpc/marshaller.h>
#include <rpc/service.h>
#include <rpc/remote_pointer.h>
#include <rpc/telemetry/i_telemetry_service.h>
#include "rpc/stub.h"

namespace rpc
{
    class service;
    class child_service;
    class service_proxy;
    class object_proxy;
    template<class T> class proxy_impl;

    // non virtual class to allow for type erasure
    class proxy_base
    {
        rpc::shared_ptr<object_proxy> object_proxy_;

    protected:
        proxy_base(rpc::shared_ptr<object_proxy> object_proxy)
            : object_proxy_(object_proxy)
        {
        }
        virtual ~proxy_base()
        {}

        template<class T> rpc::interface_descriptor proxy_bind_in_param(uint64_t protocol_version, const rpc::shared_ptr<T>& iface, rpc::shared_ptr<rpc::object_stub>& stub);
        template<class T> rpc::interface_descriptor stub_bind_out_param(uint64_t protocol_version, caller_channel_zone caller_channel_zone_id, caller_zone caller_zone_id, const rpc::shared_ptr<T>& iface);

        template<class T1, class T2>
        friend rpc::shared_ptr<T1> dynamic_pointer_cast(const shared_ptr<T2>& from) noexcept;
        friend service;
    public:
        rpc::shared_ptr<object_proxy> get_object_proxy() const { return object_proxy_; }
    };

    template<class T> class proxy_impl : public proxy_base, public T
    {
    public:
        proxy_impl(rpc::shared_ptr<object_proxy> object_proxy)
            : proxy_base(object_proxy)
        {
        }
        
        virtual void* get_address() const override
        {
            RPC_ASSERT(false);
            return (T*)get_object_proxy().get();
        }   
        rpc::proxy_base* query_proxy_base() const override 
        { 
            return static_cast<proxy_base*>(const_cast<proxy_impl*>(this)); 
        }   
        const rpc::casting_interface* query_interface(rpc::interface_ordinal interface_id) const override 
        { 
            #ifdef RPC_V2
            if(T::get_id(rpc::VERSION_2) == interface_id)
                return static_cast<const T*>(this);  
            #endif
            #ifdef RPC_V1
            if(T::get_id(rpc::VERSION_1) == interface_id)
                return static_cast<const T*>(this);  
            #endif
            return nullptr;
        }
        virtual ~proxy_impl() = default;
    };

    class object_proxy : public rpc::enable_shared_from_this<object_proxy>
    {
        object object_id_;
        rpc::shared_ptr<service_proxy> service_proxy_;
        std::unordered_map<interface_ordinal, rpc::weak_ptr<proxy_base>> proxy_map;
        std::mutex insert_control_;

        object_proxy(object object_id, rpc::shared_ptr<service_proxy> service_proxy);

        // note the interface pointer may change if there is already an interface inserted successfully
        void register_interface(interface_ordinal interface_id, rpc::weak_ptr<proxy_base>& value);

        int try_cast(std::function<interface_ordinal (uint64_t)> id_getter);
        
        friend service_proxy;

    public:
        virtual ~object_proxy();

        rpc::shared_ptr<service_proxy> get_service_proxy() const { return service_proxy_; }
        object get_object_id() const {return {object_id_};}
        destination_zone get_destination_zone_id() const;

        int send(uint64_t tag, std::function<interface_ordinal (uint64_t)> id_getter, method method_id, size_t in_size_, const char* in_buf_,
                        std::vector<char>& out_buf_);

        size_t get_proxy_count()
        {
            std::lock_guard guard(insert_control_);
            return proxy_map.size();
        }

        template<class T> void create_interface_proxy(rpc::shared_ptr<T>& inface);

        template<class T> int query_interface(rpc::shared_ptr<T>& iface, bool do_remote_check = true)
        {
            auto create = [&](std::unordered_map<interface_ordinal, rpc::weak_ptr<proxy_base>>::iterator item) -> int {
                rpc::shared_ptr<proxy_base> proxy = item->second.lock();
                if (!proxy)
                {
                    // weak pointer needs refreshing
                    create_interface_proxy<T>(iface);
                    item->second = rpc::reinterpret_pointer_cast<proxy_base>(iface);
                    return rpc::error::OK();
                }
                iface = rpc::reinterpret_pointer_cast<T>(proxy);
                return rpc::error::OK();
            };

            { // scope for the lock
                std::lock_guard guard(insert_control_);
#ifdef RPC_V2
                if(T::get_id(rpc::VERSION_2) == 0)
                {
                    return rpc::error::OK();
                }
                {
                    auto item = proxy_map.find(T::get_id(rpc::VERSION_2));
                    if (item != proxy_map.end())
                    {
                        return create(item);
                    }
                }
#endif
#ifdef RPC_V1
                if(T::get_id(rpc::VERSION_1) == 0)
                {
                    return rpc::error::OK();
                }
                {
                    auto item = proxy_map.find(T::get_id(rpc::VERSION_1));
                    if (item != proxy_map.end())
                    {
                        return create(item);
                    }
                }
#endif
                if (!do_remote_check)
                {
                    create_interface_proxy<T>(iface);
#ifdef RPC_V1
                    proxy_map[T::get_id(rpc::VERSION_1)] = rpc::reinterpret_pointer_cast<proxy_base>(iface);
#endif
#ifdef RPC_V2
                    proxy_map[T::get_id(rpc::VERSION_2)] = rpc::reinterpret_pointer_cast<proxy_base>(iface);
#endif                    
                    return rpc::error::OK();
                }
            }

            // release the lock and then check for casting
            if (do_remote_check)
            {
                // see if object_id can implement interface
                int ret = try_cast(T::get_id);
                if (ret != rpc::error::OK())
                {
                    return ret;
                }
            }
            { // another scope for the lock
                std::lock_guard guard(insert_control_);

                // check again...
#ifdef RPC_V2
                {
                    auto item = proxy_map.find(T::get_id(rpc::VERSION_2));
                    if (item != proxy_map.end())
                    {
                        return create(item);
                    }
                }
#endif
#ifdef RPC_V1
                {
                    auto item = proxy_map.find(T::get_id(rpc::VERSION_1));
                    if (item != proxy_map.end())
                    {
                        return create(item);
                    }
                }
#endif

                create_interface_proxy<T>(iface);
#ifdef RPC_V1
                proxy_map[T::get_id(rpc::VERSION_1)] = rpc::reinterpret_pointer_cast<proxy_base>(iface);
#endif
#ifdef RPC_V2
                proxy_map[T::get_id(rpc::VERSION_2)] = rpc::reinterpret_pointer_cast<proxy_base>(iface);
#endif
                return rpc::error::OK();
            }
        }
    };

    // the class that encapsulates an environment or zone
    // only host code can use this class directly other enclaves *may* have access to the i_service_proxy derived interface

    class service_proxy : 
        public i_marshaller,
        public rpc::enable_shared_from_this<service_proxy>
    {
        std::unordered_map<object, rpc::weak_ptr<object_proxy>> proxies_;
        std::mutex insert_control_;

        const zone zone_id_;
        destination_zone destination_zone_id_ = {0};
        destination_channel_zone destination_channel_zone_ = {0};
        caller_zone caller_zone_id_ = {0};
        rpc::weak_ptr<service> service_;
        rpc::shared_ptr<service_proxy> lifetime_lock_;
        std::atomic<int> lifetime_lock_count_ = 0;
        std::atomic<uint64_t> version_ = rpc::get_version();
        encoding enc_ = encoding::enc_default;
        //if a service proxy is pointing to the zones parent zone then it needs to stay alive even if there are no active references going through it
        bool is_parent_channel_ = false;
        std::string name_;

    protected:

        service_proxy(  const char* name,
                        destination_zone destination_zone_id,
                        const rpc::shared_ptr<service>& svc) : 
            zone_id_(svc->get_zone_id()),
            destination_zone_id_(destination_zone_id),
            caller_zone_id_(svc->get_zone_id().as_caller()),
            service_(svc),
            name_(name)
        {
            if (auto telemetry_service = rpc::telemetry_service_manager::get(); telemetry_service)
            {
                telemetry_service->on_service_proxy_creation(name, get_zone_id(), get_destination_zone_id(), svc->get_zone_id().as_caller());
            }
            RPC_ASSERT(svc != nullptr);
        }

        service_proxy(const service_proxy& other) : 
                zone_id_(other.zone_id_),
                destination_zone_id_(other.destination_zone_id_),
                destination_channel_zone_(other.destination_channel_zone_),
                caller_zone_id_(other.caller_zone_id_),
                service_(other.service_),
                lifetime_lock_count_(0),
                enc_(other.enc_),
                name_(other.name_)
        {       
            RPC_ASSERT(service_.lock() != nullptr);
        }

        //not thread safe
        void set_remote_rpc_version(uint64_t version) {version_ = version;}
        bool is_parent_channel() const {return is_parent_channel_;}
        void set_parent_channel(bool val) 
        {
            is_parent_channel_ = val;

            if(lifetime_lock_count_ == 0 && is_parent_channel_ == false)
            {
                RPC_ASSERT(lifetime_lock_);
                lifetime_lock_ = nullptr;
            }                 
        }

    public:
        virtual ~service_proxy()
        {
            RPC_ASSERT(proxies_.empty());
            auto svc = service_.lock();
            if(svc)
            {
                svc->remove_zone_proxy(destination_zone_id_, caller_zone_id_, destination_channel_zone_);
            }
            if (auto telemetry_service = rpc::telemetry_service_manager::get(); telemetry_service)
            {
                telemetry_service->on_service_proxy_deletion(get_zone_id(), get_destination_zone_id(), get_caller_zone_id());
            }            
        }
        
        std::string get_name() const { return name_; }

        uint64_t get_remote_rpc_version() const {return version_.load();}

        uint64_t set_encoding(encoding enc)
        {
            if(version_ == rpc::VERSION_1)
                return error::INCOMPATIBLE_SERVICE();
            enc_ = enc;
            return error::OK();
        }       
        
        virtual int connect(rpc::interface_descriptor input_descr, rpc::interface_descriptor& output_descr)
        {            
            std::ignore = input_descr;
            std::ignore = output_descr;
            return rpc::error::ZONE_NOT_SUPPORTED();
        }
        
        void add_external_ref()
        {
            std::lock_guard g(insert_control_);
            auto count = ++lifetime_lock_count_;
            if (auto telemetry_service = rpc::telemetry_service_manager::get(); telemetry_service)
            {
                telemetry_service->on_service_proxy_add_external_ref(zone_id_, destination_channel_zone_, destination_zone_id_, caller_zone_id_, count);
            } 
            RPC_ASSERT(count >= 1);
            if(count == 1)
            {
                RPC_ASSERT(!lifetime_lock_);
                lifetime_lock_ = shared_from_this();
                RPC_ASSERT(lifetime_lock_);
            }            
        }

        int release_external_ref()
        {
            std::lock_guard g(insert_control_);
            return inner_release_external_ref();
        }

        int inner_release_external_ref()
        {
            auto count = --lifetime_lock_count_;
            if (auto telemetry_service = rpc::telemetry_service_manager::get(); telemetry_service)
            {
                telemetry_service->on_service_proxy_release_external_ref(zone_id_, destination_channel_zone_, destination_zone_id_, caller_zone_id_, count);
            }            
            RPC_ASSERT(count >= 0);
            if(count == 0 && is_parent_channel_ == false)
            {
                RPC_ASSERT(lifetime_lock_);
                lifetime_lock_ = nullptr;
            }   
            return count;         
        }

        [[nodiscard]] int sp_call(
                encoding enc
                , uint64_t tag
                , object object_id
                , std::function<interface_ordinal (uint64_t)> id_getter
                , method method_id
                , size_t in_size_
                , const char* in_buf_
                , std::vector<char>& out_buf_)
        {
            //force a lowest common denominator
            if(enc != encoding::yas_json && enc_ != encoding::enc_default && enc != enc_)
            {
                return error::INCOMPATIBLE_SERIALISATION();
            }

            auto version = version_.load();
            auto ret = send(
                version,
                enc_,
                tag,
                caller_channel_zone{}, 
                get_zone_id().as_caller(), 
                get_destination_zone_id(), 
                object_id, 
                id_getter(version), 
                method_id, 
                in_size_, 
                in_buf_, 
                out_buf_);
            if(ret == rpc::error::INVALID_VERSION())
            {
                version_.compare_exchange_strong( version, version - 1);
            }
            return ret;
        }

        [[nodiscard]] int sp_try_cast(            
            destination_zone destination_zone_id 
            , object object_id 
            , std::function<interface_ordinal (uint64_t)> id_getter)
        {
            auto original_version = version_.load();
            auto version = original_version;
            while(version)
            {
                auto if_id = id_getter(version);
                if (auto telemetry_service = rpc::telemetry_service_manager::get(); telemetry_service)
                {
                    telemetry_service->on_service_proxy_try_cast(get_zone_id(),
                                                                    destination_zone_id, get_caller_zone_id(), object_id, if_id);
                }
                auto ret = try_cast(
                    version
                    , destination_zone_id
                    , object_id
                    , if_id);
                if(ret != rpc::error::INVALID_VERSION())
                {
                    if(original_version != version)
                    {
                        version_.compare_exchange_strong( original_version, version);
                    }
                    return ret;
                }
                version--;
            }
            return rpc::error::INCOMPATIBLE_SERVICE();
        }
        
        [[nodiscard]] uint64_t sp_add_ref(
            object object_id 
            , caller_channel_zone caller_channel_zone_id 
            , add_ref_options build_out_param_channel)
        {
            if (auto telemetry_service = rpc::telemetry_service_manager::get(); telemetry_service)
            {
                telemetry_service->on_service_proxy_add_ref(get_zone_id(),
                                                                destination_zone_id_, destination_channel_zone_,  get_caller_zone_id(), object_id, build_out_param_channel);
            }

            auto original_version = version_.load();
            auto version = original_version;
            while(version)
            {
                auto ret = add_ref(
                    version
                    , destination_channel_zone_
                    , destination_zone_id_ 
                    , object_id 
                    , caller_channel_zone_id 
                    , caller_zone_id_ 
                    , build_out_param_channel );
                if(ret != std::numeric_limits<uint64_t>::max())
                {
                    if(original_version != version)
                    {
                        version_.compare_exchange_strong( original_version, version);
                    }
                    return ret;
                }
                version--;
            }
            return rpc::error::INCOMPATIBLE_SERVICE();        
        }
        
        uint64_t sp_release(object object_id) 
        {
            if (auto telemetry_service = rpc::telemetry_service_manager::get(); telemetry_service)
            {
                telemetry_service->on_service_proxy_release(get_zone_id(),
                                                                destination_zone_id_, destination_channel_zone_,  get_caller_zone_id(), object_id);
            }
            
            auto original_version = version_.load();
            auto version = original_version;
            while(version)
            {
                auto ret = release(
                    version
                    , destination_zone_id_ 
                    , object_id 
                    , caller_zone_id_);
                if(ret != std::numeric_limits<uint64_t>::max())
                {                   
                    if(original_version != version)
                    {
                        version_.compare_exchange_strong( original_version, version);
                    }
                    return ret;
                }
                version--;
            }
            return rpc::error::INCOMPATIBLE_SERVICE();   
        }
        
        void on_object_proxy_released(object object_id)
        {
            auto caller_zone_id = get_zone_id().as_caller();
            RPC_ASSERT(caller_zone_id == get_caller_zone_id());

            {
                std::lock_guard l(insert_control_);
                auto item = proxies_.find(object_id);
                RPC_ASSERT(item  != proxies_.end());
                if(item->second.lock() == nullptr)
                {
                    //the reason for this if statement is between an object weak pointer dying and this attempted entry being removed another object pointer object may have sneaked in and set to a non null value
                    proxies_.erase(item);  
                }
            }

            if (auto telemetry_service = rpc::telemetry_service_manager::get(); telemetry_service)
            {
                telemetry_service->on_service_proxy_release(get_zone_id(),
                                                                destination_zone_id_, destination_channel_zone_,  caller_zone_id, object_id);
            }
            
            auto original_version = version_.load();
            auto version = original_version;
            while(version)
            {
                auto ret = release(
                    version
                    , destination_zone_id_ 
                    , object_id 
                    , caller_zone_id);
                if(ret != std::numeric_limits<uint64_t>::max())
                {
                    inner_release_external_ref();
                    if(original_version != version)
                    {
                        version_.compare_exchange_strong( original_version, version);
                    }
                    return;
                }
                version--;
            }
            {
                std::string message("unable to release on service");
                LOG_STR(message.c_str(), message.size());
                RPC_ASSERT(false);
            }
            return;   
        }            

        std::unordered_map<object, rpc::weak_ptr<object_proxy>> get_proxies(){return proxies_;}

        virtual rpc::shared_ptr<service_proxy> clone() = 0;
        virtual void clone_completed()
        {
            if (auto telemetry_service = rpc::telemetry_service_manager::get(); telemetry_service)
            {
                telemetry_service->on_service_proxy_creation(name_.c_str(), get_zone_id(), get_destination_zone_id(), get_caller_zone_id());
            }
        }
        rpc::shared_ptr<service_proxy> clone_for_zone(destination_zone destination_zone_id, caller_zone caller_zone_id)
        {
            RPC_ASSERT(!(caller_zone_id_ == caller_zone_id && destination_zone_id_ == destination_zone_id));
            auto ret = clone();
            ret->is_parent_channel_ = false;
            ret->caller_zone_id_ = caller_zone_id;
            if(destination_zone_id_ != destination_zone_id)
            {
                ret->destination_zone_id_ = destination_zone_id;
                if(!ret->destination_channel_zone_.is_set())
                    ret->destination_channel_zone_ = destination_zone_id_.as_destination_channel();
            }

            ret->clone_completed();
            return ret;
        }

        //the zone where this proxy is created
        zone get_zone_id() const {return zone_id_;}
        //the ultimate zone where this proxy is calling
        destination_zone get_destination_zone_id() const {return destination_zone_id_;}
        //the intermediate zone where this proxy is calling
        destination_channel_zone get_destination_channel_zone_id() const {return destination_channel_zone_;}
        caller_zone get_caller_zone_id() const {return caller_zone_id_;}

        //the service that this proxy lives in
        rpc::shared_ptr<service> get_operating_zone_service() const {return service_.lock();}

        rpc::shared_ptr<object_proxy> get_object_proxy(object object_id, bool& is_new)
        {
            RPC_ASSERT(get_caller_zone_id() == get_zone_id().as_caller());
            std::lock_guard l(insert_control_);
            auto item = proxies_.find(object_id);
            rpc::shared_ptr<object_proxy> op;
            if(item != proxies_.end())
                op = item->second.lock();
            if(op == nullptr)
            {
                op = rpc::shared_ptr<object_proxy>(new object_proxy(object_id, shared_from_this()));
                if (auto telemetry_service = rpc::telemetry_service_manager::get(); telemetry_service)
                {
                    telemetry_service->on_object_proxy_creation(get_zone_id(), get_destination_zone_id(), object_id, true);
                }        
                proxies_[object_id] = op;    
                is_new = true;            
                return op;
            }
            is_new = false;
            return op;            
        }
        
        friend service;
        friend child_service;
    };

    //declared here as object_proxy and service_proxy is not fully defined in the body of proxy_base
    template<class T>
    interface_descriptor proxy_base::proxy_bind_in_param(uint64_t protocol_version, const rpc::shared_ptr<T>& iface, rpc::shared_ptr<rpc::object_stub>& stub)
    {
        if(!iface)
            return interface_descriptor();
            
        auto operating_service = object_proxy_->get_service_proxy()->get_operating_zone_service();

        //this is to check that an interface is belonging to another zone and not the operating zone
        auto proxy = iface->query_proxy_base();
        if(proxy && proxy->get_object_proxy()->get_destination_zone_id() != operating_service->get_zone_id().as_destination())
        {
            return {proxy->get_object_proxy()->get_object_id(), proxy->get_object_proxy()->get_destination_zone_id()};
        }

        //else encapsulate away
        return operating_service->proxy_bind_in_param(protocol_version, iface, stub);
    }

    //do not use directly it is for the interface generator use rpc::create_interface_proxy if you want to get a proxied pointer to a remote implementation
    template<class T> 
    int stub_bind_in_param(uint64_t protocol_version, rpc::service& serv, caller_channel_zone caller_channel_zone_id, caller_zone caller_zone_id, const rpc::interface_descriptor& encap, rpc::shared_ptr<T>& iface)
    {
        //if we have a null object id then return a null ptr
        if(encap == rpc::interface_descriptor())
        {
            return rpc::error::OK();
        }
        //if it is local to this service then just get the relevant stub
        else if(serv.get_zone_id().as_destination() == encap.destination_zone_id)
        {
            iface = serv.get_local_interface<T>(protocol_version, encap.object_id);
            if(!iface)
                return rpc::error::OBJECT_NOT_FOUND();
            return rpc::error::OK();
        }
        else
        {
            auto zone_id = serv.get_zone_id();
            //get the right  service proxy
            //if the zone is different lookup or clone the right proxy
            bool new_proxy_added = false;
            auto service_proxy = serv.get_zone_proxy(caller_channel_zone_id, caller_zone_id, encap.destination_zone_id, zone_id.as_caller(), new_proxy_added);
            if(!service_proxy)
                return rpc::error::OBJECT_NOT_FOUND();

            bool is_new = false;
            rpc::shared_ptr<object_proxy> op = service_proxy->get_object_proxy(encap.object_id, is_new);
            if(is_new)
            {
                auto ret = service_proxy->sp_add_ref(
                                encap.object_id
                                , {0}
                                , rpc::add_ref_options::normal);
                if(ret == std::numeric_limits<uint64_t>::max())
                    return -1;
                if(!new_proxy_added)
                {
                    service_proxy->add_external_ref();
                }

            }
            auto ret = op->query_interface(iface, false);        
            return ret;
        }
    }

    //declared here as object_proxy and service_proxy is not fully defined in the body of proxy_base
    template<class T>
    interface_descriptor proxy_base::stub_bind_out_param(uint64_t protocol_version, caller_channel_zone caller_channel_zone_id, caller_zone caller_zone_id, const rpc::shared_ptr<T>& iface)
    {
        if(!iface)
            return {{0},{0}};
            
        auto operating_service = object_proxy_->get_service_proxy()->get_operating_zone_service();

        //this is to check that an interface is belonging to another zone and not the operating zone
        auto proxy = iface->query_proxy_base();
        if(proxy && proxy->get_object_proxy()->get_zone_id() != operating_service->get_zone_id())
        {
            return {proxy->get_object_proxy()->get_object_id(), proxy->get_object_proxy()->get_zone_id()};
        }

        //else encapsulate away
        return operating_service->stub_bind_out_param(protocol_version, caller_channel_zone_id, caller_zone_id, iface);
    }

    //do not use directly it is for the interface generator use rpc::create_interface_proxy if you want to get a proxied pointer to a remote implementation
    template<class T> 
    int proxy_bind_out_param(const rpc::shared_ptr<rpc::service_proxy>& sp, const rpc::interface_descriptor& encap, caller_zone caller_zone_id, rpc::shared_ptr<T>& val)
    {
        //if we have a null object id then return a null ptr
        if(!encap.object_id.is_set() || !encap.destination_zone_id.is_set())
            return rpc::error::OK();

        auto serv = sp->get_operating_zone_service();

        //if it is local to this service then just get the relevant stub
        if(encap.destination_zone_id == serv->get_zone_id().as_destination())
        {
            auto ob = serv->get_object(encap.object_id).lock();
            if(!ob)
                return rpc::error::OBJECT_NOT_FOUND();

            auto count = serv->release_local_stub(ob);
            RPC_ASSERT(count);
            if(!count || count == std::numeric_limits<uint64_t>::max())
            {
                return rpc::error::REFERENCE_COUNT_ERROR();
            }

#ifdef RPC_V2
            auto interface_stub = ob->get_interface(T::get_id(rpc::VERSION_2));
            if(!interface_stub)
            {
#ifdef RPC_V1
                interface_stub = ob->get_interface(T::get_id(rpc::VERSION_1));
#endif                
                if(!interface_stub)
                {
                    return rpc::error::INVALID_INTERFACE_ID();
                }
            }
#elif defined(RPC_V1)
            auto interface_stub = ob->get_interface(T::get_id(rpc::VERSION_1));
            if(!interface_stub)
            {
                return rpc::error::INVALID_INTERFACE_ID();
            }
#endif

            val = rpc::static_pointer_cast<T>(interface_stub->get_castable_interface());
            return rpc::error::OK();
        }

        //get the right  service proxy
        bool new_proxy_added = false;
        auto service_proxy = sp;
        
        if(sp->get_destination_zone_id() != encap.destination_zone_id)
        {
            //if the zone is different lookup or clone the right proxy
            //the service proxy is where the object came from so it should be used as the new caller channel for this returned object
            auto caller_channel_zone_id = sp->get_destination_zone_id().as_caller_channel(); 
            service_proxy = serv->get_zone_proxy(caller_channel_zone_id, caller_zone_id, {encap.destination_zone_id}, sp->get_zone_id().as_caller(), new_proxy_added);
        }

        bool is_new = false;
        rpc::shared_ptr<object_proxy> op = service_proxy->get_object_proxy(encap.object_id, is_new);
        if(!is_new)
        {
            //as this is an out parameter the callee will be doing an add ref if the object proxy is already found we can do a release
            RPC_ASSERT(!new_proxy_added);
            auto ret = service_proxy->sp_release(encap.object_id);
            if(ret != std::numeric_limits<uint64_t>::max())
            {
                service_proxy->release_external_ref();            
            }
        }
        return op->query_interface(val, false);
    }

    template<class T> 
    int demarshall_interface_proxy(uint64_t protocol_version, const rpc::shared_ptr<rpc::service_proxy>& sp, const rpc::interface_descriptor& encap, caller_zone caller_zone_id, rpc::shared_ptr<T>& val)
    {
        if(protocol_version > rpc::get_version())
            return rpc::error::INCOMPATIBLE_SERVICE();
            
        //if we have a null object id then return a null ptr
        if(encap.object_id == 0 || encap.destination_zone_id == 0)
            return rpc::error::OK();

        if(encap.destination_zone_id != sp->get_destination_zone_id())
        {
            return rpc::proxy_bind_out_param(sp, encap, caller_zone_id, val);
        }

        auto service_proxy = sp;
        auto serv = service_proxy->get_operating_zone_service();

        //if it is local to this service then just get the relevant stub
        if(serv->get_zone_id().as_destination() == encap.destination_zone_id)
        {
            //if we get here then we need to invent a test for this
            RPC_ASSERT(false);
            return rpc::error::INVALID_DATA();            
            // val = serv->get_local_interface<T>(protocol_version, encap.object_id);
            // if(!val)
            //     return rpc::error::OBJECT_NOT_FOUND();
            // return rpc::error::OK();
        }

        //get the right  service proxy
        //bool new_proxy_added = false;
        if(service_proxy->get_destination_zone_id() != encap.destination_zone_id)
        {
            //if we get here then we need to invent a test for this
            RPC_ASSERT(false);
            return rpc::error::INVALID_DATA();            

            // //if the zone is different lookup or clone the right proxy
            // service_proxy = serv->get_zone_proxy(
            //     {0}, 
            //     caller_zone_id, 
            //     encap.destination_zone_id, 
            //     caller_zone_id,
            //     new_proxy_added);
        }

        if(serv->get_parent_zone_id() == service_proxy->get_destination_zone_id())
            service_proxy->add_external_ref();

        bool is_new = false;
        rpc::shared_ptr<object_proxy> op = service_proxy->get_object_proxy(encap.object_id, is_new);
        return op->query_interface(val, false);
    }
}
