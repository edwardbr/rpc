// xutility internal header

// Copyright (c) Microsoft Corporation.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once
#ifndef _XUTILITY_
#define _XUTILITY_
#include <rpc/stl_clang12/rpc_yvals.h>
#if _STL_COMPILER_PREPROCESSOR

#include <climits>
#include <cstdlib>
#include <cstring>
#include <utility>
#include <type_traits>
#include <functional>
#include <algorithm>

template <bool>
struct _Select { // Select between aliases that extract either their first or second parameter
    template <class _Ty1, class>
    using _Apply = _Ty1;
};

template <>
struct _Select<false> {
    template <class, class _Ty2>
    using _Apply = _Ty2;
};

#if defined(__clang__) || defined(__GNUC__) || defined(__GNUG__)
#pragma GCC diagnostic ignored "-Wc++20-extensions"
#else
#pragma pack(push, _CRT_PACKING)
#pragma warning(push, _STL_WARNING_LEVEL)
#pragma warning(disable : _STL_DISABLED_WARNINGS)
_STL_DISABLE_CLANG_WARNINGS
#pragma push_macro("new")
#endif
#undef new



#if (defined(_M_IX86) || defined(_M_X64)) && !defined(_M_CEE_PURE) && !defined(_M_HYBRID)
#ifndef _USE_STD_VECTOR_ALGORITHMS
#define _USE_STD_VECTOR_ALGORITHMS 1
#endif // _USE_STD_VECTOR_ALGORITHMS
#else // ^^^ arch supports vector algorithms / no support for vector algorithms vvv
#ifndef _USE_STD_VECTOR_ALGORITHMS
#define _USE_STD_VECTOR_ALGORITHMS 0
#elif _USE_STD_VECTOR_ALGORITHMS
#error Vector algorithms are not supported on this architecture, but _USE_STD_VECTOR_ALGORITHMS is set.
#endif // _USE_STD_VECTOR_ALGORITHMS
#endif // ^^^ no support for vector algorithms ^^^

#ifdef __CUDACC__
#define _CONSTEXPR_BIT_CAST inline
#else // ^^^ workaround ^^^ / vvv no workaround vvv
#define _CONSTEXPR_BIT_CAST constexpr
#endif // ^^^ no workaround ^^^

#if _USE_STD_VECTOR_ALGORITHMS
_EXTERN_C
// The "noalias" attribute tells the compiler optimizer that pointers going into these hand-vectorized algorithms
// won't be stored beyond the lifetime of the function, and that the function will only reference arrays denoted by
// those pointers. The optimizer also assumes in that case that a pointer parameter is not returned to the caller via
// the return value, so functions using "noalias" must usually return void. This attribute is valuable because these
// functions are in native code objects that the compiler cannot analyze. In the absence of the noalias attribute, the
// compiler has to assume that the denoted arrays are "globally address taken", and that any later calls to
// unanalyzable routines may modify those arrays.
_DECLSPEC_NOALIAS void __cdecl __std_reverse_trivially_swappable_1(void* _First, void* _Last) noexcept;
_DECLSPEC_NOALIAS void __cdecl __std_reverse_trivially_swappable_2(void* _First, void* _Last) noexcept;
_DECLSPEC_NOALIAS void __cdecl __std_reverse_trivially_swappable_4(void* _First, void* _Last) noexcept;
_DECLSPEC_NOALIAS void __cdecl __std_reverse_trivially_swappable_8(void* _First, void* _Last) noexcept;
_DECLSPEC_NOALIAS void __cdecl __std_swap_ranges_trivially_swappable_noalias(
    void* _First1, void* _Last1, void* _First2) noexcept;
_END_EXTERN_C
#endif // _USE_STD_VECTOR_ALGORITHMS

_RPC_BEGIN




namespace _Has_ADL_swap_detail {
    void swap(); // undefined (deliberate shadowing)

    template <class, class = void>
    struct _Has_ADL_swap : false_type {};
    template <class _Ty>
    struct _Has_ADL_swap<_Ty, ::std::void_t<decltype(swap(_STD declval<_Ty&>(), _STD declval<_Ty&>()))>> : true_type {};
} // namespace _Has_ADL_swap_detail
using _Has_ADL_swap_detail::_Has_ADL_swap;

template <class _Ty>
inline constexpr bool _Is_trivially_swappable_v = std::conjunction_v<::std::is_trivially_destructible<_Ty>,
    std::is_trivially_move_constructible<_Ty>, std::is_trivially_move_assignable<_Ty>, std::negation<_Has_ADL_swap<_Ty>>>;

#ifdef __cpp_lib_byte
template <>
inline constexpr bool _Is_trivially_swappable_v<::std::byte> = true;
#endif // __cpp_lib_byte

template <class _Ty>
struct _Is_trivially_swappable : bool_constant<_Is_trivially_swappable_v<_Ty>> {
    // true_type if and only if it is valid to swap two _Ty lvalues by exchanging object representations.
};


template <class _To, class _From,
    ::std::enable_if_t<::std::conjunction_v<::std::bool_constant<sizeof(_To) == sizeof(_From)>, ::std::is_trivially_copyable<_To>,
                    ::std::is_trivially_copyable<_From>>,
        int> = 0>
_NODISCARD _CONSTEXPR_BIT_CAST _To _Bit_cast(const _From& _Val) noexcept {
#ifdef __CUDACC__
    _To _To_obj; // assumes default-init
    _CSTD memcpy(_STD addressof(_To_obj), _STD addressof(_Val), sizeof(_To));
    return _To_obj;
#else // ^^^ workaround ^^^ / vvv no workaround vvv
    return __builtin_bit_cast(_To, _Val);
#endif // ^^^ no workaround ^^^
}

template <class _Ty>
struct _Get_first_parameter;

template <template <class, class...> class _Ty, class _First, class... _Rest>
struct _Get_first_parameter<_Ty<_First, _Rest...>> { // given _Ty<_First, _Rest...>, extract _First
    using type = _First;
};

template <class _Newfirst, class _Ty>
struct _Replace_first_parameter;

template <class _Newfirst, template <class, class...> class _Ty, class _First, class... _Rest>
struct _Replace_first_parameter<_Newfirst, _Ty<_First, _Rest...>> { // given _Ty<_First, _Rest...>, replace _First
    using type = _Ty<_Newfirst, _Rest...>;
};

template <class _Ty, class = void>
struct _Get_element_type {
    using type = typename _Get_first_parameter<_Ty>::type;
};

template <class _Ty>
struct _Get_element_type<_Ty, ::std::void_t<typename _Ty::element_type>> {
    using type = typename _Ty::element_type;
};

template <class _Ty, class = void>
struct _Get_ptr_difference_type {
    using type = ptrdiff_t;
};

template <class _Ty>
struct _Get_ptr_difference_type<_Ty, ::std::void_t<typename _Ty::difference_type>> {
    using type = typename _Ty::difference_type;
};

template <class _Ty, class _Other, class = void>
struct _Get_rebind_alias {
    using type = typename _Replace_first_parameter<_Other, _Ty>::type;
};

template <class _Ty, class _Other>
struct _Get_rebind_alias<_Ty, _Other, ::std::void_t<typename _Ty::template rebind<_Other>>> {
    using type = typename _Ty::template rebind<_Other>;
};

template <class _Iter>
_NODISCARD constexpr void* _Voidify_iter(_Iter _It) noexcept {
    if constexpr (::std::is_pointer_v<_Iter>) {
        return const_cast<void*>(static_cast<const volatile void*>(_It));
    } else {
        return const_cast<void*>(static_cast<const volatile void*>(_STD addressof(*_It)));
    }
}

#if _HAS_CXX20
template <class _Ty, class... _Types,
    class = ::std::void_t<decltype(::new (declval<void*>()) _Ty(declval<_Types>()...))>>
constexpr _Ty* construct_at(_Ty* const _Location, _Types&&... _Args) noexcept(
    noexcept(::new (_Voidify_iter(_Location)) _Ty(::std::forward<_Types>(_Args)...))) /* strengthened */ {
    return ::new (_Voidify_iter(_Location)) _Ty(::std::forward<_Types>(_Args)...);
}
#endif // _HAS_CXX20

template <class _Ty, class... _Types>
_CONSTEXPR20 void _Construct_in_place(_Ty& _Obj, _Types&&... _Args) noexcept(
    ::std::is_nothrow_constructible_v<_Ty, _Types...>) {
#if _HAS_CXX20
    if (_RPC is_constant_evaluated()) {
        _RPC construct_at(_STD addressof(_Obj), ::std::forward<_Types>(_Args)...);
    } else
#endif // _HAS_CXX20
    {
        ::new (_Voidify_iter(_STD addressof(_Obj))) _Ty(::std::forward<_Types>(_Args)...);
    }
}

template <class _Ty>
void _Default_construct_in_place(_Ty& _Obj) noexcept(::std::is_nothrow_default_constructible_v<_Ty>) {
    ::new (_Voidify_iter(_STD addressof(_Obj))) _Ty;
}

template <class _Ty>
struct pointer_traits {
    using pointer         = _Ty;
    using element_type    = typename _Get_element_type<_Ty>::type;
    using difference_type = typename _Get_ptr_difference_type<_Ty>::type;

    template <class _Other>
    using rebind = typename _Get_rebind_alias<_Ty, _Other>::type;

    using _Reftype = ::std::conditional_t<::std::is_void_v<element_type>, char, element_type>&;

    _NODISCARD static pointer pointer_to(_Reftype _Val) noexcept(noexcept(_Ty::pointer_to(_Val))) /* strengthened */ {
        return _Ty::pointer_to(_Val);
    }
};

template <class _Ty>
struct pointer_traits<_Ty*> {
    using pointer         = _Ty*;
    using element_type    = _Ty;
    using difference_type = ptrdiff_t;

    template <class _Other>
    using rebind = _Other*;

    using _Reftype = ::std::conditional_t<::std::is_void_v<_Ty>, char, _Ty>&;

    _NODISCARD static _CONSTEXPR20 pointer pointer_to(_Reftype _Val) noexcept {
        return _STD addressof(_Val);
    }
};

#if _HAS_CXX20
template <class _Ty, class = void>
inline constexpr bool _Has_to_address_v = false; // determines whether _Ptr has pointer_traits<_Ptr>::to_address(p)

template <class _Ty>
inline constexpr bool
    _Has_to_address_v<_Ty, ::std::void_t<decltype(pointer_traits<_Ty>::to_address(declval<const _Ty&>()))>> = true;

template <class _Ty>
_NODISCARD constexpr _Ty* to_address(_Ty* const _Val) noexcept {
    static_assert(!is_function_v<_Ty>,
        "N4810 20.10.4 [pointer.conversion]/2: The program is ill-formed if T is a function type.");
    return _Val;
}

template <class _Ptr>
_NODISCARD constexpr auto to_address(const _Ptr& _Val) noexcept {
    if constexpr (_Has_to_address_v<_Ptr>) {
        return pointer_traits<_Ptr>::to_address(_Val);
    } else {
        return _RPC to_address(_Val.operator->()); // plain pointer overload must come first
    }
}

struct identity {
    template <class _Ty>
    _NODISCARD constexpr _Ty&& operator()(_Ty&& _Left) const noexcept {
        return ::std::forward<_Ty>(_Left);
    }

    using is_transparent = int;
};
#endif // _HAS_CXX20

// TRANSITION, VSO-386225
template <class _Fx>
struct _Ref_fn { // pass function object by value as a reference
    template <class... _Args>
    constexpr decltype(auto) operator()(_Args&&... _Vals) { // forward function call operator
        if constexpr (::std::is_member_pointer_v<_Fx>) {
            return ::std::invoke(_Fn, ::std::forward<_Args>(_Vals)...);
        } else {
            return _Fn(::std::forward<_Args>(_Vals)...);
        }
    }

    _Fx& _Fn;
};

template <class _Fn>
_INLINE_VAR constexpr bool _Pass_functor_by_value_v = ::std::conjunction_v<::std::bool_constant<sizeof(_Fn) <= sizeof(void*)>,
    ::std::is_trivially_copy_constructible<_Fn>, ::std::is_trivially_destructible<_Fn>>;

template <class _Fn, ::std::enable_if_t<_Pass_functor_by_value_v<_Fn>, int> = 0> // TRANSITION, if constexpr
constexpr _Fn _Pass_fn(_Fn _Val) { // pass functor by value
    return _Val;
}

template <class _Fn, ::std::enable_if_t<!_Pass_functor_by_value_v<_Fn>, int> = 0>
constexpr _Ref_fn<_Fn> _Pass_fn(_Fn& _Val) { // pass functor by "reference"
    return {_Val};
}

struct _Unused_parameter { // generic unused parameter struct
    constexpr _Unused_parameter() noexcept = default;
    template <class _Ty>
    constexpr _Unused_parameter(_Ty&&) noexcept {}
};

using _Any_tag = _Unused_parameter; // generic fallback/default/"other" target for tag dispatch

template <class _Ty>
using _Algorithm_int_t = ::std::conditional_t<::std::is_integral_v<_Ty>, _Ty, ptrdiff_t>;

// from <iterator>
struct input_iterator_tag {};

struct output_iterator_tag {};

struct forward_iterator_tag : input_iterator_tag {};

struct bidirectional_iterator_tag : forward_iterator_tag {};

struct random_access_iterator_tag : bidirectional_iterator_tag {};

#ifdef __cpp_lib_concepts
struct contiguous_iterator_tag : random_access_iterator_tag {};

template <class _Ty>
using _With_reference = _Ty&;

template <class _Ty>
concept _Can_reference = requires {
    typename _With_reference<_Ty>;
};

template <class _Ty>
concept _Dereferenceable = requires(_Ty& __t) {
    { *__t } -> _Can_reference;
};

template <class _Ty>
concept _Has_member_iterator_concept = requires {
    typename _Ty::iterator_concept;
};

template <class _Ty>
concept _Has_member_iterator_category = requires {
    typename _Ty::iterator_category;
};

template <class _Ty>
concept _Has_member_value_type = requires {
    typename _Ty::value_type;
};

template <class _Ty>
concept _Has_member_element_type = requires {
    typename _Ty::element_type;
};

template <class _Ty>
concept _Has_member_difference_type = requires {
    typename _Ty::difference_type;
};

template <class _Ty>
concept _Has_member_pointer = requires {
    typename _Ty::pointer;
};

template <class _Ty>
concept _Has_member_reference = requires {
    typename _Ty::reference;
};

template <class _Ty>
concept _Destructible_object = ::std::is_object_v<_Ty> && destructible<_Ty>;

template <class>
struct incrementable_traits {};

// clang-format off
template <class _Ty>
    requires ::std::is_object_v<_Ty>
struct incrementable_traits<_Ty*> {
    using difference_type = ptrdiff_t;
};
// clang-format on

template <class _Ty>
struct incrementable_traits<const _Ty> : incrementable_traits<_Ty> {};

template <_Has_member_difference_type _Ty>
struct incrementable_traits<_Ty> {
    using difference_type = typename _Ty::difference_type;
};

template <class _Ty>
concept _Can_difference = requires(const _Ty& __a, const _Ty& __b) {
    { __a - __b } -> integral;
};

// clang-format off
template <class _Ty>
    requires (!_Has_member_difference_type<_Ty> && _Can_difference<_Ty>)
struct incrementable_traits<_Ty> {
    using difference_type = make_signed_t<decltype(declval<_Ty>() - declval<_Ty>())>;
};
// clang-format on

template <class _Ty>
concept _Is_from_primary = _Same_impl<typename _Ty::_From_primary, _Ty>;

template <class>
struct iterator_traits;

template <class _Ty>
using iter_difference_t = typename ::std::conditional_t<_Is_from_primary<iterator_traits<remove_cvref_t<_Ty>>>,
    incrementable_traits<remove_cvref_t<_Ty>>, iterator_traits<remove_cvref_t<_Ty>>>::difference_type;

template <class>
struct _Cond_value_type {};

// clang-format off
template <class _Ty>
    requires ::std::is_object_v<_Ty>
struct _Cond_value_type<_Ty> {
    using value_type = ::std::remove_cv_t<_Ty>;
};
// clang-format on

template <class>
struct indirectly_readable_traits {};

template <class _Ty>
struct indirectly_readable_traits<_Ty*> : _Cond_value_type<_Ty> {};

// clang-format off
template <class _Ty>
    requires ::std::is_array_v<_Ty>
struct indirectly_readable_traits<_Ty> {
    using value_type = ::std::remove_cv_t<::std::remove_extent_t<_Ty>>;
};
// clang-format on

template <class _Ty>
struct indirectly_readable_traits<const _Ty> : indirectly_readable_traits<_Ty> {};

template <_Has_member_value_type _Ty>
struct indirectly_readable_traits<_Ty> : _Cond_value_type<typename _Ty::value_type> {};

template <_Has_member_element_type _Ty>
struct indirectly_readable_traits<_Ty> : _Cond_value_type<typename _Ty::element_type> {};

// clang-format off
template <_Has_member_value_type _Ty>
    requires _Has_member_element_type<_Ty>
struct indirectly_readable_traits<_Ty> {};

template <_Has_member_value_type _Ty>
    requires _Has_member_element_type<_Ty>
        && same_as<::std::remove_cv_t<typename _Ty::value_type>, ::std::remove_cv_t<typename _Ty::element_type>>
struct indirectly_readable_traits<_Ty> : _Cond_value_type<typename _Ty::value_type> {};
// clang-format on

template <class _Ty>
using iter_value_t = typename ::std::conditional_t<_Is_from_primary<iterator_traits<remove_cvref_t<_Ty>>>,
    indirectly_readable_traits<remove_cvref_t<_Ty>>, iterator_traits<remove_cvref_t<_Ty>>>::value_type;

template <_Dereferenceable _Ty>
using iter_reference_t = decltype(*declval<_Ty&>());

template <class>
struct _Iterator_traits_base {};

template <class _It>
concept _Has_iter_types = _Has_member_difference_type<_It> && _Has_member_value_type<_It> //
    && _Has_member_reference<_It> && _Has_member_iterator_category<_It>;

template <bool _Has_member_typedef>
struct _Old_iter_traits_pointer {
    template <class _It>
    using _Apply = typename _It::pointer;
};

template <>
struct _Old_iter_traits_pointer<false> {
    template <class>
    using _Apply = void;
};

template <_Has_iter_types _It>
struct _Iterator_traits_base<_It> {
    using iterator_category = typename _It::iterator_category;
    using value_type        = typename _It::value_type;
    using difference_type   = typename _It::difference_type;
    using pointer           = typename _Old_iter_traits_pointer<_Has_member_pointer<_It>>::template _Apply<_It>;
    using reference         = typename _It::reference;
};

template <bool _Has_member_typedef>
struct _Iter_traits_difference {
    template <class _It>
    using _Apply = typename incrementable_traits<_It>::difference_type;
};

template <>
struct _Iter_traits_difference<false> {
    template <class>
    using _Apply = void;
};

// clang-format off
template <class _It>
concept _Cpp17_iterator =
    requires(_It __i) {
        { *__i } -> _Can_reference;
        { ++__i } -> same_as<_It&>;
        { *__i++ } -> _Can_reference;
    }
    && copyable<_It>;

template <class _It>
concept _Cpp17_input_iterator = _Cpp17_iterator<_It>
    && equality_comparable<_It>
    && _Has_member_difference_type<incrementable_traits<_It>>
    && _Has_member_value_type<indirectly_readable_traits<_It>>
    && requires(_It __i) {
        typename common_reference_t<iter_reference_t<_It>&&, typename indirectly_readable_traits<_It>::value_type&>;
        typename common_reference_t<decltype(*__i++)&&, typename indirectly_readable_traits<_It>::value_type&>;
        requires signed_integral<typename incrementable_traits<_It>::difference_type>;
    };

template <class _It>
    requires (!_Has_iter_types<_It> && _Cpp17_iterator<_It> && !_Cpp17_input_iterator<_It>)
struct _Iterator_traits_base<_It> {
    using iterator_category = output_iterator_tag;
    using value_type = void;
    using difference_type =
        typename _Iter_traits_difference<_Has_member_difference_type<incrementable_traits<_It>>>::template _Apply<_It>;
    using pointer    = void;
    using reference  = void;
};
// clang-format on

enum class _Itraits_pointer_strategy { _Use_void, _Use_member, _Use_decltype };

template <_Itraits_pointer_strategy>
struct _Iter_traits_pointer;

template <>
struct _Iter_traits_pointer<_Itraits_pointer_strategy::_Use_void> {
    template <class>
    using _Apply = void;
};

template <>
struct _Iter_traits_pointer<_Itraits_pointer_strategy::_Use_member> {
    template <class _It>
    using _Apply = typename _It::pointer;
};

template <>
struct _Iter_traits_pointer<_Itraits_pointer_strategy::_Use_decltype> {
    template <class _It>
    using _Apply = decltype(declval<_It&>().operator->());
};

template <class _Ty>
concept _Has_member_arrow = requires(_Ty&& __t) {
    static_cast<_Ty&&>(__t).operator->();
};

template <bool _Has_member_typedef>
struct _Iter_traits_reference {
    template <class _It>
    using _Apply = typename _It::reference;
};

template <>
struct _Iter_traits_reference<false> {
    template <class _It>
    using _Apply = iter_reference_t<_It>;
};

template <bool _Is_random>
struct _Iter_traits_category4 {
    using type = random_access_iterator_tag;
};

template <>
struct _Iter_traits_category4<false> {
    using type = bidirectional_iterator_tag;
};

// clang-format off
template <class _It>
concept _Cpp17_random_delta = totally_ordered<_It>
    && requires(_It __i, typename incrementable_traits<_It>::difference_type __n) {
        { __i += __n } -> same_as<_It&>;
        { __i -= __n } -> same_as<_It&>;
        { __i +  __n } -> same_as<_It>;
        { __n +  __i } -> same_as<_It>;
        { __i -  __n } -> same_as<_It>;
        { __i -  __i } -> same_as<decltype(__n)>;
        {  __i[__n]  } -> convertible_to<iter_reference_t<_It>>;
    };
// clang-format on

template <bool _Is_bidi>
struct _Iter_traits_category3 {
    template <class _It>
    using _Apply = typename _Iter_traits_category4<_Cpp17_random_delta<_It>>::type;
};

template <>
struct _Iter_traits_category3<false> {
    template <class>
    using _Apply = forward_iterator_tag;
};

template <class _It>
concept _Cpp17_bidi_delta = requires(_It __i) {
    { --__i } -> same_as<_It&>;
    { __i-- } -> convertible_to<const _It&>;
    requires same_as<decltype(*__i--), iter_reference_t<_It>>;
};

template <bool _Is_forward>
struct _Iter_traits_category2 {
    template <class _It>
    using _Apply = typename _Iter_traits_category3<_Cpp17_bidi_delta<_It>>::template _Apply<_It>;
};

template <>
struct _Iter_traits_category2<false> {
    template <class>
    using _Apply = input_iterator_tag;
};

// clang-format off
template <class _It>
concept _Cpp17_forward_delta = constructible_from<_It> && is_lvalue_reference_v<iter_reference_t<_It>>
    && same_as<remove_cvref_t<iter_reference_t<_It>>, typename indirectly_readable_traits<_It>::value_type>
    && requires(_It __i) {
        { __i++ } -> convertible_to<const _It&>;
        requires same_as<decltype(*__i++), iter_reference_t<_It>>;
    };
// clang-format on

template <bool _Has_member_typedef>
struct _Iter_traits_category {
    template <class _It>
    using _Apply = typename _It::iterator_category;
};

template <>
struct _Iter_traits_category<false> {
    template <class _It>
    using _Apply = typename _Iter_traits_category2<_Cpp17_forward_delta<_It>>::template _Apply<_It>;
};

// clang-format off
template <class _It>
    requires (!_Has_iter_types<_It> && _Cpp17_input_iterator<_It>)
struct _Iterator_traits_base<_It> {
    using iterator_category = typename _Iter_traits_category<_Has_member_iterator_category<_It>>::template _Apply<_It>;
    using value_type        = typename indirectly_readable_traits<_It>::value_type;
    using difference_type   = typename incrementable_traits<_It>::difference_type;
    using pointer           = typename _Iter_traits_pointer<(
        _Has_member_pointer<_It> ? _Itraits_pointer_strategy::_Use_member
                                 : _Has_member_arrow<_It&> ? _Itraits_pointer_strategy::_Use_decltype
                                                       : _Itraits_pointer_strategy::_Use_void)>::template _Apply<_It>;
    using reference         = typename _Iter_traits_reference<_Has_member_reference<_It>>::template _Apply<_It>;
};
// clang-format on

template <class _Ty>
struct iterator_traits : _Iterator_traits_base<_Ty> {
    using _From_primary = iterator_traits;
};

// clang-format off
template <class _Ty>
    requires ::std::is_object_v<_Ty>
struct iterator_traits<_Ty*> {
    // clang-format on
    using iterator_concept  = contiguous_iterator_tag;
    using iterator_category = random_access_iterator_tag;
    using value_type        = ::std::remove_cv_t<_Ty>;
    using difference_type   = ptrdiff_t;
    using pointer           = _Ty*;
    using reference         = _Ty&;
};

namespace ranges {
    namespace _Iter_move {
        void iter_move(); // Block unqualified name lookup

        // clang-format off
        template <class _Ty>
        concept _Has_ADL = _Has_class_or_enum_type<_Ty> && requires(_Ty&& __t) {
            iter_move(static_cast<_Ty&&>(__t));
        };

        template <class _Ty>
        concept _Can_deref = requires(_Ty&& __t) {
            *static_cast<_Ty&&>(__t);
        };

        class _Cpo {
        private:
            enum class _St { _None, _Custom, _Fallback };

            template <class _Ty>
            _NODISCARD static _CONSTEVAL _Choice_t<_St> _Choose() noexcept {
                if constexpr (_Has_ADL<_Ty>) {
                    return {_St::_Custom, noexcept(iter_move(declval<_Ty>()))};
                } else if constexpr (_Can_deref<_Ty>) {
                    return {_St::_Fallback, noexcept(*declval<_Ty>())};
                } else {
                    return {_St::_None};
                }
            }

            template <class _Ty>
            static constexpr _Choice_t<_St> _Choice = _Choose<_Ty>();

        public:
            template <class _Ty>
                requires (_Choice<_Ty>._Strategy != _St::_None)
            _NODISCARD constexpr decltype(auto) operator()(_Ty&& _Val) const noexcept(_Choice<_Ty>._No_throw) {
                constexpr _St _Strat = _Choice<_Ty>._Strategy;

                if constexpr (_Strat == _St::_Custom) {
                    return iter_move(static_cast<_Ty&&>(_Val));
                } else if constexpr (_Strat == _St::_Fallback) {
                    using _Ref = decltype(*static_cast<_Ty&&>(_Val));
                    if constexpr (is_lvalue_reference_v<_Ref>) {
                        return ::std::move(*static_cast<_Ty&&>(_Val));
                    } else {
                        return *static_cast<_Ty&&>(_Val);
                    }
                } else {
                    static_assert(_Always_false<_Ty>, "should be unreachable");
                }
            }
        };
        // clang-format on
    } // namespace _Iter_move

    inline namespace _Cpos {
        inline constexpr _Iter_move::_Cpo iter_move;
    }
} // namespace ranges

// iter_swap defined below since it depends on indirectly_movable_storable

// clang-format off
template <class _Ty>
    requires _Dereferenceable<_Ty> && requires(_Ty& __t) {
        { _RANGES iter_move(__t) } -> _Can_reference;
    }
using iter_rvalue_reference_t = decltype(_RANGES iter_move(declval<_Ty&>()));

template <class _It>
concept _Indirectly_readable_impl = requires(const _It __i) {
    typename iter_value_t<_It>;
    typename iter_reference_t<_It>;
    typename iter_rvalue_reference_t<_It>;
    { *__i } -> same_as<iter_reference_t<_It>>;
    { _RANGES iter_move(__i) } -> same_as<iter_rvalue_reference_t<_It>>;
} && common_reference_with<iter_reference_t<_It>&&, iter_value_t<_It>&>
  && common_reference_with<iter_reference_t<_It>&&, iter_rvalue_reference_t<_It>&&>
  && common_reference_with<iter_rvalue_reference_t<_It>&&, const iter_value_t<_It>&>;

template <class _It>
concept indirectly_readable = _Indirectly_readable_impl<remove_cvref_t<_It>>;
// clang-format on

template <indirectly_readable _Ty>
using iter_common_reference_t = common_reference_t<iter_reference_t<_Ty>, iter_value_t<_Ty>&>;

template <class _It, class _Ty>
concept indirectly_writable = requires(_It&& __i, _Ty&& __t) {
    *__i                                                                = static_cast<_Ty&&>(__t);
    *static_cast<_It&&>(__i)                                            = static_cast<_Ty&&>(__t);
    const_cast<const iter_reference_t<_It>&&>(*__i)                     = static_cast<_Ty&&>(__t);
    const_cast<const iter_reference_t<_It>&&>(*static_cast<_It&&>(__i)) = static_cast<_Ty&&>(__t);
};

template <class _Ty>
concept _Integer_like = _Is_nonbool_integral<::std::remove_cv_t<_Ty>>;

template <class _Ty>
concept _Signed_integer_like = _Integer_like<_Ty> && static_cast<_Ty>(-1) < static_cast<_Ty>(0);

template <class _Ty>
using _Make_unsigned_like_t = ::std::make_unsigned_t<_Ty>;

template <_Integer_like _Ty>
_NODISCARD constexpr auto _To_unsigned_like(const _Ty _Value) noexcept {
    return static_cast<_Make_unsigned_like_t<_Ty>>(_Value);
}

template <class _Ty>
using _Make_signed_like_t = make_signed_t<_Ty>;

template <_Integer_like _Ty>
_NODISCARD constexpr auto _To_signed_like(const _Ty _Value) noexcept {
    return static_cast<_Make_signed_like_t<_Ty>>(_Value);
}

// clang-format off
template <class _Ty>
concept weakly_incrementable = movable<_Ty> && requires(_Ty __i) {
    typename iter_difference_t<_Ty>;
    requires _Signed_integer_like<iter_difference_t<_Ty>>;
    { ++__i } -> same_as<_Ty&>;
    __i++;
};

template <class _Ty>
concept incrementable = regular<_Ty> && weakly_incrementable<_Ty> && requires(_Ty __t) {
    { __t++ } -> same_as<_Ty>;
};

template <class _It>
concept input_or_output_iterator = requires(_It __i) {
    { *__i } -> _Can_reference;
    requires weakly_incrementable<_It>;
};

template <class _Se, class _It>
concept sentinel_for = semiregular<_Se>
    && input_or_output_iterator<_It>
    && _Weakly_equality_comparable_with<_Se, _It>;
// clang-format on

template <class _Se, class _It>
inline constexpr bool disable_sized_sentinel_for = false;

// clang-format off
template <class _Se, class _It>
concept sized_sentinel_for = sentinel_for<_Se, _It>
    && !disable_sized_sentinel_for<::std::remove_cv_t<_Se>, ::std::remove_cv_t<_It>>
    && requires(const _It& __i, const _Se& __s) {
        { __s - __i } -> same_as<iter_difference_t<_It>>;
        { __i - __s } -> same_as<iter_difference_t<_It>>;
    };
// clang-format on

template <bool _Iterator_category_present>
struct _Iter_concept_impl2 {
    template <class _It, class _Traits>
    using _Apply = typename _Traits::iterator_category;
};
template <>
struct _Iter_concept_impl2<false> {
    // clang-format off
    template <class _It, class _Traits>
        requires _Is_from_primary<iterator_traits<_It>>
    using _Apply = random_access_iterator_tag;
    // clang-format on
};

template <bool _Iterator_concept_present>
struct _Iter_concept_impl1 {
    template <class _It, class _Traits>
    using _Apply = typename _Traits::iterator_concept;
};
template <>
struct _Iter_concept_impl1<false> {
    template <class _It, class _Traits>
    using _Apply = typename _Iter_concept_impl2<_Has_member_iterator_category<_Traits>>::template _Apply<_It, _Traits>;
};

template <class _It, class _Traits = ::std::conditional_t<_Is_from_primary<iterator_traits<_It>>, _It, iterator_traits<_It>>>
using _Iter_concept =
    typename _Iter_concept_impl1<_Has_member_iterator_concept<_Traits>>::template _Apply<_It, _Traits>;

// clang-format off
template <class _It>
concept input_iterator = input_or_output_iterator<_It> && indirectly_readable<_It>
    && requires { typename _Iter_concept<_It>; }
    && derived_from<_Iter_concept<_It>, input_iterator_tag>;

template <class _It, class _Ty>
concept output_iterator = input_or_output_iterator<_It> && indirectly_writable<_It, _Ty>
    && requires(_It __i, _Ty&& __t) {
        *__i++ = static_cast<_Ty&&>(__t);
    };

template <class _It>
concept forward_iterator = input_iterator<_It> && derived_from<_Iter_concept<_It>, forward_iterator_tag>
    && incrementable<_It> && sentinel_for<_It, _It>;

template <class _It>
concept bidirectional_iterator = forward_iterator<_It> && derived_from<_Iter_concept<_It>, bidirectional_iterator_tag>
    && requires(_It __i) {
        { --__i } -> same_as<_It&>;
        { __i-- } -> same_as<_It>;
    };

template <class _It>
concept random_access_iterator = bidirectional_iterator<_It>
    && derived_from<_Iter_concept<_It>, random_access_iterator_tag> && totally_ordered<_It>
    && sized_sentinel_for<_It, _It> && requires(_It __i, const _It __j, const iter_difference_t<_It> __n) {
        { __i += __n } -> same_as<_It&>;
        { __j + __n } -> same_as<_It>;
        { __n + __j } -> same_as<_It>;
        { __i -= __n } -> same_as<_It&>;
        { __j - __n } -> same_as<_It>;
        { __j[__n] } -> same_as<iter_reference_t<_It>>;
    };

template <class _It>
concept contiguous_iterator = random_access_iterator<_It>
    && derived_from<_Iter_concept<_It>, contiguous_iterator_tag>
    && is_lvalue_reference_v<iter_reference_t<_It>>
    && same_as<iter_value_t<_It>, remove_cvref_t<iter_reference_t<_It>>>
    && requires(const _It& __i) {
        { _RPC to_address(__i) } -> same_as<add_pointer_t<iter_reference_t<_It>>>;
    };

