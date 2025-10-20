/*
 *   Copyright (c) 2025 Edward Boggis-Rolfe
 *   All rights reserved.
 */

// OS-specific headers
#include <sgx_trts.h>

// Standard C/C++ headers
#include <cstdio>
#include <cstring>
#include <stdio.h>
#include <string>

// RPC headers
#include <rpc/rpc.h>
#ifdef USE_RPC_TELEMETRY
#include <rpc/telemetry/i_telemetry_service.h>
#include <rpc/telemetry/enclave_telemetry_service.h>
#endif

// Other headers
#include <trusted/enclave_marshal_test_t.h>
#include <common/foo_impl.h>
#include <common/host_service_proxy.h>
#include <example/example.h>

using namespace marshalled_tests;

std::shared_ptr<rpc::child_service> rpc_server;

int marshal_test_init_enclave(uint64_t host_zone_id, uint64_t host_id, uint64_t child_zone_id, uint64_t* example_object_id)
{
    rpc::interface_descriptor input_descr{};
    rpc::interface_descriptor output_descr{};

    if (host_id)
    {
        input_descr = {{host_id}, {host_zone_id}};
    }

#ifdef USE_RPC_TELEMETRY
    rpc::enclave_telemetry_service::create(rpc::telemetry_service_);
#endif

    auto ret = rpc::child_service::create_child_zone<rpc::host_service_proxy, yyy::i_host, yyy::i_example>(
        "test_enclave",
        rpc::zone{child_zone_id},
        rpc::destination_zone{host_zone_id},
        input_descr,
        output_descr,
        [](const rpc::shared_ptr<yyy::i_host>& host,
            rpc::shared_ptr<yyy::i_example>& new_example,
            const std::shared_ptr<rpc::child_service>& child_service_ptr) -> int
        {
            example_import_idl_register_stubs(child_service_ptr);
            example_shared_idl_register_stubs(child_service_ptr);
            example_idl_register_stubs(child_service_ptr);
            new_example = rpc::shared_ptr<yyy::i_example>(new marshalled_tests::example(child_service_ptr, host));
            return rpc::error::OK();
        },
        rpc_server);

    if (ret != rpc::error::OK())
        return ret;

    *example_object_id = output_descr.object_id.get_val();

    return rpc::error::OK();
}

void marshal_test_destroy_enclave()
{
    rpc_server.reset();
}

