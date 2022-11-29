// -*- C++ -*-
//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#pragma once

#include <cstddef>
#include <cstdlib> // abort
#include <iosfwd>
#include <stdexcept>
#include <type_traits>
#include <typeinfo>
#include <utility>
#include <atomic>
#include <utility>
#include <tuple>
#include <functional>
#include <memory>
#include <string>
#include <typeinfo>

#ifdef DUMP_REF_COUNT
#include <sgx_error.h>
extern "C" {
#ifdef _IN_ENCLAVE
sgx_status_t __cdecl log_str(const char* str, size_t sz);
#else
void __cdecl log_str(const char* str, size_t sz);
#endif
}

#define LOG(str, sz) log_str(str, sz)
#else
#define LOG(str, sz)
#endif

namespace rpc
{
    struct __shared_ptr_dummy_rebind_allocator_type;
}

namespace std
{
    template<> class allocator<rpc::__shared_ptr_dummy_rebind_allocator_type>
    {
    public:
        template<class _Other> struct rebind
        {
            typedef allocator<_Other> other;
        };
    };

#if _LIBCPP_STD_VER <= 11
    template<class T> using remove_extent_t = typename remove_extent<T>::type;
#endif
}

namespace rpc
{

    template<class _Tp, bool> struct __dependent_type : public _Tp
    {
    };

#if defined(_LIBCPP_HAS_IS_FINAL)
    template<class _Tp> struct __libcpp_is_final : public std::integral_constant<bool, __is_final(_Tp)>
    {
    };
#else
    template<class _Tp> struct __libcpp_is_final : public std::false_type
    {
    };
#endif

    template<class _Alloc> struct __allocation_guard
    {
        using _Pointer = typename std::allocator_traits<_Alloc>::pointer;
        using _Size = typename std::allocator_traits<_Alloc>::size_type;

        template<class _AllocT> // we perform the allocator conversion inside the constructor
        explicit __allocation_guard(_AllocT __alloc, _Size __n)
            : __alloc_(std::move(__alloc))
            , __n_(__n)
            , __ptr_(std::allocator_traits<_Alloc>::allocate(__alloc_, __n_)) // initialization order is important
        {
        }

        ~__allocation_guard() noexcept
        {
            if (__ptr_ != nullptr)
            {
                std::allocator_traits<_Alloc>::deallocate(__alloc_, __ptr_, __n_);
            }
        }

        _Pointer __release_ptr() noexcept
        { // not called __release() because it's a keyword in objective-c++
            _Pointer __tmp = __ptr_;
            __ptr_ = nullptr;
            return __tmp;
        }

        _Pointer __get() const noexcept { return __ptr_; }

    private:
        _Alloc __alloc_;
        _Size __n_;
        _Pointer __ptr_;
    };

    template<class _Tp, class _Up> struct __has_rebind
    {
    private:
        struct __two
        {
            char __lx;
            char __lxx;
        };
        template<class _Xp> static __two __test(...);
        template<class _Xp> static char __test(typename _Xp::template rebind<_Up>* = 0);

    public:
        static const bool value = sizeof(__test<_Tp>(0)) == 1;
    };

    template<class _Tp, class _Up, bool = __has_rebind<_Tp, _Up>::value> struct __has_rebind_other
    {
    private:
        struct __two
        {
            char __lx;
            char __lxx;
        };
        template<class _Xp> static __two __test(...);
        template<class _Xp> static char __test(typename _Xp::template rebind<_Up>::other* = 0);

    public:
        static const bool value = sizeof(__test<_Tp>(0)) == 1;
    };

    template<class _Tp, class _Up> struct __has_rebind_other<_Tp, _Up, false>
    {
        static const bool value = false;
    };

    template<class _Tp, class _Up, bool = __has_rebind_other<_Tp, _Up>::value> struct __allocator_traits_rebind
    {
        typedef typename _Tp::template rebind<_Up>::other type;
    };

    template<template<class, class...> class _Alloc, class _Tp, class... _Args, class _Up>
    struct __allocator_traits_rebind<_Alloc<_Tp, _Args...>, _Up, true>
    {
        typedef typename _Alloc<_Tp, _Args...>::template rebind<_Up>::other type;
    };

    template<template<class, class...> class _Alloc, class _Tp, class... _Args, class _Up>
    struct __allocator_traits_rebind<_Alloc<_Tp, _Args...>, _Up, false>
    {
        typedef _Alloc<_Up, _Args...> type;
    };

    struct __value_init_tag {};

    template<class _Tp, int _Idx, bool _CanBeEmptyBase = std::is_empty<_Tp>::value && !__libcpp_is_final<_Tp>::value>
    struct __compressed_pair_elem
    {
        typedef _Tp _ParamT;
        typedef _Tp& reference;
        typedef const _Tp& const_reference;

#ifndef _LIBCPP_CXX03_LANG
        constexpr __compressed_pair_elem()
            : __value_()
        {
        }

        template<class _Up, class = typename std::enable_if<
                                !std::is_same<__compressed_pair_elem, typename std::decay<_Up>::type>::value>::type>

        constexpr explicit __compressed_pair_elem(_Up&& __u)
            : __value_(std::forward<_Up>(__u))
        {
        }

        /*template<class... _Args, size_t... _Indexes>
        constexpr __compressed_pair_elem(std::piecewise_construct_t, std::tuple<_Args...> __args,
                                                             __tuple_indices<_Indexes...>)
            : __value_(std::forward<_Args>(std::get<_Indexes>(__args))...)
        {
        }*/
#else
        __compressed_pair_elem()
            : __value_()
        {
        }

        __compressed_pair_elem(_ParamT __p)
            : __value_(std::forward<_ParamT>(__p))
        {
        }
#endif

        reference __get() noexcept { return __value_; }

        const_reference __get() const noexcept { return __value_; }

    private:
        _Tp __value_;
    };

    template<class _Tp, int _Idx> struct __compressed_pair_elem<_Tp, _Idx, true> : private _Tp
    {
        typedef _Tp _ParamT;
        typedef _Tp& reference;
        typedef const _Tp& const_reference;
        typedef _Tp __value_type;

#ifndef _LIBCPP_CXX03_LANG
        constexpr __compressed_pair_elem() = default;

        template<class _Up, class = typename std::enable_if<
                                !std::is_same<__compressed_pair_elem, typename std::decay<_Up>::type>::value>::type>

        constexpr explicit __compressed_pair_elem(_Up&& __u)
            : __value_type(std::forward<_Up>(__u))
        {
        }

        /*template<class... _Args, size_t... _Indexes>
        constexpr __compressed_pair_elem(std::piecewise_construct_t, std::tuple<_Args...> __args,
                                                             __tuple_indices<_Indexes...>)
            : __value_type(std::forward<_Args>(std::get<_Indexes>(__args))...)
        {
        }*/
#else
        __compressed_pair_elem()
            : __value_type()
        {
        }

        __compressed_pair_elem(_ParamT __p)
            : __value_type(std::forward<_ParamT>(__p))
        {
        }
#endif

        reference __get() noexcept { return *this; }

        const_reference __get() const noexcept { return *this; }
    };

    // Tag used to construct the second element of the compressed pair.
    struct __second_tag
    {
    };

    template<class _T1, class _T2>
    class __compressed_pair : private __compressed_pair_elem<_T1, 0>, private __compressed_pair_elem<_T2, 1>
    {
        typedef __compressed_pair_elem<_T1, 0> _Base1;
        typedef __compressed_pair_elem<_T2, 1> _Base2;

        // NOTE: This static assert should never fire because __compressed_pair
        // is *almost never* used in a scenario where it's possible for T1 == T2.
        // (The exception is std::function where it is possible that the function
        //  object and the allocator have the same type).
        static_assert((!std::is_same<_T1, _T2>::value),
                      "__compressed_pair cannot be instantated when T1 and T2 are the same type; "
                      "The current implementation is NOT ABI-compatible with the previous "
                      "implementation for this configuration");

    public:
#ifndef _LIBCPP_CXX03_LANG
        template<bool _Dummy = true, class = typename std::enable_if<
                                         __dependent_type<std::is_default_constructible<_T1>, _Dummy>::value
                                         && __dependent_type<std::is_default_constructible<_T2>, _Dummy>::value>::type>

        constexpr __compressed_pair()
        {
        }

        template<class _Tp, typename std::enable_if<
                                !std::is_same<typename std::decay<_Tp>::type, __compressed_pair>::value, bool>::type
                            = true>
        constexpr explicit __compressed_pair(_Tp&& __t)
            : _Base1(std::forward<_Tp>(__t))
            , _Base2()
        {
        }

        template<class _Tp>
        constexpr __compressed_pair(__second_tag, _Tp&& __t)
            : _Base1()
            , _Base2(std::forward<_Tp>(__t))
        {
        }

        template<class _U1, class _U2>
        constexpr __compressed_pair(_U1&& __t1, _U2&& __t2)
            : _Base1(std::forward<_U1>(__t1))
            , _Base2(std::forward<_U2>(__t2))
        {
        }

#ifdef _IN_ENCLAVE
        template<class... _Args1, class... _Args2>
        constexpr __compressed_pair(std::piecewise_construct_t __pc, std::tuple<_Args1...> __first_args,
                                    std::tuple<_Args2...> __second_args)
            : _Base1(__pc, std::move(__first_args), typename std::__make_tuple_indices<sizeof...(_Args1)>::type())
            , _Base2(__pc, std::move(__second_args), typename std::__make_tuple_indices<sizeof...(_Args2)>::type())
        {
        }
#endif

#else

        __compressed_pair() { }

        explicit __compressed_pair(_T1 __t1)
            : _Base1(std::forward<_T1>(__t1))
        {
        }

        __compressed_pair(__second_tag, _T2 __t2)
            : _Base1()
            , _Base2(std::forward<_T2>(__t2))
        {
        }

        __compressed_pair(_T1 __t1, _T2 __t2)
            : _Base1(std::forward<_T1>(__t1))
            , _Base2(std::forward<_T2>(__t2))
        {
        }
#endif

        typename _Base1::reference first() noexcept { return static_cast<_Base1&>(*this).__get(); }

        typename _Base1::const_reference first() const noexcept { return static_cast<_Base1 const&>(*this).__get(); }

