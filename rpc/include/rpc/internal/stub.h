/*
 *   Copyright (c) 2025 Edward Boggis-Rolfe
 *   All rights reserved.
 */

#pragma once

#include <string>
#include <memory>
#include <unordered_map>
#include <mutex>
#include <atomic>

#include <rpc/internal/assert.h>
#include <rpc/internal/types.h>
#include <rpc/internal/marshaller.h>
#include <rpc/internal/remote_pointer.h>
#include <rpc/internal/casting_interface.h>
#ifdef USE_RPC_TELEMETRY
#include <rpc/telemetry/i_telemetry_service.h>
#endif
#include <rpc/internal/service.h>

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
        std::unordered_map<interface_ordinal, std::shared_ptr<rpc::i_interface_stub>> stub_map;
        std::shared_ptr<object_stub> p_this;
        std::atomic<uint64_t> reference_count = 0;
        service& zone_;

        void add_interface(const std::shared_ptr<rpc::i_interface_stub>& iface);
        friend service; // so that it can call add_interface

    public:
        object_stub(object id, service& zone, void* target);
        ~object_stub();
        object get_id() const { return id_; }
        rpc::shared_ptr<rpc::casting_interface> get_castable_interface() const;
        void reset() { p_this.reset(); }

        // this is called once the lifetime management needs to be activated
        void on_added_to_zone(std::shared_ptr<object_stub> stub) { p_this = stub; }

        service& get_zone() const { return zone_; }

        CORO_TASK(int)
        call(uint64_t protocol_version,
            rpc::encoding enc,
            caller_channel_zone caller_channel_zone_id,
            caller_zone caller_zone_id,
            interface_ordinal interface_id,
            method method_id,
            size_t in_size_,
            const char* in_buf_,
            std::vector<char>& out_buf_);
        int try_cast(interface_ordinal interface_id);

        std::shared_ptr<rpc::i_interface_stub> get_interface(interface_ordinal interface_id);

        uint64_t add_ref();
        uint64_t release();
        void release_from_service();
    };

    class i_interface_stub
    {
    protected:
        // here to be able to call private function
        template<class T>
        CORO_TASK(interface_descriptor)
        stub_bind_out_param(rpc::service& zone,
            uint64_t protocol_version,
            caller_channel_zone caller_channel_zone_id,
            caller_zone caller_zone_id,
            const shared_ptr<T>& iface)
        {
            CO_RETURN CO_AWAIT zone.bind_out_stub(protocol_version, caller_channel_zone_id, caller_zone_id, iface);
        }

    public:
        virtual ~i_interface_stub() = default;
        virtual interface_ordinal get_interface_id(uint64_t rpc_version) const = 0;
        virtual CORO_TASK(int) call(uint64_t protocol_version,
            rpc::encoding enc,
            caller_channel_zone caller_channel_zone_id,
            caller_zone caller_zone_id,
            method method_id,
            size_t in_size_,
            const char* in_buf_,
            std::vector<char>& out_buf_)
            = 0;
        virtual int cast(interface_ordinal interface_id, std::shared_ptr<rpc::i_interface_stub>& new_stub) = 0;
        virtual std::weak_ptr<rpc::object_stub> get_object_stub() const = 0;
        virtual void* get_pointer() const = 0;
        virtual rpc::shared_ptr<rpc::casting_interface> get_castable_interface() const = 0;
    };

}
