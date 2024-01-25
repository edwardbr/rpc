#include <iostream>
#include <unordered_map>
#include <string_view>
#include <thread>

#include <sgx_urts.h>
#include <sgx_quote.h>
#include <sgx_capable.h>
#include <sgx_uae_epid.h>
#include <sgx_eid.h>
#include "untrusted/enclave_marshal_test_u.h"

#include <common/foo_impl.h>
#include <common/tests.h>
#include <common/enclave_service_proxy.h>

#include <example/example.h>

#include <rpc/basic_service_proxies.h>
#include <host_telemetry_service.h>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include <clipp.h>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>


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
std::string enclave_path_v1 = "./marshal_test_enclave_v1.signed.dll";
#else         // Linux
std::string enclave_path = "./libmarshal_test_enclave.signed.so";
std::string enclave_path_v1 = "./libmarshal_test_enclave_v1.signed.so";
#endif
bool enable_telemetry_server = true;

rpc::weak_ptr<rpc::service> current_host_service;

const rpc::i_telemetry_service* telemetry_service = nullptr;
std::atomic<uint64_t>* zone_gen = nullptr;


// This line tests that we can define tests in an unnamed namespace.
namespace {

	extern "C" int main(int argc, char* argv[])
	{
        std::string disable_telemetry_server;
        auto cli = (
			clipp::option("-t", "--disable_telemetry_server").doc("disable the telemetry_server") & clipp::value("disable_telemetry_server", disable_telemetry_server)
        );

        clipp::parsing_result res = clipp::parse(argc, argv, cli);
        
        enable_telemetry_server = disable_telemetry_server != "true";
        
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
    rpc::i_telemetry_service* telemetry_service_ = nullptr;
    bool use_v1_rpc_dll_ = false;
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

    host(rpc::i_telemetry_service* telemetry_service, rpc::zone zone_id,  bool use_v1_rpc_dll = false) : 
        telemetry_service_(telemetry_service), 
        use_v1_rpc_dll_(use_v1_rpc_dll),
        zone_id_(zone_id)
    {
        if(telemetry_service_)
            telemetry_service_->on_impl_creation("host", (uint64_t)this, zone_id_);
    }
    virtual ~host()
    {
        if(telemetry_service_)
            telemetry_service_->on_impl_deletion("host", (uint64_t)this, zone_id_);
    }
    error_code create_enclave(rpc::shared_ptr<yyy::i_example>& target) override
    {
        rpc::shared_ptr<yyy::i_host> host = shared_from_this();
        auto serv = current_host_service.lock();
        auto err_code = serv->connect_to_zone<rpc::enclave_service_proxy>( 
            {++(*zone_gen)}
            , host
            , target
            , use_v1_rpc_dll_ ? enclave_path_v1 : enclave_path);

        return err_code;
    };

    //live app registry, it should have sole responsibility for the long term storage of app shared ptrs
    error_code look_up_app(const std::string& app_name, rpc::shared_ptr<yyy::i_example>& app) override
    {
        std::scoped_lock lock(cached_apps_mux_);
        auto it = cached_apps_.find(app_name);
        if(it == cached_apps_.end())
        {
            return rpc::error::OK();
        }
        app = it->second;
        return rpc::error::OK();
    }

    error_code set_app(const std::string& name, const rpc::shared_ptr<yyy::i_example>& app) override
    {
        std::scoped_lock lock(cached_apps_mux_);
        
        cached_apps_[name] = app;
        return rpc::error::OK();
    }

    error_code unload_app(const std::string& name) override
    {
        std::scoped_lock lock(cached_apps_mux_);
        cached_apps_.erase(name);
        return rpc::error::OK();
    }
};

template<bool UseHostInChild>
class in_memory_setup
{
    rpc::shared_ptr<host_telemetry_service> tm;

    rpc::shared_ptr<yyy::i_host> i_host_ptr_;
    rpc::weak_ptr<yyy::i_host> local_host_ptr_;
    rpc::shared_ptr<yyy::i_example> i_example_ptr_;

    const bool has_enclave_ = false;
    bool use_host_in_child_ = UseHostInChild;

    std::atomic<uint64_t> zone_gen_ = 0;
public:
    virtual ~in_memory_setup() = default;

    rpc::shared_ptr<rpc::service> get_root_service() const {return nullptr;}
    bool get_has_enclave() const {return has_enclave_;}
    rpc::shared_ptr<yyy::i_example> get_example() const {return i_example_ptr_;}
    rpc::shared_ptr<yyy::i_host> get_host() const {return i_host_ptr_;}
    rpc::shared_ptr<yyy::i_host> get_local_host_ptr(){return local_host_ptr_.lock();}
    bool get_use_host_in_child() const {return use_host_in_child_;}
    rpc::shared_ptr<host_telemetry_service> get_telemetry_service() const {return tm;}

    virtual void SetUp()
    {   
        zone_gen = &zone_gen_;
        auto test_info = ::testing::UnitTest::GetInstance()->current_test_info();
        if(enable_telemetry_server)
            tm = rpc::make_shared<host_telemetry_service>(test_info->test_suite_name(), test_info->name());
        telemetry_service = tm.get();
        i_host_ptr_ = rpc::shared_ptr<yyy::i_host> (new host(tm.get(), {++zone_gen_}));
        local_host_ptr_ = i_host_ptr_;
        i_example_ptr_ = rpc::shared_ptr<yyy::i_example> (new example(telemetry_service, nullptr, use_host_in_child_ ? i_host_ptr_ : nullptr));
    }

