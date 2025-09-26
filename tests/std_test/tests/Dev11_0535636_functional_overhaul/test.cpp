// Copyright (c) Microsoft Corporation.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <functional>
#include <memory>
#include <string>
#include <type_traits>


#define STATIC_ASSERT(...) static_assert(__VA_ARGS__, #__VA_ARGS__)

// Test shared_ptr with member function pointers and result_of
struct Base {
    bool member_func() { return true; }
    std::string data;
};

struct Derived : Base {};

// Test PMF (Pointer to Member Function) with shared_ptr
using PmfBase = bool (Base::*)();
STATIC_ASSERT(std::is_same_v<std::result_of_t<PmfBase(std::shared_ptr<Base>)>, bool>);
STATIC_ASSERT(std::is_same_v<std::result_of_t<PmfBase(std::shared_ptr<Derived>)>, bool>);

// Test PMD (Pointer to Member Data) with shared_ptr
using PmdPlain = std::string Base::*;
STATIC_ASSERT(std::is_same_v<std::result_of_t<PmdPlain(std::shared_ptr<Base>)>, std::string&>);
STATIC_ASSERT(std::is_same_v<std::result_of_t<PmdPlain(std::shared_ptr<const Base>)>, const std::string&>);
STATIC_ASSERT(std::is_same_v<std::result_of_t<PmdPlain(std::shared_ptr<volatile Base>)>, volatile std::string&>);
STATIC_ASSERT(std::is_same_v<std::result_of_t<PmdPlain(std::shared_ptr<const volatile Base>)>, const volatile std::string&>);

using PmdConst = const std::string Base::*;
STATIC_ASSERT(std::is_same_v<std::result_of_t<PmdConst(std::shared_ptr<Base>)>, const std::string&>);
STATIC_ASSERT(std::is_same_v<std::result_of_t<PmdConst(std::shared_ptr<const Base>)>, const std::string&>);
STATIC_ASSERT(std::is_same_v<std::result_of_t<PmdConst(std::shared_ptr<volatile Base>)>, const volatile std::string&>);
STATIC_ASSERT(std::is_same_v<std::result_of_t<PmdConst(std::shared_ptr<const volatile Base>)>, const volatile std::string&>);

int main() {}