/*
 *   Copyright (c) 2025 Edward Boggis-Rolfe
 *   All rights reserved.
 */

#include <rpc/service_proxies/spsc/transport.h>

namespace rpc::spsc
{
    spsc_transport::spsc_transport(std::string name,
        std::shared_ptr<rpc::service> service,
        rpc::zone adjacent_zone_id,
        std::chrono::milliseconds timeout,
        queue_type* send_spsc_queue,
        queue_type* receive_spsc_queue,
        connection_handler handler)
        : rpc::transport(name, service, adjacent_zone_id)
        , send_spsc_queue_(send_spsc_queue)
        , receive_spsc_queue_(receive_spsc_queue)
        , timeout_(timeout)
        , connection_handler_(handler)
    {
        // SPSC transport starts in CONNECTING state
        // Will transition to CONNECTED after successful connection
    }

    std::shared_ptr<spsc_transport> spsc_transport::create(std::string name,
        std::shared_ptr<rpc::service> service,
        rpc::zone adjacent_zone_id,
        std::chrono::milliseconds timeout,
        queue_type* send_spsc_queue,
        queue_type* receive_spsc_queue,
        connection_handler handler)
    {
        auto transport = std::shared_ptr<spsc_transport>(
            new spsc_transport(name, service, adjacent_zone_id, timeout, send_spsc_queue, receive_spsc_queue, handler));
        transport->keep_alive_ = transport;
        return transport;
    }

    // Connection handshake
    CORO_TASK(int)
    spsc_transport::connect(rpc::interface_descriptor input_descr, rpc::interface_descriptor& output_descr)
    {
        RPC_DEBUG("spsc_transport::connect zone={}", get_zone_id().get_val());

        auto service = get_service();
        assert(connection_handler_ || !connection_handler_); // Can be null for client side

        // Schedule onto the scheduler
        CO_AWAIT service->get_scheduler()->schedule();

        // If this is a client-side connect, send init message to server
        if (!connection_handler_)
        {
            // Client side: register the proxy connection
            init_client_channel_response init_receive;
            int ret = CO_AWAIT call_peer(rpc::get_version(),
                init_client_channel_send{.caller_zone_id = get_zone_id().get_val(),
                    .caller_object_id = input_descr.object_id.get_val(),
                    .destination_zone_id = get_adjacent_zone_id().get_val()},
                init_receive);
            if (ret != rpc::error::OK())
            {
                RPC_ERROR("spsc_transport::connect call_peer failed");
                CO_RETURN ret;
            }

            if (init_receive.err_code != rpc::error::OK())
            {
                RPC_ERROR("init_client_channel_send failed");
                CO_RETURN init_receive.err_code;
            }

            rpc::object output_object_id = {init_receive.destination_object_id};
            output_descr = {output_object_id, get_adjacent_zone_id().as_destination()};
        }

        // Set transport to CONNECTED after successful handshake
        set_status(rpc::transport_status::CONNECTED);

        CO_RETURN rpc::error::OK();
    }

