/*
 *   Copyright (c) 2025 Edward Boggis-Rolfe
 *   All rights reserved.
 */
#pragma once

#include <rpc/rpc.h>
#ifdef USE_RPC_TELEMETRY
#include <rpc/telemetry/i_telemetry_service.h>
#endif
// service.h is included by rpc.h

namespace rpc
{
    struct make_shared_local_service_proxy_enabler;
    // this is an equivelent to an enclave looking at its host
    class local_service_proxy : public service_proxy
    {
        std::weak_ptr<service> parent_service_;

        friend make_shared_local_service_proxy_enabler;

        local_service_proxy(const char* name,
            const std::shared_ptr<child_service>& child_svc,
            const std::shared_ptr<rpc::service>& parent_svc)
            : service_proxy(name, parent_svc->get_zone_id().as_destination(), child_svc)
            , parent_service_(parent_svc)
        {
        }
        local_service_proxy(const local_service_proxy& other) = default;

        std::shared_ptr<rpc::service_proxy> clone() override
        {
            return std::shared_ptr<local_service_proxy>(new local_service_proxy(*this));
        }

        // if there is no use of a local_service_proxy in the zone requires_parent_release must be set to true so that
        // the zones service can clean things ups

        static std::shared_ptr<local_service_proxy> create(const char* name,
            destination_zone parent_zone_id,
            const std::shared_ptr<child_service>& child_svc,
            const std::shared_ptr<rpc::service>& parent_svc)
        {
            std::ignore = parent_zone_id;
            return std::shared_ptr<local_service_proxy>(new local_service_proxy(name, child_svc, parent_svc));
        }

        friend rpc::child_service;

    public:
        virtual ~local_service_proxy() = default;
    };

    // this is an equivelent to an host looking at its enclave
    template<class CHILD_PTR_TYPE, class PARENT_PTR_TYPE> class local_child_service_proxy : public service_proxy
    {
        stdex::member_ptr<child_service> child_service_;

        typedef std::function<CORO_TASK(int)(
            const rpc::shared_ptr<PARENT_PTR_TYPE>&, rpc::shared_ptr<CHILD_PTR_TYPE>&, const std::shared_ptr<child_service>&)>
            connect_fn;
        connect_fn fn_;

        friend rpc::service;

        local_child_service_proxy(const char* name,
            destination_zone destination_zone_id,
            const std::shared_ptr<rpc::service>& parent_svc,
            connect_fn fn)
            : service_proxy(name, destination_zone_id, parent_svc)
            , fn_(fn)
        {
            // This proxy is for a child service, so hold a strong reference to the parent service
            // to prevent premature parent destruction until after child cleanup
            set_parent_service_reference(parent_svc);
        }
        local_child_service_proxy(const local_child_service_proxy& other) = default;

        std::shared_ptr<rpc::service_proxy> clone() override
        {
            return std::shared_ptr<rpc::service_proxy>(new local_child_service_proxy(*this));
        }

        static std::shared_ptr<local_child_service_proxy> create(
            const char* name, destination_zone destination_zone_id, const std::shared_ptr<rpc::service>& svc, connect_fn fn)
        {
            return std::shared_ptr<local_child_service_proxy>(
                new local_child_service_proxy(name, destination_zone_id, svc, fn));
        }

        CORO_TASK(int) connect(rpc::interface_descriptor input_descr, rpc::interface_descriptor& output_descr) override
        {
            // local_child_service_proxy nests a local_service_proxy back to the parent service
            std::shared_ptr<rpc::child_service> new_child_service;
            auto result = CO_AWAIT rpc::child_service::create_child_zone<rpc::local_service_proxy>(get_name().c_str(),
                get_destination_zone_id().as_zone(),
                get_zone_id().as_destination(),
                input_descr,
                output_descr,
                fn_,
#ifdef BUILD_COROUTINE
                get_operating_zone_service()->get_scheduler(),
#endif
                new_child_service,
                get_operating_zone_service());

            if (result == rpc::error::OK())
            {
                child_service_ = stdex::member_ptr<rpc::child_service>(new_child_service);
            }

            CO_RETURN result;
        }

    public:
        virtual ~local_child_service_proxy() = default;
    };
}
