#pragma once

#include <memory>

// Forward declaration
namespace rpc {
    template<typename T> class shared_ptr;
}

namespace stdex {
    template<typename T>
    class member_ptr {
private:
        std::shared_ptr<T> ptr_;

    public:
        // Default constructor
        member_ptr() = default;

        // Constructor from shared_ptr
        explicit member_ptr(const std::shared_ptr<T>& ptr) : ptr_(ptr) {}
        explicit member_ptr(std::shared_ptr<T>&& ptr) : ptr_(std::move(ptr)) {}

        // Copy constructor
        member_ptr(const member_ptr& other) : ptr_(other.ptr_) {}

        // Move constructor
        member_ptr(member_ptr&& other) noexcept : ptr_(std::move(other.ptr_)) {}

        // Copy assignment operator
        member_ptr& operator=(const member_ptr& other) {
            if (this != &other) {
                ptr_ = other.ptr_;
            }
            return *this;
        }

        // Move assignment operator
        member_ptr& operator=(member_ptr&& other) noexcept {
            if (this != &other) {
                ptr_ = std::move(other.ptr_);
            }
            return *this;
        }

        // Assignment from shared_ptr
        member_ptr& operator=(const std::shared_ptr<T>& ptr) {
            ptr_ = ptr;
            return *this;
        }

        member_ptr& operator=(std::shared_ptr<T>&& ptr) {
            ptr_ = std::move(ptr);
            return *this;
        }

        // Get nullable copy
        std::shared_ptr<T> get_nullable() const {
            return ptr_;
        }

        // Reset to null
        void reset() {
            ptr_.reset();
        }

        // Destructor
        ~member_ptr() = default;
    };
}

namespace rpc {
    template<typename T>
    class member_ptr {
    private:
        rpc::shared_ptr<T> ptr_;

    public:
        // Default constructor
        member_ptr() = default;

        // Constructor from shared_ptr
        explicit member_ptr(const rpc::shared_ptr<T>& ptr) : ptr_(ptr) {}
        explicit member_ptr(rpc::shared_ptr<T>&& ptr) : ptr_(std::move(ptr)) {}

        // Copy constructor
        member_ptr(const member_ptr& other) : ptr_(other.ptr_) {}

        // Move constructor
        member_ptr(member_ptr&& other) noexcept : ptr_(std::move(other.ptr_)) {}

        // Copy assignment operator
        member_ptr& operator=(const member_ptr& other) {
            if (this != &other) {
                ptr_ = other.ptr_;
            }
            return *this;
        }

        // Move assignment operator
        member_ptr& operator=(member_ptr&& other) noexcept {
            if (this != &other) {
                ptr_ = std::move(other.ptr_);
            }
            return *this;
        }

        // Assignment from shared_ptr
        member_ptr& operator=(const rpc::shared_ptr<T>& ptr) {
            ptr_ = ptr;
            return *this;
        }

        member_ptr& operator=(rpc::shared_ptr<T>&& ptr) {
            ptr_ = std::move(ptr);
            return *this;
        }

        // Get nullable copy
        rpc::shared_ptr<T> get_nullable() const {
            return ptr_;
        }

        // Reset to null
        void reset() {
            ptr_.reset();
        }

        // Destructor
        ~member_ptr() = default;
    };
}