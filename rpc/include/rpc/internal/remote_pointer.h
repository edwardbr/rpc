/*
 *   Copyright (c) 2025 Edward Boggis-Rolfe
 *   All rights reserved.
 */

#pragma once

#include <atomic>
#include <utility>     // For std::swap, std::move, std::forward
#include <stdexcept>   // For std::bad_weak_ptr, std::invalid_argument
#include <type_traits> // For std::is_base_of_v, std::is_convertible_v, std::remove_extent_t, etc.
#include <functional>  // For std::hash
#include <cstddef>     // For std::nullptr_t, std::size_t

#ifdef TEST_STL_COMPLIANCE
namespace std
{
    // Forward declare/implement missing std library components since we can't include <memory>
    template<typename T>
    struct default_delete {
        constexpr default_delete() noexcept = default;

        template<typename U, typename = std::enable_if_t<std::is_convertible_v<U*, T*>>>
        default_delete(const default_delete<U>&) noexcept {}

        void operator()(T* ptr) const noexcept {
            static_assert(sizeof(T) > 0, "can't delete an incomplete type");
            delete ptr;
        }
    };

    template<typename T>
    struct default_delete<T[]> {
        constexpr default_delete() noexcept = default;

        template<typename U, typename = std::enable_if_t<std::is_convertible_v<U(*)[], T(*)[]>>>
        default_delete(const default_delete<U[]>&) noexcept {}

        template<typename U, typename = std::enable_if_t<std::is_convertible_v<U(*)[], T(*)[]>>>
        void operator()(U* ptr) const noexcept {
            static_assert(sizeof(U) > 0, "can't delete an incomplete type");
            delete[] ptr;
        }
    };


    class bad_weak_ptr : public std::exception {
    public:
        const char* what() const noexcept override {
            return "bad_weak_ptr";
        }
    };

    // Minimal unique_ptr implementation for tests
    template<typename T, typename Deleter = default_delete<T>>
    class unique_ptr {
        T* ptr_;
        Deleter deleter_;

    public:
        using pointer = T*;
        using element_type = T;
        using deleter_type = Deleter;

        constexpr unique_ptr() noexcept : ptr_(nullptr) {}
        constexpr unique_ptr(std::nullptr_t) noexcept : ptr_(nullptr) {}
        explicit unique_ptr(pointer p) noexcept : ptr_(p) {}

        unique_ptr(const unique_ptr&) = delete;
        unique_ptr& operator=(const unique_ptr&) = delete;

        unique_ptr(unique_ptr&& u) noexcept : ptr_(u.release()), deleter_(std::move(u.deleter_)) {}
        unique_ptr& operator=(unique_ptr&& u) noexcept {
            if (this != &u) {
                reset(u.release());
                deleter_ = std::move(u.deleter_);
            }
            return *this;
        }

        ~unique_ptr() { reset(); }

        pointer release() noexcept {
            pointer p = ptr_;
            ptr_ = nullptr;
            return p;
        }

        void reset(pointer ptr = pointer()) noexcept {
            pointer old = ptr_;
            ptr_ = ptr;
            if (old != nullptr)
                deleter_(old);
        }

        pointer get() const noexcept { return ptr_; }
        deleter_type& get_deleter() noexcept { return deleter_; }
        const deleter_type& get_deleter() const noexcept { return deleter_; }

        explicit operator bool() const noexcept { return ptr_ != nullptr; }

        T& operator*() const { return *ptr_; }
        pointer operator->() const noexcept { return ptr_; }
    };

    template<typename T, typename... Args>
    unique_ptr<T> make_unique(Args&&... args) {
        return unique_ptr<T>(new T(std::forward<Args>(args)...));
    }

#else

#include "version.h"
#include "marshaller.h"
#include "member_ptr.h"
#include "coroutine_support.h" // Needed for CORO_TASK macro
namespace rpc
{
    class object_proxy;

#endif
    // Forward declarations for circular dependency resolution
    template<typename T> class shared_ptr;
    template<typename T> class weak_ptr;

    // NAMESPACE_INLINE_BEGIN  // Commented out to simplify

