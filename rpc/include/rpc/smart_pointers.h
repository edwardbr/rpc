#ifndef RPC_SMART_POINTERS_CORE_HPP // Changed guard to reflect potential new filename
#define RPC_SMART_POINTERS_CORE_HPP

#include <atomic>
#include <memory>      // For std::default_delete, std::allocator, std::allocator_traits, std::shared_ptr
#include <utility>     // For std::swap, std::move, std::forward
#include <stdexcept>   // For std::bad_weak_ptr, std::invalid_argument
#include <type_traits> // For std::is_base_of_v, std::is_convertible_v, std::remove_extent_t, etc.
#include <functional>  // For std::hash
#include <cstddef>     // For std::nullptr_t, std::size_t
#include <vector>      // For interface_ordinal example (if still needed here, or comes from types.h)

// The rest of the smart pointer code (internal namespace, class definitions, free functions)
// will now use the forward-declared rpc::proxy_base and rpc::object_proxy.
// Their methods that call members of proxy_base/object_proxy will compile correctly
// because the full definitions will be available when proxy.h includes this file.

namespace rpc
{

    namespace internal
    {
        struct control_block_base
        {
            std::atomic<long> local_shared_owners{0};
            std::atomic<long> local_weak_owners{1};
            std::shared_ptr<rpc::object_proxy> this_cb_object_proxy_sp_;
            void* managed_object_ptr_{nullptr};

            // Constructor for objects that might have an object_proxy (e.g., from raw pointer or unique_ptr)
            control_block_base(void* obj_ptr, std::shared_ptr<rpc::object_proxy> obj_proxy_for_this_cb_obj)
                : managed_object_ptr_(obj_ptr)
                , this_cb_object_proxy_sp_(std::move(obj_proxy_for_this_cb_obj))
            {
            }

            // Constructor for make_shared (local objects, no initial object_proxy)
            control_block_base(void* obj_ptr)
                : managed_object_ptr_(obj_ptr)
                , this_cb_object_proxy_sp_(nullptr)
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
                Alloc alloc_copy = control_block_allocator_;
                std::allocator_traits<Alloc>::destroy(alloc_copy, this);
                std::allocator_traits<Alloc>::deallocate(alloc_copy, this, 1);
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
            // obj_proxy parameter removed for make_shared of local objects
            control_block_make_shared(Alloc alloc_for_t_and_cb, ConcreteArgs&&... args)
                : control_block_base(&object_instance_)
                , // Calls CB constructor that sets this_cb_object_proxy_sp_ to nullptr
                allocator_instance_(std::move(alloc_for_t_and_cb))
            {
                std::allocator_traits<Alloc>::construct(
                    allocator_instance_, &object_instance_, std::forward<ConcreteArgs>(args)...);
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
                Alloc alloc_copy = allocator_instance_;
                if (managed_object_ptr_)
                {
                    std::allocator_traits<Alloc>::destroy(allocator_instance_, &object_instance_);
                    managed_object_ptr_ = nullptr;
                }
                std::allocator_traits<Alloc>::deallocate(alloc_copy, this, 1);
            }
            ~control_block_make_shared() { }
        };
    } // namespace internal

    template<typename T>
    std::shared_ptr<rpc::object_proxy> get_ultimate_object_proxy_for(T* ptr, rpc::internal::control_block_base* cb_of_ptr)
    {
        if (!ptr)
            return nullptr;
        if (auto proxy = ptr->query_proxy_base())
        {
            return proxy->get_object_proxy();
        }
        else if (cb_of_ptr)
        {
            return cb_of_ptr->this_cb_object_proxy_sp_; // This will be nullptr for objects from make_shared
        }
        return nullptr;
    }

