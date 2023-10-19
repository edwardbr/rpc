// xtr1common internal header (core)

// Copyright (c) Microsoft Corporation.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once
#ifndef _XTR1COMMON_
#define _XTR1COMMON_
#include <rpc/stl_clang12/rpc_yvals_core.h>
#if _STL_COMPILER_PREPROCESSOR

#if defined(__clang__) || defined(__GNUC__) || defined(__GNUG__)
#else
#pragma pack(push, _CRT_PACKING)
#pragma warning(push, _STL_WARNING_LEVEL)
#pragma warning(disable : _STL_DISABLED_WARNINGS)
#pragma push_macro("new")
#endif
#undef new

_RPC_BEGIN
template <class _Ty, _Ty _Val>
struct integral_constant {
    static constexpr _Ty value = _Val;

    using value_type = _Ty;
    using type       = integral_constant;

    constexpr operator value_type() const noexcept {
        return value;
    }

    _NODISCARD constexpr value_type operator()() const noexcept {
        return value;
    }
};

template <bool _Val>
using bool_constant = integral_constant<bool, _Val>;

using true_type  = bool_constant<true>;
using false_type = bool_constant<false>;

template <bool _Test, class _Ty = void>
struct enable_if {}; // no member "type" when !_Test

template <class _Ty>
struct enable_if<true, _Ty> { // type is _Ty for _Test
    using type = _Ty;
};

template <bool _Test, class _Ty = void>
using enable_if_t = typename enable_if<_Test, _Ty>::type;

template <bool _Test, class _Ty1, class _Ty2>
struct conditional { // Choose _Ty1 if _Test is true, and _Ty2 otherwise
    using type = _Ty1;
};

template <class _Ty1, class _Ty2>
struct conditional<false, _Ty1, _Ty2> {
    using type = _Ty2;
};

template <bool _Test, class _Ty1, class _Ty2>
using conditional_t = typename conditional<_Test, _Ty1, _Ty2>::type;

#ifdef __clang__
template <class _Ty1, class _Ty2>
_INLINE_VAR constexpr bool is_same_v = __is_same(_Ty1, _Ty2);

template <class _Ty1, class _Ty2>
struct is_same : bool_constant<__is_same(_Ty1, _Ty2)> {};
#else // ^^^ Clang / not Clang vvv
template <class, class>
_INLINE_VAR constexpr bool is_same_v = false; // determine whether arguments are the same type
template <class _Ty>
_INLINE_VAR constexpr bool is_same_v<_Ty, _Ty> = true;

template <class _Ty1, class _Ty2>
struct is_same : bool_constant<is_same_v<_Ty1, _Ty2>> {};
#endif // __clang__

template <class _Ty>
struct remove_const { // remove top-level const qualifier
    using type = _Ty;
};

template <class _Ty>
struct remove_const<const _Ty> {
    using type = _Ty;
};

template <class _Ty>
using remove_const_t = typename remove_const<_Ty>::type;

template <class _Ty>
struct remove_volatile { // remove top-level volatile qualifier
    using type = _Ty;
};

template <class _Ty>
struct remove_volatile<volatile _Ty> {
    using type = _Ty;
};

template <class _Ty>
using remove_volatile_t = typename remove_volatile<_Ty>::type;

template <class _Ty>
struct remove_cv { // remove top-level const and volatile qualifiers
    using type = _Ty;

    template <template <class> class _Fn>
    using _Apply = _Fn<_Ty>; // apply cv-qualifiers from the class template argument to _Fn<_Ty>
};

template <class _Ty>
struct remove_cv<const _Ty> {
    using type = _Ty;

    template <template <class> class _Fn>
    using _Apply = const _Fn<_Ty>;
};

template <class _Ty>
struct remove_cv<volatile _Ty> {
    using type = _Ty;

    template <template <class> class _Fn>
    using _Apply = volatile _Fn<_Ty>;
};

