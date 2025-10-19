# SPSC Channel Manager Lifecycle Design

**Version**: 2.0
**Date**: 2025-01-19
**Status**: Corrected to align with Problem Statement and Q&A

## Overview
The SPSC channel manager coordinates bidirectional communication between two zones using lock-free single-producer-single-consumer queues. It manages multiple service proxies that share the same communication channel.

**CRITICAL CLARIFICATION**: The channel manager is an **SPSC-specific implementation detail**, NOT a core RPC++ abstraction. Other transports (TCP, Local, SGX) may have different internal implementations.

## Architecture Components

### Message Blob Configuration
- **Message blob size**: `std::array<uint8_t, 10024>` (10KB chunks)
- **Queue size**: 10024 entries
- Messages larger than blob size are automatically chunked across multiple blobs

### Two Independent Tasks
1. **`receive_consumer_task`** - Continuously reads from `receive_spsc_queue_` and processes incoming messages
2. **`send_producer_task`** - Continuously reads from `send_queue_` and writes to `send_spsc_queue_`

These tasks run independently and communicate through shared atomic flags.

### Message Handler Architecture
The incoming message handler has a critical design that enables reentrant calls:

```cpp
std::function<void(envelope_prefix, envelope_payload)> incoming_message_handler
```

**Key Design Points**:
- **Non-coroutine signature**: Returns `void`, not `CORO_TASK(int)` - this is intentional
- **Dual processing strategy**:
  - **Stub calls (async)**: Scheduled independently via `get_scheduler()->schedule()` - allows reentrant calls
  - **Response messages (sync)**: Processed inline to immediately wake up waiting callers
- **No reference cycle risk**: Handler does NOT capture `keep_alive_` - channel is kept alive by member variable

**IMPORTANT**: The message handler calls **i_marshaller methods** on the registered handler (service or pass_through). The handler is NOT aware of whether it's calling service or pass_through - it's polymorphic via i_marshaller interface.

### Two Independent Reference Counts

**WARNING FROM Q&A**: The `service_proxy_ref_count_` reference counting mechanism is **subject to change** in future designs. Do not rely on this pattern being permanent.

#### 1. `service_proxy_ref_count_` (Service Proxy Lifecycle) - SUBJECT TO CHANGE

**From Q&A Q2**:
> `channel_manager.service_proxy_ref_count_` tracks how many service_proxies share this channel
> - **Note**: This reference counting mechanism is **subject to change** in future designs

Tracks how many service_proxies are actively using this channel.

- **Incremented:** When a service_proxy is assigned a channel_manager
  - In `service_proxy::connect()` after creating the channel
  - In `service_proxy::attach_remote()` after receiving the channel
- **Decremented:** When `~service_proxy()` destructor runs
- **When reaches 0:** Triggers `shutdown()` to initiate graceful connection close

**Purpose:** Determines when no more service proxies need the channel and shutdown should begin.

**FUTURE DESIGN NOTE**: Pass-through architecture may eliminate this reference counting pattern. Pass-through will control lifecycle of service_proxies instead.

#### 2. `tasks_completed_` (Task Lifecycle)
Tracks when both pump tasks have finished execution.

- **Incremented:** When either task completes (at end of `send_producer_task` or `receive_consumer_task`)
- **When reaches 2:** Both tasks have completed
  - Releases `keep_alive_` (allows channel_manager destruction)
  - Sets `shutdown_event_` (wakes up waiters in `shutdown()`)

**Purpose:** Ensures `keep_alive_` is only released after both tasks finish, preventing crashes from accessing member variables.

## Lifecycle Flow

### 1. Channel Creation
```
channel_manager::create()
├─ service_proxy_ref_count_ = 0
├─ tasks_completed_ = 0
├─ keep_alive_ = shared_ptr to self
└─ Returns shared_ptr<channel_manager>
```

### 2. Service Proxy Attachment
```
service_proxy construction
├─ Assigns channel_manager_
├─ Calls channel_manager_->attach_service_proxy()
│   └─ service_proxy_ref_count_++  (e.g., 0→1)
└─ Channel now actively used
```

**CRITICAL FROM Q&A Q2**: Multiple service_proxies can attach to the same channel (ref count can be 2, 3, etc.). This is the **transport sharing** pattern - multiple service_proxies share ONE transport/channel.

