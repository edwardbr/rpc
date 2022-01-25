// Copyright 2021 Secretarium Ltd <contact@secretarium.org>

#include "stdio.h"
#include <string>
#include <cstring>
#include <cstdio>

#include "sgx_trts.h"
#include "trusted/enclave_marshal_test_t.h"

#include <example/example.h>
#include <example_stub.cpp>
#include <example_proxy.cpp>

using namespace secretarium::marshalled_foo;

void log(std::string& data)
{
	log_str(data.data(), data.size() + 1);
}

class foo : public i_foo
{
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
    int do_something_in_ptr(int* val) 
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
    int give_something_complicated_ptr(const something_complicated* val) 
	{
        log(std::string("got ") + std::to_string(val->int_val));
        return 0;
    }
    int recieve_something_complicated_ptr(something_complicated*& val) 
	{
        val = new something_complicated{33,"22"};
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
    int recieve_something_more_complicated_ptr(something_more_complicated*& val) 
	{
        val = new something_more_complicated();
		val->map_val["22"]=something_complicated{33,"22"};
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
};

/*class class_factory
{
	public:
	error_code create_foo(remote_shared_ptr<i_foo>& foo_obj)
	{
		foo_obj = remote_shared_ptr<i_foo>(new foo);
		return 0;
	};
};*/

class example : public i_example
{
	public:
	error_code create_foo(remote_shared_ptr<i_foo>& target) override
	{
		return 0;
	}
};

error_code i_marshaller_impl::send(uint64_t object_id, uint64_t interface_id, uint64_t method_id, size_t in_size_, const char* in_buf_, size_t out_size_, char* out_buf_)
{
    return 1;
}
error_code i_marshaller_impl::try_cast(i_unknown& from, uint64_t interface_id, remote_shared_ptr<i_unknown>& to)
{
    return 1;
}

class marshaller : public i_marshaller_impl
{
	std::shared_ptr<i_example_stub> the_example_stub;

	//just for now
	std::shared_ptr<i_foo_stub> foo_stub_;

public:
	error_code initialise(zone_config* config)
	{
		remote_shared_ptr<i_example> ex(new example);
		the_example_stub = std::shared_ptr<i_example_stub>(new i_example_stub(ex));

		foo_stub_ = std::make_shared<i_foo_stub>(remote_shared_ptr<i_foo>(new foo()));
		return 0;
	}
	void shutdown(){}

	error_code send(uint64_t object_id, uint64_t interface_id, uint64_t method_id, size_t in_size_, const char* in_buf_, size_t out_size_, char* out_buf_) override
	{
		error_code ret = 0;
		if(object_id == 0) // call static zone function
		{
			switch(interface_id)
			{
				case i_example::id:
					ret = the_example_stub->send(object_id, interface_id, method_id,
									in_size_, in_buf_, out_size_, out_buf_);
				break;
			}
		}
		else if(object_id == 1)
		{
			switch(interface_id)
			{
				case i_foo::id:
					ret = foo_stub_->send(object_id, interface_id, method_id,
									in_size_, in_buf_, out_size_, out_buf_);
				break;
			}
		}
		return ret;
	}
	error_code try_cast(i_unknown& from, uint64_t interface_id, remote_shared_ptr<i_unknown>& to) override
	{
		return 1;
	}
};

marshaller the_zone_marshaller;


int enclave_marshal_test_init(zone_config* config)
{
	error_code err_code = the_zone_marshaller.initialise(config);
	if(err_code)
		return err_code;

    i_foo_stub stub(remote_shared_ptr<i_foo>(new foo()));
    i_foo_proxy proxy(stub, 0);
    i_foo& foo = proxy;

    {
        foo.do_something_in_val(33);
    }

	return 0;
}

void enclave_marshal_test_destroy()
{
	the_zone_marshaller.shutdown();
}

int call(uint64_t object_id, uint64_t interface_id, uint64_t method_id, size_t sz_int, const char* data_in, size_t sz_out, char* data_out)
{
	error_code ret = the_zone_marshaller.send(object_id, interface_id, method_id, sz_int, data_in, sz_out, data_out);
	return ret;
}