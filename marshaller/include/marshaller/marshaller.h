#pragma once

#include <string>
#include <memory>

#include <yas/serialize.hpp>

using error_code = int;

class enclave_info;

//the base interface to all interfaces
class i_unknown{};

//a shared pointer that works accross enclaves
template<class T>class remote_shared_ptr
{
	public:
	remote_shared_ptr<i_unknown>& as_i_unknown()
	{
		return *(remote_shared_ptr<i_unknown>*)(this);
	}
};

//a weak pointer that works accross enclaves
template<class T>class remote_weak_ptr{};

//the used for marshalling data between zones
class i_marshaller : public i_unknown
{
	public:
	virtual error_code send(int object_id, int interface_id, int method_id, const yas::shared_buffer& in, yas::shared_buffer& out) = 0;
	virtual error_code try_cast(i_unknown& from, int interface_id, remote_shared_ptr<i_unknown>& to);
};

//a handler for new threads, this function needs to be thread safe!
class i_thread_target : public i_unknown
{
	virtual error_code thread_started(std::string& thread_name) = 0;
};

//a message channel between zones (a pair of spsc queues behind an executor) not thread safe!
class i_message_channel : public i_unknown{};

//a handler for new threads, this function needs to be thread safe!
class i_message_target : public i_unknown
{
	//Set up a link with another zone
	virtual error_code add_peer_channel(std::string link_name, i_message_channel& channel) = 0;
	//This will be called if the other zone goes down
	virtual error_code remove_peer_channel(std::string link_name) = 0;
};

//logical security environment
class i_zone : public i_unknown
{
	//this runs until the thread dies, this will also setup a connection with the message pump
	void start_thread(i_thread_target& target, std::string thread_name);
	
	//this is to allow messaging between enclaves this will create an i_message_channel
	error_code create_message_link(i_message_target& target, i_zone& other_zone, std::string link_name);
};
