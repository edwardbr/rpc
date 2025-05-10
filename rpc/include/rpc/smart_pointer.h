#ifndef RPC_SMART_POINTERS_CORE_HPP // Changed guard to reflect potential new filename
#define RPC_SMART_POINTERS_CORE_HPP

#include <atomic>
#include <memory>       // For std::default_delete, std::allocator, std::allocator_traits, std::shared_ptr
#include <utility>      // For std::swap, std::move, std::forward
#include <stdexcept>    // For std::bad_weak_ptr, std::invalid_argument
#include <type_traits>  // For std::is_base_of_v, std::is_convertible_v, std::remove_extent_t, etc.
#include <functional>   // For std::hash
#include <cstddef>      // For std::nullptr_t, std::size_t
#include <vector>       // For interface_ordinal example (if still needed here, or comes from types.h)
// #include <string> // Only if interface_ordinal uses it directly here

// --- Configuration Macros (User Defined) ---
// These should ideally be in a global config header included by rpc.h or directly by users.
// For this file, we'll assume they are defined/available.
// #define RPC_V2 
#ifndef RPC_VERSION_2 
#define RPC_VERSION_2 0 
#endif

// --- Include User's Base Headers ---
// These must exist in the include path, e.g., in an "rpc" directory.
// These headers should NOT include this smart_pointers.hpp file to avoid cycles.
// types.h should define rpc::interface_ordinal (if not defined below for completeness)
// casting_interface.h should define rpc::casting_interface and forward declare rpc::proxy_base
// #include <rpc/types.h> 
// #include <rpc/casting_interface.h> 

// --- Minimal necessary declarations if not from included headers ---
// (These are duplicated from the previous version for context if the includes above are commented out)
namespace rpc {
    // Forward declarations for types defined in other headers (e.g., proxy.h)
    // These are essential for breaking include cycles.
    class proxy_base;
    class object_proxy;

    // Forward declarations for smart pointers themselves
    template <typename T> class shared_ptr;
    template <typename T> class weak_ptr;
    template <typename T, typename Deleter = std::default_delete<T>> class unique_ptr;
    template <typename T> class optimistic_ptr;
    template <typename T> class enable_shared_from_this;

    // Assuming interface_ordinal and casting_interface are defined in included headers.
    // If not, their definitions would be needed here or forward declared if only pointers are used.
    // For this example, let's re-state minimal versions if includes are not active:
    #ifndef RPC_TYPES_H_CONCEPT // Example guard if types.h was real
    struct interface_ordinal {
        std::vector<unsigned char> id_data; 
        uint64_t version {0};
        bool operator==(const interface_ordinal& other) const { return version == other.version && id_data == other.id_data; }
        bool operator<(const interface_ordinal& other) const { if (version < other.version) return true; if (version > other.version) return false; return id_data < other.id_data; }
    };
    #endif

    #ifndef RPC_CASTING_INTERFACE_H_CONCEPT // Example guard
    class casting_interface {
    public:
        virtual ~casting_interface() = default;
        virtual void* get_address() const = 0; 
        virtual const rpc::casting_interface* query_interface(rpc::interface_ordinal interface_id) const = 0;
        template<typename InterfaceType>
        const InterfaceType* query_interface() const {
            // This requires InterfaceType to have a static get_id method.
            // Ensure InterfaceType is defined or T1::get_id won't compile in dynamic_pointer_cast.
            // This implies IDL-generated interface headers must be included before this point,
            // or this templated query_interface must be defined after InterfaceType is known.
            // For now, assume InterfaceType::get_id is resolvable.
            return static_cast<const InterfaceType*>(query_interface(InterfaceType::get_id(RPC_VERSION_2)));
        }
        virtual proxy_base* query_proxy_base() const { return nullptr; }
    };
    #endif

} // namespace rpc
// --- End of Minimal necessary declarations ---


// The rest of the smart pointer code (internal namespace, class definitions, free functions)
// will now use the forward-declared rpc::proxy_base and rpc::object_proxy.
// Their methods that call members of proxy_base/object_proxy will compile correctly
// because the full definitions will be available when proxy.h includes this file.

namespace rpc {

namespace internal {
    struct control_block_base {
        std::atomic<long> local_shared_owners{0};
        std::atomic<long> local_weak_owners{1};
        std::shared_ptr<rpc::object_proxy> this_cb_object_proxy_sp_; 
        void* managed_object_ptr_{nullptr};