template <class _Fn, class _It>
concept indirectly_unary_invocable = indirectly_readable<_It>
    && copy_constructible<_Fn>
    && invocable<_Fn&, iter_value_t<_It>&>
    && invocable<_Fn&, iter_reference_t<_It>>
    && invocable<_Fn&, iter_common_reference_t<_It>>
    && common_reference_with<
        invoke_result_t<_Fn&, iter_value_t<_It>&>,
        invoke_result_t<_Fn&, iter_reference_t<_It>>>;

template <class _Fn, class _It>
concept indirectly_regular_unary_invocable = indirectly_readable<_It>
    && copy_constructible<_Fn>
    && regular_invocable<_Fn&, iter_value_t<_It>&>
    && regular_invocable<_Fn&, iter_reference_t<_It>>
    && regular_invocable<_Fn&, iter_common_reference_t<_It>>
    && common_reference_with<
        invoke_result_t<_Fn&, iter_value_t<_It>&>,
        invoke_result_t<_Fn&, iter_reference_t<_It>>>;

template <class _Fn, class _It>
concept indirect_unary_predicate = indirectly_readable<_It>
    && copy_constructible<_Fn>
    && predicate<_Fn&, iter_value_t<_It>&>
    && predicate<_Fn&, iter_reference_t<_It>>
    && predicate<_Fn&, iter_common_reference_t<_It>>;

template <class _Fn, class _It1, class _It2>
concept indirect_binary_predicate = indirectly_readable<_It1>
    && indirectly_readable<_It2>
    && copy_constructible<_Fn>
    && predicate<_Fn&, iter_value_t<_It1>&, iter_value_t<_It2>&>
    && predicate<_Fn&, iter_value_t<_It1>&, iter_reference_t<_It2>>
    && predicate<_Fn&, iter_reference_t<_It1>, iter_value_t<_It2>&>
    && predicate<_Fn&, iter_reference_t<_It1>, iter_reference_t<_It2>>
    && predicate<_Fn&, iter_common_reference_t<_It1>, iter_common_reference_t<_It2>>;

template <class _Fn, class _It1, class _It2 = _It1>
concept indirect_equivalence_relation = indirectly_readable<_It1>
    && indirectly_readable<_It2>
    && copy_constructible<_Fn>
    && equivalence_relation<_Fn&, iter_value_t<_It1>&, iter_value_t<_It2>&>
    && equivalence_relation<_Fn&, iter_value_t<_It1>&, iter_reference_t<_It2>>
    && equivalence_relation<_Fn&, iter_reference_t<_It1>, iter_value_t<_It2>&>
    && equivalence_relation<_Fn&, iter_reference_t<_It1>, iter_reference_t<_It2>>
    && equivalence_relation<_Fn&, iter_common_reference_t<_It1>, iter_common_reference_t<_It2>>;

template <class _Fn, class _It1, class _It2 = _It1>
concept indirect_strict_weak_order = indirectly_readable<_It1>
    && indirectly_readable<_It2>
    && copy_constructible<_Fn>
    && strict_weak_order<_Fn&, iter_value_t<_It1>&, iter_value_t<_It2>&>
    && strict_weak_order<_Fn&, iter_value_t<_It1>&, iter_reference_t<_It2>>
    && strict_weak_order<_Fn&, iter_reference_t<_It1>, iter_value_t<_It2>&>
    && strict_weak_order<_Fn&, iter_reference_t<_It1>, iter_reference_t<_It2>>
    && strict_weak_order<_Fn&, iter_common_reference_t<_It1>, iter_common_reference_t<_It2>>;

template <class _Fn, class... _Its>
    requires (indirectly_readable<_Its> && ...)
        && invocable<_Fn, iter_reference_t<_Its>...>
using indirect_result_t = invoke_result_t<_Fn, iter_reference_t<_Its>...>;
// clang-format on

#pragma warning(push)
#pragma warning(disable : 5046) // '%s': Symbol involving type with internal linkage not defined
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wundefined-internal" // function '%s' has internal linkage but is not defined
#endif // __clang__

template <indirectly_readable _It, indirectly_regular_unary_invocable<_It> _Proj>
struct projected {
    using value_type = remove_cvref_t<indirect_result_t<_Proj&, _It>>;
#if defined(__clang__) || defined(__EDG__)
    indirect_result_t<_Proj&, _It> operator*() const;
#else // ^^^ no workaround / workaround vvv
    indirect_result_t<_Proj&, _It> operator*() const {
        _CSTD abort(); // TRANSITION, VSO-1308657
    }
#endif // ^^^ workaround ^^^
};

#ifdef __clang__
#pragma clang diagnostic pop
#endif // __clang__
#pragma warning(pop)

template <weakly_incrementable _It, class _Proj>
struct incrementable_traits<projected<_It, _Proj>> {
    using difference_type = iter_difference_t<_It>;
};

template <class _In, class _Out>
concept indirectly_movable = indirectly_readable<_In> && indirectly_writable<_Out, iter_rvalue_reference_t<_In>>;

// clang-format off
template <class _In, class _Out>
concept indirectly_movable_storable = indirectly_movable<_In, _Out>
    && indirectly_writable<_Out, iter_value_t<_In>>
    && movable<iter_value_t<_In>>
    && constructible_from<iter_value_t<_In>, iter_rvalue_reference_t<_In>>
    && assignable_from<iter_value_t<_In>&, iter_rvalue_reference_t<_In> >;
// clang-format on

template <class _In, class _Out>
concept indirectly_copyable = indirectly_readable<_In> && indirectly_writable<_Out, iter_reference_t<_In>>;

// clang-format off
template <class _In, class _Out>
concept indirectly_copyable_storable = indirectly_copyable<_In, _Out>
    && indirectly_writable<_Out, iter_value_t<_In>&>
    && indirectly_writable<_Out, const iter_value_t<_In>&>
    && indirectly_writable<_Out, iter_value_t<_In>&&>
    && indirectly_writable<_Out, const iter_value_t<_In>&&>
    && copyable<iter_value_t<_In>>
    && constructible_from<iter_value_t<_In>, iter_reference_t<_In>>
    && assignable_from<iter_value_t<_In>&, iter_reference_t<_In>>;
// clang-format on

namespace ranges {
    namespace _Iter_swap {
        template <class _Ty1, class _Ty2>
        void iter_swap(_Ty1, _Ty2) = delete;

        // clang-format off
        template <class _Ty1, class _Ty2>
        concept _Has_ADL = (_Has_class_or_enum_type<_Ty1> || _Has_class_or_enum_type<_Ty2>)
            && requires(_Ty1&& __t1, _Ty2&& __t2) {
                iter_swap(static_cast<_Ty1&&>(__t1), static_cast<_Ty2&&>(__t2));
            };

        template <class _Ty1, class _Ty2>
        concept _Can_swap_references = indirectly_readable<remove_reference_t<_Ty1>>
            && indirectly_readable<remove_reference_t<_Ty2>>
            && swappable_with<iter_reference_t<_Ty1>, iter_reference_t<_Ty2>>;

        template <class _Ty1, class _Ty2>
        concept _Symmetric_indirectly_movable_storable =
               indirectly_movable_storable<remove_reference_t<_Ty1>, remove_reference_t<_Ty2>>
            && indirectly_movable_storable<remove_reference_t<_Ty2>, remove_reference_t<_Ty1>>;
        // clang-format on

        template <class _Xty, class _Yty>
        _NODISCARD constexpr iter_value_t<remove_reference_t<_Xty>> _Iter_exchange_move(_Xty&& _XVal,
            _Yty&& _YVal) noexcept(noexcept(iter_value_t<remove_reference_t<_Xty>>(_RANGES iter_move(_XVal)))) {
            iter_value_t<remove_reference_t<_Xty>> _Tmp(_RANGES iter_move(_XVal));
            *_XVal = _RANGES iter_move(_YVal);
            return _Tmp;
        }

        class _Cpo {
        private:
            enum class _St { _None, _Custom, _Swap, _Exchange };

            template <class _Ty1, class _Ty2>
            _NODISCARD static _CONSTEVAL _Choice_t<_St> _Choose() noexcept {
                if constexpr (_Has_ADL<_Ty1, _Ty2>) {
                    return {_St::_Custom, noexcept(iter_swap(declval<_Ty1>(), declval<_Ty2>()))};
                } else if constexpr (_Can_swap_references<_Ty1, _Ty2>) {
                    return {_St::_Swap, noexcept(_RANGES swap(*declval<_Ty1>(), *declval<_Ty2>()))};
                } else if constexpr (_Symmetric_indirectly_movable_storable<_Ty1, _Ty2>) {
                    constexpr auto _Nothrow = noexcept(
                        *declval<_Ty1>() = _Iter_exchange_move(declval<_Ty2>(), declval<_Ty1>()));
                    return {_St::_Exchange, _Nothrow};
                } else {
                    return {_St::_None};
                }
            }

            template <class _Ty1, class _Ty2>
            static constexpr _Choice_t<_St> _Choice = _Choose<_Ty1, _Ty2>();

        public:
            // clang-format off
            template <class _Ty1, class _Ty2>
                requires (_Choice<_Ty1, _Ty2>._Strategy != _St::_None)
            constexpr void operator()(_Ty1&& _Val1, _Ty2&& _Val2) const noexcept(_Choice<_Ty1, _Ty2>._No_throw) {
                constexpr _St _Strat = _Choice<_Ty1, _Ty2>._Strategy;

                if constexpr (_Strat == _St::_Custom) {
                    iter_swap(static_cast<_Ty1&&>(_Val1), static_cast<_Ty2&&>(_Val2));
                } else if constexpr (_Strat == _St::_Swap) {
                    _RANGES swap(*static_cast<_Ty1&&>(_Val1), *static_cast<_Ty2&&>(_Val2));
                } else if constexpr (_Strat == _St::_Exchange) {
                    *static_cast<_Ty1&&>(_Val1) =
                        _Iter_exchange_move(static_cast<_Ty2&&>(_Val2), static_cast<_Ty1&&>(_Val1));
                } else {
                    static_assert(_Always_false<_Ty1>, "should be unreachable");
                }
            }
            // clang-format on
        };
    } // namespace _Iter_swap

    inline namespace _Cpos {
        inline constexpr _Iter_swap::_Cpo iter_swap;
    }
} // namespace ranges

// clang-format off
template <class _It1, class _It2 = _It1>
concept indirectly_swappable = indirectly_readable<_It1> && indirectly_readable<_It2>
    && requires(const _It1 __i1, const _It2 __i2) {
        _RANGES iter_swap(__i1, __i1);
        _RANGES iter_swap(__i2, __i2);
        _RANGES iter_swap(__i1, __i2);
        _RANGES iter_swap(__i2, __i1);
    };

template <class _It1, class _It2, class _Rel, class _Proj1 = identity, class _Proj2 = identity>
concept indirectly_comparable =
    indirect_binary_predicate<_Rel,
        projected<_It1, _Proj1>,
        projected<_It2, _Proj2>>;

template <class _It>
concept permutable = forward_iterator<_It>
    && indirectly_movable_storable<_It, _It>
    && indirectly_swappable<_It, _It>;

namespace ranges { struct ::std::less; }
template <class _It1, class _It2, class _Out, class _Pr = ranges::::std::less, class _Pj1 = identity, class _Pj2 = identity>
concept mergeable = input_iterator<_It1> && input_iterator<_It2>
    && weakly_incrementable<_Out>
    && indirectly_copyable<_It1, _Out>
    && indirectly_copyable<_It2, _Out>
    && indirect_strict_weak_order<_Pr, projected<_It1, _Pj1>, projected<_It2, _Pj2>>;
// clang-format on

template <class _It, class _Pr = ranges::::std::less, class _Proj = identity>
concept sortable = permutable<_It> && indirect_strict_weak_order<_Pr, projected<_It, _Proj>>;

template <class _Iter>
using _Iter_ref_t = iter_reference_t<_Iter>;

template <class _Iter>
using _Iter_value_t = iter_value_t<_Iter>;

template <class _Iter>
using _Iter_diff_t = iter_difference_t<_Iter>;

#else // ^^^ __cpp_lib_concepts / !__cpp_lib_concepts vvv
template <class, class = void>
struct _Iterator_traits_base {}; // empty for non-iterators

template <class _Iter>
struct _Iterator_traits_base<_Iter,
    ::std::void_t<typename _Iter::iterator_category, typename _Iter::value_type, typename _Iter::difference_type,
        typename _Iter::pointer, typename _Iter::reference>> {
    // defined if _Iter::* types exist
    using iterator_category = typename _Iter::iterator_category;
    using value_type = typename _Iter::value_type;
    using difference_type = typename _Iter::difference_type;
    using pointer = typename _Iter::pointer;
    using reference = typename _Iter::reference;
};

template <class _Ty, bool = ::std::is_object_v<_Ty>>
struct _Iterator_traits_pointer_base { // iterator properties for pointers to object
    using iterator_category = random_access_iterator_tag;
    using value_type = ::std::remove_cv_t<_Ty>;
    using difference_type = ptrdiff_t;
    using pointer = _Ty*;
    using reference = _Ty&;
};

template <class _Ty>
struct _Iterator_traits_pointer_base<_Ty, false> {}; // iterator non-properties for pointers to non-object

template <class _Iter>
struct iterator_traits : _Iterator_traits_base<_Iter> {}; // get traits from iterator _Iter, if possible

template <class _Ty>
struct iterator_traits<_Ty*> : _Iterator_traits_pointer_base<_Ty> {}; // get traits from pointer, if possible

template <class _Iter>
using _Iter_ref_t = typename iterator_traits<_Iter>::reference;

template <class _Iter>
using _Iter_value_t = typename iterator_traits<_Iter>::value_type;

template <class _Iter>
using _Iter_diff_t = typename iterator_traits<_Iter>::difference_type;
#endif // __cpp_lib_concepts

template <class... _Iters>
using _Common_diff_t = ::std::common_type_t<_Iter_diff_t<_Iters>...>;

template <class _Iter>
using _Iter_cat_t = typename iterator_traits<_Iter>::iterator_category;

template <class _Ty, class = void>
_INLINE_VAR constexpr bool _Is_iterator_v = false;

template <class _Ty>
_INLINE_VAR constexpr bool _Is_iterator_v<_Ty, ::std::void_t<_Iter_cat_t<_Ty>>> = true;

template <class _Ty>
struct _Is_iterator : ::std::bool_constant<_Is_iterator_v<_Ty>> {};

template <class _Iter>
_INLINE_VAR constexpr bool _Is_input_iter_v = ::std::is_convertible_v<_Iter_cat_t<_Iter>, input_iterator_tag>;

template <class _Iter>
_INLINE_VAR constexpr bool _Is_fwd_iter_v = ::std::is_convertible_v<_Iter_cat_t<_Iter>, forward_iterator_tag>;

template <class _Iter>
_INLINE_VAR constexpr bool _Is_bidi_iter_v = ::std::is_convertible_v<_Iter_cat_t<_Iter>, bidirectional_iterator_tag>;

template <class _Iter>
_INLINE_VAR constexpr bool _Is_random_iter_v = ::std::is_convertible_v<_Iter_cat_t<_Iter>, random_access_iterator_tag>;

template <class, class = void>
struct _Is_checked_helper {}; // default definition, no longer used, retained due to pseudo-documentation

#if _ITERATOR_DEBUG_LEVEL != 0
template <class _Ty>
constexpr void _Verify_range(const _Ty* const _First, const _Ty* const _Last) noexcept {
    // special case range verification for pointers
    _STL_VERIFY(_First <= _Last, "transposed pointer range");
}
#endif // _ITERATOR_DEBUG_LEVEL != 0

template <class _Iter, class = void>
_INLINE_VAR constexpr bool _Allow_inheriting_unwrap_v = true;

template <class _Iter>
_INLINE_VAR constexpr bool _Allow_inheriting_unwrap_v<_Iter, ::std::void_t<typename _Iter::_Prevent_inheriting_unwrap>> =
    ::std::is_same_v<_Iter, typename _Iter::_Prevent_inheriting_unwrap>;

template <class _Iter, class _Sentinel = _Iter, class = void>
_INLINE_VAR constexpr bool _Range_verifiable_v = false;

template <class _Iter, class _Sentinel>
_INLINE_VAR constexpr bool _Range_verifiable_v<_Iter, _Sentinel,
    ::std::void_t<decltype(_Verify_range(declval<const _Iter&>(), declval<const _Sentinel&>()))>> =
    _Allow_inheriting_unwrap_v<_Iter>;

template <class _Iter, class _Sentinel>
constexpr void _Adl_verify_range(const _Iter& _First, const _Sentinel& _Last) {
    // check that [_First, _Last) forms an iterator range
    if constexpr (_Range_verifiable_v<_Iter, _Sentinel>) {
        _Verify_range(_First, _Last);
    }
}

template <class _Iter, class = void>
_INLINE_VAR constexpr bool _Unwrappable_v = false;

template <class _Iter>
_INLINE_VAR constexpr bool _Unwrappable_v<_Iter,
    ::std::void_t<decltype(declval<_Remove_cvref_t<_Iter>&>()._Seek_to(declval<_Iter>()._Unwrapped()))>> =
    _Allow_inheriting_unwrap_v<_Remove_cvref_t<_Iter>>;

template <class _Iter>
_NODISCARD constexpr decltype(auto) _Get_unwrapped(_Iter&& _It) {
    // unwrap an iterator previously subjected to _Adl_verify_range or otherwise validated
    if constexpr (::std::is_pointer_v<::std::decay_t<_Iter>>) { // special-case pointers and arrays
        return _It + 0;
    } else if constexpr (_Unwrappable_v<_Iter>) {
        return static_cast<_Iter&&>(_It)._Unwrapped();
    } else {
        return static_cast<_Iter&&>(_It);
    }
}

template <class _Iter>
using _Unwrapped_t = _Remove_cvref_t<decltype(_Get_unwrapped(declval<_Iter>()))>;

template <class _Iter, class = bool>
_INLINE_VAR constexpr bool _Do_unwrap_when_unverified_v = false;

template <class _Iter>
_INLINE_VAR constexpr bool
    _Do_unwrap_when_unverified_v<_Iter, decltype(static_cast<bool>(_Iter::_Unwrap_when_unverified))> =
        static_cast<bool>(_Iter::_Unwrap_when_unverified);

template <class _Iter>
_INLINE_VAR constexpr bool _Unwrappable_for_unverified_v =
    _Unwrappable_v<_Iter>&& _Do_unwrap_when_unverified_v<_Remove_cvref_t<_Iter>>;

template <class _Iter>
_NODISCARD constexpr decltype(auto) _Get_unwrapped_unverified(_Iter&& _It) {
    // unwrap an iterator not previously subjected to _Adl_verify_range
    if constexpr (::std::is_pointer_v<::std::decay_t<_Iter>>) { // special-case pointers and arrays
        return _It + 0;
    } else if constexpr (_Unwrappable_for_unverified_v<_Iter>) {
        return static_cast<_Iter&&>(_It)._Unwrapped();
    } else {
        return static_cast<_Iter&&>(_It);
    }
}

template <class _Iter>
using _Unwrapped_unverified_t = _Remove_cvref_t<decltype(_Get_unwrapped_unverified(declval<_Iter>()))>;

struct _Distance_unknown {
    constexpr _Distance_unknown operator-() const noexcept {
        return {};
    }
};

template <class _Diff>
_INLINE_VAR constexpr _Diff _Max_possible_v{static_cast<::std::make_unsigned_t<_Diff>>(-1) >> 1};

template <class _Diff>
_INLINE_VAR constexpr _Diff _Min_possible_v{-_Max_possible_v<_Diff> - 1};

template <class _Iter, class = void>
_INLINE_VAR constexpr bool _Offset_verifiable_v = false;

template <class _Iter>
_INLINE_VAR constexpr bool
    _Offset_verifiable_v<_Iter, ::std::void_t<decltype(declval<const _Iter&>()._Verify_offset(_Iter_diff_t<_Iter>{}))>> =
        true;

template <class _Iter>
_INLINE_VAR constexpr bool _Unwrappable_for_offset_v =
    _Unwrappable_v<_Iter>&& _Offset_verifiable_v<_Remove_cvref_t<_Iter>>;

template <class _Iter, class _Diff>
_NODISCARD constexpr decltype(auto) _Get_unwrapped_n(_Iter&& _It, const _Diff _Off) {
    if constexpr (::std::is_pointer_v<::std::decay_t<_Iter>>) {
        return _It + 0;
    } else if constexpr (_Unwrappable_for_offset_v<_Iter> && ::std::is_integral_v<_Diff>) {
        // ask an iterator to assert that the iterator moved _Off positions is valid, and unwrap
        using _IDiff     = _Iter_diff_t<_Remove_cvref_t<_Iter>>;
        using _CDiff     = ::std::common_type_t<_Diff, _IDiff>;
        const auto _COff = static_cast<_CDiff>(_Off);

        _STL_ASSERT(_COff <= static_cast<_CDiff>(_Max_possible_v<_IDiff>)
                        && (::std::is_unsigned_v<_Diff> || static_cast<_CDiff>(_Min_possible_v<_IDiff>) <= _COff),
            "integer overflow");
        (void) _COff;

        _It._Verify_offset(static_cast<_IDiff>(_Off));
        return static_cast<_Iter&&>(_It)._Unwrapped();
    } else if constexpr (_Unwrappable_for_unverified_v<_Iter>) {
        // iterator doesn't support offset-based asserts, or offset unknown; defer to unverified unwrap
        return static_cast<_Iter&&>(_It)._Unwrapped();
    } else {
        // pass through iterator that doesn't participate in checking
        return static_cast<_Iter&&>(_It);
    }
}

template <class _Iter, class _UIter, class = void>
_INLINE_VAR constexpr bool _Wrapped_seekable_v = false;

template <class _Iter, class _UIter>
_INLINE_VAR constexpr bool
    _Wrapped_seekable_v<_Iter, _UIter, ::std::void_t<decltype(declval<_Iter&>()._Seek_to(declval<_UIter>()))>> =
        true;

template <class _Iter, class _UIter>
constexpr void _Seek_wrapped(_Iter& _It, _UIter&& _UIt) {
    if constexpr (_Wrapped_seekable_v<_Iter, _UIter>) {
        _It._Seek_to(::std::forward<_UIter>(_UIt));
    } else {
        _It = ::std::forward<_UIter>(_UIt);
    }
}

#if _HAS_CXX17
template <class _Ty, class = void>
struct _Is_allocator : false_type {}; // selected when _Ty can't possibly be an allocator

template <class _Ty>
struct _Is_allocator<_Ty, ::std::void_t<typename _Ty::value_type, decltype(declval<_Ty&>().deallocate(
                                                               declval<_Ty&>().allocate(size_t{1}), size_t{1}))>>
    : true_type {}; // selected when _Ty resembles an allocator, N4687 26.2.1 [container.requirements.general]/17

// deduction guide utilities (N4892 [associative.general]/2)
template <class _Iter>
using _Guide_key_t = remove_const_t<typename iterator_traits<_Iter>::value_type::first_type>;

template <class _Iter>
using _Guide_val_t = typename iterator_traits<_Iter>::value_type::second_type;

template <class _Iter>
using _Guide_pair_t = ::std::pair<::std::add_const_t<typename iterator_traits<_Iter>::value_type::first_type>,
    typename iterator_traits<_Iter>::value_type::second_type>;

template <class _Ty>
struct is_execution_policy : false_type {};

template <class _Ty>
inline constexpr bool is_execution_policy_v = is_execution_policy<_Ty>::value;

// Note: The noexcept specifiers on all parallel algorithm overloads enforce termination as per
// N4713 23.19.4 [execpol.seq]/2, 23.19.5 [execpol.par]/2, and 23.19.6 [execpol.parunseq]/2
template <class _ExPo>
using _Enable_if_execution_policy_t = typename remove_reference_t<_ExPo>::_Standard_execution_policy;

#define _REQUIRE_PARALLEL_ITERATOR(_Iter) \
    static_assert(_Is_fwd_iter_v<_Iter>, "Parallel algorithms require forward iterators or stronger.")

#endif // _HAS_CXX17

template <class _Checked, class _Iter>
_NODISCARD constexpr auto _Idl_distance(const _Iter& _First, const _Iter& _Last) {
    // tries to get the distance between _First and _Last if they are random-access iterators
    if constexpr (_Is_random_iter_v<_Iter>) {
        return static_cast<_Iter_diff_t<_Checked>>(_Last - _First);
    } else {
        return _Distance_unknown{};
    }
}

template <class _Elem, bool _Is_enum = ::std::is_enum_v<_Elem>>
struct _Unwrap_enum { // if _Elem is an enum, gets its underlying type; otherwise leaves _Elem unchanged
    using type = ::std::underlying_type_t<_Elem>;
};

template <class _Elem>
struct _Unwrap_enum<_Elem, false> { // passthrough non-enum type
    using type = _Elem;
};

template <class _Elem>
using _Unwrap_enum_t = typename _Unwrap_enum<_Elem>::type;

#if _ITERATOR_DEBUG_LEVEL < 2
#define _DEBUG_LT_PRED(pred, x, y) static_cast<bool>(pred(x, y))
#define _DEBUG_ORDER_UNWRAPPED(first, last, pred)
#define _DEBUG_ORDER_SET_UNWRAPPED(otherIter, first, last, pred)

#else // _ITERATOR_DEBUG_LEVEL < 2
#define _DEBUG_LT_PRED(pred, x, y)                _Debug_lt_pred(pred, x, y)
#define _DEBUG_ORDER_UNWRAPPED(first, last, pred) _Debug_order_unchecked(first, last, pred)
#define _DEBUG_ORDER_SET_UNWRAPPED(otherIter, first, last, pred) \
    _Debug_order_set_unchecked<otherIter>(first, last, pred)

template <class _Pr, class _Ty1, class _Ty2,
    ::std::enable_if_t<::std::is_same_v<_Remove_cvref_t<_Ty1>, _Remove_cvref_t<_Ty2>>, int> = 0>
constexpr bool _Debug_lt_pred(_Pr&& _Pred, _Ty1&& _Left, _Ty2&& _Right) noexcept(
    noexcept(_Pred(_Left, _Right)) && noexcept(_Pred(_Right, _Left))) {
    // test if _Pred(_Left, _Right) and _Pred is strict weak ordering, when the arguments are the cv-same-type
    const auto _Result = static_cast<bool>(_Pred(_Left, _Right));
    if (_Result) {
        _STL_VERIFY(!_Pred(_Right, _Left), "invalid comparator");
    }

    return _Result;
}

template <class _Pr, class _Ty1, class _Ty2,
    ::std::enable_if_t<!::std::is_same_v<_Remove_cvref_t<_Ty1>, _Remove_cvref_t<_Ty2>>, int> = 0>
constexpr bool _Debug_lt_pred(_Pr&& _Pred, _Ty1&& _Left, _Ty2&& _Right) noexcept(noexcept(_Pred(_Left, _Right))) {
    // test if _Pred(_Left, _Right); no debug checks as the types differ
    return static_cast<bool>(_Pred(_Left, _Right));
}

template <class _InIt, class _Sentinel, class _Pr>
constexpr void _Debug_order_unchecked(_InIt _First, _Sentinel _Last, _Pr&& _Pred) {
    // test if range is ordered by predicate
    if constexpr (_Is_fwd_iter_v<_InIt>) {
        if (_First != _Last) {
            for (auto _Next = _First; ++_Next != _Last; _First = _Next) {
                _STL_VERIFY(!static_cast<bool>(_Pred(*_Next, *_First)), "sequence not ordered");
            }
        }
    }
}

template <class _OtherIt, class _InIt, class _Pr>
constexpr void _Debug_order_set_unchecked(_InIt _First, _InIt _Last, _Pr&& _Pred) {
    // test if range is ordered by predicate
    if constexpr (::std::is_same_v<_Iter_value_t<_OtherIt>, _Iter_value_t<_InIt>> && _Is_fwd_iter_v<_InIt>) {
        _Debug_order_unchecked(_First, _Last, _Pred);
    }
}
#endif // _ITERATOR_DEBUG_LEVEL < 2

// from <iterator>
template <class _InIt, class _Diff>
_CONSTEXPR17 void advance(_InIt& _Where, _Diff _Off) { // increment iterator by offset
    if constexpr (_Is_random_iter_v<_InIt>) {
        _Where += _Off;
    } else {
        if constexpr (::std::is_signed_v<_Diff> && !_Is_bidi_iter_v<_InIt>) {
            _STL_ASSERT(_Off >= 0, "negative advance of non-bidirectional iterator");
        }

        decltype(auto) _UWhere      = _Get_unwrapped_n(::std::move(_Where), _Off);
        constexpr bool _Need_rewrap = !::std::is_reference_v<decltype(_Get_unwrapped_n(::std::move(_Where), _Off))>;

        if constexpr (::std::is_signed_v<_Diff> && _Is_bidi_iter_v<_InIt>) {
            for (; _Off < 0; ++_Off) {
                --_UWhere;
            }
        }

        for (; 0 < _Off; --_Off) {
            ++_UWhere;
        }

        if constexpr (_Need_rewrap) {
            _Seek_wrapped(_Where, ::std::move(_UWhere));
        }
    }
}

template <class _InIt>
_NODISCARD _CONSTEXPR17 _Iter_diff_t<_InIt> distance(_InIt _First, _InIt _Last) {
    if constexpr (_Is_random_iter_v<_InIt>) {
        return _Last - _First; // assume the iterator will do debug checking
    } else {
        _Adl_verify_range(_First, _Last);
        auto _UFirst             = _Get_unwrapped(_First);
        const auto _ULast        = _Get_unwrapped(_Last);
        _Iter_diff_t<_InIt> _Off = 0;
        for (; _UFirst != _ULast; ++_UFirst) {
            ++_Off;
        }

        return _Off;
    }
}

template <class _InIt>
constexpr _InIt _Next_iter(_InIt _First) { // increment iterator
    return ++_First;
}

template <class _InIt>
_NODISCARD _CONSTEXPR17 _InIt next(_InIt _First, _Iter_diff_t<_InIt> _Off = 1) { // increment iterator
    static_assert(_Is_input_iter_v<_InIt>, "next requires input iterator");

    _RPC advance(_First, _Off);
    return _First;
}

template <class _BidIt>
constexpr _BidIt _Prev_iter(_BidIt _First) { // decrement iterator
    return --_First;
}

template <class _BidIt>
_NODISCARD _CONSTEXPR17 _BidIt prev(_BidIt _First, _Iter_diff_t<_BidIt> _Off = 1) { // decrement iterator
    static_assert(_Is_bidi_iter_v<_BidIt>, "prev requires bidirectional iterator");

    _RPC advance(_First, -_Off);
    return _First;
}

template <class _BidIt>
class reverse_iterator {
public:
    using iterator_type = _BidIt;

#ifdef __cpp_lib_concepts
    using iterator_concept =
        ::std::conditional_t<random_access_iterator<_BidIt>, random_access_iterator_tag, bidirectional_iterator_tag>;
    using iterator_category = ::std::conditional_t<derived_from<_Iter_cat_t<_BidIt>, random_access_iterator_tag>,
        random_access_iterator_tag, _Iter_cat_t<_BidIt>>;
#else // ^^^ __cpp_lib_concepts / !__cpp_lib_concepts vvv
    using iterator_category = _Iter_cat_t<_BidIt>;
#endif // __cpp_lib_concepts
    using value_type      = _Iter_value_t<_BidIt>;
    using difference_type = _Iter_diff_t<_BidIt>;
    using pointer         = typename iterator_traits<_BidIt>::pointer;
    using reference       = _Iter_ref_t<_BidIt>;

    template <class>
    friend class reverse_iterator;

    _CONSTEXPR17 reverse_iterator() = default;

    _CONSTEXPR17 explicit reverse_iterator(_BidIt _Right) noexcept(
        ::std::is_nothrow_move_constructible_v<_BidIt>) // strengthened
        : current(::std::move(_Right)) {}

    // clang-format off
    template <class _Other>
#ifdef __cpp_lib_concepts
        requires (!::std::is_same_v<_Other, _BidIt>) && convertible_to<const _Other&, _BidIt>
#endif // __cpp_lib_concepts
    _CONSTEXPR17 reverse_iterator(const reverse_iterator<_Other>& _Right) noexcept(
        ::std::addressof<_BidIt, const _Other&>) // strengthened
        : current(_Right.current) {}

    template <class _Other>
#ifdef __cpp_lib_concepts
        requires (!::std::is_same_v<_Other, _BidIt>) && convertible_to<const _Other&, _BidIt>
            && assignable_from<_BidIt&, const _Other&>
#endif // __cpp_lib_concepts
    _CONSTEXPR17 reverse_iterator& operator=(const reverse_iterator<_Other>& _Right) {
        current = _Right.current;
        return *this;
    }
    // clang-format on

    _NODISCARD _CONSTEXPR17 _BidIt base() const {
        return current;
    }

    _NODISCARD _CONSTEXPR17 reference operator*() const {
        _BidIt _Tmp = current;
        return *--_Tmp;
    }

#ifdef __cpp_lib_concepts
    // clang-format off
    _NODISCARD constexpr pointer operator->() const
        requires (::std::is_pointer_v<_BidIt> || requires(const _BidIt __i) { __i.operator->(); }) {
        _BidIt _Tmp = current;
        --_Tmp;
        if constexpr (::std::is_pointer_v<_BidIt>) {
            return _Tmp;
        } else {
            return _Tmp.operator->();
        }
    }
    // clang-format on
#else // ^^^ __cpp_lib_concepts / !__cpp_lib_concepts vvv
    _NODISCARD _CONSTEXPR17 pointer operator->() const {
        _BidIt _Tmp = current;
        --_Tmp;
        if constexpr (::std::is_pointer_v<_BidIt>) {
            return _Tmp;
        } else {
            return _Tmp.operator->();
        }
    }
#endif // __cpp_lib_concepts

    _CONSTEXPR17 reverse_iterator& operator++() {
        --current;
        return *this;
    }

    _CONSTEXPR17 reverse_iterator operator++(int) {
        reverse_iterator _Tmp = *this;
        --current;
        return _Tmp;
    }

    _CONSTEXPR17 reverse_iterator& operator--() {
        ++current;
        return *this;
    }

