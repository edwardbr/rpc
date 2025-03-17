#include <common/spsc/channel_manager.h>
#include <common/spsc/service_proxy.h>
#include <tcp/tcp.h>

namespace rpc::spsc
{
    std::shared_ptr<channel_manager> channel_manager::create(std::chrono::milliseconds timeout, rpc::shared_ptr<rpc::service> service,
                queue_type* send_spsc_queue,
                queue_type* receive_spsc_queue,
                channel_manager::connection_handler handler)
    {
        auto channel = std::shared_ptr<channel_manager>(new channel_manager(timeout, service, send_spsc_queue, receive_spsc_queue, handler));
        channel->keep_alive_ = channel;
        return channel;
    }
                
    // this method sends queued requests to other peers and receives responses notifying the proxy when complete
    bool channel_manager::pump_send_and_receive()
    {
        // std::string msg("pump_send_and_receive ");
        // msg += std::to_string(service_->get_zone_id().get_val());
        // msg += " fd = ";
        // msg += std::to_string(client_.socket().native_handle());
        // LOG_CSTR(msg.c_str());


        auto foo = [this](tcp::envelope_prefix prefix, tcp::envelope_payload payload) -> coro::task<int>
        {
            // do a call
            if(payload.payload_fingerprint == rpc::id<tcp::call_send>::get(prefix.version))
            {
                service_->get_scheduler()->schedule(stub_handle_send(std::move(prefix), std::move(payload)));
            }
            // do a try cast
            else if(payload.payload_fingerprint == rpc::id<tcp::try_cast_send>::get(prefix.version))
            {
                service_->get_scheduler()->schedule(stub_handle_try_cast(std::move(prefix), std::move(payload)));
            }
            // do an add_ref
            else if(payload.payload_fingerprint == rpc::id<tcp::addref_send>::get(prefix.version))
            {
                service_->get_scheduler()->schedule(stub_handle_add_ref(std::move(prefix), std::move(payload)));
            }
            // do a release
            else if(payload.payload_fingerprint == rpc::id<tcp::release_send>::get(prefix.version))
            {
                service_->get_scheduler()->schedule(stub_handle_release(std::move(prefix), std::move(payload)));
            }
            // create the service proxy
            else if(payload.payload_fingerprint == rpc::id<tcp::init_client_channel_send>::get(prefix.version))
            {
                service_->get_scheduler()->schedule(create_stub(std::move(prefix), std::move(payload)));
            }
            // do a try cast
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

                // std::string msg("pump_send_and_receive prefix.sequence_number ");
                // msg += std::to_string(service_->get_zone_id().get_val());
                // msg += "\n prefix = ";
                // msg += rpc::to_yas_json<std::string>(prefix);
                // msg += "\n payload = ";
                // msg += rpc::to_yas_json<std::string>(result->payload);
                // LOG_CSTR(msg.c_str());

                result->error_code = rpc::error::OK();
                result->event.set();
            }
            co_return rpc::error::OK();
        };