**From Q&A Q2**:
> **Transport Sharing:**
> - **Multiple service proxies can share ONE transport**
> - Example: Zone A might have service_proxy(A→B, caller=A) and service_proxy(A→B, caller=C) both using same TCP socket
> - Sink on receiving side disambiguates which (destination, caller) pair the message is for

### 3. Pump Tasks Running
```
pump_send_and_receive()
├─ Schedules receive_consumer_task()
│   └─ Runs until: peer_cancel_received_ OR cancel_confirmed_
└─ Schedules send_producer_task()
    └─ Runs until: (close_ack_queued_ OR cancel_confirmed_) AND queues empty
```

Tasks run independently, yielding to scheduler when idle.

### 4. Service Proxy Destruction
```
~service_proxy()
├─ Schedules detach_service_proxy()
│   ├─ service_proxy_ref_count_--  (e.g., 2→1)
│   └─ If ref_count == 0 AND !peer_cancel_received_:
│       └─ Calls shutdown()
│   └─ If ref_count == 0 AND peer_cancel_received_:
│       └─ No action (peer already initiated shutdown)
└─ Returns immediately (async detach)
```

**Important**: `detach_service_proxy()` only initiates shutdown if the peer hasn't already done so. This prevents double shutdown attempts when both sides disconnect simultaneously.

**FUTURE CONSIDERATION**: With pass-through architecture, this lifecycle may change. Pass-through will manage service_proxy lifetime instead of reference counting to zero.

### 5. Shutdown Process
```
shutdown()
├─ If already shutting down: wait on shutdown_event_ and return
├─ Set cancel_sent_ = true
├─ Send close_connection_send to peer
├─ Await close_connection_received response
├─ Set cancel_confirmed_ = true
├─ Set shutdown_event_
└─ Await shutdown_event_ (wait for tasks to complete)
```

**CRITICAL ADDITION FROM Q&A Q9**: Shutdown must also handle **zone_terminating** notification:

```cpp
shutdown()
├─ If graceful shutdown:
│   ├─ Send close_connection_send to peer (current behavior)
│   └─ Await acknowledgment
├─ If zone_terminating event received:
│   ├─ Transport detects zone termination
│   ├─ Broadcast zone_terminating to:
│   │   ├─ Service (for local cleanup)
│   │   └─ Pass-through (if exists, for routing cleanup)
│   └─ Skip waiting for peer acknowledgment (peer is dead)
└─ Continue with shutdown sequence
```

**From Q&A Q9**:
> **Transport responsibility**:
> - Transport must pass Zone Termination notification to service AND to any relevant pass-throughs
> - A service may have **multiple transports** (e.g., several SGX enclaves, several TCP connections)

### 6. Peer Receives Close Request
```
receive_consumer_task receives close_connection_send
├─ Schedules response task:
│   ├─ Send close_connection_received acknowledgment
│   ├─ Set close_ack_queued_ = true
│   ├─ Set peer_cancel_received_ = true
│   └─ IMPORTANT: Does NOT capture keep_alive_ (prevents reference cycle)
└─ Continue receiving until peer_cancel_received_ OR cancel_confirmed_
```

**Reference Cycle Prevention**: The response task deliberately does NOT capture `keep_alive_`. The channel_manager is kept alive by its member `keep_alive_` variable until both tasks complete. Capturing it in the lambda would create a reference cycle.

**ADDITION FOR ZONE TERMINATION**:
```
receive_consumer_task receives zone_terminating message
├─ Schedules zone termination handler:
│   ├─ Mark zone as terminated
│   ├─ Notify service via i_marshaller::post(post_options::zone_terminating)
│   ├─ Notify pass-through (if exists) via same method
│   ├─ Set peer_cancel_received_ = true (zone is dead)
│   └─ Clean up all service_proxies to that zone
└─ Continue receiving until peer_cancel_received_ OR cancel_confirmed_
```

**From Problem Statement (Lines 666-683)**:
> **Transport Responsibility for Zone Termination**:
> - **Transport must pass Zone Termination notification** to:
>   - Service (for local cleanup)
>   - Any relevant pass-throughs (for routing cleanup)

### 7. Tasks Complete
```
send_producer_task exits
├─ Increments tasks_completed_ (1)
└─ If tasks_completed_ == 2:
    ├─ keep_alive_ = nullptr
    └─ shutdown_event_.set()

receive_consumer_task exits
├─ Increments tasks_completed_ (2)
└─ If tasks_completed_ == 2:
    ├─ keep_alive_ = nullptr
    └─ shutdown_event_.set()
```