    _CONSTEXPR17 reverse_iterator operator--(int) {
        reverse_iterator _Tmp = *this;
        ++current;
        return _Tmp;
    }

    _NODISCARD _CONSTEXPR17 reverse_iterator operator+(const difference_type _Off) const {
        return reverse_iterator(current - _Off);
    }

    _CONSTEXPR17 reverse_iterator& operator+=(const difference_type _Off) {
        current -= _Off;
        return *this;
    }

    _NODISCARD _CONSTEXPR17 reverse_iterator operator-(const difference_type _Off) const {
        return reverse_iterator(current + _Off);
    }

    _CONSTEXPR17 reverse_iterator& operator-=(const difference_type _Off) {
        current += _Off;
        return *this;
    }

    _NODISCARD _CONSTEXPR17 reference operator[](const difference_type _Off) const {
        return current[static_cast<difference_type>(-_Off - 1)];
    }

#ifdef __cpp_lib_concepts
    _NODISCARD friend constexpr iter_rvalue_reference_t<_BidIt> iter_move(const reverse_iterator& _It) noexcept(
        is_nothrow_copy_constructible_v<_BidIt>&& noexcept(_RANGES iter_move(--declval<_BidIt&>()))) {
        auto _Tmp = _It.current;
        --_Tmp;
        return _RANGES iter_move(_Tmp);
    }

    template <indirectly_swappable<_BidIt> _BidIt2>
    friend constexpr void iter_swap(const reverse_iterator& _Left, const reverse_iterator<_BidIt2>& _Right) noexcept(
        is_nothrow_copy_constructible_v<_BidIt>&& is_nothrow_copy_constructible_v<_BidIt2>&& noexcept(
            _RANGES iter_swap(--declval<_BidIt&>(), --declval<_BidIt2&>()))) {
        auto _LTmp = _Left.current;
        auto _RTmp = _Right.base();
        --_LTmp;
        --_RTmp;
        _RANGES iter_swap(_LTmp, _RTmp);
    }
#endif // __cpp_lib_concepts

    using _Prevent_inheriting_unwrap = reverse_iterator;

    template <class _BidIt2, ::std::enable_if_t<_Range_verifiable_v<_BidIt, _BidIt2>, int> = 0>
    friend constexpr void _Verify_range(const reverse_iterator& _First, const reverse_iterator<_BidIt2>& _Last) {
        _Verify_range(_Last._Get_current(), _First.current); // note reversed parameters
    }

    template <class _BidIt2 = _BidIt, ::std::enable_if_t<_Offset_verifiable_v<_BidIt2>, int> = 0>
    constexpr void _Verify_offset(const difference_type _Off) const {
        _STL_VERIFY(_Off != _Min_possible_v<difference_type>, "integer overflow");
        current._Verify_offset(-_Off);
    }

    template <class _BidIt2 = _BidIt, ::std::enable_if_t<_Unwrappable_v<const _BidIt2&>, int> = 0>
    _NODISCARD constexpr reverse_iterator<_Unwrapped_t<const _BidIt2&>> _Unwrapped() const {
        return static_cast<reverse_iterator<_Unwrapped_t<const _BidIt2&>>>(current._Unwrapped());
    }

    static constexpr bool _Unwrap_when_unverified = _Do_unwrap_when_unverified_v<_BidIt>;

    template <class _Src, ::std::enable_if_t<_Wrapped_seekable_v<_BidIt, const _Src&>, int> = 0>
    constexpr void _Seek_to(const reverse_iterator<_Src>& _It) {
        current._Seek_to(_It.current);
    }

    _NODISCARD constexpr const _BidIt& _Get_current() const noexcept {
        return current;
    }

protected:
    _BidIt current{};
};

template <class _BidIt1, class _BidIt2>
_NODISCARD _CONSTEXPR17 bool operator==(const reverse_iterator<_BidIt1>& _Left, const reverse_iterator<_BidIt2>& _Right)
#ifdef __cpp_lib_concepts
    // clang-format off
    requires requires {
        { _Left._Get_current() == _Right._Get_current() } -> _Implicitly_convertible_to<bool>;
    }
// clang-format on
#endif // __cpp_lib_concepts
{ return _Left._Get_current() == _Right._Get_current(); }

template <class _BidIt1, class _BidIt2>
_NODISCARD _CONSTEXPR17 bool operator!=(const reverse_iterator<_BidIt1>& _Left, const reverse_iterator<_BidIt2>& _Right)
#ifdef __cpp_lib_concepts
    // clang-format off
    requires requires {
        { _Left._Get_current() != _Right._Get_current() } -> _Implicitly_convertible_to<bool>;
    }
// clang-format on
#endif // __cpp_lib_concepts
{ return _Left._Get_current() != _Right._Get_current(); }

template <class _BidIt1, class _BidIt2>
_NODISCARD _CONSTEXPR17 bool operator<(const reverse_iterator<_BidIt1>& _Left, const reverse_iterator<_BidIt2>& _Right)
#ifdef __cpp_lib_concepts
    // clang-format off
    requires requires {
        { _Left._Get_current() > _Right._Get_current() } -> _Implicitly_convertible_to<bool>;
    }
// clang-format on
#endif // __cpp_lib_concepts
{ return _Left._Get_current() > _Right._Get_current(); }

template <class _BidIt1, class _BidIt2>
_NODISCARD _CONSTEXPR17 bool operator>(const reverse_iterator<_BidIt1>& _Left, const reverse_iterator<_BidIt2>& _Right)
#ifdef __cpp_lib_concepts
    // clang-format off
    requires requires {
        { _Left._Get_current() < _Right._Get_current() } -> _Implicitly_convertible_to<bool>;
    }
// clang-format on
#endif // __cpp_lib_concepts
{ return _Left._Get_current() < _Right._Get_current(); }

template <class _BidIt1, class _BidIt2>
_NODISCARD _CONSTEXPR17 bool operator<=(const reverse_iterator<_BidIt1>& _Left, const reverse_iterator<_BidIt2>& _Right)
#ifdef __cpp_lib_concepts
    // clang-format off
    requires requires {
        { _Left._Get_current() >= _Right._Get_current() } -> _Implicitly_convertible_to<bool>;
    }
// clang-format on
#endif // __cpp_lib_concepts
{ return _Left._Get_current() >= _Right._Get_current(); }

template <class _BidIt1, class _BidIt2>
_NODISCARD _CONSTEXPR17 bool operator>=(const reverse_iterator<_BidIt1>& _Left, const reverse_iterator<_BidIt2>& _Right)
#ifdef __cpp_lib_concepts
    // clang-format off
    requires requires {
        { _Left._Get_current() <= _Right._Get_current() } -> _Implicitly_convertible_to<bool>;
    }
// clang-format on
#endif // __cpp_lib_concepts
{ return _Left._Get_current() <= _Right._Get_current(); }

#ifdef __cpp_lib_concepts
template <class _BidIt1, three_way_comparable_with<_BidIt1> _BidIt2>
_NODISCARD constexpr compare_three_way_result_t<_BidIt1, _BidIt2> operator<=>(
    const reverse_iterator<_BidIt1>& _Left, const reverse_iterator<_BidIt2>& _Right) {
    return _Right._Get_current() <=> _Left._Get_current();
}
#endif // __cpp_lib_concepts

template <class _BidIt1, class _BidIt2>
_NODISCARD _CONSTEXPR17 auto operator-(const reverse_iterator<_BidIt1>& _Left, const reverse_iterator<_BidIt2>& _Right)
    -> decltype(_Right._Get_current() - _Left._Get_current()) {
    return _Right._Get_current() - _Left._Get_current();
}

template <class _BidIt>
_NODISCARD _CONSTEXPR17 reverse_iterator<_BidIt> operator+(
    typename reverse_iterator<_BidIt>::difference_type _Off, const reverse_iterator<_BidIt>& _Right) {
    return _Right + _Off;
}

template <class _BidIt>
_NODISCARD _CONSTEXPR17 reverse_iterator<_BidIt> make_reverse_iterator(_BidIt _Iter) noexcept(
    ::std::is_nothrow_move_constructible_v<_BidIt>) /* strengthened */ {
    return reverse_iterator<_BidIt>(::std::move(_Iter));
}

#ifdef __cpp_lib_concepts
// clang-format off
template <class _BidIt1, class _BidIt2>
    requires (!sized_sentinel_for<_BidIt1, _BidIt2>)
inline constexpr bool disable_sized_sentinel_for<reverse_iterator<_BidIt1>, reverse_iterator<_BidIt2>> = true;
// clang-format on
#endif // __cpp_lib_concepts

template <class _Container>
_NODISCARD _CONSTEXPR17 auto begin(_Container& _Cont) -> decltype(_Cont.begin()) {
    return _Cont.begin();
}

template <class _Container>
_NODISCARD _CONSTEXPR17 auto begin(const _Container& _Cont) -> decltype(_Cont.begin()) {
    return _Cont.begin();
}

template <class _Container>
_NODISCARD _CONSTEXPR17 auto end(_Container& _Cont) -> decltype(_Cont.end()) {
    return _Cont.end();
}

template <class _Container>
_NODISCARD _CONSTEXPR17 auto end(const _Container& _Cont) -> decltype(_Cont.end()) {
    return _Cont.end();
}

template <class _Ty, size_t _Size>
_NODISCARD constexpr _Ty* begin(_Ty (&_Array)[_Size]) noexcept {
    return _Array;
}

template <class _Ty, size_t _Size>
_NODISCARD constexpr _Ty* end(_Ty (&_Array)[_Size]) noexcept {
    return _Array + _Size;
}

template <class _Container>
_NODISCARD constexpr auto cbegin(const _Container& _Cont) noexcept(noexcept(_RPC begin(_Cont)))
    -> decltype(_RPC begin(_Cont)) {
    return _RPC begin(_Cont);
}

template <class _Container>
_NODISCARD constexpr auto cend(const _Container& _Cont) noexcept(noexcept(_RPC end(_Cont)))
    -> decltype(_RPC end(_Cont)) {
    return _RPC end(_Cont);
}

template <class _Container>
_NODISCARD _CONSTEXPR17 auto rbegin(_Container& _Cont) -> decltype(_Cont.rbegin()) {
    return _Cont.rbegin();
}

template <class _Container>
_NODISCARD _CONSTEXPR17 auto rbegin(const _Container& _Cont) -> decltype(_Cont.rbegin()) {
    return _Cont.rbegin();
}

template <class _Container>
_NODISCARD _CONSTEXPR17 auto rend(_Container& _Cont) -> decltype(_Cont.rend()) {
    return _Cont.rend();
}

template <class _Container>
_NODISCARD _CONSTEXPR17 auto rend(const _Container& _Cont) -> decltype(_Cont.rend()) {
    return _Cont.rend();
}

template <class _Ty, size_t _Size>
_NODISCARD _CONSTEXPR17 reverse_iterator<_Ty*> rbegin(_Ty (&_Array)[_Size]) {
    return reverse_iterator<_Ty*>(_Array + _Size);
}

template <class _Ty, size_t _Size>
_NODISCARD _CONSTEXPR17 reverse_iterator<_Ty*> rend(_Ty (&_Array)[_Size]) {
    return reverse_iterator<_Ty*>(_Array);
}

template <class _Elem>
_NODISCARD _CONSTEXPR17 reverse_iterator<const _Elem*> rbegin(::std::initializer_list<_Elem> _Ilist) {
    return reverse_iterator<const _Elem*>(_Ilist.end());
}

template <class _Elem>
_NODISCARD _CONSTEXPR17 reverse_iterator<const _Elem*> rend(::std::initializer_list<_Elem> _Ilist) {
    return reverse_iterator<const _Elem*>(_Ilist.begin());
}

template <class _Container>
_NODISCARD _CONSTEXPR17 auto crbegin(const _Container& _Cont) -> decltype(_RPC rbegin(_Cont)) {
    return _RPC rbegin(_Cont);
}

template <class _Container>
_NODISCARD _CONSTEXPR17 auto crend(const _Container& _Cont) -> decltype(_RPC rend(_Cont)) {
    return _RPC rend(_Cont);
}

template <class _Container>
_NODISCARD constexpr auto size(const _Container& _Cont) -> decltype(_Cont.size()) {
    return _Cont.size();
}

template <class _Ty, size_t _Size>
_NODISCARD constexpr size_t size(const _Ty (&)[_Size]) noexcept {
    return _Size;
}

#if _HAS_CXX20
template <class _Container>
_NODISCARD constexpr auto ssize(const _Container& _Cont)
    -> ::std::common_type_t<ptrdiff_t, make_signed_t<decltype(_Cont.size())>> {
    using _Common = ::std::common_type_t<ptrdiff_t, make_signed_t<decltype(_Cont.size())>>;
    return static_cast<_Common>(_Cont.size());
}

template <class _Ty, ptrdiff_t _Size>
_NODISCARD constexpr ptrdiff_t ssize(const _Ty (&)[_Size]) noexcept {
    return _Size;
}
#endif // _HAS_CXX20

template <class _Container>
_NODISCARD constexpr auto empty(const _Container& _Cont) -> decltype(_Cont.empty()) {
    return _Cont.empty();
}

template <class _Ty, size_t _Size>
_NODISCARD constexpr bool empty(const _Ty (&)[_Size]) noexcept {
    return false;
}

template <class _Elem>
_NODISCARD constexpr bool empty(::std::initializer_list<_Elem> _Ilist) noexcept {
    return _Ilist.size() == 0;
}

template <class _Container>
_NODISCARD constexpr auto data(_Container& _Cont) -> decltype(_Cont.data()) {
    return _Cont.data();
}

template <class _Container>
_NODISCARD constexpr auto data(const _Container& _Cont) -> decltype(_Cont.data()) {
    return _Cont.data();
}

template <class _Ty, size_t _Size>
_NODISCARD constexpr _Ty* data(_Ty (&_Array)[_Size]) noexcept {
    return _Array;
}

template <class _Elem>
_NODISCARD constexpr const _Elem* data(::std::initializer_list<_Elem> _Ilist) noexcept {
    return _Ilist.begin();
}

#ifdef __cpp_lib_concepts
template <class _Ty>
_NODISCARD _Ty _Fake_decay_copy(_Ty) noexcept;
// _Fake_decay_copy<T>(E):
// (1) has type T [::std::decay_t<decltype((E))> if T is deduced],
// (2) is well-formed if and only if E is implicitly convertible to T and T is destructible, and
// (3) is non-throwing if and only if both conversion from decltype((E)) to T and destruction of T are non-throwing.

template <class _Ty1, class _Ty2>
concept _Not_same_as = !same_as<remove_cvref_t<_Ty1>, remove_cvref_t<_Ty2>>;

namespace ranges {
    template <class>
    inline constexpr bool _Has_complete_elements = false;

    // clang-format off
    template <class _Ty>
        requires requires(_Ty& __t) { sizeof(__t[0]); }
    inline constexpr bool _Has_complete_elements<_Ty> = true;
    // clang-format on

    template <class>
    inline constexpr bool enable_borrowed_range = false;

    template <class _Rng>
    concept _Should_range_access = is_lvalue_reference_v<_Rng> || enable_borrowed_range<remove_cvref_t<_Rng>>;

    namespace _Begin {
        template <class _Ty>
        void begin(_Ty&) = delete;
        template <class _Ty>
        void begin(const _Ty&) = delete;

        template <class _Ty>
        concept _Has_member = requires(_Ty __t) {
            { _Fake_decay_copy(__t.begin()) } -> input_or_output_iterator;
        };

        template <class _Ty>
        concept _Has_ADL = _Has_class_or_enum_type<_Ty> && requires(_Ty __t) {
            { _Fake_decay_copy(begin(__t)) } -> input_or_output_iterator;
        };

        class _Cpo {
        private:
            enum class _St { _None, _Array, _Member, _Non_member };

            template <class _Ty>
            _NODISCARD static _CONSTEVAL _Choice_t<_St> _Choose() noexcept {
                _STL_INTERNAL_STATIC_ASSERT(is_lvalue_reference_v<_Ty>);
                if constexpr (::std::is_array_v<remove_reference_t<_Ty>>) {
                    static_assert(_Has_complete_elements<_Ty>,
                        "The range access customization point objects "
                        "std::ranges::begin, std::ranges::end, std::ranges::rbegin, std::ranges::rend, "
                        "and std::ranges::data do not accept arrays with incomplete element types.");
                    return {_St::_Array, true};
                } else if constexpr (_Has_member<_Ty>) {
                    return {_St::_Member, noexcept(_Fake_decay_copy(declval<_Ty>().begin()))};
                } else if constexpr (_Has_ADL<_Ty>) {
                    return {_St::_Non_member, noexcept(_Fake_decay_copy(begin(declval<_Ty>())))};
                } else {
                    return {_St::_None};
                }
            }

            template <class _Ty>
            static constexpr _Choice_t<_St> _Choice = _Choose<_Ty>();

        public:
            // clang-format off
            template <_Should_range_access _Ty>
                requires (_Choice<_Ty&>._Strategy != _St::_None)
            _NODISCARD constexpr auto operator()(_Ty&& _Val) const noexcept(_Choice<_Ty&>._No_throw) {
                constexpr _St _Strat = _Choice<_Ty&>._Strategy;

                if constexpr (_Strat == _St::_Array) {
                    return _Val;
                } else if constexpr (_Strat == _St::_Member) {
                    return _Val.begin();
                } else if constexpr (_Strat == _St::_Non_member) {
                    return begin(_Val);
                } else {
                    static_assert(_Always_false<_Ty>, "Should be unreachable");
                }
            }
            // clang-format on
        };
    } // namespace _Begin

    inline namespace _Cpos {
        inline constexpr _Begin::_Cpo begin;
    }

    template <class _Ty>
    using iterator_t = decltype(_RANGES begin(declval<_Ty&>()));

    namespace _Unchecked_begin {
        template <class _Ty>
        concept _Has_member = requires(_Ty& __t) {
            { __t._Unchecked_begin() } -> input_or_output_iterator;
        };

        template <class _Ty>
        concept _Can_begin = requires(_Ty& __t) {
            _Get_unwrapped(_RANGES begin(__t));
        };

        class _Cpo {
        private:
            enum class _St { _None, _Member, _Unwrap };

            template <class _Ty>
            _NODISCARD static _CONSTEVAL _Choice_t<_St> _Choose() noexcept {
                _STL_INTERNAL_STATIC_ASSERT(is_lvalue_reference_v<_Ty>);
                if constexpr (_Has_member<_Ty>) {
                    _STL_INTERNAL_STATIC_ASSERT(
                        same_as<decltype(declval<_Ty>()._Unchecked_begin()), _Unwrapped_t<iterator_t<_Ty>>>);
                    return {_St::_Member, noexcept(declval<_Ty>()._Unchecked_begin())};
                } else if constexpr (_Can_begin<_Ty>) {
                    return {_St::_Unwrap, noexcept(_Get_unwrapped(_RANGES begin(declval<_Ty>())))};
                } else {
                    return {_St::_None};
                }
            }

            template <class _Ty>
            static constexpr _Choice_t<_St> _Choice = _Choose<_Ty>();

        public:
            // clang-format off
            template <_Should_range_access _Ty>
                requires (_Choice<_Ty&>._Strategy != _St::_None)
            _NODISCARD constexpr auto operator()(_Ty&& _Val) const noexcept(_Choice<_Ty&>._No_throw) {
                constexpr _St _Strat = _Choice<_Ty&>._Strategy;

                if constexpr (_Strat == _St::_Member) {
                    return _Val._Unchecked_begin();
                } else if constexpr (_Strat == _St::_Unwrap) {
                    return _Get_unwrapped(_RANGES begin(_Val));
                } else {
                    static_assert(_Always_false<_Ty>, "Should be unreachable");
                }
            }
            // clang-format on
        };
    } // namespace _Unchecked_begin

    inline namespace _Cpos {
        inline constexpr _Unchecked_begin::_Cpo _Ubegin;
    }

    namespace _End {
        template <class _Ty>
        void end(_Ty&) = delete;
        template <class _Ty>
        void end(const _Ty&) = delete;

        template <class _Ty>
        concept _Has_member = requires(_Ty __t) {
            { _Fake_decay_copy(__t.end()) } -> sentinel_for<iterator_t<_Ty>>;
        };

        template <class _Ty>
        concept _Has_ADL = _Has_class_or_enum_type<_Ty> && requires(_Ty __t) {
            { _Fake_decay_copy(end(__t)) } -> sentinel_for<iterator_t<_Ty>>;
        };

        class _Cpo {
        private:
            enum class _St { _None, _Array, _Member, _Non_member };

            template <class _Ty>
            _NODISCARD static _CONSTEVAL _Choice_t<_St> _Choose() noexcept {
                _STL_INTERNAL_STATIC_ASSERT(is_lvalue_reference_v<_Ty>);
                using _UnRef = remove_reference_t<_Ty>;

                if constexpr (::std::is_array_v<_UnRef>) {
                    static_assert(_Has_complete_elements<_UnRef>,
                        "The range access customization point objects "
                        "std::ranges::begin, std::ranges::end, std::ranges::rbegin, std::ranges::rend, "
                        "and std::ranges::data do not accept arrays with incomplete element types.");
                    if constexpr (::std::extent_v<_UnRef> != 0) {
                        return {_St::_Array, true};
                    } else {
                        return {_St::_None};
                    }
                } else if constexpr (_Has_member<_Ty>) {
                    return {_St::_Member, noexcept(_Fake_decay_copy(declval<_Ty>().end()))};
                } else if constexpr (_Has_ADL<_Ty>) {
                    return {_St::_Non_member, noexcept(_Fake_decay_copy(end(declval<_Ty>())))};
                } else {
                    return {_St::_None};
                }
            }

            template <class _Ty>
            static constexpr _Choice_t<_St> _Choice = _Choose<_Ty>();

        public:
            // clang-format off
            template <_Should_range_access _Ty>
                requires (_Choice<_Ty&>._Strategy != _St::_None)
            _NODISCARD constexpr auto operator()(_Ty&& _Val) const noexcept(_Choice<_Ty&>._No_throw) {
                constexpr _St _Strat = _Choice<_Ty&>._Strategy;

                if constexpr (_Strat == _St::_Array) {
                    // ::std::extent_v<remove_reference_t<_Ty&>> reuses specializations from _Choose
                    return _Val + ::std::extent_v<remove_reference_t<_Ty&>>;
                } else if constexpr (_Strat == _St::_Member) {
                    return _Val.end();
                } else if constexpr (_Strat == _St::_Non_member) {
                    return end(_Val);
                } else {
                    static_assert(_Always_false<_Ty>, "should be unreachable");
                }
            }
            // clang-format on
        };
    } // namespace _End

    inline namespace _Cpos {
        inline constexpr _End::_Cpo end;
    }

    namespace _Unchecked_end {
        template <class _Ty>
        concept _Has_member = _Unchecked_begin::_Has_member<_Ty> && requires(_Ty& __t) {
            __t._Unchecked_begin(); // required explicitly for better diagnostics
            { __t._Unchecked_end() } -> sentinel_for<decltype(__t._Unchecked_begin())>;
        };

        template <class _Ty>
        concept _Can_end = requires(_Ty& __t) {
            _Get_unwrapped(_RANGES end(__t));
        };

        class _Cpo {
        private:
            enum class _St { _None, _Member, _Unwrap };

            template <class _Ty>
            _NODISCARD static _CONSTEVAL _Choice_t<_St> _Choose() noexcept {
                _STL_INTERNAL_STATIC_ASSERT(is_lvalue_reference_v<_Ty>);
                if constexpr (_Has_member<_Ty>) {
                    _STL_INTERNAL_STATIC_ASSERT(same_as<decltype(declval<_Ty>()._Unchecked_end()),
                        decltype(_Get_unwrapped(_RANGES end(declval<_Ty>())))>);
                    return {_St::_Member, noexcept(declval<_Ty>()._Unchecked_end())};
                } else if constexpr (_Can_end<_Ty>) {
                    return {_St::_Unwrap, noexcept(_Get_unwrapped(_RANGES end(declval<_Ty>())))};
                } else {
                    return {_St::_None};
                }
            }

            template <class _Ty>
            static constexpr _Choice_t<_St> _Choice = _Choose<_Ty>();

        public:
            // clang-format off
            template <_Should_range_access _Ty>
                requires (_Choice<_Ty&>._Strategy != _St::_None)
            _NODISCARD constexpr auto operator()(_Ty&& _Val) const noexcept(_Choice<_Ty&>._No_throw) {
                constexpr _St _Strat = _Choice<_Ty&>._Strategy;

                if constexpr (_Strat == _St::_Member) {
                    return _Val._Unchecked_end();
                } else if constexpr (_Strat == _St::_Unwrap) {
                    return _Get_unwrapped(_RANGES end(_Val));
                } else {
                    static_assert(_Always_false<_Ty>, "Should be unreachable");
                }
            }
            // clang-format on
        };
    } // namespace _Unchecked_end

    inline namespace _Cpos {
        inline constexpr _Unchecked_end::_Cpo _Uend;
    }

    template <class _Rng>
    concept range = requires(_Rng& __r) {
        _RANGES begin(__r);
        _RANGES end(__r);
    };

    template <class _Rng>
    concept borrowed_range = range<_Rng> && _Should_range_access<_Rng>;

    template <range _Rng>
    using sentinel_t = decltype(_RANGES end(declval<_Rng&>()));

    template <range _Rng>
    using range_difference_t = iter_difference_t<iterator_t<_Rng>>;

    template <range _Rng>
    using range_value_t = iter_value_t<iterator_t<_Rng>>;

    template <range _Rng>
    using range_reference_t = iter_reference_t<iterator_t<_Rng>>;

    template <range _Rng>
    using range_rvalue_reference_t = iter_rvalue_reference_t<iterator_t<_Rng>>;

    struct _Cbegin_fn {
        // clang-format off
        template <class _Ty, class _CTy = _Const_thru_ref<_Ty>>
        _NODISCARD constexpr auto operator()(_Ty&& _Val) const
            noexcept(noexcept(_RANGES begin(static_cast<_CTy&&>(_Val))))
            requires requires { _RANGES begin(static_cast<_CTy&&>(_Val)); } {
            return _RANGES begin(static_cast<_CTy&&>(_Val));
        }
        // clang-format on
    };

    inline namespace _Cpos {
        inline constexpr _Cbegin_fn cbegin;
    }

    struct _Cend_fn {
        // clang-format off
        template <class _Ty, class _CTy = _Const_thru_ref<_Ty>>
        _NODISCARD constexpr auto operator()(_Ty&& _Val) const
            noexcept(noexcept(_RANGES end(static_cast<_CTy&&>(_Val))))
            requires requires { _RANGES end(static_cast<_CTy&&>(_Val)); } {
            return _RANGES end(static_cast<_CTy&&>(_Val));
        }
        // clang-format on
    };

    inline namespace _Cpos {
        inline constexpr _Cend_fn cend;
    }

    namespace _Rbegin {
        template <class _Ty>
        void rbegin(_Ty&) = delete;
        template <class _Ty>
        void rbegin(const _Ty&) = delete;

        template <class _Ty>
        concept _Has_member = requires(_Ty __t) {
            { _Fake_decay_copy(__t.rbegin()) } -> input_or_output_iterator;
        };

        template <class _Ty>
        concept _Has_ADL = _Has_class_or_enum_type<_Ty> && requires(_Ty __t) {
            { _Fake_decay_copy(rbegin(__t)) } -> input_or_output_iterator;
        };

        template <class _Ty>
        concept _Can_make_reverse = requires(_Ty __t) {
            { _RANGES begin(__t) } -> bidirectional_iterator;
            { _RANGES end(__t) } -> same_as<decltype(_RANGES begin(__t))>;
        };

        class _Cpo {
        private:
            enum class _St { _None, _Member, _Non_member, _Make_reverse };

            template <class _Ty>
            _NODISCARD static _CONSTEVAL _Choice_t<_St> _Choose() noexcept {
                _STL_INTERNAL_STATIC_ASSERT(is_lvalue_reference_v<_Ty>);
                if constexpr (_Has_member<_Ty>) {
                    return {_St::_Member, noexcept(_Fake_decay_copy(declval<_Ty>().rbegin()))};
                } else if constexpr (_Has_ADL<_Ty>) {
                    return {_St::_Non_member, noexcept(_Fake_decay_copy(rbegin(declval<_Ty>())))};
                } else if constexpr (_Can_make_reverse<_Ty>) {
                    return {_St::_Make_reverse, noexcept(_RPC make_reverse_iterator(_RANGES end(declval<_Ty>())))};
                } else {
                    return {_St::_None};
                }
            }

            template <class _Ty>
            static constexpr _Choice_t<_St> _Choice = _Choose<_Ty>();

        public:
            // clang-format off
            template <_Should_range_access _Ty>
                requires (_Choice<_Ty&>._Strategy != _St::_None)
            _NODISCARD constexpr auto operator()(_Ty&& _Val) const noexcept(_Choice<_Ty&>._No_throw) {
                constexpr _St _Strat = _Choice<_Ty&>._Strategy;

                if constexpr (_Strat == _St::_Member) {
                    return _Val.rbegin();
                } else if constexpr (_Strat == _St::_Non_member) {
                    return rbegin(_Val);
                } else if constexpr (_Strat == _St::_Make_reverse) {
                    return _RPC make_reverse_iterator(_RANGES end(_Val));
                } else {
                    static_assert(_Always_false<_Ty>, "should be unreachable");
                }
            }
            // clang-format on
        };
    } // namespace _Rbegin

    inline namespace _Cpos {
        inline constexpr _Rbegin::_Cpo rbegin;
    }

    namespace _Rend {
        template <class _Ty>
        void rend(_Ty&) = delete;
        template <class _Ty>
        void rend(const _Ty&) = delete;

        template <class _Ty>
        concept _Has_member = requires(_Ty __t) {
            { _Fake_decay_copy(__t.rend()) } -> sentinel_for<decltype(_RANGES rbegin(__t))>;
        };

        template <class _Ty>
        concept _Has_ADL = _Has_class_or_enum_type<_Ty> && requires(_Ty __t) {
            { _Fake_decay_copy(rend(__t)) } -> sentinel_for<decltype(_RANGES rbegin(__t))>;
        };

        template <class _Ty>
        concept _Can_make_reverse = requires(_Ty __t) {
            { _RANGES begin(__t) } -> bidirectional_iterator;
            { _RANGES end(__t) } -> same_as<decltype(_RANGES begin(__t))>;
        };

        class _Cpo {
        private:
            enum class _St { _None, _Member, _Non_member, _Make_reverse };

            template <class _Ty>
            _NODISCARD static _CONSTEVAL _Choice_t<_St> _Choose() noexcept {
                _STL_INTERNAL_STATIC_ASSERT(is_lvalue_reference_v<_Ty>);
                if constexpr (_Has_member<_Ty>) {
                    return {_St::_Member, noexcept(_Fake_decay_copy(declval<_Ty>().rend()))};
                } else if constexpr (_Has_ADL<_Ty>) {
                    return {_St::_Non_member, noexcept(_Fake_decay_copy(rend(declval<_Ty>())))};
                } else if constexpr (_Can_make_reverse<_Ty>) {
                    return {
                        _St::_Make_reverse, noexcept(_RPC make_reverse_iterator(_RANGES begin(declval<_Ty>())))};
                } else {
                    return {_St::_None};
                }
            }

            template <class _Ty>
            static constexpr _Choice_t<_St> _Choice = _Choose<_Ty>();

        public:
            // clang-format off
            template <_Should_range_access _Ty>
                requires (_Choice<_Ty&>._Strategy != _St::_None)
            _NODISCARD constexpr auto operator()(_Ty&& _Val) const noexcept(_Choice<_Ty&>._No_throw) {
                constexpr _St _Strat = _Choice<_Ty&>._Strategy;

                if constexpr (_Strat == _St::_Member) {
                    return _Val.rend();
                } else if constexpr (_Strat == _St::_Non_member) {
                    return rend(_Val);
                } else if constexpr (_Strat == _St::_Make_reverse) {
                    return _RPC make_reverse_iterator(_RANGES begin(_Val));
                } else {
                    static_assert(_Always_false<_Ty>, "should be unreachable");
                }
            }
            // clang-format on
        };
    } // namespace _Rend

    inline namespace _Cpos {
        inline constexpr _Rend::_Cpo rend;
    }

    struct _Crbegin_fn {
        // clang-format off
        template <class _Ty, class _CTy = _Const_thru_ref<_Ty>>
        _NODISCARD constexpr auto operator()(_Ty&& _Val) const
            noexcept(noexcept(_RANGES rbegin(static_cast<_CTy&&>(_Val))))
            requires requires { _RANGES rbegin(static_cast<_CTy&&>(_Val)); } {
            return _RANGES rbegin(static_cast<_CTy&&>(_Val));
        }
        // clang-format on
    };

    inline namespace _Cpos {
        inline constexpr _Crbegin_fn crbegin;
    }

    struct _Crend_fn {
        // clang-format off
        template <class _Ty, class _CTy = _Const_thru_ref<_Ty>>
        _NODISCARD constexpr auto operator()(_Ty&& _Val) const
            noexcept(noexcept(_RANGES rend(static_cast<_CTy&&>(_Val))))
            requires requires { _RANGES rend(static_cast<_CTy&&>(_Val)); } {
            return _RANGES rend(static_cast<_CTy&&>(_Val));
        }
        // clang-format on
    };

    inline namespace _Cpos {
        inline constexpr _Crend_fn crend;
    }

    template <class>
    inline constexpr bool disable_sized_range = false;

    namespace _Size {
        template <class _Ty>
        void size(_Ty&) = delete;
        template <class _Ty>
        void size(const _Ty&) = delete;

        template <class _Ty, class _UnCV>
        concept _Has_member = !disable_sized_range<_UnCV> && requires(_Ty __t) {
            { _Fake_decay_copy(__t.size()) } -> _Integer_like;
        };

        template <class _Ty, class _UnCV>
        concept _Has_ADL = _Has_class_or_enum_type<_Ty> && !disable_sized_range<_UnCV> && requires(_Ty __t) {
            { _Fake_decay_copy(size(__t)) } -> _Integer_like;
        };

