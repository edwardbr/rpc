// Copyright (c) Microsoft Corporation.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <array>
#include <atomic>
#include <bitset>
#include <cassert>
#include <chrono>
#include <complex>
#include <cstddef>
#include <deque>
#include <forward_list>
#include <functional>
#include <future>
#include <initializer_list>
#include <iterator>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <regex>
#include <scoped_allocator>
#include <set>
#include <shared_mutex>
#include <sstream>
#include <stack>
#include <streambuf>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <valarray>
#include <vector>

#if _HAS_CXX23 && !defined(__clang__) // TRANSITION, DevCom-10107077, Clang has not implemented Deducing this
#define HAS_EXPLICIT_THIS_PARAMETER
#endif // _HAS_CXX23 && !defined(__clang__)


template <typename T>
struct MyAlloc {
    using value_type = T;

    MyAlloc() = default;

    template <typename U>
    MyAlloc(const MyAlloc<U>&) {}

    T* allocate(size_t n) {
        return std::allocator<T>{}.allocate(n);
    }

    void deallocate(T* p, size_t n) {
        std::allocator<T>{}.deallocate(p, n);
    }

    template <typename U>
    bool operator==(const MyAlloc<U>&) const {
        return true;
    }
    template <typename U>
    bool operator!=(const MyAlloc<U>&) const {
        return false;
    }
};

struct MyGreater : std::greater<> {};

struct MyHash : std::hash<long> {};

struct MyWideHash : std::hash<wchar_t> {};

struct MyEqual : std::equal_to<> {};

struct MyDelete : std::default_delete<long[]> {};

void nothing() {}

int square(int x) {
    return x * x;
}

long add(short x, int y) {
    return x + y;
}

template <typename Void, template <typename...> class ClassTemplate, typename... CtorArgs>
struct CanDeduceFromHelper : std::false_type {};

template <template <typename...> class ClassTemplate, typename... CtorArgs>
struct CanDeduceFromHelper<std::void_t<decltype(ClassTemplate(std::declval<CtorArgs>()...))>, ClassTemplate, CtorArgs...>
    : std::true_type {};

template <template <typename...> class ClassTemplate, typename... CtorArgs>
constexpr bool CanDeduceFrom = CanDeduceFromHelper<void, ClassTemplate, CtorArgs...>::value;

void test_initializer_list() {
    std::initializer_list<long> il{};
    std::initializer_list il2(il);

    static_assert(std::is_same_v<decltype(il2), std::initializer_list<long>>);
}

// void test_pair_and_tuple() {
//     long x       = 11L;
//     const long y = 22L;

//     pair p1(x, y);
//     pair p2(33L, 'c');
//     pair p3(p2);

//     static_assert(std::is_same_v<decltype(p1), pair<long, long>>);
//     static_assert(std::is_same_v<decltype(p2), pair<long, char>>);
//     static_assert(std::is_same_v<decltype(p3), pair<long, char>>);

//     const int arr[] = {100, 200, 300};
//     const long& z   = y;
//     std::allocator<int> al{};

//     tuple t1{};
//     tuple t2(x);
//     tuple t3(x, y);
//     tuple t4('c', 44L, 7.89);
//     tuple t5(square, arr, z);
//     tuple t6(p2);
//     tuple t7(allocator_arg, al);
//     tuple t8(allocator_arg, al, 'c');
//     tuple t9(allocator_arg, al, 'c', 'c');
//     tuple t10(allocator_arg, al, p2);
//     tuple t11(allocator_arg, al, t4);
//     tuple t12(t11);

//     static_assert(std::is_same_v<decltype(tuple()), tuple<>>);
//     static_assert(std::is_same_v<decltype(tuple{}), tuple<>>);
//     static_assert(std::is_same_v<decltype(t1), tuple<>>);
//     static_assert(std::is_same_v<decltype(t2), tuple<long>>);
//     static_assert(std::is_same_v<decltype(t3), tuple<long, long>>);
//     static_assert(std::is_same_v<decltype(t4), tuple<char, long, double>>);
//     static_assert(std::is_same_v<decltype(t5), tuple<int (*)(int), const int*, long>>);
//     static_assert(std::is_same_v<decltype(t6), tuple<long, char>>);
//     static_assert(std::is_same_v<decltype(t7), tuple<>>);
//     static_assert(std::is_same_v<decltype(t8), tuple<char>>);
//     static_assert(std::is_same_v<decltype(t9), tuple<char, char>>);
//     static_assert(std::is_same_v<decltype(t10), tuple<long, char>>);
//     static_assert(std::is_same_v<decltype(t11), tuple<char, long, double>>);
//     static_assert(std::is_same_v<decltype(t12), tuple<char, long, double>>);
// }

void test_optional() {
    std::optional opt1(1729L);
    std::optional opt2(opt1);

    static_assert(std::is_same_v<decltype(opt1), std::optional<long>>);
    static_assert(std::is_same_v<decltype(opt2), std::optional<long>>);
}

void test_bitset() {
    std::bitset<7> b{};
    std::bitset b2(b);

    static_assert(std::is_same_v<decltype(b2), std::bitset<7>>);
}

void test_allocator() {
    std::allocator<long> alloc1{};
    std::allocator alloc2(alloc1);

    static_assert(std::is_same_v<decltype(alloc2), std::allocator<long>>);
}

void test_unique_ptr() {
    static_assert(!CanDeduceFrom<std::unique_ptr>);
    static_assert(!CanDeduceFrom<std::unique_ptr, nullptr_t>);
    static_assert(!CanDeduceFrom<std::unique_ptr, long*>);
    static_assert(!CanDeduceFrom<std::unique_ptr, long*, const std::default_delete<long>&>);
    static_assert(!CanDeduceFrom<std::unique_ptr, long*, std::default_delete<long>>);
    static_assert(CanDeduceFrom<std::unique_ptr, std::unique_ptr<double>>);

    std::unique_ptr<double> up1{};
    std::unique_ptr up2(std::move(up1));

    static_assert(std::is_same_v<decltype(up2), std::unique_ptr<double>>);
}

void test_shared_ptr_and_weak_ptr() {
    std::shared_ptr<long[]> sp(new long[3]);
    std::weak_ptr<long[]> wp(sp);
    std::unique_ptr<long[], MyDelete> up{};

    std::shared_ptr sp1(sp);
    std::shared_ptr sp2(wp);
    std::shared_ptr sp3(std::move(up));
    std::weak_ptr wp1(sp);
    std::weak_ptr wp2(wp);

    static_assert(std::is_same_v<decltype(sp1), std::shared_ptr<long[]>>);
    static_assert(std::is_same_v<decltype(sp2), std::shared_ptr<long[]>>);
    static_assert(std::is_same_v<decltype(sp3), std::shared_ptr<long[]>>);
    static_assert(std::is_same_v<decltype(wp1), std::weak_ptr<long[]>>);
    static_assert(std::is_same_v<decltype(wp2), std::weak_ptr<long[]>>);
}

void test_owner_less() {
    std::owner_less<std::shared_ptr<long>> ol1{};
    std::owner_less<std::weak_ptr<long>> ol2{};
    std::owner_less<> ol3{};

    std::owner_less ol4(ol1);
    std::owner_less ol5(ol2);
    std::owner_less ol6(ol3);
    std::owner_less ol7{};

    static_assert(std::is_same_v<decltype(ol4), std::owner_less<std::shared_ptr<long>>>);
    static_assert(std::is_same_v<decltype(ol5), std::owner_less<std::weak_ptr<long>>>);
    static_assert(std::is_same_v<decltype(ol6), std::owner_less<>>);
    static_assert(std::is_same_v<decltype(ol7), std::owner_less<>>);
}