### 8. Channel Manager Destruction
```
After both tasks complete:
├─ keep_alive_ released
├─ shared_ptr ref count drops to 0
└─ ~channel_manager() destructor runs
```

## Producer/Consumer Pattern with SPSC Queues

The two tasks operate independently like io_uring's shared buffer model:

### Send Producer Task (Local Writer)
```cpp
CORO_TASK(void) send_producer_task()
{
    while ((...) || !send_queue_.empty() || !send_data.empty())
    {
        // Get data from internal queue
        // Chunk into message_blob size
        if (send_spsc_queue_->push(send_blob))
        {
            // Success - remote receiver will discover independently
        }
        else
        {
            // Queue full - yield and retry
            CO_AWAIT service_->get_scheduler()->schedule();
        }
    }
}
```

**Behavior**:
- Writes to `send_spsc_queue_` (local producer)
- Remote zone's `receive_consumer_task` polls this queue (remote consumer)
- No coordination needed - lock-free single-producer-single-consumer semantics

### Receive Consumer Task (Local Reader)
```cpp
CORO_TASK(void) receive_consumer_task(...)
{
    while (!peer_cancel_received_ && !cancel_confirmed_)
    {
        // Poll SPSC queue for data
        bool received_any = false;
        while (!receive_data.empty())
        {
            if (!receive_spsc_queue_->pop(blob))
            {
                if (!received_any)
                {
                    // No progress, yield to scheduler
                    CO_AWAIT service_->get_scheduler()->schedule();
                }
                break;
            }
            received_any = true;
            // Copy data from blob
        }

        if (receive_data.empty())
        {
            // Complete message received, process it
            incoming_message_handler(prefix, payload);
        }
    }
}
```

**Behavior**:
- Polls `receive_spsc_queue_` (local consumer)
- Remote zone's `send_producer_task` writes to this queue (remote producer)
- Yields to scheduler when no data available
- Processes complete messages via handler

**CRITICAL CLARIFICATION**: The `incoming_message_handler` unpacks the message and calls the registered `i_marshaller` handler (service or pass_through). The handler signature is:

```cpp
// Channel manager calls this after unpacking envelope
void incoming_message_handler(envelope_prefix prefix, envelope_payload payload)
{
    // Unpack prefix to get: protocol_version, encoding, tag, zones, object, interface, method, etc.

    // Call appropriate i_marshaller method on registered handler
    if (prefix.message_type == message_type::send)
    {
        handler_->send(protocol_version, encoding, tag,
                      caller_channel_zone, caller_zone, destination_zone,
                      object, interface, method,
                      in_buf, out_buf,
                      in_back_channel, out_back_channel);
    }
    else if (prefix.message_type == message_type::post)
    {
        handler_->post(protocol_version, encoding, tag,
                      caller_channel_zone, caller_zone, destination_zone,
                      object, interface, method, post_options,
                      in_buf, in_back_channel);
    }
    // ... other message types
}
```

### Lock-Free Communication
Each zone has two SPSC queues:
- **send_spsc_queue_**: This zone writes, remote zone reads
- **receive_spsc_queue_**: Remote zone writes, this zone reads

No locks needed because:
- Each queue has exactly one producer and one consumer
- Producer and consumer are in different zones (no shared state)
- Atomic operations in SPSC queue ensure visibility

## Exit Conditions

### `send_producer_task` exits when:
```cpp
while ((!close_ack_queued_ && !cancel_confirmed_) || !send_queue_.empty() || !send_data.empty())
```
Exits when:
- (**close_ack_queued_** OR **cancel_confirmed_**) is true, AND
- send_queue is empty, AND
- send_data is empty

This ensures:
- Acknowledgment for peer's close request is sent
- OR our own shutdown is confirmed
- AND all queued data is flushed

### `receive_consumer_task` exits when:
```cpp
while (!peer_cancel_received_ && !cancel_confirmed_)
```
Exits when:
- **peer_cancel_received_** is true, OR
- **cancel_confirmed_** is true

This ensures we stop receiving when either side initiates shutdown.

## Call Cancellation During Shutdown

When `peer_cancel_received_` is set, all new RPC calls immediately return with `CALL_CANCELLED` error:

```cpp
CORO_TASK(int) call_peer(...)
{
    // Early exit check - avoid queuing calls during shutdown
    if (peer_cancel_received_)
    {
        CO_RETURN rpc::error::CALL_CANCELLED();
    }
    // ... rest of call logic
}
```

This prevents new calls from being queued when the peer has already initiated shutdown, improving shutdown responsiveness.

## Race Condition Handling

### Multiple Simultaneous `shutdown()` Calls
Protected by `cancel_sent_` flag:
```cpp
if (cancel_sent_) {
    // Already shutting down, just wait
    CO_AWAIT shutdown_event_;
    CO_RETURN;
}
```

### Both Sides Call `shutdown()` Simultaneously
1. Zone 1 sends close_connection_send, waits for ack
2. Zone 2 sends close_connection_send, waits for ack
3. Both receive each other's requests
4. Both send acknowledgments (close_ack_queued_ = true)
5. Both receive acknowledgments (cancel_confirmed_ = true)
6. Both tasks exit when conditions are met

### Task Completion Race
Protected by atomic `tasks_completed_`:
- First task increments to 1, continues
- Second task increments to 2, releases keep_alive_
- Only the second task (whoever finishes last) performs cleanup

## Key Invariants

1. **`keep_alive_` is valid while tasks are running**
   - Released only after `tasks_completed_ == 2`
   - Prevents crashes from accessing member variables

2. **`service_proxy_ref_count_` independent of `tasks_completed_`**
   - Service proxies can be destroyed before tasks finish
   - Tasks continue running until proper shutdown sequence completes
   - **WARNING**: This mechanism is **subject to change** (Q&A Q2)

3. **`shutdown()` is idempotent**
   - Can be called multiple times safely
   - First call initiates shutdown, subsequent calls wait

4. **Acknowledgment is flushed before exit**
   - `close_ack_queued_` ensures send_producer_task continues
   - Until acknowledgment is actually sent to peer

5. **No data loss during shutdown**
   - send_producer_task continues until all queues are empty
   - Ensures in-flight messages are delivered

6. **Transport notifies service AND pass-through on zone termination** (NEW)
   - Required for proper cleanup of routing infrastructure
   - Service cleans up local stubs and proxies
   - Pass-through cleans up routing state

7. **Multiple service_proxies can share one channel_manager** (CLARIFIED)
   - This is the transport sharing pattern from Q&A Q2
   - Reference count can be > 2 (not just bidirectional pair)
   - Channel disambiguates messages by (destination, caller) zones

## Member Variables Summary

### Reference Counting
- `std::atomic<int> service_proxy_ref_count_{0}` - Number of active service proxies (SUBJECT TO CHANGE)
- `std::atomic<int> tasks_completed_{0}` - Number of completed pump tasks (0-2)

### Shutdown Coordination
- `bool cancel_sent_{false}` - We initiated shutdown (sent close_connection_send)
- `bool cancel_confirmed_{false}` - We received close_connection_received ack
- `std::atomic<bool> peer_cancel_received_{false}` - Peer initiated shutdown OR zone terminated
- `std::atomic<bool> close_ack_queued_{false}` - Acknowledgment queued for sending

### Lifecycle Management
- `std::shared_ptr<channel_manager> keep_alive_` - Keeps channel alive until tasks complete
- `coro::event shutdown_event_` - Signals when shutdown sequence is complete (manual-reset)

### Communication
- `queue_type* send_spsc_queue_` - Lock-free queue for sending to peer
- `queue_type* receive_spsc_queue_` - Lock-free queue for receiving from peer
- `std::queue<std::vector<uint8_t>> send_queue_` - Internal queue for messages to send
- `coro::mutex send_queue_mtx_` - Protects send_queue_ access

### Handler Registration (NEW ADDITION)
- `std::weak_ptr<i_marshaller> handler_` - Service or pass_through that handles incoming messages
  - Polymorphic handler via i_marshaller interface
  - Transport does NOT know if it's service or pass_through
  - Transport calls handler methods after unpacking envelopes

## Comparison with Old `pump_messages`

### Old Design (Single Coroutine)
- Combined send and receive in one coroutine
- Loop condition: `while (peer_cancel_received_ == false || cancel_confirmed_ == false || send_queue_.empty() == false || send_data.empty() == false)`
- Exited when ALL flags were true AND queues were empty
- Synchronous handling of all messages
- Message handler returned `CORO_TASK(int)` and was awaited inline

