/*
 *   Copyright (c) 2025 Edward Boggis-Rolfe
 *   All rights reserved.
 */

#include <common/spsc/channel_manager.h>
#include <common/spsc/service_proxy.h>

namespace rpc::spsc
{
    channel_manager::channel_manager(std::chrono::milliseconds timeout,
        std::shared_ptr<rpc::service> service,
        queue_type* send_spsc_queue,
        queue_type* receive_spsc_queue,
        connection_handler handler)
        : send_spsc_queue_(send_spsc_queue)
        , receive_spsc_queue_(receive_spsc_queue)
        , timeout_(timeout)
        , service_(service)
        , connection_handler_(handler)
    {
    }

    std::shared_ptr<channel_manager> channel_manager::create(std::chrono::milliseconds timeout,
        std::shared_ptr<rpc::service> service,
        queue_type* send_spsc_queue,
        queue_type* receive_spsc_queue,
        channel_manager::connection_handler handler)
    {
        auto channel = std::shared_ptr<channel_manager>(
            new channel_manager(timeout, service, send_spsc_queue, receive_spsc_queue, handler));
        channel->keep_alive_ = channel;
        return channel;
    }

    // this method sends queued requests to other peers and receives responses notifying the proxy when complete
    CORO_TASK(void) channel_manager::pump_send_and_receive()
    {
        RPC_DEBUG("pump_send_and_receive {}", service_->get_zone_id().get_val());

        // Message handler lambda that processes incoming messages
        // Stub calls are scheduled to run independently (allowing reentrant calls)
        // Response messages are processed inline to wake up waiting callers
        auto incoming_message_handler = [this](envelope_prefix prefix, envelope_payload payload) -> void
        {
            // do a call
            if (payload.payload_fingerprint == rpc::id<call_send>::get(prefix.version))
            {
                assert(!peer_cancel_received_);
                service_->get_scheduler()->schedule(stub_handle_send(std::move(prefix), std::move(payload)));
            }
            // do a try cast
            else if (payload.payload_fingerprint == rpc::id<try_cast_send>::get(prefix.version))
            {
                assert(!peer_cancel_received_);
                service_->get_scheduler()->schedule(stub_handle_try_cast(std::move(prefix), std::move(payload)));
            }
            // do an add_ref
            else if (payload.payload_fingerprint == rpc::id<addref_send>::get(prefix.version))
            {
                assert(!peer_cancel_received_);
                service_->get_scheduler()->schedule(stub_handle_add_ref(std::move(prefix), std::move(payload)));
            }
            // do a release
            else if (payload.payload_fingerprint == rpc::id<release_send>::get(prefix.version))
            {
                service_->get_scheduler()->schedule(stub_handle_release(std::move(prefix), std::move(payload)));
            }
            // create the service proxy
            else if (payload.payload_fingerprint == rpc::id<init_client_channel_send>::get(prefix.version))
            {
                assert(!peer_cancel_received_);
                service_->get_scheduler()->schedule(create_stub(std::move(prefix), std::move(payload)));
            }
            // handle close connection
            else if (payload.payload_fingerprint == rpc::id<close_connection_send>::get(prefix.version))
            {
                // This is a stub call that needs to send a response
                // The acknowledgment is queued for sending by send_producer_task
                // NOTE: Do NOT capture keep_alive_ here - it would create a reference cycle
                // The channel_manager is kept alive by keep_alive_ until both tasks complete
                auto response_task = [this, seq = prefix.sequence_number]() -> coro::task<void> {
                    RPC_DEBUG("close_connection: sending response for zone {}", service_->get_zone_id().get_val());
                    std::ignore = CO_AWAIT send_payload(
                        rpc::get_version(), message_direction::receive, close_connection_received{}, seq);

                    // Mark that acknowledgment is queued (will be flushed by send_producer)
                    close_ack_queued_ = true;
                    peer_cancel_received_ = true;

                    RPC_DEBUG("close_connection: response queued, close_ack_queued=true peer_cancel_received=true for zone {}",
                              service_->get_zone_id().get_val());
                    CO_RETURN;
                };
                service_->get_scheduler()->schedule(response_task());
            }
            // handle response messages
            else
            {
                // Process responses inline to immediately wake up waiting callers
                result_listener* result = nullptr;
                {
                    std::scoped_lock lock(pending_transmits_mtx_);
                    auto it = pending_transmits_.find(prefix.sequence_number);
                    RPC_DEBUG("pending_transmits_ zone: {} sequence_number: {} id: {}",
                        service_->get_zone_id().get_val(),
                        prefix.sequence_number,
                        payload.payload_fingerprint);
                    assert(it != pending_transmits_.end());
                    result = it->second;
                    pending_transmits_.erase(it);
                }

                result->prefix = std::move(prefix);
                result->payload = std::move(payload);

                RPC_DEBUG("pump_send_and_receive prefix.sequence_number {}\n prefix = {}\n payload = {}",
                    service_->get_zone_id().get_val(),
                    rpc::to_yas_json<std::string>(prefix),
                    rpc::to_yas_json<std::string>(result->payload));

                result->error_code = rpc::error::OK();
                result->event.set();
            }
        };

        // Schedule both producer and consumer tasks independently
        // They will run concurrently via the scheduler, yielding when blocked
        // This function returns immediately - the tasks run in the background until shutdown
        service_->get_scheduler()->schedule(receive_consumer_task(incoming_message_handler));
        service_->get_scheduler()->schedule(send_producer_task());

        RPC_DEBUG("pump_send_and_receive: scheduled tasks for zone {}", service_->get_zone_id().get_val());
        CO_RETURN;
    }

