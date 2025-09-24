// Copyright (c) Microsoft Corporation.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <cassert>
#include <memory>

using namespace std;

int main() {
    // LWG-2315 "weak_ptr should be movable"
    {
        shared_ptr<int> sp = make_shared<int>(1729);

        weak_ptr<int> wp1(sp);
        assert(wp1.lock() == sp);

        weak_ptr<int> wp2(std::move(wp1));
        assert(wp1.expired());
        assert(wp2.lock() == sp);

        weak_ptr<int> wp3;
        wp3 = std::move(wp2);
        assert(wp2.expired());
        assert(wp3.lock() == sp);

        weak_ptr<const int> wp4(std::move(wp3));
        assert(wp3.expired());
        assert(wp4.lock() == sp);

        wp1 = sp;
        assert(wp1.lock() == sp);
        weak_ptr<const int> wp5;
        wp5 = std::move(wp1);
        assert(wp1.expired());
        assert(wp5.lock() == sp);
    }
}