/*
 *   Copyright (c) 2025 Edward Boggis-Rolfe
 *   All rights reserved.
 */
// this is the legacy rpc implementation kept for win32 for now

#include <rpc/rpc.h>

namespace rpc
{
    // Commented out legacy implementation - not needed for new shared_ptr/weak_ptr
    // bad_weak_ptr::~bad_weak_ptr() noexcept { }
    //
    // const char* bad_weak_ptr::what() const noexcept
    // {
    //     return "bad_weak_ptr";
    // }
    // __shared_count::~__shared_count() { }
    // __shared_weak_count::~__shared_weak_count() { }
    //
    // void __shared_weak_count::__release_weak() noexcept
    // {
    //     // Implementation commented out
    // }
    // __shared_weak_count* __shared_weak_count::lock() noexcept
    // {
    //     // Implementation commented out
    // }
    // const void* __shared_weak_count::__get_deleter(const std::type_info&) const noexcept
    // {
    //     return nullptr;
    // }

}