// void test_scoped_allocator_adaptor() {
//     MyAlloc<short> myal_short{};
//     MyAlloc<int> myal_int{};
//     MyAlloc<long> myal_long{};

//     scoped_allocator_adaptor saa1(myal_short);
//     scoped_allocator_adaptor saa2(myal_short, myal_int);
//     scoped_allocator_adaptor saa3(myal_short, myal_int, myal_long);
//     scoped_allocator_adaptor saa4(saa3);

//     static_assert(std::is_same_v<decltype(saa1), scoped_allocator_adaptor<MyAlloc<short>>>);
//     static_assert(std::is_same_v<decltype(saa2), scoped_allocator_adaptor<MyAlloc<short>, MyAlloc<int>>>);
//     static_assert(std::is_same_v<decltype(saa3), scoped_allocator_adaptor<MyAlloc<short>, MyAlloc<int>, MyAlloc<long>>>);
//     static_assert(std::is_same_v<decltype(saa4), scoped_allocator_adaptor<MyAlloc<short>, MyAlloc<int>, MyAlloc<long>>>);
// }

void test_reference_wrapper() {
    long x = 11L;
    std::reference_wrapper rw1(x);
    std::reference_wrapper rw2(rw1);

    static_assert(std::is_same_v<decltype(rw1), std::reference_wrapper<long>>);
    static_assert(std::is_same_v<decltype(rw2), std::reference_wrapper<long>>);
}

void test_transparent_operator_functors() {
    std::plus op1{};
    std::minus op2{};
    std::multiplies op3{};
    std::divides op4{};
    std::modulus op5{};
    std::negate op6{};
    std::equal_to op7{};
    std::not_equal_to op8{};
    std::greater op9{};
    std::less op10{};
    std::greater_equal op11{};
    std::less_equal op12{};
    std::logical_and op13{};
    std::logical_or op14{};
    std::logical_not op15{};
    std::bit_and op16{};
    std::bit_or op17{};
    std::bit_xor op18{};
    std::bit_not op19{};

    static_assert(std::is_same_v<decltype(op1), std::plus<>>);
    static_assert(std::is_same_v<decltype(op2), std::minus<>>);
    static_assert(std::is_same_v<decltype(op3), std::multiplies<>>);
    static_assert(std::is_same_v<decltype(op4), std::divides<>>);
    static_assert(std::is_same_v<decltype(op5), std::modulus<>>);
    static_assert(std::is_same_v<decltype(op6), std::negate<>>);
    static_assert(std::is_same_v<decltype(op7), std::equal_to<>>);
    static_assert(std::is_same_v<decltype(op8), std::not_equal_to<>>);
    static_assert(std::is_same_v<decltype(op9), std::greater<>>);
    static_assert(std::is_same_v<decltype(op10), std::less<>>);
    static_assert(std::is_same_v<decltype(op11), std::greater_equal<>>);
    static_assert(std::is_same_v<decltype(op12), std::less_equal<>>);
    static_assert(std::is_same_v<decltype(op13), std::logical_and<>>);
    static_assert(std::is_same_v<decltype(op14), std::logical_or<>>);
    static_assert(std::is_same_v<decltype(op15), std::logical_not<>>);
    static_assert(std::is_same_v<decltype(op16), std::bit_and<>>);
    static_assert(std::is_same_v<decltype(op17), std::bit_or<>>);
    static_assert(std::is_same_v<decltype(op18), std::bit_xor<>>);
    static_assert(std::is_same_v<decltype(op19), std::bit_not<>>);

    static_assert(std::is_same_v<decltype(std::greater()), std::greater<>>);
    static_assert(std::is_same_v<decltype(std::greater{}), std::greater<>>);
}

template <template <typename> typename F>
void test_function_wrapper() {
    F<short(int, long)> f1{};

    if constexpr (std::is_copy_constructible_v<F<short(int, long)>>) {
        F f2copy(f1);
        static_assert(std::is_same_v<decltype(f2copy), F<short(int, long)>>);
    }

    F f2(std::move(f1));
    static_assert(std::is_same_v<decltype(f2), F<short(int, long)>>);

    F f3(nothing);
    F f4(&nothing);
    F f5(square);
    F f6(&square);
    F f7(add);
    F f8(&add);

    static_assert(std::is_same_v<decltype(f3), F<void()>>);
    static_assert(std::is_same_v<decltype(f4), F<void()>>);
    static_assert(std::is_same_v<decltype(f5), F<int(int)>>);
    static_assert(std::is_same_v<decltype(f6), F<int(int)>>);
    static_assert(std::is_same_v<decltype(f7), F<long(short, int)>>);
    static_assert(std::is_same_v<decltype(f8), F<long(short, int)>>);

    int n      = 0;
    auto accum = [&n](int x, int y) { return n += x + y; };

    F f9(std::plus<double>{});
    F f10(accum);

    static_assert(std::is_same_v<decltype(f9), F<double(const double&, const double&)>>);
    static_assert(std::is_same_v<decltype(f10), F<int(int, int)>>);

#ifdef HAS_EXPLICIT_THIS_PARAMETER
    struct ExplicitThisByVal {
        void operator()(this ExplicitThisByVal, char) {}
    };

    ExplicitThisByVal explicit_this_by_val_functor{};

    F f11(explicit_this_by_val_functor);
    F f12(std::as_const(explicit_this_by_val_functor));
    F f13(std::move(explicit_this_by_val_functor));
    F f14(std::move(std::as_const(explicit_this_by_val_functor)));

    static_assert(std::is_same_v<decltype(f11), F<void(char)>>);
    static_assert(std::is_same_v<decltype(f12), F<void(char)>>);
    static_assert(std::is_same_v<decltype(f13), F<void(char)>>);
    static_assert(std::is_same_v<decltype(f14), F<void(char)>>);

    struct ExplicitThisByRef {
        void operator()(this ExplicitThisByRef&, short) {}
    };

    ExplicitThisByRef explicit_this_by_ref_functor{};

    F f15(explicit_this_by_ref_functor);

    static_assert(std::is_same_v<decltype(f15), F<void(short)>>);

    struct ExplicitThisByCRef {
        void operator()(this const ExplicitThisByCRef&, int) {}
    };

    ExplicitThisByCRef explicit_this_by_cref_functor{};

    F f16(explicit_this_by_cref_functor);
    F f17(std::as_const(explicit_this_by_cref_functor));
    F f18(std::move(explicit_this_by_cref_functor));
    F f19(std::move(std::as_const(explicit_this_by_cref_functor)));

    static_assert(std::is_same_v<decltype(f16), F<void(int)>>);
    static_assert(std::is_same_v<decltype(f17), F<void(int)>>);
    static_assert(std::is_same_v<decltype(f18), F<void(int)>>);
    static_assert(std::is_same_v<decltype(f19), F<void(int)>>);

    struct ExplicitThisByConv {
        struct That {};

        operator That(this ExplicitThisByConv) {
            return {};
        }

        void operator()(this That, long long) {}
    };

    ExplicitThisByConv explicit_this_by_conv_functor{};

    F f20(explicit_this_by_conv_functor);
    F f21(std::as_const(explicit_this_by_conv_functor));
    F f22(std::move(explicit_this_by_conv_functor));
    F f23(std::move(std::as_const(explicit_this_by_conv_functor)));

    static_assert(std::is_same_v<decltype(f20), F<void(long long)>>);
    static_assert(std::is_same_v<decltype(f21), F<void(long long)>>);
    static_assert(std::is_same_v<decltype(f22), F<void(long long)>>);
    static_assert(std::is_same_v<decltype(f23), F<void(long long)>>);

    struct ExplicitThisNoexcept {
        float operator()(this ExplicitThisNoexcept, double) noexcept {
            return 3.14f;
        }
    };

    ExplicitThisNoexcept explicit_this_noexcept_functor{};

    F f24(explicit_this_noexcept_functor);
    F f25(std::as_const(explicit_this_noexcept_functor));
    F f26(std::move(explicit_this_noexcept_functor));
    F f27(std::move(std::as_const(explicit_this_noexcept_functor)));

    static_assert(std::is_same_v<decltype(f24), F<float(double)>>);
    static_assert(std::is_same_v<decltype(f25), F<float(double)>>);
    static_assert(std::is_same_v<decltype(f26), F<float(double)>>);
    static_assert(std::is_same_v<decltype(f27), F<float(double)>>);
#endif // HAS_EXPLICIT_THIS_PARAMETER
}

