// Copyright (c) Microsoft Corporation.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Cross-platform compatibility by Edward Boggis-Rolfe

#pragma once

// Platform-specific includes and definitions
#ifdef _MSC_VER
    #include <yvals_core.h>
    // _INLINE_VAR is available from MSVC
#else
    // GCC/Clang compatibility
    #if __cplusplus >= 201703L
        #define _INLINE_VAR inline constexpr
    #else
        #define _INLINE_VAR constexpr
    #endif
#endif

namespace detail {
    constexpr bool permissive() {
        return false;
    }

    template <class>
    struct PermissiveTestBase {
        static constexpr bool permissive() {
            return true;
        }
    };

    template <class T>
    struct PermissiveTest : PermissiveTestBase<T> {
        static constexpr bool test() {
            return permissive();
        }
    };
} // namespace detail

template <class T>
constexpr bool is_permissive_v = detail::PermissiveTest<T>::test();

constexpr bool is_permissive = is_permissive_v<int>;