    template<typename T> class shared_ptr
    {
        internal::control_block_base* cb_{nullptr};
        T* ptr_{nullptr};
        std::shared_ptr<rpc::object_proxy> ultimate_actual_object_proxy_sp_{nullptr};

        void acquire_this() noexcept
        {
            if (cb_)
                cb_->increment_local_shared();
            if (ultimate_actual_object_proxy_sp_)
                ultimate_actual_object_proxy_sp_->increment_remote_strong();
        }
        void release_this() noexcept
        {
            if (ultimate_actual_object_proxy_sp_)
                ultimate_actual_object_proxy_sp_->decrement_remote_strong_and_signal_if_appropriate();
            if (cb_)
                cb_->decrement_local_shared_and_dispose_if_zero();
        }

        // Private constructor for make_shared, weak_ptr::lock
        shared_ptr(internal::control_block_base* cb, T* p)
            : cb_(cb)
            , ptr_(p)
        {
            if (cb_)
            {
                ultimate_actual_object_proxy_sp_ = get_ultimate_object_proxy_for(ptr_, cb_);
                // If ultimate_actual_object_proxy_sp_ is not null (e.g. from lock of a proxy-observing weak_ptr,
                // or if make_shared was for an object that somehow got an object_proxy immediately),
                // then increment remote strong. For typical make_shared of local obj, this will be null.
                if (ultimate_actual_object_proxy_sp_)
                {
                    ultimate_actual_object_proxy_sp_->increment_remote_strong();
                }
            }
            else
            {
                ptr_ = nullptr;
            }
        }

    public:
        using element_type = std::remove_extent_t<T>;
        using weak_type = rpc::weak_ptr<T>;

        constexpr shared_ptr() noexcept = default;
        constexpr shared_ptr(std::nullptr_t) noexcept { }

        // Constructor for taking ownership of a raw pointer, WITH an explicit object_proxy
        template<typename Y,
            typename Deleter = std::default_delete<Y>,
            typename Alloc = std::allocator<char>,
            typename = std::enable_if_t<std::is_base_of_v<T, Y> && std::is_base_of_v<casting_interface, Y>>>
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
                // Pass obj_proxy_for_p_obj to the control block constructor
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
        shared_ptr(const shared_ptr<Y>& r, T* p_alias) noexcept
            : cb_(r.internal_get_cb())
            , ptr_(p_alias)
            , ultimate_actual_object_proxy_sp_(r.internal_get_ultimate_object_proxy())
        {
            acquire_this();
        }

        shared_ptr(const shared_ptr& r) noexcept
            : cb_(r.cb_)
            , ptr_(r.ptr_)
            , ultimate_actual_object_proxy_sp_(r.ultimate_actual_object_proxy_sp_)
        {
            acquire_this();
        }
        shared_ptr(shared_ptr&& r) noexcept
            : cb_(r.cb_)
            , ptr_(r.ptr_)
            , ultimate_actual_object_proxy_sp_(std::move(r.ultimate_actual_object_proxy_sp_))
        {
            r.cb_ = nullptr;
            r.ptr_ = nullptr;
        }

        template<typename Y, typename = std::enable_if_t<std::is_convertible_v<Y*, T*>>>
        shared_ptr(const shared_ptr<Y>& r) noexcept
            : cb_(r.internal_get_cb())
            , ptr_(static_cast<T*>(r.internal_get_ptr()))
            , ultimate_actual_object_proxy_sp_(r.internal_get_ultimate_object_proxy())
        {
            acquire_this();
        }
        template<typename Y, typename = std::enable_if_t<std::is_convertible_v<Y*, T*>>>
        shared_ptr(shared_ptr<Y>&& r) noexcept
            : cb_(r.internal_get_cb())
            , ptr_(static_cast<T*>(r.internal_get_ptr()))
            , ultimate_actual_object_proxy_sp_(std::move(r.internal_get_ultimate_object_proxy_ref()))
        {
            r.cb_ = nullptr;
            r.ptr_ = nullptr;
        }

        // Constructor from unique_ptr - associates the given object_proxy with the new shared object
        template<typename Y,
            typename Deleter_U,
            typename = std::enable_if_t<std::is_convertible_v<Y*, T*> && std::is_base_of_v<casting_interface, Y>>>
        shared_ptr(rpc::unique_ptr<Y, Deleter_U>&& u, std::shared_ptr<rpc::object_proxy> obj_proxy_for_u_obj)
            : ptr_(nullptr)
            , cb_(nullptr)
        {
            if (!u)
                return;
            Y* raw_p = u.get();
            Deleter_U deleter = u.get_deleter();
            using ActualCB = internal::control_block_impl<Y, Deleter_U, std::allocator<char>>;
            std::allocator<ActualCB> cba;
            ActualCB* raw_cb = nullptr;
            try
            {
                raw_cb = cba.allocate(1);
                new (raw_cb) ActualCB(raw_p, obj_proxy_for_u_obj, std::move(deleter), std::allocator<char>());
                cb_ = raw_cb;
                ptr_ = raw_p;
                u.release();
            }
            catch (...)
            {
                if (raw_cb)
                    cba.deallocate(raw_cb, 1);
                throw;
            }
            if (cb_)
            {
                ultimate_actual_object_proxy_sp_ = get_ultimate_object_proxy_for(ptr_, cb_);
                acquire_this();
            }
            else
            {
                ptr_ = nullptr;
            }
        }

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
        // Reset with a raw pointer, needs an object_proxy
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
        template<typename U, typename... Args>
        friend rpc::shared_ptr<U> make_shared(Args&&... args); // Changed signature
        template<class T1_cast, class T2_cast>
        friend shared_ptr<T1_cast> dynamic_pointer_cast(const shared_ptr<T2_cast>& from) noexcept;
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
            if (proxy_sp_param && proxy_sp_param->query_proxy_base() != nullptr)
            {
                held_proxy_sp_ = proxy_sp_param;
                if (auto proxy_base_ptr = held_proxy_sp_->query_proxy_base())
                {
                    ultimate_actual_object_proxy_sp_ = proxy_base_ptr->get_object_proxy();
                }
                if (ultimate_actual_object_proxy_sp_)
                {
                    ultimate_actual_object_proxy_sp_->increment_remote_weak_callable();
                }
                else
                {
                    held_proxy_sp_.reset();
                }
            }
            else if (proxy_sp_param)
            { /* Error: not a proxy */
            }
        }
        explicit optimistic_ptr(rpc::shared_ptr<T>&& proxy_sp_param)
        {
            if (proxy_sp_param && proxy_sp_param->query_proxy_base() != nullptr)
            {
                held_proxy_sp_ = std::move(proxy_sp_param);
                if (auto proxy_base_ptr = held_proxy_sp_->query_proxy_base())
                {
                    ultimate_actual_object_proxy_sp_ = proxy_base_ptr->get_object_proxy();
                }
                if (ultimate_actual_object_proxy_sp_)
                {
                    ultimate_actual_object_proxy_sp_->increment_remote_weak_callable();
                }
                else
                {
                    held_proxy_sp_.reset();
                }
            }
            else if (proxy_sp_param)
            { /* Error: not a proxy */
            }
        }