void test_searchers() {
    const wchar_t first[] = {L'x', L'y', L'z'};
    const auto last       = std::end(first);
    MyWideHash wh{};
    MyEqual eq{};

    std::default_searcher ds1(first, last);
    std::default_searcher ds2(first, last, eq);
    std::boyer_moore_searcher bms1(first, last);
    std::boyer_moore_searcher bms2(first, last, wh);
    std::boyer_moore_searcher bms3(first, last, wh, eq);
    std::boyer_moore_horspool_searcher bmhs1(first, last);
    std::boyer_moore_horspool_searcher bmhs2(first, last, wh);
    std::boyer_moore_horspool_searcher bmhs3(first, last, wh, eq);

    static_assert(std::is_same_v<decltype(ds1), std::default_searcher<const wchar_t*>>);
    static_assert(std::is_same_v<decltype(ds2), std::default_searcher<const wchar_t*, MyEqual>>);
    static_assert(std::is_same_v<decltype(bms1), std::boyer_moore_searcher<const wchar_t*>>);
    static_assert(std::is_same_v<decltype(bms2), std::boyer_moore_searcher<const wchar_t*, MyWideHash>>);
    static_assert(std::is_same_v<decltype(bms3), std::boyer_moore_searcher<const wchar_t*, MyWideHash, MyEqual>>);
    static_assert(std::is_same_v<decltype(bmhs1), std::boyer_moore_horspool_searcher<const wchar_t*>>);
    static_assert(std::is_same_v<decltype(bmhs2), std::boyer_moore_horspool_searcher<const wchar_t*, MyWideHash>>);
    static_assert(std::is_same_v<decltype(bmhs3), std::boyer_moore_horspool_searcher<const wchar_t*, MyWideHash, MyEqual>>);
}

void test_duration_and_time_point() {
    using namespace std::chrono;

    std::chrono::duration dur1(11ns);
    std::chrono::duration dur2(22h);

    static_assert(std::is_same_v<decltype(dur1), std::chrono::nanoseconds>);
    static_assert(std::is_same_v<decltype(dur2), std::chrono::hours>);

    std::chrono::time_point<std::chrono::system_clock, std::chrono::hours> tp{};
    std::chrono::time_point tp2(tp);
    (void) tp2;

    static_assert(std::is_same_v<decltype(tp2), std::chrono::time_point<std::chrono::system_clock, std::chrono::hours>>);
}

void test_basic_string() {
    using namespace std::literals::string_view_literals;
    const wchar_t first[] = {L'x', L'y', L'z'};
    const auto last       = std::end(first);
    MyAlloc<wchar_t> myal{};
    std::initializer_list<wchar_t> il = {L'x', L'y', L'z'};

    std::basic_string str1(first, last);
    std::basic_string str2(first, last, myal);
    std::basic_string str3(str2);
    std::basic_string str4(str2, 1);
    std::basic_string str5(str2, 1, myal);
    std::basic_string str6(str2, 1, 1);
    std::basic_string str7(str2, 1, 1, myal);
    std::basic_string str8(L"kitten"sv);
    std::basic_string str9(L"kitten"sv, myal);
    std::basic_string str9a(L"kitten"sv, 1, 2);
    std::basic_string str9b(L"kitten"sv, 1, 2, myal);
    std::basic_string str10(L"meow");
    std::basic_string str11(L"meow", myal);
    std::basic_string str12(L"meow", 1);
    std::basic_string str13(L"meow", 1, myal);
    std::basic_string str14(7, L'x');
    std::basic_string str15(7, L'x', myal);
    std::basic_string str16(il);
    std::basic_string str17(il, myal);
    std::basic_string str18(str2, myal);

    static_assert(std::is_same_v<decltype(str1), std::wstring>);
    static_assert(std::is_same_v<decltype(str2), std::basic_string<wchar_t, std::char_traits<wchar_t>, MyAlloc<wchar_t>>>);
    static_assert(std::is_same_v<decltype(str3), std::basic_string<wchar_t, std::char_traits<wchar_t>, MyAlloc<wchar_t>>>);
    static_assert(std::is_same_v<decltype(str4), std::basic_string<wchar_t, std::char_traits<wchar_t>, MyAlloc<wchar_t>>>);
    static_assert(std::is_same_v<decltype(str5), std::basic_string<wchar_t, std::char_traits<wchar_t>, MyAlloc<wchar_t>>>);
    static_assert(std::is_same_v<decltype(str6), std::basic_string<wchar_t, std::char_traits<wchar_t>, MyAlloc<wchar_t>>>);
    static_assert(std::is_same_v<decltype(str7), std::basic_string<wchar_t, std::char_traits<wchar_t>, MyAlloc<wchar_t>>>);
    static_assert(std::is_same_v<decltype(str8), std::wstring>);
    static_assert(std::is_same_v<decltype(str9), std::basic_string<wchar_t, std::char_traits<wchar_t>, MyAlloc<wchar_t>>>);
    static_assert(std::is_same_v<decltype(str9a), std::wstring>);
    static_assert(std::is_same_v<decltype(str9b), std::basic_string<wchar_t, std::char_traits<wchar_t>, MyAlloc<wchar_t>>>);
    static_assert(std::is_same_v<decltype(str10), std::wstring>);
    static_assert(std::is_same_v<decltype(str11), std::basic_string<wchar_t, std::char_traits<wchar_t>, MyAlloc<wchar_t>>>);
    static_assert(std::is_same_v<decltype(str12), std::wstring>);
    static_assert(std::is_same_v<decltype(str13), std::basic_string<wchar_t, std::char_traits<wchar_t>, MyAlloc<wchar_t>>>);
    static_assert(std::is_same_v<decltype(str14), std::wstring>);
    static_assert(std::is_same_v<decltype(str15), std::basic_string<wchar_t, std::char_traits<wchar_t>, MyAlloc<wchar_t>>>);
    static_assert(std::is_same_v<decltype(str16), std::wstring>);
    static_assert(std::is_same_v<decltype(str17), std::basic_string<wchar_t, std::char_traits<wchar_t>, MyAlloc<wchar_t>>>);
    static_assert(std::is_same_v<decltype(str18), std::basic_string<wchar_t, std::char_traits<wchar_t>, MyAlloc<wchar_t>>>);

#if _HAS_CXX23
    std::basic_string str19(from_range, first);
    std::basic_string str20(from_range, first, myal);

    static_assert(std::is_same_v<decltype(str19), std::wstring>);
    static_assert(std::is_same_v<decltype(str20), std::basic_string<wchar_t, std::char_traits<wchar_t>, MyAlloc<wchar_t>>>);
#endif // _HAS_CXX23
}

