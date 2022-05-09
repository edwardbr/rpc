#pragma once

#include <string>
#include <memory>
#include <unordered_map>
#include <mutex>
#include <assert.h>
#include <atomic>

#include <marshaller/marshaller.h>
#include <marshaller/remote_pointer.h>

namespace rpc
{

    class i_interface_stub;
    class object_stub;
    class service;
    class service_proxy;

    class object_stub
    {
        uint64_t id_ = 0;
        // stubs have stong pointers
        std::unordered_map<uint64_t, rpc::shared_ptr<i_interface_stub>> stub_map;
        std::mutex insert_control;
        rpc::shared_ptr<object_stub> p_this;
        std::atomic<uint64_t> reference_count = 0;
        service& zone_;

    public:
        object_stub(uint64_t id, service& zone)
            : id_(id)
            , zone_(zone)
        {
        }
        ~object_stub() {};
        uint64_t get_id() { return id_; }
        void* get_pointer();

        // this is called once the lifetime management needs to be activated
        void on_added_to_zone(rpc::shared_ptr<object_stub> stub) { p_this = stub; }

        service& get_zone() { return zone_; }

        error_code call(uint64_t interface_id, uint64_t method_id, size_t in_size_, const char* in_buf_,
                        std::vector<char>& out_buf_);
        error_code try_cast(uint64_t interface_id);

        void add_interface(rpc::shared_ptr<i_interface_stub> iface);

        uint64_t add_ref();
        uint64_t release(std::function<void()> on_delete);
    };

    class i_interface_stub
    {
    public:
        virtual uint64_t get_interface_id() = 0;
        virtual error_code call(uint64_t method_id, size_t in_size_, const char* in_buf_, std::vector<char>& out_buf_)
            = 0;
        virtual error_code cast(uint64_t interface_id, rpc::shared_ptr<i_interface_stub>& new_stub) = 0;
        virtual rpc::weak_ptr<object_stub> get_object_stub() = 0;
        virtual void* get_pointer() = 0;
    };

}