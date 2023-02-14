#include "common/host_service_proxy.h"
#include "rpc/i_telemetry_service.h"


#ifdef _IN_ENCLAVE

#include "trusted/enclave_marshal_test_t.h"

namespace rpc
{
    host_service_proxy::host_service_proxy(destination_zone host_zone_id, const rpc::shared_ptr<service>& svc,
                        const rpc::i_telemetry_service* telemetry_service)
        : service_proxy(host_zone_id, svc, svc->get_zone_id().as_caller(), telemetry_service)
    {
        if (auto* telemetry_service = get_telemetry_service(); telemetry_service)
        {
            telemetry_service->on_service_proxy_creation("host_service_proxy", get_zone_id(), get_destination_zone_id(), get_caller_zone_id());
        }
    }

    rpc::shared_ptr<service_proxy> host_service_proxy::create(destination_zone host_zone_id, object host_id, const rpc::shared_ptr<rpc::child_service>& svc, const rpc::i_telemetry_service* telemetry_service)
    {
        auto ret = rpc::shared_ptr<host_service_proxy>(new host_service_proxy(host_zone_id, svc, telemetry_service));
        auto pthis = rpc::static_pointer_cast<service_proxy>(ret);
        ret->weak_this_ = pthis;
        svc->add_zone_proxy(ret);
        ret->add_external_ref();
        svc->set_parent(pthis, host_id.id ? false : true);
        return pthis;
    }

    host_service_proxy::~host_service_proxy()
    {
        if (auto* telemetry_service = get_telemetry_service(); telemetry_service)
        {
            telemetry_service->on_service_proxy_deletion("host_service_proxy", get_zone_id(), get_destination_zone_id(), get_caller_zone_id());
        }
    }

    int host_service_proxy::send(caller_channel_zone caller_channel_zone_id, caller_zone caller_zone_id, destination_zone destination_zone_id, object object_id, interface_ordinal interface_id, method method_id, size_t in_size_,
                                       const char* in_buf_, std::vector<char>& out_buf_)
    {
        if(destination_zone_id != get_destination_zone_id())
            return rpc::error::ZONE_NOT_SUPPORTED();  

        int err_code = 0;
        size_t data_out_sz = 0;
        sgx_status_t status
            = ::call_host(&err_code, caller_channel_zone_id.get_val(), caller_zone_id.get_val(), destination_zone_id.get_val(), object_id.get_val(), interface_id.get_val(), method_id.get_val(), in_size_, in_buf_, out_buf_.size(), out_buf_.data(), &data_out_sz);

        if (status)
        {
            if (auto* telemetry_service = get_telemetry_service(); telemetry_service)
            {
                telemetry_service->message(rpc::i_telemetry_service::err, "call_host failed");
            }
            return rpc::error::TRANSPORT_ERROR();
        }

        if (err_code == rpc::error::NEED_MORE_MEMORY())
        {
            //data too small reallocate memory and try again
            out_buf_.resize(data_out_sz);

            status = ::call_host(&err_code, caller_channel_zone_id.get_val(), caller_zone_id.get_val(), destination_zone_id.get_val(), object_id.get_val(), interface_id.get_val(), method_id.get_val(), in_size_, in_buf_, out_buf_.size(), out_buf_.data(), &data_out_sz);
            if (status)
            {
                if (auto* telemetry_service = get_telemetry_service(); telemetry_service)
                {
                    telemetry_service->message(rpc::i_telemetry_service::err, "call_host failed");
                }
                return rpc::error::TRANSPORT_ERROR();
            }

            //recover err_code from the out buffer
            yas::load<yas::mem|yas::binary|yas::no_header>(yas::intrusive_buffer{out_buf_.data(), out_buf_.size()}, YAS_OBJECT_NVP(
            "out"
            ,("__return_value", err_code)
            )); 
        }
        return err_code;
    }

    int host_service_proxy::try_cast(destination_zone destination_zone_id, object object_id, interface_ordinal interface_id)
    {
        if (auto* telemetry_service = get_telemetry_service(); telemetry_service)
        {
            telemetry_service->on_service_proxy_try_cast("host_service_proxy", get_zone_id(),
                                                            destination_zone_id, object_id, interface_id);
        }
        int err_code = 0;
        sgx_status_t status = ::try_cast_host(&err_code, destination_zone_id.get_val(), object_id.get_val(), interface_id.get_val());
        if (status)
        {
            if (auto* telemetry_service = get_telemetry_service(); telemetry_service)
            {
                telemetry_service->message(rpc::i_telemetry_service::err, "try_cast failed");
            }
            return rpc::error::TRANSPORT_ERROR();
        }
        return err_code;
    }

    uint64_t host_service_proxy::add_ref(destination_channel_zone destination_channel_zone_id, destination_zone destination_zone_id, object object_id, caller_channel_zone caller_channel_zone_id, caller_zone caller_zone_id, add_ref_channel build_out_param_channel, bool proxy_add_ref)
    {
        if (auto* telemetry_service = get_telemetry_service(); telemetry_service)
        {
            telemetry_service->on_service_proxy_add_ref("host_service_proxy", get_zone_id(),
                                                        destination_zone_id, object_id, caller_zone_id);
        }
        uint64_t ret = 0;
        sgx_status_t status = ::add_ref_host(&ret, destination_channel_zone_id.get_val(), destination_zone_id.get_val(), object_id.get_val(), caller_channel_zone_id.get_val(), caller_zone_id.get_val(), (std::uint8_t)build_out_param_channel);
        if (status)
        {
            if (auto* telemetry_service = get_telemetry_service(); telemetry_service)
            {
                telemetry_service->message(rpc::i_telemetry_service::err, "add_ref_host failed");
            }
            return std::numeric_limits<uint64_t>::max();
        }        
        if(proxy_add_ref && ret != std::numeric_limits<uint64_t>::max())
        {
            add_external_ref();
        }
        return ret;
    }

    uint64_t host_service_proxy::release(destination_zone destination_zone_id, object object_id, caller_zone caller_zone_id)
    {
        if (auto* telemetry_service = get_telemetry_service(); telemetry_service)
        {
            telemetry_service->on_service_proxy_release("host_service_proxy", get_zone_id(),
                                                        destination_zone_id, object_id, caller_zone_id);
        }
        uint64_t ret = 0;
        sgx_status_t status = ::release_host(&ret, destination_zone_id.get_val(), object_id.get_val(), caller_zone_id.get_val());
        if (status)
        {
            if (auto* telemetry_service = get_telemetry_service(); telemetry_service)
            {
                telemetry_service->message(rpc::i_telemetry_service::err, "release_host failed");
            }
            return std::numeric_limits<uint64_t>::max();
        }
        if(ret != std::numeric_limits<uint64_t>::max())
        {
            release_external_ref();
        }   
        return ret;
    }
}

#endif
