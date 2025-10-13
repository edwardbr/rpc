
/*
 *   Copyright (c) 2025 Edward Boggis-Rolfe
 *   All rights reserved.
 */

#ifdef USE_RPC_TELEMETRY
#include <rpc/telemetry/i_telemetry_service.h>
#include <rpc/telemetry/multiplexing_telemetry_service.h>
#endif

/////////////////////////////////////////////////////////////////

template<bool UseHostInChild, bool RunStandardTests, bool CreateNewZoneThenCreateSubordinatedZone> class spsc_setup
{
    std::shared_ptr<rpc::service> root_service_;
    std::shared_ptr<rpc::service> peer_service_;

    rpc::spsc::queue_type send_spsc_queue_;
    rpc::spsc::queue_type receive_spsc_queue_;

    rpc::shared_ptr<yyy::i_host> i_host_ptr_;
    rpc::weak_ptr<yyy::i_host> local_host_ptr_;
    rpc::shared_ptr<yyy::i_example> i_example_ptr_;

    const bool has_enclave_ = true;
    bool use_host_in_child_ = UseHostInChild;
    bool run_standard_tests_ = RunStandardTests;

    std::atomic<uint64_t> zone_gen_ = 0;

    std::shared_ptr<coro::io_scheduler> io_scheduler_;
    bool error_has_occured_ = false;
    bool has_stopped_ = true;

public:
    std::shared_ptr<coro::io_scheduler> get_scheduler() const { return io_scheduler_; }
    bool error_has_occured() const { return error_has_occured_; }

    virtual ~spsc_setup() = default;

    std::shared_ptr<rpc::service> get_root_service() const { return root_service_; }
    bool get_has_enclave() const { return has_enclave_; }
    bool is_enclave_setup() const { return false; }
    rpc::shared_ptr<yyy::i_example> get_example() const { return i_example_ptr_; }
    void set_example(const rpc::shared_ptr<yyy::i_example>& example) { i_example_ptr_ = example; }
    rpc::shared_ptr<yyy::i_host> get_host() const { return i_host_ptr_; }
    void set_host(const rpc::shared_ptr<yyy::i_host>& host) { i_host_ptr_ = host; }
    rpc::shared_ptr<yyy::i_host> get_local_host_ptr() { return local_host_ptr_.lock(); }
    bool get_use_host_in_child() const { return use_host_in_child_; }

    CORO_TASK(void) check_for_error(coro::task<bool> task)
    {
        auto ret = CO_AWAIT task;
        if (!ret)
        {
            error_has_occured_ = true;
        }
        CO_RETURN;
    }

    CORO_TASK(bool) CoroSetUp(bool& is_ready)
    {
        zone_gen = &zone_gen_;
#ifdef USE_RPC_TELEMETRY
        auto test_info = ::testing::UnitTest::GetInstance()->current_test_info();
        if (auto telemetry_service
            = std::static_pointer_cast<rpc::multiplexing_telemetry_service>(rpc::get_telemetry_service()))
        {
            telemetry_service->start_test(test_info->test_suite_name(), test_info->name());
        }
#endif

        auto root_zone_id = rpc::zone{++zone_gen_};
        auto peer_zone_id = rpc::zone{++zone_gen_};
        root_service_ = std::make_shared<rpc::service>("host", root_zone_id, io_scheduler_);
        example_import_idl_register_stubs(root_service_);
        example_shared_idl_register_stubs(root_service_);
        example_idl_register_stubs(root_service_);

        peer_service_ = std::make_shared<rpc::service>("peer", peer_zone_id, io_scheduler_);
        example_import_idl_register_stubs(peer_service_);
        example_shared_idl_register_stubs(peer_service_);
        example_idl_register_stubs(peer_service_);

        // This makes the receiving service proxy for the connection
        rpc::spsc::channel_manager::connection_handler handler
            = [send_spsc_queue = &send_spsc_queue_,
                  receive_spsc_queue = &receive_spsc_queue_,
                  use_host_in_child = use_host_in_child_](const rpc::interface_descriptor& input_interface,
                  rpc::interface_descriptor& output_interface,
                  std::shared_ptr<rpc::service> service,
                  std::shared_ptr<rpc::spsc::channel_manager> channel) -> CORO_TASK(int)
        {
            auto ret = CO_AWAIT service->attach_remote_zone<rpc::spsc::service_proxy, yyy::i_host, yyy::i_example>(
                "service_proxy",
                input_interface,
                output_interface,
                [&](const rpc::shared_ptr<yyy::i_host>& host,
                    rpc::shared_ptr<yyy::i_example>& new_example,
                    const std::shared_ptr<rpc::service>& child_service_ptr) -> CORO_TASK(int)
                {
                    new_example = rpc::shared_ptr<yyy::i_example>(new marshalled_tests::example(child_service_ptr, host));

                    if (use_host_in_child)
                        CO_AWAIT new_example->set_host(host);
                    CO_RETURN rpc::error::OK();
                },
                input_interface.destination_zone_id,
                channel,
                send_spsc_queue,
                receive_spsc_queue);
            co_return ret;
        };

        auto channel = rpc::spsc::channel_manager::create(std::chrono::milliseconds(1000),
            peer_service_,
            &receive_spsc_queue_, // these two parameters are reversed for the receiver
            &send_spsc_queue_,    // these two parameters are reversed for the receiver
            handler);
        channel->pump_send_and_receive(); // get the receiver pump going

        rpc::shared_ptr<yyy::i_host> hst(new host());
        local_host_ptr_ = hst; // assign to weak ptr

        auto ret = CO_AWAIT root_service_->connect_to_zone<rpc::spsc::service_proxy>("main child",
            peer_zone_id.as_destination(),
            hst,
            i_example_ptr_,
            std::chrono::milliseconds(100000),
            &send_spsc_queue_,
            &receive_spsc_queue_);

        if (ret != rpc::error::OK())
        {
            CO_RETURN false;
        }
        is_ready = true;
        CO_RETURN true;
    }

    virtual void set_up()
    {
        has_stopped_ = false;
        io_scheduler_ = coro::io_scheduler::make_shared(
            coro::io_scheduler::options{.thread_strategy = coro::io_scheduler::thread_strategy_t::manual,
                .pool = coro::thread_pool::options{
                    .thread_count = 1,
                }});

        bool is_ready = false;
        io_scheduler_->schedule(check_for_error(CoroSetUp(is_ready)));
        while (!is_ready)
        {
            io_scheduler_->process_events(std::chrono::milliseconds(1));
        }

        // auto err_code = SYNC_WAIT();

        ASSERT_EQ(error_has_occured_, false);
    }

    CORO_TASK(void) CoroTearDown()
    {
        i_example_ptr_ = nullptr;
        i_host_ptr_ = nullptr;
        local_host_ptr_.reset();
        while (peer_service_->has_service_proxies())
            co_await io_scheduler_->schedule();
        while (root_service_->has_service_proxies())
            co_await io_scheduler_->schedule();

        // has_stopped_ = true;
        CO_RETURN;
    }

    virtual void tear_down()
    {
        io_scheduler_->schedule(CoroTearDown());
        while (!io_scheduler_->empty())
        {
            io_scheduler_->process_events(std::chrono::milliseconds(10));
        }
        peer_service_.reset();
        root_service_.reset();
        zone_gen = nullptr;
#ifdef USE_RPC_TELEMETRY
        if (auto telemetry_service
            = std::static_pointer_cast<rpc::multiplexing_telemetry_service>(rpc::get_telemetry_service()))
        {
            telemetry_service->reset_for_test();
        }
#endif
        // SYNC_WAIT(CoroTearDown());
    }

    CORO_TASK(rpc::shared_ptr<yyy::i_example>) create_new_zone()
    {
        rpc::shared_ptr<yyy::i_host> hst;
        if (use_host_in_child_)
            hst = local_host_ptr_.lock();

        rpc::shared_ptr<yyy::i_example> example_relay_ptr;

        auto new_zone_id = ++(zone_gen_);
        auto err_code
            = CO_AWAIT root_service_->connect_to_zone<rpc::local_child_service_proxy<yyy::i_example, yyy::i_host>>(
                "main child",
                {new_zone_id},
                hst,
                example_relay_ptr,
                [&](const rpc::shared_ptr<yyy::i_host>& host,
                    rpc::shared_ptr<yyy::i_example>& new_example,
                    const std::shared_ptr<rpc::child_service>& child_service_ptr) -> CORO_TASK(int)
                {
                    example_import_idl_register_stubs(child_service_ptr);
                    example_shared_idl_register_stubs(child_service_ptr);
                    example_idl_register_stubs(child_service_ptr);
                    new_example = rpc::make_shared<marshalled_tests::example>(child_service_ptr, nullptr);
                    if (use_host_in_child_)
                        CO_AWAIT new_example->set_host(host);
                    CO_RETURN rpc::error::OK();
                });

        if (CreateNewZoneThenCreateSubordinatedZone)
        {
            rpc::shared_ptr<yyy::i_example> new_ptr;
            if (CO_AWAIT example_relay_ptr->create_example_in_subordinate_zone(
                    new_ptr, use_host_in_child_ ? hst : nullptr, ++zone_gen_)
                == rpc::error::OK())
            {
                CO_AWAIT example_relay_ptr->set_host(nullptr);
                example_relay_ptr = new_ptr;
            }
        }
        CO_RETURN example_relay_ptr;
    }
};