    namespace impl
    {
        struct control_block_base
        {
            std::atomic<long> local_shared_owners{0};
            std::atomic<long> local_weak_owners{1};

        protected:
            void* managed_object_ptr_{nullptr};

        public:
            control_block_base(void* obj_ptr)
                : managed_object_ptr_(obj_ptr)
            {
            }

            control_block_base()
                :managed_object_ptr_(nullptr)
            {
            }

            virtual ~control_block_base() = default;
            virtual void dispose_object_actual() = 0;
            virtual void destroy_self_actual() = 0;

            void* get_managed_object_ptr() const { return managed_object_ptr_; }

            void increment_local_shared() { local_shared_owners.fetch_add(1, std::memory_order_relaxed); }
            void increment_local_weak() { local_weak_owners.fetch_add(1, std::memory_order_relaxed); }

            void decrement_local_shared_and_dispose_if_zero()
            {
                if (local_shared_owners.fetch_sub(1, std::memory_order_acq_rel) == 1)
                {
                    dispose_object_actual();
                    decrement_local_weak_and_destroy_if_zero();
                }
            }

            void decrement_local_weak_and_destroy_if_zero()
            {
                if (local_weak_owners.fetch_sub(1, std::memory_order_acq_rel) == 1)
                {
                    if (local_shared_owners.load(std::memory_order_acquire) == 0)
                    {
                        destroy_self_actual();
                    }
                }
            }
        };

        template<typename T>
        static void* to_void_ptr(T* p) {
            if constexpr (std::is_function_v<T>) {
                return reinterpret_cast<void*>(p);
            } else {
                return static_cast<void*>(const_cast<std::remove_cv_t<T>*>(p));
            }
        }

        template<typename T, typename Deleter, typename Alloc> struct control_block_impl : public control_block_base
        {
            Deleter object_deleter_;
            Alloc control_block_allocator_;
            control_block_impl(T* p, Deleter d, Alloc a)
                : control_block_base(to_void_ptr(p))
                , object_deleter_(std::move(d))
                , control_block_allocator_(std::move(a))
            {
            }

            void dispose_object_actual() override
            {
                if (managed_object_ptr_)
                {
                    T* obj_ptr;
                    if constexpr (std::is_function_v<T>) {
                        obj_ptr = reinterpret_cast<T*>(managed_object_ptr_);
                    } else {
                        obj_ptr = static_cast<T*>(managed_object_ptr_);
                    }
                    object_deleter_(obj_ptr);
                    managed_object_ptr_ = nullptr;
                }
            }
            void destroy_self_actual() override
            {
                using ThisType = control_block_impl<T, Deleter, Alloc>;
                using ReboundAlloc = typename std::allocator_traits<Alloc>::template rebind_alloc<ThisType>;
                ReboundAlloc rebound_alloc(control_block_allocator_);
                std::allocator_traits<ReboundAlloc>::destroy(rebound_alloc, this);
                std::allocator_traits<ReboundAlloc>::deallocate(rebound_alloc, this, 1);
            }
        };

        template<typename T, typename Alloc, typename... Args>
        struct control_block_make_shared : public control_block_base
        {
            Alloc allocator_instance_;
            union
            {
                T object_instance_;
            };

            template<typename... ConcreteArgs>
            control_block_make_shared(Alloc alloc_for_t, ConcreteArgs&&... args_for_t)
                : control_block_base()
                , allocator_instance_(std::move(alloc_for_t))
            {
                ::new (const_cast<void*>(static_cast<const void*>(&object_instance_))) T(std::forward<ConcreteArgs>(args_for_t)...);
                this->managed_object_ptr_ = to_void_ptr(&object_instance_);
            }

            void dispose_object_actual() override
            {
                if (managed_object_ptr_)
                {
                    object_instance_.~T();
                    managed_object_ptr_ = nullptr;
                }
            }

            void destroy_self_actual() override
            {
                if (managed_object_ptr_)
                {
                    object_instance_.~T();
                    managed_object_ptr_ = nullptr;
                }
                using ThisCBType = control_block_make_shared<T, Alloc, Args...>;
                typename std::allocator_traits<Alloc>::template rebind_alloc<ThisCBType> actual_cb_allocator(
                    allocator_instance_);
                std::allocator_traits<decltype(actual_cb_allocator)>::deallocate(actual_cb_allocator, this, 1);
            }
            ~control_block_make_shared() { }
        };
    } // namespace impl

