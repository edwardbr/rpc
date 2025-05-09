#pragma once

#include <string>
#include <memory>
#include <unordered_map>
#include <mutex>
#include <rpc/assert.h>
#include <atomic>

#include <rpc/types.h>
#include <rpc/marshaller.h>
#include <rpc/remote_pointer.h>
#include <rpc/casting_interface.h>
#ifdef USE_RPC_TELEMETRY
#include <rpc/telemetry/i_telemetry_service.h>
#endif
#include <rpc/service.h>

namespace rpc
{

    class i_interface_stub;
    class object_stub;
    class service;
    class service_proxy;

    class object_stub
    {
        object id_ = {0};
        // stubs have stong pointers
        mutable std::mutex map_control;
        std::unordered_map<interface_ordinal, shared_ptr<i_interface_stub>> stub_map;
        shared_ptr<object_stub> p_this;
        std::atomic<uint64_t> reference_count = 0;
        service& zone_;

        void add_interface(const shared_ptr<i_interface_stub>& iface);
        friend service; // so that it can call add_interface

    public:
        object_stub(object id, service& zone, void* target);
        ~object_stub();
        object get_id() const { return id_; }
        shared_ptr<casting_interface> get_castable_interface() const;
        void reset() { p_this.reset(); }

        // this is called once the lifetime management needs to be activated
        void on_added_to_zone(shared_ptr<object_stub> stub) { p_this = stub; }

        service& get_zone() const { return zone_; }

        int call(uint64_t protocol_version,
            rpc::encoding enc,
            caller_channel_zone caller_channel_zone_id,
            caller_zone caller_zone_id,
            interface_ordinal interface_id,
            method method_id,
            size_t in_size_,
            const char* in_buf_,
            std::vector<char>& out_buf_);
        int try_cast(interface_ordinal interface_id);

        shared_ptr<i_interface_stub> get_interface(interface_ordinal interface_id);

        uint64_t add_ref();
        uint64_t release();
        void release_from_service();
    };

    class i_interface_stub
    {
    protected:
        // here to be able to call private function
        template<class T>
        interface_descriptor stub_bind_out_param(rpc::service& zone,
            uint64_t protocol_version,
            caller_channel_zone caller_channel_zone_id,
            caller_zone caller_zone_id,
            const shared_ptr<T>& iface)
        {
            return zone.stub_bind_out_param(protocol_version, caller_channel_zone_id, caller_zone_id, iface);
        }

    public:
        virtual ~i_interface_stub() = default;
        virtual interface_ordinal get_interface_id(uint64_t rpc_version) const = 0;
        virtual int call(uint64_t protocol_version,
            rpc::encoding enc,
            caller_channel_zone caller_channel_zone_id,
            caller_zone caller_zone_id,
            method method_id,
            size_t in_size_,
            const char* in_buf_,
            std::vector<char>& out_buf_)
            = 0;
        virtual int cast(interface_ordinal interface_id, shared_ptr<i_interface_stub>& new_stub) = 0;
        virtual weak_ptr<object_stub> get_object_stub() const = 0;
        virtual void* get_pointer() const = 0;
        virtual shared_ptr<casting_interface> get_castable_interface() const = 0;
    };

}