template <class _Ty>
struct remove_cv<const volatile _Ty> {
    using type = _Ty;

    template <template <class> class _Fn>
    using _Apply = const volatile _Fn<_Ty>;
};

template <class _Ty>
using remove_cv_t = typename remove_cv<_Ty>::type;

template <bool _First_value, class _First, class... _Rest>
struct _Disjunction { // handle true trait or last trait
    using type = _First;
};

template <class _False, class _Next, class... _Rest>
struct _Disjunction<false, _False, _Next, _Rest...> { // first trait is false, try the next trait
    using type = typename _Disjunction<_Next::value, _Next, _Rest...>::type;
};

template <class... _Traits>
struct disjunction : false_type {}; // If _Traits is empty, false_type

template <class _First, class... _Rest>
struct disjunction<_First, _Rest...> : _Disjunction<_First::value, _First, _Rest...>::type {
    // the first true trait in _Traits, or the last trait if none are true
};

template <class... _Traits>
_INLINE_VAR constexpr bool disjunction_v = disjunction<_Traits...>::value;

template <class _Ty, class... _Types>
_INLINE_VAR constexpr bool _Is_any_of_v = // true if and only if _Ty is in _Types
    disjunction_v<is_same<_Ty, _Types>...>;

#if _HAS_CXX20
_NODISCARD constexpr bool is_constant_evaluated() noexcept {
    return __builtin_is_constant_evaluated();
}
#endif // _HAS_CXX20

template <class _Ty>
_INLINE_VAR constexpr bool is_integral_v = _Is_any_of_v<remove_cv_t<_Ty>, bool, char, signed char, unsigned char,
    wchar_t,
#ifdef __cpp_char8_t
    char8_t,
#endif // __cpp_char8_t
    char16_t, char32_t, short, unsigned short, int, unsigned int, long, unsigned long, long long, unsigned long long>;

template <class _Ty>
struct is_integral : bool_constant<is_integral_v<_Ty>> {};

template <class _Ty>
_INLINE_VAR constexpr bool is_floating_point_v = _Is_any_of_v<remove_cv_t<_Ty>, float, double, long double>;

template <class _Ty>
struct is_floating_point : bool_constant<is_floating_point_v<_Ty>> {};

template <class _Ty>
_INLINE_VAR constexpr bool is_arithmetic_v = // determine whether _Ty is an arithmetic type
    is_integral_v<_Ty> || is_floating_point_v<_Ty>;

template <class _Ty>
struct is_arithmetic : bool_constant<is_arithmetic_v<_Ty>> {};

template <class _Ty>
struct remove_reference {
    using type                 = _Ty;
    using _Const_thru_ref_type = const _Ty;
};

template <class _Ty>
struct remove_reference<_Ty&> {
    using type                 = _Ty;
    using _Const_thru_ref_type = const _Ty&;
};

template <class _Ty>
struct remove_reference<_Ty&&> {
    using type                 = _Ty;
    using _Const_thru_ref_type = const _Ty&&;
};

template <class _Ty>
using remove_reference_t = typename remove_reference<_Ty>::type;

template <class _Ty>
using _Const_thru_ref = typename remove_reference<_Ty>::_Const_thru_ref_type;

template <class _Ty>
using _Remove_cvref_t _MSVC_KNOWN_SEMANTICS = remove_cv_t<remove_reference_t<_Ty>>;

#if _HAS_CXX20
template <class _Ty>
using remove_cvref_t = _Remove_cvref_t<_Ty>;

template <class _Ty>
struct remove_cvref {
    using type = remove_cvref_t<_Ty>;
};
#endif // _HAS_CXX20

_RPC_END
#if defined(__clang__) || defined(__GNUC__) || defined(__GNUG__)
#else
#pragma pop_macro("new")
_STL_RESTORE_CLANG_WARNINGS
#pragma warning(pop)
#pragma pack(pop)
#endif
#endif // _STL_COMPILER_PREPROCESSOR
#endif // _XTR1COMMON_
