#pragma once

#include <example/example.h>
#include <rpc/i_telemetry_service.h>

void log(const std::string& data)
{
    log_str(data.data(), data.size() + 1);
}

namespace marshalled_tests
{
    class baz : 
        public xxx::i_baz,
        public xxx::i_bar
    {
        const rpc::i_telemetry_service* telemetry_ = nullptr;
        void* get_address() const override { return (void*)this; }
        const rpc::casting_interface* query_interface(uint64_t interface_id) const override 
        { 
            if(xxx::i_baz::id == interface_id)
                return static_cast<const xxx::i_baz*>(this); 
            if(xxx::i_bar::id == interface_id)
                return static_cast<const xxx::i_bar*>(this); 
            return nullptr;
        }
    public:
        baz(const rpc::i_telemetry_service* telemetry) : telemetry_(telemetry)
        {
            if(telemetry_)
                telemetry_->on_impl_creation("baz", xxx::i_baz::id);
        }

        virtual ~baz()
        {
            if(telemetry_)
                telemetry_->on_impl_deletion("baz", xxx::i_baz::id);
        }
        int callback(int val) override
        {            
            log(std::string("callback ") + std::to_string(val));
            return rpc::error::OK();
        }
        error_code blob_test(const std::vector<uint8_t>& inval, std::vector<uint8_t>& out_val) override
        {
            log(std::string("baz blob_test ") + std::to_string(inval.size()));
            out_val = inval;
            return rpc::error::OK();
        }
        error_code do_something_else(int val) override
        {
            log(std::string("baz do_something_else"));
            return rpc::error::OK();
        }
    };

    class foo : public xxx::i_foo
    {
        const rpc::i_telemetry_service* telemetry_ = nullptr;
        void* get_address() const override { return (void*)this; }
        const rpc::casting_interface* query_interface(uint64_t interface_id) const override 
        { 
            if(xxx::i_foo::id == interface_id)
                return static_cast<const xxx::i_foo*>(this); 
            return nullptr;
        }

