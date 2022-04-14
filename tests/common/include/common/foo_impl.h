#pragma once

#include <example/example.h>

void log(const std::string& data)
{
    log_str(data.data(), data.size() + 1);
}

namespace marshalled_tests
{
    class foo : public xxx::i_foo
    {
    public:
        virtual ~foo() { }
        error_code do_something_in_val(int val)
        {
            log(std::string("got ") + std::to_string(val));
            return 0;
        }
        error_code do_something_in_ref(const int& val)
        {
            log(std::string("got ") + std::to_string(val));
            return 0;
        }
        error_code do_something_in_by_val_ref(const int& val)
        {
            log(std::string("got ") + std::to_string(val));
            return 0;
        }
        error_code do_something_in_move_ref(int&& val)
        {
            log(std::string("got ") + std::to_string(val));
            return 0;
        }
        error_code do_something_in_ptr(const int* val)
        {
            log(std::string("got ") + std::to_string(*val));
            return 0;
        }
        error_code do_something_out_val(int& val)
        {
            val = 33;
            return 0;
        };
        error_code do_something_out_ptr_ref(int*& val)
        {
            val = new int(33);
            return 0;
        }
        error_code do_something_out_ptr_ptr(int** val)
        {
            *val = new int(33);
            return 0;
        }
        error_code do_something_in_out_ref(int& val)
        {
            log(std::string("got ") + std::to_string(val));
            val = 33;
            return 0;
        }
        error_code give_something_complicated_val(const xxx::something_complicated val)
        {
            log(std::string("got ") + std::to_string(val.int_val));
            return 0;
        }
        error_code give_something_complicated_ref(const xxx::something_complicated& val)
        {
            log(std::string("got ") + std::to_string(val.int_val));
            return 0;
        }
        error_code give_something_complicated_ref_val(const xxx::something_complicated& val)
        {
            log(std::string("got ") + std::to_string(val.int_val));
            return 0;
        }
        error_code give_something_complicated_move_ref(xxx::something_complicated&& val)
        {
            log(std::string("got ") + std::to_string(val.int_val));
            return 0;
        }
        error_code give_something_complicated_ptr(const xxx::something_complicated* val)
        {
            log(std::string("got ") + std::to_string(val->int_val));
            return 0;
        }
        error_code recieve_something_complicated_ref(xxx::something_complicated& val)
        {
            val = xxx::something_complicated {33, "22"};
            return 0;
        }
        error_code recieve_something_complicated_ptr(xxx::something_complicated*& val)
        {
            val = new xxx::something_complicated {33, "22"};
            return 0;
        }
        error_code recieve_something_complicated_in_out_ref(xxx::something_complicated& val)
        {
            log(std::string("got ") + std::to_string(val.int_val));
            val.int_val = 33;
            return 0;
        }
        error_code give_something_more_complicated_val(const xxx::something_more_complicated val)
        {
            log(std::string("got ") + val.map_val.begin()->first);
            return 0;
        }
        error_code give_something_more_complicated_ref(const xxx::something_more_complicated& val)
        {
            log(std::string("got ") + val.map_val.begin()->first);
            return 0;
        }
        error_code give_something_more_complicated_move_ref(xxx::something_more_complicated&& val)
        {
            log(std::string("got ") + val.map_val.begin()->first);
            return 0;
        }
        error_code give_something_more_complicated_ref_val(const xxx::something_more_complicated& val)
        {
            log(std::string("got ") + val.map_val.begin()->first);
            return 0;
        }
        error_code give_something_more_complicated_ptr(const xxx::something_more_complicated* val)
        {
            log(std::string("got ") + val->map_val.begin()->first);
            return 0;
        }
        error_code recieve_something_more_complicated_ref(xxx::something_more_complicated& val)
        {
            val.map_val["22"] = xxx::something_complicated {33, "22"};
            return 0;
        }
        error_code recieve_something_more_complicated_ptr(xxx::something_more_complicated*& val)
        {
            val = new xxx::something_more_complicated();
            val->map_val["22"] = xxx::something_complicated {33, "22"};
            return 0;
        }
        error_code recieve_something_more_complicated_in_out_ref(xxx::something_more_complicated& val)
        {
            log(std::string("got ") + val.map_val.begin()->first);
            val.map_val["22"] = xxx::something_complicated {33, "23"};
            return 0;
        }
        error_code do_multi_val(int val1, int val2)
        {
            log(std::string("got ") + std::to_string(val1));
            return 0;
        }
        error_code do_multi_complicated_val(const xxx::something_more_complicated val1, const xxx::something_more_complicated val2)
        {
            log(std::string("got ") + val1.map_val.begin()->first);
            return 0;
        }

        error_code recieve_interface(rpc::shared_ptr<i_foo>& val)
        {
            val = rpc::shared_ptr<xxx::i_foo>(new foo);
            return 0;
        }
        error_code give_interface(rpc::shared_ptr<xxx::i_baz> val)
        {
            val->callback(22);
            return 0;
        }        
    };
    class example : public yyy::i_example
    {
    public:
        error_code create_foo(rpc::shared_ptr<xxx::i_foo>& target) override
        {
            target = rpc::shared_ptr<xxx::i_foo>(new foo);
            return 0;
        }

        error_code add(int a, int b, int& c) override
        {
            c = a + b;
            return 0;
        }
    };
}