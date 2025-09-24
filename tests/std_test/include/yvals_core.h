// Cross-platform compatibility for yvals_core.h
// Copyright (c) 2025 Edward Boggis-Rolfe
// All rights reserved.

#pragma once

#ifdef _MSC_VER
    // On Windows, use the real yvals_core.h
    #include_next <yvals_core.h>
#else
    // On non-Windows platforms, provide compatibility definitions
    #include "msvc_compat.hpp"

    // Common definitions that might be expected from yvals_core.h
    #ifndef _STD
        #define _STD std::
    #endif

    #ifndef _NODISCARD
        #if __cplusplus >= 201703L
            #define _NODISCARD [[nodiscard]]
        #elif defined(__GNUC__) || defined(__clang__)
            #define _NODISCARD __attribute__((warn_unused_result))
        #else
            #define _NODISCARD
        #endif
    #endif

    #ifndef _CONSTEXPR20
        #if __cplusplus >= 202002L
            #define _CONSTEXPR20 constexpr
        #else
            #define _CONSTEXPR20
        #endif
    #endif

    // Other common MSVC macros
    #ifndef _INLINE_VAR
        #if __cplusplus >= 201703L
            #define _INLINE_VAR inline constexpr
        #else
            #define _INLINE_VAR constexpr
        #endif
    #endif
#endif