    void channel_manager::attach_service_proxy()
    {
        int new_count = ++service_proxy_ref_count_;
        RPC_DEBUG("attach_service_proxy: zone={} ref_count={}",
                  service_->get_zone_id().get_val(), new_count);
    }

    CORO_TASK(void) channel_manager::detach_service_proxy()
    {
        int new_count = --service_proxy_ref_count_;
        RPC_DEBUG("detach_service_proxy: zone={} ref_count={} peer_cancel_received={}",
                  service_->get_zone_id().get_val(), new_count, peer_cancel_received_.load());

        if (new_count == 0)
        {
            // Last service_proxy detached
            // Only initiate shutdown if peer hasn't already done so
            if (!peer_cancel_received_)
            {
                RPC_DEBUG("Last service_proxy detached, initiating shutdown for zone {}",
                          service_->get_zone_id().get_val());
                CO_AWAIT shutdown();
            }
            else
            {
                RPC_DEBUG("Last service_proxy detached, but peer already initiated shutdown for zone {} - no action needed",
                          service_->get_zone_id().get_val());
            }
        }
        CO_RETURN;
    }

    CORO_TASK(void) channel_manager::shutdown()
    {
        if (cancel_sent_)
        {
            // Already shutting down, just wait for completion
            RPC_DEBUG("shutdown() already in progress for zone {}, waiting for completion",
                      service_->get_zone_id().get_val());
            CO_AWAIT shutdown_event_;
            CO_RETURN;
        }

        RPC_DEBUG("shutdown() initiating for zone {}", service_->get_zone_id().get_val());
        cancel_sent_ = true;

        close_connection_received received{};
        auto err = CO_AWAIT call_peer(rpc::get_version(), close_connection_send{}, received);
        RPC_DEBUG("shutdown() received response for zone {}, err={}",
                  service_->get_zone_id().get_val(), err);

        cancel_confirmed_ = true;
        RPC_DEBUG("shutdown() set cancel_confirmed=true for zone {}",
                  service_->get_zone_id().get_val());

        if (err != rpc::error::OK())
        {
            // Something has gone wrong on the other side so pretend that it has succeeded
            peer_cancel_received_ = true;
        }

        // Wait for both tasks to complete
        CO_AWAIT shutdown_event_;
        RPC_DEBUG("shutdown() completed for zone {}", service_->get_zone_id().get_val());
        CO_RETURN;
    }

