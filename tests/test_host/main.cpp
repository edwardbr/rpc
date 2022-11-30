#include <iostream>
#include <unordered_map>

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

#include <example_shared_proxy.cpp>
#include <example_shared_stub.cpp>

#include <example_import_proxy.cpp>
#include <example_import_stub.cpp>

#include <example_proxy.cpp>
#include <example_stub.cpp>

#include <rpc/basic_service_proxies.h>
#include <host_telemetry_service.h>

#include "gmock/gmock.h"
#include "gtest/gtest.h"


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
auto enclave_path = "./marshal_test_enclave.signed.dll";
#else         // Linux
auto enclave_path = "./libmarshal_test_enclave.signed.so";
#endif

rpc::weak_ptr<rpc::service> current_host_service;

const rpc::i_telemetry_service* telemetry_service = nullptr;
int* zone_gen = nullptr;

class host : 
    public yyy::i_host,
    public rpc::enable_shared_from_this<host>
{
    void* get_address() const override { return (void*)this; }
    const rpc::casting_interface* query_interface(uint64_t interface_id) const override
    {
        if (yyy::i_host::id == interface_id)
            return static_cast<const yyy::i_host*>(this);
        return nullptr;
    }
    error_code create_enclave(rpc::shared_ptr<yyy::i_example>& target) override
    {
        auto err_code = rpc::enclave_service_proxy::create(
            ++(*zone_gen), 
            enclave_path, 
            current_host_service.lock(),
            rpc::shared_ptr<yyy::i_host>(), //shared_from_this(),
            target, 
            telemetry_service);
        return err_code;
    };
};

struct in_memory_setup
{
    rpc::shared_ptr<host_telemetry_service> tm;

    rpc::shared_ptr<yyy::i_host> i_host_ptr;
    rpc::shared_ptr<yyy::i_example> i_example_ptr;

    const bool has_enclave = false;
    bool use_host_in_child = false;

    int zone_gen_ = 0;

    virtual void SetUp()
    {   
        zone_gen = &zone_gen_;
        tm = rpc::make_shared<host_telemetry_service>();
        {
            i_host_ptr = rpc::shared_ptr<yyy::i_host> (new host());
            i_example_ptr = rpc::shared_ptr<yyy::i_example> (new example(tm.get(), use_host_in_child ? i_host_ptr : nullptr));
        }
    }

    virtual void TearDown()
    {
        i_host_ptr = nullptr;
        i_example_ptr = nullptr;
        tm = nullptr;
        zone_gen = nullptr;
    }
};
struct in_memory_setup_with_host_in_enclave : public in_memory_setup
{
    in_memory_setup_with_host_in_enclave() : in_memory_setup()
    {
        use_host_in_child = true;
    }
};

struct inproc_setup
{
    rpc::shared_ptr<host_telemetry_service> tm;

    rpc::shared_ptr<rpc::service> root_service;
    rpc::shared_ptr<rpc::child_service> child_service;
    rpc::shared_ptr<yyy::i_host> i_host_ptr;
    rpc::shared_ptr<yyy::i_example> i_example_ptr;

    const bool has_enclave = true;
    bool use_host_in_child = false;

    int zone_gen_ = 0;