        control_block_base(void* obj_ptr, std::shared_ptr<rpc::object_proxy> obj_proxy_for_this_cb_obj)
            : managed_object_ptr_(obj_ptr), this_cb_object_proxy_sp_(std::move(obj_proxy_for_this_cb_obj)) {}
        virtual ~control_block_base() = default;
        virtual void dispose_object_actual() = 0;
        virtual void destroy_self_actual() = 0;
        void* get_managed_object_ptr() const { return managed_object_ptr_; }
        void increment_local_shared() { local_shared_owners.fetch_add(1, std::memory_order_relaxed); }
        void increment_local_weak() { local_weak_owners.fetch_add(1, std::memory_order_relaxed); }

        void decrement_local_shared_and_dispose_if_zero() {
            if (local_shared_owners.fetch_sub(1, std::memory_order_acq_rel) == 1) { 
                dispose_object_actual();
                decrement_local_weak_and_destroy_if_zero(); 
            }
        }

        void decrement_local_weak_and_destroy_if_zero() {
            if (local_weak_owners.fetch_sub(1, std::memory_order_acq_rel) == 1) { 
                if (local_shared_owners.load(std::memory_order_acquire) == 0) { 
                    destroy_self_actual();
                }
            }
        }
    };

    template <typename T, typename Deleter, typename Alloc>
    struct control_block_impl : public control_block_base {
        Deleter object_deleter_; Alloc control_block_allocator_;
        control_block_impl(T* p, std::shared_ptr<rpc::object_proxy> obj_proxy, Deleter d, Alloc a)
            : control_block_base(p, std::move(obj_proxy)), object_deleter_(std::move(d)), control_block_allocator_(std::move(a)) {}
        void dispose_object_actual() override { T* obj_ptr = static_cast<T*>(managed_object_ptr_); if (obj_ptr) { object_deleter_(obj_ptr); managed_object_ptr_ = nullptr; } }
        void destroy_self_actual() override { Alloc alloc_copy = control_block_allocator_; std::allocator_traits<Alloc>::destroy(alloc_copy, this); std::allocator_traits<Alloc>::deallocate(alloc_copy, this, 1); }
    };
    
    template <typename T, typename Alloc, typename... Args> 
    struct control_block_make_shared : public control_block_base {
        Alloc allocator_instance_; 
        union { T object_instance_; }; 

        template<typename... ConcreteArgs>
        control_block_make_shared(std::shared_ptr<rpc::object_proxy> obj_proxy, Alloc alloc_for_t_and_cb, ConcreteArgs&&... args)
        : control_block_base(&object_instance_, std::move(obj_proxy)), 
          allocator_instance_(std::move(alloc_for_t_and_cb)) {
            std::allocator_traits<Alloc>::construct(allocator_instance_, &object_instance_, std::forward<ConcreteArgs>(args)...);
        }
        void dispose_object_actual() override {
            if (managed_object_ptr_) {
                 std::allocator_traits<Alloc>::destroy(allocator_instance_, &object_instance_);
                 managed_object_ptr_ = nullptr;
            }
        }
        void destroy_self_actual() override {
            Alloc alloc_copy = allocator_instance_; 
            if (managed_object_ptr_){ 
                 std::allocator_traits<Alloc>::destroy(allocator_instance_, &object_instance_);
                 managed_object_ptr_ = nullptr;
            }
            std::allocator_traits<Alloc>::deallocate(alloc_copy, this, 1); 
        }
         ~control_block_make_shared() {}
    };
} // namespace internal

template<typename T> 
std::shared_ptr<rpc::object_proxy> get_ultimate_object_proxy_for(T* ptr, rpc::internal::control_block_base* cb_of_ptr) {
    if (!ptr) return nullptr;
    if (auto proxy = ptr->query_proxy_base()) { 
        return proxy->get_object_proxy(); 
    } else if (cb_of_ptr) { 
        return cb_of_ptr->this_cb_object_proxy_sp_;
    }
    return nullptr;
}

template <typename T> 
class shared_ptr {
    internal::control_block_base* cb_{nullptr};
    T* ptr_{nullptr};
    std::shared_ptr<rpc::object_proxy> ultimate_actual_object_proxy_sp_{nullptr};

    void acquire_this() noexcept { 
        if (cb_) cb_->increment_local_shared(); 
        if (ultimate_actual_object_proxy_sp_) ultimate_actual_object_proxy_sp_->increment_remote_strong(); 
    }
    void release_this() noexcept { 
        if (ultimate_actual_object_proxy_sp_) ultimate_actual_object_proxy_sp_->decrement_remote_strong_and_signal_if_appropriate(); 
        if (cb_) cb_->decrement_local_shared_and_dispose_if_zero(); 
    }
    