        typename _Base2::reference second() noexcept { return static_cast<_Base2&>(*this).__get(); }

        typename _Base2::const_reference second() const noexcept { return static_cast<_Base2 const&>(*this).__get(); }

        void
        swap(__compressed_pair& __x) noexcept(std::__is_nothrow_swappable<_T1>::value&& std::__is_nothrow_swappable<_T2>::value)
        {
            using std::swap;
            swap(first(), __x.first());
            swap(second(), __x.second());
        }
    };

    template<class _T1, class _T2>
    inline void swap(__compressed_pair<_T1, _T2>& __x, __compressed_pair<_T1, _T2>& __y) noexcept(
        std::__is_nothrow_swappable<_T1>::value&& std::__is_nothrow_swappable<_T2>::value)
    {
        __x.swap(__y);
    }

    /*template<class _Alloc> class __allocator_destructor
    {
        typedef std::allocator_traits<_Alloc> __alloc_traits;

    public:
        typedef typename __alloc_traits::pointer pointer;
        typedef typename __alloc_traits::size_type size_type;

    private:
        _Alloc& __alloc_;
        size_type __s_;

    public:
        __allocator_destructor(_Alloc& __a, size_type __s) noexcept : __alloc_(__a),
                                                                                                 __s_(__s)
        {
        }

        void operator()(pointer __p) noexcept { __alloc_traits::deallocate(__alloc_, __p, __s_); }
    };

// NOTE: Relaxed and acq/rel atomics (for increment and decrement respectively)
// should be sufficient for thread safety.
// See https://llvm.org/PR22803
#if defined(__clang__) && __has_builtin(__atomic_add_fetch) && defined(__ATOMIC_RELAXED) && defined(__ATOMIC_ACQ_REL)
#define _LIBCPP_HAS_BUILTIN_ATOMIC_SUPPORT
#elif defined(_LIBCPP_COMPILER_GCC)
#define _LIBCPP_HAS_BUILTIN_ATOMIC_SUPPORT
#endif*/

    template<class _Tp> inline _Tp __libcpp_atomic_refcount_increment(_Tp& __t) noexcept
    {
#if defined(_LIBCPP_HAS_BUILTIN_ATOMIC_SUPPORT) && !defined(_LIBCPP_HAS_NO_THREADS)
        return __atomic_add_fetch(&__t, 1, __ATOMIC_RELAXED);
#else
        return __t += 1;
#endif
    }

    template<class _Tp> inline _Tp __libcpp_atomic_refcount_decrement(_Tp& __t) noexcept
    {
#if defined(_LIBCPP_HAS_BUILTIN_ATOMIC_SUPPORT) && !defined(_LIBCPP_HAS_NO_THREADS)
        return __atomic_add_fetch(&__t, -1, __ATOMIC_ACQ_REL);
#else
        return __t -= 1;
#endif
    }

    class bad_weak_ptr : public std::exception
    {
    public:
        bad_weak_ptr() noexcept = default;
        bad_weak_ptr(const bad_weak_ptr&) noexcept = default;
        virtual ~bad_weak_ptr() noexcept;
        virtual const char* what() const noexcept;
    };

    [[noreturn]] inline void __throw_bad_weak_ptr()
    {
#ifndef _LIBCPP_NO_EXCEPTIONS
        throw bad_weak_ptr();
#else
        std::abort();
#endif
    }

    template<class _Tp> class weak_ptr;

    class __shared_count
    {
        __shared_count(const __shared_count&);
        __shared_count& operator=(const __shared_count&);

    protected:
        std::atomic<long> __shared_owners_;
        virtual ~__shared_count();

    private:
        virtual void __on_zero_shared() noexcept = 0;

    public:
        explicit __shared_count(long __refs = 0) noexcept
            : __shared_owners_(__refs)
        {
        }



        void __add_shared() noexcept { __shared_owners_++; }

        bool __release_shared() noexcept
        {
            LOG((std::string("__release_shared()") + std::to_string(__shared_owners_.load())).data(),100);
            if (--__shared_owners_ == -1)
            {
                __on_zero_shared();
                return true;
            }
            return false;
        }

        long use_count() const noexcept { return __shared_owners_.load(std::memory_order_relaxed) + 1; }
    };

    class __shared_weak_count : private __shared_count
    {
        std::atomic<long> __shared_weak_owners_;

    public:
        explicit __shared_weak_count(long __refs = 0) noexcept
            : __shared_count(__refs)
            , __shared_weak_owners_(__refs)
        {
        }

    protected:
        virtual ~__shared_weak_count();

    public:
#if defined(_LIBCPP_BUILDING_LIBRARY) && defined(_LIBCPP_DEPRECATED_ABI_LEGACY_LIBRARY_DEFINITIONS_FOR_INLINE_FUNCTIONS)
        void __add_shared() noexcept;
        void __add_weak() noexcept;
        void __release_shared() noexcept;
#else

        void __add_shared() noexcept { __shared_count::__add_shared(); }

        void __add_weak() noexcept { __shared_weak_owners_++; }

        void __release_shared() noexcept
        {
            if (__shared_count::__release_shared())
                __release_weak();
        }
#endif
        void __release_weak() noexcept;

        long use_count() const noexcept { return __shared_count::use_count(); }
        __shared_weak_count* lock() noexcept;

        virtual const void* __get_deleter(const std::type_info&) const noexcept;

    private:
        virtual void __on_zero_shared_weak() noexcept = 0;
    };

    template<class _Tp, class _Dp, class _Alloc> class __shared_ptr_pointer : public __shared_weak_count
    {
        __compressed_pair<__compressed_pair<_Tp, _Dp>, _Alloc> __data_;

    public:
        __shared_ptr_pointer(_Tp __p, _Dp __d, _Alloc __a)
            : __data_(__compressed_pair<_Tp, _Dp>(__p, std::move(__d)), std::move(__a))
        {
        }

        virtual const void* __get_deleter(const std::type_info&) const noexcept;

    private:
        virtual void __on_zero_shared() noexcept;
        virtual void __on_zero_shared_weak() noexcept;
    };

    template<class _Tp, class _Dp, class _Alloc>
    const void* __shared_ptr_pointer<_Tp, _Dp, _Alloc>::__get_deleter(const std::type_info& __t) const noexcept
    {
        return __t == typeid(_Dp) ? std::addressof(__data_.first().second()) : nullptr;
    }

    template<class _Tp, class _Dp, class _Alloc>
    void __shared_ptr_pointer<_Tp, _Dp, _Alloc>::__on_zero_shared() noexcept
    {
        LOG(std::to_string((uint64_t)&__data_.first().second()).data(),100);
        __data_.first().second()(__data_.first().first());
        __data_.first().second().~_Dp();
    }

    template<class _Tp, class _Dp, class _Alloc>
    void __shared_ptr_pointer<_Tp, _Dp, _Alloc>::__on_zero_shared_weak() noexcept
    {
        typedef typename __allocator_traits_rebind<_Alloc, __shared_ptr_pointer>::type _Al;
        typedef std::allocator_traits<_Al> _ATraits;
        typedef std::pointer_traits<typename _ATraits::pointer> _PTraits;

        _Al __a(__data_.second());
        __data_.second().~_Alloc();
        __a.deallocate(_PTraits::pointer_to(*this), 1);
    }

    template<class _Tp, class _Alloc> struct __shared_ptr_emplace : __shared_weak_count
    {
        template<class... _Args>
        explicit __shared_ptr_emplace(_Alloc __a, _Args&&... __args)
            : __storage_(std::move(__a))
        {
#if _LIBCPP_STD_VER > 17
            using _TpAlloc = typename __allocator_traits_rebind<_Alloc, _Tp>::type;
            _TpAlloc __tmp(*__get_alloc());
            std::allocator_traits<_TpAlloc>::construct(__tmp, __get_elem(), std::forward<_Args>(__args)...);
#else
            ::new ((void*)__get_elem()) _Tp(std::forward<_Args>(__args)...);
#endif
        }

        _Alloc* __get_alloc() noexcept { return __storage_.__get_alloc(); }

        _Tp* __get_elem() noexcept { return __storage_.__get_elem(); }

    private:
        virtual void __on_zero_shared() noexcept
        {
#if _LIBCPP_STD_VER > 17
            using _TpAlloc = typename __allocator_traits_rebind<_Alloc, _Tp>::type;
            _TpAlloc __tmp(*__get_alloc());
            std::allocator_traits<_TpAlloc>::destroy(__tmp, __get_elem());
#else
            __get_elem()->~_Tp();
#endif
        }

        virtual void __on_zero_shared_weak() noexcept
        {
            using _ControlBlockAlloc = typename __allocator_traits_rebind<_Alloc, __shared_ptr_emplace>::type;
            using _ControlBlockPointer = typename std::allocator_traits<_ControlBlockAlloc>::pointer;
            _ControlBlockAlloc __tmp(*__get_alloc());
            __storage_.~_Storage();
            std::allocator_traits<_ControlBlockAlloc>::deallocate(
                __tmp, std::pointer_traits<_ControlBlockPointer>::pointer_to(*this), 1);
        }

        // This class implements the control block for non-array shared pointers created
        // through `std::allocate_shared` and `std::make_shared`.
        //
        // In previous versions of the library, we used a compressed pair to store
        // both the _Alloc and the _Tp. This implies using EBO, which is incompatible
        // with Allocator construction for _Tp. To allow implementing P0674 in C++20,
        // we now use a properly aligned char buffer while making sure that we maintain
        // the same layout that we had when we used a compressed pair.
        using _CompressedPair = __compressed_pair<_Alloc, _Tp>;
        struct alignas(_CompressedPair) _Storage
        {
            char __blob_[sizeof(_CompressedPair)];

            explicit _Storage(_Alloc&& __a) { ::new ((void*)__get_alloc()) _Alloc(std::move(__a)); }
            ~_Storage() { __get_alloc()->~_Alloc(); }
            _Alloc* __get_alloc() noexcept
            {
                _CompressedPair* __as_pair = reinterpret_cast<_CompressedPair*>(__blob_);
                auto* __first = &__as_pair->first();
                _Alloc* __alloc = reinterpret_cast<_Alloc*>(__first);
                return __alloc;
            }
            _Tp* __get_elem() noexcept
            {
                _CompressedPair* __as_pair = reinterpret_cast<_CompressedPair*>(__blob_);
                auto* __second = &__as_pair->second();
                _Tp* __elem = reinterpret_cast<_Tp*>(__second);
                return __elem;
            }
        };

        static_assert(alignof(_Storage) == alignof(_CompressedPair), "");
        static_assert(sizeof(_Storage) == sizeof(_CompressedPair), "");
        _Storage __storage_;
    };

