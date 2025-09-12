/*
 *   Copyright (c) 2025 Edward Boggis-Rolfe
 *   All rights reserved.
 */
#pragma once

#include <example/example.h>
#include <rpc/error_codes.h>
#include <rpc/logger.h>
#include <rpc/coroutine_support.h>
#include "gtest/gtest.h"

#ifdef BUILD_COROUTINE
#define ASSERT_ERROR_CODE(x) CORO_ASSERT_EQ(CO_AWAIT x, rpc::error::OK())
#define TEST_RETURN_VAL CORO_TASK(bool)
#define TEST_RETURN_SUCCESFUL CO_RETURN true
#define TEST_SYNC_WAIT(x) ASSERT_EQ(SYNC_WAIT(x), true)
#else
#define ASSERT_ERROR_CODE(x) ASSERT_EQ(x, rpc::error::OK())
#define TEST_RETURN_VAL void
#define TEST_RETURN_SUCCESFUL
#define TEST_SYNC_WAIT(x) SYNC_WAIT(x)
#endif

namespace marshalled_tests
{
    TEST_RETURN_VAL standard_tests(xxx::i_foo& foo, bool enclave)
    {
        {
            ASSERT_ERROR_CODE(foo.do_something_in_val(33));
        }
        {
            int val = 33;
            ASSERT_ERROR_CODE(foo.do_something_in_ref(val));
        }
        {
            int val = 33;
            ASSERT_ERROR_CODE(foo.do_something_in_by_val_ref(val));
        }
        {
            int val = 33;
            ASSERT_ERROR_CODE(foo.do_something_in_move_ref(std::move(val)));
        }
        {
            int val = 33;
            ASSERT_ERROR_CODE(foo.do_something_in_ptr(&val));
        }
        {
            int val = 0;
            ASSERT_ERROR_CODE(foo.do_something_out_val(val));
        }
        if (!enclave)
        {
            int* val = nullptr;
            ASSERT_ERROR_CODE(foo.do_something_out_ptr_ref(val));
            delete val;
        }
        if (!enclave)
        {
            int* val = nullptr;
            ASSERT_ERROR_CODE(foo.do_something_out_ptr_ptr(&val));
            delete val;
        }
        {
            int val = 32;
            ASSERT_ERROR_CODE(foo.do_something_in_out_ref(val));
        }
        {
            xxx::something_complicated val{33, "22"};
            ASSERT_ERROR_CODE(foo.give_something_complicated_val(val));
        }
        {
            xxx::something_complicated val{33, "22"};
            ASSERT_ERROR_CODE(foo.give_something_complicated_ref(val));
        }
        {
            xxx::something_complicated val{33, "22"};
            ASSERT_ERROR_CODE(foo.give_something_complicated_ref_val(val));
        }
        {
            xxx::something_complicated val{33, "22"};
            ASSERT_ERROR_CODE(foo.give_something_complicated_move_ref(std::move(val)));
        }
        {
            xxx::something_complicated val{33, "22"};
            ASSERT_ERROR_CODE(foo.give_something_complicated_ptr(&val));
        }
        {
            xxx::something_complicated val;
            ASSERT_ERROR_CODE(foo.receive_something_complicated_ref(val));
            RPC_INFO("got {}", val.string_val);
        }
        if (!enclave)
        {
            xxx::something_complicated* val = nullptr;
            ASSERT_ERROR_CODE(foo.receive_something_complicated_ptr(val));
            RPC_INFO("got {}", val->int_val);
            delete val;
        }
        {
            xxx::something_complicated val;
            val.int_val = 32;
            ASSERT_ERROR_CODE(foo.receive_something_complicated_in_out_ref(val));
            RPC_INFO("got {}", val.int_val);
        }
        {
            xxx::something_more_complicated val;
            val.map_val["22"] = xxx::something_complicated{33, "22"};
            ASSERT_ERROR_CODE(foo.give_something_more_complicated_val(val));
        }
        if (!enclave)
        {
            xxx::something_more_complicated val;
            val.map_val["22"] = xxx::something_complicated{33, "22"};
            ASSERT_ERROR_CODE(foo.give_something_more_complicated_ref(val));
        }
        {
            xxx::something_more_complicated val;
            val.map_val["22"] = xxx::something_complicated{33, "22"};
            ASSERT_ERROR_CODE(foo.give_something_more_complicated_move_ref(std::move(val)));
        }
        {
            xxx::something_more_complicated val;
            val.map_val["22"] = xxx::something_complicated{33, "22"};
            ASSERT_ERROR_CODE(foo.give_something_more_complicated_ref_val(val));
        }
        if (!enclave)
        {
            xxx::something_more_complicated val;
            val.map_val["22"] = xxx::something_complicated{33, "22"};
            ASSERT_ERROR_CODE(foo.give_something_more_complicated_ptr(&val));
        }
        if (!enclave)
        {
            xxx::something_more_complicated val;
            ASSERT_ERROR_CODE(foo.receive_something_more_complicated_ref(val));
            if (val.map_val.size() == 0)
            {
                RPC_ERROR("receive_something_more_complicated_ref returned no data");
            }
            else
            {
                RPC_INFO("got {}", val.map_val.begin()->first);
            }
        }
        if (!enclave)
        {
            xxx::something_more_complicated* val = nullptr;
            ASSERT_ERROR_CODE(foo.receive_something_more_complicated_ptr(val));
            if (val->map_val.size() == 0)
            {
                RPC_ERROR("receive_something_more_complicated_ref returned no data");
            }
            else
            {
                RPC_INFO("got {}", val->map_val.begin()->first);
            }
            delete val;
        }
        {
            xxx::something_more_complicated val;
            val.map_val["22"] = xxx::something_complicated{33, "22"};
            ASSERT_ERROR_CODE(foo.receive_something_more_complicated_in_out_ref(val));
            if (val.map_val.size() == 0)
            {
                RPC_ERROR("receive_something_more_complicated_in_out_ref returned no data");
            }
            else
            {
                RPC_INFO("got {}", val.map_val.begin()->first);
            }
        }
        {
            int val1 = 1;
            int val2 = 2;
            ASSERT_ERROR_CODE(foo.do_multi_val(val1, val2));
        }
        {
            xxx::something_more_complicated val1;
            xxx::something_more_complicated val2;
            val1.map_val["22"] = xxx::something_complicated{33, "22"};
            val2.map_val["22"] = xxx::something_complicated{33, "22"};
            ASSERT_ERROR_CODE(foo.do_multi_complicated_val(val1, val2));
        }
        TEST_RETURN_SUCCESFUL;
    }

