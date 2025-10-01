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
#include <typeinfo>    // For std::type_info used by get_deleter

#include <typeinfo> // For std::type_info used by get_deleter


// note that these classes borrow the std namespace for testing purposes in tests/std_test/tests
// for normal rpc mode these classes use the rpc namespace

#ifdef TEST_STL_COMPLIANCE

#define RPC_MEMORY std

namespace std
{
#else

#include "version.h"
#include "marshaller.h"
#include "member_ptr.h"
#include "coroutine_support.h" // Needed for CORO_TASK macro
#include "casting_interface.h"

#define RPC_MEMORY rpc

namespace rpc
{
    class object_proxy;
    class casting_interface;

#endif

    template<typename T> struct default_delete
    {
        constexpr default_delete() noexcept = default;

        template<typename U, typename = std::enable_if_t<std::is_convertible_v<U*, T*>>>
        default_delete(const default_delete<U>&) noexcept
        {
        }

        void operator()(T* ptr) const noexcept
        {
            static_assert(sizeof(T) > 0, "can't delete an incomplete type");
            delete ptr;
        }
    };

    class bad_weak_ptr : public std::exception
    {
    public:
        const char* what() const noexcept override { return "bad_weak_ptr"; }
    };

    // Forward declarations for circular dependency resolution
    template<typename T> class shared_ptr;
    template<typename T> class weak_ptr;

    namespace __rpc_internal
    {
#ifndef TEST_STL_COMPLIANCE
        // Generic check for casting_interface inheritance that handles multiple inheritance
        template<typename T, typename = void>
        struct is_casting_interface_derived : std::false_type
        {
        };

        template<typename T>
        struct is_casting_interface_derived<T, std::void_t<std::enable_if_t<
            std::is_base_of_v<rpc::casting_interface, std::remove_cv_t<T>> || std::is_same_v<std::remove_cv_t<T>, void>>>> : std::true_type
        {
        };
#endif        

        namespace __shared_ptr_pointer_utils
        {
            template<typename Y, typename Ty>
            struct sp_convertible : std::bool_constant<std::is_convertible_v<Y*, Ty*>>
            {
#ifndef TEST_STL_COMPLIANCE
                static_assert(is_casting_interface_derived<Y>::value,
                    "rpc::shared_ptr can only manage casting_interface-derived types or void");
                static_assert(is_casting_interface_derived<Ty>::value,
                    "rpc::shared_ptr can only manage casting_interface-derived types or void");
#endif
            };

            template<typename Y, typename Ty>
            struct sp_pointer_compatible : std::bool_constant<std::is_convertible_v<Y*, Ty*>>
            {
#ifndef TEST_STL_COMPLIANCE
                static_assert(is_casting_interface_derived<Y>::value,
                    "rpc::shared_ptr can only manage casting_interface-derived types or void");
                static_assert(is_casting_interface_derived<Ty>::value,
                    "rpc::shared_ptr can only manage casting_interface-derived types or void");
#endif
            };
        } // namespace __shared_ptr_pointer_utils

        namespace __shared_ptr_callable_traits
        {
            template<typename T, typename Enable = void> struct call_signature;

            template<typename R, typename... Args> struct call_signature<R(Args...), void>
            {
                using signature = R(Args...);
            };

            template<typename R, typename... Args>
            struct call_signature<R (*)(Args...), void> : call_signature<R(Args...)>
            {
            };

            template<typename R, typename... Args>
            struct call_signature<R (&)(Args...), void> : call_signature<R(Args...)>
            {
            };

            template<typename R, typename... Args>
            struct call_signature<R (*)(Args...) noexcept, void> : call_signature<R(Args...)>
            {
            };

            template<typename R, typename... Args>
            struct call_signature<R (&)(Args...) noexcept, void> : call_signature<R(Args...)>
            {
            };

            template<typename R, typename Class, typename... Args>
            struct call_signature<R (Class::*)(Args...), void> : call_signature<R(Args...)>
            {
            };

            template<typename R, typename Class, typename... Args>
            struct call_signature<R (Class::*)(Args...) const, void> : call_signature<R(Args...)>
            {
            };

            template<typename R, typename Class, typename... Args>
            struct call_signature<R (Class::*)(Args...) volatile, void> : call_signature<R(Args...)>
            {
            };

            template<typename R, typename Class, typename... Args>
            struct call_signature<R (Class::*)(Args...) const volatile, void> : call_signature<R(Args...)>
            {
            };

            template<typename R, typename Class, typename... Args>
            struct call_signature<R (Class::*)(Args...) noexcept, void> : call_signature<R(Args...)>
            {
            };

            template<typename R, typename Class, typename... Args>
            struct call_signature<R (Class::*)(Args...) const noexcept, void> : call_signature<R(Args...)>
            {
            };

            template<typename R, typename Class, typename... Args>
            struct call_signature<R (Class::*)(Args...) volatile noexcept, void> : call_signature<R(Args...)>
            {
            };

            template<typename R, typename Class, typename... Args>
            struct call_signature<R (Class::*)(Args...) const volatile noexcept, void> : call_signature<R(Args...)>
            {
            };

            template<typename R, typename Class, typename... Args>
            struct call_signature<R (Class::*)(Args...) &, void> : call_signature<R (Class::*)(Args...)>
            {
            };

            template<typename R, typename Class, typename... Args>
            struct call_signature<R (Class::*)(Args...) const&, void> : call_signature<R (Class::*)(Args...) const>
            {
            };

            template<typename R, typename Class, typename... Args>
            struct call_signature<R (Class::*)(Args...) volatile&, void> : call_signature<R (Class::*)(Args...) volatile>
            {
            };

            template<typename R, typename Class, typename... Args>
            struct call_signature<R (Class::*)(Args...) const volatile&, void>
                : call_signature<R (Class::*)(Args...) const volatile>
            {
            };

            template<typename R, typename Class, typename... Args>
            struct call_signature<R (Class::*)(Args...) &&, void> : call_signature<R (Class::*)(Args...)>
            {
            };

            template<typename R, typename Class, typename... Args>
            struct call_signature<R (Class::*)(Args...) const&&, void> : call_signature<R (Class::*)(Args...) const>
            {
            };

            template<typename R, typename Class, typename... Args>
            struct call_signature<R (Class::*)(Args...) volatile&&, void> : call_signature<R (Class::*)(Args...) volatile>
            {
            };

            template<typename R, typename Class, typename... Args>
            struct call_signature<R (Class::*)(Args...) const volatile&&, void>
                : call_signature<R (Class::*)(Args...) const volatile>
            {
            };

            template<typename R, typename Class, typename... Args>
            struct call_signature<R (Class::*)(Args...) & noexcept, void> : call_signature<R (Class::*)(Args...) noexcept>
            {
            };

            template<typename R, typename Class, typename... Args>
            struct call_signature<R (Class::*)(Args...) const & noexcept, void>
                : call_signature<R (Class::*)(Args...) const noexcept>
            {
            };

            template<typename R, typename Class, typename... Args>
            struct call_signature<R (Class::*)(Args...) volatile & noexcept, void>
                : call_signature<R (Class::*)(Args...) volatile noexcept>
            {
            };

            template<typename R, typename Class, typename... Args>
            struct call_signature<R (Class::*)(Args...) const volatile & noexcept, void>
                : call_signature<R (Class::*)(Args...) const volatile noexcept>
            {
            };

            template<typename R, typename Class, typename... Args>
            struct call_signature<R (Class::*)(Args...) && noexcept, void> : call_signature<R (Class::*)(Args...) noexcept>
            {
            };

            template<typename R, typename Class, typename... Args>
            struct call_signature<R (Class::*)(Args...) const && noexcept, void>
                : call_signature<R (Class::*)(Args...) const noexcept>
            {
            };

            template<typename R, typename Class, typename... Args>
            struct call_signature<R (Class::*)(Args...) volatile && noexcept, void>
                : call_signature<R (Class::*)(Args...) volatile noexcept>
            {
            };

            template<typename R, typename Class, typename... Args>
            struct call_signature<R (Class::*)(Args...) const volatile && noexcept, void>
                : call_signature<R (Class::*)(Args...) const volatile noexcept>
            {
            };

            template<typename F>
            struct call_signature<F, std::void_t<decltype(&std::decay_t<F>::operator())>>
                : call_signature<decltype(&std::decay_t<F>::operator())>
            {
            };

            template<typename F, typename = void> struct is_callable : std::false_type
            {
            };

            template<typename F>
            struct is_callable<F, std::void_t<typename call_signature<F>::signature>> : std::true_type
            {
            };
        } // namespace __shared_ptr_callable_traits

