#include <iostream>

#include <example/example.h>
#include <example_proxy.cpp>
#include <example_stub.cpp>

using namespace secretarium::marshalled_foo;

class foo : public i_foo
{
    int do_something_in_val(int val)
    {
        std::cout << "got " << val << "\n";
        return 0;
    }
    int do_something_in_ref(const int& val) 
	{
        std::cout << "got " << val << "\n";
        return 0;
    }
    int do_something_in_by_val_ref(const int& val) 
	{
        std::cout << "got " << val << "\n";
        return 0;
    }
    int do_something_in_ptr(int* val) 
	{
        std::cout << "got " << *val << "\n";
        return 0;
    }
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
        std::cout << "got " << val.int_val << "\n";
        return 0;
    }
    int give_something_complicated_ref(const something_complicated& val) 
	{
        std::cout << "got " << val.int_val << "\n";
        return 0;
    }
    int give_something_complicated_ref_val(const something_complicated& val) 
	{
        std::cout << "got " << val.int_val << "\n";
        return 0;
    }
    int give_something_complicated_ptr(const something_complicated* val) 
	{
        std::cout << "got " << val->int_val << "\n";
        return 0;
    }
    int recieve_something_complicated_ptr(something_complicated*& val) 
	{
        val = new something_complicated{33,"22"};
        return 0;
    }
    int give_something_more_complicated_val(const something_more_complicated val) 
	{
        std::cout << "got " << val.map_val.begin()->first << "\n";
        return 0;
    }
    int give_something_more_complicated_ref(const something_more_complicated& val) 
	{
        std::cout << "got " << val.map_val.begin()->first << "\n";
        return 0;
    }
    int give_something_more_complicated_ref_val(const something_more_complicated& val) 
	{
        std::cout << "got " << val.map_val.begin()->first << "\n";
        return 0;
    }
    int give_something_more_complicated_ptr(const something_more_complicated* val) 
	{
        std::cout << "got " << val->map_val.begin()->first << "\n";
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
        std::cout << "got " << val1 << "\n";
        return 0;
	}
    int do_multi_complicated_val(const something_more_complicated val1, const something_more_complicated val2)
	{
        std::cout << "got " << val1.map_val.begin()->first << "\n";
        return 0;
	}
};

error_code i_marshaller_impl::send(uint64_t object_id, uint64_t interface_id, uint64_t method_id,
                                   const yas::shared_buffer& in, yas::shared_buffer& out)
{
    return 1;
}
error_code i_marshaller_impl::try_cast(i_unknown& from, uint64_t interface_id, remote_shared_ptr<i_unknown>& to)
{
    return 1;
}

#define ASSERT(x)                                                                                                      \
    if (!(x))                                                                                                          \
    std::cerr << "bad test " #x

int main()
{
    i_foo_stub stub(remote_shared_ptr<i_foo>(new foo()));
    i_foo_proxy proxy(stub, 0);
    i_foo& foo = proxy;

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
        ASSERT(foo.do_something_in_ptr(&val));
    }
    {
        int* val = nullptr;
        ASSERT(foo.do_something_out_ptr_ref(val));
        delete val;
    }
    {
        int* val = nullptr;
        ASSERT(foo.do_something_out_ptr_ptr(&val));
        delete val;
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
        ASSERT(foo.give_something_complicated_ptr(&val));
    }
    {
        something_complicated* val = nullptr;
        ASSERT(foo.recieve_something_complicated_ptr(val));
		std::cout << "got " << val->int_val << "\n";
        delete val;
    }
    {
        something_more_complicated val;
		val.map_val["22"]=something_complicated{33,"22"};
        ASSERT(foo.give_something_more_complicated_val(val));
    }
    {
        something_more_complicated val;
		val.map_val["22"]=something_complicated{33,"22"};
        ASSERT(foo.give_something_more_complicated_ref(val));
    }
    {
        something_more_complicated val;
		val.map_val["22"]=something_complicated{33,"22"};
        ASSERT(foo.give_something_more_complicated_ref_val(val));
    }
    {
        something_more_complicated val;
		val.map_val["22"]=something_complicated{33,"22"};
        ASSERT(foo.give_something_more_complicated_ptr(&val));
    }
    {
        something_more_complicated* val = nullptr;
        ASSERT(foo.recieve_something_more_complicated_ptr(val));
		std::cout << "got " << val->map_val.begin()->first << "\n";
        delete val;
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