    CORO_TASK(void) remote_tests(bool use_host_in_child, rpc::shared_ptr<yyy::i_example> example_ptr, rpc::zone zone_id)
    {
        int val = 0;
        CO_AWAIT example_ptr->add(1, 2, val);

        {
            // check the creation of an object that is passed back via interface
            rpc::shared_ptr<xxx::i_foo> foo;
            CO_AWAIT example_ptr->create_foo(foo);
            CO_AWAIT foo->do_something_in_val(22);

            // test casting logic
            auto i_bar_ptr = CO_AWAIT rpc::dynamic_pointer_cast<xxx::i_bar>(foo);
            RPC_ASSERT(i_bar_ptr == nullptr);

            // test recursive interface passing
            rpc::shared_ptr<xxx::i_foo> other_foo;
            int err_code = CO_AWAIT foo->receive_interface(other_foo);
            if (err_code != rpc::error::OK())
            {
                RPC_ERROR("create_foo failed");
            }
            else
            {
                CO_AWAIT other_foo->do_something_in_val(22);
            }

            if (use_host_in_child)
            {
                rpc::shared_ptr<xxx::i_baz> b(new baz(zone_id));
                err_code = CO_AWAIT foo->call_baz_interface(b);
            }

            if (CO_AWAIT foo->exception_test() != rpc::error::EXCEPTION())
            {
                RPC_ERROR("exception_test failed");
            }
        }
        {
            rpc::shared_ptr<xxx::i_baz> i_baz_ptr;
            CO_AWAIT example_ptr->create_multiple_inheritance(i_baz_ptr);
            // repeat twice
            for (int i = 0; i < 2; i++)
            {
                auto i_bar_ptr1 = CO_AWAIT rpc::dynamic_pointer_cast<xxx::i_bar>(i_baz_ptr);
                RPC_ASSERT(i_bar_ptr1 != nullptr);
                auto i_baz_ptr2 = CO_AWAIT rpc::dynamic_pointer_cast<xxx::i_baz>(i_bar_ptr1);
                RPC_ASSERT(i_baz_ptr2 == i_baz_ptr);
                auto i_bar_ptr2 = CO_AWAIT rpc::dynamic_pointer_cast<xxx::i_bar>(i_baz_ptr2);
                RPC_ASSERT(i_bar_ptr2 == i_bar_ptr1);
                auto i_foo = CO_AWAIT rpc::dynamic_pointer_cast<xxx::i_foo>(i_baz_ptr2);
                RPC_ASSERT(i_foo == nullptr);
            }
        }
    }
}
