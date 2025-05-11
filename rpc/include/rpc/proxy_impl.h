/*
 *   Copyright (c) 2024 Edward Boggis-Rolfe
 *   All rights reserved.
 */
#pragma once

#include <string>
#include <memory>
#include <unordered_map>
#include <mutex>


namespace rpc
{
    template<class T> class proxy_impl : public proxy_base, public T
    {
    public:
        proxy_impl(std::shared_ptr<object_proxy> object_proxy)
            : proxy_base(object_proxy)
        {
        }

        virtual void* get_address() const override
        {
            RPC_ASSERT(false);
            return (T*)get_object_proxy().get();
        }
        rpc::proxy_base* query_proxy_base() const override
        {
            return static_cast<proxy_base*>(const_cast<proxy_impl*>(this));
        }
        const rpc::casting_interface* query_interface(rpc::interface_ordinal interface_id) const override
        {
#ifdef RPC_V2
            if (T::get_id(rpc::VERSION_2) == interface_id)
                return static_cast<const T*>(this);
#endif
            return nullptr;
        }
        virtual ~proxy_impl() = default;
    };
}