    /*struct __shared_ptr_dummy_rebind_allocator_type;
    template<> class allocator<__shared_ptr_dummy_rebind_allocator_type>
    {
    public:
        template<class _Other> struct rebind
        {
            typedef allocator<_Other> other;
        };
    };*/

    template<class _Tp> class enable_shared_from_this;

    template<class _Tp, class _Up>
    struct __compatible_with : std::is_convertible<std::remove_extent_t<_Tp>*, std::remove_extent_t<_Up>*>
    {
    };

    template<class _Ptr, class = void> struct __is_deletable : std::false_type
    {
    };
    
    #ifdef __linux__
    #pragma clang diadnostic push
    #pragma clang diadnostic ignored "-Wc++20-extensions"
    #endif // __linux__

    template<class _Ptr> struct __is_deletable<_Ptr, decltype(delete declval<_Ptr>())> : std::true_type
    {
    };

    template<class _Ptr, class = void> struct __is_array_deletable : std::false_type
    {
    };
    
    
    
    template<class _Ptr> struct __is_array_deletable<_Ptr, decltype(delete[] declval<_Ptr>())> : std::true_type
    {
    };

    #ifdef __linux__
    #pragma clang diadnostic pop
    #endif // __linux__

    template<class _Dp, class _Pt, class = decltype(declval<_Dp>()(declval<_Pt>()))>
    static std::true_type __well_formed_deleter_test(int);

    template<class, class> static std::false_type __well_formed_deleter_test(...);

    template<class _Dp, class _Pt> struct __well_formed_deleter : decltype(__well_formed_deleter_test<_Dp, _Pt>(0))
    {
    };

    template<class _Dp, class _Tp, class _Yp> struct __shared_ptr_deleter_ctor_reqs
    {
        static const bool value = __compatible_with<_Tp, _Yp>::value && std::is_move_constructible<_Dp>::value
                                  && __well_formed_deleter<_Dp, _Tp*>::value;
    };

    template<class> struct __void_t
    {
        typedef void type;
    };

#define _LIBCPP_ALLOCATOR_TRAITS_HAS_XXX(NAME, PROPERTY)                                                               \
    template<class _Tp, class = void> struct NAME : std::false_type                                                    \
    {                                                                                                                  \
    };                                                                                                                 \
    template<class _Tp> struct NAME<_Tp, typename __void_t<typename _Tp::PROPERTY>::type> : std::true_type             \
    {                                                                                                                  \
    }
    // __pointer
    _LIBCPP_ALLOCATOR_TRAITS_HAS_XXX(__has_pointer, pointer);

    template<class _Tp, class _Alloc, class _RawAlloc = typename std::remove_reference<_Alloc>::type,
             bool = __has_pointer<_RawAlloc>::value>
    struct __pointer
    {
        using type = typename _RawAlloc::pointer;
    };
    template<class _Tp, class _Alloc, class _RawAlloc> struct __pointer<_Tp, _Alloc, _RawAlloc, false>
    {
        using type = _Tp*;
    };

    template<class _Deleter> struct __unique_ptr_deleter_sfinae
    {
        static_assert(!std::is_reference<_Deleter>::value, "incorrect specialization");
        typedef const _Deleter& __lval_ref_type;
        typedef _Deleter&& __good_rval_ref_type;
        typedef std::true_type __enable_rval_overload;
    };
    template<class _Deleter> struct __unique_ptr_deleter_sfinae<_Deleter const&>
    {
        typedef const _Deleter& __lval_ref_type;
        typedef const _Deleter&& __bad_rval_ref_type;
        typedef std::false_type __enable_rval_overload;
    };
    template<class _Deleter> struct __unique_ptr_deleter_sfinae<_Deleter&>
    {
        typedef _Deleter& __lval_ref_type;
        typedef _Deleter&& __bad_rval_ref_type;
        typedef std::false_type __enable_rval_overload;
    };

    template <class _Tp>
    struct __type_identity { typedef _Tp type; };

