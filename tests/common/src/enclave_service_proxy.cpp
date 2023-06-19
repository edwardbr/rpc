#include "common/enclave_service_proxy.h"

#ifndef _IN_ENCLAVE
#include <thread>

#include <sgx_urts.h>
#include <sgx_capable.h>
#include <untrusted/enclave_marshal_test_u.h>
#endif

namespace rpc
{
#ifndef _IN_ENCLAVE
    enclave_service_proxy::enclave_service_proxy(destination_zone destination_zone_id, std::string filename, const rpc::shared_ptr<service>& svc, object host_id, const rpc::i_telemetry_service* telemetry_service)
        : service_proxy(destination_zone_id, svc, svc->get_zone_id().as_caller(), telemetry_service)
        , filename_(filename)
        , host_id_(host_id)
    {
        if (auto* telemetry_service = get_telemetry_service(); telemetry_service)
        {
            telemetry_service->on_service_proxy_creation("enclave_service_proxy", get_zone_id(), get_destination_zone_id(), get_caller_zone_id());
        }
    }

    enclave_service_proxy::~enclave_service_proxy()
    {
        if (auto* telemetry_service = get_telemetry_service(); telemetry_service)
        {
            telemetry_service->on_service_proxy_deletion("enclave_service_proxy", get_zone_id(), get_destination_zone_id(), get_caller_zone_id());
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
            auto status = sgx_create_enclavea(filename_.data(), SGX_DEBUG_FLAG, &token, &updated, &eid_, NULL);
        #else
            auto status = sgx_create_enclave(filename_.data(), SGX_DEBUG_FLAG, &token, &updated, &eid_, NULL);
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

    int enclave_service_proxy::send(
        uint64_t protocol_version, 
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
        if(destination_zone_id != get_destination_zone_id())
            return rpc::error::ZONE_NOT_SUPPORTED();   
                 
        int err_code = 0;
        size_t data_out_sz = 0;
        void* tls = nullptr;
        sgx_status_t status = ::call_enclave(eid_, &err_code, protocol_version, (uint64_t)encoding, tag, caller_channel_zone_id.get_val(), caller_zone_id.get_val(), destination_zone_id.get_val(), object_id.get_val(), interface_id.get_val(), method_id.get_val(), in_size_, in_buf_,
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
            
            status = ::call_enclave(eid_, &err_code, protocol_version, (uint64_t)encoding, tag, caller_channel_zone_id.get_val(), caller_zone_id.get_val(), destination_zone_id.get_val(), object_id.get_val(), interface_id.get_val(), method_id.get_val(), in_size_, in_buf_,
                                    out_buf_.size(), out_buf_.data(), &data_out_sz, &tls);
            if (status)
            {
                if (auto* telemetry_service = get_telemetry_service(); telemetry_service)
                {
                    telemetry_service->message(rpc::i_telemetry_service::err, "call_enclave failed");
                }
                return rpc::error::TRANSPORT_ERROR();
            }

#ifdef RPC_V1
            if(protocol_version == rpc::VERSION_1)
            {
                //recover err_code from the out buffer
                yas::load<
#ifdef RPC_SERIALISATION_TEXT
                    yas::mem|yas::text|yas::no_header
#else
                    yas::mem|yas::binary|yas::no_header
#endif
                >(yas::intrusive_buffer{out_buf_.data(), out_buf_.size()}, YAS_OBJECT_NVP(
                "out"
                ,("__return_value", err_code)
                ));      
            }
#endif            
        }

        return err_code;
    }

    int enclave_service_proxy::try_cast(uint64_t protocol_version, destination_zone destination_zone_id, object object_id, interface_ordinal interface_id)
    {
        if (auto* telemetry_service = get_telemetry_service(); telemetry_service)
        {
            telemetry_service->on_service_proxy_try_cast("enclave_service_proxy", get_zone_id(), destination_zone_id,
                                                            object_id, interface_id);
        }
        int err_code = 0;
        sgx_status_t status = ::try_cast_enclave(eid_, &err_code, protocol_version, destination_zone_id.get_val(), object_id.get_val(), interface_id.get_val());
        if(status == SGX_ERROR_ECALL_NOT_ALLOWED)
        {
            auto task = std::thread([&]()
            {
                status = ::try_cast_enclave(eid_, &err_code, protocol_version, destination_zone_id.get_val(), object_id.get_val(), interface_id.get_val());
            });
            task.join();
        }
        if (status)
        {
            if (auto* telemetry_service = get_telemetry_service(); telemetry_service)
            {
                telemetry_service->message(rpc::i_telemetry_service::err, "try_cast_enclave failed");
            }
            assert(false);
            return rpc::error::TRANSPORT_ERROR();
        }
        return err_code;
    }

    uint64_t enclave_service_proxy::add_ref(uint64_t protocol_version, destination_channel_zone destination_channel_zone_id, destination_zone destination_zone_id, object object_id, caller_channel_zone caller_channel_zone_id, caller_zone caller_zone_id, add_ref_options build_out_param_channel, bool proxy_add_ref)
    {
        if (auto* telemetry_service = get_telemetry_service(); telemetry_service)
        {
            telemetry_service->on_service_proxy_add_ref("enclave_service_proxy", get_zone_id(), destination_zone_id,
                                                        object_id, caller_zone_id);
        }
        uint64_t ret = 0;
        constexpr auto add_ref_failed_val = std::numeric_limits<uint64_t>::max();
        sgx_status_t status = ::add_ref_enclave(eid_, &ret, protocol_version, destination_channel_zone_id.get_val(), destination_zone_id.get_val(), object_id.get_val(), caller_channel_zone_id.get_val(), caller_zone_id.get_val(), (uint8_t)build_out_param_channel);
        if(status == SGX_ERROR_ECALL_NOT_ALLOWED)
        {
            auto task = std::thread([&]()
            {
                status = ::add_ref_enclave(eid_, &ret, protocol_version, destination_channel_zone_id.get_val(), destination_zone_id.get_val(), object_id.get_val(), caller_channel_zone_id.get_val(), caller_zone_id.get_val(), (uint8_t)build_out_param_channel);
            });
            task.join();
        }
        if (status)
        {
            if (auto* telemetry_service = get_telemetry_service(); telemetry_service)
            {
                telemetry_service->message(rpc::i_telemetry_service::err, "add_ref_enclave failed");
            }
            assert(false);
            return add_ref_failed_val;
        }        
        if(ret == add_ref_failed_val)
        {
            if (auto* telemetry_service = get_telemetry_service(); telemetry_service)
            {
                telemetry_service->on_service_proxy_release("enclave_service_proxy", get_zone_id(), destination_zone_id,
                                                            object_id, caller_zone_id);
            }
        }
        if(proxy_add_ref && ret != add_ref_failed_val)
        {
            add_external_ref();
        }
        return ret;
    }

    uint64_t enclave_service_proxy::release(uint64_t protocol_version, destination_zone destination_zone_id, object object_id, caller_zone caller_zone_id)
    {
        if (auto* telemetry_service = get_telemetry_service(); telemetry_service)
        {
            telemetry_service->on_service_proxy_release("enclave_service_proxy", get_zone_id(), destination_zone_id,
                                                        object_id, caller_zone_id);
        }
        uint64_t ret = 0;
        sgx_status_t status = ::release_enclave(eid_, &ret, protocol_version, destination_zone_id.get_val(), object_id.get_val(), caller_zone_id.get_val());
        if(status == SGX_ERROR_ECALL_NOT_ALLOWED)
        {
            auto task = std::thread([&]()
            {
                status = ::release_enclave(eid_, &ret, protocol_version, destination_zone_id.get_val(), object_id.get_val(), caller_zone_id.get_val());
            });
            task.join();
        }
        if (status)
        {
            if (auto* telemetry_service = get_telemetry_service(); telemetry_service)
            {
                telemetry_service->message(rpc::i_telemetry_service::err, "release_enclave failed");
            }
            return std::numeric_limits<uint64_t>::max();
        }
        if(ret != std::numeric_limits<uint64_t>::max())
        {
            release_external_ref();
        }  
        return ret;
    }
#endif
}