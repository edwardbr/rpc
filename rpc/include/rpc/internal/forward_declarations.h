/*
 *   Copyright (c) 2024 Edward Boggis-Rolfe
 *   All rights reserved.
 */
#pragma once

namespace rpc
{
    NAMESPACE_INLINE_BEGIN
    
    template<typename T> class shared_ptr;
    template<typename T> class weak_ptr;
    template<typename T, typename Deleter = std::default_delete<T>> class unique_ptr;
    template<typename T> class optimistic_ptr;
    template<typename T> class enable_shared_from_this;
    class object_proxy;
    class proxy_base;
    class service_proxy;
    class object_stub;

    class i_interface_stub;
    class service;
    class child_service;
    struct current_service_tracker;
    
    NAMESPACE_INLINE_END
}