        template <class _Ty>
        concept _Can_difference = requires(_Ty __t) {
            { _RANGES begin(__t) } -> forward_iterator;
            { _RANGES end(__t) } -> sized_sentinel_for<decltype(_RANGES begin(__t))>;
        };

        class _Cpo {
        private:
            enum class _St { _None, _Array, _Member, _Non_member, _Subtract };

            template <class _Ty>
            _NODISCARD static _CONSTEVAL _Choice_t<_St> _Choose() noexcept {
                _STL_INTERNAL_STATIC_ASSERT(is_lvalue_reference_v<_Ty>);
                using _UnCV = remove_cvref_t<_Ty>;

                if constexpr (::std::is_array_v<_UnCV>) {
                    if constexpr (::std::extent_v<_UnCV> != 0) {
                        return {_St::_Array, true};
                    } else {
                        return {_St::_None};
                    }
                } else if constexpr (_Has_member<_Ty, _UnCV>) {
                    return {_St::_Member, noexcept(_Fake_decay_copy(declval<_Ty>().size()))};
                } else if constexpr (_Has_ADL<_Ty, _UnCV>) {
                    return {_St::_Non_member, noexcept(_Fake_decay_copy(size(declval<_Ty>())))};
                } else if constexpr (_Can_difference<_Ty>) {
                    return {_St::_Subtract,
                        noexcept(_RANGES end(declval<_Ty>()) - _RANGES begin(declval<_Ty>()))};
                } else {
                    return {_St::_None};
                }
            }

            template <class _Ty>
            static constexpr _Choice_t<_St> _Choice = _Choose<_Ty>();

        public:
            // clang-format off
            template <class _Ty>
                requires (_Choice<_Ty&>._Strategy != _St::_None)
            _NODISCARD constexpr auto operator()(_Ty&& _Val) const noexcept(_Choice<_Ty&>._No_throw) {
                constexpr _St _Strat = _Choice<_Ty&>._Strategy;

                if constexpr (_Strat == _St::_Array) {
                    // ::std::extent_v<remove_cvref_t<_Ty&>> reuses specializations from _Choose
                    return ::std::extent_v<remove_cvref_t<_Ty&>>;
                } else if constexpr (_Strat == _St::_Member) {
                    return _Val.size();
                } else if constexpr (_Strat == _St::_Non_member) {
                    return size(_Val);
                } else if constexpr (_Strat == _St::_Subtract) {
                    const auto _Delta = _RANGES end(_Val) - _RANGES begin(_Val);
                    return static_cast<_Make_unsigned_like_t<::std::remove_cv_t<decltype(_Delta)>>>(_Delta);
                } else {
                    static_assert(_Always_false<_Ty>, "should be unreachable");
                }
            }
            // clang-format on
        };
    } // namespace _Size

    inline namespace _Cpos {
        inline constexpr _Size::_Cpo size;
    }

    namespace _Empty {
        template <class _Ty>
        concept _Has_member = requires(_Ty __t) {
            static_cast<bool>(__t.empty());
        };

        template <class _Ty>
        concept _Has_size = requires(_Ty __t) {
            _RANGES size(__t);
        };

        template <class _Ty>
        concept _Can_begin_end = requires(_Ty __t) {
            { _RANGES begin(__t) } -> forward_iterator;
            _RANGES end(__t);
        };

        class _Cpo {
        private:
            enum class _St { _None, _Member, _Size, _Compare };

            template <class _Ty>
            _NODISCARD static _CONSTEVAL _Choice_t<_St> _Choose() noexcept {
                _STL_INTERNAL_STATIC_ASSERT(is_lvalue_reference_v<_Ty>);
                if constexpr (is_unbounded_array_v<remove_reference_t<_Ty>>) {
                    return {_St::_None};
                } else if constexpr (_Has_member<_Ty>) {
                    return {_St::_Member, noexcept(static_cast<bool>(declval<_Ty>().empty()))};
                } else if constexpr (_Has_size<_Ty>) {
                    return {_St::_Size, noexcept(_RANGES size(declval<_Ty>()))};
                } else if constexpr (_Can_begin_end<_Ty>) {
                    constexpr auto _Nothrow = noexcept(
                        static_cast<bool>(_RANGES begin(declval<_Ty>()) == _RANGES end(declval<_Ty>())));
                    return {_St::_Compare, _Nothrow};
                } else {
                    return {_St::_None};
                }
            }

            template <class _Ty>
            static constexpr _Choice_t<_St> _Choice = _Choose<_Ty>();

        public:
            // clang-format off
            template <class _Ty>
                requires (_Choice<_Ty&>._Strategy != _St::_None)
            _NODISCARD constexpr bool operator()(_Ty&& _Val) const noexcept(_Choice<_Ty&>._No_throw) {
                constexpr _St _Strat = _Choice<_Ty&>._Strategy;

                if constexpr (_Strat == _St::_Member) {
                    return static_cast<bool>(_Val.empty());
                } else if constexpr (_Strat == _St::_Size) {
                    return _RANGES size(_Val) == 0;
                } else if constexpr (_Strat == _St::_Compare) {
                    return static_cast<bool>(_RANGES begin(_Val) == _RANGES end(_Val));
                } else {
                    static_assert(_Always_false<_Ty>, "should be unreachable");
                }
            }
            // clang-format on
        };
    } // namespace _Empty

    inline namespace _Cpos {
        inline constexpr _Empty::_Cpo empty;
    }

    namespace _Data {
        template <class _Ty>
        concept _Points_to_object = ::std::is_pointer_v<_Ty> && ::std::is_object_v<::std::remove_pointer_t<_Ty>>;

        template <class _Ty>
        concept _Has_member = requires(_Ty __t) {
            { _Fake_decay_copy(__t.data()) } -> _Points_to_object;
        };

        template <class _Ty>
        concept _Has_contiguous_iterator = requires(_Ty __t) {
            { _RANGES begin(__t) } -> contiguous_iterator;
        };

        class _Cpo {
        private:
            enum class _St { _None, _Member, _Address };

            template <class _Ty>
            _NODISCARD static _CONSTEVAL _Choice_t<_St> _Choose() noexcept {
                _STL_INTERNAL_STATIC_ASSERT(is_lvalue_reference_v<_Ty>);
                if constexpr (_Has_member<_Ty>) {
                    return {_St::_Member, noexcept(declval<_Ty>().data())};
                } else if constexpr (_Has_contiguous_iterator<_Ty>) {
                    return {_St::_Address, noexcept(_RPC to_address(_RANGES begin(declval<_Ty>())))};
                } else {
                    return {_St::_None};
                }
            }

            template <class _Ty>
            static constexpr _Choice_t<_St> _Choice = _Choose<_Ty>();

        public:
            // clang-format off
            template <_Should_range_access _Ty>
                requires (_Choice<_Ty&>._Strategy != _St::_None)
            _NODISCARD constexpr auto operator()(_Ty&& _Val) const noexcept(_Choice<_Ty&>._No_throw) {
                constexpr _St _Strat = _Choice<_Ty&>._Strategy;

                if constexpr (_Strat == _St::_Member) {
                    return _Val.data();
                } else if constexpr (_Strat == _St::_Address) {
                    return _RPC to_address(_RANGES begin(_Val));
                } else {
                    static_assert(_Always_false<_Ty>, "should be unreachable");
                }
            }
            // clang-format on
        };
    } // namespace _Data

    inline namespace _Cpos {
        inline constexpr _Data::_Cpo data;
    }

    struct _Cdata_fn {
        // clang-format off
        template <class _Ty, class _CTy = _Const_thru_ref<_Ty>>
        _NODISCARD constexpr auto operator()(_Ty&& _Val) const
            noexcept(noexcept(_RANGES data(static_cast<_CTy&&>(_Val))))
            requires requires { _RANGES data(static_cast<_CTy&&>(_Val)); } {
            return _RANGES data(static_cast<_CTy&&>(_Val));
        }
        // clang-format on
    };

    inline namespace _Cpos {
        inline constexpr _Cdata_fn cdata;
    }

    template <class _Rng>
    concept sized_range = range<_Rng> && requires(_Rng& __r) {
        _RANGES size(__r);
    };

    template <sized_range _Rng>
    using range_size_t = decltype(_RANGES size(declval<_Rng&>()));

    struct view_base {};

    template <template <class...> class _Template, class... _Args>
    void _Derived_from_specialization_impl(const _Template<_Args...>&);

    template <class _Ty, template <class...> class _Template>
    concept _Derived_from_specialization_of = requires(const _Ty& _Obj) {
        _Derived_from_specialization_impl<_Template>(_Obj);
    };

    template <class _Derived>
        requires is_class_v<_Derived> && same_as<_Derived, ::std::remove_cv_t<_Derived>>
    class view_interface;

    template <class _Ty>
    inline constexpr bool enable_view =
        derived_from<_Ty, view_base> || _Derived_from_specialization_of<_Ty, view_interface>;

#ifdef __cpp_lib_ranges // TRANSITION, GH-1814
    template <class _Ty>
    concept view = range<_Ty> && movable<_Ty> && enable_view<_Ty>;
#endif // TRANSITION, GH-1814

    template <class _Rng, class _Ty>
    concept output_range = range<_Rng> && output_iterator<iterator_t<_Rng>, _Ty>;

    template <class _Rng>
    concept input_range = range<_Rng> && input_iterator<iterator_t<_Rng>>;

    template <class _Rng>
    concept forward_range = range<_Rng> && forward_iterator<iterator_t<_Rng>>;

    template <class _Rng>
    concept bidirectional_range = range<_Rng> && bidirectional_iterator<iterator_t<_Rng>>;

    template <class _Rng>
    concept random_access_range = range<_Rng> && random_access_iterator<iterator_t<_Rng>>;

    template <class _Rng>
    concept contiguous_range = range<_Rng> && contiguous_iterator<iterator_t<_Rng>> && requires(_Rng& __r) {
        { _RANGES data(__r) } -> same_as<add_pointer_t<range_reference_t<_Rng>>>;
    };

    class _Not_quite_object {
    public:
        // Some overload sets in the library have the property that their constituent function templates are not visible
        // to argument-dependent name lookup (ADL) and that they inhibit ADL when found via unqualified name lookup.
        // This property allows these overload sets to be implemented as function objects. We derive such function
        // objects from this type to remove some typical object-ish behaviors which helps users avoid depending on their
        // non-specified object-ness.

        struct _Construct_tag {
            explicit _Construct_tag() = default;
        };

        _Not_quite_object() = delete;

        constexpr explicit _Not_quite_object(_Construct_tag) noexcept {}

        _Not_quite_object(const _Not_quite_object&) = delete;
        _Not_quite_object& operator=(const _Not_quite_object&) = delete;

        void operator&() const = delete;

    protected:
        ~_Not_quite_object() = default;
    };

    class _Advance_fn : private _Not_quite_object {
    public:
        using _Not_quite_object::_Not_quite_object;

        template <input_or_output_iterator _It>
        constexpr void operator()(_It& _Where, iter_difference_t<_It> _Off) const {
            if constexpr (random_access_iterator<_It>) {
                _Where += _Off;
            } else {
                if constexpr (!bidirectional_iterator<_It>) {
                    _STL_ASSERT(_Off >= 0, "negative advance of non-bidirectional iterator");
                }

                decltype(auto) _UWhere      = _Get_unwrapped_n(::std::move(_Where), _Off);
                constexpr bool _Need_rewrap = !::std::is_reference_v<decltype(_Get_unwrapped_n(::std::move(_Where), _Off))>;

                if constexpr (bidirectional_iterator<_It>) {
                    for (; _Off < 0; ++_Off) {
                        --_UWhere;
                    }
                }

                for (; _Off > 0; --_Off) {
                    ++_UWhere;
                }

                if constexpr (_Need_rewrap) {
                    _Seek_wrapped(_Where, ::std::move(_UWhere));
                }
            }
        }

        template <input_or_output_iterator _It, sentinel_for<_It> _Se>
        constexpr void operator()(_It& _Where, _Se _Last) const {
            if constexpr (assignable_from<_It&, _Se>) {
                _Where = static_cast<_Se&&>(_Last);
            } else if constexpr (sized_sentinel_for<_Se, _It>) {
                (*this)(_Where, _Last - _Where);
            } else {
                _Adl_verify_range(_Where, _Last);

                decltype(auto) _UWhere      = _Get_unwrapped(static_cast<_It&&>(_Where));
                constexpr bool _Need_rewrap = !::std::is_reference_v<decltype(_Get_unwrapped(static_cast<_It&&>(_Where)))>;
                decltype(auto) _ULast       = _Get_unwrapped(static_cast<_Se&&>(_Last));

                while (_UWhere != _ULast) {
                    ++_UWhere;
                }

                if constexpr (_Need_rewrap) {
                    _Seek_wrapped(_Where, ::std::move(_UWhere));
                }
            }
        }

        template <input_or_output_iterator _It, sentinel_for<_It> _Se>
        constexpr iter_difference_t<_It> operator()(_It& _Where, iter_difference_t<_It> _Off, _Se _Last) const {
            if constexpr (sized_sentinel_for<_Se, _It>) {
                const iter_difference_t<_It> _Delta = _Last - _Where;
                if ((_Off < 0 && _Off <= _Delta) || (_Off > 0 && _Off >= _Delta)) {
                    if constexpr (assignable_from<_It&, _Se>) {
                        _Where = static_cast<_Se&&>(_Last);
                    } else {
                        (*this)(_Where, _Delta);
                    }
                    return _Off - _Delta;
                }

                (*this)(_Where, _Off);
                return 0;
            } else {
                // performance note: develop unwrapping technology for (i, n, s)?
                if constexpr (bidirectional_iterator<_It>) {
                    for (; _Off < 0 && _Where != _Last; ++_Off) {
                        --_Where;
                    }
                } else {
                    _STL_ASSERT(_Off >= 0, "negative advance of non-bidirectional iterator");
                }

                for (; _Off > 0 && _Where != _Last; --_Off) {
                    ++_Where;
                }

                return _Off;
            }
        }
    };

    inline constexpr _Advance_fn advance{_Not_quite_object::_Construct_tag{}};

    class _Distance_fn : private _Not_quite_object {
    public:
        using _Not_quite_object::_Not_quite_object;

        template <input_or_output_iterator _It, sentinel_for<_It> _Se>
        _NODISCARD constexpr iter_difference_t<_It> operator()(_It _First, _Se _Last) const
            noexcept(_Nothrow_distance<_It, _Se>) /* strengthened */ {
            if constexpr (sized_sentinel_for<_Se, _It>) {
                return _Last - _First;
            } else {
                _Adl_verify_range(_First, _Last);
                return _Distance_unchecked(_Get_unwrapped(::std::move(_First)), _Get_unwrapped(::std::move(_Last)));
            }
        }

        template <range _Rng>
        _NODISCARD constexpr range_difference_t<_Rng> operator()(_Rng&& _Range) const
            noexcept(_Nothrow_size<_Rng>) /* strengthened */ {
            if constexpr (sized_range<_Rng>) {
                return static_cast<range_difference_t<_Rng>>(_RANGES size(_Range));
            } else {
                return _Distance_unchecked(_Ubegin(_Range), _Uend(_Range));
            }
        }

    private:
        template <class _It, class _Se>
        _NODISCARD static constexpr iter_difference_t<_It> _Distance_unchecked(_It _First, const _Se _Last) noexcept(
            _Nothrow_distance<_It, _Se>) {
            _STL_INTERNAL_STATIC_ASSERT(input_or_output_iterator<_It>);
            _STL_INTERNAL_STATIC_ASSERT(sentinel_for<_Se, _It>);

            iter_difference_t<_It> _Count = 0;
            for (; _First != _Last; ++_First) {
                ++_Count;
            }

            return _Count;
        }

        template <class _It, class _Se>
        static constexpr bool _Nothrow_distance = noexcept(
            noexcept(++declval<_Unwrapped_t<_It>&>() != declval<const _Unwrapped_t<_Se>&>()));
        template <class _It, sized_sentinel_for<_It> _Se>
        static constexpr bool _Nothrow_distance<_It, _Se> = noexcept(
            noexcept(declval<_Se&>() - declval<_It&>()));

        template <class _Rng>
        static constexpr bool _Nothrow_size = _Nothrow_distance<iterator_t<_Rng>, sentinel_t<_Rng>>;
        template <sized_range _Rng>
        static constexpr bool _Nothrow_size<_Rng> = noexcept(_RANGES size(declval<_Rng&>()));
    };

    inline constexpr _Distance_fn distance{_Not_quite_object::_Construct_tag{}};

    class _Ssize_fn {
    public:
        // clang-format off
        template <class _Rng>
        _NODISCARD constexpr auto operator()(_Rng&& _Range) const requires requires { _RANGES size(_Range); } {
            using _Sty = _Make_signed_like_t<decltype(_RANGES size(_Range))>;
            using _Ty  = ::std::common_type_t<::std::conditional_t<::std::is_integral_v<_Sty>, ptrdiff_t, _Sty>, _Sty>;
            return static_cast<_Ty>(_RANGES size(_Range));
        }
        // clang-format on
    };

    inline namespace _Cpos {
        inline constexpr _Ssize_fn ssize;
    }

    class _Next_fn : private _Not_quite_object {
    public:
        using _Not_quite_object::_Not_quite_object;

        template <input_or_output_iterator _It>
        _NODISCARD constexpr _It operator()(_It _Where) const {
            ++_Where;
            return _Where;
        }

        template <input_or_output_iterator _It>
        _NODISCARD constexpr _It operator()(_It _Where, const iter_difference_t<_It> _Off) const {
            _RANGES advance(_Where, _Off);
            return _Where;
        }

        template <input_or_output_iterator _It, sentinel_for<_It> _Se>
        _NODISCARD constexpr _It operator()(_It _Where, _Se _Last) const {
            _RANGES advance(_Where, static_cast<_Se&&>(_Last));
            return _Where;
        }

        template <input_or_output_iterator _It, sentinel_for<_It> _Se>
        _NODISCARD constexpr _It operator()(_It _Where, const iter_difference_t<_It> _Off, _Se _Last) const {
            _RANGES advance(_Where, _Off, static_cast<_Se&&>(_Last));
            return _Where;
        }
    };

    inline constexpr _Next_fn next{_Not_quite_object::_Construct_tag{}};

    class _Prev_fn : private _Not_quite_object {
    public:
        using _Not_quite_object::_Not_quite_object;

        template <bidirectional_iterator _It>
        _NODISCARD constexpr _It operator()(_It _Where) const {
            --_Where;
            return _Where;
        }

        template <bidirectional_iterator _It>
        _NODISCARD constexpr _It operator()(_It _Where, const iter_difference_t<_It> _Off) const {
            _STL_ASSERT(_Off != _Min_possible_v<iter_difference_t<_It>>, "integer overflow");
            _RANGES advance(_Where, -_Off);
            return _Where;
        }

        template <bidirectional_iterator _It>
        _NODISCARD constexpr _It operator()(_It _Where, const iter_difference_t<_It> _Off, _It _Last) const {
            _STL_ASSERT(_Off != _Min_possible_v<iter_difference_t<_It>>, "integer overflow");
            _RANGES advance(_Where, -_Off, static_cast<_It&&>(_Last));
            return _Where;
        }
    };

    inline constexpr _Prev_fn prev{_Not_quite_object::_Construct_tag{}};

    template <forward_iterator _It, sentinel_for<_It> _Se>
    _NODISCARD constexpr _It _Find_last_iterator(
        const _It& _First, const _Se& _Last, const iter_difference_t<_It> _Count) {
        // Find the iterator in [_First, _Last) (of length _Count) which equals _Last
        _STL_INTERNAL_CHECK(_RANGES distance(_First, _Last) == _Count);
        if constexpr (::std::is_same_v<_It, _Se>) {
            return _Last;
        } else {
            return _RANGES next(_First, _Count);
        }
    }

    struct ::std::equal_to {
        // clang-format off
        template <class _Ty1, class _Ty2>
            requires equality_comparable_with<_Ty1, _Ty2>
        _NODISCARD constexpr bool operator()(_Ty1&& _Left, _Ty2&& _Right) const noexcept(noexcept(
            static_cast<bool>(static_cast<_Ty1&&>(_Left) == static_cast<_Ty2&&>(_Right)))) /* strengthened */ {
            return static_cast<bool>(static_cast<_Ty1&&>(_Left) == static_cast<_Ty2&&>(_Right));
        }
        // clang-format on

        using is_transparent = int;
    };

    struct ::std::less {
        // clang-format off
        template <class _Ty1, class _Ty2>
            requires totally_ordered_with<_Ty1, _Ty2>
        _NODISCARD constexpr bool operator()(_Ty1&& _Left, _Ty2&& _Right) const noexcept(noexcept(
            static_cast<bool>(static_cast<_Ty1&&>(_Left) < static_cast<_Ty2&&>(_Right)))) /* strengthened */ {
            return static_cast<bool>(static_cast<_Ty1&&>(_Left) < static_cast<_Ty2&&>(_Right));
        }
        // clang-format on

        using is_transparent = int;
    };

    struct ::std::greater {
        // clang-format off
        template <class _Ty1, class _Ty2>
            requires totally_ordered_with<_Ty1, _Ty2>
        _NODISCARD constexpr bool operator()(_Ty1&& _Left, _Ty2&& _Right) const noexcept(noexcept(
            static_cast<bool>(static_cast<_Ty2&&>(_Right) < static_cast<_Ty1&&>(_Left)))) /* strengthened */ {
            return static_cast<bool>(static_cast<_Ty2&&>(_Right) < static_cast<_Ty1&&>(_Left));
        }
        // clang-format on

        using is_transparent = int;
    };

    template <class _Rng>
    concept common_range = range<_Rng> && same_as<iterator_t<_Rng>, sentinel_t<_Rng>>;

    template <class _Ty>
    concept _Can_empty = requires(_Ty __t) {
        _RANGES empty(__t);
    };

    // clang-format off
    template <class _Derived>
        requires is_class_v<_Derived> && same_as<_Derived, ::std::remove_cv_t<_Derived>>
    class view_interface {
    private:
        _NODISCARD constexpr _Derived& _Cast() noexcept {
            static_assert(derived_from<_Derived, view_interface>,
                "view_interface's template argument D must derive from view_interface<D> (N4849 [view.interface]/2).");
#ifdef __cpp_lib_ranges // TRANSITION, GH-1814
            static_assert(view<_Derived>,
                "view_interface's template argument must model the view concept (N4849 [view.interface]/2).");
#endif // TRANSITION, GH-1814
            return static_cast<_Derived&>(*this);
        }

        _NODISCARD constexpr const _Derived& _Cast() const noexcept {
            static_assert(derived_from<_Derived, view_interface>,
                "view_interface's template argument D must derive from view_interface<D> (N4849 [view.interface]/2).");
#ifdef __cpp_lib_ranges // TRANSITION, GH-1814
            static_assert(view<_Derived>,
                "view_interface's template argument must model the view concept (N4849 [view.interface]/2).");
#endif // TRANSITION, GH-1814
            return static_cast<const _Derived&>(*this);
        }

    public:
#ifdef __clang__ // TRANSITION, LLVM-44833
        template <class _Dx = _Derived>
        _NODISCARD constexpr bool empty() requires forward_range<_Dx>
#else // ^^^ workaround / no workaround vvv
        _NODISCARD constexpr bool empty() requires forward_range<_Derived>
#endif // TRANSITION, LLVM-44833
        {
            auto& _Self = _Cast();
            return _RANGES begin(_Self) == _RANGES end(_Self);
        }

#ifdef __clang__ // TRANSITION, LLVM-44833
        template <class _Dx = _Derived>
        _NODISCARD constexpr bool empty() const requires forward_range<const _Dx>
#else // ^^^ workaround / no workaround vvv
        _NODISCARD constexpr bool empty() const requires forward_range<const _Derived>
#endif // TRANSITION, LLVM-44833
        {
            auto& _Self = _Cast();
            return _RANGES begin(_Self) == _RANGES end(_Self);
        }

#ifdef __clang__ // TRANSITION, LLVM-44833
        template <class _Dx = _Derived>
        constexpr explicit operator bool() requires _Can_empty<_Dx>
#else // ^^^ workaround / no workaround vvv
        constexpr explicit operator bool() requires _Can_empty<_Derived>
#endif // TRANSITION, LLVM-44833
        {
            return !_RANGES empty(_Cast());
        }

#ifdef __clang__ // TRANSITION, LLVM-44833
        template <class _Dx = _Derived>
        constexpr explicit operator bool() const requires _Can_empty<const _Dx>
#else // ^^^ workaround / no workaround vvv
        constexpr explicit operator bool() const requires _Can_empty<const _Derived>
#endif // TRANSITION, LLVM-44833
        {
            return !_RANGES empty(_Cast());
        }

#ifdef __clang__ // TRANSITION, LLVM-44833
        template <class _Dx = _Derived>
        _NODISCARD constexpr auto data() requires contiguous_iterator<iterator_t<_Dx>>
#else // ^^^ workaround / no workaround vvv
        _NODISCARD constexpr auto data() requires contiguous_iterator<iterator_t<_Derived>>
#endif // TRANSITION, LLVM-44833
        {
            return _RPC to_address(_RANGES begin(_Cast()));
        }

#ifdef __clang__ // TRANSITION, LLVM-44833
        template <class _Dx = _Derived>
        _NODISCARD constexpr auto data() const
            requires range<const _Dx> && contiguous_iterator<iterator_t<const _Dx>>
#else // ^^^ workaround / no workaround vvv
        _NODISCARD constexpr auto data() const
            requires range<const _Derived> && contiguous_iterator<iterator_t<const _Derived>>
#endif // TRANSITION, LLVM-44833
        {
            return _RPC to_address(_RANGES begin(_Cast()));
        }

#ifdef __clang__ // TRANSITION, LLVM-44833
        template <class _Dx = _Derived>
        _NODISCARD constexpr auto size()
            requires forward_range<_Dx> && sized_sentinel_for<sentinel_t<_Dx>, iterator_t<_Dx>>
#else // ^^^ workaround / no workaround vvv
        _NODISCARD constexpr auto size()
            requires forward_range<_Derived> && sized_sentinel_for<sentinel_t<_Derived>, iterator_t<_Derived>>
#endif // TRANSITION, LLVM-44833
        {
            auto& _Self = _Cast();
            return _RANGES end(_Self) - _RANGES begin(_Self);
        }

#ifdef __clang__ // TRANSITION, LLVM-44833
        template <class _Dx = _Derived>
        _NODISCARD constexpr auto size() const requires forward_range<const _Dx>
            && sized_sentinel_for<sentinel_t<const _Dx>, iterator_t<const _Dx>>
#else // ^^^ workaround / no workaround vvv
        _NODISCARD constexpr auto size() const requires forward_range<const _Derived>
            && sized_sentinel_for<sentinel_t<const _Derived>, iterator_t<const _Derived>>
#endif // TRANSITION, LLVM-44833
        {
            auto& _Self = _Cast();
            return _RANGES end(_Self) - _RANGES begin(_Self);
        }

#ifdef __clang__ // TRANSITION, LLVM-44833
        template <class _Dx = _Derived>
        _NODISCARD constexpr decltype(auto) front() requires forward_range<_Dx>
#else // ^^^ workaround / no workaround vvv
        _NODISCARD constexpr decltype(auto) front() requires forward_range<_Derived>
#endif // TRANSITION, LLVM-44833
        {
            auto& _Self = _Cast();
#if _CONTAINER_DEBUG_LEVEL > 0
            _STL_VERIFY(!_RANGES empty(_Self), "front called on empty view_interface");
#endif // _CONTAINER_DEBUG_LEVEL > 0
            return *_RANGES begin(_Self);
        }

#ifdef __clang__ // TRANSITION, LLVM-44833
        template <class _Dx = _Derived>
        _NODISCARD constexpr decltype(auto) front() const requires forward_range<const _Dx>
#else // ^^^ workaround / no workaround vvv
        _NODISCARD constexpr decltype(auto) front() const requires forward_range<const _Derived>
#endif // TRANSITION, LLVM-44833
        {
            auto& _Self = _Cast();
#if _CONTAINER_DEBUG_LEVEL > 0
            _STL_VERIFY(!_RANGES empty(_Self), "front called on empty view_interface");
#endif // _CONTAINER_DEBUG_LEVEL > 0
            return *_RANGES begin(_Self);
        }

#ifdef __clang__ // TRANSITION, LLVM-44833
        template <class _Dx = _Derived>
        _NODISCARD constexpr decltype(auto) back() requires bidirectional_range<_Dx> && common_range<_Dx>
#else // ^^^ workaround / no workaround vvv
        _NODISCARD constexpr decltype(auto) back() requires bidirectional_range<_Derived> && common_range<_Derived>
#endif // TRANSITION, LLVM-44833
        {
            auto& _Self = _Cast();
#if _CONTAINER_DEBUG_LEVEL > 0
            _STL_VERIFY(!_RANGES empty(_Self), "back called on empty view_interface");
#endif // _CONTAINER_DEBUG_LEVEL > 0
            auto _Last = _RANGES end(_Self);
            return *--_Last;
        }

#ifdef __clang__ // TRANSITION, LLVM-44833
        template <class _Dx = _Derived>
        _NODISCARD constexpr decltype(auto) back() const
            requires bidirectional_range<const _Dx> && common_range<const _Dx>
#else // ^^^ workaround / no workaround vvv
        _NODISCARD constexpr decltype(auto) back() const
            requires bidirectional_range<const _Derived> && common_range<const _Derived>
#endif // TRANSITION, LLVM-44833
        {
            auto& _Self = _Cast();
#if _CONTAINER_DEBUG_LEVEL > 0
            _STL_VERIFY(!_RANGES empty(_Self), "back called on empty view_interface");
#endif // _CONTAINER_DEBUG_LEVEL > 0
            auto _Last = _RANGES end(_Self);
            return *--_Last;
        }

        template <random_access_range _Rng = _Derived>
        _NODISCARD constexpr decltype(auto) operator[](const range_difference_t<_Rng> _Idx) {
            auto& _Self = _Cast();
#if _CONTAINER_DEBUG_LEVEL > 0
            if constexpr (sized_range<_Derived>) {
                using _U_diff = _Make_unsigned_like_t<range_difference_t<_Rng>>;
                _STL_VERIFY(static_cast<_U_diff>(_Idx) < static_cast<_U_diff>(_RANGES size(_Self)),
                    "index out of range for view_interface");
            }
#endif // _CONTAINER_DEBUG_LEVEL > 0
            return _RANGES begin(_Self)[_Idx];
        }

        template <random_access_range _Rng = const _Derived>
        _NODISCARD constexpr decltype(auto) operator[](const range_difference_t<_Rng> _Idx) const {
            auto& _Self = _Cast();
#if _CONTAINER_DEBUG_LEVEL > 0
            if constexpr (sized_range<_Derived>) {
                using _U_diff = _Make_unsigned_like_t<range_difference_t<_Rng>>;
                _STL_VERIFY(static_cast<_U_diff>(_Idx) < static_cast<_U_diff>(_RANGES size(_Self)),
                    "index out of range for view_interface");
            }
#endif // _CONTAINER_DEBUG_LEVEL > 0
            return _RANGES begin(_Self)[_Idx];
        }
    };

    enum class subrange_kind : bool { unsized, sized };
    // clang-format on
} // namespace ranges

// These declarations must be visible to qualified name lookup for _RPC get in _Pair_like below, even if <tuple> hasn't
// yet been included.
template <size_t _Index, class... _Types>
_NODISCARD constexpr tuple_element_t<_Index, tuple<_Types...>>& get(tuple<_Types...>& _Tuple) noexcept;
template <size_t _Index, class... _Types>
_NODISCARD constexpr const tuple_element_t<_Index, tuple<_Types...>>& get(const tuple<_Types...>& _Tuple) noexcept;
template <size_t _Index, class... _Types>
_NODISCARD constexpr tuple_element_t<_Index, tuple<_Types...>>&& get(tuple<_Types...>&& _Tuple) noexcept;
template <size_t _Index, class... _Types>
_NODISCARD constexpr const tuple_element_t<_Index, tuple<_Types...>>&& get(const tuple<_Types...>&& _Tuple) noexcept;

namespace ranges {
    // clang-format off
    template <class _From, class _To>
    concept _Uses_nonqualification_pointer_conversion =
        ::std::is_pointer_v<_From> && ::std::is_pointer_v<_To>
        && !convertible_to<::std::remove_pointer_t<_From>(*)[], ::std::remove_pointer_t<_To>(*)[]>;

    template <class _From, class _To>
    concept _Convertible_to_non_slicing = convertible_to<_From, _To>
        && !_Uses_nonqualification_pointer_conversion<::std::decay_t<_From>, ::std::decay_t<_To>>;

    template <class _Ty>
    concept _Pair_like = !::std::is_reference_v<_Ty> && requires(_Ty __t) {
        typename tuple_size<_Ty>::type;
        requires derived_from<tuple_size<_Ty>, integral_constant<size_t, 2>>;
        typename tuple_element_t<0, remove_const_t<_Ty>>;
        typename tuple_element_t<1, remove_const_t<_Ty>>;
        { _RPC get<0>(__t) } -> convertible_to<const tuple_element_t<0, _Ty>&>;
        { _RPC get<1>(__t) } -> convertible_to<const tuple_element_t<1, _Ty>&>;
    };

    template <class _Ty, class _First, class _Second>
    concept _Pair_like_convertible_from = !range<_Ty> && _Pair_like<_Ty>
        && constructible_from<_Ty, _First, _Second>
        && _Convertible_to_non_slicing<_First, tuple_element_t<0, _Ty>>
        && convertible_to<_Second, tuple_element_t<1, _Ty>>;

    template <input_or_output_iterator _It, sentinel_for<_It> _Se = _It,
        subrange_kind _Ki = sized_sentinel_for<_Se, _It> ? subrange_kind::sized : subrange_kind::unsized>
        requires (_Ki == subrange_kind::sized || !sized_sentinel_for<_Se, _It>)
    class subrange;
    // clang-format on

    template <class _It, class _Se, subrange_kind _Ki,
        bool _Store = _Ki == subrange_kind::sized && !sized_sentinel_for<_Se, _It>>
    class _Subrange_base : public view_interface<subrange<_It, _Se, _Ki>> { // TRANSITION, [[no_unique_address]]
    protected:
        using _Size_type                  = _Make_unsigned_like_t<iter_difference_t<_It>>;
        static constexpr bool _Store_size = true;

