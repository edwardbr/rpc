/*
 *   Copyright (c) 2024 Edward Boggis-Rolfe
 *   All rights reserved.
 */
#pragma once

#include <rpc/types.h>
#include <rpc/proxy.h>
#include <rpc/error_codes.h>
#include <rpc/member_ptr.h>
#ifdef USE_RPC_TELEMETRY
#include <rpc/telemetry/i_telemetry_service.h>
#endif
#include <rpc/service.h>

namespace rpc
{
    struct make_shared_local_service_proxy_enabler;
    // this is an equivelent to an enclave looking at its host
    class local_service_proxy : public service_proxy
    {
        rpc::weak_ptr<service> parent_service_;

        friend make_shared_local_service_proxy_enabler;

        local_service_proxy(
            const char* name, const rpc::shared_ptr<child_service>& child_svc, const rpc::shared_ptr<service>& parent_svc)
            : service_proxy(name, parent_svc->get_zone_id().as_destination(), child_svc)
            , parent_service_(parent_svc)
        {
        }
        local_service_proxy(const local_service_proxy& other) = default;

        rpc::shared_ptr<service_proxy> clone() override
        {
            return rpc::shared_ptr<local_service_proxy>(new local_service_proxy(*this));
        }

        // if there is no use of a local_service_proxy in the zone requires_parent_release must be set to true so that
        // the zones service can clean things ups

        static rpc::shared_ptr<local_service_proxy> create(const char* name,
            destination_zone parent_zone_id,
            const rpc::shared_ptr<child_service>& child_svc,
            const rpc::shared_ptr<service>& parent_svc)
        {
            std::ignore = parent_zone_id;
            return rpc::shared_ptr<local_service_proxy>(new local_service_proxy(name, child_svc, parent_svc));
        }

        int send(uint64_t protocol_version,
            encoding encoding,
            uint64_t tag,
            caller_channel_zone caller_channel_zone_id,
            caller_zone caller_zone_id,
            destination_zone destination_zone_id,
            object object_id,
            interface_ordinal interface_id,
            method method_id,
            size_t in_size_,
            const char* in_buf_,
            std::vector<char>& out_buf_) override
        {
            return parent_service_.lock()->send(protocol_version,
                encoding,
                tag,
                caller_channel_zone_id,
                caller_zone_id,
                destination_zone_id,
                object_id,
                interface_id,
                method_id,
                in_size_,
                in_buf_,
                out_buf_);
        }
        int try_cast(uint64_t protocol_version,
            destination_zone destination_zone_id,
            object object_id,
            interface_ordinal interface_id) override
        {
            return parent_service_.lock()->try_cast(protocol_version, destination_zone_id, object_id, interface_id);
        }
        uint64_t add_ref(uint64_t protocol_version,
            destination_channel_zone destination_channel_zone_id,
            destination_zone destination_zone_id,
            object object_id,
            caller_channel_zone caller_channel_zone_id,
            caller_zone caller_zone_id,
            known_direction_zone known_direction_zone_id,
            add_ref_options build_out_param_channel) override
        {
            RPC_ASSERT(((std::uint8_t)build_out_param_channel & (std::uint8_t)rpc::add_ref_options::build_caller_route)
                       || destination_channel_zone_id == 0
                       || destination_channel_zone_id == get_destination_channel_zone_id());
            auto dest = parent_service_.lock();
            auto ret = dest->add_ref(protocol_version,
                destination_channel_zone_id,
                destination_zone_id,
                object_id,
                caller_channel_zone_id,
                caller_zone_id,
                known_direction_zone_id,
                build_out_param_channel);

            // auto svc = rpc::static_pointer_cast<child_service>(get_operating_zone_service());
            return ret;
        }
        uint64_t release(
            uint64_t protocol_version, destination_zone destination_zone_id, object object_id, caller_zone caller_zone_id) override
        {
            auto ret = parent_service_.lock()->release(protocol_version, destination_zone_id, object_id, caller_zone_id);
            return ret;
        }

        friend rpc::child_service;

    public:
        virtual ~local_service_proxy() = default;
    };

