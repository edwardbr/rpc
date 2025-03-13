/*
 *   Copyright (c) 2024 Edward Boggis-Rolfe
 *   All rights reserved.
 */
#include <iostream>
#include <unordered_map>
#include <string_view>
#include <thread>

#ifdef BUILD_ENCLAVE
#include "untrusted/enclave_marshal_test_u.h"
#include <common/enclave_service_proxy.h>
#endif

#include <common/foo_impl.h>
#include <common/tests.h>

#include <example/example.h>

#include <rpc/basic_service_proxies.h>
#ifdef USE_RPC_TELEMETRY
#include <rpc/telemetry/host_telemetry_service.h>
#endif
#include "common/tcp/service_proxy.h"
#include "common/tcp/listener.h"

#include <rpc/coroutine_support.h>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include <clipp.h>

#ifdef BUILD_COROUTINE
#include <coro/io_scheduler.hpp>
#endif

#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <rpc/error_codes.h>


// This list should be kept sorted.
using testing::Action;
using testing::ActionInterface;
using testing::Assign;
using testing::ByMove;
using testing::ByRef;
using testing::DoDefault;
using testing::IgnoreResult;
using testing::Invoke;
using testing::InvokeWithoutArgs;
using testing::MakePolymorphicAction;
using testing::Ne;
using testing::PolymorphicAction;
using testing::Return;
using testing::ReturnNull;
using testing::ReturnRef;
using testing::ReturnRefOfCopy;
using testing::SetArgPointee;
using testing::SetArgumentPointee;
using testing::_;
using testing::get;
using testing::make_tuple;
using testing::tuple;
using testing::tuple_element;
using testing::Types;
using ::testing::Sequence;

using namespace marshalled_tests;

#ifdef _WIN32 // windows
std::string enclave_path = "./marshal_test_enclave.signed.dll";
#else         // Linux
std::string enclave_path = "./libmarshal_test_enclave.signed.so";
#endif

#ifdef USE_RPC_TELEMETRY
TELEMETRY_SERVICE_MANAGER
#endif
bool enable_telemetry_server = true;
bool enable_multithreaded_tests = false;

rpc::weak_ptr<rpc::service> current_host_service;

std::atomic<uint64_t>* zone_gen = nullptr;


// This line tests that we can define tests in an unnamed namespace.
namespace {

	extern "C" int main(int argc, char* argv[])
	{
        std::string disable_telemetry_server;
        std::string enable_multithreaded_tests_flag;
        auto cli = (
			clipp::option("-t", "--disable_telemetry_server").doc("disable the telemetry_server") & clipp::value("disable_telemetry_server", disable_telemetry_server),
			clipp::option("-m", "--enable_multithreaded_tests").doc("enable multithreaded tests") & clipp::value("enable_multithreaded_tests", enable_multithreaded_tests_flag)
        );

        clipp::parsing_result res = clipp::parse(argc, argv, cli);
        
        enable_telemetry_server = disable_telemetry_server != "true";
        enable_multithreaded_tests = enable_multithreaded_tests_flag == "true";
        
        auto logger = spdlog::stdout_color_mt("console");
        logger->set_pattern("[%^%l%$] %v");        
        spdlog::set_default_logger(logger);
		::testing::InitGoogleTest(&argc, argv);
		return RUN_ALL_TESTS();
	}
}

class host : 
    public yyy::i_host,
    public rpc::enable_shared_from_this<host>
{
    rpc::zone zone_id_;

  	//perhaps this should be an unsorted list but map is easier to debug for now
    std::map<std::string, rpc::shared_ptr<yyy::i_example>> cached_apps_; 
    std::mutex cached_apps_mux_;

    void* get_address() const override { return (void*)this; }
    const rpc::casting_interface* query_interface(rpc::interface_ordinal interface_id) const override
    {
        if (rpc::match<yyy::i_host>(interface_id))
            return static_cast<const yyy::i_host*>(this);
        return nullptr;
    }

public:

    host(rpc::zone zone_id) : 
        zone_id_(zone_id)
    {
#ifdef USE_RPC_TELEMETRY
        if (auto telemetry_service = rpc::telemetry_service_manager::get(); telemetry_service)
            telemetry_service->on_impl_creation("host", (uint64_t)this, zone_id_);
#endif            
    }
    virtual ~host()
    {
#ifdef USE_RPC_TELEMETRY
        if (auto telemetry_service = rpc::telemetry_service_manager::get(); telemetry_service)
            telemetry_service->on_impl_deletion((uint64_t)this, zone_id_);
#endif            
    }
    CORO_TASK(error_code) create_enclave(rpc::shared_ptr<yyy::i_example>& target) override
    {
#ifdef BUILD_ENCLAVE
        rpc::shared_ptr<yyy::i_host> host = shared_from_this();
        auto serv = current_host_service.lock();
        auto err_code = serv->connect_to_zone<rpc::enclave_service_proxy>( 
            "an enclave"
            , {++(*zone_gen)}
            , host
            , target
            , enclave_path);

        CO_RETURN err_code;
#endif
        CO_RETURN rpc::error::INCOMPATIBLE_SERVICE();
    };

    //live app registry, it should have sole responsibility for the long term storage of app shared ptrs
    CORO_TASK(error_code) look_up_app(const std::string& app_name, rpc::shared_ptr<yyy::i_example>& app) override
    {
        {
            std::scoped_lock lock(cached_apps_mux_);
            auto it = cached_apps_.find(app_name);
            if(it != cached_apps_.end())
            {
                app = it->second;
            }
        }
        CO_RETURN rpc::error::OK();
    }

    CORO_TASK(error_code) set_app(const std::string& name, const rpc::shared_ptr<yyy::i_example>& app) override
    {
        {
            std::scoped_lock lock(cached_apps_mux_);        
            cached_apps_[name] = app;
        }
        CO_RETURN rpc::error::OK();
    }

    CORO_TASK(error_code) unload_app(const std::string& name) override
    {
        {
            std::scoped_lock lock(cached_apps_mux_);
            cached_apps_.erase(name);
        }
        CO_RETURN rpc::error::OK();
    }
};

template<bool UseHostInChild>
class in_memory_setup
{
    rpc::shared_ptr<yyy::i_host> i_host_ptr_;
    rpc::weak_ptr<yyy::i_host> local_host_ptr_;
    rpc::shared_ptr<yyy::i_example> i_example_ptr_;

    const bool has_enclave_ = false;
    bool use_host_in_child_ = UseHostInChild;

    std::atomic<uint64_t> zone_gen_ = 0;

    std::shared_ptr<coro::io_scheduler> io_scheduler_;
    bool error_has_occured_ = false;
public:
    virtual ~in_memory_setup() = default;

    rpc::shared_ptr<rpc::service> get_root_service() const {return nullptr;}
    bool get_has_enclave() const {return has_enclave_;}
    rpc::shared_ptr<yyy::i_example> get_example() const {return i_example_ptr_;}
    rpc::shared_ptr<yyy::i_host> get_host() const {return i_host_ptr_;}
    rpc::shared_ptr<yyy::i_host> get_local_host_ptr(){return local_host_ptr_.lock();}
    bool get_use_host_in_child() const {return use_host_in_child_;}

    std::shared_ptr<coro::io_scheduler> get_scheduler()const {return io_scheduler_;}
    bool error_has_occured()const {return error_has_occured_;}
    
        
    CORO_TASK(void) check_for_error(coro::task<bool> task)
    {       
        auto ret = CO_AWAIT task; 
        if(!ret)
        {
            error_has_occured_ = true;
        }
        CO_RETURN;
    }

