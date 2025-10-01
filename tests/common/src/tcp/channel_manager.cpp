/*
 *   Copyright (c) 2025 Edward Boggis-Rolfe
 *   All rights reserved.
 */

#include <common/tcp/channel_manager.h>
#include <common/tcp/service_proxy.h>

namespace rpc::tcp
{
    /// Manages sending queued requests to other peers and receiving responses.
    /// This method is responsible for the bidirectional communication flow between peers.
    ///
    /// The method:
    /// - Sends queued requests to other peers
    /// - Receives and processes responses
    /// - Notifies the service proxy when operations complete
    /// - Handles different message types (calls, try casts, reference counting)
    ///
    /// The supported message types are:
    /// - call_send: For remote procedure calls
    /// - try_cast_send: For type casting attempts
    /// - addref_send: For reference counting increments
    /// - release_send: For reference counting decrements
    /// - fulfils promises for pending awaits on sent messages to the peer
    ///
    /// @note This method runs as a coroutine task and depends on a valid socket connection

    CORO_TASK(void) channel_manager::pump_send_and_receive()
    {
        assert(client_.socket().is_valid());

        co_await pump_messages(
            [this](envelope_prefix prefix, envelope_payload payload) -> coro::task<int>
            {
                // do a call
                if (payload.payload_fingerprint == rpc::id<tcp::call_send>::get(prefix.version))
                {
                    service_->get_scheduler()->schedule(stub_handle_send(std::move(prefix), std::move(payload)));
                }
                // do a try cast
                else if (payload.payload_fingerprint == rpc::id<tcp::try_cast_send>::get(prefix.version))
                {
                    service_->get_scheduler()->schedule(stub_handle_try_cast(std::move(prefix), std::move(payload)));
                }
                // do an add_ref
                else if (payload.payload_fingerprint == rpc::id<tcp::addref_send>::get(prefix.version))
                {
                    service_->get_scheduler()->schedule(stub_handle_add_ref(std::move(prefix), std::move(payload)));
                }
                // do a release
                else if (payload.payload_fingerprint == rpc::id<tcp::release_send>::get(prefix.version))
                {
                    service_->get_scheduler()->schedule(stub_handle_release(std::move(prefix), std::move(payload)));
                }
                else
                {
                    // now find the relevant event handler and set its values before triggering it
                    result_listener* result = nullptr;
                    {
                        std::scoped_lock lock(pending_transmits_mtx_);
                        auto it = pending_transmits_.find(prefix.sequence_number);
                        assert(it != pending_transmits_.end());
                        result = it->second;
                        pending_transmits_.erase(it);
                    }

                    result->prefix = std::move(prefix);
                    result->payload = std::move(payload);
                    result->error_code = rpc::error::OK();
                    result->event.set();
                }
                co_return rpc::error::OK();
            });
    }

    /// Continuously pumps messages between peers, handling both sending queued messages and receiving incoming ones.
    /// This is a core communication loop that manages the bidirectional flow of RPC messages.
    ///
    /// @param incoming_message_handler A callback function that processes received messages.
    ///        It takes an envelope_prefix and envelope_payload as parameters and returns a coroutine task with an int
    ///        result.
    ///
    /// The method performs the following operations:
    /// - Sends queued messages from send_queue_ when the socket is ready for writing
    /// - Receives incoming messages in two parts: prefix and payload
    /// - Handles socket polling and network errors
    /// - Manages partial reads and writes
    /// - Processes received messages through the provided handler
    /// - Cancels pending transmissions if the connection is lost
    ///
    /// The message format consists of:
    /// 1. A fixed-size envelope prefix containing metadata
    /// 2. A variable-size payload containing the actual message data
    ///
    /// @note This method runs until worker_release_ is unlocked or a fatal error occurs

