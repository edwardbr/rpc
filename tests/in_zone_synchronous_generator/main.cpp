#include <iostream>

#include <example/example.h>
#include <example_proxy.cpp>
#include <example_stub.cpp>

using namespace secretarium::marshalled_foo;

class i_foo_impl : public i_foo
{
	int do_something_in_val(int val)
	{
		std::cout << "got " << val << "\n";
		return 42;
	}
	int do_something_in_ref(const int& val)
	{
		return 42;
	}
	int do_something_in_by_val_ref(const int& val)
	{
		return 42;
	}
	int do_something_in_ptr(int* val)
	{
		return 42;
	}
	int do_something_out_ptr_ref(int*& val)
	{
		return 42;
	}
	int do_something_out_ptr_ptr(int** val)
	{
		return 42;
	}
	int give_something_complicated_val(const something_complicated val)
	{
		return 42;
	}
	int give_something_complicated_ref(const something_complicated& val)
	{
		return 42;
	}
	int give_something_complicated_ref_val(const something_complicated& val)
	{
		return 42;
	}
	int give_something_complicated_ptr(const something_complicated* val)
	{
		return 42;
	}
	int recieve_something_complicated_ptr(something_complicated*& val)
	{
		return 42;
	}
	int give_something_more_complicated_val(const something_more_complicated val)
	{
		return 42;
	}
	int give_something_more_complicated_ref(const something_more_complicated& val)
	{
		return 42;
	}
	int give_something_more_complicated_ref_val(const something_more_complicated& val)
	{
		return 42;
	}
	int give_something_more_complicated_ptr(const something_more_complicated* val)
	{
		return 42;
	}
	int recieve_something_more_complicated_ptr(something_more_complicated*& val)
	{
		return 42;
	}
	int do_multi_val(int val1, int val2)
	{
		return 42;
	}
	int do_multi_complicated_val(const something_more_complicated val1, const something_more_complicated val2)
	{
		return 42;
	}
};


error_code i_marshaller_impl::send(uint64_t object_id, uint64_t interface_id, uint64_t method_id, const yas::shared_buffer& in, yas::shared_buffer& out)
{
	return 1;
}
error_code i_marshaller_impl::try_cast(i_unknown& from, uint64_t interface_id, remote_shared_ptr<i_unknown>& to)
{
	return 1;
}

int main()
{
	i_foo_stub stub(remote_shared_ptr<i_foo>(new i_foo_impl()));
	i_foo_proxy proxy(stub, 0);
	i_foo& foo = proxy;
	auto ret = foo.do_something_in_val(33);
	std::cout << "recieved " << ret << "\n";
}