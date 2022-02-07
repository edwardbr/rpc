#pragma once

#include <string>
#include <memory>
#include <unordered_map>
#include <mutex>

#include <marshaller/marshaller.h>

namespace rpc
{
    class rpc_proxy;
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

        template<class T1, class T2>
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
        rpc::shared_ptr<rpc_proxy> marshaller_;
        std::unordered_map<uint64_t, rpc::weak_ptr<proxy_base>> proxy_map;
        std::mutex insert_control;
        mutable rpc::weak_ptr<object_proxy> weak_this_;

        // note the interface pointer may change if there is already an interface inserted successfully
        void register_interface(uint64_t interface_id, rpc::weak_ptr<proxy_base>& value);
        object_proxy(uint64_t object_id, uint64_t zone_id, rpc::shared_ptr<rpc_proxy> marshaller)
            : object_id_(object_id)
            , zone_id_(zone_id)
            , marshaller_(marshaller)
        {
        }

    public:
        static rpc::shared_ptr<object_proxy> create(uint64_t object_id, uint64_t zone_id,
                                                    rpc::shared_ptr<rpc_proxy> marshaller)
        {
            rpc::shared_ptr<object_proxy> ret(new object_proxy(object_id, zone_id, marshaller));
            ret->weak_this_ = ret;
            return ret;
        }

        virtual ~object_proxy();

        rpc::shared_ptr<object_proxy> shared_from_this() { return rpc::shared_ptr<object_proxy>(weak_this_); }

        rpc::shared_ptr<rpc_proxy> get_zone_base() { return marshaller_; }

        error_code send(uint64_t interface_id, uint64_t method_id, size_t in_size_, const char* in_buf_,
                        size_t out_size_, char* out_buf_);

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
                error_code ret = marshaller_->try_cast(zone_id_, object_id_, T::id);
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

    class rpc_proxy : public i_marshaller
    {
        std::unordered_map<uint64_t, rpc::weak_ptr<object_proxy>> proxies;
        std::mutex insert_control;
        rpc::shared_ptr<object_proxy> root_object_proxy_;
        uint64_t zone_id_ = 0;

    protected:
        rpc_proxy() = default;
        mutable rpc::weak_ptr<rpc_proxy> weak_this_;

    public:
        rpc::shared_ptr<rpc_proxy> shared_from_this() { return rpc::shared_ptr<rpc_proxy>(weak_this_); }
        error_code set_root_object(uint64_t object_id);

        template<class T> error_code create_proxy(uint64_t object_id, rpc::shared_ptr<T>& val)
        {
            rpc::shared_ptr<object_proxy> op;
            {
                std::lock_guard l(insert_control);
                auto& item = proxies.find(object_id);
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

    class enclave_rpc_proxy : public rpc_proxy
    {
        enclave_rpc_proxy(std::string filename);

    public:
        static rpc::shared_ptr<enclave_rpc_proxy> create(std::string filename)
        {
            auto ret = rpc::shared_ptr<enclave_rpc_proxy>(new enclave_rpc_proxy(filename));
            ret->weak_this_ = rpc::static_pointer_cast<rpc_proxy>(ret);
            return ret;
        }
        uint64_t eid_ = 0;
        std::string filename_;

    public:
        ~enclave_rpc_proxy();
        error_code initialise();

        error_code send(uint64_t object_id, uint64_t interface_id, uint64_t method_id, size_t in_size_,
                        const char* in_buf_, size_t out_size_, char* out_buf_) override;
        error_code try_cast(uint64_t zone_id, uint64_t object_id, uint64_t interface_id) override;
        uint64_t add_ref(uint64_t zone_id, uint64_t object_id) override;
        uint64_t release(uint64_t zone_id, uint64_t object_id) override;
    };

    class local_rpc_proxy : public rpc_proxy
    {
        rpc::shared_ptr<i_marshaller> the_service_;
        local_rpc_proxy(rpc::shared_ptr<i_marshaller> the_service)
            : the_service_(the_service)
        {
        }

    public:
        static rpc::shared_ptr<local_rpc_proxy> create(rpc::shared_ptr<i_marshaller> the_service, uint64_t object_id)
        {
            auto ret = rpc::shared_ptr<local_rpc_proxy>(new local_rpc_proxy(the_service));
            ret->weak_this_ = rpc::static_pointer_cast<rpc_proxy>(ret);
            ret->set_root_object(object_id);
            return ret;
        }

        error_code send(uint64_t object_id, uint64_t interface_id, uint64_t method_id, size_t in_size_,
                        const char* in_buf_, size_t out_size_, char* out_buf_) override
        {
            return the_service_->send(object_id, interface_id, method_id, in_size_, in_buf_, out_size_, out_buf_);
        }
        error_code try_cast(uint64_t zone_id, uint64_t object_id, uint64_t interface_id) override
        {
            return the_service_->try_cast(zone_id, object_id, interface_id);
        }
        uint64_t add_ref(uint64_t zone_id, uint64_t object_id) override
        {
            return the_service_->add_ref(zone_id, object_id);
        }
        uint64_t release(uint64_t zone_id, uint64_t object_id) override
        {
            return the_service_->release(zone_id, object_id);
        }
    };
}