void test_basic_string_view() {
    std::basic_string_view sv1(L"meow");
    std::basic_string_view sv2(L"meow", 1);
    std::basic_string_view sv3(sv2);

    static_assert(std::is_same_v<decltype(sv1), std::wstring_view>);
    static_assert(std::is_same_v<decltype(sv2), std::wstring_view>);
    static_assert(std::is_same_v<decltype(sv3), std::wstring_view>);
}

void test_array() {
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-braces"
#endif // __clang__

    long x       = 11L;
    const long y = 22L;

    std::array a1{x};
    std::array a2{x, y};
    std::array a3{x, y, 33L};
    std::array b{a3};

    static_assert(std::is_same_v<decltype(a1), std::array<long, 1>>);
    static_assert(std::is_same_v<decltype(a2), std::array<long, 2>>);
    static_assert(std::is_same_v<decltype(a3), std::array<long, 3>>);
    static_assert(std::is_same_v<decltype(b), std::array<long, 3>>);

#ifdef __clang__
#pragma clang diagnostic pop
#endif // __clang__
}

template <template <typename T, typename A = std::allocator<T>> typename Sequence>
void test_sequence_container() {
    const long first[] = {10L, 20L, 30L};
    const auto last    = std::end(first);
    MyAlloc<long> myal{};
    std::initializer_list<long> il = {11L, 22L, 33L};

    Sequence c1(first, last);
    Sequence c2(first, last, myal);
    Sequence c3(il);
    Sequence c4(il, myal);
    Sequence c5(7, 44L);
    Sequence c6(7, 44L, myal);
    Sequence c7(c6);

    static_assert(std::is_same_v<decltype(c1), Sequence<long>>);
    static_assert(std::is_same_v<decltype(c2), Sequence<long, MyAlloc<long>>>);
    static_assert(std::is_same_v<decltype(c3), Sequence<long>>);
    static_assert(std::is_same_v<decltype(c4), Sequence<long, MyAlloc<long>>>);
    static_assert(std::is_same_v<decltype(c5), Sequence<long>>);
    static_assert(std::is_same_v<decltype(c6), Sequence<long, MyAlloc<long>>>);
    static_assert(std::is_same_v<decltype(c7), Sequence<long, MyAlloc<long>>>);

#if _HAS_CXX23
    Sequence c8(from_range, first);
    Sequence c9(from_range, first, myal);

    static_assert(std::is_same_v<decltype(c8), Sequence<long>>);
    static_assert(std::is_same_v<decltype(c9), Sequence<long, MyAlloc<long>>>);
#endif // _HAS_CXX23
}

void test_vector_bool() {
    const bool first[] = {true, false, true};
    const auto last    = std::end(first);
    MyAlloc<bool> myal{};
    std::initializer_list<bool> il = {true, false, true};

    std::vector vb1(first, last);
    std::vector vb2(first, last, myal);
    std::vector vb3(il);
    std::vector vb4(il, myal);
    std::vector vb5(7, true);
    std::vector vb6(7, true, myal);
    std::vector vb7(vb6);

    static_assert(std::is_same_v<decltype(vb1), std::vector<bool>>);
    static_assert(std::is_same_v<decltype(vb2), std::vector<bool, MyAlloc<bool>>>);
    static_assert(std::is_same_v<decltype(vb3), std::vector<bool>>);
    static_assert(std::is_same_v<decltype(vb4), std::vector<bool, MyAlloc<bool>>>);
    static_assert(std::is_same_v<decltype(vb5), std::vector<bool>>);
    static_assert(std::is_same_v<decltype(vb6), std::vector<bool, MyAlloc<bool>>>);
    static_assert(std::is_same_v<decltype(vb7), std::vector<bool, MyAlloc<bool>>>);

#if _HAS_CXX23
    std::vector vb8(from_range, first);
    std::vector vb9(from_range, first, myal);

    static_assert(std::is_same_v<decltype(vb8), std::vector<bool>>);
    static_assert(std::is_same_v<decltype(vb9), std::vector<bool, MyAlloc<bool>>>);
#endif // _HAS_CXX23
}

// template <template <typename K, typename V, typename C = less<K>, typename A = std::allocator<pair<const K, V>>> typename M>
// void test_map_or_multimap() {
//     using Purr          = std::pair<long, char>;
//     using CPurr         = std::pair<const long, char>;
//     const CPurr first[] = {{10L, 'a'}, {20L, 'b'}, {30L, 'c'}};
//     const auto last     = std::end(first);
//     MyGreater gt{};
//     MyAlloc<CPurr> myal{};
//     std::initializer_list<CPurr> il = {{11L, 'd'}, {22L, 'e'}, {33L, 'f'}};

//     M m1(first, last);
//     M m2(first, last, gt);
//     M m3(first, last, gt, myal);
//     M m4(il);
//     M m5(il, gt);
//     M m7(first, last, myal);
//     M m9(m7);
//     M m10{Purr{11L, 'd'}, Purr{22L, 'e'}, Purr{33L, 'f'}};
//     M m11({Purr{11L, 'd'}, Purr{22L, 'e'}, Purr{33L, 'f'}}, gt);
//     M m12({Purr{11L, 'd'}, Purr{22L, 'e'}, Purr{33L, 'f'}}, gt, myal);
//     M m13({Purr{11L, 'd'}, Purr{22L, 'e'}, Purr{33L, 'f'}}, myal);

//     static_assert(std::is_same_v<decltype(m1), M<long, char>>);
//     static_assert(std::is_same_v<decltype(m2), M<long, char, MyGreater>>);
//     static_assert(std::is_same_v<decltype(m3), M<long, char, MyGreater, MyAlloc<CPurr>>>);
//     static_assert(std::is_same_v<decltype(m4), M<long, char>>);
//     static_assert(std::is_same_v<decltype(m5), M<long, char, MyGreater>>);
//     static_assert(std::is_same_v<decltype(m7), M<long, char, less<long>, MyAlloc<CPurr>>>);
//     static_assert(std::is_same_v<decltype(m9), M<long, char, less<long>, MyAlloc<CPurr>>>);
//     static_assert(std::is_same_v<decltype(m10), M<long, char>>);
//     static_assert(std::is_same_v<decltype(m11), M<long, char, MyGreater>>);
//     static_assert(std::is_same_v<decltype(m12), M<long, char, MyGreater, MyAlloc<CPurr>>>);
//     static_assert(std::is_same_v<decltype(m13), M<long, char, less<long>, MyAlloc<CPurr>>>);

// #if _HAS_CXX23
//     M m14(from_range, first);
//     M m15(from_range, first, gt);
//     M m16(from_range, first, gt, myal);
//     M m17(from_range, first, myal);

//     static_assert(std::is_same_v<decltype(m14), M<long, char>>);
//     static_assert(std::is_same_v<decltype(m15), M<long, char, MyGreater>>);
//     static_assert(std::is_same_v<decltype(m16), M<long, char, MyGreater, MyAlloc<CPurr>>>);
//     static_assert(std::is_same_v<decltype(m17), M<long, char, less<long>, MyAlloc<CPurr>>>);

