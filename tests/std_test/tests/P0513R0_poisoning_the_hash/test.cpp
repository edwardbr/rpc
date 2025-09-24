// Copyright (c) Microsoft Corporation.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <cassert>
#include <cstddef>
#include <functional>
#include <memory>
#include <type_traits>

using namespace std;

#define STATIC_ASSERT(...) static_assert(__VA_ARGS__, #__VA_ARGS__)

template <typename T, typename = void>
struct hash_callable : false_type {};

template <typename T>
struct hash_callable<T, void_t<decltype(declval<hash<T>>()(declval<const T&>()))>> : true_type {};

template <typename T, bool NoExcept = true>
constexpr bool standard_hash_enabled() {
    return is_trivially_default_constructible_v<hash<T>> //
        && is_trivially_copy_constructible_v<hash<T>> //
        && is_trivially_move_constructible_v<hash<T>> //
        && is_trivially_copy_assignable_v<hash<T>> //
        && is_trivially_move_assignable_v<hash<T>> //
        && hash_callable<T>::value //
        && (NoExcept ? is_nothrow_invocable_v<hash<T>, const T&> : is_invocable_v<hash<T>, const T&>);
}

void test_shared_ptr_hash_invariants() {
    STATIC_ASSERT(standard_hash_enabled<shared_ptr<int>>());
    const auto x = make_shared<int>(70);
    assert(hash<shared_ptr<int>>{}(x) == hash<int*>{}(x.get()));
}

int main() {
    test_shared_ptr_hash_invariants();
}