        rpc::shared_ptr<xxx::i_baz> cached_;
    public:
        foo(const rpc::i_telemetry_service* telemetry) : telemetry_(telemetry)
        {
            if(telemetry_)
                telemetry_->on_impl_creation("foo", xxx::i_foo::id);
        }
        virtual ~foo()
        {
            if(telemetry_)
                telemetry_->on_impl_deletion("foo", xxx::i_foo::id);
        }
        error_code do_something_in_val(int val) override
        {
            log(std::string("got ") + std::to_string(val));
            return rpc::error::OK();
        }
        error_code do_something_in_ref(const int& val) override
        {
            log(std::string("got ") + std::to_string(val));
            return rpc::error::OK();
        }
        error_code do_something_in_by_val_ref(const int& val) override
        {
            log(std::string("got ") + std::to_string(val));
            return rpc::error::OK();
        }
        error_code do_something_in_move_ref(int&& val) override
        {
            log(std::string("got ") + std::to_string(val));
            return rpc::error::OK();
        }
        error_code do_something_in_ptr(const int* val) override
        {
            log(std::string("got ") + std::to_string(*val));
            return rpc::error::OK();
        }
        error_code do_something_out_val(int& val) override
        {
            val = 33;
            return rpc::error::OK();
        };
        error_code do_something_out_ptr_ref(int*& val) override
        {
            val = new int(33);
            return rpc::error::OK();
        }
        error_code do_something_out_ptr_ptr(int** val) override
        {
            *val = new int(33);
            return rpc::error::OK();
        }
        error_code do_something_in_out_ref(int& val) override
        {
            log(std::string("got ") + std::to_string(val));
            val = 33;
            return rpc::error::OK();
        }
        error_code give_something_complicated_val(const xxx::something_complicated val) override
        {
            log(std::string("got ") + std::to_string(val.int_val));
            return rpc::error::OK();
        }
        error_code give_something_complicated_ref(const xxx::something_complicated& val) override
        {
            log(std::string("got ") + std::to_string(val.int_val));
            return rpc::error::OK();
        }
        error_code give_something_complicated_ref_val(const xxx::something_complicated& val) override
        {
            log(std::string("got ") + std::to_string(val.int_val));
            return rpc::error::OK();
        }
        error_code give_something_complicated_move_ref(xxx::something_complicated&& val) override
        {
            log(std::string("got ") + std::to_string(val.int_val));
            return rpc::error::OK();
        }
        error_code give_something_complicated_ptr(const xxx::something_complicated* val) override
        {
            log(std::string("got ") + std::to_string(val->int_val));
            return rpc::error::OK();
        }
        error_code recieve_something_complicated_ref(xxx::something_complicated& val) override
        {
            val = xxx::something_complicated {33, "22"};
            return rpc::error::OK();
        }
        error_code recieve_something_complicated_ptr(xxx::something_complicated*& val) override
        {
            val = new xxx::something_complicated {33, "22"};
            return rpc::error::OK();
        }
        error_code recieve_something_complicated_in_out_ref(xxx::something_complicated& val) override
        {
            log(std::string("got ") + std::to_string(val.int_val));
            val.int_val = 33;
            return rpc::error::OK();
        }
        error_code give_something_more_complicated_val(const xxx::something_more_complicated val) override
        {
            log(std::string("got ") + val.map_val.begin()->first);
            return rpc::error::OK();
        }
        error_code give_something_more_complicated_ref(const xxx::something_more_complicated& val) override
        {
            log(std::string("got ") + val.map_val.begin()->first);
            return rpc::error::OK();
        }
        error_code give_something_more_complicated_move_ref(xxx::something_more_complicated&& val) override
        {
            log(std::string("got ") + val.map_val.begin()->first);
            return rpc::error::OK();
        }
        error_code give_something_more_complicated_ref_val(const xxx::something_more_complicated& val) override
        {
            log(std::string("got ") + val.map_val.begin()->first);
            return rpc::error::OK();
        }
        error_code give_something_more_complicated_ptr(const xxx::something_more_complicated* val) override
        {
            log(std::string("got ") + val->map_val.begin()->first);
            return rpc::error::OK();
        }
        error_code recieve_something_more_complicated_ref(xxx::something_more_complicated& val) override
        {
            val.map_val["22"] = xxx::something_complicated {33, "22"};
            return rpc::error::OK();
        }
        error_code recieve_something_more_complicated_ptr(xxx::something_more_complicated*& val) override
        {
            val = new xxx::something_more_complicated();
            val->map_val["22"] = xxx::something_complicated {33, "22"};
            return rpc::error::OK();
        }
        error_code recieve_something_more_complicated_in_out_ref(xxx::something_more_complicated& val) override
        {
            log(std::string("got ") + val.map_val.begin()->first);
            val.map_val["22"] = xxx::something_complicated {33, "23"};
            return rpc::error::OK();
        }
        error_code do_multi_val(int val1, int val2) override
        {
            log(std::string("got ") + std::to_string(val1));
            return rpc::error::OK();
        }
        error_code do_multi_complicated_val(const xxx::something_more_complicated val1, const xxx::something_more_complicated val2) override
        {
            log(std::string("got ") + val1.map_val.begin()->first);
            return rpc::error::OK();
        }

        error_code recieve_interface(rpc::shared_ptr<i_foo>& val) override
        {
            val = rpc::shared_ptr<xxx::i_foo>(new foo(telemetry_));
            auto val1 = rpc::dynamic_pointer_cast<xxx::i_bar>(val);
            return rpc::error::OK();
        }

        error_code give_interface(rpc::shared_ptr<xxx::i_baz> baz) override
        {
            baz->callback(22);
            auto val1 = rpc::dynamic_pointer_cast<xxx::i_bar>(baz);
            return rpc::error::OK();
        }

