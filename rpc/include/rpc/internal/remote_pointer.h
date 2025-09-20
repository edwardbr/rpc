/*
 *   Copyright (c) 2025 Edward Boggis-Rolfe
 *   All rights reserved.
 */

#pragma once

#include <atomic>
#include <cstddef>
#include <functional>
#include <memory>
#include <stdexcept>
#include <type_traits>
#include <typeinfo>
#include <utility>

#include <rpc/internal/casting_interface.h>
#include <rpc/internal/version.h>
#include <rpc/internal/coroutine_support.h>

namespace rpc
{
    template<class T> class shared_ptr;
    template<class T> class weak_ptr;
    template<class T> class enable_shared_from_this;

    template<class T>
    struct is_casting_compatible
        : std::bool_constant<std::is_base_of_v<casting_interface, std::remove_cv_t<std::remove_extent_t<T>>>>
    {
    };

    template<class T>
    inline constexpr bool is_casting_compatible_v = is_casting_compatible<T>::value;

    namespace internal
    {
        class ref_count_base
        {
            std::atomic<long> uses_{1};
            std::atomic<long> weaks_{1};

        public:
            constexpr ref_count_base() noexcept = default;
            virtual ~ref_count_base() = default;

            void add_ref() noexcept { uses_.fetch_add(1, std::memory_order_relaxed); }

            bool add_ref_lock() noexcept
            {
                long count = uses_.load(std::memory_order_acquire);
                for (;;)
                {
                    if (count == 0)
                        return false;
                    if (uses_.compare_exchange_weak(
                            count, count + 1, std::memory_order_acq_rel, std::memory_order_acquire))
                        return true;
                }
            }

            void release() noexcept
            {
                if (uses_.fetch_sub(1, std::memory_order_acq_rel) == 1)
                {
                    destroy();
                    weak_release();
                }
            }

            void weak_add_ref() noexcept { weaks_.fetch_add(1, std::memory_order_relaxed); }

            void weak_release() noexcept
            {
                if (weaks_.fetch_sub(1, std::memory_order_acq_rel) == 1)
                {
                    delete_this();
                }
            }

            long use_count() const noexcept { return uses_.load(std::memory_order_acquire); }

            virtual void destroy() noexcept = 0;
            virtual void delete_this() noexcept = 0;
            virtual void* get_deleter(const std::type_info&) noexcept { return nullptr; }
        };

        template<class T, class Deleter, class Alloc>
        class ref_count final : public ref_count_base
        {
            T* ptr_;
            Deleter deleter_;
            Alloc allocator_;

        public:
            ref_count(T* ptr, Deleter deleter, Alloc allocator) noexcept(std::is_nothrow_move_constructible_v<Deleter>
                && std::is_nothrow_move_constructible_v<Alloc>)
                : ptr_(ptr)
                , deleter_(std::move(deleter))
                , allocator_(std::move(allocator))
            {
            }

            void destroy() noexcept override
            {
                if (ptr_)
                {
                    deleter_(ptr_);
                    ptr_ = nullptr;
                }
            }

            void delete_this() noexcept override
            {
                using rebound_alloc = typename std::allocator_traits<Alloc>::template rebind_alloc<ref_count>;
                rebound_alloc alloc_copy(allocator_);
                std::allocator_traits<rebound_alloc>::destroy(alloc_copy, this);
                std::allocator_traits<rebound_alloc>::deallocate(alloc_copy, this, 1);
            }

            void* get_deleter(const std::type_info& ti) noexcept override
            {
                return ti == typeid(Deleter) ? std::addressof(deleter_) : nullptr;
            }
        };

        template<class T, class Alloc>
        class ref_count_obj final : public ref_count_base
        {
            Alloc allocator_;
            bool constructed_{false};
            alignas(T) unsigned char storage_[sizeof(T)];

            T* ptr() noexcept { return reinterpret_cast<T*>(&storage_); }

        public:
            template<class... Args>
            explicit ref_count_obj(const Alloc& allocator, Args&&... args)
                : allocator_(allocator)
            {
                std::allocator_traits<Alloc>::construct(allocator_, ptr(), std::forward<Args>(args)...);
                constructed_ = true;
            }