    // Producer/consumer tasks for message pumping
    CORO_TASK(void) spsc_transport::pump_send_and_receive()
    {
        RPC_DEBUG("pump_send_and_receive zone={}", get_service()->get_zone_id().get_val());

        // Message handler lambda that processes incoming messages
        auto incoming_message_handler = [this](envelope_prefix prefix, envelope_payload payload) -> void
        {
            // Handle different message types
            if (payload.payload_fingerprint == rpc::id<call_send>::get(prefix.version))
            {
                assert(!peer_cancel_received_);
                get_service()->get_scheduler()->schedule(stub_handle_send(std::move(prefix), std::move(payload)));
            }
            else if (payload.payload_fingerprint == rpc::id<try_cast_send>::get(prefix.version))
            {
                assert(!peer_cancel_received_);
                get_service()->get_scheduler()->schedule(stub_handle_try_cast(std::move(prefix), std::move(payload)));
            }
            else if (payload.payload_fingerprint == rpc::id<addref_send>::get(prefix.version))
            {
                assert(!peer_cancel_received_);
                get_service()->get_scheduler()->schedule(stub_handle_add_ref(std::move(prefix), std::move(payload)));
            }
            else if (payload.payload_fingerprint == rpc::id<release_send>::get(prefix.version))
            {
                get_service()->get_scheduler()->schedule(stub_handle_release(std::move(prefix), std::move(payload)));
            }
            else if (payload.payload_fingerprint == rpc::id<init_client_channel_send>::get(prefix.version))
            {
                assert(!peer_cancel_received_);
                get_service()->get_scheduler()->schedule(create_stub(std::move(prefix), std::move(payload)));
            }
            else if (payload.payload_fingerprint == rpc::id<post_send>::get(prefix.version))
            {
                assert(!peer_cancel_received_);
                get_service()->get_scheduler()->schedule(stub_handle_post(std::move(prefix), std::move(payload)));
            }
            else if (payload.payload_fingerprint == rpc::id<close_connection_send>::get(prefix.version))
            {
                // Handle close connection request
                auto response_task = [this, seq = prefix.sequence_number]() -> coro::task<void>
                {
                    RPC_DEBUG("close_connection: sending response for zone {}", get_service()->get_zone_id().get_val());
                    std::ignore = CO_AWAIT send_payload(
                        rpc::get_version(), message_direction::receive, close_connection_received{}, seq);

                    close_ack_queued_ = true;
                    peer_cancel_received_ = true;

                    RPC_DEBUG("close_connection: response queued for zone {}", get_service()->get_zone_id().get_val());
                    CO_RETURN;
                };
                get_service()->get_scheduler()->schedule(response_task());
            }
            else
            {
                // Handle response messages
                result_listener* result = nullptr;
                {
                    std::scoped_lock lock(pending_transmits_mtx_);
                    auto it = pending_transmits_.find(prefix.sequence_number);
                    RPC_DEBUG("pending_transmits_ zone: {} sequence_number: {} id: {}",
                        get_service()->get_zone_id().get_val(),
                        prefix.sequence_number,
                        payload.payload_fingerprint);
                    assert(it != pending_transmits_.end());
                    result = it->second;
                    pending_transmits_.erase(it);
                }

                result->prefix = std::move(prefix);
                result->payload = std::move(payload);

                RPC_DEBUG("pump_send_and_receive prefix.sequence_number {}\n prefix = {}\n payload = {}",
                    get_service()->get_zone_id().get_val(),
                    rpc::to_yas_json<std::string>(prefix),
                    rpc::to_yas_json<std::string>(result->payload));

                result->error_code = rpc::error::OK();
                result->event.set();
            }
        };

        // Schedule both producer and consumer tasks
        get_service()->get_scheduler()->schedule(receive_consumer_task(incoming_message_handler));
        get_service()->get_scheduler()->schedule(send_producer_task());

        RPC_DEBUG("pump_send_and_receive: scheduled tasks for zone {}", get_service()->get_zone_id().get_val());
        CO_RETURN;
    }

    void spsc_transport::attach_service_proxy()
    {
        int new_count = ++service_proxy_ref_count_;
        RPC_DEBUG("attach_service_proxy: zone={} ref_count={}", get_service()->get_zone_id().get_val(), new_count);
    }

    CORO_TASK(void) spsc_transport::detach_service_proxy()
    {
        int new_count = --service_proxy_ref_count_;
        RPC_DEBUG("detach_service_proxy: zone={} ref_count={} peer_cancel_received={}",
            get_service()->get_zone_id().get_val(),
            new_count,
            peer_cancel_received_.load());

        if (new_count == 0)
        {
            // Last service_proxy detached
            if (!peer_cancel_received_)
            {
                RPC_DEBUG("Last service_proxy detached, initiating shutdown for zone {}",
                    get_service()->get_zone_id().get_val());
                CO_AWAIT shutdown();
            }
            else
            {
                RPC_DEBUG("Last service_proxy detached, but peer already initiated shutdown for zone {}",
                    get_service()->get_zone_id().get_val());
            }
        }
        CO_RETURN;
    }

