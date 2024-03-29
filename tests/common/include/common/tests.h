/*
 *   Copyright (c) 2024 Edward Boggis-Rolfe
 *   All rights reserved.
 */
#pragma once

#include <example/example.h>

void error(int x)
{
    if (!x)
        std::cerr << "bad test " << x << "\n";
}

#define ASSERT(x) error(x)

namespace marshalled_tests
{

    void standard_tests(xxx::i_foo& foo, bool enclave, const rpc::i_telemetry_service* telemetry_service)
    {
        {
            ASSERT(!foo.do_something_in_val(33));
        }
        {
            int val = 33;
            ASSERT(!foo.do_something_in_ref(val));
        }
        {
            int val = 33;
            ASSERT(!foo.do_something_in_by_val_ref(val));
        }
        {
            int val = 33;
            ASSERT(!foo.do_something_in_move_ref(std::move(val)));
        }
        {
            int val = 33;
            ASSERT(!foo.do_something_in_ptr(&val));
        }
        {
            int val = 0;
            ASSERT(!foo.do_something_out_val(val));
        }
        if (!enclave)
        {
            int* val = nullptr;
            ASSERT(!foo.do_something_out_ptr_ref(val));
            delete val;
        }
        if (!enclave)
        {
            int* val = nullptr;
            ASSERT(!foo.do_something_out_ptr_ptr(&val));
            delete val;
        }
        {
            int val = 32;
            ASSERT(!foo.do_something_in_out_ref(val));
        }
        {
            xxx::something_complicated val {33, "22"};
            ASSERT(!foo.give_something_complicated_val(val));
        }
        {
            xxx::something_complicated val {33, "22"};
            ASSERT(!foo.give_something_complicated_ref(val));
        }
        {
            xxx::something_complicated val {33, "22"};
            ASSERT(!foo.give_something_complicated_ref_val(val));
        }
        {
            xxx::something_complicated val {33, "22"};
            ASSERT(!foo.give_something_complicated_move_ref(std::move(val)));
        }
        {
            xxx::something_complicated val {33, "22"};
            ASSERT(!foo.give_something_complicated_ptr(&val));
        }
        {
            xxx::something_complicated val;
            ASSERT(!foo.recieve_something_complicated_ref(val));
            std::cout << "got " << val.string_val << "\n";
        }
        if (!enclave)
        {
            xxx::something_complicated* val = nullptr;
            ASSERT(!foo.recieve_something_complicated_ptr(val));
            std::cout << "got " << val->int_val << "\n";
            delete val;
        }
        {
            xxx::something_complicated val;
            val.int_val = 32;
            ASSERT(!foo.recieve_something_complicated_in_out_ref(val));
            std::cout << "got " << val.int_val << "\n";
        }
        {
            xxx::something_more_complicated val;
            val.map_val["22"] = xxx::something_complicated {33, "22"};
            ASSERT(!foo.give_something_more_complicated_val(val));
        }
        if (!enclave)
        {
            xxx::something_more_complicated val;
            val.map_val["22"] = xxx::something_complicated {33, "22"};
            ASSERT(!foo.give_something_more_complicated_ref(val));
        }
        {
            xxx::something_more_complicated val;
            val.map_val["22"] = xxx::something_complicated {33, "22"};
            ASSERT(!foo.give_something_more_complicated_move_ref(std::move(val)));
        }
        {
            xxx::something_more_complicated val;
            val.map_val["22"] = xxx::something_complicated {33, "22"};
            ASSERT(!foo.give_something_more_complicated_ref_val(val));
        }
        if (!enclave)
        {
            xxx::something_more_complicated val;
            val.map_val["22"] = xxx::something_complicated {33, "22"};
            ASSERT(!foo.give_something_more_complicated_ptr(&val));
        }
        if (!enclave)
        {
            xxx::something_more_complicated val;
            ASSERT(!foo.recieve_something_more_complicated_ref(val));
            std::cout << "got " << val.map_val.begin()->first << "\n";
        }
        if (!enclave)
        {
            xxx::something_more_complicated* val = nullptr;
            ASSERT(!foo.recieve_something_more_complicated_ptr(val));
            std::cout << "got " << val->map_val.begin()->first << "\n";
            delete val;
        }
        {
            xxx::something_more_complicated val;
            val.map_val["22"] = xxx::something_complicated {33, "22"};
            ASSERT(!foo.recieve_something_more_complicated_in_out_ref(val));
            std::cout << "got " << val.map_val.begin()->first << "\n";
        }
        {
            int val1 = 1;
            int val2 = 2;
            ASSERT(!foo.do_multi_val(val1, val2));
        }
        {
            xxx::something_more_complicated val1;
            xxx::something_more_complicated val2;
            val1.map_val["22"] = xxx::something_complicated {33, "22"};
            val2.map_val["22"] = xxx::something_complicated {33, "22"};
            ASSERT(!foo.do_multi_complicated_val(val1, val2));
        }
    }

    void remote_tests(bool use_host_in_child, rpc::shared_ptr<yyy::i_example> example_ptr, const rpc::i_telemetry_service* telemetry_service)
    {
        int val = 0;
        example_ptr->add(1, 2, val);

        {
            // check the creation of an object that is passed back via interface
            rpc::shared_ptr<xxx::i_foo> foo;
            example_ptr->create_foo(foo);
            foo->do_something_in_val(22);

            // test casting logic
            auto i_bar_ptr = rpc::dynamic_pointer_cast<xxx::i_bar>(foo);
            assert(i_bar_ptr == nullptr);

            // test recursive interface passing
            rpc::shared_ptr<xxx::i_foo> other_foo;
            int err_code = foo->recieve_interface(other_foo);
            if (err_code != rpc::error::OK())
            {
                std::cout << "create_foo failed\n";
            }
            else
            {
                other_foo->do_something_in_val(22);
            }

            if(use_host_in_child)
            {
                rpc::shared_ptr<xxx::i_baz> b(new baz(telemetry_service));
                err_code = foo->call_baz_interface(b);
            }

            ASSERT(foo->exception_test() == rpc::error::EXCEPTION());
        }
        {
            rpc::shared_ptr<xxx::i_baz> i_baz_ptr;
            example_ptr->create_multiple_inheritance(i_baz_ptr);
            auto i_bar_ptr1 = rpc::dynamic_pointer_cast<xxx::i_bar>(i_baz_ptr);
            assert(i_bar_ptr1 != nullptr);
            auto i_baz_ptr2 = rpc::dynamic_pointer_cast<xxx::i_baz>(i_bar_ptr1);
            assert(i_baz_ptr2 != nullptr);
            auto i_bar_ptr2 = rpc::dynamic_pointer_cast<xxx::i_bar>(i_baz_ptr2);
            assert(i_bar_ptr2 != nullptr);
        }
    }
}