    template<typename T> class shared_ptr
    {
        T* ptr_{nullptr};
        impl::control_block_base* cb_{nullptr};

        void acquire_this() noexcept
        {
            if (cb_)
                cb_->increment_local_shared();
        }

        void release_this() noexcept
        {
            if (cb_)
                cb_->decrement_local_shared_and_dispose_if_zero();
        }

        shared_ptr(impl::control_block_base* cb, T* p)
            : ptr_(p)
            , cb_(cb)
        {
            if (!cb_)
            {
                ptr_ = nullptr;
            }
        }

        enum class for_enable_shared_tag
        {
        };
        shared_ptr(impl::control_block_base* cb, T* p, for_enable_shared_tag)
            : ptr_(p)
            , cb_(cb)
        {
            // Don't call acquire_this() here - this is for enable_shared_from_this internal use
            // The reference count is already correct from the original shared_ptr
        }

        template<typename Y, typename Deleter, typename Alloc> void create_cb_local(Y* p, Deleter d, Alloc cb_alloc)
        {
            if (!p)
            {
                if constexpr (std::is_same_v<Deleter, std::default_delete<Y>>
                              && std::is_same_v<Alloc, std::allocator<char>>)
                {
                    return;
                }
            }

            using ActualCB = impl::control_block_impl<Y, Deleter, Alloc>;
            typename std::allocator_traits<Alloc>::template rebind_alloc<ActualCB> actual_cb_alloc(cb_alloc);
            cb_ = std::allocator_traits<decltype(actual_cb_alloc)>::allocate(actual_cb_alloc, 1);
            new (cb_) ActualCB(p, std::move(d), std::move(cb_alloc));
        }

    public:
        using element_type = std::remove_extent_t<T>;
        using weak_type = weak_ptr<T>;

        constexpr shared_ptr() noexcept = default;
        constexpr shared_ptr(std::nullptr_t) noexcept { }

        template<typename Y, typename = std::enable_if_t<std::is_convertible_v<Y*, T*> ||
                                                       (std::is_array_v<T> && std::is_convertible_v<Y*, std::remove_extent_t<T>*>)>>
        explicit shared_ptr(Y* p)
            : ptr_(p)
            , cb_(nullptr)
        {
            if (ptr_)
            {
                try
                {
                    create_cb_local<Y>(p, std::default_delete<Y>(), std::allocator<char>());
                }
                catch (...)
                {
                    ptr_ = nullptr;
                    cb_ = nullptr;
                    throw;
                }
                acquire_this();
                try_enable_shared_from_this(*this, p);
            }
        }

        template<typename Y, typename Deleter, typename = std::enable_if_t<std::is_convertible_v<Y*, T*>>>
        shared_ptr(Y* p, Deleter d)
            : ptr_(p)
            , cb_(nullptr)
        {
            try
            {
                create_cb_local<Y>(p, std::move(d), std::allocator<char>());
            }
            catch (...)
            {
                ptr_ = nullptr;
                cb_ = nullptr;
                throw;
            }
            if (cb_ || ptr_)
            {
                acquire_this();
                try_enable_shared_from_this(*this, p);
            }
        }

        template<typename Deleter>
        shared_ptr(std::nullptr_t, Deleter d)
            : ptr_(nullptr)
            , cb_(nullptr)
        {
            try
            {
                create_cb_local<T>(nullptr, std::move(d), std::allocator<char>());
            }
            catch (...)
            {
                cb_ = nullptr;
                throw;
            }
            if (cb_)
            {
                acquire_this();
            }
        }

        template<typename Y, typename Deleter, typename Alloc, typename = std::enable_if_t<std::is_convertible_v<Y*, T*>>>
        shared_ptr(Y* p, Deleter d, Alloc cb_alloc)
            : ptr_(p)
            , cb_(nullptr)
        {
            try
            {
                create_cb_local<Y>(p, std::move(d), std::move(cb_alloc));
            }
            catch (...)
            {
                ptr_ = nullptr;
                cb_ = nullptr;
                throw;
            }
            if (cb_ || ptr_)
            {
                acquire_this();
                try_enable_shared_from_this(*this, p);
            }
        }

        template<typename Deleter, typename Alloc>
        shared_ptr(std::nullptr_t, Deleter d, Alloc cb_alloc)
            : ptr_(nullptr)
            , cb_(nullptr)
        {
            try
            {
                create_cb_local<T>(nullptr, std::move(d), std::move(cb_alloc));
            }
            catch (...)
            {
                cb_ = nullptr;
                throw;
            }
            if (cb_)
            {
                acquire_this();
            }
        }

#ifdef TEST_STL_COMPLIANCE
        template<typename Y, typename D, typename = std::enable_if_t<std::is_convertible_v<Y*, T*>>>
        shared_ptr(unique_ptr<Y, D>&& r)
            : ptr_(r.get())
            , cb_(nullptr)
        {
            if (ptr_)
            {
                try
                {
                    create_cb_local<Y>(r.release(), r.get_deleter(), std::allocator<char>());
                }
                catch (...)
                {
                    ptr_ = nullptr;
                    cb_ = nullptr;
                    throw;
                }
                acquire_this();
            }
        }
#endif


        template<typename Y>
        shared_ptr(const shared_ptr<Y>& r, element_type* p_alias) noexcept
            : ptr_(p_alias)
            , cb_(r.internal_get_cb())
        {
            acquire_this();
        }

        template<typename Y>
        shared_ptr(shared_ptr<Y>&& r, element_type* p_alias) noexcept
            : ptr_(p_alias)
            , cb_(r.internal_get_cb())
        {
            r.cb_ = nullptr;
            r.ptr_ = nullptr;
        }

        shared_ptr(const shared_ptr& r) noexcept
            : ptr_(r.ptr_)
            , cb_(r.cb_)
        {
            acquire_this();
        }
        shared_ptr(shared_ptr&& r) noexcept
            : ptr_(r.ptr_)
            , cb_(r.cb_)
        {
            r.cb_ = nullptr;
            r.ptr_ = nullptr;
        }

        template<typename Y, typename = std::enable_if_t<std::is_convertible_v<Y*, T*>>>
        shared_ptr(const shared_ptr<Y>& r) noexcept
            : ptr_(static_cast<T*>(r.internal_get_ptr()))
            , cb_(r.internal_get_cb())
        {
            acquire_this();
        }
        template<typename Y, typename = std::enable_if_t<std::is_convertible_v<Y*, T*>>>
        shared_ptr(shared_ptr<Y>&& r) noexcept
            : ptr_(static_cast<T*>(r.internal_get_ptr()))
            , cb_(r.internal_get_cb())
        {
            r.cb_ = nullptr;
            r.ptr_ = nullptr;
        }

        template<typename Y, typename = std::enable_if_t<std::is_convertible_v<Y*, T*>>>
        explicit shared_ptr(const weak_ptr<Y>& r)
        {
            shared_ptr<Y> temp = r.lock();
            if (!temp)
            {
                throw std::bad_weak_ptr();
            }
            shared_ptr<T> temp_T(std::move(temp));
            this->swap(temp_T);
        }

        ~shared_ptr() { release_this(); }
        shared_ptr& operator=(const shared_ptr& r) noexcept
        {
            if (this != std::addressof(r))
            {
                release_this();
                cb_ = r.cb_;
                ptr_ = r.ptr_;
                acquire_this();
            }
            return *this;
        }
        shared_ptr& operator=(shared_ptr&& r) noexcept
        {
            if (this != std::addressof(r))
            {
                release_this();
                cb_ = r.cb_;
                ptr_ = r.ptr_;
                r.cb_ = nullptr;
                r.ptr_ = nullptr;
            }
            return *this;
        }

        void reset() noexcept { shared_ptr().swap(*this); }
        template<typename Y, typename Deleter = std::default_delete<Y>, typename Alloc = std::allocator<char>>
        void reset(Y* p, Deleter d = Deleter(), Alloc cb_alloc = Alloc())
        {
            shared_ptr(p, std::move(d), std::move(cb_alloc)).swap(*this);
        }

        void swap(shared_ptr& r) noexcept
        {
            std::swap(ptr_, r.ptr_);
            std::swap(cb_, r.cb_);
        }

        T* get() const noexcept { return ptr_; }

        template<typename U = T>
        std::enable_if_t<!std::is_void_v<U> && !std::is_array_v<U>, U&>
        operator*() const noexcept { return *ptr_; }

        template<typename U = T>
        std::enable_if_t<!std::is_void_v<U> && std::is_array_v<U>, std::remove_extent_t<U>&>
        operator*() const noexcept { return *ptr_; }

        T* operator->() const noexcept { return ptr_; }
        long use_count() const noexcept { return cb_ ? cb_->local_shared_owners.load(std::memory_order_relaxed) : 0; }
        explicit operator bool() const noexcept { return ptr_ != nullptr; }

        template<typename Y>
        bool owner_before(const shared_ptr<Y>& other) const noexcept {
            return cb_ < other.internal_get_cb();
        }

        template<typename Y>
        bool owner_before(const weak_ptr<Y>& other) const noexcept {
            return cb_ < other.internal_get_cb();
        }

        impl::control_block_base* internal_get_cb() const { return cb_; }
        T* internal_get_ptr() const { return ptr_; }

        template<typename U> friend class weak_ptr;
        template<typename U>
        friend class shared_ptr; // Allow different template instantiations to access private members
        template<typename U, typename... Args> friend shared_ptr<U> make_shared(Args&&... args);
        template<typename U, typename Alloc, typename... Args> friend shared_ptr<U> allocate_shared(const Alloc& alloc, Args&&... args);
        template<class T1_cast, class T2_cast>
        friend shared_ptr<T1_cast> dynamic_pointer_cast(const shared_ptr<T2_cast>& from) noexcept;
        template<typename U> friend class enable_shared_from_this;
    };