    CORO_TASK(void) spsc_transport::shutdown()
    {
        if (cancel_sent_)
        {
            // Already shutting down
            RPC_DEBUG("shutdown() already in progress for zone {}", get_service()->get_zone_id().get_val());
            CO_AWAIT shutdown_event_;
            CO_RETURN;
        }

        RPC_DEBUG("shutdown() initiating for zone {}", get_service()->get_zone_id().get_val());
        cancel_sent_ = true;

        // Set transport status to DISCONNECTED
        set_status(rpc::transport_status::DISCONNECTED);

        close_connection_received received{};
        auto err = CO_AWAIT call_peer(rpc::get_version(), close_connection_send{}, received);
        RPC_DEBUG("shutdown() received response for zone {}, err={}", get_service()->get_zone_id().get_val(), err);

        cancel_confirmed_ = true;

        if (err != rpc::error::OK())
        {
            peer_cancel_received_ = true;
        }

        // Wait for both tasks to complete
        CO_AWAIT shutdown_event_;
        RPC_DEBUG("shutdown() completed for zone {}", get_service()->get_zone_id().get_val());
        CO_RETURN;
    }

    // Receive consumer task
    CORO_TASK(void)
    spsc_transport::receive_consumer_task(std::function<void(envelope_prefix, envelope_payload)> incoming_message_handler)
    {
        static auto envelope_prefix_saved_size = rpc::yas_binary_saved_size(envelope_prefix());

        std::vector<uint8_t> prefix_buf(envelope_prefix_saved_size);
        std::vector<uint8_t> buf;

        bool receiving_prefix = true;
        std::span<uint8_t> receive_data;

        RPC_DEBUG("receive_consumer_task started for zone {}", get_service()->get_zone_id().get_val());

        while (!peer_cancel_received_ && !cancel_confirmed_)
        {
            envelope_prefix prefix{};

            // Receive prefix chunks
            if (receiving_prefix)
            {
                if (receive_data.empty())
                {
                    receive_data = {prefix_buf.begin(), prefix_buf.end()};
                }

                bool received_any = false;
                while (!receive_data.empty())
                {
                    message_blob blob;
                    if (!receive_spsc_queue_->pop(blob))
                    {
                        if (!received_any)
                        {
                            CO_AWAIT get_service()->get_scheduler()->schedule();
                        }
                        break;
                    }

                    received_any = true;
                    size_t copy_size = std::min(receive_data.size(), blob.size());
                    std::copy_n(blob.begin(), copy_size, receive_data.begin());

                    if (receive_data.size() <= blob.size())
                    {
                        receive_data = {receive_data.end(), receive_data.end()};
                    }
                    else
                    {
                        receive_data = receive_data.subspan(blob.size(), receive_data.size() - blob.size());
                    }
                }

                if (receive_data.empty())
                {
                    auto str_err = rpc::from_yas_binary(rpc::span(prefix_buf), prefix);
                    if (!str_err.empty())
                    {
                        RPC_ERROR("failed invalid prefix");
                        break;
                    }
                    assert(prefix.direction);
                    receiving_prefix = false;
                }
                else
                {
                    continue;
                }
            }

            // Receive payload chunks
            if (!receiving_prefix)
            {
                if (receive_data.empty())
                {
                    buf = std::vector<uint8_t>(prefix.payload_size);
                    receive_data = {buf.begin(), buf.end()};
                }

                bool received_any = false;
                while (!receive_data.empty())
                {
                    message_blob blob;
                    if (!receive_spsc_queue_->pop(blob))
                    {
                        if (!received_any)
                        {
                            CO_AWAIT get_service()->get_scheduler()->schedule();
                        }
                        break;
                    }

                    received_any = true;
                    size_t copy_size = std::min(receive_data.size(), blob.size());
                    std::copy_n(blob.begin(), copy_size, receive_data.begin());

                    if (receive_data.size() <= blob.size())
                    {
                        receive_data = {receive_data.end(), receive_data.end()};
                    }
                    else
                    {
                        receive_data = receive_data.subspan(blob.size(), receive_data.size() - blob.size());
                    }
                }

                if (receive_data.empty())
                {
                    envelope_payload payload;
                    auto str_err = rpc::from_yas_binary(rpc::span(buf), payload);
                    if (!str_err.empty())
                    {
                        RPC_ERROR("failed bad payload format");
                        break;
                    }

                    incoming_message_handler(std::move(prefix), std::move(payload));
                    receiving_prefix = true;
                }
            }
        }

        RPC_DEBUG("receive_consumer_task exiting for zone {}", get_service()->get_zone_id().get_val());

        int completed = ++tasks_completed_;
        RPC_DEBUG("receive_consumer_task: tasks_completed={} for zone {}", completed, get_service()->get_zone_id().get_val());

        if (completed == 2)
        {
            RPC_DEBUG("Both tasks completed, releasing keep_alive for zone {}", get_service()->get_zone_id().get_val());
            keep_alive_ = nullptr;
            shutdown_event_.set();
        }

        CO_RETURN;
    }

