#include <common/spsc/channel_manager.h>
#include <common/spsc/service_proxy.h>

namespace rpc::spsc
{
    channel_manager::channel_manager(std::chrono::milliseconds timeout, rpc::shared_ptr<rpc::service> service,
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
    
        
    std::shared_ptr<channel_manager> channel_manager::create(std::chrono::milliseconds timeout,
                                                             rpc::shared_ptr<rpc::service> service,
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
    bool channel_manager::pump_send_and_receive()
    {
        RPC_DEBUG("pump_send_and_receive {}", service_->get_zone_id().get_val());

        auto foo = [this](envelope_prefix prefix, envelope_payload payload) -> coro::task<int>
        {
            // do a call
            if(payload.payload_fingerprint == rpc::id<call_send>::get(prefix.version))
            {
                assert(!peer_cancel_received_);
                service_->get_scheduler()->schedule(stub_handle_send(std::move(prefix), std::move(payload)));
            }
            // do a try cast
            else if(payload.payload_fingerprint == rpc::id<try_cast_send>::get(prefix.version))
            {
                assert(!peer_cancel_received_);
                service_->get_scheduler()->schedule(stub_handle_try_cast(std::move(prefix), std::move(payload)));
            }
            // do an add_ref
            else if(payload.payload_fingerprint == rpc::id<addref_send>::get(prefix.version))
            {
                assert(!peer_cancel_received_);
                service_->get_scheduler()->schedule(stub_handle_add_ref(std::move(prefix), std::move(payload)));
            }
            // do a release
            else if(payload.payload_fingerprint == rpc::id<release_send>::get(prefix.version))
            {
                service_->get_scheduler()->schedule(stub_handle_release(std::move(prefix), std::move(payload)));
            }
            // create the service proxy
            else if(payload.payload_fingerprint == rpc::id<init_client_channel_send>::get(prefix.version))
            {
                assert(!peer_cancel_received_);
                service_->get_scheduler()->schedule(create_stub(std::move(prefix), std::move(payload)));
            }
            // create the service proxy
            else if(payload.payload_fingerprint == rpc::id<close_connection_send>::get(prefix.version))
            {
                std::ignore = CO_AWAIT send_payload(rpc::get_version(), message_direction::receive, close_connection_received{}, prefix.sequence_number);
                peer_cancel_received_ = true;
            }
            // do a try cast
            else
            {
                // now find the relevant event handler and set its values before triggering it
                result_listener* result = nullptr;
                {
                    std::scoped_lock lock(pending_transmits_mtx_);
                    auto it = pending_transmits_.find(prefix.sequence_number);
                    RPC_DEBUG("pending_transmits_ zone: {} sequence_number: {} id: {}", 
                             service_->get_zone_id().get_val(), prefix.sequence_number, payload.payload_fingerprint);
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
            co_return rpc::error::OK();
        };

        return service_->get_scheduler()->schedule(pump_messages(foo));
    }

    CORO_TASK(void) channel_manager::shutdown()
    {
        cancel_sent_ = true;
        close_connection_received received{};
        auto err = CO_AWAIT call_peer(rpc::get_version(), close_connection_send{}, received);
        cancel_confirmed_ = true;
        if(err != rpc::error::OK())
        {
            // Something has gone wrong on the other side so pretend that it has succeeded
            peer_cancel_received_ = true;    
        }
        co_await shutdown_event_;
    }

    CORO_TASK(void)
    channel_manager::pump_messages(
        std::function<CORO_TASK(int)(envelope_prefix, envelope_payload)> incoming_message_handler)
    {
        static auto envelope_prefix_saved_size = rpc::yas_binary_saved_size(envelope_prefix());

        std::vector<uint8_t> prefix_buf(envelope_prefix_saved_size);
        std::vector<uint8_t> buf;

        auto zone = service_->get_zone_id().get_val();
        std::ignore = zone;

        bool receiving_prefix = true;
        std::span<uint8_t> receive_data;
        std::span<uint8_t> send_data;
        message_blob send_blob;
        bool retry_send_blob = false;
        bool no_pending_send = false;
        bool incoming_queue_empty = false;
        
        while(peer_cancel_received_ == false || cancel_confirmed_ == false || send_queue_.empty() == false || send_data.empty() == false)
        {
            no_pending_send = false;
            if(retry_send_blob)
            {
                if(send_spsc_queue_->push(send_blob))
                {
                    retry_send_blob = false;
                    if(send_data.empty())
                    {
                        send_queue_.pop();
                    }
                }
            }

            if(!retry_send_blob)
            {
                if(send_data.empty())
                {
                    if(send_queue_.empty())
                    {
                        no_pending_send = true;
                    }
                    else
                    {
                        auto& item = send_queue_.front();
                        send_data = {item.begin(), item.end()};
                    }
                }
                if(!send_data.empty())
                {
                    if(send_data.size() < send_blob.size())
                    {
                        std::copy(send_data.begin(), send_data.end(), send_blob.begin());
                        send_data = {send_data.end(), send_data.end()};
                    }
                    else
                    {
                        std::copy_n(send_data.begin(), send_blob.size(), send_blob.begin());
                        send_data = send_data.subspan(send_blob.size(), send_data.size() - send_blob.size());
                    }
                    if(send_spsc_queue_->push(send_blob))
                    {
                        if(send_data.empty())
                        {
                            send_queue_.pop();
                        }
                    }
                    else
                    {
                        retry_send_blob = true;
                    }
                }
            }

            {
                envelope_prefix prefix {};

                if(receiving_prefix)
                {
                    if(receive_data.empty())
                    {
                        receive_data = {prefix_buf.begin(), prefix_buf.end()};
                    }
                    do
                    {
                        message_blob blob;
                        if(!receive_spsc_queue_->pop(blob))
                        {
                            incoming_queue_empty = true;
                            break;
                        }
                        std::copy_n(blob.begin(), std::min(receive_data.size(), blob.size()), receive_data.begin());
                        if(receive_data.size() <= blob.size())
                        {
                            receive_data = {receive_data.end(), receive_data.end()};
                            break;
                        }
                        receive_data = receive_data.subspan(blob.size(), receive_data.size() - blob.size());
                    } while(true);

                    if(!incoming_queue_empty)
                    {
                        auto str_err = rpc::from_yas_binary(rpc::span(prefix_buf), prefix);
                        if(!str_err.empty())
                        {
                            RPC_ERROR("failed invalid prefix");
                            break;
                        }
                        assert(prefix.direction);

                        receiving_prefix = false;
                    }
                }

                if(!incoming_queue_empty)
                {
                    if(receive_data.empty())
                    {
                        buf = std::vector<uint8_t>(prefix.payload_size);
                        receive_data = {buf.begin(), buf.end()};
                    }
                    do
                    {
                        message_blob blob;
                        if(!receive_spsc_queue_->pop(blob))
                        {
                            incoming_queue_empty = true;
                            break;
                        }
                        std::copy_n(blob.begin(), std::min(receive_data.size(), blob.size()), receive_data.begin());
                        if(receive_data.size() <= blob.size())
                        {
                            receive_data = {receive_data.end(), receive_data.end()};
                            break;
                        }
                        receive_data = receive_data.subspan(blob.size(), receive_data.size() - blob.size());
                    } while(true);

                    if(!incoming_queue_empty)
                    {
                        envelope_payload payload;
                        auto str_err = rpc::from_yas_binary(rpc::span(buf), payload);
                        if(!str_err.empty())
                        {
                            RPC_ERROR("failed bad payload format");
                            break;
                        }
                        auto ret = co_await incoming_message_handler(std::move(prefix), std::move(payload));
                        if(ret != rpc::error::OK())
                        {
                            RPC_ERROR("failed incoming_message_handler");
                            break;
                        }
                        receiving_prefix = true;
                    }
                }
            }

            if((retry_send_blob || no_pending_send) && incoming_queue_empty)
                CO_AWAIT service_->get_scheduler()->schedule();
            incoming_queue_empty = false;
        }

        CO_AWAIT service_->get_scheduler()->schedule_after(std::chrono::milliseconds(100));

        {
            std::scoped_lock lock(pending_transmits_mtx_);
            for(auto it : pending_transmits_)
            {
                it.second->error_code = rpc::error::CALL_CANCELLED();
                it.second->event.set();
            }
        }
        shutdown_event_.set();
        keep_alive_ = nullptr;
    }
    // do a try cast
    CORO_TASK(void) channel_manager::stub_handle_send(envelope_prefix prefix, envelope_payload payload)
    {
        RPC_DEBUG("send request");

        assert(prefix.direction == message_direction::send || prefix.direction == message_direction::one_way);
        
        if(cancel_sent_ == true)
        {
            auto err = CO_AWAIT send_payload(prefix.version, message_direction::receive,
                                            call_receive {.payload = {}, .err_code = rpc::error::CALL_CANCELLED()},
                                            prefix.sequence_number);
            if(err != rpc::error::OK())
            {
                RPC_ERROR("failed send_payload");
                kill_connection();
                CO_RETURN;
            }
        }

        call_send request;
        auto str_err = rpc::from_yas_compressed_binary(rpc::span(payload.payload), request);
        if(!str_err.empty())
        {
            RPC_ERROR("failed from_yas_compressed_binary call_send");
            kill_connection();
            CO_RETURN;
        }

        std::vector<char> out_buf;
        auto ret = co_await service_->send(
            prefix.version, request.encoding, request.tag, {request.caller_channel_zone_id}, {request.caller_zone_id},
            {request.destination_zone_id}, {request.object_id}, {request.interface_id}, {request.method_id},
            request.payload.size(), request.payload.data(), out_buf);

        if(ret != rpc::error::OK())
        {
            RPC_ERROR("failed send");
        }

        if(prefix.direction == message_direction::one_way)
            CO_RETURN;

        auto err = CO_AWAIT send_payload(prefix.version, message_direction::receive,
                                         call_receive {.payload = std::move(out_buf), .err_code = ret},
                                         prefix.sequence_number);
        if(err != rpc::error::OK())
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
        if(!str_err.empty())
        {
            RPC_ERROR("failed try_cast_send from_yas_compressed_binary");
            kill_connection();
            CO_RETURN;
        }

        std::vector<char> out_buf;
        auto ret = co_await service_->try_cast(prefix.version, {request.destination_zone_id}, {request.object_id},
                                               {request.interface_id});
        if(ret != rpc::error::OK())
        {
            RPC_ERROR("failed try_cast");
        }

        auto err = CO_AWAIT send_payload(prefix.version, message_direction::receive,
                                         try_cast_receive {.err_code = ret}, prefix.sequence_number);
        if(err != rpc::error::OK())
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
        if(!str_err.empty())
        {
            RPC_ERROR("failed addref_send from_yas_compressed_binary");
            kill_connection();
            CO_RETURN;
        }

        std::vector<char> out_buf;
        uint64_t ref_count = 0;
        auto ret = co_await service_->add_ref(prefix.version, {request.destination_channel_zone_id},
                                              {request.destination_zone_id}, {request.object_id},
                                              {request.caller_channel_zone_id}, {request.caller_zone_id},
                                              {request.known_direction_zone_id},
                                              (rpc::add_ref_options)request.build_out_param_channel,
                                              ref_count);

        if(ret != rpc::error::OK())
        {
            RPC_ERROR("failed add_ref");
        }

        auto err = CO_AWAIT send_payload(prefix.version, message_direction::receive,
                                         addref_receive {.ref_count = ref_count, .err_code = ret},
                                         prefix.sequence_number);
        if(err != rpc::error::OK())
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
        if(!str_err.empty())
        {
            RPC_ERROR("failed release_send from_yas_compressed_binary");
            kill_connection();
            CO_RETURN;
        }

        std::vector<char> out_buf;
        uint64_t ref_count = 0;
        auto ret = co_await service_->release(prefix.version, {request.destination_zone_id}, {request.object_id},
                                              {request.caller_zone_id}, ref_count);

        if(ret != rpc::error::OK())
        {
            RPC_ERROR("failed release");
        }

        auto err = CO_AWAIT send_payload(prefix.version, message_direction::receive,
                                         release_receive {.ref_count = ref_count, .err_code = ret},
                                         prefix.sequence_number);
        if(err != rpc::error::OK())
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
        if(!err.empty())
        {
            RPC_ERROR("failed run_client init_client_channel_send");
            CO_RETURN;
        }
        rpc::interface_descriptor input_descr {{request.caller_object_id}, {request.caller_zone_id}};
        rpc::interface_descriptor output_interface;

        int ret = co_await connection_handler_(input_descr, output_interface, service_, keep_alive_);
        connection_handler_ = nullptr; // handover references etc
        if(ret != rpc::error::OK())
        {
            // report error
            RPC_ERROR("failed to connect to zone {}", ret);
            CO_RETURN;
        }

        err = CO_AWAIT send_payload(
            prefix.version, message_direction::receive,
            init_client_channel_response {rpc::error::OK(), output_interface.destination_zone_id.get_val(),
                                                    output_interface.object_id.get_val(), 0},
            prefix.sequence_number);
        std::ignore = err;

        CO_RETURN;
    }
}