    template<typename T> class weak_ptr
    {
        impl::control_block_base* cb_{nullptr};
        T* ptr_for_lock_{nullptr};

    public:
        using element_type = std::remove_extent_t<T>;
        constexpr weak_ptr() noexcept = default;

        template<typename Y>
        weak_ptr(const shared_ptr<Y>& r) noexcept
            : cb_(r.internal_get_cb())
            , ptr_for_lock_(r.internal_get_ptr())
        {
            static_assert(std::is_convertible<Y*, T*>::value, "Y* must be convertible to T*");
            if (cb_)
                cb_->increment_local_weak();
        }

        weak_ptr(const weak_ptr& r) noexcept
            : cb_(r.cb_)
            , ptr_for_lock_(r.ptr_for_lock_)
        {
            if (cb_)
                cb_->increment_local_weak();
        }
        template<typename Y>
        weak_ptr(const weak_ptr<Y>& r) noexcept
            : cb_(r.cb_)
            , ptr_for_lock_(static_cast<T*>(r.ptr_for_lock_))
        {
            static_assert(std::is_convertible_v<Y*, T*>, "Y* must be convertible to T*");
            if (cb_)
                cb_->increment_local_weak();
        }

        weak_ptr(weak_ptr&& r) noexcept
            : cb_(r.cb_)
            , ptr_for_lock_(r.ptr_for_lock_)
        {
            r.cb_ = nullptr;
            r.ptr_for_lock_ = nullptr;
        }
        template<typename Y>
        weak_ptr(weak_ptr<Y>&& r) noexcept
            : cb_(r.cb_)
            , ptr_for_lock_(static_cast<T*>(r.ptr_for_lock_))
        {
            static_assert(std::is_convertible_v<Y*, T*>, "Y* must be convertible to T*");
            r.cb_ = nullptr;
            r.ptr_for_lock_ = nullptr;
        }

        ~weak_ptr()
        {
            if (cb_)
                cb_->decrement_local_weak_and_destroy_if_zero();
        }

        weak_ptr& operator=(const weak_ptr& r) noexcept
        {
            weak_ptr(r).swap(*this);
            return *this;
        }
        template<typename Y> weak_ptr& operator=(const weak_ptr<Y>& r) noexcept
        {
            weak_ptr(r).swap(*this);
            return *this;
        }
        template<typename Y> weak_ptr& operator=(const shared_ptr<Y>& r) noexcept
        {
            weak_ptr(r).swap(*this);
            return *this;
        }
        weak_ptr& operator=(weak_ptr&& r) noexcept
        {
            weak_ptr(std::move(r)).swap(*this);
            return *this;
        }
        template<typename Y> weak_ptr& operator=(weak_ptr<Y>&& r) noexcept
        {
            weak_ptr(std::move(r)).swap(*this);
            return *this;
        }

        shared_ptr<T> lock() const
        {
            if (!cb_)
                return {};
            long c = cb_->local_shared_owners.load(std::memory_order_relaxed);
            while (c > 0)
            {
                if (cb_->local_shared_owners.compare_exchange_weak(
                        c, c + 1, std::memory_order_acq_rel, std::memory_order_relaxed))
                    return shared_ptr<T>(cb_, static_cast<T*>(ptr_for_lock_));
            }
            return {};
        }
        long use_count() const noexcept { return cb_ ? cb_->local_shared_owners.load(std::memory_order_relaxed) : 0; }
        bool expired() const noexcept
        {
            if (!cb_)
                return true;
            if (use_count() > 0)
                return false;
            return true;
        }
        void reset() noexcept { weak_ptr().swap(*this); }
        void swap(weak_ptr& r) noexcept
        {
            std::swap(cb_, r.cb_);
            std::swap(ptr_for_lock_, r.ptr_for_lock_);
        }

        template<typename Y>
        bool owner_before(const shared_ptr<Y>& other) const noexcept {
            return cb_ < other.internal_get_cb();
        }

        template<typename Y>
        bool owner_before(const weak_ptr<Y>& other) const noexcept {
            return cb_ < other.cb_;
        }

        impl::control_block_base* internal_get_cb() const { return cb_; }

        template<typename U> friend class enable_shared_from_this;
        template<typename U> friend class weak_ptr;
    };


