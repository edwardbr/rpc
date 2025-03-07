#pragma once

#include <mutex>
#include <coro/coro.hpp>

#include <tcp/tcp.h>

class tcp_channel_manager
{
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

    coro::mutex connection_mtx_;
    coro::net::tcp::client client_;
    std::chrono::milliseconds timeout_ = std::chrono::milliseconds(10000);
    std::atomic<uint64_t> sequence_number_ = 0;

    std::queue<std::vector<uint8_t>> send_queue_;

    // read from the peer and fill the buffer as it has been presized
    CORO_TASK(int) read(std::vector<char>& buf);

    // read and deserialise a envelope_prefix from a peer
    CORO_TASK(int) receive_prefix(tcp::envelope_prefix& prefix);

public:
    tcp_channel_manager(coro::net::tcp::client&& client, std::chrono::milliseconds timeout)
        : client_(std::move(client))
        , timeout_(timeout)
    {
    }

    CORO_TASK(void) launch_send_receive();

    // read a messsage from a peer
    CORO_TASK(int)
    receive_anonymous_payload(tcp::envelope_prefix& prefix, tcp::envelope_payload& payload, uint64_t sequence_number);

    // read a messsage from a peer
    template<class ReceivePayload>
    CORO_TASK(int)
    receive_payload(ReceivePayload& receivePayload, uint64_t sequence_number)
    {
        tcp::envelope_prefix prefix;
        tcp::envelope_payload payload;
        int err = CO_AWAIT receive_anonymous_payload(prefix, payload, sequence_number);
        if(err != rpc::error::OK())
            co_return err;

        assert(payload.payload_fingerprint == rpc::id<ReceivePayload>::get(prefix.version));

        auto str_err = rpc::from_yas_compressed_binary(rpc::span(payload.payload), receivePayload);
        if(!str_err.empty())
        {
            co_return rpc::error::TRANSPORT_ERROR();
        }

        co_return rpc::error::OK();
    }

    // send a message to a peer
    template<class SendPayload>
    CORO_TASK(int)
    send_payload(std::uint64_t protocol_version, SendPayload&& sendPayload, uint64_t sequence_number)
    {
        auto scoped_lock = co_await connection_mtx_.lock();

        auto payload = rpc::to_yas_binary(
            tcp::envelope_payload {.payload_fingerprint = rpc::id<SendPayload>::get(protocol_version),
                                   .payload = rpc::to_compressed_yas_binary(sendPayload)});

        auto prefix = tcp::envelope_prefix {
            .version = protocol_version, .sequence_number = sequence_number, .payload_size = payload.size()};
        send_queue_.push(rpc::to_yas_binary(prefix));
        send_queue_.push(payload);

        co_return rpc::error::OK();
    }

    // send a message from a peer and wait for a reply
    // this needs to go over a multi-coroutine multiplexer as multiple coroutines will be doing sends over the same tcp
    // connection concurrently and the receives need to be routed back to the senders
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

        auto err = CO_AWAIT send_payload(protocol_version, std::move(sendPayload), sequence_number);
        if(err != rpc::error::OK())
        {
            std::scoped_lock lock(pending_transmits_mtx_);
            pending_transmits_.erase(sequence_number);
            co_return err;
        }

        co_await res_payload.event; // now wait for the reply

        assert(res_payload.payload.payload_fingerprint == rpc::id<ReceivePayload>::get(res_payload.prefix.version));

        auto str_err = rpc::from_yas_compressed_binary(rpc::span(res_payload.payload.payload), receivePayload);
        if(!str_err.empty())
        {
            co_return rpc::error::TRANSPORT_ERROR();
        }

        co_return rpc::error::OK();
    }
};