    // Send producer task
    CORO_TASK(void) spsc_transport::send_producer_task()
    {
        std::span<uint8_t> send_data;

        RPC_DEBUG("send_producer_task started for zone {}", get_service()->get_zone_id().get_val());

        while ((!close_ack_queued_ && !cancel_confirmed_) || !send_queue_.empty() || !send_data.empty())
        {
            if (send_data.empty())
            {
                auto scoped_lock = CO_AWAIT send_queue_mtx_.lock();
                if (send_queue_.empty())
                {
                    scoped_lock.unlock();
                    CO_AWAIT get_service()->get_scheduler()->schedule();
                    continue;
                }

                auto& item = send_queue_.front();
                send_data = {item.begin(), item.end()};
                scoped_lock.unlock();
            }

            message_blob send_blob;
            if (send_data.size() < send_blob.size())
            {
                std::copy(send_data.begin(), send_data.end(), send_blob.begin());
                send_data = {send_data.end(), send_data.end()};
            }
            else
            {
                std::copy_n(send_data.begin(), send_blob.size(), send_blob.begin());
                send_data = send_data.subspan(send_blob.size(), send_data.size() - send_blob.size());
            }

            if (send_spsc_queue_->push(send_blob))
            {
                if (send_data.empty())
                {
                    auto scoped_lock = CO_AWAIT send_queue_mtx_.lock();
                    send_queue_.pop();
                }
            }
            else
            {
                CO_AWAIT get_service()->get_scheduler()->schedule();
            }
        }

        CO_AWAIT get_service()->get_scheduler()->schedule_after(std::chrono::milliseconds(100));

        RPC_DEBUG("send_producer_task completed sending for zone {}", get_service()->get_zone_id().get_val());

        {
            std::scoped_lock lock(pending_transmits_mtx_);
            for (auto it : pending_transmits_)
            {
                it.second->error_code = rpc::error::CALL_CANCELLED();
                it.second->event.set();
            }
        }

        int completed = ++tasks_completed_;
        RPC_DEBUG("send_producer_task: tasks_completed={} for zone {}", completed, get_service()->get_zone_id().get_val());

        if (completed == 2)
        {
            RPC_DEBUG("Both tasks completed, releasing keep_alive for zone {}", get_service()->get_zone_id().get_val());
            keep_alive_ = nullptr;
            shutdown_event_.set();
        }

        RPC_DEBUG("send_producer_task exiting for zone {}", get_service()->get_zone_id().get_val());
        CO_RETURN;
    }

    // Stub handlers (server-side message processing)
    CORO_TASK(void) spsc_transport::stub_handle_send(envelope_prefix prefix, envelope_payload payload)
    {
        RPC_DEBUG("stub_handle_send");

        assert(prefix.direction == message_direction::send || prefix.direction == message_direction::one_way);

        if (cancel_sent_ == true)
        {
            auto err = CO_AWAIT send_payload(prefix.version,
                message_direction::receive,
                call_receive{.payload = {}, .err_code = rpc::error::CALL_CANCELLED()},
                prefix.sequence_number);
            if (err != rpc::error::OK())
            {
                RPC_ERROR("failed send_payload");
                kill_connection();
                CO_RETURN;
            }
        }

        call_send request;
        auto str_err = rpc::from_yas_compressed_binary(rpc::span(payload.payload), request);
        if (!str_err.empty())
        {
            RPC_ERROR("failed from_yas_compressed_binary call_send");
            kill_connection();
            CO_RETURN;
        }

        std::vector<char> out_buf;
        std::vector<rpc::back_channel_entry> in_back_channel;
        std::vector<rpc::back_channel_entry> out_back_channel;
        auto ret = CO_AWAIT get_service()->send(prefix.version,
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
            out_buf,
            in_back_channel,
            out_back_channel);

        if (ret != rpc::error::OK())
        {
            RPC_ERROR("failed send");
        }

        if (prefix.direction == message_direction::one_way)
            CO_RETURN;

        auto err = CO_AWAIT send_payload(prefix.version,
            message_direction::receive,
            call_receive{.payload = std::move(out_buf), .err_code = ret},
            prefix.sequence_number);
        if (err != rpc::error::OK())
        {
            RPC_ERROR("failed send_payload");
            kill_connection();
            CO_RETURN;
        }
        CO_RETURN;
    }

