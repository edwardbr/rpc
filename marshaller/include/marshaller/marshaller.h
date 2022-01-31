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

// a shared pointer that works accross enclaves
/*template<class T>class remote_shared_ptr
{
        T* the_interface = nullptr;
public:
        remote_shared_ptr();
        remote_shared_ptr(T* iface);
        T* operator->();
};

//a weak pointer that works accross enclaves
template<class T>
class remote_weak_ptr
{
        T* the_interface = nullptr;
public:
        T* operator->();
        const T* get() const {return the_interface;}

        T* get(){return the_interface;}


};

template<class T>
bool operator == (const remote_weak_ptr<T>& first, const remote_weak_ptr<T>& second)
{
        return first.get() == second.the_interface.get();
}

template<class T>
bool operator == (const remote_weak_ptr<T>& first, std::nullptr_t second)
{
        return first.get() == nullptr;
}

template<class T>
bool operator == (std::nullptr_t first, const remote_weak_ptr<T>& second)
{
        return second.get() == nullptr;
}

template<class _Tp>
class enable_shared_remote_from_this
{
    mutable weak_ptr<_Tp> __weak_this_;
protected:
    constexpr enable_shared_remote_from_this() noexcept {}
    enable_shared_remote_from_this( const enable_shared_remote_from_this<T>&obj ) noexcept {}
    enable_shared_remote_from_this& operator=(enable_shared_remote_from_this const&) noexcept
        {return *this;}
    ~enable_shared_remote_from_this() {}
public:
    shared_ptr<_Tp> shared_from_this()
        {return shared_ptr<_Tp>(__weak_this_);}
    shared_ptr<_Tp const> shared_from_this() const
        {return shared_ptr<const _Tp>(__weak_this_);}

    template <class _Up> friend class shared_ptr;
};*/

class object_proxy;

class i_proxy
{
protected:
    std::shared_ptr<object_proxy> object_proxy_;

public:
    i_proxy(std::shared_ptr<object_proxy> object_proxy)
        : object_proxy_(object_proxy)
    {
    }
    virtual ~i_proxy() = default;

    std::shared_ptr<object_proxy> get_object_proxy() { return object_proxy_; }
};

class object_proxy : std::enable_shared_from_this<object_proxy>
{
    uint64_t object_id_;
    uint64_t zone_id_;
    std::shared_ptr<i_marshaller> marshaller_;
    std::unordered_map<uint64_t, rpc_cpp::remote_weak_ptr<i_proxy>> proxy_map;
    std::mutex insert_control;

    // note the interface pointer may change if there is already an interface inserted successfully
    void register_interface(uint64_t interface_id, rpc_cpp::remote_weak_ptr<i_proxy>& value);

public:
    object_proxy(uint64_t object_id, uint64_t zone_id, std::shared_ptr<i_marshaller> marshaller)
        : object_id_(object_id)
        , zone_id_(zone_id)
        , marshaller_(marshaller)
    {
    }
    error_code send(uint64_t interface_id, uint64_t method_id, size_t in_size_, const char* in_buf_, size_t out_size_,
                    char* out_buf_);

    template<class T> void create_interface_proxy(rpc_cpp::remote_shared_ptr<T>& inface);

    template<class T> error_code query_interface(rpc_cpp::remote_shared_ptr<T>& iface)
    {
		auto create = [&](std::unordered_map<uint64_t, rpc_cpp::remote_weak_ptr<i_proxy>>::iterator item) -> error_code
		{
			rpc_cpp::remote_shared_ptr<i_proxy> proxy = item->second.lock();
			if (!proxy)
			{
				//weak pointer needs refreshing
				create_interface_proxy<T>(iface);
				item->second = rpc_cpp::reinterpret_pointer_cast<i_proxy>(iface);
				return 0;
			}
			iface = rpc_cpp::reinterpret_pointer_cast<T>(proxy);
			return 0;
		};
        {
            std::lock_guard guard(insert_control);
            auto item = proxy_map.find(T::id);
            if (item != proxy_map.end())
            {
				return create(item);
            }
        }
        {
			//see if object_id can implement interface
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
            proxy_map[T::id] = rpc_cpp::reinterpret_pointer_cast<i_proxy>(iface);
            return 0;
        }
    }
};

/*template<class T>
remote_shared_ptr<T>::remote_shared_ptr()
{}

template<class T>
remote_shared_ptr<T>::remote_shared_ptr(T* iface) : the_interface(iface)
{}

template<class T>
T* remote_shared_ptr<T>::operator->()
{
        return the_interface;
}
template<class T>
T* remote_weak_ptr<T>::operator->()
{
        return the_interface;
}*/

// the used for marshalling data between zones
class i_marshaller
{
public:
    virtual error_code send(uint64_t object_id, uint64_t interface_id, uint64_t method_id, size_t in_size_,
                            const char* in_buf_, size_t out_size_, char* out_buf_)
        = 0;
    virtual error_code try_cast(uint64_t zone_id_, uint64_t object_id, uint64_t interface_id) = 0;
};

class i_marshaller_impl : public i_marshaller
{
public:
    uint64_t eid_ = 0;
    error_code send(uint64_t object_id, uint64_t interface_id, uint64_t method_id, size_t in_size_, const char* in_buf_,
                    size_t out_size_, char* out_buf_) override;
    error_code try_cast(uint64_t zone_id_, uint64_t object_id, uint64_t interface_id) override;
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
