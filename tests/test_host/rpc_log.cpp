#include <thread>
#include <iostream>
#include <chrono>

#include <spdlog/spdlog.h>

#include <rpc/service.h>
#include <rpc/telemetry/host_telemetry_service.h>
#include <rpc/telemetry/telemetry_handler.h>

using namespace std::chrono_literals;

extern rpc::weak_ptr<rpc::service> current_host_service;
extern const rpc::i_telemetry_service* telemetry_service;

// an ocall for logging the test
extern "C"
{
    int call_host(
        uint64_t protocol_version                          //version of the rpc call protocol
        , uint64_t encoding                                  //format of the serialised data
        , uint64_t tag                                       //info on the type of the call 
        , uint64_t caller_channel_zone_id
        , uint64_t caller_zone_id
        , uint64_t destination_zone_id
        , uint64_t object_id
        , uint64_t interface_id
        , uint64_t method_id
        , size_t sz_int
        , const char* data_in
        , size_t sz_out
        , char* data_out
        , size_t* data_out_sz)
    {
        thread_local rpc::retry_buffer retry_buf;

        auto root_service = current_host_service.lock();
        if (!root_service)
        {
            retry_buf.data.clear();
            return rpc::error::TRANSPORT_ERROR();
        }
        if (retry_buf.data.empty())
        {
            std::vector<char> out_data(sz_out);
            retry_buf.return_value = root_service->send(protocol_version, rpc::encoding(encoding), tag, {caller_channel_zone_id}, {caller_zone_id}, {destination_zone_id}, {object_id}, {interface_id}, {method_id}, sz_int,
                                         data_in, out_data);
            if (retry_buf.return_value >= rpc::error::MIN() && retry_buf.return_value <= rpc::error::MAX())
            {
                return retry_buf.return_value;
            }
            retry_buf.data.swap(out_data);
        }
        *data_out_sz = retry_buf.data.size();
        if (*data_out_sz > sz_out)
            return rpc::error::NEED_MORE_MEMORY();
        memcpy(data_out, retry_buf.data.data(), retry_buf.data.size());
        retry_buf.data.clear();
        return retry_buf.return_value;
    }
    int try_cast_host(
        uint64_t protocol_version                          //version of the rpc call protocol
        , uint64_t zone_id
        , uint64_t object_id
        , uint64_t interface_id)
    {
        auto root_service = current_host_service.lock();
        if (!root_service)
        {
            return rpc::error::TRANSPORT_ERROR();
        }
        int ret = root_service->try_cast(protocol_version, {zone_id}, {object_id}, {interface_id});
        return ret;
    }
    uint64_t add_ref_host(
        uint64_t protocol_version                          //version of the rpc call protocol
        , uint64_t destination_channel_zone_id
        , uint64_t destination_zone_id
        , uint64_t object_id
        , uint64_t caller_channel_zone_id
        , uint64_t caller_zone_id
        , char build_out_param_channel)
    {
        auto root_service = current_host_service.lock();
        if (!root_service)
        {
            return rpc::error::TRANSPORT_ERROR();
        }
        return root_service->add_ref(
            protocol_version, 
            {destination_channel_zone_id}, 
            {destination_zone_id}, 
            {object_id}, 
            {caller_channel_zone_id}, 
            {caller_zone_id}, 
            static_cast<rpc::add_ref_options>(build_out_param_channel));
    }
    uint64_t release_host(
        uint64_t protocol_version                          //version of the rpc call protocol
        , uint64_t zone_id
        , uint64_t object_id
        , uint64_t caller_zone_id)
    {
        auto root_service = current_host_service.lock();
        if (!root_service)
        {
            return rpc::error::TRANSPORT_ERROR();
        }
        return root_service->release(protocol_version, {zone_id}, {object_id}, {caller_zone_id});
    }

    void rpc_log(const char* str, size_t sz)
    {
#ifdef USE_RPC_LOGGING        
        spdlog::info(std::string(str, sz));
#endif
    }
    
    void hang()
    {
        std::cerr << "hanging for debugger\n";
        bool loop = true;
        while(loop)
        {
            std::this_thread::sleep_for(1s);
        }
    }
}