            void destroy() noexcept override
            {
                if (constructed_)
                {
                    std::allocator_traits<Alloc>::destroy(allocator_, ptr());
                    constructed_ = false;
                }
            }

            void delete_this() noexcept override
            {
                using rebound_alloc = typename std::allocator_traits<Alloc>::template rebind_alloc<ref_count_obj>;
                rebound_alloc alloc_copy(allocator_);
                std::allocator_traits<rebound_alloc>::destroy(alloc_copy, this);
                std::allocator_traits<rebound_alloc>::deallocate(alloc_copy, this, 1);
            }

            T* get_pointer() noexcept { return ptr(); }
        };
    } // namespace internal

    enum class adopt_ref
    {
        assume_ownership,
        add_ref
    };

    template<class T> class weak_ptr;

    template<class T>
    class shared_ptr
    {
    public:
        using element_type = std::remove_extent_t<T>;
        using weak_type = weak_ptr<T>;

        static_assert(is_casting_compatible_v<T>, "rpc::shared_ptr requires T to derive from rpc::casting_interface");

        constexpr shared_ptr() noexcept = default;
        constexpr shared_ptr(std::nullptr_t) noexcept
        {
        }

        template<class Y, std::enable_if_t<std::is_convertible_v<Y*, T*>, int> = 0>
        explicit shared_ptr(Y* ptr)
        {
            if (ptr)
            {
                construct_control_block(ptr, std::default_delete<Y>(), std::allocator<Y>());
                enable_shared_from_this_if_needed(ptr);
            }
        }

        template<class Y, class Deleter, std::enable_if_t<std::is_convertible_v<Y*, T*>, int> = 0>
        shared_ptr(Y* ptr, Deleter deleter)
        {
            construct_with_optional_control_block(ptr, std::forward<Deleter>(deleter), std::allocator<Y>());
            if (rep_)
                enable_shared_from_this_if_needed(ptr);
        }

        template<class Y, class Deleter, class Alloc, std::enable_if_t<std::is_convertible_v<Y*, T*>, int> = 0>
        shared_ptr(Y* ptr, Deleter deleter, Alloc alloc)
        {
            construct_with_optional_control_block(ptr, std::forward<Deleter>(deleter), std::forward<Alloc>(alloc));
            if (rep_)
                enable_shared_from_this_if_needed(ptr);
        }

        shared_ptr(const shared_ptr& other) noexcept
            : ptr_(other.ptr_)
            , rep_(other.rep_)
        {
            if (rep_)
                rep_->add_ref();
        }

        template<class Y, std::enable_if_t<std::is_convertible_v<Y*, T*>, int> = 0>
        shared_ptr(const shared_ptr<Y>& other) noexcept
            : ptr_(other.ptr_)
            , rep_(other.rep_)
        {
            if (rep_)
                rep_->add_ref();
        }

        shared_ptr(shared_ptr&& other) noexcept
            : ptr_(other.ptr_)
            , rep_(other.rep_)
        {
            other.ptr_ = nullptr;
            other.rep_ = nullptr;
        }

        template<class Y, std::enable_if_t<std::is_convertible_v<Y*, T*>, int> = 0>
        shared_ptr(shared_ptr<Y>&& other) noexcept
            : ptr_(other.ptr_)
            , rep_(other.rep_)
        {
            other.ptr_ = nullptr;
            other.rep_ = nullptr;
        }

        template<class Y, std::enable_if_t<std::is_convertible_v<Y*, T*>, int> = 0>
        shared_ptr(const shared_ptr<Y>& other, element_type* alias) noexcept
            : ptr_(alias)
            , rep_(other.rep_)
        {
            if (rep_)
                rep_->add_ref();
        }

        template<class Y, std::enable_if_t<std::is_convertible_v<Y*, T*>, int> = 0>
        shared_ptr(shared_ptr<Y>&& other, element_type* alias) noexcept
            : ptr_(alias)
            , rep_(other.rep_)
        {
            other.ptr_ = nullptr;
            other.rep_ = nullptr;
        }