    CORO_TASK(void)
    channel_manager::pump_messages(std::function<CORO_TASK(int)(envelope_prefix, envelope_payload)> incoming_message_handler)
    {
        static auto envelope_prefix_saved_size = rpc::yas_binary_saved_size(envelope_prefix());

        assert(client_.socket().is_valid());

        bool expecting_prefix = true;
        bool read_complete = true;
        envelope_prefix prefix{};
        std::vector<char> buf;
        std::span remaining_span(buf.begin(), buf.end());

        while (worker_release_.lock())
        {
            auto scoped_lock = co_await send_queue_mtx_.lock();
            bool failed = false;
            while (!send_queue_.empty())
            {
                auto& item = send_queue_.front();
                auto marshal_status = client_.send(std::span{(const char*)item.data(), item.size()});
                if (marshal_status.first == coro::net::send_status::try_again)
                {
                    auto status = co_await client_.poll(coro::poll_op::write, std::chrono::milliseconds(1));
                    if (status == coro::poll_status::timeout)
                    {
                        break;
                    }
                    if (status != coro::poll_status::event)
                    {
                        failed = true;
                        break;
                    }

                    marshal_status = client_.send(std::span{(const char*)item.data(), item.size()});
                }
                if (marshal_status.first != coro::net::send_status::ok)
                {
                    RPC_ERROR("failed to send data to peer");
                    failed = true;

                    break;
                }
                send_queue_.pop();
            }
            if (failed)
            {
                break;
            }

            if (read_complete)
            {
                size_t buf_size = expecting_prefix ? envelope_prefix_saved_size : prefix.payload_size;
                buf = std::vector<char>(buf_size, '\0');
                remaining_span = std::span(buf.begin(), buf.end());
                read_complete = false;
            }

            {
                auto pstatus = co_await client_.poll(coro::poll_op::read, std::chrono::milliseconds(1));
                if (pstatus == coro::poll_status::timeout)
                {
                    continue;
                }
                if (pstatus == coro::poll_status::error)
                {
                    RPC_ERROR("failed pstatus == coro::poll_status::error");
                    break;
                }

                if (pstatus == coro::poll_status::closed)
                {
                    RPC_ERROR("failed pstatus == coro::poll_status::closed");
                    break;
                }

                auto [recv_status, recv_bytes] = client_.recv(remaining_span);

                if (recv_status == coro::net::recv_status::try_again || recv_status == coro::net::recv_status::would_block)
                {
                    continue;
                }
                if (recv_status == coro::net::recv_status::ok)
                {
                    if (recv_bytes.size() != remaining_span.size())
                    {
                        remaining_span = std::span(remaining_span.begin() + recv_bytes.size(), remaining_span.end());
                        continue;
                    }
                    else
                    {
                        // buf is now fully proceed with reading the rest of the data with this message
                        read_complete = true;
                        if (expecting_prefix)
                        {
                            expecting_prefix = false;
                            auto str_err = rpc::from_yas_binary(rpc::span(buf), prefix);
                            if (!str_err.empty())
                            {
                                RPC_ERROR("failed invalid prefix");
                                break;
                            }
                            assert(prefix.direction);
                        }
                        else
                        {
                            expecting_prefix = true;

                            envelope_payload payload;
                            auto str_err = rpc::from_yas_binary(rpc::span(buf), payload);
                            if (!str_err.empty())
                            {
                                RPC_ERROR("failed bad payload format");
                                break;
                            }
                            auto ret = co_await incoming_message_handler(std::move(prefix), std::move(payload));
                            if (ret != rpc::error::OK())
                            {
                                RPC_ERROR("failed incoming_message_handler");
                                break;
                            }
                        }
                    }
                }
                else
                {
                    RPC_ERROR("failed invalid received message");
                    break;
                }
            }
        }

        {
            std::scoped_lock lock(pending_transmits_mtx_);
            for (auto it : pending_transmits_)
            {
                it.second->error_code = rpc::error::CALL_CANCELLED();
                it.second->event.set();
            }
        }
    }

    // do a try cast
    CORO_TASK(void) channel_manager::stub_handle_send(envelope_prefix prefix, envelope_payload payload)
    {
        RPC_DEBUG("send request");

        assert(prefix.direction == message_direction::send || prefix.direction == message_direction::one_way);

        tcp::call_send request;
        auto str_err = rpc::from_yas_compressed_binary(rpc::span(payload.payload), request);
        if (!str_err.empty())
        {
            RPC_ERROR("failed from_yas_compressed_binary call_send");
            kill_connection();
            CO_RETURN;
        }

        std::vector<char> out_buf;
        auto ret = co_await service_->send(prefix.version,
            request.encoding,
            request.tag,
            {request.caller_channel_zone_id},
            {request.caller_zone_id},
            {request.destination_zone_id},
            {request.object_id},
            {request.interface_id},
            {request.method_id},
            request.payload.size(),
            request.payload.data(),
            out_buf);

        if (ret != rpc::error::OK())
        {
            RPC_ERROR("failed send");
        }

        if (prefix.direction == message_direction::one_way)
            CO_RETURN;

        auto err = CO_AWAIT send_payload(prefix.version,
            message_direction::receive,
            tcp::call_receive{.payload = std::move(out_buf), .err_code = ret},
            prefix.sequence_number);
        if (err != rpc::error::OK())
        {
            RPC_ERROR("failed send_payload");
            kill_connection();
            CO_RETURN;
        }
        RPC_DEBUG("send request complete");
        CO_RETURN;
    }