    CORO_TASK(void) spsc_transport::stub_handle_post(envelope_prefix prefix, envelope_payload payload)
    {
        RPC_DEBUG("stub_handle_post");

        post_send request;
        auto str_err = rpc::from_yas_compressed_binary(rpc::span(payload.payload), request);
        if (!str_err.empty())
        {
            RPC_ERROR("failed post_send from_yas_compressed_binary");
            kill_connection();
            CO_RETURN;
        }

        std::vector<rpc::back_channel_entry> in_back_channel;
        CO_AWAIT get_service()->post(prefix.version,
            request.encoding,
            request.tag,
            {request.caller_channel_zone_id},
            {request.caller_zone_id},
            {request.destination_zone_id},
            {request.object_id},
            {request.interface_id},
            {request.method_id},
            static_cast<rpc::post_options>(request.options),
            request.payload.size(),
            request.payload.data(),
            in_back_channel);

        RPC_DEBUG("stub_handle_post complete");
        CO_RETURN;
    }

    CORO_TASK(void) spsc_transport::stub_handle_try_cast(envelope_prefix prefix, envelope_payload payload)
    {
        RPC_DEBUG("stub_handle_try_cast");

        try_cast_send request;
        auto str_err = rpc::from_yas_compressed_binary(rpc::span(payload.payload), request);
        if (!str_err.empty())
        {
            RPC_ERROR("failed try_cast_send from_yas_compressed_binary");
            kill_connection();
            CO_RETURN;
        }

        std::vector<rpc::back_channel_entry> in_back_channel;
        std::vector<rpc::back_channel_entry> out_back_channel;
        auto ret = CO_AWAIT get_service()->try_cast(prefix.version,
            {request.destination_zone_id},
            {request.object_id},
            {request.interface_id},
            in_back_channel,
            out_back_channel);
        if (ret != rpc::error::OK())
        {
            RPC_ERROR("failed try_cast");
        }

        auto err = CO_AWAIT send_payload(
            prefix.version, message_direction::receive, try_cast_receive{.err_code = ret}, prefix.sequence_number);
        if (err != rpc::error::OK())
        {
            RPC_ERROR("failed try_cast_send send_payload");
            kill_connection();
            CO_RETURN;
        }
        RPC_DEBUG("stub_handle_try_cast complete");
        CO_RETURN;
    }

    CORO_TASK(void) spsc_transport::stub_handle_add_ref(envelope_prefix prefix, envelope_payload payload)
    {
        RPC_DEBUG("stub_handle_add_ref");

        addref_send request;
        auto str_err = rpc::from_yas_compressed_binary(rpc::span(payload.payload), request);
        if (!str_err.empty())
        {
            RPC_ERROR("failed addref_send from_yas_compressed_binary");
            kill_connection();
            CO_RETURN;
        }

        uint64_t ref_count = 0;
        std::vector<rpc::back_channel_entry> in_back_channel;
        std::vector<rpc::back_channel_entry> out_back_channel;
        auto ret = CO_AWAIT get_service()->add_ref(prefix.version,
            {request.destination_zone_id},
            {request.object_id},
            {request.caller_channel_zone_id},
            {request.caller_zone_id},
            {request.known_direction_zone_id},
            (rpc::add_ref_options)request.build_out_param_channel,
            ref_count,
            in_back_channel,
            out_back_channel);

        if (ret != rpc::error::OK())
        {
            RPC_ERROR("failed add_ref");
        }

        auto err = CO_AWAIT send_payload(prefix.version,
            message_direction::receive,
            addref_receive{.ref_count = ref_count, .err_code = ret},
            prefix.sequence_number);
        if (err != rpc::error::OK())
        {
            RPC_ERROR("failed addref_send send_payload");
            kill_connection();
            CO_RETURN;
        }
        CO_RETURN;
    }