        template<class Y, std::enable_if_t<std::is_convertible_v<Y*, T*>, int> = 0>
        explicit shared_ptr(const weak_ptr<Y>& weak)
        {
            auto tmp = weak.lock();
            if (!tmp)
                throw std::bad_weak_ptr();
            swap(tmp);
        }

        ~shared_ptr() { release(); }

        shared_ptr& operator=(const shared_ptr& other) noexcept
        {
            if (this != &other)
            {
                shared_ptr(other).swap(*this);
            }
            return *this;
        }

        template<class Y>
        shared_ptr& operator=(const shared_ptr<Y>& other) noexcept
        {
            shared_ptr(other).swap(*this);
            return *this;
        }

        shared_ptr& operator=(shared_ptr&& other) noexcept
        {
            if (this != &other)
            {
                shared_ptr(std::move(other)).swap(*this);
            }
            return *this;
        }

        template<class Y>
        shared_ptr& operator=(shared_ptr<Y>&& other) noexcept
        {
            shared_ptr(std::move(other)).swap(*this);
            return *this;
        }

        shared_ptr& operator=(std::nullptr_t) noexcept
        {
            reset();
            return *this;
        }

        void reset() noexcept { shared_ptr().swap(*this); }

        template<class Y, class Deleter = std::default_delete<Y>, class Alloc = std::allocator<Y>>
        void reset(Y* ptr, Deleter deleter = Deleter(), Alloc alloc = Alloc())
        {
            shared_ptr(ptr, std::move(deleter), std::move(alloc)).swap(*this);
        }

        element_type* get() const noexcept { return ptr_; }
        element_type& operator*() const noexcept { return *ptr_; }
        element_type* operator->() const noexcept { return ptr_; }
        long use_count() const noexcept { return rep_ ? rep_->use_count() : 0; }
        bool unique() const noexcept { return use_count() == 1; }
        explicit operator bool() const noexcept { return ptr_ != nullptr; }

        void swap(shared_ptr& other) noexcept
        {
            std::swap(ptr_, other.ptr_);
            std::swap(rep_, other.rep_);
        }

        template<class U>
        bool owner_before(const shared_ptr<U>& other) const noexcept
        {
            return std::less<internal::ref_count_base*>()(rep_, other.rep_);
        }

        template<class U>
        bool owner_before(const weak_ptr<U>& other) const noexcept;

    private:
        element_type* ptr_{nullptr};
        internal::ref_count_base* rep_{nullptr};

        template<class Y, class Deleter, class Alloc>
        void construct_control_block(Y* ptr, Deleter&& deleter, Alloc&& alloc)
        {
            using dec_del = std::decay_t<Deleter>;
            using dec_alloc = std::decay_t<Alloc>;
            using rep_type = internal::ref_count<Y, dec_del, dec_alloc>;
            dec_del deleter_copy(std::forward<Deleter>(deleter));
            dec_alloc alloc_copy(std::forward<Alloc>(alloc));
            typename std::allocator_traits<dec_alloc>::template rebind_alloc<rep_type> rep_alloc(alloc_copy);
            rep_type* rep = std::allocator_traits<decltype(rep_alloc)>::allocate(rep_alloc, 1);
            try
            {
                std::allocator_traits<decltype(rep_alloc)>::construct(
                    rep_alloc, rep, ptr, std::move(deleter_copy), std::move(alloc_copy));
            }
            catch (...)
            {
                std::allocator_traits<decltype(rep_alloc)>::deallocate(rep_alloc, rep, 1);
                if (ptr)
                {
                    dec_del fallback_deleter(std::move(deleter_copy));
                    fallback_deleter(ptr);
                }
                throw;
            }
            ptr_ = ptr;
            rep_ = rep;
        }

