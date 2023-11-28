#include <iostream>
#include <unordered_map>
#include <string_view>

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

rpc::weak_ptr<rpc::service> current_host_service;

const rpc::i_telemetry_service* telemetry_service = nullptr;
uint64_t* zone_gen = nullptr;


// This line tests that we can define tests in an unnamed namespace.
namespace {

	extern "C" int main(int argc, char* argv[])
	{
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

    bool use_v1_rpc_dll_ = false;

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

    host(bool use_v1_rpc_dll = false) : use_v1_rpc_dll_(use_v1_rpc_dll){}
    ~host(){}
    error_code create_enclave(rpc::shared_ptr<yyy::i_example>& target) override
    {
        rpc::shared_ptr<yyy::i_host> host = shared_from_this();
        auto serv = current_host_service.lock();
        auto err_code = rpc::enclave_service_proxy::create(
            {++(*zone_gen)}, 
            use_v1_rpc_dll_ ? enclave_path_v1 : enclave_path, 
            serv,
            host,
            target, 
            telemetry_service);
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

    rpc::shared_ptr<yyy::i_host> i_host_ptr;
    rpc::shared_ptr<yyy::i_example> i_example_ptr;

    const bool has_enclave = false;
    bool use_host_in_child = UseHostInChild;

    uint64_t zone_gen_ = 0;
public:

    bool get_has_enclave() const {return has_enclave;}
    rpc::shared_ptr<yyy::i_example> get_example(){return i_example_ptr;}
    bool get_use_host_in_child() const {return use_host_in_child;}

    virtual void SetUp()
    {   
        zone_gen = &zone_gen_;
        tm = rpc::make_shared<host_telemetry_service>();
        telemetry_service = tm.get();
        i_host_ptr = rpc::shared_ptr<yyy::i_host> (new host());
        i_example_ptr = rpc::shared_ptr<yyy::i_example> (new example(telemetry_service, nullptr, use_host_in_child ? i_host_ptr : nullptr));
    }

    virtual void TearDown()
    {
        i_host_ptr = nullptr;
        i_example_ptr = nullptr;
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

    rpc::shared_ptr<rpc::service> root_service;
    rpc::shared_ptr<rpc::child_service> child_service;
    rpc::shared_ptr<yyy::i_host> i_host_ptr;
    rpc::shared_ptr<yyy::i_example> i_example_ptr;

    const bool has_enclave = true;
    bool use_host_in_child = UseHostInChild;
    bool run_standard_tests = RunStandardTests;

    uint64_t zone_gen_ = 0;
    
public:

    bool get_has_enclave() const {return has_enclave;}
    rpc::shared_ptr<yyy::i_example> get_example(){return i_example_ptr;}
    bool get_use_host_in_child() const {return use_host_in_child;}
    
    virtual void SetUp()
    {
        zone_gen = &zone_gen_;
        tm = rpc::make_shared<host_telemetry_service>();
        telemetry_service = tm.get();
        root_service = rpc::make_shared<rpc::service>(rpc::zone{++zone_gen_});
        root_service->add_service_logger(std::make_shared<test_service_logger>());
        current_host_service = root_service;
        child_service = rpc::make_shared<rpc::child_service>(rpc::zone{++zone_gen_});
        example_import_idl_register_stubs(child_service);
        example_shared_idl_register_stubs(child_service);
        example_idl_register_stubs(child_service);

        rpc::interface_descriptor host_encap {};
        rpc::interface_descriptor example_encap {};

        // create a proxy to the rpc::service hosting the child service
        auto service_proxy_to_host
            = rpc::local_service_proxy::create(root_service, child_service, telemetry_service, false);

        // create a proxy to the rpc::service that contains the example object
        auto service_proxy_to_child
            = rpc::local_child_service_proxy::create(child_service, root_service, telemetry_service);

        {
            // create a host implementation object
            rpc::shared_ptr<yyy::i_host> hst(new host());

            // register implementation to the root service and held by a stub
            // note! There is a memory leak if the interface_descriptor object is not bound to a proxy
            host_encap = rpc::create_interface_stub(*root_service, hst);

            // simple test to check that we can get a useful local interface based on type and object id
            auto example_from_cast = root_service->get_local_interface<yyy::i_host>(rpc::get_version(), host_encap.object_id);
            EXPECT_EQ(example_from_cast, hst);
        }

        ASSERT(!rpc::demarshall_interface_proxy(rpc::get_version(), service_proxy_to_host, host_encap,  child_service->get_zone_id().as_caller(), i_host_ptr));

        {
            // create the example object implementation
            rpc::shared_ptr<yyy::i_example> remote_example(new example(telemetry_service, child_service, use_host_in_child ? i_host_ptr : nullptr));

            example_encap
                = rpc::create_interface_stub(*child_service, remote_example);

            // simple test to check that we can get a usefule local interface based on type and object id
            auto example_from_cast
                = child_service->get_local_interface<yyy::i_example>(rpc::get_version(), example_encap.object_id);
            EXPECT_EQ(example_from_cast, remote_example);
        }

        ASSERT(!rpc::demarshall_interface_proxy(rpc::get_version(), service_proxy_to_child, example_encap, root_service->get_zone_id().as_caller(), i_example_ptr));
    }

    virtual void TearDown()
    {
        i_example_ptr = nullptr;
        child_service = nullptr;
        i_host_ptr = nullptr;
        root_service = nullptr;
        telemetry_service = nullptr;
        tm = nullptr;
        zone_gen = nullptr;
    }

    rpc::shared_ptr<yyy::i_example> create_new_zone()
    {        
        rpc::shared_ptr<rpc::child_service> new_service = rpc::make_shared<rpc::child_service>(rpc::zone{++zone_gen_});
        example_import_idl_register_stubs(child_service);
        example_shared_idl_register_stubs(child_service);
        example_idl_register_stubs(child_service);
        
        // create a proxy to the rpc::service hosting the child service
        auto service_proxy_to_host
            = rpc::local_service_proxy::create(root_service, new_service, telemetry_service, true);

        // create a proxy to the rpc::service that contains the example object
        auto service_proxy_to_child = rpc::local_child_service_proxy::create(new_service, root_service, telemetry_service);

        // create the example object implementation
        rpc::shared_ptr<yyy::i_example> remote_example(new example(telemetry_service, new_service, use_host_in_child ? i_host_ptr : nullptr));

        rpc::interface_descriptor example_encap
            = rpc::create_interface_stub(*new_service, remote_example);

        // simple test to check that we can get a usefule local interface based on type and object id
        auto example_from_cast
            = new_service->get_local_interface<yyy::i_example>(rpc::get_version(), example_encap.object_id);
        EXPECT_EQ(example_from_cast, remote_example);

        rpc::shared_ptr<yyy::i_example> example_relay_ptr;
        ASSERT(!rpc::demarshall_interface_proxy(rpc::get_version(), service_proxy_to_child, example_encap, root_service->get_zone_id().as_caller(), example_relay_ptr));                
        
        if(CreateNewZoneThenCreateSubordinatedZone)
        {
            rpc::shared_ptr<yyy::i_example> new_ptr;
            ASSERT(!example_relay_ptr->create_example_in_subordnate_zone(new_ptr, ++zone_gen_));
            example_relay_ptr = new_ptr;
        }
        return example_relay_ptr;
    }
};

template<bool UseHostInChild, bool RunStandardTests, bool CreateNewZoneThenCreateSubordinatedZone, bool use_v1_rpc_dll>
class enclave_setup
{
    rpc::shared_ptr<host_telemetry_service> tm;

    rpc::shared_ptr<rpc::service> root_service;
    rpc::shared_ptr<yyy::i_host> i_host_ptr;
    rpc::shared_ptr<yyy::i_example> i_example_ptr;

    const bool has_enclave = true;
    bool use_host_in_child = UseHostInChild;
    bool run_standard_tests = RunStandardTests;

    uint64_t zone_gen_ = 0;
public:

    bool get_has_enclave() const {return has_enclave;}
    rpc::shared_ptr<yyy::i_example> get_example(){return i_example_ptr;}
    bool get_use_host_in_child() const {return use_host_in_child;}
    
    virtual void SetUp()
    {
        zone_gen = &zone_gen_;
        tm = rpc::make_shared<host_telemetry_service>();
        telemetry_service = tm.get();
        root_service = rpc::make_shared<rpc::service>(rpc::zone{++zone_gen_});
        root_service->add_service_logger(std::make_shared<test_service_logger>());
        example_import_idl_register_stubs(root_service);
        example_shared_idl_register_stubs(root_service);
        example_idl_register_stubs(root_service);
        current_host_service = root_service;
        
        i_host_ptr = rpc::shared_ptr<yyy::i_host> (new host(use_v1_rpc_dll));

        auto err_code = rpc::enclave_service_proxy::create(
            {++zone_gen_}, 
            use_v1_rpc_dll ? enclave_path_v1 : enclave_path, 
            root_service, 
            use_host_in_child ? i_host_ptr : nullptr, 
            i_example_ptr,
            telemetry_service);

        ASSERT(!err_code);
    }

    virtual void TearDown()
    {
        i_example_ptr = nullptr;
        i_host_ptr = nullptr;
        root_service = nullptr;
        telemetry_service = nullptr;
        tm = nullptr;
        zone_gen = nullptr;
    }

    rpc::shared_ptr<yyy::i_example> create_new_zone()
    {
        rpc::shared_ptr<yyy::i_example> ptr;
        auto err_code = rpc::enclave_service_proxy::create(
            {++zone_gen_}, 
            use_v1_rpc_dll ? enclave_path_v1 : enclave_path, 
            root_service, 
            use_host_in_child ? i_host_ptr : nullptr, 
            ptr,
            telemetry_service);
        ASSERT(!err_code);
        if(CreateNewZoneThenCreateSubordinatedZone)
        {
            rpc::shared_ptr<yyy::i_example> new_ptr;
            err_code = ptr->create_example_in_subordnate_zone(new_ptr, ++zone_gen_);
            ASSERT(!err_code);
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
    foo f(telemetry_service);
    
    standard_tests(f, this->get_lib().get_has_enclave(), telemetry_service);
}



TYPED_TEST(type_test, dyanmic_cast_tests)
{
    auto f = rpc::shared_ptr<xxx::i_foo>(new foo(telemetry_service));

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

TYPED_TEST(remote_type_test, remote_tests)
{    
    remote_tests(this->get_lib().get_use_host_in_child(), this->get_lib().get_example(), telemetry_service);
}

TYPED_TEST(remote_type_test, create_new_zone)
{
    rpc::shared_ptr<xxx::i_foo> i_foo_relay_ptr;
    auto example_relay_ptr = this->get_lib().create_new_zone();
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

// check for null
TYPED_TEST(remote_type_test, check_for_null_interface)
{
    rpc::shared_ptr<xxx::i_foo> i_foo_ptr;
    ASSERT_EQ(this->get_lib().get_example()->create_foo(i_foo_ptr), 0);
    rpc::shared_ptr<xxx::i_baz> c;
    i_foo_ptr->get_interface(c);
    assert(c == nullptr);
}

TYPED_TEST(remote_type_test, check_for_multiple_sets)
{
    if(!this->get_lib().get_use_host_in_child())
        return;
    rpc::shared_ptr<xxx::i_foo> i_foo_ptr;
    ASSERT_EQ(this->get_lib().get_example()->create_foo(i_foo_ptr), 0);

    auto b = rpc::make_shared<marshalled_tests::baz>(telemetry_service);
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
    auto b = rpc::make_shared<marshalled_tests::baz>(telemetry_service);
    i_foo_ptr->set_interface(b);
    i_foo_ptr->get_interface(c);
    i_foo_ptr->set_interface(nullptr);
    assert(b == c);
}

TYPED_TEST(remote_type_test, check_for_set_multiple_inheritance)
{
    if(!this->get_lib().get_use_host_in_child())
        return;
    auto ret = this->get_lib().get_example()->give_interface(
        rpc::shared_ptr<xxx::i_baz>(new multiple_inheritance(telemetry_service)));
    assert(ret == rpc::error::OK());
}



TYPED_TEST(remote_type_test, host_test)
{    
    auto h = rpc::make_shared<host>();

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
    auto h = rpc::make_shared<host>();
    auto ret = this->get_lib().get_example()->call_create_enclave_val(h);
    assert(ret == rpc::error::OK());
}

TYPED_TEST(remote_type_test, check_identity)
{
    if(!this->get_lib().get_use_host_in_child())
        return;

    rpc::shared_ptr<xxx::i_baz> output;

    rpc::shared_ptr<yyy::i_example> new_zone;
    //this->get_lib().i_example_ptr //first level
    ASSERT_EQ(this->get_lib().get_example()->create_example_in_subordnate_zone(new_zone, ++(*zone_gen)), rpc::error::OK()); //second level

    rpc::shared_ptr<yyy::i_example> new_new_zone;
    ASSERT_EQ(new_zone->create_example_in_subordnate_zone(new_new_zone, ++(*zone_gen)), rpc::error::OK()); //third level

    auto new_zone_fork = this->get_lib().create_new_zone();//second level

    auto base_baz = rpc::shared_ptr<xxx::i_baz>(new baz(telemetry_service));

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


    auto input = base_baz;

    ASSERT_EQ(this->get_lib().get_example()->send_interface_back(input, output), rpc::error::OK());
    ASSERT_EQ(input, output);
    
    ASSERT_EQ(new_zone->send_interface_back(input, output), rpc::error::OK());
    ASSERT_EQ(input, output);    

    ASSERT_EQ(new_new_zone->send_interface_back(input, output), rpc::error::OK());
    ASSERT_EQ(input, output); 

    input = new_baz;

    ASSERT_EQ(this->get_lib().get_example()->send_interface_back(input, output), rpc::error::OK());
    ASSERT_EQ(input, output);
    
    ASSERT_EQ(new_zone->send_interface_back(input, output), rpc::error::OK());
    ASSERT_EQ(input, output);  

    ASSERT_EQ(new_new_zone->send_interface_back(input, output), rpc::error::OK());
    ASSERT_EQ(input, output);    


    input = new_new_baz;

    ASSERT_EQ(this->get_lib().get_example()->send_interface_back(input, output), rpc::error::OK());
    ASSERT_EQ(input, output);
    
    ASSERT_EQ(new_zone->send_interface_back(input, output), rpc::error::OK());
    ASSERT_EQ(input, output);    

    ASSERT_EQ(new_new_zone->send_interface_back(input, output), rpc::error::OK());
    ASSERT_EQ(input, output);    


    input = new_baz_fork;
    
    // *z2   *i5                 #
    //  \  /                     #
    //   h1                      #

    ASSERT_EQ(this->get_lib().get_example()->send_interface_back(input, output), rpc::error::OK());
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
    telemetry_service->message(rpc::i_telemetry_service::info, "call_host_look_up_app_not_return");
    ASSERT_EQ(this->get_lib().get_example()->call_host_look_up_app_not_return("target", run_standard_tests), rpc::error::OK());
    telemetry_service->message(rpc::i_telemetry_service::info, "call_host_look_up_app_not_return complete");
    ASSERT_EQ(this->get_lib().get_example()->call_host_unload_app("target"), rpc::error::OK());
    target = nullptr;
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
    telemetry_service->message(rpc::i_telemetry_service::info, "call_host_look_up_app");
    ASSERT_EQ(this->get_lib().get_example()->call_host_look_up_app("target", target2, run_standard_tests), rpc::error::OK());
    telemetry_service->message(rpc::i_telemetry_service::info, "call_host_look_up_app complete");
    ASSERT_EQ(this->get_lib().get_example()->call_host_unload_app("target"), rpc::error::OK());
    ASSERT_EQ(target, target2);
    target = nullptr;
    target2 = nullptr;
    telemetry_service->message(rpc::i_telemetry_service::info, "app released");
}

TYPED_TEST(type_test_with_host, create_store_not_return_delete)
{  
    bool run_standard_tests = false;
    rpc::shared_ptr<yyy::i_example> target;

    ASSERT_EQ(this->get_lib().get_example()->call_host_create_enclave(target, run_standard_tests), rpc::error::OK());
    ASSERT_NE(target, nullptr);

    ASSERT_EQ(this->get_lib().get_example()->call_host_set_app("target", target, run_standard_tests), rpc::error::OK());
    telemetry_service->message(rpc::i_telemetry_service::info, "call_host_look_up_app_not_return");
    ASSERT_EQ(this->get_lib().get_example()->call_host_look_up_app_not_return_and_delete("target", run_standard_tests), rpc::error::OK());
    telemetry_service->message(rpc::i_telemetry_service::info, "call_host_look_up_app_not_return complete");
    target = nullptr;
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
    telemetry_service->message(rpc::i_telemetry_service::info, "call_host_look_up_app_and_delete");
    ASSERT_EQ(this->get_lib().get_example()->call_host_look_up_app_and_delete("target", target2, run_standard_tests), rpc::error::OK());
    telemetry_service->message(rpc::i_telemetry_service::info, "call_host_look_up_app_and_delete complete");
    ASSERT_EQ(target, target2);
    target = nullptr;
    target2 = nullptr;
    telemetry_service->message(rpc::i_telemetry_service::info, "app released");
}

TYPED_TEST(type_test_with_host, create_subordinate_zone)
{  
    rpc::shared_ptr<yyy::i_example> target;
    ASSERT_EQ(this->get_lib().get_example()->create_example_in_subordnate_zone(target, ++(*zone_gen)), rpc::error::OK());
}