//     { // Verify changes from P2165R4
//         using TupleIter = tuple<int, double>*;
//         static_assert(std::is_same_v<decltype(M{TupleIter{}, TupleIter{}}), M<int, double>>);

//         using PairIter = pair<int, float>*;
//         static_assert(std::is_same_v<decltype(M{PairIter{}, PairIter{}}), M<int, float>>);

//         using ArrayIter = std::array<int, 2>*;
//         static_assert(std::is_same_v<decltype(M{ArrayIter{}, ArrayIter{}}), M<int, int>>);

//         using SubrangeIter = ranges::subrange<int*, int*>*;
//         static_assert(std::is_same_v<decltype(M{SubrangeIter{}, SubrangeIter{}}), M<int*, int*>>);
//     }
// #endif // _HAS_CXX23
// }

// template <template <typename K, typename C = less<K>, typename A = std::allocator<K>> typename S>
// void test_set_or_multiset() {
//     const long first[] = {10L, 20L, 30L};
//     const auto last    = std::end(first);
//     MyGreater gt{};
//     MyAlloc<long> myal{};
//     std::initializer_list<long> il = {11L, 22L, 33L};

//     S s1(first, last);
//     S s2(first, last, gt);
//     S s3(first, last, gt, myal);
//     S s4(il);
//     S s5(il, gt);
//     S s6(il, gt, myal);
//     S s7(first, last, myal);
//     S s8(il, myal);
//     S s9(s8);

//     static_assert(std::is_same_v<decltype(s1), S<long>>);
//     static_assert(std::is_same_v<decltype(s2), S<long, MyGreater>>);
//     static_assert(std::is_same_v<decltype(s3), S<long, MyGreater, MyAlloc<long>>>);
//     static_assert(std::is_same_v<decltype(s4), S<long>>);
//     static_assert(std::is_same_v<decltype(s5), S<long, MyGreater>>);
//     static_assert(std::is_same_v<decltype(s6), S<long, MyGreater, MyAlloc<long>>>);
//     static_assert(std::is_same_v<decltype(s7), S<long, less<long>, MyAlloc<long>>>);
//     static_assert(std::is_same_v<decltype(s8), S<long, less<long>, MyAlloc<long>>>);
//     static_assert(std::is_same_v<decltype(s9), S<long, less<long>, MyAlloc<long>>>);

// #if _HAS_CXX23
//     S s10(from_range, first);
//     S s11(from_range, first, gt);
//     S s12(from_range, first, gt, myal);
//     S s13(from_range, first, myal);

//     static_assert(std::is_same_v<decltype(s10), S<long>>);
//     static_assert(std::is_same_v<decltype(s11), S<long, MyGreater>>);
//     static_assert(std::is_same_v<decltype(s12), S<long, MyGreater, MyAlloc<long>>>);
//     static_assert(std::is_same_v<decltype(s13), S<long, less<long>, MyAlloc<long>>>);
// #endif // _HAS_CXX23
// }

template <template <typename K, typename V, typename H = std::hash<K>, typename P = std::equal_to<K>,
    typename A = std::allocator<std::pair<const K, V>>> typename UM>
void test_unordered_map_or_unordered_multimap() {
    using Purr          = std::pair<long, char>;
    using CPurr         = std::pair<const long, char>;
    const CPurr first[] = {{10L, 'a'}, {20L, 'b'}, {30L, 'c'}};
    const auto last     = std::end(first);
    MyHash hf{};
    MyEqual eq{};
    MyAlloc<CPurr> myal{};
    std::initializer_list<CPurr> il = {{11L, 'd'}, {22L, 'e'}, {33L, 'f'}};

    UM um1(first, last);
    UM um2(first, last, 7);
    UM um3(first, last, 7, hf);
    UM um4(first, last, 7, hf, eq);
    UM um5(first, last, 7, hf, eq, myal);
    UM um6(il);
    UM um11(first, last, myal);
    UM um12(first, last, 7, myal);
    UM um13(first, last, 7, hf, myal);
    UM um17(um5);
    UM um18{Purr{10L, 'a'}, Purr{20L, 'b'}, Purr{30L, 'c'}};
    UM um19({Purr{10L, 'a'}, Purr{20L, 'b'}, Purr{30L, 'c'}}, 7);
    UM um20({Purr{10L, 'a'}, Purr{20L, 'b'}, Purr{30L, 'c'}}, 7, hf);
    UM um21({Purr{10L, 'a'}, Purr{20L, 'b'}, Purr{30L, 'c'}}, 7, hf, eq);
    UM um22({Purr{10L, 'a'}, Purr{20L, 'b'}, Purr{30L, 'c'}}, 7, hf, eq, myal);
    UM um23({Purr{10L, 'a'}, Purr{20L, 'b'}, Purr{30L, 'c'}}, myal);
    UM um24({Purr{10L, 'a'}, Purr{20L, 'b'}, Purr{30L, 'c'}}, 7, myal);
    UM um25({Purr{10L, 'a'}, Purr{20L, 'b'}, Purr{30L, 'c'}}, 7, hf, myal);

    static_assert(std::is_same_v<decltype(um1), UM<long, char>>);
    static_assert(std::is_same_v<decltype(um2), UM<long, char>>);
    static_assert(std::is_same_v<decltype(um3), UM<long, char, MyHash>>);
    static_assert(std::is_same_v<decltype(um4), UM<long, char, MyHash, MyEqual>>);
    static_assert(std::is_same_v<decltype(um5), UM<long, char, MyHash, MyEqual, MyAlloc<CPurr>>>);
    static_assert(std::is_same_v<decltype(um6), UM<long, char>>);
    static_assert(std::is_same_v<decltype(um11), UM<long, char, std::hash<long>, std::equal_to<long>, MyAlloc<CPurr>>>);
    static_assert(std::is_same_v<decltype(um12), UM<long, char, std::hash<long>, std::equal_to<long>, MyAlloc<CPurr>>>);
    static_assert(std::is_same_v<decltype(um13), UM<long, char, MyHash, std::equal_to<long>, MyAlloc<CPurr>>>);
    static_assert(std::is_same_v<decltype(um17), UM<long, char, MyHash, MyEqual, MyAlloc<CPurr>>>);
    static_assert(std::is_same_v<decltype(um18), UM<long, char>>);
    static_assert(std::is_same_v<decltype(um19), UM<long, char>>);
    static_assert(std::is_same_v<decltype(um20), UM<long, char, MyHash>>);
    static_assert(std::is_same_v<decltype(um21), UM<long, char, MyHash, MyEqual>>);
    static_assert(std::is_same_v<decltype(um22), UM<long, char, MyHash, MyEqual, MyAlloc<CPurr>>>);
    static_assert(std::is_same_v<decltype(um23), UM<long, char, std::hash<long>, std::equal_to<long>, MyAlloc<CPurr>>>);
    static_assert(std::is_same_v<decltype(um24), UM<long, char, std::hash<long>, std::equal_to<long>, MyAlloc<CPurr>>>);
    static_assert(std::is_same_v<decltype(um25), UM<long, char, MyHash, std::equal_to<long>, MyAlloc<CPurr>>>);

#if _HAS_CXX23
    UM um26(from_range, first);
    UM um27(from_range, first, 7);
    UM um28(from_range, first, 7, hf);
    UM um29(from_range, first, 7, hf, eq);
    UM um30(from_range, first, 7, hf, eq, myal);
    UM um31(from_range, first, myal);
    UM um32(from_range, first, 7, myal);
    UM um33(from_range, first, 7, hf, myal);

    static_assert(std::is_same_v<decltype(um26), UM<long, char>>);
    static_assert(std::is_same_v<decltype(um27), UM<long, char>>);
    static_assert(std::is_same_v<decltype(um28), UM<long, char, MyHash>>);
    static_assert(std::is_same_v<decltype(um29), UM<long, char, MyHash, MyEqual>>);
    static_assert(std::is_same_v<decltype(um30), UM<long, char, MyHash, MyEqual, MyAlloc<CPurr>>>);
    static_assert(std::is_same_v<decltype(um31), UM<long, char, std::hash<long>, std::equal_to<long>, MyAlloc<CPurr>>>);
    static_assert(std::is_same_v<decltype(um32), UM<long, char, std::hash<long>, std::equal_to<long>, MyAlloc<CPurr>>>);
    static_assert(std::is_same_v<decltype(um33), UM<long, char, MyHash, std::equal_to<long>, MyAlloc<CPurr>>>);
#endif // _HAS_CXX23
}

