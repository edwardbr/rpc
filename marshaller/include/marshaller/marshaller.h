#pragma once

#include <string>
#include <memory>

#include <yas/serialize.hpp>

using error_code = int;

//the base interface to all interfaces
class i_unknown;

//a shared pointer that works accross enclaves
template<class T>class remote_shared_ptr
{
	T* the_interface = nullptr;
public:
	remote_shared_ptr();
	remote_shared_ptr(T* iface);
	remote_shared_ptr<i_unknown>& as_i_unknown();
	T* operator->();
};

//a weak pointer that works accross enclaves
template<class T>class remote_weak_ptr{};

/*template<class _Tp>
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

class i_unknown
{
protected:
	i_unknown() = default;
	virtual ~i_unknown() = default;
public:

	//these functons are implemented by the code generators, there is no implementation here
	//this function is deliberately non virtual
	template<class T>
	error_code query_interface(remote_shared_ptr<T>& iface);
};

template<class T>
remote_shared_ptr<T>::remote_shared_ptr()
{}

template<class T>
remote_shared_ptr<T>::remote_shared_ptr(T* iface) : the_interface(iface)
{}

template<class T>
remote_shared_ptr<i_unknown>& remote_shared_ptr<T>::as_i_unknown()
{
	return *(remote_shared_ptr<i_unknown>*)(this);
}

template<class T>
T* remote_shared_ptr<T>::operator->()
{
	return the_interface;
}

//the used for marshalling data between zones
class i_marshaller : public i_unknown
{
public:
	virtual error_code send(uint64_t object_id, uint64_t interface_id, uint64_t method_id, size_t in_size_, const char* in_buf_, size_t out_size_, char* out_buf_) = 0;
	virtual error_code try_cast(i_unknown& from, uint64_t interface_id, remote_shared_ptr<i_unknown>& to) = 0;
};

class i_marshaller_impl : public i_marshaller
{
public:
	uint64_t eid_ = 0;
	error_code send(uint64_t object_id, uint64_t interface_id, uint64_t method_id, size_t in_size_, const char* in_buf_, size_t out_size_, char* out_buf_) override;
	error_code try_cast(i_unknown& from, uint64_t interface_id, remote_shared_ptr<i_unknown>& to) override;
};

//a handler for new threads, this function needs to be thread safe!
class i_thread_target : public i_unknown
{
public:
	virtual error_code thread_started(std::string& thread_name) = 0;
};

//a message channel between zones (a pair of spsc queues behind an executor) not thread safe!
class i_message_channel : public i_unknown{};

//a handler for new threads, this function needs to be thread safe!
class i_message_target : public i_unknown
{
public:
	//Set up a link with another zone
	virtual error_code add_peer_channel(std::string link_name, i_message_channel& channel) = 0;
	//This will be called if the other zone goes down
	virtual error_code remove_peer_channel(std::string link_name) = 0;
};

//logical security environment
class i_zone : public i_unknown
{
public:
	//this runs until the thread dies, this will also setup a connection with the message pump
	void start_thread(i_thread_target& target, std::string thread_name);
	
	//this is to allow messaging between enclaves this will create an i_message_channel
	error_code create_message_link(i_message_target& target, i_zone& other_zone, std::string link_name);
};
