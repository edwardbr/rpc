// Copyright (c) Microsoft Corporation.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <cassert>
#include <cstddef>
#include <functional>
#include <memory>
#include <type_traits>

#define STATIC_ASSERT(...) static_assert(__VA_ARGS__, #__VA_ARGS__)

template <typename T, typename = void>
struct hash_callable : std::false_type {};

template <typename T>
struct hash_callable<T, std::void_t<decltype(std::declval<std::hash<T>>()(std::declval<const T&>()))>> : std::true_type {};
template <typename T, bool NoExcept = true>
constexpr bool standard_hash_enabled() {
    return std::is_trivially_default_constructible_v<std::hash<T>> //
        && std::is_trivially_copy_constructible_v<std::hash<T>> //
        && std::is_trivially_move_constructible_v<std::hash<T>> //
        && std::is_trivially_copy_assignable_v<std::hash<T>> //
        && std::is_trivially_move_assignable_v<std::hash<T>> //
        && hash_callable<T>::value //
        && (NoExcept ? std::is_nothrow_invocable_v<std::hash<T>, const T&> : std::is_invocable_v<std::hash<T>, const T&>);
}

void test_shared_ptr_hash_invariants() {
    STATIC_ASSERT(standard_hash_enabled<std::shared_ptr<int>>());
    const auto x = std::make_shared<int>(70);
    assert(std::hash<std::shared_ptr<int>>{}(x) == std::hash<int*>{}(x.get()));
}

int main() {
    test_shared_ptr_hash_invariants();
}