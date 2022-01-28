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

    void standard_tests(i_foo& foo, bool enclave)
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
            ASSERT(foo.do_something_out_ref(val));
        }
        if(!enclave)
        {
            int* val = nullptr;
            ASSERT(foo.do_something_out_ptr_ref(val));
            delete val;
        }
        if(!enclave)
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
            something_complicated val{33,"22"};
            ASSERT(foo.give_something_complicated_val(val));
        }
        {
            something_complicated val{33,"22"};
            ASSERT(foo.give_something_complicated_ref(val));
        }
        {
            something_complicated val{33,"22"};
            ASSERT(foo.give_something_complicated_ref_val(val));
        }
        {
            something_complicated val{33,"22"};
            ASSERT(foo.give_something_complicated_move_ref(std::move(val)));
        }
        {
            something_complicated val{33,"22"};
            ASSERT(foo.give_something_complicated_ptr(&val));
        }
        {
            something_complicated val;
            ASSERT(foo.recieve_something_complicated_ref(val));
            std::cout << "got " << val.string_val << "\n";
        }
        if(!enclave)
        {
            something_complicated* val = nullptr;
            ASSERT(foo.recieve_something_complicated_ptr(val));
            std::cout << "got " << val->int_val << "\n";
            delete val;
        }
        {
            something_complicated val;
            val.int_val = 32;
            ASSERT(foo.recieve_something_complicated_in_out_ref(val));
            std::cout << "got " << val.int_val << "\n";
        }
        {
            something_more_complicated val;
            val.map_val["22"]=something_complicated{33,"22"};
            ASSERT(foo.give_something_more_complicated_val(val));
        }
        if(!enclave)
        {
            something_more_complicated val;
            val.map_val["22"]=something_complicated{33,"22"};
            ASSERT(foo.give_something_more_complicated_ref(val));
        }
        {
            something_more_complicated val;
            val.map_val["22"]=something_complicated{33,"22"};
            ASSERT(foo.give_something_more_complicated_move_ref(std::move(val)));
        }
        {
            something_more_complicated val;
            val.map_val["22"]=something_complicated{33,"22"};
            ASSERT(foo.give_something_more_complicated_ref_val(val));
        }
        if(!enclave)
        {
            something_more_complicated val;
            val.map_val["22"]=something_complicated{33,"22"};
            ASSERT(foo.give_something_more_complicated_ptr(&val));
        }
        if(!enclave)
        {
            something_more_complicated val;
            ASSERT(foo.recieve_something_more_complicated_ref(val));
            std::cout << "got " << val.map_val.begin()->first << "\n";
        }
        if(!enclave)
        {
            something_more_complicated* val = nullptr;
            ASSERT(foo.recieve_something_more_complicated_ptr(val));
            std::cout << "got " << val->map_val.begin()->first << "\n";
            delete val;
        }
        {
            something_more_complicated val;
            val.map_val["22"]=something_complicated{33,"22"};
            ASSERT(foo.recieve_something_more_complicated_in_out_ref(val));
            std::cout << "got " << val.map_val.begin()->first << "\n";
        }
        {
            int val1 = 1;
            int val2 = 2;
            ASSERT(foo.do_multi_val(val1, val2));
        }
        {
            something_more_complicated val1;
            something_more_complicated val2;
            val1.map_val["22"]=something_complicated{33,"22"};
            val2.map_val["22"]=something_complicated{33,"22"};
            ASSERT(foo.do_multi_complicated_val(val1, val2));
        }
    }
}