    CORO_TASK(void) spsc_transport::stub_handle_release(envelope_prefix prefix, envelope_payload payload)
    {
        RPC_DEBUG("stub_handle_release");

        release_send request;
        auto str_err = rpc::from_yas_compressed_binary(rpc::span(payload.payload), request);
        if (!str_err.empty())
        {
            RPC_ERROR("failed release_send from_yas_compressed_binary");
            kill_connection();
            CO_RETURN;
        }

        uint64_t ref_count = 0;
        std::vector<rpc::back_channel_entry> in_back_channel;
        std::vector<rpc::back_channel_entry> out_back_channel;
        auto ret = CO_AWAIT get_service()->release(prefix.version,
            {request.destination_zone_id},
            {request.object_id},
            {request.caller_zone_id},
            request.options,
            ref_count,
            in_back_channel,
            out_back_channel);

        if (ret != rpc::error::OK())
        {
            RPC_ERROR("failed release");
        }

        auto err = CO_AWAIT send_payload(prefix.version,
            message_direction::receive,
            release_receive{.ref_count = ref_count, .err_code = ret},
            prefix.sequence_number);
        if (err != rpc::error::OK())
        {
            RPC_ERROR("failed release_send send_payload");
            kill_connection();
            CO_RETURN;
        }
        CO_RETURN;
    }

    CORO_TASK(void) spsc_transport::create_stub(envelope_prefix prefix, envelope_payload payload)
    {
        RPC_DEBUG("create_stub zone: {}", get_service()->get_zone_id().get_val());

        init_client_channel_send request;
        auto err = rpc::from_yas_compressed_binary(rpc::span(payload.payload), request);
        if (!err.empty())
        {
            RPC_ERROR("failed create_stub init_client_channel_send");
            CO_RETURN;
        }
        rpc::interface_descriptor input_descr{{request.caller_object_id}, {request.caller_zone_id}};
        rpc::interface_descriptor output_interface;

        int ret = CO_AWAIT connection_handler_(input_descr, output_interface, get_service(), keep_alive_);
        connection_handler_ = nullptr;
        if (ret != rpc::error::OK())
        {
            RPC_ERROR("failed to connect to zone {}", ret);
            CO_RETURN;
        }

        err = CO_AWAIT send_payload(prefix.version,
            message_direction::receive,
            init_client_channel_response{
                rpc::error::OK(), output_interface.destination_zone_id.get_val(), output_interface.object_id.get_val(), 0},
            prefix.sequence_number);
        std::ignore = err;

        CO_RETURN;
    }

    // Client-side i_marshaller implementations
    CORO_TASK(int)
    spsc_transport::send(uint64_t protocol_version,
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
        std::vector<rpc::back_channel_entry>& out_back_channel)
    {
        RPC_DEBUG("spsc_transport::send zone={}", get_zone_id().get_val());

        // Check transport status
        if (get_status() != rpc::transport_status::CONNECTED)
        {
            RPC_ERROR("spsc_transport::send: transport not connected");
            CO_RETURN rpc::error::TRANSPORT_ERROR();
        }

        if (destination_zone_id != get_adjacent_zone_id().as_destination())
        {
            // Route through base transport's inbound_send for destination routing
            CO_RETURN CO_AWAIT inbound_send(protocol_version,
                encoding,
                tag,
                caller_zone_id,
                destination_zone_id,
                object_id,
                interface_id,
                method_id,
                in_size_,
                in_buf_,
                out_buf_,
                in_back_channel,
                out_back_channel);
        }

        call_receive call_receive;
        int ret = CO_AWAIT call_peer(protocol_version,
            call_send{.encoding = encoding,
                .tag = tag,
                .caller_channel_zone_id = caller_channel_zone_id.get_val(),
                .caller_zone_id = caller_zone_id.get_val(),
                .destination_zone_id = destination_zone_id.get_val(),
                .object_id = object_id.get_val(),
                .interface_id = interface_id.get_val(),
                .method_id = method_id.get_val(),
                .payload = std::vector<char>(in_buf_, in_buf_ + in_size_),
                .back_channel = in_back_channel},
            call_receive);
        if (ret != rpc::error::OK())
        {
            RPC_ERROR("failed spsc_transport::send call_send");
            CO_RETURN ret;
        }

        out_buf_.swap(call_receive.payload);
        out_back_channel.swap(call_receive.back_channel);

        RPC_DEBUG("spsc_transport::send complete zone={}", get_zone_id().get_val());

        CO_RETURN call_receive.err_code;
    }

