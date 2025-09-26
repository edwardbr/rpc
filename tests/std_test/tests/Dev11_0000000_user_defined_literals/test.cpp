// Copyright (c) Microsoft Corporation.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <cassert>
#include <memory>


int main() {
    // LWG-2315 "weak_ptr should be movable"
    {
        std::shared_ptr<int> sp = std::make_shared<int>(1729);

        std::weak_ptr<int> wp1(sp);
        assert(wp1.lock() == sp);

        std::weak_ptr<int> wp2(std::move(wp1));
        assert(wp1.expired());
        assert(wp2.lock() == sp);

        std::weak_ptr<int> wp3;
        wp3 = std::move(wp2);
        assert(wp2.expired());
        assert(wp3.lock() == sp);

        std::weak_ptr<const int> wp4(std::move(wp3));
        assert(wp3.expired());
        assert(wp4.lock() == sp);

        wp1 = sp;
        assert(wp1.lock() == sp);
        std::weak_ptr<const int> wp5;
        wp5 = std::move(wp1);
        assert(wp1.expired());
        assert(wp5.lock() == sp);
    }
}