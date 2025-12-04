/*
 *   Copyright (c) 2025 Edward Boggis-Rolfe
 *   All rights reserved.
 */

#pragma once

#include <array>
#include <mutex>
#include <queue>
#include <unordered_map>
#include <functional>

#include <coro/coro.hpp>
#include <rpc/rpc.h>
#include <spsc/spsc.h>
#include <rpc/service_proxies/spsc/queue.h>

namespace rpc::spsc
{
    using message_blob = std::array<uint8_t, 10024>;
    using queue_type = ::spsc::queue<message_blob, 10024>;

    class spsc_transport : public rpc::transport
    {
    public:
        using connection_handler = std::function<CORO_TASK(int)(const rpc::interface_descriptor& input_descr,
            rpc::interface_descriptor& output_interface,
            std::shared_ptr<rpc::service> child_service_ptr,
            std::shared_ptr<spsc_transport>)>;

    private:
        struct result_listener
        {
            coro::event event;
            envelope_prefix prefix;
            envelope_payload payload;
            int error_code = rpc::error::OK();
            std::chrono::steady_clock::time_point start_time = std::chrono::steady_clock::now();
        };

        std::unordered_map<uint64_t, result_listener*> pending_transmits_;
        std::mutex pending_transmits_mtx_;

        queue_type* send_spsc_queue_;
        queue_type* receive_spsc_queue_;
        std::chrono::milliseconds timeout_;

        std::atomic<uint64_t> sequence_number_ = 0;

        std::queue<std::vector<uint8_t>> send_queue_;
        coro::mutex send_queue_mtx_;

        connection_handler connection_handler_;
        coro::event shutdown_event_;
        std::shared_ptr<spsc_transport> keep_alive_;

        bool cancel_sent_ = false;
        bool cancel_confirmed_ = false;
        std::atomic<bool> peer_cancel_received_{false};
        std::atomic<bool> close_ack_queued_{false};

        // Reference counting for service proxies and task completion
        std::atomic<int> service_proxy_ref_count_{0};
        std::atomic<int> tasks_completed_{0};

        spsc_transport(std::string name,
            std::shared_ptr<rpc::service> service,
            rpc::zone adjacent_zone_id,
            std::chrono::milliseconds timeout,
            queue_type* send_spsc_queue,
            queue_type* receive_spsc_queue,
            connection_handler handler);

        // Producer/consumer coroutines
        CORO_TASK(void)
        receive_consumer_task(std::function<void(envelope_prefix, envelope_payload)> incoming_message_handler);
        CORO_TASK(void) send_producer_task();
        CORO_TASK(int) flush_send_queue();

        // Stub handlers (called when receiving messages)
        CORO_TASK(void) stub_handle_send(envelope_prefix prefix, envelope_payload payload);
        CORO_TASK(void) stub_handle_try_cast(envelope_prefix prefix, envelope_payload payload);
        CORO_TASK(void) stub_handle_add_ref(envelope_prefix prefix, envelope_payload payload);
        CORO_TASK(void) stub_handle_release(envelope_prefix prefix, envelope_payload payload);
        CORO_TASK(void) stub_handle_post(envelope_prefix prefix, envelope_payload payload);
        CORO_TASK(void) create_stub(envelope_prefix prefix, envelope_payload payload);

        void kill_connection() { }

    public:
        static std::shared_ptr<spsc_transport> create(std::string name,
            std::shared_ptr<rpc::service> service,
            rpc::zone adjacent_zone_id,
            std::chrono::milliseconds timeout,
            queue_type* send_spsc_queue,
            queue_type* receive_spsc_queue,
            connection_handler handler);

        virtual ~spsc_transport() = default;

        CORO_TASK(void) pump_send_and_receive();
        CORO_TASK(void) shutdown();

        // Service proxy lifecycle management
        void attach_service_proxy();
        CORO_TASK(void) detach_service_proxy();

        // Internal send payload helper
        template<class SendPayload>
        CORO_TASK(int)
        send_payload(
            std::uint64_t protocol_version, message_direction direction, SendPayload&& sendPayload, uint64_t sequence_number)
        {
            assert(direction);
            auto scoped_lock = CO_AWAIT send_queue_mtx_.lock();

            envelope_payload payload_envelope = {.payload_fingerprint = rpc::id<SendPayload>::get(protocol_version),
                .payload = rpc::to_compressed_yas_binary(sendPayload)};
            auto payload = rpc::to_yas_binary(payload_envelope);

            auto prefix = envelope_prefix{.version = protocol_version,
                .direction = direction,
                .sequence_number = sequence_number,
                .payload_size = payload.size()};

            RPC_DEBUG("send_payload {}\nprefix = {}\npayload = {}",
                get_service()->get_zone_id().get_val(),
                rpc::to_yas_json<std::string>(prefix),
                rpc::to_yas_json<std::string>(payload_envelope));

            send_queue_.push(rpc::to_yas_binary(prefix));
            send_queue_.push(payload);

            CO_RETURN rpc::error::OK();
        }