    // --- Free Functions ---

    // Comparison operators for shared_ptr
    template<typename T, typename U>
    bool operator==(const shared_ptr<T>& a, const shared_ptr<U>& b) noexcept
    {
        return a.get() == b.get();
    }

    template<typename T, typename U>
    bool operator!=(const shared_ptr<T>& a, const shared_ptr<U>& b) noexcept
    {
        return a.get() != b.get();
    }

    template<typename T, typename U>
    bool operator<(const shared_ptr<T>& a, const shared_ptr<U>& b) noexcept
    {
        return std::less<std::common_type_t<T*, U*>>{}(a.get(), b.get());
    }

    template<typename T, typename U>
    bool operator<=(const shared_ptr<T>& a, const shared_ptr<U>& b) noexcept
    {
        return !(b < a);
    }

    template<typename T, typename U>
    bool operator>(const shared_ptr<T>& a, const shared_ptr<U>& b) noexcept
    {
        return b < a;
    }

    template<typename T, typename U>
    bool operator>=(const shared_ptr<T>& a, const shared_ptr<U>& b) noexcept
    {
        return !(a < b);
    }

    template<typename T, typename... Args> shared_ptr<T> make_shared(Args&&... args)
    {
        using Alloc = std::allocator<std::remove_cv_t<T>>;
        using CBAllocForMakeShared =
            typename std::allocator_traits<Alloc>::template rebind_alloc<impl::control_block_make_shared<T, Alloc, Args...>>;

        CBAllocForMakeShared cb_alloc;
        auto* cb_ptr = std::allocator_traits<CBAllocForMakeShared>::allocate(cb_alloc, 1);

        try
        {
            std::allocator_traits<CBAllocForMakeShared>::construct(cb_alloc, cb_ptr, Alloc(), std::forward<Args>(args)...);
        }
        catch (...)
        {
            std::allocator_traits<CBAllocForMakeShared>::deallocate(cb_alloc, cb_ptr, 1);
            throw;
        }
        cb_ptr->increment_local_shared();
        shared_ptr<T> result(
            static_cast<impl::control_block_base*>(cb_ptr), static_cast<T*>(cb_ptr->get_managed_object_ptr()));
        try_enable_shared_from_this(result, static_cast<T*>(cb_ptr->get_managed_object_ptr()));
        return result;
    }

