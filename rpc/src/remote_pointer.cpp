/*
 *   Copyright (c) 2024 Edward Boggis-Rolfe
 *   All rights reserved.
 */
// this is the legacy rpc implementation kept for win32 for now

#include <rpc/rpc.h>

namespace rpc
{
    namespace internal
    {
        void control_block_base::decrement_local_shared_and_notify_proxy()
        {
            if (local_shared_owners.fetch_sub(1, std::memory_order_acq_rel) == 1)
            {
                if (this_cb_object_proxy_sp_)
                {
                    this_cb_object_proxy_sp_->signal_local_strong_references_gone();
                }
                else
                {
                    dispose_object_actual();
                }
                decrement_local_weak_and_notify_proxy();
            }
        }

        void control_block_base::decrement_local_weak_and_notify_proxy()
        {
            if (local_weak_owners.fetch_sub(1, std::memory_order_acq_rel) == 1)
            {
                if (local_shared_owners.load(std::memory_order_acquire) == 0)
                {
                    if (this_cb_object_proxy_sp_)
                    {
                        this_cb_object_proxy_sp_->signal_all_local_references_gone();
                    }
                    else
                    {
                        destroy_self_actual();
                    }
                }
            }
        }
    }
}