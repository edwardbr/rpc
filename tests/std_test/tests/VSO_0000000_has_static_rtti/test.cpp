// Copyright (c) Microsoft Corporation.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-std::exception

#include <cassert>
#include <memory>
#include <regex>
#include <type_traits>

#define STATIC_ASSERT(...) static_assert(__VA_ARGS__, #__VA_ARGS__)

// Test RTTI functionality with std::shared_ptr
template <typename To, typename From>
using DynamicPointerCastResult = decltype(std::dynamic_pointer_cast<To>(std::declval<From>()));
template <typename To, typename From, typename = void>
struct HasDynamicPointerCast : std::false_type {};

template <typename To, typename From>
struct HasDynamicPointerCast<To, From, std::void_t<DynamicPointerCastResult<To, From>>> : std::true_type {};

// Test get_deleter functionality with std::shared_ptr
template <typename Deleter, typename SmartPtr>
using GetDeleterResult = decltype(std::get_deleter<Deleter>(std::declval<SmartPtr>()));
template <typename Deleter, typename SmartPtr, typename = void>
struct HasGetDeleter : std::false_type {};

template <typename Deleter, typename SmartPtr>
struct HasGetDeleter<Deleter, SmartPtr, std::void_t<GetDeleterResult<Deleter, SmartPtr>>> : std::true_type {};

int main() {
    // Test dynamic_pointer_cast with std::shared_ptr
    {
        const std::shared_ptr<std::exception> base      = std::make_shared<std::regex_error>(std::regex_constants::error_paren);
        const std::shared_ptr<std::regex_error> derived = std::dynamic_pointer_cast<std::regex_error>(base);
        assert(derived && derived->code() == std::regex_constants::error_paren);
        STATIC_ASSERT(HasDynamicPointerCast<std::logic_error, std::shared_ptr<std::exception>>::value);
    }

    // Test get_deleter with std::shared_ptr
    {
        std::shared_ptr<int> sp1(new int(11));
        std::shared_ptr<int> sp2(new int(22), std::default_delete<int>{});
        std::shared_ptr<int> sp3(new int(33), std::default_delete<int>{}, std::allocator<int>{});

        assert(!std::get_deleter<std::default_delete<int>>(sp1));
        assert(std::get_deleter<std::default_delete<int>>(sp2) != nullptr);
        assert(std::get_deleter<std::default_delete<int>>(sp3) != nullptr);
        STATIC_ASSERT(HasGetDeleter<std::default_delete<short>, std::shared_ptr<short>>::value);
    }
}