### New Design (Split Tasks)
- Separate send and receive coroutines running independently
- Each has its own exit condition based on its responsibilities
- Asynchronous message handling (stub calls scheduled independently)
- Synchronous response handling (inline processing to wake callers)
- Message handler returns `void` and schedules stub calls asynchronously
- More complex coordination but better concurrency
- Enables reentrant RPC calls within stub handlers

## Testing Considerations

### Empty Test (initialisation_test)
1. Setup creates channels and service proxies
2. Test body is empty (no operations)
3. Teardown:
   - Schedules CoroTearDown (clears interface pointers)
   - Service destructors run
   - ~service_proxy() calls detach_service_proxy()
   - When last service_proxy detaches, shutdown() is called
   - Shutdown sequence completes
   - Both tasks exit
   - keep_alive_ released
   - Scheduler becomes empty
   - Test completes

### Actual Teardown Implementation
```cpp
bool shutdown_complete = false;
auto shutdown_task = [&]() -> coro::task<void> {
    CO_AWAIT CoroTearDown();
    // Give time for service_proxy destructors to schedule detach coroutines
    CO_AWAIT io_scheduler_->schedule();
    CO_AWAIT io_scheduler_->schedule();
    shutdown_complete = true;
    CO_RETURN;
};

io_scheduler_->schedule(shutdown_task());

// Process events until shutdown completes
while (!shutdown_complete)
{
    io_scheduler_->process_events(std::chrono::milliseconds(1));
}

// Continue processing to allow shutdown to finish and pump tasks to exit
for (int i = 0; i < 100; ++i)
{
    io_scheduler_->process_events(std::chrono::milliseconds(1));
}
```

**Key Implementation Details**:
- Uses a completion flag (`shutdown_complete`) to track CoroTearDown completion
- Allows 2 scheduler cycles after teardown for destructors to schedule detach coroutines
- Continues processing events for 100ms to allow shutdown sequence and pump task exit
- Does NOT wait for empty scheduler as pump tasks keep it busy until shutdown triggers

## Synchronous Mode Considerations (NEW SECTION)

**From Q&A Q13**:
> **Synchronous Mode (BUILD_COROUTINE=OFF):**
> - All i_marshaller methods become BLOCKING
> - Fire-and-forget `post()` becomes **blocking call**
> - This is why async is valuable - posts are not scalable in synchronous mode

**SPSC in Synchronous Mode**: SPSC transport is **COROUTINE-ONLY** and cannot be used in synchronous builds.

**From Q&A Q13**:
> | Transport | Sync Mode | Async Mode |
> |-----------|-----------|------------|
> | SPSC | ❌ No | ✅ Yes |

**Reason**: SPSC requires async pump tasks to poll queues. Without coroutines, no mechanism to pump messages.

**Impact**: This document only applies to `BUILD_COROUTINE=ON` builds. SPSC is unavailable in synchronous mode.

## Future Architecture Changes (NEW SECTION)

### Pass-Through Integration

**From Q&A Q3**:
> **Pass-Through Reference Counting Responsibility:**
> The **pass-through should be responsible** for reference counting. This design has important implications:
> 1. **Pass-through controls lifetime of its service_proxies**
> 2. **Pass-through maintains single reference count to service**
> 3. **service_proxy may not need lifetime_lock_**

**Potential Changes to SPSC Channel Manager**:
1. **service_proxy_ref_count_ may be eliminated**
   - Pass-through will control service_proxy lifetime instead
   - Channel lifetime tied to pass-through, not individual proxies

2. **Handler registration will use pass_through**
   - Instead of service as handler, pass_through becomes handler
   - Pass-through implements i_marshaller for routing

3. **Both-or-Neither Guarantee Required**
   - **From Q&A Q3**: Pass-through guarantees BOTH or NEITHER service_proxies operational
   - Channel manager must refuse operations if transport not operational
   - Prevents asymmetric states

**From Q&A Q3**:
> **Transport Viability Check**:
> - **If transport is NOT operational**, `clone_for_zone()` function **must refuse** to create new service_proxies for that transport

**SPSC Implementation Impact**: SPSC channel_manager will need to expose `is_operational()` method that checks:
- `!peer_cancel_received_` (peer hasn't terminated)
- `!cancel_sent_` (we haven't initiated shutdown)
- Tasks are still running

---

**End of Document**