        error_code call_baz_interface(const rpc::shared_ptr<xxx::i_baz>& val) override
        {
            if(!val)
                return rpc::error::OK();
            val->callback(22);
            auto val1 = rpc::dynamic_pointer_cast<xxx::i_baz>(val);
//#sgx dynamic cast in an enclave this fails
            auto val2 = rpc::dynamic_pointer_cast<xxx::i_bar>(val);

            std::vector<uint8_t> in_val{1,2,3,4};
            std::vector<uint8_t> out_val;

            val->blob_test(in_val, out_val);
            assert(in_val == out_val);

            //this should trigger NEED_MORE_MEMORY signal requiring more out param data to be provided to the called 
            //the out param data is temporarily cached and given over when enough memory has been provided, without
            //recalling the implementation
            in_val.resize(100000);
            std::fill(in_val.begin(), in_val.end(), 42);
            val->blob_test(in_val, out_val);
            assert(in_val == out_val);
            return rpc::error::OK();
        }        

        
        error_code create_baz_interface(rpc::shared_ptr<xxx::i_baz>& val) override
        {
            val = rpc::shared_ptr<xxx::i_baz>(new baz(telemetry_));
            return rpc::error::OK();
        }
                
        error_code get_null_interface(rpc::shared_ptr<xxx::i_baz>& val) override
        {
            val = nullptr;
            return rpc::error::OK();
        }
    
        error_code set_interface(const rpc::shared_ptr<xxx::i_baz>& val) override
        {
            cached_ = val;
            return rpc::error::OK();
        }
        error_code get_interface(rpc::shared_ptr<xxx::i_baz>& val) override
        {
            val = cached_;
            return rpc::error::OK();
        }
    };

    class multiple_inheritance : 
        public xxx::i_bar,
        public xxx::i_baz
    {
        const rpc::i_telemetry_service* telemetry_ = nullptr;
        
        void* get_address() const override { return (void*)this; }
        const rpc::casting_interface* query_interface(uint64_t interface_id) const override 
        { 
            if(xxx::i_bar::id == interface_id)
                return static_cast<const xxx::i_bar*>(this); 
            if(xxx::i_baz::id == interface_id)
                return static_cast<const xxx::i_baz*>(this); 
            return nullptr;
        }
    public:
        multiple_inheritance(const rpc::i_telemetry_service* telemetry) : telemetry_(telemetry)
        {
            if(telemetry_)
                telemetry_->on_impl_creation("multiple_inheritance", xxx::i_bar::id);
        }
        virtual ~multiple_inheritance()
        {
            if(telemetry_)
                telemetry_->on_impl_deletion("multiple_inheritance", xxx::i_bar::id);
        }

        error_code do_something_else(int val) override
        {
            return rpc::error::OK();
        }
        int callback(int val) override
        {            
            log(std::string("callback ") + std::to_string(val));
            return rpc::error::OK();
        }
        error_code blob_test(const std::vector<uint8_t>& inval, std::vector<uint8_t>& out_val) override
        {
            out_val = inval;
            return rpc::error::OK();
        }
    };

    class example : public yyy::i_example
    {
        const rpc::i_telemetry_service* telemetry_ = nullptr;
        void* get_address() const override { return (void*)this; }
        const rpc::casting_interface* query_interface(uint64_t interface_id) const override 
        { 
            if(yyy::i_example::id == interface_id)
                return static_cast<const yyy::i_example*>(this); 
            return nullptr;
        }
    public:
        example(const rpc::i_telemetry_service* telemetry) : telemetry_(telemetry)
        {
            if(telemetry_)
                telemetry_->on_impl_creation("example", yyy::i_example::id);
        }
        virtual ~example()
        {
            if(telemetry_)
                telemetry_->on_impl_deletion("example", yyy::i_example::id);
        }
        
        error_code create_multiple_inheritance(rpc::shared_ptr<xxx::i_baz>& target) override
        {
            target = rpc::shared_ptr<xxx::i_baz>(new multiple_inheritance(telemetry_));
            return rpc::error::OK();
        }

        error_code create_foo(rpc::shared_ptr<xxx::i_foo>& target) override
        {
            target = rpc::shared_ptr<xxx::i_foo>(new foo(telemetry_));
            return rpc::error::OK();
        }

        error_code add(int a, int b, int& c) override
        {
            c = a + b;
            return rpc::error::OK();
        }
    };
}