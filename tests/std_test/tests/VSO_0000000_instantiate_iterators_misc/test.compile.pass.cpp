// Copyright (c) Microsoft Corporation.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#define _HAS_DEPRECATED_ADAPTOR_TYPEDEFS     1
#define _HAS_DEPRECATED_ALLOCATOR_MEMBERS    1
#define _HAS_DEPRECATED_NEGATORS             1
#define _HAS_DEPRECATED_RAW_STORAGE_ITERATOR 1
#define _HAS_DEPRECATED_TEMPORARY_BUFFER     1
#define _SILENCE_CXX17_C_HEADER_DEPRECATION_WARNING
#define _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING
#define _SILENCE_CXX17_NEGATORS_DEPRECATION_WARNING
#define _SILENCE_CXX17_OLD_ALLOCATOR_MEMBERS_DEPRECATION_WARNING
#define _SILENCE_CXX17_RAW_STORAGE_ITERATOR_DEPRECATION_WARNING
#define _SILENCE_CXX17_TEMPORARY_BUFFER_DEPRECATION_WARNING
#define _SILENCE_CXX20_ATOMIC_INIT_DEPRECATION_WARNING
#define _SILENCE_CXX20_CODECVT_FACETS_DEPRECATION_WARNING
#define _SILENCE_CXX20_OLD_SHARED_PTR_ATOMIC_SUPPORT_DEPRECATION_WARNING
#define _SILENCE_CXX20_REL_OPS_DEPRECATION_WARNING
#define _SILENCE_CXX20_U8PATH_DEPRECATION_WARNING
#define _SILENCE_CXX20_VOLATILE_DEPRECATION_WARNING
#define _SILENCE_EXPERIMENTAL_FILESYSTEM_DEPRECATION_WARNING
#define _SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING
#define _SILENCE_TR1_NAMESPACE_DEPRECATION_WARNING
#define _USE_NAMED_IDL_NAMESPACE 1

#include <algorithm>
// #include <any> // All templates instantiated in P0220R1_any
#include <array>
#include <cassert>
#include <ccomplex>
#include <cctype>
#include <cerrno>
#include <cfenv>
#include <cfloat>
#include <chrono>
#include <cinttypes>
#include <climits>
#include <clocale>
#include <cmath>
#include <codecvt>
#include <csetjmp>
#include <csignal>
#include <cstdalign>
#include <cstdarg>
#include <cstdbool>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctgmath>
#include <ctime>
#include <cuchar>
#include <cwchar>
#include <cwctype>
#include <deque>
#include <exception>
#include <filesystem>
#include <forward_list>
#include <fstream>
#include <functional>
#include <initializer_list>
#include <iomanip>
#include <ios>
#include <iosfwd>
#include <iostream>
#include <iso646.h>
#include <istream>
#include <iterator>
#include <limits>
#include <list>
#include <locale>
#include <memory>
#include <new>
#include <numeric>
// #include <optional> // All templates instantiated in P0220R1_optional
#include <ostream>
#include <random>
#include <ratio>
#include <regex>
#include <scoped_allocator>
#include <sstream>
#include <stdexcept>
#include <streambuf>
#include <string>
#include <strstream>
#include <system_error>
#include <tuple>
#include <typeindex>
#include <typeinfo>
#include <utility>
// #include <variant> // All templates instantiated in P0088R3_variant
#include <vector>

// Headers not allowed with /clr:pure
#ifndef _M_CEE_PURE
#include <atomic>
#include <condition_variable>
#include <future>
#include <mutex>
#include <shared_mutex>
#include <thread>
#endif // _M_CEE_PURE

#include <experimental/filesystem>

#include <instantiate_containers_iterators_common.hpp>

#define STATIC_ASSERT(...) static_assert(__VA_ARGS__, #__VA_ARGS__)

#ifndef _M_CEE_PURE

#if _HAS_CXX20
STATIC_ASSERT(memory_order_relaxed == memory_order::relaxed);
STATIC_ASSERT(memory_order_consume == memory_order::consume);
STATIC_ASSERT(memory_order_acquire == memory_order::acquire);
STATIC_ASSERT(memory_order_release == memory_order::release);
STATIC_ASSERT(memory_order_acq_rel == memory_order::acq_rel);
STATIC_ASSERT(std::memory_order_seq_cst == memory_order::seq_cst);

// LWG-3268
STATIC_ASSERT(memory_order::memory_order_relaxed == memory_order::relaxed);
STATIC_ASSERT(memory_order::memory_order_consume == memory_order::consume);
STATIC_ASSERT(memory_order::memory_order_acquire == memory_order::acquire);
STATIC_ASSERT(memory_order::memory_order_release == memory_order::release);
STATIC_ASSERT(memory_order::memory_order_acq_rel == memory_order::acq_rel);
STATIC_ASSERT(memory_order::std::memory_order_seq_cst == memory_order::seq_cst);
#endif // _HAS_CXX20

template <typename AtomicType>
void atomic_read_test_impl(AtomicType& value) {
    (void) atomic_is_lock_free(&value);
    (void) atomic_load(&value);
    (void) atomic_load_explicit(&value, std::memory_order_seq_cst);
}

template <typename AtomicType, typename ValueType>
void atomic_has_arithmetic_ops_test_impl(AtomicType& value, ValueType element, std::true_type) {
    std::atomic_fetch_add(&value, element);
    std::atomic_fetch_add_explicit(&value, element, std::memory_order_seq_cst);
    std::atomic_fetch_sub(&value, element);
    std::atomic_fetch_sub_explicit(&value, element, std::memory_order_seq_cst);
    std::atomic_fetch_and(&value, element);
    std::atomic_fetch_and_explicit(&value, element, std::memory_order_seq_cst);
    std::atomic_fetch_or(&value, element);
    std::atomic_fetch_or_explicit(&value, element, std::memory_order_seq_cst);
    std::atomic_fetch_xor(&value, element);
    std::atomic_fetch_xor_explicit(&value, element, std::memory_order_seq_cst);
}

template <typename AtomicType, typename ValueType>
void atomic_has_arithmetic_ops_test_impl(AtomicType&, ValueType, std::false_type) {}

template <typename AtomicType, typename ValueType>
void atomic_write_test_impl(AtomicType& value, ValueType element) {
    ValueType* ptr_element = &element;

    std::atomic_init(&value, element);
    std::atomic_store(&value, element);
    std::atomic_store_explicit(&value, element, std::memory_order_seq_cst);
    std::atomic_exchange(&value, element);
    std::atomic_exchange_explicit(&value, element, std::memory_order_seq_cst);
    std::atomic_compare_exchange_weak(&value, ptr_element, element);
    std::atomic_compare_exchange_weak_explicit(&value, ptr_element, element, std::memory_order_seq_cst, std::memory_order_seq_cst);
    std::atomic_compare_exchange_strong(&value, ptr_element, element);
    std::atomic_compare_exchange_strong_explicit(&value, ptr_element, element, std::memory_order_seq_cst, std::memory_order_seq_cst);

    atomic_has_arithmetic_ops_test_impl(value, element, std::negation<std::is_same<ValueType, bool>>());
}

template <typename T>
void atomic_test_impl(T element) {
    std::atomic<T> value{element};
    volatile std::atomic<T> v_value{element};
    const std::atomic<T> c_value{element};
    const volatile std::atomic<T> cv_value{element};

    atomic_read_test_impl(value);
    atomic_read_test_impl(v_value);
    atomic_read_test_impl(c_value);
    atomic_read_test_impl(cv_value);

    atomic_write_test_impl(value, element);
    atomic_write_test_impl(v_value, element);
}

template <typename T>
void atomic_test_impl() {
    T element{};
    atomic_test_impl(element);
}

void atomic_test() {
    int a = 0;
    std::kill_dependency(a);

    uint8_t one_byte = 0;
    STATIC_ASSERT(sizeof(one_byte) == 1);

    uint16_t two_bytes = 1;
    STATIC_ASSERT(sizeof(two_bytes) == 2);

    uint32_t four_bytes = 2;
    STATIC_ASSERT(sizeof(four_bytes) == 4);

    uint64_t eight_bytes = 3;
    STATIC_ASSERT(sizeof(eight_bytes) == 8);

    atomic_test_impl(one_byte);
    atomic_test_impl(two_bytes);
    atomic_test_impl(four_bytes);
    atomic_test_impl(eight_bytes);

    atomic_test_impl<bool>();
    atomic_test_impl<char>();
    atomic_test_impl<signed char>();
    atomic_test_impl<unsigned char>();
#ifdef __cpp_char8_t
    atomic_test_impl<char8_t>();
#endif // __cpp_char8_t
    atomic_test_impl<char16_t>();
    atomic_test_impl<char32_t>();
    atomic_test_impl<wchar_t>();
    atomic_test_impl<short>();
    atomic_test_impl<unsigned short>();
    atomic_test_impl<int>();
    atomic_test_impl<unsigned int>();
    atomic_test_impl<long>();
    atomic_test_impl<unsigned long>();
    atomic_test_impl<long long>();
    atomic_test_impl<unsigned long long>();
}
#endif // _M_CEE_PURE

