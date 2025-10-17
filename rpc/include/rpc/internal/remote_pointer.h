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

#include "assert.h" //rpc assert.h


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
#ifndef TEST_STL_COMPLIANCE
    template<typename T> class optimistic_ptr;
#endif

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
            // add_ref is async because object_proxy::add_ref() makes RPC calls on 0→1 transitions
            CORO_TASK(int) object_proxy_add_ref(const std::shared_ptr<rpc::object_proxy>& ob, rpc::add_ref_options options);
            // release is synchronous - just decrements local counters
            void object_proxy_release(const std::shared_ptr<rpc::object_proxy>& ob, bool is_optimistic);
            void get_object_proxy_reference_counts(const std::shared_ptr<rpc::object_proxy>& ob, int& shared_count, int& optimistic_count);
            // Synchronous direct increment for control block construction (no remote calls)
            void object_proxy_add_ref_shared(const std::shared_ptr<rpc::object_proxy>& ob);
#endif

            template<typename T> class shared_ptr;
            template<typename T> class weak_ptr;

            // NAMESPACE_INLINE_BEGIN  // Commented out to simplify
            struct control_block_base
            {
                std::atomic<long> shared_count_{0};
                std::atomic<long> weak_count_{1};
#ifndef TEST_STL_COMPLIANCE
                std::atomic<long> optimistic_count_{0};
                bool is_local_ = false;
#endif

            protected:
                void* managed_object_ptr_{nullptr}; // Already has trailing underscore

            public:
                control_block_base(void* obj_ptr)
                    : managed_object_ptr_(obj_ptr)
                {
#ifndef TEST_STL_COMPLIANCE
                    rpc::casting_interface* ptr = reinterpret_cast<rpc::casting_interface*>(managed_object_ptr_);
                    if(ptr)
                    {
                        is_local_ = ptr->is_local();
                        // CRITICAL: Initialize object_proxy counters to match control block's initial state.
                        // The control block starts with shared_count_=0, then immediately increments to 1.
                        // We need object_proxy to track this so that when control block decrements 1→0,
                        // the object_proxy can aggregate across all interface types and send sp_release
                        // when the total count reaches 0.
                        if (!is_local_)
                        {
                            auto obj_proxy = ptr->get_object_proxy();
                            if (obj_proxy)
                            {
                                object_proxy_add_ref_shared(obj_proxy);
                            }
                        }
                    }
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

                // Synchronous because shared_ptr operations don't need async on shared 0→1 transitions
                // Called in these scenarios:
                // 1. Copy from existing shared_ptr (already > 0) - synchronous copy
                // 2. Construct from weak_ptr (only succeeds if shared_count_ > 0) - synchronous lock
                // 3. Construct from raw pointer via acquire_this() (0→1 transition) - synchronous, single-threaded constructor context
                // 4. Aliasing constructor (already > 0) - synchronous copy
                //
                // NOTE: For interface proxies wrapping REMOTE objects (is_local_==false), the 0→1 transition happens
                // during shared_ptr<T>(T*) constructor, which is single-threaded and doesn't need async remote call
                // because the object hasn't been marshalled yet (no remote stub exists).
                void increment_shared() { shared_count_.fetch_add(1, std::memory_order_relaxed); }
                void increment_weak() { weak_count_.fetch_add(1, std::memory_order_relaxed); }

                // Try to increment shared count only if it's not zero (thread-safe)
                bool try_increment_shared() noexcept
                {
                    long current = shared_count_.load(std::memory_order_relaxed);
                    do
                    {
                        if (current == 0)
                        {
                            return false; // Already expired, cannot increment
                        }
                    } while (!shared_count_.compare_exchange_weak(current, current + 1, std::memory_order_relaxed));
                    return true;
                }

                void decrement_shared_and_dispose_if_zero()
                {
                    long prev_opt = shared_count_.fetch_sub(1, std::memory_order_relaxed);

                    if (prev_opt <= 0)
                    {
                        RPC_ERROR("decrement_shared_and_dispose_if_zero: shared_count_ was {} before decrement (now {})",
                                  prev_opt, shared_count_.load());
                        RPC_ASSERT(false && "Negative shared_count_ count detected");
                    }

                    if (prev_opt == 1)
                    {
#ifndef TEST_STL_COMPLIANCE
                        // Call object_proxy release on 1→0 transition for remote objects
                        control_block_call_release(false);

                        // For remote objects, delay disposal until optimistic_count_ also reaches 0
                        if (!is_local_ && optimistic_count_.load(std::memory_order_acquire) > 0)
                        {
                            // Don't dispose yet - optimistic_ptrs are keeping interface_proxy alive
                            decrement_weak_and_destroy_if_zero();
                            return;
                        }
#endif
                        dispose_object_actual();
                        decrement_weak_and_destroy_if_zero();
                    }
                }

                void decrement_weak_and_destroy_if_zero()
                {
                    long prev_weak = weak_count_.fetch_sub(1, std::memory_order_acq_rel);

                    if (prev_weak <= 0)
                    {
                        RPC_ERROR("decrement_weak_and_destroy_if_zero: weak_count_ was {} before decrement (now {})",
                                  prev_weak, weak_count_.load());
                        RPC_ASSERT(false && "Negative weak_count_ count detected");
                    }

                    if (prev_weak == 1)
                    {
                        if (shared_count_.load(std::memory_order_acquire) == 0)
                        {
                            destroy_self_actual();
                        }
                    }
                }

#ifndef TEST_STL_COMPLIANCE
                // Fast increment when control block is guaranteed alive
                // Used by optimistic_ptr copy constructor and acquire_this()
                // Synchronous - no remote calls needed, just local reference counting
                void increment_optimistic_no_lock() noexcept
                {
                    optimistic_count_.fetch_add(1, std::memory_order_relaxed);
                }

                // Safe increment when control block lifetime is uncertain (e.g., converting from shared_ptr/weak_ptr)
                // Returns 0 on success, error code if control block expired or remote add_ref failed
                // ASYNC because on 0→1 transition must CO_AWAIT control_block_call_add_ref()
                CORO_TASK(int) try_increment_optimistic()
                {
                    // First, ensure control block stays alive during our operation
                    long weak_count = weak_count_.load(std::memory_order_relaxed);

                    do
                    {
                        // If weak_count is 0, control block is being/has been destroyed
                        if (weak_count == 0)
                        {
                            CO_RETURN error::OBJECT_GONE();
                        }
                        // Try to increment weak_count_ to keep control block alive
                    } while (!weak_count_.compare_exchange_weak(weak_count, weak_count + 1,
                                                                 std::memory_order_acquire,
                                                                 std::memory_order_relaxed));

                    // Control block is now guaranteed alive, safe to check optimistic_count_
                    long prev = optimistic_count_.fetch_add(1, std::memory_order_relaxed);

                    if (prev == 0)
                    {
                        // First optimistic_ptr: 0→1 transition - establish remote reference immediately
                        // This increment becomes the "optimistic_ptr weak_owner"
                        // Call object_proxy add_ref on 0→1 transition for remote objects
                        auto err = CO_AWAIT control_block_call_add_ref(add_ref_options::optimistic);
                        if (err)
                        {
                            // Failed - rollback local state
                            long prev_rollback = optimistic_count_.fetch_sub(1, std::memory_order_relaxed);
                            if (prev_rollback <= 0)
                            {
                                RPC_ERROR("try_increment_optimistic rollback: optimistic_count_ was {} before rollback", prev_rollback);
                                RPC_ASSERT(false && "Negative optimistic_count_ in rollback");
                            }
                            decrement_weak_and_destroy_if_zero();
                            CO_RETURN err;
                        }
                    }
                    else
                    {
                        // Not the first optimistic_ptr: we pre-emptively incremented weak_count_
                        // Undo that increment since weak_count_ is only for control block lifetime
                        decrement_weak_and_destroy_if_zero();
                    }

                    CO_RETURN error::OK();
                }

                void decrement_optimistic_and_dispose_if_zero() noexcept
                {
                    long prev = optimistic_count_.fetch_sub(1, std::memory_order_acq_rel);

                    if (prev <= 0)
                    {
                        RPC_ERROR("decrement_optimistic_and_dispose_if_zero: optimistic_count_ was {} before decrement (now {})",
                                  prev, optimistic_count_.load());
                        RPC_ASSERT(false && "Negative optimistic_count_ count detected");
                    }

                    // If this was the last optimistic_ptr, decrement weak_count_
                    // CRITICAL: This must be done last to prevent control block destruction
                    // while other threads might be incrementing optimistic_count_
                    if (prev == 1)
                    {
                        // Call object_proxy release on 1→0 transition for remote objects
                        control_block_call_release(true);

                        // For remote objects, dispose interface_proxy if shared_count_ is also 0
                        if (!is_local_ && shared_count_.load(std::memory_order_acquire) == 0)
                        {
                            dispose_object_actual();
                        }

                        // Last optimistic_ptr: decrement the weak_count_ that was added by first optimistic_ptr
                        decrement_weak_and_destroy_if_zero();
                    }
                }
                
                // Async because object_proxy_add_ref() is async
                inline CORO_TASK(int) control_block_call_add_ref(rpc::add_ref_options options)
                {
                    if (managed_object_ptr_ && !is_local_)
                    {
                        auto ci = reinterpret_cast<::rpc::casting_interface*>(managed_object_ptr_);
                        if (auto obj_proxy = ci->get_object_proxy())
                        {
                            CO_RETURN CO_AWAIT object_proxy_add_ref(obj_proxy, options);
                        }
                    }
                    CO_RETURN error::OK();
                }

                inline void control_block_call_release(bool is_optimistic) noexcept
                {
                    if (managed_object_ptr_ && !is_local_)
                    {
                        auto ci = reinterpret_cast<::rpc::casting_interface*>(managed_object_ptr_);
                        if (auto obj_proxy = ci->get_object_proxy())
                        {
                            object_proxy_release(obj_proxy, is_optimistic);
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
                Deleter object_deleter_; // Already has trailing underscore
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
                Deleter object_deleter_; // Already has trailing underscore
                Alloc control_block_allocator_; // Already has trailing underscore
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
                Alloc allocator_instance_; // Already has trailing underscore
                union
                {
                    T object_instance_; // Already has trailing underscore
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

        ~shared_ptr()
        {
            release_this();
#ifdef _MSVC_STL_DESTRUCTOR_TOMBSTONES
            // Write tombstone markers to detect use-after-destruction
            ptr_ = reinterpret_cast<T*>(static_cast<uintptr_t>(0xDEADBEEFDEADBEEFULL));
            cb_ = reinterpret_cast<__rpc_internal::__shared_ptr_control_block::control_block_base*>(static_cast<uintptr_t>(0xDEADBEEFDEADBEEFULL));
#endif
        }
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
        long use_count() const noexcept { return cb_ ? cb_->shared_count_.load(std::memory_order_relaxed) : 0; }
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
        // Friend declarations for factory functions
        template<typename U> friend CORO_TASK(int) make_optimistic(const shared_ptr<U>&, optimistic_ptr<U>&) noexcept;
        template<typename U> friend CORO_TASK(int) make_optimistic(const weak_ptr<U>&, optimistic_ptr<U>&) noexcept;
        template<typename U> friend CORO_TASK(int) make_shared(const optimistic_ptr<U>&, shared_ptr<U>&) noexcept;
        template<typename U> friend CORO_TASK(int) make_weak(const optimistic_ptr<U>&, weak_ptr<U>&) noexcept;
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
#ifdef _MSVC_STL_DESTRUCTOR_TOMBSTONES
            // Write tombstone markers to detect use-after-destruction
            cb_ = reinterpret_cast<__rpc_internal::__shared_ptr_control_block::control_block_base*>(static_cast<uintptr_t>(0xDEADBEEFDEADBEEFULL));
#endif
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
            long c = cb_->shared_count_.load(std::memory_order_relaxed);
            while (c > 0)
            {
                if (cb_->shared_count_.compare_exchange_weak(
                        c, c + 1, std::memory_order_acq_rel, std::memory_order_relaxed))
                    return shared_ptr<T>(cb_, ptr_for_lock_);
            }
            return {};
        }
        long use_count() const noexcept { return cb_ ? cb_->shared_count_.load(std::memory_order_relaxed) : 0; }
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
#ifndef TEST_STL_COMPLIANCE
        template<typename U> friend class optimistic_ptr;
        // Friend declarations for factory functions
        template<typename U> friend CORO_TASK(int) make_optimistic(const shared_ptr<U>&, optimistic_ptr<U>&) noexcept;
        template<typename U> friend CORO_TASK(int) make_optimistic(const weak_ptr<U>&, optimistic_ptr<U>&) noexcept;
        template<typename U> friend CORO_TASK(int) make_shared(const optimistic_ptr<U>&, shared_ptr<U>&) noexcept;
        template<typename U> friend CORO_TASK(int) make_weak(const optimistic_ptr<U>&, weak_ptr<U>&) noexcept;
#endif
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
    
    template<class T> class local_proxy : public T
    {
    protected:
        rpc::weak_ptr<T> ptr_;

    public:
        local_proxy(const rpc::weak_ptr<T>& ptr)
            : ptr_(ptr)
        {
        }
        virtual ~local_proxy() = default;
        rpc::weak_ptr<T> __get_weak() { return ptr_; }
    };

    // optimistic_ptr<T> - Non-RAII smart pointer for RPC scenarios
    // Weak semantics for local objects, shared semantics for remote proxies
    template<typename T>
    class optimistic_ptr
    {
        using element_type_impl = std::remove_extent_t<T>;

        static_assert(!std::is_array_v<T>, "optimistic_ptr does not support array types");
        static_assert(__rpc_internal::is_casting_interface_derived<T>::value,
                     "optimistic_ptr can only manage casting_interface-derived types");

        // For local proxies: local_proxy_holder_ holds the local_proxy (callable target), ptr_ and cb_ are nullptr
        // For remote proxies: ptr_ points to interface_proxy (callable target), cb_ for refcounting, local_proxy_holder_ is nullptr
        // Note: local_proxy_holder_ uses conditional forwarding - it IS the callable proxy, not the underlying object
        element_type_impl* ptr_{nullptr};
        __rpc_internal::__shared_ptr_control_block::control_block_base* cb_{nullptr};
        std::shared_ptr<local_proxy<T>> local_proxy_holder_;

        template<typename Y> using is_pointer_compatible = __rpc_internal::__shared_ptr_pointer_utils::sp_pointer_compatible<Y, T>;

        void acquire_this() noexcept
        {
            if (cb_)
            {
                // Use optimistic references for both local and remote objects
                // For remote: optimistic refs don't hold stub lifetime but keep interface_proxy alive
                cb_->increment_optimistic_no_lock();
            }
        }

        void release_this() noexcept
        {
            if (cb_)
            {
                // Use optimistic references for both local and remote objects
                // For remote: dispose interface_proxy only when BOTH shared and optimistic counts reach 0
                cb_->decrement_optimistic_and_dispose_if_zero();
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
            , local_proxy_holder_(r.local_proxy_holder_)
        {
            // Both local and remote proxies can now be safely copied
            // Local: shared_ptr copy increments reference count
            // Remote: acquire optimistic reference
            acquire_this();
        }

        // Move constructor
        optimistic_ptr(optimistic_ptr&& r) noexcept
            : ptr_(r.ptr_)
            , cb_(r.cb_)
            , local_proxy_holder_(std::move(r.local_proxy_holder_))
        {
            r.ptr_ = nullptr;
            r.cb_ = nullptr;
        }

        // Heterogeneous copy constructor (upcasts)
        template<typename Y, typename = std::enable_if_t<is_pointer_compatible<Y>::value>>
        optimistic_ptr(const optimistic_ptr<Y>& r) noexcept
            : ptr_(static_cast<element_type_impl*>(r.ptr_))
            , cb_(r.cb_)
            , local_proxy_holder_(r.local_proxy_holder_)
        {
            // Both local and remote proxies can now be safely copied
            acquire_this();
        }

        // Heterogeneous move constructor
        template<typename Y, typename = std::enable_if_t<is_pointer_compatible<Y>::value>>
        optimistic_ptr(optimistic_ptr<Y>&& r) noexcept
            : ptr_(static_cast<element_type_impl*>(r.ptr_))
            , cb_(r.cb_)
            , local_proxy_holder_(std::move(r.local_proxy_holder_))
        {
            r.ptr_ = nullptr;
            r.cb_ = nullptr;
        }

        ~optimistic_ptr() noexcept
        {
            // For local proxies: local_proxy_holder_ shared_ptr will automatically clean up
            // For remote proxies: release optimistic reference
            release_this();
            // local_proxy_holder_ destructor runs automatically
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

        // Access operators - operator-> is safe for making calls through the proxy
        element_type_impl* operator->() const noexcept
        {
            // For local objects: ptr_ points to __i_xxx_local_proxy (safe - returns OBJECT_GONE)
            // For remote objects: ptr_ points to interface_proxy (safe - RPC handles errors)
            if(local_proxy_holder_)
                return local_proxy_holder_.get();
            return ptr_;
        }

        // UNSAFE: Direct pointer access - use ONLY for testing/comparison
        // The returned pointer may become invalid at any time due to concurrent deletion
        // For safe calls, use operator-> instead
        element_type_impl* get_unsafe_only_for_testing() const noexcept
        {
            // WARNING: This pointer can dangle at any moment in multi-threaded scenarios

            // For local objects: local_proxy_holder_ is set, ptr_ is nullptr
            if (local_proxy_holder_)
            {
                // Access the weak_ptr's underlying pointer directly (we're a friend)
                // This is unsafe - the pointer may be dangling
                rpc::weak_ptr<T> weak = local_proxy_holder_->__get_weak();
                return weak.ptr_for_lock_;
            }

            // For remote objects: ptr_ is set
            return ptr_;
        }

        explicit operator bool() const noexcept
        {
            return local_proxy_holder_ != nullptr || ptr_ != nullptr;
        }

        void reset() noexcept
        {
            optimistic_ptr().swap(*this);
        }

        void swap(optimistic_ptr& r) noexcept
        {
            std::swap(ptr_, r.ptr_);
            std::swap(cb_, r.cb_);
            std::swap(local_proxy_holder_, r.local_proxy_holder_);
        }

    private:
        // Internal accessors - NOT part of public API
        __rpc_internal::__shared_ptr_control_block::control_block_base* internal_get_cb() const noexcept { return cb_; }
        element_type_impl* internal_get_ptr() const noexcept { return ptr_; }

        template<typename Y> friend class shared_ptr;
        template<typename Y> friend class weak_ptr;
#ifndef TEST_STL_COMPLIANCE
        template<typename Y> friend class optimistic_ptr;
        // Friend declarations for factory functions
        template<typename U> friend CORO_TASK(int) make_optimistic(const shared_ptr<U>&, optimistic_ptr<U>&) noexcept;
        template<typename U> friend CORO_TASK(int) make_optimistic(const weak_ptr<U>&, optimistic_ptr<U>&) noexcept;
        template<typename U> friend CORO_TASK(int) make_shared(const optimistic_ptr<U>&, shared_ptr<U>&) noexcept;
        template<typename U> friend CORO_TASK(int) make_weak(const optimistic_ptr<U>&, weak_ptr<U>&) noexcept;
#endif
    };

    // Dynamic pointer cast for optimistic_ptr (uses local query_interface)
    template<typename T, typename U>
    CORO_TASK(int) dynamic_pointer_cast(const optimistic_ptr<U>& from, optimistic_ptr<T>& to) noexcept
    {
        if constexpr (std::is_same<T,U>::value)
        {
            to = from;
            CO_RETURN error::OK();
        }

        if (!from)
        {
            to = optimistic_ptr<T>();
            CO_RETURN error::OK();
        }

        if (from.local_proxy_holder_)
        {
            auto local_shared = from.local_proxy_holder_->__get_weak().lock();
            if(!local_shared)
            {
                to = nullptr;
                CO_RETURN error::OK();
            }

            auto ptr = const_cast<T*>(static_cast<const T*>(local_shared->query_interface(T::get_id(VERSION_2))));
            if (ptr)
            {
                to = optimistic_ptr<T>(from, ptr);
                CO_RETURN error::OK();
            }
            else
            {
                to = optimistic_ptr<T>();
                CO_RETURN error::OK();
            }
        }

        // Then try remote interface casting through object_proxy
        auto ob = from->get_object_proxy();
        if (!ob)
        {
            to = optimistic_ptr<T>();
            CO_RETURN error::OK();
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

    // ============================================================================
    // Async Factory Functions for Cross-Type Conversions
    // ============================================================================
    // These replace constructors/assignment operators which cannot be async

    // Convert shared_ptr → optimistic_ptr
    template<typename T>
    CORO_TASK(int) make_optimistic(const shared_ptr<T>& in, optimistic_ptr<T>& out) noexcept
    {
        out.reset();  // Clear any existing value

        auto cb = in.internal_get_cb();
        if (!in || !cb)
        {
            CO_RETURN error::OK();  // Null input → null output (success)
        }

        // Check if this is a local object
        if (cb->is_local_)
        {
            // Local object: create local_proxy using generated static method
            weak_ptr<T> wp(in);
            out.local_proxy_holder_ = T::create_local_proxy(wp);
            // out.ptr_ and out.cb_ remain nullptr for local objects
            CO_RETURN error::OK();
        }
        else
        {
            // Remote object: establish optimistic reference (0→1 transition, async!)
#ifdef USE_RPC_LOGGING
            // TELEMETRY: Check reference counts BEFORE establishing optimistic reference
            long cb_shared_before = cb->shared_count_.load(std::memory_order_acquire);
            long cb_optimistic_before = cb->optimistic_count_.load(std::memory_order_acquire);

            // Get object_proxy to check inherited counts (service-level stub counts)
            auto casting_iface = reinterpret_cast<rpc::casting_interface*>(in.internal_get_ptr());
            auto obj_proxy = casting_iface->get_object_proxy();
            int inherited_shared_before = 0;
            int inherited_optimistic_before = 0;
            if (obj_proxy)
            {
                __rpc_internal::__shared_ptr_control_block::get_object_proxy_reference_counts(obj_proxy, inherited_shared_before, inherited_optimistic_before);
            }

            RPC_DEBUG("make_optimistic(shared_ptr→optimistic_ptr): BEFORE - control_block(shared={}, optimistic={}), object_proxy(inherited_shared={}, inherited_optimistic={})",
                      cb_shared_before, cb_optimistic_before, inherited_shared_before, inherited_optimistic_before);

            // NOTE: It's EXPECTED that before make_optimistic, we have shared=1, optimistic=0
            // This is the normal state when converting a shared_ptr to optimistic_ptr
            // The validation below checks this expected state

            // Verify control block shared count matches object proxy shared count
            if (obj_proxy && cb_shared_before != inherited_shared_before)
            {
                RPC_ERROR("make_optimistic: Control block shared count ({}) doesn't match object_proxy shared count ({})",
                          cb_shared_before, inherited_shared_before);
            }

            // Verify control block optimistic count matches object proxy optimistic count
            if (obj_proxy && cb_optimistic_before != inherited_optimistic_before)
            {
                RPC_ERROR("make_optimistic: Control block optimistic count ({}) doesn't match object_proxy optimistic count ({})",
                          cb_optimistic_before, inherited_optimistic_before);
            }
#endif

            auto err = CO_AWAIT cb->try_increment_optimistic();
            if (err)
            {
                CO_RETURN err;  // Failed to establish remote reference
            }

#ifdef USE_RPC_LOGGING
            // TELEMETRY: Check reference counts AFTER establishing optimistic reference
            long cb_shared_after = cb->shared_count_.load(std::memory_order_acquire);
            long cb_optimistic_after = cb->optimistic_count_.load(std::memory_order_acquire);
            int inherited_shared_after = 0;
            int inherited_optimistic_after = 0;
            if (obj_proxy)
            {
                __rpc_internal::__shared_ptr_control_block::get_object_proxy_reference_counts(obj_proxy, inherited_shared_after, inherited_optimistic_after);
            }

            RPC_DEBUG("make_optimistic(shared_ptr→optimistic_ptr): AFTER - control_block(shared={}, optimistic={}), object_proxy(inherited_shared={}, inherited_optimistic={})",
                      cb_shared_after, cb_optimistic_after, inherited_shared_after, inherited_optimistic_after);

            // After make_optimistic, we expect shared=1, optimistic=1 (both counts should be positive)
            if (cb_shared_after == 0 || cb_optimistic_after == 0)
            {
                RPC_ERROR("make_optimistic: Control block count zero AFTER increment - shared={} optimistic={} (both should be > 0)",
                          cb_shared_after, cb_optimistic_after);
            }

            // Verify control block shared count matches object proxy shared count
            if (obj_proxy && cb_shared_after != inherited_shared_after)
            {
                RPC_ERROR("make_optimistic: Control block shared count ({}) doesn't match object_proxy shared count ({}) AFTER",
                          cb_shared_after, inherited_shared_after);
            }

            // Verify control block optimistic count matches object proxy optimistic count
            if (obj_proxy && cb_optimistic_after != inherited_optimistic_after)
            {
                RPC_ERROR("make_optimistic: Control block optimistic count ({}) doesn't match object_proxy optimistic count ({}) AFTER",
                          cb_optimistic_after, inherited_optimistic_after);
            }
#endif

            out.cb_ = cb;
            out.ptr_ = in.internal_get_ptr();
            // out.local_proxy_holder_ remains empty for remote objects
            CO_RETURN error::OK();
        }
    }

    // Convert weak_ptr → optimistic_ptr
    template<typename T>
    CORO_TASK(int) make_optimistic(const weak_ptr<T>& in, optimistic_ptr<T>& out) noexcept
    {
        out.reset();  // Clear any existing value

        auto cb = in.cb_;
        if (!cb)
        {
            CO_RETURN error::OK();  // Null input → null output (success)
        }

        // Check if this is a local object
        if (cb->is_local_)
        {
            // Local object: create local_proxy using generated static method
            out.local_proxy_holder_ = T::create_local_proxy(in);
            // out.ptr_ and out.cb_ remain nullptr for local objects
            CO_RETURN error::OK();
        }
        else
        {
            // Remote object: establish optimistic reference (0→1 transition, async!)
#ifdef USE_RPC_LOGGING
            // TELEMETRY: Check reference counts BEFORE establishing optimistic reference
            long cb_shared_before = cb->shared_count_.load(std::memory_order_acquire);
            long cb_optimistic_before = cb->optimistic_count_.load(std::memory_order_acquire);

            // Get object_proxy to check inherited counts (service-level stub counts)
            auto casting_iface = reinterpret_cast<rpc::casting_interface*>(in.ptr_);
            auto obj_proxy = casting_iface ? casting_iface->get_object_proxy() : nullptr;
            int inherited_shared_before = 0;
            int inherited_optimistic_before = 0;
            if (obj_proxy)
            {
                __rpc_internal::__shared_ptr_control_block::get_object_proxy_reference_counts(obj_proxy, inherited_shared_before, inherited_optimistic_before);
            }

            RPC_DEBUG("make_optimistic(weak_ptr→optimistic_ptr): BEFORE - control_block(shared={}, optimistic={}), object_proxy(inherited_shared={}, inherited_optimistic={})",
                      cb_shared_before, cb_optimistic_before, inherited_shared_before, inherited_optimistic_before);

            // Verify both control block counts are either zero or both non-zero
            if ((cb_shared_before == 0) != (cb_optimistic_before == 0))
            {
                RPC_ERROR("make_optimistic(weak_ptr): Control block reference count mismatch BEFORE - shared={} optimistic={} (should both be 0 or both be non-zero)",
                          cb_shared_before, cb_optimistic_before);
            }

            // Verify both inherited counts are either zero or both non-zero
            if (obj_proxy && (inherited_shared_before == 0) != (inherited_optimistic_before == 0))
            {
                RPC_ERROR("make_optimistic(weak_ptr): Object proxy inherited count mismatch BEFORE - inherited_shared={} inherited_optimistic={} (should both be 0 or both be non-zero)",
                          inherited_shared_before, inherited_optimistic_before);
            }
#endif

            auto err = CO_AWAIT cb->try_increment_optimistic();
            if (err)
            {
                CO_RETURN err;  // Failed (likely OBJECT_GONE)
            }

#ifdef USE_RPC_LOGGING
            // TELEMETRY: Check reference counts AFTER establishing optimistic reference
            long cb_shared_after = cb->shared_count_.load(std::memory_order_acquire);
            long cb_optimistic_after = cb->optimistic_count_.load(std::memory_order_acquire);
            int inherited_shared_after = 0;
            int inherited_optimistic_after = 0;
            if (obj_proxy)
            {
                __rpc_internal::__shared_ptr_control_block::get_object_proxy_reference_counts(obj_proxy, inherited_shared_after, inherited_optimistic_after);
            }

            RPC_DEBUG("make_optimistic(weak_ptr→optimistic_ptr): AFTER - control_block(shared={}, optimistic={}), object_proxy(inherited_shared={}, inherited_optimistic={})",
                      cb_shared_after, cb_optimistic_after, inherited_shared_after, inherited_optimistic_after);

            // Verify both control block counts are non-zero after increment
            if (cb_shared_after == 0 || cb_optimistic_after == 0)
            {
                RPC_ERROR("make_optimistic(weak_ptr): Control block count zero AFTER increment - shared={} optimistic={} (both should be > 0)",
                          cb_shared_after, cb_optimistic_after);
            }

            // Verify both inherited counts are either zero or both non-zero after increment
            if (obj_proxy && (inherited_shared_after == 0) != (inherited_optimistic_after == 0))
            {
                RPC_ERROR("make_optimistic(weak_ptr): Object proxy inherited count mismatch AFTER - inherited_shared={} inherited_optimistic={} (should both be 0 or both be non-zero)",
                          inherited_shared_after, inherited_optimistic_after);
            }
#endif

            out.cb_ = cb;
            out.ptr_ = in.ptr_;
            // out.local_proxy_holder_ remains empty for remote objects
            CO_RETURN error::OK();
        }
    }

    // Convert optimistic_ptr → shared_ptr
    template<typename T>
    CORO_TASK(int) make_shared(const optimistic_ptr<T>& in, shared_ptr<T>& out) noexcept
    {
        out.reset();  // Clear any existing value

        // Check if input holds local_proxy
        if (in.local_proxy_holder_)
        {
            // Local object: get the weak_ptr from local_proxy and try to lock it
            auto weak = in.local_proxy_holder_->__get_weak();
            out = weak.lock();
            CO_RETURN error::OK();
        }
        else if (in.cb_)
        {
            // Remote object: create shared_ptr from interface_proxy
            // The control block already exists, just increment shared count
            auto err = CO_AWAIT in.cb_->increment_shared();
            if (err)
            {
                CO_RETURN err;
            }
            out.cb_ = in.cb_;
            out.ptr_ = in.ptr_;
            CO_RETURN error::OK();
        }
        else
        {
            // Null input
            CO_RETURN error::OK();
        }
    }

    // Convert optimistic_ptr → weak_ptr
    template<typename T>
    CORO_TASK(int) make_weak(const optimistic_ptr<T>& in, weak_ptr<T>& out) noexcept
    {
        out.reset();  // Clear any existing value

        // Check if input holds local_proxy
        if (in.local_proxy_holder_)
        {
            // Local object: get the weak_ptr directly from local_proxy
            out = in.local_proxy_holder_->__get_weak();
            CO_RETURN error::OK();
        }
        else if (in.cb_)
        {
            // Remote object: create weak_ptr from control block
            // For remote objects we need to go through shared_ptr first
            shared_ptr<T> temp_shared;
            auto err = CO_AWAIT make_shared(in, temp_shared);
            if (err)
            {
                CO_RETURN err;
            }
            // Then create weak_ptr from shared_ptr (synchronous)
            out = weak_ptr<T>(temp_shared);
            CO_RETURN error::OK();
        }
        else
        {
            // Null input
            CO_RETURN error::OK();
        }
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
