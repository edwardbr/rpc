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

namespace rpc_cpp
{

    template<class T, class _Dx = std::default_delete<T>> class unique_ptr;

    template<class T> class shared_ptr;

    template<class T> class weak_ptr;

    template<class T> class shared_ptr : public std::shared_ptr<T>
    {
    public:
        using typename std::shared_ptr<T>::element_type;

        using weak_type = std::weak_ptr<T>;

        constexpr shared_ptr() noexcept
            : std::shared_ptr<T>()
        {
        }

        constexpr shared_ptr(std::nullptr_t) noexcept
            : std::shared_ptr<T>(nullptr)
        {
        } // construct empty shared_ptr

        constexpr shared_ptr(std::shared_ptr<T> val) noexcept
            : std::shared_ptr<T>(val)
        {
        } // construct empty shared_ptr

        template<class _Ux>
        explicit shared_ptr(_Ux* _Px)
            : std::shared_ptr<T>(_Px)
        {
        }

        template<class _Ux, class _Dx>
        shared_ptr(_Ux* _Px, _Dx _Dt)
            : std::shared_ptr<T>(_Px, _Dt)
        {
        }

        template<class _Ux, class _Dx, class _Alloc>
        shared_ptr(_Ux* _Px, _Dx _Dt, _Alloc _Ax)
            : std::shared_ptr<T>(_Px, _Dt, _Ax)
        {
        }

        template<class _Dx>
        shared_ptr(std::nullptr_t, _Dx _Dt)
            : std::shared_ptr<T>(nullptr, _Dt)
        {
        }

        template<class _Dx, class _Alloc>
        shared_ptr(std::nullptr_t, _Dx _Dt, _Alloc _Ax)
            : std::shared_ptr<T>(nullptr, _Dt, _Ax)
        {
        }

        template<class T2>
        shared_ptr(const shared_ptr<T2>& _Right, std::shared_ptr<T>::element_type* _Px) noexcept
            : std::shared_ptr<T>(_Right, _Px)
        {
        }

        template<class T2>
        shared_ptr(shared_ptr<T2>&& _Right, element_type* _Px) noexcept
            : std::shared_ptr<T>(std::move(_Right), _Px)
        {
        }

        shared_ptr(const rpc_cpp::shared_ptr<T>& _Other) noexcept
            : std::shared_ptr<T>(_Other)
        {
            *this = _Other;
        }

        template<class T2>
        shared_ptr(const shared_ptr<T2>& _Other) noexcept
            : std::shared_ptr<T>(_Other)
        {
        }

        shared_ptr(shared_ptr&& _Right) noexcept
            : std::shared_ptr<T>(std::move(_Right))
        {
        }

        template<class T2>
        shared_ptr(shared_ptr<T2>&& _Right) noexcept
            : std::shared_ptr<T>(std::move(_Right))
        {
        }

        template<class T2>
        explicit shared_ptr(const weak_ptr<T2>& _Other)
            : std::shared_ptr<T>(_Other)
        {
        }

        template<class _Ux, class _Dx>
        shared_ptr(unique_ptr<_Ux, _Dx>&& _Other)
            : std::shared_ptr<T>(std::move(_Other))
        {
        }

        shared_ptr& operator=(const shared_ptr& _Right) noexcept
        {
            std::shared_ptr<T>* psp = this;
            *psp = _Right;
            return *this;
        }

        template<class T2> shared_ptr& operator=(const shared_ptr<T2>& _Right) noexcept
        {
            std::shared_ptr<T>* psp = this;
            *psp = _Right;
            return *this;
        }

        shared_ptr& operator=(shared_ptr&& _Right) noexcept
        {
            std::shared_ptr<T>* psp = this;
            *psp = std::move(_Right);
            return *this;
        }

        template<class T2> shared_ptr& operator=(shared_ptr<T2>&& _Right) noexcept
        {
            std::shared_ptr<T>* psp = this;
            *psp = std::move(_Right);
            return *this;
        }

        template<class _Ux, class _Dx> shared_ptr& operator=(unique_ptr<_Ux, _Dx>&& _Right)
        {
            std::shared_ptr<T>* psp = this;
            *psp = std::move(_Right);
            return *this;
        }
    };

    template<class T> class weak_ptr : public std::weak_ptr<T>
    {
    public:
        constexpr weak_ptr() noexcept
            : std::weak_ptr<T>()
        {
        }

        weak_ptr(const weak_ptr& _Other) noexcept
            : std::weak_ptr<T>(_Other)
        {
        }
        weak_ptr(const std::weak_ptr<T>& _Other) noexcept
            : std::weak_ptr<T>(_Other)
        {
        }
        weak_ptr(std::weak_ptr<T>&& _Other) noexcept
            : std::weak_ptr<T>(std::move(_Other))
        {
        }

        template<class T2>
        weak_ptr(const shared_ptr<T2>& _Other) noexcept
            : std::weak_ptr<T>(_Other)
        {
        }

        template<class T2>
        weak_ptr(const weak_ptr<T2>& _Other) noexcept
            : std::weak_ptr<T>(_Other)
        {
        }

        weak_ptr(weak_ptr&& _Other) noexcept
            : std::weak_ptr<T>(std::move(_Other))
        {
        }

        template<class T2>
        weak_ptr(weak_ptr<T2>&& _Other) noexcept
            : std::weak_ptr<T>(std::move(_Other))
        {
        }

        weak_ptr& operator=(const weak_ptr& _Right) noexcept
        {
            std::weak_ptr<T>* psp = this;
            *psp = _Right;

            return *this;
        }

        template<class T2> weak_ptr& operator=(const weak_ptr<T2>& _Right) noexcept
        {
            std::weak_ptr<T>* psp = this;
            return *psp = _Right;
        }

        weak_ptr& operator=(weak_ptr&& _Right) noexcept
        {
            std::weak_ptr<T>* psp = this;
            return *psp = std::move(_Right);
        }
        template<class T2> weak_ptr& operator=(weak_ptr<T2>&& _Right) noexcept
        {
            std::weak_ptr<T>* psp = this;
            return *psp = std::move(_Right);
        }

        template<class T2> weak_ptr& operator=(const shared_ptr<T2>& _Right) noexcept
        {
            std::weak_ptr<T>* psp = this;
            *psp = _Right;
            return *this;
        }

        void reset() noexcept
        { // release resource, convert to null weak_ptr object
            weak_ptr {}.swap(*this);
        }

        [[nodiscard]] shared_ptr<T> lock() const noexcept
        { // convert to shared_ptr
            const std::weak_ptr<T>* psp = this;
            return shared_ptr<T>(psp->lock());
        }
    };

