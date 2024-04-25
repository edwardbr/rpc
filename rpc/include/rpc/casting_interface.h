/*
 *   Copyright (c) 2024 Edward Boggis-Rolfe
 *   All rights reserved.
 */
#pragma once

#include <rpc/types.h>
#include <rpc/version.h>

namespace rpc
{
    class interface_proxy;
    template <class _Ty>
    class _Ptr_base;

    //this do nothing class is for static pointer casting
    class casting_interface
    {
    public:
        virtual ~casting_interface() = default;
        virtual void* get_address() const = 0;     
        virtual const rpc::casting_interface* query_interface(rpc::interface_ordinal interface_id) const = 0;        
        // this is only implemented by interface_proxy
        virtual const interface_proxy* get_interface_proxy() const {return nullptr;} 
        //if a class implements multiple interfaces then this method needs to be overriden to specify a specific interface to work with, 
        //needed for the rpc::shared_ptr it is in effect a cast to specify a particular vtable implementation of casting_interface
        //this does not seem to be a problem for msvc
        virtual const casting_interface* get_default_interface() const {return this;}
    private:
        virtual void __add_shared_ptr() const;
        virtual void __remove_shared_ptr() const;
        template <class _Ty>
        friend class _Ptr_base;
    };
    bool are_in_same_zone(const casting_interface* first, const casting_interface* second);

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