        _Size_type _Size = 0;

    public:
        _Subrange_base() = default;
        constexpr explicit _Subrange_base(const _Size_type& _Size_) noexcept : _Size(_Size_) {}
    };

    template <class _It, class _Se, subrange_kind _Ki>
    class _Subrange_base<_It, _Se, _Ki, false> : public view_interface<subrange<_It, _Se, _Ki>> {
    protected:
        using _Size_type                  = _Make_unsigned_like_t<iter_difference_t<_It>>;
        static constexpr bool _Store_size = false;

    public:
        _Subrange_base() = default;
        constexpr explicit _Subrange_base(const _Size_type&) noexcept {}
    };

    // clang-format off
    template <input_or_output_iterator _It, sentinel_for<_It> _Se, subrange_kind _Ki>
        requires (_Ki == subrange_kind::sized || !sized_sentinel_for<_Se, _It>)
    class subrange : public _Subrange_base<_It, _Se, _Ki> {
        // clang-format on
    private:
        using _Mybase = _Subrange_base<_It, _Se, _Ki>;
        using _Mybase::_Store_size;
        using typename _Mybase::_Size_type;

        // TRANSITION, [[no_unique_address]]:
        /* [[no_unique_address]] */ _It _First{};
        /* [[no_unique_address]] */ _Se _Last{};
        // [[no_unique_address]] ::std::conditional_t<_Store_size, _Size_type, _Nil> _Size{};

        template <class _Rng>
        constexpr subrange(true_type, _Rng&& _Val) : subrange(::std::forward<_Rng>(_Val), _RANGES size(_Val)) {
            // delegation target for subrange(_Rng&&) when we must store the range size
            _STL_INTERNAL_STATIC_ASSERT(_Store_size);
        }

        template <class _Rng>
        constexpr subrange(false_type, _Rng&& _Val) : subrange(_RANGES begin(_Val), _RANGES end(_Val)) {
            // delegation target for subrange(_Rng&&) when we need not store the range size
            _STL_INTERNAL_STATIC_ASSERT(!_Store_size);
        }

    public:
        // clang-format off
        subrange() requires default_initializable<_It> = default;

#if !defined(_MSVC_INTERNAL_TESTING) && !defined(__clang__) && !defined(__EDG__) // TRANSITION, VS 17.1p3
        // This was originally annotated as a workaround for DevCom-1331017, but the problem it corrects continued to
        // manifest after that bug was fixed. We reduced a repro to file an additional bug, but the underlying issue had
        // already been fixed in the internal compiler (see GH-2326).
        constexpr subrange(const subrange&) = default;
        constexpr subrange(subrange&&)      = default;
        constexpr subrange& operator=(const subrange&) = default;
        constexpr subrange& operator=(subrange&&) = default;
#endif // ^^^ workaround ^^^

        template <_Convertible_to_non_slicing<_It> _It2>
        constexpr subrange(_It2 _First_, _Se _Last_) requires (!_Store_size)
            : _First(::std::move(_First_)), _Last(::std::move(_Last_)) {}

        template <_Convertible_to_non_slicing<_It> _It2>
        constexpr subrange(_It2 _First_, _Se _Last_, const _Size_type _Size_) requires (_Ki == subrange_kind::sized)
            : _Mybase(_Size_), _First(::std::move(_First_)), _Last(::std::move(_Last_)) {
            if constexpr (sized_sentinel_for<_Se, _It>) {
                _STL_ASSERT(_Size_ == static_cast<_Size_type>(_Last - _First),
                    "This constructor's third argument should be equal to the distance "
                    "between the first and second arguments (N4849 [range.subrange.ctor]/3).");
            }
        }

        template <_Not_same_as<subrange> _Rng>
            requires borrowed_range<_Rng>
                && _Convertible_to_non_slicing<iterator_t<_Rng>, _It>
                && convertible_to<sentinel_t<_Rng>, _Se>
        constexpr subrange(_Rng&& _Val) requires (!_Store_size || sized_range<_Rng>)
            : subrange{::std::bool_constant<_Store_size>{}, ::std::forward<_Rng>(_Val)} {}

        template <borrowed_range _Rng>
            requires _Convertible_to_non_slicing<iterator_t<_Rng>, _It> && convertible_to<sentinel_t<_Rng>, _Se>
        constexpr subrange(_Rng&& _Val, const _Size_type _Count) requires (_Ki == subrange_kind::sized)
            : subrange{_RANGES begin(_Val), _RANGES end(_Val), _Count} {}

        template <_Not_same_as<subrange> _Pair_like>
            requires _Pair_like_convertible_from<_Pair_like, const _It&, const _Se&>
        constexpr operator _Pair_like() const {
            return _Pair_like(_First, _Last);
        }
        // clang-format on

        _NODISCARD constexpr _It begin() const requires copyable<_It> {
            return _First;
        }
        // clang-format off
        _NODISCARD constexpr _It begin() requires (!copyable<_It>) {
            // clang-format on
            return ::std::move(_First);
        }

        _NODISCARD constexpr _Se end() const {
            return _Last;
        }

        _NODISCARD constexpr bool empty() const {
            return _First == _Last;
        }

        // clang-format off
        _NODISCARD constexpr _Size_type size() const requires (_Ki == subrange_kind::sized) {
            // clang-format on
            if constexpr (_Store_size) {
                return this->_Size;
            } else {
                return static_cast<_Size_type>(_Last - _First);
            }
        }

        _NODISCARD constexpr subrange next() const& requires forward_iterator<_It> {
            auto _Tmp = *this;
            if (_Tmp._First != _Tmp._Last) {
                ++_Tmp._First;
                if constexpr (_Store_size) {
                    --_Tmp._Size;
                }
            }
            return _Tmp;
        }
        _NODISCARD constexpr subrange next(const iter_difference_t<_It> _Count) const& requires forward_iterator<_It> {
            auto _Tmp = *this;
            _Tmp.advance(_Count);
            return _Tmp;
        }

        _NODISCARD constexpr subrange next() && {
            if (_First != _Last) {
                ++_First;
                if constexpr (_Store_size) {
                    --this->_Size;
                }
            }
            return ::std::move(*this);
        }
        _NODISCARD constexpr subrange next(const iter_difference_t<_It> _Count) && {
            advance(_Count);
            return ::std::move(*this);
        }

        _NODISCARD constexpr subrange prev() const requires bidirectional_iterator<_It> {
            auto _Tmp = *this;
            --_Tmp._First;
            if constexpr (_Store_size) {
                ++_Tmp._Size;
            }
            return _Tmp;
        }
        _NODISCARD constexpr subrange prev(
            const iter_difference_t<_It> _Count) const requires bidirectional_iterator<_It> {
            auto _Tmp = *this;
            _Tmp.advance(-_Count);
            return _Tmp;
        }

        constexpr subrange& advance(const iter_difference_t<_It> _Count) {
            if constexpr (bidirectional_iterator<_It>) {
                if (_Count < 0) {
                    _RANGES advance(_First, _Count);
                    if constexpr (_Store_size) {
                        this->_Size += static_cast<_Size_type>(-_Count);
                    }
                    return *this;
                }
            }

            const auto _Remainder = _RANGES advance(_First, _Count, _Last);
            if constexpr (_Store_size) {
                this->_Size -= static_cast<_Size_type>(_Count - _Remainder);
            }
            return *this;
        }
    };

    template <input_or_output_iterator _It, sentinel_for<_It> _Se>
    subrange(_It, _Se) -> subrange<_It, _Se>;

    template <input_or_output_iterator _It, sentinel_for<_It> _Se>
    subrange(_It, _Se, _Make_unsigned_like_t<iter_difference_t<_It>>) -> subrange<_It, _Se, subrange_kind::sized>;

    template <borrowed_range _Rng>
    subrange(_Rng&&) -> subrange<iterator_t<_Rng>, sentinel_t<_Rng>,
        (sized_range<_Rng> || sized_sentinel_for<sentinel_t<_Rng>, iterator_t<_Rng>>) ? subrange_kind::sized
                                                                                      : subrange_kind::unsized>;

    template <borrowed_range _Rng>
    subrange(_Rng&&, _Make_unsigned_like_t<range_difference_t<_Rng>>)
        -> subrange<iterator_t<_Rng>, sentinel_t<_Rng>, subrange_kind::sized>;

    template <class _It, class _Se, subrange_kind _Ki>
    inline constexpr bool enable_borrowed_range<subrange<_It, _Se, _Ki>> = true;

    // clang-format off
    template <size_t _Idx, class _It, class _Se, subrange_kind _Ki>
        requires (_Idx < 2)
    _NODISCARD constexpr auto get(const subrange<_It, _Se, _Ki>& _Val) {
        if constexpr (_Idx == 0) {
            return _Val.begin();
        } else {
            return _Val.end();
        }
    }

    template <size_t _Idx, class _It, class _Se, subrange_kind _Ki>
        requires (_Idx < 2)
    _NODISCARD constexpr auto get(subrange<_It, _Se, _Ki>&& _Val) {
        if constexpr (_Idx == 0) {
            return _Val.begin();
        } else {
            return _Val.end();
        }
    }
    // clang-format on
} // namespace ranges

using ranges::get;

template <class _It, class _Se, ranges::subrange_kind _Ki>
struct tuple_size<ranges::subrange<_It, _Se, _Ki>> : integral_constant<size_t, 2> {};

template <class _It, class _Se, ranges::subrange_kind _Ki>
struct tuple_element<0, ranges::subrange<_It, _Se, _Ki>> {
    using type = _It;
};

template <class _It, class _Se, ranges::subrange_kind _Ki>
struct tuple_element<1, ranges::subrange<_It, _Se, _Ki>> {
    using type = _Se;
};

template <class _It, class _Se, ranges::subrange_kind _Ki>
struct tuple_element<0, const ranges::subrange<_It, _Se, _Ki>> {
    using type = _It;
};

template <class _It, class _Se, ranges::subrange_kind _Ki>
struct tuple_element<1, const ranges::subrange<_It, _Se, _Ki>> {
    using type = _Se;
};

namespace ranges {
    struct dangling {
        constexpr dangling() noexcept = default;
        template <class... _Args>
        constexpr dangling(_Args&&...) noexcept {}
    };

    template <range _Rng>
    using borrowed_iterator_t = ::std::conditional_t<borrowed_range<_Rng>, iterator_t<_Rng>, dangling>;

    template <range _Rng>
    using borrowed_subrange_t = ::std::conditional_t<borrowed_range<_Rng>, subrange<iterator_t<_Rng>>, dangling>;
} // namespace ranges
#endif // __cpp_lib_concepts

struct _Container_proxy;
struct _Iterator_base12;

struct _Default_sentinel {}; // empty struct to serve as the end of a range

#ifdef __cpp_lib_concepts
template <semiregular>
class move_sentinel;

template <class>
struct _Move_iterator_category {};

// clang-format off
template <class _Iter>
    requires requires { typename _Iter_cat_t<_Iter>; }
struct _Move_iterator_category<_Iter> {
    using iterator_category = ::std::conditional_t<derived_from<_Iter_cat_t<_Iter>, random_access_iterator_tag>,
        random_access_iterator_tag, _Iter_cat_t<_Iter>>;
};
// clang-format on
#else // ^^^ Ranges / no Ranges vvv
template <class _Iter>
struct _Move_iterator_category {
    using iterator_category = _Iter_cat_t<_Iter>;
};
#endif // __cpp_lib_concepts

template <class _Iter>
class move_iterator : public _Move_iterator_category<_Iter> {
public:
    using iterator_type   = _Iter;
    using value_type      = _Iter_value_t<_Iter>;
    using difference_type = _Iter_diff_t<_Iter>;
    using pointer         = _Iter;

#ifdef __cpp_lib_concepts
    using iterator_concept = input_iterator_tag;
    using reference        = iter_rvalue_reference_t<_Iter>;
#else // ^^^ __cpp_lib_concepts / !__cpp_lib_concepts vvv
    using reference =
        ::std::conditional_t<::std::is_reference_v<_Iter_ref_t<_Iter>>, remove_reference_t<_Iter_ref_t<_Iter>>&&, _Iter_ref_t<_Iter>>;
#endif // __cpp_lib_concepts

    _CONSTEXPR17 move_iterator() = default;

    _CONSTEXPR17 explicit move_iterator(_Iter _Right) noexcept(::std::is_nothrow_move_constructible_v<_Iter>) // strengthened
        : _Current(::std::move(_Right)) {}

    // clang-format off
    template <class _Other>
#ifdef __cpp_lib_concepts
        requires (!::std::is_same_v<_Other, _Iter>) && convertible_to<const _Other&, _Iter>
#endif // __cpp_lib_concepts
    _CONSTEXPR17 move_iterator(const move_iterator<_Other>& _Right) noexcept(
        ::std::addressof<_Iter, const _Other&>) // strengthened
        : _Current(_Right.base()) {}

    template <class _Other>
#ifdef __cpp_lib_concepts
        requires (!::std::is_same_v<_Other, _Iter>) && convertible_to<const _Other&, _Iter>
            && assignable_from<_Iter&, const _Other&>
#endif // __cpp_lib_concepts
    _CONSTEXPR17 move_iterator& operator=(const move_iterator<_Other>& _Right) noexcept(
        ::std::is_nothrow_assignable_v<_Iter&, const _Other&>) /* strengthened */ {
        _Current = _Right.base();
        return *this;
    }
    // clang-format on

#ifdef __cpp_lib_concepts
    _NODISCARD constexpr const iterator_type& base() const& noexcept {
        return _Current;
    }
    _NODISCARD constexpr iterator_type base() && noexcept(::std::is_nothrow_move_constructible_v<_Iter>) /* strengthened */ {
        return ::std::move(_Current);
    }
#else // ^^^ __cpp_lib_concepts / !__cpp_lib_concepts vvv
    _NODISCARD _CONSTEXPR17 iterator_type base() const {
        return _Current;
    }
#endif // __cpp_lib_concepts

    _NODISCARD _CONSTEXPR17 reference operator*() const {
#ifdef __cpp_lib_concepts
        return _RANGES iter_move(_Current);
#else // ^^^ __cpp_lib_concepts / !__cpp_lib_concepts vvv
        return static_cast<reference>(*_Current);
#endif // __cpp_lib_concepts
    }

    _CXX20_DEPRECATE_MOVE_ITERATOR_ARROW _NODISCARD _CONSTEXPR17 pointer operator->() const {
        return _Current;
    }

    _CONSTEXPR17 move_iterator& operator++() {
        ++_Current;
        return *this;
    }

    _CONSTEXPR17 auto operator++(int) {
#ifdef __cpp_lib_concepts
        if constexpr (forward_iterator<_Iter>) {
#endif // __cpp_lib_concepts
            move_iterator _Tmp = *this;
            ++_Current;
            return _Tmp;
#ifdef __cpp_lib_concepts
        } else {
            ++_Current;
        }
#endif // __cpp_lib_concepts
    }

    _CONSTEXPR17 move_iterator& operator--() {
        --_Current;
        return *this;
    }

    _CONSTEXPR17 move_iterator operator--(int) {
        move_iterator _Tmp = *this;
        --_Current;
        return _Tmp;
    }

    template <class _Iter2 = _Iter>
    _NODISCARD auto operator==(_Default_sentinel _Sentinel) const noexcept
        -> decltype(declval<const _Iter2&>() == _Sentinel) {
        return _Current == _Sentinel;
    }

    template <class _Iter2 = _Iter>
    _NODISCARD auto operator!=(_Default_sentinel _Sentinel) const noexcept
        -> decltype(declval<const _Iter2&>() != _Sentinel) {
        return _Current != _Sentinel;
    }

    _NODISCARD _CONSTEXPR17 move_iterator operator+(const difference_type _Off) const {
        return move_iterator(_Current + _Off);
    }

    _CONSTEXPR17 move_iterator& operator+=(const difference_type _Off) {
        _Current += _Off;
        return *this;
    }

    _NODISCARD _CONSTEXPR17 move_iterator operator-(const difference_type _Off) const {
        return move_iterator(_Current - _Off);
    }

    _CONSTEXPR17 move_iterator& operator-=(const difference_type _Off) {
        _Current -= _Off;
        return *this;
    }

    _NODISCARD _CONSTEXPR17 reference operator[](const difference_type _Off) const {
#ifdef __cpp_lib_concepts
        return _RANGES iter_move(_Current + _Off);
#else // ^^^ __cpp_lib_concepts / !__cpp_lib_concepts vvv
        return ::std::move(_Current[_Off]);
#endif // __cpp_lib_concepts
    }

#ifdef __cpp_lib_concepts
    template <sentinel_for<_Iter> _Sent>
    _NODISCARD friend constexpr bool operator==(const move_iterator& _Left, const move_sentinel<_Sent>& _Right) {
        return _Left._Current == _Right._Get_last();
    }

    template <sized_sentinel_for<_Iter> _Sent>
    _NODISCARD friend constexpr difference_type operator-(
        const move_sentinel<_Sent>& _Left, const move_iterator& _Right) {
        return _Left._Get_last() - _Right._Current;
    }

    template <sized_sentinel_for<_Iter> _Sent>
    _NODISCARD friend constexpr difference_type operator-(
        const move_iterator& _Left, const move_sentinel<_Sent>& _Right) {
        return _Left._Current - _Right._Get_last();
    }

    _NODISCARD friend constexpr reference iter_move(const move_iterator& _It)
#ifdef __EDG__ // TRANSITION, VSO-1222776
        noexcept(noexcept(_RANGES iter_move(declval<const _Iter&>())))
#else // ^^^ workaround / no workaround vvv
        noexcept(noexcept(_RANGES iter_move(_It._Current)))
#endif // TRANSITION, VSO-1222776
    {
        return _RANGES iter_move(_It._Current);
    }

    template <indirectly_swappable<_Iter> _Iter2>
    friend constexpr void iter_swap(const move_iterator& _Left, const move_iterator<_Iter2>& _Right)
#ifdef __EDG__ // TRANSITION, VSO-1222776
        noexcept(noexcept(_RANGES iter_swap(declval<const _Iter&>(), declval<const _Iter2&>())))
#else // ^^^ workaround / no workaround vvv
        noexcept(noexcept(_RANGES iter_swap(_Left._Current, _Right.base())))
#endif // TRANSITION, VSO-1222776
    {
        _RANGES iter_swap(_Left._Current, _Right.base());
    }
#endif // __cpp_lib_concepts

    template <class _Iter2, ::std::enable_if_t<_Range_verifiable_v<_Iter, _Iter2>, int> = 0>
    friend constexpr void _Verify_range(const move_iterator& _First, const move_iterator<_Iter2>& _Last) {
        _Verify_range(_First._Current, _Last.base());
    }
#ifdef __cpp_lib_concepts
    template <sentinel_for<_Iter> _Sent, ::std::enable_if_t<_Range_verifiable_v<_Iter, _Sent>, int> = 0>
    friend constexpr void _Verify_range(const move_iterator& _First, const move_sentinel<_Sent>& _Last) {
        _Verify_range(_First._Current, _Last._Get_last());
    }
#endif // __cpp_lib_concepts

    using _Prevent_inheriting_unwrap = move_iterator;

    template <class _Iter2 = iterator_type, ::std::enable_if_t<_Offset_verifiable_v<_Iter2>, int> = 0>
    constexpr void _Verify_offset(const difference_type _Off) const {
        _Current._Verify_offset(_Off);
    }

    template <class _Iter2 = iterator_type, ::std::enable_if_t<_Unwrappable_v<const _Iter2&>, int> = 0>
    _NODISCARD constexpr move_iterator<_Unwrapped_t<const _Iter2&>> _Unwrapped() const& {
        return static_cast<move_iterator<_Unwrapped_t<const _Iter2&>>>(_Current._Unwrapped());
    }
    template <class _Iter2 = iterator_type, ::std::enable_if_t<_Unwrappable_v<_Iter2>, int> = 0>
    _NODISCARD constexpr move_iterator<_Unwrapped_t<_Iter2>> _Unwrapped() && {
        return static_cast<move_iterator<_Unwrapped_t<_Iter2>>>(::std::move(_Current)._Unwrapped());
    }

    static constexpr bool _Unwrap_when_unverified = _Do_unwrap_when_unverified_v<iterator_type>;

    template <class _Src, ::std::enable_if_t<_Wrapped_seekable_v<iterator_type, const _Src&>, int> = 0>
    constexpr void _Seek_to(const move_iterator<_Src>& _It) {
        _Current._Seek_to(_It.base());
    }
    template <class _Src, ::std::enable_if_t<_Wrapped_seekable_v<iterator_type, _Src>, int> = 0>
    constexpr void _Seek_to(move_iterator<_Src>&& _It) {
        _Current._Seek_to(::std::move(_It).base());
    }

private:
    iterator_type _Current{};
};

template <class _Iter1, class _Iter2>
_NODISCARD _CONSTEXPR17 bool operator==(const move_iterator<_Iter1>& _Left, const move_iterator<_Iter2>& _Right)
#ifdef __cpp_lib_concepts
    // clang-format off
    requires requires {
        { _Left.base() == _Right.base() } -> _Implicitly_convertible_to<bool>;
    }
// clang-format on
#endif // __cpp_lib_concepts
{ return _Left.base() == _Right.base(); }

#if !_HAS_CXX20
template <class _Iter1, class _Iter2>
_NODISCARD _CONSTEXPR17 bool operator!=(const move_iterator<_Iter1>& _Left, const move_iterator<_Iter2>& _Right) {
    return !(_Left == _Right);
}
#endif // !_HAS_CXX20

template <class _Iter1, class _Iter2>
_NODISCARD _CONSTEXPR17 bool operator<(const move_iterator<_Iter1>& _Left, const move_iterator<_Iter2>& _Right)
#ifdef __cpp_lib_concepts
    // clang-format off
    requires requires {
        { _Left.base() < _Right.base() } -> _Implicitly_convertible_to<bool>;
    }
// clang-format on
#endif // __cpp_lib_concepts
{ return _Left.base() < _Right.base(); }

template <class _Iter1, class _Iter2>
_NODISCARD _CONSTEXPR17 bool operator>(const move_iterator<_Iter1>& _Left, const move_iterator<_Iter2>& _Right)
#ifdef __cpp_lib_concepts
    // clang-format off
    requires requires { _Right < _Left; }
// clang-format on
#endif // __cpp_lib_concepts
{ return _Right < _Left; }

template <class _Iter1, class _Iter2>
_NODISCARD _CONSTEXPR17 bool operator<=(const move_iterator<_Iter1>& _Left, const move_iterator<_Iter2>& _Right)
#ifdef __cpp_lib_concepts
    // clang-format off
    requires requires { _Right < _Left; }
// clang-format on
#endif // __cpp_lib_concepts
{ return !(_Right < _Left); }

template <class _Iter1, class _Iter2>
_NODISCARD _CONSTEXPR17 bool operator>=(const move_iterator<_Iter1>& _Left, const move_iterator<_Iter2>& _Right)
#ifdef __cpp_lib_concepts
    // clang-format off
    requires requires { _Left < _Right; }
// clang-format on
#endif // __cpp_lib_concepts
{ return !(_Left < _Right); }

#ifdef __cpp_lib_concepts
template <class _Iter1, three_way_comparable_with<_Iter1> _Iter2>
_NODISCARD constexpr compare_three_way_result_t<_Iter1, _Iter2> operator<=>(
    const move_iterator<_Iter1>& _Left, const move_iterator<_Iter2>& _Right) {
    return _Left.base() <=> _Right.base();
}
#endif // __cpp_lib_concepts

template <class _Iter1, class _Iter2>
_NODISCARD _CONSTEXPR17 auto operator-(const move_iterator<_Iter1>& _Left, const move_iterator<_Iter2>& _Right)
    -> decltype(_Left.base() - _Right.base()) {
    return _Left.base() - _Right.base();
}

template <class _Iter>
_NODISCARD _CONSTEXPR17 move_iterator<_Iter> operator+(
    typename move_iterator<_Iter>::difference_type _Off, const move_iterator<_Iter>& _Right)
#ifdef __cpp_lib_concepts
    // clang-format off
    requires requires {
        { _Right.base() + _Off } -> same_as<_Iter>;
    }
// clang-format on
#endif // __cpp_lib_concepts
{ return move_iterator<_Iter>{_Right.base() + _Off}; }

template <class _Iter>
_NODISCARD _CONSTEXPR17 move_iterator<_Iter> make_move_iterator(_Iter _It) { // make move_iterator from iterator
    return move_iterator<_Iter>(::std::move(_It));
}

#ifdef __cpp_lib_concepts
struct default_sentinel_t {};

inline constexpr default_sentinel_t default_sentinel{};

struct unreachable_sentinel_t;
namespace _Unreachable_sentinel_detail {
    struct _Base {
        template <weakly_incrementable _Winc>
        _NODISCARD friend constexpr bool operator==(const unreachable_sentinel_t&, const _Winc&) noexcept {
            return false;
        }
    };
} // namespace _Unreachable_sentinel_detail
struct unreachable_sentinel_t : _Unreachable_sentinel_detail::_Base {}; // TRANSITION, /permissive-

inline constexpr unreachable_sentinel_t unreachable_sentinel{};
#endif // __cpp_lib_concepts

// _Iterator_is_contiguous<_Iter> reports whether an iterator is known to be contiguous.
// (Without concepts, this detection is limited, which will limit when we can activate optimizations.)

#ifdef __cpp_lib_concepts
// When concepts are available, we can detect arbitrary contiguous iterators.
template <class _Iter>
inline constexpr bool _Iterator_is_contiguous = contiguous_iterator<_Iter>;

template <class _Iter>
_NODISCARD constexpr auto _To_address(const _Iter& _Val) noexcept {
    _STL_INTERNAL_STATIC_ASSERT(contiguous_iterator<_Iter>);
    return _RPC to_address(_Val);
}
#else // ^^^ defined(__cpp_lib_concepts) ^^^ / vvv !defined(__cpp_lib_concepts) vvv
// When concepts aren't available, we can detect pointers. (Iterators should be unwrapped before using this.)
template <class _Iter>
_INLINE_VAR constexpr bool _Iterator_is_contiguous = ::std::is_pointer_v<_Iter>;

template <class _Iter>
_NODISCARD constexpr auto _To_address(const _Iter& _Val) noexcept {
    _STL_INTERNAL_STATIC_ASSERT(::std::is_pointer_v<_Iter>);
    return _Val;
}
#endif // ^^^ !defined(__cpp_lib_concepts) ^^^

template <class _Iter>
_NODISCARD constexpr auto _To_address(const move_iterator<_Iter>& _Val) noexcept {
    return _To_address(_Val.base());
}

// _Iterators_are_contiguous<_Iter1, _Iter2> reports whether both iterators are known to be contiguous.

template <class _Iter1, class _Iter2>
_INLINE_VAR constexpr bool _Iterators_are_contiguous =
    _Iterator_is_contiguous<_Iter1>&& _Iterator_is_contiguous<_Iter2>;

template <class _Iter>
_INLINE_VAR constexpr bool _Iterator_is_volatile = ::std::is_volatile_v<remove_reference_t<_Iter_ref_t<_Iter>>>;

template <class _Source, class _Dest>
_INLINE_VAR constexpr bool _Is_pointer_address_convertible = ::std::is_void_v<_Source> || ::std::is_void_v<_Dest>
            // NOTE: ::std::is_same_v is required for function pointers to work
            || ::std::is_same_v<::std::remove_cv_t<_Source>, ::std::remove_cv_t<_Dest>>
#ifdef __cpp_lib_is_pointer_interconvertible
            || is_pointer_interconvertible_base_of_v<_Dest, _Source>
#endif // __cpp_lib_is_pointer_interconvertible
        ;

template <class _Source, class _Dest, class _SourceRef, class _DestRef>
struct _Trivial_cat {
    using _USource = _Unwrap_enum_t<_Source>;
    using _UDest   = _Unwrap_enum_t<_Dest>;

    static constexpr bool _Same_size_and_compatible =
        sizeof(_Source) == sizeof(_Dest)
        // If _UDest is bool, _USource also needs to be bool
        // Conversion from non-bool => non-bool | bool => bool | bool => non-bool is fine.
        // Conversion from non-bool => bool is not fine.
        && ::std::is_same_v<bool, _USource> >= ::std::is_same_v<bool, _UDest> //
        && (::std::is_same_v<_USource, _UDest> || (::std::is_integral_v<_USource> && ::std::is_integral_v<_UDest>)
            || (is_floating_point_v<_USource> && is_floating_point_v<_UDest>) );

    static constexpr bool _Bitcopy_constructible =
        _Same_size_and_compatible && ::std::is_trivially_constructible_v<_Dest, _SourceRef>;

    static constexpr bool _Bitcopy_assignable =
        _Same_size_and_compatible && ::std::is_trivially_assignable_v<_DestRef, _SourceRef>;
};

template <class _Source, class _Dest, class _SourceRef, class _DestRef>
struct _Trivial_cat<_Source*, _Dest*, _SourceRef, _DestRef> {
    static constexpr bool _Bitcopy_constructible =
        _Is_pointer_address_convertible<_Source, _Dest> && ::std::is_trivially_constructible_v<_Dest*, _SourceRef>;

    static constexpr bool _Bitcopy_assignable =
        _Is_pointer_address_convertible<_Source, _Dest> && ::std::is_trivially_assignable_v<_DestRef, _SourceRef>;
};

struct _False_trivial_cat {
    static constexpr bool _Bitcopy_constructible = false;
    static constexpr bool _Bitcopy_assignable    = false;
};

template <class _SourceIt, class _DestIt,
    bool _Are_contiguous = _Iterators_are_contiguous<_SourceIt, _DestIt> //
                        && !_Iterator_is_volatile<_SourceIt> && !_Iterator_is_volatile<_DestIt>>
struct _Iter_move_cat : _Trivial_cat<_Iter_value_t<_SourceIt>, _Iter_value_t<_DestIt>,
                            remove_reference_t<_Iter_ref_t<_SourceIt>>&&, _Iter_ref_t<_DestIt>> {};

template <class _SourceIt, class _DestIt>
struct _Iter_move_cat<_SourceIt, _DestIt, false> : _False_trivial_cat {};

template <class _SourceIt, class _DestIt>
struct _Iter_move_cat<move_iterator<_SourceIt>, _DestIt, false> : _Iter_move_cat<_SourceIt, _DestIt> {};

template <class _SourceIt, class _DestIt,
    bool _Are_contiguous = _Iterators_are_contiguous<_SourceIt, _DestIt> //
                        && !_Iterator_is_volatile<_SourceIt> && !_Iterator_is_volatile<_DestIt>>
struct _Iter_copy_cat
    : _Trivial_cat<_Iter_value_t<_SourceIt>, _Iter_value_t<_DestIt>, _Iter_ref_t<_SourceIt>, _Iter_ref_t<_DestIt>> {};

template <class _SourceIt, class _DestIt>
struct _Iter_copy_cat<_SourceIt, _DestIt, false> : _False_trivial_cat {};

template <class _SourceIt, class _DestIt>
struct _Iter_copy_cat<move_iterator<_SourceIt>, _DestIt, false> : _Iter_move_cat<_SourceIt, _DestIt> {};

template <class _CtgIt, class _OutCtgIt>
_OutCtgIt _Copy_memmove(_CtgIt _First, _CtgIt _Last, _OutCtgIt _Dest) {
    auto _FirstPtr              = _To_address(_First);
    auto _LastPtr               = _To_address(_Last);
    auto _DestPtr               = _To_address(_Dest);
    const char* const _First_ch = const_cast<const char*>(reinterpret_cast<const volatile char*>(_FirstPtr));
    const char* const _Last_ch  = const_cast<const char*>(reinterpret_cast<const volatile char*>(_LastPtr));
    char* const _Dest_ch        = const_cast<char*>(reinterpret_cast<const volatile char*>(_DestPtr));
    const auto _Count           = static_cast<size_t>(_Last_ch - _First_ch);
    _CSTD memmove(_Dest_ch, _First_ch, _Count);
    if constexpr (::std::is_pointer_v<_OutCtgIt>) {
        return reinterpret_cast<_OutCtgIt>(_Dest_ch + _Count);
    } else {
        return _Dest + (_LastPtr - _FirstPtr);
    }
}

template <class _It, bool _RequiresMutable = false>
_INLINE_VAR constexpr bool _Is_vb_iterator = false;

template <class _InIt, class _OutIt>
_CONSTEXPR20 _OutIt _Copy_unchecked(_InIt _First, _InIt _Last, _OutIt _Dest) {
    // copy [_First, _Last) to [_Dest, ...)
    // note: _Copy_unchecked has callers other than the copy family
    if constexpr (_Iter_copy_cat<_InIt, _OutIt>::_Bitcopy_assignable) {
#if _HAS_CXX20
        if (!_RPC is_constant_evaluated())
#endif // _HAS_CXX20
        {
            return _Copy_memmove(_First, _Last, _Dest);
        }
    }

    for (; _First != _Last; ++_Dest, (void) ++_First) {
        *_Dest = *_First;
    }

    return _Dest;
}

template <class _InIt, class _OutIt>
_CONSTEXPR20 _OutIt copy(_InIt _First, _InIt _Last, _OutIt _Dest) { // copy [_First, _Last) to [_Dest, ...)
    _Adl_verify_range(_First, _Last);
    const auto _UFirst = _Get_unwrapped(_First);
    const auto _ULast  = _Get_unwrapped(_Last);
    const auto _UDest  = _Get_unwrapped_n(_Dest, _Idl_distance<_InIt>(_UFirst, _ULast));
    _Seek_wrapped(_Dest, _Copy_unchecked(_UFirst, _ULast, _UDest));
    return _Dest;
}

#if _HAS_CXX17
template <class _ExPo, class _FwdIt1, class _FwdIt2, _Enable_if_execution_policy_t<_ExPo> = 0>
_FwdIt2 copy(_ExPo&&, _FwdIt1 _First, _FwdIt1 _Last, _FwdIt2 _Dest) noexcept /* terminates */ {
    // copy [_First, _Last) to [_Dest, ...)
    // not parallelized as benchmarks show it isn't worth it
    _REQUIRE_PARALLEL_ITERATOR(_FwdIt1);
    _REQUIRE_PARALLEL_ITERATOR(_FwdIt2);
    return _RPC copy(_First, _Last, _Dest);
}
#endif // _HAS_CXX17

