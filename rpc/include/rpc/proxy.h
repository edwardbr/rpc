#pragma once

#include <string>
#include <memory>
#include <unordered_map>
#include <mutex>

#include <rpc/types.h>
#include <rpc/marshaller.h>
#include <rpc/service.h>
#include <rpc/remote_pointer.h>
#include <rpc/i_telemetry_service.h>
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

        template<class T> rpc::interface_descriptor proxy_bind_in_param(const rpc::shared_ptr<T>& iface, rpc::shared_ptr<rpc::object_stub>& stub);
        template<class T> rpc::interface_descriptor stub_bind_out_param(caller_channel_zone caller_channel_zone_id, caller_zone caller_zone_id, const rpc::shared_ptr<T>& iface);

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
            assert(false);
            return (T*)get_object_proxy().get();
        }   
        rpc::proxy_base* query_proxy_base() const override 
        { 
            return static_cast<proxy_base*>(const_cast<proxy_impl*>(this)); 
        }   
        const rpc::casting_interface* query_interface(rpc::interface_ordinal interface_id) const override 
        { 
            if(T::id == interface_id.get_val())
                return static_cast<const T*>(this);  
            return nullptr;
        }
        virtual ~proxy_impl() = default;
    };

    class object_proxy
    {
        object object_id_;
        rpc::shared_ptr<service_proxy> service_proxy_;
        std::unordered_map<interface_ordinal, rpc::weak_ptr<proxy_base>> proxy_map;
        std::mutex insert_control_;
        mutable rpc::weak_ptr<object_proxy> weak_this_;

        // note the interface pointer may change if there is already an interface inserted successfully
        void register_interface(interface_ordinal interface_id, rpc::weak_ptr<proxy_base>& value);
        object_proxy(   object object_id, 
                        rpc::shared_ptr<service_proxy> service_proxy,
                        bool stub_needs_add_ref,
                        bool service_proxy_needs_add_ref);

        int try_cast(interface_ordinal interface_id);

    public:
        static rpc::shared_ptr<object_proxy> create(object object_id,
                                                    const rpc::shared_ptr<service_proxy>& service_proxy,
                                                    bool stub_needs_add_ref,
                                                    bool service_proxy_needs_add_ref);

        virtual ~object_proxy();

        rpc::shared_ptr<object_proxy const> shared_from_this() const { return rpc::shared_ptr<object_proxy const>(weak_this_); }
        rpc::shared_ptr<object_proxy> shared_from_this() { return rpc::shared_ptr<object_proxy>(weak_this_); }

        rpc::shared_ptr<service_proxy> get_service_proxy() const { return service_proxy_; }
        object get_object_id() const {return {object_id_};}
        destination_zone get_destination_zone_id() const;

        int send(interface_ordinal interface_id, method method_id, size_t in_size_, const char* in_buf_,
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
                if(T::id == 0)
                {
                    return rpc::error::OK();
                }
                auto item = proxy_map.find({T::id});
                if (item != proxy_map.end())
                {
                    return create(item);
                }
                if (!do_remote_check)
                {
                    create_interface_proxy<T>(iface);
                    proxy_map[{T::id}] = rpc::reinterpret_pointer_cast<proxy_base>(iface);
                    return rpc::error::OK();
                }
            }

            // release the lock and then check for casting
            if (do_remote_check)
            {
                // see if object_id can implement interface
                int ret = try_cast({T::id});
                if (ret != rpc::error::OK())
                {
                    return ret;
                }
            }
            { // another scope for the lock
                std::lock_guard guard(insert_control_);

                // check again...
                auto item = proxy_map.find({T::id});
                if (item != proxy_map.end())
                {
                    return create(item);
                }

                create_interface_proxy<T>(iface);
                proxy_map[{T::id}] = rpc::reinterpret_pointer_cast<proxy_base>(iface);
                return rpc::error::OK();
            }
        }
    };


	//this bundle of joy implements a non locking pointer where if the remote object is not there
    //it will return an error without crashing
	template<class T> class optimistic_proxy_impl : public T
	{
		object object_id_;
		zone destination_zone_id_;
		rpc::weak_ptr<service_proxy> service_proxy_;
	public:
		optimistic_proxy_impl(rpc::shared_ptr<object_proxy> object_proxy)
			: object_id_(object_proxy->get_object_id())
			, destination_zone_id_(object_proxy->get_zone_id())
			, service_proxy_(object_proxy->get_service_proxy())
		{}
		virtual ~optimistic_proxy_impl() = default;
		object get_object_id() const { return object_id_; }
		destination_zone get_zone_id() const { return destination_zone_id_; }
		rpc::weak_ptr<service_proxy> get_service_proxy() const { return service_proxy_; }
	};

    // the class that encapsulates an environment or zone
    // only host code can use this class directly other enclaves *may* have access to the i_service_proxy derived interface

    class service_proxy : 
        public i_marshaller
    {
        std::unordered_map<object, rpc::weak_ptr<object_proxy>> proxies_;
        std::mutex insert_control_;

        zone zone_id_ = {0};
        destination_zone destination_zone_id_ = {0};
        destination_channel_zone destination_channel_zone_ = {0};
        caller_zone caller_zone_id_ = {0};
        rpc::weak_ptr<service> operating_zone_service_;
        rpc::shared_ptr<service_proxy> dependent_services_lock_;
        std::atomic<int> dependent_services_count_ = 0;
        const rpc::i_telemetry_service* const telemetry_service_ = nullptr;

    protected:

        service_proxy(  destination_zone destination_zone_id,
                        const rpc::shared_ptr<service>& operating_zone_service,
                        caller_zone caller_zone_id,
                        const rpc::i_telemetry_service* telemetry_service) : 
            zone_id_(operating_zone_service->get_zone_id()),
            destination_zone_id_(destination_zone_id),
            operating_zone_service_(operating_zone_service),
            caller_zone_id_(caller_zone_id),
            telemetry_service_(telemetry_service)
        {
            assert(operating_zone_service != nullptr);
        }

        service_proxy(const service_proxy& other) : 
                zone_id_(other.zone_id_),
                destination_zone_id_(other.destination_zone_id_),
                operating_zone_service_(other.operating_zone_service_),
                caller_zone_id_(other.caller_zone_id_),
                telemetry_service_(other.telemetry_service_),
                dependent_services_count_(0)
        {
            assert(operating_zone_service_.lock() != nullptr);
        }

        mutable rpc::weak_ptr<service_proxy> weak_this_;
        void set_destination_zone_id(destination_zone destination_zone_id) 
        {
            destination_channel_zone_ = destination_zone_id_.as_destination_channel();
            destination_zone_id_ = destination_zone_id;
        }

    public:
        virtual ~service_proxy()
        {
            assert(proxies_.empty());
            auto operating_zone_service = operating_zone_service_.lock();
            if(operating_zone_service)
            {
                operating_zone_service->remove_zone_proxy(destination_zone_id_);
            }
        }
        
        void add_external_ref()
        {
            std::lock_guard g(insert_control_);
            auto count = ++dependent_services_count_;
            if (auto* telemetry_service = get_telemetry_service(); telemetry_service)
            {
                telemetry_service->on_service_proxy_add_external_ref("service_proxy", zone_id_, destination_zone_id_, count, caller_zone_id_);
            }            
            assert(count >= 1);
            if(count == 1)
            {
                assert(!dependent_services_lock_);
                dependent_services_lock_ = weak_this_.lock();
            }            
        }

        void release_external_ref()
        {
            std::lock_guard g(insert_control_);
            auto count = --dependent_services_count_;
            if (auto* telemetry_service = get_telemetry_service(); telemetry_service)
            {
                telemetry_service->on_service_proxy_release_external_ref("service_proxy", zone_id_, destination_zone_id_, count, caller_zone_id_);
            }            
            assert(count >= 0);
            if(count == 0)
            {
                assert(dependent_services_lock_);
                dependent_services_lock_ = nullptr;
            }            
        }

        std::unordered_map<object, rpc::weak_ptr<object_proxy>> get_proxies(){return proxies_;}

        virtual rpc::shared_ptr<service_proxy> deep_copy_for_clone() = 0;
        rpc::shared_ptr<service_proxy> clone_for_zone(destination_zone destination_zone_id, caller_zone caller_zone_id)
        {
            auto ret = deep_copy_for_clone();
            ret->set_destination_zone_id(destination_zone_id);
            ret->caller_zone_id_ = caller_zone_id;
            ret->weak_this_ = ret;
            if (auto* telemetry_service = get_telemetry_service(); telemetry_service)
            {
                telemetry_service->on_service_proxy_creation("service_proxy", zone_id_, ret->get_destination_zone_id());
            }
            get_operating_zone_service()->inner_add_zone_proxy(ret);
            return ret;
        }

        rpc::shared_ptr<service_proxy> shared_from_this() { return rpc::shared_ptr<service_proxy>(weak_this_); }
        rpc::shared_ptr<service_proxy const> shared_from_this() const { return rpc::shared_ptr<service_proxy const>(weak_this_); }

        //the zone where this proxy is created
        zone get_zone_id() const {return zone_id_;}
        //the ultimate zone where this proxy is calling
        destination_zone get_destination_zone_id() const {return destination_zone_id_;}
        //the intermediate zone where this proxy is calling
        destination_channel_zone get_destination_channel_zone_id() const {return destination_channel_zone_;}
        caller_zone get_caller_zone_id() const {return caller_zone_id_;}

        //the service that this proxy lives in
        rpc::shared_ptr<service> get_operating_zone_service() const {return operating_zone_service_.lock();}

        //for low level logging of rpc
        const rpc::i_telemetry_service* get_telemetry_service(){return telemetry_service_;}

        void add_object_proxy(rpc::shared_ptr<object_proxy> op)
        {
            std::lock_guard l(insert_control_);
            assert(proxies_.find(op->get_object_id()) == proxies_.end());
            proxies_[op->get_object_id()] = op;
        }

        rpc::shared_ptr<object_proxy> get_object_proxy(object object_id)
        {
            std::lock_guard l(insert_control_);
            auto item = proxies_.find(object_id);
            if(item == proxies_.end())
                return nullptr;
            return item->second.lock();            
        }

        void remove_object_proxy(object object_id)
        {
            std::lock_guard l(insert_control_);
            auto item = proxies_.find(object_id);
            assert(item  != proxies_.end());
            proxies_.erase(item);       
        }

        friend service;
        friend child_service;
    };

    //declared here as object_proxy and service_proxy is not fully defined in the body of proxy_base
    template<class T>
    interface_descriptor proxy_base::proxy_bind_in_param(const rpc::shared_ptr<T>& iface, rpc::shared_ptr<rpc::object_stub>& stub)
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
        return operating_service->proxy_bind_in_param(iface, stub);
    }

    //do not use directly it is for the interface generator use rpc::create_interface_proxy if you want to get a proxied pointer to a remote implementation
    template<class T> 
    int stub_bind_in_param(rpc::service& serv, caller_channel_zone caller_channel_zone_id, caller_zone caller_zone_id, const rpc::interface_descriptor& encap, rpc::shared_ptr<T>& iface)
    {
        //if we have a null object id then return a null ptr
        if(encap.object_id == 0 || encap.destination_zone_id == 0)
        {
            return rpc::error::OK();
        }
        //if it is local to this service then just get the relevant stub
        else if(serv.get_zone_id().as_destination() == encap.destination_zone_id)
        {
            iface = serv.get_local_interface<T>(encap.object_id);
            if(!iface)
                return rpc::error::OBJECT_NOT_FOUND();
            return rpc::error::OK();
        }
        else
        {
            //get the right  service proxy
            //if the zone is different lookup or clone the right proxy
            bool new_proxy_added = false;
            auto service_proxy = serv.get_zone_proxy(caller_channel_zone_id, caller_zone_id, {encap.destination_zone_id}, new_proxy_added);
            if(!service_proxy)
                return rpc::error::OBJECT_NOT_FOUND();

            rpc::shared_ptr<object_proxy> op = service_proxy->get_object_proxy(encap.object_id);
            if(!op)
            {
                op = object_proxy::create(encap.object_id, service_proxy, true, false);
            }
            auto ret = op->query_interface(iface, false);        
            return ret;
        }        
    }

    //declared here as object_proxy and service_proxy is not fully defined in the body of proxy_base
    template<class T>
    interface_descriptor proxy_base::stub_bind_out_param(caller_channel_zone caller_channel_zone_id, caller_zone caller_zone_id, const rpc::shared_ptr<T>& iface)
    {
        if(!iface)
            return {0,0};
            
        auto operating_service = object_proxy_->get_service_proxy()->get_operating_zone_service();

        //this is to check that an interface is belonging to another zone and not the operating zone
        auto proxy = iface->query_proxy_base();
        if(proxy && proxy->get_object_proxy()->get_zone_id() != operating_service->get_zone_id())
        {
            return {proxy->get_object_proxy()->get_object_id(), proxy->get_object_proxy()->get_zone_id()};
        }

        //else encapsulate away
        return operating_service->stub_bind_out_param(caller_channel_zone_id, caller_zone_id, iface);
    }

    //do not use directly it is for the interface generator use rpc::create_interface_proxy if you want to get a proxied pointer to a remote implementation
    template<class T> 
    int proxy_bind_out_param(const rpc::shared_ptr<rpc::service_proxy>& sp, const rpc::interface_descriptor& encap, caller_zone caller_zone_id, rpc::shared_ptr<T>& val)
    {
        //if we have a null object id then return a null ptr
        if(!encap.object_id.is_set() || !encap.destination_zone_id.is_set())
            return rpc::error::OK();

        auto service_proxy = sp;
        auto serv = service_proxy->get_operating_zone_service();

        //if it is local to this service then just get the relevant stub
        if(encap.destination_zone_id == serv->get_zone_id().as_destination())
        {
            val = serv->get_local_interface<T>(encap.object_id);
            if(!val)
                return rpc::error::OBJECT_NOT_FOUND();
            return rpc::error::OK();
        }

        //get the right  service proxy
        bool new_proxy_added = false;
        if(service_proxy->get_destination_zone_id() != encap.destination_zone_id)
        {
            //if the zone is different lookup or clone the right proxy
            service_proxy = serv->get_zone_proxy(service_proxy->get_destination_zone_id().as_caller_channel(), caller_zone_id, {encap.destination_zone_id}, new_proxy_added);
            if(service_proxy && new_proxy_added)
                service_proxy->add_external_ref();
        }

        rpc::shared_ptr<object_proxy> op = service_proxy->get_object_proxy(encap.object_id);
        if(op)
        {
            //as this is an out parameter the callee will be doing an add ref if the object proxy is already found we can do a release
            assert(!new_proxy_added);
            service_proxy->release(encap.destination_zone_id, encap.object_id, caller_zone_id);
        }
        else
        {
            op = object_proxy::create(encap.object_id, service_proxy, false, new_proxy_added ? false : (encap.destination_zone_id == sp->get_destination_zone_id()));
        }
        return op->query_interface(val, false);
    }

    template<class T> 
    int demarshall_interface_proxy(const rpc::shared_ptr<rpc::service_proxy>& sp, const rpc::interface_descriptor& encap, caller_zone caller_zone_id, rpc::shared_ptr<T>& val)
    {
        //if we have a null object id then return a null ptr
        if(encap.object_id == 0 || encap.destination_zone_id == 0)
            return rpc::error::OK();

        auto service_proxy = sp;
        auto serv = service_proxy->get_operating_zone_service();

        //if it is local to this service then just get the relevant stub
        if(serv->get_zone_id().as_destination() == encap.destination_zone_id)
        {
            val = serv->get_local_interface<T>(encap.object_id);
            if(!val)
                return rpc::error::OBJECT_NOT_FOUND();
            return rpc::error::OK();
        }

        //get the right  service proxy
        bool new_proxy_added = false;
        if(service_proxy->get_destination_zone_id() != encap.destination_zone_id)
        {
            //if the zone is different lookup or clone the right proxy
            service_proxy = serv->get_zone_proxy(
                caller_zone_id.as_caller_channel(), 
                caller_zone_id, 
                encap.destination_zone_id, 
                new_proxy_added);
            if(service_proxy && new_proxy_added)
                service_proxy->add_external_ref();
        }

        rpc::shared_ptr<object_proxy> op = service_proxy->get_object_proxy(encap.object_id);
        if(!op)
        {
            op = object_proxy::create(encap.object_id, service_proxy, false, false);
        }
        return op->query_interface(val, false);
    }
}