    // DECLARATIONS
    template<class _Tp, class _Dp = std::default_delete<_Tp>> class unique_ptr
    {
    public:
        typedef _Tp element_type;
        typedef _Dp deleter_type;
        typedef typename __pointer<_Tp, deleter_type>::type pointer;
        static_assert(!std::is_rvalue_reference<deleter_type>::value,
                      "the specified deleter type cannot be an rvalue reference");

    private:
        __compressed_pair<pointer, deleter_type> __ptr_;
        struct __nat
        {
            int __for_bool_;
        };
        typedef __unique_ptr_deleter_sfinae<_Dp> _DeleterSFINAE;
        template<bool _Dummy> using _LValRefType = typename __dependent_type<_DeleterSFINAE, _Dummy>::__lval_ref_type;
        template<bool _Dummy>
        using _GoodRValRefType = typename __dependent_type<_DeleterSFINAE, _Dummy>::__good_rval_ref_type;
        template<bool _Dummy>
        using _BadRValRefType = typename __dependent_type<_DeleterSFINAE, _Dummy>::__bad_rval_ref_type;
        template<bool _Dummy, class _Deleter = typename __dependent_type<__type_identity<deleter_type>, _Dummy>::type>
        using _EnableIfDeleterDefaultConstructible =
            typename std::enable_if<std::is_default_constructible<_Deleter>::value && !std::is_pointer<_Deleter>::value>::type;
        template<class _ArgType>
        using _EnableIfDeleterConstructible = typename std::enable_if<std::is_constructible<deleter_type, _ArgType>::value>::type;
        template<class _UPtr, class _Up>
        using _EnableIfMoveConvertible =
            typename std::enable_if<std::is_convertible<typename _UPtr::pointer, pointer>::value && !std::is_array<_Up>::value>::type;
        template<class _UDel>
        using _EnableIfDeleterConvertible =
            typename std::enable_if<(std::is_reference<_Dp>::value && std::is_same<_Dp, _UDel>::value)
                               || (!std::is_reference<_Dp>::value && std::is_convertible<_UDel, _Dp>::value)>::type;
        template<class _UDel>
        using _EnableIfDeleterAssignable = typename std::enable_if<std::is_assignable<_Dp&, _UDel&&>::value>::type;

    public:
        template<bool _Dummy = true, class = _EnableIfDeleterDefaultConstructible<_Dummy>>
        constexpr unique_ptr() noexcept
            : __ptr_(__value_init_tag(), __value_init_tag())
        {
        }
        template<bool _Dummy = true, class = _EnableIfDeleterDefaultConstructible<_Dummy>>
        constexpr unique_ptr(std::nullptr_t) noexcept
            : __ptr_(__value_init_tag(), __value_init_tag())
        {
        }
        template<bool _Dummy = true, class = _EnableIfDeleterDefaultConstructible<_Dummy>>
        explicit unique_ptr(pointer __p) noexcept : __ptr_(__p, __value_init_tag())
        {
        }
        template<bool _Dummy = true, class = _EnableIfDeleterConstructible<_LValRefType<_Dummy>>>
        unique_ptr(pointer __p, _LValRefType<_Dummy> __d) noexcept : __ptr_(__p, __d)
        {
        }
        template<bool _Dummy = true, class = _EnableIfDeleterConstructible<_GoodRValRefType<_Dummy>>>
        unique_ptr(pointer __p, _GoodRValRefType<_Dummy> __d) noexcept
            : __ptr_(__p, std::move(__d))
        {
            static_assert(!std::is_reference<deleter_type>::value, "rvalue deleter bound to reference");
        }
        template<bool _Dummy = true, class = _EnableIfDeleterConstructible<_BadRValRefType<_Dummy>>>
        unique_ptr(pointer __p, _BadRValRefType<_Dummy> __d) = delete;
        
        unique_ptr(unique_ptr&& __u) noexcept : __ptr_(__u.release(), std::forward<deleter_type>(__u.get_deleter()))
        {
        }
        template<class _Up, class _Ep, class = _EnableIfMoveConvertible<unique_ptr<_Up, _Ep>, _Up>,
                 class = _EnableIfDeleterConvertible<_Ep>>
        unique_ptr(unique_ptr<_Up, _Ep>&& __u) noexcept
            : __ptr_(__u.release(), std::forward<_Ep>(__u.get_deleter()))
        {
        }
        
        unique_ptr& operator=(unique_ptr&& __u) noexcept
        {
            reset(__u.release());
            __ptr_.second() = std::forward<deleter_type>(__u.get_deleter());
            return *this;
        }
        template<class _Up, class _Ep, class = _EnableIfMoveConvertible<unique_ptr<_Up, _Ep>, _Up>,
                 class = _EnableIfDeleterAssignable<_Ep>>
        unique_ptr& operator=(unique_ptr<_Up, _Ep>&& __u) noexcept
        {
            reset(__u.release());
            __ptr_.second() = std::forward<_Ep>(__u.get_deleter());
            return *this;
        }
        
        ~unique_ptr() { reset(); }
        
        unique_ptr& operator=(std::nullptr_t) noexcept
        {
            reset();
            return *this;
        }
        
        typename std::add_lvalue_reference<_Tp>::type operator*() const { return *__ptr_.first(); }
        
        pointer operator->() const noexcept { return __ptr_.first(); }
        
        pointer get() const noexcept { return __ptr_.first(); }
        
        deleter_type& get_deleter() noexcept { return __ptr_.second(); }
        
        const deleter_type& get_deleter() const noexcept { return __ptr_.second(); }
        
        explicit operator bool() const noexcept { return __ptr_.first() != nullptr; }
        
        pointer release() noexcept
        {
            pointer __t = __ptr_.first();
            __ptr_.first() = pointer();
            return __t;
        }
        
        void reset(pointer __p = pointer()) noexcept
        {
            pointer __tmp = __ptr_.first();
            __ptr_.first() = __p;
            if (__tmp)
                __ptr_.second()(__tmp);
        }
        
        void swap(unique_ptr& __u) noexcept { __ptr_.swap(__u.__ptr_); }
    };
    template<class _Tp, class _Dp> class unique_ptr<_Tp[], _Dp>
    {
    public:
        typedef _Tp element_type;
        typedef _Dp deleter_type;
        typedef typename __pointer<_Tp, deleter_type>::type pointer;

    private:
        __compressed_pair<pointer, deleter_type> __ptr_;
        template<class _From> struct _CheckArrayPointerConversion : std::is_same<_From, pointer>
        {
        };
        template<class _FromElem>
        struct _CheckArrayPointerConversion<_FromElem*>
            : std::integral_constant<bool, std::is_same<_FromElem*, pointer>::value
                                          || (std::is_same<pointer, element_type*>::value
                                              && std::is_convertible<_FromElem (*)[], element_type (*)[]>::value)>
        {
        };
        typedef __unique_ptr_deleter_sfinae<_Dp> _DeleterSFINAE;
        template<bool _Dummy> using _LValRefType = typename __dependent_type<_DeleterSFINAE, _Dummy>::__lval_ref_type;
        template<bool _Dummy>
        using _GoodRValRefType = typename __dependent_type<_DeleterSFINAE, _Dummy>::__good_rval_ref_type;
        template<bool _Dummy>
        using _BadRValRefType = typename __dependent_type<_DeleterSFINAE, _Dummy>::__bad_rval_ref_type;
        template<bool _Dummy, class _Deleter = typename __dependent_type<__type_identity<deleter_type>, _Dummy>::type>
        using _EnableIfDeleterDefaultConstructible =
            typename std::enable_if<std::is_default_constructible<_Deleter>::value && !std::is_pointer<_Deleter>::value>::type;
        template<class _ArgType>
        using _EnableIfDeleterConstructible = typename std::enable_if<std::is_constructible<deleter_type, _ArgType>::value>::type;
        template<class _Pp>
        using _EnableIfPointerConvertible = typename std::enable_if<_CheckArrayPointerConversion<_Pp>::value>::type;
        template<class _UPtr, class _Up, class _ElemT = typename _UPtr::element_type>
        using _EnableIfMoveConvertible =
            typename std::enable_if<std::is_array<_Up>::value && std::is_same<pointer, element_type*>::value
                               && std::is_same<typename _UPtr::pointer, _ElemT*>::value
                               && std::is_convertible<_ElemT (*)[], element_type (*)[]>::value>::type;
        template<class _UDel>
        using _EnableIfDeleterConvertible =
            typename std::enable_if<(std::is_reference<_Dp>::value && std::is_same<_Dp, _UDel>::value)
                               || (!std::is_reference<_Dp>::value && std::is_convertible<_UDel, _Dp>::value)>::type;
        template<class _UDel>
        using _EnableIfDeleterAssignable = typename std::enable_if<std::is_assignable<_Dp&, _UDel&&>::value>::type;

    public:
        template<bool _Dummy = true, class = _EnableIfDeleterDefaultConstructible<_Dummy>>
        constexpr unique_ptr() noexcept
            : __ptr_(__value_init_tag(), __value_init_tag())
        {
        }
        template<bool _Dummy = true, class = _EnableIfDeleterDefaultConstructible<_Dummy>>
        constexpr unique_ptr(std::nullptr_t) noexcept
            : __ptr_(__value_init_tag(), __value_init_tag())
        {
        }
        template<class _Pp, bool _Dummy = true, class = _EnableIfDeleterDefaultConstructible<_Dummy>,
                 class = _EnableIfPointerConvertible<_Pp>>
        explicit unique_ptr(_Pp __p) noexcept : __ptr_(__p, __value_init_tag())
        {
        }
        template<class _Pp, bool _Dummy = true, class = _EnableIfDeleterConstructible<_LValRefType<_Dummy>>,
                 class = _EnableIfPointerConvertible<_Pp>>
        unique_ptr(_Pp __p, _LValRefType<_Dummy> __d) noexcept : __ptr_(__p, __d)
        {
        }
        template<bool _Dummy = true, class = _EnableIfDeleterConstructible<_LValRefType<_Dummy>>>
        unique_ptr(std::nullptr_t, _LValRefType<_Dummy> __d) noexcept : __ptr_(nullptr, __d)
        {
        }
        template<class _Pp, bool _Dummy = true, class = _EnableIfDeleterConstructible<_GoodRValRefType<_Dummy>>,
                 class = _EnableIfPointerConvertible<_Pp>>
        unique_ptr(_Pp __p, _GoodRValRefType<_Dummy> __d) noexcept
            : __ptr_(__p, std::move(__d))
        {
            static_assert(!std::is_reference<deleter_type>::value, "rvalue deleter bound to reference");
        }
        template<bool _Dummy = true, class = _EnableIfDeleterConstructible<_GoodRValRefType<_Dummy>>>
        unique_ptr(std::nullptr_t, _GoodRValRefType<_Dummy> __d) noexcept
            : __ptr_(nullptr, std::move(__d))
        {
            static_assert(!std::is_reference<deleter_type>::value, "rvalue deleter bound to reference");
        }
        template<class _Pp, bool _Dummy = true, class = _EnableIfDeleterConstructible<_BadRValRefType<_Dummy>>,
                 class = _EnableIfPointerConvertible<_Pp>>
        unique_ptr(_Pp __p, _BadRValRefType<_Dummy> __d) = delete;
        
        unique_ptr(unique_ptr&& __u) noexcept : __ptr_(__u.release(), std::forward<deleter_type>(__u.get_deleter()))
        {
        }
        
        unique_ptr& operator=(unique_ptr&& __u) noexcept
        {
            reset(__u.release());
            __ptr_.second() = std::forward<deleter_type>(__u.get_deleter());
            return *this;
        }
        template<class _Up, class _Ep, class = _EnableIfMoveConvertible<unique_ptr<_Up, _Ep>, _Up>,
                 class = _EnableIfDeleterConvertible<_Ep>>
        unique_ptr(unique_ptr<_Up, _Ep>&& __u) noexcept
            : __ptr_(__u.release(), std::forward<_Ep>(__u.get_deleter()))
        {
        }
        template<class _Up, class _Ep, class = _EnableIfMoveConvertible<unique_ptr<_Up, _Ep>, _Up>,
                 class = _EnableIfDeleterAssignable<_Ep>>
        unique_ptr& operator=(unique_ptr<_Up, _Ep>&& __u) noexcept
        {
            reset(__u.release());
            __ptr_.second() = std::forward<_Ep>(__u.get_deleter());
            return *this;
        }

    public:
        
        ~unique_ptr() { reset(); }
        
        unique_ptr& operator=(std::nullptr_t) noexcept
        {
            reset();
            return *this;
        }
        
        typename std::add_lvalue_reference<_Tp>::type operator[](size_t __i) const { return __ptr_.first()[__i]; }
        
        pointer get() const noexcept { return __ptr_.first(); }
        
        deleter_type& get_deleter() noexcept { return __ptr_.second(); }
        
        const deleter_type& get_deleter() const noexcept { return __ptr_.second(); }
        
        explicit operator bool() const noexcept { return __ptr_.first() != nullptr; }
        
        pointer release() noexcept
        {
            pointer __t = __ptr_.first();
            __ptr_.first() = pointer();
            return __t;
        }
        template<class _Pp>
        typename std::enable_if<_CheckArrayPointerConversion<_Pp>::value>::type
        reset(_Pp __p) noexcept
        {
            pointer __tmp = __ptr_.first();
            __ptr_.first() = __p;
            if (__tmp)
                __ptr_.second()(__tmp);
        }
        
        void reset(std::nullptr_t = nullptr) noexcept
        {
            pointer __tmp = __ptr_.first();
            __ptr_.first() = nullptr;
            if (__tmp)
                __ptr_.second()(__tmp);
        }
        
        void swap(unique_ptr& __u) noexcept { __ptr_.swap(__u.__ptr_); }
    };
    /*template<class _Tp, class _Dp>
    inline typename std::enable_if<__is_swappable<_Dp>::value, void>::type
    swap(unique_ptr<_Tp, _Dp>& __x, unique_ptr<_Tp, _Dp>& __y) noexcept
    {
        __x.swap(__y);
    }*/
    template<class _T1, class _D1, class _T2, class _D2>
    inline bool operator==(const unique_ptr<_T1, _D1>& __x, const unique_ptr<_T2, _D2>& __y)
    {
        return __x.get() == __y.get();
    }
    template<class _T1, class _D1, class _T2, class _D2>
    inline bool operator!=(const unique_ptr<_T1, _D1>& __x, const unique_ptr<_T2, _D2>& __y)
    {
        return !(__x == __y);
    }
    template<class _T1, class _D1, class _T2, class _D2>
    inline bool operator<(const unique_ptr<_T1, _D1>& __x, const unique_ptr<_T2, _D2>& __y)
    {
        typedef typename unique_ptr<_T1, _D1>::pointer _P1;
        typedef typename unique_ptr<_T2, _D2>::pointer _P2;
        typedef typename std::common_type<_P1, _P2>::type _Vp;
        return less<_Vp>()(__x.get(), __y.get());
    }
    template<class _T1, class _D1, class _T2, class _D2>
    inline bool operator>(const unique_ptr<_T1, _D1>& __x, const unique_ptr<_T2, _D2>& __y)
    {
        return __y < __x;
    }
    template<class _T1, class _D1, class _T2, class _D2>
    inline bool operator<=(const unique_ptr<_T1, _D1>& __x, const unique_ptr<_T2, _D2>& __y)
    {
        return !(__y < __x);
    }
    template<class _T1, class _D1, class _T2, class _D2>
    inline bool operator>=(const unique_ptr<_T1, _D1>& __x, const unique_ptr<_T2, _D2>& __y)
    {
        return !(__x < __y);
    }
    template<class _T1, class _D1>
    inline bool operator==(const unique_ptr<_T1, _D1>& __x, std::nullptr_t) noexcept
    {
        return !__x;
    }
    template<class _T1, class _D1>
    inline bool operator==(std::nullptr_t, const unique_ptr<_T1, _D1>& __x) noexcept
    {
        return !__x;
    }
    template<class _T1, class _D1>
    inline bool operator!=(const unique_ptr<_T1, _D1>& __x, std::nullptr_t) noexcept
    {
        return static_cast<bool>(__x);
    }
    template<class _T1, class _D1>
    inline bool operator!=(std::nullptr_t, const unique_ptr<_T1, _D1>& __x) noexcept
    {
        return static_cast<bool>(__x);
    }
    template<class _T1, class _D1>
    inline bool operator<(const unique_ptr<_T1, _D1>& __x, std::nullptr_t)
    {
        typedef typename unique_ptr<_T1, _D1>::pointer _P1;
        return less<_P1>()(__x.get(), nullptr);
    }
    template<class _T1, class _D1>
    inline bool operator<(std::nullptr_t, const unique_ptr<_T1, _D1>& __x)
    {
        typedef typename unique_ptr<_T1, _D1>::pointer _P1;
        return less<_P1>()(nullptr, __x.get());
    }
    template<class _T1, class _D1>
    inline bool operator>(const unique_ptr<_T1, _D1>& __x, std::nullptr_t)
    {
        return nullptr < __x;
    }
    template<class _T1, class _D1>
    inline bool operator>(std::nullptr_t, const unique_ptr<_T1, _D1>& __x)
    {
        return __x < nullptr;
    }
    template<class _T1, class _D1>
    inline bool operator<=(const unique_ptr<_T1, _D1>& __x, std::nullptr_t)
    {
        return !(nullptr < __x);
    }
    template<class _T1, class _D1>
    inline bool operator<=(std::nullptr_t, const unique_ptr<_T1, _D1>& __x)
    {
        return !(__x < nullptr);
    }
    template<class _T1, class _D1>
    inline bool operator>=(const unique_ptr<_T1, _D1>& __x, std::nullptr_t)
    {
        return !(__x < nullptr);
    }
    template<class _T1, class _D1>
    inline bool operator>=(std::nullptr_t, const unique_ptr<_T1, _D1>& __x)
    {
        return !(nullptr < __x);
    }
    template<class _Tp> struct __unique_if
    {
        typedef unique_ptr<_Tp> __unique_single;
    };
    template<class _Tp> struct __unique_if<_Tp[]>
    {
        typedef unique_ptr<_Tp[]> __unique_array_unknown_bound;
    };
    template<class _Tp, size_t _Np> struct __unique_if<_Tp[_Np]>
    {
        typedef void __unique_array_known_bound;
    };
    template<class _Tp, class... _Args>
    inline typename __unique_if<_Tp>::__unique_single make_unique(_Args&&... __args)
    {
        return unique_ptr<_Tp>(new _Tp(std::forward<_Args>(__args)...));
    }
    template<class _Tp>
    inline typename __unique_if<_Tp>::__unique_array_unknown_bound make_unique(size_t __n)
    {
        typedef typename std::remove_extent<_Tp>::type _Up;
        return unique_ptr<_Tp>(new _Up[__n]());
    }
    /*template<class _Tp, class... _Args>
    typename __unique_if<_Tp>::__unique_array_known_bound make_unique(_Args&&...) = delete;
    template<class _Tp> struct hash;
    template<class _Tp, class _Dp>
    struct std::hash<__enable_hash_helper<unique_ptr<_Tp, _Dp>, typename unique_ptr<_Tp, _Dp>::pointer>>
    {
        
        size_t operator()(const unique_ptr<_Tp, _Dp>& __ptr) const
        {
            typedef typename unique_ptr<_Tp, _Dp>::pointer pointer;
            return hash<pointer>()(__ptr.get());
        }
    };*/