    virtual void SetUp()
    {   
        io_scheduler_ = coro::io_scheduler::make_shared(
        coro::io_scheduler::options
        {
            .thread_strategy = coro::io_scheduler::thread_strategy_t::manual,
            .pool            = 
                coro::thread_pool::options
                {
                    .thread_count = 1,
                }
        });
        zone_gen = &zone_gen_;
        auto test_info = ::testing::UnitTest::GetInstance()->current_test_info();
#ifdef USE_RPC_TELEMETRY
        if(enable_telemetry_server)
            CREATE_TELEMETRY_SERVICE(rpc::host_telemetry_service, test_info->test_suite_name(), test_info->name(), "../../rpc_test_diagram/")
#endif            
        i_host_ptr_ = rpc::shared_ptr<yyy::i_host> (new host({++zone_gen_}));
        local_host_ptr_ = i_host_ptr_;
        i_example_ptr_ = rpc::shared_ptr<yyy::i_example> (new example(nullptr, use_host_in_child_ ? i_host_ptr_ : nullptr));
    }

    virtual void TearDown()
    {
        i_host_ptr_ = nullptr;
        i_example_ptr_ = nullptr;
        zone_gen = nullptr;
#ifdef USE_RPC_TELEMETRY
        RESET_TELEMETRY_SERVICE
#endif        
    }
};

class test_service_logger : public rpc::service_logger
{
    inline static std::shared_ptr<spdlog::logger> logr = spdlog::basic_logger_mt("basic_logger", "./conversation.txt", true);

public:

    test_service_logger() 
    {
        logr->info("************************************");
        logr->info("test {}", ::testing::UnitTest::GetInstance()->current_test_info()->name());
    }
    virtual ~test_service_logger()
    {
        
    }


    void before_send(rpc::caller_zone caller_zone_id, rpc::object object_id, rpc::interface_ordinal interface_id, rpc::method method_id, size_t in_size_, const char* in_buf_)
    {
        logr->info("caller_zone_id {} object_id {} interface_ordinal {} method {} data {}", caller_zone_id.id, object_id.id, interface_id.id, method_id.id, std::string_view(in_buf_, in_size_));
    }

    void after_send(rpc::caller_zone caller_zone_id, rpc::object object_id, rpc::interface_ordinal interface_id, rpc::method method_id, int ret, const std::vector<char>& out_buf_)
    {
        logr->info("caller_zone_id {} object_id {} interface_ordinal {} method {} ret {} data {}", caller_zone_id.id, object_id.id, interface_id.id, method_id.id, ret, std::string_view(out_buf_.data(), out_buf_.size()));
    }
};


template<bool UseHostInChild, bool RunStandardTests, bool CreateNewZoneThenCreateSubordinatedZone>
class inproc_setup
{
    rpc::shared_ptr<rpc::service> root_service_;
    rpc::shared_ptr<rpc::child_service> child_service_;
    rpc::shared_ptr<yyy::i_host> i_host_ptr_;
    rpc::weak_ptr<yyy::i_host> local_host_ptr_;
    rpc::shared_ptr<yyy::i_example> i_example_ptr_;

    const bool has_enclave_ = true;
    bool use_host_in_child_ = UseHostInChild;
    bool run_standard_tests_ = RunStandardTests;

    std::atomic<uint64_t> zone_gen_ = 0;
    
    std::shared_ptr<coro::io_scheduler> io_scheduler_;
    bool error_has_occured_ = false;
    
public:
    
    std::shared_ptr<coro::io_scheduler> get_scheduler()const {return io_scheduler_;}
    bool error_has_occured()const {return error_has_occured_;}
    
    virtual ~inproc_setup() = default;

    rpc::shared_ptr<rpc::service> get_root_service() const {return root_service_;}
    bool get_has_enclave() const {return has_enclave_;}
    bool is_enclave_setup() const {return false;}
    rpc::shared_ptr<yyy::i_example> get_example() const {return i_example_ptr_;}
    rpc::shared_ptr<yyy::i_host> get_host() const {return i_host_ptr_;}
    rpc::shared_ptr<yyy::i_host> get_local_host_ptr(){return local_host_ptr_.lock();}
    bool get_use_host_in_child() const {return use_host_in_child_;}
    
    CORO_TASK(void) check_for_error(coro::task<bool> task)
    {       
        auto ret = CO_AWAIT task; 
        if(!ret)
        {
            error_has_occured_ = true;
        }
        CO_RETURN;
    }
    
    CORO_TASK(bool) CoroSetUp()
    {
        zone_gen = &zone_gen_;
        auto test_info = ::testing::UnitTest::GetInstance()->current_test_info();
#ifdef USE_RPC_TELEMETRY
        if(enable_telemetry_server)
            CREATE_TELEMETRY_SERVICE(rpc::host_telemetry_service, test_info->test_suite_name(), test_info->name(), "../../rpc_test_diagram/")
#endif

        root_service_ = rpc::make_shared<rpc::service>("host", rpc::zone{++zone_gen_}, io_scheduler_);
        root_service_->add_service_logger(std::make_shared<test_service_logger>());
        current_host_service = root_service_;

        rpc::shared_ptr<yyy::i_host> hst(new host(root_service_->get_zone_id()));
        local_host_ptr_ = hst;//assign to weak ptr
        
        auto ret = CO_AWAIT root_service_->connect_to_zone<rpc::local_child_service_proxy<yyy::i_example, yyy::i_host>> (
            "main child"
            , {++zone_gen_}
            , hst
            , i_example_ptr_
            , [&](
                const rpc::shared_ptr<yyy::i_host>& host
                , rpc::shared_ptr<yyy::i_example>& new_example
                , const rpc::shared_ptr<rpc::child_service>& child_service_ptr) -> CORO_TASK(int)
            {
                i_host_ptr_ = host;
                child_service_ = child_service_ptr;
                example_import_idl_register_stubs(child_service_ptr);
                example_shared_idl_register_stubs(child_service_ptr);
                example_idl_register_stubs(child_service_ptr);
                new_example = rpc::shared_ptr<yyy::i_example>(new example(child_service_ptr, nullptr));  
                if(use_host_in_child_) 
                    CO_AWAIT new_example->set_host(host);
                CO_RETURN rpc::error::OK();
            });
        if(ret != rpc::error::OK())
        {
            CO_RETURN false;
        }
        CO_RETURN true;
    }
    
    virtual void SetUp()
    {
        io_scheduler_ = coro::io_scheduler::make_shared(
        coro::io_scheduler::options
        {
            .thread_strategy = coro::io_scheduler::thread_strategy_t::manual,
            .pool            = 
                coro::thread_pool::options
                {
                    .thread_count = 1,
                }
        });
        
        io_scheduler_->schedule(check_for_error(CoroSetUp()));
        io_scheduler_->process_events();

        //auto err_code = SYNC_WAIT();
        
        ASSERT_EQ(error_has_occured_, false);
    }

    CORO_TASK(void) CoroTearDown()
    {
        i_example_ptr_ = nullptr;
        i_host_ptr_ = nullptr;
        child_service_ = nullptr;
        root_service_ = nullptr;
        zone_gen = nullptr;
#ifdef USE_RPC_TELEMETRY
        RESET_TELEMETRY_SERVICE
#endif 
        CO_RETURN;       
    }
    
    virtual void TearDown()
    {
        io_scheduler_->schedule(CoroTearDown());
        io_scheduler_->process_events();
        // SYNC_WAIT(CoroTearDown());
    }