    template<class T> class enable_shared_from_this
    { // provide member functions that create shared_ptr to this
    public:
        using _Esft_type = enable_shared_from_this;

        [[nodiscard]] shared_ptr<T> shared_from_this() { return shared_ptr<T>(_Wptr); }

        [[nodiscard]] shared_ptr<const T> shared_from_this() const { return shared_ptr<const T>(_Wptr); }

        [[nodiscard]] weak_ptr<T> weak_from_this() noexcept { return _Wptr; }

        [[nodiscard]] weak_ptr<const T> weak_from_this() const noexcept { return _Wptr; }

    protected:
        constexpr enable_shared_from_this() noexcept
            : _Wptr()
        {
        }

        enable_shared_from_this(const enable_shared_from_this&) noexcept
            : _Wptr()
        {
            // construct (must value-initialize _Wptr)
        }

        enable_shared_from_this& operator=(const enable_shared_from_this&) noexcept
        { // assign (must not change _Wptr)
            return *this;
        }

        ~enable_shared_from_this() = default;

    private:
        template<class _Yty> friend class shared_ptr;
        mutable weak_ptr<T> _Wptr;
    };

    template<class T1, class T2>[[nodiscard]] shared_ptr<T1> static_pointer_cast(const shared_ptr<T2>& _Other) noexcept
    {
        // static_cast for shared_ptr that properly respects the reference count control block
        const auto _Ptr = static_cast<typename shared_ptr<T1>::element_type*>(_Other.get());
        return shared_ptr<T1>(_Other, _Ptr);
    }

    template<class T1, class T2>[[nodiscard]] shared_ptr<T1> static_pointer_cast(shared_ptr<T2>&& _Other) noexcept
    {
        // static_cast for shared_ptr that properly respects the reference count control block
        const auto _Ptr = static_cast<typename shared_ptr<T1>::element_type*>(_Other.get());
        return shared_ptr<T1>(std::move(_Other), _Ptr);
    }

    template<class T1, class T2>[[nodiscard]] shared_ptr<T1> const_pointer_cast(const shared_ptr<T2>& _Other) noexcept
    {
        // const_cast for shared_ptr that properly respects the reference count control block
        const auto _Ptr = const_cast<typename shared_ptr<T1>::element_type*>(_Other.get());
        return shared_ptr<T1>(_Other, _Ptr);
    }

    template<class T1, class T2>[[nodiscard]] shared_ptr<T1> const_pointer_cast(shared_ptr<T2>&& _Other) noexcept
    {
        // const_cast for shared_ptr that properly respects the reference count control block
        const auto _Ptr = const_cast<typename shared_ptr<T1>::element_type*>(_Other.get());
        return shared_ptr<T1>(std::move(_Other), _Ptr);
    }

    template<class T1, class T2>
    [[nodiscard]] shared_ptr<T1> reinterpret_pointer_cast(const shared_ptr<T2>& _Other) noexcept
    {
        // reinterpret_cast for shared_ptr that properly respects the reference count control block
        const auto _Ptr = reinterpret_cast<typename shared_ptr<T1>::element_type*>(_Other.get());
        return shared_ptr<T1>(_Other, _Ptr);
    }

    template<class T1, class T2>[[nodiscard]] shared_ptr<T1> reinterpret_pointer_cast(shared_ptr<T2>&& _Other) noexcept
    {
        // reinterpret_cast for shared_ptr that properly respects the reference count control block
        const auto _Ptr = reinterpret_cast<typename shared_ptr<T1>::element_type*>(_Other.get());
        return shared_ptr<T1>(std::move(_Other), _Ptr);
    }

    template<class T1, class T2> inline shared_ptr<T1> dynamic_pointer_cast(const shared_ptr<T2>& from) noexcept
    {
        auto* ptr = dynamic_cast<shared_ptr<T1>::element_type*>(from.get());
        if (ptr)
            return shared_ptr<T1>(from, ptr);
        i_proxy_impl* proxy = dynamic_cast<i_proxy_impl*>(from.get());
        if (!proxy)
        {
            return shared_ptr<T1>();
        }
        auto ob = proxy->get_object_proxy();
        shared_ptr<T1> ret;
        ob->query_interface<T1>(ret);
        return ret;
    }
}