    template<class _Tp> class shared_ptr
    {
    public:
        typedef weak_ptr<_Tp> weak_type;
        typedef std::remove_extent_t<_Tp> element_type;

    private:
        element_type* __ptr_;
        __shared_weak_count* __cntrl_;

    public:
        constexpr shared_ptr() noexcept
            : __ptr_(nullptr)
            , __cntrl_(nullptr)
        {
        }

        constexpr shared_ptr(std::nullptr_t) noexcept
            : __ptr_(nullptr)
            , __cntrl_(nullptr)
        {
        }
        
        template<class _Yp
        /*,         class = __std::enable_if_t<_And<__compatible_with<_Yp, _Tp>
        // In C++03 we get errors when trying to do SFINAE with the
        // delete operator, so we always pretend that it's deletable.
        // The same happens on GCC.
#if !defined(_LIBCPP_CXX03_LANG) && !defined(_LIBCPP_COMPILER_GCC)
                                            ,
                                            _If<std::is_array<_Tp>::value, __is_array_deletable<_Yp*>, __is_deletable<_Yp*>>
#endif
                                            >::value>*/>
        explicit shared_ptr(_Yp* __p)
            : __ptr_(__p)
        {
            std::unique_ptr<_Yp> __hold(__p);
            typedef typename __shared_ptr_default_allocator<_Yp>::type _AllocT;
            typedef __shared_ptr_pointer<_Yp*, __shared_ptr_default_delete<_Tp, _Yp>, _AllocT> _CntrlBlk;
            __cntrl_ = new _CntrlBlk(__p, __shared_ptr_default_delete<_Tp, _Yp>(), _AllocT());
            __hold.release();
            __enable_weak_this(__p, __p);
        }

        template<class _Yp, class _Dp,
                 class = typename std::enable_if<__shared_ptr_deleter_ctor_reqs<_Dp, _Yp, element_type>::value>::type>
        shared_ptr(_Yp* __p, _Dp __d)
            : __ptr_(__p)
        {
#ifndef _LIBCPP_NO_EXCEPTIONS
            try
            {
#endif // _LIBCPP_NO_EXCEPTIONS
                typedef typename __shared_ptr_default_allocator<_Yp>::type _AllocT;
                typedef __shared_ptr_pointer<_Yp*, _Dp, _AllocT> _CntrlBlk;
#ifndef _LIBCPP_CXX03_LANG
                __cntrl_ = new _CntrlBlk(__p, std::move(__d), _AllocT());
#else
            __cntrl_ = new _CntrlBlk(__p, __d, _AllocT());
#endif // not _LIBCPP_CXX03_LANG
                __enable_weak_this(__p, __p);
#ifndef _LIBCPP_NO_EXCEPTIONS
            }
            catch (...)
            {
                __d(__p);
                throw;
            }
#endif // _LIBCPP_NO_EXCEPTIONS
        }
#ifdef _IN_ENCLAVE
        template<class _Yp, class _Dp, class _Alloc,
                 class = typename std::enable_if<__shared_ptr_deleter_ctor_reqs<_Dp, _Yp, element_type>::value>::type>
        shared_ptr(_Yp* __p, _Dp __d, _Alloc __a)
            : __ptr_(__p)
        {
#ifndef _LIBCPP_NO_EXCEPTIONS
            try
            {
#endif // _LIBCPP_NO_EXCEPTIONS
                typedef __shared_ptr_pointer<_Yp*, _Dp, _Alloc> _CntrlBlk;
                typedef typename __allocator_traits_rebind<_Alloc, _CntrlBlk>::type _A2;
                typedef std::__allocator_destructor<_A2> _D2;
                _A2 __a2(__a);
                std::unique_ptr<_CntrlBlk, _D2> __hold2(__a2.allocate(1), _D2(__a2, 1));
                ::new ((void*)std::addressof(*__hold2.get()))
#ifndef _LIBCPP_CXX03_LANG
                    _CntrlBlk(__p, std::move(__d), __a);
#else
                _CntrlBlk(__p, __d, __a);
#endif // not _LIBCPP_CXX03_LANG
                __cntrl_ = std::addressof(*__hold2.release());
                __enable_weak_this(__p, __p);
#ifndef _LIBCPP_NO_EXCEPTIONS
            }
            catch (...)
            {
                __d(__p);
                throw;
            }
#endif // _LIBCPP_NO_EXCEPTIONS
        }
#endif

        template<class _Dp>
        shared_ptr(std::nullptr_t __p, _Dp __d)
            : __ptr_(nullptr)
        {
#ifndef _LIBCPP_NO_EXCEPTIONS
            try
            {
#endif // _LIBCPP_NO_EXCEPTIONS
                typedef typename __shared_ptr_default_allocator<_Tp>::type _AllocT;
                typedef __shared_ptr_pointer<std::nullptr_t, _Dp, _AllocT> _CntrlBlk;
#ifndef _LIBCPP_CXX03_LANG
                __cntrl_ = new _CntrlBlk(__p, std::move(__d), _AllocT());
#else
            __cntrl_ = new _CntrlBlk(__p, __d, _AllocT());
#endif // not _LIBCPP_CXX03_LANG
#ifndef _LIBCPP_NO_EXCEPTIONS
            }
            catch (...)
            {
                __d(__p);
                throw;
            }
#endif // _LIBCPP_NO_EXCEPTIONS
        }

#ifdef _IN_ENCLAVE
        template<class _Dp, class _Alloc>
        shared_ptr(std::nullptr_t __p, _Dp __d, _Alloc __a)
            : __ptr_(nullptr)
        {
#ifndef _LIBCPP_NO_EXCEPTIONS
            try
            {
#endif // _LIBCPP_NO_EXCEPTIONS
                typedef __shared_ptr_pointer<std::nullptr_t, _Dp, _Alloc> _CntrlBlk;
                typedef typename __allocator_traits_rebind<_Alloc, _CntrlBlk>::type _A2;
                typedef std::__allocator_destructor<_A2> _D2;
                _A2 __a2(__a);
                std::unique_ptr<_CntrlBlk, _D2> __hold2(__a2.allocate(1), _D2(__a2, 1));
                ::new ((void*)std::addressof(*__hold2.get()))
#ifndef _LIBCPP_CXX03_LANG
                    _CntrlBlk(__p, std::move(__d), __a);
#else
                _CntrlBlk(__p, __d, __a);
#endif // not _LIBCPP_CXX03_LANG
                __cntrl_ = std::addressof(*__hold2.release());
#ifndef _LIBCPP_NO_EXCEPTIONS
            }
            catch (...)
            {
                __d(__p);
                throw;
            }
#endif // _LIBCPP_NO_EXCEPTIONS
        }

#endif

        template<class _Yp>
        shared_ptr(const shared_ptr<_Yp>& __r, element_type* __p) noexcept
            : __ptr_(__p)
            , __cntrl_(__r.__cntrl_)
        {
            if (__cntrl_)
                __cntrl_->__add_shared();
        }

        shared_ptr(const shared_ptr& __r) noexcept
            : __ptr_(__r.__ptr_)
            , __cntrl_(__r.__cntrl_)
        {
            if (__cntrl_)
                __cntrl_->__add_shared();
        }

        template<class _Yp, class = typename std::enable_if<__compatible_with<_Yp, _Tp>::value>::type>
        shared_ptr(const shared_ptr<_Yp>& __r) noexcept
            : __ptr_(__r.__ptr_)
            , __cntrl_(__r.__cntrl_)
        {
            if (__cntrl_)
                __cntrl_->__add_shared();
        }

        shared_ptr(shared_ptr&& __r) noexcept
            : __ptr_(__r.__ptr_)
            , __cntrl_(__r.__cntrl_)
        {
            __r.__ptr_ = nullptr;
            __r.__cntrl_ = nullptr;
        }

        template<class _Yp, class = typename std::enable_if<__compatible_with<_Yp, _Tp>::value>::type>
        shared_ptr(shared_ptr<_Yp>&& __r) noexcept
            : __ptr_(__r.__ptr_)
            , __cntrl_(__r.__cntrl_)
        {
            __r.__ptr_ = nullptr;
            __r.__cntrl_ = nullptr;
        }

        template<class _Yp>
        explicit shared_ptr(const weak_ptr<_Yp>& __r)
            : __ptr_(__r.__ptr_)
            , __cntrl_(__r.__cntrl_ ? __r.__cntrl_->lock() : __r.__cntrl_)
        {
            if (__cntrl_ == nullptr)
                __throw_bad_weak_ptr();
        }

        /*template<class _Yp, class _Dp,
                 class = __enable_if_t<!std::is_lvalue_reference<_Dp>::value
                                       && std::is_convertible<typename unique_ptr<_Yp, _Dp>::pointer,
element_type*>::value>> shared_ptr(unique_ptr<_Yp, _Dp>&& __r) : __ptr_(__r.get())
        {
#if _LIBCPP_STD_VER > 11
            if (__ptr_ == nullptr)
                __cntrl_ = nullptr;
            else
#endif
            {
                typedef typename __shared_ptr_default_allocator<_Yp>::type _AllocT;
                typedef __shared_ptr_pointer<typename unique_ptr<_Yp, _Dp>::pointer, _Dp, _AllocT> _CntrlBlk;
                __cntrl_ = new _CntrlBlk(__r.get(), __r.get_deleter(), _AllocT());
                __enable_weak_this(__r.get(), __r.get());
            }
            __r.release();
        }*/

        /*template<class _Yp, class _Dp, class = void,
                 class = __enable_if_t<std::is_lvalue_reference<_Dp>::value
                                       && std::is_convertible<typename unique_ptr<_Yp, _Dp>::pointer,
element_type*>::value>> shared_ptr(unique_ptr<_Yp, _Dp>&& __r) : __ptr_(__r.get())
        {
#if _LIBCPP_STD_VER > 11
            if (__ptr_ == nullptr)
                __cntrl_ = nullptr;
            else
#endif
            {
                typedef typename __shared_ptr_default_allocator<_Yp>::type _AllocT;
                typedef __shared_ptr_pointer<typename unique_ptr<_Yp, _Dp>::pointer,
                                             reference_wrapper<typename remove_reference<_Dp>::type>, _AllocT>
                    _CntrlBlk;
                __cntrl_ = new _CntrlBlk(__r.get(), std::ref(__r.get_deleter()), _AllocT());
                __enable_weak_this(__r.get(), __r.get());
            }
            __r.release();
        }*/

        ~shared_ptr()
        {
            if (__cntrl_)
            {
                LOG(typeid(*this).raw_name(),100);
                __cntrl_->__release_shared();
            }
        }

        shared_ptr<_Tp>& operator=(const shared_ptr& __r) noexcept
        {
            shared_ptr(__r).swap(*this);
            return *this;
        }

        template<class _Yp, class = typename std::enable_if<__compatible_with<_Yp, _Tp>::value>::type>
        shared_ptr<_Tp>& operator=(const shared_ptr<_Yp>& __r) noexcept
        {
            shared_ptr(__r).swap(*this);
            return *this;
        }

        shared_ptr<_Tp>& operator=(shared_ptr&& __r) noexcept
        {
            shared_ptr(std::move(__r)).swap(*this);
            return *this;
        }

        template<class _Yp, class = typename std::enable_if<__compatible_with<_Yp, _Tp>::value>::type>
        shared_ptr<_Tp>& operator=(shared_ptr<_Yp>&& __r)
        {
            shared_ptr(std::move(__r)).swap(*this);
            return *this;
        }

        /*template<class _Yp, class _Dp,
                 class = __enable_if_t<std::is_convertible<typename unique_ptr<_Yp, _Dp>::pointer,
        element_type*>::value>> shared_ptr<_Tp>& operator=(unique_ptr<_Yp, _Dp>&& __r)
        {
            shared_ptr(std::move(__r)).swap(*this);
            return *this;
        }*/

        void swap(shared_ptr& __r) noexcept
        {
            std::swap(__ptr_, __r.__ptr_);
            std::swap(__cntrl_, __r.__cntrl_);
        }

        void reset() noexcept { shared_ptr().swap(*this); }

        template<class _Yp, class = typename std::enable_if<__compatible_with<_Yp, _Tp>::value>::type> void reset(_Yp* __p)
        {
            shared_ptr(__p).swap(*this);
        }

        template<class _Yp, class _Dp, class = typename std::enable_if<__compatible_with<_Yp, _Tp>::value>::type>
        void reset(_Yp* __p, _Dp __d)
        {
            shared_ptr(__p, __d).swap(*this);
        }

        template<class _Yp, class _Dp, class _Alloc, class = typename std::enable_if<__compatible_with<_Yp, _Tp>::value>::type>
        void reset(_Yp* __p, _Dp __d, _Alloc __a)
        {
            shared_ptr(__p, __d, __a).swap(*this);
        }

        element_type* get() const noexcept { return __ptr_; }

        typename std::add_lvalue_reference<element_type>::type operator*() const noexcept { return *__ptr_; }

        element_type* operator->() const noexcept
        {
            static_assert(!std::is_array<_Tp>::value,
                          "std::shared_ptr<T>::operator-> is only valid when T is not an array type.");
            return __ptr_;
        }

        long use_count() const noexcept { return __cntrl_ ? __cntrl_->use_count() : 0; }

        bool unique() const noexcept { return use_count() == 1; }

        explicit operator bool() const noexcept { return get() != nullptr; }

        template<class _Up> bool owner_before(shared_ptr<_Up> const& __p) const noexcept
        {
            return __cntrl_ < __p.__cntrl_;
        }

        template<class _Up> bool owner_before(weak_ptr<_Up> const& __p) const noexcept
        {
            return __cntrl_ < __p.__cntrl_;
        }

        bool __owner_equivalent(const shared_ptr& __p) const { return __cntrl_ == __p.__cntrl_; }

        typename std::add_lvalue_reference<element_type>::type operator[](ptrdiff_t __i) const
        {
            static_assert(std::is_array<_Tp>::value,
                          "std::shared_ptr<T>::operator[] is only valid when T is an array type.");
            return __ptr_[__i];
        }

        template<class _Dp> _Dp* __get_deleter() const noexcept
        {
            return static_cast<_Dp*>(__cntrl_ ? const_cast<void*>(__cntrl_->__get_deleter(typeid(_Dp))) : nullptr);
        }

        template<class _Yp, class _CntrlBlk>
        static shared_ptr<_Tp> __create_with_control_block(_Yp* __p, _CntrlBlk* __cntrl) noexcept
        {
            shared_ptr<_Tp> __r;
            __r.__ptr_ = __p;
            __r.__cntrl_ = __cntrl;
            __r.__enable_weak_this(__r.__ptr_, __r.__ptr_);
            return __r;
        }

    private:
        template<class _Yp, bool = std::is_function<_Yp>::value> struct __shared_ptr_default_allocator
        {
            typedef std::allocator<_Yp> type;
        };

        template<class _Yp> struct __shared_ptr_default_allocator<_Yp, true>
        {
            typedef std::allocator<__shared_ptr_dummy_rebind_allocator_type> type;
        };

        template<
            class _Yp, class _OrigPtr,
            class = typename std::enable_if<std::is_convertible<_OrigPtr*, const enable_shared_from_this<_Yp>*>::value>::type>
        void __enable_weak_this(const enable_shared_from_this<_Yp>* __e, _OrigPtr* __ptr) noexcept
        {
            typedef typename std::remove_cv<_Yp>::type _RawYp;
            if (__e && __e->__weak_this_.expired())
            {
                __e->__weak_this_
                    = shared_ptr<_RawYp>(*this, const_cast<_RawYp*>(static_cast<const _Yp*>(__ptr)));
            }
        }

        void __enable_weak_this(...) noexcept { }

        template<class, class _Yp> struct __shared_ptr_default_delete : std::default_delete<_Yp>
        {
        };

        template<class _Yp, class _Un, size_t _Sz>
        struct __shared_ptr_default_delete<_Yp[_Sz], _Un> : std::default_delete<_Yp[]>
        {
        };

        template<class _Yp, class _Un> struct __shared_ptr_default_delete<_Yp[], _Un> : std::default_delete<_Yp[]>
        {
        };

        template<class _Up> friend class shared_ptr;
        template<class _Up> friend class weak_ptr;
    };

