#include "common/enclave_service_proxy.h"

#ifndef _IN_ENCLAVE
#include <sgx_urts.h>
#include <sgx_capable.h>

#include <untrusted/enclave_marshal_test_u.h>

#endif

namespace rpc
{
#ifndef _IN_ENCLAVE
    enclave_service_proxy::enclave_service_proxy(destination_zone destination_zone_id, std::string filename, const rpc::shared_ptr<service>& operating_zone_service, object host_id, const rpc::i_telemetry_service* telemetry_service)
        : service_proxy(destination_zone_id, operating_zone_service, operating_zone_service->get_zone_id().as_caller(), telemetry_service)
        , filename_(filename)
        , host_id_(host_id)
    {
        if (auto* telemetry_service = get_telemetry_service(); telemetry_service)
        {
            telemetry_service->on_service_proxy_creation("enclave_service_proxy", get_zone_id(), get_destination_zone_id());
        }
    }

    enclave_service_proxy::~enclave_service_proxy()
    {
        if (auto* telemetry_service = get_telemetry_service(); telemetry_service)
        {
            telemetry_service->on_service_proxy_deletion("enclave_service_proxy", get_zone_id(), get_destination_zone_id());
        }
    }

    enclave_service_proxy::enclave_owner::~enclave_owner()
    {
        marshal_test_destroy_enclave(eid_);
        sgx_destroy_enclave(eid_);
    }

    int enclave_service_proxy::initialise_enclave(object& object_id)
    {
        sgx_launch_token_t token = {0};
        int updated = 0;
        #ifdef _WIN32
            auto status = sgx_create_enclavea(filename_.data(), 1, &token, &updated, &eid_, NULL);
        #else
            auto status = sgx_create_enclave(filename_.data(), 1, &token, &updated, &eid_, NULL);
        #endif
        if (status)
        {
            if (auto* telemetry_service = get_telemetry_service(); telemetry_service)
            {
                telemetry_service->message(rpc::i_telemetry_service::err, "sgx_create_enclavea failed");
            }
            return rpc::error::TRANSPORT_ERROR();
        }
        int err_code = error::OK();
        status = marshal_test_init_enclave(eid_, &err_code, get_zone_id().get_val(), host_id_.get_val(), get_destination_zone_id().get_val(), &(object_id.get_ref()));
        if (status)
        {
            if (auto* telemetry_service = get_telemetry_service(); telemetry_service)
            {
                telemetry_service->message(rpc::i_telemetry_service::err, "marshal_test_init_enclave failed");
            }
            sgx_destroy_enclave(eid_);
            return rpc::error::TRANSPORT_ERROR();
        }
        if (err_code)
            return err_code;      
        
        //class takes ownership of the enclave
        enclave_owner_ = std::make_shared<enclave_owner>(eid_);
        if (err_code)
            return err_code;
        return rpc::error::OK();
    }

    int enclave_service_proxy::send(caller_channel_zone caller_channel_zone_id, caller_zone caller_zone_id, destination_zone destination_zone_id, object object_id, interface_ordinal interface_id, method method_id,
                                           size_t in_size_, const char* in_buf_, std::vector<char>& out_buf_)
    {
        if(destination_zone_id != get_destination_zone_id())
            return rpc::error::ZONE_NOT_SUPPORTED();        
        int err_code = 0;
        size_t data_out_sz = 0;
        void* tls = nullptr;
        sgx_status_t status = ::call_enclave(eid_, &err_code, caller_channel_zone_id.get_val(), caller_zone_id.get_val(), destination_zone_id.get_val(), object_id.get_val(), interface_id.get_val(), method_id.get_val(), in_size_, in_buf_,
                                             out_buf_.size(), out_buf_.data(), &data_out_sz, &tls);

        if (status)
        {
            if (auto* telemetry_service = get_telemetry_service(); telemetry_service)
            {
                telemetry_service->message(rpc::i_telemetry_service::err, "call_enclave failed");
            }
            return rpc::error::TRANSPORT_ERROR();
        }

        if (err_code == rpc::error::NEED_MORE_MEMORY())
        {
            // data too small reallocate memory and try again
            out_buf_.resize(data_out_sz);
            status = ::call_enclave(eid_, &err_code, caller_channel_zone_id.get_val(), caller_zone_id.get_val(), destination_zone_id.get_val(), object_id.get_val(), interface_id.get_val(), method_id.get_val(), in_size_, in_buf_,
                                    out_buf_.size(), out_buf_.data(), &data_out_sz, &tls);
            if (status)
            {
                if (auto* telemetry_service = get_telemetry_service(); telemetry_service)
                {
                    telemetry_service->message(rpc::i_telemetry_service::err, "call_enclave failed");
                }
                return rpc::error::TRANSPORT_ERROR();
            }
        }

        return err_code;
    }

    int enclave_service_proxy::try_cast(destination_zone destination_zone_id, object object_id, interface_ordinal interface_id)
    {
        if (auto* telemetry_service = get_telemetry_service(); telemetry_service)
        {
            telemetry_service->on_service_proxy_try_cast("enclave_service_proxy", get_zone_id(), destination_zone_id,
                                                            object_id, interface_id);
        }
        int err_code = 0;
        sgx_status_t status = ::try_cast_enclave(eid_, &err_code, destination_zone_id.get_val(), object_id.get_val(), interface_id.get_val());
        if (status)
        {
            if (auto* telemetry_service = get_telemetry_service(); telemetry_service)
            {
                telemetry_service->message(rpc::i_telemetry_service::err, "try_cast_enclave failed");
            }
            return rpc::error::TRANSPORT_ERROR();
        }
        return err_code;
    }

    uint64_t enclave_service_proxy::add_ref(destination_zone destination_zone_id, object object_id, caller_zone caller_zone_id)
    {
        if (auto* telemetry_service = get_telemetry_service(); telemetry_service)
        {
            telemetry_service->on_service_proxy_add_ref("enclave_service_proxy", get_zone_id(), destination_zone_id,
                                                        object_id, caller_zone_id);
        }
        uint64_t ret = 0;
        sgx_status_t status = ::add_ref_enclave(eid_, &ret, destination_zone_id.get_val(), object_id.get_val(), caller_zone_id.get_val());
        if (status)
        {
            if (auto* telemetry_service = get_telemetry_service(); telemetry_service)
            {
                telemetry_service->message(rpc::i_telemetry_service::err, "add_ref_enclave failed");
            }
            return std::numeric_limits<uint64_t>::max();
        }
        return ret;
    }

    uint64_t enclave_service_proxy::release(destination_zone destination_zone_id, object object_id, caller_zone caller_zone_id)
    {
        if (auto* telemetry_service = get_telemetry_service(); telemetry_service)
        {
            telemetry_service->on_service_proxy_release("enclave_service_proxy", get_zone_id(), destination_zone_id,
                                                        object_id, caller_zone_id);
        }
        uint64_t ret = 0;
        sgx_status_t status = ::release_enclave(eid_, &ret, destination_zone_id.get_val(), object_id.get_val(), caller_zone_id.get_val());
        if (status)
        {
            if (auto* telemetry_service = get_telemetry_service(); telemetry_service)
            {
                telemetry_service->message(rpc::i_telemetry_service::err, "release_enclave failed");
            }
            return std::numeric_limits<uint64_t>::max();
        }
        return ret;
    }
#endif
}