    shared_ptr(internal::control_block_base* cb, T* p) : cb_(cb), ptr_(p) {
        if (cb_) { 
            ultimate_actual_object_proxy_sp_ = get_ultimate_object_proxy_for(ptr_, cb_);
            if (ultimate_actual_object_proxy_sp_) {
                ultimate_actual_object_proxy_sp_->increment_remote_strong();
            }
        } else { ptr_ = nullptr; }
    }

public:
    using element_type = std::remove_extent_t<T>;
    using weak_type = rpc::weak_ptr<T>;

    constexpr shared_ptr() noexcept = default;
    constexpr shared_ptr(std::nullptr_t) noexcept {}

    template <typename Y, typename Deleter = std::default_delete<Y>, typename Alloc = std::allocator<char>,
              typename = std::enable_if_t<std::is_base_of_v<T, Y> && std::is_base_of_v<casting_interface, Y>>>
    explicit shared_ptr(Y* p, std::shared_ptr<rpc::object_proxy> obj_proxy_for_p_obj, Deleter d = Deleter(), Alloc cb_alloc = Alloc())
        : ptr_(p), cb_(nullptr), ultimate_actual_object_proxy_sp_(nullptr) {
        if (!ptr_) return; 
        try {
            using ActualCB = internal::control_block_impl<Y, Deleter, Alloc>;
            typename std::allocator_traits<Alloc>::template rebind_alloc<ActualCB> actual_cb_alloc(cb_alloc);
            cb_ = std::allocator_traits<decltype(actual_cb_alloc)>::allocate(actual_cb_alloc, 1);
            new (cb_) ActualCB(p, std::move(obj_proxy_for_p_obj), std::move(d), std::move(cb_alloc)); 
        } catch (...) {
            if(cb_) { 
                typename std::allocator_traits<Alloc>::template rebind_alloc<internal::control_block_impl<Y, Deleter, Alloc>> actual_cb_alloc(cb_alloc);
                std::allocator_traits<decltype(actual_cb_alloc)>::deallocate(actual_cb_alloc, static_cast<internal::control_block_impl<Y, Deleter, Alloc>*>(cb_), 1);
            }
            cb_ = nullptr; ptr_ = nullptr; 
            throw; 
        }
        ultimate_actual_object_proxy_sp_ = get_ultimate_object_proxy_for(ptr_, cb_);
        acquire_this(); 
    }
    
    template <typename Y> 
    shared_ptr(const shared_ptr<Y>& r, T* p_alias) noexcept
        : cb_(r.internal_get_cb()), ptr_(p_alias), 
          ultimate_actual_object_proxy_sp_(r.internal_get_ultimate_object_proxy()) {
        acquire_this();
    }

    shared_ptr(const shared_ptr& r) noexcept : cb_(r.cb_), ptr_(r.ptr_), ultimate_actual_object_proxy_sp_(r.ultimate_actual_object_proxy_sp_) { acquire_this(); }
    shared_ptr(shared_ptr&& r) noexcept : cb_(r.cb_), ptr_(r.ptr_), ultimate_actual_object_proxy_sp_(std::move(r.ultimate_actual_object_proxy_sp_)) { r.cb_ = nullptr; r.ptr_ = nullptr; }
    
    template <typename Y, typename = std::enable_if_t<std::is_convertible_v<Y*, T*>>>
    shared_ptr(const shared_ptr<Y>& r) noexcept
        : cb_(r.internal_get_cb()), ptr_(static_cast<T*>(r.internal_get_ptr())),
          ultimate_actual_object_proxy_sp_(r.internal_get_ultimate_object_proxy()) {
        acquire_this();
    }
    template <typename Y, typename = std::enable_if_t<std::is_convertible_v<Y*, T*>>>
    shared_ptr(shared_ptr<Y>&& r) noexcept
        : cb_(r.internal_get_cb()), ptr_(static_cast<T*>(r.internal_get_ptr())),
          ultimate_actual_object_proxy_sp_(std::move(r.internal_get_ultimate_object_proxy_ref())) { 
        r.cb_ = nullptr; r.ptr_ = nullptr;
    }

