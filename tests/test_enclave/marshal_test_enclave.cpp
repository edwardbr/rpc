#include "stdio.h"
#include <string>
#include <cstring>
#include <cstdio>

#include "sgx_trts.h"
#include "trusted/enclave_marshal_test_t.h"

#include "../common/foo_impl.h"

#include <example/example.h>
#include <example_stub.cpp>
#include <example_proxy.cpp>

using namespace marshalled_tests;


/*class class_factory
{
	public:
	error_code create_foo(rpc_cpp::remote_shared_ptr<i_foo>& foo_obj)
	{
		foo_obj = rpc_cpp::remote_shared_ptr<i_foo>(new foo);
		return 0;
	};
};*/

class example : public i_example
{
	public:
	/*error_code create_foo(rpc_cpp::remote_shared_ptr<i_foo>& target) override
	{
		return 0;
	}*/
};

error_code i_marshaller_impl::send(uint64_t object_id, uint64_t interface_id, uint64_t method_id, size_t in_size_, const char* in_buf_, size_t out_size_, char* out_buf_)
{
    return 1;
}
error_code i_marshaller_impl::try_cast(uint64_t zone_id_, uint64_t object_id, uint64_t interface_id)
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
		rpc_cpp::remote_shared_ptr<i_example> ex(new example);
		the_example_stub = std::shared_ptr<i_example_stub>(new i_example_stub(ex));

		foo_stub_ = std::make_shared<i_foo_stub>(rpc_cpp::remote_shared_ptr<i_foo>(new foo()));
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
	error_code try_cast(uint64_t zone_id_, uint64_t object_id, uint64_t interface_id) override
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