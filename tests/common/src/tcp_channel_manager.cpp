#include <common/tcp_channel_manager.h>
#include <common/tcp_service_proxy.h>

// this method sends queued requests to other peers and receives responses notifying the proxy when complete
CORO_TASK(void) tcp_channel_manager::pump_send_and_receive()
{
    static auto envelope_prefix_saved_size = rpc::yas_binary_saved_size(tcp::envelope_prefix());
    bool expecting_prefix = true;
    bool read_complete = true;
    tcp::envelope_prefix prefix {};
    std::vector<char> buf;
    std::span remaining_span(buf.begin(), buf.end());

    while(worker_release_.lock())
    {
        {
            auto scoped_lock = co_await connection_mtx_.lock();
            while(!send_queue_.empty())
            {
                auto status = co_await client_.poll(coro::poll_op::write, timeout_);
                if(status != coro::poll_status::event)
                {
                    CO_RETURN;
                }

                auto& item = send_queue_.front();
                auto marshal_status = client_.send(std::span {(const char*)item.data(), item.size()});
                if(marshal_status.first != coro::net::send_status::ok)
                {
                    CO_RETURN;
                }
                send_queue_.pop();
            }
        }

        if(read_complete)
        {
            size_t buf_size = expecting_prefix ? envelope_prefix_saved_size : prefix.payload_size;
            buf = std::vector<char>(buf_size, '\0');
            remaining_span = std::span(buf.begin(), buf.end());
            read_complete = false;
        }

        {
            auto pstatus = co_await client_.poll(coro::poll_op::read, std::chrono::milliseconds(1));
            if(pstatus == coro::poll_status::timeout)
            {
                // a timeout has occurred
                continue;
            }

            if(pstatus == coro::poll_status::error)
            {
                // an error has occured
                // errno;
                CO_RETURN;
            }

            if(pstatus == coro::poll_status::closed)
            {
                // connection closed by peer
                CO_RETURN;
            }

            {
                auto [recv_status, recv_bytes] = client_.recv(remaining_span);
                if(recv_status == coro::net::recv_status::ok)
                {
                    if(recv_bytes.size() != remaining_span.size())
                    {
                        remaining_span = std::span(remaining_span.begin() + recv_bytes.size(), remaining_span.end());
                        continue;
                    }
                    else
                    {
                        // buf is now fully proceed with reading the rest of the data with this message
                        read_complete = true;
                        if(expecting_prefix)
                        {
                            expecting_prefix = false;
                            auto str_err = rpc::from_yas_binary(rpc::span(buf), prefix);
                            if(!str_err.empty())
                            {
                                co_return;
                            }
                            assert(prefix.sequence_number);
                        }
                        else
                        {
                            expecting_prefix = true;

                            // now find the relevant event handler and set its values before triggering it
                            std::scoped_lock lock(pending_transmits_mtx_);
                            auto it = pending_transmits_.find(prefix.sequence_number);
                            assert(it != pending_transmits_.end());
                            auto& result = *it->second;
                            pending_transmits_.erase(it);

                            result.prefix = prefix;
                            auto str_err = rpc::from_yas_binary(rpc::span(buf), result.payload);
                            if(!str_err.empty())
                            {
                                result.error_code = rpc::error::TRANSPORT_ERROR();
                                co_return;
                            }
                            result.error_code = rpc::error::OK();
                            result.event.set();
                        }
                    }
                }
                else if(recv_status == coro::net::recv_status::try_again
                        || recv_status == coro::net::recv_status::would_block)
                {
                    // something went wrong so we dont expect the recv_bytes to have anything
                    assert(recv_bytes.empty());

                    continue;
                }
                else
                {
                    // connection closed by peer
                    // errno;
                    CO_RETURN;
                }
            }
        }
    }

    {
        std::scoped_lock lock(pending_transmits_mtx_);
        for(auto it : pending_transmits_)
        {
            it.second->error_code = rpc::error::CALL_CANCELLED();
            it.second->event.set();
        }
    }
}

