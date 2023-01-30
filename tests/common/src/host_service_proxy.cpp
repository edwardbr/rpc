#include "common/host_service_proxy.h"
#include "rpc/i_telemetry_service.h"


#ifdef _IN_ENCLAVE

#include "trusted/enclave_marshal_test_t.h"

namespace rpc
{
    host_service_proxy::host_service_proxy(destination_zone host_zone_id, const rpc::shared_ptr<service>& operating_zone_service,
                        const rpc::i_telemetry_service* telemetry_service)
        : service_proxy(host_zone_id, operating_zone_service, operating_zone_service->get_zone_id().as_caller(), telemetry_service)
    {
        if (auto* telemetry_service = get_telemetry_service(); telemetry_service)
        {
            telemetry_service->on_service_proxy_creation("host_service_proxy", get_operating_zone_id(), get_zone_id());
        }
    }

    rpc::shared_ptr<service_proxy> host_service_proxy::create(destination_zone host_zone_id, object host_id, const rpc::shared_ptr<rpc::child_service>& operating_zone_service, const rpc::i_telemetry_service* telemetry_service)
    {
        auto ret = rpc::shared_ptr<host_service_proxy>(new host_service_proxy(host_zone_id, operating_zone_service, telemetry_service));
        auto pthis = rpc::static_pointer_cast<service_proxy>(ret);
        ret->weak_this_ = pthis;
        operating_zone_service->add_zone_proxy(ret);
        ret->add_external_ref();
        operating_zone_service->set_parent(pthis, host_id.id ? false : true);
        return pthis;
    }

    host_service_proxy::~host_service_proxy()
    {
        if (auto* telemetry_service = get_telemetry_service(); telemetry_service)
        {
            telemetry_service->on_service_proxy_deletion("host_service_proxy", get_operating_zone_id(), get_zone_id());
        }
    }

    int host_service_proxy::send(caller_channel_zone originating_zone_id, caller_zone caller_zone_id, destination_zone zone_id, object object_id, interface_ordinal interface_id, method method_id, size_t in_size_,
                                       const char* in_buf_, std::vector<char>& out_buf_)
    {
        int err_code = 0;
        size_t data_out_sz = 0;
        sgx_status_t status
            = ::call_host(&err_code, originating_zone_id.get_val(), caller_zone_id.get_val(), zone_id.get_val(), object_id.get_val(), interface_id.get_val(), method_id.get_val(), in_size_, in_buf_, out_buf_.size(), out_buf_.data(), &data_out_sz);

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
            out_buf_.resize(data_out_sz);
            //data too small reallocate memory and try again

            status
                = ::call_host(&err_code, originating_zone_id.get_val(), caller_zone_id.get_val(), zone_id.get_val(), object_id.get_val(), interface_id.get_val(), method_id.get_val(), in_size_, in_buf_, out_buf_.size(), out_buf_.data(), &data_out_sz);
            if (status)
            {
                if (auto* telemetry_service = get_telemetry_service(); telemetry_service)
                {
                    telemetry_service->message(rpc::i_telemetry_service::err, "call_host failed");
                }
                return rpc::error::TRANSPORT_ERROR();
            }
        }
        return err_code;
    }

    int host_service_proxy::try_cast(destination_zone zone_id, object object_id, interface_ordinal interface_id)
    {
        if (auto* telemetry_service = get_telemetry_service(); telemetry_service)
        {
            telemetry_service->on_service_proxy_try_cast("host_service_proxy", get_operating_zone_id(),
                                                            zone_id, object_id, interface_id);
        }
        int err_code = 0;
        sgx_status_t status = ::try_cast_host(&err_code, zone_id.get_val(), object_id.get_val(), interface_id.get_val());
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

    uint64_t host_service_proxy::add_ref(destination_zone zone_id, object object_id, caller_zone caller_zone_id)
    {
        if (auto* telemetry_service = get_telemetry_service(); telemetry_service)
        {
            telemetry_service->on_service_proxy_add_ref("host_service_proxy", get_operating_zone_id(),
                                                        zone_id, object_id, caller_zone_id);
        }
        uint64_t ret = 0;
        sgx_status_t status = ::add_ref_host(&ret, zone_id.get_val(), object_id.get_val(), caller_zone_id.get_val());
        if (status)
        {
            if (auto* telemetry_service = get_telemetry_service(); telemetry_service)
            {
                telemetry_service->message(rpc::i_telemetry_service::err, "add_ref_host failed");
            }
            return std::numeric_limits<uint64_t>::max();
        }
        return ret;
    }

    uint64_t host_service_proxy::release(destination_zone zone_id, object object_id, caller_zone caller_zone_id)
    {
        if (auto* telemetry_service = get_telemetry_service(); telemetry_service)
        {
            telemetry_service->on_service_proxy_release("host_service_proxy", get_operating_zone_id(),
                                                        zone_id, object_id, caller_zone_id);
        }
        uint64_t ret = 0;
        sgx_status_t status = ::release_host(&ret, zone_id.get_val(), object_id.get_val(), caller_zone_id.get_val());
        if (status)
        {
            if (auto* telemetry_service = get_telemetry_service(); telemetry_service)
            {
                telemetry_service->message(rpc::i_telemetry_service::err, "release_host failed");
            }
            return std::numeric_limits<uint64_t>::max();
        }
        return ret;
    }
}

#endif
