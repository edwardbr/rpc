// Copyright (c) Microsoft Corporation.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <cassert>
#include <rpc/internal/remote_pointer.h>
#include <type_traits>

using namespace std;

#define STATIC_ASSERT(...) static_assert(__VA_ARGS__, #__VA_ARGS__)

struct diagnostic_context
{
    virtual ~diagnostic_context() = default;
    virtual const char* component() const noexcept = 0;
};

struct regex_constants
{
    enum error_type
    {
        error_paren = 1
    };
};

class regex_error : public logic_error, public diagnostic_context
{
public:
    explicit regex_error(regex_constants::error_type error_code)
        : logic_error("regex_error")
        , error_code_(error_code)
    {
    }

    regex_constants::error_type code() const noexcept
    {
        return error_code_;
    }

    const char* component() const noexcept override
    {
        return "test_regex";
    }

private:
    regex_constants::error_type error_code_;
};

// Test RTTI functionality with shared_ptr
template <typename To, typename From>
using DynamicPointerCastResult = decltype(dynamic_pointer_cast<To>(declval<From>()));

template <typename To, typename From, typename = void>
struct HasDynamicPointerCast : false_type {};

template <typename To, typename From>
struct HasDynamicPointerCast<To, From, void_t<DynamicPointerCastResult<To, From>>> : true_type {};

// Test get_deleter functionality with shared_ptr
template <typename Deleter, typename SmartPtr>
using GetDeleterResult = decltype(get_deleter<Deleter>(declval<SmartPtr>()));

template <typename Deleter, typename SmartPtr, typename = void>
struct HasGetDeleter : false_type {};

template <typename Deleter, typename SmartPtr>
struct HasGetDeleter<Deleter, SmartPtr, void_t<GetDeleterResult<Deleter, SmartPtr>>> : true_type {};

int main() {
    // Test dynamic_pointer_cast with shared_ptr
    {
        const shared_ptr<exception> base      = make_shared<regex_error>(regex_constants::error_paren);
        const shared_ptr<regex_error> derived = dynamic_pointer_cast<regex_error>(base);
        assert(derived && derived->code() == regex_constants::error_paren);

        STATIC_ASSERT(HasDynamicPointerCast<logic_error, shared_ptr<exception>>::value);
    }

    // Test get_deleter with shared_ptr
    {
        shared_ptr<int> sp1(new int(11));
        shared_ptr<int> sp2(new int(22), default_delete<int>{});
        shared_ptr<int> sp3(new int(33), default_delete<int>{}, allocator<int>{});

        assert(!get_deleter<default_delete<int>>(sp1));
        assert(get_deleter<default_delete<int>>(sp2) != nullptr);
        assert(get_deleter<default_delete<int>>(sp3) != nullptr);

        STATIC_ASSERT(HasGetDeleter<default_delete<short>, shared_ptr<short>>::value);
    }
}