coro::task<void> tcp_channel_manager::pump_stub_receive_and_send()
{
    while(worker_release_.lock())
    {
        tcp::envelope_prefix prefix;
        tcp::envelope_payload payload;
        int err = CO_AWAIT receive_anonymous_payload(prefix, payload, 0);
        if(err != rpc::error::OK())
            CO_RETURN;

        // do a call
        if(payload.payload_fingerprint == rpc::id<tcp::call_send>::get(prefix.version))
        {
            tcp::call_send request;
            auto str_err = rpc::from_yas_compressed_binary(rpc::span(payload.payload), request);
            if(!str_err.empty())
            {
                // bad format
                CO_RETURN;
            }

            std::vector<char> out_buf;
            auto ret = co_await service_->send(
                prefix.version, request.encoding, request.tag, {request.caller_channel_zone_id},
                {request.caller_zone_id}, {request.destination_zone_id}, {request.object_id}, {request.interface_id},
                {request.method_id}, request.payload.size(), request.payload.data(), out_buf);

            auto err = CO_AWAIT send_payload(prefix.version,
                                             tcp::call_receive {.payload = std::move(out_buf), .err_code = ret},
                                             prefix.sequence_number);
            if(err != rpc::error::OK())
            {
                // report error
                CO_RETURN;
            }
        }
        // do a try cast
        else if(payload.payload_fingerprint == rpc::id<tcp::try_cast_send>::get(prefix.version))
        {
            tcp::try_cast_send request;
            auto str_err = rpc::from_yas_compressed_binary(rpc::span(payload.payload), request);
            if(!str_err.empty())
            {
                // bad format
                CO_RETURN;
            }

            std::vector<char> out_buf;
            auto ret = co_await service_->try_cast(prefix.version, {request.destination_zone_id}, {request.object_id},
                                                   {request.interface_id});

            auto err = CO_AWAIT send_payload(prefix.version, tcp::try_cast_receive {.err_code = ret},
                                             prefix.sequence_number);
            if(err != rpc::error::OK())
            {
                // report error
                CO_RETURN;
            }
        }
        // do an add_ref
        else if(payload.payload_fingerprint == rpc::id<tcp::addref_send>::get(prefix.version))
        {
            tcp::addref_send request;
            auto str_err = rpc::from_yas_compressed_binary(rpc::span(payload.payload), request);
            if(!str_err.empty())
            {
                // bad format
                CO_RETURN;
            }

            std::vector<char> out_buf;
            auto ret = co_await service_->add_ref(prefix.version, {request.destination_channel_zone_id},
                                                  {request.destination_zone_id}, {request.object_id},
                                                  {request.caller_channel_zone_id}, {request.caller_zone_id},
                                                  (rpc::add_ref_options)request.build_out_param_channel);

            auto err = CO_AWAIT send_payload(prefix.version,
                                             tcp::addref_receive {.ref_count = ret, .err_code = rpc::error::OK()},
                                             prefix.sequence_number);
            if(err != rpc::error::OK())
            {
                // report error
                CO_RETURN;
            }
        }
        // do a release
        else if(payload.payload_fingerprint == rpc::id<tcp::release_send>::get(prefix.version))
        {
            tcp::release_send request;
            auto str_err = rpc::from_yas_compressed_binary(rpc::span(payload.payload), request);
            if(!str_err.empty())
            {
                // bad format
                CO_RETURN;
            }

            std::vector<char> out_buf;
            auto ret = co_await service_->release(prefix.version, {request.destination_zone_id}, {request.object_id},
                                                  {request.caller_zone_id});

            auto err = CO_AWAIT send_payload(prefix.version,
                                             tcp::release_receive {.ref_count = ret, .err_code = rpc::error::OK()},
                                             prefix.sequence_number);
            if(err != rpc::error::OK())
            {
                // report error
                CO_RETURN;
            }
        }
        else
        {
            // bad message
            CO_RETURN;
        }
    }
    worker_release_.reset();
}

// read from the connection and fill the buffer as it has been presized
CORO_TASK(int) tcp_channel_manager::read(std::vector<char>& buf)
{
    std::span remaining_span(buf.begin(), buf.end());
    do
    {
        // if(!client_.socket().is_valid())
        // {
        //     co_return rpc::error::SERVICE_PROXY_LOST_CONNECTION();
        // }

        auto pstatus = co_await client_.poll(coro::poll_op::read, timeout_);
        if(pstatus == coro::poll_status::timeout)
        {
            // a timeout has occurred
            co_return rpc::error::SERVICE_PROXY_LOST_CONNECTION();
        }

        if(pstatus == coro::poll_status::error)
        {
            // an error has occured
            // errno;
            co_return rpc::error::SERVICE_PROXY_LOST_CONNECTION();
        }

        if(pstatus == coro::poll_status::closed)
        {
            // connection closed by peer
            co_return rpc::error::SERVICE_PROXY_LOST_CONNECTION();
        }
        {

            auto [recv_status, recv_bytes] = client_.recv(remaining_span);
            if(recv_status == coro::net::recv_status::ok)
            {
                if(recv_bytes.size() == remaining_span.size())
                {
                    // buf is now full proceed with reading the rest of the data with this message
                    break;
                }
                remaining_span = std::span(remaining_span.begin() + recv_bytes.size(), remaining_span.end());
                continue;
            }

            // something went wrong so we dont expect the recv_bytes to have anything
            assert(recv_bytes.empty());

            if(recv_status == coro::net::recv_status::try_again || recv_status == coro::net::recv_status::would_block)
            {
                continue;
            }

            // connection closed by peer
            // errno;
            co_return rpc::error::SERVICE_PROXY_LOST_CONNECTION();
        }
        /* code */
    } while(false);
    co_return rpc::error::OK();
}

CORO_TASK(int)
tcp_channel_manager::receive_prefix(tcp::envelope_prefix& prefix)
{
    static auto envelope_prefix_saved_size = rpc::yas_binary_saved_size(tcp::envelope_prefix());

    std::vector<char> buf(envelope_prefix_saved_size, '\0');
    int err = CO_AWAIT read(buf);
    if(err != rpc::error::OK())
        co_return err;

    auto str_err = rpc::from_yas_binary(rpc::span(buf), prefix);
    if(!str_err.empty())
    {
        co_return rpc::error::TRANSPORT_ERROR();
    }
    co_return rpc::error::OK();
}

// read a messsage from a peer
CORO_TASK(int)
tcp_channel_manager::receive_anonymous_payload(tcp::envelope_prefix& prefix, tcp::envelope_payload& payload,
                                               uint64_t sequence_number)
{
    if(sequence_number == 0)
    {
        int err = CO_AWAIT receive_prefix(prefix);
        if(err != rpc::error::OK())
            co_return err;

        std::vector<char> buf(prefix.payload_size, '\0');
        err = CO_AWAIT read(buf);
        if(err != rpc::error::OK())
        {
            co_return err;
        }

        auto str_err = rpc::from_yas_binary(rpc::span(buf), payload);
        if(!str_err.empty())
        {
            co_return rpc::error::TRANSPORT_ERROR();
        }
    }
    else
    {
        std::scoped_lock lock(pending_transmits_mtx_);
        result_listener res_payload;
        auto [it, success] = pending_transmits_.try_emplace(sequence_number, &res_payload);
        assert(success);
        co_await res_payload.event; // now wait for the reply
        prefix = res_payload.prefix;
        payload = res_payload.payload;
    }
    co_return rpc::error::OK();
}