void chrono_test() {
    using namespace std::chrono;
    std::chrono::treat_as_floating_point<float> a{};
    std::chrono::duration_values<float> b{};
    std::chrono::duration<float> c{};
    (void) std::chrono::duration_cast<std::chrono::duration<float, std::ratio<2>>>(c);
    std::chrono::duration<double> d(1.0f);
    std::chrono::duration<double> e(c);

    std::chrono::time_point<std::chrono::system_clock> time_pt = std::chrono::system_clock::now();

    INSTANTIATE(std::common_type_t<std::chrono::duration<float>, std::chrono::duration<double>>);
    INSTANTIATE(std::common_type_t<std::chrono::time_point<std::chrono::system_clock>, std::chrono::time_point<std::chrono::system_clock>>);

    (void) a;
    (void) b;
    (void) d;
    (void) e;

    std::chrono::duration<float> dur1{};
    std::chrono::duration<double, std::ratio<2>> dur2{};

    (void) (dur1 + dur2);
    (void) (dur1 - dur2);
    (void) (1.f * dur2);
    (void) (dur1 * 2.0);
    (void) (dur1 / 2.0);
    (void) (dur1 / dur2);

    std::chrono::duration<int> dur3{};
    std::chrono::duration<int, std::ratio<2>> dur4{};

    (void) (dur3 % 2);
    (void) (dur3 % dur4);

    comparable_test(dur1, dur2);

    (void) std::chrono::floor<std::chrono::duration<int>>(dur1);
    (void) ceil<std::chrono::duration<int>>(dur1);

    (void) round<std::chrono::duration<int>>(dur1); // float -> int
    (void) round<std::chrono::duration<int>>(dur3); // int -> int

    (void) abs(dur1);

    (void) (time_pt + dur1);
    (void) (dur1 + time_pt);
    (void) (time_pt - dur1);
    (void) (time_pt - time_pt);
    comparable_test(time_pt);

    (void) (time_point_cast<std::chrono::duration<float>>(time_pt));

    (void) (std::chrono::floor<std::chrono::duration<float>>(time_pt));
    (void) (ceil<std::chrono::duration<float>>(time_pt));
}

#ifndef _M_CEE_PURE
template <typename ConditionVariable>
void condition_variable_test_impl() {
    ConditionVariable cv{};
    std::unique_lock<std::mutex> lock;

    auto pr0  = []() { return true; };
    auto soon = std::chrono::system_clock::now() + std::chrono::minutes(2);

    cv.wait(lock);
    cv.wait(lock, pr0);
    (void) cv.wait_for(lock, std::chrono::seconds(1));
    (void) cv.wait_for(lock, std::chrono::seconds(1), pr0);
    (void) cv.wait_until(lock, soon);
    (void) cv.wait_until(lock, soon, pr0);
}

void condition_variable_test() {
    condition_variable_test_impl<std::condition_variable_any>();
}
#endif // _M_CEE_PURE

void check_nested_exception_impl(const std::exception& ex) { // unroll nested exceptions
    try {
        std::rethrow_if_nested(ex);
    } catch (const std::exception& e) {
        check_nested_exception_impl(e);
    } catch (...) {
    }
}

template <typename ThrowingFunction>
void exception_test_impl(const ThrowingFunction& tf) {
    try {
        try {
            tf();
        } catch (...) {
            std::throw_with_nested("WOOFx2");
        }
    } catch (const std::exception& e) {
        check_nested_exception_impl(e);
    }
}

void exception_test() {
    std::exception e{};
    std::exception_ptr e_ptr = std::make_exception_ptr(e);

    exception_test_impl([]() { throw 23; }); // can't nest
    exception_test_impl([]() { throw std::runtime_error("WOOF"); }); // can nest
}

// template <typename CharType>
// void filesystem_test_impl(const CharType* c_str) {
//     using namespace experimental::filesystem;

//     basic_string<CharType> str = c_str;

//     path p0(str.begin(), str.std::end());
//     path p1(c_str);
//     path p2(str);

//     locale default_locale{};
//     path p3(str.begin(), str.std::end(), default_locale);
//     path p4(c_str, default_locale);
//     path p5(str, default_locale);

//     p0 = c_str;
//     p1 = str;
//     p2.assign(str.begin(), str.std::end());
//     p3.assign(c_str);
//     p4.assign(str);
//     p5 /= c_str;
//     p0 /= str;
//     p0.append(str.begin(), str.std::end());
//     p0.append(c_str);
//     p0 += *c_str;
//     p0 += str;
//     p0 += c_str;
//     p0.concat(str.begin(), str.std::end());
//     p0.concat(str.begin());
//     p0.concat(c_str);
//     p0.concat(*c_str);
//     p0.concat(str);

//     (void) p0.std::string<CharType>(str.get_allocator());
//     (void) p0.generic_string<CharType>(str.get_allocator());

//     std::stringstream ss{};

//     ss << p0;
//     ss >> p0;

//     auto u8str = p0.u8string();
//     (void) u8path(u8str.begin(), u8str.std::end());
//     (void) u8path(u8str.c_str());
// }

// void filesystem_test() {
//     filesystem_test_impl("narrow");
//     filesystem_test_impl(L"wide");
// #ifndef _M_CEE_PURE
// #ifdef __cpp_char8_t
//     filesystem_test_impl(u8"utf8");
// #endif // __cpp_char8_t
//     filesystem_test_impl(u"utf16");
//     filesystem_test_impl(U"utf32");
// #endif // _M_CEE_PURE
// }

template <typename CharType>
void fstream_test_impl() {
    std::basic_filebuf<CharType, std::char_traits<CharType>> fb;
    std::basic_ifstream<CharType, std::char_traits<CharType>> ifs;
    std::basic_ofstream<CharType, std::char_traits<CharType>> ofs;
    std::basic_fstream<CharType, std::char_traits<CharType>> fs;

    swap_test(fb);
    swap_test(ifs);
    swap_test(ofs);
    swap_test(fs);
}

void fstream_test() {
    fstream_test_impl<char>();
    fstream_test_impl<wchar_t>();
}

template <typename ReturnType, typename Function>
void function_test_impl(Function func) {
    std::function<ReturnType()> f0;
    std::function<ReturnType()> f1 = nullptr;
    std::function<ReturnType()> f2 = f0;
    std::function<ReturnType()> f3 = std::move(f0);
    std::function<ReturnType()> f4 = func;
    const auto& cf            = f0;
    f0                        = f1;
    f0                        = std::move(f1);
    f0                        = nullptr;
    f0                        = func;
    f0                        = std::ref(func);
    swap_test(f0);
    (void) !cf;
    (void) cf();
    (void) cf.target_type();
    (void) f0.template target<ReturnType (*)()>();
    (void) cf.template target<ReturnType (*)()>();
    equality_test(f0, nullptr);
    equality_test(nullptr, f0);

#if _HAS_FUNCTION_ALLOCATOR_SUPPORT
    std::function<ReturnType()> f5(std::allocator_arg, std::allocator<double>{});
    std::function<ReturnType()> f6(std::allocator_arg, std::allocator<double>{}, nullptr);
    std::function<ReturnType()> f7(std::allocator_arg, std::allocator<double>{}, f5);
    std::function<ReturnType()> f8(std::allocator_arg, std::allocator<double>{}, std::move(f5));
    std::function<ReturnType()> f9(std::allocator_arg, std::allocator<double>{}, func);
    TRAIT_V(std::uses_allocator, std::function<ReturnType()>, std::allocator<double>);
#endif // _HAS_FUNCTION_ALLOCATOR_SUPPORT
}

void functional_test() {
    using namespace std::placeholders;

    // different return types to trigger <type_traits>::_Invoke_ret
    function_test_impl<void>([]() {});
    function_test_impl<int>([]() { return 4; });
    function_test_impl<std::vector<int>>([]() { return std::vector<int>{}; });

    auto ph                  = std::placeholders::_1;
    const auto cph           = std::placeholders::_2;
    volatile auto vph        = std::placeholders::_3;
    const volatile auto cvph = std::placeholders::_4;

    TRAIT_V(std::is_placeholder, decltype(ph));
    TRAIT_V(std::is_placeholder, decltype(cph));
    TRAIT_V(std::is_placeholder, decltype(vph));
    TRAIT_V(std::is_placeholder, decltype(cvph));

    TRAIT_V(std::is_placeholder, int);
    TRAIT_V(std::is_placeholder, const int);
    TRAIT_V(std::is_placeholder, volatile int);
    TRAIT_V(std::is_placeholder, const volatile int);

    // implicit return type
    auto be                  = std::bind([](int, int) {}, std::placeholders::_1, 2);
    const auto cbe           = be;
    volatile auto vbe        = be;
    const volatile auto cvbe = be;

    auto not_be                  = []() {};
    const auto not_cbe           = not_be;
    volatile auto not_vbe        = not_be;
    const volatile auto not_cvbe = not_be;

    TRAIT_V(std::is_bind_expression, decltype(be));
    TRAIT_V(std::is_bind_expression, decltype(cbe));
    TRAIT_V(std::is_bind_expression, decltype(vbe));
    TRAIT_V(std::is_bind_expression, decltype(cvbe));

    TRAIT_V(std::is_bind_expression, decltype(not_be));
    TRAIT_V(std::is_bind_expression, decltype(not_cbe));
    TRAIT_V(std::is_bind_expression, decltype(not_vbe));
    TRAIT_V(std::is_bind_expression, decltype(not_cvbe));

    be(1);
    cbe(2);
    // volatile binder calls not supported

    // with explicit return type
    auto be_ret                  = std::bind<void>([](int, int) {}, std::placeholders::_1, 2);
    const auto cbe_ret           = be;
    volatile auto vbe_ret        = be;
    const volatile auto cvbe_ret = be;

    TRAIT_V(std::is_bind_expression, decltype(be_ret));
    TRAIT_V(std::is_bind_expression, decltype(cbe_ret));
    TRAIT_V(std::is_bind_expression, decltype(vbe_ret));
    TRAIT_V(std::is_bind_expression, decltype(cvbe_ret));

    be_ret(3);
    cbe_ret(4);
    // volatile binder calls not supported
}

