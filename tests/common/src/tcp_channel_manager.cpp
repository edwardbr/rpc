#include <common/tcp_channel_manager.h>

CORO_TASK(void) tcp_channel_manager::launch_send_receive()
{
    static auto envelope_prefix_saved_size = rpc::compressed_yas_binary_saved_size(tcp::envelope_prefix());
    bool expecting_prefix = true;
    bool read_complete = true;
    tcp::envelope_prefix prefix {};
    std::vector<char> buf;
    std::span remaining_span(buf.begin(), buf.end());

    do
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
    } while(true);
}

// read from the connection and fill the buffer as it has been presized
CORO_TASK(int) tcp_channel_manager::read(std::vector<char>& buf)
{
    std::span remaining_span(buf.begin(), buf.end());
    do
    {
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

        if(pstatus == coro::poll_status::error)
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
    static auto envelope_prefix_saved_size = rpc::compressed_yas_binary_saved_size(tcp::envelope_prefix());

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