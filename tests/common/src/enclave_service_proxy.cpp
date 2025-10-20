/*
 *   Copyright (c) 2025 Edward Boggis-Rolfe
 *   All rights reserved.
 */

#ifndef _IN_ENCLAVE
#include <thread>

#ifdef BUILD_ENCLAVE
#include <sgx_urts.h>
#include <sgx_capable.h>

#include <rpc/rpc.h>
#include "common/enclave_service_proxy.h"
#include <untrusted/enclave_marshal_test_u.h>

namespace rpc
{
    enclave_service_proxy::enclave_service_proxy(
        const char* name, destination_zone destination_zone_id, std::string filename, const std::shared_ptr<rpc::service>& svc)
        : service_proxy(name, destination_zone_id, svc)
        , filename_(filename)
    {
        // This proxy is for a child service, so hold a strong reference to the parent service
        // to prevent premature parent destruction until after child cleanup
        set_parent_service_reference(svc);
    }

    enclave_service_proxy::enclave_owner::~enclave_owner()
    {
        marshal_test_destroy_enclave(eid_);
        sgx_destroy_enclave(eid_);
    }

    std::shared_ptr<rpc::service_proxy> enclave_service_proxy::clone()
    {
        return std::shared_ptr<rpc::service_proxy>(new enclave_service_proxy(*this));
    }

    std::shared_ptr<enclave_service_proxy> enclave_service_proxy::create(
        const char* name, destination_zone destination_zone_id, const std::shared_ptr<rpc::service>& svc, std::string filename)
    {
        RPC_ASSERT(svc);
        auto ret
            = std::shared_ptr<enclave_service_proxy>(new enclave_service_proxy(name, destination_zone_id, filename, svc));
        return ret;
    }