    CORO_TASK(rpc::shared_ptr<yyy::i_example>) create_new_zone()
    {        
        rpc::shared_ptr<yyy::i_host> hst;
        if(use_host_in_child_)
            hst = local_host_ptr_.lock();
            
        rpc::shared_ptr<yyy::i_example> example_relay_ptr;

        auto err_code = CO_AWAIT root_service_->connect_to_zone<rpc::local_child_service_proxy<yyy::i_example, yyy::i_host>>(
            "main child"
            , {++zone_gen_}
            , hst
            , example_relay_ptr
            , [&](
                const rpc::shared_ptr<yyy::i_host>& host
                , rpc::shared_ptr<yyy::i_example>& new_example
                , const rpc::shared_ptr<rpc::child_service>& child_service_ptr) -> CORO_TASK(int)
            {
                example_import_idl_register_stubs(child_service_ptr);
                example_shared_idl_register_stubs(child_service_ptr);
                example_idl_register_stubs(child_service_ptr);
                new_example = rpc::shared_ptr<yyy::i_example>(new example(child_service_ptr, nullptr));  
                if(use_host_in_child_) 
                    CO_AWAIT new_example->set_host(host);
                CO_RETURN rpc::error::OK();
            });
        
        if(CreateNewZoneThenCreateSubordinatedZone)
        {
            rpc::shared_ptr<yyy::i_example> new_ptr;
            if(CO_AWAIT example_relay_ptr->create_example_in_subordnate_zone(new_ptr, use_host_in_child_ ? hst : nullptr, ++zone_gen_) == rpc::error::OK())
            {
                CO_AWAIT example_relay_ptr->set_host(nullptr);
                example_relay_ptr = new_ptr;
            }
        }
        CO_RETURN example_relay_ptr;
    }
};

template<bool UseHostInChild, bool RunStandardTests, bool CreateNewZoneThenCreateSubordinatedZone>
class tcp_setup
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
    
    std::shared_ptr<coro::io_scheduler> get_scheduler()const {return io_scheduler_;}
    bool error_has_occured()const {return error_has_occured_;}
    
    virtual ~tcp_setup() = default;

    rpc::shared_ptr<rpc::service> get_root_service() const {return root_service_;}
    std::shared_ptr<rpc::tcp::listener<yyy::i_host, yyy::i_example>> get_peer_listener() const {return peer_listener_;};
    bool get_has_enclave() const {return has_enclave_;}
    bool is_enclave_setup() const {return false;}
    rpc::shared_ptr<yyy::i_example> get_example() const {return i_example_ptr_;}
    rpc::shared_ptr<yyy::i_host> get_host() const {return i_host_ptr_;}
    rpc::shared_ptr<yyy::i_host> get_local_host_ptr(){return local_host_ptr_.lock();}
    bool get_use_host_in_child() const {return use_host_in_child_;}
    
    CORO_TASK(void) check_for_error(coro::task<bool> task)
    {       
        auto ret = CO_AWAIT task; 
        if(!ret)
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
        if(enable_telemetry_server)
            CREATE_TELEMETRY_SERVICE(rpc::host_telemetry_service, test_info->test_suite_name(), test_info->name(), "../../rpc_test_diagram/")
#endif

        auto root_zone_id = rpc::zone{++zone_gen_};
        auto peer_zone_id = rpc::zone{++zone_gen_};
        root_service_ = rpc::make_shared<rpc::service>("host", root_zone_id, io_scheduler_);
        root_service_->add_service_logger(std::make_shared<test_service_logger>());

        peer_service_ = rpc::make_shared<rpc::service>("peer", peer_zone_id, io_scheduler_);
        peer_service_->add_service_logger(std::make_shared<test_service_logger>());
        
        peer_listener_ = std::make_shared<rpc::tcp::listener<yyy::i_host, yyy::i_example>>(
            [](const rpc::shared_ptr<yyy::i_host>& host, rpc::shared_ptr<yyy::i_example>& new_example,
               const rpc::shared_ptr<rpc::service>& child_service_ptr) -> CORO_TASK(int)
            {
                new_example = rpc::shared_ptr<yyy::i_example>(new marshalled_tests::example(child_service_ptr, host));
                CO_RETURN rpc::error::OK();
            },
            std::chrono::milliseconds(100000));
        peer_listener_->start_listening(peer_service_);
        
        current_host_service = root_service_;

        rpc::shared_ptr<yyy::i_host> hst(new host(root_service_->get_zone_id()));
        local_host_ptr_ = hst;//assign to weak ptr
        
        
        
        auto ret = CO_AWAIT root_service_->connect_to_zone<rpc::tcp::service_proxy> (
            "main child"
            , peer_zone_id.as_destination()
            , hst
            , i_example_ptr_
            , std::chrono::milliseconds(100000)
            , coro::net::tcp::client::options{
                                  .address = {coro::net::ip_address::from_string("127.0.0.1")},
                                  .port    = 8080,
        });
        if(ret != rpc::error::OK())
        {
            CO_RETURN false;
        }
        is_ready = true;
        CO_RETURN true;
    }
    
    virtual void SetUp()
    {
        has_stopped_ = false;
        io_scheduler_ = coro::io_scheduler::make_shared(
        coro::io_scheduler::options
        {
            .thread_strategy = coro::io_scheduler::thread_strategy_t::manual,
            .pool            = 
                coro::thread_pool::options
                {
                    .thread_count = 1,
                }
        });
        
        bool is_ready = false;
        io_scheduler_->schedule(check_for_error(CoroSetUp(is_ready)));
        while(!is_ready)
        {
            io_scheduler_->process_events(std::chrono::milliseconds(1));
        }

        //auto err_code = SYNC_WAIT();
        
        ASSERT_EQ(error_has_occured_, false);
    }

    CORO_TASK(void) CoroTearDown()
    {
        i_example_ptr_ = nullptr;
        i_host_ptr_ = nullptr;
        local_host_ptr_.reset();
        CO_AWAIT peer_listener_->stop_listening();
        peer_listener_.reset();
        while(!peer_service_->has_service_proxies())
            co_await io_scheduler_->schedule();
        while(!root_service_->has_service_proxies())
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
    
    virtual void TearDown()
    {
        io_scheduler_->schedule(CoroTearDown());
        while(has_stopped_ == false)
        {
            io_scheduler_->process_events(std::chrono::milliseconds(1000000));
        }
        // SYNC_WAIT(CoroTearDown());
    }

    /*CORO_TASK(rpc::shared_ptr<yyy::i_example>) create_new_zone()
    {        
        rpc::shared_ptr<yyy::i_host> hst;
        if(use_host_in_child_)
            hst = local_host_ptr_.lock();
            
        rpc::shared_ptr<yyy::i_example> example_relay_ptr;

        auto err_code = CO_AWAIT root_service_->connect_to_zone<rpc::local_child_service_proxy<yyy::i_example, yyy::i_host>>(
            "main child"
            , {++zone_gen_}
            , hst
            , example_relay_ptr
            , [&](
                const rpc::shared_ptr<yyy::i_host>& host
                , rpc::shared_ptr<yyy::i_example>& new_example
                , const rpc::shared_ptr<rpc::child_service>& child_service_ptr) -> CORO_TASK(int)
            {
                example_import_idl_register_stubs(child_service_ptr);
                example_shared_idl_register_stubs(child_service_ptr);
                example_idl_register_stubs(child_service_ptr);
                new_example = rpc::shared_ptr<yyy::i_example>(new example(child_service_ptr, nullptr));  
                if(use_host_in_child_) 
                    CO_AWAIT new_example->set_host(host);
                CO_RETURN rpc::error::OK();
            });
        
        if(CreateNewZoneThenCreateSubordinatedZone)
        {
            rpc::shared_ptr<yyy::i_example> new_ptr;
            if(CO_AWAIT example_relay_ptr->create_example_in_subordnate_zone(new_ptr, use_host_in_child_ ? hst : nullptr, ++zone_gen_) == rpc::error::OK())
            {
                CO_AWAIT example_relay_ptr->set_host(nullptr);
                example_relay_ptr = new_ptr;
            }
        }
        CO_RETURN example_relay_ptr;
    }*/
};