#ifndef _M_CEE_PURE
template <typename Future>
void future_test_impl(Future& f) {
    using namespace std::chrono;

    (void) f.wait_for(seconds(3));
    (void) f.wait_until(std::chrono::system_clock::now());
}

void future_test() {
    auto bp = std::future_errc::broken_promise;
    (void) bp;
    bool iece = std::is_error_code_enum_v<decltype(bp)>;
    (void) iece;

    std::future<int> f{};
    std::future<int&> fr{};
    std::future<void> fv{};

    std::shared_future<int> sf{};
    std::shared_future<int&> sfr{};
    std::shared_future<void> sfv{};

    future_test_impl(f);
    future_test_impl(fr);
    future_test_impl(fv);
    future_test_impl(sf);
    future_test_impl(sfr);
    future_test_impl(sfv);

    std::promise<int> p(std::allocator_arg, std::allocator<double>{});
    std::promise<int&> pr(std::allocator_arg, std::allocator<double>{});
    std::promise<void> pv(std::allocator_arg, std::allocator<double>{});

    swap_test(p);
    swap_test(pr);
    swap_test(pv);

    std::packaged_task<void()> pt([]() {});
    // GH-321: "<future>: packaged_task can't be constructed from a move-only lambda"
    // packaged_task<void()> pt2([]() {});

#if _HAS_FUNCTION_ALLOCATOR_SUPPORT
    std::packaged_task<void()> pta(std::allocator_arg, std::allocator<double>{}, []() {});
    // GH-321: "<future>: packaged_task can't be constructed from a move-only lambda"
    // packaged_task<void()> pta2(std::allocator_arg, std::allocator<double>{}, []() {});
#endif // _HAS_FUNCTION_ALLOCATOR_SUPPORT

    swap_test(pt);

    (void) std::async([]() {});
    (void) std::async(std::launch::async, []() {});

    TRAIT_V(std::uses_allocator, std::promise<int>, std::allocator<double>);

#if _HAS_FUNCTION_ALLOCATOR_SUPPORT
    TRAIT_V(std::uses_allocator, std::packaged_task<void()>, std::allocator<double>);
#endif // _HAS_FUNCTION_ALLOCATOR_SUPPORT
}
#endif // _M_CEE_PURE

template <typename IoManipOut, typename IoManipIn>
void iomanip_test_impl(IoManipOut out, IoManipIn in) {
    std::stringstream ss{};
    ss << out;
    ss >> in;
}

// template <typename IoManip>
// void iomanip_test_impl(IoManip iom) {
//     iomanip_test_impl(iom, iom);
// }

// template <typename IoManip>
// void iomanip_test_impl_for_setfill(IoManip out) {
//     std::stringstream ss{};
//     ss << out;
// }

// void iomanip_test() {
//     auto sf           = setfill('*');
//     long double money = 123.45;
//     time_t t          = time(nullptr);
//     tm time{};
//     localtime_s(&time, &t);
//     std::string str = "std::string with \" quotes ";

//     iomanip_test_impl_for_setfill(sf);
//     iomanip_test_impl(put_money(money), get_money(money));
//     iomanip_test_impl(put_time(&time, "%c %Z"), get_time(&time, "%c %Z"));
//     iomanip_test_impl(quoted(str.c_str()), quoted(str));
//     iomanip_test_impl(quoted(str));
//     iomanip_test_impl(resetiosflags(ios_base::basefield));
//     iomanip_test_impl(setiosflags(ios_base::basefield));
//     iomanip_test_impl(setbase(2));
//     iomanip_test_impl(setprecision(10));
//     iomanip_test_impl(setw(10));
// }

void ios_test() {
    std::stringstream ss{};
    std::basic_ios<char, std::char_traits<char>> b_ios(ss.rdbuf());
}

void iosfwd_test() {
    std::streampos sp; // fpos<std::char_traits<char>::state_type>
    std::wstreampos wsp; // fpos<std::char_traits<wchar_t>::state_type>

    INSTANTIATE(std::char_traits<char>);
    INSTANTIATE(std::char_traits<wchar_t>);
#ifdef __cpp_char8_t
    INSTANTIATE(std::char_traits<char8_t>);
#endif // __cpp_char8_t
    INSTANTIATE(std::char_traits<char16_t>);
    INSTANTIATE(std::char_traits<char32_t>);
    INSTANTIATE(std::char_traits<unsigned short>);
    // Other templates in iosfwd are forward decls.
    // Will instantiate in the test for the header where they are defined.
}

