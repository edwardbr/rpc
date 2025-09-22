/*
 *   Copyright (c) 2025 Edward Boggis-Rolfe
 *   All rights reserved.
 */

#pragma once

#include <atomic>
#include <memory>      // For std::default_delete, std::allocator, std::allocator_traits, std::shared_ptr
#include <utility>     // For std::swap, std::move, std::forward
#include <stdexcept>   // For std::bad_weak_ptr, std::invalid_argument
#include <type_traits> // For std::is_base_of_v, std::is_convertible_v, std::remove_extent_t, etc.
#include <functional>  // For std::hash
#include <cstddef>     // For std::nullptr_t, std::size_t

#include <rpc/internal/version.h>
#include <rpc/internal/marshaller.h>
#include <rpc/internal/member_ptr.h>
#include <rpc/internal/coroutine_support.h> // Needed for CORO_TASK macros
// #include <rpc/internal/proxy.h>  // Commented out to break circular dependency
#include <rpc/internal/casting_interface.h> // Needed for dynamic_pointer_cast

namespace rpc
{
    class object_proxy;
    class proxy_base;

    // Forward declarations for circular dependency resolution
    template<typename T> class shared_ptr;
    template<typename T> class weak_ptr;
    template<typename T> class optimistic_ptr;
    // template<typename T, typename Deleter = std::default_delete<T>> class unique_ptr;  // Commented out - not needed

    // NAMESPACE_INLINE_BEGIN  // Commented out to simplify

    namespace internal
    {
        struct control_block_base
        {
            std::atomic<long> local_shared_owners{0};
            std::atomic<long> local_weak_owners{1};
            std::shared_ptr<rpc::object_proxy> this_cb_object_proxy_sp_;

        protected:
            void* managed_object_ptr_{nullptr};

        public:
            control_block_base(void* obj_ptr, std::shared_ptr<rpc::object_proxy> obj_proxy_for_this_cb_obj)
                : this_cb_object_proxy_sp_(std::move(obj_proxy_for_this_cb_obj))
                , // Initialized in declaration order
                managed_object_ptr_(obj_ptr)
            {
            }

            control_block_base()
                : this_cb_object_proxy_sp_(nullptr)
                , // Initialized in declaration order
                managed_object_ptr_(nullptr)
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

        template<typename T, typename Deleter, typename Alloc> struct control_block_impl : public control_block_base
        {
            Deleter object_deleter_;
            Alloc control_block_allocator_;
            control_block_impl(T* p, std::shared_ptr<rpc::object_proxy> obj_proxy, Deleter d, Alloc a)
                : control_block_base(p, std::move(obj_proxy))
                , object_deleter_(std::move(d))
                , control_block_allocator_(std::move(a))
            {
            }
            control_block_impl(T* p, Deleter d, Alloc a)
                : control_block_base(p, nullptr)
                , object_deleter_(std::move(d))
                , control_block_allocator_(std::move(a))
            {
            }

            void dispose_object_actual() override
            {
                T* obj_ptr = static_cast<T*>(managed_object_ptr_);
                if (obj_ptr)
                {
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
                std::allocator_traits<Alloc>::construct(
                    allocator_instance_, &object_instance_, std::forward<ConcreteArgs>(args_for_t)...);
                this->managed_object_ptr_ = &object_instance_;
            }

            void dispose_object_actual() override
            {
                if (managed_object_ptr_)
                {
                    std::allocator_traits<Alloc>::destroy(allocator_instance_, &object_instance_);
                    managed_object_ptr_ = nullptr;
                }
            }

            void destroy_self_actual() override
            {
                if (managed_object_ptr_)
                {
                    std::allocator_traits<Alloc>::destroy(allocator_instance_, &object_instance_);
                    managed_object_ptr_ = nullptr;
                }
                using ThisCBType = control_block_make_shared<T, Alloc, Args...>;
                typename std::allocator_traits<Alloc>::template rebind_alloc<ThisCBType> actual_cb_allocator(
                    allocator_instance_);
                std::allocator_traits<decltype(actual_cb_allocator)>::deallocate(actual_cb_allocator, this, 1);
            }
            ~control_block_make_shared() { }
        };
    } // namespace internal