    template <typename Y, typename Deleter_U, typename = std::enable_if_t<std::is_convertible_v<Y*, T*> && std::is_base_of_v<casting_interface, Y>>>
    shared_ptr(rpc::unique_ptr<Y, Deleter_U>&& u, std::shared_ptr<rpc::object_proxy> obj_proxy_for_u_obj) : ptr_(nullptr), cb_(nullptr) { 
        if(!u) return; Y* raw_p = u.get(); Deleter_U deleter = u.get_deleter(); 
        using ActualCB = internal::control_block_impl<Y,Deleter_U,std::allocator<char>>; std::allocator<ActualCB> cba; ActualCB* raw_cb=nullptr; 
        try {raw_cb=cba.allocate(1); new(raw_cb)ActualCB(raw_p,obj_proxy_for_u_obj,std::move(deleter),std::allocator<char>()); cb_=raw_cb;ptr_=raw_p;u.release();}
        catch(...){if(raw_cb)cba.deallocate(raw_cb,1); throw;} 
        if (cb_) { ultimate_actual_object_proxy_sp_ = get_ultimate_object_proxy_for(ptr_, cb_); acquire_this(); } else { ptr_ = nullptr; }
    }

    ~shared_ptr() { release_this(); }
    shared_ptr& operator=(const shared_ptr& r) noexcept { if (this != &r) { release_this(); cb_ = r.cb_; ptr_ = r.ptr_; ultimate_actual_object_proxy_sp_ = r.ultimate_actual_object_proxy_sp_; acquire_this(); } return *this; }
    shared_ptr& operator=(shared_ptr&& r) noexcept { if (this != &r) { release_this(); cb_ = r.cb_; ptr_ = r.ptr_; ultimate_actual_object_proxy_sp_ = std::move(r.ultimate_actual_object_proxy_sp_); r.cb_ = nullptr; r.ptr_ = nullptr; } return *this; }

    void reset() noexcept { shared_ptr().swap(*this); }
    template <typename Y, typename Deleter = std::default_delete<Y>, typename Alloc = std::allocator<char>>
    void reset(Y* p, std::shared_ptr<rpc::object_proxy> obj_proxy_for_p_obj, Deleter d = Deleter(), Alloc cb_alloc = Alloc()) {
        shared_ptr(p, std::move(obj_proxy_for_p_obj), std::move(d), std::move(cb_alloc)).swap(*this);
    }

    void swap(shared_ptr& r) noexcept { std::swap(ptr_, r.ptr_); std::swap(cb_, r.cb_); ultimate_actual_object_proxy_sp_.swap(r.ultimate_actual_object_proxy_sp_); }

    T* get() const noexcept { return ptr_; }
    T& operator*() const noexcept { return *ptr_; }
    T* operator->() const noexcept { return ptr_; }
    long use_count() const noexcept { return cb_ ? cb_->local_shared_owners.load(std::memory_order_relaxed) : 0; }
    explicit operator bool() const noexcept { return ptr_ != nullptr; }

    internal::control_block_base* internal_get_cb() const { return cb_; }
    T* internal_get_ptr() const { return ptr_; }
    const std::shared_ptr<rpc::object_proxy>& internal_get_ultimate_object_proxy() const { return ultimate_actual_object_proxy_sp_; }
    std::shared_ptr<rpc::object_proxy>& internal_get_ultimate_object_proxy_ref() { return ultimate_actual_object_proxy_sp_; }

    template<typename U> friend class optimistic_ptr;
    template<typename U> friend class weak_ptr;
    template<typename U, typename... Args> friend rpc::shared_ptr<U> make_shared(std::shared_ptr<rpc::object_proxy> obj_proxy, Args&&... args);
    template<class T1_cast, class T2_cast> friend shared_ptr<T1_cast> dynamic_pointer_cast(const shared_ptr<T2_cast>& from) noexcept;
};

template <typename T> 
class optimistic_ptr {
    rpc::shared_ptr<T> held_proxy_sp_; 
    std::shared_ptr<rpc::object_proxy> ultimate_actual_object_proxy_sp_{nullptr};
public:
    using element_type = T;
    constexpr optimistic_ptr() noexcept = default;
    constexpr optimistic_ptr(std::nullptr_t) noexcept {}

    explicit optimistic_ptr(const rpc::shared_ptr<T>& proxy_sp_param) {
        if (proxy_sp_param && proxy_sp_param->query_proxy_base() != nullptr) {
            held_proxy_sp_ = proxy_sp_param;
            if (auto proxy_base_ptr = held_proxy_sp_->query_proxy_base()) { 
                 ultimate_actual_object_proxy_sp_ = proxy_base_ptr->get_object_proxy();
            }
            if (ultimate_actual_object_proxy_sp_) {
                ultimate_actual_object_proxy_sp_->increment_remote_weak_callable();
            } else { held_proxy_sp_.reset(); }
        } else if (proxy_sp_param) { /* Error: not a proxy */ }
    }
    explicit optimistic_ptr(rpc::shared_ptr<T>&& proxy_sp_param) { 
         if (proxy_sp_param && proxy_sp_param->query_proxy_base() != nullptr) {
            held_proxy_sp_ = std::move(proxy_sp_param);
            if (auto proxy_base_ptr = held_proxy_sp_->query_proxy_base()) {
                 ultimate_actual_object_proxy_sp_ = proxy_base_ptr->get_object_proxy();
            }
            if (ultimate_actual_object_proxy_sp_) {
                ultimate_actual_object_proxy_sp_->increment_remote_weak_callable();
            } else { held_proxy_sp_.reset(); }
        } else if (proxy_sp_param) { /* Error: not a proxy */ }
    }