        namespace __shared_ptr_control_block
        {
#ifndef TEST_STL_COMPLIANCE
            // forward declarations implemented in object_proxy.cpp
            void object_proxy_add_ref(const std::shared_ptr<rpc::object_proxy>& ob, rpc::add_ref_options options);
            void object_proxy_release(const std::shared_ptr<rpc::object_proxy>& ob, rpc::release_options options);
#endif

            template<typename T> class shared_ptr;
            template<typename T> class weak_ptr;

            // NAMESPACE_INLINE_BEGIN  // Commented out to simplify
            struct control_block_base
            {
                std::atomic<long> shared_owners{0};
                std::atomic<long> weak_owners{1};
#ifndef TEST_STL_COMPLIANCE
                std::atomic<long> optimistic_owners{0};
                bool is_local = false;
#endif

            protected:
                void* managed_object_ptr_{nullptr};

            public:
                control_block_base(void* obj_ptr)
                    : managed_object_ptr_(obj_ptr)
                {
#ifndef TEST_STL_COMPLIANCE
                    rpc::casting_interface* ptr = reinterpret_cast<rpc::casting_interface*>(managed_object_ptr_);
                    if(ptr)
                        is_local = ptr->is_local();
#endif
                }

                control_block_base()
                    : managed_object_ptr_(nullptr)
                {
                }

                virtual ~control_block_base() = default;
                virtual void dispose_object_actual() = 0;
                virtual void destroy_self_actual() = 0;
                virtual void* get_deleter_ptr(const std::type_info&) noexcept { return nullptr; }

                void* get_managed_object_ptr() const { return managed_object_ptr_; }

                void increment_shared()
                {
                    long prev = shared_owners.fetch_add(1, std::memory_order_relaxed);
#ifndef TEST_STL_COMPLIANCE
                    // Call object_proxy add_ref on 0→1 transition for remote objects
                    if (prev == 0)
                    {
                        control_block_call_add_ref(add_ref_options::normal);
                    }
#endif
                }
                void increment_weak() { weak_owners.fetch_add(1, std::memory_order_relaxed); }

                // Try to increment shared count only if it's not zero (thread-safe)
                bool try_increment_shared() noexcept
                {
                    long current = shared_owners.load(std::memory_order_relaxed);
                    do
                    {
                        if (current == 0)
                        {
                            return false; // Already expired, cannot increment
                        }
                    } while (!shared_owners.compare_exchange_weak(current, current + 1, std::memory_order_relaxed));
                    return true;
                }

                void decrement_shared_and_dispose_if_zero()
                {
                    if (shared_owners.fetch_sub(1, std::memory_order_acq_rel) == 1)
                    {
#ifndef TEST_STL_COMPLIANCE
                        // Call object_proxy release on 1→0 transition for remote objects
                        control_block_call_release(release_options::normal);
#endif
                        dispose_object_actual();
                        decrement_weak_and_destroy_if_zero();
                    }
                }

                void decrement_weak_and_destroy_if_zero()
                {
                    if (weak_owners.fetch_sub(1, std::memory_order_acq_rel) == 1)
                    {
                        if (shared_owners.load(std::memory_order_acquire) == 0)
                        {
                            destroy_self_actual();
                        }
                    }
                }

#ifndef TEST_STL_COMPLIANCE
                // Fast increment when control block is guaranteed alive (e.g., copying from valid optimistic_ptr)
                void increment_optimistic_no_lock() noexcept
                {
                    long prev = optimistic_owners.fetch_add(1, std::memory_order_relaxed);
                    // Call object_proxy add_ref on 0→1 transition for remote objects
                    if (prev == 0)
                    {
                        control_block_call_add_ref(add_ref_options::optimistic);
                    }
                }

                // Safe increment when control block lifetime is uncertain (e.g., converting from shared_ptr/weak_ptr)
                // Returns true if successful, false if control block is expired
                bool try_increment_optimistic() noexcept
                {
                    // First, ensure control block stays alive during our operation
                    long weak_count = weak_owners.load(std::memory_order_relaxed);

                    do
                    {
                        // If weak_count is 0, control block is being/has been destroyed
                        if (weak_count == 0)
                        {
                            return false;
                        }
                        // Try to increment weak_owners to keep control block alive
                    } while (!weak_owners.compare_exchange_weak(weak_count, weak_count + 1,
                                                                 std::memory_order_acquire,
                                                                 std::memory_order_relaxed));

                    // Control block is now guaranteed alive, safe to check optimistic_owners
                    long prev = optimistic_owners.fetch_add(1, std::memory_order_relaxed);

                    if (prev == 0)
                    {
                        // First optimistic_ptr: weak_owners already incremented above
                        // This increment becomes the "optimistic_ptr weak_owner"
                        // Call object_proxy add_ref on 0→1 transition for remote objects
                        control_block_call_add_ref(add_ref_options::optimistic);
                    }
                    else
                    {
                        // Not the first optimistic_ptr: we pre-emptively incremented weak_owners
                        // Undo that increment since weak_owners is only for control block lifetime
                        decrement_weak_and_destroy_if_zero();
                    }

                    return true;
                }

                void decrement_optimistic_and_dispose_if_zero() noexcept
                {
                    long prev = optimistic_owners.fetch_sub(1, std::memory_order_acq_rel);

                    // If this was the last optimistic_ptr, decrement weak_owners
                    // CRITICAL: This must be done last to prevent control block destruction
                    // while other threads might be incrementing optimistic_owners
                    if (prev == 1)
                    {
                        // Call object_proxy release on 1→0 transition for remote objects
                        control_block_call_release(release_options::optimistic);

                        // Last optimistic_ptr: decrement the weak_owners that was added by first optimistic_ptr
                        decrement_weak_and_destroy_if_zero();
                    }
                }
                
                inline void control_block_call_add_ref(rpc::add_ref_options options) noexcept
                {
                    if (managed_object_ptr_ && !is_local)
                    {
                        auto ci = reinterpret_cast<::rpc::casting_interface*>(managed_object_ptr_);
                        if (auto obj_proxy = ci->get_object_proxy())
                        {
                            object_proxy_add_ref(obj_proxy, options);
                        }
                    }
                }

                inline void control_block_call_release(::rpc::release_options options) noexcept
                {
                    if (managed_object_ptr_ && !is_local)
                    {
                        auto ci = reinterpret_cast<::rpc::casting_interface*>(managed_object_ptr_);
                        if (auto obj_proxy = ci->get_object_proxy())
                        {
                            object_proxy_release(obj_proxy, options);
                        }
                    }
                }
#endif
            };
            

            template<typename T> static void* to_void_ptr(T* p)
            {
                if constexpr (std::is_function_v<T>)
                {
                    return reinterpret_cast<void*>(p);
                }
                else
                {
                    return static_cast<void*>(const_cast<std::remove_cv_t<T>*>(p));
                }
            }

            template<typename T> struct control_block_impl_default_delete : public control_block_base
            {
                control_block_impl_default_delete(T* p)
                    : control_block_base(to_void_ptr(p))
                {
                }

                void dispose_object_actual() override
                {
                    if (managed_object_ptr_)
                    {
                        default_delete<T>{}(static_cast<T*>(managed_object_ptr_));
                        managed_object_ptr_ = nullptr;
                    }
                }

                void destroy_self_actual() override { delete this; }

                void* get_deleter_ptr(const std::type_info&) noexcept override { return nullptr; }
            };

            template<typename T, typename Deleter> struct control_block_impl_with_deleter : public control_block_base
            {
                Deleter object_deleter_;
                control_block_impl_with_deleter(T* p, Deleter d)
                    : control_block_base(to_void_ptr(p))
                    , object_deleter_(std::move(d))
                {
                }

                void dispose_object_actual() override
                {
                    if (managed_object_ptr_)
                    {
                        T* obj_ptr;
                        if constexpr (std::is_function_v<T>)
                        {
                            obj_ptr = reinterpret_cast<T*>(managed_object_ptr_);
                        }
                        else
                        {
                            obj_ptr = static_cast<T*>(managed_object_ptr_);
                        }
                        object_deleter_(obj_ptr);
                        managed_object_ptr_ = nullptr;
                    }
                }

                void destroy_self_actual() override { delete this; }

                void* get_deleter_ptr(const std::type_info& type) noexcept override
                {
                    if (type == typeid(Deleter))
                    {
                        return static_cast<void*>(&object_deleter_);
                    }
                    return nullptr;
                }
            };

            template<typename T, typename Deleter, typename Alloc>
            struct control_block_impl_with_deleter_alloc : public control_block_base
            {
                Deleter object_deleter_;
                Alloc control_block_allocator_;
                control_block_impl_with_deleter_alloc(T* p, Deleter d, Alloc a)
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
                        if constexpr (std::is_function_v<T>)
                        {
                            obj_ptr = reinterpret_cast<T*>(managed_object_ptr_);
                        }
                        else
                        {
                            obj_ptr = static_cast<T*>(managed_object_ptr_);
                        }
                        object_deleter_(obj_ptr);
                        managed_object_ptr_ = nullptr;
                    }
                }

