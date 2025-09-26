// Copyright (c) Microsoft Corporation.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <cassert>
#include <memory>
#include <string>
#include <utility>


class Base {
public:
    Base() {}
    virtual ~Base() {}

    virtual std::string str() const {
        return "Base";
    }

private:
    Base(const Base&);
    Base& operator=(const Base&);
};

class Derived : public Base {
public:
    std::string str() const override {
        return "Derived";
    }
};

int main() {
    {
        // move ctor

        std::shared_ptr<int> src(new int(1729));

        assert(src && *src == 1729);

        std::shared_ptr<int> dest(std::move(src));

        assert(!src);

        assert(dest && *dest == 1729);
    }

    {
        // move assign

        std::shared_ptr<int> src(new int(123));

        std::shared_ptr<int> dest(new int(888));

        assert(src && *src == 123);

        assert(dest && *dest == 888);

        dest = std::move(src);

        assert(!src);

        assert(dest && *dest == 123);
    }

    {
        // template move ctor

        std::shared_ptr<Derived> src(new Derived);

        assert(src && src->str() == "Derived");

        std::shared_ptr<Base> dest(std::move(src));

        assert(!src);

        assert(dest && dest->str() == "Derived");
    }

    {
        // template move assign

        std::shared_ptr<Derived> src(new Derived);

        std::shared_ptr<Base> dest(new Base);

        assert(src && src->str() == "Derived");

        assert(dest && dest->str() == "Base");

        dest = std::move(src);

        assert(!src);

        assert(dest && dest->str() == "Derived");
    }
}