    virtual void SetUp()
    {
        zone_gen = &zone_gen_;
        tm = rpc::make_shared<host_telemetry_service>();
        root_service = rpc::make_shared<rpc::service>(++zone_gen_);
        current_host_service = root_service;
        child_service = rpc::make_shared<rpc::child_service>(++zone_gen_);

        rpc::interface_descriptor host_encap {};
        rpc::interface_descriptor example_encap {};

        // create a proxy to the rpc::service hosting the child service
        auto service_proxy_to_host
            = rpc::local_service_proxy::create(root_service, child_service, tm.get(), false);

        // create a proxy to the rpc::service that contains the example object
        auto service_proxy_to_child
            = rpc::local_child_service_proxy::create(child_service, root_service, tm.get());

        {
            // create a host implementation object
            rpc::shared_ptr<yyy::i_host> hst(new host());

            // register implementation to the root service and held by a stub
            // note! There is a memory leak if the interface_descriptor object is not bound to a proxy
            host_encap = rpc::create_interface_stub(*root_service, hst);

            // simple test to check that we can get a useful local interface based on type and object id
            auto example_from_cast = root_service->get_local_interface<yyy::i_host>(host_encap.object_id);
            EXPECT_EQ(example_from_cast, hst);
        }

        {
            // create the example object implementation
            rpc::shared_ptr<yyy::i_example> remote_example(new example(tm.get(), use_host_in_child ? i_host_ptr : nullptr));

            example_encap
                = rpc::create_interface_stub(*child_service, remote_example);

            // simple test to check that we can get a usefule local interface based on type and object id
            auto example_from_cast
                = child_service->get_local_interface<yyy::i_example>(example_encap.object_id);
            EXPECT_EQ(example_from_cast, remote_example);
        }

        ASSERT(!rpc::create_interface_proxy(service_proxy_to_host, host_encap, i_host_ptr));

        ASSERT(!rpc::create_interface_proxy(service_proxy_to_child, example_encap, i_example_ptr));
    }

    virtual void TearDown()
    {
        i_example_ptr = nullptr;
        child_service = nullptr;
        i_host_ptr = nullptr;
        root_service = nullptr;
        tm = nullptr;
        zone_gen = nullptr;
    }

    rpc::shared_ptr<yyy::i_example> create_new_zone()
    {        
        rpc::shared_ptr<rpc::child_service> new_service = rpc::make_shared<rpc::child_service>(++zone_gen_);
        
        // create a proxy to the rpc::service hosting the child service
        auto service_proxy_to_host
            = rpc::local_service_proxy::create(root_service, new_service, tm.get(), true);

        // create a proxy to the rpc::service that contains the example object
        auto service_proxy_to_child = rpc::local_child_service_proxy::create(new_service, root_service, tm.get());

        // create the example object implementation
        rpc::shared_ptr<yyy::i_example> remote_example(new example(tm.get(), use_host_in_child ? i_host_ptr : nullptr));

        rpc::interface_descriptor example_encap
            = rpc::create_interface_stub(*new_service, remote_example);

        // simple test to check that we can get a usefule local interface based on type and object id
        auto example_from_cast
            = new_service->get_local_interface<yyy::i_example>(example_encap.object_id);
        EXPECT_EQ(example_from_cast, remote_example);

        rpc::shared_ptr<yyy::i_example> example_relay_ptr;
        ASSERT(!rpc::create_interface_proxy(service_proxy_to_child, example_encap, example_relay_ptr));                
        return example_relay_ptr;
    }
};

struct inproc_setup_with_host_in_enclave : public inproc_setup
{
    inproc_setup_with_host_in_enclave() : inproc_setup()
    {
        use_host_in_child = true;
    }
};

struct enclave_setup
{
    rpc::shared_ptr<host_telemetry_service> tm;

    rpc::shared_ptr<rpc::service> root_service;
    rpc::shared_ptr<yyy::i_host> i_host_ptr;
    rpc::shared_ptr<yyy::i_example> i_example_ptr;

    const bool has_enclave = true;
    bool use_host_in_child = false;

    int zone_gen_ = 0;

    virtual void SetUp()
    {
        zone_gen = &zone_gen_;
        tm = rpc::make_shared<host_telemetry_service>();
        root_service = rpc::make_shared<rpc::service>(++zone_gen_);
        current_host_service = root_service;
        
        i_host_ptr = rpc::shared_ptr<yyy::i_host> (new host());

        auto err_code = rpc::enclave_service_proxy::create(
            ++zone_gen_, 
            enclave_path, 
            root_service, 
            use_host_in_child ? i_host_ptr : nullptr, 
            i_example_ptr,
            tm.get());

        ASSERT(!err_code);
    }