    optimistic_ptr(const optimistic_ptr& r) : held_proxy_sp_(r.held_proxy_sp_), ultimate_actual_object_proxy_sp_(r.ultimate_actual_object_proxy_sp_) { if (ultimate_actual_object_proxy_sp_) ultimate_actual_object_proxy_sp_->increment_remote_weak_callable(); }
    optimistic_ptr(optimistic_ptr&& r) noexcept : held_proxy_sp_(std::move(r.held_proxy_sp_)), ultimate_actual_object_proxy_sp_(std::move(r.ultimate_actual_object_proxy_sp_)) {}
    optimistic_ptr& operator=(const optimistic_ptr& r) { if(this != &r){ if(ultimate_actual_object_proxy_sp_) ultimate_actual_object_proxy_sp_->decrement_remote_weak_callable_and_signal_if_appropriate(); held_proxy_sp_=r.held_proxy_sp_; ultimate_actual_object_proxy_sp_=r.ultimate_actual_object_proxy_sp_; if(ultimate_actual_object_proxy_sp_) ultimate_actual_object_proxy_sp_->increment_remote_weak_callable(); } return *this; }
    optimistic_ptr& operator=(optimistic_ptr&& r) noexcept { if(this != &r){ if(ultimate_actual_object_proxy_sp_) ultimate_actual_object_proxy_sp_->decrement_remote_weak_callable_and_signal_if_appropriate(); held_proxy_sp_=std::move(r.held_proxy_sp_); ultimate_actual_object_proxy_sp_=std::move(r.ultimate_actual_object_proxy_sp_); } return *this; }
    ~optimistic_ptr() { if (ultimate_actual_object_proxy_sp_) ultimate_actual_object_proxy_sp_->decrement_remote_weak_callable_and_signal_if_appropriate(); }