        template<class Y, class Deleter, class Alloc>
        void construct_with_optional_control_block(Y* ptr, Deleter deleter, Alloc alloc)
        {
            if (ptr != nullptr || !std::is_same_v<std::decay_t<Deleter>, std::default_delete<Y>>
                || !std::is_same_v<std::decay_t<Alloc>, std::allocator<Y>>)
            {
                construct_control_block(ptr, std::forward<Deleter>(deleter), std::forward<Alloc>(alloc));
            }
            else
            {
                ptr_ = nullptr;
                rep_ = nullptr;
            }
        }

        void release() noexcept
        {
            if (rep_)
            {
                rep_->release();
                rep_ = nullptr;
                ptr_ = nullptr;
            }
        }

        template<class Y>
        void enable_shared_from_this_if_needed(Y* raw) noexcept
        {
            using raw_type = std::remove_cv_t<Y>;
            if constexpr (std::is_base_of_v<enable_shared_from_this<raw_type>, raw_type>)
            {
                if (raw && rep_)
                {
                    auto* esf = static_cast<enable_shared_from_this<raw_type>*>(raw);
                    shared_ptr<raw_type> alias(rep_, raw, adopt_ref::add_ref);
                    esf->internal_accept_owner(alias, raw);
                }
            }
        }

        shared_ptr(internal::ref_count_base* rep, element_type* ptr, adopt_ref tag) noexcept
            : ptr_(ptr)
            , rep_(rep)
        {
            if (rep_ && tag == adopt_ref::add_ref)
            {
                rep_->add_ref();
            }
        }

        template<class U> friend class shared_ptr;
        template<class U> friend class weak_ptr;
        template<class U> friend class enable_shared_from_this;
        template<class U, class... Args> friend shared_ptr<U> make_shared(Args&&... args);
        template<class Deleter, class U> friend Deleter* std::get_deleter(const shared_ptr<U>&) noexcept;
    };

    template<class T>
    class weak_ptr
    {
        template<class U> friend class shared_ptr;
        template<class U> friend class weak_ptr;
        template<class U> friend class enable_shared_from_this;

    public:
        using element_type = std::remove_extent_t<T>;

        static_assert(is_casting_compatible_v<T>, "rpc::weak_ptr requires T to derive from rpc::casting_interface");

        constexpr weak_ptr() noexcept = default;

        template<class Y, std::enable_if_t<std::is_convertible_v<Y*, T*>, int> = 0>
        weak_ptr(const shared_ptr<Y>& shared) noexcept
            : ptr_(shared.ptr_)
            , rep_(shared.rep_)
        {
            if (rep_)
                rep_->weak_add_ref();
        }

        weak_ptr(const weak_ptr& other) noexcept
            : ptr_(other.ptr_)
            , rep_(other.rep_)
        {
            if (rep_)
                rep_->weak_add_ref();
        }

        template<class Y, std::enable_if_t<std::is_convertible_v<Y*, T*>, int> = 0>
        weak_ptr(const weak_ptr<Y>& other) noexcept
            : ptr_(other.ptr_)
            , rep_(other.rep_)
        {
            if (rep_)
                rep_->weak_add_ref();
        }

        weak_ptr(weak_ptr&& other) noexcept
            : ptr_(other.ptr_)
            , rep_(other.rep_)
        {
            other.ptr_ = nullptr;
            other.rep_ = nullptr;
        }

        template<class Y, std::enable_if_t<std::is_convertible_v<Y*, T*>, int> = 0>
        weak_ptr(weak_ptr<Y>&& other) noexcept
            : ptr_(other.ptr_)
            , rep_(other.rep_)
        {
            other.ptr_ = nullptr;
            other.rep_ = nullptr;
        }

        ~weak_ptr()
        {
            if (rep_)
                rep_->weak_release();
        }

        weak_ptr& operator=(const weak_ptr& other) noexcept
        {
            if (this != &other)
            {
                weak_ptr(other).swap(*this);
            }
            return *this;
        }

        template<class Y>
        weak_ptr& operator=(const weak_ptr<Y>& other) noexcept
        {
            weak_ptr(other).swap(*this);
            return *this;
        }

        template<class Y>
        weak_ptr& operator=(const shared_ptr<Y>& shared) noexcept
        {
            weak_ptr(shared).swap(*this);
            return *this;
        }

