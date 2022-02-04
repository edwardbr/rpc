#pragma once

#include <string>
#include <memory>
#include <unordered_map>
#include <mutex>
#include <assert.h>

#include <marshaller/marshaller.h>

class i_interface_stub;
class zone_stub;
class object_stub;

class object_stub
{
    uint64_t id_ = 0;
    // stubs have stong pointers
    std::unordered_map<uint64_t, rpc_cpp::shared_ptr<i_interface_stub>> proxy_map;
    std::mutex insert_control;
    rpc_cpp::shared_ptr<object_stub> p_this;
    std::atomic<uint64_t> reference_count = 0;
    zone_stub& zone_;

public:
    object_stub(uint64_t id, zone_stub& zone)
        : id_(id)
        , zone_(zone)
    {
    }
    ~object_stub(){};
    uint64_t get_id() { return id_; }
    void* get_pointer();
    
    //this is called once the lifetime management needs to be activated
    void on_added_to_zone(rpc_cpp::shared_ptr<object_stub> stub){p_this = stub;}

    zone_stub& get_zone() { return zone_; }

    error_code call(uint64_t interface_id, uint64_t method_id, size_t in_size_, const char* in_buf_, size_t out_size_,
                    char* out_buf_);
    error_code try_cast(uint64_t interface_id);

    void add_interface(rpc_cpp::shared_ptr<i_interface_stub> iface);

    uint64_t add_ref();
    uint64_t release(std::function<void()> on_delete);
};

class i_interface_stub
{
public:
    virtual uint64_t get_interface_id() = 0;
    virtual error_code call(uint64_t method_id, size_t in_size_, const char* in_buf_, size_t out_size_, char* out_buf_)
        = 0;
    virtual error_code cast(uint64_t interface_id, rpc_cpp::shared_ptr<i_interface_stub>& new_stub) = 0;
    virtual rpc_cpp::weak_ptr<object_stub> get_target_stub() = 0;
    virtual void* get_pointer() = 0;
};

// responsible for all object lifetimes created within the zone
class zone_stub : public i_marshaller
{
    uint64_t zone_id = 0;
    std::atomic<uint64_t> object_id_generator;

    // map object_id's to stubs
    std::unordered_map<uint64_t, rpc_cpp::weak_ptr<object_stub>> stubs;
    // map wrapped objects pointers to stubs
    std::unordered_map<void*, rpc_cpp::weak_ptr<object_stub>> wrapped_object_to_stub;
    std::mutex insert_control;

    // hard lock on the root object
    rpc_cpp::shared_ptr<i_interface_stub> root_stub;

public:
    virtual ~zone_stub();
    template<class T, class Stub> error_code initialise(rpc_cpp::shared_ptr<T> root_ob)
    {
        assert(check_is_empty());

        auto id = get_new_object_id();
        auto os = rpc_cpp::shared_ptr<object_stub>(new object_stub(id, *this));
        root_stub = rpc_cpp::shared_ptr<i_interface_stub>(new Stub(root_ob, os));
        os->add_interface(root_stub);
        add_object(root_ob.get(), os);
        return 0;
    }

    //this function is needed by services where there is no shared pointer to this object, and its lifetime
    void shutdown();
    bool check_is_empty();
    uint64_t get_root_object_id();
    uint64_t get_new_object_id() { return object_id_generator++; }

    template<class T> uint64_t encapsulate_outbound_interfaces(rpc_cpp::shared_ptr<T> object);

    error_code send(uint64_t object_id, uint64_t interface_id, uint64_t method_id, size_t in_size_, const char* in_buf_,
                    size_t out_size_, char* out_buf_) override;

    uint64_t add_lookup_stub(void* pointer,
                             std::function<rpc_cpp::shared_ptr<i_interface_stub>(rpc_cpp::shared_ptr<object_stub>)> fn);
    error_code add_object(void* pointer, rpc_cpp::shared_ptr<object_stub> stub);
    rpc_cpp::weak_ptr<object_stub> get_object(uint64_t object_id);

    error_code try_cast(uint64_t zone_id, uint64_t object_id, uint64_t interface_id) override;
    uint64_t add_ref(uint64_t zone_id, uint64_t object_id) override;
    uint64_t release(uint64_t zone_id, uint64_t object_id) override;
};