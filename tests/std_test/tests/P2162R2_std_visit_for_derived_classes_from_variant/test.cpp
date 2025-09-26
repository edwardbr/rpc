// Copyright (c) Microsoft Corporation.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <cassert>
#include <memory>
#include <utility>
#include <variant>


struct Disconnected {
    int val;
};

struct Connecting {
    char val;
};

struct Connected {
    double val;
};

struct State : std::variant<Disconnected, Connecting, Connected> {
    using variant::variant;
};

void example1_from_p2162r2() {
    State v1       = Disconnected{45};
    const State v2 = Connecting{'d'};
    std::visit([](auto x) { assert(x.val == 45); }, v1);
    std::visit([](auto x) { assert(x.val == 'd'); }, v2);
    std::visit([](auto x) { assert(x.val == 5.5); }, State{Connected{5.5}});
    std::visit([](auto x) { assert(x.val == 45); }, std::move(v1));
    std::visit([](auto x) { assert(x.val == 'd'); }, std::move(v2));
}

struct Expr;

struct Neg {
    std::shared_ptr<Expr> expr;
};

struct Add {
    std::shared_ptr<Expr> lhs;
    std::shared_ptr<Expr> rhs;
};

struct Mul {
    std::shared_ptr<Expr> lhs;
    std::shared_ptr<Expr> rhs;
};

struct Expr : std::variant<int, Neg, Add, Mul> {
    using variant::variant;
};

int eval(const Expr& expr) {
    struct visitor {
        int operator()(int i) const {
            return i;
        }
        int operator()(const Neg& n) const {
            return -eval(*n.expr);
        }
        int operator()(const Add& a) const {
            return eval(*a.lhs) + eval(*a.rhs);
        }
        int operator()(const Mul& m) const {
            return eval(*m.lhs) * eval(*m.rhs);
        }
    };
    return std::visit(visitor{}, expr);
}

void example2_from_p2162r2() {
    // (1) + (2*3)
    const Expr e = Add{std::make_shared<Expr>(1), std::make_shared<Expr>(Mul{std::make_shared<Expr>(2), std::make_shared<Expr>(3)})};
    assert(eval(e) == (1 + 2 * 3));
}

int main() {
    example1_from_p2162r2();
    example2_from_p2162r2();
}