#ifdef BUILD_ENCLAVE
template<bool UseHostInChild, bool RunStandardTests, bool CreateNewZoneThenCreateSubordinatedZone>
class enclave_setup
{
    rpc::shared_ptr<rpc::service> root_service_;
    rpc::shared_ptr<yyy::i_host> i_host_ptr_;
    rpc::weak_ptr<yyy::i_host> local_host_ptr_;
    rpc::shared_ptr<yyy::i_example> i_example_ptr_;

    const bool has_enclave_ = true;
    bool use_host_in_child_ = UseHostInChild;
    bool run_standard_tests_ = RunStandardTests;

    std::atomic<uint64_t> zone_gen_ = 0;
public:
    virtual ~enclave_setup() = default;

    rpc::shared_ptr<rpc::service> get_root_service() const {return root_service_;}
    bool get_has_enclave() const {return has_enclave_;}
    bool is_enclave_setup() const {return true;}
    rpc::shared_ptr<yyy::i_host> get_local_host_ptr(){return local_host_ptr_.lock();}
    rpc::shared_ptr<yyy::i_example> get_example() const {return i_example_ptr_;}
    rpc::shared_ptr<yyy::i_host> get_host() const {return i_host_ptr_;}
    bool get_use_host_in_child() const {return use_host_in_child_;}
    
    virtual void SetUp()
    {
        zone_gen = &zone_gen_;
        auto test_info = ::testing::UnitTest::GetInstance()->current_test_info();
#ifdef USE_RPC_TELEMETRY
        if(enable_telemetry_server)
        {
            CREATE_TELEMETRY_SERVICE(rpc::host_telemetry_service, test_info->test_suite_name(), test_info->name(), "../../rpc_test_diagram/")
        }
#endif        
        root_service_ = rpc::make_shared<rpc::service>("host", rpc::zone{++zone_gen_});
        root_service_->add_service_logger(std::make_shared<test_service_logger>());
        example_import_idl_register_stubs(root_service_);
        example_shared_idl_register_stubs(root_service_);
        example_idl_register_stubs(root_service_);
        current_host_service = root_service_;
        
        i_host_ptr_ = rpc::shared_ptr<yyy::i_host> (new host(root_service_->get_zone_id()));
        local_host_ptr_ = i_host_ptr_;        


        auto err_code = root_service_->connect_to_zone<rpc::enclave_service_proxy>(
            "main child"
            , {++(*zone_gen)}
            , use_host_in_child_ ? i_host_ptr_ : nullptr
            , i_example_ptr_
            , enclave_path);

        ASSERT_ERROR_CODE(err_code);
    }

    virtual void TearDown()
    {
        i_example_ptr_ = nullptr;
        i_host_ptr_ = nullptr;
        root_service_ = nullptr;
        zone_gen = nullptr;
#ifdef USE_RPC_TELEMETRY
        RESET_TELEMETRY_SERVICE
#endif        
    }

    rpc::shared_ptr<yyy::i_example> create_new_zone()
    {
        rpc::shared_ptr<yyy::i_example> ptr;
        
        auto err_code = root_service_->connect_to_zone<rpc::enclave_service_proxy>(
            "main child"
            , {++zone_gen_}
            , use_host_in_child_ ? i_host_ptr_ : nullptr
            , ptr
            , enclave_path);
            
        if(err_code != rpc::error::OK())
            return nullptr;
        if(CreateNewZoneThenCreateSubordinatedZone)
        {
            rpc::shared_ptr<yyy::i_example> new_ptr;
            err_code = ptr->create_example_in_subordnate_zone(new_ptr, use_host_in_child_ ? i_host_ptr_ : nullptr, ++zone_gen_);
            if(err_code != rpc::error::OK())
                return nullptr;
            ptr = new_ptr;
        }
        return ptr;
    }
};
#endif

template <class T>
class type_test : 
    public testing::Test
{
    T lib_;
public:
    T& get_lib() {return lib_;}

    void SetUp() override {
        this->lib_.SetUp();
    }
    void TearDown() override {
        this->lib_.TearDown();
    }    
};

using local_implementations = ::testing::Types<
    in_memory_setup<false>, 
    in_memory_setup<true>, 
    inproc_setup<false, false, false>, 
    inproc_setup<false, false, true>, 
    inproc_setup<false, true, false>, 
    inproc_setup<false, true, true>, 
    inproc_setup<true, false, false>, 
    inproc_setup<true, false, true>, 
    inproc_setup<true, true, false>, 
    inproc_setup<true, true, true>,
    tcp_setup<false, false, false>, 
    tcp_setup<false, false, true>, 
    tcp_setup<false, true, false>, 
    tcp_setup<false, true, true>, 
    tcp_setup<true, false, false>, 
    tcp_setup<true, false, true>, 
    tcp_setup<true, true, false>, 
    tcp_setup<true, true, true>

#ifdef BUILD_ENCLAVE
    ,
    enclave_setup<false, false, false>, 
    enclave_setup<false, false, true>, 
    enclave_setup<false, true, false>, 
    enclave_setup<false, true, true>, 
    enclave_setup<true, false, false>, 
    enclave_setup<true, false, true>, 
    enclave_setup<true, true, false>, 
    enclave_setup<true, true, true>
#endif
    >;
TYPED_TEST_SUITE(type_test, local_implementations);

TYPED_TEST(type_test, initialisation_test)
{
}

template <class T>
CORO_TASK(bool) coro_standard_tests(bool& is_ready, T& lib)
{
    auto root_service = lib.get_root_service();

    rpc::zone zone_id;
    if(root_service)
        zone_id = root_service->get_zone_id();
    else
        zone_id = {0};

    foo f(zone_id);
    
    co_await lib.check_for_error(standard_tests(f, lib.get_has_enclave()));
    is_ready = true;
    CO_RETURN !lib.error_has_occured();
}

TYPED_TEST(type_test, standard_tests)
{
    bool is_ready = false;
    auto& lib = this->get_lib();
    auto root_service = lib.get_root_service();
    root_service->get_scheduler()->schedule(lib.check_for_error(coro_standard_tests(is_ready, lib)));
    while(!is_ready)
    {
        root_service->get_scheduler()->process_events(std::chrono::milliseconds(1));
    }  
    ASSERT_EQ(lib.error_has_occured(), false);
}

TEST_RETURN_VAL dyanmic_cast_tests(rpc::shared_ptr<rpc::service> root_service)
{
    rpc::zone zone_id;
    if(root_service)
        zone_id = root_service->get_zone_id();
    else
        zone_id = {0};
    auto f = rpc::shared_ptr<xxx::i_foo>(new foo(zone_id));

    rpc::shared_ptr<xxx::i_baz> baz;
    CORO_ASSERT_EQ(CO_AWAIT f->create_baz_interface(baz), 0);
    CORO_ASSERT_EQ(CO_AWAIT f->call_baz_interface(nullptr), 0); // feed in a nullptr
    CORO_ASSERT_EQ(CO_AWAIT f->call_baz_interface(baz), 0);     // feed back to the implementation

    {
        //test for identity
        auto x = CO_AWAIT rpc::dynamic_pointer_cast<xxx::i_baz>(baz);
        CORO_ASSERT_NE(x, nullptr);
        CORO_ASSERT_EQ(x, baz);
        auto y = CO_AWAIT rpc::dynamic_pointer_cast<xxx::i_bar>(baz);
        CORO_ASSERT_NE(y, nullptr);
        CO_AWAIT y->do_something_else(1);
        CORO_ASSERT_NE(y, nullptr);
        auto z = CO_AWAIT rpc::dynamic_pointer_cast<xxx::i_foo>(baz);
        CORO_ASSERT_EQ(z, nullptr);
    }
    //retest
    {
        auto x = CO_AWAIT rpc::dynamic_pointer_cast<xxx::i_baz>(baz);
        CORO_ASSERT_NE(x, nullptr);
        CORO_ASSERT_EQ(x, baz);
        auto y = CO_AWAIT rpc::dynamic_pointer_cast<xxx::i_bar>(baz);
        CORO_ASSERT_NE(y, nullptr);
        CO_AWAIT y->do_something_else(1);
        CORO_ASSERT_NE(y, nullptr);
        auto z = CO_AWAIT rpc::dynamic_pointer_cast<xxx::i_foo>(baz);
        CORO_ASSERT_EQ(z, nullptr);
    }
    TEST_RETURN_SUCCESFUL;
}