    // this is an equivelent to an host looking at its enclave
    template<class CHILD_PTR_TYPE, class PARENT_PTR_TYPE> class local_child_service_proxy : public service_proxy
    {
        rpc::member_ptr<child_service> child_service_;

        typedef std::function<int(
            const rpc::shared_ptr<PARENT_PTR_TYPE>&, rpc::shared_ptr<CHILD_PTR_TYPE>&, const rpc::shared_ptr<child_service>&)>
            connect_fn;
        connect_fn fn_;

        friend rpc::service;

        local_child_service_proxy(
            const char* name, destination_zone destination_zone_id, const rpc::shared_ptr<service>& parent_svc, connect_fn fn)
            : service_proxy(name, destination_zone_id, parent_svc)
            , fn_(fn)
        {
            // This proxy is for a child service, so hold a strong reference to the parent service
            // to prevent premature parent destruction until after child cleanup
            set_parent_service_reference(parent_svc);
        }
        local_child_service_proxy(const local_child_service_proxy& other) = default;

        rpc::shared_ptr<service_proxy> clone() override
        {
            return rpc::shared_ptr<service_proxy>(new local_child_service_proxy(*this));
        }

        static rpc::shared_ptr<local_child_service_proxy> create(
            const char* name, destination_zone destination_zone_id, const rpc::shared_ptr<service>& svc, connect_fn fn)
        {
            return rpc::shared_ptr<local_child_service_proxy>(
                new local_child_service_proxy(name, destination_zone_id, svc, fn));
        }

        int connect(rpc::interface_descriptor input_descr, rpc::interface_descriptor& output_descr) override
        {
            // local_child_service_proxy nests a local_service_proxy back to the parent service
            rpc::shared_ptr<rpc::child_service> new_child_service;
            auto result = rpc::child_service::create_child_zone<rpc::local_service_proxy>(get_name().c_str(),
                get_destination_zone_id().as_zone(),
                get_zone_id().as_destination(),
                input_descr,
                output_descr,
                fn_,
                new_child_service,
                get_operating_zone_service());

            if (result == rpc::error::OK())
            {
                child_service_ = rpc::member_ptr<rpc::child_service>(new_child_service);
            }

            return result;
        }

        int send(uint64_t protocol_version,
            encoding encoding,
            uint64_t tag,
            caller_channel_zone caller_channel_zone_id,
            caller_zone caller_zone_id,
            destination_zone destination_zone_id,
            object object_id,
            interface_ordinal interface_id,
            method method_id,
            size_t in_size_,
            const char* in_buf_,
            std::vector<char>& out_buf_) override
        {
            auto child_service = child_service_.get_nullable();
            RPC_ASSERT(child_service);
            if (!child_service)
                return rpc::error::ZONE_NOT_INITIALISED();
            return child_service->send(protocol_version,
                encoding,
                tag,
                caller_channel_zone_id,
                caller_zone_id,
                destination_zone_id,
                object_id,
                interface_id,
                method_id,
                in_size_,
                in_buf_,
                out_buf_);
        }
        int try_cast(uint64_t protocol_version,
            destination_zone destination_zone_id,
            object object_id,
            interface_ordinal interface_id) override
        {
            auto child_service = child_service_.get_nullable();
            RPC_ASSERT(child_service);
            if (!child_service)
                return rpc::error::ZONE_NOT_INITIALISED();
            return child_service->try_cast(protocol_version, destination_zone_id, object_id, interface_id);
        }
        uint64_t add_ref(uint64_t protocol_version,
            destination_channel_zone destination_channel_zone_id,
            destination_zone destination_zone_id,
            object object_id,
            caller_channel_zone caller_channel_zone_id,
            caller_zone caller_zone_id,
            known_direction_zone known_direction_zone_id,
            add_ref_options build_out_param_channel) override
        {
            auto child_service = child_service_.get_nullable();
            RPC_ASSERT(child_service);
            if (!child_service)
                return std::numeric_limits<uint64_t>::max();
            auto ret = child_service->add_ref(protocol_version,
                destination_channel_zone_id,
                destination_zone_id,
                object_id,
                caller_channel_zone_id,
                caller_zone_id,
                known_direction_zone_id,
                build_out_param_channel);
            return ret;
        }
        uint64_t release(
            uint64_t protocol_version, destination_zone destination_zone_id, object object_id, caller_zone caller_zone_id) override
        {
            auto child_service = child_service_.get_nullable();
            RPC_ASSERT(child_service);
            if (!child_service)
                return std::numeric_limits<uint64_t>::max();
            auto ret = child_service->release(protocol_version, destination_zone_id, object_id, caller_zone_id);
            return ret;
        }

    public:
        virtual ~local_child_service_proxy() = default;
    };
}