    template<typename T>
    std::shared_ptr<rpc::object_proxy> get_ultimate_object_proxy_for(T* ptr, rpc::internal::control_block_base* cb_of_ptr)
    {
        if (!ptr)
            return nullptr;

        // Commented out proxy-specific code to break circular dependency
        // if (rpc::proxy_base* proxy = dynamic_cast<rpc::proxy_base*>(const_cast<std::remove_cv_t<T>*>(ptr)))
        // {
        //     return proxy->get_object_proxy();
        // }
        if (cb_of_ptr)
        {
            return cb_of_ptr->this_cb_object_proxy_sp_;
        }
        return nullptr;
    }

    template<typename T> class shared_ptr
    {
        T* ptr_{nullptr};
        internal::control_block_base* cb_{nullptr};
        std::shared_ptr<rpc::object_proxy> ultimate_actual_object_proxy_sp_{nullptr};

        void acquire_this() noexcept
        {
            if (cb_)
                cb_->increment_local_shared();
            // Commented out remote reference counting to break circular dependency
            // if (ultimate_actual_object_proxy_sp_)
            //     ultimate_actual_object_proxy_sp_->increment_remote_strong();
        }
        void release_this() noexcept
        {
            // Commented out remote reference counting to break circular dependency
            // if (ultimate_actual_object_proxy_sp_)
            //     ultimate_actual_object_proxy_sp_->decrement_remote_strong_and_signal_if_appropriate();
            if (cb_)
                cb_->decrement_local_shared_and_dispose_if_zero();
        }

        shared_ptr(internal::control_block_base* cb, T* p)
            : ptr_(p)
            , cb_(cb)
        {
            if (cb_)
            {
                // Commented out remote reference counting to break circular dependency
                ultimate_actual_object_proxy_sp_ = get_ultimate_object_proxy_for(ptr_, cb_);
                // if (ultimate_actual_object_proxy_sp_)
                // {
                //     ultimate_actual_object_proxy_sp_->increment_remote_strong();
                // }
            }
            else
            {
                ptr_ = nullptr;
            }
        }

        enum class for_enable_shared_tag
        {
        };
        shared_ptr(internal::control_block_base* cb, T* p, for_enable_shared_tag)
            : ptr_(p)
            , cb_(cb)
            , ultimate_actual_object_proxy_sp_(nullptr)
        {
            if (cb_ && ptr_)
            {
                ultimate_actual_object_proxy_sp_ = get_ultimate_object_proxy_for(ptr_, cb_);
            }
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

            using ActualCB = internal::control_block_impl<Y, Deleter, Alloc>;
            typename std::allocator_traits<Alloc>::template rebind_alloc<ActualCB> actual_cb_alloc(cb_alloc);
            cb_ = std::allocator_traits<decltype(actual_cb_alloc)>::allocate(actual_cb_alloc, 1);
            new (cb_) ActualCB(p, std::move(d), std::move(cb_alloc));
        }

    public:
        using element_type = std::remove_extent_t<T>;
        using weak_type = weak_ptr<T>;

        constexpr shared_ptr() noexcept = default;
        constexpr shared_ptr(std::nullptr_t) noexcept { }

        template<typename Y, typename = std::enable_if_t<std::is_convertible_v<Y*, T*>>>
        explicit shared_ptr(Y* p)
            : ptr_(p)
            , cb_(nullptr)
            , ultimate_actual_object_proxy_sp_(nullptr)
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
            }
        }