    template<class _Tp> shared_ptr(weak_ptr<_Tp>) -> shared_ptr<_Tp>;
    // template<class _Tp, class _Dp> shared_ptr(unique_ptr<_Tp, _Dp>) -> shared_ptr<_Tp>;

    //
    // std::allocate_shared and std::make_shared
    //
    template<class _Tp, class _Alloc, class... _Args>
    shared_ptr<_Tp> allocate_shared(const _Alloc& __a, _Args&&... __args)
    {
        using _ControlBlock = __shared_ptr_emplace<_Tp, _Alloc>;
        using _ControlBlockAllocator = typename __allocator_traits_rebind<_Alloc, _ControlBlock>::type;
        __allocation_guard<_ControlBlockAllocator> __guard(__a, 1);
        ::new ((void*)std::addressof(*__guard.__get())) _ControlBlock(__a, std::forward<_Args>(__args)...);
        auto __control_block = __guard.__release_ptr();
        return shared_ptr<_Tp>::__create_with_control_block((*__control_block).__get_elem(),
                                                                   std::addressof(*__control_block));
    }

    template<class _Tp, class... _Args> shared_ptr<_Tp> make_shared(_Args&&... __args)
    {
        auto tmp = rpc::allocate_shared<_Tp>(std::allocator<_Tp>(), std::forward<_Args>(__args)...);
        LOG(typeid(tmp).raw_name(),100);
        LOG("make_shared",100);
        LOG(std::to_string((uint64_t)tmp.get()).data(),100);
        return tmp;
    }

