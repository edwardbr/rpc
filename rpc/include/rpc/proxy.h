#pragma once

#include <string>
#include <memory>
#include <unordered_map>
#include <mutex>

#include <rpc/marshaller.h>
#include <rpc/service.h>
#include <rpc/remote_pointer.h>
#include <rpc/i_telemetry_service.h>
namespace rpc
{
    class service;
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
        rpc::shared_ptr<object_proxy> get_object_proxy() const { return object_proxy_; }

        template<class T>
        uint64_t encapsulate_outbound_interfaces(const rpc::shared_ptr<T>& iface);

        template<class T>
        uint64_t get_interface_zone_id(const rpc::shared_ptr<T>& iface);

        template<class T1, class T2, class proxy>
        friend rpc::shared_ptr<T1> dynamic_pointer_cast(const shared_ptr<T2>& from) noexcept;
    };

    template<class T> class proxy_impl : public proxy_base, public T
    {
    public:
        proxy_impl(rpc::shared_ptr<object_proxy> object_proxy)
            : proxy_base(object_proxy)
        {
        }
        virtual ~proxy_impl() = default;
    };

    class object_proxy
    {
        uint64_t object_id_;
        uint64_t zone_id_;
        rpc::shared_ptr<service_proxy> service_proxy_;
        std::unordered_map<uint64_t, rpc::weak_ptr<proxy_base>> proxy_map;
        std::mutex insert_control;
        mutable rpc::weak_ptr<object_proxy> weak_this_;

        // note the interface pointer may change if there is already an interface inserted successfully
        void register_interface(uint64_t interface_id, rpc::weak_ptr<proxy_base>& value);
        object_proxy(uint64_t object_id, uint64_t zone_id, rpc::shared_ptr<service_proxy> service_proxy);

        int try_cast(uint64_t interface_id);

    public:
        static rpc::shared_ptr<object_proxy> create(uint64_t object_id, uint64_t zone_id,
                                                    rpc::shared_ptr<service_proxy> service_proxy)
        {
            rpc::shared_ptr<object_proxy> ret(new object_proxy(object_id, zone_id, service_proxy));
            ret->weak_this_ = ret;
            return ret;
        }

        virtual ~object_proxy();

        rpc::shared_ptr<object_proxy const> shared_from_this() const { return rpc::shared_ptr<object_proxy const>(weak_this_); }
        rpc::shared_ptr<object_proxy> shared_from_this() { return rpc::shared_ptr<object_proxy>(weak_this_); }

        rpc::shared_ptr<service_proxy> get_service_proxy() const { return service_proxy_; }
        uint64_t get_object_id() const {return object_id_;}
        uint64_t get_zone_id() const {return zone_id_;}

        int send(uint64_t interface_id, uint64_t method_id, size_t in_size_, const char* in_buf_,
                        std::vector<char>& out_buf_);

        size_t get_proxy_count()
        {
            std::lock_guard guard(insert_control);
            return proxy_map.size();
        }

        template<class T> void create_interface_proxy(rpc::shared_ptr<T>& inface);

        template<class T> int query_interface(rpc::shared_ptr<T>& iface, bool do_remote_check = true)
        {
            auto create = [&](std::unordered_map<uint64_t, rpc::weak_ptr<proxy_base>>::iterator item) -> int {
                rpc::shared_ptr<proxy_base> proxy = item->second.lock();
                if (!proxy)
                {
                    // weak pointer needs refreshing
                    create_interface_proxy<T>(iface);
                    item->second = rpc::reinterpret_pointer_cast<proxy_base>(iface);
                    return 0;
                }
                iface = rpc::reinterpret_pointer_cast<T>(proxy);
                return 0;
            };

            { // scope for the lock
                std::lock_guard guard(insert_control);
                if(T::id == 0)
                {
                    return 0;
                }
                auto item = proxy_map.find(T::id);
                if (item != proxy_map.end())
                {
                    return create(item);
                }
                if (!do_remote_check)
                {
                    create_interface_proxy<T>(iface);
                    proxy_map[T::id] = rpc::reinterpret_pointer_cast<proxy_base>(iface);
                    return 0;
                }
            }

            // release the lock and then check for casting
            if (do_remote_check)
            {
                // see if object_id can implement interface
                int ret = try_cast(T::id);
                if (ret != rpc::error::OK())
                {
                    return ret;
                }
            }
            { // another scope for the lock
                std::lock_guard guard(insert_control);

                // check again...
                auto item = proxy_map.find(T::id);
                if (item != proxy_map.end())
                {
                    return create(item);
                }

                create_interface_proxy<T>(iface);
                proxy_map[T::id] = rpc::reinterpret_pointer_cast<proxy_base>(iface);
                return 0;
            }
        }
    };


	//this bundle of joy implements a non locking pointer where if the remote object is not there
    //it will return an error without crashing
	template<class T> class optimistic_proxy_impl : public T
	{
		uint64_t object_id_;
		uint64_t zone_id_;
		rpc::weak_ptr<service_proxy> service_proxy_;
	public:
		optimistic_proxy_impl(rpc::shared_ptr<object_proxy> object_proxy)
			: object_id_(object_proxy->get_object_id())
			, zone_id_(object_proxy->get_zone_id())
			, service_proxy_(object_proxy->get_service_proxy())
		{}
		virtual ~optimistic_proxy_impl() = default;
		uint64_t get_object_id() const { return object_id_; }
		uint64_t get_zone_id() const { return zone_id_; }
		rpc::weak_ptr<service_proxy> get_service_proxy() const { return service_proxy_; }
	};

    // the class that encapsulates an environment or zone
    // only host code can use this class directly other enclaves *may* have access to the i_service_proxy derived interface

    class service_proxy : public i_marshaller
    {
        std::unordered_map<uint64_t, rpc::weak_ptr<object_proxy>> proxies;
        std::mutex insert_control;
        rpc::weak_ptr<service> service_;
        uint64_t zone_id_ = 0;
        uint64_t operating_zone_id = 0;
        rpc::weak_ptr<service> operating_zone_service_;
        const rpc::i_telemetry_service* const telemetry_service_ = nullptr;

    protected:

        service_proxy(  const rpc::shared_ptr<service>& serv, 
                        const rpc::shared_ptr<service>& operating_zone_service,
                        const rpc::i_telemetry_service* telemetry_service) : 
            service_(serv), 
            zone_id_(serv->get_zone_id()),
            operating_zone_id(operating_zone_service ? operating_zone_service->get_zone_id() : 0),
            operating_zone_service_(operating_zone_service),
            telemetry_service_(telemetry_service)
        {}
        service_proxy(  uint64_t zone_id, 
                        const rpc::shared_ptr<service>& operating_zone_service,
                        const rpc::i_telemetry_service* telemetry_service) : 
            zone_id_(zone_id),
            operating_zone_id(operating_zone_service ? operating_zone_service->get_zone_id() : 0),
            operating_zone_service_(operating_zone_service),
            telemetry_service_(telemetry_service)
        {}
        mutable rpc::weak_ptr<service_proxy> weak_this_;

    public:
        virtual ~service_proxy()
        {
            auto srv = service_.lock();
            if(srv)
            {
                srv->remove_zone_proxy(zone_id_);
            }
        }
        rpc::shared_ptr<service_proxy> shared_from_this() { return rpc::shared_ptr<service_proxy>(weak_this_); }
        rpc::shared_ptr<service_proxy const> shared_from_this() const { return rpc::shared_ptr<service_proxy const>(weak_this_); }

        service& get_service() const {return *service_.lock();}
        uint64_t get_zone_id() const {return zone_id_;}
        uint64_t get_operating_zone_id() const {return operating_zone_id;}
        rpc::shared_ptr<service> get_operating_zone_service() const {return operating_zone_service_.lock();}
        const rpc::i_telemetry_service* get_telemetry_service(){return telemetry_service_;}

        template<class T> 
        int create_proxy(uint64_t object_id, rpc::shared_ptr<T>& val, uint64_t zone_id = 0)
        {
            rpc::shared_ptr<object_proxy> op;
            {
                std::lock_guard l(insert_control);
                auto item = proxies.find(object_id);
                if (item != proxies.end())
                    op = item->second.lock();
                else
                {
                    if(!zone_id)
                        zone_id = zone_id_;
                    op = object_proxy::create(object_id, zone_id, shared_from_this());
                    proxies[object_id] = op;
                }
            }
            return op->query_interface(val, false);
        }
    };

    //declared here as object_proxy and service_proxy is not fully defined in the body of proxy_base
    template<class T>
    uint64_t proxy_base::encapsulate_outbound_interfaces(const rpc::shared_ptr<T>& iface)
    {
        if(!iface)
            return 0;
            
        auto operating_service = object_proxy_->get_service_proxy()->get_operating_zone_service();

        //this is to check that an interface is belonging to another zone and not the operating zone
        auto cast = dynamic_cast<proxy_base*>(iface.get());
        if(cast && cast->get_object_proxy()->get_zone_id() != operating_service->get_zone_id())
        {
            return cast->get_object_proxy()->get_object_id();
        }

        //else encapsulate away
        return operating_service->encapsulate_outbound_interfaces(iface);
    }

    template<class T>
    uint64_t proxy_base::get_interface_zone_id(const rpc::shared_ptr<T>& iface)
    {
        if(!iface)
            return 0;
        //fist check if this shared pointer is a remote one
        auto* impl = dynamic_cast<rpc::proxy_impl<T>*>(iface.get());
        if(impl)
        {
            return impl->object_proxy_->get_zone_id();
        }
        //else get the zone id of the service that runs this zone or enclave
        return object_proxy_->get_service_proxy()->get_operating_zone_id();
    }
}