TYPED_TEST(type_test, dyanmic_cast_tests)
{
    auto& lib = this->get_lib();
    auto root_service = lib.get_root_service();
    // TEST_SYNC_WAIT(dyanmic_cast_tests(root_service));
    
    lib.get_scheduler()->schedule(lib.check_for_error(dyanmic_cast_tests(root_service)));
    lib.get_scheduler()->process_events();

    ASSERT_EQ(lib.error_has_occured(), false);
}


template <class T>
using remote_type_test = type_test<T>;


typedef Types<
    //inproc_setup<false, false, false>, 

    inproc_setup<true, false, false>, 
    inproc_setup<true, false, true>, 
    inproc_setup<true, true, false>, 
    inproc_setup<true, true, true>,
    tcp_setup<true, false, false>, 
    tcp_setup<true, false, true>, 
    tcp_setup<true, true, false>, 
    tcp_setup<true, true, true>

#ifdef BUILD_ENCLAVE
    ,
    enclave_setup<true, false, false>, 
    enclave_setup<true, false, true>, 
    enclave_setup<true, true, false>, 
    enclave_setup<true, true, true>
#endif
> remote_implementations;
TYPED_TEST_SUITE(remote_type_test, remote_implementations);



template <class T>
CORO_TASK(void) coro_remote_standard_tests(bool& is_ready, T& lib)
{
    rpc::shared_ptr<xxx::i_foo> i_foo_ptr;
    auto ret = co_await lib.get_example()->create_foo(i_foo_ptr);
    if(ret != rpc::error::OK())
    {
        LOG_CSTR("failed create_foo");
        CO_RETURN;
    }
    co_await lib.check_for_error(standard_tests(*i_foo_ptr, lib.get_has_enclave()));
    is_ready = true;
}

TYPED_TEST(remote_type_test, remote_standard_tests)
{    
    bool is_ready = false;
    auto& lib = this->get_lib();
    auto root_service = lib.get_root_service();
    root_service->get_scheduler()->schedule(coro_remote_standard_tests(is_ready, lib));
    while(!is_ready)
    {
        root_service->get_scheduler()->process_events(std::chrono::milliseconds(1));
    }  
    ASSERT_EQ(lib.error_has_occured(), false);
}

// TYPED_TEST(remote_type_test, multithreaded_standard_tests)
// {
//     if(!enable_multithreaded_tests || lib.is_enclave_setup())
//     {
//         GTEST_SKIP() << "multithreaded tests are skipped";
//         return;
//     }

//     rpc::shared_ptr<xxx::i_foo> i_foo_ptr;
//     ASSERT_EQ(lib.get_example()->create_foo(i_foo_ptr), 0);
    
//     std::vector<std::thread> threads(lib.is_enclave_setup() ? 3 : 100);
//     for(auto& thread_target : threads)
//     {
//         thread_target = std::thread([&](){
//             standard_tests(*i_foo_ptr, true);   
//         });
//     }
//     for(auto& thread_target : threads)
//     {
//         thread_target.join();
//     }
// }

// TYPED_TEST(remote_type_test, multithreaded_standard_tests_with_and_foos)
// {
//     if(!enable_multithreaded_tests || lib.is_enclave_setup())
//     {
//         GTEST_SKIP() << "multithreaded tests are skipped";
//         return;
//     }

//     std::vector<std::thread> threads(lib.is_enclave_setup() ? 3 : 100);
//     for(auto& thread_target : threads)
//     {
//         thread_target = std::thread([&](){
//             rpc::shared_ptr<xxx::i_foo> i_foo_ptr;
//             ASSERT_EQ(lib.get_example()->create_foo(i_foo_ptr), 0);
//             standard_tests(*i_foo_ptr, true);   
//         });
//     }
//     for(auto& thread_target : threads)
//     {
//         thread_target.join();
//     }
// }

// TYPED_TEST(remote_type_test, remote_tests)
// {    
//     auto root_service = lib.get_root_service();
//     rpc::zone zone_id;
//     if(root_service)
//         zone_id = root_service->get_zone_id();
//     else
//         zone_id = {0};
//     remote_tests(lib.get_use_host_in_child(), lib.get_example(), zone_id);
// }

// TYPED_TEST(remote_type_test, multithreaded_remote_tests)
// {
//     if(!enable_multithreaded_tests || lib.is_enclave_setup())
//     {
//         GTEST_SKIP() << "multithreaded tests are skipped";
//         return;
//     }

//     auto root_service = lib.get_root_service();
//     rpc::zone zone_id;
//     if(root_service)
//         zone_id = root_service->get_zone_id();
//     else
//         zone_id = {0};
//     std::vector<std::thread> threads(lib.is_enclave_setup() ? 3 : 100);
//     for(auto& thread_target : threads)
//     {
//         thread_target = std::thread([&](){
//             remote_tests(lib.get_use_host_in_child(), lib.get_example(), zone_id);
//         });
//     }
//     for(auto& thread_target : threads)
//     {
//         thread_target.join();
//     }
// }

// TYPED_TEST(remote_type_test, create_new_zone)
// {
//     auto example_relay_ptr = lib.create_new_zone();
//     example_relay_ptr->set_host(nullptr);
// }

// TYPED_TEST(remote_type_test, multithreaded_create_new_zone)
// {
//     if(!enable_multithreaded_tests || lib.is_enclave_setup())
//     {
//         GTEST_SKIP() << "multithreaded tests are skipped";
//         return;
//     }

//     std::vector<std::thread> threads(lib.is_enclave_setup() ? 3 : 100);
//     for(auto& thread_target : threads)
//     {
//         thread_target = std::thread([&](){
//             auto example_relay_ptr = lib.create_new_zone();
//             example_relay_ptr->set_host(nullptr);        
//         });
//     }
//     for(auto& thread_target : threads)
//     {
//         thread_target.join();
//     }
// }

// TYPED_TEST(remote_type_test, create_new_zone_releasing_host_then_running_on_other_enclave)
// {
//     rpc::shared_ptr<xxx::i_foo> i_foo_relay_ptr;
//     auto example_relay_ptr = lib.create_new_zone();
//     example_relay_ptr->create_foo(i_foo_relay_ptr);
//     standard_tests(*i_foo_relay_ptr, true);

//     //rpc::shared_ptr<xxx::i_foo> i_foo_ptr;
//     //ASSERT_EQ(lib.get_example()->create_foo(i_foo_ptr), 0);
//     example_relay_ptr = nullptr;
//     //standard_tests(*i_foo_ptr, true);    
// }

// TYPED_TEST(remote_type_test, multithreaded_create_new_zone_releasing_host_then_running_on_other_enclave)
// {
//     if(!enable_multithreaded_tests || lib.is_enclave_setup())
//     {
//         GTEST_SKIP() << "multithreaded tests are skipped";
//         return;
//     }