#ifdef __cpp_lib_concepts
namespace ranges {
    template <class _To, class _From>
    concept _Convertible_from = convertible_to<_From, _To>;

    template <class _In, class _Out>
    struct in_out_result {
        /* [[no_unique_address]] */ _In in;
        /* [[no_unique_address]] */ _Out out;

        template <_Convertible_from<const _In&> _IIn, _Convertible_from<const _Out&> _OOut>
        constexpr operator in_out_result<_IIn, _OOut>() const& {
            return {in, out};
        }

        template <_Convertible_from<_In> _IIn, _Convertible_from<_Out> _OOut>
        constexpr operator in_out_result<_IIn, _OOut>() && {
            return {::std::move(in), ::std::move(out)};
        }
    };

    template <class _In, class _Out>
    using copy_result = in_out_result<_In, _Out>;

    // clang-format off
    template <input_iterator _It, sentinel_for<_It> _Se, weakly_incrementable _Out>
        requires indirectly_copyable<_It, _Out>
    _NODISCARD constexpr copy_result<_It, _Out> _Copy_unchecked(_It _First, _Se _Last, _Out _Result) {
        // clang-format on
        if constexpr (_Iter_copy_cat<_It, _Out>::_Bitcopy_assignable && sized_sentinel_for<_Se, _It>) {
            if (!_RPC is_constant_evaluated()) {
                auto _Final = _RANGES next(_First, ::std::move(_Last));
                _Result     = _Copy_memmove(::std::move(_First), _Final, ::std::move(_Result));
                return {::std::move(_Final), ::std::move(_Result)};
            }
        }

        for (; _First != _Last; ++_First, (void) ++_Result) {
            *_Result = *_First;
        }

        return {::std::move(_First), ::std::move(_Result)};
    }

    class _Copy_fn : private _Not_quite_object {
    public:
        using _Not_quite_object::_Not_quite_object;

        // clang-format off
        template <input_iterator _It, sentinel_for<_It> _Se, weakly_incrementable _Out>
            requires indirectly_copyable<_It, _Out>
        constexpr copy_result<_It, _Out> operator()(_It _First, _Se _Last, _Out _Result) const {
            _Adl_verify_range(_First, _Last);
            auto _UResult = _RANGES _Copy_unchecked(
                _Get_unwrapped(::std::move(_First)), _Get_unwrapped(::std::move(_Last)), ::std::move(_Result));
            _Seek_wrapped(_First, ::std::move(_UResult.in));
            return {::std::move(_First), ::std::move(_UResult.out)};
        }

        template <input_range _Rng, weakly_incrementable _Out>
            requires indirectly_copyable<iterator_t<_Rng>, _Out>
        constexpr copy_result<borrowed_iterator_t<_Rng>, _Out> operator()(_Rng&& _Range, _Out _Result) const {
            auto _First = _RANGES begin(_Range);
            auto _UResult =
                _RANGES _Copy_unchecked(_Get_unwrapped(::std::move(_First)), _Uend(_Range), ::std::move(_Result));
            _Seek_wrapped(_First, ::std::move(_UResult.in));
            return {::std::move(_First), ::std::move(_UResult.out)};
        }
        // clang-format on
    };

    inline constexpr _Copy_fn copy{_Not_quite_object::_Construct_tag{}};
} // namespace ranges
#endif // __cpp_lib_concepts

template <class _InIt, class _Diff, class _OutIt>
_CONSTEXPR20 _OutIt copy_n(_InIt _First, _Diff _Count_raw, _OutIt _Dest) {
    // copy [_First, _First + _Count) to [_Dest, ...)
    _Algorithm_int_t<_Diff> _Count = _Count_raw;
    if (0 < _Count) {
        auto _UFirst = _Get_unwrapped_n(_First, _Count);
        auto _UDest  = _Get_unwrapped_n(_Dest, _Count);
        if constexpr (_Iter_copy_cat<decltype(_UFirst), decltype(_UDest)>::_Bitcopy_assignable) {
#if _HAS_CXX20
            if (!_RPC is_constant_evaluated())
#endif // _HAS_CXX20
            {
                _UDest = _Copy_memmove(_UFirst, _UFirst + _Count, _UDest);
                _Seek_wrapped(_Dest, _UDest);
                return _Dest;
            }
        }

        for (;;) {
            *_UDest = *_UFirst;
            ++_UDest;
            --_Count;
            if (_Count == 0) { // note that we avoid an extra ++_First here to allow istream_iterator to work,
                               // see LWG-2471
                break;
            }

            ++_UFirst;
        }

        _Seek_wrapped(_Dest, _UDest);
    }

    return _Dest;
}

#if _HAS_CXX17
template <class _ExPo, class _FwdIt1, class _Diff, class _FwdIt2, _Enable_if_execution_policy_t<_ExPo> = 0>
_FwdIt2 copy_n(_ExPo&&, _FwdIt1 _First, _Diff _Count_raw, _FwdIt2 _Dest) noexcept /* terminates */ {
    // copy [_First, _First + _Count) to [_Dest, ...)
    // not parallelized as benchmarks show it isn't worth it
    _REQUIRE_PARALLEL_ITERATOR(_FwdIt1);
    _REQUIRE_PARALLEL_ITERATOR(_FwdIt2);
    return _RPC copy_n(_First, _Count_raw, _Dest);
}
#endif // _HAS_CXX17

template <class _CtgIt1, class _CtgIt2>
_CtgIt2 _Copy_backward_memmove(_CtgIt1 _First, _CtgIt1 _Last, _CtgIt2 _Dest) {
    // implement copy_backward-like function as memmove
    auto _FirstPtr              = _To_address(_First);
    auto _LastPtr               = _To_address(_Last);
    auto _DestPtr               = _To_address(_Dest);
    const char* const _First_ch = const_cast<const char*>(reinterpret_cast<const volatile char*>(_FirstPtr));
    const char* const _Last_ch  = const_cast<const char*>(reinterpret_cast<const volatile char*>(_LastPtr));
    char* const _Dest_ch        = const_cast<char*>(reinterpret_cast<const volatile char*>(_DestPtr));
    const auto _Count           = static_cast<size_t>(_Last_ch - _First_ch);
    auto _Result                = _CSTD memmove(_Dest_ch - _Count, _First_ch, _Count);
    if constexpr (::std::is_pointer_v<_CtgIt2>) {
        return static_cast<_CtgIt2>(_Result);
    } else {
        return _Dest - (_LastPtr - _FirstPtr);
    }
}

template <class _BidIt1, class _BidIt2>
_BidIt2 _Copy_backward_memmove(move_iterator<_BidIt1> _First, move_iterator<_BidIt1> _Last, _BidIt2 _Dest) {
    return _Copy_backward_memmove(_First.base(), _Last.base(), _Dest);
}

template <class _BidIt1, class _BidIt2>
_NODISCARD _CONSTEXPR20 _BidIt2 _Copy_backward_unchecked(_BidIt1 _First, _BidIt1 _Last, _BidIt2 _Dest) {
    // copy [_First, _Last) backwards to [..., _Dest)
    if constexpr (_Iter_copy_cat<_BidIt1, _BidIt2>::_Bitcopy_assignable) {
#if _HAS_CXX20
        if (!_RPC is_constant_evaluated())
#endif // _HAS_CXX20
        {
            return _Copy_backward_memmove(_First, _Last, _Dest);
        }
    }

    while (_First != _Last) {
        *--_Dest = *--_Last;
    }

    return _Dest;
}

template <class _BidIt1, class _BidIt2>
_CONSTEXPR20 _BidIt2 copy_backward(_BidIt1 _First, _BidIt1 _Last, _BidIt2 _Dest) {
    // copy [_First, _Last) backwards to [..., _Dest)
    _Adl_verify_range(_First, _Last);
    const auto _UFirst = _Get_unwrapped(_First);
    const auto _ULast  = _Get_unwrapped(_Last);
    const auto _UDest  = _Get_unwrapped_n(_Dest, -_Idl_distance<_BidIt1>(_UFirst, _ULast));
    _Seek_wrapped(_Dest, _Copy_backward_unchecked(_UFirst, _ULast, _UDest));
    return _Dest;
}

#if _HAS_CXX17
template <class _ExPo, class _BidIt1, class _BidIt2, _Enable_if_execution_policy_t<_ExPo> = 0>
_BidIt2 copy_backward(_ExPo&&, _BidIt1 _First, _BidIt1 _Last, _BidIt2 _Dest) noexcept /* terminates */ {
    // copy [_First, _Last) backwards to [..., _Dest)
    // not parallelized as benchmarks show it isn't worth it
    return _RPC copy_backward(_First, _Last, _Dest);
}
#endif // _HAS_CXX17

template <class _InIt, class _OutIt>
_CONSTEXPR20 _OutIt _Move_unchecked(_InIt _First, _InIt _Last, _OutIt _Dest) {
    // move [_First, _Last) to [_Dest, ...)
    // note: _Move_unchecked has callers other than the move family
    if constexpr (_Iter_move_cat<_InIt, _OutIt>::_Bitcopy_assignable) {
#if _HAS_CXX20
        if (!_RPC is_constant_evaluated())
#endif // _HAS_CXX20
        {
            return _Copy_memmove(_First, _Last, _Dest);
        }
    }

    for (; _First != _Last; ++_Dest, (void) ++_First) {
        *_Dest = ::std::move(*_First);
    }

    return _Dest;
}

template <class _InIt, class _OutIt>
_CONSTEXPR20 _OutIt move(_InIt _First, _InIt _Last, _OutIt _Dest) {
    // move [_First, _Last) to [_Dest, ...)
    _Adl_verify_range(_First, _Last);
    const auto _UFirst = _Get_unwrapped(_First);
    const auto _ULast  = _Get_unwrapped(_Last);
    const auto _UDest  = _Get_unwrapped_n(_Dest, _Idl_distance<_InIt>(_UFirst, _ULast));
    _Seek_wrapped(_Dest, _Move_unchecked(_UFirst, _ULast, _UDest));
    return _Dest;
}

#if _HAS_CXX17
template <class _ExPo, class _FwdIt1, class _FwdIt2, _Enable_if_execution_policy_t<_ExPo> = 0>
_FwdIt2 move(_ExPo&&, _FwdIt1 _First, _FwdIt1 _Last, _FwdIt2 _Dest) noexcept /* terminates */ {
    // move [_First, _Last) to [_Dest, ...)
    // not parallelized as benchmarks show it isn't worth it
    _REQUIRE_PARALLEL_ITERATOR(_FwdIt1);
    _REQUIRE_PARALLEL_ITERATOR(_FwdIt2);
    return ::std::move(_First, _Last, _Dest);
}
#endif // _HAS_CXX17

template <class _BidIt1, class _BidIt2>
_CONSTEXPR20 _BidIt2 _Move_backward_unchecked(_BidIt1 _First, _BidIt1 _Last, _BidIt2 _Dest) {
    // move [_First, _Last) backwards to [..., _Dest)
    // note: _Move_backward_unchecked has callers other than the move_backward family
    if constexpr (_Iter_move_cat<_BidIt1, _BidIt2>::_Bitcopy_assignable) {
#if _HAS_CXX20
        if (!_RPC is_constant_evaluated())
#endif // _HAS_CXX20
        {
            return _Copy_backward_memmove(_First, _Last, _Dest);
        }
    }

    while (_First != _Last) {
        *--_Dest = ::std::move(*--_Last);
    }

    return _Dest;
}

template <class _BidIt1, class _BidIt2>
_CONSTEXPR20 _BidIt2 move_backward(_BidIt1 _First, _BidIt1 _Last, _BidIt2 _Dest) {
    // move [_First, _Last) backwards to [..., _Dest)
    _Adl_verify_range(_First, _Last);
    const auto _UFirst = _Get_unwrapped(_First);
    const auto _ULast  = _Get_unwrapped(_Last);
    const auto _UDest  = _Get_unwrapped_n(_Dest, -_Idl_distance<_BidIt1>(_UFirst, _ULast));
    _Seek_wrapped(_Dest, _Move_backward_unchecked(_UFirst, _ULast, _UDest));
    return _Dest;
}

#if _HAS_CXX17
template <class _ExPo, class _BidIt1, class _BidIt2, _Enable_if_execution_policy_t<_ExPo> = 0>
_BidIt2 move_backward(_ExPo&&, _BidIt1 _First, _BidIt1 _Last, _BidIt2 _Dest) noexcept /* terminates */ {
    // move [_First, _Last) backwards to [..., _Dest)
    // not parallelized as benchmarks show it isn't worth it
    return _RPC move_backward(_First, _Last, _Dest);
}
#endif // _HAS_CXX17


template <class _Ty>
struct _Is_character : false_type {}; // by default, not a character type

template <>
struct _Is_character<char> : true_type {}; // chars are characters

template <>
struct _Is_character<signed char> : true_type {}; // signed chars are also characters

template <>
struct _Is_character<unsigned char> : true_type {}; // unsigned chars are also characters

#ifdef __cpp_char8_t
template <>
struct _Is_character<char8_t> : true_type {}; // UTF-8 code units are sort-of characters
#endif // __cpp_char8_t

template <class _Ty>
struct _Is_character_or_bool : _Is_character<_Ty>::type {};

template <>
struct _Is_character_or_bool<bool> : true_type {};

template <class _Ty>
struct _Is_character_or_byte_or_bool : _Is_character_or_bool<_Ty>::type {};

#ifdef __cpp_lib_byte
template <>
struct _Is_character_or_byte_or_bool<::std::byte> : true_type {};
#endif // __cpp_lib_byte

// _Fill_memset_is_safe determines if _FwdIt and _Ty are eligible for memset optimization in fill.
// Need to explicitly test for volatile because _Unwrap_enum_t discards qualifiers.
template <class _FwdIt, class _Ty, bool = _Iterator_is_contiguous<_FwdIt>>
_INLINE_VAR constexpr bool _Fill_memset_is_safe = ::std::conjunction_v<::std::is_scalar<_Ty>,
    _Is_character_or_byte_or_bool<_Unwrap_enum_t<remove_reference_t<_Iter_ref_t<_FwdIt>>>>,
    ::std::negation<::std::is_volatile<remove_reference_t<_Iter_ref_t<_FwdIt>>>>, ::std::is_assignable<_Iter_ref_t<_FwdIt>, const _Ty&>>;

template <class _FwdIt, class _Ty>
_INLINE_VAR constexpr bool _Fill_memset_is_safe<_FwdIt, _Ty, false> = false;

template <class _FwdIt, class _Ty, bool = _Iterator_is_contiguous<_FwdIt>>
_INLINE_VAR constexpr bool _Fill_zero_memset_is_safe =
    ::std::conjunction_v<::std::is_scalar<_Ty>, ::std::is_scalar<_Iter_value_t<_FwdIt>>, ::std::negation<::std::is_member_pointer<_Iter_value_t<_FwdIt>>>,
        ::std::negation<::std::is_volatile<remove_reference_t<_Iter_ref_t<_FwdIt>>>>, ::std::is_assignable<_Iter_ref_t<_FwdIt>, const _Ty&>>;

template <class _FwdIt, class _Ty>
_INLINE_VAR constexpr bool _Fill_zero_memset_is_safe<_FwdIt, _Ty, false> = false;

template <class _CtgIt, class _Ty>
void _Fill_memset(_CtgIt _Dest, const _Ty _Val, const size_t _Count) {
    // implicitly convert (a cast would suppress warnings); also handles _Iter_value_t<_CtgIt> being bool
    _Iter_value_t<_CtgIt> _Dest_val = _Val;
    _CSTD memset(_To_address(_Dest), static_cast<unsigned char>(_Dest_val), _Count);
}

template <class _CtgIt>
void _Fill_zero_memset(_CtgIt _Dest, const size_t _Count) {
    _CSTD memset(_To_address(_Dest), 0, _Count * sizeof(_Iter_value_t<_CtgIt>));
}

template <class _Ty>
_NODISCARD bool _Is_all_bits_zero(const _Ty& _Val) {
    // checks if scalar type has all bits set to zero
    _STL_INTERNAL_STATIC_ASSERT(is_scalar_v<_Ty> && !::std::is_member_pointer_v<_Ty>);
    if constexpr (::std::is_same_v<_Ty, ::std::nullptr_t>) {
        return true;
    } else {
        constexpr _Ty _Zero{};
        return _CSTD memcmp(&_Val, &_Zero, sizeof(_Ty)) == 0;
    }
}

template <class _FwdIt, class _Ty>
_CONSTEXPR20 void fill(const _FwdIt _First, const _FwdIt _Last, const _Ty& _Val) {
    // copy _Val through [_First, _Last)
    _Adl_verify_range(_First, _Last);
    if constexpr (_Is_vb_iterator<_FwdIt, true>) {
        _Fill_vbool(_First, _Last, _Val);
    } else {
        auto _UFirst      = _Get_unwrapped(_First);
        const auto _ULast = _Get_unwrapped(_Last);
#if _HAS_CXX20
        if (!_RPC is_constant_evaluated())
#endif // _HAS_CXX20
        {
            if constexpr (_Fill_memset_is_safe<decltype(_UFirst), _Ty>) {
                _Fill_memset(_UFirst, _Val, static_cast<size_t>(_ULast - _UFirst));
                return;
            } else if constexpr (_Fill_zero_memset_is_safe<decltype(_UFirst), _Ty>) {
                if (_Is_all_bits_zero(_Val)) {
                    _Fill_zero_memset(_UFirst, static_cast<size_t>(_ULast - _UFirst));
                    return;
                }
            }
        }

        for (; _UFirst != _ULast; ++_UFirst) {
            *_UFirst = _Val;
        }
    }
}

#if _HAS_CXX17
template <class _ExPo, class _FwdIt, class _Ty, _Enable_if_execution_policy_t<_ExPo> = 0>
void fill(_ExPo&&, _FwdIt _First, _FwdIt _Last, const _Ty& _Val) noexcept /* terminates */ {
    // copy _Val through [_First, _Last)
    // not parallelized as benchmarks show it isn't worth it
    _REQUIRE_PARALLEL_ITERATOR(_FwdIt);
    return _RPC fill(_First, _Last, _Val);
}
#endif // _HAS_CXX17

template <class _OutIt, class _Diff, class _Ty>
_CONSTEXPR20 _OutIt fill_n(_OutIt _Dest, const _Diff _Count_raw, const _Ty& _Val) {
    // copy _Val _Count times through [_Dest, ...)
    _Algorithm_int_t<_Diff> _Count = _Count_raw;
    if (0 < _Count) {
        if constexpr (_Is_vb_iterator<_OutIt, true>) {
            const auto _Last = _Dest + static_cast<typename _OutIt::difference_type>(_Count);
            _Fill_vbool(_Dest, _Last, _Val);
            return _Last;
        } else {
            auto _UDest = _Get_unwrapped_n(_Dest, _Count);
#if _HAS_CXX20
            if (!_RPC is_constant_evaluated())
#endif // _HAS_CXX20
            {
                if constexpr (_Fill_memset_is_safe<decltype(_UDest), _Ty>) {
                    _Fill_memset(_UDest, _Val, static_cast<size_t>(_Count));
                    _Seek_wrapped(_Dest, _UDest + _Count);
                    return _Dest;
                } else if constexpr (_Fill_zero_memset_is_safe<decltype(_UDest), _Ty>) {
                    if (_Is_all_bits_zero(_Val)) {
                        _Fill_zero_memset(_UDest, static_cast<size_t>(_Count));
                        _Seek_wrapped(_Dest, _UDest + _Count);
                        return _Dest;
                    }
                }
            }

            for (; 0 < _Count; --_Count, (void) ++_UDest) {
                *_UDest = _Val;
            }

            _Seek_wrapped(_Dest, _UDest);
        }
    }
    return _Dest;
}

#if _HAS_CXX17
template <class _ExPo, class _FwdIt, class _Diff, class _Ty, _Enable_if_execution_policy_t<_ExPo> = 0>
_FwdIt fill_n(_ExPo&&, _FwdIt _Dest, _Diff _Count_raw, const _Ty& _Val) noexcept /* terminates */ {
    // copy _Val _Count times through [_Dest, ...)
    // not parallelized as benchmarks show it isn't worth it
    _REQUIRE_PARALLEL_ITERATOR(_FwdIt);
    return _RPC fill_n(_Dest, _Count_raw, _Val);
}
#endif // _HAS_CXX17

#ifdef __cpp_lib_concepts
namespace ranges {
    class _Fill_n_fn : private _Not_quite_object {
    public:
        using _Not_quite_object::_Not_quite_object;

        template <class _Ty, output_iterator<const _Ty&> _It>
        constexpr _It operator()(_It _First, iter_difference_t<_It> _Count, const _Ty& _Value) const {
            if (_Count > 0) {
                auto _UFirst = _Get_unwrapped_n(::std::move(_First), _Count);
                if (!_RPC is_constant_evaluated()) {
                    if constexpr (_Fill_memset_is_safe<decltype(_UFirst), _Ty>) {
                        _Fill_memset(_UFirst, _Value, static_cast<size_t>(_Count));
                        _Seek_wrapped(_First, _UFirst + _Count); // no need to move since _UFirst is a pointer
                        return _First;
                    } else if constexpr (_Fill_zero_memset_is_safe<decltype(_UFirst), _Ty>) {
                        if (_Is_all_bits_zero(_Value)) {
                            _Fill_zero_memset(_UFirst, static_cast<size_t>(_Count));
                            _Seek_wrapped(_First, _UFirst + _Count); // no need to move since _UFirst is a pointer
                            return _First;
                        }
                    }
                }

                for (; _Count > 0; ++_UFirst, (void) --_Count) {
                    *_UFirst = _Value;
                }

                _Seek_wrapped(_First, ::std::move(_UFirst));
            }

            return _First;
        }
    };

    inline constexpr _Fill_n_fn fill_n{_Not_quite_object::_Construct_tag{}};
} // namespace ranges
#endif // __cpp_lib_concepts

template <class _Ty1, class _Ty2, class = void>
_INLINE_VAR constexpr bool _Can_compare_with_operator_equal = false;

template <class _Ty1, class _Ty2>
_INLINE_VAR constexpr bool
    _Can_compare_with_operator_equal<_Ty1, _Ty2, ::std::void_t<decltype(declval<_Ty1&>() == declval<_Ty2&>())>> =
        true;

template <class _Ty1, class _Ty2>
_INLINE_VAR constexpr bool _Is_pointer_address_comparable =
    _Can_compare_with_operator_equal<_Ty1*,
        _Ty2*> && (_Is_pointer_address_convertible<_Ty1, _Ty2> || _Is_pointer_address_convertible<_Ty2, _Ty1>);

// _Can_memcmp_elements<_Elem1, _Elem2> reports whether `_Elem1 == _Elem2` can be optimized to memcmp.
// Here, _Elem1 and _Elem2 aren't top-level const, because we remove_const_t before using _Can_memcmp_elements.

// Integral types are eligible for memcmp in very specific cases.
// * They must be the same size. (`int == long` is eligible; `int == long long` isn't.)
// * The usual arithmetic conversions must preserve bit patterns. (This is true for `int == unsigned int`,
//   but false for `short == unsigned short`.)
#if defined(__clang__) || defined(__GNUC__) || defined(__GNUG__)
#else
#pragma warning(push)
#pragma warning(disable : 4806) // no value of type 'bool' promoted to type 'char' can equal the given constant
#endif
template <class _Elem1, class _Elem2,
    bool = sizeof(_Elem1) == sizeof(_Elem2) && ::std::is_integral_v<_Elem1>&& ::std::is_integral_v<_Elem2>>
_INLINE_VAR constexpr bool _Can_memcmp_elements =
    ::std::is_same_v<_Elem1, bool> || ::std::is_same_v<_Elem2, bool> || static_cast<_Elem1>(-1) == static_cast<_Elem2>(-1);
#if defined(__clang__) || defined(__GNUC__) || defined(__GNUG__)
#else
#pragma warning(pop)
#endif

#ifdef __cpp_lib_byte
// Allow memcmping std::::std::byte.
// inline is required here as explicit specializations of variable templates are problematic in C++14.
// However, std::::std::byte is C++17 and above so we are safe.
template <>
inline constexpr bool _Can_memcmp_elements<::std::byte, ::std::byte, false> = true;
#endif // __cpp_lib_byte

// Pointer elements are eligible for memcmp when they point to the same type, ignoring cv-qualification.
// This handles pointers to object types, pointers to void, and pointers to function types.
template <class _Ty1, class _Ty2>
_INLINE_VAR constexpr bool _Can_memcmp_elements<_Ty1*, _Ty2*, false> = _Is_pointer_address_comparable<_Ty1, _Ty2>;

template <class _Elem1, class _Elem2>
_INLINE_VAR constexpr bool _Can_memcmp_elements<_Elem1, _Elem2, false> = false;

// _Can_memcmp_elements_with_pred<_Elem1, _Elem2, _Pr> reports whether the memcmp optimization is applicable,
// given contiguously stored elements. (This avoids having to repeat the metaprogramming that finds the element types.)
// _Elem1 and _Elem2 aren't top-level const here.
template <class _Elem1, class _Elem2, class _Pr>
_INLINE_VAR constexpr bool _Can_memcmp_elements_with_pred = false;

// With ::std::equal_to<_Elem3> we need to make sure that both _Elem1 and _Elem2 are convertible to _Elem3 without changing
// object representation (we use _Iter_copy_cat for this task) and _Elem3 can be safely memcmp'ed with itself
template <class _Elem1, class _Elem2, class _Elem3>
_INLINE_VAR constexpr bool _Can_memcmp_elements_with_pred<_Elem1, _Elem2, ::std::equal_to<_Elem3>> =
    _Iter_copy_cat<_Elem1*, _Elem3*>::_Bitcopy_constructible&& _Iter_copy_cat<_Elem2*,
        _Elem3*>::_Bitcopy_constructible&& _Can_memcmp_elements<::std::remove_cv_t<_Elem3>, ::std::remove_cv_t<_Elem3>>;

// ::std::equal_to<> is transparent.
template <class _Elem1, class _Elem2>
_INLINE_VAR constexpr bool _Can_memcmp_elements_with_pred<_Elem1, _Elem2, ::std::equal_to<>> =
    _Can_memcmp_elements<_Elem1, _Elem2>;

#ifdef __cpp_lib_concepts
// ranges::equal_to is also transparent.
template <class _Elem1, class _Elem2>
_INLINE_VAR constexpr bool _Can_memcmp_elements_with_pred<_Elem1, _Elem2, _RANGES ::std::equal_to> =
    _Can_memcmp_elements<_Elem1, _Elem2>;
#endif // __cpp_lib_concepts

// _Equal_memcmp_is_safe<_Iter1, _Iter2, _Pr> reports whether we can activate the memcmp optimization
// for arbitrary iterators and predicates.
// It ignores top-level constness on the iterators and on the elements.
template <class _Iter1, class _Iter2, class _Pr>
_INLINE_VAR constexpr bool _Equal_memcmp_is_safe_helper =
    _Iterators_are_contiguous<_Iter1, _Iter2> //
    && !_Iterator_is_volatile<_Iter1> && !_Iterator_is_volatile<_Iter2> //
    && _Can_memcmp_elements_with_pred<_Iter_value_t<_Iter1>, _Iter_value_t<_Iter2>, _Pr>;

template <class _Iter1, class _Iter2, class _Pr>
_INLINE_VAR constexpr bool _Equal_memcmp_is_safe =
    _Equal_memcmp_is_safe_helper<remove_const_t<_Iter1>, remove_const_t<_Iter2>, _Pr>;

template <class _CtgIt1, class _CtgIt2>
_NODISCARD int _Memcmp_ranges(_CtgIt1 _First1, _CtgIt1 _Last1, _CtgIt2 _First2) {
    _STL_INTERNAL_STATIC_ASSERT(sizeof(_Iter_value_t<_CtgIt1>) == sizeof(_Iter_value_t<_CtgIt2>));
    const auto _First1_ch = reinterpret_cast<const char*>(_To_address(_First1));
    const auto _Last1_ch  = reinterpret_cast<const char*>(_To_address(_Last1));
    const auto _First2_ch = reinterpret_cast<const char*>(_To_address(_First2));
    return _CSTD memcmp(_First1_ch, _First2_ch, static_cast<size_t>(_Last1_ch - _First1_ch));
}

template <class _CtgIt1, class _CtgIt2>
_NODISCARD int _Memcmp_count(_CtgIt1 _First1, _CtgIt2 _First2, const size_t _Count) {
    _STL_INTERNAL_STATIC_ASSERT(sizeof(_Iter_value_t<_CtgIt1>) == sizeof(_Iter_value_t<_CtgIt2>));
    const auto _First1_ch = reinterpret_cast<const char*>(_To_address(_First1));
    const auto _First2_ch = reinterpret_cast<const char*>(_To_address(_First2));
    return _CSTD memcmp(_First1_ch, _First2_ch, _Count * sizeof(_Iter_value_t<_CtgIt1>));
}

template <class _InIt1, class _InIt2, class _Pr>
_NODISCARD _CONSTEXPR20 bool equal(const _InIt1 _First1, const _InIt1 _Last1, const _InIt2 _First2, _Pr _Pred) {
    // compare [_First1, _Last1) to [_First2, ...)
    _Adl_verify_range(_First1, _Last1);
    auto _UFirst1      = _Get_unwrapped(_First1);
    const auto _ULast1 = _Get_unwrapped(_Last1);
    auto _UFirst2      = _Get_unwrapped_n(_First2, _Idl_distance<_InIt1>(_UFirst1, _ULast1));
    if constexpr (_Equal_memcmp_is_safe<decltype(_UFirst1), decltype(_UFirst2), _Pr>) {
#if _HAS_CXX20
        if (!_RPC is_constant_evaluated())
#endif // _HAS_CXX20
        {
            return _Memcmp_ranges(_UFirst1, _ULast1, _UFirst2) == 0;
        }
    }

    for (; _UFirst1 != _ULast1; ++_UFirst1, (void) ++_UFirst2) {
        if (!_Pred(*_UFirst1, *_UFirst2)) {
            return false;
        }
    }

    return true;
}

#if _HAS_CXX17
template <class _ExPo, class _FwdIt1, class _FwdIt2, class _Pr, _Enable_if_execution_policy_t<_ExPo> = 0>
_NODISCARD bool equal(_ExPo&& _Exec, const _FwdIt1 _First1, const _FwdIt1 _Last1, const _FwdIt2 _First2,
    _Pr _Pred) noexcept; // terminates
#endif // _HAS_CXX17

template <class _InIt1, class _InIt2>
_NODISCARD _CONSTEXPR20 bool equal(const _InIt1 _First1, const _InIt1 _Last1, const _InIt2 _First2) {
    // compare [_First1, _Last1) to [_First2, ...)
    return _RPC equal(_First1, _Last1, _First2, ::std::equal_to<>{});
}

#if _HAS_CXX17
template <class _ExPo, class _FwdIt1, class _FwdIt2, _Enable_if_execution_policy_t<_ExPo> = 0>
_NODISCARD bool equal(_ExPo&& _Exec, const _FwdIt1 _First1, const _FwdIt1 _Last1, const _FwdIt2 _First2) noexcept
/* terminates */ {
    // compare [_First1, _Last1) to [_First2, ...)
    return _RPC equal(::std::forward<_ExPo>(_Exec), _First1, _Last1, _First2, ::std::equal_to{});
}
#endif // _HAS_CXX17

template <class _InIt1, class _InIt2, class _Pr>
_NODISCARD _CONSTEXPR20 bool equal(
    const _InIt1 _First1, const _InIt1 _Last1, const _InIt2 _First2, const _InIt2 _Last2, _Pr _Pred) {
    // compare [_First1, _Last1) to [_First2, _Last2)
    _Adl_verify_range(_First1, _Last1);
    _Adl_verify_range(_First2, _Last2);
    auto _UFirst1      = _Get_unwrapped(_First1);
    const auto _ULast1 = _Get_unwrapped(_Last1);
    auto _UFirst2      = _Get_unwrapped(_First2);
    const auto _ULast2 = _Get_unwrapped(_Last2);
    if constexpr (_Is_random_iter_v<_InIt1> && _Is_random_iter_v<_InIt2>) {
        if (_ULast1 - _UFirst1 != _ULast2 - _UFirst2) {
            return false;
        }

        return _RPC equal(_UFirst1, _ULast1, _UFirst2, _Pass_fn(_Pred));
    } else {
        for (;;) {
            if (_UFirst1 == _ULast1) {
                return _UFirst2 == _ULast2;
            }

            if (_UFirst2 == _ULast2) {
                return false;
            }

            if (!_Pred(*_UFirst1, *_UFirst2)) {
                return false;
            }

            ++_UFirst1;
            ++_UFirst2;
        }
    }
}

#if _HAS_CXX17
template <class _ExPo, class _FwdIt1, class _FwdIt2, class _Pr, _Enable_if_execution_policy_t<_ExPo> = 0>
_NODISCARD bool equal(_ExPo&& _Exec, const _FwdIt1 _First1, const _FwdIt1 _Last1, const _FwdIt2 _First2,
    const _FwdIt2 _Last2, _Pr _Pred) noexcept; // terminates
#endif // _HAS_CXX17

template <class _InIt1, class _InIt2>
_NODISCARD _CONSTEXPR20 bool equal(
    const _InIt1 _First1, const _InIt1 _Last1, const _InIt2 _First2, const _InIt2 _Last2) {
    // compare [_First1, _Last1) to [_First2, _Last2)
    return _RPC equal(_First1, _Last1, _First2, _Last2, ::std::equal_to<>{});
}

#if _HAS_CXX17
template <class _ExPo, class _FwdIt1, class _FwdIt2, _Enable_if_execution_policy_t<_ExPo> = 0>
_NODISCARD bool equal(_ExPo&& _Exec, const _FwdIt1 _First1, const _FwdIt1 _Last1, const _FwdIt2 _First2,
    const _FwdIt2 _Last2) noexcept /* terminates */ {
    // compare [_First1, _Last1) to [_First2, _Last2)
    return _RPC equal(::std::forward<_ExPo>(_Exec), _First1, _Last1, _First2, _Last2, ::std::equal_to{});
}
#endif // _HAS_CXX17