    template<typename T, typename Alloc, typename... Args>
    shared_ptr<T> allocate_shared(const Alloc& alloc, Args&&... args)
    {
        using CleanAlloc = std::allocator<std::remove_cv_t<T>>;
        using CBAllocForMakeShared =
            typename std::allocator_traits<CleanAlloc>::template rebind_alloc<impl::control_block_make_shared<T, CleanAlloc, Args...>>;

        CBAllocForMakeShared cb_alloc;
        auto* cb_ptr = std::allocator_traits<CBAllocForMakeShared>::allocate(cb_alloc, 1);

        try
        {
            std::allocator_traits<CBAllocForMakeShared>::construct(cb_alloc, cb_ptr, CleanAlloc(), std::forward<Args>(args)...);
        }
        catch (...)
        {
            std::allocator_traits<CBAllocForMakeShared>::deallocate(cb_alloc, cb_ptr, 1);
            throw;
        }
        cb_ptr->increment_local_shared();
        shared_ptr<T> result(
            static_cast<impl::control_block_base*>(cb_ptr), static_cast<T*>(cb_ptr->get_managed_object_ptr()));
        try_enable_shared_from_this(result, static_cast<T*>(cb_ptr->get_managed_object_ptr()));
        return result;
    }

    template<typename T> class enable_shared_from_this
    {
    protected:
        mutable weak_ptr<T> weak_this_;
        constexpr enable_shared_from_this() noexcept = default;
        enable_shared_from_this(const enable_shared_from_this&) noexcept = default;
        enable_shared_from_this& operator=(const enable_shared_from_this&) noexcept = default;
        ~enable_shared_from_this() = default;

        template<typename ActualPtrType>
        void internal_set_weak_this(impl::control_block_base* cb_for_this_obj, ActualPtrType* ptr_to_this_obj) const
        {
            if (static_cast<const void*>(static_cast<const T*>(ptr_to_this_obj)) == static_cast<const void*>(this))
            {
                if (weak_this_.expired())
                {
                    if (cb_for_this_obj && ptr_to_this_obj)
                    {
                        // Directly construct weak_ptr from the control block and pointer
                        // instead of creating a temporary shared_ptr
                        weak_this_.cb_ = cb_for_this_obj;
                        weak_this_.ptr_for_lock_ = const_cast<T*>(static_cast<const T*>(ptr_to_this_obj));
                        if (cb_for_this_obj) {
                            cb_for_this_obj->increment_local_weak();
                        }
                    }
                }
            }
        }

    public:
        shared_ptr<T> shared_from_this() { return weak_this_.lock(); }
        shared_ptr<const T> shared_from_this() const { return weak_this_.lock(); }
        weak_ptr<T> weak_from_this() noexcept { return weak_this_; }
        weak_ptr<const T> weak_from_this() const noexcept { return weak_this_; }

        template<typename U, typename... Args> friend shared_ptr<U> make_shared(Args&&...);
        template<typename U> friend class shared_ptr;
        template<typename U> friend class weak_ptr;
        template<typename T2, typename Y2> friend void try_enable_shared_from_this(shared_ptr<T2>& sp, Y2* ptr) noexcept;
    };

