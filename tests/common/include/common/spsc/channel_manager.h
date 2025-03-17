#pragma once

#include <array>
#include <mutex>
#include <coro/coro.hpp>

#include <tcp/tcp.h>

#include <common/spsc/queue.h>

namespace rpc::spsc
{
    class channel_manager;

    using message_blob = std::array<uint8_t, 1024>;
    using queue_type = ::spsc::queue<message_blob, 1024>;
    
    class channel_manager
    {
    public:
        using connection_handler
            = std::function<CORO_TASK(int)(rpc::tcp::init_client_channel_send request,
                                           rpc::tcp::init_client_channel_response& response,
                                           rpc::shared_ptr<rpc::service> child_service_ptr,
                                           std::shared_ptr<channel_manager>)>;
    private:
    
    
        channel_manager(std::chrono::milliseconds timeout, rpc::shared_ptr<rpc::service> service,
                        queue_type* send_spsc_queue,
                        queue_type* receive_spsc_queue,
                        connection_handler handler
                    )
            : send_spsc_queue_(send_spsc_queue)
            , receive_spsc_queue_(receive_spsc_queue)
            , timeout_(timeout)
            , service_(service)
            , connection_handler_(handler)
        {}
    
        struct result_listener
        {
            coro::event event;
            tcp::envelope_prefix prefix;
            tcp::envelope_payload payload;
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

        rpc::shared_ptr<rpc::service> service_;

        connection_handler connection_handler_;
        coro::event shutdown_event_;
        bool shutdown_ = false;
        std::shared_ptr<channel_manager> keep_alive_;
        
        CORO_TASK(void)
        pump_messages(std::function<CORO_TASK(int)(tcp::envelope_prefix, tcp::envelope_payload)> incoming_message_handler);
        CORO_TASK(int) flush_send_queue();

        CORO_TASK(void) stub_handle_send(tcp::envelope_prefix prefix, tcp::envelope_payload payload);
        CORO_TASK(void) stub_handle_try_cast(tcp::envelope_prefix prefix, tcp::envelope_payload payload);
        CORO_TASK(void) stub_handle_add_ref(tcp::envelope_prefix prefix, tcp::envelope_payload payload);
        CORO_TASK(void) stub_handle_release(tcp::envelope_prefix prefix, tcp::envelope_payload payload);
        CORO_TASK(void) create_stub(tcp::envelope_prefix prefix, tcp::envelope_payload payload);
        
        void kill_connection() { }

    public:
        
        static std::shared_ptr<channel_manager> create(std::chrono::milliseconds timeout, rpc::shared_ptr<rpc::service> service,
                        queue_type* send_spsc_queue,
                        queue_type* receive_spsc_queue,
                        connection_handler handler);

        bool pump_send_and_receive();
        
        CORO_TASK(void) shutdown();

        // send a message to a peer
        template<class SendPayload>
        CORO_TASK(int)
        send_payload(std::uint64_t protocol_version, tcp::message_direction direction, SendPayload&& sendPayload,
                     uint64_t sequence_number)
        {
            assert(direction);
            auto scoped_lock = co_await send_queue_mtx_.lock();

            tcp::envelope_payload payload_envelope = {.payload_fingerprint = rpc::id<SendPayload>::get(protocol_version),
                                                 .payload = rpc::to_compressed_yas_binary(sendPayload)};
            auto payload = rpc::to_yas_binary(payload_envelope);

            auto prefix = tcp::envelope_prefix {.version = protocol_version,
                                           .direction = direction,
                                           .sequence_number = sequence_number,
                                           .payload_size = payload.size()};

            // std::string msg("send_payload ");
            // msg += std::to_string(service_->get_zone_id().get_val());
            // msg += std::string("\nprefix = ");
            // msg += rpc::to_yas_json<std::string>(prefix);
            // msg += std::string("\npayload = ");
            // msg += rpc::to_yas_json<std::string>(payload_envelope);
            // LOG_CSTR(msg.c_str());

            send_queue_.push(rpc::to_yas_binary(prefix));
            send_queue_.push(payload);

            co_return rpc::error::OK();
        }

        // send a message from a peer and wait for a reply
        // this needs to go over a multi-coroutine multiplexer as multiple coroutines will be doing sends over the same
        // tcp connection concurrently and the receives need to be routed back to the senders
        template<class SendPayload, class ReceivePayload>
        CORO_TASK(int)
        call_peer(std::uint64_t protocol_version, SendPayload&& sendPayload, ReceivePayload& receivePayload)
        {
            auto sequence_number = ++sequence_number_;

            // we register the receive listener before we do the send
            result_listener res_payload;
            {
                std::scoped_lock lock(pending_transmits_mtx_);
                auto [it, success] = pending_transmits_.try_emplace(sequence_number, &res_payload);
                assert(success);
            }

            auto err = CO_AWAIT send_payload(protocol_version, tcp::message_direction::send, std::move(sendPayload),
                                             sequence_number);
            if(err != rpc::error::OK())
            {
                LOG_CSTR("failed call_peer send_payload send");
                std::scoped_lock lock(pending_transmits_mtx_);
                pending_transmits_.erase(sequence_number);
                co_return err;
            }

            co_await res_payload.event; // now wait for the reply

            assert(res_payload.payload.payload_fingerprint == rpc::id<ReceivePayload>::get(res_payload.prefix.version));

            auto str_err = rpc::from_yas_compressed_binary(rpc::span(res_payload.payload.payload), receivePayload);
            if(!str_err.empty())
            {
                LOG_CSTR("failed call_peer send_payload from_yas_compressed_binary");
                co_return rpc::error::TRANSPORT_ERROR();
            }

            co_return rpc::error::OK();
        }
    };
}