    // Receive consumer task - continuously reads from receive_spsc_queue_ and processes messages
    // This runs as an independent coroutine, completely decoupled from send_producer_task
    // Like io_uring: polls shared buffer, remote zone's sender writes independently
    CORO_TASK(void) channel_manager::receive_consumer_task(
        std::function<void(envelope_prefix, envelope_payload)> incoming_message_handler)
    {
        static auto envelope_prefix_saved_size = rpc::yas_binary_saved_size(envelope_prefix());

        std::vector<uint8_t> prefix_buf(envelope_prefix_saved_size);
        std::vector<uint8_t> buf;

        bool receiving_prefix = true;
        std::span<uint8_t> receive_data;

        RPC_DEBUG("receive_consumer_task started for zone {}", service_->get_zone_id().get_val());

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

                // Poll SPSC queue until we have the complete prefix
                bool received_any = false;
                while (!receive_data.empty())
                {
                    message_blob blob;
                    if (!receive_spsc_queue_->pop(blob))
                    {
                        // Queue is empty
                        if (!received_any)
                        {
                            // No progress made, yield to scheduler
                            // Remote sender writing to this buffer will add data
                            CO_AWAIT service_->get_scheduler()->schedule();
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
                    // Successfully received complete prefix
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
                    // Still need more data, continue outer loop
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

                // Poll SPSC queue until we have the complete payload
                bool received_any = false;
                while (!receive_data.empty())
                {
                    message_blob blob;
                    if (!receive_spsc_queue_->pop(blob))
                    {
                        // Queue is empty
                        if (!received_any)
                        {
                            // No progress made, yield to scheduler
                            // Remote sender writing to this buffer will add data
                            CO_AWAIT service_->get_scheduler()->schedule();
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
                    // Successfully received complete payload
                    envelope_payload payload;
                    auto str_err = rpc::from_yas_binary(rpc::span(buf), payload);
                    if (!str_err.empty())
                    {
                        RPC_ERROR("failed bad payload format");
                        break;
                    }

                    // Process the message by calling the handler (non-blocking)
                    // The handler schedules stub handlers (async) and processes responses inline (sync)
                    // This allows reentrant calls to work since stub handlers run independently
                    incoming_message_handler(std::move(prefix), std::move(payload));

                    receiving_prefix = true;
                }
                // else: Still need more data, continue outer loop
            }
        }

        RPC_DEBUG("receive_consumer_task exiting for zone {}", service_->get_zone_id().get_val());

        // Increment task completion counter
        int completed = ++tasks_completed_;
        RPC_DEBUG("receive_consumer_task: tasks_completed={} for zone {}", completed, service_->get_zone_id().get_val());

        if (completed == 2)
        {
            // Both tasks completed, release keep_alive and signal shutdown
            RPC_DEBUG("Both tasks completed, releasing keep_alive and signaling shutdown for zone {}",
                      service_->get_zone_id().get_val());
            keep_alive_ = nullptr;
            shutdown_event_.set();
        }

        CO_RETURN;
    }

    // Send producer task - continuously reads from send_queue_ and writes to send_spsc_queue_
    // This runs as an independent coroutine, completely decoupled from receive_consumer_task
    // Like io_uring: writes to shared buffer, remote zone's receiver polls independently
    CORO_TASK(void) channel_manager::send_producer_task()
    {
        std::span<uint8_t> send_data;

        RPC_DEBUG("send_producer_task started for zone {}", service_->get_zone_id().get_val());

        // Exit when:
        // - We've queued acknowledgment for peer's cancel (close_ack_queued_) OR our shutdown is confirmed (cancel_confirmed_)
        // - AND all queued data has been sent
        while ((!close_ack_queued_ && !cancel_confirmed_) || !send_queue_.empty() || !send_data.empty())
        {
            // RPC_DEBUG("send_producer_task loop: zone={} close_ack_queued={} cancel_confirmed={} send_queue_size={} send_data_size={}",
            //     service_->get_zone_id().get_val(),
            //     close_ack_queued_.load(),
            //     cancel_confirmed_,
            //     send_queue_.size(),
            //     send_data.size());

            // Get next item from send_queue_ if needed
            if (send_data.empty())
            {
                auto scoped_lock = co_await send_queue_mtx_.lock();
                if (send_queue_.empty())
                {
                    // No data to send, yield to allow other coroutines to run
                    scoped_lock.unlock();
                    CO_AWAIT service_->get_scheduler()->schedule();
                    continue;
                }

                auto& item = send_queue_.front();
                send_data = {item.begin(), item.end()};
                scoped_lock.unlock();
            }

            // Chunk the data into message_blob size
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

            // Try to push to SPSC queue - remote zone's receiver will poll this buffer
            if (send_spsc_queue_->push(send_blob))
            {
                // Successfully pushed - remote receiver will discover this independently
                if (send_data.empty())
                {
                    // Finished sending current item, remove it from queue
                    auto scoped_lock = co_await send_queue_mtx_.lock();
                    send_queue_.pop();
                }
            }
            else
            {
                // Queue is full, yield to scheduler
                // Remote receiver consuming from this buffer will free up space
                CO_AWAIT service_->get_scheduler()->schedule();
            }
        }

        CO_AWAIT service_->get_scheduler()->schedule_after(std::chrono::milliseconds(100));

        RPC_DEBUG("send_producer_task completed sending for zone {}", service_->get_zone_id().get_val());

        {
            std::scoped_lock lock(pending_transmits_mtx_);
            for (auto it : pending_transmits_)
            {
                it.second->error_code = rpc::error::CALL_CANCELLED();
                it.second->event.set();
            }
        }

        // Increment task completion counter
        int completed = ++tasks_completed_;
        RPC_DEBUG("send_producer_task: tasks_completed={} for zone {}", completed, service_->get_zone_id().get_val());

        if (completed == 2)
        {
            // Both tasks completed, release keep_alive and signal shutdown
            RPC_DEBUG("Both tasks completed, releasing keep_alive and signaling shutdown for zone {}",
                      service_->get_zone_id().get_val());
            keep_alive_ = nullptr;
            shutdown_event_.set();
        }

        RPC_DEBUG("send_producer_task exiting for zone {}", service_->get_zone_id().get_val());
        CO_RETURN;
    }

    // do a try cast
    CORO_TASK(void) channel_manager::stub_handle_send(envelope_prefix prefix, envelope_payload payload)
    {
        RPC_DEBUG("send request");

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
            call_receive{.payload = std::move(out_buf), .err_code = ret},
            prefix.sequence_number);
        if (err != rpc::error::OK())
        {
            RPC_ERROR("failed send_payload");
            kill_connection();
            CO_RETURN;
        }
        // RPC_DEBUG("send request complete");
        CO_RETURN;
    }

    // do a try cast
    CORO_TASK(void)
    channel_manager::stub_handle_try_cast(envelope_prefix prefix, envelope_payload payload)
    {
        RPC_DEBUG("try_cast request");

        try_cast_send request;
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
            prefix.version, message_direction::receive, try_cast_receive{.err_code = ret}, prefix.sequence_number);
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

        addref_send request;
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
            addref_receive{.ref_count = ref_count, .err_code = ret},
            prefix.sequence_number);
        if (err != rpc::error::OK())
        {
            RPC_ERROR("failed addref_send send_payload");
            kill_connection();
            CO_RETURN;
        }
        // RPC_DEBUG("add_ref request complete");
        CO_RETURN;
    }