#ifdef __cpp_lib_concepts
namespace ranges {
    template <forward_range _Rng, class _It>
    _NODISCARD constexpr iterator_t<_Rng> _Rewrap_iterator(_Rng&& _Range, _It&& _Val) {
        _STL_INTERNAL_STATIC_ASSERT(::std::is_same_v<remove_cvref_t<_It>, _Unwrapped_t<iterator_t<_Rng>>>);

        if constexpr (::std::is_same_v<remove_cvref_t<_It>, iterator_t<_Rng>>) {
            return ::std::forward<_It>(_Val);
        } else {
            auto _Result = _RANGES begin(_Range);
            _Result._Seek_to(::std::forward<_It>(_Val));
            return _Result;
        }
    }

    template <class _In1, class _In2>
    struct in_in_result {
        /* [[no_unique_address]] */ _In1 in1;
        /* [[no_unique_address]] */ _In2 in2;

        template <_Convertible_from<const _In1&> _IIn1, _Convertible_from<const _In2&> _IIn2>
        constexpr operator in_in_result<_IIn1, _IIn2>() const& {
            return {in1, in2};
        }

        template <_Convertible_from<_In1> _IIn1, _Convertible_from<_In2> _IIn2>
        constexpr operator in_in_result<_IIn1, _IIn2>() && {
            return {::std::move(in1), ::std::move(in2)};
        }
    };

    template <class _In1, class _In2>
    using mismatch_result = in_in_result<_In1, _In2>;

    // clang-format off
    template <input_iterator _It1, input_iterator _It2, class _Pr, class _Pj1, class _Pj2>
        requires indirectly_comparable<_It1, _It2, _Pr, _Pj1, _Pj2>
    _NODISCARD constexpr mismatch_result<_It1, _It2> _Mismatch_n(
        _It1 _First1, _It2 _First2, iter_difference_t<_It1> _Count, _Pr _Pred, _Pj1 _Proj1, _Pj2 _Proj2) {
        // clang-format on
        _STL_INTERNAL_CHECK(_Count >= 0);
        for (; _Count != 0; ++_First1, (void) ++_First2, --_Count) {
            if (!::std::invoke(_Pred, ::std::invoke(_Proj1, *_First1), ::std::invoke(_Proj2, *_First2))) {
                break;
            }
        }

        return {::std::move(_First1), ::std::move(_First2)};
    }

    // clang-format off
    template <input_iterator _It1, sentinel_for<_It1> _Se1, input_iterator _It2, sentinel_for<_It2> _Se2, class _Pr,
        class _Pj1, class _Pj2>
        requires indirectly_comparable<_It1, _It2, _Pr, _Pj1, _Pj2>
    _NODISCARD constexpr mismatch_result<_It1, _It2> _Mismatch_4(
        _It1 _First1, _Se1 _Last1, _It2 _First2, _Se2 _Last2, _Pr _Pred, _Pj1 _Proj1, _Pj2 _Proj2) {
        // clang-format on

        for (; _First1 != _Last1 && _First2 != _Last2; ++_First1, (void) ++_First2) {
            if (!::std::invoke(_Pred, ::std::invoke(_Proj1, *_First1), ::std::invoke(_Proj2, *_First2))) {
                break;
            }
        }

        return {::std::move(_First1), ::std::move(_First2)};
    }

    class _Mismatch_fn : private _Not_quite_object {
    public:
        using _Not_quite_object::_Not_quite_object;

        // clang-format off
        template <input_iterator _It1, sentinel_for<_It1> _Se1, input_iterator _It2, sentinel_for<_It2> _Se2,
            class _Pr = ranges::equal_to, class _Pj1 = identity, class _Pj2 = identity>
            requires indirectly_comparable<_It1, _It2, _Pr, _Pj1, _Pj2>
        _NODISCARD constexpr mismatch_result<_It1, _It2> operator()(_It1 _First1, _Se1 _Last1,
            _It2 _First2, _Se2 _Last2, _Pr _Pred = {}, _Pj1 _Proj1 = {}, _Pj2 _Proj2 = {}) const {
            // clang-format on
            _Adl_verify_range(_First1, _Last1);
            _Adl_verify_range(_First2, _Last2);

            if constexpr (sized_sentinel_for<_Se1, _It1> && sized_sentinel_for<_Se2, _It2>) {
                iter_difference_t<_It1> _Count1       = _Last1 - _First1;
                const iter_difference_t<_It2> _Count2 = _Last2 - _First2;
                if (_Count1 > _Count2) {
                    _Count1 = static_cast<decltype(_Count1)>(_Count2);
                }

                auto _Result = _RANGES _Mismatch_n(_Get_unwrapped(::std::move(_First1)),
                    _Get_unwrapped(::std::move(_First2)), _Count1, _Pass_fn(_Pred), _Pass_fn(_Proj1), _Pass_fn(_Proj2));
                _Seek_wrapped(_First1, ::std::move(_Result.in1));
                _Seek_wrapped(_First2, ::std::move(_Result.in2));
                return {::std::move(_First1), ::std::move(_First2)};
            } else {
                auto _Result = _RANGES _Mismatch_4(_Get_unwrapped(::std::move(_First1)),
                    _Get_unwrapped(::std::move(_Last1)), _Get_unwrapped(::std::move(_First2)),
                    _Get_unwrapped(::std::move(_Last2)), _Pass_fn(_Pred), _Pass_fn(_Proj1), _Pass_fn(_Proj2));
                _Seek_wrapped(_First1, ::std::move(_Result.in1));
                _Seek_wrapped(_First2, ::std::move(_Result.in2));
                return {::std::move(_First1), ::std::move(_First2)};
            }
        }

        // clang-format off
        template <input_range _Rng1, input_range _Rng2, class _Pr = ranges::equal_to, class _Pj1 = identity,
            class _Pj2 = identity>
            requires indirectly_comparable<iterator_t<_Rng1>, iterator_t<_Rng2>, _Pr, _Pj1, _Pj2>
        _NODISCARD constexpr mismatch_result<borrowed_iterator_t<_Rng1>, borrowed_iterator_t<_Rng2>> operator()(
            _Rng1&& _Range1, _Rng2&& _Range2, _Pr _Pred = {}, _Pj1 _Proj1 = {}, _Pj2 _Proj2 = {}) const {
            // clang-format on
            if constexpr (sized_range<_Rng1> && sized_range<_Rng2>) {
                range_difference_t<_Rng1> _Count1       = _RANGES distance(_Range1);
                const range_difference_t<_Rng2> _Count2 = _RANGES distance(_Range2);
                if (_Count1 > _Count2) {
                    _Count1 = static_cast<range_difference_t<_Rng1>>(_Count2);
                }

                auto _First1 = _RANGES begin(_Range1);
                auto _First2 = _RANGES begin(_Range2);
                auto _Result = _RANGES _Mismatch_n(_Get_unwrapped(::std::move(_First1)),
                    _Get_unwrapped(::std::move(_First2)), _Count1, _Pass_fn(_Pred), _Pass_fn(_Proj1), _Pass_fn(_Proj2));
                _Seek_wrapped(_First1, ::std::move(_Result.in1));
                _Seek_wrapped(_First2, ::std::move(_Result.in2));
                return {::std::move(_First1), ::std::move(_First2)};
            } else {
                auto _First1 = _RANGES begin(_Range1);
                auto _First2 = _RANGES begin(_Range2);
                auto _Result = _RANGES _Mismatch_4(_Get_unwrapped(::std::move(_First1)), _Uend(_Range1),
                    _Get_unwrapped(::std::move(_First2)), _Uend(_Range2), _Pass_fn(_Pred), _Pass_fn(_Proj1),
                    _Pass_fn(_Proj2));
                _Seek_wrapped(_First1, ::std::move(_Result.in1));
                _Seek_wrapped(_First2, ::std::move(_Result.in2));
                return {::std::move(_First1), ::std::move(_First2)};
            }
        }
    };

    inline constexpr _Mismatch_fn mismatch{_Not_quite_object::_Construct_tag{}};
} // namespace ranges
#endif // __cpp_lib_concepts

template <class _Elem1, class _Elem2>
_INLINE_VAR constexpr bool _Lex_compare_memcmp_classify_elements = ::std::conjunction_v<_Is_character_or_bool<_Elem1>,
    _Is_character_or_bool<_Elem2>, ::std::is_unsigned<_Elem1>, ::std::is_unsigned<_Elem2>>;

#ifdef __cpp_lib_byte
template <>
inline constexpr bool _Lex_compare_memcmp_classify_elements<::std::byte, ::std::byte> = true;
#endif // __cpp_lib_byte

template <class _Elem1, class _Elem2, class _Pr>
struct _Lex_compare_memcmp_classify_pred {
    using _Pred = void;
};

template <class _Elem1, class _Elem2, class _Elem3>
struct _Lex_compare_memcmp_classify_pred<_Elem1, _Elem2, ::std::less<_Elem3>> {
    using _Pred = ::std::conditional_t<_Lex_compare_memcmp_classify_elements<_Elem3, _Elem3> //
                                    && _Iter_copy_cat<_Elem1*, _Elem3*>::_Bitcopy_constructible
                                    && _Iter_copy_cat<_Elem2*, _Elem3*>::_Bitcopy_constructible,
        ::std::less<int>, void>;
};

template <class _Elem1, class _Elem2>
struct _Lex_compare_memcmp_classify_pred<_Elem1, _Elem2, ::std::less<>> {
    using _Pred = ::std::conditional_t<_Lex_compare_memcmp_classify_elements<_Elem1, _Elem2>, ::std::less<int>, void>;
};

template <class _Elem1, class _Elem2, class _Elem3>
struct _Lex_compare_memcmp_classify_pred<_Elem1, _Elem2, ::std::greater<_Elem3>> {
    using _Pred = ::std::conditional_t<_Lex_compare_memcmp_classify_elements<_Elem3, _Elem3> //
                                    && _Iter_copy_cat<_Elem1*, _Elem3*>::_Bitcopy_constructible
                                    && _Iter_copy_cat<_Elem2*, _Elem3*>::_Bitcopy_constructible,
        ::std::greater<int>, void>;
};

template <class _Elem1, class _Elem2>
struct _Lex_compare_memcmp_classify_pred<_Elem1, _Elem2, ::std::greater<>> {
    using _Pred = ::std::conditional_t<_Lex_compare_memcmp_classify_elements<_Elem1, _Elem2>, ::std::greater<int>, void>;
};

#ifdef __cpp_lib_concepts
template <class _Elem1, class _Elem2>
struct _Lex_compare_memcmp_classify_pred<_Elem1, _Elem2, _RANGES ::std::less> {
    using _Pred = ::std::conditional_t<_Lex_compare_memcmp_classify_elements<_Elem1, _Elem2>, ::std::less<int>, void>;
};

template <class _Elem1, class _Elem2>
struct _Lex_compare_memcmp_classify_pred<_Elem1, _Elem2, _RANGES ::std::greater> {
    using _Pred = ::std::conditional_t<_Lex_compare_memcmp_classify_elements<_Elem1, _Elem2>, ::std::greater<int>, void>;
};
#endif // __cpp_lib_concepts

template <class _It1, class _It2, class _Pr>
using _Lex_compare_memcmp_classify =
    ::std::conditional_t<_Iterators_are_contiguous<_It1, _It2> && !_Iterator_is_volatile<_It1> && !_Iterator_is_volatile<_It2>,
        typename _Lex_compare_memcmp_classify_pred<_Iter_value_t<_It1>, _Iter_value_t<_It2>, _Pr>::_Pred, void>;

template <class _InIt1, class _InIt2, class _Pr>
_NODISCARD _CONSTEXPR20 bool lexicographical_compare(
    const _InIt1 _First1, const _InIt1 _Last1, const _InIt2 _First2, const _InIt2 _Last2, _Pr _Pred) {
    // order [_First1, _Last1) vs. [_First2, _Last2)
    _Adl_verify_range(_First1, _Last1);
    _Adl_verify_range(_First2, _Last2);
    auto _UFirst1      = _Get_unwrapped(_First1);
    const auto _ULast1 = _Get_unwrapped(_Last1);
    auto _UFirst2      = _Get_unwrapped(_First2);
    const auto _ULast2 = _Get_unwrapped(_Last2);

    using _Memcmp_pred = _Lex_compare_memcmp_classify<decltype(_UFirst1), decltype(_UFirst2), _Pr>;
    if constexpr (!::std::is_void_v<_Memcmp_pred>) {
#if _HAS_CXX20
        if (!_RPC is_constant_evaluated())
#endif // _HAS_CXX20
        {
            const auto _Num1 = static_cast<size_t>(_ULast1 - _UFirst1);
            const auto _Num2 = static_cast<size_t>(_ULast2 - _UFirst2);
            const int _Ans   = _Memcmp_count(_UFirst1, _UFirst2, (::std::min) (_Num1, _Num2));
            return _Memcmp_pred{}(_Ans, 0) || (_Ans == 0 && _Num1 < _Num2);
        }
    }

    for (; _UFirst1 != _ULast1 && _UFirst2 != _ULast2; ++_UFirst1, (void) ++_UFirst2) { // something to compare, do it
        if (_DEBUG_LT_PRED(_Pred, *_UFirst1, *_UFirst2)) {
            return true;
        } else if (_Pred(*_UFirst2, *_UFirst1)) {
            return false;
        }
    }

    return _UFirst1 == _ULast1 && _UFirst2 != _ULast2;
}

template <class _InIt1, class _InIt2>
_NODISCARD _CONSTEXPR20 bool lexicographical_compare(
    const _InIt1 _First1, const _InIt1 _Last1, const _InIt2 _First2, const _InIt2 _Last2) {
    // order [_First1, _Last1) vs. [_First2, _Last2)
    return _RPC lexicographical_compare(_First1, _Last1, _First2, _Last2, ::std::less<>{});
}

#if _HAS_CXX17
template <class _ExPo, class _FwdIt1, class _FwdIt2, class _Pr, _Enable_if_execution_policy_t<_ExPo> = 0>
_NODISCARD bool lexicographical_compare(_ExPo&&, const _FwdIt1 _First1, const _FwdIt1 _Last1, const _FwdIt2 _First2,
    const _FwdIt2 _Last2, _Pr _Pred) noexcept /* terminates */ {
    // order [_First1, _Last1) vs. [_First2, _Last2)
    // not parallelized at present, parallelism expected to be feasible in a future release
    _REQUIRE_PARALLEL_ITERATOR(_FwdIt1);
    _REQUIRE_PARALLEL_ITERATOR(_FwdIt2);
    return _RPC lexicographical_compare(_First1, _Last1, _First2, _Last2, _Pass_fn(_Pred));
}

template <class _ExPo, class _FwdIt1, class _FwdIt2, _Enable_if_execution_policy_t<_ExPo> = 0>
_NODISCARD bool lexicographical_compare(_ExPo&&, const _FwdIt1 _First1, const _FwdIt1 _Last1, const _FwdIt2 _First2,
    const _FwdIt2 _Last2) noexcept /* terminates */ {
    // order [_First1, _Last1) vs. [_First2, _Last2)
    // not parallelized at present, parallelism expected to be feasible in a future release
    _REQUIRE_PARALLEL_ITERATOR(_FwdIt1);
    _REQUIRE_PARALLEL_ITERATOR(_FwdIt2);
    return _RPC lexicographical_compare(_First1, _Last1, _First2, _Last2);
}
#endif // _HAS_CXX17

#ifdef __cpp_lib_concepts
template <class _Elem1, class _Elem2, class _Cmp>
struct _Lex_compare_three_way_memcmp_classify_comp {
    using _Comp = void;
};

template <class _Elem1, class _Elem2>
struct _Lex_compare_three_way_memcmp_classify_comp<_Elem1, _Elem2, compare_three_way> {
    using _Comp = ::std::conditional_t<_Lex_compare_memcmp_classify_elements<_Elem1,
                                    _Elem2> && three_way_comparable_with<const _Elem1&, const _Elem2&>,
        compare_three_way, void>;
};

template <class _Elem1, class _Elem2>
struct _Lex_compare_three_way_memcmp_classify_comp<_Elem1, _Elem2, _Strong_order::_Cpo> {
    using _Comp =
        ::std::conditional_t<_Lex_compare_memcmp_classify_elements<_Elem1, _Elem2> && _Can_strong_order<_Elem1, _Elem2>,
            _Strong_order::_Cpo, void>;
};

template <class _Elem1, class _Elem2>
struct _Lex_compare_three_way_memcmp_classify_comp<_Elem1, _Elem2, _Weak_order::_Cpo> {
    using _Comp =
        ::std::conditional_t<_Lex_compare_memcmp_classify_elements<_Elem1, _Elem2> && _Can_weak_order<_Elem1, _Elem2>,
            _Weak_order::_Cpo, void>;
};

template <class _Elem1, class _Elem2>
struct _Lex_compare_three_way_memcmp_classify_comp<_Elem1, _Elem2, _Partial_order::_Cpo> {
    using _Comp =
        ::std::conditional_t<_Lex_compare_memcmp_classify_elements<_Elem1, _Elem2> && _Can_partial_order<_Elem1, _Elem2>,
            _Partial_order::_Cpo, void>;
};

template <class _It1, class _It2, class _Cmp>
using _Lex_compare_three_way_memcmp_classify =
    ::std::conditional_t<_Iterators_are_contiguous<_It1, _It2> && !_Iterator_is_volatile<_It1> && !_Iterator_is_volatile<_It2>,
        typename _Lex_compare_three_way_memcmp_classify_comp<_Iter_value_t<_It1>, _Iter_value_t<_It2>, _Cmp>::_Comp,
        void>;

template <class _InIt1, class _InIt2, class _Cmp>
_NODISCARD constexpr auto lexicographical_compare_three_way(const _InIt1 _First1, const _InIt1 _Last1,
    const _InIt2 _First2, const _InIt2 _Last2, _Cmp _Comp) -> decltype(_Comp(*_First1, *_First2)) {
    _Adl_verify_range(_First1, _Last1);
    _Adl_verify_range(_First2, _Last2);
    auto _UFirst1      = _Get_unwrapped(_First1);
    const auto _ULast1 = _Get_unwrapped(_Last1);
    auto _UFirst2      = _Get_unwrapped(_First2);
    const auto _ULast2 = _Get_unwrapped(_Last2);

    using _Memcmp_pred = _Lex_compare_three_way_memcmp_classify<decltype(_UFirst1), decltype(_UFirst2), _Cmp>;
    if constexpr (!::std::is_void_v<_Memcmp_pred>) {
        if (!_RPC is_constant_evaluated()) {
            const auto _Num1 = static_cast<size_t>(_ULast1 - _UFirst1);
            const auto _Num2 = static_cast<size_t>(_ULast2 - _UFirst2);
            const int _Ans   = _Memcmp_count(_UFirst1, _UFirst2, (::std::min) (_Num1, _Num2));
            if (_Ans == 0) {
                return _Num1 <=> _Num2;
            } else {
                return _Memcmp_pred{}(_Ans, 0);
            }
        }
    }

    for (;;) {
        if (_UFirst1 == _ULast1) {
            return _UFirst2 == _ULast2 ? strong_ordering::equal : strong_ordering::::std::less;
        }

        if (_UFirst2 == _ULast2) {
            return strong_ordering::::std::greater;
        }

        if (const auto _CmpResult = _Comp(*_UFirst1, *_UFirst2); _CmpResult != 0) {
            return _CmpResult;
        }

        ++_UFirst1;
        ++_UFirst2;
    }
}

template <class _InIt1, class _InIt2>
_NODISCARD constexpr auto lexicographical_compare_three_way(
    const _InIt1 _First1, const _InIt1 _Last1, const _InIt2 _First2, const _InIt2 _Last2) {
    return _RPC lexicographical_compare_three_way(_First1, _Last1, _First2, _Last2, compare_three_way{});
}
#endif // __cpp_lib_concepts

template <class _InIt, class _Ty>
_NODISCARD constexpr bool _Within_limits(const _InIt&, const _Ty& _Val) {
    // check whether _Val is within the limits of _Elem
    if constexpr (::std::is_same_v<_Ty, bool>) {
        return true;
    }
#ifdef __cpp_lib_byte
    else if constexpr (::std::is_same_v<_Ty, ::std::byte>) {
        return true;
    }
#endif // __cpp_lib_byte
    else {
        using _Elem = _Iter_value_t<_InIt>;
        _STL_INTERNAL_STATIC_ASSERT(sizeof(_Elem) == 1);
        if constexpr (::std::is_same_v<_Elem, bool>) {
            return _Val == true || _Val == false;
        } else if constexpr (::std::is_signed_v<_Elem>) {
            if constexpr (::std::is_signed_v<_Ty>) {
                // signed _Elem, signed _Ty
                return SCHAR_MIN <= _Val && _Val <= SCHAR_MAX;
            } else {
                if constexpr (-1 == static_cast<_Ty>(-1)) {
                    // signed _Elem, unsigned _Ty, -1 == static_cast<_Ty>(-1)
                    return _Val <= SCHAR_MAX || static_cast<_Ty>(SCHAR_MIN) <= _Val;
                } else {
                    // signed _Elem, unsigned _Ty, -1 != static_cast<_Ty>(-1)
                    return _Val <= SCHAR_MAX;
                }
            }
        } else {
            if constexpr (::std::is_signed_v<_Ty>) {
                // unsigned _Elem, signed _Ty
                return 0 <= _Val && _Val <= UCHAR_MAX;
            } else {
                // unsigned _Elem, unsigned _Ty
                return _Val <= UCHAR_MAX;
            }
        }
    }
}

template <class _Iter, class _Ty>
_INLINE_VAR constexpr bool _Memchr_in_find_is_safe =
    _Iterator_is_contiguous<_Iter>&&
        disjunction_v<::std::conjunction<is_integral<_Ty>, _Is_character_or_bool<_Iter_value_t<_Iter>>>
#ifdef __cpp_lib_byte
            ,
            ::std::conjunction<is_same<_Ty, ::std::byte>, is_same<_Iter_value_t<_Iter>, ::std::byte>>
#endif // __cpp_lib_byte
            > && !::std::is_volatile_v<remove_reference_t<_Iter_ref_t<_Iter>>>;

template <class _InIt, class _Ty>
_NODISCARD _CONSTEXPR20 _InIt _Find_unchecked(_InIt _First, const _InIt _Last, const _Ty& _Val) {
    // find first matching _Val; choose optimization
    // activate optimization for contiguous iterators to (const) bytes and integral values
#if _HAS_CXX20
    if (!_RPC is_constant_evaluated())
#endif // _HAS_CXX20
    {
        if constexpr (_Memchr_in_find_is_safe<_InIt, _Ty>) {
            // find first ::std::byte matching integral _Val
            if (!_Within_limits(_First, _Val)) {
                return _Last;
            }
            const auto _First_ptr = _To_address(_First);
            const auto _Result    = static_cast<remove_reference_t<_Iter_ref_t<_InIt>>*>(
                _CSTD memchr(_First_ptr, static_cast<unsigned char>(_Val), static_cast<size_t>(_Last - _First)));
            if constexpr (::std::is_pointer_v<_InIt>) {
                return _Result ? _Result : _Last;
            } else {
                return _Result ? _First + (_Result - _First_ptr) : _Last;
            }
        }
    }

    for (; _First != _Last; ++_First) {
        if (*_First == _Val) {
            break;
        }
    }

    return _First;
}

template <class _InIt, class _Ty>
_NODISCARD _CONSTEXPR20 _InIt find(_InIt _First, const _InIt _Last, const _Ty& _Val) { // find first matching _Val
    _Adl_verify_range(_First, _Last);
    if constexpr (_Is_vb_iterator<_InIt> && ::std::is_same_v<_Ty, bool>) {
        return _Find_vbool(_First, _Last, _Val);
    } else {
        _Seek_wrapped(_First, _Find_unchecked(_Get_unwrapped(_First), _Get_unwrapped(_Last), _Val));
        return _First;
    }
}

#if _HAS_CXX17
template <class _ExPo, class _FwdIt, class _Ty, _Enable_if_execution_policy_t<_ExPo> = 0>
_NODISCARD _FwdIt find(_ExPo&& _Exec, _FwdIt _First, const _FwdIt _Last, const _Ty& _Val) noexcept; // terminates
#endif // _HAS_CXX17

#ifdef __cpp_lib_concepts
namespace ranges {
    template <class _Se, class _It>
    concept _Sized_or_unreachable_sentinel_for = sized_sentinel_for<_Se, _It> || same_as<_Se, unreachable_sentinel_t>;

    // clang-format off
    template <class _Rng>
    concept _Sized_or_infinite_range = range<_Rng>
                                    && (sized_range<_Rng> || same_as<sentinel_t<_Rng>, unreachable_sentinel_t>);

    // concept-constrained for strict enforcement as it is used by several algorithms
    template <input_iterator _It, sentinel_for<_It> _Se, class _Ty, class _Pj = identity>
        requires indirect_binary_predicate<ranges::equal_to, projected<_It, _Pj>, const _Ty*>
    _NODISCARD constexpr _It _Find_unchecked(_It _First, const _Se _Last, const _Ty& _Val, _Pj _Proj = {}) {
        // clang-format on
        constexpr bool _Is_sized = sized_sentinel_for<_Se, _It>;
        if constexpr (_Memchr_in_find_is_safe<_It,
                          _Ty> && _Sized_or_unreachable_sentinel_for<_Se, _It> && same_as<_Pj, identity>) {
            if (!_RPC is_constant_evaluated()) {
                if (!_Within_limits(_First, _Val)) {
                    if constexpr (_Is_sized) {
                        return _RANGES next(::std::move(_First), _Last);
                    } else {
                        _STL_ASSERT(false, "Tried to find a value in a range with unreachable sentinel"
                                           " that cannot be represented by the range's value type");
                    }
                }

                size_t _Count;
                if constexpr (_Is_sized) {
                    _Count = static_cast<size_t>(_Last - _First);
                } else {
                    _Count = SIZE_MAX;
                }
                const auto _First_ptr = _RPC to_address(_First);
                const auto _Result    = static_cast<remove_reference_t<_Iter_ref_t<_It>>*>(
                    _CSTD memchr(_First_ptr, static_cast<unsigned char>(_Val), _Count));
                if (_Result) {
                    if constexpr (::std::is_pointer_v<_It>) {
                        return _Result;
                    } else {
                        return _RANGES next(::std::move(_First), _Result - _First_ptr);
                    }
                } else {
                    return _RANGES next(::std::move(_First), _Last);
                }
            }
        }

        for (; _First != _Last; ++_First) {
            if (::std::invoke(_Proj, *_First) == _Val) {
                break;
            }
        }

        return _First;
    }

    class _Find_fn : private _Not_quite_object {
    public:
        using _Not_quite_object::_Not_quite_object;

        // clang-format off
        template <input_iterator _It, sentinel_for<_It> _Se, class _Ty, class _Pj = identity>
            requires indirect_binary_predicate<ranges::equal_to, projected<_It, _Pj>, const _Ty*>
        _NODISCARD constexpr _It operator()(_It _First, _Se _Last, const _Ty& _Val, _Pj _Proj = {}) const {
            _Adl_verify_range(_First, _Last);
            auto _UResult = _RANGES _Find_unchecked(
                _Get_unwrapped(::std::move(_First)), _Get_unwrapped(::std::move(_Last)), _Val, _Pass_fn(_Proj));

            _Seek_wrapped(_First, ::std::move(_UResult));
            return _First;
        }

        template <input_range _Rng, class _Ty, class _Pj = identity>
            requires indirect_binary_predicate<ranges::equal_to, projected<iterator_t<_Rng>, _Pj>, const _Ty*>
        _NODISCARD constexpr borrowed_iterator_t<_Rng> operator()(
            _Rng&& _Range, const _Ty& _Val, _Pj _Proj = {}) const {
            auto _First = _RANGES begin(_Range);
            auto _UResult =
                _RANGES _Find_unchecked(_Get_unwrapped(::std::move(_First)), _Uend(_Range), _Val, _Pass_fn(_Proj));

            _Seek_wrapped(_First, ::std::move(_UResult));
            return _First;
        }
        // clang-format on
    };

    inline constexpr _Find_fn find{_Not_quite_object::_Construct_tag{}};
} // namespace ranges
#endif // __cpp_lib_concepts

template <class _InIt, class _Ty>
_NODISCARD _CONSTEXPR20 _Iter_diff_t<_InIt> count(const _InIt _First, const _InIt _Last, const _Ty& _Val) {
    // count elements that match _Val
    _Adl_verify_range(_First, _Last);
    if constexpr (_Is_vb_iterator<_InIt> && ::std::is_same_v<_Ty, bool>) {
        return _Count_vbool(_First, _Last, _Val);
    } else {
        auto _UFirst               = _Get_unwrapped(_First);
        const auto _ULast          = _Get_unwrapped(_Last);
        _Iter_diff_t<_InIt> _Count = 0;

        for (; _UFirst != _ULast; ++_UFirst) {
            if (*_UFirst == _Val) {
                ++_Count;
            }
        }

        return _Count;
    }
}

#if _HAS_CXX17
template <class _ExPo, class _FwdIt, class _Ty, _Enable_if_execution_policy_t<_ExPo> = 0>
_NODISCARD _Iter_diff_t<_FwdIt> count(
    _ExPo&& _Exec, const _FwdIt _First, const _FwdIt _Last, const _Ty& _Val) noexcept; // terminates
#endif // _HAS_CXX17

template <class _InIt, class _Ty, class _Pr>
_NODISCARD constexpr _InIt _Find_pr(_InIt _First, const _InIt _Last, const _Ty& _Val, _Pr _Pred) {
    for (; _First != _Last; ++_First) {
        if (_Pred(*_First, _Val)) {
            break;
        }
    }

    return _First;
}

template <class _InIt, class _Ty, class _Pr>
_NODISCARD constexpr _Iter_diff_t<_InIt> _Count_pr(_InIt _First, const _InIt _Last, const _Ty& _Val, _Pr _Pred) {
    _Iter_diff_t<_InIt> _Count = 0;

    for (; _First != _Last; ++_First) {
        if (_Pred(*_First, _Val)) {
            ++_Count;
        }
    }

    return _Count;
}

enum class _TrimResult : unsigned char { _KeepTrimming, _HaveWorkAfterTrimming, _ReturnFalse, _ReturnTrue };

template <class _BidIt1, class _BidIt2, class _Pr>
_NODISCARD _CONSTEXPR20 _TrimResult _Trim_equal(
    _BidIt1& _First1, _BidIt1& _Back1, _BidIt2& _First2, _BidIt2& _Back2, _Pr _Pred) {
    // advances the iterators, trimming matching prefixes then matching suffixes
    // from [_First1, _Back1] and [_First2, _Back2]
    _STL_INTERNAL_CHECK(_First1 != _Back1);
    _STL_INTERNAL_CHECK(_RPC distance(_First1, _Back1) == _RPC distance(_First2, _Back2));
    if (_Pred(*_First1, *_First2)) {
        do {
            ++_First1;
            ++_First2;
            if (_First1 == _Back1) {
                // only one element is left
                return _Pred(*_First1, *_First2) ? _TrimResult::_ReturnTrue : _TrimResult::_ReturnFalse;
            }
        } while (_Pred(*_First1, *_First2));
    } else {
        if (!_Pred(*_Back1, *_Back2)) {
            // nothing to trim
            return _TrimResult::_HaveWorkAfterTrimming;
        }
        --_Back1;
        --_Back2;
    }

    for (;;) {
        if (_First1 == _Back1) {
            // only one element is left, it can't match because it wasn't trimmed by the first loop
            return _TrimResult::_ReturnFalse;
        }

        if (!_Pred(*_Back1, *_Back2)) {
            return _TrimResult::_KeepTrimming;
        }
        --_Back1;
        --_Back2;
    }
}

template <class _BidIt1, class _BidIt2, class _Pr>
_NODISCARD _CONSTEXPR20 _TrimResult _Trim_reversed(
    _BidIt1& _First1, _BidIt1& _Back1, _BidIt2& _First2, _BidIt2& _Back2, _Pr _Pred) {
    // advances the iterators, trimming each range's prefix that matches the other range's suffix
    // from [_First1, _Back1] and [_First2, _Back2]
    _STL_INTERNAL_CHECK(_First1 != _Back1);
    _STL_INTERNAL_CHECK(_RPC distance(_First1, _Back1) == _RPC distance(_First2, _Back2));
    if (_Pred(*_First1, *_Back2)) {
        do {
            ++_First1;
            --_Back2;
            if (_First1 == _Back1) {
                // only one element is left
                return _Pred(*_First1, *_First2) ? _TrimResult::_ReturnTrue : _TrimResult::_ReturnFalse;
            }
        } while (_Pred(*_First1, *_Back2));
    } else {
        if (!_Pred(*_Back1, *_First2)) {
            // nothing to trim
            return _TrimResult::_HaveWorkAfterTrimming;
        }
        --_Back1;
        ++_First2;
    }

    for (;;) {
        if (_First1 == _Back1) {
            // only one element is left, it can't match because it wasn't trimmed by the first loop
            return _TrimResult::_ReturnFalse;
        }

        if (!_Pred(*_Back1, *_First2)) {
            return _TrimResult::_KeepTrimming;
        }
        --_Back1;
        ++_First2;
    }
}

template <class _BidIt1, class _BidIt2, class _Pr>
_NODISCARD _CONSTEXPR20 _TrimResult _Trim_completely(
    _BidIt1& _First1, _BidIt1& _Back1, _BidIt2& _First2, _BidIt2& _Back2, _Pr _Pred) {
    // alternates between calling _Trim_reversed and _Trim_equal until no more trimming is possible
    _TrimResult _Res = _TrimResult::_KeepTrimming;

    for (bool _Check_reversed = true; _Res == _TrimResult::_KeepTrimming; _Check_reversed = !_Check_reversed) {
        if (_Check_reversed) {
            _Res = _Trim_reversed(_First1, _Back1, _First2, _Back2, _Pred);
        } else {
            _Res = _Trim_equal(_First1, _Back1, _First2, _Back2, _Pred);
        }
    }

    return _Res;
}