template <typename Container>
void nonmember_reverse_iterator_functions_test() {
    Container c{1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

    (void) std::rbegin(c);
    (void) std::crbegin(c);
    (void) std::rend(c);
    (void) std::crend(c);
}

template <typename Container>
void nonmember_iterator_functions_test() {
    Container c{1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

    (void) std::begin(c);
    (void) std::cbegin(c);
    (void) std::end(c);
    (void) std::cend(c);
}

// void msvc_array_iterators_test() {
//     using namespace stdext;
//     int arr[] = {1, 2, 3, 4};

//     auto unchecked = make_unchecked_array_iterator(arr);
//     auto checked   = make_checked_array_iterator(arr, size(arr));

//     random_access_iterator_test(unchecked);
//     random_access_iterator_test(checked);

//     swap_test(unchecked);
//     swap_test(checked);
//     comparable_test(unchecked);
//     comparable_test(checked);
// }

void iterators_test() {
    fwd_iterators_test<std::forward_list<int>>();
    fwd_iterators_test<std::list<int>>();
    fwd_iterators_test<std::vector<int>>();

    bidi_iterators_test<std::list<int>>();
    bidi_iterators_test<std::vector<int>>();

    nonmember_iterator_functions_test<int[]>();
    nonmember_iterator_functions_test<std::initializer_list<int>>();
    nonmember_iterator_functions_test<std::forward_list<int>>();
    nonmember_iterator_functions_test<std::list<int>>();
    nonmember_iterator_functions_test<std::vector<int>>();

    nonmember_reverse_iterator_functions_test<int[]>();
    nonmember_reverse_iterator_functions_test<std::initializer_list<int>>();
    nonmember_reverse_iterator_functions_test<std::list<int>>();
    nonmember_reverse_iterator_functions_test<std::vector<int>>();

    // msvc_array_iterators_test();

    int arr[]                = {1};
    std::initializer_list<int> il = {2};
    std::forward_list<int> flist{3};
    std::list<int> lst{4};
    std::vector<int> vec{5};

    (void) std::size(arr);
    (void) std::data(arr);
    (void) std::empty(arr);

    (void) std::size(il);
    (void) std::data(il);
    (void) std::empty(il);

    (void) std::empty(flist);

    (void) std::size(lst);
    (void) std::empty(lst);

    (void) std::size(vec);
    (void) std::data(vec);
    (void) std::empty(vec);

#if _HAS_CXX20
    (void) ssize(arr);
    (void) ssize(il);
    (void) ssize(lst);
    (void) ssize(vec);
#endif // _HAS_CXX20

    std::deque<int> value{1, 2, 3};
    auto it  = inserter(value, begin(value));
    auto fit = front_inserter(value);
    auto bit = back_inserter(value);

    swap_test(it);
    swap_test(fit);
    swap_test(bit);

    std::stringstream ss("1 2 3 4 5");
    std::istream_iterator<int> isi(ss);
    std::ostream_iterator<int> osi(ss, " ");
    std::istreambuf_iterator<char> isbi(ss);
    std::ostreambuf_iterator<char> osbi(ss);

    equality_test(isi);
    equality_test(isbi);
    swap_test(isi);
    swap_test(isbi);
    swap_test(osi);
    swap_test(osbi);
}

void istream_test() {
    std::stringstream ss{};
    std::istream is(ss.rdbuf());

    std::wstringstream wss{};
    std::wistream wis(wss.rdbuf());

    unsigned short us{};
    wis >> us;

    std::iostream ios(ss.rdbuf());
    std::wiostream wios(wss.rdbuf());

    // /analyze doesn't like these being nullptr
    char c_str[]           = "1";
    signed char sc_str[]   = "2";
    unsigned char uc_str[] = "3";

    is >> c_str;
    is >> *c_str;
    is >> sc_str;
    is >> *sc_str;
    is >> uc_str;
    is >> *uc_str;
    std::move(is) >> c_str;
    is >> std::ws; // io manipulator, not variable
}

template <typename T>
void numeric_limits_test_impl() {
    INSTANTIATE(std::numeric_limits<T>);
    INSTANTIATE(std::numeric_limits<const T>);
    INSTANTIATE(std::numeric_limits<volatile T>);
    INSTANTIATE(std::numeric_limits<const volatile T>);
}

void limits_test() {
    numeric_limits_test_impl<bool>();
    numeric_limits_test_impl<char>();
    numeric_limits_test_impl<signed char>();
    numeric_limits_test_impl<unsigned char>();
#ifdef __cpp_char8_t
    numeric_limits_test_impl<char8_t>();
#endif // __cpp_char8_t
    numeric_limits_test_impl<char16_t>();
    numeric_limits_test_impl<char32_t>();
    numeric_limits_test_impl<wchar_t>();
    numeric_limits_test_impl<short>();
    numeric_limits_test_impl<int>();
    numeric_limits_test_impl<long>();
    numeric_limits_test_impl<long long>();
    numeric_limits_test_impl<unsigned short>();
    numeric_limits_test_impl<unsigned int>();
    numeric_limits_test_impl<unsigned long>();
    numeric_limits_test_impl<unsigned long long>();
    numeric_limits_test_impl<float>();
    numeric_limits_test_impl<double>();
    numeric_limits_test_impl<long double>();

    numeric_limits_test_impl<std::string>();
}

void locale_test() {
    char c{};
    std::locale loc{};
    // need all collates to instantiate <locale> _Lstrcoll and _Lstrxfrm
    auto cc   = std::has_facet<std::collate<char>>(loc);
    auto cw   = std::has_facet<std::collate<wchar_t>>(loc);
    auto cbnc = std::has_facet<std::collate_byname<char>>(loc);
    auto cbnw = std::has_facet<std::collate_byname<wchar_t>>(loc);
    auto ctc  = std::has_facet<std::ctype<char>>(loc);
    (void) std::isalnum(c, loc);
    (void) std::isalpha(c, loc);
    (void) std::isblank(c, loc);
    (void) std::iscntrl(c, loc);
    (void) std::isdigit(c, loc);
    (void) std::isgraph(c, loc);
    (void) std::islower(c, loc);
    (void) std::isprint(c, loc);
    (void) std::ispunct(c, loc);
    (void) std::isspace(c, loc);
    (void) std::isupper(c, loc);
    (void) std::isxdigit(c, loc);
    (void) std::tolower(c, loc);
    (void) std::toupper(c, loc);

    (void) cc;
    (void) cw;
    (void) cbnc;
    (void) cbnw;
    (void) ctc;
}

template <typename OwnerLess, typename SmartPtr1, typename SmartPtr2>
void owner_less_test_impl(OwnerLess ol, SmartPtr1 ptr1, SmartPtr2 ptr2) {
    (void) ol(ptr1, ptr2);
    (void) ol(ptr2, ptr1);
}

template <typename OwnerLess, typename SmartPtr>
void owner_less_test_impl(OwnerLess ol, SmartPtr ptr) {
    // also tests _Ptr_base::owner_before
    owner_less_test_impl(ol, ptr, ptr);
}

void shared_ptr_test_impl() {
    std::shared_ptr<int> sptr0(new int());
    std::shared_ptr<int> sptr1(new int(), std::default_delete<int>{});
    std::shared_ptr<int> sptr2(nullptr, std::default_delete<int>{});
    std::shared_ptr<int> sptr3(nullptr, std::default_delete<int>{}, std::allocator<int>{});
    std::shared_ptr<int> sptr4(new int(), std::default_delete<int>{}, std::allocator<int>{});
    std::shared_ptr<int> sptr5(sptr0, nullptr);
    std::shared_ptr<void> sptr6(sptr1);
    std::shared_ptr<int> sptr7(std::weak_ptr<int>{sptr2});
#if _HAS_AUTO_PTR_ETC
    std::shared_ptr<int> sptr8(std::auto_ptr<int>{});
    sptr8 = std::auto_ptr<int>{};
#endif // _HAS_AUTO_PTR_ETC
    std::shared_ptr<void> sptr9(std::move(sptr3));
    sptr6 = std::move(sptr5);
    sptr9 = sptr7;

    sptr0.reset(new int(), std::default_delete<int>{});
    sptr0.reset(new int(), std::default_delete<int>{}, std::allocator<int>{});

    comparable_test(sptr0, sptr6);
    comparable_test(sptr0, nullptr);
    comparable_test(nullptr, sptr0);
    std::cout << sptr0;
    swap_test(sptr0);

    auto sptr11 = std::make_shared<int>(5);
    auto sptr12 = std::allocate_shared<int>(std::allocator<int>{}, 6);

    struct Cat {
        virtual ~Cat() {}
    };

    struct Kitten : Cat {};

    (void) std::static_pointer_cast<void>(sptr0);
    (void) std::const_pointer_cast<const int>(sptr0);
    (void) std::dynamic_pointer_cast<Kitten>(std::make_shared<Cat>());

    (void) std::get_deleter<std::default_delete<int>>(sptr0);
    hash_test(sptr0);

    (void) std::atomic_is_lock_free(&sptr0);
    (void) std::atomic_load(&sptr0);
    (void) std::atomic_load_explicit(&sptr0, std::memory_order_seq_cst);
    std::atomic_store(&sptr0, sptr0);
    std::atomic_store_explicit(&sptr0, sptr0, std::memory_order_seq_cst);
    std::atomic_exchange(&sptr0, sptr0);
    std::atomic_exchange_explicit(&sptr0, sptr0, std::memory_order_seq_cst);
    std::atomic_compare_exchange_weak(&sptr0, &sptr0, sptr0);
    std::atomic_compare_exchange_weak_explicit(&sptr0, &sptr0, sptr0, std::memory_order_seq_cst, std::memory_order_seq_cst);
    std::atomic_compare_exchange_strong(&sptr0, &sptr0, sptr0);
    std::atomic_compare_exchange_strong_explicit(&sptr0, &sptr0, sptr0, std::memory_order_seq_cst, std::memory_order_seq_cst);
}

void weak_ptr_test_impl() {
    std::weak_ptr<int> wptr0(std::make_shared<int>(5));
    std::weak_ptr<void> wptr1(wptr0);
    std::weak_ptr<void> wptr2(std::move(wptr0));

    wptr1 = wptr0;
    wptr2 = std::move(wptr0);
    wptr0 = std::make_shared<int>(5);
    swap_test(wptr0);
}


void memory_test() {
    shared_ptr_test_impl();
    weak_ptr_test_impl();

    struct my_shared_from_this : std::enable_shared_from_this<my_shared_from_this> {
        my_shared_from_this() {}
    };
    my_shared_from_this msft{};
    std::default_delete<void> dd0{std::default_delete<int>{}};
    (void) dd0;
    std::default_delete<int[]> dd1{std::default_delete<int[]>{}};
    dd1(new int[5]);

#if _HAS_GARBAGE_COLLECTION_SUPPORT_DELETED_IN_CXX23
    int* int_ptr{};
    std::undeclare_reachable(int_ptr);
#endif // _HAS_GARBAGE_COLLECTION_SUPPORT_DELETED_IN_CXX23

    auto sptr = std::make_shared<int>(5);
    auto wptr = std::weak_ptr<int>(sptr);
    owner_less_test_impl(std::owner_less<std::shared_ptr<int>>{}, sptr);
    owner_less_test_impl(std::owner_less<std::weak_ptr<int>>{}, std::weak_ptr<int>(sptr));
    owner_less_test_impl(std::owner_less<void>{}, sptr, wptr);
}

#ifndef _M_CEE_PURE
template <typename Mutex>
void timed_mutex_test_impl() {
    Mutex mtx{};
    (void) mtx.try_lock_for(std::chrono::seconds(1));
    (void) mtx.try_lock_until(std::chrono::system_clock::now());
}

void mutex_test() {
    std::mutex mtx{};
    std::recursive_mutex rmtx{};
    std::recursive_timed_mutex rtmtx{};

    (void) std::try_lock(mtx, rmtx);
    (void) std::try_lock(mtx, rmtx, rtmtx);
    std::lock(mtx, rmtx);
    std::lock(mtx, rmtx, rtmtx);
    // lock_guard and scoped_lock instantiated in P0156R2_scoped_lock
    std::unique_lock<std::recursive_timed_mutex> ul_mtx1{rtmtx, std::chrono::seconds(1)};
    std::unique_lock<std::recursive_timed_mutex> ul_mtx2{rtmtx, std::chrono::system_clock::now()};

    timed_mutex_test_impl<std::unique_lock<std::recursive_timed_mutex>>();
    swap_test(ul_mtx2);
    std::once_flag of{};
    std::call_once(of, []() {});
    condition_variable_test_impl<std::condition_variable>();

    timed_mutex_test_impl<std::timed_mutex>();
    timed_mutex_test_impl<std::recursive_timed_mutex>();
}
#endif // _M_CEE_PURE

void ostream_test() {
    std::stringstream ss{};
    std::ostream os{ss.rdbuf()};

    std::wstringstream wss{};
    std::wostream wos{wss.rdbuf()};

    unsigned short us{};
    wos << us;

    // /analyze doesn't like these being nullptr
    char c_str[]     = "1";
    wchar_t wc_str[] = L"2";

    wos << c_str;
    wos << *c_str;
    os << c_str;
    os << *c_str;
    wos << wc_str;
    wos << *wc_str;

    signed char sc_str[]   = "3";
    unsigned char uc_str[] = "4";

    os << sc_str;
    os << *sc_str;
    os << uc_str;
    os << *uc_str;

    std::string str{};
    os << str;
    os << std::endl;
    os << std::ends;
    os << std::flush;
    os << std::error_code{};
}

template <typename Distribution>
void distribution_test_impl(Distribution& d) {
    std::random_device rd{};
    std::mt19937 gen(rd());
    (void) d(gen);
    (void) d(gen, d.param());
    equality_test(d);
    std::cin >> d;
    std::cout << d;
}

template <typename Engine, typename SeedArg>
void common_engine_test_impl(SeedArg& sa) {
    Engine eng(sa);
    eng.seed(sa);
    equality_test(eng);
    std::cin >> eng;
    std::cout << eng;
}

template <typename Engine>
void engine_test_impl() {
    std::seed_seq ss({1, 2, 3, 4, 5});
    common_engine_test_impl<Engine>(ss);
}

#if _HAS_TR1_NAMESPACE
template <typename Engine>
void tr1_engine_test_impl() {
    std::random_device rd{};
    std::mt19937 gen(rd());
    common_engine_test_impl<Engine>(gen);
}
#endif // _HAS_TR1_NAMESPACE

void random_test() {
    std::seed_seq ss0({1, 2, 3, 4, 5, 6});
    std::vector<uint32_t> v{1, 2, 3, 4};
    std::seed_seq ss1(v.begin(), v.end());
    ss0.generate(v.begin(), v.end());
    ss1.param(std::ostream_iterator<unsigned int>(std::cout, " "));

    std::random_device rd{};
    std::mt19937 gen(rd());
    (void) std::generate_canonical<double, 10>(gen);

    engine_test_impl<std::minstd_rand0>(); // linear_congruential_engine
    engine_test_impl<std::mt19937>(); // mersenne_twister_engine
    engine_test_impl<std::ranlux24_base>(); // subtract_with_carry_engine
    engine_test_impl<std::ranlux24>(); // discard_block_engine
    engine_test_impl<std::knuth_b>(); // shuffle_order_engine
    engine_test_impl<std::independent_bits_engine<std::minstd_rand0, 2, uint32_t>>();
    engine_test_impl<std::mt19937_64>();
    engine_test_impl<std::ranlux48_base>();
    engine_test_impl<std::ranlux48>();

#if _HAS_TR1_NAMESPACE
    std::tr1::linear_congruential<uint_fast32_t, 16807, 0, 2147483647> minstd_rand_eng(gen);
    minstd_rand_eng.seed(gen);

    tr1_engine_test_impl<std::tr1::linear_congruential<uint_fast32_t, 16807, 0, 2147483647>>();
    tr1_engine_test_impl<std::tr1::ranlux3>();
    tr1_engine_test_impl<std::tr1::ranlux4>();
    tr1_engine_test_impl<std::tr1::ranlux3_01>();
    tr1_engine_test_impl<std::tr1::ranlux4_01>();
    tr1_engine_test_impl<std::tr1::ranlux64_base_01>();
    tr1_engine_test_impl<std::tr1::ranlux_base_01>();
#endif // _HAS_TR1_NAMESPACE

    std::uniform_int_distribution<> uni_int_d{};
    std::bernoulli_distribution bern_d{};
    std::geometric_distribution<> geo_d{};
    std::poisson_distribution<> pois_d{};
    std::binomial_distribution<> binom_d{};
    std::uniform_real_distribution<> uni_real_d{};
    std::exponential_distribution<> expon_d{};
    std::normal_distribution<> norm_d{};
    std::gamma_distribution<> gamma_d{};
    std::weibull_distribution<> weib_d{};
    std::extreme_value_distribution<> ext_v_d{};
    std::lognormal_distribution<> log_norm_d{};
    std::chi_squared_distribution<> chi_sq_d{};
    std::cauchy_distribution<> cauchy_d{};
    std::fisher_f_distribution<> fish_f_d{};
    std::student_t_distribution<> stud_t_d{};
    std::negative_binomial_distribution<> neg_bi_d{};
    std::discrete_distribution<> disc_d1(v.begin(), v.end());
    auto sq_func = [](double val) { return val * val; };
    std::discrete_distribution<> disc_d2(5, 3.5, 7.5, sq_func);
    std::piecewise_constant_distribution<> piece_const_d1(v.begin(), v.end(), v.begin());
    std::piecewise_constant_distribution<> piece_const_d2{{1.0, 2.0, 3.0, 4.0, 5.0}, sq_func};
    std::piecewise_constant_distribution<> piece_const_d3(4, 0.0, 10.0, sq_func);
    std::piecewise_linear_distribution<> piece_line_d1(v.begin(), v.end(), v.begin());
    std::piecewise_linear_distribution<> piece_line_d2({1.0, 2.0, 3.0, 4.0}, sq_func);
    std::piecewise_linear_distribution<> piece_line_d3(4, 0.0, 10.0, sq_func);

    distribution_test_impl(uni_int_d);
    distribution_test_impl(bern_d);
    distribution_test_impl(geo_d);
    distribution_test_impl(pois_d);
    distribution_test_impl(binom_d);
    distribution_test_impl(uni_real_d);
    distribution_test_impl(expon_d);
    distribution_test_impl(norm_d);
    distribution_test_impl(gamma_d);
    distribution_test_impl(weib_d);
    distribution_test_impl(ext_v_d);
    distribution_test_impl(log_norm_d);
    distribution_test_impl(chi_sq_d);
    distribution_test_impl(cauchy_d);
    distribution_test_impl(fish_f_d);
    distribution_test_impl(stud_t_d);
    distribution_test_impl(neg_bi_d);
    distribution_test_impl(disc_d1);
    distribution_test_impl(disc_d2);
    distribution_test_impl(piece_const_d1);
    distribution_test_impl(piece_const_d2);
    distribution_test_impl(piece_const_d3);
    distribution_test_impl(piece_line_d1);
    distribution_test_impl(piece_line_d2);
    distribution_test_impl(piece_line_d3);
}

void ratio_test() {
    using half       = std::ratio<1, 2>;
    using one        = std::ratio_add<half, half>;
    using half_again = std::ratio_subtract<one, half>;
    using quarter    = std::ratio_multiply<half, half>;
    using two        = std::ratio_divide<one, half>;

    TRAIT_V(std::ratio_equal, half, half);
    TRAIT_V(std::ratio_not_equal, half_again, quarter);
    TRAIT_V(std::ratio_less, half, two);
    TRAIT_V(std::ratio_less_equal, half, half_again);
    TRAIT_V(std::ratio_greater, one, half);
    TRAIT_V(std::ratio_greater_equal, half, half_again);
}

template <typename CharType>
void regex_traits_test_impl() {
    CharType buffer[10]{};
    std::regex_traits<CharType> rt{};

    rt.transform(std::begin(buffer), std::end(buffer));
    rt.transform_primary(std::begin(buffer), std::end(buffer));
    rt.lookup_classname(std::begin(buffer), std::end(buffer));
    rt.lookup_collatename(std::begin(buffer), std::end(buffer));
}

template <typename SubMatchType>
void sub_match_test_impl() {
    SubMatchType sm{};
    using char_type = typename SubMatchType::value_type;

    char_type value{};
    std::basic_string<char_type> str{};

    comparable_test(sm);
    comparable_test(sm, &value);
    comparable_test(&value, sm);
    comparable_test(sm, value);
    comparable_test(value, sm);
    comparable_test(sm, str);
    comparable_test(str, sm);

    std::basic_stringstream<char_type> ss{};
    ss << sm;
}

template <typename MatchResultsType>
void match_results_test_impl() {
    MatchResultsType mr{};
    using char_type = typename MatchResultsType::char_type;
    std::basic_stringstream<char_type> ss{};
    std::ostream_iterator<int, char_type> out_it(ss);
    std::basic_string<char_type> str{};

    char_type out_buffer[5]{};

    mr.format(out_it, str.data(), str.data() + str.size());
    mr.format(out_it, str);
    mr.format(out_buffer, str.data(), str.data() + str.size());
    mr.format(out_buffer, str);
    str = mr.format(str);

    equality_test(mr);
    swap_test(mr);
}

template <typename BasicRegexType>
void basic_regex_test_impl() {
    using char_type = typename BasicRegexType::value_type;
    std::basic_string<char_type> str{};

    BasicRegexType br1(str);
    BasicRegexType br2(std::begin(str), std::end(str), std::regex_constants::ECMAScript);
    BasicRegexType br3(std::begin(str), std::end(str));

    br1 = str;
    br2.assign(str);
    br3.assign(std::begin(str), std::end(str));

    swap_test(br1);

    (void) std::regex_match(std::begin(str), std::end(str), br1);
    (void) std::regex_match(str.c_str(), br1);

    std::match_results<const char_type*> mrc{};
    std::regex_match(str.c_str(), mrc, br1);

    std::match_results<typename std::basic_string<char_type>::const_iterator> mrs{};
    std::regex_match(str, mrs, br1);
    (void) std::regex_match(str, br1);

    std::regex_search(std::cbegin(str), std::cend(str), mrs, br1);
    (void) std::regex_search(std::cbegin(str), std::cend(str), br1);
    (void) std::regex_search(str.c_str(), br1);
    std::regex_search(str.c_str(), mrc, br1);
    std::regex_search(str, mrs, br1);
    (void) std::regex_search(str, br1);

    std::basic_stringstream<char_type> ss{};
    std::ostream_iterator<int, char_type> out_it(ss);
    char_type out_buffer[5] = {0};

    std::regex_replace(out_it, std::cbegin(str), std::cend(str), br1, str);
    std::regex_replace(out_buffer, std::cbegin(str), std::cend(str), br1, str);
    std::regex_replace(out_it, std::begin(str), std::end(str), br1, str.c_str());
    (void) std::regex_replace(str, br1, str);
    (void) std::regex_replace(str, br1, str.c_str());
    (void) std::regex_replace(str.c_str(), br1, str);
    (void) std::regex_replace(str.c_str(), br1, str.c_str());
}

template <typename RegexTokenIterator>
void regex_token_iterator_test_impl() {
    using it_type      = typename RegexTokenIterator::value_type::iterator;
    int submatches[10] = {0};
    it_type start{};
    it_type finish{};
    typename RegexTokenIterator::regex_type rgx{};
    RegexTokenIterator rti0(start, finish, rgx, submatches);
}

void regex_test() {
    regex_traits_test_impl<char>();
    regex_traits_test_impl<wchar_t>();

    sub_match_test_impl<std::csub_match>();
    sub_match_test_impl<std::wcsub_match>();
    sub_match_test_impl<std::ssub_match>();
    sub_match_test_impl<std::wssub_match>();

    match_results_test_impl<std::cmatch>();
    match_results_test_impl<std::wcmatch>();
    match_results_test_impl<std::smatch>();
    match_results_test_impl<std::wsmatch>();

    basic_regex_test_impl<std::regex>();
    basic_regex_test_impl<std::wregex>();

    std::cregex_iterator cri{};
    std::wcregex_iterator wcri{};
    std::sregex_iterator sri{};
    std::wsregex_iterator wsri{};

    regex_token_iterator_test_impl<std::cregex_token_iterator>();
    regex_token_iterator_test_impl<std::wcregex_token_iterator>();
    regex_token_iterator_test_impl<std::sregex_token_iterator>();
    regex_token_iterator_test_impl<std::wsregex_token_iterator>();
}

// need custom minimal allocator to use for scoped_allocator_test to reach all template paths
template <typename T>
struct custom_allocator {
    using value_type = T;

    custom_allocator() {}

    template <class U>
    custom_allocator(const std::allocator<U>&) {}

    T* allocate(size_t) {
        return nullptr;
    }

    void deallocate(T*, size_t) {}
};

template <typename T, typename U>
bool operator==(const std::allocator<T>&, const std::allocator<U>&) {
    return true;
}

template <typename T, typename U>
bool operator!=(const std::allocator<T>& a, const std::allocator<U>& b) {
    return !(a == b);
}

void scoped_allocator_test() {
    using my_vector_saa = scoped_allocator_adaptor<std::allocator<std::vector<int>>, std::allocator<int>>;
    using my_double_saa = scoped_allocator_adaptor<std::allocator<std::vector<double>>, std::allocator<int>>;

    INSTANTIATE(my_vector_saa::rebind<std::allocator<double>>);

    std::allocator<std::vector<int>> vec_alloc{};
    std::allocator<int> int_alloc{};

    my_vector_saa saa1(std::move(vec_alloc), std::move(int_alloc));
    my_double_saa saa2(saa1);
    my_vector_saa saa3(std::move(saa2));
    saa2 = std::move(saa1);
    saa3 = saa2;

    scoped_allocator_adaptor<std::allocator<int>>
        custom_saa{}; // needed to hit .construct paths with nonconvertible allocator

    std::vector<int> val{};
    std::vector<int>* ptr = &val;
    custom_saa.construct(ptr, static_cast<size_t>(1), 2);

    saa1.construct(ptr, static_cast<size_t>(1), 2);

    tuple<int, int> tuple_val{};
    tuple<int, int>* tuple_ptr = &tuple_val; // tuple needed to hit .construct paths with std::allocator_arg_t
    saa1.construct(tuple_ptr, make_pair(1, 2));

    using my_pair = pair<std::vector<int>, std::vector<int>>;
    auto pair_ptr = static_cast<my_pair*>(malloc(sizeof(my_pair)));

    custom_saa.construct(
        pair_ptr, piecewise_construct, make_tuple(static_cast<size_t>(1), 2), make_tuple(static_cast<size_t>(1), 2));
    saa1.construct(
        pair_ptr, piecewise_construct, make_tuple(static_cast<size_t>(1), 2), make_tuple(static_cast<size_t>(1), 2));

    using tuple_pair = pair<tuple<int, int>, tuple<int, int>>;
    auto tp_ptr      = static_cast<tuple_pair*>(malloc(sizeof(tuple_pair)));
    saa1.construct(tp_ptr, piecewise_construct, make_tuple(make_pair(1, 2)), make_tuple(make_pair(1, 2)));

    saa1.construct(pair_ptr);
    std::vector<int> vec{};
    saa1.construct(pair_ptr, std::move(vec), std::move(vec));

    my_pair vec_pair{};
    saa1.construct(pair_ptr, vec_pair);
    saa1.construct(pair_ptr, std::move(vec_pair));

    equality_test(saa1); // same outer, same inner
    equality_test(saa1, saa2); // different outer, same inner

    scoped_allocator_adaptor<std::allocator<std::vector<double>>, std::allocator<double>> saa4;
    equality_test(saa1, saa4); // different outer, different inner

    scoped_allocator_adaptor<std::allocator<std::vector<int>>, std::allocator<double>> saa5{};
    equality_test(saa1, saa5); // same outer, different inner

    scoped_allocator_adaptor<std::allocator<std::vector<double>>> saa6; // different outer, missing inner
    equality_test(saa1, saa6);

    scoped_allocator_adaptor<std::allocator<std::vector<int>>> saa7; // same outer, missing inner
    equality_test(saa1, saa7);
}

#ifndef _M_CEE_PURE
void shared_mutex_test() {
    using namespace std::chrono;

    timed_mutex_test_impl<std::shared_timed_mutex>();

    std::shared_timed_mutex stm{};
    (void) stm.try_lock_shared_for(seconds(1));
    (void) stm.try_lock_shared_until(std::chrono::system_clock::now());

    std::shared_lock<std::shared_timed_mutex> sl1(stm, std::chrono::seconds(1));
    std::shared_lock<std::shared_timed_mutex> sl2(stm, std::chrono::system_clock::now());

    (void) sl1.try_lock_for(seconds(1));
    (void) sl2.try_lock_until(std::chrono::system_clock::now());
    swap_test(sl1);
}
#endif // _M_CEE_PURE

template <typename T>
void sstream_test_impl() {
    T val{};
    swap_test(val);
}

void sstream_test() {
    sstream_test_impl<std::stringbuf>();
    sstream_test_impl<std::wstringbuf>();
    sstream_test_impl<std::istringstream>();
    sstream_test_impl<std::wistringstream>();
    sstream_test_impl<std::ostringstream>();
    sstream_test_impl<std::wostringstream>();
    sstream_test_impl<std::stringstream>();
    sstream_test_impl<std::wstringstream>();
}

void streambuf_test() {
    std::stringstream ss{};
    std::basic_streambuf<char>& bsbc = *ss.rdbuf();
    (void) bsbc;

    std::wstringstream wss{};
    std::basic_streambuf<wchar_t>& bsbwc = *wss.rdbuf();
    (void) bsbwc;

    // istreambuf_iterator and ostreambuf_iterator covered in iterators test
}

#ifndef _M_CEE_PURE
void thread_test() {
    using namespace std::chrono;

    std::thread thr([](int, int) {}, 1, 2);

    std::this_thread::sleep_for(seconds(1));
    std::this_thread::sleep_until(std::chrono::system_clock::now());

    std::thread::id thr_id{};

    std::cout << thr_id;
    hash_test(thr_id);
}
#endif // _M_CEE_PURE

void tuple_test() {
    std::allocator<double> my_alloc{};
    std::allocator<double> custom_alloc{}; // to hit _Tuple_val non-convertible allocator

    tuple<> empty_tuple1(std::allocator_arg, std::allocator<double>());
    tuple<> empty_tuple2(std::allocator_arg, std::allocator<double>(), empty_tuple1);
    (void) empty_tuple2;

    tuple<int, int> tup1{};
    tuple<const int&, const int&> tup2(tup1);
    tuple<const int&, const int&> tup3(std::allocator_arg, my_alloc, tup1);
    tuple<const char*, const char*> tup4("Hello", "World");
    tuple<const char*, const char*> tup5(std::allocator_arg, my_alloc, "Hello", "World");
    tuple<std::string, std::string> tup6("Hello", "World");
    tuple<std::string, std::string> tup7(std::allocator_arg, my_alloc, "Hello", "World");

    tup6 = tup4;
    tup7 = std::move(tup5);

    tuple<int, int> tup8(std::allocator_arg, my_alloc);
    tuple<int, int> tup9(std::allocator_arg, my_alloc, tup8);
    auto int_pair = make_pair(1, 2);
    tuple<int, int> tup10(int_pair);
    tuple<int, int> tup11(std::allocator_arg, my_alloc, int_pair);

    tup10 = int_pair;

    tuple<int, int> tup12(std::allocator_arg, my_alloc, std::move(tup9));
    tuple<int, int> tup13(std::move(int_pair));
    tuple<int, int> tup14(std::allocator_arg, my_alloc, std::move(int_pair));

    tup11 = std::move(int_pair);

    comparable_test(tup12);
    swap_test(tup13);

    // Extras to ensure hitting _Tuple_val specializations.
    tuple<int> tup15(std::allocator_arg, custom_alloc, 1); // construct with non-convertible allocator.
    tuple<tuple<int, int>> tup16(std::allocator_arg, my_alloc, tup14); // construct with leading allocator.
    tuple<std::string> tup17(std::allocator_arg, my_alloc, "MyStr"); // construct with trailing allocator.

    (void) get<0>(tup15);
    (void) get<0>(std::move(tup16));

    const tuple<std::string> tup17_c = tup17;
    (void) get<0>(tup17_c);
    (void) get<0>(std::move(tup17_c));

    (void) get<int>(tup15);
    (void) get<tuple<int, int>>(std::move(tup16));
    (void) get<std::string>(tup17_c);
    (void) get<std::string>(std::move(tup17_c));

    // (void) get<volatile int>(tuple<volatile int>{});
    // (void) get<const volatile int>(tuple<const volatile int>{});

    int a = 1, b = 2;
    tie(a, b) = make_tuple(a, b);
    (void) forward_as_tuple(std::string{}, std::string{});

    (void) tuple_cat(tup15, tup16);

#if _HAS_CXX17
    apply(plus<>{}, tup1);
    (void) make_from_tuple<long>(tuple<int>(1729));
#endif // _HAS_CXX17

    pair<std::string, std::string> pair1(
        piecewise_construct, make_tuple("Hello", static_cast<size_t>(6)), make_tuple("World", static_cast<size_t>(6)));

    TRAIT_V(uses_allocator, tuple<int>, std::allocator<double>);
}

struct utility_test_helper {};

bool operator==(const utility_test_helper&, const utility_test_helper&) {
    return true;
}

bool operator<(const utility_test_helper&, const utility_test_helper&) {
    return false;
}

void utility_test() {
    struct my_class1 {};
    my_class1 mc1{};
    swap_test(mc1);

    // iter_swap covered in algorithms_test

    int arr1[5] = {0};
    int arr2[5] = {0};
    swap(arr1, arr2);

    pair<int, int> p1 = make_pair(1, 2);
    pair<const int&, const int&> p2(p1);
    p1 = p2;

    // pair piecewise construct from tuple covered in tuple_test

    pair<std::string, std::string> p3("Hello", "World");
    pair<const char*, const char*> p4("Hello", "World");
    pair<std::string, std::string> p5(std::move(p4));
    p5 = std::move(p4);

    swap_test(p3);
    comparable_test(p1);

    {
        utility_test_helper uth{};
        using namespace rel_ops;
        USE_VALUE(uth != uth);
        USE_VALUE(uth > uth);
        USE_VALUE(uth <= uth);
        USE_VALUE(uth >= uth);
    }

    TRAIT_V(tuple_size, array<int, 5>);
    TRAIT_V(tuple_size, pair<int, double>);
    TRAIT_V(tuple_size, tuple<int, int, int>);
    TRAIT_V(tuple_size, const tuple<int, int, int>);
    TRAIT_V(tuple_size, volatile tuple<int, int, int>);
    TRAIT_V(tuple_size, const volatile tuple<int, int, int>);

    INSTANTIATE(tuple_element_t<1, array<int, 5>>);
    INSTANTIATE(tuple_element_t<0, pair<int, double>>);
    INSTANTIATE(tuple_element_t<1, pair<int, double>>);
    INSTANTIATE(tuple_element_t<2, tuple<int, int, int>>);
    INSTANTIATE(tuple_element_t<2, const tuple<int, int, int>>);
    INSTANTIATE(tuple_element_t<2, volatile tuple<int, int, int>>);
    INSTANTIATE(tuple_element_t<2, const volatile tuple<int, int, int>>);

    auto p6       = make_pair(1, std::string("test"));
    const auto p7 = as_const(p6);

    (void) get<0>(p6);
    (void) get<int>(p6);
    (void) get<std::string>(p6);

    (void) get<0>(p7);
    (void) get<int>(p7);
    (void) get<std::string>(p7);

    (void) get<0>(std::move(p6));
    (void) get<int>(std::move(p6));
    (void) get<std::string>(std::move(p6));

    (void) get<0>(std::move(p7));
    (void) get<int>(std::move(p7));
    (void) get<std::string>(std::move(p7));

    exchange(p3, std::move(p5));
}

void typeindex_test() {
    type_index ti(typeid(int));
    hash_test(ti);
}

template <typename FunctorArg, typename Arg>
void functors_test_impl(Arg val) {
    // Following from <type_traits> and <xutility>:
    (void) plus<FunctorArg>()(val, val);
    (void) minus<FunctorArg>()(val, val);
    (void) multiplies<FunctorArg>()(val, val);
    (void) equal_to<FunctorArg>()(val, val);
    (void) less<FunctorArg>()(val, val);

    (void) divides<FunctorArg>()(val, val);
    (void) modulus<FunctorArg>()(val, val);
    (void) negate<FunctorArg>()(val);
    (void) not_equal_to<FunctorArg>()(val, val);
    (void) greater<FunctorArg>()(val, val);
    (void) greater_equal<FunctorArg>()(val, val);
    (void) less_equal<FunctorArg>()(val, val);
    (void) logical_and<FunctorArg>()(val, val);
    (void) logical_or<FunctorArg>()(val, val);
    (void) logical_not<FunctorArg>()(val);
    (void) bit_and<FunctorArg>()(val, val);
    (void) bit_or<FunctorArg>()(val, val);
    (void) bit_xor<FunctorArg>()(val, val);
    (void) bit_not<FunctorArg>()(val);
}

int real_unary_function(int) {
    return 1;
}

int real_binary_function(int, int) {
    return 1;
}

void xfunctional_test() {
    functors_test_impl<int>(5);
    functors_test_impl<void>(5);
    // (void) not1(negate<int>())(5); // not1 requires T::second_argument_type
    // (void) not2(less_equal<int>())(5, 5); // not2 requires T::second_argument_type

#if _HAS_AUTO_PTR_ETC
    auto b1 = bind1st(plus<int>(), 1);
    auto b2 = bind2nd(plus<int>(), 2);

    (void) b1;
    (void) b2;

    auto ptuf = ptr_fun(real_unary_function);
    auto ptbf = ptr_fun(real_binary_function);

    (void) ptuf;
    (void) ptbf;

    struct A {
        int fn() {
            return 1;
        }
        int fn1(int) {
            return 2;
        }
        int cfn() const {
            return 3;
        }
        int cfn1(int) const {
            return 4;
        }
    };

    auto mft   = mem_fun(&A::fn);
    auto mft1  = mem_fun(&A::fn1);
    auto cmft  = mem_fun(&A::cfn);
    auto cmft1 = mem_fun(&A::cfn1);

    auto mfrt   = std::mem_fun_ref(&A::fn);
    auto mfrt1  = std::mem_fun_ref(&A::fn1);
    auto cmfrt  = std::mem_fun_ref(&A::cfn);
    auto cmfrt1 = std::mem_fun_ref(&A::cfn1);

    (void) mft;
    (void) mft1;
    (void) cmft;
    (void) cmft1;

    (void) mfrt;
    (void) mfrt1;
    (void) cmfrt;
    (void) cmfrt1;
#endif // _HAS_AUTO_PTR_ETC
}

// template <typename CharType>
// void facet_unicode_and_native_wchart_test_impl() {
//     locale loc{};

//     // <codecvt>
//     codecvt_utf8<CharType> cvt_utf8{};
//     codecvt_utf16<CharType> cvt_utf16{};
//     codecvt_utf8_utf16<CharType> cvt_utf8_utf16{};
// }

// template <typename CharType>
// void facet_all_test_impl() {
//     locale loc{};
//     auto ccvt    = has_facet<codecvt<CharType, char, mbstate_t>>(loc);
//     auto ccvt_bn = has_facet<codecvt_byname<CharType, char, mbstate_t>>(loc);
//     auto ct      = has_facet<ctype<CharType>>(loc);
//     auto ct_bn   = has_facet<ctype_byname<CharType>>(loc);

//     // <xlocmon>
//     auto mp        = has_facet<moneypunct<CharType, false>>(loc);
//     auto mpbn      = has_facet<moneypunct_byname<CharType, false>>(loc);
//     auto mp_intl   = has_facet<moneypunct<CharType, true>>(loc);
//     auto mpbn_intl = has_facet<moneypunct_byname<CharType, true>>(loc);
//     auto mg        = has_facet<money_get<CharType>>(loc);
//     auto mput      = has_facet<money_put<CharType>>(loc);

//     // <xlocnum>
//     auto np   = has_facet<numpunct<CharType>>(loc);
//     auto npbn = has_facet<numpunct_byname<CharType>>(loc);
//     auto ng   = has_facet<num_get<CharType>>(loc);
//     auto nput = has_facet<num_put<CharType>>(loc);

//     // <xloctime>
//     auto tg   = has_facet<time_get<CharType>>(loc);
//     auto tgbn = has_facet<time_get_byname<CharType>>(loc);
//     auto tp   = has_facet<time_put<CharType>>(loc);
//     auto tpbn = has_facet<time_put_byname<CharType>>(loc);

//     // <xlocmes>
//     auto locmes   = has_facet<messages<CharType>>(loc);
//     auto locmesbn = has_facet<messages_byname<CharType>>(loc);

//     facet_unicode_and_native_wchart_test_impl<CharType>();

//     (void) ccvt;
//     (void) ccvt_bn;
//     (void) ct;
//     (void) ct_bn;
//     (void) mp;
//     (void) mpbn;
//     (void) mp_intl;
//     (void) mpbn_intl;
//     (void) mg;
//     (void) mput;
//     (void) np;
//     (void) npbn;
//     (void) ng;
//     (void) nput;
//     (void) tg;
//     (void) tgbn;
//     (void) tp;
//     (void) tpbn;
//     (void) locmes;
//     (void) locmesbn;
// }

// void xlocale_test() {
//     locale loc1{};

//     loc1(std::string{"Hello"}, std::string{"World"});
//     loc1.combine<numpunct<char>>(loc1);
//     locale loc2(loc1, new codecvt_utf8<wchar_t>);
//     use_facet<moneypunct<char, true>>(loc2);

//     // This test also covers <codecvt>
//     facet_all_test_impl<char>();
//     facet_all_test_impl<wchar_t>();
//     facet_unicode_and_native_wchart_test_impl<char16_t>();
//     facet_unicode_and_native_wchart_test_impl<char32_t>();
// #ifndef _NATIVE_WCHAR_T_DEFINED
//     facet_all_test_impl<unsigned short>();
// #endif // _NATIVE_WCHAR_T_DEFINED
// }

// void xlocbuf_test() {
//     wbuffer_convert<codecvt_utf8<wchar_t>> wb_cvt{};
//     wstring_convert<codecvt_utf8<wchar_t>> ws_cvt{};
// }

// void xmemory_test() {
//     int* buff = get_temporary_buffer<int>(4).first;
//     raw_storage_iterator<int*, int> rsi(buff);
//     return_temporary_buffer(buff);
//     (void) rsi;

//     // uninitialized_* in algorithms_test

// #if _HAS_AUTO_PTR_ETC
//     struct Base {};
//     struct Derived : Base {};

//     auto_ptr<Derived> ap1{};
//     auto auto_ptr_source = []() { return auto_ptr<Derived>{}; };
//     auto_ptr<Base> ap2(auto_ptr_source()); // converts to auto_ptr_ref

//     USE_VALUE(static_cast<auto_ptr<Base>>(ap1));

//     auto_ptr<Base> ap3(ap1);
//     ap3 = ap1;
// #endif // _HAS_AUTO_PTR_ETC
// }

void xmemory0_test() {

    struct Base {};

    struct Derived : Base {};

    INSTANTIATE(pointer_traits<int*>);
    INSTANTIATE(pointer_traits<Base*>::rebind<Derived*>);

    INSTANTIATE(allocator_traits<std::allocator<int>>);
    INSTANTIATE(allocator_traits<std::allocator<int>>::rebind_alloc<double>);
    INSTANTIATE(allocator_traits<std::allocator<int>>::rebind_traits<double>);

    std::allocator<int> ai{};
    std::allocator<int> cai{};
    int* ptr{};

    allocator_traits<std::allocator<int>>::construct(cai, ptr, 5);
    allocator_traits<std::allocator<int>>::destroy(cai, ptr);

    allocator_traits<std::allocator<int>>::construct(ai, ptr, 5);
    allocator_traits<std::allocator<int>>::destroy(ai, ptr);

    INSTANTIATE(std::allocator<int>::rebind<double>);

    std::allocator<double> ad(ai);
    ai = ad;

    std::allocator<int> ai2(ad);

    ai2.construct(ptr, 1);
    ai2.destroy(ptr);

    INSTANTIATE(std::allocator<double>::rebind<int>);

    ad = ai2;
    equality_test(ad, ai);
}

void xstddef_test() {
#if _HAS_AUTO_PTR_ETC
    INSTANTIATE(unary_function<int, int>);
    INSTANTIATE(binary_function<int, int, int>);
#endif // _HAS_AUTO_PTR_ETC

    // plus<>, minus<>, multiplies<>, equal_to<>, less<> tested in xfunctional_test

    hash_test<bool>();
    hash_test<char>();
    hash_test<signed char>();
    hash_test<unsigned char>();
#ifdef __cpp_char8_t
    hash_test<char8_t>();
#endif // __cpp_char8_t
    hash_test<char16_t>();
    hash_test<char32_t>();
    hash_test<wchar_t>();
    hash_test<short>();
    hash_test<unsigned short>();
    hash_test<int>();
    hash_test<unsigned int>();
    hash_test<long>();
    hash_test<unsigned long>();
    hash_test<long long>();
    hash_test<unsigned long long>();
    hash_test<float>();
    hash_test<double>();
    hash_test<long double>();
    hash_test<void*>();
    // is_function covered in traits_test

    int value{};
    (void) addressof(value);
    (void) addressof(real_unary_function);
}

template <typename T1>
void xtgmath_integral_test_impl() {
    T1 arg1{1};
    int int_value{};
    long long_value{};
    long double ld_value{};

    (void) acos(arg1);
    (void) asin(arg1);
    (void) atan(arg1);
    (void) ceil(arg1);
    (void) cos(arg1);
    (void) cosh(arg1);
    (void) exp(arg1);
    (void) fabs(arg1);
    (void) floor(arg1);
    (void) frexp(arg1, &int_value);
    (void) ldexp(arg1, int_value);
    (void) log(arg1);
    (void) log10(arg1);
    (void) sin(arg1);
    (void) sinh(arg1);
    (void) sqrt(arg1);
    (void) tan(arg1);
    (void) tanh(arg1);
    (void) acosh(arg1);
    (void) asinh(arg1);
    (void) atanh(arg1);
    (void) cbrt(arg1);
    (void) erf(arg1);
    (void) erfc(arg1);
    (void) expm1(arg1);
    (void) exp2(arg1);
    (void) ilogb(arg1);
    (void) lgamma(arg1);
    (void) llrint(arg1);
    (void) llround(arg1);
    (void) log1p(arg1);
    (void) log2(arg1);
    (void) logb(arg1);
    (void) lrint(arg1);
    (void) lround(arg1);
    (void) nearbyint(arg1);
    (void) nexttoward(arg1, ld_value);
    (void) rint(arg1);
    (void) round(arg1);
    (void) scalbln(arg1, long_value);
    (void) scalbn(arg1, int_value);
    (void) tgamma(arg1);
    (void) trunc(arg1);
}

template <typename T1, typename T2, typename T3>
void xtgmath_arithmetic_test_impl() {
    T1 arg1{1};
    T2 arg2{2};

    int int_value{};

    USE_VALUE(atan2(arg1, arg2));
    USE_VALUE(fmod(arg1, arg2));
    T3 arg3{3};
    USE_VALUE(fma(arg1, arg2, arg3));
    USE_VALUE(remquo(arg1, arg2, &int_value));
    USE_VALUE(copysign(arg1, arg2));
    USE_VALUE(fdim(arg1, arg2));
    USE_VALUE(fmax(arg1, arg2));
    USE_VALUE(fmin(arg1, arg2));
    USE_VALUE(hypot(arg1, arg2));
    USE_VALUE(nextafter(arg1, arg2));
    USE_VALUE(remainder(arg1, arg2));
}

void xtgmath_test() {
    xtgmath_integral_test_impl<int>();
    xtgmath_arithmetic_test_impl<int, int, int>();
    xtgmath_arithmetic_test_impl<int, int, float>();
    xtgmath_arithmetic_test_impl<int, float, float>();
    xtgmath_arithmetic_test_impl<float, float, float>();
    xtgmath_arithmetic_test_impl<int, float, double>();
    xtgmath_arithmetic_test_impl<int, double, double>();
    xtgmath_arithmetic_test_impl<double, double, double>();
    xtgmath_arithmetic_test_impl<float, float, double>();
    xtgmath_arithmetic_test_impl<float, double, double>();
}

void xtr1common_test() {
    INSTANTIATE(std::integral_constant<int, 5>);
    INSTANTIATE(std::bool_constant<true>);
    INSTANTIATE(std::enable_if<true>);
    INSTANTIATE(std::enable_if<false>);
    INSTANTIATE(std::conditional_t<true, int, double>);
    INSTANTIATE(std::conditional_t<false, int, double>);
    // rest of traits in this header tested in type_traits_test:
    // is_same, remove_const, remove_volatile, remove_cv,
    // is_integral, is_floating_point, is_arithmetic,
    // remove_reference
}


int main()
{return 0;}