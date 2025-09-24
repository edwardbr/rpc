// Copyright (c) Microsoft Corporation.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <atomic>
#include <cassert>
#include <chrono>
#include <memory>
#include <thread>

template <class UnderlyingType>
void test_atomic_wait_func_ptr(UnderlyingType old_value, const UnderlyingType new_value,
    const std::chrono::steady_clock::duration waiting_duration) {
    std::atomic<UnderlyingType> a(old_value);
    a.wait(new_value);

    std::thread thd([&] {
        std::this_thread::sleep_for(waiting_duration);
        a.notify_all();
        a.store(old_value);
        a.notify_one();
        a.store(new_value);
        a.notify_one();
    });

    a.wait(old_value);
    const auto loaded = a.load();
    assert(loaded.get() == new_value.get() || !loaded.owner_before(new_value) && !new_value.owner_before(loaded));
    thd.join();
}

template <class UnderlyingType>
void test_notify_all_notifies_all_ptr(UnderlyingType old_value, const UnderlyingType new_value,
    const std::chrono::steady_clock::duration waiting_duration) {
    std::atomic<UnderlyingType> c(old_value);
    const auto waitFn = [&c, old_value] { c.wait(old_value); };

    std::thread w1{waitFn};
    std::thread w2{waitFn};
    std::thread w3{waitFn};

    std::this_thread::sleep_for(3 * waiting_duration);
    c.store(new_value);
    c.notify_all(); // if this doesn't really notify all, the following joins will deadlock

    w1.join();
    w2.join();
    w3.join();
}

template <class T, class U>
[[nodiscard]] bool ownership_equal(const T& t, const U& u) {
    return !t.owner_before(u) && !u.owner_before(t);
}

inline void test_gh_3602() {
    // GH-3602 std::atomic<std::shared_ptr>::wait does not seem to care about control block difference. Is this a bug?
    {
        auto sp1    = std::make_shared<char>();
        auto holder = [sp1] {};
        auto sp2    = std::make_shared<decltype(holder)>(holder);
        std::shared_ptr<char> sp3{sp2, sp1.get()};

        std::atomic<std::shared_ptr<char>> asp{sp1};
        asp.wait(sp3);
    }
    {
        auto sp1    = std::make_shared<char>();
        auto holder = [sp1] {};
        auto sp2    = std::make_shared<decltype(holder)>(holder);
        std::shared_ptr<char> sp3{sp2, sp1.get()};
        std::weak_ptr<char> wp3{sp3};

        std::atomic<std::weak_ptr<char>> awp{sp1};
        awp.wait(wp3);
    }

    {
        auto sp1    = std::make_shared<char>();
        auto holder = [sp1] {};
        auto sp2    = std::make_shared<decltype(holder)>(holder);
        std::shared_ptr<char> sp3{sp2, sp1.get()};

        std::atomic<std::shared_ptr<char>> asp{sp3};

        std::thread t([&] {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            asp = sp1;
            asp.notify_one();
        });

        asp.wait(sp3);

        t.join();
    }

    { // Also test shared_ptrs that own the null pointer.
        int* const raw = nullptr;

        std::shared_ptr<int> sp_empty;
        std::shared_ptr<int> sp_also_empty;
        std::shared_ptr<int> sp_original(raw);
        std::shared_ptr<int> sp_copy(sp_original);
        std::shared_ptr<int> sp_different(raw);

        assert(ownership_equal(sp_empty, sp_also_empty));
        assert(!ownership_equal(sp_original, sp_empty));
        assert(ownership_equal(sp_original, sp_copy));
        assert(!ownership_equal(sp_original, sp_different));

        std::atomic<std::shared_ptr<int>> asp_empty;
        asp_empty.wait(sp_original);

        std::atomic<std::shared_ptr<int>> asp_copy{sp_copy};
        asp_copy.wait(sp_empty);
        asp_copy.wait(sp_different);
    }
}

