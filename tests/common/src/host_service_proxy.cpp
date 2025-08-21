/*
 *   Copyright (c) 2024 Edward Boggis-Rolfe
 *   All rights reserved.
 */
#include "common/host_service_proxy.h"
#ifdef USE_RPC_TELEMETRY
#include "rpc/telemetry/i_telemetry_service.h"
#endif

#ifdef _IN_ENCLAVE

#include "trusted/enclave_marshal_test_t.h"

namespace rpc
{
    host_service_proxy::host_service_proxy(
        const char* name, destination_zone host_zone_id, const rpc::shared_ptr<rpc::child_service>& svc)
        : service_proxy(name, host_zone_id, svc)
    {
    }

    rpc::shared_ptr<service_proxy> host_service_proxy::create(
        const char* name, destination_zone host_zone_id, const rpc::shared_ptr<rpc::child_service>& svc)
    {
        return rpc::shared_ptr<host_service_proxy>(new host_service_proxy(name, host_zone_id, svc));
    }

    int host_service_proxy::send(uint64_t protocol_version,
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
        std::vector<char>& out_buf_)
    {
        if (destination_zone_id != get_destination_zone_id())
            return rpc::error::ZONE_NOT_SUPPORTED();

        int err_code = 0;
        size_t data_out_sz = 0;
        sgx_status_t status = ::call_host(&err_code,
            protocol_version,
            (uint64_t)encoding,
            tag,
            caller_channel_zone_id.get_val(),
            caller_zone_id.get_val(),
            destination_zone_id.get_val(),
            object_id.get_val(),
            interface_id.get_val(),
            method_id.get_val(),
            in_size_,
            in_buf_,
            out_buf_.size(),
            out_buf_.data(),
            &data_out_sz);

        if (status)
        {
#ifdef USE_RPC_TELEMETRY
            if (auto telemetry_service = rpc::telemetry_service_manager::get(); telemetry_service)
            {
                telemetry_service->message(rpc::i_telemetry_service::err, "call_host failed");
            }
#endif
            return rpc::error::TRANSPORT_ERROR();
        }

        if (err_code == rpc::error::NEED_MORE_MEMORY())
        {
            // data too small reallocate memory and try again
            out_buf_.resize(data_out_sz);

            status = ::call_host(&err_code,
                protocol_version,
                (uint64_t)encoding,
                tag,
                caller_channel_zone_id.get_val(),
                caller_zone_id.get_val(),
                destination_zone_id.get_val(),
                object_id.get_val(),
                interface_id.get_val(),
                method_id.get_val(),
                in_size_,
                in_buf_,
                out_buf_.size(),
                out_buf_.data(),
                &data_out_sz);
            if (status)
            {
#ifdef USE_RPC_TELEMETRY
                if (auto telemetry_service = rpc::telemetry_service_manager::get(); telemetry_service)
                {
                    telemetry_service->message(rpc::i_telemetry_service::err, "call_host failed");
                }
#endif
                std::string error = "call_host gave an enclave error ";
                error += std::to_string(status);
                LOG_STR(error.c_str(), error.length());
                RPC_ASSERT(!error.c_str());
                return rpc::error::TRANSPORT_ERROR();
            }
        }
        return err_code;
    }

    int host_service_proxy::try_cast(
        uint64_t protocol_version, destination_zone destination_zone_id, object object_id, interface_ordinal interface_id)
    {
        RPC_ASSERT(destination_zone_id == get_destination_zone_id());
        int err_code = 0;
        sgx_status_t status = ::try_cast_host(
            &err_code, protocol_version, destination_zone_id.get_val(), object_id.get_val(), interface_id.get_val());
        if (status)
        {
#ifdef USE_RPC_TELEMETRY
            if (auto telemetry_service = rpc::telemetry_service_manager::get(); telemetry_service)
            {
                telemetry_service->message(rpc::i_telemetry_service::err, "try_cast failed");
            }
#endif
            std::string error = "try_cast_host gave an enclave error ";
            error += std::to_string(status);
            LOG_STR(error.c_str(), error.length());
            RPC_ASSERT(!error.c_str());
            return rpc::error::TRANSPORT_ERROR();
        }
        return err_code;
    }

    uint64_t host_service_proxy::add_ref(uint64_t protocol_version,
        destination_channel_zone destination_channel_zone_id,
        destination_zone destination_zone_id,
        object object_id,
        caller_channel_zone caller_channel_zone_id,
        caller_zone caller_zone_id,
        add_ref_options build_out_param_channel)
    {
#ifdef USE_RPC_TELEMETRY
        if (auto telemetry_service = rpc::telemetry_service_manager::get(); telemetry_service)
        {
            telemetry_service->on_service_proxy_add_ref(get_zone_id(),
                destination_zone_id,
                destination_channel_zone_id,
                get_caller_zone_id(),
                object_id,
                build_out_param_channel);
        }
#endif
        uint64_t ret = 0;
        sgx_status_t status = ::add_ref_host(&ret,
            protocol_version,
            destination_channel_zone_id.get_val(),
            destination_zone_id.get_val(),
            object_id.get_val(),
            caller_channel_zone_id.get_val(),
            caller_zone_id.get_val(),
            (std::uint8_t)build_out_param_channel);
        if (status)
        {
#ifdef USE_RPC_TELEMETRY
            if (auto telemetry_service = rpc::telemetry_service_manager::get(); telemetry_service)
            {
                telemetry_service->message(rpc::i_telemetry_service::err, "add_ref_host failed");
            }
#endif
            std::string error = "add_ref_host gave an enclave error ";
            error += std::to_string(status);
            LOG_STR(error.c_str(), error.length());
            RPC_ASSERT(!error.c_str());
            return std::numeric_limits<uint64_t>::max();
        }

        auto svc = rpc::static_pointer_cast<child_service>(get_operating_zone_service());
        return ret;
    }

    uint64_t host_service_proxy::release(
        uint64_t protocol_version, destination_zone destination_zone_id, object object_id, caller_zone caller_zone_id)
    {
        uint64_t ret = 0;
        sgx_status_t status = ::release_host(
            &ret, protocol_version, destination_zone_id.get_val(), object_id.get_val(), caller_zone_id.get_val());
        if (status)
        {
#ifdef USE_RPC_TELEMETRY
            if (auto telemetry_service = rpc::telemetry_service_manager::get(); telemetry_service)
            {
                telemetry_service->message(rpc::i_telemetry_service::err, "release_host failed");
            }
#endif
            std::string error = "release_host gave an enclave error ";
            error += std::to_string(status);
            LOG_STR(error.c_str(), error.length());
            RPC_ASSERT(!error.c_str());
            return std::numeric_limits<uint64_t>::max();
        }
        return ret;
    }
}

#endif