template <template <typename K, typename H = std::hash<K>, typename P = std::equal_to<K>, typename A = std::allocator<K>> typename US>
void test_unordered_set_or_unordered_multiset() {
    const long first[] = {10L, 20L, 30L};
    const auto last    = std::end(first);
    MyHash hf{};
    MyEqual eq{};
    MyAlloc<long> myal{};
    std::initializer_list<long> il = {11L, 22L, 33L};

    US us1(first, last);
    US us2(first, last, 7);
    US us3(first, last, 7, hf);
    US us4(first, last, 7, hf, eq);
    US us5(first, last, 7, hf, eq, myal);
    US us6(il);
    US us7(il, 7);
    US us8(il, 7, hf);
    US us9(il, 7, hf, eq);
    US us10(il, 7, hf, eq, myal);
    US us11(first, last, 7, myal);
    US us12(first, last, 7, hf, myal);
    US us13(il, 7, myal);
    US us14(il, 7, hf, myal);
    US us15(us5);

    static_assert(std::is_same_v<decltype(us1), US<long>>);
    static_assert(std::is_same_v<decltype(us2), US<long>>);
    static_assert(std::is_same_v<decltype(us3), US<long, MyHash>>);
    static_assert(std::is_same_v<decltype(us4), US<long, MyHash, MyEqual>>);
    static_assert(std::is_same_v<decltype(us5), US<long, MyHash, MyEqual, MyAlloc<long>>>);
    static_assert(std::is_same_v<decltype(us6), US<long>>);
    static_assert(std::is_same_v<decltype(us7), US<long>>);
    static_assert(std::is_same_v<decltype(us8), US<long, MyHash>>);
    static_assert(std::is_same_v<decltype(us9), US<long, MyHash, MyEqual>>);
    static_assert(std::is_same_v<decltype(us10), US<long, MyHash, MyEqual, MyAlloc<long>>>);
    static_assert(std::is_same_v<decltype(us11), US<long, std::hash<long>, std::equal_to<long>, MyAlloc<long>>>);
    static_assert(std::is_same_v<decltype(us12), US<long, MyHash, std::equal_to<long>, MyAlloc<long>>>);
    static_assert(std::is_same_v<decltype(us13), US<long, std::hash<long>, std::equal_to<long>, MyAlloc<long>>>);
    static_assert(std::is_same_v<decltype(us14), US<long, MyHash, std::equal_to<long>, MyAlloc<long>>>);
    static_assert(std::is_same_v<decltype(us15), US<long, MyHash, MyEqual, MyAlloc<long>>>);

#if _HAS_CXX23
    US us16(from_range, first);
    US us17(from_range, first, 7);
    US us18(from_range, first, 7, hf);
    US us19(from_range, first, 7, hf, eq);
    US us20(from_range, first, 7, hf, eq, myal);
    US us21(from_range, first, 7, myal);
    US us22(from_range, first, 7, hf, myal);

    static_assert(std::is_same_v<decltype(us16), US<long>>);
    static_assert(std::is_same_v<decltype(us17), US<long>>);
    static_assert(std::is_same_v<decltype(us18), US<long, MyHash>>);
    static_assert(std::is_same_v<decltype(us19), US<long, MyHash, MyEqual>>);
    static_assert(std::is_same_v<decltype(us20), US<long, MyHash, MyEqual, MyAlloc<long>>>);
    static_assert(std::is_same_v<decltype(us21), US<long, std::hash<long>, std::equal_to<long>, MyAlloc<long>>>);
    static_assert(std::is_same_v<decltype(us22), US<long, MyHash, std::equal_to<long>, MyAlloc<long>>>);
#endif // _HAS_CXX23
}

void test_queue_and_stack() {
    std::list<long, MyAlloc<long>> lst{};
    MyAlloc<long> myal{};

    std::queue q1(lst);
    std::queue q2(lst, myal);
    std::queue q3(q2);

    static_assert(std::is_same_v<decltype(q1), std::queue<long, std::list<long, MyAlloc<long>>>>);
    static_assert(std::is_same_v<decltype(q2), std::queue<long, std::list<long, MyAlloc<long>>>>);
    static_assert(std::is_same_v<decltype(q3), std::queue<long, std::list<long, MyAlloc<long>>>>);

#if _HAS_CXX23
    const long first[] = {10L, 20L, 30L};
    const auto last    = std::end(first);

    std::queue q4(first, last);
    std::queue q5(first, last, myal);

    static_assert(std::is_same_v<decltype(q4), std::queue<long>>);
    static_assert(std::is_same_v<decltype(q5), std::queue<long, std::deque<long, MyAlloc<long>>>>);

    std::queue q6(from_range, first);
    std::queue q7(from_range, first, myal);

    static_assert(std::is_same_v<decltype(q6), std::queue<long>>);
    static_assert(std::is_same_v<decltype(q7), std::queue<long, std::deque<long, MyAlloc<long>>>>);
#endif // _HAS_CXX23

    std::stack s1(lst);
    std::stack s2(lst, myal);
    std::stack s3(s2);

    static_assert(std::is_same_v<decltype(s1), std::stack<long, std::list<long, MyAlloc<long>>>>);
    static_assert(std::is_same_v<decltype(s2), std::stack<long, std::list<long, MyAlloc<long>>>>);
    static_assert(std::is_same_v<decltype(s3), std::stack<long, std::list<long, MyAlloc<long>>>>);

#if _HAS_CXX23
    std::stack s4(first, last);
    std::stack s5(first, last, myal);

    static_assert(std::is_same_v<decltype(s4), std::stack<long>>);
    static_assert(std::is_same_v<decltype(s5), std::stack<long, std::deque<long, MyAlloc<long>>>>);

    std::stack s6(from_range, first);
    std::stack s7(from_range, first, myal);

    static_assert(std::is_same_v<decltype(s6), std::stack<long>>);
    static_assert(std::is_same_v<decltype(s7), std::stack<long, std::deque<long, MyAlloc<long>>>>);
#endif // _HAS_CXX23
}

// void test_priority_queue() {
//     MyGreater gt{};
//     deque<long, MyAlloc<long>> deq{};
//     MyAlloc<long> myal{};
//     const long first[] = {10L, 20L, 30L};
//     const auto last    = std::end(first);