template <class _FwdIt1, class _FwdIt2, class _Pr>
_NODISCARD _CONSTEXPR20 bool _Check_match_counts(
    _FwdIt1 _First1, _FwdIt1 _Last1, _FwdIt2 _First2, _FwdIt2 _Last2, _Pr _Pred) {
    // test if [_First1, _Last1) == permuted [_First2, _Last2), after matching prefix removal
    _STL_INTERNAL_CHECK(!_Pred(*_First1, *_First2));
    _STL_INTERNAL_CHECK(_RPC distance(_First1, _Last1) == _RPC distance(_First2, _Last2));
    if constexpr (_Is_bidi_iter_v<_FwdIt1> && _Is_bidi_iter_v<_FwdIt2>) {
        do { // find last inequality
            --_Last1;
            --_Last2;
        } while (_Pred(*_Last1, *_Last2));

        if (_First1 == _Last1) {
            return false;
        }

        const _TrimResult _Res = _Trim_completely(_First1, _Last1, _First2, _Last2, _Pred);

        if (_Res != _TrimResult::_HaveWorkAfterTrimming) {
            return _Res == _TrimResult::_ReturnTrue;
        }

        ++_Last1;
        ++_Last2;
    }

    for (_FwdIt1 _Next1 = _First1; _Next1 != _Last1; ++_Next1) {
        if (_Next1 == _Find_pr(_First1, _Next1, *_Next1, _Pred)) { // new value, compare match counts
            _Iter_diff_t<_FwdIt2> _Count2 = _Count_pr(_First2, _Last2, *_Next1, _Pred);
            if (_Count2 == 0) {
                return false; // second range lacks value, not a permutation
            }

            _FwdIt1 _Skip1                = _Next_iter(_Next1);
            _Iter_diff_t<_FwdIt1> _Count1 = _Count_pr(_Skip1, _Last1, *_Next1, _Pred) + 1;
            if (_Count2 != _Count1) {
                return false; // match counts differ, not a permutation
            }
        }
    }

    return true;
}

template <class _BidIt>
_CONSTEXPR20 void reverse(const _BidIt _First, const _BidIt _Last) { // reverse elements in [_First, _Last)
    _Adl_verify_range(_First, _Last);
    auto _UFirst = _Get_unwrapped(_First);
    auto _ULast  = _Get_unwrapped(_Last);
#if _USE_STD_VECTOR_ALGORITHMS
    using _Elem                         = remove_reference_t<_Iter_ref_t<decltype(_UFirst)>>;
    constexpr bool _Allow_vectorization = ::std::conjunction_v<::std::bool_constant<_Iterator_is_contiguous<decltype(_UFirst)>>,
        _Is_trivially_swappable<_Elem>, ::std::negation<::std::is_volatile<_Elem>>>;
    constexpr size_t _Nx                = sizeof(_Elem);

#if defined(__clang__) || defined(__GNUC__) || defined(__GNUG__)
#else
#pragma warning(suppress : 6326) // Potential comparison of a constant with another constant
#endif
    if constexpr (_Allow_vectorization && _Nx <= 8 && (_Nx & (_Nx - 1)) == 0) {
#if _HAS_CXX20
        if (!_RPC is_constant_evaluated())
#endif // _HAS_CXX20
        {
            if constexpr (_Nx == 1) {
                __std_reverse_trivially_swappable_1(_To_address(_UFirst), _To_address(_ULast));
            } else if constexpr (_Nx == 2) {
                __std_reverse_trivially_swappable_2(_To_address(_UFirst), _To_address(_ULast));
            } else if constexpr (_Nx == 4) {
                __std_reverse_trivially_swappable_4(_To_address(_UFirst), _To_address(_ULast));
            } else {
                __std_reverse_trivially_swappable_8(_To_address(_UFirst), _To_address(_ULast));
            }

            return;
        }
    }
#endif // _USE_STD_VECTOR_ALGORITHMS

    for (; _UFirst != _ULast && _UFirst != --_ULast; ++_UFirst) {
        ::std::iter_swap(_UFirst, _ULast);
    }
}

#if _HAS_CXX17
template <class _ExPo, class _BidIt, _Enable_if_execution_policy_t<_ExPo> = 0>
void reverse(_ExPo&&, _BidIt _First, _BidIt _Last) noexcept /* terminates */ {
    // reverse elements in [_First, _Last)
    // not parallelized as benchmarks show it isn't worth it
    return _RPC reverse(_First, _Last);
}
#endif // _HAS_CXX17

template <class _BidIt>
constexpr ::std::pair<_BidIt, _BidIt> _Reverse_until_sentinel_unchecked(_BidIt _First, _BidIt _Sentinel, _BidIt _Last) {
    // reverse until either _First or _Last hits _Sentinel
    while (_First != _Sentinel && _Last != _Sentinel) {
        ::std::iter_swap(_First, --_Last);
        ++_First;
    }

    return ::std::pair<_BidIt, _BidIt>(_First, _Last);
}

template <class _FwdIt>
_CONSTEXPR20 _FwdIt rotate(_FwdIt _First, _FwdIt _Mid, _FwdIt _Last) {
    // exchange the ranges [_First, _Mid) and [_Mid, _Last)
    // that is, rotates [_First, _Last) left by distance(_First, _Mid) positions
    // returns the iterator pointing at *_First's new home
    _Adl_verify_range(_First, _Mid);
    _Adl_verify_range(_Mid, _Last);
    auto _UFirst      = _Get_unwrapped(_First);
    auto _UMid        = _Get_unwrapped(_Mid);
    const auto _ULast = _Get_unwrapped(_Last);
    if (_UFirst == _UMid) {
        return _Last;
    }

    if (_UMid == _ULast) {
        return _First;
    }

    if constexpr (_Is_random_iter_v<_FwdIt>) {
        _RPC reverse(_UFirst, _UMid);
        _RPC reverse(_UMid, _ULast);
        _RPC reverse(_UFirst, _ULast);
        _Seek_wrapped(_First, _UFirst + (_ULast - _UMid));
    } else if constexpr (_Is_bidi_iter_v<_FwdIt>) {
        _RPC reverse(_UFirst, _UMid);
        _RPC reverse(_UMid, _ULast);
        auto _Tmp = _Reverse_until_sentinel_unchecked(_UFirst, _UMid, _ULast);
        _RPC reverse(_Tmp.first, _Tmp.second);
        _Seek_wrapped(_First, _UMid != _Tmp.first ? _Tmp.first : _Tmp.second);
    } else {
        auto _UNext = _UMid;
        do { // rotate the first cycle
            ::std::iter_swap(_UFirst, _UNext);
            ++_UFirst;
            ++_UNext;
            if (_UFirst == _UMid) {
                _UMid = _UNext;
            }
        } while (_UNext != _ULast);
        _Seek_wrapped(_First, _UFirst);
        while (_UMid != _ULast) { // rotate subsequent cycles
            _UNext = _UMid;
            do {
                ::std::iter_swap(_UFirst, _UNext);
                ++_UFirst;
                ++_UNext;
                if (_UFirst == _UMid) {
                    _UMid = _UNext;
                }
            } while (_UNext != _ULast);
        }
    }

    return _First;
}

#if _HAS_CXX17
template <class _ExPo, class _FwdIt, _Enable_if_execution_policy_t<_ExPo> = 0>
_FwdIt rotate(_ExPo&&, _FwdIt _First, _FwdIt _Mid, _FwdIt _Last) noexcept /* terminates */ {
    // rotate [_First, _Last) left by distance(_First, _Mid) positions
    // not parallelized as benchmarks show it isn't worth it
    return _RPC rotate(_First, _Mid, _Last);
}
#endif // _HAS_CXX17

template <class _InIt, class _Pr>
_NODISCARD _CONSTEXPR20 _InIt find_if(_InIt _First, const _InIt _Last, _Pr _Pred) { // find first satisfying _Pred
    _Adl_verify_range(_First, _Last);
    auto _UFirst      = _Get_unwrapped(_First);
    const auto _ULast = _Get_unwrapped(_Last);
    for (; _UFirst != _ULast; ++_UFirst) {
        if (_Pred(*_UFirst)) {
            break;
        }
    }

    _Seek_wrapped(_First, _UFirst);
    return _First;
}

#ifdef __cpp_lib_concepts
namespace ranges {
    template <class _Result, class _Wrapped, class _Unwrapped>
    _NODISCARD constexpr _Result _Rewrap_subrange(_Wrapped& _Val, subrange<_Unwrapped>&& _UResult) {
        // conditionally computes a wrapped subrange from a wrapped iterator or range and unwrapped subrange
        if constexpr (::std::is_same_v<_Result, dangling>) {
            return dangling{};
        } else if constexpr (::std::is_same_v<_Result, subrange<_Unwrapped>>) {
            return ::std::move(_UResult);
        } else if constexpr (range<_Wrapped>) {
            _STL_INTERNAL_STATIC_ASSERT(forward_range<_Wrapped>);
            _STL_INTERNAL_STATIC_ASSERT(::std::is_same_v<_Unwrapped, _Unwrapped_t<iterator_t<_Wrapped>>>);
            _STL_INTERNAL_STATIC_ASSERT(::std::is_same_v<_Result, subrange<iterator_t<_Wrapped>>>);

            auto _First = _RANGES begin(_Val);
            auto _Last  = _First;
            _First._Seek_to(_UResult.begin());
            _Last._Seek_to(_UResult.end());
            return _Result{::std::move(_First), ::std::move(_Last)};
        } else {
            _STL_INTERNAL_STATIC_ASSERT(::std::is_same_v<_Unwrapped, _Unwrapped_t<_Wrapped>>);
            _STL_INTERNAL_STATIC_ASSERT(::std::is_same_v<_Result, subrange<_Wrapped>>);

            auto _Last = _Val;
            _Val._Seek_to(_UResult.begin());
            _Last._Seek_to(_UResult.end());
            return _Result{::std::move(_Val), ::std::move(_Last)};
        }
    }

    // concept-constrained for strict enforcement as it is used by several algorithms
    template <input_iterator _It, sentinel_for<_It> _Se, class _Pj, indirect_unary_predicate<projected<_It, _Pj>> _Pr>
    _NODISCARD constexpr _It _Find_if_unchecked(_It _First, const _Se _Last, _Pr _Pred, _Pj _Proj) {
        for (; _First != _Last; ++_First) {
            if (::std::invoke(_Pred, ::std::invoke(_Proj, *_First))) {
                break;
            }
        }

        return _First;
    }

    class _Find_if_fn : private _Not_quite_object {
    public:
        using _Not_quite_object::_Not_quite_object;

        template <input_iterator _It, sentinel_for<_It> _Se, class _Pj = identity,
            indirect_unary_predicate<projected<_It, _Pj>> _Pr>
        _NODISCARD constexpr _It operator()(_It _First, _Se _Last, _Pr _Pred, _Pj _Proj = {}) const {
            _Adl_verify_range(_First, _Last);
            auto _UResult = _RANGES _Find_if_unchecked(
                _Get_unwrapped(::std::move(_First)), _Get_unwrapped(::std::move(_Last)), _Pass_fn(_Pred), _Pass_fn(_Proj));

            _Seek_wrapped(_First, ::std::move(_UResult));
            return _First;
        }

        template <input_range _Rng, class _Pj = identity,
            indirect_unary_predicate<projected<iterator_t<_Rng>, _Pj>> _Pr>
        _NODISCARD constexpr borrowed_iterator_t<_Rng> operator()(_Rng&& _Range, _Pr _Pred, _Pj _Proj = {}) const {
            auto _First   = _RANGES begin(_Range);
            auto _UResult = _RANGES _Find_if_unchecked(
                _Get_unwrapped(::std::move(_First)), _Uend(_Range), _Pass_fn(_Pred), _Pass_fn(_Proj));

            _Seek_wrapped(_First, ::std::move(_UResult));
            return _First;
        }
    };

    inline constexpr _Find_if_fn find_if{_Not_quite_object::_Construct_tag{}};

    class _Find_if_not_fn : private _Not_quite_object {
    public:
        using _Not_quite_object::_Not_quite_object;

        template <input_iterator _It, sentinel_for<_It> _Se, class _Pj = identity,
            indirect_unary_predicate<projected<_It, _Pj>> _Pr>
        _NODISCARD constexpr _It operator()(_It _First, _Se _Last, _Pr _Pred, _Pj _Proj = {}) const {
            _Adl_verify_range(_First, _Last);

            auto _UResult = _Find_if_not_unchecked(
                _Get_unwrapped(::std::move(_First)), _Get_unwrapped(::std::move(_Last)), _Pass_fn(_Pred), _Pass_fn(_Proj));

            _Seek_wrapped(_First, ::std::move(_UResult));
            return _First;
        }

        template <input_range _Rng, class _Pj = identity,
            indirect_unary_predicate<projected<iterator_t<_Rng>, _Pj>> _Pr>
        _NODISCARD constexpr borrowed_iterator_t<_Rng> operator()(_Rng&& _Range, _Pr _Pred, _Pj _Proj = {}) const {
            auto _First = _RANGES begin(_Range);

            auto _UResult = _Find_if_not_unchecked(
                _Get_unwrapped(::std::move(_First)), _Uend(_Range), _Pass_fn(_Pred), _Pass_fn(_Proj));

            _Seek_wrapped(_First, ::std::move(_UResult));
            return _First;
        }

    private:
        template <class _It, class _Se, class _Pj, class _Pr>
        _NODISCARD static constexpr _It _Find_if_not_unchecked(_It _First, const _Se _Last, _Pr _Pred, _Pj _Proj) {
            _STL_INTERNAL_STATIC_ASSERT(input_iterator<_It>);
            _STL_INTERNAL_STATIC_ASSERT(sentinel_for<_Se, _It>);
            _STL_INTERNAL_STATIC_ASSERT(indirect_unary_predicate<_Pr, projected<_It, _Pj>>);

            for (; _First != _Last; ++_First) {
                if (!::std::invoke(_Pred, ::std::invoke(_Proj, *_First))) {
                    break;
                }
            }

            return _First;
        }
    };

    inline constexpr _Find_if_not_fn find_if_not{_Not_quite_object::_Construct_tag{}};

    // clang-format off
    template <class _It1, class _It2, class _Se2, class _Pr, class _Pj1, class _Pj2>
    concept _Equal_rev_pred_can_memcmp = ::std::is_same_v<_Pj1, identity> && ::std::is_same_v<_Pj2, identity>
        && sized_sentinel_for<_Se2, _It2> && _Equal_memcmp_is_safe<_It1, _It2, _Pr>;

    template <input_iterator _It1, input_iterator _It2, sentinel_for<_It2> _Se2, class _Pr, class _Pj1, class _Pj2>
        requires indirectly_comparable<_It1, _It2, _Pr, _Pj1, _Pj2>
    _NODISCARD constexpr ::std::pair<bool, _It1> _Equal_rev_pred(
        _It1 _First1, _It2 _First2, const _Se2 _Last2, _Pr _Pred, _Pj1 _Proj1, _Pj2 _Proj2) {
        // clang-format on
        // Returns {true, _First1 + (_Last2 - _First2)} if [_First1, ...) equals [_First2, _Last2), and {false, {}}
        // otherwise.
        constexpr bool _Optimize = _Equal_rev_pred_can_memcmp<_It1, _It2, _Se2, _Pr, _Pj1, _Pj2>;
        if constexpr (_Optimize) {
            if (!_RPC is_constant_evaluated()) {
                bool _Ans;
                if constexpr (same_as<_It2, _Se2>) {
                    _Ans = _Memcmp_ranges(_First2, _Last2, _First1) == 0;
                } else {
                    _Ans = _Memcmp_count(_First1, _First2, static_cast<size_t>(_Last2 - _First2)) == 0;
                }

                if (_Ans) {
                    _First1 += (_Last2 - _First2);
                    return {true, ::std::move(_First1)};
                } else {
                    return {false, _It1{}};
                }
            }
        }

        for (; _First2 != _Last2; ++_First1, (void) ++_First2) {
            if (!::std::invoke(_Pred, ::std::invoke(_Proj1, *_First1), ::std::invoke(_Proj2, *_First2))) {
                return {false, _It1{}};
            }
        }

        return {true, ::std::move(_First1)};
    }

    class _Search_fn : private _Not_quite_object {
    public:
        using _Not_quite_object::_Not_quite_object;

        // clang-format off
        template <forward_iterator _It1, sentinel_for<_It1> _Se1, forward_iterator _It2, sentinel_for<_It2> _Se2,
            class _Pr = ranges::equal_to, class _Pj1 = identity, class _Pj2 = identity>
            requires indirectly_comparable<_It1, _It2, _Pr, _Pj1, _Pj2>
        _NODISCARD constexpr subrange<_It1> operator()(_It1 _First1, _Se1 _Last1, _It2 _First2, _Se2 _Last2,
            _Pr _Pred = {}, _Pj1 _Proj1 = {}, _Pj2 _Proj2 = {}) const {
            // clang-format on
            _Adl_verify_range(_First1, _Last1);
            _Adl_verify_range(_First2, _Last2);
            auto _UFirst1 = _Get_unwrapped(::std::move(_First1));
            auto _ULast1  = _Get_unwrapped(::std::move(_Last1));
            auto _UFirst2 = _Get_unwrapped(::std::move(_First2));
            auto _ULast2  = _Get_unwrapped(::std::move(_Last2));

            if constexpr (sized_sentinel_for<_Se1, _It1> && sized_sentinel_for<_Se2, _It2>) {
                const auto _Count1 = _ULast1 - _UFirst1;
                const auto _Count2 = _ULast2 - _UFirst2;
                auto _UResult = _Search_sized(::std::move(_UFirst1), ::std::move(_ULast1), _Count1, ::std::move(_UFirst2),
                    ::std::move(_ULast2), _Count2, _Pass_fn(_Pred), _Pass_fn(_Proj1), _Pass_fn(_Proj2));
                return _Rewrap_subrange<subrange<_It1>>(_First1, ::std::move(_UResult));
            } else {
                auto _UResult = _Search_unsized(::std::move(_UFirst1), ::std::move(_ULast1), ::std::move(_UFirst2),
                    ::std::move(_ULast2), _Pass_fn(_Pred), _Pass_fn(_Proj1), _Pass_fn(_Proj2));
                return _Rewrap_subrange<subrange<_It1>>(_First1, ::std::move(_UResult));
            }
        }

        // clang-format off
        template <forward_range _Rng1, forward_range _Rng2, class _Pr = ranges::equal_to, class _Pj1 = identity,
            class _Pj2 = identity>
            requires indirectly_comparable<iterator_t<_Rng1>, iterator_t<_Rng2>, _Pr, _Pj1, _Pj2>
        _NODISCARD constexpr borrowed_subrange_t<_Rng1> operator()(
            _Rng1&& _Range1, _Rng2&& _Range2, _Pr _Pred = {}, _Pj1 _Proj1 = {}, _Pj2 _Proj2 = {}) const {
            // clang-format on
            if constexpr (sized_range<_Rng1> && sized_range<_Rng2>) {
                const auto _Count1 = _RANGES distance(_Range1);
                const auto _Count2 = _RANGES distance(_Range2);
                auto _UResult      = _Search_sized(_Ubegin(_Range1), _Uend(_Range1), _Count1, _Ubegin(_Range2),
                    _Uend(_Range2), _Count2, _Pass_fn(_Pred), _Pass_fn(_Proj1), _Pass_fn(_Proj2));
                return _Rewrap_subrange<borrowed_subrange_t<_Rng1>>(_Range1, ::std::move(_UResult));
            } else {
                auto _UResult = _Search_unsized(_Ubegin(_Range1), _Uend(_Range1), _Ubegin(_Range2), _Uend(_Range2),
                    _Pass_fn(_Pred), _Pass_fn(_Proj1), _Pass_fn(_Proj2));
                return _Rewrap_subrange<borrowed_subrange_t<_Rng1>>(_Range1, ::std::move(_UResult));
            }
        }

    private:
        template <class _It1, class _Se1, class _It2, class _Se2, class _Pr, class _Pj1, class _Pj2>
        _NODISCARD static constexpr subrange<_It1> _Search_sized(_It1 _First1, const _Se1 _Last1,
            iter_difference_t<_It1> _Count1, _It2 _First2, const _Se2 _Last2, const iter_difference_t<_It2> _Count2,
            _Pr _Pred, _Pj1 _Proj1, _Pj2 _Proj2) {
            _STL_INTERNAL_STATIC_ASSERT(forward_iterator<_It1>);
            _STL_INTERNAL_STATIC_ASSERT(sentinel_for<_Se1, _It1>);
            _STL_INTERNAL_STATIC_ASSERT(forward_iterator<_It2>);
            _STL_INTERNAL_STATIC_ASSERT(sentinel_for<_Se2, _It2>);
            _STL_INTERNAL_STATIC_ASSERT(indirectly_comparable<_It1, _It2, _Pr, _Pj1, _Pj2>);
            _STL_INTERNAL_CHECK(_RANGES distance(_First1, _Last1) == _Count1);
            _STL_INTERNAL_CHECK(_RANGES distance(_First2, _Last2) == _Count2);

            for (; _Count1 >= _Count2; ++_First1, (void) --_Count1) {
                auto [_Match, _Mid1] = _RANGES _Equal_rev_pred(_First1, _First2, _Last2, _Pred, _Proj1, _Proj2);
                if (_Match) {
                    return {::std::move(_First1), ::std::move(_Mid1)};
                }
            }

            _First1 = _Find_last_iterator(_First1, _Last1, _Count1);
            return {_First1, _First1};
        }

        template <class _It1, class _Se1, class _It2, class _Se2, class _Pr, class _Pj1, class _Pj2>
        _NODISCARD static constexpr subrange<_It1> _Search_unsized(
            _It1 _First1, const _Se1 _Last1, _It2 _First2, const _Se2 _Last2, _Pr _Pred, _Pj1 _Proj1, _Pj2 _Proj2) {
            _STL_INTERNAL_STATIC_ASSERT(forward_iterator<_It1>);
            _STL_INTERNAL_STATIC_ASSERT(sentinel_for<_Se1, _It1>);
            _STL_INTERNAL_STATIC_ASSERT(forward_iterator<_It2>);
            _STL_INTERNAL_STATIC_ASSERT(sentinel_for<_Se2, _It2>);
            _STL_INTERNAL_STATIC_ASSERT(indirectly_comparable<_It1, _It2, _Pr, _Pj1, _Pj2>);

            for (;; ++_First1) {
                auto _Mid1 = _First1;
                for (auto _Mid2 = _First2;; ++_Mid1, (void) ++_Mid2) {
                    if (_Mid2 == _Last2) { // match
                        return {::std::move(_First1), ::std::move(_Mid1)};
                    }

                    if (_Mid1 == _Last1) { // not enough haystack left to find a match
                        return {_Mid1, _Mid1};
                    }

                    if (!::std::invoke(_Pred, ::std::invoke(_Proj1, *_Mid1), ::std::invoke(_Proj2, *_Mid2))) { // mismatch
                        break;
                    }
                }
            }
        }
    };

    inline constexpr _Search_fn search{_Not_quite_object::_Construct_tag{}};
} // namespace ranges

template <auto> // _Require_constant<E> is a valid template-id iff E is a constant expression of structural type
struct _Require_constant;
#endif // __cpp_lib_concepts

template <class _FwdIt, class _Ty, class _Pr>
_NODISCARD _CONSTEXPR20 _FwdIt lower_bound(_FwdIt _First, const _FwdIt _Last, const _Ty& _Val, _Pr _Pred) {
    // find first element not before _Val
    _Adl_verify_range(_First, _Last);
    auto _UFirst                = _Get_unwrapped(_First);
    _Iter_diff_t<_FwdIt> _Count = _RPC distance(_UFirst, _Get_unwrapped(_Last));

    while (0 < _Count) { // divide and conquer, find half that contains answer
        const _Iter_diff_t<_FwdIt> _Count2 = _Count / 2;
        const auto _UMid                   = _RPC next(_UFirst, _Count2);
        if (_Pred(*_UMid, _Val)) { // try top half
            _UFirst = _Next_iter(_UMid);
            _Count -= _Count2 + 1;
        } else {
            _Count = _Count2;
        }
    }

    _Seek_wrapped(_First, _UFirst);
    return _First;
}

template <class _FwdIt, class _Ty>
_NODISCARD _CONSTEXPR20 _FwdIt lower_bound(_FwdIt _First, _FwdIt _Last, const _Ty& _Val) {
    // find first element not before _Val
    return _RPC lower_bound(_First, _Last, _Val, ::std::less<>{});
}

template <class _FwdIt1, class _FwdIt2>
_CONSTEXPR20 _FwdIt2 _Swap_ranges_unchecked(_FwdIt1 _First1, const _FwdIt1 _Last1, _FwdIt2 _First2) {
    // swap [_First1, _Last1) with [_First2, ...)

#if _USE_STD_VECTOR_ALGORITHMS
    using _Elem1 = remove_reference_t<_Iter_ref_t<_FwdIt1>>;
    using _Elem2 = remove_reference_t<_Iter_ref_t<_FwdIt2>>;
    if constexpr (::std::is_same_v<_Elem1, _Elem2> && _Is_trivially_swappable_v<_Elem1> //
                  && _Iterators_are_contiguous<_FwdIt1, _FwdIt2>) {
#if _HAS_CXX20
        if (!_RPC is_constant_evaluated())
#endif // _HAS_CXX20
        {
            __std_swap_ranges_trivially_swappable_noalias(
                _To_address(_First1), _To_address(_Last1), _To_address(_First2));
            return _First2 + (_Last1 - _First1);
        }
    }
#endif // _USE_STD_VECTOR_ALGORITHMS

    for (; _First1 != _Last1; ++_First1, (void) ++_First2) {
        ::std::iter_swap(_First1, _First2);
    }

    return _First2;
}

template <class _Diff, class _Urng>
class _Rng_from_urng { // wrap a URNG as an RNG
public:
    using _Ty0 = ::std::make_unsigned_t<_Diff>;
    using _Ty1 = typename _Urng::result_type;

    using _Udiff = ::std::conditional_t<sizeof(_Ty1) < sizeof(_Ty0), _Ty0, _Ty1>;

    explicit _Rng_from_urng(_Urng& _Func) : _Ref(_Func), _Bits(CHAR_BIT * sizeof(_Udiff)), _Bmask(_Udiff(-1)) {
        for (; (_Urng::max) () - (_Urng::min) () < _Bmask; _Bmask >>= 1) {
            --_Bits;
        }
    }

    _Diff operator()(_Diff _Index) { // adapt _Urng closed range to [0, _Index)
        for (;;) { // try a sample random value
            _Udiff _Ret  = 0; // random bits
            _Udiff _Mask = 0; // 2^N - 1, _Ret is within [0, _Mask]

            while (_Mask < _Udiff(_Index - 1)) { // need more random bits
                _Ret <<= _Bits - 1; // avoid full shift
                _Ret <<= 1;
                _Ret |= _Get_bits();
                _Mask <<= _Bits - 1; // avoid full shift
                _Mask <<= 1;
                _Mask |= _Bmask;
            }

            // _Ret is [0, _Mask], _Index - 1 <= _Mask, return if unbiased
            if (_Ret / _Index < _Mask / _Index || _Mask % _Index == _Udiff(_Index - 1)) {
                return static_cast<_Diff>(_Ret % _Index);
            }
        }
    }

    _Udiff _Get_all_bits() {
        _Udiff _Ret = 0;

        for (size_t _Num = 0; _Num < CHAR_BIT * sizeof(_Udiff); _Num += _Bits) { // don't mask away any bits
            _Ret <<= _Bits - 1; // avoid full shift
            _Ret <<= 1;
            _Ret |= _Get_bits();
        }

        return _Ret;
    }

    _Rng_from_urng(const _Rng_from_urng&) = delete;
    _Rng_from_urng& operator=(const _Rng_from_urng&) = delete;

private:
    _Udiff _Get_bits() { // return a random value within [0, _Bmask]
        for (;;) { // repeat until random value is in range
            _Udiff _Val = _Ref() - (_Urng::min) ();

            if (_Val <= _Bmask) {
                return _Val;
            }
        }
    }

    _Urng& _Ref; // reference to URNG
    size_t _Bits; // number of random bits generated by _Get_bits()
    _Udiff _Bmask; // 2^_Bits - 1
};

template <class _Ty, class _Alloc, class = void>
struct _Has_allocator_type : false_type {}; // tests for suitable _Ty::allocator_type

template <class _Ty, class _Alloc>
struct _Has_allocator_type<_Ty, _Alloc, ::std::void_t<typename _Ty::allocator_type>>
    : ::std::is_convertible<_Alloc, typename _Ty::allocator_type>::type {}; // tests for suitable _Ty::allocator_type

struct allocator_arg_t { // tag type for added allocator argument
    explicit allocator_arg_t() = default;
};

_INLINE_VAR constexpr allocator_arg_t allocator_arg{};

// [[noreturn]] _CRTIMP2_PURE void __CLRCALL_PURE_OR_CDECL _Xbad_alloc();
// [[noreturn]] _CRTIMP2_PURE void __CLRCALL_PURE_OR_CDECL _Xinvalid_argument(_In_z_ const char*);
// [[noreturn]] _CRTIMP2_PURE void __CLRCALL_PURE_OR_CDECL _Xlength_error(_In_z_ const char*);
// [[noreturn]] _CRTIMP2_PURE void __CLRCALL_PURE_OR_CDECL _Xout_of_range(_In_z_ const char*);
// [[noreturn]] _CRTIMP2_PURE void __CLRCALL_PURE_OR_CDECL _Xoverflow_error(_In_z_ const char*);
// [[noreturn]] _CRTIMP2_PURE void __CLRCALL_PURE_OR_CDECL _Xruntime_error(_In_z_ const char*);
// [[noreturn]] _CRTIMP2_PURE void __CLRCALL_PURE_OR_CDECL _XGetLastError();

template <class _Ty, class _Alloc>
struct uses_allocator : _Has_allocator_type<_Ty, _Alloc>::type {
    // determine whether _Ty has an allocator_type member type
};

template <class _Ty, class _Alloc>
_INLINE_VAR constexpr bool uses_allocator_v = uses_allocator<_Ty, _Alloc>::value;

template <class _Category, class _Ty, class _Diff = ptrdiff_t, class _Pointer = _Ty*, class _Reference = _Ty&>
struct _CXX17_DEPRECATE_ITERATOR_BASE_CLASS iterator { // base type for iterator classes
    using iterator_category = _Category;
    using value_type        = _Ty;
    using difference_type   = _Diff;
    using pointer           = _Pointer;
    using reference         = _Reference;
};

// template <class _Ty, ::std::enable_if_t<is_floating_point_v<_Ty>, int> = 0>
// _NODISCARD _CONSTEXPR_BIT_CAST auto _Float_abs_bits(const _Ty& _Xx) {
//     using _Traits    = _Floating_type_traits<_Ty>;
//     using _Uint_type = typename _Traits::_Uint_type;
//     const auto _Bits = _Bit_cast<_Uint_type>(_Xx);
//     return _Bits & ~_Traits::_Shifted_sign_mask;
// }

// template <class _Ty, ::std::enable_if_t<is_floating_point_v<_Ty>, int> = 0>
// _NODISCARD _CONSTEXPR_BIT_CAST _Ty _Float_abs(const _Ty _Xx) { // constexpr floating-point abs()
//     return _Bit_cast<_Ty>(_Float_abs_bits(_Xx));
// }

// template <class _Ty, ::std::enable_if_t<is_floating_point_v<_Ty>, int> = 0>
// _NODISCARD _CONSTEXPR_BIT_CAST _Ty _Float_copysign(const _Ty _Magnitude, const _Ty _Sign) { // constexpr copysign()
//     using _Traits       = _Floating_type_traits<_Ty>;
//     using _Uint_type    = typename _Traits::_Uint_type;
//     const auto _Signbit = _Bit_cast<_Uint_type>(_Sign) & _Traits::_Shifted_sign_mask;
//     return _Bit_cast<_Ty>(_Float_abs_bits(_Magnitude) | _Signbit);
// }

// template <class _Ty, ::std::enable_if_t<is_floating_point_v<_Ty>, int> = 0>
// _NODISCARD _CONSTEXPR_BIT_CAST bool _Is_nan(const _Ty _Xx) { // constexpr isnan()
//     using _Traits = _Floating_type_traits<_Ty>;
//     return _Float_abs_bits(_Xx) > _Traits::_Shifted_exponent_mask;
// }

// // TRANSITION, workaround x86 ABI
// // On x86 ABI, floating-point by-value arguments and return values are passed in 80-bit x87 registers.
// // When the value is a 32-bit or 64-bit signaling NaN, the conversion to/from 80-bit raises FE_INVALID
// // and turns it into a quiet NaN. This behavior is undesirable if we want to test for signaling NaNs.
// template <class _Ty, ::std::enable_if_t<is_floating_point_v<_Ty>, int> = 0>
// _NODISCARD _CONSTEXPR_BIT_CAST bool _Is_signaling_nan(const _Ty& _Xx) { // returns true if input is a signaling NaN
//     using _Traits        = _Floating_type_traits<_Ty>;
//     const auto _Abs_bits = _Float_abs_bits(_Xx);
//     return _Abs_bits > _Traits::_Shifted_exponent_mask && ((_Abs_bits & _Traits::_Special_nan_mantissa_mask) == 0);
// }

// template <class _Ty, ::std::enable_if_t<is_floating_point_v<_Ty>, int> = 0>
// _NODISCARD _CONSTEXPR_BIT_CAST bool _Is_inf(const _Ty _Xx) { // constexpr isinf()
//     using _Traits = _Floating_type_traits<_Ty>;
//     return _Float_abs_bits(_Xx) == _Traits::_Shifted_exponent_mask;
// }

// template <class _Ty, ::std::enable_if_t<is_floating_point_v<_Ty>, int> = 0>
// _NODISCARD _CONSTEXPR_BIT_CAST bool _Is_finite(const _Ty _Xx) { // constexpr isfinite()
//     using _Traits = _Floating_type_traits<_Ty>;
//     return _Float_abs_bits(_Xx) < _Traits::_Shifted_exponent_mask;
// }

#if _HAS_CXX17
struct monostate {};
#endif // _HAS_CXX17

_RPC_END
#if defined(__clang__) || defined(__GNUC__) || defined(__GNUG__)
#else
#pragma pop_macro("new")
_STL_RESTORE_CLANG_WARNINGS
#pragma warning(pop)
#pragma pack(pop)
#endif
#endif // _STL_COMPILER_PREPROCESSOR
#endif // _XUTILITY_