int main() {
    constexpr std::chrono::milliseconds waiting_duration{100};

    // Test atomic operations with shared_ptr and weak_ptr
    test_atomic_wait_func_ptr(std::make_shared<int>('a'), std::make_shared<int>('a'), waiting_duration);
    test_atomic_wait_func_ptr(
        std::weak_ptr{std::make_shared<int>('a')}, std::weak_ptr{std::make_shared<int>('a')}, waiting_duration);
    test_atomic_wait_func_ptr(std::make_shared<int[]>(0), std::make_shared<int[]>(0), waiting_duration);
    test_atomic_wait_func_ptr(
        std::weak_ptr{std::make_shared<int[]>(0)}, std::weak_ptr{std::make_shared<int[]>(0)}, waiting_duration);
    test_atomic_wait_func_ptr(std::make_shared<int[]>(1), std::make_shared<int[]>(1), waiting_duration);
    test_atomic_wait_func_ptr(
        std::weak_ptr{std::make_shared<int[]>(1)}, std::weak_ptr{std::make_shared<int[]>(1)}, waiting_duration);
    test_atomic_wait_func_ptr(std::make_shared<int[2]>(), std::make_shared<int[2]>(), waiting_duration);
    test_atomic_wait_func_ptr(
        std::weak_ptr{std::make_shared<int[2]>()}, std::weak_ptr{std::make_shared<int[2]>()}, waiting_duration);
    test_atomic_wait_func_ptr(std::make_shared<int[][2]>(2), std::make_shared<int[][2]>(2), waiting_duration);
    test_atomic_wait_func_ptr(
        std::weak_ptr{std::make_shared<int[][2]>(2)}, std::weak_ptr{std::make_shared<int[][2]>(2)}, waiting_duration);
    test_atomic_wait_func_ptr(std::make_shared<int[2][2]>(), std::make_shared<int[2][2]>(), waiting_duration);
    test_atomic_wait_func_ptr(
        std::weak_ptr{std::make_shared<int[2][2]>()}, std::weak_ptr{std::make_shared<int[2][2]>()}, waiting_duration);

    test_notify_all_notifies_all_ptr(std::make_shared<int>('a'), std::make_shared<int>('a'), waiting_duration);
    test_notify_all_notifies_all_ptr(
        std::weak_ptr{std::make_shared<int>('a')}, std::weak_ptr{std::make_shared<int>('a')}, waiting_duration);
    test_notify_all_notifies_all_ptr(std::make_shared<int[]>(0), std::make_shared<int[]>(0), waiting_duration);
    test_notify_all_notifies_all_ptr(
        std::weak_ptr{std::make_shared<int[]>(0)}, std::weak_ptr{std::make_shared<int[]>(0)}, waiting_duration);
    test_notify_all_notifies_all_ptr(std::make_shared<int[]>(1), std::make_shared<int[]>(1), waiting_duration);
    test_notify_all_notifies_all_ptr(
        std::weak_ptr{std::make_shared<int[]>(1)}, std::weak_ptr{std::make_shared<int[]>(1)}, waiting_duration);
    test_notify_all_notifies_all_ptr(std::make_shared<int[2]>(), std::make_shared<int[2]>(), waiting_duration);
    test_notify_all_notifies_all_ptr(
        std::weak_ptr{std::make_shared<int[2]>()}, std::weak_ptr{std::make_shared<int[2]>()}, waiting_duration);
    test_notify_all_notifies_all_ptr(std::make_shared<int[][2]>(2), std::make_shared<int[][2]>(2), waiting_duration);
    test_notify_all_notifies_all_ptr(
        std::weak_ptr{std::make_shared<int[][2]>(2)}, std::weak_ptr{std::make_shared<int[][2]>(2)}, waiting_duration);
    test_notify_all_notifies_all_ptr(std::make_shared<int[2][2]>(), std::make_shared<int[2][2]>(), waiting_duration);
    test_notify_all_notifies_all_ptr(
        std::weak_ptr{std::make_shared<int[2][2]>()}, std::weak_ptr{std::make_shared<int[2][2]>()}, waiting_duration);

    test_gh_3602();
}