//     priority_queue pq1(gt, deq);
//     priority_queue pq2(gt, deq, myal);
//     priority_queue pq3(first, last);
//     priority_queue pq4(first, last, gt);
//     priority_queue pq5(first, last, gt, deq);
//     priority_queue pq6(first, last, myal);
//     priority_queue pq7(first, last, gt, myal);
//     priority_queue pq8(first, last, gt, deq, myal);
//     priority_queue pq9(pq5);

//     static_assert(std::is_same_v<decltype(pq1), priority_std::queue<long, deque<long, MyAlloc<long>>, MyGreater>>);
//     static_assert(std::is_same_v<decltype(pq2), priority_std::queue<long, deque<long, MyAlloc<long>>, MyGreater>>);
//     static_assert(std::is_same_v<decltype(pq3), priority_std::queue<long>>);
//     static_assert(std::is_same_v<decltype(pq4), priority_std::queue<long, std::vector<long>, MyGreater>>);
//     static_assert(std::is_same_v<decltype(pq5), priority_std::queue<long, deque<long, MyAlloc<long>>, MyGreater>>);
//     static_assert(std::is_same_v<decltype(pq6), priority_std::queue<long, std::vector<long, MyAlloc<long>>>>);
//     static_assert(std::is_same_v<decltype(pq7), priority_std::queue<long, std::vector<long, MyAlloc<long>>, MyGreater>>);
//     static_assert(std::is_same_v<decltype(pq8), priority_std::queue<long, deque<long, MyAlloc<long>>, MyGreater>>);
//     static_assert(std::is_same_v<decltype(pq9), priority_std::queue<long, deque<long, MyAlloc<long>>, MyGreater>>);

// #if _HAS_CXX23
//     priority_queue pq10(from_range, first);
//     priority_queue pq11(from_range, first, gt);
//     priority_queue pq12(from_range, first, gt, myal);
//     priority_queue pq13(from_range, first, myal);

//     static_assert(std::is_same_v<decltype(pq10), priority_std::queue<long>>);
//     static_assert(std::is_same_v<decltype(pq11), priority_std::queue<long, std::vector<long>, MyGreater>>);
//     static_assert(std::is_same_v<decltype(pq12), priority_std::queue<long, std::vector<long, MyAlloc<long>>, MyGreater>>);
//     static_assert(std::is_same_v<decltype(pq13), priority_std::queue<long, std::vector<long, MyAlloc<long>>>>);
// #endif // _HAS_CXX23
// }

// void test_iterator_adaptors() {
//     long* const ptr = nullptr;

//     reverse_iterator ri1(ptr);
//     reverse_iterator ri2(ri1);

//     static_assert(std::is_same_v<decltype(ri1), reverse_iterator<long*>>);
//     static_assert(std::is_same_v<decltype(ri2), reverse_iterator<long*>>);

//     move_iterator mi1(ptr);
//     move_iterator mi2(mi1);

//     static_assert(std::is_same_v<decltype(mi1), move_iterator<long*>>);
//     static_assert(std::is_same_v<decltype(mi2), move_iterator<long*>>);
// }

// void test_insert_iterators() {
//     std::list<long, MyAlloc<long>> lst{};

//     back_insert_iterator bii1(lst);
//     back_insert_iterator bii2(bii1);

//     static_assert(std::is_same_v<decltype(bii1), back_insert_iterator<std::list<long, MyAlloc<long>>>>);
//     static_assert(std::is_same_v<decltype(bii2), back_insert_iterator<std::list<long, MyAlloc<long>>>>);

//     front_insert_iterator fii1(lst);
//     front_insert_iterator fii2(fii1);

//     static_assert(std::is_same_v<decltype(fii1), front_insert_iterator<std::list<long, MyAlloc<long>>>>);
//     static_assert(std::is_same_v<decltype(fii2), front_insert_iterator<std::list<long, MyAlloc<long>>>>);

//     insert_iterator ii1(lst, lst.std::end());
//     insert_iterator ii2(ii1);

//     static_assert(std::is_same_v<decltype(ii1), insert_iterator<std::list<long, MyAlloc<long>>>>);
//     static_assert(std::is_same_v<decltype(ii2), insert_iterator<std::list<long, MyAlloc<long>>>>);
// }

// void test_stream_iterators() {
//     wostringstream woss{};

//     istream_iterator<double, wchar_t> istr_iter{};
//     istream_iterator istr_iter2(istr_iter);

//     static_assert(std::is_same_v<decltype(istr_iter2), istream_iterator<double, wchar_t>>);

//     ostream_iterator<double, wchar_t> ostr_iter(woss);
//     ostream_iterator ostr_iter2(ostr_iter);

//     static_assert(std::is_same_v<decltype(ostr_iter2), ostream_iterator<double, wchar_t>>);

//     istreambuf_iterator<wchar_t> istrbuf_iter{};
//     istreambuf_iterator istrbuf_iter2(istrbuf_iter);

//     static_assert(std::is_same_v<decltype(istrbuf_iter2), istreambuf_iterator<wchar_t>>);

//     ostreambuf_iterator<wchar_t> ostrbuf_iter(woss);
//     ostreambuf_iterator ostrbuf_iter2(ostrbuf_iter);

//     static_assert(std::is_same_v<decltype(ostrbuf_iter2), ostreambuf_iterator<wchar_t>>);
// }

// void test_complex() {
//     complex c1(1.0f);
//     complex c2(1.0f, 1.0f);
//     complex c3(c2);
//     complex c4(1.0);
//     complex c5(1.0, 1.0);
//     complex c6(c5);

//     static_assert(std::is_same_v<decltype(c1), complex<float>>);
//     static_assert(std::is_same_v<decltype(c2), complex<float>>);
//     static_assert(std::is_same_v<decltype(c3), complex<float>>);
//     static_assert(std::is_same_v<decltype(c4), complex<double>>);
//     static_assert(std::is_same_v<decltype(c5), complex<double>>);
//     static_assert(std::is_same_v<decltype(c6), complex<double>>);
// }

// void test_valstd::array() {
//     int arr1[]                = {10, 20, 30};
//     const long arr2[]         = {11L, 22L, 33L};
//     std::initializer_list<long> il = {11L, 22L, 33L};

//     valstd::array va1(arr1, 3);
//     valstd::array va2(arr2, 3);
//     valstd::array va3(44L, 3);
//     valstd::array va4(begin(arr2), 3);
//     valstd::array va5(va4);
//     valstd::array va6(il);

//     static_assert(std::is_same_v<decltype(va1), valstd::array<int>>);
//     static_assert(std::is_same_v<decltype(va2), valstd::array<long>>);
//     static_assert(std::is_same_v<decltype(va3), valstd::array<long>>);
//     static_assert(std::is_same_v<decltype(va4), valstd::array<long>>);
//     static_assert(std::is_same_v<decltype(va5), valstd::array<long>>);
//     static_assert(std::is_same_v<decltype(va6), valstd::array<long>>);
// }

// void test_regex() {
//     const wchar_t first[]        = {L'x', L'y', L'z'};
//     const auto last              = std::end(first);
//     std::initializer_list<wchar_t> il = {L'x', L'y', L'z'};

