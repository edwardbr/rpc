#pragma once

namespace rpc
{
    //this do nothing class is for static pointer casting
    class casting_interface
    {
    public:
        ~casting_interface() = default;
        virtual void* get_address() const = 0;     
        virtual rpc::casting_interface* query_interface(uint64_t interface_id) const = 0;        
    };
}