        return service_->get_scheduler()->schedule(pump_messages(foo));
    }

    CORO_TASK(void) channel_manager::shutdown()
    {
        shutdown_ = true;
        co_await shutdown_event_;
    }

    CORO_TASK(void)
    channel_manager::pump_messages(
        std::function<CORO_TASK(int)(tcp::envelope_prefix, tcp::envelope_payload)> incoming_message_handler)
    {
        static auto envelope_prefix_saved_size = rpc::yas_binary_saved_size(tcp::envelope_prefix());

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

        while(shutdown_ == false)
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
                tcp::envelope_prefix prefix {};

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
                            LOG_CSTR("failed invalid prefix");
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
                        tcp::envelope_payload payload;
                        auto str_err = rpc::from_yas_binary(rpc::span(buf), payload);
                        if(!str_err.empty())
                        {
                            LOG_CSTR("failed bad payload format");
                            break;
                        }
                        auto ret = co_await incoming_message_handler(std::move(prefix), std::move(payload));
                        if(ret != rpc::error::OK())
                        {
                            LOG_CSTR("failed incoming_message_handler");
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
    CORO_TASK(void) channel_manager::stub_handle_send(tcp::envelope_prefix prefix, tcp::envelope_payload payload)
    {
        LOG_CSTR("send request");

        assert(prefix.direction == tcp::message_direction::send || prefix.direction == tcp::message_direction::one_way);

        tcp::call_send request;
        auto str_err = rpc::from_yas_compressed_binary(rpc::span(payload.payload), request);
        if(!str_err.empty())
        {
            LOG_CSTR("failed from_yas_compressed_binary call_send");
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
            LOG_CSTR("failed send");
        }

        if(prefix.direction == tcp::message_direction::one_way)
            CO_RETURN;

        auto err = CO_AWAIT send_payload(prefix.version, tcp::message_direction::receive,
                                         tcp::call_receive {.payload = std::move(out_buf), .err_code = ret},
                                         prefix.sequence_number);
        if(err != rpc::error::OK())
        {
            LOG_CSTR("failed send_payload");
            kill_connection();
            CO_RETURN;
        }
        // LOG_CSTR("send request complete");
        CO_RETURN;
    }

    // do a try cast
    CORO_TASK(void)
    channel_manager::stub_handle_try_cast(tcp::envelope_prefix prefix, tcp::envelope_payload payload)
    {
        LOG_CSTR("try_cast request");

        tcp::try_cast_send request;
        auto str_err = rpc::from_yas_compressed_binary(rpc::span(payload.payload), request);
        if(!str_err.empty())
        {
            LOG_CSTR("failed try_cast_send from_yas_compressed_binary");
            kill_connection();
            CO_RETURN;
        }

        std::vector<char> out_buf;
        auto ret = co_await service_->try_cast(prefix.version, {request.destination_zone_id}, {request.object_id},
                                               {request.interface_id});
        if(ret != rpc::error::OK())
        {
            LOG_CSTR("failed try_cast");
        }

        auto err = CO_AWAIT send_payload(prefix.version, tcp::message_direction::receive,
                                         tcp::try_cast_receive {.err_code = ret}, prefix.sequence_number);
        if(err != rpc::error::OK())
        {
            LOG_CSTR("failed try_cast_send send_payload");
            kill_connection();
            CO_RETURN;
        }
        LOG_CSTR("try_cast request complete");
        CO_RETURN;
    }

    // do an add_ref
    CORO_TASK(void) channel_manager::stub_handle_add_ref(tcp::envelope_prefix prefix, tcp::envelope_payload payload)
    {
        LOG_CSTR("add_ref request");

        tcp::addref_send request;
        auto str_err = rpc::from_yas_compressed_binary(rpc::span(payload.payload), request);
        if(!str_err.empty())
        {
            LOG_CSTR("failed addref_send from_yas_compressed_binary");
            kill_connection();
            CO_RETURN;
        }

        std::vector<char> out_buf;
        auto ret = co_await service_->add_ref(prefix.version, {request.destination_channel_zone_id},
                                              {request.destination_zone_id}, {request.object_id},
                                              {request.caller_channel_zone_id}, {request.caller_zone_id},
                                              (rpc::add_ref_options)request.build_out_param_channel);

        if(ret == std::numeric_limits<uint64_t>::max())
        {
            LOG_CSTR("failed add_ref");
        }

        auto err = CO_AWAIT send_payload(prefix.version, tcp::message_direction::receive,
                                         tcp::addref_receive {.ref_count = ret, .err_code = rpc::error::OK()},
                                         prefix.sequence_number);
        if(err != rpc::error::OK())
        {
            LOG_CSTR("failed addref_send send_payload");
            kill_connection();
            CO_RETURN;
        }
        // LOG_CSTR("add_ref request complete");
        CO_RETURN;
    }

    // do a release
    CORO_TASK(void) channel_manager::stub_handle_release(tcp::envelope_prefix prefix, tcp::envelope_payload payload)
    {
        LOG_CSTR("release request");
        tcp::release_send request;
        auto str_err = rpc::from_yas_compressed_binary(rpc::span(payload.payload), request);
        if(!str_err.empty())
        {
            LOG_CSTR("failed release_send from_yas_compressed_binary");
            kill_connection();
            CO_RETURN;
        }

        std::vector<char> out_buf;
        auto ret = co_await service_->release(prefix.version, {request.destination_zone_id}, {request.object_id},
                                              {request.caller_zone_id});

        if(ret == std::numeric_limits<uint64_t>::max())
        {
            LOG_CSTR("failed release");
        }

        auto err = CO_AWAIT send_payload(prefix.version, tcp::message_direction::receive,
                                         tcp::release_receive {.ref_count = ret, .err_code = rpc::error::OK()},
                                         prefix.sequence_number);
        if(err != rpc::error::OK())
        {
            LOG_CSTR("failed release_send send_payload");
            kill_connection();
            CO_RETURN;
        }
        // LOG_CSTR("release request complete");
        CO_RETURN;
    }

    CORO_TASK(void) channel_manager::create_stub(tcp::envelope_prefix prefix, tcp::envelope_payload payload)
    {
        std::string msg("run_client init_client_channel_send ");
        msg += std::to_string(service_->get_zone_id().get_val());
        LOG_CSTR(msg.c_str());

        tcp::init_client_channel_send request;
        auto err = rpc::from_yas_compressed_binary(rpc::span(payload.payload), request);
        if(!err.empty())
        {
            LOG_CSTR("failed run_client init_client_channel_send");
            CO_RETURN;
        }
        tcp::init_client_channel_response response;
        int ret = co_await connection_handler_(request, response, service_, keep_alive_);
        connection_handler_ = nullptr; // handover references etc
        if(ret != rpc::error::OK())
        {
            // report error
            auto randonmumber = "failed to connect to zone " + std::to_string(ret) + " \n";
            LOG_STR(randonmumber.c_str(), randonmumber.size());
            CO_RETURN;
        }

        err = CO_AWAIT send_payload(prefix.version, tcp::message_direction::receive, std::move(response),
                                    prefix.sequence_number);
        std::ignore = err;

        CO_RETURN;
    }
}