        weak_ptr& operator=(weak_ptr&& other) noexcept
        {
            weak_ptr(std::move(other)).swap(*this);
            return *this;
        }

        template<class Y>
        weak_ptr& operator=(weak_ptr<Y>&& other) noexcept
        {
            weak_ptr(std::move(other)).swap(*this);
            return *this;
        }

        void reset() noexcept { weak_ptr().swap(*this); }

        void swap(weak_ptr& other) noexcept
        {
            std::swap(ptr_, other.ptr_);
            std::swap(rep_, other.rep_);
        }

        long use_count() const noexcept { return rep_ ? rep_->use_count() : 0; }
        bool expired() const noexcept { return use_count() == 0; }

        shared_ptr<T> lock() const noexcept
        {
            if (rep_ && rep_->add_ref_lock())
            {
                return shared_ptr<T>(rep_, ptr_, adopt_ref::assume_ownership);
            }
            return shared_ptr<T>();
        }

        template<class U>
        bool owner_before(const shared_ptr<U>& other) const noexcept
        {
            return std::less<internal::ref_count_base*>()(rep_, other.rep_);
        }

        template<class U>
        bool owner_before(const weak_ptr<U>& other) const noexcept
        {
            return std::less<internal::ref_count_base*>()(rep_, other.rep_);
        }

    private:
        element_type* ptr_{nullptr};
        internal::ref_count_base* rep_{nullptr};
    };

    template<class T>
    template<class U>
    bool shared_ptr<T>::owner_before(const weak_ptr<U>& other) const noexcept
    {
        return std::less<internal::ref_count_base*>()(rep_, other.rep_);
    }

    template<class T>
    class enable_shared_from_this
    {
    protected:
        mutable weak_ptr<T> weak_this_;

        constexpr enable_shared_from_this() noexcept = default;
        enable_shared_from_this(const enable_shared_from_this&) noexcept = default;
        enable_shared_from_this& operator=(const enable_shared_from_this&) noexcept = default;
        ~enable_shared_from_this() = default;

        template<class Y>
        void internal_accept_owner(const shared_ptr<Y>& owner, T* ptr) const noexcept
        {
            if (ptr == static_cast<const void*>(this) && weak_this_.expired())
            {
                weak_this_ = owner;
            }
        }

        template<class U> friend class shared_ptr;
        template<class U> friend class weak_ptr;
        template<class U, class... Args> friend shared_ptr<U> make_shared(Args&&... args);

    public:
        shared_ptr<T> shared_from_this()
        {
            auto result = weak_this_.lock();
            if (!result)
                throw std::bad_weak_ptr();
            return result;
        }

        shared_ptr<const T> shared_from_this() const
        {
            auto result = weak_this_.lock();
            if (!result)
                throw std::bad_weak_ptr();
            return result;
        }

        weak_ptr<T> weak_from_this() const noexcept { return weak_this_; }
    };

    template<class T, class... Args>
    shared_ptr<T> make_shared(Args&&... args)
    {
        static_assert(is_casting_compatible_v<T>, "rpc::make_shared requires T to derive from rpc::casting_interface");

        using Alloc = std::allocator<T>;
        using Rep = internal::ref_count_obj<T, Alloc>;
        Alloc alloc;
        typename std::allocator_traits<Alloc>::template rebind_alloc<Rep> rep_alloc(alloc);
        Rep* rep = std::allocator_traits<decltype(rep_alloc)>::allocate(rep_alloc, 1);
        try
        {
            std::allocator_traits<decltype(rep_alloc)>::construct(rep_alloc, rep, alloc, std::forward<Args>(args)...);
        }
        catch (...)
        {
            std::allocator_traits<decltype(rep_alloc)>::deallocate(rep_alloc, rep, 1);
            throw;
        }

        shared_ptr<T> result;
        result.ptr_ = rep->get_pointer();
        result.rep_ = rep;
        result.enable_shared_from_this_if_needed(result.ptr_);
        return result;
    }

