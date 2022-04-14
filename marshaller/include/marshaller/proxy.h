#pragma once

#include <string>
#include <memory>
#include <unordered_map>
#include <mutex>

#include <marshaller/marshaller.h>
#include <marshaller/service.h>

namespace rpc
{
    class service_proxy;
    class object_proxy;

    // non virtual class to allow for type erasure
    class proxy_base
    {
        rpc::shared_ptr<object_proxy> object_proxy_;

    protected:
        proxy_base(rpc::shared_ptr<object_proxy> object_proxy)
            : object_proxy_(object_proxy)
        {
        }
        virtual ~proxy_base() = default;
        rpc::shared_ptr<object_proxy> get_object_proxy() { return object_proxy_; }

        template<class T1, class T2, class proxy>
        friend rpc::shared_ptr<T1> rpc::dynamic_pointer_cast(const shared_ptr<T2>&) noexcept;
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
        rpc::shared_ptr<service_proxy> marshaller_;
        std::unordered_map<uint64_t, rpc::weak_ptr<proxy_base>> proxy_map;
        std::mutex insert_control;
        mutable rpc::weak_ptr<object_proxy> weak_this_;

        // note the interface pointer may change if there is already an interface inserted successfully
        void register_interface(uint64_t interface_id, rpc::weak_ptr<proxy_base>& value);
        object_proxy(uint64_t object_id, uint64_t zone_id, rpc::shared_ptr<service_proxy> marshaller)
            : object_id_(object_id)
            , zone_id_(zone_id)
            , marshaller_(marshaller)
        {
        }

        error_code try_cast(uint64_t interface_id);

    public:
        static rpc::shared_ptr<object_proxy> create(uint64_t object_id, uint64_t zone_id,
                                                    rpc::shared_ptr<service_proxy> marshaller)
        {
            rpc::shared_ptr<object_proxy> ret(new object_proxy(object_id, zone_id, marshaller));
            ret->weak_this_ = ret;
            return ret;
        }

        virtual ~object_proxy();

        rpc::shared_ptr<object_proxy> shared_from_this() { return rpc::shared_ptr<object_proxy>(weak_this_); }

        rpc::shared_ptr<service_proxy> get_zone_base() { return marshaller_; }

        error_code send(uint64_t interface_id, uint64_t method_id, size_t in_size_, const char* in_buf_,
                        std::vector<char>& out_buf_);

        template<class T> void create_interface_proxy(rpc::shared_ptr<T>& inface);

        template<class T> error_code query_interface(rpc::shared_ptr<T>& iface, bool do_remote_check = true)
        {
            auto create = [&](std::unordered_map<uint64_t, rpc::weak_ptr<proxy_base>>::iterator item) -> error_code {
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
                error_code ret = try_cast(T::id);
                if (ret != 0)
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

    // the class that encapsulates an environment or zone
    // only host code can use this class directly other enclaves *may* have access to the i_marshaller derived interface

    class service_proxy : public i_marshaller
    {
        std::unordered_map<uint64_t, rpc::weak_ptr<object_proxy>> proxies;
        std::mutex insert_control;
        rpc::shared_ptr<object_proxy> root_object_proxy_;
        rpc::shared_ptr<service> service_;
        uint64_t zone_id_ = 0;

    protected:
        service_proxy(const rpc::shared_ptr<service>& service, uint64_t zone_id) : service_(service), zone_id_(zone_id){}
        mutable rpc::weak_ptr<service_proxy> weak_this_;

    public:
        virtual ~service_proxy(){service_->remove_zone(zone_id_);}
        rpc::shared_ptr<service_proxy> shared_from_this() { return rpc::shared_ptr<service_proxy>(weak_this_); }
        error_code set_root_object(uint64_t object_id);

        service& get_service(){return *service_;}
        uint64_t get_zone_id(){return zone_id_;}

        template<class T> error_code create_proxy(uint64_t object_id, rpc::shared_ptr<T>& val)
        {
            rpc::shared_ptr<object_proxy> op;
            {
                std::lock_guard l(insert_control);
                auto item = proxies.find(object_id);
                if (item != proxies.end())
                    op = item->second.lock();
                else
                {
                    op = object_proxy::create(object_id, 0, shared_from_this());
                    proxies[object_id] = op;
                }
            }
            return op->query_interface(val, false);
        }
        template<class T> rpc::shared_ptr<T> get_interface()
        {
            auto tmp = root_object_proxy_;
            if (!tmp)
                return rpc::shared_ptr<T>();
            rpc::shared_ptr<T> out;
            tmp->query_interface(out);
            return out;
        }
    };



}