        optimistic_ptr(const optimistic_ptr& r)
            : held_proxy_sp_(r.held_proxy_sp_)
            , ultimate_actual_object_proxy_sp_(r.ultimate_actual_object_proxy_sp_)
        {
            if (ultimate_actual_object_proxy_sp_)
                ultimate_actual_object_proxy_sp_->increment_remote_weak_callable();
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
                if (ultimate_actual_object_proxy_sp_)
                    ultimate_actual_object_proxy_sp_->decrement_remote_weak_callable_and_signal_if_appropriate();
                held_proxy_sp_ = r.held_proxy_sp_;
                ultimate_actual_object_proxy_sp_ = r.ultimate_actual_object_proxy_sp_;
                if (ultimate_actual_object_proxy_sp_)
                    ultimate_actual_object_proxy_sp_->increment_remote_weak_callable();
            }
            return *this;
        }
        optimistic_ptr& operator=(optimistic_ptr&& r) noexcept
        {
            if (this != &r)
            {
                if (ultimate_actual_object_proxy_sp_)
                    ultimate_actual_object_proxy_sp_->decrement_remote_weak_callable_and_signal_if_appropriate();
                held_proxy_sp_ = std::move(r.held_proxy_sp_);
                ultimate_actual_object_proxy_sp_ = std::move(r.ultimate_actual_object_proxy_sp_);
            }
            return *this;
        }
        ~optimistic_ptr()
        {
            if (ultimate_actual_object_proxy_sp_)
                ultimate_actual_object_proxy_sp_->decrement_remote_weak_callable_and_signal_if_appropriate();
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
            static_assert(std::is_convertible_v<Y*, T*>, "Y* must be convertible to T*");
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
        weak_ptr(weak_ptr&& r) noexcept
            : cb_(r.cb_)
            , ptr_for_lock_(r.ptr_for_lock_)
        {
            r.cb_ = nullptr;
            r.ptr_for_lock_ = nullptr;
        }
        ~weak_ptr()
        {
            if (cb_)
                cb_->decrement_local_weak_and_destroy_if_zero();
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
            // This check assumes this_cb_object_proxy_sp_ exists and has get_remote_strong_count.
            // If object_proxy doesn't expose counts, this logic for expired() needs adjustment.
            // if (cb_->this_cb_object_proxy_sp_ && cb_->this_cb_object_proxy_sp_->get_remote_strong_count() > 0) {
            //    return false;
            // }
            return true;
        }
        void reset() noexcept { weak_ptr().swap(*this); }
        void swap(weak_ptr& r) noexcept
        {
            std::swap(cb_, r.cb_);
            std::swap(ptr_for_lock_, r.ptr_for_lock_);
        }
    };

    template<typename T, typename Deleter> class unique_ptr
    {
    public:
        using pointer = T*;
        using element_type = T;
        using deleter_type = Deleter;
        constexpr unique_ptr() noexcept
            : ptr_(nullptr)
            , deleter_()
        {
        }
        constexpr unique_ptr(std::nullptr_t) noexcept
            : unique_ptr()
        {
        }
        explicit unique_ptr(pointer p) noexcept
            : ptr_(p)
            , deleter_()
        {
        }
        unique_ptr(pointer p, const Deleter& d) noexcept
            : ptr_(p)
            , deleter_(d)
        {
        }
        unique_ptr(pointer p, Deleter&& d) noexcept
            : ptr_(p)
            , deleter_(std::move(d))
        {
        }
        unique_ptr(unique_ptr&& u) noexcept
            : ptr_(u.release())
            , deleter_(std::move(u.get_deleter()))
        {
        }
        template<typename U,
            typename E,
            typename = std::enable_if_t<std::is_convertible_v<typename unique_ptr<U, E>::pointer, pointer>
                                        && std::is_assignable_v<Deleter&, E&&>>>
        unique_ptr(unique_ptr<U, E>&& u) noexcept
            : ptr_(u.release())
            , deleter_(std::move(u.get_deleter()))
        {
        }
        ~unique_ptr()
        {
            if (ptr_)
                get_deleter()(ptr_);
        }
        unique_ptr& operator=(unique_ptr&& u) noexcept
        {
            if (this != &u)
            {
                reset(u.release());
                get_deleter() = std::move(u.get_deleter());
            }
            return *this;
        }
        unique_ptr& operator=(std::nullptr_t) noexcept
        {
            reset();
            return *this;
        }
        T& operator*() const { return *ptr_; }
        pointer operator->() const noexcept { return ptr_; }
        pointer get() const noexcept { return ptr_; }
        deleter_type& get_deleter() noexcept { return deleter_; }
        const deleter_type& get_deleter() const noexcept { return deleter_; }
        explicit operator bool() const noexcept { return ptr_ != nullptr; }
        pointer release() noexcept
        {
            pointer p = ptr_;
            ptr_ = nullptr;
            return p;
        }
        void reset(pointer p = pointer()) noexcept
        {
            pointer old = ptr_;
            ptr_ = p;
            if (old)
                get_deleter()(old);
        }
        void swap(unique_ptr& other) noexcept
        {
            std::swap(ptr_, other.ptr_);
            std::swap(deleter_, other.deleter_);
        }
        unique_ptr(const unique_ptr&) = delete;
        unique_ptr& operator=(const unique_ptr&) = delete;

    private:
        pointer ptr_;
        Deleter deleter_;
    };
    template<typename T, typename Deleter> class unique_ptr<T[], Deleter>
    { /* Array version needs T& operator[](size_t) etc. */
    };

    // --- Free Functions ---
    template<class T1, class T2>
    [[nodiscard]] inline shared_ptr<T1> dynamic_pointer_cast(const shared_ptr<T2>& from) noexcept
    {
        if (!from)
            return nullptr;

        if (const T1* ptr_local_casted = static_cast<const T1*>(from->query_interface(T1::get_id(RPC_VERSION_2))))
        {
            return shared_ptr<T1>(from, const_cast<T1*>(ptr_local_casted));
        }

        if (auto proxy_from = from->query_proxy_base())
        {
            if (auto actual_obj_proxy = proxy_from->get_object_proxy())
            {
                shared_ptr<T1> result_iface_proxy;
                if (actual_obj_proxy->template query_interface<T1>(result_iface_proxy) == 0)
                {
                    return result_iface_proxy;
                }
            }
        }
        return shared_ptr<T1>();
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

    // make_shared for local objects (no initial object_proxy)
    template<typename T, typename... Args> rpc::shared_ptr<T> make_shared(Args&&... args)
    {
        // This version of make_shared is for local objects that do not initially have an object_proxy.
        // Their this_cb_object_proxy_sp_ in the control block will be nullptr.
        static_assert(std::is_base_of_v<rpc::casting_interface, T>,
            "T must be a casting_interface for rpc::make_shared, "
            "even if initially local, to support potential RPC interactions later.");

        using Alloc = std::allocator<T>; // Allocator for T object itself
        // Allocator for the combined control block and T object
        using CBAllocForMakeShared =
            typename std::allocator_traits<Alloc>::template rebind_alloc<internal::control_block_make_shared<T, Alloc, Args...>>;

        CBAllocForMakeShared cb_alloc; // Default construct the allocator for the CB
        auto* cb_ptr = std::allocator_traits<CBAllocForMakeShared>::allocate(cb_alloc, 1);

        try
        {
            // Construct control_block_make_shared.
            // This version of control_block_make_shared's constructor will pass nullptr
            // for the object_proxy to the control_block_base constructor.
            std::allocator_traits<CBAllocForMakeShared>::construct(cb_alloc,
                cb_ptr,
                Alloc(),                      // Pass allocator for T to CB ctor
                std::forward<Args>(args)...); // Args for T's constructor
        }
        catch (...)
        {
            std::allocator_traits<CBAllocForMakeShared>::deallocate(cb_alloc, cb_ptr, 1);
            throw;
        }
        // CB's local_shared_owners is implicitly 1 after make_shared's CB construction.
        // Use private shared_ptr constructor.
        // The ultimate_actual_object_proxy_sp_ in the new shared_ptr will be nullptr.
        return rpc::shared_ptr<T>(
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

        struct access_detail_for_est
        {
            template<typename U> static internal::control_block_base*& cb(rpc::shared_ptr<U>& sp) { return sp.cb_; }
            template<typename U> static U*& ptr(rpc::shared_ptr<U>& sp) { return sp.ptr_; }
            template<typename U> static std::shared_ptr<rpc::object_proxy>& ultimate_proxy(rpc::shared_ptr<U>& sp)
            {
                return sp.ultimate_actual_object_proxy_sp_;
            }
        };

        template<typename ActualPtrType>
        void internal_set_weak_this(internal::control_block_base* cb_for_this_obj, ActualPtrType* ptr_to_this_obj) const
        {
            if (static_cast<const void*>(static_cast<const T*>(ptr_to_this_obj)) == static_cast<const void*>(this))
            {
                if (weak_this_.expired())
                {
                    if (cb_for_this_obj && ptr_to_this_obj)
                    {
                        rpc::shared_ptr<T> temp_sp_for_weak_init;
                        access_detail_for_est::cb(temp_sp_for_weak_init) = cb_for_this_obj;
                        access_detail_for_est::ptr(temp_sp_for_weak_init)
                            = const_cast<T*>(static_cast<const T*>(ptr_to_this_obj));
                        access_detail_for_est::ultimate_proxy(temp_sp_for_weak_init) = get_ultimate_object_proxy_for(
                            const_cast<T*>(static_cast<const T*>(ptr_to_this_obj)), cb_for_this_obj);
                        weak_this_ = temp_sp_for_weak_init;
                    }
                }
            }
        }

    public:
        rpc::shared_ptr<T> shared_from_this() { return weak_this_.lock(); }
        rpc::shared_ptr<const T> shared_from_this() const { return weak_this_.lock(); }
        rpc::weak_ptr<T> weak_from_this() const noexcept { return weak_this_; }

        template<typename U, typename... Args>
        friend rpc::shared_ptr<U> make_shared(Args&&...); // Updated make_shared signature
        template<typename U> friend class shared_ptr;
    };

    template<typename InterfaceType, typename ConcreteLocalProxyType>
    rpc::shared_ptr<InterfaceType> create_shared_ptr_to_local_proxy(
        const rpc::shared_ptr<InterfaceType>& actual_object_sp_target,
        std::shared_ptr<rpc::object_proxy> obj_proxy_for_local_proxy_instance)
    {
        static_assert(std::is_base_of_v<InterfaceType, ConcreteLocalProxyType>, "Proxy must implement interface.");
        static_assert(std::is_base_of_v<rpc::proxy_base, ConcreteLocalProxyType>, "Proxy must be a proxy_base.");

        ConcreteLocalProxyType* raw_local_proxy = new ConcreteLocalProxyType(actual_object_sp_target);

        if (auto* proxy_base_ptr = raw_local_proxy->query_proxy_base())
        {
            const_cast<rpc::proxy_base*>(proxy_base_ptr)->object_proxy_ = obj_proxy_for_local_proxy_instance;
        }

        return rpc::shared_ptr<InterfaceType>(static_cast<InterfaceType*>(raw_local_proxy),
            obj_proxy_for_local_proxy_instance,
            std::default_delete<ConcreteLocalProxyType>());
    }

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
    template<typename T, typename D> struct hash<rpc::unique_ptr<T, D>>
    {
        size_t operator()(const rpc::unique_ptr<T, D>& p) const noexcept { return std::hash<T*>()(p.get()); }
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

#endif // RPC_SMART_POINTERS_CORE_HPP