    CORO_TASK(void)
    spsc_transport::post(uint64_t protocol_version,
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
        const std::vector<rpc::back_channel_entry>& in_back_channel)
    {
        RPC_DEBUG("spsc_transport::post zone={}", get_zone_id().get_val());

        // Check transport status
        if (get_status() != rpc::transport_status::CONNECTED)
        {
            RPC_ERROR("spsc_transport::post: transport not connected");
            CO_RETURN;
        }

        if (destination_zone_id != get_adjacent_zone_id().as_destination())
        {
            // Route through base transport's inbound_post for destination routing
            CO_AWAIT inbound_post(protocol_version,
                encoding,
                tag,
                caller_zone_id,
                destination_zone_id,
                object_id,
                interface_id,
                method_id,
                options,
                in_size_,
                in_buf_,
                in_back_channel);
            CO_RETURN;
        }

        // Fire-and-forget
        int ret = CO_AWAIT send_payload(protocol_version,
            spsc::message_direction::one_way,
            spsc::post_send{.encoding = encoding,
                .tag = tag,
                .caller_channel_zone_id = caller_channel_zone_id.get_val(),
                .caller_zone_id = caller_zone_id.get_val(),
                .destination_zone_id = destination_zone_id.get_val(),
                .object_id = object_id.get_val(),
                .interface_id = interface_id.get_val(),
                .method_id = method_id.get_val(),
                .options = options,
                .payload = std::vector<char>(in_buf_, in_buf_ + in_size_),
                .back_channel = in_back_channel},
            0); // sequence number 0 for one-way

        if (ret != rpc::error::OK())
        {
            RPC_ERROR("failed spsc_transport::post send_payload");
        }

        CO_RETURN;
    }

    CORO_TASK(int)
    spsc_transport::try_cast(uint64_t protocol_version,
        rpc::destination_zone destination_zone_id,
        rpc::object object_id,
        rpc::interface_ordinal interface_id,
        const std::vector<rpc::back_channel_entry>& in_back_channel,
        std::vector<rpc::back_channel_entry>& out_back_channel)
    {
        RPC_DEBUG("spsc_transport::try_cast zone={}", get_zone_id().get_val());

        // Check transport status
        if (get_status() != rpc::transport_status::CONNECTED)
        {
            RPC_ERROR("spsc_transport::try_cast: transport not connected");
            CO_RETURN rpc::error::TRANSPORT_ERROR();
        }

        if (destination_zone_id != get_adjacent_zone_id().as_destination())
        {
            // Route through base transport for destination routing
            CO_RETURN CO_AWAIT inbound_try_cast(protocol_version, destination_zone_id, object_id, interface_id, in_back_channel, out_back_channel);
        }

        try_cast_receive response_data;
        int ret = CO_AWAIT call_peer(protocol_version,
            try_cast_send{.destination_zone_id = destination_zone_id.get_val(),
                .object_id = object_id.get_val(),
                .interface_id = interface_id.get_val(),
                .back_channel = in_back_channel},
            response_data);
        if (ret != rpc::error::OK())
        {
            RPC_ERROR("failed try_cast call_peer");
            CO_RETURN ret;
        }

        RPC_DEBUG("spsc_transport::try_cast complete zone={}", get_zone_id().get_val());

        out_back_channel.swap(response_data.back_channel);
        CO_RETURN response_data.err_code;
    }

