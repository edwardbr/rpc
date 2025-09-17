/*
 *   Copyright (c) 2025 Edward Boggis-Rolfe
 *   All rights reserved.
 */
#pragma once

#include <example/example.h>
#include <rpc/rpc.h>
#include "gtest/gtest.h"

namespace marshalled_tests
{
    CORO_TASK(bool) standard_tests(xxx::i_foo& foo, bool enclave)
    {
        {
            CORO_ASSERT_EQ(CO_AWAIT foo.do_something_in_val(33), rpc::error::OK());
        }
        {
            int val = 33;
            CORO_ASSERT_EQ(CO_AWAIT foo.do_something_in_ref(val), rpc::error::OK());
        }
        {
            int val = 33;
            CORO_ASSERT_EQ(CO_AWAIT foo.do_something_in_by_val_ref(val), rpc::error::OK());
        }
        {
            int val = 33;
            CORO_ASSERT_EQ(CO_AWAIT foo.do_something_in_move_ref(std::move(val)), rpc::error::OK());
        }
        {
            int val = 33;
            CORO_ASSERT_EQ(CO_AWAIT foo.do_something_in_ptr(&val), rpc::error::OK());
        }
        {
            int val = 0;
            CORO_ASSERT_EQ(CO_AWAIT foo.do_something_out_val(val), rpc::error::OK());
        }
        if (!enclave)
        {
            int* val = nullptr;
            CORO_ASSERT_EQ(CO_AWAIT foo.do_something_out_ptr_ref(val), rpc::error::OK());
            delete val;
        }
        if (!enclave)
        {
            int* val = nullptr;
            CORO_ASSERT_EQ(CO_AWAIT foo.do_something_out_ptr_ptr(&val), rpc::error::OK());
            delete val;
        }
        {
            int val = 32;
            CORO_ASSERT_EQ(CO_AWAIT foo.do_something_in_out_ref(val), rpc::error::OK());
        }
        {
            xxx::something_complicated val{33, "22"};
            CORO_ASSERT_EQ(CO_AWAIT foo.give_something_complicated_val(val), rpc::error::OK());
        }
        {
            xxx::something_complicated val{33, "22"};
            CORO_ASSERT_EQ(CO_AWAIT foo.give_something_complicated_ref(val), rpc::error::OK());
        }
        {
            xxx::something_complicated val{33, "22"};
            CORO_ASSERT_EQ(CO_AWAIT foo.give_something_complicated_ref_val(val), rpc::error::OK());
        }
        {
            xxx::something_complicated val{33, "22"};
            CORO_ASSERT_EQ(CO_AWAIT foo.give_something_complicated_move_ref(std::move(val)), rpc::error::OK());
        }
        {
            xxx::something_complicated val{33, "22"};
            CORO_ASSERT_EQ(CO_AWAIT foo.give_something_complicated_ptr(&val), rpc::error::OK());
        }
        {
            xxx::something_complicated val;
            CORO_ASSERT_EQ(CO_AWAIT foo.receive_something_complicated_ref(val), rpc::error::OK());
            RPC_INFO("got {}", val.string_val);
        }
        if (!enclave)
        {
            xxx::something_complicated* val = nullptr;
            CORO_ASSERT_EQ(CO_AWAIT foo.receive_something_complicated_ptr(val), rpc::error::OK());
            RPC_INFO("got {}", val->int_val);
            delete val;
        }
        {
            xxx::something_complicated val;
            val.int_val = 32;
            CORO_ASSERT_EQ(CO_AWAIT foo.receive_something_complicated_in_out_ref(val), rpc::error::OK());
            RPC_INFO("got {}", val.int_val);
        }
        {
            xxx::something_more_complicated val;
            val.map_val["22"] = xxx::something_complicated{33, "22"};
            CORO_ASSERT_EQ(CO_AWAIT foo.give_something_more_complicated_val(val), rpc::error::OK());
        }
        if (!enclave)
        {
            xxx::something_more_complicated val;
            val.map_val["22"] = xxx::something_complicated{33, "22"};
            CORO_ASSERT_EQ(CO_AWAIT foo.give_something_more_complicated_ref(val), rpc::error::OK());
        }
        {
            xxx::something_more_complicated val;
            val.map_val["22"] = xxx::something_complicated{33, "22"};
            CORO_ASSERT_EQ(CO_AWAIT foo.give_something_more_complicated_move_ref(std::move(val)), rpc::error::OK());
        }
        {
            xxx::something_more_complicated val;
            val.map_val["22"] = xxx::something_complicated{33, "22"};
            CORO_ASSERT_EQ(CO_AWAIT foo.give_something_more_complicated_ref_val(val), rpc::error::OK());
        }
        if (!enclave)
        {
            xxx::something_more_complicated val;
            val.map_val["22"] = xxx::something_complicated{33, "22"};
            CORO_ASSERT_EQ(CO_AWAIT foo.give_something_more_complicated_ptr(&val), rpc::error::OK());
        }
        if (!enclave)
        {
            xxx::something_more_complicated val;
            CORO_ASSERT_EQ(CO_AWAIT foo.receive_something_more_complicated_ref(val), rpc::error::OK());
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
            CORO_ASSERT_EQ(CO_AWAIT foo.receive_something_more_complicated_ptr(val), rpc::error::OK());
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
            CORO_ASSERT_EQ(CO_AWAIT foo.receive_something_more_complicated_in_out_ref(val), rpc::error::OK());
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
            CORO_ASSERT_EQ(CO_AWAIT foo.do_multi_val(val1, val2), rpc::error::OK());
        }
        {
            xxx::something_more_complicated val1;
            xxx::something_more_complicated val2;
            val1.map_val["22"] = xxx::something_complicated{33, "22"};
            val2.map_val["22"] = xxx::something_complicated{33, "22"};
            CORO_ASSERT_EQ(CO_AWAIT foo.do_multi_complicated_val(val1, val2), rpc::error::OK());
        }
        CO_RETURN true;
    }