        // Send and wait for reply (used internally)
        template<class SendPayload, class ReceivePayload>
        CORO_TASK(int)
        call_peer(std::uint64_t protocol_version, SendPayload&& sendPayload, ReceivePayload& receivePayload)
        {
            // If peer has initiated shutdown, we're disconnected
            if (peer_cancel_received_)
            {
                RPC_DEBUG("call_peer: peer_cancel_received=true, returning CALL_CANCELLED for zone {}",
                    get_service()->get_zone_id().get_val());
                CO_RETURN rpc::error::CALL_CANCELLED();
            }

            auto sequence_number = ++sequence_number_;

            // Register the receive listener before we do the send
            result_listener res_payload;
            {
                RPC_DEBUG("call_peer started zone: {} sequence_number: {} id: {}",
                    get_service()->get_zone_id().get_val(),
                    sequence_number,
                    rpc::id<SendPayload>::get(rpc::get_version()));
                std::scoped_lock lock(pending_transmits_mtx_);
                auto [it, success] = pending_transmits_.try_emplace(sequence_number, &res_payload);
                assert(success);
            }

            auto err = CO_AWAIT send_payload(
                protocol_version, message_direction::send, std::move(sendPayload), sequence_number);
            if (err != rpc::error::OK())
            {
                RPC_ERROR("failed call_peer send_payload send");
                std::scoped_lock lock(pending_transmits_mtx_);
                RPC_ERROR("call_peer failed zone: {} sequence_number: {} id: {}",
                    get_service()->get_zone_id().get_val(),
                    sequence_number,
                    rpc::id<SendPayload>::get(rpc::get_version()));
                pending_transmits_.erase(sequence_number);
                CO_RETURN err;
            }

            CO_AWAIT res_payload.event; // now wait for the reply

            RPC_DEBUG("call_peer succeeded zone: {} sequence_number: {} id: {}",
                get_service()->get_zone_id().get_val(),
                sequence_number,
                rpc::id<SendPayload>::get(rpc::get_version()));

            assert(res_payload.payload.payload_fingerprint == rpc::id<ReceivePayload>::get(res_payload.prefix.version));

            auto str_err = rpc::from_yas_compressed_binary(rpc::span(res_payload.payload.payload), receivePayload);
            if (!str_err.empty())
            {
                RPC_ERROR("failed call_peer send_payload from_yas_compressed_binary");
                CO_RETURN rpc::error::TRANSPORT_ERROR();
            }

            CO_RETURN rpc::error::OK();
        }

        // rpc::transport override - connect handshake
        CORO_TASK(int) connect(rpc::interface_descriptor input_descr, rpc::interface_descriptor& output_descr) override;

        // i_marshaller implementations (from rpc::transport)
        CORO_TASK(int)
        send(uint64_t protocol_version,
            rpc::encoding encoding,
            uint64_t tag,
            rpc::caller_channel_zone caller_channel_zone_id,
            rpc::caller_zone caller_zone_id,
            rpc::destination_zone destination_zone_id,
            rpc::object object_id,
            rpc::interface_ordinal interface_id,
            rpc::method method_id,
            size_t in_size_,
            const char* in_buf_,
            std::vector<char>& out_buf_,
            const std::vector<rpc::back_channel_entry>& in_back_channel,
            std::vector<rpc::back_channel_entry>& out_back_channel) override;

        CORO_TASK(void)
        post(uint64_t protocol_version,
            rpc::encoding encoding,
            uint64_t tag,
            rpc::caller_channel_zone caller_channel_zone_id,
            rpc::caller_zone caller_zone_id,
            rpc::destination_zone destination_zone_id,
            rpc::object object_id,
            rpc::interface_ordinal interface_id,
            rpc::method method_id,
            rpc::post_options options,
            size_t in_size_,
            const char* in_buf_,
            const std::vector<rpc::back_channel_entry>& in_back_channel) override;

        CORO_TASK(int)
        try_cast(uint64_t protocol_version,
            rpc::destination_zone destination_zone_id,
            rpc::object object_id,
            rpc::interface_ordinal interface_id,
            const std::vector<rpc::back_channel_entry>& in_back_channel,
            std::vector<rpc::back_channel_entry>& out_back_channel) override;

        CORO_TASK(int)
        add_ref(uint64_t protocol_version,
            rpc::destination_zone destination_zone_id,
            rpc::object object_id,
            rpc::caller_channel_zone caller_channel_zone_id,
            rpc::caller_zone caller_zone_id,
            rpc::known_direction_zone known_direction_zone_id,
            rpc::add_ref_options build_out_param_channel,
            uint64_t& reference_count,
            const std::vector<rpc::back_channel_entry>& in_back_channel,
            std::vector<rpc::back_channel_entry>& out_back_channel) override;

        CORO_TASK(int)
        release(uint64_t protocol_version,
            rpc::destination_zone destination_zone_id,
            rpc::object object_id,
            rpc::caller_zone caller_zone_id,
            rpc::release_options options,
            uint64_t& reference_count,
            const std::vector<rpc::back_channel_entry>& in_back_channel,
            std::vector<rpc::back_channel_entry>& out_back_channel) override;
    };
}