    virtual void TearDown()
    {
        i_host_ptr_ = nullptr;
        i_example_ptr_ = nullptr;
        telemetry_service = nullptr;
        tm = nullptr;
        zone_gen = nullptr;
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
    rpc::shared_ptr<host_telemetry_service> tm;

    rpc::shared_ptr<rpc::service> root_service_;
    rpc::shared_ptr<rpc::child_service> child_service_;
    rpc::shared_ptr<yyy::i_host> i_host_ptr_;
    rpc::weak_ptr<yyy::i_host> local_host_ptr_;
    rpc::shared_ptr<yyy::i_example> i_example_ptr_;

    const bool has_enclave_ = true;
    bool use_host_in_child_ = UseHostInChild;
    bool run_standard_tests_ = RunStandardTests;

    std::atomic<uint64_t> zone_gen_ = 0;
    
public:
    virtual ~inproc_setup() = default;

    rpc::shared_ptr<rpc::service> get_root_service() const {return root_service_;}
    bool get_has_enclave() const {return has_enclave_;}
    bool is_enclave_setup() const {return false;}
    rpc::shared_ptr<yyy::i_example> get_example() const {return i_example_ptr_;}
    rpc::shared_ptr<yyy::i_host> get_host() const {return i_host_ptr_;}
    rpc::shared_ptr<yyy::i_host> get_local_host_ptr(){return local_host_ptr_.lock();}
    bool get_use_host_in_child() const {return use_host_in_child_;}
    rpc::shared_ptr<host_telemetry_service> get_telemetry_service() const {return tm;}
    
    virtual void SetUp()
    {
        zone_gen = &zone_gen_;
        auto test_info = ::testing::UnitTest::GetInstance()->current_test_info();
        if(enable_telemetry_server)
            tm = rpc::make_shared<host_telemetry_service>(test_info->test_suite_name(), test_info->name());

        telemetry_service = tm.get();
        root_service_ = rpc::make_shared<rpc::service>(rpc::zone{++zone_gen_}, tm.get());
        root_service_->add_service_logger(std::make_shared<test_service_logger>());
        current_host_service = root_service_;

        rpc::shared_ptr<yyy::i_host> hst(new host(tm.get(), root_service_->get_zone_id()));
        local_host_ptr_ = hst;//assign to weak ptr
        
        auto err_code = root_service_->connect_to_zone<rpc::local_child_service_proxy<yyy::i_example, yyy::i_host>>(
            {++zone_gen_}
            , hst
            , i_example_ptr_
            , [&](
                const rpc::shared_ptr<yyy::i_host>& host
                , rpc::shared_ptr<yyy::i_example>& new_example
                , const rpc::shared_ptr<rpc::child_service>& child_service_ptr) -> int
            {
                i_host_ptr_ = host;
                child_service_ = child_service_ptr;
                example_import_idl_register_stubs(child_service_ptr);
                example_shared_idl_register_stubs(child_service_ptr);
                example_idl_register_stubs(child_service_ptr);
                new_example = rpc::shared_ptr<yyy::i_example>(new example(child_service_ptr->get_telemetry_service(), child_service_ptr, nullptr));  
                if(use_host_in_child_) 
                    new_example->set_host(host);
                return rpc::error::OK();
            });
        ASSERT_ERROR_CODE(err_code);
    }

    virtual void TearDown()
    {
        i_example_ptr_ = nullptr;
        i_host_ptr_ = nullptr;
        child_service_ = nullptr;
        root_service_ = nullptr;
        telemetry_service = nullptr;
        tm = nullptr;
        zone_gen = nullptr;
    }

    rpc::shared_ptr<yyy::i_example> create_new_zone()
    {        
        rpc::shared_ptr<yyy::i_host> hst;
        if(use_host_in_child_)
            hst = local_host_ptr_.lock();
            
        rpc::shared_ptr<yyy::i_example> example_relay_ptr;

        auto err_code = root_service_->connect_to_zone<rpc::local_child_service_proxy<yyy::i_example, yyy::i_host>>(
            {++zone_gen_}
            , hst
            , example_relay_ptr
            , [&](
                const rpc::shared_ptr<yyy::i_host>& host
                , rpc::shared_ptr<yyy::i_example>& new_example
                , const rpc::shared_ptr<rpc::child_service>& child_service_ptr) -> int
            {
                example_import_idl_register_stubs(child_service_ptr);
                example_shared_idl_register_stubs(child_service_ptr);
                example_idl_register_stubs(child_service_ptr);
                new_example = rpc::shared_ptr<yyy::i_example>(new example(child_service_ptr->get_telemetry_service(), child_service_ptr, nullptr));  
                if(use_host_in_child_) 
                    new_example->set_host(host);
                return rpc::error::OK();
            });
        
        if(CreateNewZoneThenCreateSubordinatedZone)
        {
#ifdef USE_RPC_LOGGING
            message = std::string("******** create_example_in_subordnate_zone");
            LOG_STR(message.data(),message.size());
#endif         
            rpc::shared_ptr<yyy::i_example> new_ptr;
            auto err_code = example_relay_ptr->create_example_in_subordnate_zone(new_ptr, use_host_in_child_ ? local_host_ptr_.lock() : nullptr, ++zone_gen_);
            ASSERT_ERROR_CODE(err_code);
            example_relay_ptr->set_host(nullptr);
            example_relay_ptr = new_ptr;
        }
#ifdef USE_RPC_LOGGING
        message = std::string("******** returning");
        LOG_STR(message.data(),message.size());
#endif         
        return example_relay_ptr;
    }
};

template<bool UseHostInChild, bool RunStandardTests, bool CreateNewZoneThenCreateSubordinatedZone, bool use_v1_rpc_dll>
class enclave_setup
{
    rpc::shared_ptr<host_telemetry_service> tm;

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
    rpc::shared_ptr<host_telemetry_service> get_telemetry_service() const {return tm;}
    