    template<class _Tp, class _Up>
    inline bool operator==(const shared_ptr<_Tp>& __x, const shared_ptr<_Up>& __y) noexcept
    {
        return __x.get() == __y.get();
    }

    template<class _Tp, class _Up>
    inline bool operator!=(const shared_ptr<_Tp>& __x, const shared_ptr<_Up>& __y) noexcept
    {
        return !(__x == __y);
    }

    template<class _Tp, class _Up>
    inline bool operator<(const shared_ptr<_Tp>& __x, const shared_ptr<_Up>& __y) noexcept
    {
        return __x.get( ) < __y.get();
    }

    template<class _Tp, class _Up>
    inline bool operator>(const shared_ptr<_Tp>& __x, const shared_ptr<_Up>& __y) noexcept
    {
        return __y < __x;
    }

    template<class _Tp, class _Up>
    inline bool operator<=(const shared_ptr<_Tp>& __x, const shared_ptr<_Up>& __y) noexcept
    {
        return !(__y < __x);
    }

    template<class _Tp, class _Up>
    inline bool operator>=(const shared_ptr<_Tp>& __x, const shared_ptr<_Up>& __y) noexcept
    {
        return !(__x < __y);
    }

    template<class _Tp> inline bool operator==(const shared_ptr<_Tp>& __x, std::nullptr_t) noexcept
    {
        return !__x;
    }

    template<class _Tp> inline bool operator==(std::nullptr_t, const shared_ptr<_Tp>& __x) noexcept
    {
        return !__x;
    }

    template<class _Tp> inline bool operator!=(const shared_ptr<_Tp>& __x, std::nullptr_t) noexcept
    {
        return static_cast<bool>(__x);
    }

    template<class _Tp> inline bool operator!=(std::nullptr_t, const shared_ptr<_Tp>& __x) noexcept
    {
        return static_cast<bool>(__x);
    }

    template<class _Tp> inline bool operator<(const shared_ptr<_Tp>& __x, std::nullptr_t) noexcept
    {
        return std::less<_Tp*>()(__x.get(), nullptr);
    }

    template<class _Tp> inline bool operator<(std::nullptr_t, const shared_ptr<_Tp>& __x) noexcept
    {
        return std::less<_Tp*>()(nullptr, __x.get());
    }

    template<class _Tp> inline bool operator>(const shared_ptr<_Tp>& __x, std::nullptr_t) noexcept
    {
        return nullptr < __x;
    }

    template<class _Tp> inline bool operator>(std::nullptr_t, const shared_ptr<_Tp>& __x) noexcept
    {
        return __x < nullptr;
    }

    template<class _Tp> inline bool operator<=(const shared_ptr<_Tp>& __x, std::nullptr_t) noexcept
    {
        return !(nullptr < __x);
    }

    template<class _Tp> inline bool operator<=(std::nullptr_t, const shared_ptr<_Tp>& __x) noexcept
    {
        return !(__x < nullptr);
    }

    template<class _Tp> inline bool operator>=(const shared_ptr<_Tp>& __x, std::nullptr_t) noexcept
    {
        return !(__x < nullptr);
    }

    template<class _Tp> inline bool operator>=(std::nullptr_t, const shared_ptr<_Tp>& __x) noexcept
    {
        return !(nullptr < __x);
    }

    template<class _Tp> inline void swap(shared_ptr<_Tp>& __x, shared_ptr<_Tp>& __y) noexcept
    {
        __x.swap(__y);
    }

    template<class _Tp, class _Up>
    inline shared_ptr<_Tp> static_pointer_cast(const shared_ptr<_Up>& __r) noexcept
    {
        return shared_ptr<_Tp>(__r, static_cast<typename shared_ptr<_Tp>::element_type*>(__r.get()));
    }

    template<class _Tp, class _Up> shared_ptr<_Tp> const_pointer_cast(const shared_ptr<_Up>& __r) noexcept
    {
        typedef typename shared_ptr<_Tp>::element_type _RTp;
        return shared_ptr<_Tp>(__r, const_cast<_RTp*>(__r.get()));
    }

    template<class _Tp, class _Up> shared_ptr<_Tp> reinterpret_pointer_cast(const shared_ptr<_Up>& __r)
    noexcept
    {
        return shared_ptr<_Tp>(__r, reinterpret_cast<typename shared_ptr<_Tp>::element_type*>(__r.get()));
    }

    template<class _Dp, class _Tp> inline _Dp* get_deleter(const shared_ptr<_Tp>& __p) noexcept
    {
        return __p.template __get_deleter<_Dp>();
    }

    template<class _Tp> class weak_ptr
    {
    public:
        typedef std::remove_extent_t<_Tp> element_type;

    private:
        element_type* __ptr_;
        __shared_weak_count* __cntrl_;

    public:
        constexpr weak_ptr() noexcept;
        template<class _Yp>
        weak_ptr(shared_ptr<_Yp> const& __r,
                        typename std::enable_if<__compatible_with<_Yp, _Tp>::value, int>::type = 0) noexcept;

        weak_ptr(weak_ptr const& __r) noexcept;
        template<class _Yp>
        weak_ptr(weak_ptr<_Yp> const& __r,
                        typename std::enable_if<__compatible_with<_Yp, _Tp>::value, int>::type = 0) noexcept;

        weak_ptr(weak_ptr&& __r) noexcept;
        template<class _Yp>
        weak_ptr(weak_ptr<_Yp>&& __r,
                        typename std::enable_if<__compatible_with<_Yp, _Tp>::value, int>::type = 0) noexcept;
        ~weak_ptr();

        weak_ptr& operator=(weak_ptr const& __r) noexcept;
        template<class _Yp>
        typename std::enable_if<__compatible_with<_Yp, _Tp>::value, weak_ptr&>::type
        operator=(weak_ptr<_Yp> const& __r) noexcept;

        weak_ptr& operator=(weak_ptr&& __r) noexcept;
        template<class _Yp>
        typename std::enable_if<__compatible_with<_Yp, _Tp>::value, weak_ptr&>::type
        operator=(weak_ptr<_Yp>&& __r) noexcept;

        template<class _Yp>
        typename std::enable_if<__compatible_with<_Yp, _Tp>::value, weak_ptr&>::type
        operator=(shared_ptr<_Yp> const& __r) noexcept;

        void swap(weak_ptr& __r) noexcept;

        void reset() noexcept;

        long use_count() const noexcept { return __cntrl_ ? __cntrl_->use_count() : 0; }

        bool expired() const noexcept { return __cntrl_ == nullptr || __cntrl_->use_count() == 0; }
        shared_ptr<_Tp> lock() const noexcept;
        template<class _Up> bool owner_before(const shared_ptr<_Up>& __r) const noexcept
        {
            return __cntrl_ < __r.__cntrl_;
        }
        template<class _Up> bool owner_before(const weak_ptr<_Up>& __r) const noexcept
        {
            return __cntrl_ < __r.__cntrl_;
        }

        template<class _Up> friend class weak_ptr;
        template<class _Up> friend class shared_ptr;
    };

    template<class _Tp> weak_ptr(shared_ptr<_Tp>) -> weak_ptr<_Tp>;

    template<class _Tp>
    inline constexpr weak_ptr<_Tp>::weak_ptr() noexcept
        : __ptr_(nullptr)
        , __cntrl_(nullptr)
    {
    }

    template<class _Tp>
    inline weak_ptr<_Tp>::weak_ptr(weak_ptr const& __r) noexcept
        : __ptr_(__r.__ptr_)
        , __cntrl_(__r.__cntrl_)
    {
        if (__cntrl_)
            __cntrl_->__add_weak();
    }

    template<class _Tp>
    template<class _Yp>
    inline weak_ptr<_Tp>::weak_ptr(
        shared_ptr<_Yp> const& __r,
        typename std::enable_if<__compatible_with<_Yp, _Tp>::value, int>::type) noexcept
        : __ptr_(__r.__ptr_)
        , __cntrl_(__r.__cntrl_)
    {
        if (__cntrl_)
            __cntrl_->__add_weak();
    }

    template<class _Tp>
    template<class _Yp>
    inline weak_ptr<_Tp>::weak_ptr(
        weak_ptr<_Yp> const& __r,
        typename std::enable_if<__compatible_with<_Yp, _Tp>::value, int>::type) noexcept
        : __ptr_(__r.__ptr_)
        , __cntrl_(__r.__cntrl_)
    {
        if (__cntrl_)
            __cntrl_->__add_weak();
    }

    template<class _Tp>
    inline weak_ptr<_Tp>::weak_ptr(weak_ptr&& __r) noexcept
        : __ptr_(__r.__ptr_)
        , __cntrl_(__r.__cntrl_)
    {
        __r.__ptr_ = nullptr;
        __r.__cntrl_ = nullptr;
    }

    template<class _Tp>
    template<class _Yp>
    inline weak_ptr<_Tp>::weak_ptr(
        weak_ptr<_Yp>&& __r, typename std::enable_if<__compatible_with<_Yp, _Tp>::value, int>::type) noexcept
        : __ptr_(__r.__ptr_)
        , __cntrl_(__r.__cntrl_)
    {
        __r.__ptr_ = nullptr;
        __r.__cntrl_ = nullptr;
    }

