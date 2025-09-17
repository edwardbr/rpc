/*
 *   Copyright (c) 2025 Edward Boggis-Rolfe
 *   All rights reserved.
 */

#pragma once

#include <array>
#include <mutex>
#include <coro/coro.hpp>

#include <spsc/spsc.h>

#include <common/spsc/queue.h>

namespace rpc::spsc
{
    class channel_manager;

    using message_blob = std::array<uint8_t, 1024>;
    using queue_type = ::spsc::queue<message_blob, 1024>;

    class channel_manager
    {
    public:
        using connection_handler = std::function<CORO_TASK(int)(const rpc::interface_descriptor& input_descr,
            rpc::interface_descriptor& output_interface,
            std::shared_ptr<rpc::service> child_service_ptr,
            std::shared_ptr<channel_manager>)>;

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

        std::shared_ptr<rpc::service> service_;

        connection_handler connection_handler_;
        coro::event shutdown_event_;
        std::shared_ptr<channel_manager> keep_alive_;

        bool cancel_sent_ = false;
        bool cancel_confirmed_ = false;
        bool peer_cancel_received_ = false;

        channel_manager(std::chrono::milliseconds timeout,
            std::shared_ptr<rpc::service> service,
            queue_type* send_spsc_queue,
            queue_type* receive_spsc_queue,
            connection_handler handler);

        CORO_TASK(void)
        pump_messages(std::function<CORO_TASK(int)(envelope_prefix, envelope_payload)> incoming_message_handler);
        CORO_TASK(int) flush_send_queue();

        CORO_TASK(void) stub_handle_send(envelope_prefix prefix, envelope_payload payload);
        CORO_TASK(void) stub_handle_try_cast(envelope_prefix prefix, envelope_payload payload);
        CORO_TASK(void) stub_handle_add_ref(envelope_prefix prefix, envelope_payload payload);
        CORO_TASK(void) stub_handle_release(envelope_prefix prefix, envelope_payload payload);
        CORO_TASK(void) create_stub(envelope_prefix prefix, envelope_payload payload);

        void kill_connection() { }

    public:
        static std::shared_ptr<channel_manager> create(std::chrono::milliseconds timeout,
            std::shared_ptr<rpc::service> service,
            queue_type* send_spsc_queue,
            queue_type* receive_spsc_queue,
            connection_handler handler);

        bool pump_send_and_receive();

        CORO_TASK(void) shutdown();

        // send a message to a peer
        template<class SendPayload>
        CORO_TASK(int)
        send_payload(
            std::uint64_t protocol_version, message_direction direction, SendPayload&& sendPayload, uint64_t sequence_number)
        {
            assert(direction);
            auto scoped_lock = co_await send_queue_mtx_.lock();

            envelope_payload payload_envelope = {.payload_fingerprint = rpc::id<SendPayload>::get(protocol_version),
                .payload = rpc::to_compressed_yas_binary(sendPayload)};
            auto payload = rpc::to_yas_binary(payload_envelope);

            auto prefix = envelope_prefix{.version = protocol_version,
                .direction = direction,
                .sequence_number = sequence_number,
                .payload_size = payload.size()};

            RPC_DEBUG("send_payload {}\nprefix = {}\npayload = {}",
                     service_->get_zone_id().get_val(),
                     rpc::to_yas_json<std::string>(prefix),
                     rpc::to_yas_json<std::string>(payload_envelope));

            send_queue_.push(rpc::to_yas_binary(prefix));
            send_queue_.push(payload);

            CO_RETURN rpc::error::OK();
        }

        // send a message from a peer and wait for a reply
        // this needs to go over a multi-coroutine multiplexer as multiple coroutines will be doing sends over the same
        // spsc connection concurrently and the receives need to be routed back to the senders
        template<class SendPayload, class ReceivePayload>
        CORO_TASK(int)
        call_peer(std::uint64_t protocol_version, SendPayload&& sendPayload, ReceivePayload& receivePayload)
        {
            // this should never happen as the service proxy is dieing

            auto sequence_number = ++sequence_number_;

            // we register the receive listener before we do the send
            result_listener res_payload;
            {
                RPC_DEBUG("call_peer started zone: {} sequence_number: {} id: {}", 
                         service_->get_zone_id().get_val(), sequence_number, rpc::id<SendPayload>::get(rpc::get_version()));
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
                         service_->get_zone_id().get_val(), sequence_number, rpc::id<SendPayload>::get(rpc::get_version()));
                pending_transmits_.erase(sequence_number);
                CO_RETURN err;
            }

            co_await res_payload.event; // now wait for the reply

            RPC_DEBUG("call_peer succeeded zone: {} sequence_number: {} id: {}", 
                     service_->get_zone_id().get_val(), sequence_number, rpc::id<SendPayload>::get(rpc::get_version()));

            assert(res_payload.payload.payload_fingerprint == rpc::id<ReceivePayload>::get(res_payload.prefix.version));

            auto str_err = rpc::from_yas_compressed_binary(rpc::span(res_payload.payload.payload), receivePayload);
            if (!str_err.empty())
            {
                RPC_ERROR("failed call_peer send_payload from_yas_compressed_binary");
                CO_RETURN rpc::error::TRANSPORT_ERROR();
            }

            CO_RETURN rpc::error::OK();
        }
    };
}