    virtual void SetUp()
    {
        zone_gen = &zone_gen_;
        auto test_info = ::testing::UnitTest::GetInstance()->current_test_info();
        if(enable_telemetry_server)
            tm = rpc::make_shared<host_telemetry_service>(test_info->test_suite_name(), test_info->name());
        telemetry_service = tm.get();
        root_service_ = rpc::make_shared<rpc::service>(rpc::zone{++zone_gen_}, tm.get());
        root_service_->add_service_logger(std::make_shared<test_service_logger>());
        example_import_idl_register_stubs(root_service_);
        example_shared_idl_register_stubs(root_service_);
        example_idl_register_stubs(root_service_);
        current_host_service = root_service_;
        
        i_host_ptr_ = rpc::shared_ptr<yyy::i_host> (new host(tm.get(), root_service_->get_zone_id(), use_v1_rpc_dll));
        local_host_ptr_ = i_host_ptr_;        


        auto err_code = root_service_->connect_to_zone<rpc::enclave_service_proxy>(
            {++(*zone_gen)}, 
            use_host_in_child_ ? i_host_ptr_ : nullptr, 
            i_example_ptr_, 
            use_v1_rpc_dll ? enclave_path_v1 : enclave_path);

        ASSERT_ERROR_CODE(err_code);
    }

    virtual void TearDown()
    {
        i_example_ptr_ = nullptr;
        i_host_ptr_ = nullptr;
        root_service_ = nullptr;
        telemetry_service = nullptr;
        tm = nullptr;
        zone_gen = nullptr;
    }

    rpc::shared_ptr<yyy::i_example> create_new_zone()
    {
        rpc::shared_ptr<yyy::i_example> ptr;
        
        auto err_code = root_service_->connect_to_zone<rpc::enclave_service_proxy>(
            {++zone_gen_}, 
            use_host_in_child_ ? i_host_ptr_ : nullptr, 
            ptr, 
            use_v1_rpc_dll ? enclave_path_v1 : enclave_path);
            
        ASSERT_ERROR_CODE(err_code);
        if(CreateNewZoneThenCreateSubordinatedZone)
        {
            rpc::shared_ptr<yyy::i_example> new_ptr;
            err_code = ptr->create_example_in_subordnate_zone(new_ptr, use_host_in_child_ ? i_host_ptr_ : nullptr, ++zone_gen_);
            ASSERT_ERROR_CODE(err_code);
            ptr = new_ptr;
        }
        return ptr;
    }
};

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


    enclave_setup<false, false, false, false>, 
    enclave_setup<false, false, true, false>, 
    enclave_setup<false, true, false, false>, 
    enclave_setup<false, true, true, false>, 
    enclave_setup<true, false, false, false>, 
    enclave_setup<true, false, true, false>, 
    enclave_setup<true, true, false, false>, 
    enclave_setup<true, true, true, false>,

    enclave_setup<false, false, false, true>, 
    enclave_setup<false, false, true, true>, 
    enclave_setup<false, true, false, true>, 
    enclave_setup<false, true, true, true>, 
    enclave_setup<true, false, false, true>, 
    enclave_setup<true, false, true, true>, 
    enclave_setup<true, true, false, true>, 
    enclave_setup<true, true, true, true>>;
TYPED_TEST_SUITE(type_test, local_implementations);

TYPED_TEST(type_test, initialisation_test)
{
}

TYPED_TEST(type_test, standard_tests)
{
    auto root_service = this->get_lib().get_root_service();
    rpc::zone zone_id;
    if(root_service)
        zone_id = root_service->get_zone_id();
    else
        zone_id = {0};

    foo f(telemetry_service, zone_id);
    
    standard_tests(f, this->get_lib().get_has_enclave(), telemetry_service);
}

TYPED_TEST(type_test, dyanmic_cast_tests)
{
    auto root_service = this->get_lib().get_root_service();
    rpc::zone zone_id;
    if(root_service)
        zone_id = root_service->get_zone_id();
    else
        zone_id = {0};
    auto f = rpc::shared_ptr<xxx::i_foo>(new foo(telemetry_service, zone_id));

    rpc::shared_ptr<xxx::i_baz> baz;
    ASSERT_EQ(f->create_baz_interface(baz), 0);
    ASSERT_EQ(f->call_baz_interface(nullptr), 0); // feed in a nullptr
    ASSERT_EQ(f->call_baz_interface(baz), 0);     // feed back to the implementation

    auto x = rpc::dynamic_pointer_cast<xxx::i_baz>(baz);
    ASSERT_NE(x, nullptr);
    auto y = rpc::dynamic_pointer_cast<xxx::i_bar>(baz);
    ASSERT_NE(y, nullptr);
    y->do_something_else(1);
    auto z = rpc::dynamic_pointer_cast<xxx::i_foo>(baz);
    ASSERT_NE(y, nullptr);
}


template <class T>
using remote_type_test = type_test<T>;


typedef Types<
    //inproc_setup<false, false, false>, 

