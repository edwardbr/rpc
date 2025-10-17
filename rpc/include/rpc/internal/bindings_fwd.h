/*
 *   Copyright (c) 2025 Edward Boggis-Rolfe
 *   All rights reserved.
 */
#pragma once

#include <rpc/internal/error_codes.h>
#include <rpc/internal/types.h>
#include <rpc/internal/version.h>
#include <rpc/internal/remote_pointer.h>
#include <rpc/internal/coroutine_support.h>

namespace rpc
{
    class object_stub;
    class service;
    class service_proxy;

    template<class T>
    CORO_TASK(int)
    demarshall_interface_proxy(uint64_t protocol_version,
        const std::shared_ptr<rpc::service_proxy>& sp,
        const rpc::interface_descriptor& encap,
        caller_zone caller_zone_id,
        rpc::shared_ptr<T>& val);

    template<class T>
    CORO_TASK(rpc::interface_descriptor)
    create_interface_stub(rpc::service& serv, const rpc::shared_ptr<T>& iface);

    template<class T>
    CORO_TASK(rpc::interface_descriptor)
    stub_bind_out_param(rpc::service& zone,
        uint64_t protocol_version,
        rpc::caller_channel_zone caller_channel_zone_id,
        rpc::caller_zone caller_zone_id,
        const rpc::shared_ptr<T>& iface);

    template<class T>
    CORO_TASK(rpc::interface_descriptor)
    proxy_bind_in_param(std::shared_ptr<rpc::object_proxy> object_p,
        uint64_t protocol_version,
        const rpc::shared_ptr<T>& iface,
        std::shared_ptr<rpc::object_stub>& stub);

    // do not use directly it is for the interface generator use rpc::create_interface_proxy if you want to get a proxied pointer to a remote implementation
    template<class T>
    CORO_TASK(int)
    stub_bind_in_param(uint64_t protocol_version,
        rpc::service& serv,
        caller_channel_zone caller_channel_zone_id,
        caller_zone caller_zone_id,
        const rpc::interface_descriptor& encap,
        rpc::shared_ptr<T>& iface);

    // do not use directly it is for the interface generator use rpc::create_interface_proxy if you want to get a proxied pointer to a remote implementation
    template<class T>
    CORO_TASK(int)
    proxy_bind_out_param(const std::shared_ptr<rpc::service_proxy>& sp,
        const rpc::interface_descriptor& encap,
        caller_zone caller_zone_id,
        rpc::shared_ptr<T>& val);

    template<class T>
    CORO_TASK(int)
    demarshall_interface_proxy(uint64_t protocol_version,
        const std::shared_ptr<rpc::service_proxy>& sp,
        const rpc::interface_descriptor& encap,
        caller_zone caller_zone_id,
        rpc::shared_ptr<T>& val);
}