//     std::vector<std::thread> threads(lib.is_enclave_setup() ? 3 : 100);
//     for(auto& thread_target : threads)
//     {
//         thread_target = std::thread([&](){
//             rpc::shared_ptr<xxx::i_foo> i_foo_relay_ptr;
//             auto example_relay_ptr = lib.create_new_zone();
//             example_relay_ptr->create_foo(i_foo_relay_ptr);
//             standard_tests(*i_foo_relay_ptr, true);
//         });
//     }
//     for(auto& thread_target : threads)
//     {
//         thread_target.join();
//     }
// }

// TYPED_TEST(remote_type_test, dyanmic_cast_tests)
// {
//     rpc::shared_ptr<xxx::i_foo> i_foo_ptr;
//     ASSERT_EQ(lib.get_example()->create_foo(i_foo_ptr), 0);

//     rpc::shared_ptr<xxx::i_baz> baz;
//     i_foo_ptr->create_baz_interface(baz);
//     i_foo_ptr->call_baz_interface(nullptr); // feed in a nullptr
//     i_foo_ptr->call_baz_interface(baz);     // feed back to the implementation

//     auto x = rpc::dynamic_pointer_cast<xxx::i_baz>(baz);
//     ASSERT_NE(x, nullptr);
//     auto y = rpc::dynamic_pointer_cast<xxx::i_bar>(baz);
//     ASSERT_NE(y, nullptr);
//     y->do_something_else(1);
//     auto z = rpc::dynamic_pointer_cast<xxx::i_foo>(baz);
//     ASSERT_EQ(z, nullptr);
// }

// TYPED_TEST(remote_type_test, bounce_baz_between_two_interfaces)
// {
//     rpc::shared_ptr<xxx::i_foo> i_foo_ptr;
//     ASSERT_EQ(lib.get_example()->create_foo(i_foo_ptr), 0);

//     rpc::shared_ptr<xxx::i_foo> i_foo_relay_ptr;
//     auto example_relay_ptr = lib.create_new_zone();
//     example_relay_ptr->create_foo(i_foo_relay_ptr);

//     rpc::shared_ptr<xxx::i_baz> baz;
//     i_foo_ptr->create_baz_interface(baz);
//     i_foo_relay_ptr->call_baz_interface(baz);
// }

// TYPED_TEST(remote_type_test, multithreaded_bounce_baz_between_two_interfaces)
// {
//     if(!enable_multithreaded_tests || lib.is_enclave_setup())
//     {
//         GTEST_SKIP() << "multithreaded tests are skipped";
//         return;
//     }

    
//     std::vector<std::thread> threads(lib.is_enclave_setup() ? 3 : 100);
//     for(auto& thread_target : threads)
//     {
//         thread_target = std::thread([&](){
//             rpc::shared_ptr<xxx::i_foo> i_foo_ptr;
//             ASSERT_EQ(lib.get_example()->create_foo(i_foo_ptr), 0);

//             rpc::shared_ptr<xxx::i_foo> i_foo_relay_ptr;
//             auto example_relay_ptr = lib.create_new_zone();
//             example_relay_ptr->create_foo(i_foo_relay_ptr);

//             rpc::shared_ptr<xxx::i_baz> baz;
//             i_foo_ptr->create_baz_interface(baz);
//             i_foo_relay_ptr->call_baz_interface(baz);
//         });
//     }
//     for(auto& thread_target : threads)
//     {
//         thread_target.join();
//     }
// }

// // check for null
// TYPED_TEST(remote_type_test, check_for_null_interface)
// {
//     rpc::shared_ptr<xxx::i_foo> i_foo_ptr;
//     ASSERT_EQ(lib.get_example()->create_foo(i_foo_ptr), 0);
//     rpc::shared_ptr<xxx::i_baz> c;
//     i_foo_ptr->get_interface(c);
//     RPC_ASSERT(c == nullptr);
// }

// TYPED_TEST(remote_type_test, check_for_multiple_sets)
// {
//     if(!lib.get_use_host_in_child())
//         return;

//     rpc::shared_ptr<xxx::i_foo> i_foo_ptr;
//     ASSERT_EQ(lib.get_example()->create_foo(i_foo_ptr), 0);
    
//     auto zone_id = i_foo_ptr->query_proxy_base()->get_object_proxy()->get_service_proxy()->get_zone_id();

//     auto b = rpc::make_shared<marshalled_tests::baz>(zone_id);
//     // set
//     i_foo_ptr->set_interface(b);
//     // reset
//     i_foo_ptr->set_interface(nullptr);
//     // set
//     i_foo_ptr->set_interface(b);
//     // reset
//     i_foo_ptr->set_interface(nullptr);
// }

// TYPED_TEST(remote_type_test, check_for_interface_storage)
// {
//     if(!lib.get_use_host_in_child())
//         return;
        
//     rpc::shared_ptr<xxx::i_foo> i_foo_ptr;
//     ASSERT_EQ(lib.get_example()->create_foo(i_foo_ptr), 0);

//     rpc::shared_ptr<xxx::i_baz> c;
//     auto zone_id = i_foo_ptr->query_proxy_base()->get_object_proxy()->get_service_proxy()->get_zone_id();

//     auto b = rpc::make_shared<marshalled_tests::baz>(zone_id);
//     i_foo_ptr->set_interface(b);
//     i_foo_ptr->get_interface(c);
//     i_foo_ptr->set_interface(nullptr);
//     RPC_ASSERT(b == c);
// }

// TYPED_TEST(remote_type_test, check_for_set_multiple_inheritance)
// {
//     if(!lib.get_use_host_in_child())
//         return;
//     auto proxy = lib.get_example()->query_proxy_base();        
//     auto ret = lib.get_example()->give_interface(
//         rpc::shared_ptr<xxx::i_baz>(new multiple_inheritance(proxy->get_object_proxy()->get_service_proxy()->get_zone_id())));
//     RPC_ASSERT(ret == rpc::error::OK());
// }

// #ifdef BUILD_ENCLAVE
// TYPED_TEST(remote_type_test, host_test)
// {    
//     auto root_service = lib.get_root_service();
//     rpc::zone zone_id;
//     if(root_service)
//         zone_id = root_service->get_zone_id();
//     else
//         zone_id = {0};
//     auto h = rpc::make_shared<host>(zone_id);

//     rpc::shared_ptr<yyy::i_example> target;
//     rpc::shared_ptr<yyy::i_example> target2;
//     h->create_enclave(target);
//     ASSERT_NE(target, nullptr);

//     ASSERT_EQ(h->set_app("target", target), rpc::error::OK());
//     ASSERT_EQ(h->look_up_app("target", target2), rpc::error::OK());
//     ASSERT_EQ(h->unload_app("target"), rpc::error::OK());
//     target = nullptr;
//     target2 = nullptr;

// }

// TYPED_TEST(remote_type_test, check_for_call_enclave_zone)
// {
//     if(!lib.get_use_host_in_child())
//         return;

//     auto root_service = lib.get_root_service();
//     rpc::zone zone_id;
//     if(root_service)
//         zone_id = root_service->get_zone_id();
//     else
//         zone_id = {0};
        
//     auto h = rpc::make_shared<host>(zone_id);
//     auto ret = lib.get_example()->call_create_enclave_val(h);
//     RPC_ASSERT(ret == rpc::error::OK());
// }
// #endif

// TYPED_TEST(remote_type_test, check_sub_subordinate)
// {
//     auto& lib = lib;
//     if(!lib.get_use_host_in_child())
//         return;

//     rpc::shared_ptr<yyy::i_example> new_zone;
//     ASSERT_EQ(lib.get_example()->create_example_in_subordnate_zone(new_zone, lib.get_local_host_ptr(), ++(*zone_gen)), rpc::error::OK()); //second level

//     rpc::shared_ptr<yyy::i_example> new_new_zone;
//     ASSERT_EQ(new_zone->create_example_in_subordnate_zone(new_new_zone, lib.get_local_host_ptr(), ++(*zone_gen)), rpc::error::OK()); //third level
// }