    template<class T, class U>
    bool operator==(const shared_ptr<T>& lhs, const shared_ptr<U>& rhs) noexcept
    {
        return lhs.get() == rhs.get();
    }
    template<class T>
    bool operator==(const shared_ptr<T>& lhs, std::nullptr_t) noexcept
    {
        return !lhs;
    }
    template<class T>
    bool operator==(std::nullptr_t, const shared_ptr<T>& rhs) noexcept
    {
        return !rhs;
    }
    template<class T, class U>
    bool operator!=(const shared_ptr<T>& lhs, const shared_ptr<U>& rhs) noexcept
    {
        return !(lhs == rhs);
    }
    template<class T>
    bool operator!=(const shared_ptr<T>& lhs, std::nullptr_t) noexcept
    {
        return static_cast<bool>(lhs);
    }
    template<class T>
    bool operator!=(std::nullptr_t, const shared_ptr<T>& rhs) noexcept
    {
        return static_cast<bool>(rhs);
    }
    template<class T, class U>
    bool operator<(const shared_ptr<T>& lhs, const shared_ptr<U>& rhs) noexcept
    {
        return std::less<const void*>{}(lhs.get(), rhs.get());
    }
    template<class T, class U>
    bool operator>(const shared_ptr<T>& lhs, const shared_ptr<U>& rhs) noexcept
    {
        return rhs < lhs;
    }
    template<class T, class U>
    bool operator<=(const shared_ptr<T>& lhs, const shared_ptr<U>& rhs) noexcept
    {
        return !(rhs < lhs);
    }
    template<class T, class U>
    bool operator>=(const shared_ptr<T>& lhs, const shared_ptr<U>& rhs) noexcept
    {
        return !(lhs < rhs);
    }

    template<class T, class U>
    shared_ptr<T> static_pointer_cast(const shared_ptr<U>& from) noexcept
    {
        if (!from)
            return shared_ptr<T>();
        T* ptr = static_cast<T*>(from.get());
        return shared_ptr<T>(from, ptr);
    }

    template<class T, class U>
    shared_ptr<T> const_pointer_cast(const shared_ptr<U>& from) noexcept
    {
        if (!from)
            return shared_ptr<T>();
        T* ptr = const_cast<T*>(from.get());
        return shared_ptr<T>(from, ptr);
    }

    template<class T, class U>
    shared_ptr<T> reinterpret_pointer_cast(const shared_ptr<U>& from) noexcept
    {
        if (!from)
            return shared_ptr<T>();
        T* ptr = reinterpret_cast<T*>(from.get());
        return shared_ptr<T>(from, ptr);
    }

    template<class T1, class T2>
    [[nodiscard]] inline CORO_TASK(shared_ptr<T1>) dynamic_pointer_cast(const shared_ptr<T2>& from) noexcept
    {
        if (!from)
            CO_RETURN shared_ptr<T1>();

        T1* ptr = dynamic_cast<T1*>(from.get());
        if (ptr)
        {
            CO_RETURN shared_ptr<T1>(from, ptr);
        }

        // Remote proxy casting is intentionally deferred; the implementation can consult
        // casting_interface::query_casting_interface when the proxy pipeline is wired up.
        CO_RETURN shared_ptr<T1>();
    }
} // namespace rpc

namespace std
{
    template<class T>
    struct hash<rpc::shared_ptr<T>>
    {
        size_t operator()(const rpc::shared_ptr<T>& value) const noexcept { return hash<T*>()(value.get()); }
    };

    template<class T>
    struct hash<rpc::weak_ptr<T>>
    {
        size_t operator()(const rpc::weak_ptr<T>& value) const noexcept
        {
            if (auto locked = value.lock())
                return hash<T*>()(locked.get());
            return 0;
        }
    };

    template<class Deleter, class T>
    Deleter* get_deleter(const rpc::shared_ptr<T>& pointer) noexcept
    {
        return static_cast<Deleter*>(pointer.rep_ ? pointer.rep_->get_deleter(typeid(Deleter)) : nullptr);
    }
} // namespace std