int call_enclave(uint64_t protocol_version, // version of the rpc call protocol
    uint64_t encoding,                      // format of the serialised data
    uint64_t tag,                           // info on the type of the call passed from the idl generator
    uint64_t caller_channel_zone_id,
    uint64_t caller_zone_id,
    uint64_t zone_id,
    uint64_t object_id,
    uint64_t interface_id,
    uint64_t method_id,
    size_t sz_int,
    const char* data_in,
    size_t sz_out,
    char* data_out,
    size_t* data_out_sz,
    void** enclave_retry_buffer)
{
    if (protocol_version > rpc::get_version())
    {
        RPC_ERROR("Invalid version in call_enclave");
        return rpc::error::INVALID_VERSION();
    }
    // a retry cache using enclave_retry_buffer as thread local storage, leaky if the client does not retry with more
    // memory
    if (!enclave_retry_buffer)
    {
        RPC_ERROR("Invalid data - null enclave_retry_buffer");
        return rpc::error::INVALID_DATA();
    }

    auto*& retry_buf = *reinterpret_cast<rpc::retry_buffer**>(enclave_retry_buffer);
    if (retry_buf && !sgx_is_within_enclave(retry_buf, sizeof(rpc::retry_buffer*)))
    {
        RPC_ERROR("Security error - retry_buf not within enclave");
        return rpc::error::SECURITY_ERROR();
    }

    if (retry_buf)
    {
        *data_out_sz = retry_buf->data.size();
        if (*data_out_sz > sz_out)
        {
            return rpc::error::NEED_MORE_MEMORY();
        }

        memcpy(data_out, retry_buf->data.data(), retry_buf->data.size());
        auto ret = retry_buf->return_value;
        delete retry_buf;
        retry_buf = nullptr;
        return ret;
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

    std::vector<char> tmp;
    tmp.resize(sz_out);
    std::vector<rpc::back_channel_entry> out_back_channel;

    int ret = rpc_server->send(protocol_version, // version of the rpc call protocol
        rpc::encoding(encoding),                 // format of the serialised data
        tag,
        {caller_channel_zone_id},
        {caller_zone_id},
        {zone_id},
        {object_id},
        {interface_id},
        {method_id},
        payload_size,
        payload.data(),
        tmp,
        in_back_channel,
        out_back_channel);
    if (ret >= rpc::error::MIN() && ret <= rpc::error::MAX())
        return ret;

    // Combine output payload + back-channel into single buffer
    std::vector<char> combined_out;
    {
        yas::mem_ostream os;
        yas::binary_oarchive<yas::mem_ostream, yas::binary | yas::no_header> oa(os);
        oa& tmp.size();  // Write payload size
        if (tmp.size() > 0)
            oa.write(tmp.data(), tmp.size());  // Write payload
        oa& out_back_channel;  // Write back-channel
        auto yas_buf = os.get_shared_buffer();
        combined_out.assign(yas_buf.data.get(), yas_buf.data.get() + yas_buf.size);
    }

    *data_out_sz = combined_out.size();
    if (*data_out_sz <= sz_out)
    {
        memcpy(data_out, combined_out.data(), *data_out_sz);
        return ret;
    }

    retry_buf = new rpc::retry_buffer{std::move(combined_out), ret};
    return rpc::error::NEED_MORE_MEMORY();
}

int post_enclave(uint64_t protocol_version, // version of the rpc call protocol
    uint64_t encoding,                      // format of the serialised data
    uint64_t tag,                           // info on the type of the call passed from the idl generator
    uint64_t caller_channel_zone_id,
    uint64_t caller_zone_id,
    uint64_t zone_id,
    uint64_t object_id,
    uint64_t interface_id,
    uint64_t method_id,
    uint64_t post_options_val,
    size_t sz_int,
    const char* data_in,
    size_t* data_out_sz,
    void** enclave_retry_buffer)
{
    if (protocol_version > rpc::get_version())
    {
        RPC_ERROR("Invalid version in post_enclave");
        return rpc::error::INVALID_VERSION();
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

    rpc_server->post(protocol_version, // version of the rpc call protocol
        rpc::encoding(encoding),       // format of the serialised data
        tag,
        {caller_channel_zone_id},
        {caller_zone_id},
        {zone_id},
        {object_id},
        {interface_id},
        {method_id},
        static_cast<rpc::post_options>(post_options_val),
        payload_size,
        payload.data(),
        in_back_channel);

    // Fire and forget - no output
    *data_out_sz = 0;
    return rpc::error::OK();
}

int try_cast_enclave(uint64_t protocol_version, uint64_t zone_id, uint64_t object_id, uint64_t interface_id,
    size_t sz_in_back_channel, const char* in_back_channel_buf,
    size_t sz_out_back_channel, char* out_back_channel_buf, size_t* out_back_channel_sz)
{
    if (protocol_version > rpc::get_version())
    {
        RPC_ERROR("Invalid version in try_cast_enclave");
        return rpc::error::INVALID_VERSION();
    }

    // Deserialize input back-channel
    std::vector<rpc::back_channel_entry> in_back_channel;
    if (sz_in_back_channel > 0)
    {
        yas::mem_istream is(in_back_channel_buf, sz_in_back_channel);
        yas::binary_iarchive<yas::mem_istream, yas::binary | yas::no_header> ia(is);
        ia& in_back_channel;
    }

    std::vector<rpc::back_channel_entry> out_back_channel;
    int ret = rpc_server->try_cast(protocol_version, {zone_id}, {object_id}, {interface_id},
        in_back_channel, out_back_channel);

    // Serialize output back-channel
    if (ret == rpc::error::OK() && !out_back_channel.empty())
    {
        yas::mem_ostream os;
        yas::binary_oarchive<yas::mem_ostream, yas::binary | yas::no_header> oa(os);
        oa& out_back_channel;
        auto yas_buf = os.get_shared_buffer();

        *out_back_channel_sz = yas_buf.size;
        if (*out_back_channel_sz <= sz_out_back_channel)
        {
            memcpy(out_back_channel_buf, yas_buf.data.get(), *out_back_channel_sz);
        }
    }
    else
    {
        *out_back_channel_sz = 0;
    }

    return ret;
}

int add_ref_enclave(uint64_t protocol_version,
    uint64_t destination_channel_zone_id,
    uint64_t destination_zone_id,
    uint64_t object_id,
    uint64_t caller_channel_zone_id,
    uint64_t caller_zone_id,
    uint64_t known_direction_zone_id,
    char build_out_param_channel,
    uint64_t* reference_count,
    size_t sz_in_back_channel, const char* in_back_channel_buf,
    size_t sz_out_back_channel, char* out_back_channel_buf, size_t* out_back_channel_sz)
{
    if (protocol_version > rpc::get_version())
    {
        *reference_count = 0;
        return rpc::error::INCOMPATIBLE_SERVICE();
    }

    // Deserialize input back-channel
    std::vector<rpc::back_channel_entry> in_back_channel;
    if (sz_in_back_channel > 0)
    {
        yas::mem_istream is(in_back_channel_buf, sz_in_back_channel);
        yas::binary_iarchive<yas::mem_istream, yas::binary | yas::no_header> ia(is);
        ia& in_back_channel;
    }

    std::vector<rpc::back_channel_entry> out_back_channel;
    auto err_code = rpc_server->add_ref(protocol_version,
        {destination_channel_zone_id},
        {destination_zone_id},
        {object_id},
        {caller_channel_zone_id},
        {caller_zone_id},
        {known_direction_zone_id},
        static_cast<rpc::add_ref_options>(build_out_param_channel),
        *reference_count,
        in_back_channel,
        out_back_channel);

    // Serialize output back-channel
    if (err_code == rpc::error::OK() && !out_back_channel.empty())
    {
        yas::mem_ostream os;
        yas::binary_oarchive<yas::mem_ostream, yas::binary | yas::no_header> oa(os);
        oa& out_back_channel;
        auto yas_buf = os.get_shared_buffer();

        *out_back_channel_sz = yas_buf.size;
        if (*out_back_channel_sz <= sz_out_back_channel)
        {
            memcpy(out_back_channel_buf, yas_buf.data.get(), *out_back_channel_sz);
        }
    }
    else
    {
        *out_back_channel_sz = 0;
    }

    return err_code;
}

int release_enclave(
    uint64_t protocol_version, uint64_t zone_id, uint64_t object_id, uint64_t caller_zone_id, char options, uint64_t* reference_count,
    size_t sz_in_back_channel, const char* in_back_channel_buf,
    size_t sz_out_back_channel, char* out_back_channel_buf, size_t* out_back_channel_sz)
{
    if (protocol_version > rpc::get_version())
    {
        *reference_count = 0;
        return rpc::error::INCOMPATIBLE_SERVICE();
    }

    // Deserialize input back-channel
    std::vector<rpc::back_channel_entry> in_back_channel;
    if (sz_in_back_channel > 0)
    {
        yas::mem_istream is(in_back_channel_buf, sz_in_back_channel);
        yas::binary_iarchive<yas::mem_istream, yas::binary | yas::no_header> ia(is);
        ia& in_back_channel;
    }

    std::vector<rpc::back_channel_entry> out_back_channel;
    auto err_code = rpc_server->release(protocol_version, {zone_id}, {object_id}, {caller_zone_id},
        static_cast<rpc::release_options>(options), *reference_count,
        in_back_channel, out_back_channel);

    // Serialize output back-channel
    if (err_code == rpc::error::OK() && !out_back_channel.empty())
    {
        yas::mem_ostream os;
        yas::binary_oarchive<yas::mem_ostream, yas::binary | yas::no_header> oa(os);
        oa& out_back_channel;
        auto yas_buf = os.get_shared_buffer();

        *out_back_channel_sz = yas_buf.size;
        if (*out_back_channel_sz <= sz_out_back_channel)
        {
            memcpy(out_back_channel_buf, yas_buf.data.get(), *out_back_channel_sz);
        }
    }
    else
    {
        *out_back_channel_sz = 0;
    }

    return err_code;
}

extern "C"
{
    int _Uelf64_valid_object()
    {
        return -1;
    }
}