    // do a try cast
    CORO_TASK(void)
    channel_manager::stub_handle_try_cast(envelope_prefix prefix, envelope_payload payload)
    {
        RPC_DEBUG("try_cast request");

        tcp::try_cast_send request;
        auto str_err = rpc::from_yas_compressed_binary(rpc::span(payload.payload), request);
        if (!str_err.empty())
        {
            RPC_ERROR("failed try_cast_send from_yas_compressed_binary");
            kill_connection();
            CO_RETURN;
        }

        std::vector<char> out_buf;
        auto ret = co_await service_->try_cast(
            prefix.version, {request.destination_zone_id}, {request.object_id}, {request.interface_id});
        if (ret != rpc::error::OK())
        {
            RPC_ERROR("failed try_cast");
        }

        auto err = CO_AWAIT send_payload(
            prefix.version, message_direction::receive, tcp::try_cast_receive{.err_code = ret}, prefix.sequence_number);
        if (err != rpc::error::OK())
        {
            RPC_ERROR("failed try_cast_send send_payload");
            kill_connection();
            CO_RETURN;
        }
        RPC_DEBUG("try_cast request complete");
        CO_RETURN;
    }

    // do an add_ref
    CORO_TASK(void) channel_manager::stub_handle_add_ref(envelope_prefix prefix, envelope_payload payload)
    {
        RPC_DEBUG("add_ref request");

        tcp::addref_send request;
        auto str_err = rpc::from_yas_compressed_binary(rpc::span(payload.payload), request);
        if (!str_err.empty())
        {
            RPC_ERROR("failed addref_send from_yas_compressed_binary");
            kill_connection();
            CO_RETURN;
        }

        std::vector<char> out_buf;
        uint64_t ref_count = 0;
        auto ret = co_await service_->add_ref(prefix.version,
            {request.destination_channel_zone_id},
            {request.destination_zone_id},
            {request.object_id},
            {request.caller_channel_zone_id},
            {request.caller_zone_id},
            {request.known_direction_zone_id},
            (rpc::add_ref_options)request.build_out_param_channel,
            ref_count);

        if (ret != rpc::error::OK())
        {
            RPC_ERROR("failed add_ref");
        }

        auto err = CO_AWAIT send_payload(prefix.version,
            message_direction::receive,
            tcp::addref_receive{.ref_count = ref_count, .err_code = ret},
            prefix.sequence_number);
        if (err != rpc::error::OK())
        {
            RPC_ERROR("failed addref_send send_payload");
            kill_connection();
            CO_RETURN;
        }
        RPC_DEBUG("add_ref request complete");
        CO_RETURN;
    }

    // do a release
    CORO_TASK(void) channel_manager::stub_handle_release(envelope_prefix prefix, envelope_payload payload)
    {
        RPC_DEBUG("release request");
        tcp::release_send request;
        auto str_err = rpc::from_yas_compressed_binary(rpc::span(payload.payload), request);
        if (!str_err.empty())
        {
            RPC_ERROR("failed release_send from_yas_compressed_binary");
            kill_connection();
            CO_RETURN;
        }

        std::vector<char> out_buf;
        uint64_t ref_count = 0;
        auto ret = co_await service_->release(
            prefix.version, {request.destination_zone_id}, {request.object_id}, {request.caller_zone_id}, static_cast<rpc::release_options>(request.options), ref_count);

        if (ret != rpc::error::OK())
        {
            RPC_ERROR("failed release");
        }

        auto err = CO_AWAIT send_payload(prefix.version,
            message_direction::receive,
            tcp::release_receive{.ref_count = ref_count, .err_code = ret},
            prefix.sequence_number);
        if (err != rpc::error::OK())
        {
            RPC_ERROR("failed release_send send_payload");
            kill_connection();
            CO_RETURN;
        }
        RPC_DEBUG("release request complete");
        CO_RETURN;
    }