    virtual void TearDown()
    {
        i_example_ptr = nullptr;
        i_host_ptr = nullptr;
        root_service = nullptr;
        tm = nullptr;
        zone_gen = nullptr;
    }

    rpc::shared_ptr<yyy::i_example> create_new_zone()
    {
        rpc::shared_ptr<yyy::i_example> example_relay_ptr;
        auto err_code = rpc::enclave_service_proxy::create(
            ++zone_gen_, 
            enclave_path, 
            root_service, 
            use_host_in_child ? i_host_ptr : nullptr, 
            example_relay_ptr,
            telemetry_service);
        ASSERT(!err_code);
        return example_relay_ptr;
    }
};

struct enclave_setup_with_host_in_enclave : public enclave_setup
{
    enclave_setup_with_host_in_enclave() : enclave_setup()
    {
        use_host_in_child = true;
    }
};


template <class T>
class type_test : 
    public testing::Test
{
    public:
    T lib_;

    void SetUp() override {
        lib_.SetUp();
    }
    void TearDown() override {
        lib_.TearDown();
    }    
};

typedef Types<
    in_memory_setup, 
    in_memory_setup_with_host_in_enclave, 
    inproc_setup, 
    inproc_setup_with_host_in_enclave, 
    enclave_setup, 
    enclave_setup_with_host_in_enclave> local_implementations;
TYPED_TEST_SUITE(type_test, local_implementations);

TYPED_TEST(type_test, initialisation_test)
{
}

TYPED_TEST(type_test, standard_tests)
{
    auto* tm = this->lib_.tm.get();
    foo f(tm);
    standard_tests(f, this->lib_.has_enclave, tm);
}



TYPED_TEST(type_test, dyanmic_cast_tests)
{
    auto f = rpc::shared_ptr<xxx::i_foo>(new foo(this->lib_.tm.get()));

    rpc::shared_ptr<marshalled_tests::xxx::i_baz> baz;
    ASSERT_EQ(f->create_baz_interface(baz), 0);
    ASSERT_EQ(f->call_baz_interface(nullptr), 0); // feed in a nullptr
    ASSERT_EQ(f->call_baz_interface(baz), 0);     // feed back to the implementation

    auto x = rpc::dynamic_pointer_cast<marshalled_tests::xxx::i_baz>(baz);
    ASSERT_NE(x, nullptr);
    auto y = rpc::dynamic_pointer_cast<marshalled_tests::xxx::i_bar>(baz);
    ASSERT_NE(y, nullptr);
    y->do_something_else(1);
    auto z = rpc::dynamic_pointer_cast<marshalled_tests::xxx::i_foo>(baz);
    ASSERT_NE(y, nullptr);
}


template <class T>
using remote_type_test = type_test<T>;


typedef Types<
    inproc_setup, 
    inproc_setup_with_host_in_enclave, 
    enclave_setup, 
    enclave_setup_with_host_in_enclave> remote_implementations;
TYPED_TEST_SUITE(remote_type_test, remote_implementations);

TYPED_TEST(remote_type_test, remote_standard_tests)
{    
    rpc::shared_ptr<xxx::i_foo> i_foo_ptr;
    ASSERT_EQ(this->lib_.i_example_ptr->create_foo(i_foo_ptr), 0);
    standard_tests(*i_foo_ptr, true, telemetry_service);
}

TYPED_TEST(remote_type_test, remote_tests)
{    
    auto* tm = this->lib_.tm.get();
    remote_tests(this->lib_.i_example_ptr, tm);
}


TYPED_TEST(remote_type_test, create_new_zone)
{
    rpc::shared_ptr<xxx::i_foo> i_foo_relay_ptr;
    auto example_relay_ptr = this->lib_.create_new_zone();
}

