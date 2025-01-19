/*
 *   Copyright (c) 2024 Edward Boggis-Rolfe
 *   All rights reserved.
 */
#pragma once


#ifdef _MSC_VER
    #include <type_traits>
    #include <tuple>
    #include <memory>

#ifndef _IN_ENCLAVE    
    #include <corecrt.h>
    #include <yvals.h>
#endif    

namespace std
{
    template <class...> using void_t = void;
}
    // #include <yvals.h>
    
    // #if _STL_COMPILER_PREPROCESSOR
    //     #include <xmemory>

    //     #if _HAS_CXX17
    //         #include <xpolymorphic_allocator.h>
    //     #endif // _HAS_CXX17

    // #endif
//#include <xtr1common>
#endif

// #ifdef _MSC_VER
// #include <rpc/stl_legacy/rpc_memory.h>
// #else
#include <rpc/stl_clang12/rpc_memory.h>
// #endif
