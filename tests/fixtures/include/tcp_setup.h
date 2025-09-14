#include <common/foo_impl.h>

template<bool UseHostInChild, bool RunStandardTests, bool CreateNewZoneThenCreateSubordinatedZone> class tcp_setup
{
    rpc::shared_ptr<rpc::service> root_service_;
    rpc::shared_ptr<rpc::service> peer_service_;

    std::shared_ptr<rpc::tcp::listener<yyy::i_host, yyy::i_example>> peer_listener_;
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

    virtual ~tcp_setup() = default;

    rpc::shared_ptr<rpc::service> get_root_service() const { return root_service_; }
    std::shared_ptr<rpc::tcp::listener<yyy::i_host, yyy::i_example>> get_peer_listener() const
    {
        return peer_listener_;
    };
    bool get_has_enclave() const { return has_enclave_; }
    bool is_enclave_setup() const { return false; }
    rpc::shared_ptr<yyy::i_example> get_example() const { return i_example_ptr_; }
    rpc::shared_ptr<yyy::i_host> get_host() const { return i_host_ptr_; }
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
        auto test_info = ::testing::UnitTest::GetInstance()->current_test_info();
#ifdef USE_RPC_TELEMETRY
        if (enable_telemetry_server)
            telemetry_service_manager_.create(test_info->test_suite_name(), test_info->name(), "../../rpc_test_diagram/");
#endif

        auto root_zone_id = rpc::zone{++zone_gen_};
        auto peer_zone_id = rpc::zone{++zone_gen_};
        root_service_ = rpc::make_shared<rpc::service>("host", root_zone_id, io_scheduler_);
        example_import_idl_register_stubs(root_service_);
        example_shared_idl_register_stubs(root_service_);
        example_idl_register_stubs(root_service_);
        peer_service_ = rpc::make_shared<rpc::service>("peer", peer_zone_id, io_scheduler_);
        example_import_idl_register_stubs(peer_service_);
        example_shared_idl_register_stubs(peer_service_);
        example_idl_register_stubs(peer_service_);

        peer_listener_ = std::make_shared<rpc::tcp::listener<yyy::i_host, yyy::i_example>>(
            [](const rpc::shared_ptr<yyy::i_host>& host,
                rpc::shared_ptr<yyy::i_example>& new_example,
                const rpc::shared_ptr<rpc::service>& child_service_ptr) -> CORO_TASK(int)
            {
                new_example = rpc::shared_ptr<yyy::i_example>(new marshalled_tests::example(child_service_ptr, host));
                CO_RETURN rpc::error::OK();
            },
            std::chrono::milliseconds(100000));
        peer_listener_->start_listening(peer_service_);

        current_host_service = root_service_;

        rpc::shared_ptr<yyy::i_host> hst(new host());
        local_host_ptr_ = hst; // assign to weak ptr

        auto ret = CO_AWAIT root_service_->connect_to_zone<rpc::tcp::service_proxy>("main child",
            peer_zone_id.as_destination(),
            hst,
            i_example_ptr_,
            std::chrono::milliseconds(100000),
            coro::net::tcp::client::options{
                .address = {coro::net::ip_address::from_string("127.0.0.1")},
                .port = 8080,
            });
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
        CO_AWAIT peer_listener_->stop_listening();
        peer_listener_.reset();
        while (!peer_service_->has_service_proxies())
            co_await io_scheduler_->schedule();
        while (!root_service_->has_service_proxies())
            co_await io_scheduler_->schedule();
        peer_service_.reset();
        root_service_.reset();
        zone_gen = nullptr;
#ifdef USE_RPC_TELEMETRY
        RESET_TELEMETRY_SERVICE
#endif
        has_stopped_ = true;
        CO_RETURN;
    }

    virtual void tear_down()
    {
        io_scheduler_->schedule(CoroTearDown());
        while (has_stopped_ == false)
        {
            io_scheduler_->process_events(std::chrono::milliseconds(1000000));
        }
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
                    const rpc::shared_ptr<rpc::child_service>& child_service_ptr) -> CORO_TASK(int)
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
