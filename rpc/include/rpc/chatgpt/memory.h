#pragma once

#include <atomic>
#include <cstddef>
#include <utility>
#include <memory>
#include <type_traits>
#include <mutex>
#include <functional>

namespace rpc {

// Forward declarations
template <typename T> class shared_ptr;
template <typename T> class weak_ptr;
template <typename T> class unique_ptr;

// Control block for reference counting
class control_block {
public:
    std::atomic<size_t> shared_count{1};
    std::atomic<size_t> weak_count{0};

    virtual ~control_block() = default;
    virtual void destroy() noexcept = 0;
};

// Control block for regular objects
template <typename T>
class control_block_ptr : public control_block {
public:
    T* ptr;
    explicit control_block_ptr(T* p) : ptr(p) {}
    void destroy() noexcept override { delete ptr; }
};

// Shared Pointer Implementation
template <typename T>
class shared_ptr {
private:
    T* ptr = nullptr;
    control_block* ctrl = nullptr;

    void release() {
        if (ctrl) {
            if (--ctrl->shared_count == 0) {
                ctrl->destroy();
                if (ctrl->weak_count == 0) {
                    delete ctrl;
                }
            }
        }
    }

public:
    constexpr shared_ptr() noexcept = default;
    explicit shared_ptr(T* p) : ptr(p), ctrl(new control_block_ptr<T>(p)) {}

    shared_ptr(const shared_ptr& other) noexcept : ptr(other.ptr), ctrl(other.ctrl) {
        if (ctrl) {
            ++ctrl->shared_count;
        }
    }

    shared_ptr(shared_ptr&& other) noexcept : ptr(other.ptr), ctrl(other.ctrl) {
        other.ptr = nullptr;
        other.ctrl = nullptr;
    }

    ~shared_ptr() { release(); }

    shared_ptr& operator=(const shared_ptr& other) noexcept {
        if (this != &other) {
            release();
            ptr = other.ptr;
            ctrl = other.ctrl;
            if (ctrl) {
                ++ctrl->shared_count;
            }
        }
        return *this;
    }

    shared_ptr& operator=(shared_ptr&& other) noexcept {
        if (this != &other) {
            release();
            ptr = other.ptr;
            ctrl = other.ctrl;
            other.ptr = nullptr;
            other.ctrl = nullptr;
        }
        return *this;
    }

    T* get() const noexcept { return ptr; }
    T& operator*() const { return *ptr; }
    T* operator->() const noexcept { return ptr; }
    explicit operator bool() const noexcept { return ptr != nullptr; }

    bool operator==(const shared_ptr& other) const noexcept { return ptr == other.ptr; }
    bool operator!=(const shared_ptr& other) const noexcept { return ptr != other.ptr; }
};

// Factory function for shared_ptr
template <typename T, typename... Args>
shared_ptr<T> make_shared(Args&&... args) {
    return shared_ptr<T>(new T(std::forward<Args>(args)...));
}

// Weak Pointer Implementation
template <typename T>
class weak_ptr {
private:
    T* ptr = nullptr;
    control_block* ctrl = nullptr;

public:
    constexpr weak_ptr() noexcept = default;
    weak_ptr(const shared_ptr<T>& sp) noexcept : ptr(sp.ptr), ctrl(sp.ctrl) {
        if (ctrl) {
            ++ctrl->weak_count;
        }
    }
    ~weak_ptr() {
        if (ctrl && --ctrl->weak_count == 0 && ctrl->shared_count == 0) {
            delete ctrl;
        }
    }
    shared_ptr<T> lock() const noexcept {
        return (ctrl && ctrl->shared_count > 0) ? shared_ptr<T>(*this) : shared_ptr<T>();
    }
};

// Unique Pointer Implementation
template <typename T>
class unique_ptr {
private:
    T* ptr = nullptr;

public:
    constexpr unique_ptr() noexcept = default;
    explicit unique_ptr(T* p) noexcept : ptr(p) {}
    unique_ptr(unique_ptr&& other) noexcept : ptr(other.ptr) { other.ptr = nullptr; }
    ~unique_ptr() { delete ptr; }
    unique_ptr& operator=(unique_ptr&& other) noexcept {
        if (this != &other) {
            delete ptr;
            ptr = other.ptr;
            other.ptr = nullptr;
        }
        return *this;
    }
    T* get() const noexcept { return ptr; }
    T& operator*() const { return *ptr; }
    T* operator->() const noexcept { return ptr; }
    explicit operator bool() const noexcept { return ptr != nullptr; }

    bool operator==(const unique_ptr& other) const noexcept { return ptr == other.ptr; }
    bool operator!=(const unique_ptr& other) const noexcept { return ptr != other.ptr; }
};

// Factory function for unique_ptr
template <typename T, typename... Args>
unique_ptr<T> make_unique(Args&&... args) {
    return unique_ptr<T>(new T(std::forward<Args>(args)...));
}

// Pointer casts
template <typename T, typename U>
shared_ptr<T> static_pointer_cast(const shared_ptr<U>& sp) {
    return shared_ptr<T>(static_cast<T*>(sp.get()));
}

template <typename T, typename U>
shared_ptr<T> dynamic_pointer_cast(const shared_ptr<U>& sp) {
    T* casted_ptr = dynamic_cast<T*>(sp.get());
    return casted_ptr ? shared_ptr<T>(casted_ptr) : shared_ptr<T>();
}

// Hash function specialization
template <typename T>
struct hash<shared_ptr<T>> {
    size_t operator()(const shared_ptr<T>& sp) const noexcept {
        return std::hash<T*>{}(sp.get());
    }
};

template <typename T>
struct hash<unique_ptr<T>> {
    size_t operator()(const unique_ptr<T>& up) const noexcept {
        return std::hash<T*>{}(up.get());
    }
};

} // namespace rpc