// TYPED_TEST(remote_type_test, multithreaded_check_sub_subordinate)
// {
//     if(!enable_multithreaded_tests || lib.is_enclave_setup())
//     {
//         GTEST_SKIP() << "multithreaded tests are skipped";
//         return;
//     }

//     auto& lib = lib;
//     if(!lib.get_use_host_in_child())
//         return;
//     std::vector<std::thread> threads(lib.is_enclave_setup() ? 3 : 100);
//     for(auto& thread_target : threads)
//     {
//         thread_target = std::thread([&](){
//             rpc::shared_ptr<yyy::i_example> new_zone;
//             ASSERT_EQ(lib.get_example()->create_example_in_subordnate_zone(new_zone, lib.get_local_host_ptr(), ++(*zone_gen)), rpc::error::OK()); //second level

//             rpc::shared_ptr<yyy::i_example> new_new_zone;
//             ASSERT_EQ(new_zone->create_example_in_subordnate_zone(new_new_zone, lib.get_local_host_ptr(), ++(*zone_gen)), rpc::error::OK()); //third level
//         });
//     }
//     for(auto& thread_target : threads)
//     {
//         thread_target.join();
//     }
// }

// TYPED_TEST(remote_type_test, send_interface_back)
// {
//     auto& lib = lib;
//     if(!lib.get_use_host_in_child())
//         return;

//     rpc::shared_ptr<xxx::i_baz> output;

//     rpc::shared_ptr<yyy::i_example> new_zone;
//     //lib.i_example_ptr_ //first level
//     ASSERT_EQ(lib.get_example()->create_example_in_subordnate_zone(new_zone, lib.get_local_host_ptr(), ++(*zone_gen)), rpc::error::OK()); //second level

//     rpc::shared_ptr<xxx::i_baz> new_baz;
//     new_zone->create_baz(new_baz);

//     ASSERT_EQ(lib.get_example()->send_interface_back(new_baz, output), rpc::error::OK());
//     ASSERT_EQ(new_baz, output);
// }

// TYPED_TEST(remote_type_test, two_zones_get_one_to_lookup_other)
// {
//     auto root_service = lib.get_root_service();

//     rpc::zone zone_id;
//     if(root_service)
//         zone_id = root_service->get_zone_id();
//     else
//         zone_id = {0};
//     auto h = lib.get_local_host_ptr();  
//     auto ex = lib.get_example();      
    
//     auto enclaveb = lib.create_new_zone();
//     enclaveb->set_host(h);
//     ASSERT_EQ(h->set_app("enclaveb", enclaveb), rpc::error::OK());

//     ex->call_host_look_up_app_not_return("enclaveb", false);
    
//     enclaveb->set_host(nullptr);
// }
// TYPED_TEST(remote_type_test, multithreaded_two_zones_get_one_to_lookup_other)
// {
//     if(!enable_multithreaded_tests || lib.is_enclave_setup())
//     {
//         GTEST_SKIP() << "multithreaded tests are skipped";
//         return;
//     }

//     auto root_service = lib.get_root_service();

//     rpc::zone zone_id;
//     if(root_service)
//         zone_id = root_service->get_zone_id();
//     else
//         zone_id = {0};
//     auto h = rpc::make_shared<host>(zone_id);        
    
//     auto enclavea = lib.create_new_zone();
//     enclavea->set_host(h);
//     ASSERT_EQ(h->set_app("enclavea", enclavea), rpc::error::OK());
    
//     auto enclaveb = lib.create_new_zone();
//     enclaveb->set_host(h);
//     ASSERT_EQ(h->set_app("enclaveb", enclaveb), rpc::error::OK());
    
//     const auto thread_size = 3;
//     std::array<std::thread, thread_size> threads;
//     for(auto& thread : threads)
//     {        
//         thread = std::thread([&](){
//             enclavea->call_host_look_up_app_not_return("enclaveb", true);
//         });
//     }
//     for(auto& thread : threads)
//     {        
//         thread.join();
//     }
//     enclavea->set_host(nullptr);
//     enclaveb->set_host(nullptr);
// }

// TYPED_TEST(remote_type_test, check_identity)
// {
//     auto& lib = lib;
//     if(!lib.get_use_host_in_child())
//         return;

//     rpc::shared_ptr<xxx::i_baz> output;

//     rpc::shared_ptr<yyy::i_example> new_zone;
//     //lib.i_example_ptr_ //first level
//     ASSERT_EQ(lib.get_example()->create_example_in_subordnate_zone(new_zone, lib.get_local_host_ptr(), ++(*zone_gen)), rpc::error::OK()); //second level

//     rpc::shared_ptr<yyy::i_example> new_new_zone;
//     ASSERT_EQ(new_zone->create_example_in_subordnate_zone(new_new_zone, lib.get_local_host_ptr(), ++(*zone_gen)), rpc::error::OK()); //third level

//     auto new_zone_fork = lib.create_new_zone();//second level

//     rpc::shared_ptr<xxx::i_baz> new_baz;
//     new_zone->create_baz(new_baz);
    
//     rpc::shared_ptr<xxx::i_baz> new_new_baz;
//     new_new_zone->create_baz(new_new_baz);

//     rpc::shared_ptr<xxx::i_baz> new_baz_fork;
//     new_zone_fork->create_baz(new_baz_fork);

//     // topology looks like this now flinging bazes around these nodes to ensure that the identity of bazes is the same
//     // *4                           #
//     //  \                           #
//     //   *3                         #
//     //    \                         #
//     //     *2  *5                   #
//     //      \ /                     #
//     //       1                      #


//     auto proxy = lib.get_example()->query_proxy_base();
//     auto base_baz = rpc::shared_ptr<xxx::i_baz>(new baz(proxy->get_object_proxy()->get_service_proxy()->get_zone_id()));
//     auto input = base_baz;

//     ASSERT_EQ(lib.get_example()->send_interface_back(input, output), rpc::error::OK());
//     ASSERT_EQ(input, output);
    
//     ASSERT_EQ(new_zone->send_interface_back(input, output), rpc::error::OK());
//     ASSERT_EQ(input, output);    

//     ASSERT_EQ(new_new_zone->send_interface_back(input, output), rpc::error::OK());
//     ASSERT_EQ(input, output); 

//     input = new_baz;

//     ASSERT_EQ(lib.get_example()->send_interface_back(input, output), rpc::error::OK());
//     ASSERT_EQ(input, output);
    
//     ASSERT_EQ(new_zone->send_interface_back(input, output), rpc::error::OK());
//     ASSERT_EQ(input, output);  

//     ASSERT_EQ(new_new_zone->send_interface_back(input, output), rpc::error::OK());
//     ASSERT_EQ(input, output);    


//     input = new_new_baz;

//     ASSERT_EQ(lib.get_example()->send_interface_back(input, output), rpc::error::OK());
//     ASSERT_EQ(input, output);
    
//     ASSERT_EQ(new_zone->send_interface_back(input, output), rpc::error::OK());
//     ASSERT_EQ(input, output);    

//     ASSERT_EQ(new_new_zone->send_interface_back(input, output), rpc::error::OK());
//     ASSERT_EQ(input, output);    



//     input = new_baz_fork;
    
//     // *z2   *i5                 #
//     //  \  /                     #
//     //   h1                      #

//     ASSERT_EQ(lib.get_example()->send_interface_back(input, output), rpc::error::OK());
//     ASSERT_EQ(input, output);
    
//     // *z3                       #
//     //  \                        #
//     //   *2   *i5                #
//     //    \ /                    #
//     //     h1                    #
    
