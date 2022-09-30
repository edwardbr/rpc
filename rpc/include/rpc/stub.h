#pragma once

#include <string>
#include <memory>
#include <unordered_map>
#include <mutex>
#include <assert.h>
#include <atomic>

#include <rpc/marshaller.h>
#include <rpc/remote_pointer.h>

namespace rpc
{

    class i_interface_stub;
    class object_stub;
    class service;
    class service_proxy;
    class casting_interface;

    class object_stub
    {
        uint64_t id_ = 0;
        // stubs have stong pointers
        std::unordered_map<uint64_t, rpc::shared_ptr<i_interface_stub>> stub_map;
        std::mutex insert_control;
        rpc::shared_ptr<object_stub> p_this;
        std::atomic<uint64_t> reference_count = 0;
        service& zone_;
		void* pointer_ = 0x0;//necessary because of stub_map ambiguity

    public:
        object_stub(uint64_t id, service& zone,void* pointer)
            : id_(id)
            , zone_(zone)
			, pointer_(pointer)
        {
        }
        ~object_stub();
        uint64_t get_id() const
        {
            return id_; 
        }
        void* get_pointer() const;

        // this is called once the lifetime management needs to be activated
        void on_added_to_zone(rpc::shared_ptr<object_stub> stub) { p_this = stub; }

        service& get_zone() const { return zone_; }

        int call(uint64_t interface_id, uint64_t method_id, size_t in_size_, const char* in_buf_,
                        std::vector<char>& out_buf_);
        int try_cast(uint64_t interface_id);

        void add_interface(const rpc::shared_ptr<i_interface_stub>& iface);
        rpc::shared_ptr<i_interface_stub> get_interface(uint64_t interface_id);

        uint64_t add_ref();
        uint64_t release(std::function<void()> on_delete);
    };

    class i_interface_stub
    {
    public:
        virtual uint64_t get_interface_id() const = 0;
        virtual int call(uint64_t method_id, size_t in_size_, const char* in_buf_, std::vector<char>& out_buf_)
            = 0;
        virtual int cast(uint64_t interface_id, rpc::shared_ptr<i_interface_stub>& new_stub) = 0;
        virtual rpc::weak_ptr<object_stub> get_object_stub() const = 0;
        virtual void* get_pointer() const = 0;
        virtual rpc::shared_ptr<casting_interface> get_castable_pointer() const = 0;
    };

}