    template<class _Tp> weak_ptr<_Tp>::~weak_ptr()
    {
        if (__cntrl_)
            __cntrl_->__release_weak();
    }

    template<class _Tp>
    inline weak_ptr<_Tp>& weak_ptr<_Tp>::operator=(weak_ptr const& __r) noexcept
    {
        weak_ptr(__r).swap(*this);
        return *this;
    }

    template<class _Tp>
    template<class _Yp>
    inline typename std::enable_if<__compatible_with<_Yp, _Tp>::value, weak_ptr<_Tp>&>::type
    weak_ptr<_Tp>::operator=(weak_ptr<_Yp> const& __r) noexcept
    {
        weak_ptr(__r).swap(*this);
        return *this;
    }

    template<class _Tp> inline weak_ptr<_Tp>& weak_ptr<_Tp>::operator=(weak_ptr&& __r) noexcept
    {
        weak_ptr(std::move(__r)).swap(*this);
        return *this;
    }

    template<class _Tp>
    template<class _Yp>
    inline typename std::enable_if<__compatible_with<_Yp, _Tp>::value, weak_ptr<_Tp>&>::type
    weak_ptr<_Tp>::operator=(weak_ptr<_Yp>&& __r) noexcept
    {
        weak_ptr(std::move(__r)).swap(*this);
        return *this;
    }

    template<class _Tp>
    template<class _Yp>
    inline typename std::enable_if<__compatible_with<_Yp, _Tp>::value, weak_ptr<_Tp>&>::type
    weak_ptr<_Tp>::operator=(shared_ptr<_Yp> const& __r) noexcept
    {
        weak_ptr(__r).swap(*this);
        return *this;
    }

    template<class _Tp> inline void weak_ptr<_Tp>::swap(weak_ptr& __r) noexcept
    {
        std::swap(__ptr_, __r.__ptr_);
        std::swap(__cntrl_, __r.__cntrl_);
    }

    template<class _Tp> inline void swap(weak_ptr<_Tp>& __x, weak_ptr<_Tp>& __y) noexcept
    {
        __x.swap(__y);
    }

    template<class _Tp> inline void weak_ptr<_Tp>::reset() noexcept { weak_ptr().swap(*this); }

    template<class _Tp> shared_ptr<_Tp> weak_ptr<_Tp>::lock() const noexcept
    {
        shared_ptr<_Tp> __r;
        __r.__cntrl_ = __cntrl_ ? __cntrl_->lock() : __cntrl_;
        if (__r.__cntrl_)
            __r.__ptr_ = __ptr_;
        return __r;
    }

    template<class _Tp = void> struct owner_less;

    template<class _Tp> struct owner_less<shared_ptr<_Tp>>
    {

        bool operator()(shared_ptr<_Tp> const& __x, shared_ptr<_Tp> const& __y) const noexcept
        {
            return __x.owner_before(__y);
        }

        bool operator()(shared_ptr<_Tp> const& __x, weak_ptr<_Tp> const& __y) const noexcept
        {
            return __x.owner_before(__y);
        }

        bool operator()(weak_ptr<_Tp> const& __x, shared_ptr<_Tp> const& __y) const noexcept
        {
            return __x.owner_before(__y);
        }
    };

    template<class _Tp> struct owner_less<weak_ptr<_Tp>>
    {

        bool operator()(weak_ptr<_Tp> const& __x, weak_ptr<_Tp> const& __y) const noexcept
        {
            return __x.owner_before(__y);
        }

        bool operator()(shared_ptr<_Tp> const& __x, weak_ptr<_Tp> const& __y) const noexcept
        {
            return __x.owner_before(__y);
        }

        bool operator()(weak_ptr<_Tp> const& __x, shared_ptr<_Tp> const& __y) const noexcept
        {
            return __x.owner_before(__y);
        }
    };

    template<> struct owner_less<void>
    {
        template<class _Tp, class _Up>
        bool operator()(shared_ptr<_Tp> const& __x, shared_ptr<_Up> const& __y) const noexcept
        {
            return __x.owner_before(__y);
        }
        template<class _Tp, class _Up>
        bool operator()(shared_ptr<_Tp> const& __x, weak_ptr<_Up> const& __y) const noexcept
        {
            return __x.owner_before(__y);
        }
        template<class _Tp, class _Up>
        bool operator()(weak_ptr<_Tp> const& __x, shared_ptr<_Up> const& __y) const noexcept
        {
            return __x.owner_before(__y);
        }
        template<class _Tp, class _Up>
        bool operator()(weak_ptr<_Tp> const& __x, weak_ptr<_Up> const& __y) const noexcept
        {
            return __x.owner_before(__y);
        }
        typedef void is_transparent;
    };

    template<class _Tp> class enable_shared_from_this
    {
        mutable weak_ptr<_Tp> __weak_this_;

    protected:
        constexpr enable_shared_from_this() noexcept { }

        enable_shared_from_this(enable_shared_from_this const&) noexcept { }

        enable_shared_from_this& operator=(enable_shared_from_this const&) noexcept { return *this; }

        ~enable_shared_from_this() { }

    public:
        shared_ptr<_Tp> shared_from_this() { return shared_ptr<_Tp>(__weak_this_); }

        shared_ptr<_Tp const> shared_from_this() const { return shared_ptr<const _Tp>(__weak_this_); }

        weak_ptr<_Tp> weak_from_this() noexcept { return __weak_this_; }

        weak_ptr<const _Tp> weak_from_this() const noexcept { return __weak_this_; }

        template<class _Up> friend class shared_ptr;
    };

    template<class _Tp> struct hash;

    template<class _Tp> struct hash<shared_ptr<_Tp>>
    {
        size_t operator()(const shared_ptr<_Tp>& __ptr) const noexcept
        {
            return hash<typename shared_ptr<_Tp>::element_type*>()(__ptr.get());
        }
    };

    //    template<class _CharT, class _Traits, class _Yp>
    //    inline basic_ostream<_CharT, _Traits>& operator<<(basic_ostream<_CharT, _Traits>& __os, shared_ptr<_Yp>
    //    const& __p);

    class __sp_mut
    {
        void* __lx;

    public:
        void lock() noexcept;
        void unlock() noexcept;

    private:
        constexpr __sp_mut(void*) noexcept;
        __sp_mut(const __sp_mut&);
        __sp_mut& operator=(const __sp_mut&);

        friend __sp_mut& __get_sp_mut(const void*);
    };

    __sp_mut& __get_sp_mut(const void*);

    template<class _Tp> inline bool atomic_is_lock_free(const shared_ptr<_Tp>*) { return false; }

    template<class _Tp> shared_ptr<_Tp> atomic_load(const shared_ptr<_Tp>* __p)
    {
        __sp_mut& __m = __get_sp_mut(__p);
        __m.lock();
        shared_ptr<_Tp> __q = *__p;
        __m.unlock();
        return __q;
    }

    template<class _Tp>
    inline shared_ptr<_Tp> atomic_load_explicit(const shared_ptr<_Tp>* __p, std::memory_order)
    {
        return atomic_load(__p);
    }

    template<class _Tp> void atomic_store(shared_ptr<_Tp>* __p, shared_ptr<_Tp> __r)
    {
        __sp_mut& __m = __get_sp_mut(__p);
        __m.lock();
        __p->swap(__r);
        __m.unlock();
    }

    template<class _Tp>
    inline void atomic_store_explicit(shared_ptr<_Tp>* __p, shared_ptr<_Tp> __r, std::memory_order)
    {
        atomic_store(__p, __r);
    }

    template<class _Tp> shared_ptr<_Tp> atomic_exchange(shared_ptr<_Tp>* __p, shared_ptr<_Tp> __r)
    {
        __sp_mut& __m = __get_sp_mut(__p);
        __m.lock();
        __p->swap(__r);
        __m.unlock();
        return __r;
    }

    template<class _Tp>
    inline shared_ptr<_Tp> atomic_exchange_explicit(shared_ptr<_Tp>* __p, shared_ptr<_Tp> __r,
                                                           std::memory_order)
    {
        return atomic_exchange(__p, __r);
    }

    template<class _Tp>
    bool atomic_compare_exchange_strong(shared_ptr<_Tp>* __p, shared_ptr<_Tp>* __v,
                                        shared_ptr<_Tp> __w)
    {
        shared_ptr<_Tp> __temp;
        __sp_mut& __m = __get_sp_mut(__p);
        __m.lock();
        if (__p->__owner_equivalent(*__v))
        {
            std::swap(__temp, *__p);
            *__p = __w;
            __m.unlock();
            return true;
        }
        std::swap(__temp, *__v);
        *__v = *__p;
        __m.unlock();
        return false;
    }

    template<class _Tp>
    inline bool atomic_compare_exchange_weak(shared_ptr<_Tp>* __p, shared_ptr<_Tp>* __v,
                                             shared_ptr<_Tp> __w)
    {
        return atomic_compare_exchange_strong(__p, __v, __w);
    }

    template<class _Tp>
    inline bool atomic_compare_exchange_strong_explicit(shared_ptr<_Tp>* __p, shared_ptr<_Tp>* __v,
                                                        shared_ptr<_Tp> __w, std::memory_order,
                                                        std::memory_order)
    {
        return atomic_compare_exchange_strong(__p, __v, __w);
    }

    template<class _Tp>
    inline bool atomic_compare_exchange_weak_explicit(shared_ptr<_Tp>* __p, shared_ptr<_Tp>* __v,
                                                      shared_ptr<_Tp> __w, std::memory_order, std::memory_order)
    {
        return atomic_compare_exchange_weak(__p, __v, __w);
    }

    template<class T1, class T2>
    [[nodiscard]] inline shared_ptr<T1> dynamic_pointer_cast(const shared_ptr<T2>& from) noexcept
    {
        auto* ptr = const_cast<T1*>(static_cast<const T1*>(from->query_interface(T1::id)));
        if (ptr)
            return shared_ptr<T1>(from, ptr);
        auto proxy_ = from->query_proxy_base();
        if (!proxy_)
        {
            return shared_ptr<T1>();
        }
        auto ob = proxy_->get_object_proxy();
        shared_ptr<T1> ret;
        ob->template query_interface<T1>(ret);
        return ret;
    }
}