    // Helper function to initialize enable_shared_from_this
    template<typename T, typename Y>
    void try_enable_shared_from_this(shared_ptr<T>& sp, Y* ptr) noexcept
    {
        if constexpr (std::is_base_of_v<enable_shared_from_this<Y>, Y>) {
            if (ptr && sp.internal_get_cb()) {
                static_cast<enable_shared_from_this<Y>*>(ptr)->internal_set_weak_this(sp.internal_get_cb(), ptr);
            }
        }
    }

    // Pointer cast functions
    template<typename T, typename U>
    shared_ptr<T> static_pointer_cast(const shared_ptr<U>& r) noexcept
    {
        auto p = static_cast<T*>(r.get());
        return shared_ptr<T>(r, p);
    }

    template<typename T, typename U>
    shared_ptr<T> dynamic_pointer_cast(const shared_ptr<U>& r) noexcept
    {
        if (auto p = dynamic_cast<T*>(r.get()))
        {
            return shared_ptr<T>(r, p);
        }
        return shared_ptr<T>();
    }

    template<typename T, typename U>
    shared_ptr<T> const_pointer_cast(const shared_ptr<U>& r) noexcept
    {
        auto p = const_cast<T*>(r.get());
        return shared_ptr<T>(r, p);
    }

    template<typename T, typename U>
    shared_ptr<T> reinterpret_pointer_cast(const shared_ptr<U>& r) noexcept
    {
        auto p = reinterpret_cast<T*>(r.get());
        return shared_ptr<T>(r, p);
    }

