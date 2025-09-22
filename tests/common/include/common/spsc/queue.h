/*
 *   Copyright (c) 2025 Edward Boggis-Rolfe
 *   All rights reserved.
 */

#pragma once

#include <atomic>

namespace spsc
{
    template<typename T, std::size_t Size> class queue
    {
    public:
        queue() noexcept
            : head_(0)
            , tail_(0)
        {
        }

        bool push(const T& value) noexcept
        {
            std::size_t head = head_.load(std::memory_order_relaxed);
            std::size_t next_head = next(head);
            if (next_head == tail_.load(std::memory_order_acquire))
                return false;
            ring_[head] = value;
            head_.store(next_head, std::memory_order_release);
            return true;
        }

        bool pop(T& value) noexcept
        {
            std::size_t tail = tail_.load(std::memory_order_relaxed);
            if (tail == head_.load(std::memory_order_acquire))
                return false;
            // make sure we don't keep a copy of the objects in the ring, which may take up memory
            // (for instance if the objects have big buffers), and more importantly may contain
            // shared_ptr that could prevent timely deletion.
            value = std::move(ring_[tail]);
            tail_.store(next(tail), std::memory_order_release);
            return true;
        }

    private:
        std::size_t next(std::size_t current) const noexcept { return (current + 1) % (Size + 1); }
        T ring_[Size + 1];
        std::atomic<std::size_t> head_, tail_;
    };
}