    // read from the connection and fill the buffer as it has been presized
    CORO_TASK(int) channel_manager::read(std::vector<char>& buf)
    {
        assert(client_.socket().is_valid());

        std::span remaining_span(buf.begin(), buf.end());
        do
        {
            auto [recv_status, recv_bytes] = client_.recv(remaining_span);
            if (recv_status == coro::net::recv_status::ok)
            {
                if (recv_bytes.size() == remaining_span.size())
                {
                    // buf is now full proceed with reading the rest of the data with this message
                    break;
                }
                remaining_span = std::span(remaining_span.begin() + recv_bytes.size(), remaining_span.end());
                continue;
            }

            // something went wrong so we dont expect the recv_bytes to have anything
            assert(recv_bytes.empty());

            if (recv_status == coro::net::recv_status::try_again || recv_status == coro::net::recv_status::would_block)
            {

                auto pstatus = co_await client_.poll(coro::poll_op::read, timeout_);

                if (pstatus == coro::poll_status::timeout)
                {
                    RPC_ERROR("failed read timeout");

                    // a timeout has occurred
                    co_return rpc::error::SERVICE_PROXY_LOST_CONNECTION();
                }

                if (pstatus == coro::poll_status::error)
                {
                    RPC_ERROR("failed read error");
                    // errno;
                    co_return rpc::error::SERVICE_PROXY_LOST_CONNECTION();
                }

                if (pstatus == coro::poll_status::closed)
                {
                    RPC_ERROR("failed read closed");
                    // connection closed by peer
                    co_return rpc::error::SERVICE_PROXY_LOST_CONNECTION();
                }

                continue;
            }

            // connection closed by peer
            RPC_ERROR("failed read connection closed by peer");
            co_return rpc::error::SERVICE_PROXY_LOST_CONNECTION();
        } while (true);
        co_return rpc::error::OK();
    }

    CORO_TASK(int)
    channel_manager::receive_prefix(envelope_prefix& prefix)
    {
        static auto envelope_prefix_saved_size = rpc::yas_binary_saved_size(envelope_prefix());

        std::vector<char> buf(envelope_prefix_saved_size, '\0');
        int err = CO_AWAIT read(buf);
        if (err != rpc::error::OK())
        {
            RPC_ERROR("failed receive_prefix read");
            CO_RETURN err;
        }

        auto str_err = rpc::from_yas_binary(rpc::span(buf), prefix);
        if (!str_err.empty())
        {
            RPC_ERROR("failed receive_prefix from_yas_binary");
            co_return rpc::error::TRANSPORT_ERROR();
        }
        assert(prefix.direction);

        RPC_DEBUG("received prefix\n prefix = {}", rpc::to_yas_json<std::string>(prefix));

        co_return rpc::error::OK();
    }

    // read a messsage from a peer
    CORO_TASK(int)
    channel_manager::receive_anonymous_payload(envelope_prefix& prefix, envelope_payload& payload, uint64_t sequence_number)
    {
        if (sequence_number == 0)
        {
            int err = CO_AWAIT receive_prefix(prefix);
            if (err != rpc::error::OK())
            {
                RPC_ERROR("failed receive_anonymous_payload receive_prefix");
                co_return err;
            }

            std::vector<char> buf(prefix.payload_size, '\0');
            err = CO_AWAIT read(buf);
            if (err != rpc::error::OK())
            {
                RPC_ERROR("failed receive_anonymous_payload read");
                co_return err;
            }

            auto str_err = rpc::from_yas_binary(rpc::span(buf), payload);
            if (!str_err.empty())
            {
                RPC_ERROR("failed receive_anonymous_payload from_yas_binary");
                co_return rpc::error::TRANSPORT_ERROR();
            }

            RPC_DEBUG("receive_anonymous_payload seq = 0 {}\n payload = {}",
                service_->get_zone_id().get_val(),
                rpc::to_yas_json<std::string>(payload));
        }
        else
        {
            RPC_DEBUG("receive_anonymous_payload seq != 0 {} sequence_number = {}",
                service_->get_zone_id().get_val(),
                prefix.sequence_number);

            result_listener res_payload;
            {
                std::scoped_lock lock(pending_transmits_mtx_);
                auto [it, success] = pending_transmits_.try_emplace(sequence_number, &res_payload);
                if (!success)
                {
                    RPC_ERROR("failed receive_anonymous_payload try_emplace");
                }
            }
            co_await res_payload.event; // now wait for the reply
            prefix = res_payload.prefix;
            payload = res_payload.payload;
        }
        co_return rpc::error::OK();
    }
}