        template<typename Y, typename Deleter, typename = std::enable_if_t<std::is_convertible_v<Y*, T*>>>
        shared_ptr(Y* p, Deleter d)
            : ptr_(p)
            , cb_(nullptr)
            , ultimate_actual_object_proxy_sp_(nullptr)
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
            }
        }

        template<typename Deleter>
        shared_ptr(std::nullptr_t, Deleter d)
            : ptr_(nullptr)
            , cb_(nullptr)
            , ultimate_actual_object_proxy_sp_(nullptr)
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
            , ultimate_actual_object_proxy_sp_(nullptr)
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
            }
        }

        template<typename Deleter, typename Alloc>
        shared_ptr(std::nullptr_t, Deleter d, Alloc cb_alloc)
            : ptr_(nullptr)
            , cb_(nullptr)
            , ultimate_actual_object_proxy_sp_(nullptr)
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

        // Commented out casting_interface specific constructor to break circular dependency
        // template<typename Y,
        //     typename Deleter = std::default_delete<Y>,
        //     typename Alloc = std::allocator<char>,
        //     typename = std::enable_if_t<std::is_base_of<T, Y>::value && std::is_base_of<casting_interface, Y>::value>>
        // explicit shared_ptr(
        //     Y* p, std::shared_ptr<rpc::object_proxy> obj_proxy_for_p_obj, Deleter d = Deleter(), Alloc cb_alloc = Alloc())
        template<typename Y, typename Deleter = std::default_delete<Y>, typename Alloc = std::allocator<char>>
        explicit shared_ptr(
            Y* p, std::shared_ptr<rpc::object_proxy> obj_proxy_for_p_obj, Deleter d = Deleter(), Alloc cb_alloc = Alloc())
            : ptr_(p)
            , cb_(nullptr)
            , ultimate_actual_object_proxy_sp_(nullptr)
        {
            if (!ptr_)
                return;
            try
            {
                using ActualCB = internal::control_block_impl<Y, Deleter, Alloc>;
                typename std::allocator_traits<Alloc>::template rebind_alloc<ActualCB> actual_cb_alloc(cb_alloc);
                cb_ = std::allocator_traits<decltype(actual_cb_alloc)>::allocate(actual_cb_alloc, 1);
                new (cb_) ActualCB(p, std::move(obj_proxy_for_p_obj), std::move(d), std::move(cb_alloc));
            }
            catch (...)
            {
                if (cb_)
                {
                    typename std::allocator_traits<Alloc>::template rebind_alloc<internal::control_block_impl<Y, Deleter, Alloc>>
                        actual_cb_alloc(cb_alloc);
                    std::allocator_traits<decltype(actual_cb_alloc)>::deallocate(
                        actual_cb_alloc, static_cast<internal::control_block_impl<Y, Deleter, Alloc>*>(cb_), 1);
                }
                cb_ = nullptr;
                ptr_ = nullptr;
                throw;
            }
            ultimate_actual_object_proxy_sp_ = get_ultimate_object_proxy_for(ptr_, cb_);
            acquire_this();
        }

        template<typename Y>
        shared_ptr(const shared_ptr<Y>& r, element_type* p_alias) noexcept
            : ptr_(p_alias)
            , cb_(r.internal_get_cb())
            , ultimate_actual_object_proxy_sp_(r.internal_get_ultimate_object_proxy())
        {
            acquire_this();
        }

        template<typename Y>
        shared_ptr(shared_ptr<Y>&& r, element_type* p_alias) noexcept
            : ptr_(p_alias)
            , cb_(r.internal_get_cb())
            , ultimate_actual_object_proxy_sp_(std::move(r.internal_get_ultimate_object_proxy_ref()))
        {
            r.cb_ = nullptr;
            r.ptr_ = nullptr;
        }

        shared_ptr(const shared_ptr& r) noexcept
            : ptr_(r.ptr_)
            , cb_(r.cb_)
            , ultimate_actual_object_proxy_sp_(r.ultimate_actual_object_proxy_sp_)
        {
            acquire_this();
        }
        shared_ptr(shared_ptr&& r) noexcept
            : ptr_(r.ptr_)
            , cb_(r.cb_)
            , ultimate_actual_object_proxy_sp_(std::move(r.ultimate_actual_object_proxy_sp_))
        {
            r.cb_ = nullptr;
            r.ptr_ = nullptr;
        }

        template<typename Y, typename = std::enable_if_t<std::is_convertible_v<Y*, T*>>>
        shared_ptr(const shared_ptr<Y>& r) noexcept
            : ptr_(static_cast<T*>(r.internal_get_ptr()))
            , cb_(r.internal_get_cb())
            , ultimate_actual_object_proxy_sp_(r.internal_get_ultimate_object_proxy())
        {
            acquire_this();
        }
        template<typename Y, typename = std::enable_if_t<std::is_convertible_v<Y*, T*>>>
        shared_ptr(shared_ptr<Y>&& r) noexcept
            : ptr_(static_cast<T*>(r.internal_get_ptr()))
            , cb_(r.internal_get_cb())
            , ultimate_actual_object_proxy_sp_(std::move(r.internal_get_ultimate_object_proxy_ref()))
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

        // Commented out unique_ptr constructor - not needed
        // template<typename Y, typename Deleter_U, typename = std::enable_if_t<std::is_convertible_v<Y*, T*>>>
        // shared_ptr(rpc::unique_ptr<Y, Deleter_U>&& u)
        //     : ptr_(nullptr)
        //     , cb_(nullptr)
        //     , ultimate_actual_object_proxy_sp_(nullptr)
        // {
        //     if (!u)
        //         return;
        //     Y* raw_p = u.get();
        //     Deleter_U deleter = u.get_deleter();
        //     try
        //     {
        //         create_cb_local<Y>(raw_p, std::move(deleter), std::allocator<char>());
        //         ptr_ = raw_p;
        //         u.release();
        //     }
        //     catch (...)
        //     {
        //         cb_ = nullptr;
        //         ptr_ = nullptr;
        //         throw;
        //     }
        //     if (cb_ || ptr_)
        //     {
        //         ultimate_actual_object_proxy_sp_ = get_ultimate_object_proxy_for(ptr_, cb_);
        //         acquire_this();
        //     }
        // }

        // Commented out all unique_ptr constructors - not needed
        // template<typename Y,
        //     typename Deleter_U,
        //     typename = std::enable_if_t<std::is_convertible_v<Y*, T*> && std::is_base_of<casting_interface, Y>::value>>
        // shared_ptr(rpc::unique_ptr<Y, Deleter_U>&& u, std::shared_ptr<rpc::object_proxy> obj_proxy_for_u_obj)
        // {
        //     // Implementation commented out
        // }

        ~shared_ptr() { release_this(); }
        shared_ptr& operator=(const shared_ptr& r) noexcept
        {
            if (this != &r)
            {
                release_this();
                cb_ = r.cb_;
                ptr_ = r.ptr_;
                ultimate_actual_object_proxy_sp_ = r.ultimate_actual_object_proxy_sp_;
                acquire_this();
            }
            return *this;
        }
        shared_ptr& operator=(shared_ptr&& r) noexcept
        {
            if (this != &r)
            {
                release_this();
                cb_ = r.cb_;
                ptr_ = r.ptr_;
                ultimate_actual_object_proxy_sp_ = std::move(r.ultimate_actual_object_proxy_sp_);
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
        template<typename Y, typename Deleter = std::default_delete<Y>, typename Alloc = std::allocator<char>>
        void reset(
            Y* p, std::shared_ptr<rpc::object_proxy> obj_proxy_for_p_obj, Deleter d = Deleter(), Alloc cb_alloc = Alloc())
        {
            shared_ptr(p, std::move(obj_proxy_for_p_obj), std::move(d), std::move(cb_alloc)).swap(*this);
        }

        void swap(shared_ptr& r) noexcept
        {
            std::swap(ptr_, r.ptr_);
            std::swap(cb_, r.cb_);
            ultimate_actual_object_proxy_sp_.swap(r.ultimate_actual_object_proxy_sp_);
        }

        T* get() const noexcept { return ptr_; }
        T& operator*() const noexcept { return *ptr_; }
        T* operator->() const noexcept { return ptr_; }
        long use_count() const noexcept { return cb_ ? cb_->local_shared_owners.load(std::memory_order_relaxed) : 0; }
        explicit operator bool() const noexcept { return ptr_ != nullptr; }

        internal::control_block_base* internal_get_cb() const { return cb_; }
        T* internal_get_ptr() const { return ptr_; }
        const std::shared_ptr<rpc::object_proxy>& internal_get_ultimate_object_proxy() const
        {
            return ultimate_actual_object_proxy_sp_;
        }
        std::shared_ptr<rpc::object_proxy>& internal_get_ultimate_object_proxy_ref()
        {
            return ultimate_actual_object_proxy_sp_;
        }

        template<typename U> friend class optimistic_ptr;
        template<typename U> friend class weak_ptr;
        template<typename U>
        friend class shared_ptr; // Allow different template instantiations to access private members
        template<typename U, typename... Args> friend shared_ptr<U> make_shared(Args&&... args);
        template<class T1_cast, class T2_cast>
        friend shared_ptr<T1_cast> dynamic_pointer_cast(const shared_ptr<T2_cast>& from) noexcept;
        template<typename U> friend class enable_shared_from_this;
    };

    template<typename T> class optimistic_ptr
    {
        rpc::shared_ptr<T> held_proxy_sp_;
        std::shared_ptr<rpc::object_proxy> ultimate_actual_object_proxy_sp_{nullptr};

    public:
        using element_type = T;
        constexpr optimistic_ptr() noexcept = default;
        constexpr optimistic_ptr(std::nullptr_t) noexcept { }

        explicit optimistic_ptr(const rpc::shared_ptr<T>& proxy_sp_param)
        {
            // Commented out proxy-specific code to break circular dependency
            // if (proxy_sp_param && proxy_sp_param->query_proxy_base() != nullptr)
            // {
            //     held_proxy_sp_ = proxy_sp_param;
            //     if (auto proxy_base_ptr = held_proxy_sp_->query_proxy_base())
            //     {
            //         ultimate_actual_object_proxy_sp_ = proxy_base_ptr->get_object_proxy();
            //     }
            //     if (ultimate_actual_object_proxy_sp_)
            //     {
            //         ultimate_actual_object_proxy_sp_->increment_remote_weak_callable();
            //     }
            //     else
            //     {
            //         held_proxy_sp_.reset();
            //     }
            // }
            // else if (proxy_sp_param)
            // { /* Error: not a proxy */
            // }

            // Simple assignment for now - will be enhanced later
            held_proxy_sp_ = proxy_sp_param;
        }
        explicit optimistic_ptr(rpc::shared_ptr<T>&& proxy_sp_param)
        {
            // Commented out proxy-specific code to break circular dependency
            // if (proxy_sp_param && proxy_sp_param->query_proxy_base() != nullptr)
            // {
            //     held_proxy_sp_ = std::move(proxy_sp_param);
            //     if (auto proxy_base_ptr = held_proxy_sp_->query_proxy_base())
            //     {
            //         ultimate_actual_object_proxy_sp_ = proxy_base_ptr->get_object_proxy();
            //     }
            //     if (ultimate_actual_object_proxy_sp_)
            //     {
            //         ultimate_actual_object_proxy_sp_->increment_remote_weak_callable();
            //     }
            //     else
            //     {
            //         held_proxy_sp_.reset();
            //     }
            // }
            // else if (proxy_sp_param)
            // { /* Error: not a proxy */
            // }

            // Simple assignment for now - will be enhanced later
            held_proxy_sp_ = std::move(proxy_sp_param);
        }

        optimistic_ptr(const optimistic_ptr& r)
            : held_proxy_sp_(r.held_proxy_sp_)
            , ultimate_actual_object_proxy_sp_(r.ultimate_actual_object_proxy_sp_)
        {
            // Commented out remote reference counting to break circular dependency
            // if (ultimate_actual_object_proxy_sp_)
            //     ultimate_actual_object_proxy_sp_->increment_remote_weak_callable();
        }
        optimistic_ptr(optimistic_ptr&& r) noexcept
            : held_proxy_sp_(std::move(r.held_proxy_sp_))
            , ultimate_actual_object_proxy_sp_(std::move(r.ultimate_actual_object_proxy_sp_))
        {
        }
        optimistic_ptr& operator=(const optimistic_ptr& r)
        {
            if (this != &r)
            {
                // Commented out remote reference counting to break circular dependency
                // if (ultimate_actual_object_proxy_sp_)
                //     ultimate_actual_object_proxy_sp_->decrement_remote_weak_callable_and_signal_if_appropriate();
                held_proxy_sp_ = r.held_proxy_sp_;
                ultimate_actual_object_proxy_sp_ = r.ultimate_actual_object_proxy_sp_;
                // if (ultimate_actual_object_proxy_sp_)
                //     ultimate_actual_object_proxy_sp_->increment_remote_weak_callable();
            }
            return *this;
        }
        optimistic_ptr& operator=(optimistic_ptr&& r) noexcept
        {
            if (this != &r)
            {
                // Commented out remote reference counting to break circular dependency
                // if (ultimate_actual_object_proxy_sp_)
                //     ultimate_actual_object_proxy_sp_->decrement_remote_weak_callable_and_signal_if_appropriate();
                held_proxy_sp_ = std::move(r.held_proxy_sp_);
                ultimate_actual_object_proxy_sp_ = std::move(r.ultimate_actual_object_proxy_sp_);
            }
            return *this;
        }
        ~optimistic_ptr()
        {
            // Commented out remote reference counting to break circular dependency
            // if (ultimate_actual_object_proxy_sp_)
            //     ultimate_actual_object_proxy_sp_->decrement_remote_weak_callable_and_signal_if_appropriate();
        }

        T* operator->() const noexcept { return held_proxy_sp_.operator->(); }
        T& operator*() const { return *held_proxy_sp_; }
        T* get() const noexcept { return held_proxy_sp_.get(); }
        long use_count() const noexcept { return held_proxy_sp_.use_count(); }
        explicit operator bool() const noexcept { return static_cast<bool>(held_proxy_sp_); }
        rpc::shared_ptr<T> get_proxy_shared_ptr() const { return held_proxy_sp_; }
        void reset() noexcept { optimistic_ptr().swap(*this); }
        void reset(const rpc::shared_ptr<T>& proxy_sp) { optimistic_ptr(proxy_sp).swap(*this); }
        void reset(rpc::shared_ptr<T>&& proxy_sp) { optimistic_ptr(std::move(proxy_sp)).swap(*this); }
        void swap(optimistic_ptr& r) noexcept
        {
            held_proxy_sp_.swap(r.held_proxy_sp_);
            ultimate_actual_object_proxy_sp_.swap(r.ultimate_actual_object_proxy_sp_);
        }
    };

    template<typename T> class weak_ptr
    {
        internal::control_block_base* cb_{nullptr};
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
        template<typename Y>
        weak_ptr(const optimistic_ptr<Y>& o) noexcept
            : weak_ptr(o.get_proxy_shared_ptr())
        {
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
        template<typename Y> weak_ptr& operator=(const optimistic_ptr<Y>& r) noexcept
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

        rpc::shared_ptr<T> lock() const
        {
            if (!cb_)
                return {};
            long c = cb_->local_shared_owners.load(std::memory_order_relaxed);
            while (c > 0)
            {
                if (cb_->local_shared_owners.compare_exchange_weak(
                        c, c + 1, std::memory_order_acq_rel, std::memory_order_relaxed))
                    return rpc::shared_ptr<T>(cb_, static_cast<T*>(ptr_for_lock_));
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

        template<typename U> friend class enable_shared_from_this;
    };

    // Commented out entire unique_ptr implementation - not needed
    // template<typename T, typename Deleter> class unique_ptr
    // {
    //     // Implementation commented out
    // };
    // template<typename T, typename Deleter> class unique_ptr<T[], Deleter>
    // {
    //     // Array version implementation commented out
    // };

    // --- Free Functions ---
    template<class T1, class T2>
    [[nodiscard]] inline CORO_TASK(shared_ptr<T1>) dynamic_pointer_cast(const shared_ptr<T2>& from) noexcept
    {
        if (!from)
            CO_RETURN nullptr;

        T1* ptr = nullptr;

        // First try local interface casting
        ptr = const_cast<T1*>(static_cast<const T1*>(from->query_interface(T1::get_id(VERSION_2))));
        if (ptr)
            CO_RETURN shared_ptr<T1>(from, ptr);

        // Then try remote interface casting through object_proxy
        auto ob = casting_interface::get_object_proxy(*from);
        if (!ob)
        {
            CO_RETURN shared_ptr<T1>();
        }
        shared_ptr<T1> ret;
        // This magic function will return a shared pointer to a new interface proxy
        // Its reference count will not be the same as the "from" pointer's value, semantically it
        // behaves the same as with normal dynamic_pointer_cast in that you can use this function to
        // cast back to the original. However static_pointer_cast in this case will not work for
        // remote interfaces.
        CO_AWAIT ob->template query_interface<T1>(ret);
        CO_RETURN ret;
    }

    template<class T1, class T2>
    [[nodiscard]] inline shared_ptr<T1> static_pointer_cast(const shared_ptr<T2>& from) noexcept
    {
        if (!from)
            return nullptr;
        T1* p = static_cast<T1*>(from.get());
        return shared_ptr<T1>(from, p);
    }
    template<class T1, class T2>
    [[nodiscard]] inline shared_ptr<T1> const_pointer_cast(const shared_ptr<T2>& from) noexcept
    {
        if (!from)
            return nullptr;
        T1* p = const_cast<T1*>(from.get());
        return shared_ptr<T1>(from, p);
    }
    template<class T1, class T2>
    [[nodiscard]] inline shared_ptr<T1> reinterpret_pointer_cast(const shared_ptr<T2>& from) noexcept
    {
        if (!from)
            return nullptr;
        T1* p = reinterpret_cast<T1*>(from.get());
        return shared_ptr<T1>(from, p);
    }

    template<class T1, class T2> bool operator==(const shared_ptr<T1>& a, const shared_ptr<T2>& b) noexcept
    {
        return a.get() == b.get();
    }
    template<class T> bool operator==(const shared_ptr<T>& a, std::nullptr_t) noexcept
    {
        return !a;
    }
    template<class T> bool operator==(std::nullptr_t, const shared_ptr<T>& b) noexcept
    {
        return !b;
    }
    template<class T1, class T2> bool operator!=(const shared_ptr<T1>& a, const shared_ptr<T2>& b) noexcept
    {
        return a.get() != b.get();
    }
    template<class T> bool operator!=(const shared_ptr<T>& a, std::nullptr_t) noexcept
    {
        return static_cast<bool>(a);
    }
    template<class T> bool operator!=(std::nullptr_t, const shared_ptr<T>& b) noexcept
    {
        return static_cast<bool>(b);
    }

    // Add comparison operators for shared_ptr
    template<class T1, class T2> bool operator<(const shared_ptr<T1>& a, const shared_ptr<T2>& b) noexcept
    {
        return std::less<std::common_type_t<T1*, T2*>>{}(a.get(), b.get());
    }
    template<class T1, class T2> bool operator<=(const shared_ptr<T1>& a, const shared_ptr<T2>& b) noexcept
    {
        return !(b < a);
    }
    template<class T1, class T2> bool operator>(const shared_ptr<T1>& a, const shared_ptr<T2>& b) noexcept
    {
        return b < a;
    }
    template<class T1, class T2> bool operator>=(const shared_ptr<T1>& a, const shared_ptr<T2>& b) noexcept
    {
        return !(a < b);
    }

    template<typename T, typename... Args> shared_ptr<T> make_shared(Args&&... args)
    {
        // Commented out casting_interface static_assert to break circular dependency
        // static_assert(std::is_base_of<rpc::casting_interface, T>::value,
        //     "T must be a casting_interface for rpc::make_shared, "
        //     "even if initially local, to support potential RPC interactions later.");

        using Alloc = std::allocator<T>;
        using CBAllocForMakeShared =
            typename std::allocator_traits<Alloc>::template rebind_alloc<internal::control_block_make_shared<T, Alloc, Args...>>;

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
        return shared_ptr<T>(
            static_cast<internal::control_block_base*>(cb_ptr), static_cast<T*>(cb_ptr->get_managed_object_ptr()));
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
        void internal_set_weak_this(internal::control_block_base* cb_for_this_obj, ActualPtrType* ptr_to_this_obj) const
        {
            if (static_cast<const void*>(static_cast<const T*>(ptr_to_this_obj)) == static_cast<const void*>(this))
            {
                if (weak_this_.expired())
                {
                    if (cb_for_this_obj && ptr_to_this_obj)
                    {
                        shared_ptr<T> temp_sp(cb_for_this_obj,
                            const_cast<T*>(static_cast<const T*>(ptr_to_this_obj)),
                            typename shared_ptr<T>::for_enable_shared_tag{});
                        weak_this_ = temp_sp;
                    }
                }
            }
        }

    public:
        shared_ptr<T> shared_from_this() { return weak_this_.lock(); }
        shared_ptr<const T> shared_from_this() const { return weak_this_.lock(); }
        weak_ptr<T> weak_from_this() const noexcept { return weak_this_; }

        template<typename U, typename... Args> friend shared_ptr<U> make_shared(Args&&...);
        template<typename U> friend class shared_ptr;
    };

    // template<typename InterfaceType, typename ConcreteLocalProxyType>
    // rpc::shared_ptr<InterfaceType> create_shared_ptr_to_local_proxy(
    //     const rpc::shared_ptr<InterfaceType>& actual_object_sp_target,
    //     std::shared_ptr<rpc::object_proxy> obj_proxy_for_local_proxy_instance)
    // {
    //     static_assert(std::is_base_of<InterfaceType, ConcreteLocalProxyType>::value, "Proxy must implement interface.");
    //     static_assert(std::is_base_of<rpc::proxy_base, ConcreteLocalProxyType>::value, "Proxy must be a proxy_base.");

    //     ConcreteLocalProxyType* raw_local_proxy = new ConcreteLocalProxyType(actual_object_sp_target);

    //     if (auto* proxy_base_ptr = raw_local_proxy->query_proxy_base())
    //     {
    //         const_cast<rpc::proxy_base*>(proxy_base_ptr)->object_proxy_ = obj_proxy_for_local_proxy_instance;
    //     }

    //     return rpc::shared_ptr<InterfaceType>(static_cast<InterfaceType*>(raw_local_proxy),
    //         obj_proxy_for_local_proxy_instance,
    //         std::default_delete<ConcreteLocalProxyType>());
    // }

    // NAMESPACE_INLINE_END  // Commented out to simplify

} // namespace rpc

namespace std
{
    template<typename T> struct hash<rpc::shared_ptr<T>>
    {
        size_t operator()(const rpc::shared_ptr<T>& p) const noexcept { return std::hash<T*>()(p.get()); }
    };
    template<typename T> struct hash<rpc::optimistic_ptr<T>>
    {
        size_t operator()(const rpc::optimistic_ptr<T>& p) const noexcept
        {
            return std::hash<rpc::shared_ptr<T>>()(p.get_proxy_shared_ptr());
        }
    };
    // Commented out unique_ptr hash specialization - not needed
    // template<typename T, typename D> struct hash<rpc::unique_ptr<T, D>>
    // {
    //     size_t operator()(const rpc::unique_ptr<T, D>& p) const noexcept { return std::hash<T*>()(p.get()); }
    // };
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
