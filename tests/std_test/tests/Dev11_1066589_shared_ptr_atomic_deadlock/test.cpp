// Copyright (c) Microsoft Corporation.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#define _SILENCE_CXX20_OLD_SHARED_PTR_ATOMIC_SUPPORT_DEPRECATION_WARNING

#include <cstdlib>
#include <memory>

#ifndef _M_CEE_PURE
#include <atomic>
#endif // _M_CEE_PURE


std::shared_ptr<int> g_sp;

struct Noisy {
    Noisy() = default;

    ~Noisy() {
        (void) std::atomic_load(&g_sp);
    }

    Noisy(const Noisy&)            = delete;
    Noisy& operator=(const Noisy&) = delete;
};

int main() {
    {
        std::shared_ptr<Noisy> dest;
        std::shared_ptr<Noisy> src;
        std::atomic_store(&dest, src);
    }

    {
        std::shared_ptr<Noisy> dest;
        std::shared_ptr<Noisy> src = std::make_shared<Noisy>();
        std::atomic_store(&dest, src);
    }

    {
        std::shared_ptr<Noisy> dest = std::make_shared<Noisy>();
        std::shared_ptr<Noisy> src;
        std::atomic_store(&dest, src);
    }

    {
        std::shared_ptr<Noisy> dest = std::make_shared<Noisy>();
        std::shared_ptr<Noisy> src  = dest;
        std::atomic_store(&dest, src);
    }

    {
        std::shared_ptr<Noisy> dest = std::make_shared<Noisy>();
        std::shared_ptr<Noisy> src  = std::make_shared<Noisy>();
        std::atomic_store(&dest, src);
    }

    // **********

    {
        std::shared_ptr<Noisy> dest;
        std::shared_ptr<Noisy> src;
        std::atomic_exchange(&dest, src);
    }

    {
        std::shared_ptr<Noisy> dest;
        std::shared_ptr<Noisy> src = std::make_shared<Noisy>();
        std::atomic_exchange(&dest, src);
    }

    {
        std::shared_ptr<Noisy> dest = std::make_shared<Noisy>();
        std::shared_ptr<Noisy> src;
        std::atomic_exchange(&dest, src);
    }

    {
        std::shared_ptr<Noisy> dest = std::make_shared<Noisy>();
        std::shared_ptr<Noisy> src  = dest;
        std::atomic_exchange(&dest, src);
    }

    {
        std::shared_ptr<Noisy> dest = std::make_shared<Noisy>();
        std::shared_ptr<Noisy> src  = std::make_shared<Noisy>();
        std::atomic_exchange(&dest, src);
    }

    // **********

    {
        std::shared_ptr<Noisy> object;
        std::shared_ptr<Noisy> expected = std::make_shared<Noisy>();
        std::shared_ptr<Noisy> desired;
        std::atomic_compare_exchange_weak(&object, &expected, desired);
    }

    {
        // Nothing forbids aliasing here.
        std::shared_ptr<Noisy> object = std::make_shared<Noisy>();
        std::shared_ptr<Noisy> desired;
        std::atomic_compare_exchange_weak(&object, &object, desired);
    }

    // **********

    {
        std::shared_ptr<Noisy> object;
        std::shared_ptr<Noisy> expected = std::make_shared<Noisy>();
        std::shared_ptr<Noisy> desired;
        std::atomic_compare_exchange_strong(&object, &expected, desired);
    }

    {
        // Nothing forbids aliasing here.
        std::shared_ptr<Noisy> object = std::make_shared<Noisy>();
        std::shared_ptr<Noisy> desired;
        std::atomic_compare_exchange_strong(&object, &object, desired);
    }

    // Also test VSO-911206 "Error specifying explicit template argument of abstract type for atomic non-member
    // functions"
    {
        struct Base {
            virtual ~Base()     = default;
            virtual void test() = 0;
        };

        struct Derived final : Base {
            void test() override {}
        };

        std::shared_ptr<Base> object;

        std::atomic_store<Base>(&object, std::make_shared<Derived>());
        std::atomic_store_explicit<Base>(&object, std::make_shared<Derived>(), std::memory_order_seq_cst);

        std::atomic_exchange<Base>(&object, std::make_shared<Derived>());
        std::atomic_exchange_explicit<Base>(&object, std::make_shared<Derived>(), std::memory_order_seq_cst);

        std::atomic_compare_exchange_weak<Base>(&object, &object, std::make_shared<Derived>());
        std::atomic_compare_exchange_weak_explicit<Base>(
            &object, &object, std::make_shared<Derived>(), std::memory_order_seq_cst, std::memory_order_seq_cst);

        std::atomic_compare_exchange_strong<Base>(&object, &object, std::make_shared<Derived>());
        std::atomic_compare_exchange_strong_explicit<Base>(
            &object, &object, std::make_shared<Derived>(), std::memory_order_seq_cst, std::memory_order_seq_cst);
    }
}