    // Comparison operators with nullptr
    template<typename T>
    bool operator==(const shared_ptr<T>& x, std::nullptr_t) noexcept
    {
        return !x;
    }

    template<typename T>
    bool operator==(std::nullptr_t, const shared_ptr<T>& x) noexcept
    {
        return !x;
    }

    template<typename T>
    bool operator!=(const shared_ptr<T>& x, std::nullptr_t) noexcept
    {
        return static_cast<bool>(x);
    }

    template<typename T>
    bool operator!=(std::nullptr_t, const shared_ptr<T>& x) noexcept
    {
        return static_cast<bool>(x);
    }

    template<typename T>
    bool operator<(const shared_ptr<T>& x, std::nullptr_t) noexcept
    {
        return std::less<T*>()(x.get(), nullptr);
    }

    template<typename T>
    bool operator<(std::nullptr_t, const shared_ptr<T>& x) noexcept
    {
        return std::less<T*>()(nullptr, x.get());
    }

    template<typename T>
    bool operator<=(const shared_ptr<T>& x, std::nullptr_t) noexcept
    {
        return !(nullptr < x);
    }

    template<typename T>
    bool operator<=(std::nullptr_t, const shared_ptr<T>& x) noexcept
    {
        return !(x < nullptr);
    }

    template<typename T>
    bool operator>(const shared_ptr<T>& x, std::nullptr_t) noexcept
    {
        return nullptr < x;
    }

    template<typename T>
    bool operator>(std::nullptr_t, const shared_ptr<T>& x) noexcept
    {
        return x < nullptr;
    }

    template<typename T>
    bool operator>=(const shared_ptr<T>& x, std::nullptr_t) noexcept
    {
        return !(x < nullptr);
    }

    template<typename T>
    bool operator>=(std::nullptr_t, const shared_ptr<T>& x) noexcept
    {
        return !(nullptr < x);
    }

    // Output stream operator
    template<typename T>
    std::ostream& operator<<(std::ostream& os, const shared_ptr<T>& ptr)
    {
        return os << ptr.get();
    }

    // NAMESPACE_INLINE_END  // Commented out to simplify

#ifdef TEST_STL_COMPLIANCE
    // Hash specializations for std namespace
    template<typename T>
    struct hash<shared_ptr<T>>
    {
        size_t operator()(const shared_ptr<T>& p) const noexcept {
            return std::hash<T*>()(p.get());
        }
    };

    template<typename T>
    struct hash<weak_ptr<T>>
    {
        size_t operator()(const weak_ptr<T>& p) const noexcept
        {
            if (auto sp = p.lock())
            {
                return hash<shared_ptr<T>>()(sp);
            }
            return 0;
        }
    };

} // namespace std

#else

} // namespace rpc

namespace std
{
    template<typename T> struct hash<rpc::shared_ptr<T>>
    {
        size_t operator()(const rpc::shared_ptr<T>& p) const noexcept { return std::hash<T*>()(p.get()); }
    };

    template<typename T> struct hash<rpc::weak_ptr<T>>
    {
        size_t operator()(const rpc::weak_ptr<T>& p) const noexcept
        {
            if (auto sp = p.lock())
            {
                return std::hash<rpc::shared_ptr<T>>()(sp);
            }
            return 0;
        }
    };

} // namespace std

#endif