    T* operator->() const noexcept { return held_proxy_sp_.operator->(); }
    T& operator*() const { return *held_proxy_sp_; }
    T* get() const noexcept { return held_proxy_sp_.get(); }
    long use_count() const noexcept { return held_proxy_sp_.use_count(); }
    explicit operator bool() const noexcept { return static_cast<bool>(held_proxy_sp_); }
    rpc::shared_ptr<T> get_proxy_shared_ptr() const { return held_proxy_sp_; }
    void reset() noexcept { optimistic_ptr().swap(*this); }
    void reset(const rpc::shared_ptr<T>& proxy_sp) { optimistic_ptr(proxy_sp).swap(*this); }
    void reset(rpc::shared_ptr<T>&& proxy_sp) { optimistic_ptr(std::move(proxy_sp)).swap(*this); }
    void swap(optimistic_ptr& r) noexcept { held_proxy_sp_.swap(r.held_proxy_sp_); ultimate_actual_object_proxy_sp_.swap(r.ultimate_actual_object_proxy_sp_); }
};

template <typename T>
class weak_ptr {
    internal::control_block_base* cb_{nullptr}; T* ptr_for_lock_{nullptr};
public:
    using element_type = std::remove_extent_t<T>; constexpr weak_ptr() noexcept = default;
    template <typename Y> weak_ptr(const shared_ptr<Y>& r) noexcept : cb_(r.internal_get_cb()), ptr_for_lock_(r.internal_get_ptr()) { static_assert(std::is_convertible_v<Y*, T*>, "Y* must be convertible to T*"); if (cb_) cb_->increment_local_weak(); }
    template <typename Y> weak_ptr(const optimistic_ptr<Y>& o) noexcept : weak_ptr(o.get_proxy_shared_ptr()) {}
    weak_ptr(const weak_ptr& r) noexcept : cb_(r.cb_), ptr_for_lock_(r.ptr_for_lock_) { if (cb_) cb_->increment_local_weak(); }
    weak_ptr(weak_ptr&& r) noexcept : cb_(r.cb_), ptr_for_lock_(r.ptr_for_lock_) { r.cb_ = nullptr; r.ptr_for_lock_ = nullptr; }
    ~weak_ptr() { if (cb_) cb_->decrement_local_weak_and_destroy_if_zero(); } 
    rpc::shared_ptr<T> lock() const { if (!cb_) return {}; long c = cb_->local_shared_owners.load(std::memory_order_relaxed); while (c > 0) { if (cb_->local_shared_owners.compare_exchange_weak(c,c+1,std::memory_order_acq_rel,std::memory_order_relaxed)) return rpc::shared_ptr<T>(cb_, static_cast<T*>(ptr_for_lock_));} return {}; }
    long use_count() const noexcept { return cb_ ? cb_->local_shared_owners.load(std::memory_order_relaxed) : 0; }
    bool expired() const noexcept { 
        if (!cb_) return true; 
        if (use_count() > 0) return false; 
        // This check relies on object_proxy having get_remote_strong_count()
        // and this_cb_object_proxy_sp_ being the correct one for the observed object.
        // If object_proxy doesn't expose counts, this needs to be re-evaluated.
        // if (cb_->this_cb_object_proxy_sp_ && cb_->this_cb_object_proxy_sp_->get_remote_strong_count() > 0) {
        //    return false; 
        // }
        return true; 
    }
    void reset() noexcept { weak_ptr().swap(*this); }
    void swap(weak_ptr& r) noexcept { std::swap(cb_, r.cb_); std::swap(ptr_for_lock_, r.ptr_for_lock_); }
};

// unique_ptr: Default argument removed from the primary template definition.
template <typename T, typename Deleter /* = std::default_delete<T> */ > 
class unique_ptr {
public:
    using pointer = T*; using element_type = T; using deleter_type = Deleter;
    constexpr unique_ptr() noexcept : ptr_(nullptr), deleter_() {}
    constexpr unique_ptr(std::nullptr_t) noexcept : unique_ptr() {}
    explicit unique_ptr(pointer p) noexcept : ptr_(p), deleter_() {}
    unique_ptr(pointer p, const Deleter& d) noexcept : ptr_(p), deleter_(d) {}
    unique_ptr(pointer p, Deleter&& d) noexcept : ptr_(p), deleter_(std::move(d)) {}
    unique_ptr(unique_ptr&& u) noexcept : ptr_(u.release()), deleter_(std::move(u.get_deleter())) {}
    template <typename U, typename E, typename = std::enable_if_t<std::is_convertible_v<typename unique_ptr<U, E>::pointer, pointer> && std::is_assignable_v<Deleter&, E&&>>>
    unique_ptr(unique_ptr<U, E>&& u) noexcept : ptr_(u.release()), deleter_(std::move(u.get_deleter())) {}
    ~unique_ptr() { if (ptr_) get_deleter()(ptr_); }
    unique_ptr& operator=(unique_ptr&& u) noexcept { if (this != &u) { reset(u.release()); get_deleter() = std::move(u.get_deleter()); } return *this; }
    unique_ptr& operator=(std::nullptr_t) noexcept { reset(); return *this; }
    T& operator*() const { return *ptr_; }
    pointer operator->() const noexcept { return ptr_; }
    pointer get() const noexcept { return ptr_; }
    deleter_type& get_deleter() noexcept { return deleter_; }
    const deleter_type& get_deleter() const noexcept { return deleter_; }
    explicit operator bool() const noexcept { return ptr_ != nullptr; }
    pointer release() noexcept { pointer p = ptr_; ptr_ = nullptr; return p; }
    void reset(pointer p = pointer()) noexcept { pointer old = ptr_; ptr_ = p; if (old) get_deleter()(old); }
    void swap(unique_ptr& other) noexcept { std::swap(ptr_, other.ptr_); std::swap(deleter_, other.deleter_); }
    unique_ptr(const unique_ptr&) = delete;
    unique_ptr& operator=(const unique_ptr&) = delete;
private:
    pointer ptr_; Deleter deleter_;
};
// Array specialization for unique_ptr - default argument for Deleter should also be on forward declaration only.
template <typename T, typename Deleter> class unique_ptr<T[], Deleter> { /* Array version needs T& operator[](size_t) etc. */ };

// --- Free Functions ---
template<class T1, class T2> 
[[nodiscard]] inline shared_ptr<T1> dynamic_pointer_cast(const shared_ptr<T2>& from) noexcept {
    if(!from) return nullptr;
    
    // This requires T1 to have a static get_id method.
    if (const T1* ptr_local_casted = static_cast<const T1*>(from->query_interface(T1::get_id(RPC_VERSION_2)))) {
        return shared_ptr<T1>(from, const_cast<T1*>(ptr_local_casted));
    }

    // This part requires proxy_base and object_proxy to be fully defined when instantiated.
    if (auto proxy_from = from->query_proxy_base()) { 
        if (auto actual_obj_proxy = proxy_from->get_object_proxy()) { 
            shared_ptr<T1> result_iface_proxy;
            // This requires object_proxy::query_interface to be defined.
            if (actual_obj_proxy->template query_interface<T1>(result_iface_proxy) == 0) { 
                return result_iface_proxy;
            }
        }
    }
    return shared_ptr<T1>();
}

template<class T1, class T2> [[nodiscard]] inline shared_ptr<T1> static_pointer_cast(const shared_ptr<T2>& from) noexcept { if (!from) return nullptr; T1* p = static_cast<T1*>(from.get()); return shared_ptr<T1>(from, p); }
template<class T1, class T2> [[nodiscard]] inline shared_ptr<T1> const_pointer_cast(const shared_ptr<T2>& from) noexcept { if (!from) return nullptr; T1* p = const_cast<T1*>(from.get()); return shared_ptr<T1>(from, p); }
template<class T1, class T2> [[nodiscard]] inline shared_ptr<T1> reinterpret_pointer_cast(const shared_ptr<T2>& from) noexcept { if (!from) return nullptr; T1* p = reinterpret_cast<T1*>(from.get()); return shared_ptr<T1>(from, p); }

template <typename T, typename... Args> 
rpc::shared_ptr<T> make_shared(std::shared_ptr<rpc::object_proxy> obj_proxy_for_t, Args&&... args) {
    static_assert(std::is_base_of_v<rpc::casting_interface, T>, "T must be a casting_interface for make_shared with object_proxy.");
    using Alloc = std::allocator<T>;
    using CBType = internal::control_block_make_shared<T, Alloc, Args...>;
    typename std::allocator_traits<Alloc>::template rebind_alloc<CBType> cb_alloc_rebound;
    CBType* cb_ptr = std::allocator_traits<decltype(cb_alloc_rebound)>::allocate(cb_alloc_rebound, 1);
    try {
        std::allocator_traits<decltype(cb_alloc_rebound)>::construct(cb_alloc_rebound, cb_ptr,
            std::move(obj_proxy_for_t), Alloc(), std::forward<Args>(args)...);
    } catch (...) {
        std::allocator_traits<decltype(cb_alloc_rebound)>::deallocate(cb_alloc_rebound, cb_ptr, 1);
        throw;
    }
    cb_ptr->increment_local_shared(); 
    return rpc::shared_ptr<T>(static_cast<internal::control_block_base*>(cb_ptr),
                               static_cast<T*>(cb_ptr->get_managed_object_ptr()));
}

template <typename T> class enable_shared_from_this {
protected:
    mutable weak_ptr<T> weak_this_; 
    constexpr enable_shared_from_this() noexcept = default;
    enable_shared_from_this(const enable_shared_from_this&) noexcept = default;
    enable_shared_from_this& operator=(const enable_shared_from_this&) noexcept = default;
    ~enable_shared_from_this() = default;

    // Helper struct for friend access to shared_ptr internals for enable_shared_from_this
    // This is needed to initialize weak_this_ without triggering remote ref counting.
    struct access_detail_for_est {
        template<typename U> static internal::control_block_base*& cb(rpc::shared_ptr<U>& sp) { return sp.cb_; }
        template<typename U> static U*& ptr(rpc::shared_ptr<U>& sp) { return sp.ptr_; }
        template<typename U> static std::shared_ptr<rpc::object_proxy>& ultimate_proxy(rpc::shared_ptr<U>& sp) { return sp.ultimate_actual_object_proxy_sp_; }
    };


    template <typename ActualPtrType> 
    void internal_set_weak_this(internal::control_block_base* cb_for_this_obj, ActualPtrType* ptr_to_this_obj) const {
        if (static_cast<const void*>(static_cast<const T*>(ptr_to_this_obj)) == static_cast<const void*>(this)) {
            if (weak_this_.expired()) { 
                if (cb_for_this_obj && ptr_to_this_obj) {
                    rpc::shared_ptr<T> temp_sp_for_weak_init;
                    // Manually assign members to bypass acquire_this() logic.
                    access_detail_for_est::cb(temp_sp_for_weak_init) = cb_for_this_obj;
                    access_detail_for_est::ptr(temp_sp_for_weak_init) = const_cast<T*>(static_cast<const T*>(ptr_to_this_obj));
                    // Crucially, ultimate_actual_object_proxy_sp_ should also be set if weak_ptr::lock()
                    // is to reconstruct a fully valid shared_ptr. It should be the same as what a
                    // normal shared_ptr to this object would have.
                    access_detail_for_est::ultimate_proxy(temp_sp_for_weak_init) = get_ultimate_object_proxy_for(const_cast<T*>(static_cast<const T*>(ptr_to_this_obj)), cb_for_this_obj);
                    // NO call to temp_sp_for_weak_init.acquire_this()
                    
                    weak_this_ = temp_sp_for_weak_init; // weak_ptr constructor from shared_ptr handles weak count
                }
            }
        }
    }
public:
    rpc::shared_ptr<T> shared_from_this() { return weak_this_.lock(); }
    rpc::shared_ptr<const T> shared_from_this() const { 
        // weak_ptr<T>::lock() returns shared_ptr<T>.
        // This will implicitly convert to shared_ptr<const T> if T is not already const.
        return weak_this_.lock(); 
    }
    rpc::weak_ptr<T> weak_from_this() const noexcept { return weak_this_; }

    template<typename U, typename ... Args> friend rpc::shared_ptr<U> make_shared(std::shared_ptr<rpc::object_proxy>, Args&&...);
    template<typename U> friend class shared_ptr; // To call internal_set_weak_this
};


template<typename InterfaceType, typename ConcreteLocalProxyType>
rpc::shared_ptr<InterfaceType> create_shared_ptr_to_local_proxy(
    const rpc::shared_ptr<InterfaceType>& actual_object_sp_target, // sp to the actual local object
    std::shared_ptr<rpc::object_proxy> obj_proxy_for_local_proxy_instance // object_proxy for the proxy obj itself (if any)
    ) {
    static_assert(std::is_base_of_v<InterfaceType, ConcreteLocalProxyType>, "Proxy must implement interface.");
    static_assert(std::is_base_of_v<rpc::proxy_base, ConcreteLocalProxyType>, "Proxy must be a proxy_base.");
    
    // ConcreteLocalProxyType constructor needs actual_object_sp_target to init its internal weak_ptr
    // and to ensure it can return the correct ultimate object_proxy via get_object_proxy().
    // It should also initialize its own proxy_base::object_proxy_ member, possibly with
    // obj_proxy_for_local_proxy_instance if that's for the proxy itself.
    ConcreteLocalProxyType* raw_local_proxy = new ConcreteLocalProxyType(actual_object_sp_target); 
    
    // Ensure the proxy's own object_proxy (from proxy_base) is set if provided.
    // This assumes ConcreteLocalProxyType constructor might not set it, or this overrides.
    if (auto* proxy_base_ptr = raw_local_proxy->query_proxy_base()) { // Should be true
        // This directly assigns the member in proxy_base.
        // ConcreteLocalProxyType's ctor should ideally handle setting its own object_proxy_.
        const_cast<rpc::proxy_base*>(proxy_base_ptr)->object_proxy_ = obj_proxy_for_local_proxy_instance;
    }
    
    return rpc::shared_ptr<InterfaceType>(
        static_cast<InterfaceType*>(raw_local_proxy),
        obj_proxy_for_local_proxy_instance, // This is the object_proxy for the proxy object itself.
                                            // The ultimate_actual_object_proxy_sp_ inside the returned shared_ptr
                                            // will be determined by get_ultimate_object_proxy_for, which for a proxy,
                                            // calls raw_local_proxy->get_object_proxy().
        std::default_delete<ConcreteLocalProxyType>()
    );
}

} // namespace rpc

namespace std {
    template<typename T> struct hash<rpc::shared_ptr<T>> { size_t operator()(const rpc::shared_ptr<T>& p) const noexcept { return std::hash<T*>()(p.get()); } };
    template<typename T> struct hash<rpc::optimistic_ptr<T>> { size_t operator()(const rpc::optimistic_ptr<T>& p) const noexcept { return std::hash<rpc::shared_ptr<T>>()(p.get_proxy_shared_ptr()); } };
    template<typename T, typename D> struct hash<rpc::unique_ptr<T, D>> { size_t operator()(const rpc::unique_ptr<T, D>& p) const noexcept { return std::hash<T*>()(p.get()); } };
    template<typename T> struct hash<rpc::weak_ptr<T>> { size_t operator()(const rpc::weak_ptr<T>& p) const noexcept { if (auto sp = p.lock()) { return std::hash<rpc::shared_ptr<T>>()(sp); } return 0; } };
} // namespace std

#endif // RPC_SMART_POINTERS_CORE_HPP