    CORO_TASK(bool) remote_tests(bool use_host_in_child, rpc::shared_ptr<yyy::i_example> example_ptr)
    {
        int val = 0;
        CO_AWAIT example_ptr->add(1, 2, val);

        {
            // check the creation of an object that is passed back via interface
            rpc::shared_ptr<xxx::i_foo> foo;
            CORO_ASSERT_EQ(CO_AWAIT example_ptr->create_foo(foo), rpc::error::OK());
            CORO_ASSERT_EQ(CO_AWAIT foo->do_something_in_val(22), rpc::error::OK());

            // test casting logic
            auto i_bar_ptr = CO_AWAIT rpc::dynamic_pointer_cast<xxx::i_bar>(foo);
            CORO_ASSERT_EQ(i_bar_ptr, nullptr);

            // test recursive interface passing
            rpc::shared_ptr<xxx::i_foo> other_foo;
            int err_code = CO_AWAIT foo->receive_interface(other_foo);
            if (err_code != rpc::error::OK())
            {
                RPC_ERROR("create_foo failed");
            }
            else
            {
                CORO_ASSERT_EQ(CO_AWAIT other_foo->do_something_in_val(22), rpc::error::OK());
            }

            if (use_host_in_child)
            {
                rpc::shared_ptr<xxx::i_baz> b(new baz());
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
                CORO_ASSERT_NE(i_bar_ptr1, nullptr);
                auto i_baz_ptr2 = CO_AWAIT rpc::dynamic_pointer_cast<xxx::i_baz>(i_bar_ptr1);
                CORO_ASSERT_EQ(i_baz_ptr2, i_baz_ptr);
                auto i_bar_ptr2 = CO_AWAIT rpc::dynamic_pointer_cast<xxx::i_bar>(i_baz_ptr2);
                CORO_ASSERT_EQ(i_bar_ptr2, i_bar_ptr1);
                auto i_foo = CO_AWAIT rpc::dynamic_pointer_cast<xxx::i_foo>(i_baz_ptr2);
                CORO_ASSERT_EQ(i_foo, nullptr);
            }
        }
        CO_RETURN true;
    }
}
