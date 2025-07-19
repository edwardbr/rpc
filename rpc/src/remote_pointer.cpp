/*
 *   Copyright (c) 2024 Edward Boggis-Rolfe
 *   All rights reserved.
 */
// this is the legacy rpc implementation kept for win32 for now

#include <rpc/remote_pointer.h>

namespace rpc
{
    bad_weak_ptr::~bad_weak_ptr() noexcept { }

    const char* bad_weak_ptr::what() const noexcept
    {
        return "bad_weak_ptr";
    }
    __shared_count::~__shared_count() { }
    __shared_weak_count::~__shared_weak_count() { }

    void __shared_weak_count::__release_weak() noexcept
    {
        // NOTE: The acquire load here is an optimization of the very
        // common case where a shared pointer is being destructed while
        // having no other contended references.
        //
        // BENEFIT: We avoid expensive atomic stores like XADD and STREX
        // in a common case.  Those instructions are slow and do nasty
        // things to caches.
        //
        // IS THIS SAFE?  Yes.  During weak destruction, if we see that we
        // are the last reference, we know that no-one else is accessing
        // us. If someone were accessing us, then they would be doing so
        // while the last shared / weak_ptr was being destructed, and
        // that's undefined anyway.
        //
        // If we see anything other than a 0, then we have possible
        // contention, and need to use an atomicrmw primitive.
        // The same arguments don't apply for increment, where it is legal
        // (though inadvisable) to share shared_ptr references between
        // threads, and have them all get copied at once.  The argument
        // also doesn't apply for __release_shared, because an outstanding
        // weak_ptr::lock() could read / modify the shared count.
        if (__shared_weak_owners_.load(std::memory_order_acquire) == 0)
        {
            // no need to do this store, because we are about
            // to destroy everything.
            //__libcpp_atomic_store(&__shared_weak_owners_, -1, _AO_Release);
            __on_zero_shared_weak();
        }
        else if (--__shared_weak_owners_ == -1)
            __on_zero_shared_weak();
    }
    __shared_weak_count* __shared_weak_count::lock() noexcept
    {
        long object_owners = __shared_owners_.load(std::memory_order_seq_cst);
        while (object_owners != -1)
        {
            if (__shared_owners_.compare_exchange_weak(object_owners, object_owners + 1))
                return this;
        }
        return nullptr;
    }
    const void* __shared_weak_count::__get_deleter(const std::type_info&) const noexcept
    {
        return nullptr;
    }

}