TYPED_TEST(remote_type_test, dyanmic_cast_tests)
{
    rpc::shared_ptr<xxx::i_foo> i_foo_ptr;
    ASSERT_EQ(this->lib_.i_example_ptr->create_foo(i_foo_ptr), 0);

    rpc::shared_ptr<marshalled_tests::xxx::i_baz> baz;
    i_foo_ptr->create_baz_interface(baz);
    i_foo_ptr->call_baz_interface(nullptr); // feed in a nullptr
    i_foo_ptr->call_baz_interface(baz);     // feed back to the implementation

    auto x = rpc::dynamic_pointer_cast<marshalled_tests::xxx::i_baz>(baz);
    ASSERT_NE(x, nullptr);
    auto y = rpc::dynamic_pointer_cast<marshalled_tests::xxx::i_bar>(baz);
    ASSERT_NE(y, nullptr);
    y->do_something_else(1);
    auto z = rpc::dynamic_pointer_cast<marshalled_tests::xxx::i_foo>(baz);
    ASSERT_EQ(z, nullptr);
}

TYPED_TEST(remote_type_test, bounce_baz_between_two_interfaces)
{
    rpc::shared_ptr<xxx::i_foo> i_foo_ptr;
    ASSERT_EQ(this->lib_.i_example_ptr->create_foo(i_foo_ptr), 0);

    rpc::shared_ptr<xxx::i_foo> i_foo_relay_ptr;
    auto example_relay_ptr = this->lib_.create_new_zone();
    example_relay_ptr->create_foo(i_foo_relay_ptr);

    rpc::shared_ptr<marshalled_tests::xxx::i_baz> baz;
    i_foo_ptr->create_baz_interface(baz);
    i_foo_relay_ptr->call_baz_interface(baz);
}

// check for null
TYPED_TEST(remote_type_test, check_for_null_interface)
{
    rpc::shared_ptr<xxx::i_foo> i_foo_ptr;
    ASSERT_EQ(this->lib_.i_example_ptr->create_foo(i_foo_ptr), 0);
    rpc::shared_ptr<marshalled_tests::xxx::i_baz> c;
    i_foo_ptr->get_interface(c);
    assert(c == nullptr);
}

TYPED_TEST(remote_type_test, check_for_multiple_sets)
{
    rpc::shared_ptr<xxx::i_foo> i_foo_ptr;
    ASSERT_EQ(this->lib_.i_example_ptr->create_foo(i_foo_ptr), 0);

    auto b = rpc::make_shared<marshalled_tests::baz>(this->lib_.tm.get());
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
    rpc::shared_ptr<xxx::i_foo> i_foo_ptr;
    ASSERT_EQ(this->lib_.i_example_ptr->create_foo(i_foo_ptr), 0);

    rpc::shared_ptr<marshalled_tests::xxx::i_baz> c;
    auto b = rpc::make_shared<marshalled_tests::baz>(this->lib_.tm.get());
    i_foo_ptr->set_interface(b);
    i_foo_ptr->get_interface(c);
    i_foo_ptr->set_interface(nullptr);
    assert(b == c);
}

TYPED_TEST(remote_type_test, check_for_set_multiple_inheritance)
{
    auto ret = this->lib_.i_example_ptr->give_interface(
        rpc::shared_ptr<xxx::i_baz>(new multiple_inheritance(this->lib_.tm.get())));
    assert(ret == rpc::error::OK());
}

TYPED_TEST(remote_type_test, check_for_call_enclave_zone)
{
    auto h = rpc::make_shared<host>();
    auto ret = this->lib_.i_example_ptr->call_create_enclave_val(h);
    assert(ret == rpc::error::OK());
}

/*TYPED_TEST(remote_type_test, check_for_call_inproc_zone)
{
    auto h = rpc::make_shared<host>();
    auto ret = this->lib_.i_example_ptr->call_create_enclave_val(h);
    assert(ret == rpc::error::OK());
}*/