    inproc_setup<true, false, false>, 
    inproc_setup<true, false, true>, 
    inproc_setup<true, true, false>, 
    inproc_setup<true, true, true>, 

    enclave_setup<true, false, false, false>, 
    enclave_setup<true, false, true, false>, 
    enclave_setup<true, true, false, false>, 
    enclave_setup<true, true, true, false>, 

    enclave_setup<true, false, false, true>, 
    enclave_setup<true, false, true, true>, 
    enclave_setup<true, true, false, true>, 
    enclave_setup<true, true, true, true>> remote_implementations;
TYPED_TEST_SUITE(remote_type_test, remote_implementations);

TYPED_TEST(remote_type_test, remote_standard_tests)
{    
    rpc::shared_ptr<xxx::i_foo> i_foo_ptr;
    ASSERT_EQ(this->get_lib().get_example()->create_foo(i_foo_ptr), 0);
    standard_tests(*i_foo_ptr, true, telemetry_service);
}



TYPED_TEST(remote_type_test, multithreaded_standard_tests)
{
    rpc::shared_ptr<xxx::i_foo> i_foo_ptr;
    ASSERT_EQ(this->get_lib().get_example()->create_foo(i_foo_ptr), 0);
    
    std::vector<std::thread> threads(this->get_lib().is_enclave_setup() ? 3 : 100);
    for(auto& thread_target : threads)
    {
        thread_target = std::thread([&](){
            standard_tests(*i_foo_ptr, true, telemetry_service);   
        });
    }
    for(auto& thread_target : threads)
    {
        thread_target.join();
    }
}


TYPED_TEST(remote_type_test, multithreaded_standard_tests_with_and_foos)
{
    std::vector<std::thread> threads(this->get_lib().is_enclave_setup() ? 3 : 100);
    for(auto& thread_target : threads)
    {
        thread_target = std::thread([&](){
            rpc::shared_ptr<xxx::i_foo> i_foo_ptr;
            ASSERT_EQ(this->get_lib().get_example()->create_foo(i_foo_ptr), 0);
            standard_tests(*i_foo_ptr, true, telemetry_service);   
        });
    }
    for(auto& thread_target : threads)
    {
        thread_target.join();
    }
}

TYPED_TEST(remote_type_test, remote_tests)
{    
    auto root_service = this->get_lib().get_root_service();
    rpc::zone zone_id;
    if(root_service)
        zone_id = root_service->get_zone_id();
    else
        zone_id = {0};
    remote_tests(this->get_lib().get_use_host_in_child(), this->get_lib().get_example(), telemetry_service, zone_id);
}

TYPED_TEST(remote_type_test, multithreaded_remote_tests)
{
    if(this->get_lib().is_enclave_setup())
        return; // enclaves barf on memory exhastion;
    auto root_service = this->get_lib().get_root_service();
    rpc::zone zone_id;
    if(root_service)
        zone_id = root_service->get_zone_id();
    else
        zone_id = {0};
    std::vector<std::thread> threads(this->get_lib().is_enclave_setup() ? 3 : 100);
    for(auto& thread_target : threads)
    {
        thread_target = std::thread([&](){
            remote_tests(this->get_lib().get_use_host_in_child(), this->get_lib().get_example(), telemetry_service, zone_id);
        });
    }
    for(auto& thread_target : threads)
    {
        thread_target.join();
    }
}

TYPED_TEST(remote_type_test, create_new_zone)
{
    auto example_relay_ptr = this->get_lib().create_new_zone();
    example_relay_ptr->set_host(nullptr);
}

TYPED_TEST(remote_type_test, multithreaded_create_new_zone)
{
    std::vector<std::thread> threads(this->get_lib().is_enclave_setup() ? 3 : 100);
    for(auto& thread_target : threads)
    {
        thread_target = std::thread([&](){
            auto example_relay_ptr = this->get_lib().create_new_zone();
            example_relay_ptr->set_host(nullptr);        
        });
    }
    for(auto& thread_target : threads)
    {
        thread_target.join();
    }
}

TYPED_TEST(remote_type_test, create_new_zone_releasing_host_then_running_on_other_enclave)
{
    rpc::shared_ptr<xxx::i_foo> i_foo_relay_ptr;
    auto example_relay_ptr = this->get_lib().create_new_zone();
    example_relay_ptr->create_foo(i_foo_relay_ptr);
    standard_tests(*i_foo_relay_ptr, true, telemetry_service);

    //rpc::shared_ptr<xxx::i_foo> i_foo_ptr;
    //ASSERT_EQ(this->get_lib().get_example()->create_foo(i_foo_ptr), 0);
    example_relay_ptr = nullptr;
    //standard_tests(*i_foo_ptr, true, telemetry_service);    
}

TYPED_TEST(remote_type_test, multithreaded_create_new_zone_releasing_host_then_running_on_other_enclave)
{
    std::vector<std::thread> threads(this->get_lib().is_enclave_setup() ? 3 : 100);
    for(auto& thread_target : threads)
    {
        thread_target = std::thread([&](){
            rpc::shared_ptr<xxx::i_foo> i_foo_relay_ptr;
            auto example_relay_ptr = this->get_lib().create_new_zone();
            example_relay_ptr->create_foo(i_foo_relay_ptr);
            standard_tests(*i_foo_relay_ptr, true, telemetry_service);
        });
    }
    for(auto& thread_target : threads)
    {
        thread_target.join();
    }
}

TYPED_TEST(remote_type_test, dyanmic_cast_tests)
{
    rpc::shared_ptr<xxx::i_foo> i_foo_ptr;
    ASSERT_EQ(this->get_lib().get_example()->create_foo(i_foo_ptr), 0);

    rpc::shared_ptr<xxx::i_baz> baz;
    i_foo_ptr->create_baz_interface(baz);
    i_foo_ptr->call_baz_interface(nullptr); // feed in a nullptr
    i_foo_ptr->call_baz_interface(baz);     // feed back to the implementation

    auto x = rpc::dynamic_pointer_cast<xxx::i_baz>(baz);
    ASSERT_NE(x, nullptr);
    auto y = rpc::dynamic_pointer_cast<xxx::i_bar>(baz);
    ASSERT_NE(y, nullptr);
    y->do_something_else(1);
    auto z = rpc::dynamic_pointer_cast<xxx::i_foo>(baz);
    ASSERT_EQ(z, nullptr);
}

TYPED_TEST(remote_type_test, bounce_baz_between_two_interfaces)
{
    rpc::shared_ptr<xxx::i_foo> i_foo_ptr;
    ASSERT_EQ(this->get_lib().get_example()->create_foo(i_foo_ptr), 0);

    rpc::shared_ptr<xxx::i_foo> i_foo_relay_ptr;
    auto example_relay_ptr = this->get_lib().create_new_zone();
    example_relay_ptr->create_foo(i_foo_relay_ptr);

    rpc::shared_ptr<xxx::i_baz> baz;
    i_foo_ptr->create_baz_interface(baz);
    i_foo_relay_ptr->call_baz_interface(baz);
}


TYPED_TEST(remote_type_test, multithreaded_bounce_baz_between_two_interfaces)
{
    if(this->get_lib().is_enclave_setup())
        return; // enclaves barf on memory exhastion
    
    std::vector<std::thread> threads(this->get_lib().is_enclave_setup() ? 3 : 100);
    for(auto& thread_target : threads)
    {
        thread_target = std::thread([&](){
            rpc::shared_ptr<xxx::i_foo> i_foo_ptr;
            ASSERT_EQ(this->get_lib().get_example()->create_foo(i_foo_ptr), 0);

            rpc::shared_ptr<xxx::i_foo> i_foo_relay_ptr;
            auto example_relay_ptr = this->get_lib().create_new_zone();
            example_relay_ptr->create_foo(i_foo_relay_ptr);

            rpc::shared_ptr<xxx::i_baz> baz;
            i_foo_ptr->create_baz_interface(baz);
            i_foo_relay_ptr->call_baz_interface(baz);
        });
    }
    for(auto& thread_target : threads)
    {
        thread_target.join();
    }
}

// check for null
TYPED_TEST(remote_type_test, check_for_null_interface)
{
    rpc::shared_ptr<xxx::i_foo> i_foo_ptr;
    ASSERT_EQ(this->get_lib().get_example()->create_foo(i_foo_ptr), 0);
    rpc::shared_ptr<xxx::i_baz> c;
    i_foo_ptr->get_interface(c);
    RPC_ASSERT(c == nullptr);
}

TYPED_TEST(remote_type_test, check_for_multiple_sets)
{
    if(!this->get_lib().get_use_host_in_child())
        return;

    rpc::shared_ptr<xxx::i_foo> i_foo_ptr;
    ASSERT_EQ(this->get_lib().get_example()->create_foo(i_foo_ptr), 0);
    
    auto zone_id = i_foo_ptr->query_proxy_base()->get_object_proxy()->get_service_proxy()->get_zone_id();

    auto b = rpc::make_shared<marshalled_tests::baz>(telemetry_service, zone_id);
    // set
    i_foo_ptr->set_interface(b);
    // reset
    i_foo_ptr->set_interface(nullptr);
    // set
    i_foo_ptr->set_interface(b);
    // reset
    i_foo_ptr->set_interface(nullptr);
}

TYPED_TEST(remote_type_test, check_for_interface_storage)
{
    if(!this->get_lib().get_use_host_in_child())
        return;
        
    rpc::shared_ptr<xxx::i_foo> i_foo_ptr;
    ASSERT_EQ(this->get_lib().get_example()->create_foo(i_foo_ptr), 0);

    rpc::shared_ptr<xxx::i_baz> c;
    auto zone_id = i_foo_ptr->query_proxy_base()->get_object_proxy()->get_service_proxy()->get_zone_id();

    auto b = rpc::make_shared<marshalled_tests::baz>(telemetry_service, zone_id);
    i_foo_ptr->set_interface(b);
    i_foo_ptr->get_interface(c);
    i_foo_ptr->set_interface(nullptr);
    RPC_ASSERT(b == c);
}

TYPED_TEST(remote_type_test, check_for_set_multiple_inheritance)
{
    if(!this->get_lib().get_use_host_in_child())
        return;
    auto proxy = this->get_lib().get_example()->query_proxy_base();        
    auto ret = this->get_lib().get_example()->give_interface(
        rpc::shared_ptr<xxx::i_baz>(new multiple_inheritance(telemetry_service, proxy->get_object_proxy()->get_service_proxy()->get_zone_id())));
    RPC_ASSERT(ret == rpc::error::OK());
}



TYPED_TEST(remote_type_test, host_test)
{    
    auto root_service = this->get_lib().get_root_service();
    rpc::zone zone_id;
    if(root_service)
        zone_id = root_service->get_zone_id();
    else
        zone_id = {0};
    auto h = rpc::make_shared<host>(this->get_lib().get_telemetry_service().get(), zone_id);

    rpc::shared_ptr<yyy::i_example> target;
    rpc::shared_ptr<yyy::i_example> target2;
    h->create_enclave(target);
    ASSERT_NE(target, nullptr);

    ASSERT_EQ(h->set_app("target", target), rpc::error::OK());
    ASSERT_EQ(h->look_up_app("target", target2), rpc::error::OK());
    ASSERT_EQ(h->unload_app("target"), rpc::error::OK());
    target = nullptr;
    target2 = nullptr;

}

TYPED_TEST(remote_type_test, check_for_call_enclave_zone)
{
    if(!this->get_lib().get_use_host_in_child())
        return;

    auto root_service = this->get_lib().get_root_service();
    rpc::zone zone_id;
    if(root_service)
        zone_id = root_service->get_zone_id();
    else
        zone_id = {0};
        
    auto h = rpc::make_shared<host>(this->get_lib().get_telemetry_service().get(), zone_id);
    auto ret = this->get_lib().get_example()->call_create_enclave_val(h);
    RPC_ASSERT(ret == rpc::error::OK());
}

TYPED_TEST(remote_type_test, check_sub_subordinate)
{
    auto& lib = this->get_lib();
    if(!lib.get_use_host_in_child())
        return;

    rpc::shared_ptr<yyy::i_example> new_zone;
    ASSERT_EQ(lib.get_example()->create_example_in_subordnate_zone(new_zone, lib.get_local_host_ptr(), ++(*zone_gen)), rpc::error::OK()); //second level

    rpc::shared_ptr<yyy::i_example> new_new_zone;
    ASSERT_EQ(new_zone->create_example_in_subordnate_zone(new_new_zone, lib.get_local_host_ptr(), ++(*zone_gen)), rpc::error::OK()); //third level
}



TYPED_TEST(remote_type_test, multithreaded_check_sub_subordinate)
{
    auto& lib = this->get_lib();
    if(!lib.get_use_host_in_child())
        return;
    std::vector<std::thread> threads(this->get_lib().is_enclave_setup() ? 3 : 100);
    for(auto& thread_target : threads)
    {
        thread_target = std::thread([&](){
            rpc::shared_ptr<yyy::i_example> new_zone;
            ASSERT_EQ(lib.get_example()->create_example_in_subordnate_zone(new_zone, lib.get_local_host_ptr(), ++(*zone_gen)), rpc::error::OK()); //second level

            rpc::shared_ptr<yyy::i_example> new_new_zone;
            ASSERT_EQ(new_zone->create_example_in_subordnate_zone(new_new_zone, lib.get_local_host_ptr(), ++(*zone_gen)), rpc::error::OK()); //third level
        });
    }
    for(auto& thread_target : threads)
    {
        thread_target.join();
    }
}

TYPED_TEST(remote_type_test, check_identity)
{
    auto& lib = this->get_lib();
    if(!lib.get_use_host_in_child())
        return;

    rpc::shared_ptr<xxx::i_baz> output;

    rpc::shared_ptr<yyy::i_example> new_zone;
    //lib.i_example_ptr_ //first level
    ASSERT_EQ(lib.get_example()->create_example_in_subordnate_zone(new_zone, lib.get_local_host_ptr(), ++(*zone_gen)), rpc::error::OK()); //second level

    rpc::shared_ptr<yyy::i_example> new_new_zone;
    ASSERT_EQ(new_zone->create_example_in_subordnate_zone(new_new_zone, lib.get_local_host_ptr(), ++(*zone_gen)), rpc::error::OK()); //third level

    auto new_zone_fork = lib.create_new_zone();//second level

    rpc::shared_ptr<xxx::i_baz> new_baz;
    new_zone->create_baz(new_baz);
    
    rpc::shared_ptr<xxx::i_baz> new_new_baz;
    new_new_zone->create_baz(new_new_baz);

    rpc::shared_ptr<xxx::i_baz> new_baz_fork;
    new_zone_fork->create_baz(new_baz_fork);

    // topology looks like this now flinging bazes around these nodes to ensure that the identity of bazes is the same
    // *4                           #
    //  \                           #
    //   *3                         #
    //    \                         #
    //     *2  *5                   #
    //      \ /                     #
    //       1                      #


    auto proxy = lib.get_example()->query_proxy_base();
    auto base_baz = rpc::shared_ptr<xxx::i_baz>(new baz(telemetry_service, proxy->get_object_proxy()->get_service_proxy()->get_zone_id()));
    auto input = base_baz;

    ASSERT_EQ(lib.get_example()->send_interface_back(input, output), rpc::error::OK());
    ASSERT_EQ(input, output);
    
    ASSERT_EQ(new_zone->send_interface_back(input, output), rpc::error::OK());
    ASSERT_EQ(input, output);    

    ASSERT_EQ(new_new_zone->send_interface_back(input, output), rpc::error::OK());
    ASSERT_EQ(input, output); 

    input = new_baz;

    ASSERT_EQ(lib.get_example()->send_interface_back(input, output), rpc::error::OK());
    ASSERT_EQ(input, output);
    
    ASSERT_EQ(new_zone->send_interface_back(input, output), rpc::error::OK());
    ASSERT_EQ(input, output);  

    ASSERT_EQ(new_new_zone->send_interface_back(input, output), rpc::error::OK());
    ASSERT_EQ(input, output);    


    input = new_new_baz;

    ASSERT_EQ(lib.get_example()->send_interface_back(input, output), rpc::error::OK());
    ASSERT_EQ(input, output);
    
    ASSERT_EQ(new_zone->send_interface_back(input, output), rpc::error::OK());
    ASSERT_EQ(input, output);    

    ASSERT_EQ(new_new_zone->send_interface_back(input, output), rpc::error::OK());
    ASSERT_EQ(input, output);    



    input = new_baz_fork;
    
    // *z2   *i5                 #
    //  \  /                     #
    //   h1                      #

    ASSERT_EQ(lib.get_example()->send_interface_back(input, output), rpc::error::OK());
    ASSERT_EQ(input, output);
    
    // *z3                       #
    //  \                        #
    //   *2   *i5                #
    //    \ /                    #
    //     h1                    #
    
    ASSERT_EQ(new_zone->send_interface_back(input, output), rpc::error::OK());
    ASSERT_EQ(input, output);   

        
    // *z4                          #
    //  \                           #
    //   *3                         #
    //    \                         #
    //     *2  *i5                  #
    //      \ /                     #
    //       h1                     #

    ASSERT_EQ(new_new_zone->send_interface_back(input, output), rpc::error::OK());
    ASSERT_EQ(input, output);

}

template <class T>
class type_test_with_host : 
    public type_test<T>
{
    
};

typedef Types<
    inproc_setup<true, false, false>, 
    inproc_setup<true, false, true>, 
    inproc_setup<true, true, false>, 
    inproc_setup<true, true, true>, 