    CORO_TASK(int)
    enclave_service_proxy::connect(rpc::interface_descriptor input_descr, rpc::interface_descriptor& output_descr)
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
#ifdef USE_RPC_TELEMETRY
            if (auto telemetry_service = rpc::get_telemetry_service(); telemetry_service)
            {
                auto error_message = std::string("sgx_create_enclave failed ") + std::to_string(status);
                telemetry_service->message(rpc::i_telemetry_service::err, error_message.c_str());
            }
#endif
            RPC_ERROR("Transport error - sgx_create_enclave failed");
            return rpc::error::TRANSPORT_ERROR();
        }
        int err_code = error::OK();
        uint64_t output_object_id = 0;
        status = marshal_test_init_enclave(eid_,
            &err_code,
            get_zone_id().get_val(),
            input_descr.object_id.get_val(),
            get_destination_zone_id().get_val(),
            &output_object_id);
        if (status)
        {
#ifdef USE_RPC_TELEMETRY
            if (auto telemetry_service = rpc::get_telemetry_service(); telemetry_service)
            {
                telemetry_service->message(rpc::i_telemetry_service::err, "marshal_test_init_enclave failed");
            }
#endif
            sgx_destroy_enclave(eid_);
            RPC_ERROR("Transport error - marshal_test_init_enclave failed");
            return rpc::error::TRANSPORT_ERROR();
        }
        if (err_code)
            return err_code;

        // class takes ownership of the enclave
        enclave_owner_ = std::make_shared<enclave_owner>(eid_);
        if (err_code)
            return err_code;

        output_descr = {{output_object_id}, get_destination_zone_id()};
        return err_code;
    }

    int enclave_service_proxy::send(uint64_t protocol_version,
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
        std::vector<char>& out_buf_,
        const std::vector<back_channel_entry>& in_back_channel,
        std::vector<back_channel_entry>& out_back_channel)
    {
        if (destination_zone_id != get_destination_zone_id())
        {
            RPC_ERROR("Zone not supported");
            return rpc::error::ZONE_NOT_SUPPORTED();
        }

        // Combine payload + back-channel into single buffer using YAS serialization
        std::vector<char> combined_in;
        {
            yas::mem_ostream os;
            yas::binary_oarchive<yas::mem_ostream, yas::binary | yas::no_header> oa(os);
            oa& in_size_;  // First write payload size
            if (in_size_ > 0)
                oa.write(in_buf_, in_size_);  // Then payload data
            oa& in_back_channel;  // Then back-channel vector
            auto yas_buf = os.get_shared_buffer();
            combined_in.assign(yas_buf.data.get(), yas_buf.data.get() + yas_buf.size);
        }

        // Prepare combined output buffer (will be split later)
        std::vector<char> combined_out(out_buf_.size() + 4096);  // Add space for back-channel

        int err_code = 0;
        size_t data_out_sz = 0;
        void* tls = nullptr;
        sgx_status_t status = ::call_enclave(eid_,
            &err_code,
            protocol_version,
            (uint64_t)encoding,
            tag,
            caller_channel_zone_id.get_val(),
            caller_zone_id.get_val(),
            destination_zone_id.get_val(),
            object_id.get_val(),
            interface_id.get_val(),
            method_id.get_val(),
            combined_in.size(),
            combined_in.data(),
            combined_out.size(),
            combined_out.data(),
            &data_out_sz,
            &tls);

        if (status)
        {
#ifdef USE_RPC_TELEMETRY
            if (auto telemetry_service = rpc::get_telemetry_service(); telemetry_service)
            {
                auto error_message = std::string("call_enclave failed ") + std::to_string(status);
                telemetry_service->message(rpc::i_telemetry_service::err, error_message.c_str());
            }
#endif
            RPC_ERROR("call_enclave gave an enclave error {}", (int)status);
            RPC_ASSERT(false);
            return rpc::error::TRANSPORT_ERROR();
        }

        if (err_code == rpc::error::NEED_MORE_MEMORY())
        {
            // data too small reallocate memory and try again
            combined_out.resize(data_out_sz);

            status = ::call_enclave(eid_,
                &err_code,
                protocol_version,
                (uint64_t)encoding,
                tag,
                caller_channel_zone_id.get_val(),
                caller_zone_id.get_val(),
                destination_zone_id.get_val(),
                object_id.get_val(),
                interface_id.get_val(),
                method_id.get_val(),
                combined_in.size(),
                combined_in.data(),
                combined_out.size(),
                combined_out.data(),
                &data_out_sz,
                &tls);
            if (status)
            {
#ifdef USE_RPC_TELEMETRY
                if (auto telemetry_service = rpc::get_telemetry_service(); telemetry_service)
                {
                    auto error_message = std::string("call_enclave failed ") + std::to_string(status);
                    telemetry_service->message(rpc::i_telemetry_service::err, error_message.c_str());
                }
#endif
                RPC_ERROR("call_enclave gave an enclave error {}", (int)status);
                RPC_ASSERT(false);
                return rpc::error::TRANSPORT_ERROR();
            }
        }

        // Split combined output back into payload + back-channel
        if (err_code == rpc::error::OK() && data_out_sz > 0)
        {
            yas::mem_istream is(combined_out.data(), data_out_sz);
            yas::binary_iarchive<yas::mem_istream, yas::binary | yas::no_header> ia(is);
            size_t payload_size = 0;
            ia& payload_size;  // Read payload size
            out_buf_.resize(payload_size);
            if (payload_size > 0)
                ia.read(out_buf_.data(), payload_size);  // Read payload
            ia& out_back_channel;  // Read back-channel
        }

        return err_code;
    }

    void enclave_service_proxy::post(uint64_t protocol_version,
        encoding encoding,
        uint64_t tag,
        caller_channel_zone caller_channel_zone_id,
        caller_zone caller_zone_id,
        destination_zone destination_zone_id,
        object object_id,
        interface_ordinal interface_id,
        method method_id,
        post_options options,
        size_t in_size_,
        const char* in_buf_,
        const std::vector<back_channel_entry>& in_back_channel)
    {
        if (destination_zone_id != get_destination_zone_id())
        {
            RPC_ERROR("Zone not supported");
            return;
        }

        // Combine payload + back-channel into single buffer using YAS serialization
        std::vector<char> combined_in;
        {
            yas::mem_ostream os;
            yas::binary_oarchive<yas::mem_ostream, yas::binary | yas::no_header> oa(os);
            oa& in_size_;  // First write payload size
            if (in_size_ > 0)
                oa.write(in_buf_, in_size_);  // Then payload data
            oa& in_back_channel;  // Then back-channel vector
            auto yas_buf = os.get_shared_buffer();
            combined_in.assign(yas_buf.data.get(), yas_buf.data.get() + yas_buf.size);
        }

        size_t data_out_sz = 0;
        void* tls = nullptr;

        int err_code = 0;
        sgx_status_t status = ::post_enclave(eid_,
            &err_code,
            protocol_version,
            (uint64_t)encoding,
            tag,
            caller_channel_zone_id.get_val(),
            caller_zone_id.get_val(),
            destination_zone_id.get_val(),
            object_id.get_val(),
            interface_id.get_val(),
            method_id.get_val(),
            (uint64_t)options,
            combined_in.size(),
            combined_in.data(),
            &data_out_sz,
            &tls);

        if (status)
        {
#ifdef USE_RPC_TELEMETRY
            if (auto telemetry_service = rpc::get_telemetry_service(); telemetry_service)
            {
                auto error_message = std::string("post_enclave failed ") + std::to_string(status);
                telemetry_service->message(rpc::i_telemetry_service::err, error_message.c_str());
            }
#endif
            RPC_ERROR("post_enclave gave an enclave error {}", (int)status);
        }
        // Fire and forget - ignore err_code for post
    }

    int enclave_service_proxy::try_cast(
        uint64_t protocol_version, destination_zone destination_zone_id, object object_id, interface_ordinal interface_id,
        const std::vector<back_channel_entry>& in_back_channel,
        std::vector<back_channel_entry>& out_back_channel)
    {
        // Serialize back-channel for EDL
        std::vector<char> in_bc_buf, out_bc_buf(1024);
        {
            yas::mem_ostream os;
            yas::binary_oarchive<yas::mem_ostream, yas::binary | yas::no_header> oa(os);
            oa& in_back_channel;
            auto yas_buf = os.get_shared_buffer();
            in_bc_buf.assign(yas_buf.data.get(), yas_buf.data.get() + yas_buf.size);
        }

        int err_code = 0;
        size_t out_bc_sz = 0;
        sgx_status_t status = ::try_cast_enclave(eid_, &err_code, protocol_version,
            destination_zone_id.get_val(), object_id.get_val(), interface_id.get_val(),
            in_bc_buf.size(), in_bc_buf.data(),
            out_bc_buf.size(), out_bc_buf.data(), &out_bc_sz);
        if (status == SGX_ERROR_ECALL_NOT_ALLOWED)
        {
            auto task = std::thread(
                [&]()
                {
                    status = ::try_cast_enclave(eid_, &err_code, protocol_version,
                        destination_zone_id.get_val(), object_id.get_val(), interface_id.get_val(),
                        in_bc_buf.size(), in_bc_buf.data(),
                        out_bc_buf.size(), out_bc_buf.data(), &out_bc_sz);
                });
            task.join();
        }
        if (status)
        {
#ifdef USE_RPC_TELEMETRY
            if (auto telemetry_service = rpc::get_telemetry_service(); telemetry_service)
            {
                auto error_message = std::string("try_cast_enclave failed ") + std::to_string(status);
                telemetry_service->message(rpc::i_telemetry_service::err, error_message.c_str());
            }
#endif
            RPC_ERROR("try_cast_enclave gave an enclave error {}", (int)status);
            RPC_ASSERT(false);
            return rpc::error::TRANSPORT_ERROR();
        }

        // Deserialize output back-channel
        if (err_code == rpc::error::OK() && out_bc_sz > 0)
        {
            yas::mem_istream is(out_bc_buf.data(), out_bc_sz);
            yas::binary_iarchive<yas::mem_istream, yas::binary | yas::no_header> ia(is);
            ia& out_back_channel;
        }
        return err_code;
    }

    int enclave_service_proxy::add_ref(uint64_t protocol_version,
        destination_channel_zone destination_channel_zone_id,
        destination_zone destination_zone_id,
        object object_id,
        caller_channel_zone caller_channel_zone_id,
        caller_zone caller_zone_id,
        known_direction_zone known_direction_zone_id,
        add_ref_options build_out_param_channel,
        uint64_t& reference_count,
        const std::vector<back_channel_entry>& in_back_channel,
        std::vector<back_channel_entry>& out_back_channel)
    {
#ifdef USE_RPC_TELEMETRY
        if (auto telemetry_service = rpc::get_telemetry_service(); telemetry_service)
        {
            telemetry_service->on_service_proxy_add_ref(get_zone_id(),
                destination_zone_id,
                destination_channel_zone_id,
                get_caller_zone_id(),
                object_id,
                build_out_param_channel);
        }
#endif
        // Serialize back-channel
        std::vector<char> in_bc_buf, out_bc_buf(1024);
        {
            yas::mem_ostream os;
            yas::binary_oarchive<yas::mem_ostream, yas::binary | yas::no_header> oa(os);
            oa& in_back_channel;
            auto yas_buf = os.get_shared_buffer();
            in_bc_buf.assign(yas_buf.data.get(), yas_buf.data.get() + yas_buf.size);
        }

        int err_code = 0;
        size_t out_bc_sz = 0;
        sgx_status_t status = ::add_ref_enclave(eid_,
            &err_code,
            protocol_version,
            destination_channel_zone_id.get_val(),
            destination_zone_id.get_val(),
            object_id.get_val(),
            caller_channel_zone_id.get_val(),
            caller_zone_id.get_val(),
            known_direction_zone_id.get_val(),
            (uint8_t)build_out_param_channel,
            &reference_count,
            in_bc_buf.size(), in_bc_buf.data(),
            out_bc_buf.size(), out_bc_buf.data(), &out_bc_sz);
        if (status == SGX_ERROR_ECALL_NOT_ALLOWED)
        {
            auto task = std::thread(
                [&]()
                {
                    status = ::add_ref_enclave(eid_,
                        &err_code,
                        protocol_version,
                        destination_channel_zone_id.get_val(),
                        destination_zone_id.get_val(),
                        object_id.get_val(),
                        caller_channel_zone_id.get_val(),
                        caller_zone_id.get_val(),
                        known_direction_zone_id.get_val(),
                        (uint8_t)build_out_param_channel,
                        &reference_count,
                        in_bc_buf.size(), in_bc_buf.data(),
                        out_bc_buf.size(), out_bc_buf.data(), &out_bc_sz);
                });
            task.join();
        }
        if (status)
        {
#ifdef USE_RPC_TELEMETRY
            if (auto telemetry_service = rpc::get_telemetry_service(); telemetry_service)
            {
                auto error_message = std::string("add_ref_enclave failed ") + std::to_string(status);
                telemetry_service->message(rpc::i_telemetry_service::err, error_message.c_str());
            }
#endif
            RPC_ERROR("add_ref_enclave gave an enclave error {}", (int)status);
            RPC_ASSERT(false);
            reference_count = 0;
            return rpc::error::ZONE_NOT_FOUND();
        }
        // Deserialize output back-channel
        if (err_code == rpc::error::OK() && out_bc_sz > 0)
        {
            yas::mem_istream is(out_bc_buf.data(), out_bc_sz);
            yas::binary_iarchive<yas::mem_istream, yas::binary | yas::no_header> ia(is);
            ia& out_back_channel;
        }
        return err_code;
    }

    int enclave_service_proxy::release(uint64_t protocol_version,
        destination_zone destination_zone_id,
        object object_id,
        caller_zone caller_zone_id,
        rpc::release_options options,
        uint64_t& reference_count,
        const std::vector<back_channel_entry>& in_back_channel,
        std::vector<back_channel_entry>& out_back_channel)
    {
        // Serialize back-channel
        std::vector<char> in_bc_buf, out_bc_buf(1024);
        {
            yas::mem_ostream os;
            yas::binary_oarchive<yas::mem_ostream, yas::binary | yas::no_header> oa(os);
            oa& in_back_channel;
            auto yas_buf = os.get_shared_buffer();
            in_bc_buf.assign(yas_buf.data.get(), yas_buf.data.get() + yas_buf.size);
        }

        int err_code = 0;
        size_t out_bc_sz = 0;
        sgx_status_t status = ::release_enclave(eid_,
            &err_code,
            protocol_version,
            destination_zone_id.get_val(),
            object_id.get_val(),
            caller_zone_id.get_val(),
            static_cast<char>(options),
            &reference_count,
            in_bc_buf.size(), in_bc_buf.data(),
            out_bc_buf.size(), out_bc_buf.data(), &out_bc_sz);
        if (status == SGX_ERROR_ECALL_NOT_ALLOWED)
        {
            auto task = std::thread(
                [&]()
                {
                    status = ::release_enclave(eid_,
                        &err_code,
                        protocol_version,
                        destination_zone_id.get_val(),
                        object_id.get_val(),
                        caller_zone_id.get_val(),
                        static_cast<char>(options),
                        &reference_count,
                        in_bc_buf.size(), in_bc_buf.data(),
                        out_bc_buf.size(), out_bc_buf.data(), &out_bc_sz);
                });
            task.join();
        }
        if (status)
        {
#ifdef USE_RPC_TELEMETRY
            if (auto telemetry_service = rpc::get_telemetry_service(); telemetry_service)
            {
                auto error_message = std::string("release_enclave failed ") + std::to_string(status);
                telemetry_service->message(rpc::i_telemetry_service::err, error_message.c_str());
            }
#endif
            RPC_ERROR("release_enclave gave an enclave error {}", (int)status);
            RPC_ASSERT(false);
            reference_count = 0;
            return rpc::error::ZONE_NOT_FOUND();
        }
        // Deserialize output back-channel
        if (err_code == rpc::error::OK() && out_bc_sz > 0)
        {
            yas::mem_istream is(out_bc_buf.data(), out_bc_sz);
            yas::binary_iarchive<yas::mem_istream, yas::binary | yas::no_header> ia(is);
            ia& out_back_channel;
        }
        return err_code;
    }
}
#endif
#endif
