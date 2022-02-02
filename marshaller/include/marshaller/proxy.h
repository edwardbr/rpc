#pragma once

#include <string>
#include <memory>
#include <unordered_map>
#include <mutex>

#include <marshaller/marshaller.h>

//non virtual class to allow for type erasure
class i_proxy_impl
{
    rpc_cpp::shared_ptr<object_proxy> object_proxy_;
protected:
    i_proxy_impl(rpc_cpp::shared_ptr<object_proxy> object_proxy)
        : object_proxy_(object_proxy)
    {
    }
    virtual ~i_proxy_impl() = default;
    rpc_cpp::shared_ptr<object_proxy> get_object_proxy() { return object_proxy_; }

    template<class T1, class T2>
    friend rpc_cpp::shared_ptr<T1> rpc_cpp::dynamic_pointer_cast(const shared_ptr<T2>&) noexcept;
};

template<class T> class i_proxy : public i_proxy_impl, public T
{
public:
    i_proxy(rpc_cpp::shared_ptr<object_proxy> object_proxy)
        : i_proxy_impl(object_proxy)
    {
    }
    virtual ~i_proxy() = default;
};

class object_proxy : public std::enable_shared_from_this<object_proxy>
{
    uint64_t object_id_;
    uint64_t zone_id_;
    rpc_cpp::shared_ptr<zone_base_proxy> marshaller_;
    std::unordered_map<uint64_t, rpc_cpp::weak_ptr<i_proxy_impl>> proxy_map;
    std::mutex insert_control;

    // note the interface pointer may change if there is already an interface inserted successfully
    void register_interface(uint64_t interface_id, rpc_cpp::weak_ptr<i_proxy_impl>& value);

public:
    object_proxy(uint64_t object_id, uint64_t zone_id, rpc_cpp::shared_ptr<zone_base_proxy> marshaller)
        : object_id_(object_id)
        , zone_id_(zone_id)
        , marshaller_(marshaller)
    {
    }

    rpc_cpp::shared_ptr<zone_base_proxy> get_zone_base() {return marshaller_;}

    error_code send(uint64_t interface_id, uint64_t method_id, size_t in_size_, const char* in_buf_, size_t out_size_,
                    char* out_buf_);

    template<class T> void create_interface_proxy(rpc_cpp::shared_ptr<T>& inface);

    template<class T> error_code query_interface(rpc_cpp::shared_ptr<T>& iface)
    {
        auto create = [&](std::unordered_map<uint64_t, rpc_cpp::weak_ptr<i_proxy_impl>>::iterator item) -> error_code {
            rpc_cpp::shared_ptr<i_proxy_impl> proxy = item->second.lock();
            if (!proxy)
            {
                // weak pointer needs refreshing
                create_interface_proxy<T>(iface);
                item->second = rpc_cpp::reinterpret_pointer_cast<i_proxy_impl>(iface);
                return 0;
            }
            iface = rpc_cpp::reinterpret_pointer_cast<T>(proxy);
            return 0;
        };

        /*T* t = dynamic_cast<T>(iface.get());
        if(t)
        {
                iface = rpc_cpp::shared_ptr<T>();
        }*/

        {
            std::lock_guard guard(insert_control);
            auto item = proxy_map.find(T::id);
            if (item != proxy_map.end())
            {
                return create(item);
            }
        }
        {
            // see if object_id can implement interface
            error_code ret = marshaller_->try_cast(zone_id_, object_id_, T::id);
            if (ret != 0)
            {
                return ret;
            }
        }
        {
            std::lock_guard guard(insert_control);

            // check again...
            auto item = proxy_map.find(T::id);
            if (item != proxy_map.end())
            {
                return create(item);
            }

            create_interface_proxy<T>(iface);
            proxy_map[T::id] = rpc_cpp::reinterpret_pointer_cast<i_proxy_impl>(iface);
            return 0;
        }
    }
};



// the class that encapsulates an environment or zone
// only host code can use this class directly other enclaves *may* have access to the i_zone derived interface
class zone_base_proxy : public i_marshaller, public rpc_cpp::enable_shared_from_this<zone_base_proxy>
{
    std::unordered_map<uint64_t, rpc_cpp::weak_ptr<object_proxy>> proxies;
    std::mutex insert_control;
public:
    template<class T> error_code create_proxy(uint64_t object_id, rpc_cpp::shared_ptr<T>& val)
    {
        rpc_cpp::shared_ptr<object_proxy> op;
        {
            std::lock_guard l(insert_control);
            auto& item = proxies.find(object_id);
            if(item != proxies.end())
                op = item->second.lock();
            else
            {
                op = rpc_cpp::shared_ptr<object_proxy>(new object_proxy(object_id, 0, shared_from_this()));
                proxies[object_id] = op;
            }
        }
        return op->query_interface(val);
    }
};

struct zone_config;

class enclave_zone_proxy : public zone_base_proxy
{
    uint64_t eid_ = 0;
    std::string filename_;

public:
    enclave_zone_proxy(std::string filename);
    ~enclave_zone_proxy();
    error_code load(zone_config& config);

    error_code send(uint64_t object_id, uint64_t interface_id, uint64_t method_id, size_t in_size_, const char* in_buf_,
                    size_t out_size_, char* out_buf_);
    error_code try_cast(uint64_t zone_id_, uint64_t object_id, uint64_t interface_id);

};


template<class T, class TProxy> class zone_proxy : public enclave_zone_proxy
{
public:
    zone_proxy(std::string filename)
        : enclave_zone_proxy(filename)
    {
    }

    rpc_cpp::shared_ptr<T> get_remote_interface()
    {
        auto op = std::make_shared<object_proxy>(0, 0, shared_from_this());
        return rpc_cpp::shared_ptr<T>(new TProxy(op));
    }
};
