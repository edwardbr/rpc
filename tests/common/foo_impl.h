#pragma once

#include <example/example.h>

void log(std::string& data)
{
	log_str(data.data(), data.size() + 1);
}

namespace marshalled_tests
{
    class foo : public i_foo
    {
        public:
        ~foo(){}
        int do_something_in_val(int val)
        {
            log(std::string("got ") + std::to_string(val));
            return 0;
        }
        int do_something_in_ref(const int& val) 
        {
            log(std::string("got ") + std::to_string(val));
            return 0;
        }
        int do_something_in_by_val_ref(const int& val) 
        {
            log(std::string("got ") + std::to_string(val));
            return 0;
        }
        int do_something_in_move_ref(int&& val) 
        {
            log(std::string("got ") + std::to_string(val));
            return 0;
        }
        int do_something_in_ptr(const int* val) 
        {
            log(std::string("got ") + std::to_string(*val));
            return 0;
        }
        int do_something_out_ref(int& val)
        {
            val = 33;
            return 0;
        };
        int do_something_out_ptr_ref(int*& val)
        {
            val = new int(33);
            return 0;
        }
        int do_something_out_ptr_ptr(int** val)
        {
            *val = new int(33);
            return 0;
        }
        int do_something_in_out_ref(int& val)
        {
            log(std::string("got ") + std::to_string(val));
            val = 33;
            return 0;
        }
        int give_something_complicated_val(const something_complicated val) 
        {
            log(std::string("got ") + std::to_string(val.int_val));
            return 0;
        }
        int give_something_complicated_ref(const something_complicated& val) 
        {
            log(std::string("got ") + std::to_string(val.int_val));
            return 0;
        }
        int give_something_complicated_ref_val(const something_complicated& val) 
        {
            log(std::string("got ") + std::to_string(val.int_val));
            return 0;
        }
        int give_something_complicated_move_ref(something_complicated&& val) 
{
                log(std::string("got ") + std::to_string(val.int_val));
            return 0;
}
        int give_something_complicated_ptr(const something_complicated* val) 
        {
            log(std::string("got ") + std::to_string(val->int_val));
            return 0;
        }
        int recieve_something_complicated_ref(something_complicated& val)
        {
            val = something_complicated{33,"22"};
            return 0;
        }
        int recieve_something_complicated_ptr(something_complicated*& val) 
        {
            val = new something_complicated{33,"22"};
            return 0;
        }
        int recieve_something_complicated_in_out_ref(something_complicated& val) 
        {
            log(std::string("got ") + std::to_string(val.int_val));
            val.int_val = 33;
            return 0;
        }
        int give_something_more_complicated_val(const something_more_complicated val) 
        {
            log(std::string("got ") + val.map_val.begin()->first);
            return 0;
        }
        int give_something_more_complicated_ref(const something_more_complicated& val) 
        {
            log(std::string("got ") + val.map_val.begin()->first);
            return 0;
        }
        int give_something_more_complicated_move_ref(something_more_complicated&& val) 
        {
            log(std::string("got ") + val.map_val.begin()->first);
            return 0;
        }
        int give_something_more_complicated_ref_val(const something_more_complicated& val) 
        {
            log(std::string("got ") + val.map_val.begin()->first);
            return 0;
        }
        int give_something_more_complicated_ptr(const something_more_complicated* val) 
        {
            log(std::string("got ") + val->map_val.begin()->first);
            return 0;
        }
        int recieve_something_more_complicated_ref(something_more_complicated& val)
        {
            val.map_val["22"]=something_complicated{33,"22"};
            return 0;
        }
        int recieve_something_more_complicated_ptr(something_more_complicated*& val) 
        {
            val = new something_more_complicated();
            val->map_val["22"]=something_complicated{33,"22"};
            return 0;
        }        
        int recieve_something_more_complicated_in_out_ref(something_more_complicated& val) 
        {
            log(std::string("got ") + val.map_val.begin()->first);
            val.map_val["22"]=something_complicated{33,"23"};
            return 0;
        }
        int do_multi_val(int val1, int val2) 
        {
            log(std::string("got ") + std::to_string(val1));
            return 0;
        }
        int do_multi_complicated_val(const something_more_complicated val1, const something_more_complicated val2)
        {
            log(std::string("got ") + val1.map_val.begin()->first);
            return 0;
        }

        error_code recieve_interface(rpc_cpp::shared_ptr<i_foo>& val)
        {
            val = rpc_cpp::shared_ptr<i_foo>(new foo);
            return 0;
        }
    };
    class example : public i_example
    {
    public:
        error_code create_foo(rpc_cpp::shared_ptr<i_foo>& target) override
        {
            target = rpc_cpp::shared_ptr<i_foo>(new foo);
            return 0;
        }

        
        error_code add(int a, int b, int& c) override
        {
            c = a+b;
            return 0;
        }
    };
}