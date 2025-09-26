// Copyright (c) Microsoft Corporation.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <memory>
#include <type_traits>
#include <utility>

#if _HAS_CXX17
#include <any>
#include <initializer_list>
#include <optional>
#endif // _HAS_CXX17

#define STATIC_ASSERT(...) static_assert(__VA_ARGS__, #__VA_ARGS__)


// tests for shared_ptr<T>::operator=
template <class T, class U, class = void>
constexpr bool can_shared_ptr_assign = false;

template <class T, class U>
constexpr bool can_shared_ptr_assign<T, U, std::void_t<decltype(std::declval<std::shared_ptr<T>&>() = std::declval<U>())>> = true;

STATIC_ASSERT(can_shared_ptr_assign<int, std::shared_ptr<int>>);
STATIC_ASSERT(!can_shared_ptr_assign<int, std::shared_ptr<long>>);
STATIC_ASSERT(!can_shared_ptr_assign<int, const std::shared_ptr<long>&>);
#if _HAS_AUTO_PTR_ETC
STATIC_ASSERT(!can_shared_ptr_assign<int, std::auto_ptr<long>>);
#endif

// tests for shared_ptr<T>::reset
template <class Void, class T, class... Us>
constexpr bool can_shared_ptr_reset_impl = false;

template <class T, class... Us>
constexpr bool
    can_shared_ptr_reset_impl<std::void_t<decltype(std::declval<std::shared_ptr<T>&>().reset(std::declval<Us>()...))>, T, Us...> = true;

template <class T, class... Us>
constexpr bool can_shared_ptr_reset = can_shared_ptr_reset_impl<void, T, Us...>;

STATIC_ASSERT(can_shared_ptr_reset<int, int*>);
STATIC_ASSERT(!can_shared_ptr_reset<int, long*>);
STATIC_ASSERT(!can_shared_ptr_reset<int, long*, std::default_delete<long>>);
STATIC_ASSERT(!can_shared_ptr_reset<int, long*, std::default_delete<long>, std::allocator<long>>);

// tests for weak_ptr<T>::operator=
template <class T, class U, class = void>
constexpr bool can_weak_ptr_assign = false;

template <class T, class U>
constexpr bool can_weak_ptr_assign<T, U, std::void_t<decltype(std::declval<std::weak_ptr<T>&>() = std::declval<U>())>> = true;

STATIC_ASSERT(can_weak_ptr_assign<int, std::weak_ptr<int>>);
STATIC_ASSERT(!can_weak_ptr_assign<int, std::weak_ptr<long>>);
STATIC_ASSERT(!can_weak_ptr_assign<int, const std::weak_ptr<long>&>);
STATIC_ASSERT(!can_weak_ptr_assign<int, const std::shared_ptr<long>&>);

#if _HAS_CXX17
// tests for make_optional
template <class T, class = void>
constexpr bool can_make_optional_decay = false;

template <class T>
constexpr bool can_make_optional_decay<T, std::void_t<decltype(std::make_optional(std::declval<T>()))>> = true;

template <class Void, class T, class... Us>
constexpr bool can_make_optional_impl = false;

template <class T, class... Us>
constexpr bool can_make_optional_impl<std::void_t<decltype(std::make_optional<T>(std::declval<Us>()...))>, T, Us...> = true;

template <class T, class... Us>
constexpr bool can_make_optional_usual = can_make_optional_impl<void, T, Us...>;

STATIC_ASSERT(can_make_optional_usual<int, int>);
STATIC_ASSERT(!can_make_optional_usual<int, int, int>);
STATIC_ASSERT(!can_make_optional_usual<int, std::initializer_list<int>&>);

// tests for make_any
template <class Void, class T, class... Us>
constexpr bool can_make_any_impl = false;

template <class T, class... Us>
constexpr bool can_make_any_impl<std::void_t<decltype(std::make_any<T>(std::declval<Us>()...))>, T, Us...> = true;

template <class T, class... Us>
constexpr bool can_make_any = can_make_any_impl<void, T, Us...>;

STATIC_ASSERT(can_make_any<int, int>);
STATIC_ASSERT(!can_make_any<int, int, int>);
STATIC_ASSERT(!can_make_any<int, std::initializer_list<int>&>);
#endif // _HAS_CXX17

int main()
{
    return 0;
}
