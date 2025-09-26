// Copyright (c) Microsoft Corporation.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <cassert>
#include <cstdlib>
#include <memory>

// Test std::shared_ptr and std::weak_ptr with custom allocators
int g_mallocs = 0;

template <typename T>
struct Mallocator {
    typedef T value_type;

    T* allocate(size_t n) {
        ++g_mallocs;
        return static_cast<T*>(malloc(sizeof(T) * n));
    }

    void deallocate(T* p, size_t) {
        free(p);
    }

    Mallocator() {}

    template <typename U>
    Mallocator(const Mallocator<U>&) {}
};

template <typename T, typename U>
bool operator==(const Mallocator<T>&, const Mallocator<U>&) {
    return true;
}

template <typename T, typename U>
bool operator!=(const Mallocator<T>&, const Mallocator<U>&) {
    return false;
}

void custom_deleter(int* p) {
    delete p;
}

int main() {
    // Test std::shared_ptr with custom deleter and allocator
    {
        int* const raw = new int(47);

        int initial_mallocs = g_mallocs;
std::shared_ptr<int> sp(raw, custom_deleter, Mallocator<double>());
        assert(g_mallocs > initial_mallocs); // allocator was used
std::weak_ptr<int> wp(sp);
        assert(wp.lock().get() == raw);
        assert(*wp.lock() == 47);

        sp.reset();
        assert(wp.expired());

        wp.reset();
    }

    // Test std::allocate_shared
    {
        int initial_mallocs = g_mallocs;
        std::shared_ptr<int> sp = std::allocate_shared<int>(Mallocator<int>(), 123);
        assert(g_mallocs > initial_mallocs); // allocator was used
        assert(*sp == 123);
    }
}