    enclave_setup<true, false, false, false>, 
    enclave_setup<true, false, true, false>, 
    enclave_setup<true, true, false, false>, 
    enclave_setup<true, true, true, false>, 

    enclave_setup<true, false, false, true>, 
    enclave_setup<true, false, true, true>, 
    enclave_setup<true, true, false, true>, 
    enclave_setup<true, true, true, true>> type_test_with_host_implementations;
TYPED_TEST_SUITE(type_test_with_host, type_test_with_host_implementations);


TYPED_TEST(type_test_with_host, call_host_create_enclave_and_throw_away)
{  
    bool run_standard_tests = false;
    ASSERT_EQ(this->get_lib().get_example()->call_host_create_enclave_and_throw_away(run_standard_tests), rpc::error::OK());
}

TYPED_TEST(type_test_with_host, call_host_create_enclave)
{  
    bool run_standard_tests = false;
    rpc::shared_ptr<yyy::i_example> target;

    ASSERT_EQ(this->get_lib().get_example()->call_host_create_enclave(target, run_standard_tests), rpc::error::OK());
    ASSERT_NE(target, nullptr);
}

TYPED_TEST(type_test_with_host, look_up_app_and_return_with_nothing)
{  
    bool run_standard_tests = false;
    rpc::shared_ptr<yyy::i_example> target;

    ASSERT_EQ(this->get_lib().get_example()->call_host_look_up_app("target", target, run_standard_tests), rpc::error::OK());
    ASSERT_EQ(target, nullptr);
}


TYPED_TEST(type_test_with_host, call_host_unload_app_not_there)
{  
    ASSERT_EQ(this->get_lib().get_example()->call_host_unload_app("target"), rpc::error::OK());
}


TYPED_TEST(type_test_with_host, call_host_look_up_app_unload_app)
{  
    bool run_standard_tests = false;
    rpc::shared_ptr<yyy::i_example> target;

    ASSERT_EQ(this->get_lib().get_example()->call_host_create_enclave(target, run_standard_tests), rpc::error::OK());
    ASSERT_NE(target, nullptr);

    ASSERT_EQ(this->get_lib().get_example()->call_host_set_app("target", target, run_standard_tests), rpc::error::OK());
    if(telemetry_service)
        telemetry_service->message(rpc::i_telemetry_service::info, "call_host_unload_app");
    ASSERT_EQ(this->get_lib().get_example()->call_host_unload_app("target"), rpc::error::OK());
    target = nullptr;
}


TYPED_TEST(type_test_with_host, call_host_look_up_app_not_return)
{  
    bool run_standard_tests = false;
    rpc::shared_ptr<yyy::i_example> target;

    ASSERT_EQ(this->get_lib().get_example()->call_host_create_enclave(target, run_standard_tests), rpc::error::OK());
    ASSERT_NE(target, nullptr);

    ASSERT_EQ(this->get_lib().get_example()->call_host_set_app("target", target, run_standard_tests), rpc::error::OK());
    if(telemetry_service)
        telemetry_service->message(rpc::i_telemetry_service::info, "call_host_look_up_app_not_return");
    ASSERT_EQ(this->get_lib().get_example()->call_host_look_up_app_not_return("target", run_standard_tests), rpc::error::OK());
    if(telemetry_service)
        telemetry_service->message(rpc::i_telemetry_service::info, "call_host_look_up_app_not_return complete");
    ASSERT_EQ(this->get_lib().get_example()->call_host_unload_app("target"), rpc::error::OK());
    target = nullptr;
    if(telemetry_service)
        telemetry_service->message(rpc::i_telemetry_service::info, "app released");
}

TYPED_TEST(type_test_with_host, create_store_fetch_delete)
{  
    bool run_standard_tests = false;
    rpc::shared_ptr<yyy::i_example> target;
    rpc::shared_ptr<yyy::i_example> target2;

    ASSERT_EQ(this->get_lib().get_example()->call_host_create_enclave(target, run_standard_tests), rpc::error::OK());
    ASSERT_NE(target, nullptr);

    ASSERT_EQ(this->get_lib().get_example()->call_host_set_app("target", target, run_standard_tests), rpc::error::OK());
    if(telemetry_service)
        telemetry_service->message(rpc::i_telemetry_service::info, "call_host_look_up_app");
    ASSERT_EQ(this->get_lib().get_example()->call_host_look_up_app("target", target2, run_standard_tests), rpc::error::OK());
    if(telemetry_service)   
        telemetry_service->message(rpc::i_telemetry_service::info, "call_host_look_up_app complete");
    ASSERT_EQ(this->get_lib().get_example()->call_host_unload_app("target"), rpc::error::OK());
    ASSERT_EQ(target, target2);
    target = nullptr;
    target2 = nullptr;
    if(telemetry_service)
        telemetry_service->message(rpc::i_telemetry_service::info, "app released");
}

TYPED_TEST(type_test_with_host, create_store_not_return_delete)
{  
    bool run_standard_tests = false;
    rpc::shared_ptr<yyy::i_example> target;

    ASSERT_EQ(this->get_lib().get_example()->call_host_create_enclave(target, run_standard_tests), rpc::error::OK());
    ASSERT_NE(target, nullptr);

    ASSERT_EQ(this->get_lib().get_example()->call_host_set_app("target", target, run_standard_tests), rpc::error::OK());
    if(telemetry_service)
        telemetry_service->message(rpc::i_telemetry_service::info, "call_host_look_up_app_not_return");
    ASSERT_EQ(this->get_lib().get_example()->call_host_look_up_app_not_return_and_delete("target", run_standard_tests), rpc::error::OK());
    if(telemetry_service)
        telemetry_service->message(rpc::i_telemetry_service::info, "call_host_look_up_app_not_return complete");
    target = nullptr;
    if(telemetry_service)
        telemetry_service->message(rpc::i_telemetry_service::info, "app released");
}

TYPED_TEST(type_test_with_host, create_store_delete)
{  
    bool run_standard_tests = false;
    rpc::shared_ptr<yyy::i_example> target;
    rpc::shared_ptr<yyy::i_example> target2;

    ASSERT_EQ(this->get_lib().get_example()->call_host_create_enclave(target, run_standard_tests), rpc::error::OK());
    ASSERT_NE(target, nullptr);

    ASSERT_EQ(this->get_lib().get_example()->call_host_set_app("target", target, run_standard_tests), rpc::error::OK());
    if(telemetry_service)
        telemetry_service->message(rpc::i_telemetry_service::info, "call_host_look_up_app_and_delete");
    ASSERT_EQ(this->get_lib().get_example()->call_host_look_up_app_and_delete("target", target2, run_standard_tests), rpc::error::OK());
    if(telemetry_service)
        telemetry_service->message(rpc::i_telemetry_service::info, "call_host_look_up_app_and_delete complete");
    ASSERT_EQ(target, target2);
    target = nullptr;
    target2 = nullptr;
    if(telemetry_service)
        telemetry_service->message(rpc::i_telemetry_service::info, "app released");
}

TYPED_TEST(type_test_with_host, create_subordinate_zone)
{  
    rpc::shared_ptr<yyy::i_example> target;
    ASSERT_EQ(this->get_lib().get_example()->create_example_in_subordnate_zone(target, this->get_lib().get_local_host_ptr(), ++(*zone_gen)), rpc::error::OK());
}


TYPED_TEST(type_test_with_host, create_subordinate_zone_and_set_in_host)
{  
    ASSERT_EQ(this->get_lib().get_example()->create_example_in_subordnate_zone_and_set_in_host(++(*zone_gen), "foo", this->get_lib().get_local_host_ptr()), rpc::error::OK());
    rpc::shared_ptr<yyy::i_example> target;
    this->get_lib().get_host()->look_up_app("foo", target);
    this->get_lib().get_host()->unload_app("foo");
    target->set_host(nullptr);
}