                void destroy_self_actual() override
                {
                    using ThisType = control_block_impl_with_deleter_alloc<T, Deleter, Alloc>;
                    using ReboundAlloc = typename std::allocator_traits<Alloc>::template rebind_alloc<ThisType>;
                    ReboundAlloc rebound_alloc(control_block_allocator_);
                    std::allocator_traits<ReboundAlloc>::destroy(rebound_alloc, this);
                    std::allocator_traits<ReboundAlloc>::deallocate(rebound_alloc, this, 1);
                }

                void* get_deleter_ptr(const std::type_info& type) noexcept override
                {
                    if (type == typeid(Deleter))
                    {
                        return static_cast<void*>(&object_deleter_);
                    }
                    return nullptr;
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
                    ::new (const_cast<void*>(static_cast<const void*>(&object_instance_)))
                        T(std::forward<ConcreteArgs>(args_for_t)...);
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
                void* get_deleter_ptr(const std::type_info&) noexcept override { return nullptr; }
                ~control_block_make_shared() { }
            };
        } // namespace __shared_ptr_control_block

        namespace __shared_ptr_make_support
        {
            template<typename T, typename ValueAlloc, typename... Args>
            shared_ptr<T> make_shared_with_value_alloc(const ValueAlloc&, Args&&...);
        }

        // Forward declaration - definition moved after enable_shared_from_this
        template<typename T, typename Y>
        void try_enable_shared_from_this(shared_ptr<T>& sp, Y* ptr) noexcept;

    } // namespace __rpc_internal

