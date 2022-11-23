#include "common/host_service_proxy.h"
#include "rpc/i_telemetry_service.h"


#ifdef _IN_ENCLAVE

#include "trusted/enclave_marshal_test_t.h"


namespace rpc
{
    host_service_proxy::host_service_proxy(uint64_t host_zone_id, const rpc::shared_ptr<service>& operating_zone_service,
                        const rpc::i_telemetry_service* telemetry_service)
        : service_proxy(host_zone_id, operating_zone_service, telemetry_service)
    {
        if (auto* telemetry_service = get_telemetry_service(); telemetry_service)
        {
            telemetry_service->on_service_proxy_creation("host_service_proxy", get_operating_zone_id(), get_zone_id());
        }
    }

    host_service_proxy::~host_service_proxy()
    {
        if (auto* telemetry_service = get_telemetry_service(); telemetry_service)
        {
            telemetry_service->on_service_proxy_deletion("host_service_proxy", get_operating_zone_id(), get_zone_id());
        }
    }

    int host_service_proxy::send(uint64_t originating_zone_id, uint64_t zone_id, uint64_t object_id, uint64_t interface_id, uint64_t method_id, size_t in_size_,
                                       const char* in_buf_, std::vector<char>& out_buf_)
    {
        int err_code = 0;
        size_t data_out_sz = 0;
        sgx_status_t status
            = ::call_host(&err_code, originating_zone_id, zone_id, object_id, interface_id, method_id, in_size_, in_buf_, out_buf_.size(), out_buf_.data(), &data_out_sz);

        if (status)
            return rpc::error::TRANSPORT_ERROR();

        if (err_code == rpc::error::NEED_MORE_MEMORY())
        {
            out_buf_.resize(data_out_sz);
            //data too small reallocate memory and try again

            status
                = ::call_host(&err_code, originating_zone_id, zone_id, object_id, interface_id, method_id, in_size_, in_buf_, out_buf_.size(), out_buf_.data(), &data_out_sz);
            if (status)
                return rpc::error::TRANSPORT_ERROR();
        }
        return err_code;
    }

    int host_service_proxy::try_cast(uint64_t zone_id, uint64_t object_id, uint64_t interface_id)
    {
        if (auto* telemetry_service = get_telemetry_service(); telemetry_service)
        {
            telemetry_service->on_service_proxy_try_cast("host_service_proxy", get_operating_zone_id(),
                                                            zone_id, object_id, interface_id);
        }
        int err_code = 0;
        sgx_status_t status = ::try_cast_host(&err_code, zone_id, object_id, interface_id);
        if (status)
            return rpc::error::TRANSPORT_ERROR();
        return err_code;
    }

    uint64_t host_service_proxy::add_ref(uint64_t zone_id, uint64_t object_id)
    {
        if (auto* telemetry_service = get_telemetry_service(); telemetry_service)
        {
            telemetry_service->on_service_proxy_add_ref("host_service_proxy", get_operating_zone_id(),
                                                        zone_id, object_id);
        }
        uint64_t ret = 0;
        sgx_status_t status = ::add_ref_host(&ret, zone_id, object_id);
        if (status)
            return std::numeric_limits<uint64_t>::max();
        
        if(ret != std::numeric_limits<uint64_t>::max())
        {
            add_external_ref();
        }
        return ret;
    }

    uint64_t host_service_proxy::release(uint64_t zone_id, uint64_t object_id)
    {
        if (auto* telemetry_service = get_telemetry_service(); telemetry_service)
        {
            telemetry_service->on_service_proxy_release("host_service_proxy", get_operating_zone_id(),
                                                        zone_id, object_id);
        }
        uint64_t ret = 0;
        sgx_status_t status = ::release_host(&ret, zone_id, object_id);
        if (status)
            return std::numeric_limits<uint64_t>::max();
        if(ret != std::numeric_limits<uint64_t>::max())
        {
            release_external_ref();
        }   
        return ret;
    }
}

#endif