    CORO_TASK(int)
    spsc_transport::add_ref(uint64_t protocol_version,
        rpc::destination_zone destination_zone_id,
        rpc::object object_id,
        rpc::caller_channel_zone caller_channel_zone_id,
        rpc::caller_zone caller_zone_id,
        rpc::known_direction_zone known_direction_zone_id,
        rpc::add_ref_options build_out_param_channel,
        uint64_t& reference_count,
        const std::vector<rpc::back_channel_entry>& in_back_channel,
        std::vector<rpc::back_channel_entry>& out_back_channel)
    {
        RPC_DEBUG("spsc_transport::add_ref zone={}", get_zone_id().get_val());

        // Check transport status
        if (get_status() != rpc::transport_status::CONNECTED)
        {
            RPC_ERROR("spsc_transport::add_ref: transport not connected");
            CO_RETURN rpc::error::TRANSPORT_ERROR();
        }

#ifdef USE_RPC_TELEMETRY
        if (auto telemetry_service = rpc::get_telemetry_service(); telemetry_service)
        {
            telemetry_service->on_service_proxy_add_ref(
                get_zone_id(), destination_zone_id, caller_zone_id, object_id, build_out_param_channel);
        }
#endif

        if (destination_zone_id != get_adjacent_zone_id().as_destination())
        {
            // Route through base transport for destination routing
            CO_RETURN CO_AWAIT inbound_add_ref(protocol_version,
                destination_zone_id,
                object_id,
                caller_zone_id,
                known_direction_zone_id,
                build_out_param_channel,
                reference_count,
                in_back_channel,
                out_back_channel);
        }

        addref_receive response_data;
        int ret = CO_AWAIT call_peer(protocol_version,
            addref_send{.destination_zone_id = destination_zone_id.get_val(),
                .object_id = object_id.get_val(),
                .caller_channel_zone_id = caller_channel_zone_id.get_val(),
                .caller_zone_id = caller_zone_id.get_val(),
                .known_direction_zone_id = known_direction_zone_id.get_val(),
                .build_out_param_channel = build_out_param_channel,
                .back_channel = in_back_channel},
            response_data);
        if (ret != rpc::error::OK())
        {
            RPC_ERROR("failed add_ref addref_send");
            CO_RETURN ret;
        }

        reference_count = response_data.ref_count;
        out_back_channel.swap(response_data.back_channel);
        if (response_data.err_code != rpc::error::OK())
        {
            RPC_ERROR("failed addref_receive.err_code failed");
#ifdef USE_RPC_TELEMETRY
            if (auto telemetry_service = rpc::get_telemetry_service(); telemetry_service)
            {
                auto error_message = std::string("add_ref failed ") + std::to_string(response_data.err_code);
                telemetry_service->message(rpc::i_telemetry_service::err, error_message.c_str());
            }
#endif
            RPC_ASSERT(false);
            CO_RETURN response_data.err_code;
        }
        RPC_DEBUG("spsc_transport::add_ref complete zone={}", get_zone_id().get_val());

        CO_RETURN rpc::error::OK();
    }

    CORO_TASK(int)
    spsc_transport::release(uint64_t protocol_version,
        rpc::destination_zone destination_zone_id,
        rpc::object object_id,
        rpc::caller_zone caller_zone_id,
        rpc::release_options options,
        uint64_t& reference_count,
        const std::vector<rpc::back_channel_entry>& in_back_channel,
        std::vector<rpc::back_channel_entry>& out_back_channel)
    {
        RPC_DEBUG("spsc_transport::release zone={}", get_zone_id().get_val());

        // Check transport status
        if (get_status() != rpc::transport_status::CONNECTED)
        {
            RPC_ERROR("spsc_transport::release: transport not connected");
            CO_RETURN rpc::error::TRANSPORT_ERROR();
        }

        if (destination_zone_id != get_adjacent_zone_id().as_destination())
        {
            // Route through base transport for destination routing
            CO_RETURN CO_AWAIT inbound_release(
                protocol_version, destination_zone_id, object_id, caller_zone_id, options, reference_count, in_back_channel, out_back_channel);
        }

        release_receive response_data;
        int ret = CO_AWAIT call_peer(protocol_version,
            release_send{.destination_zone_id = destination_zone_id.get_val(),
                .object_id = object_id.get_val(),
                .caller_zone_id = caller_zone_id.get_val(),
                .options = options,
                .back_channel = in_back_channel},
            response_data);
        if (ret != rpc::error::OK())
        {
            RPC_ERROR("failed release release_send");
            CO_RETURN ret;
        }

        if (response_data.err_code != rpc::error::OK())
        {
            RPC_ERROR("failed response_data.err_code failed");
#ifdef USE_RPC_TELEMETRY
            if (auto telemetry_service = rpc::get_telemetry_service(); telemetry_service)
            {
                auto error_message = std::string("release failed ") + std::to_string(response_data.err_code);
                telemetry_service->message(rpc::i_telemetry_service::err, error_message.c_str());
            }
#endif
            RPC_ASSERT(false);
            CO_RETURN response_data.err_code;
        }

        RPC_DEBUG("spsc_transport::release complete zone={}", get_zone_id().get_val());

        reference_count = response_data.ref_count;
        out_back_channel.swap(response_data.back_channel);
        CO_RETURN rpc::error::OK();
    }
}