//     ASSERT_EQ(new_zone->send_interface_back(input, output), rpc::error::OK());
//     ASSERT_EQ(input, output);   

        
//     // *z4                          #
//     //  \                           #
//     //   *3                         #
//     //    \                         #
//     //     *2  *i5                  #
//     //      \ /                     #
//     //       h1                     #

//     ASSERT_EQ(new_new_zone->send_interface_back(input, output), rpc::error::OK());
//     ASSERT_EQ(input, output);

// }

// template <class T>
// class type_test_with_host : 
//     public type_test<T>
// {
    
// };

// typedef Types<
//     inproc_setup<true, false, false>, 
//     inproc_setup<true, false, true>, 
//     inproc_setup<true, true, false>, 
//     inproc_setup<true, true, true>

// #ifdef BUILD_ENCLAVE
//     ,
//     enclave_setup<true, false, false>, 
//     enclave_setup<true, false, true>, 
//     enclave_setup<true, true, false>, 
//     enclave_setup<true, true, true>
// #endif    
//     > type_test_with_host_implementations;
// TYPED_TEST_SUITE(type_test_with_host, type_test_with_host_implementations);


// #ifdef BUILD_ENCLAVE
// TYPED_TEST(type_test_with_host, call_host_create_enclave_and_throw_away)
// {  
//     bool run_standard_tests = false;
//     ASSERT_EQ(lib.get_example()->call_host_create_enclave_and_throw_away(run_standard_tests), rpc::error::OK());
// }

// TYPED_TEST(type_test_with_host, call_host_create_enclave)
// {  
//     bool run_standard_tests = false;
//     rpc::shared_ptr<yyy::i_example> target;

//     ASSERT_EQ(lib.get_example()->call_host_create_enclave(target, run_standard_tests), rpc::error::OK());
//     ASSERT_NE(target, nullptr);
// }
// #endif

// TYPED_TEST(type_test_with_host, look_up_app_and_return_with_nothing)
// {  
//     bool run_standard_tests = false;
//     rpc::shared_ptr<yyy::i_example> target;

//     ASSERT_EQ(lib.get_example()->call_host_look_up_app("target", target, run_standard_tests), rpc::error::OK());
//     ASSERT_EQ(target, nullptr);
// }


// TYPED_TEST(type_test_with_host, call_host_unload_app_not_there)
// {  
//     ASSERT_EQ(lib.get_example()->call_host_unload_app("target"), rpc::error::OK());
// }


// #ifdef BUILD_ENCLAVE
// TYPED_TEST(type_test_with_host, call_host_look_up_app_unload_app)
// {  
//     bool run_standard_tests = false;
//     rpc::shared_ptr<yyy::i_example> target;

//     ASSERT_EQ(lib.get_example()->call_host_create_enclave(target, run_standard_tests), rpc::error::OK());
//     ASSERT_NE(target, nullptr);

//     ASSERT_EQ(lib.get_example()->call_host_set_app("target", target, run_standard_tests), rpc::error::OK());
//     ASSERT_EQ(lib.get_example()->call_host_unload_app("target"), rpc::error::OK());
//     target = nullptr;
// }


// TYPED_TEST(type_test_with_host, call_host_look_up_app_not_return)
// {  
//     bool run_standard_tests = false;
//     rpc::shared_ptr<yyy::i_example> target;

//     ASSERT_EQ(lib.get_example()->call_host_create_enclave(target, run_standard_tests), rpc::error::OK());
//     ASSERT_NE(target, nullptr);

//     ASSERT_EQ(lib.get_example()->call_host_set_app("target", target, run_standard_tests), rpc::error::OK());
//     ASSERT_EQ(lib.get_example()->call_host_look_up_app_not_return("target", run_standard_tests), rpc::error::OK());
//     ASSERT_EQ(lib.get_example()->call_host_unload_app("target"), rpc::error::OK());
//     target = nullptr;
// }

// TYPED_TEST(type_test_with_host, create_store_fetch_delete)
// {  
//     bool run_standard_tests = false;
//     rpc::shared_ptr<yyy::i_example> target;
//     rpc::shared_ptr<yyy::i_example> target2;

//     ASSERT_EQ(lib.get_example()->call_host_create_enclave(target, run_standard_tests), rpc::error::OK());
//     ASSERT_NE(target, nullptr);

//     ASSERT_EQ(lib.get_example()->call_host_set_app("target", target, run_standard_tests), rpc::error::OK());
//     ASSERT_EQ(lib.get_example()->call_host_look_up_app("target", target2, run_standard_tests), rpc::error::OK());
//     ASSERT_EQ(lib.get_example()->call_host_unload_app("target"), rpc::error::OK());
//     ASSERT_EQ(target, target2);
//     target = nullptr;
//     target2 = nullptr;
// }

// TYPED_TEST(type_test_with_host, create_store_not_return_delete)
// {  
//     bool run_standard_tests = false;
//     rpc::shared_ptr<yyy::i_example> target;

//     ASSERT_EQ(lib.get_example()->call_host_create_enclave(target, run_standard_tests), rpc::error::OK());
//     ASSERT_NE(target, nullptr);

//     ASSERT_EQ(lib.get_example()->call_host_set_app("target", target, run_standard_tests), rpc::error::OK());
//     ASSERT_EQ(lib.get_example()->call_host_look_up_app_not_return_and_delete("target", run_standard_tests), rpc::error::OK());
//     target = nullptr;
// }

// TYPED_TEST(type_test_with_host, create_store_delete)
// {  
//     bool run_standard_tests = false;
//     rpc::shared_ptr<yyy::i_example> target;
//     rpc::shared_ptr<yyy::i_example> target2;

//     ASSERT_EQ(lib.get_example()->call_host_create_enclave(target, run_standard_tests), rpc::error::OK());
//     ASSERT_NE(target, nullptr);

//     ASSERT_EQ(lib.get_example()->call_host_set_app("target", target, run_standard_tests), rpc::error::OK());
//     ASSERT_EQ(lib.get_example()->call_host_look_up_app_and_delete("target", target2, run_standard_tests), rpc::error::OK());
//     ASSERT_EQ(target, target2);
//     target = nullptr;
//     target2 = nullptr;
// }
// #endif

// TYPED_TEST(type_test_with_host, create_subordinate_zone)
// {  
//     rpc::shared_ptr<yyy::i_example> target;
//     ASSERT_EQ(lib.get_example()->create_example_in_subordnate_zone(target, lib.get_local_host_ptr(), ++(*zone_gen)), rpc::error::OK());
// }


// TYPED_TEST(type_test_with_host, create_subordinate_zone_and_set_in_host)
// {  
//     ASSERT_EQ(lib.get_example()->create_example_in_subordnate_zone_and_set_in_host(++(*zone_gen), "foo", lib.get_local_host_ptr()), rpc::error::OK());
//     rpc::shared_ptr<yyy::i_example> target;
//     lib.get_host()->look_up_app("foo", target);
//     lib.get_host()->unload_app("foo");
//     target->set_host(nullptr);
// }


// static_assert(rpc::id<std::string>::get(rpc::VERSION_2) == rpc::STD_STRING_ID);

// static_assert(rpc::id<xxx::test_template<std::string>>::get(rpc::VERSION_2) == 0xAFFFFFEB79FBFBFB);
// static_assert(rpc::id<xxx::test_template_without_params_in_id<std::string>>::get(rpc::VERSION_2) == 0x62C84BEB07545E2B);
// static_assert(rpc::id<xxx::test_template_use_legacy_empty_template_struct_id<std::string>>::get(rpc::VERSION_2) == 0x2E7E56276F6E36BE);
// static_assert(rpc::id<xxx::test_template_use_old<std::string>>::get(rpc::VERSION_2) == 0x66D71EBFF8C6FFA7);
// 
