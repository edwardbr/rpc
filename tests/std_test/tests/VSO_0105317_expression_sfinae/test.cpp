// Copyright (c) Microsoft Corporation.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <functional>
#include <rpc/internal/remote_pointer.h>
#include <type_traits>

using namespace std;

#define STATIC_ASSERT(...) static_assert(__VA_ARGS__, #__VA_ARGS__)

// Test shared_ptr with function type conversions and SFINAE
using SharedPtrInt_FP_Nullary = shared_ptr<int> (*)();

// Test basic SFINAE with shared_ptr return types
STATIC_ASSERT(is_invocable_r_v<shared_ptr<const int>, SharedPtrInt_FP_Nullary>);
STATIC_ASSERT(!is_invocable_r_v<bool, SharedPtrInt_FP_Nullary>);

// Test std::function compatibility with shared_ptr
// GOOD: Output implicitly convertible.
STATIC_ASSERT(is_constructible_v<function<shared_ptr<const int>()>, shared_ptr<int> (*)()>);
STATIC_ASSERT(is_assignable_v<function<shared_ptr<const int>()>&, shared_ptr<int> (*)()>);

// BAD: Output explicitly convertible, not implicitly.
STATIC_ASSERT(!is_constructible_v<function<bool()>, shared_ptr<int> (*)()>);
STATIC_ASSERT(!is_assignable_v<function<bool()>&, shared_ptr<int> (*)()>);

// Test allocator-aware function construction with shared_ptr
#if _HAS_FUNCTION_ALLOCATOR_SUPPORT
struct Al {};
struct Tag {};

STATIC_ASSERT(is_constructible_v<function<shared_ptr<const int>()>, Tag, Al, shared_ptr<int> (*)()>);
STATIC_ASSERT(!is_constructible_v<function<bool()>, Tag, Al, shared_ptr<int> (*)()>);
#endif // _HAS_FUNCTION_ALLOCATOR_SUPPORT

int main() {}