#pragma once

#include <string>
#include <memory>
#include <unordered_map>
#include <mutex>

#include <marshaller/remote_pointer.h>

#include <yas/serialize.hpp>

using error_code = int;

class i_marshaller;

class object_proxy;

class rpc_proxy;

// the used for marshalling data between zones
class i_marshaller
{
public:
    virtual error_code send(uint64_t object_id, uint64_t interface_id, uint64_t method_id, size_t in_size_,
                            const char* in_buf_, size_t out_size_, char* out_buf_)
        = 0;
    virtual error_code try_cast(uint64_t zone_id, uint64_t object_id, uint64_t interface_id) = 0;
    virtual uint64_t add_ref(uint64_t zone_id, uint64_t object_id) = 0;
    virtual uint64_t release(uint64_t zone_id, uint64_t object_id) = 0;
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
