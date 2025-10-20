/*
 *   Copyright (c) 2025 Edward Boggis-Rolfe
 *   All rights reserved.
 */

// Standard C++ headers
#include <chrono>
#include <iostream>
#include <thread>

// RPC headers
#include <rpc/rpc.h>
#ifdef USE_RPC_TELEMETRY
#include <rpc/telemetry/i_telemetry_service.h>
#include <rpc/telemetry/telemetry_handler.h>
#endif

// Other headers
#include "rpc_global_logger.h"

using namespace std::chrono_literals;

extern std::weak_ptr<rpc::service> current_host_service;

// an ocall for logging the test
extern "C"
{
    // #ifdef USE_RPC_TELEMETRY
    CORO_TASK(int)
    call_host(uint64_t protocol_version // version of the rpc call protocol
        ,
        uint64_t encoding // format of the serialised data
        ,
        uint64_t tag // info on the type of the call
        ,
        uint64_t caller_channel_zone_id,
        uint64_t caller_zone_id,
        uint64_t destination_zone_id,
        uint64_t object_id,
        uint64_t interface_id,
        uint64_t method_id,
        size_t sz_int,
        const char* data_in,
        size_t sz_out,
        char* data_out,
        size_t* data_out_sz)
    {
        thread_local rpc::retry_buffer retry_buf;

        auto root_service = current_host_service.lock();
        if (!root_service)
        {
            retry_buf.data.clear();
            RPC_ERROR("Transport error - no root service in call_host");
            CO_RETURN rpc::error::TRANSPORT_ERROR();
        }
        if (retry_buf.data.empty())
        {
            // Split combined input buffer into payload + back-channel
            size_t payload_size = 0;
            std::vector<char> payload;
            std::vector<rpc::back_channel_entry> in_back_channel;

            if (sz_int > 0)
            {
                yas::mem_istream is(data_in, sz_int);
                yas::binary_iarchive<yas::mem_istream, yas::binary | yas::no_header> ia(is);
                ia& payload_size;  // Read payload size
                payload.resize(payload_size);
                if (payload_size > 0)
                    ia.read(payload.data(), payload_size);  // Read payload data
                ia& in_back_channel;  // Read back-channel
            }

            std::vector<char> out_data(sz_out);
            std::vector<rpc::back_channel_entry> out_back_channel;
            retry_buf.return_value = CO_AWAIT root_service->send(protocol_version,
                rpc::encoding(encoding),
                tag,
                {caller_channel_zone_id},
                {caller_zone_id},
                {destination_zone_id},
                {object_id},
                {interface_id},
                {method_id},
                payload_size,
                payload.data(),
                out_data,
                in_back_channel,
                out_back_channel);
            if (retry_buf.return_value >= rpc::error::MIN() && retry_buf.return_value <= rpc::error::MAX())
            {
                CO_RETURN retry_buf.return_value;
            }

            // Combine output payload + back-channel into single buffer
            yas::mem_ostream os;
            yas::binary_oarchive<yas::mem_ostream, yas::binary | yas::no_header> oa(os);
            oa& out_data.size();  // Write payload size
            if (out_data.size() > 0)
                oa.write(out_data.data(), out_data.size());  // Write payload
            oa& out_back_channel;  // Write back-channel
            auto yas_buf = os.get_shared_buffer();
            retry_buf.data.assign(yas_buf.data.get(), yas_buf.data.get() + yas_buf.size);
        }
        *data_out_sz = retry_buf.data.size();
        if (*data_out_sz > sz_out)
            CO_RETURN rpc::error::NEED_MORE_MEMORY();
        memcpy(data_out, retry_buf.data.data(), retry_buf.data.size());
        retry_buf.data.clear();
        CO_RETURN retry_buf.return_value;
    }

    CORO_TASK(int)
    post_host(uint64_t protocol_version // version of the rpc call protocol
        ,
        uint64_t encoding // format of the serialised data
        ,
        uint64_t tag // info on the type of the call
        ,
        uint64_t caller_channel_zone_id,
        uint64_t caller_zone_id,
        uint64_t destination_zone_id,
        uint64_t object_id,
        uint64_t interface_id,
        uint64_t method_id,
        uint64_t post_options_val,
        size_t sz_int,
        const char* data_in,
        size_t* data_out_sz)
    {
        auto root_service = current_host_service.lock();
        if (!root_service)
        {
            RPC_ERROR("Transport error - no root service in post_host");
            CO_RETURN rpc::error::TRANSPORT_ERROR();
        }

        // Split combined input buffer into payload + back-channel
        size_t payload_size = 0;
        std::vector<char> payload;
        std::vector<rpc::back_channel_entry> in_back_channel;

        if (sz_int > 0)
        {
            yas::mem_istream is(data_in, sz_int);
            yas::binary_iarchive<yas::mem_istream, yas::binary | yas::no_header> ia(is);
            ia& payload_size;  // Read payload size
            payload.resize(payload_size);
            if (payload_size > 0)
                ia.read(payload.data(), payload_size);  // Read payload data
            ia& in_back_channel;  // Read back-channel
        }

        CO_AWAIT root_service->post(protocol_version,
            rpc::encoding(encoding),
            tag,
            {caller_channel_zone_id},
            {caller_zone_id},
            {destination_zone_id},
            {object_id},
            {interface_id},
            {method_id},
            static_cast<rpc::post_options>(post_options_val),
            payload_size,
            payload.data(),
            in_back_channel);

        // Fire and forget - no output
        *data_out_sz = 0;
        CO_RETURN rpc::error::OK();
    }

    CORO_TASK(int)
    try_cast_host(uint64_t protocol_version // version of the rpc call protocol
        ,
        uint64_t zone_id,
        uint64_t object_id,
        uint64_t interface_id)
    {
        auto root_service = current_host_service.lock();
        if (!root_service)
        {
            RPC_ERROR("Transport error - no root service in try_cast_host");
            CO_RETURN rpc::error::TRANSPORT_ERROR();
        }
        std::vector<rpc::back_channel_entry> in_back_channel;
        std::vector<rpc::back_channel_entry> out_back_channel;
        int ret = CO_AWAIT root_service->try_cast(protocol_version, {zone_id}, {object_id}, {interface_id}, in_back_channel, out_back_channel);
        CO_RETURN ret;
    }

    CORO_TASK(int)
    add_ref_host(uint64_t protocol_version // version of the rpc call protocol
        ,
        uint64_t destination_channel_zone_id,
        uint64_t destination_zone_id,
        uint64_t object_id,
        uint64_t caller_channel_zone_id,
        uint64_t caller_zone_id,
        uint64_t known_direction_zone_id,
        char build_out_param_channel,
        uint64_t* reference_count)
    {
        auto root_service = current_host_service.lock();
        if (!root_service)
        {
            RPC_ERROR("Transport error - no root service in add_ref_host");
            CO_RETURN rpc::error::TRANSPORT_ERROR();
        }
        std::vector<rpc::back_channel_entry> in_back_channel;
        std::vector<rpc::back_channel_entry> out_back_channel;
        CO_RETURN CO_AWAIT root_service->add_ref(protocol_version,
            {destination_channel_zone_id},
            {destination_zone_id},
            {object_id},
            {caller_channel_zone_id},
            {caller_zone_id},
            {known_direction_zone_id}, // known_direction_zone - using 0 for unknown
            static_cast<rpc::add_ref_options>(build_out_param_channel),
            *reference_count,
            in_back_channel,
            out_back_channel);
    }

    CORO_TASK(int)
    release_host(uint64_t protocol_version // version of the rpc call protocol
        ,
        uint64_t zone_id,
        uint64_t object_id,
        uint64_t caller_zone_id,
        char options,
        uint64_t* reference_count)
    {
        auto root_service = current_host_service.lock();
        if (!root_service)
        {
            RPC_ERROR("Transport error - no root service in release_host");
            CO_RETURN rpc::error::TRANSPORT_ERROR();
        }
        std::vector<rpc::back_channel_entry> in_back_channel;
        std::vector<rpc::back_channel_entry> out_back_channel;
        CO_RETURN CO_AWAIT root_service->release(
            protocol_version, {zone_id}, {object_id}, {caller_zone_id}, static_cast<rpc::release_options>(options), *reference_count, in_back_channel, out_back_channel);
    }
    // #endif

    void rpc_log(int level, const char* str, size_t sz)
    {
#ifdef USE_RPC_LOGGING
        std::string message(str, sz);
        switch (level)
        {
        case 0:
            rpc_global_logger::debug(message);
            break;
        case 1:
            rpc_global_logger::trace(message);
            break;
        case 2:
            rpc_global_logger::info(message);
            break;
        case 3:
            rpc_global_logger::warn(message);
            break;
        case 4:
            rpc_global_logger::error(message);
            break;
        case 5:
            rpc_global_logger::critical(message);
            break;
        default:
            rpc_global_logger::info(message);
            break;
        }
#endif
    }

    void hang()
    {
        std::cerr << "hanging for debugger\n";
        bool loop = true;
        while (loop)
        {
            std::this_thread::sleep_for(1s);
        }
    }
}