    template<typename T> class shared_ptr
    {
        using element_type_impl = std::remove_extent_t<T>;

        static_assert(!std::is_array_v<T>, "shared_ptr no longer supports array types");

#ifndef TEST_STL_COMPLIANCE
        template<typename Candidate>
        static constexpr void assert_casting_interface()
        {
            using clean_t = std::remove_cv_t<std::remove_reference_t<Candidate>>;
            static_assert(__rpc_internal::is_casting_interface_derived<clean_t>::value,
                "rpc::shared_ptr can only manage casting_interface-derived types");
        }
#endif

        element_type_impl* ptr_{nullptr};
        __rpc_internal::__shared_ptr_control_block::control_block_base* cb_{nullptr};

        template<typename Y> using is_ptr_convertible = __rpc_internal::__shared_ptr_pointer_utils::sp_convertible<Y, T>;

        template<typename Y> using is_pointer_compatible = __rpc_internal::__shared_ptr_pointer_utils::sp_pointer_compatible<Y, T>;

        void acquire_this() noexcept
        {
            if (cb_)
                cb_->increment_shared();
        }

        void release_this() noexcept
        {
            if (cb_)
                cb_->decrement_shared_and_dispose_if_zero();
        }

        shared_ptr(__rpc_internal::__shared_ptr_control_block::control_block_base* cb, element_type_impl* p)
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
        shared_ptr(__rpc_internal::__shared_ptr_control_block::control_block_base* cb, element_type_impl* p, for_enable_shared_tag)
            : ptr_(p)
            , cb_(cb)
        {
            // Don't call acquire_this() here - this is for enable_shared_from_this internal use
            // The reference count is already correct from the original shared_ptr
        }

    public:
        using element_type = element_type_impl;
        using weak_type = weak_ptr<T>;

        // Tag type for internal construction (for use by friends)
        struct internal_construct_tag {};

        constexpr shared_ptr() noexcept = default;
        constexpr shared_ptr(std::nullptr_t) noexcept { }

        template<typename Y, typename = std::enable_if_t<is_ptr_convertible<Y>::value>>
        explicit shared_ptr(Y* p)
            : ptr_(static_cast<element_type_impl*>(p))
            , cb_(nullptr)
        {
#ifndef TEST_STL_COMPLIANCE
            assert_casting_interface<Y>();
#endif
            try
            {
                cb_ = new __rpc_internal::__shared_ptr_control_block::control_block_impl_default_delete<Y>(p);
            }
            catch (...)
            {
                ptr_ = nullptr;
                cb_ = nullptr;
                throw;
            }
            acquire_this();
            if (ptr_)
                __rpc_internal::try_enable_shared_from_this(*this, p);
        }

        template<typename Y, typename Deleter, typename = std::enable_if_t<is_ptr_convertible<Y>::value>>
        shared_ptr(Y* p, Deleter d)
            : ptr_(static_cast<element_type_impl*>(p))
            , cb_(nullptr)
        {
#ifndef TEST_STL_COMPLIANCE
            assert_casting_interface<Y>();
#endif
            try
            {
                using CleanDeleter = std::decay_t<Deleter>;
                cb_ = new __rpc_internal::__shared_ptr_control_block::control_block_impl_with_deleter<Y, CleanDeleter>(
                    p, CleanDeleter(std::move(d)));
            }
            catch (...)
            {
                ptr_ = nullptr;
                cb_ = nullptr;
                throw;
            }
            acquire_this();
            if (ptr_)
                __rpc_internal::try_enable_shared_from_this(*this, p);
        }

        template<typename Deleter>
        shared_ptr(std::nullptr_t, Deleter d)
            : ptr_(nullptr)
            , cb_(nullptr)
        {
            try
            {
                using CleanDeleter = std::decay_t<Deleter>;
                cb_ = new __rpc_internal::__shared_ptr_control_block::control_block_impl_with_deleter<T, CleanDeleter>(
                    nullptr, CleanDeleter(std::move(d)));
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

        template<typename Y, typename Deleter, typename Alloc, typename = std::enable_if_t<is_ptr_convertible<Y>::value>>
        shared_ptr(Y* p, Deleter d, Alloc cb_alloc)
            : ptr_(static_cast<element_type_impl*>(p))
            , cb_(nullptr)
        {
#ifndef TEST_STL_COMPLIANCE
            assert_casting_interface<Y>();
#endif
            try
            {
                using CleanDeleter = std::decay_t<Deleter>;
                using CleanAlloc = std::decay_t<Alloc>;
                if constexpr (std::is_same_v<CleanAlloc, std::allocator<char>>)
                {
                    cb_ = new __rpc_internal::__shared_ptr_control_block::control_block_impl_with_deleter<Y, CleanDeleter>(
                        p, CleanDeleter(std::move(d)));
                }
                else
                {
                    CleanDeleter deleter_copy(std::move(d));
                    CleanAlloc alloc_copy(cb_alloc);
                    using CBAlloc = typename std::allocator_traits<CleanAlloc>::template rebind_alloc<
                        __rpc_internal::__shared_ptr_control_block::control_block_impl_with_deleter_alloc<Y, CleanDeleter, CleanAlloc>>;
                    CBAlloc cb_allocator(alloc_copy);
                    auto* storage = std::allocator_traits<CBAlloc>::allocate(cb_allocator, 1);
                    try
                    {
                        std::allocator_traits<CBAlloc>::construct(
                            cb_allocator, storage, p, std::move(deleter_copy), alloc_copy);
                    }
                    catch (...)
                    {
                        std::allocator_traits<CBAlloc>::deallocate(cb_allocator, storage, 1);
                        throw;
                    }
                    cb_ = storage;
                }
            }
            catch (...)
            {
                ptr_ = nullptr;
                cb_ = nullptr;
                throw;
            }
            acquire_this();
            if (ptr_)
                __rpc_internal::try_enable_shared_from_this(*this, p);
        }

        template<typename Deleter, typename Alloc>
        shared_ptr(std::nullptr_t, Deleter d, Alloc cb_alloc)
            : ptr_(nullptr)
            , cb_(nullptr)
        {
            try
            {
                using CleanDeleter = std::decay_t<Deleter>;
                using CleanAlloc = std::decay_t<Alloc>;
                CleanDeleter deleter_copy(std::move(d));
                CleanAlloc alloc_copy(cb_alloc);
                using CBAlloc = typename std::allocator_traits<CleanAlloc>::template rebind_alloc<
                    __rpc_internal::__shared_ptr_control_block::control_block_impl_with_deleter_alloc<T, CleanDeleter, CleanAlloc>>;
                CBAlloc cb_allocator(alloc_copy);
                auto* storage = std::allocator_traits<CBAlloc>::allocate(cb_allocator, 1);
                try
                {
                    std::allocator_traits<CBAlloc>::construct(
                        cb_allocator, storage, nullptr, std::move(deleter_copy), alloc_copy);
                }
                catch (...)
                {
                    std::allocator_traits<CBAlloc>::deallocate(cb_allocator, storage, 1);
                    throw;
                }
                cb_ = storage;
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

        template<typename Y, typename = std::enable_if_t<is_pointer_compatible<Y>::value>>
        shared_ptr(const shared_ptr<Y>& r) noexcept
            : ptr_(static_cast<element_type_impl*>(r.internal_get_ptr()))
            , cb_(r.internal_get_cb())
        {
            acquire_this();
        }
        template<typename Y, typename = std::enable_if_t<is_pointer_compatible<Y>::value>>
        shared_ptr(shared_ptr<Y>&& r) noexcept
            : ptr_(static_cast<element_type_impl*>(r.internal_get_ptr()))
            , cb_(r.internal_get_cb())
        {
            r.cb_ = nullptr;
            r.ptr_ = nullptr;
        }

        template<typename Y, typename = std::enable_if_t<is_pointer_compatible<Y>::value>>
        explicit shared_ptr(const weak_ptr<Y>& r)
        {
            shared_ptr<Y> temp = r.lock();
            if (!temp)
            {
                throw bad_weak_ptr();
            }
            shared_ptr<T> temp_T(std::move(temp));
            this->swap(temp_T);
        }

        ~shared_ptr() { release_this(); }
        shared_ptr& operator=(const shared_ptr& r) noexcept
        {
            shared_ptr(r).swap(*this);
            return *this;
        }
        template<typename Y, typename = std::enable_if_t<is_pointer_compatible<Y>::value>>
        shared_ptr& operator=(const shared_ptr<Y>& r) noexcept
        {
            shared_ptr(r).swap(*this);
            return *this;
        }
        shared_ptr& operator=(shared_ptr&& r) noexcept
        {
            shared_ptr(std::move(r)).swap(*this);
            return *this;
        }
        template<typename Y, typename = std::enable_if_t<is_pointer_compatible<Y>::value>>
        shared_ptr& operator=(shared_ptr<Y>&& r) noexcept
        {
            shared_ptr(std::move(r)).swap(*this);
            return *this;
        }
        shared_ptr& operator=(std::nullptr_t) noexcept
        {
            reset();
            return *this;
        }

        void reset() noexcept { shared_ptr().swap(*this); }
        template<typename Y,
            typename Deleter = default_delete<Y>,
            typename Alloc = std::allocator<char>,
            typename = std::enable_if_t<is_ptr_convertible<Y>::value>>
        void reset(Y* p, Deleter d = Deleter(), Alloc cb_alloc = Alloc())
        {
            shared_ptr(p, std::move(d), std::move(cb_alloc)).swap(*this);
        }

        void swap(shared_ptr& r) noexcept
        {
            std::swap(ptr_, r.ptr_);
            std::swap(cb_, r.cb_);
        }

        element_type_impl* get() const noexcept { return ptr_; }

        template<typename U = T>
        std::enable_if_t<!std::is_void_v<U> && !std::is_array_v<U>, std::remove_extent_t<U>&> operator*() const noexcept
        {
            using result_type = std::remove_extent_t<U>;
            return *static_cast<result_type*>(ptr_);
        }

        template<typename U = T>
        std::enable_if_t<!std::is_void_v<U> && !std::is_array_v<U>, std::remove_extent_t<U>*> operator->() const noexcept
        {
            using result_type = std::remove_extent_t<U>;
            return static_cast<result_type*>(ptr_);
        }
        long use_count() const noexcept { return cb_ ? cb_->shared_owners.load(std::memory_order_relaxed) : 0; }
        bool unique() const noexcept { return use_count() == 1; }
        explicit operator bool() const noexcept { return ptr_ != nullptr; }

        template<typename Y> bool owner_before(const shared_ptr<Y>& other) const noexcept
        {
            return cb_ < other.internal_get_cb();
        }

        template<typename Y> bool owner_before(const weak_ptr<Y>& other) const noexcept
        {
            return cb_ < other.internal_get_cb();
        }

    private:
        // Internal accessors - NOT part of STL shared_ptr API
        __rpc_internal::__shared_ptr_control_block::control_block_base* internal_get_cb() const { return cb_; }
        element_type_impl* internal_get_ptr() const { return ptr_; }
        // Private constructor for constructing from existing control block (for friends only)
        shared_ptr(__rpc_internal::__shared_ptr_control_block::control_block_base* cb,
                   element_type_impl* ptr,
                   internal_construct_tag) noexcept
            : ptr_(ptr), cb_(cb)
        {
            // Caller is responsible for having already incremented the control block
        }

    public:
        template<typename U> friend class weak_ptr;
        template<typename U>
        friend class shared_ptr; // Allow different template instantiations to access private members
        template<typename U, typename... Args> friend shared_ptr<U> make_shared(Args&&... args);
        template<typename U, typename Alloc, typename... Args>
        friend shared_ptr<U> allocate_shared(const Alloc& alloc, Args&&... args);
        template<typename U, typename ValueAlloc, typename... Args>
        friend shared_ptr<U> __rpc_internal::__shared_ptr_make_support::make_shared_with_value_alloc(
            const ValueAlloc&, Args&&...);
        template<class T1_cast, class T2_cast>
        friend shared_ptr<T1_cast> dynamic_pointer_cast(const shared_ptr<T2_cast>& from) noexcept;
        template<typename U> friend class enable_shared_from_this;
        template<typename T2, typename Y2>
        friend void __rpc_internal::try_enable_shared_from_this(shared_ptr<T2>& sp, Y2* ptr) noexcept;
        template<typename Deleter, typename U> friend Deleter* get_deleter(const shared_ptr<U>& p) noexcept;
#ifndef TEST_STL_COMPLIANCE
        template<typename U> friend class optimistic_ptr;
        template<typename U> friend class local_optimistic_ptr;
#endif
    };

    template<typename T> class weak_ptr
    {
        using element_type_impl = std::remove_extent_t<T>;

        static_assert(!std::is_array_v<T>, "weak_ptr no longer supports array types");

        __rpc_internal::__shared_ptr_control_block::control_block_base* cb_{nullptr};
        element_type_impl* ptr_for_lock_{nullptr};

        template<typename Y> using is_pointer_compatible = __rpc_internal::__shared_ptr_pointer_utils::sp_pointer_compatible<Y, T>;

        template<typename SourceElement>
        static element_type_impl* convert_ptr_for_lock(
            __rpc_internal::__shared_ptr_control_block::control_block_base* source_cb, SourceElement* source_ptr) noexcept
        {
            if (!source_ptr)
                return nullptr;

            using source_base_t = std::remove_cv_t<SourceElement>;
            using dest_base_t = std::remove_cv_t<element_type_impl>;

            if constexpr (std::is_same_v<source_base_t, dest_base_t>)
            {
                return static_cast<element_type_impl*>(source_ptr);
            }
            else
            {
                if (!source_cb || source_cb->get_managed_object_ptr() == nullptr)
                    return nullptr;
                return static_cast<element_type_impl*>(source_ptr);
            }
        }

        // Thread-safe conversion method that avoids race conditions with virtual inheritance
        template<typename Y> void weakly_convert_avoiding_expired_conversions(const weak_ptr<Y>& other) noexcept
        {
            if (other.cb_)
            {
                cb_ = other.cb_; // Always share control block first
                cb_->increment_weak();

                // Try to temporarily lock the object to safely convert the pointer
                if (cb_->try_increment_shared())
                {
                    ptr_for_lock_
                        = convert_ptr_for_lock<typename weak_ptr<Y>::element_type>(other.cb_, other.ptr_for_lock_);
                    cb_->decrement_shared_and_dispose_if_zero();
                }
                else
                {
                    // Object is already expired, pointer stays null
                    ptr_for_lock_ = nullptr;
                }
            }
            else
            {
                cb_ = nullptr;
                ptr_for_lock_ = nullptr;
            }
        }

        // Fast but potentially unsafe conversion (no temporary lock)
        template<typename Y> void weakly_construct_from(const weak_ptr<Y>& other) noexcept
        {
            cb_ = other.cb_;
            ptr_for_lock_ = convert_ptr_for_lock<typename weak_ptr<Y>::element_type>(other.cb_, other.ptr_for_lock_);
            if (cb_)
                cb_->increment_weak();
        }

        // Thread-safe move conversion
        template<typename Y> void weakly_convert_move_avoiding_expired_conversions(weak_ptr<Y>&& other) noexcept
        {
            if (other.cb_)
            {
                cb_ = other.cb_; // Take ownership of control block
                cb_->increment_weak();

                // Try to temporarily lock the object to safely convert the pointer
                if (cb_->try_increment_shared())
                {
                    ptr_for_lock_
                        = convert_ptr_for_lock<typename weak_ptr<Y>::element_type>(other.cb_, other.ptr_for_lock_);
                    cb_->decrement_shared_and_dispose_if_zero();
                }
                else
                {
                    // Object is already expired, pointer stays null
                    ptr_for_lock_ = nullptr;
                }

                // Clear the other weak_ptr
                other.cb_->decrement_weak_and_destroy_if_zero();
                other.cb_ = nullptr;
                other.ptr_for_lock_ = nullptr;
            }
            else
            {
                cb_ = nullptr;
                ptr_for_lock_ = nullptr;
            }
        }

        // Fast move (no temporary lock)
        template<typename Y> void weakly_move_construct_from(weak_ptr<Y>&& other) noexcept
        {
            cb_ = other.cb_;
            ptr_for_lock_ = convert_ptr_for_lock<typename weak_ptr<Y>::element_type>(other.cb_, other.ptr_for_lock_);
            other.cb_ = nullptr;
            other.ptr_for_lock_ = nullptr;
        }

    public:
        using element_type = element_type_impl;

    private:
        // Helper for _Must_avoid_expired_conversions_from SFINAE detection
        template<class _Ty2, class = void> struct _Can_static_cast_to : std::true_type
        {
        };

        template<class _Ty2>
        struct _Can_static_cast_to<_Ty2, std::void_t<decltype(static_cast<const _Ty2*>(static_cast<T*>(nullptr)))>>
            : std::false_type
        {
        };

    public:
        // _Must_avoid_expired_conversions_from - detects virtual inheritance cases where conversion optimization is unsafe
        // When static_cast from T* to const _Ty2* succeeds, we can optimize (return false)
        // When static_cast fails (SFINAE), we must avoid optimization (return true)
        template<class _Ty2>
        static constexpr bool _Must_avoid_expired_conversions_from = _Can_static_cast_to<_Ty2>::value;

        constexpr weak_ptr() noexcept = default;

        template<typename Y, typename = std::enable_if_t<is_pointer_compatible<Y>::value>>
        weak_ptr(const shared_ptr<Y>& r) noexcept
            : cb_(r.internal_get_cb())
            , ptr_for_lock_(r.internal_get_ptr())
        {
            if (cb_)
                cb_->increment_weak();
        }

        weak_ptr(const weak_ptr& r) noexcept
            : cb_(r.cb_)
            , ptr_for_lock_(r.ptr_for_lock_)
        {
            if (cb_)
                cb_->increment_weak();
        }
        template<typename Y, typename = std::enable_if_t<is_pointer_compatible<Y>::value>>
        weak_ptr(const weak_ptr<Y>& r) noexcept
        {
            // Use thread-safe conversion for virtual inheritance cases, fast path otherwise
            constexpr bool avoid_expired_conversions = _Must_avoid_expired_conversions_from<Y>;
            if constexpr (avoid_expired_conversions)
            {
                weakly_convert_avoiding_expired_conversions(r);
            }
            else
            {
                weakly_construct_from(r);
            }
        }

        weak_ptr(weak_ptr&& r) noexcept
            : cb_(r.cb_)
            , ptr_for_lock_(r.ptr_for_lock_)
        {
            r.cb_ = nullptr;
            r.ptr_for_lock_ = nullptr;
        }
        template<typename Y,
            typename = std::enable_if_t<std::is_convertible_v<typename weak_ptr<Y>::element_type*, element_type_impl*>>>
        weak_ptr(weak_ptr<Y>&& r) noexcept
        {
            // Use thread-safe conversion for virtual inheritance cases, fast path otherwise
            constexpr bool avoid_expired_conversions = _Must_avoid_expired_conversions_from<Y>;
            if constexpr (avoid_expired_conversions)
            {
                weakly_convert_move_avoiding_expired_conversions(std::move(r));
            }
            else
            {
                weakly_move_construct_from(std::move(r));
            }
        }

        ~weak_ptr()
        {
            if (cb_)
                cb_->decrement_weak_and_destroy_if_zero();
        }

        weak_ptr& operator=(const weak_ptr& r) noexcept
        {
            weak_ptr(r).swap(*this);
            return *this;
        }
        template<typename Y, typename = std::enable_if_t<is_pointer_compatible<Y>::value>>
        weak_ptr& operator=(const weak_ptr<Y>& r) noexcept
        {
            weak_ptr(r).swap(*this);
            return *this;
        }
        template<typename Y, typename = std::enable_if_t<is_pointer_compatible<Y>::value>>
        weak_ptr& operator=(const shared_ptr<Y>& r) noexcept
        {
            weak_ptr(r).swap(*this);
            return *this;
        }
        weak_ptr& operator=(weak_ptr&& r) noexcept
        {
            weak_ptr(std::move(r)).swap(*this);
            return *this;
        }
        template<typename Y, typename = std::enable_if_t<is_pointer_compatible<Y>::value>>
        weak_ptr& operator=(weak_ptr<Y>&& r) noexcept
        {
            weak_ptr(std::move(r)).swap(*this);
            return *this;
        }

        shared_ptr<T> lock() const
        {
            if (!cb_)
                return {};
            long c = cb_->shared_owners.load(std::memory_order_relaxed);
            while (c > 0)
            {
                if (cb_->shared_owners.compare_exchange_weak(
                        c, c + 1, std::memory_order_acq_rel, std::memory_order_relaxed))
                    return shared_ptr<T>(cb_, ptr_for_lock_);
            }
            return {};
        }
        long use_count() const noexcept { return cb_ ? cb_->shared_owners.load(std::memory_order_relaxed) : 0; }
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

        template<typename Y> bool owner_before(const shared_ptr<Y>& other) const noexcept
        {
            return cb_ < other.internal_get_cb();
        }

        template<typename Y> bool owner_before(const weak_ptr<Y>& other) const noexcept { return cb_ < other.cb_; }

    private:
        // Internal accessors - NOT part of STL weak_ptr API
        __rpc_internal::__shared_ptr_control_block::control_block_base* internal_get_cb() const { return cb_; }

        template<typename U> friend class shared_ptr;
        template<typename U> friend class enable_shared_from_this;
        template<typename U> friend class weak_ptr;
    };

    template<typename T = void> struct owner_less;

    template<typename T> struct owner_less
    {
        using is_transparent = void;

        bool operator()(const shared_ptr<T>& lhs, const shared_ptr<T>& rhs) const noexcept
        {
            return lhs.owner_before(rhs);
        }
        bool operator()(const shared_ptr<T>& lhs, const weak_ptr<T>& rhs) const noexcept
        {
            return lhs.owner_before(rhs);
        }
        bool operator()(const weak_ptr<T>& lhs, const shared_ptr<T>& rhs) const noexcept
        {
            return lhs.owner_before(rhs);
        }
        bool operator()(const weak_ptr<T>& lhs, const weak_ptr<T>& rhs) const noexcept { return lhs.owner_before(rhs); }
    };

    // STL-compliant specialization for shared_ptr<T>
    template<typename T> struct owner_less<shared_ptr<T>>
    {
        using is_transparent = void;

        bool operator()(const shared_ptr<T>& lhs, const shared_ptr<T>& rhs) const noexcept
        {
            return lhs.owner_before(rhs);
        }
        bool operator()(const shared_ptr<T>& lhs, const weak_ptr<T>& rhs) const noexcept
        {
            return lhs.owner_before(rhs);
        }
        bool operator()(const weak_ptr<T>& lhs, const shared_ptr<T>& rhs) const noexcept
        {
            return lhs.owner_before(rhs);
        }
        bool operator()(const weak_ptr<T>& lhs, const weak_ptr<T>& rhs) const noexcept { return lhs.owner_before(rhs); }

        // STL compatibility: deprecated nested types (until C++20)
        using result_type = bool;
        using first_argument_type = shared_ptr<T>;
        using second_argument_type = shared_ptr<T>;
    };

    // STL-compliant specialization for weak_ptr<T>
    template<typename T> struct owner_less<weak_ptr<T>>
    {
        using is_transparent = void;

        bool operator()(const shared_ptr<T>& lhs, const shared_ptr<T>& rhs) const noexcept
        {
            return lhs.owner_before(rhs);
        }
        bool operator()(const shared_ptr<T>& lhs, const weak_ptr<T>& rhs) const noexcept
        {
            return lhs.owner_before(rhs);
        }
        bool operator()(const weak_ptr<T>& lhs, const shared_ptr<T>& rhs) const noexcept
        {
            return lhs.owner_before(rhs);
        }
        bool operator()(const weak_ptr<T>& lhs, const weak_ptr<T>& rhs) const noexcept { return lhs.owner_before(rhs); }

        // STL compatibility: deprecated nested types (until C++20)
        using result_type = bool;
        using first_argument_type = weak_ptr<T>;
        using second_argument_type = weak_ptr<T>;
    };

    template<> struct owner_less<void>
    {
        using is_transparent = void;

        template<typename T, typename U>
        bool operator()(const shared_ptr<T>& lhs, const shared_ptr<U>& rhs) const noexcept
        {
            return lhs.owner_before(rhs);
        }

        template<typename T, typename U>
        bool operator()(const shared_ptr<T>& lhs, const weak_ptr<U>& rhs) const noexcept
        {
            return lhs.owner_before(rhs);
        }

        template<typename T, typename U>
        bool operator()(const weak_ptr<T>& lhs, const shared_ptr<U>& rhs) const noexcept
        {
            return lhs.owner_before(rhs);
        }

        template<typename T, typename U> bool operator()(const weak_ptr<T>& lhs, const weak_ptr<U>& rhs) const noexcept
        {
            return lhs.owner_before(rhs);
        }
    };

    template<typename T> shared_ptr(const weak_ptr<T>&) -> shared_ptr<T>;

    template<typename T> shared_ptr(weak_ptr<T>&&) -> shared_ptr<T>;

    template<typename T> weak_ptr(const shared_ptr<T>&) -> weak_ptr<T>;

    template<typename T> weak_ptr(const weak_ptr<T>&) -> weak_ptr<T>;

    template<typename T> weak_ptr(weak_ptr<T>&&) -> weak_ptr<T>;

    owner_less() -> owner_less<void>;

    template<typename T> owner_less(const owner_less<T>&) -> owner_less<T>;

    namespace __rpc_internal
    {
        namespace __shared_ptr_make_support
        {
            template<typename T, typename ValueAlloc, typename... Args>
            shared_ptr<T> make_shared_with_value_alloc(const ValueAlloc& value_alloc, Args&&... args)
            {
#ifndef TEST_STL_COMPLIANCE
                shared_ptr<T>::template assert_casting_interface<std::remove_extent_t<T>>();
#endif
                using ControlBlockAlloc = typename std::allocator_traits<ValueAlloc>::template rebind_alloc<
                    __shared_ptr_control_block::control_block_make_shared<T, ValueAlloc, Args...>>;
                using ControlBlockAllocTraits = std::allocator_traits<ControlBlockAlloc>;

                ControlBlockAlloc cb_alloc(value_alloc);
                auto* cb_ptr = ControlBlockAllocTraits::allocate(cb_alloc, 1);

                try
                {
                    ControlBlockAllocTraits::construct(cb_alloc, cb_ptr, value_alloc, std::forward<Args>(args)...);
                }
                catch (...)
                {
                    ControlBlockAllocTraits::deallocate(cb_alloc, cb_ptr, 1);
                    throw;
                }

                cb_ptr->increment_shared();
                using result_element_type = typename shared_ptr<T>::element_type;
                shared_ptr<T> result(static_cast<__shared_ptr_control_block::control_block_base*>(cb_ptr),
                    static_cast<result_element_type*>(cb_ptr->get_managed_object_ptr()));
                __rpc_internal::try_enable_shared_from_this(result, static_cast<result_element_type*>(cb_ptr->get_managed_object_ptr()));
                return result;
            }
        } // namespace __shared_ptr_make_support
    } // namespace __rpc_internal

    // --- Free Functions ---

    // Comparison operators for shared_ptr
    template<typename T, typename U> bool operator==(const shared_ptr<T>& a, const shared_ptr<U>& b) noexcept
    {
        return a.get() == b.get();
    }

    template<typename T, typename U> bool operator!=(const shared_ptr<T>& a, const shared_ptr<U>& b) noexcept
    {
        return a.get() != b.get();
    }

    template<typename T, typename U> bool operator<(const shared_ptr<T>& a, const shared_ptr<U>& b) noexcept
    {
        using APtr = typename shared_ptr<T>::element_type*;
        using BPtr = typename shared_ptr<U>::element_type*;
        return std::less<std::common_type_t<APtr, BPtr>>{}(a.get(), b.get());
    }

    template<typename T, typename U> bool operator<=(const shared_ptr<T>& a, const shared_ptr<U>& b) noexcept
    {
        return !(b < a);
    }

    template<typename T, typename U> bool operator>(const shared_ptr<T>& a, const shared_ptr<U>& b) noexcept
    {
        return b < a;
    }

    template<typename T, typename U> bool operator>=(const shared_ptr<T>& a, const shared_ptr<U>& b) noexcept
    {
        return !(a < b);
    }

    template<typename T> void swap(shared_ptr<T>& lhs, shared_ptr<T>& rhs) noexcept
    {
        lhs.swap(rhs);
    }

    template<typename T> void swap(weak_ptr<T>& lhs, weak_ptr<T>& rhs) noexcept
    {
        lhs.swap(rhs);
    }

    template<typename Deleter, typename T> Deleter* get_deleter(const shared_ptr<T>& p) noexcept
    {
        if constexpr (std::is_reference_v<Deleter>)
        {
            return nullptr;
        }

        using StoredDeleter = std::remove_cv_t<std::remove_reference_t<Deleter>>;

        auto* cb = p.internal_get_cb();
        if (!cb)
        {
            return nullptr;
        }

        void* result = cb->get_deleter_ptr(typeid(StoredDeleter));
        if (!result)
        {
            return nullptr;
        }

        return static_cast<Deleter*>(result);
    }

    template<typename T, typename... Args> shared_ptr<T> make_shared(Args&&... args)
    {
        using ValueAlloc = std::allocator<std::remove_cv_t<T>>;
        ValueAlloc value_alloc;
        return __rpc_internal::__shared_ptr_make_support::make_shared_with_value_alloc<T>(
            value_alloc, std::forward<Args>(args)...);
    }

    template<typename T, typename Alloc, typename... Args>
    shared_ptr<T> allocate_shared(const Alloc& alloc, Args&&... args)
    {
        using ValueAlloc = typename std::allocator_traits<Alloc>::template rebind_alloc<std::remove_cv_t<T>>;
        ValueAlloc value_alloc(alloc);
        return __rpc_internal::__shared_ptr_make_support::make_shared_with_value_alloc<T>(
            value_alloc, std::forward<Args>(args)...);
    }

    template<typename T> class enable_shared_from_this
    {
    protected:
        mutable weak_ptr<T> weak_this_;
        constexpr enable_shared_from_this() noexcept = default;
        enable_shared_from_this(const enable_shared_from_this&) noexcept { }
        enable_shared_from_this& operator=(const enable_shared_from_this&) noexcept { return *this; }
        enable_shared_from_this(enable_shared_from_this&&) noexcept { }
        enable_shared_from_this& operator=(enable_shared_from_this&&) noexcept { return *this; }
        ~enable_shared_from_this() = default;

        template<typename ActualPtrType>
        void internal_set_weak_this(
            __rpc_internal::__shared_ptr_control_block::control_block_base* cb_for_this_obj, ActualPtrType* ptr_to_this_obj) const
        {
            using esft_element = typename shared_ptr<T>::element_type;
            if (static_cast<const void*>(static_cast<const esft_element*>(ptr_to_this_obj))
                == static_cast<const void*>(this))
            {
                if (weak_this_.expired())
                {
                    if (cb_for_this_obj && ptr_to_this_obj)
                    {
                        // Directly construct weak_ptr from the control block and pointer
                        // instead of creating a temporary shared_ptr
                        weak_this_.cb_ = cb_for_this_obj;
                        weak_this_.ptr_for_lock_
                            = const_cast<esft_element*>(static_cast<const esft_element*>(ptr_to_this_obj));
                        if (cb_for_this_obj)
                        {
                            cb_for_this_obj->increment_weak();
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
        template<typename T2, typename Y2>
        friend void __rpc_internal::try_enable_shared_from_this(shared_ptr<T2>& sp, Y2* ptr) noexcept;
    };

    // Internal helper function to initialize enable_shared_from_this - definition
    namespace __rpc_internal
    {
        template<typename T, typename Y>
        void try_enable_shared_from_this(shared_ptr<T>& sp, Y* ptr) noexcept
        {
            if constexpr (std::is_base_of_v<RPC_MEMORY::enable_shared_from_this<Y>, Y>)
            {
                if (ptr && sp.internal_get_cb())
                {
                    static_cast<RPC_MEMORY::enable_shared_from_this<Y>*>(ptr)->internal_set_weak_this(sp.internal_get_cb(), ptr);
                }
            }
        }
    }

    // Pointer cast functions
    template<typename T, typename U> shared_ptr<T> static_pointer_cast(const shared_ptr<U>& r) noexcept
    {
        auto p = static_cast<T*>(r.get());
        return shared_ptr<T>(r, p);
    }

#ifdef TEST_STL_COMPLIANCE
    template<typename T, typename U> shared_ptr<T> dynamic_pointer_cast(const shared_ptr<U>& r) noexcept
    {
        if (auto p = dynamic_cast<T*>(r.get()))
        {
            return shared_ptr<T>(r, p);
        }
        return shared_ptr<T>();
    }

#else

    template<typename T, typename U> CORO_TASK(shared_ptr<T>) dynamic_pointer_cast(const shared_ptr<U>& from) noexcept
    {
        if (!from)
            CO_RETURN shared_ptr<T>();

        T* ptr = nullptr;

        // First try local interface casting
        ptr = const_cast<T*>(static_cast<const T*>(from->query_interface(T::get_id(VERSION_2))));
        if (ptr)
            CO_RETURN shared_ptr<T>(from, ptr);

        // Then try remote interface casting through object_proxy
        auto ob = from->get_object_proxy();
        if (!ob)
        {
            CO_RETURN shared_ptr<T>();
        }

        shared_ptr<T> ret;
        // This magic function will return a shared pointer to a new interface proxy
        // Its reference count will not be the same as the "from" pointer's value, semantically it
        // behaves the same as with normal dynamic_pointer_cast in that you can use this function to
        // cast back to the original. However static_pointer_cast in this case will not work for
        // remote interfaces.
        CO_AWAIT ob->template query_interface<T>(ret);
        CO_RETURN ret;
    }
#endif

    template<typename T, typename U> shared_ptr<T> const_pointer_cast(const shared_ptr<U>& r) noexcept
    {
        auto p = const_cast<T*>(r.get());
        return shared_ptr<T>(r, p);
    }

    template<typename T, typename U> shared_ptr<T> reinterpret_pointer_cast(const shared_ptr<U>& r) noexcept
    {
        auto p = reinterpret_cast<T*>(r.get());
        return shared_ptr<T>(r, p);
    }

    // Comparison operators with nullptr
    template<typename T> bool operator==(const shared_ptr<T>& x, std::nullptr_t) noexcept
    {
        return !x;
    }

    template<typename T> bool operator==(std::nullptr_t, const shared_ptr<T>& x) noexcept
    {
        return !x;
    }

    template<typename T> bool operator!=(const shared_ptr<T>& x, std::nullptr_t) noexcept
    {
        return static_cast<bool>(x);
    }

    template<typename T> bool operator!=(std::nullptr_t, const shared_ptr<T>& x) noexcept
    {
        return static_cast<bool>(x);
    }

    template<typename T> bool operator<(const shared_ptr<T>& x, std::nullptr_t) noexcept
    {
        using Ptr = typename shared_ptr<T>::element_type*;
        return std::less<Ptr>()(x.get(), nullptr);
    }

    template<typename T> bool operator<(std::nullptr_t, const shared_ptr<T>& x) noexcept
    {
        using Ptr = typename shared_ptr<T>::element_type*;
        return std::less<Ptr>()(nullptr, x.get());
    }

    template<typename T> bool operator<=(const shared_ptr<T>& x, std::nullptr_t) noexcept
    {
        return !(nullptr < x);
    }

    template<typename T> bool operator<=(std::nullptr_t, const shared_ptr<T>& x) noexcept
    {
        return !(x < nullptr);
    }

    template<typename T> bool operator>(const shared_ptr<T>& x, std::nullptr_t) noexcept
    {
        return nullptr < x;
    }

    template<typename T> bool operator>(std::nullptr_t, const shared_ptr<T>& x) noexcept
    {
        return x < nullptr;
    }

    template<typename T> bool operator>=(const shared_ptr<T>& x, std::nullptr_t) noexcept
    {
        return !(x < nullptr);
    }

    template<typename T> bool operator>=(std::nullptr_t, const shared_ptr<T>& x) noexcept
    {
        return !(nullptr < x);
    }

    // Output stream operator
    template<typename T> std::ostream& operator<<(std::ostream& os, const shared_ptr<T>& ptr)
    {
        return os << ptr.get();
    }

#ifndef TEST_STL_COMPLIANCE
    // Forward declaration
    template<typename T> class optimistic_ptr;
    template<typename T> class local_optimistic_ptr;

    // optimistic_ptr<T> - Non-RAII smart pointer for RPC scenarios
    // Weak semantics for local objects, shared semantics for remote proxies
    template<typename T>
    class optimistic_ptr
    {
        using element_type_impl = std::remove_extent_t<T>;

        static_assert(!std::is_array_v<T>, "optimistic_ptr does not support array types");
        static_assert(__rpc_internal::is_casting_interface_derived<T>::value,
                     "optimistic_ptr can only manage casting_interface-derived types");

        element_type_impl* ptr_{nullptr};
        __rpc_internal::__shared_ptr_control_block::control_block_base* cb_{nullptr};

        template<typename Y> using is_pointer_compatible = __rpc_internal::__shared_ptr_pointer_utils::sp_pointer_compatible<Y, T>;

        void acquire_this() noexcept
        {
            if (cb_)
            {
                // Weak semantics for local objects, shared semantics for remote proxies
                if (cb_->is_local)
                    cb_->increment_optimistic_no_lock();
                else
                    cb_->increment_shared();  // Keep remote proxies alive
            }
        }

        void release_this() noexcept
        {
            if (cb_)
            {
                // Weak semantics for local objects, shared semantics for remote proxies
                if (cb_->is_local)
                    cb_->decrement_optimistic_and_dispose_if_zero();
                else
                    cb_->decrement_shared_and_dispose_if_zero();  // Release remote proxies
            }
        }

    public:
        using element_type = element_type_impl;

        constexpr optimistic_ptr() noexcept = default;
        constexpr optimistic_ptr(std::nullptr_t) noexcept {}

        // Copy constructor - source is valid, control block guaranteed alive
        optimistic_ptr(const optimistic_ptr& r) noexcept
            : ptr_(r.ptr_)
            , cb_(r.cb_)
        {
            acquire_this();
        }

        // Move constructor
        optimistic_ptr(optimistic_ptr&& r) noexcept
            : ptr_(r.ptr_)
            , cb_(r.cb_)
        {
            r.ptr_ = nullptr;
            r.cb_ = nullptr;
        }

        // Heterogeneous copy constructor (upcasts)
        template<typename Y, typename = std::enable_if_t<is_pointer_compatible<Y>::value>>
        optimistic_ptr(const optimistic_ptr<Y>& r) noexcept
            : ptr_(static_cast<element_type_impl*>(r.ptr_))
            , cb_(r.cb_)
        {
            acquire_this();
        }

        // Heterogeneous move constructor
        template<typename Y, typename = std::enable_if_t<is_pointer_compatible<Y>::value>>
        optimistic_ptr(optimistic_ptr<Y>&& r) noexcept
            : ptr_(static_cast<element_type_impl*>(r.ptr_))
            , cb_(r.cb_)
        {
            r.ptr_ = nullptr;
            r.cb_ = nullptr;
        }

        // Construct from shared_ptr - control block lifetime uncertain
        explicit optimistic_ptr(const shared_ptr<T>& sp) noexcept
            : ptr_(nullptr)
            , cb_(nullptr)
        {
            auto cb = sp.internal_get_cb();
            if (sp && cb)
            {
                // Weak semantics for local objects, shared semantics for remote proxies
                bool success = cb->is_local ? cb->try_increment_optimistic() : cb->try_increment_shared();
                if (success)
                {
                    cb_ = cb;
                    ptr_ = sp.internal_get_ptr();
                }
            }
        }

        // Construct from weak_ptr
        explicit optimistic_ptr(const weak_ptr<T>& wp) noexcept
            : ptr_(nullptr)
            , cb_(nullptr)
        {
            // weak_ptr has cb_ member we can access via friend
            auto cb = wp.cb_;
            if (cb)
            {
                // Weak semantics for local objects, shared semantics for remote proxies
                bool success = cb->is_local ? cb->try_increment_optimistic() : cb->try_increment_shared();
                if (success)
                {
                    cb_ = cb;
                    ptr_ = wp.ptr_;
                }
            }
        }

        ~optimistic_ptr() noexcept
        {
            release_this();
        }

        // Copy assignment
        optimistic_ptr& operator=(const optimistic_ptr& r) noexcept
        {
            optimistic_ptr(r).swap(*this);
            return *this;
        }

        // Move assignment
        optimistic_ptr& operator=(optimistic_ptr&& r) noexcept
        {
            optimistic_ptr(std::move(r)).swap(*this);
            return *this;
        }

        // Heterogeneous copy assignment
        template<typename Y, typename = std::enable_if_t<is_pointer_compatible<Y>::value>>
        optimistic_ptr& operator=(const optimistic_ptr<Y>& r) noexcept
        {
            optimistic_ptr(r).swap(*this);
            return *this;
        }

        // Heterogeneous move assignment
        template<typename Y, typename = std::enable_if_t<is_pointer_compatible<Y>::value>>
        optimistic_ptr& operator=(optimistic_ptr<Y>&& r) noexcept
        {
            optimistic_ptr(std::move(r)).swap(*this);
            return *this;
        }

        optimistic_ptr& operator=(std::nullptr_t) noexcept
        {
            reset();
            return *this;
        }

        // Access operators - work for both local and remote objects
        element_type_impl* operator->() const noexcept { return ptr_; }
        element_type_impl& operator*() const noexcept { return *ptr_; }
        element_type_impl* get() const noexcept { return ptr_; }

        explicit operator bool() const noexcept { return ptr_ != nullptr; }

        void reset() noexcept
        {
            optimistic_ptr().swap(*this);
        }

        void swap(optimistic_ptr& r) noexcept
        {
            std::swap(ptr_, r.ptr_);
            std::swap(cb_, r.cb_);
        }

        long use_count() const noexcept
        {
            return cb_ ? cb_->shared_owners.load(std::memory_order_relaxed) : 0;
        }

        bool unique() const noexcept
        {
            return use_count() == 1;
        }

    private:
        // Internal accessors - NOT part of public API
        __rpc_internal::__shared_ptr_control_block::control_block_base* internal_get_cb() const noexcept { return cb_; }
        element_type_impl* internal_get_ptr() const noexcept { return ptr_; }

        template<typename Y> friend class shared_ptr;
        template<typename Y> friend class weak_ptr;
#ifndef TEST_STL_COMPLIANCE
        template<typename Y> friend class optimistic_ptr;
        template<typename Y> friend class local_optimistic_ptr;
#endif
    };

    // local_optimistic_ptr<T> - Temporary RAII lock for calling through optimistic_ptr
    // Stack-only usage - NOT for use as class member
    template<typename T>
    class local_optimistic_ptr
    {
        using element_type_impl = std::remove_extent_t<T>;

        T* ptr_{nullptr};
        shared_ptr<T> local_lock_;  // Only set if object is local (RAII lock)

    public:
        // Constructor from optimistic_ptr
        explicit local_optimistic_ptr(const optimistic_ptr<T>& opt) noexcept
            : ptr_(nullptr)
        {
            if (!opt)
            {
                return;
            }

            auto cb = opt.internal_get_cb();
            if (cb && cb->is_local)
            {
                // Local object - create temporary shared_ptr to RAII lock it
                // Try to increment shared_owners
                if (cb->try_increment_shared())
                {
                    // Construct shared_ptr using private constructor (we already incremented)
                    local_lock_ = shared_ptr<T>(cb, opt.internal_get_ptr(), typename shared_ptr<T>::internal_construct_tag{});
                    ptr_ = local_lock_.get();
                }
            }
            else
            {
                // Remote object - just store the pointer (no RAII lock needed)
                ptr_ = opt.internal_get_ptr();
            }
        }

        // Default constructor
        constexpr local_optimistic_ptr() noexcept = default;

        // Assignment from optimistic_ptr
        local_optimistic_ptr& operator=(const optimistic_ptr<T>& opt) noexcept
        {
            local_optimistic_ptr temp(opt);
            swap(temp);
            return *this;
        }

        // Move constructor
        local_optimistic_ptr(local_optimistic_ptr&& other) noexcept
            : ptr_(other.ptr_)
            , local_lock_(std::move(other.local_lock_))
        {
            other.ptr_ = nullptr;
        }

        // Move assignment
        local_optimistic_ptr& operator=(local_optimistic_ptr&& other) noexcept
        {
            local_optimistic_ptr temp(std::move(other));
            swap(temp);
            return *this;
        }

        // Delete copy constructor and assignment (prevent accidental copies)
        local_optimistic_ptr(const local_optimistic_ptr&) = delete;
        local_optimistic_ptr& operator=(const local_optimistic_ptr&) = delete;

        ~local_optimistic_ptr() = default;  // RAII: local_lock_ releases automatically

        // Access operators - return valid pointer to local object or remote proxy
        T* operator->() const noexcept { return ptr_; }
        T& operator*() const noexcept { return *ptr_; }
        T* get() const noexcept { return ptr_; }

        explicit operator bool() const noexcept { return ptr_ != nullptr; }

        void swap(local_optimistic_ptr& other) noexcept
        {
            std::swap(ptr_, other.ptr_);
            local_lock_.swap(other.local_lock_);
        }

        // Check if this is a local object (has RAII lock)
        bool is_local() const noexcept { return local_lock_.operator bool(); }
    };

    // Comparison operators for optimistic_ptr
    template<typename T, typename U>
    bool operator==(const optimistic_ptr<T>& lhs, const optimistic_ptr<U>& rhs) noexcept
    {
        return lhs.get() == rhs.get();
    }

    template<typename T>
    bool operator==(const optimistic_ptr<T>& lhs, std::nullptr_t) noexcept
    {
        return !lhs;
    }

    template<typename T>
    bool operator==(std::nullptr_t, const optimistic_ptr<T>& rhs) noexcept
    {
        return !rhs;
    }

    template<typename T, typename U>
    bool operator!=(const optimistic_ptr<T>& lhs, const optimistic_ptr<U>& rhs) noexcept
    {
        return !(lhs == rhs);
    }

    template<typename T>
    bool operator!=(const optimistic_ptr<T>& lhs, std::nullptr_t) noexcept
    {
        return static_cast<bool>(lhs);
    }

    template<typename T>
    bool operator!=(std::nullptr_t, const optimistic_ptr<T>& rhs) noexcept
    {
        return static_cast<bool>(rhs);
    }

    // Pointer cast functions for optimistic_ptr
    template<typename T, typename U>
    optimistic_ptr<T> static_pointer_cast(const optimistic_ptr<U>& r) noexcept
    {
        auto p = static_cast<T*>(r.get());
        return optimistic_ptr<T>(r, p);
    }

    template<typename T, typename U>
    optimistic_ptr<T> const_pointer_cast(const optimistic_ptr<U>& r) noexcept
    {
        auto p = const_cast<T*>(r.get());
        return optimistic_ptr<T>(r, p);
    }

    template<typename T, typename U>
    optimistic_ptr<T> reinterpret_pointer_cast(const optimistic_ptr<U>& r) noexcept
    {
        auto p = reinterpret_cast<T*>(r.get());
        return optimistic_ptr<T>(r, p);
    }

    // Dynamic pointer cast for optimistic_ptr (uses local query_interface)
    template<typename T, typename U>
    CORO_TASK(optimistic_ptr<T>) dynamic_pointer_cast(const optimistic_ptr<U>& from) noexcept
    {
        if (!from)
            CO_RETURN optimistic_ptr<T>();

        T* ptr = nullptr;

        // First try local interface casting
        ptr = const_cast<T*>(static_cast<const T*>(from->query_interface(T::get_id(VERSION_2))));
        if (ptr)
            CO_RETURN optimistic_ptr<T>(from, ptr);

        // Then try remote interface casting through object_proxy
        auto ob = from->get_object_proxy();
        if (!ob)
        {
            CO_RETURN optimistic_ptr<T>();
        }

        optimistic_ptr<T> ret;
        // This magic function will return an optimistic pointer to a new interface proxy
        // Its reference count will not be the same as the "from" pointer's value, semantically it
        // behaves the same as with normal dynamic_pointer_cast in that you can use this function to
        // cast back to the original. However static_pointer_cast in this case will not work for
        // remote interfaces.
        CO_AWAIT ob->template query_interface<T, optimistic_ptr<T>, true>(ret);
        CO_RETURN ret;
    }

#endif // !TEST_STL_COMPLIANCE

    // NAMESPACE_INLINE_END  // Commented out to simplify

#ifdef TEST_STL_COMPLIANCE
} // namespace std

#else

} // namespace rpc

#endif


namespace std
{
    template<typename T> struct hash<::RPC_MEMORY::shared_ptr<T>>
    {
        size_t operator()(const ::RPC_MEMORY::shared_ptr<T>& p) const noexcept
        {
            using Ptr = typename ::RPC_MEMORY::shared_ptr<T>::element_type*;
            return std::hash<Ptr>()(p.get());
        }
    };

    template<typename T> struct hash<::RPC_MEMORY::weak_ptr<T>>
    {
        size_t operator()(const ::RPC_MEMORY::weak_ptr<T>& p) const noexcept
        {
            if (auto sp = p.lock())
            {
                return std::hash<::RPC_MEMORY::shared_ptr<T>>()(sp);
            }
            return 0;
        }
    };

#ifndef TEST_STL_COMPLIANCE
    template<typename T> struct hash<::RPC_MEMORY::optimistic_ptr<T>>
    {
        size_t operator()(const ::RPC_MEMORY::optimistic_ptr<T>& p) const noexcept
        {
            using Ptr = typename ::RPC_MEMORY::optimistic_ptr<T>::element_type*;
            return std::hash<Ptr>()(p.get());
        }
    };
#endif

} // namespace std
