#pragma once

#include <mutex>
#include <coro/coro.hpp>

#include <tcp/tcp.h>

namespace rpc::tcp
{
    class channel_manager;

    struct worker_release
    {
        std::shared_ptr<channel_manager> channel_manager;
    };

    class channel_manager
    {
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

        coro::net::tcp::client client_;
        std::chrono::milliseconds timeout_;
        std::weak_ptr<worker_release> worker_release_;

        std::atomic<uint64_t> sequence_number_ = 0;

        std::queue<std::vector<uint8_t>> send_queue_;
        coro::mutex send_queue_mtx_;

        rpc::shared_ptr<rpc::service> service_;

        // read from the peer and fill the buffer as it has been presized
        CORO_TASK(int) read(std::vector<char>& buf);

        // read and deserialise a envelope_prefix from a peer
        CORO_TASK(int) receive_prefix(envelope_prefix& prefix);

        CORO_TASK(void)
        pump_messages(std::function<CORO_TASK(int)(envelope_prefix, envelope_payload)> incoming_message_handler);

        CORO_TASK(void) stub_handle_send(envelope_prefix prefix, envelope_payload payload);
        CORO_TASK(void) stub_handle_try_cast(envelope_prefix prefix, envelope_payload payload);
        CORO_TASK(void) stub_handle_add_ref(envelope_prefix prefix, envelope_payload payload);
        CORO_TASK(void) stub_handle_release(envelope_prefix prefix, envelope_payload payload);

        void kill_connection() { }

    public:
        channel_manager(coro::net::tcp::client client, std::chrono::milliseconds timeout,
                        std::weak_ptr<worker_release> worker_release, rpc::shared_ptr<rpc::service> service)
            : client_(std::move(client))
            , timeout_(timeout)
            , worker_release_(worker_release)
            , service_(service)
        {
            assert(client_.socket().is_valid());
        }

        ~channel_manager() { }

        CORO_TASK(void) pump_send_and_receive();

        // read a messsage from a peer
        CORO_TASK(int)
        receive_anonymous_payload(envelope_prefix& prefix, envelope_payload& payload, uint64_t sequence_number);

        // read a messsage from a peer
        template<class ReceivePayload>
        CORO_TASK(int)
        receive_payload(ReceivePayload& receivePayload, uint64_t sequence_number)
        {
            // std::string msg("receive_payload ");
            // msg += std::to_string(service_->get_zone_id().get_val());
            // LOG_CSTR(msg.c_str());

            envelope_prefix prefix;
            envelope_payload payload;
            int err = CO_AWAIT receive_anonymous_payload(prefix, payload, sequence_number);
            if(err != rpc::error::OK())
            {
                LOG_CSTR("failed receive_payload receive_anonymous_payload");
                co_return err;
            }

            assert(payload.payload_fingerprint == rpc::id<ReceivePayload>::get(prefix.version));

            auto str_err = rpc::from_yas_compressed_binary(rpc::span(payload.payload), receivePayload);
            if(!str_err.empty())
            {
                LOG_CSTR("failed receive_payload from_yas_compressed_binary");
                co_return rpc::error::TRANSPORT_ERROR();
            }

            // msg = std::string("receive_payload complete ");
            // msg += std::to_string(service_->get_zone_id().get_val());
            // msg += std::string("\nprefix = ");
            // msg += rpc::to_yas_json<std::string>(prefix);
            // msg += std::string("\npayload = ");
            // msg += rpc::to_yas_json<std::string>(payload);
            // LOG_CSTR(msg.c_str());

            co_return rpc::error::OK();
        }

        // send a message to a peer
        template<class SendPayload>
        CORO_TASK(int)
        send_payload(std::uint64_t protocol_version, message_direction direction, SendPayload&& sendPayload,
                     uint64_t sequence_number)
        {
            assert(direction);
            auto scoped_lock = co_await send_queue_mtx_.lock();

            envelope_payload payload_envelope = {.payload_fingerprint = rpc::id<SendPayload>::get(protocol_version),
                                                 .payload = rpc::to_compressed_yas_binary(sendPayload)};
            auto payload = rpc::to_yas_binary(payload_envelope);

            auto prefix = envelope_prefix {.version = protocol_version,
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

        template<class SendPayload>
        CORO_TASK(int)
        immediate_send_payload(std::uint64_t protocol_version, message_direction direction, SendPayload&& sendPayload,
                               uint64_t sequence_number)
        {
            assert(direction);
            auto scoped_lock = co_await send_queue_mtx_.lock();

            auto payload = rpc::to_yas_binary(
                envelope_payload {.payload_fingerprint = rpc::id<SendPayload>::get(protocol_version),
                                  .payload = rpc::to_compressed_yas_binary(sendPayload)});

            auto prefix = envelope_prefix {.version = protocol_version,
                                           .direction = direction,
                                           .sequence_number = sequence_number,
                                           .payload_size = payload.size()};

            // std::string msg("immediate_send_payload ");
            // msg += std::to_string(service_->get_zone_id().get_val());
            // msg = std::string("\nprefix = ");
            // msg += rpc::to_yas_json<std::string>(prefix);
            // msg += std::string("\npayload = ");
            // msg += rpc::to_yas_json<std::string>(payload);
            // LOG_CSTR(msg.c_str());

            std::vector<uint8_t> buf = rpc::to_yas_binary(prefix);
            auto marshal_status = client_.send(std::span {(const char*)buf.data(), buf.size()});
            if(marshal_status.first == coro::net::send_status::try_again)
            {
                auto status = co_await client_.poll(coro::poll_op::write, timeout_);
                if(status != coro::poll_status::event)
                {
                    LOG_CSTR("failed immediate_send_payload prefix");
                    CO_RETURN rpc::error::TRANSPORT_ERROR();
                }
                marshal_status = client_.send(std::span {(const char*)buf.data(), buf.size()});
            }
            if(marshal_status.first != coro::net::send_status::ok)
            {
                LOG_CSTR("failed immediate_send_payload prefix send");
                CO_RETURN rpc::error::TRANSPORT_ERROR();
            }

            marshal_status = client_.send(std::span {(const char*)payload.data(), payload.size()});
            if(marshal_status.first == coro::net::send_status::try_again)
            {
                auto status = co_await client_.poll(coro::poll_op::write, timeout_);
                if(status != coro::poll_status::event)
                {
                    // std::string msg("client_.poll failed ");
                    // msg += std::to_string(service_->get_zone_id().get_val());
                    // msg += " fd = ";
                    // msg += std::to_string(client_.socket().native_handle());
                    // LOG_CSTR(msg.c_str());

                    CO_RETURN rpc::error::TRANSPORT_ERROR();
                }

                marshal_status = client_.send(std::span {(const char*)payload.data(), payload.size()});
            }
            if(marshal_status.first != coro::net::send_status::ok)
            {
                // std::string msg("client_.send failed ");
                // msg += std::to_string(service_->get_zone_id().get_val());
                // msg += " fd = ";
                // msg += std::to_string(client_.socket().native_handle());
                // LOG_CSTR(msg.c_str());

                CO_RETURN rpc::error::TRANSPORT_ERROR();
            }
            CO_RETURN rpc::error::OK();
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

            auto err = CO_AWAIT send_payload(protocol_version, message_direction::send, std::move(sendPayload),
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