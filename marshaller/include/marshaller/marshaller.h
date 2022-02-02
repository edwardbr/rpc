#pragma once

#include <string>
#include <memory>
#include <unordered_map>
#include <mutex>

#include <marshaller/remote_pointer.h>

#include <yas/serialize.hpp>

using error_code = int;

// the base interface to all interfaces
class i_marshaller;

class object_proxy;

class i_proxy_impl
{
protected:
    i_proxy_impl(std::shared_ptr<object_proxy> object_proxy)
        : object_proxy_(object_proxy)
    {
    }
    std::shared_ptr<object_proxy> object_proxy_;
    virtual ~i_proxy_impl() = default;
    std::shared_ptr<object_proxy> get_object_proxy() { return object_proxy_; }

    template<class T1, class T2>
    friend rpc_cpp::shared_ptr<T1> rpc_cpp::dynamic_pointer_cast(const shared_ptr<T2>&) noexcept;
};

template<class T> class i_proxy : public i_proxy_impl, public T
{
public:
    i_proxy(std::shared_ptr<object_proxy> object_proxy)
        : i_proxy_impl(object_proxy)
    {
    }
    virtual ~i_proxy() = default;
};

class object_proxy : public std::enable_shared_from_this<object_proxy>
{
    uint64_t object_id_;
    uint64_t zone_id_;
    std::shared_ptr<i_marshaller> marshaller_;
    std::unordered_map<uint64_t, rpc_cpp::weak_ptr<i_proxy_impl>> proxy_map;
    std::mutex insert_control;

    // note the interface pointer may change if there is already an interface inserted successfully
    void register_interface(uint64_t interface_id, rpc_cpp::weak_ptr<i_proxy_impl>& value);

public:
    object_proxy(uint64_t object_id, uint64_t zone_id, std::shared_ptr<i_marshaller> marshaller)
        : object_id_(object_id)
        , zone_id_(zone_id)
        , marshaller_(marshaller)
    {
    }
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

// the used for marshalling data between zones
class i_marshaller
{
public:
    virtual error_code send(uint64_t object_id, uint64_t interface_id, uint64_t method_id, size_t in_size_,
                            const char* in_buf_, size_t out_size_, char* out_buf_)
        = 0;
    virtual error_code try_cast(uint64_t zone_id_, uint64_t object_id, uint64_t interface_id) = 0;
};

class i_interface_marshaller
{
public:
    virtual uint64_t get_interface_id() = 0;
    virtual error_code call(uint64_t method_id, size_t in_size_, const char* in_buf_, size_t out_size_, char* out_buf_)
        = 0;
    virtual error_code cast(uint64_t interface_id, std::shared_ptr<i_interface_marshaller>& new_stub) = 0;
};

class object_stub
{
    // stubs have stong pointers
    std::unordered_map<uint64_t, std::shared_ptr<i_interface_marshaller>> proxy_map;
    std::mutex insert_control;

public:
    object_stub(std::shared_ptr<i_interface_marshaller> initial_interface)
    {
        proxy_map[initial_interface->get_interface_id()] = initial_interface;
    }
    error_code call(uint64_t interface_id, uint64_t method_id, size_t in_size_, const char* in_buf_, size_t out_size_,
                    char* out_buf_)
    {
        error_code ret = -1;
        std::lock_guard l(insert_control);
        auto item = proxy_map.find(interface_id);
        if (item != proxy_map.end())
        {
            ret = item->second->call(method_id, in_size_, in_buf_, out_size_, out_buf_);
        }
        return ret;
    }
    error_code try_cast(uint64_t interface_id)
    {
        error_code ret = 0;
        std::lock_guard l(insert_control);
        auto item = proxy_map.find(interface_id);
        if (item == proxy_map.end())
        {
            std::shared_ptr<i_interface_marshaller> new_stub;
            ret = proxy_map.begin()->second->cast(interface_id, new_stub);
            if (!ret)
            {
                proxy_map.emplace(interface_id, std::move(new_stub));
            }
        }
        return ret;
    }
};

// a handler for new threads, this function needs to be thread safe!
class i_thread_target
{
public:
    virtual error_code thread_started(std::string& thread_name) = 0;
};

// a message channel between zones (a pair of spsc queues behind an executor) not thread safe!
class i_message_channel
{
};

// a handler for new threads, this function needs to be thread safe!
class i_message_target
{
public:
    // Set up a link with another zone
    virtual error_code add_peer_channel(std::string link_name, i_message_channel& channel) = 0;
    // This will be called if the other zone goes down
    virtual error_code remove_peer_channel(std::string link_name) = 0;
};

// logical security environment
class i_zone
{
public:
    // this runs until the thread dies, this will also setup a connection with the message pump
    void start_thread(i_thread_target& target, std::string thread_name);

    // this is to allow messaging between enclaves this will create an i_message_channel
    error_code create_message_link(i_message_target& target, i_zone& other_zone, std::string link_name);
};

struct zone_config;

// the class that encapsulates an environment or zone
// only host code can use this class directly other enclaves *may* have access to the i_zone derived interface
class zone_base : public i_marshaller
{
    uint64_t eid_ = 0;
    std::string filename_;

public:
    zone_base(std::string filename);
    ~zone_base();
    error_code load(zone_config& config);

    error_code send(uint64_t object_id, uint64_t interface_id, uint64_t method_id, size_t in_size_, const char* in_buf_,
                    size_t out_size_, char* out_buf_);
    error_code try_cast(uint64_t zone_id_, uint64_t object_id, uint64_t interface_id);
};

template<class T, class TProxy> class zone : public zone_base, public rpc_cpp::enable_shared_from_this<zone<T, TProxy>>
{
public:
    zone(std::string filename)
        : zone_base(filename)
    {
    }

    rpc_cpp::shared_ptr<T> get_remote_interface()
    {
        auto op = std::make_shared<object_proxy>(0, 0, shared_from_this());
        return rpc_cpp::shared_ptr<T>(new TProxy(op));
    }
};
