/*
 *   Copyright (c) 2024 Edward Boggis-Rolfe
 *   All rights reserved.
 */
#pragma once

#include <rpc/types.h>
#include <rpc/version.h>
#include <rpc/casting_interface.h>

namespace rpc
{
    class interface_proxy;

    //this do nothing class is for static pointer casting
    class casting_interface
    {
    public:
        virtual ~casting_interface() = default;
        virtual void* get_address() const = 0;     
        virtual const rpc::casting_interface* query_interface(rpc::interface_ordinal interface_id) const = 0;        
        // this is only implemented by interface_proxy
        virtual const interface_proxy* get_interface_proxy() const {return nullptr;} 
    };

    //this is a nice helper function to match an interface id to a interface in a version independant way
    template<class T>
    bool match(rpc::interface_ordinal interface_id)
    {
        return 
            #ifdef RPC_V2
                T::get_id(rpc::VERSION_2) == interface_id
            #endif
            #if defined(RPC_V1) && defined(RPC_V2)
                ||
            #endif
            #ifdef RPC_V1
                T::get_id(rpc::VERSION_1) == interface_id
            #endif            
            #if !defined(RPC_V1) && !defined(RPC_V2)
                false
            #endif
            ;
    }
}