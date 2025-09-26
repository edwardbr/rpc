// Copyright (c) Microsoft Corporation.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#define _HAS_AUTO_PTR_ETC 1
#include <cassert>
#include <memory>
#include <utility>


class A : public std::enable_shared_from_this<A> {
public:
    explicit A(const int n) : m_n(n) {}

    int num() const {
        return m_n;
    }

private:
    int m_n;
};

int main() {
    {
        std::auto_ptr<A> a(new A(4));
        std::shared_ptr<A> s(std::move(a));
        std::shared_ptr<A> t = s->shared_from_this();
        assert(t->num() == 4);
    }

    {
        std::auto_ptr<A> a(new A(7));
        std::shared_ptr<A> s;
        s               = std::move(a);
        std::shared_ptr<A> t = s->shared_from_this();
        assert(t->num() == 7);
    }
}
