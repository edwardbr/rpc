#pragma once

#include <example/example.h>

void error(int x)
{
    if (x)
        std::cerr << "bad test " << x << "\n";
}

#define ASSERT(x) error(x)

namespace marshalled_tests
{

    void standard_tests(xxx::i_foo& foo, bool enclave)
    {
        error_code ret = 0;
        {
            ASSERT(foo.do_something_in_val(33));
        }
        {
            int val = 33;
            ASSERT(foo.do_something_in_ref(val));
        }
        {
            int val = 33;
            ASSERT(foo.do_something_in_by_val_ref(val));
        }
        {
            int val = 33;
            ASSERT(foo.do_something_in_move_ref(std::move(val)));
        }
        {
            int val = 33;
            ASSERT(foo.do_something_in_ptr(&val));
        }
        {
            int val = 0;
            ASSERT(foo.do_something_out_val(val));
        }
        if (!enclave)
        {
            int* val = nullptr;
            ASSERT(foo.do_something_out_ptr_ref(val));
            delete val;
        }
        if (!enclave)
        {
            int* val = nullptr;
            ASSERT(foo.do_something_out_ptr_ptr(&val));
            delete val;
        }
        {
            int val = 32;
            ASSERT(foo.do_something_in_out_ref(val));
        }
        {
            xxx::something_complicated val {33, "22"};
            ASSERT(foo.give_something_complicated_val(val));
        }
        {
            xxx::something_complicated val {33, "22"};
            ASSERT(foo.give_something_complicated_ref(val));
        }
        {
            xxx::something_complicated val {33, "22"};
            ASSERT(foo.give_something_complicated_ref_val(val));
        }
        {
            xxx::something_complicated val {33, "22"};
            ASSERT(foo.give_something_complicated_move_ref(std::move(val)));
        }
        {
            xxx::something_complicated val {33, "22"};
            ASSERT(foo.give_something_complicated_ptr(&val));
        }
        {
            xxx::something_complicated val;
            ASSERT(foo.recieve_something_complicated_ref(val));
            std::cout << "got " << val.string_val << "\n";
        }
        if (!enclave)
        {
            xxx::something_complicated* val = nullptr;
            ASSERT(foo.recieve_something_complicated_ptr(val));
            std::cout << "got " << val->int_val << "\n";
            delete val;
        }
        {
            xxx::something_complicated val;
            val.int_val = 32;
            ASSERT(foo.recieve_something_complicated_in_out_ref(val));
            std::cout << "got " << val.int_val << "\n";
        }
        {
            xxx::something_more_complicated val;
            val.map_val["22"] = xxx::something_complicated {33, "22"};
            ASSERT(foo.give_something_more_complicated_val(val));
        }
        if (!enclave)
        {
            xxx::something_more_complicated val;
            val.map_val["22"] = xxx::something_complicated {33, "22"};
            ASSERT(foo.give_something_more_complicated_ref(val));
        }
        {
            xxx::something_more_complicated val;
            val.map_val["22"] = xxx::something_complicated {33, "22"};
            ASSERT(foo.give_something_more_complicated_move_ref(std::move(val)));
        }
        {
            xxx::something_more_complicated val;
            val.map_val["22"] = xxx::something_complicated {33, "22"};
            ASSERT(foo.give_something_more_complicated_ref_val(val));
        }
        if (!enclave)
        {
            xxx::something_more_complicated val;
            val.map_val["22"] = xxx::something_complicated {33, "22"};
            ASSERT(foo.give_something_more_complicated_ptr(&val));
        }
        if (!enclave)
        {
            xxx::something_more_complicated val;
            ASSERT(foo.recieve_something_more_complicated_ref(val));
            std::cout << "got " << val.map_val.begin()->first << "\n";
        }
        if (!enclave)
        {
            xxx::something_more_complicated* val = nullptr;
            ASSERT(foo.recieve_something_more_complicated_ptr(val));
            std::cout << "got " << val->map_val.begin()->first << "\n";
            delete val;
        }
        {
            xxx::something_more_complicated val;
            val.map_val["22"] = xxx::something_complicated {33, "22"};
            ASSERT(foo.recieve_something_more_complicated_in_out_ref(val));
            std::cout << "got " << val.map_val.begin()->first << "\n";
        }
        {
            int val1 = 1;
            int val2 = 2;
            ASSERT(foo.do_multi_val(val1, val2));
        }
        {
            xxx::something_more_complicated val1;
            xxx::something_more_complicated val2;
            val1.map_val["22"] = xxx::something_complicated {33, "22"};
            val2.map_val["22"] = xxx::something_complicated {33, "22"};
            ASSERT(foo.do_multi_complicated_val(val1, val2));
        }
    }

    class baz : public xxx::i_baz
    {
        error_code callback(int val)
        {            
            std::cout << "callback " << val << "\n";
            return 0;
        }
    };

    void remote_tests(rpc::shared_ptr<yyy::i_example> example_ptr)
    {
        int val = 0;
        example_ptr->add(1, 2, val);

        // check the creation of an object that is passed back via interface
        rpc::shared_ptr<marshalled_tests::xxx::i_foo> foo;
        example_ptr->create_foo(foo);
        foo->do_something_in_val(22);

        // test casting logic
        auto i_bar_ptr = rpc::dynamic_pointer_cast<xxx::i_bar>(foo);
        if (i_bar_ptr)
            i_bar_ptr->do_something_else(33);

        // test recursive interface passing
        rpc::shared_ptr<xxx::i_foo> other_foo;
        error_code err_code = foo->recieve_interface(other_foo);
        if (err_code)
        {
            std::cout << "create_foo failed\n";
        }
        else
        {
            other_foo->do_something_in_val(22);
        }

        rpc::shared_ptr<xxx::i_baz> b(new baz());
        err_code = foo->give_interface(b);
    }
}