//     basic_regex r1(first, last);
//     basic_regex r2(first, last, regex_constants::icase);
//     basic_regex r3(L"meow");
//     basic_regex r4(L"meow", regex_constants::icase);
//     basic_regex r5(L"meow", 1);
//     basic_regex r6(L"meow", 1, regex_constants::icase);
//     basic_regex r7(r6);
//     basic_regex r8(L"kitty"s);
//     basic_regex r9(L"kitty"s, regex_constants::icase);
//     basic_regex r10(il);
//     basic_regex r11(il, regex_constants::icase);

//     static_assert(std::is_same_v<decltype(r1), wregex>);
//     static_assert(std::is_same_v<decltype(r2), wregex>);
//     static_assert(std::is_same_v<decltype(r3), wregex>);
//     static_assert(std::is_same_v<decltype(r4), wregex>);
//     static_assert(std::is_same_v<decltype(r5), wregex>);
//     static_assert(std::is_same_v<decltype(r6), wregex>);
//     static_assert(std::is_same_v<decltype(r7), wregex>);
//     static_assert(std::is_same_v<decltype(r8), wregex>);
//     static_assert(std::is_same_v<decltype(r9), wregex>);
//     static_assert(std::is_same_v<decltype(r10), wregex>);
//     static_assert(std::is_same_v<decltype(r11), wregex>);

//     wssub_match sm1{};
//     sub_match sm2(sm1);

//     static_assert(std::is_same_v<decltype(sm2), wssub_match>);

//     wsmatch mr1{};
//     match_results mr2(mr1);

//     static_assert(std::is_same_v<decltype(mr2), wsmatch>);

//     regex_iterator ri1(first, last, r1);
//     regex_iterator ri2(first, last, r1, regex_constants::match_default);
//     regex_iterator ri3(ri2);

//     static_assert(std::is_same_v<decltype(ri1), wcregex_iterator>);
//     static_assert(std::is_same_v<decltype(ri2), wcregex_iterator>);
//     static_assert(std::is_same_v<decltype(ri3), wcregex_iterator>);

//     const std::vector<int> vec_submatches({0});
//     std::initializer_list<int> il_submatches = {0};
//     const int arr_submatches[]          = {0};

//     regex_token_iterator rti1(first, last, r1);
//     regex_token_iterator rti2(first, last, r1, 0);
//     regex_token_iterator rti3(first, last, r1, 0, regex_constants::match_default);
//     regex_token_iterator rti4(first, last, r1, vec_submatches);
//     regex_token_iterator rti5(first, last, r1, vec_submatches, regex_constants::match_default);
//     regex_token_iterator rti6(first, last, r1, il_submatches);
//     regex_token_iterator rti7(first, last, r1, il_submatches, regex_constants::match_default);
//     regex_token_iterator rti8(first, last, r1, arr_submatches);
//     regex_token_iterator rti9(first, last, r1, arr_submatches, regex_constants::match_default);
//     regex_token_iterator rti10(rti9);

//     static_assert(std::is_same_v<decltype(rti1), wcregex_token_iterator>);
//     static_assert(std::is_same_v<decltype(rti2), wcregex_token_iterator>);
//     static_assert(std::is_same_v<decltype(rti3), wcregex_token_iterator>);
//     static_assert(std::is_same_v<decltype(rti4), wcregex_token_iterator>);
//     static_assert(std::is_same_v<decltype(rti5), wcregex_token_iterator>);
//     static_assert(std::is_same_v<decltype(rti6), wcregex_token_iterator>);
//     static_assert(std::is_same_v<decltype(rti7), wcregex_token_iterator>);
//     static_assert(std::is_same_v<decltype(rti8), wcregex_token_iterator>);
//     static_assert(std::is_same_v<decltype(rti9), wcregex_token_iterator>);
//     static_assert(std::is_same_v<decltype(rti10), wcregex_token_iterator>);
// }

void test_atomic() {
    long x = 11L;

    std::atomic atom1(x);
    std::atomic atom2(&x);

    static_assert(std::is_same_v<decltype(atom1), std::atomic<long>>);
    static_assert(std::is_same_v<decltype(atom2), std::atomic<long*>>);
}

void test_locks() {
    std::recursive_mutex rm{};
    std::recursive_timed_mutex rtm{};
    std::lock_guard lg(rm);
    std::unique_lock ul(rm);
    std::unique_lock ul2(std::move(ul));
    std::scoped_lock scoped0{};
    std::scoped_lock scoped1(rm);
    std::scoped_lock scoped2(rm, rtm);
    std::scoped_lock scoped3(std::adopt_lock);
    rm.lock();
    std::scoped_lock scoped4(std::adopt_lock, rm);
    rm.lock();
    rtm.lock();
    std::scoped_lock scoped5(std::adopt_lock, rm, rtm);
    std::shared_timed_mutex stm{};
    std::shared_lock shared(stm);
    std::shared_lock shared2(std::move(shared));

    static_assert(std::is_same_v<decltype(lg), std::lock_guard<std::recursive_mutex>>);
    static_assert(std::is_same_v<decltype(ul), std::unique_lock<std::recursive_mutex>>);
    static_assert(std::is_same_v<decltype(ul2), std::unique_lock<std::recursive_mutex>>);
    static_assert(std::is_same_v<decltype(scoped0), std::scoped_lock<>>);
    static_assert(std::is_same_v<decltype(scoped1), std::scoped_lock<std::recursive_mutex>>);
    static_assert(std::is_same_v<decltype(scoped2), std::scoped_lock<std::recursive_mutex, std::recursive_timed_mutex>>);
    static_assert(std::is_same_v<decltype(scoped3), std::scoped_lock<>>);
    static_assert(std::is_same_v<decltype(scoped4), std::scoped_lock<std::recursive_mutex>>);
    static_assert(std::is_same_v<decltype(scoped5), std::scoped_lock<std::recursive_mutex, std::recursive_timed_mutex>>);
    static_assert(std::is_same_v<decltype(shared), std::shared_lock<std::shared_timed_mutex>>);
    static_assert(std::is_same_v<decltype(shared2), std::shared_lock<std::shared_timed_mutex>>);
}

int main() {
    // test_initializer_list();
    // test_pair_and_tuple();
    // test_optional();
    // test_bitset();
    test_allocator();
    // test_unique_ptr();
    test_shared_ptr_and_weak_ptr();
    test_owner_less();
    // test_scoped_allocator_adaptor();
    test_reference_wrapper();
    test_transparent_operator_functors();

    test_function_wrapper<std::function>();
    test_function_wrapper<std::packaged_task>();

    test_searchers();
    test_duration_and_time_point();
    test_basic_string();
    test_basic_string_view();
    test_array();

    test_sequence_container<std::deque>();
    test_sequence_container<std::forward_list>();
    test_sequence_container<std::list>();
    test_sequence_container<std::vector>();

    test_vector_bool();

    // test_map_or_multimap<map>();
    // test_map_or_multimap<multimap>();

    // test_set_or_multiset<set>();
    // test_set_or_multiset<multiset>();

    // test_unordered_map_or_unordered_multimap<unordered_map>();
    // test_unordered_map_or_unordered_multimap<unordered_multimap>();

    // test_unordered_set_or_unordered_multiset<unordered_set>();
    // test_unordered_set_or_unordered_multiset<unordered_multiset>();

    test_queue_and_stack();
    // test_priority_queue();
    // test_iterator_adaptors();
    // test_insert_iterators();
    // test_stream_iterators();
    // test_complex();
    // test_valarray();
    // test_regex();
    test_atomic();
    test_locks();
}
