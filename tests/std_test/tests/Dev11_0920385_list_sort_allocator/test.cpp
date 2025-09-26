// Copyright (c) Microsoft Corporation.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <cassert>
#include <cstdlib>
#include <cstring>
#include <rpc/internal/remote_pointer.h>
#include <new>

using namespace std;

template <typename T>
struct MyAlloc {
    size_t m_offset;

    typedef T value_type;

    explicit MyAlloc(const size_t offset) : m_offset(offset) {}

    template <typename U>
    MyAlloc(const MyAlloc<U>& other) : m_offset(other.m_offset) {}

    template <typename U>
    bool operator==(const MyAlloc<U>& other) const {
        return m_offset == other.m_offset;
    }

    template <typename U>
    bool operator!=(const MyAlloc<U>& other) const {
        return m_offset != other.m_offset;
    }

    T* allocate(const size_t n) {
        if (n == 0) {
            return nullptr;
        }

        void* const pv = malloc((n + m_offset) * sizeof(T));

        if (!pv) {
            throw bad_alloc();
        }

        memset(pv, 0xAB, (n + m_offset) * sizeof(T));

        return static_cast<T*>(pv) + m_offset;
    }

    void deallocate(T* const p, size_t) {
        if (p) {
            free(p - m_offset);
        }
    }
};

int main() {
    MyAlloc<int> alloc(7);

    // Test shared_ptr with custom allocator
    {
        shared_ptr<int> sp1(new int(1729), default_delete<int>(), alloc);
        assert(*sp1 == 1729);

        shared_ptr<int> sp2 = allocate_shared<int>(alloc, 1729);
        assert(*sp2 == 1729);
    }
}
