// Copyright (c) Microsoft Corporation.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <functional>
#include <memory>
#include <string>
#include <type_traits>

using namespace std;

#define STATIC_ASSERT(...) static_assert(__VA_ARGS__, #__VA_ARGS__)

// Test shared_ptr with member function pointers and result_of
struct Base {
    bool member_func() { return true; }
    string data;
};

struct Derived : Base {};

// Test PMF (Pointer to Member Function) with shared_ptr
using PmfBase = bool (Base::*)();
STATIC_ASSERT(is_same_v<result_of_t<PmfBase(shared_ptr<Base>)>, bool>);
STATIC_ASSERT(is_same_v<result_of_t<PmfBase(shared_ptr<Derived>)>, bool>);

// Test PMD (Pointer to Member Data) with shared_ptr
using PmdPlain = string Base::*;
STATIC_ASSERT(is_same_v<result_of_t<PmdPlain(shared_ptr<Base>)>, string&>);
STATIC_ASSERT(is_same_v<result_of_t<PmdPlain(shared_ptr<const Base>)>, const string&>);
STATIC_ASSERT(is_same_v<result_of_t<PmdPlain(shared_ptr<volatile Base>)>, volatile string&>);
STATIC_ASSERT(is_same_v<result_of_t<PmdPlain(shared_ptr<const volatile Base>)>, const volatile string&>);

using PmdConst = const string Base::*;
STATIC_ASSERT(is_same_v<result_of_t<PmdConst(shared_ptr<Base>)>, const string&>);
STATIC_ASSERT(is_same_v<result_of_t<PmdConst(shared_ptr<const Base>)>, const string&>);
STATIC_ASSERT(is_same_v<result_of_t<PmdConst(shared_ptr<volatile Base>)>, const volatile string&>);
STATIC_ASSERT(is_same_v<result_of_t<PmdConst(shared_ptr<const volatile Base>)>, const volatile string&>);

int main() {}