    // do a release
    CORO_TASK(void) channel_manager::stub_handle_release(envelope_prefix prefix, envelope_payload payload)
    {
        RPC_DEBUG("release request");
        release_send request;
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
            release_receive{.ref_count = ref_count, .err_code = ret},
            prefix.sequence_number);
        if (err != rpc::error::OK())
        {
            RPC_ERROR("failed release_send send_payload");
            kill_connection();
            CO_RETURN;
        }
        // RPC_DEBUG("release request complete");
        CO_RETURN;
    }

    CORO_TASK(void) channel_manager::create_stub(envelope_prefix prefix, envelope_payload payload)
    {
        RPC_DEBUG("run_client init_client_channel_send zone: {}", service_->get_zone_id().get_val());

        init_client_channel_send request;
        auto err = rpc::from_yas_compressed_binary(rpc::span(payload.payload), request);
        if (!err.empty())
        {
            RPC_ERROR("failed run_client init_client_channel_send");
            CO_RETURN;
        }
        rpc::interface_descriptor input_descr{{request.caller_object_id}, {request.caller_zone_id}};
        rpc::interface_descriptor output_interface;

        int ret = co_await connection_handler_(input_descr, output_interface, service_, keep_alive_);
        connection_handler_ = nullptr; // handover references etc
        if (ret != rpc::error::OK())
        {
            // report error
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
}
