#pragma once

#include <rpc/types.h>

namespace rpc
{
    class proxy_base;

    //this do nothing class is for static pointer casting
    class casting_interface
    {
    public:
        virtual ~casting_interface() = default;
        virtual void* get_address() const = 0;     
        virtual const rpc::casting_interface* query_interface(rpc::interface_ordinal interface_id) const = 0;        
        // this is only implemented by proxy_base
        virtual proxy_base* query_proxy_base() const {return nullptr;} 
    };
}