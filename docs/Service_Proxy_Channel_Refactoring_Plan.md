<!--
Copyright (c) 2025 Edward Boggis-Rolfe
All rights reserved.
-->

# Service Proxy, Service, and Channel Refactoring Plan

**Version**: 1.0
**Date**: 2025-01-17
**Status**: Planning Phase

---

## Executive Summary

This document outlines a comprehensive refactoring plan to separate transport concerns (channels) from routing concerns (service proxies) in the RPC++ framework. The current design has service proxies tightly coupled to transport mechanisms, leading to complexity, race conditions, and the "Y-topology bug" documented in `service.cpp:239`.

**Key Goals:**
1. **Extract Channel Concept**: Create a generic `channel` abstraction that encapsulates bidirectional communication between zones
2. **Symmetrical Service Proxies**: Implement bidirectional service proxy pairs that exist as long as a channel is active, independent of reference counts
3. **Eliminate On-Demand Proxy Creation**: Remove the race-prone pattern of creating service proxies on-the-fly during add_ref operations
4. **Simplify Service Proxy Lifecycle**: Service proxies become lightweight routing objects that delegate to channels for actual transport

---

## Current Architecture Analysis

### Current Problems

#### 1. The Y-Topology Race Condition (service.cpp:239-250)

**Problem Description:**
```
Zone Topology:
    Root (Zone 1)
     ├─ Prong A (Zone 2)
     └─ Prong B (Zone 3) ← created by Prong A

Flow:
1. Prong A creates Prong B
2. Prong B creates object and passes back to Prong A
3. Prong A returns object to Root
4. Root is unaware of Prong B's existence
5. Root attempts add_ref on object in Zone 3
6. Root → Zone 1 has no service proxy to Zone 3
7. Race condition: service proxy must be created on-the-fly
```

**Current Workaround:**
```cpp
// NOTE: Race condition addressed in service.cpp:239
// Force creation of reverse-direction service proxy during send()
if (auto found = other_zones.find({caller_zone_id.as_destination(), destination_zone_id.as_caller()});
    found != other_zones.end())
{
    opposite_direction_proxy = found->second.lock();
    if (!opposite_direction_proxy)
    {
        // Service proxy was destroyed - create on-the-fly
        opposite_direction_proxy = temp->clone_for_zone(caller_zone_id.as_destination(), destination_zone_id.as_caller());
        inner_add_zone_proxy(opposite_direction_proxy);
    }
}
```

**Why This is Problematic:**
- Creates service proxies reactively during critical path operations
- Race window between proxy creation and external_ref increment
- Null pointer checks scatter across codebase
- Violates single responsibility principle (send() shouldn't manage proxy lifecycle)

#### 2. Service Proxy Has Multiple Responsibilities

**Current Responsibilities:**
1. **Routing Logic**: Determining which zone to forward calls to
2. **Transport Logic**: Actual serialization and network/IPC communication
3. **Lifecycle Management**: Reference counting and cleanup
4. **Protocol Negotiation**: Version and encoding fallback

**Problems:**
- Hard to test each concern in isolation
- Changes to transport affect routing logic
- Adding new transports requires duplicating routing logic

#### 3. Asymmetric Service Proxy Creation

**Current Pattern:**
```cpp
// Zone A → Zone B: Explicit service proxy
auto sp_a_to_b = service->connect_to_zone<...>(...);

// Zone B → Zone A: Created on-demand only when needed
// No explicit creation - happens reactively during add_ref/send
```

**Problems:**
- Service proxies in reverse direction don't exist until first use
- Lifetime is unpredictable (destroyed when ref count hits zero)
- Can't reason about bidirectional connectivity
- Race conditions when both directions try to create simultaneously

#### 4. Channel Concept is Implicit

**Current State:**
- SPSC transport has explicit `channel_manager` (good!)
- TCP transport has implicit channel buried in service proxy
- Local/SGX transports have no channel concept at all
- Cannot share a transport connection across multiple service proxy pairs

**Example Problem:**
```cpp
// Want: Zone A ↔ Zone B channel shared by multiple object streams
// Reality: Each service proxy pair potentially creates its own connection

// Zone A → Zone B for object 1
auto sp1 = connect_to_zone<spsc::service_proxy>(...);  // Creates channel

// Zone A → Zone B for object 2
auto sp2 = connect_to_zone<spsc::service_proxy>(...);  // Should reuse channel, but doesn't
```

#### 5. Destination Channel vs Destination Zone Confusion

**Current Types:**
- `destination_zone`: Ultimate destination of the call (e.g., E in A→B→C→D→E)
- `destination_channel_zone`: Immediate adjacent zone toward destination (e.g., D when C is processing)
- `caller_zone`: Original caller (e.g., A in A→B→C→D→E)
- `caller_channel_zone`: Immediate adjacent zone toward caller (e.g., B when C is processing)

**The Problem:**
In multi-hop routing (A→B→C→D→E), zone C needs to know:
1. Final destination (E) - to know where the call is ultimately going
2. Next hop toward destination (D) - to select which transport to use
3. Original caller (A) - for reference counting and response routing
4. Previous hop from caller (B) - to know which transport the call arrived on

These four distinct concepts must be tracked separately, but they all use zone IDs, making the code confusing.

---

## Proposed Architecture

### Core Concept: Separation of Concerns

```
┌─────────────────────────────────────────────────────────────┐
│                         Service                              │
│  - Manages object stubs                                      │
│  - Registers service proxies                                 │
│  - Routes calls to correct service proxy                     │
└─────────────────────────────────────────────────────────────┘
                           │
                           │ owns multiple
                           ↓
┌─────────────────────────────────────────────────────────────┐
│                    Service Proxy Pair                        │
│  - Lightweight routing objects                               │
│  - Bidirectional (always created in pairs)                   │
│  - Each holds shared_ptr to transport                        │
│  - Lifetime = object ref counts (shared + optimistic)        │
└─────────────────────────────────────────────────────────────┘
                           │
                           │ holds shared_ptr to
                           ↓
┌─────────────────────────────────────────────────────────────┐
│                        Transport                             │
│  - Encapsulates bidirectional communication                  │
│  - Manages physical connection (SPSC queue, TCP socket, etc) │
│  - Shared by service proxy pair                              │
│  - Destroyed when both proxies have zero ref counts          │
└─────────────────────────────────────────────────────────────┘
```

### Entity Relationship Design

```
┌──────────────────────────────────────────────────────────────┐
│                     SERVICE (Zone)                            │
├──────────────────────────────────────────────────────────────┤
│ - zone_id: zone                                              │
│ - stubs: map<object, weak_ptr<object_stub>>                 │
│ - transports: map<transport_id, weak_ptr<transport>>        │
│ - service_proxies: map<route_key, weak_ptr<service_proxy>>  │
└──────────────────────────────────────────────────────────────┘
       │ *                                                 │ *
       │ owns                                              │ owns
       │                                                   │
       ↓                                                   ↓
┌─────────────────────────┐                   ┌─────────────────────────┐
│    SERVICE PROXY        │                   │    SERVICE PROXY        │
├─────────────────────────┤                   ├─────────────────────────┤
│ - destination_zone      │                   │ - destination_zone      │
│ - caller_zone           │                   │ - caller_zone           │
│ - transport: shared_ptr─┼──────shares──────►│ - transport: shared_ptr │
│ - object_proxies: map   │                   │ - object_proxies: map   │
│                         │                   │                         │
│ Lifetime managed by:    │                   │ Lifetime managed by:    │
│ - object ref counts     │                   │ - object ref counts     │
│ - (shared + optimistic) │                   │ - (shared + optimistic) │
└─────────────────────────┘                   └─────────────────────────┘
       │ *                                           │ *
       │ aggregates                                  │ aggregates
       ↓                                             ↓
┌─────────────────────────┐                   ┌─────────────────────────┐
│    OBJECT PROXY         │                   │    OBJECT PROXY         │
├─────────────────────────┤                   ├─────────────────────────┤
│ - object_id: object     │                   │ - object_id: object     │
│ - service_proxy: weak   │                   │ - service_proxy: weak   │
│ - shared_count: int     │                   │ - shared_count: int     │
│ - optimistic_count: int │                   │ - optimistic_count: int │
└─────────────────────────┘                   └─────────────────────────┘

             Both service proxies share same transport
                              │
                              ↓
                   ┌─────────────────────────┐
                   │  TRANSPORT (abstract)   │
                   ├─────────────────────────┤
                   │ - local_zone: zone      │
                   │ - remote_zone: zone     │
                   │                         │
                   │ Virtual methods:        │
                   │ - send()                │
                   │ - try_cast()            │
                   │ - add_ref()             │
                   │ - release()             │
                   │ - shutdown()            │
                   └─────────────────────────┘
                              △
                              │ implements
                              │
                   ┌──────────┴──────────┬────────┬─────────┐
                   │                     │        │         │
                ┌──┴───┐             ┌──┴───┐  ┌─┴───┐  ┌──┴────┐
                │ SPSC │             │ TCP  │  │Local│  │ SGX   │
                │      │             │      │  │     │  │       │
                └──────┘             └──────┘  └─────┘  └───────┘
```

### Key Design Decisions

#### 1. Transport as First-Class Citizen

**Interface:**
```cpp
class transport : public std::enable_shared_from_this<transport>
{
public:
    // Identity
    // Transport connects this zone to an adjacent zone
    virtual zone get_local_zone() const = 0;
    virtual zone get_adjacent_zone() const = 0;

    // In routing: this transport is identified by its adjacent zone
    // Example: Zone C has transport to D (adjacent_zone = D)
    // When routing A→B→C→D→E, destination_channel_zone = D identifies this transport
    destination_channel_zone get_destination_channel_zone() const
    {
        return adjacent_zone_.as_destination_channel();
    }

    caller_channel_zone get_caller_channel_zone() const
    {
        return adjacent_zone_.as_caller_channel();
    }

    // RPC operations
    virtual CORO_TASK(int) send(uint64_t protocol_version,
                                 encoding enc,
                                 uint64_t tag,
                                 caller_zone caller_zone_id,
                                 destination_zone destination_zone_id,
                                 object object_id,
                                 interface_ordinal interface_id,
                                 method method_id,
                                 size_t in_size,
                                 const char* in_buf,
                                 std::vector<char>& out_buf) = 0;

    virtual CORO_TASK(int) try_cast(uint64_t protocol_version,
                                     destination_zone dest,
                                     object object_id,
                                     interface_ordinal interface_id) = 0;

    virtual CORO_TASK(int) add_ref(uint64_t protocol_version,
                                    destination_zone dest,
                                    object object_id,
                                    caller_zone caller,
                                    add_ref_options options,
                                    uint64_t& ref_count) = 0;

    virtual CORO_TASK(int) release(uint64_t protocol_version,
                                    destination_zone dest,
                                    object object_id,
                                    caller_zone caller,
                                    release_options options,
                                    uint64_t& ref_count) = 0;

    // Lifecycle
    virtual CORO_TASK(void) shutdown() = 0;

    // Connection status
    virtual bool is_connected() const = 0;
    virtual const char* get_transport_type() const = 0;

protected:
    zone local_zone_;     // This zone (e.g., C)
    zone adjacent_zone_;  // The adjacent zone this transport connects to (e.g., B or D)

    // Transport destroyed when last service_proxy releases it
    // (both forward and reverse proxy ref counts at zero)

    // Transport is identified by adjacent_zone:
    // - Zone C has transport to B: adjacent_zone = B
    // - Zone C has transport to D: adjacent_zone = D
    //
    // When routing A→B→C→D→E through zone C:
    // - Call arrives on transport where adjacent_zone = B (caller_channel_zone = B)
    // - Call forwarded on transport where adjacent_zone = D (destination_channel_zone = D)
};
```

#### 2. Symmetrical Service Proxy Pairs

**Key Principle**: Service proxies always come in pairs, created together with shared transport.

**Creation Pattern:**
```cpp
// When creating a transport, ALWAYS create both service proxy directions
auto transport = transport::create<spsc_transport>(
    local_zone,
    remote_zone,
    transport_args...);

// Create BOTH service proxies sharing the same transport:
// 1. local_zone → remote_zone service proxy (holds shared_ptr<transport>)
// 2. remote_zone → local_zone service proxy (holds shared_ptr<transport>)

service->register_transport(transport);  // Registers both proxies
```

**Lifecycle Invariant:**
```cpp
// Service proxy lifetime = aggregate object ref counts (shared + optimistic)
// Transport lifetime = shared_ptr ref count (held by both service proxies)

// Key behavior:
// 1. Both service proxies hold shared_ptr to transport
// 2. When a service proxy's object ref counts hit zero → service proxy destroyed
// 3. When BOTH service proxies destroyed → transport ref count hits zero
// 4. Transport destroyed → connection closed

// Example:
// Forward proxy: 5 objects (3 shared + 2 optimistic)
// Reverse proxy: 2 objects (2 shared + 0 optimistic)
// → Both proxies alive, transport alive

// Forward proxy: 0 objects → proxy destroyed, transport ref_count = 1
// Reverse proxy: 2 objects → proxy still alive

// Reverse proxy: 0 objects → proxy destroyed, transport ref_count = 0
// → Transport destroyed, connection closed
```

**Benefits:**
- Eliminates Y-topology race condition (both directions always exist)
- Transport naturally destroyed when no longer needed
- Service proxy lifecycle tied to actual usage (object ref counts)
- No manual transport reference counting needed (use shared_ptr)

#### 3. Transport Implementations

**Concrete Implementations:**
```cpp
class spsc_transport : public transport
{
    queue_type* send_queue_;
    queue_type* receive_queue_;
    std::shared_ptr<rpc::service> service_;

    // Absorb channel_manager functionality directly
    std::atomic<uint64_t> sequence_number_;
    std::queue<std::vector<uint8_t>> send_queue_;
    coro::mutex send_queue_mtx_;

    // Pumps managed by transport
    CORO_TASK(void) receive_consumer_task();
    CORO_TASK(void) send_producer_task();
};

class tcp_transport : public transport
{
    coro::net::socket socket_;
    // TCP-specific connection management
};

class local_transport : public transport
{
    std::weak_ptr<rpc::service> remote_service_;
    // Direct function calls, no serialization
};

class sgx_transport : public transport
{
    // Enclave boundary crossing
    sgx_enclave_id_t enclave_id_;
};
```

#### 4. Service Proxy Simplification

**New Responsibilities (Reduced):**
```cpp
class service_proxy
{
public:
    // Identity
    zone get_zone_id() const;
    destination_zone get_destination_zone_id() const;
    caller_zone get_caller_zone_id() const;

    // Delegation to transport
    CORO_TASK(int) send(...) { return transport_->send(...); }
    CORO_TASK(int) try_cast(...) { return transport_->try_cast(...); }
    CORO_TASK(int) add_ref(...) { return transport_->add_ref(...); }
    CORO_TASK(int) release(...) { return transport_->release(...); }

    // Object proxy aggregation (unchanged)
    std::shared_ptr<object_proxy> get_or_create_object_proxy(...);

private:
    std::shared_ptr<transport> transport_;  // Holds shared_ptr to transport
    std::unordered_map<object, std::weak_ptr<object_proxy>> proxies_;

    // Lifecycle: Destroyed when aggregate object ref counts reach zero
    // (sum of shared_count + optimistic_count across all object_proxies)

    // No more: lifetime_lock_ (transport manages itself via shared_ptr)
};
```

**Key Changes:**
- Service proxy holds `shared_ptr<transport>` (not weak_ptr!)
- All RPC operations delegate to transport
- Lifetime = aggregate object reference counts (shared + optimistic)
- Transport destroyed automatically when last service proxy releases it
- Much simpler lifecycle management

---

## Critical Lifecycle Scenarios and Race Conditions

### Scenario 1: Routing/Bridging Service Proxies (No Object Proxies)

**Problem Statement:**
Service proxies may act as pure routing nodes without hosting any objects. These need to stay alive even with zero object references.

```
Zone A ←→ Zone B ←→ Zone C
         (Bridge)

// Zone B has:
// - Service proxy: A ↔ B (may have objects)
// - Service proxy: B ↔ C (may have objects)
// - Service proxy: A ↔ C (routing only, NO objects!)

// The A↔C proxy in Zone B must stay alive even with zero object refs
// because it routes calls from A to C through B
```

**Solution: Transport Reference Counting Independent of Object Counts**

```cpp
class service_proxy
{
private:
    std::shared_ptr<transport> transport_;  // Keeps transport alive
    std::unordered_map<object, std::weak_ptr<object_proxy>> proxies_;

    // NEW: Separate lifecycle tracking
    bool is_routing_proxy_ = false;  // True if proxy is for routing only

    // For debugging: which adjacent zone does this proxy route through?
    destination_channel_zone destination_channel_;  // The adjacent zone on destination side
    caller_channel_zone caller_channel_;            // The adjacent zone on caller side

public:
    // Destructor logic
    ~service_proxy()
    {
        // Routing proxies DON'T check object ref counts
        if (!is_routing_proxy_)
        {
            // Regular proxy - destroy when object refs hit zero
            RPC_ASSERT(get_aggregate_ref_count() == 0);
        }

        // Transport released via shared_ptr destructor
    }

    int get_aggregate_ref_count() const
    {
        int total = 0;
        for (const auto& [obj_id, weak_op] : proxies_)
        {
            if (auto op = weak_op.lock())
            {
                total += op->get_shared_count();
                total += op->get_optimistic_count();
            }
        }
        return total;
    }
};

// Service tracks routing proxies separately
class service
{
private:
    // Regular proxies (destroyed when object refs = 0)
    std::map<zone_route, std::weak_ptr<service_proxy>> service_proxies_;

    // Routing proxies (destroyed explicitly or when transport dies)
    std::map<zone_route, std::shared_ptr<service_proxy>> routing_proxies_;

public:
    void add_routing_proxy(const std::shared_ptr<service_proxy>& proxy)
    {
        proxy->is_routing_proxy_ = true;
        routing_proxies_[{proxy->get_destination_zone(),
                         proxy->get_caller_zone()}] = proxy;
    }

    void remove_routing_proxy(destination_zone dest, caller_zone caller)
    {
        routing_proxies_.erase({dest, caller});
    }
};
```

### Scenario 2: Concurrent add_ref/release Through Bridge

**Problem Statement:**
```
Timeline:
T1: Zone A releases last object reference → sends release to Zone C via B
T2: Zone C creates new reference to same object → sends add_ref to Zone A via B
T3: Both operations arrive at Zone B simultaneously

Race conditions:
- Zone B's routing proxy might be destroyed between T1 and T2
- Mutexes cannot span remote calls (deadlock risk)
- Coroutine yields allow interleaving
```

**Example Code Showing the Race:**
```cpp
// Thread 1 in Zone A (release path)
CORO_TASK(void) zone_a_release_object()
{
    // Last reference released
    uint64_t ref_count;
    auto error = CO_AWAIT proxy_to_c->release(..., ref_count);

    // This travels through Zone B's routing proxy
    // Zone B might be destroying the routing proxy right now!
}

// Thread 2 in Zone C (add_ref path)
CORO_TASK(void) zone_c_add_ref_object()
{
    // New reference created
    uint64_t ref_count;
    auto error = CO_AWAIT proxy_to_a->add_ref(..., ref_count);

    // This also travels through Zone B's routing proxy
    // Might arrive at same time as release from A!
}
```

**Solution: Transport-Level Operation Serialization**

**Key Insight**: Transport must serialize overlapping operations, but service proxies remain independent.

```cpp
class transport
{
private:
    // Per-object operation tracking
    struct object_operation_state
    {
        std::atomic<int> in_flight_operations{0};
        coro::mutex operation_mutex;  // Serializes operations on same object
    };

    std::unordered_map<object, object_operation_state> object_states_;
    std::mutex object_states_mutex_;

    // Transport-wide operation tracking
    std::atomic<int> total_in_flight_operations_{0};
    coro::event shutdown_event_;

public:
    // Wrapper that tracks in-flight operations
    template<typename Callable>
    CORO_TASK(int) execute_operation(object object_id, Callable&& operation)
    {
        // Get or create per-object state
        object_operation_state* state;
        {
            std::lock_guard g(object_states_mutex_);
            state = &object_states_[object_id];
        }

        // Increment in-flight counters
        state->in_flight_operations++;
        total_in_flight_operations_++;

        // Serialize operations on same object
        auto lock = CO_AWAIT state->operation_mutex.lock();

        // Execute the actual operation
        int result = CO_AWAIT operation();

        // Decrement counters
        state->in_flight_operations--;
        total_in_flight_operations_--;

        // Signal if this was the last operation during shutdown
        if (total_in_flight_operations_ == 0)
        {
            shutdown_event_.set();
        }

        CO_RETURN result;
    }

    CORO_TASK(int) add_ref(uint64_t protocol_version,
                           destination_zone dest,
                           object object_id,
                           caller_zone caller,
                           add_ref_options options,
                           uint64_t& ref_count) override
    {
        CO_RETURN CO_AWAIT execute_operation(object_id, [&]() -> CORO_TASK(int) {
            // Actual add_ref implementation
            CO_RETURN CO_AWAIT transport_impl_add_ref(...);
        });
    }

    CORO_TASK(int) release(uint64_t protocol_version,
                           destination_zone dest,
                           object object_id,
                           caller_zone caller,
                           release_options options,
                           uint64_t& ref_count) override
    {
        CO_RETURN CO_AWAIT execute_operation(object_id, [&]() -> CORO_TASK(int) {
            // Actual release implementation
            CO_RETURN CO_AWAIT transport_impl_release(...);
        });
    }

    CORO_TASK(void) shutdown() override
    {
        // Wait for all in-flight operations to complete
        while (total_in_flight_operations_ > 0)
        {
            CO_AWAIT shutdown_event_;
        }

        // Now safe to destroy transport
        CO_AWAIT transport_impl_shutdown();
    }
};
```

### Scenario 3: Service Proxy Destruction During Active Call

**Problem Statement:**
```
Timeline:
T1: Zone A initiates call to Zone C through Zone B
T2: Zone B's routing proxy begins processing
T3: Different thread in Zone B destroys last object → routing proxy destructor runs
T4: Original call in T2 tries to use destroyed routing proxy

This is a use-after-free bug!
```

**Solution: Reference Counting for Active Operations**

```cpp
class service_proxy
{
private:
    std::shared_ptr<transport> transport_;
    std::atomic<int> active_operations_{0};
    coro::event all_operations_complete_;

public:
    // RAII helper for operation tracking
    class operation_guard
    {
        service_proxy* proxy_;
    public:
        operation_guard(service_proxy* p) : proxy_(p)
        {
            proxy_->active_operations_++;
        }

        ~operation_guard()
        {
            if (--proxy_->active_operations_ == 0)
            {
                proxy_->all_operations_complete_.set();
            }
        }
    };

    CORO_TASK(int) send(...)
    {
        operation_guard guard(this);  // Tracks this operation
        CO_RETURN CO_AWAIT transport_->send(...);
    }

    ~service_proxy()
    {
        // Block destructor until all operations complete
#ifdef BUILD_COROUTINE
        // Can't use CO_AWAIT in destructor, must use sync_wait
        if (active_operations_ > 0)
        {
            coro::sync_wait(all_operations_complete_);
        }
#else
        // Blocking mode - operations already complete
        RPC_ASSERT(active_operations_ == 0);
#endif
    }
};
```

### Scenario 4: Transport Destruction Race with Service Proxy Pair

**Problem Statement:**
```
Service Proxy A→B: 1 object (about to be released)
Service Proxy B→A: 0 objects (already at zero)

Timeline:
T1: Thread 1 releases last object in A→B proxy
T2: Thread 1 begins A→B proxy destructor
T3: Thread 2 is simultaneously in B→A proxy destructor (already at zero)
T4: Both threads release shared_ptr<transport> simultaneously
T5: Transport destructor runs - which thread context?

Race: Transport destructor must be safe in either thread context
```

**Solution: Transport Manages Own Cleanup**

```cpp
class transport : public std::enable_shared_from_this<transport>
{
private:
    std::atomic<bool> shutdown_initiated_{false};
    std::atomic<int> destructor_entered_count_{0};

    // Cleanup coroutine scheduled in service context
    CORO_TASK(void) async_shutdown()
    {
        // Wait for all in-flight operations
        while (total_in_flight_operations_ > 0)
        {
            CO_AWAIT shutdown_event_;
        }

        // Close physical connection
        CO_AWAIT close_connection();

        // Notify service that transport is gone
        if (auto svc = service_.lock())
        {
            svc->on_transport_destroyed(get_transport_id());
        }
    }

public:
    virtual ~transport()
    {
        // Ensure shutdown only happens once
        bool expected = false;
        if (!shutdown_initiated_.compare_exchange_strong(expected, true))
        {
            // Another thread already initiated shutdown
            return;
        }

        // Count destructor entries (debugging aid)
        destructor_entered_count_++;

        RPC_DEBUG("Transport to adjacent zone {} destructor entered (count={})",
                  adjacent_zone_.get_val(),
                  destructor_entered_count_.load());

        // Schedule async cleanup
        if (auto svc = service_.lock())
        {
#ifdef BUILD_COROUTINE
            svc->schedule(async_shutdown());
#else
            async_shutdown();  // Blocking mode
#endif
        }
    }
};
```

### Scenario 5: Mutex Ordering Across Zones

**Problem Statement:**
```
Zone A: mutex_a
Zone B: mutex_b
Zone C: mutex_c

Bad pattern:
Thread 1: Lock mutex_a → RPC to B → Lock mutex_b → RPC to C → Lock mutex_c
Thread 2: Lock mutex_c → RPC to B → Lock mutex_b → RPC to A → Lock mutex_a

DEADLOCK! Classic cycle in lock ordering graph.
```

**Solution: No Mutexes Held During RPC Calls**

**Design Principle**: All mutexes are released before any RPC call.

```cpp
class service_proxy
{
private:
    std::unordered_map<object, std::weak_ptr<object_proxy>> proxies_;
    std::mutex proxies_mutex_;

public:
    CORO_TASK(std::shared_ptr<object_proxy>) get_or_create_object_proxy(...)
    {
        std::shared_ptr<object_proxy> op;
        bool is_new = false;

        // Critical section 1: Check if proxy exists
        {
            std::lock_guard g(proxies_mutex_);
            auto it = proxies_.find(object_id);
            if (it != proxies_.end())
            {
                op = it->second.lock();
            }

            if (!op)
            {
                op = std::make_shared<object_proxy>(...);
                proxies_[object_id] = op;
                is_new = true;
            }
        }
        // Mutex released here!

        // RPC call OUTSIDE mutex
        if (is_new)
        {
            uint64_t ref_count;
            auto ret = CO_AWAIT transport_->add_ref(..., ref_count);
            // ^^^ No mutex held during RPC call

            if (ret != error::OK())
            {
                // Cleanup on failure
                std::lock_guard g(proxies_mutex_);
                proxies_.erase(object_id);
                CO_RETURN nullptr;
            }
        }

        CO_RETURN op;
    }
};
```

### Scenario 6: Coroutine Yield Points and State Consistency

**Problem Statement:**
```cpp
CORO_TASK(void) dangerous_pattern()
{
    std::lock_guard g(mutex_);

    auto value = shared_state_;  // Read shared state

    CO_AWAIT some_rpc_call();    // ❌ YIELD POINT with mutex held!

    shared_state_ = value + 1;   // Write shared state
}

// Another coroutine can't acquire mutex while this one is suspended!
// Deadlock if both coroutines need the same mutex.
```

**Solution: Separate Data Copy from RPC Calls**

```cpp
CORO_TASK(void) safe_pattern()
{
    // Copy data while holding mutex
    int value_copy;
    {
        std::lock_guard g(mutex_);
        value_copy = shared_state_;
    }
    // Mutex released before yield point

    // RPC call without mutex
    CO_AWAIT some_rpc_call();

    // Update state with mutex
    {
        std::lock_guard g(mutex_);
        shared_state_ = value_copy + 1;
    }
}
```

---

## Revised Lifecycle Management Design

### Transport Lifecycle States

```cpp
enum class transport_state
{
    CONNECTING,      // Initial state, handshake in progress
    CONNECTED,       // Active, can send/receive
    DISCONNECTING,   // Shutdown initiated, draining operations
    DISCONNECTED     // Closed, no operations allowed
};

class transport
{
private:
    std::atomic<transport_state> state_{transport_state::CONNECTING};
    std::atomic<int> service_proxy_count_{0};  // How many proxies reference this
    std::atomic<int> in_flight_operations_{0}; // Active RPC calls

public:
    // Called by service_proxy constructor
    void register_service_proxy()
    {
        service_proxy_count_++;
    }

    // Called by service_proxy destructor
    void unregister_service_proxy()
    {
        auto count = --service_proxy_count_;
        if (count == 0)
        {
            // Last proxy gone - initiate shutdown
            initiate_shutdown();
        }
    }

    void initiate_shutdown()
    {
        transport_state expected = transport_state::CONNECTED;
        if (state_.compare_exchange_strong(expected, transport_state::DISCONNECTING))
        {
            // Schedule async shutdown coroutine
            schedule_shutdown();
        }
    }

    CORO_TASK(int) send(...)
    {
        // Check state before operation
        if (state_ != transport_state::CONNECTED)
        {
            CO_RETURN error::SERVICE_PROXY_LOST_CONNECTION();
        }

        // Track operation
        in_flight_operations_++;

        // Perform actual send
        auto result = CO_AWAIT transport_impl_send(...);

        // Untrack operation
        in_flight_operations_--;

        CO_RETURN result;
    }

    CORO_TASK(void) schedule_shutdown()
    {
        // Wait for all operations to drain
        while (in_flight_operations_ > 0)
        {
            CO_AWAIT scheduler_->yield();
        }

        // Mark as disconnected
        state_ = transport_state::DISCONNECTED;

        // Close physical connection
        CO_AWAIT close_physical_connection();

        // Notify service
        if (auto svc = service_.lock())
        {
            svc->on_transport_destroyed(adjacent_zone_);
        }
    }
};
```

### Service Proxy Lifecycle Management

```cpp
class service_proxy
{
private:
    std::shared_ptr<transport> transport_;
    std::weak_ptr<service> service_;

    std::unordered_map<object, std::weak_ptr<object_proxy>> proxies_;
    std::mutex proxies_mutex_;

    std::atomic<int> active_operations_{0};
    bool is_routing_proxy_{false};

public:
    service_proxy(std::shared_ptr<transport> t,
                 std::shared_ptr<service> s,
                 bool is_routing)
        : transport_(std::move(t))
        , service_(s)
        , is_routing_proxy_(is_routing)
    {
        transport_->register_service_proxy();
    }

    ~service_proxy()
    {
        // Wait for active operations to complete
        while (active_operations_ > 0)
        {
            std::this_thread::yield();
        }

        // Verify object proxies cleaned up (unless routing proxy)
        if (!is_routing_proxy_)
        {
            std::lock_guard g(proxies_mutex_);

            if (!proxies_.empty())
            {
                RPC_WARNING("Service proxy destroyed with {} object proxies remaining",
                           proxies_.size());
                RPC_ASSERT(proxies_.empty());
            }
        }

        // Unregister from transport (may trigger transport destruction)
        transport_->unregister_service_proxy();
    }

    // Prevent destruction while operations in flight
    class operation_scope_guard
    {
        service_proxy* proxy_;
    public:
        operation_scope_guard(service_proxy* p) : proxy_(p)
        {
            proxy_->active_operations_++;
        }

        ~operation_scope_guard()
        {
            proxy_->active_operations_--;
        }
    };

    CORO_TASK(int) send(...)
    {
        operation_scope_guard guard(this);
        CO_RETURN CO_AWAIT transport_->send(...);
    }
};
```

### Service Integration

```cpp
class service
{
private:
    // Regular service proxies (weak - destroyed when object refs = 0)
    std::map<zone_route, std::weak_ptr<service_proxy>> service_proxies_;
    std::mutex service_proxies_mutex_;

    // Routing proxies (strong - explicitly managed)
    std::map<zone_route, std::shared_ptr<service_proxy>> routing_proxies_;
    std::mutex routing_proxies_mutex_;

    // Transports (weak - destroyed when all service proxies gone)
    // Key = adjacent zone (the immediate neighbor this transport connects to)
    // Example: Zone C has transports_[B] and transports_[D]
    std::map<zone, std::weak_ptr<transport>> transports_;
    std::mutex transports_mutex_;

public:
    std::shared_ptr<transport> create_transport(zone adjacent_zone, ...)
    {
        // Create transport to adjacent zone
        // Example: Zone C creates transport to zone D
        auto transport = std::make_shared<spsc_transport>(
            zone_id_,        // local_zone = C
            adjacent_zone,   // adjacent_zone = D
            ...);

        // Create bidirectional service proxy pair
        auto forward = std::make_shared<service_proxy>(
            transport, shared_from_this(), false);  // Not routing

        auto reverse = std::make_shared<service_proxy>(
            transport, shared_from_this(), false);  // Not routing

        // Register both
        {
            std::lock_guard g(service_proxies_mutex_);
            service_proxies_[{adjacent_zone.as_destination(), zone_id_.as_caller()}] = forward;
            service_proxies_[{zone_id_.as_destination(), adjacent_zone.as_caller()}] = reverse;
        }

        // Track transport (keyed by adjacent zone)
        {
            std::lock_guard g(transports_mutex_);
            transports_[adjacent_zone] = transport;
        }

        return transport;
    }

    void create_routing_proxy(destination_zone dest, caller_zone caller)
    {
        // Find transport that can route to dest
        std::shared_ptr<transport> routing_transport;

        {
            std::lock_guard g(transports_mutex_);
            // Find appropriate transport (implementation depends on topology)
            routing_transport = find_routing_transport(dest);
        }

        if (!routing_transport)
        {
            RPC_ERROR("No route to destination zone {}", dest.get_val());
            return;
        }

        // Create routing proxy (marked as routing)
        auto routing_proxy = std::make_shared<service_proxy>(
            routing_transport, shared_from_this(), true);  // Is routing

        {
            std::lock_guard g(routing_proxies_mutex_);
            routing_proxies_[{dest, caller}] = routing_proxy;
        }
    }

    void on_transport_destroyed(zone adjacent_zone)
    {
        // Called by transport destructor
        // Example: Zone C's transport to D is being destroyed

        // Remove transport tracking
        {
            std::lock_guard g(transports_mutex_);
            transports_.erase(adjacent_zone);
        }

        // Service proxies already destroyed (weak_ptrs automatically cleaned)
        // Routing proxies manually removed if they used this transport
        {
            std::lock_guard g(routing_proxies_mutex_);

            for (auto it = routing_proxies_.begin(); it != routing_proxies_.end();)
            {
                // Check if routing proxy uses this transport
                // Routing proxy's destination_channel_zone indicates which transport
                auto proxy_dest_channel = it->first.dest;
                if (proxy_dest_channel.as_zone() == adjacent_zone)
                {
                    it = routing_proxies_.erase(it);
                }
                else
                {
                    ++it;
                }
            }
        }
    }
};
```

---

## Migration Plan

### Phase 1: Extract Transport Interface (Week 1-2)

**Deliverables:**
- [ ] Define `rpc::transport` abstract base class
- [ ] Update `service` to have `transports_` map (keyed by remote zone)
- [ ] Clarify that `destination_channel_zone` IS the transport identifier
- [ ] Add documentation explaining `destination_channel_zone` = transport ID
- [ ] Implement transport registration/unregistration mechanism

**Testing:**
- Unit tests for channel lifecycle
- Reference counting correctness tests

### Phase 2: Refactor SPSC Transport (Week 3-4)

**Rationale**: SPSC already has embryonic channel manager concept, making it the ideal pilot implementation.

**Deliverables:**
- [ ] Create `spsc_transport` implementing `transport` interface
- [ ] Refactor `spsc::service_proxy` to delegate to transport
- [ ] Migrate `channel_manager` logic into `spsc_transport`
- [ ] Use `adjacent_zone` as transport identifier
- [ ] Understand: `destination_channel_zone` = adjacent zone in destination direction
- [ ] Understand: `caller_channel_zone` = adjacent zone in caller direction
- [ ] Update SPSC tests to use new architecture

**Testing:**
- All existing SPSC tests must pass
- New tests for channel sharing across service proxies
- Symmetrical proxy creation tests

### Phase 3: Implement Symmetrical Service Proxies (Week 5-6)

**Deliverables:**
- [ ] Modify `service::connect_to_zone()` to create channel + proxy pair
- [ ] Modify `service::attach_remote_zone()` to create channel + proxy pair
- [ ] Implement `service::register_channel()` method
- [ ] Update routing logic to use channel-based lookup
- [ ] Remove on-demand proxy creation from `service::send()`
- [ ] Remove opposite_direction_proxy workaround (service.cpp:239)

**Testing:**
- Y-topology tests with new architecture
- Verify both directions exist immediately after connection
- Test proxy destruction only when channel destroyed

### Phase 4: Refactor Remaining Transports (Week 7-10)

**TCP Transport:**
- [ ] Create `tcp_transport` class
- [ ] Create `tcp_channel` class
- [ ] Migrate TCP-specific connection logic
- [ ] Update tests

**Local Transport:**
- [ ] Create `local_transport` class (direct calls)
- [ ] Create `local_channel` class
- [ ] Update tests

**SGX Transport:**
- [ ] Create `sgx_transport` class
- [ ] Create `sgx_channel` class
- [ ] Handle enclave boundary semantics
- [ ] Update tests

### Phase 5: Cleanup and Optimization (Week 11-12)

**Deliverables:**
- [ ] Remove unused code (old proxy creation patterns)
- [ ] Optimize channel lookup performance
- [ ] Add telemetry for channel lifecycle
- [ ] Update documentation
- [ ] Performance benchmarking vs old architecture

**Testing:**
- Full regression test suite
- Performance comparison tests
- Memory leak detection
- Stress tests with many channels

---

## Detailed Technical Specifications

### Channel Lifecycle Management

#### Creation

```cpp
// Service creates channel and registers both proxy directions
template<class ChannelType, typename... Args>
std::shared_ptr<channel> service::create_channel(
    zone remote_zone_id,
    Args&&... args)
{
    auto channel_id = generate_channel_id();

    auto channel = std::make_shared<ChannelType>(
        zone_id_,
        remote_zone_id,
        channel_id,
        std::forward<Args>(args)...);

    // Create bidirectional service proxy pair
    auto forward_proxy = std::make_shared<service_proxy>(
        zone_id_,
        remote_zone_id,
        channel);

    auto reverse_proxy = std::make_shared<service_proxy>(
        zone_id_,
        zone_id_.as_caller(),  // Swap: remote becomes caller
        channel);

    // Register both
    service_proxies_[{remote_zone_id, zone_id_.as_caller()}] = forward_proxy;
    service_proxies_[{zone_id_.as_destination(), remote_zone_id.as_caller()}] = reverse_proxy;

    // Register channel
    channels_[channel_id] = channel;

    return channel;
}
```

#### Reference Counting

**Channel Reference Counting:**
```cpp
// Channel ref count tracks:
// - Service proxy pairs (always +2 when both directions exist)
// - Active operations (temporary +1 during calls)

void channel::add_channel_ref()
{
    channel_ref_count_++;
}

int channel::release_channel_ref()
{
    auto count = --channel_ref_count_;
    if (count == 0)
    {
        // Initiate shutdown
        schedule_shutdown();
    }
    return count;
}
```

**Service Proxy Reference Counting:**
```cpp
// Service proxies NO LONGER manage their own lifetime
// Lifetime = channel lifetime

service_proxy::~service_proxy()
{
    // Notify channel that one direction is gone
    if (auto ch = channel_.lock())
    {
        ch->on_proxy_destroyed();
    }
}
```

#### Destruction

```cpp
// Channel destruction sequence:
// 1. ref_count reaches 0
// 2. Shutdown initiated
// 3. Pending operations complete
// 4. Transport closed
// 5. Service proxies destroyed
// 6. Channel deleted

CORO_TASK(void) channel::shutdown()
{
    // Close transport
    CO_AWAIT transport_->shutdown();

    // Notify service to remove proxies
    auto svc = service_.lock();
    if (svc)
    {
        svc->on_channel_destroyed(id_);
    }
}

void service::on_channel_destroyed(channel_id id)
{
    auto channel = channels_[id];

    // Remove both service proxy directions
    remove_service_proxy(channel->get_remote_zone(), zone_id_.as_caller());
    remove_service_proxy(zone_id_.as_destination(), channel->get_remote_zone().as_caller());

    // Remove channel
    channels_.erase(id);
}
```

### Routing Logic Changes

#### Old Pattern (Current)

```cpp
// In service::send() - complex routing with on-demand proxy creation
std::shared_ptr<service_proxy> other_zone;
std::shared_ptr<service_proxy> opposite_direction_proxy;

{
    std::lock_guard g(zone_control);

    // Find forward direction
    if (auto found = other_zones.find({destination_zone_id, caller_zone_id});
        found != other_zones.end())
    {
        other_zone = found->second.lock();
    }

    // Find or CREATE reverse direction (THE BUG!)
    if (auto found = other_zones.find({caller_zone_id.as_destination(),
                                       destination_zone_id.as_caller()});
        found != other_zones.end())
    {
        opposite_direction_proxy = found->second.lock();
        if (!opposite_direction_proxy)
        {
            // NULL POINTER - must create on-the-fly
            // This is the Y-topology race condition!
            opposite_direction_proxy = temp->clone_for_zone(...);
            inner_add_zone_proxy(opposite_direction_proxy);
        }
    }
}
```

#### New Pattern (Proposed)

```cpp
// In service::send() - simple routing, no on-demand creation
std::shared_ptr<service_proxy> proxy;

{
    std::lock_guard g(proxy_control);

    auto found = service_proxies_.find({destination_zone_id, caller_zone_id});
    if (found != service_proxies_.end())
    {
        proxy = found->second.lock();
    }
}

if (!proxy)
{
    // Proxy doesn't exist = channel doesn't exist
    // This is a programming error, not a race condition
    RPC_ERROR("No route to zone {}", destination_zone_id);
    return error::ZONE_NOT_FOUND();
}

// Delegate to channel
return CO_AWAIT proxy->get_channel()->send(...);
```

**Key Differences:**
- No on-demand creation
- No opposite_direction_proxy workaround
- Simple lookup and delegate
- Failure = missing route, not race condition

---

## Race Condition Analysis and Solutions

### Race Condition 1: Y-Topology Object Passing

**Scenario:**
```
Root (Z1) → Prong A (Z2) → Prong B (Z3)
                            ↓ creates object
Root (Z1) ← ← ← ← ← ← ← ← Prong B (Z3)
```

**Current Problem:**
- Root has no service proxy to Zone 3
- add_ref from Root to Zone 3 requires proxy creation
- Proxy creation happens in critical path
- Race window between creation and use

**Solution with Channels:**
```cpp
// When Prong A creates Prong B:
auto channel = prong_a_service->create_channel<...>(zone_3, ...);

// This IMMEDIATELY creates service proxies in BOTH DIRECTIONS:
// - Zone 2 → Zone 3 (forward)
// - Zone 3 → Zone 2 (reverse)

// When object is passed to Root:
// - Object contains zone 3 reference
// - Root looks up route to zone 3
// - Route exists through Zone 2 (intermediate hop)
// - No on-demand creation needed
```

**How Routing Works:**
```cpp
// Root wants to call Zone 3
// Routing table:
//   Root → Zone 2 (direct channel)
//   Zone 2 → Zone 3 (direct channel)
//
// Root delegates to Zone 2 proxy
// Zone 2 proxy delegates to Zone 2's Zone 3 proxy
// Zone 3 receives call

// NO creation needed - all proxies exist from channel creation
```

### Race Condition 2: Concurrent Proxy Destruction

**Scenario:**
- Thread 1: Last reference to object released
- Thread 2: New reference to same object created
- Race on proxy map insertion/deletion

**Current Problem:**
```cpp
void service_proxy::on_object_proxy_released(...)
{
    {
        std::lock_guard l(insert_control_);
        proxies_.erase(object_id);  // Thread 1
    }
    // Thread 2 might insert here!
    cleanup_after_object(...);
}
```

**Solution:**
```cpp
// Service proxy lifetime is independent of object references
// Proxy map only cleaned up when channel destroyed
// No race between object add_ref and service proxy destruction

CORO_TASK(void) channel::shutdown()
{
    // All service proxies destroyed atomically
    // No incremental cleanup during object operations
}
```

### Race Condition 3: Weak Pointer Lock Failures

**Scenario:**
```cpp
auto found = other_zones.find({dest, caller});
auto proxy = found->second.lock();  // Returns nullptr!
```

**Current Problem:**
- Service proxy destroyed between find and lock
- Must handle null pointer reactively

**Solution:**
```cpp
// Service proxies never destroyed while channel is active
// Channel ensures proxies exist for its lifetime
// Lock never fails unless channel is being destroyed

auto found = service_proxies_.find({dest, caller});
if (found == service_proxies_.end())
{
    // No route - explicit error
    return error::ZONE_NOT_FOUND();
}

auto proxy = found->second.lock();
if (!proxy)
{
    // Channel is being destroyed - connection lost
    return error::SERVICE_PROXY_LOST_CONNECTION();
}
```

---

## Testing Strategy

### Unit Tests

**Channel Tests:**
- [ ] Channel creation with different transports
- [ ] Channel reference counting
- [ ] Channel shutdown behavior
- [ ] Multiple service proxy pairs on same channel

**Service Proxy Tests:**
- [ ] Symmetrical creation
- [ ] Delegation to channel
- [ ] Lifetime tied to channel
- [ ] Object proxy aggregation

**Transport Tests:**
- [ ] SPSC transport send/receive
- [ ] TCP transport connection management
- [ ] Local transport (no serialization)
- [ ] SGX transport boundary crossing

### Integration Tests

**Topology Tests:**
- [ ] Y-topology object passing (regression test for old bug)
- [ ] Tree topology (Root with multiple children)
- [ ] Mesh topology (multiple interconnected zones)
- [ ] Dynamic topology (zones created and destroyed)

**Routing Tests:**
- [ ] Direct routing (single hop)
- [ ] Indirect routing (multi-hop through intermediates)
- [ ] Route discovery failure
- [ ] Channel loss during routing

**Reference Counting Tests:**
- [ ] Channel ref counting with multiple proxies
- [ ] Object ref counting independent of channel
- [ ] Cleanup ordering (objects before proxies before channel)

### Stress Tests

**Concurrency:**
- [ ] Many threads creating channels simultaneously
- [ ] Many threads performing RPC calls
- [ ] Channel creation and destruction under load

**Scale:**
- [ ] 1000+ zones in single process
- [ ] 10000+ objects passed between zones
- [ ] Long-running connections (hours/days)

**Failure Scenarios:**
- [ ] Channel failure during RPC call
- [ ] Unexpected zone destruction
- [ ] Memory exhaustion
- [ ] Transport-specific failures (network errors, queue full)

---

## Performance Considerations

### Potential Improvements

1. **Channel Pooling**: Reuse channels for same zone pairs
2. **Lock-Free Routing**: Read-only lookup without locks
3. **Inline Local Calls**: Optimize local transport to avoid serialization
4. **Batch Operations**: Group multiple add_ref/release calls

### Performance Metrics

**Baseline (Current):**
- RPC call latency (SPSC, TCP, Local)
- Memory per service proxy
- Lock contention in routing

**Target (After Refactoring):**
- Same or better latency
- 30-50% less memory per connection (fewer service proxy instances)
- Reduced lock contention (no on-demand proxy creation)

---

## Backward Compatibility

### API Changes

**Breaking Changes:**
```cpp
// Old: service_proxy specific to transport
auto proxy = service->connect_to_zone<spsc::service_proxy>(...);

// New: channel-based creation
auto channel = service->connect_to_zone<spsc_channel>(...);
auto proxy = channel->get_forward_proxy();
```

**Migration Path:**
1. Keep old API as compatibility shim
2. Deprecate old API
3. Provide migration tool/script
4. Remove old API in next major version

### Wire Protocol

**No Changes Required:**
- Wire protocol unchanged
- Encoding/decoding unchanged
- Version negotiation unchanged

**Internal Changes:**
- Channel ID added to internal routing
- Does not appear on wire

---

## Open Questions and Future Work

### Open Questions

1. **Channel Sharing**: Should multiple object streams automatically share channels?
   - Pro: Fewer connections, better multiplexing
   - Con: More complex lifecycle management

2. **Channel Discovery**: How do zones discover existing channels to reuse?
   - Option A: Service maintains channel registry by remote_zone
   - Option B: Application explicitly passes channel handle

3. **Partial Channel Failure**: What happens if transport fails but service proxies still referenced?
   - Option A: Mark channel as "dead", all operations return error
   - Option B: Attempt reconnection transparently

4. **Channel Handshake**: Should channels have explicit connect/handshake phase?
   - Current: Implicit (first message establishes connection)
   - Proposed: Explicit handshake with capability negotiation

### Future Enhancements

1. **Multi-Transport Channels**: Single channel with multiple transports for redundancy
2. **Dynamic Transport Selection**: Choose TCP vs SPSC based on latency/bandwidth
3. **Channel Migration**: Move channel to different transport without destroying proxies
4. **Telemetry Integration**: Channel-level metrics (bandwidth, latency, error rates)

---

## Success Criteria

### Functional

- [ ] All existing tests pass with new architecture
- [ ] Y-topology object passing works without special-case code
- [ ] No race conditions in service proxy creation/destruction
- [ ] Symmetrical proxies exist for all channels

### Performance

- [ ] RPC call latency within 5% of baseline
- [ ] Memory usage reduced by at least 20%
- [ ] Lock contention measurably reduced
- [ ] No memory leaks under stress testing

### Code Quality

- [ ] Service proxy < 500 LOC (currently ~800 LOC)
- [ ] Service < 1500 LOC (currently ~1900 LOC)
- [ ] No null pointer checks in hot paths
- [ ] Clear separation of concerns (routing vs transport)

### Documentation

- [ ] Architecture document updated
- [ ] User guide with channel examples
- [ ] Migration guide for existing code
- [ ] Performance characterization report

---

## Risk Assessment

### High Risk

1. **Breaking Changes**: API changes affect all users
   - **Mitigation**: Compatibility shim, thorough testing, staged rollout

2. **Performance Regression**: New abstraction adds overhead
   - **Mitigation**: Benchmark before and after, optimize critical paths

### Medium Risk

3. **Incomplete Migration**: Some transport doesn't fit new model
   - **Mitigation**: Pilot with SPSC (already has channel concept)

4. **Telemetry Gaps**: Existing telemetry assumes old architecture
   - **Mitigation**: Update telemetry alongside refactoring

### Low Risk

5. **Test Coverage**: New code paths not adequately tested
   - **Mitigation**: Comprehensive test plan, code review

---

## Timeline Summary

| Phase | Duration | Key Deliverables |
|-------|----------|------------------|
| Phase 1: Channel Interface | 2 weeks | channel/transport abstractions |
| Phase 2: SPSC Pilot | 2 weeks | Working SPSC channel |
| Phase 3: Symmetrical Proxies | 2 weeks | Y-topology bug fix |
| Phase 4: Remaining Transports | 4 weeks | TCP, Local, SGX channels |
| Phase 5: Cleanup & Optimization | 2 weeks | Documentation, benchmarks |
| **Total** | **12 weeks** | Production-ready implementation |

---

## Approval and Sign-Off

This document represents the planning phase for a major architectural refactoring. Implementation should not begin until:

- [ ] Technical review completed
- [ ] Performance targets agreed upon
- [ ] Test strategy approved
- [ ] Timeline and resource allocation confirmed

---

## Appendix A: Code Examples

### Example 1: Creating a Channel (New API)

```cpp
// Application code
auto root_service = std::make_shared<rpc::service>("root", zone{1}, scheduler);

// Create SPSC channel to child zone
auto channel = root_service->create_channel<spsc_channel>(
    zone{2},              // Remote zone
    "child_connection",   // Channel name
    timeout_ms,           // Transport-specific args...
    &send_queue,
    &receive_queue);

// Get service proxy (automatically created with channel)
auto proxy = root_service->get_service_proxy(zone{2}, zone{1}.as_caller());

// Use proxy (same as before)
int result;
auto error = CO_AWAIT proxy->send(...);
```

### Example 2: Y-Topology with New Architecture

```cpp
// Root creates Prong A
auto channel_a = root->create_channel<local_channel>(zone{2}, "prong_a");
auto proxy_a = root->get_service_proxy(zone{2}, zone{1}.as_caller());

// Prong A creates Prong B
auto prong_a_service = /* obtain from callback */;
auto channel_b = prong_a_service->create_channel<local_channel>(zone{3}, "prong_b");
auto proxy_b = prong_a_service->get_service_proxy(zone{3}, zone{2}.as_caller());

// At this point:
// - Root has no direct channel to Zone 3 (correct!)
// - Root → Zone 2 channel exists
// - Zone 2 → Zone 3 channel exists
// - BOTH directions exist for each channel

// Prong B creates object and returns to Prong A
rpc::shared_ptr<i_object> obj;
CO_AWAIT prong_b_impl->create_object(obj);

// Prong A returns object to Root
CO_AWAIT root_callback(obj);

// Root attempts to use object
CO_AWAIT obj->method();  // Works!

// How routing works:
// 1. Root looks up proxy for Zone 3, caller=Zone 1
// 2. Not found directly
// 3. Root looks up intermediate route through Zone 2
// 4. Finds Root→Zone2 proxy
// 5. Delegates to Zone 2's service
// 6. Zone 2 finds Zone2→Zone3 proxy
// 7. Call succeeds

// NO RACE CONDITION - all proxies existed from channel creation
```

### Example 3: Channel Reference Counting

```cpp
auto channel = service->create_channel<spsc_channel>(...);
// channel ref_count = 2 (forward + reverse proxy)

{
    // Temporarily hold reference during operation
    channel->add_channel_ref();
    // channel ref_count = 3

    CO_AWAIT channel->send(...);

    channel->release_channel_ref();
    // channel ref_count = 2
}

// Both service proxies destroyed
channel.reset();
// channel ref_count = 0
// Shutdown initiated
```

---

## Appendix B: Glossary

**Transport**: Bidirectional communication connection between two zones, managing physical layer (SPSC queue, TCP socket, etc.). The transport IS what was previously called "channel". Identified by `destination_channel_zone` (which equals remote zone ID). Destroyed when all service proxies holding `shared_ptr` to it are destroyed.

**Service Proxy**: Lightweight routing object that holds `shared_ptr<transport>` and delegates RPC operations to it. Always created in pairs (bidirectional). Lifetime tied to aggregate object reference counts (shared + optimistic).

**Routing Proxy**: Special service proxy with zero object references, used purely for routing calls through intermediate zones. Held by service via `shared_ptr` (not `weak_ptr`) to keep it alive.

**Zone**: Execution context (process, thread, enclave) with its own service and object stubs.

**Symmetrical Proxies**: Bidirectional service proxy pairs created together when a transport is established. Both hold `shared_ptr` to same transport.

**Y-Topology**: Zone hierarchy where Root→A→B but Root has no direct connection to B. Solved by routing proxies in zone A that route Root→B calls through A.

**Aggregate Ref Count**: Sum of (shared_count + optimistic_count) across all object proxies in a service proxy. When this reaches zero, service proxy is destroyed.

**Transport Registration**: Each service proxy calls `transport->register_service_proxy()` in constructor and `transport->unregister_service_proxy()` in destructor. When registration count hits zero, transport initiates shutdown.

**destination_channel_zone**: The immediate adjacent zone in the destination direction. This identifies which transport to use for the next hop toward the destination. In a routing scenario A→B→C→D→E where zone C is processing, `destination_channel_zone` = D (the next hop).

**caller_channel_zone**: The immediate adjacent zone in the caller direction. This identifies which transport the call arrived on. In the same routing scenario, `caller_channel_zone` = B (where the call came from).

**Transport Identification**: A transport in zone C is identified by the adjacent zone it connects to. Zone C will have transports to both B and D. When routing a call from A to E, zone C looks up transport to D using `destination_channel_zone`, and knows the call came from transport to B using `caller_channel_zone`.

**Routing Example**:
```
Call path: A → B → C → D → E

In Zone C:
  caller_zone = A               (original caller)
  destination_zone = E          (final destination)
  caller_channel_zone = B       (adjacent zone where call came from)
  destination_channel_zone = D  (adjacent zone to forward call to)

Zone C has two transports:
  transports_[B] → transport connecting C to B
  transports_[D] → transport connecting C to D

To route the call:
  1. Look up transports_[D] using destination_channel_zone
  2. Forward call on that transport toward E
```

**Operation Guard**: RAII helper that increments `active_operations_` counter during RPC call, preventing service proxy destruction mid-operation.

**Transport State Machine**: CONNECTING → CONNECTED → DISCONNECTING → DISCONNECTED. Operations rejected if not in CONNECTED state.

**Mutex Discipline**: NO mutexes held across RPC calls or coroutine yield points. All critical sections are short and release before remote operations.

---

## Appendix C: References

- **Current Code**: `/rpc/include/rpc/internal/service_proxy.h`, `/rpc/src/service.cpp`
- **Y-Topology Bug**: `service.cpp:239-250`
- **SPSC Channel**: `/tests/common/include/common/spsc/channel_manager.h`
- **Documentation**: `SPSC_Channel_Lifecycle.md`, `RPC++_User_Guide.md`

---

## Summary of Key Design Decisions

### 1. **Transport = What Was Called "Channel"**
The transport IS the bidirectional connection. There's no separate "channel" and "transport" - they're the same thing.

### 2. **Service Proxy Lifecycle = Object Reference Counts**
Service proxies are destroyed when their aggregate object reference count (shared + optimistic) reaches zero. This naturally cleans up unused routes.

### 3. **Routing Proxies Have Special Lifecycle**
Routing proxies (zero objects, used for bridging) are held by service via `shared_ptr` in separate `routing_proxies_` map. They stay alive until explicitly removed or transport destroyed.

### 4. **Transport Lifecycle = Service Proxy Registration Count**
- Each service proxy calls `register_service_proxy()` on construction
- Each calls `unregister_service_proxy()` on destruction
- When count hits zero, transport initiates shutdown
- Natural: transport lives as long as any proxy needs it

### 5. **Symmetrical Proxy Pairs Share Transport**
```cpp
auto transport = std::make_shared<spsc_transport>(...);

// Both proxies hold shared_ptr to same transport
auto forward = std::make_shared<service_proxy>(transport, ...);
auto reverse = std::make_shared<service_proxy>(transport, ...);

// When BOTH destroyed → transport ref count = 0 → shutdown
```

### 6. **Per-Object Operation Serialization**
Transport maintains `coro::mutex` per object to serialize conflicting operations (concurrent add_ref/release). Prevents race conditions in bridging scenarios.

### 7. **No Mutexes Across Zones**
Critical design rule: ALL mutexes released before RPC calls. Prevents deadlock in multi-zone scenarios.

### 8. **Operation Guards Prevent Use-After-Free**
```cpp
CORO_TASK(int) service_proxy::send(...)
{
    operation_scope_guard guard(this);  // active_operations_++
    CO_RETURN CO_AWAIT transport_->send(...);
}  // active_operations_--

~service_proxy()
{
    // Blocks until active_operations_ == 0
    while (active_operations_ > 0)
        yield();
}
```

### 9. **Transport State Machine**
Transport state prevents operations on disconnecting/disconnected transports, providing clear error codes rather than undefined behavior.

### 10. **Coroutine-Safe Mutex Discipline**
```cpp
// GOOD: Copy data, release mutex, then RPC
{
    std::lock_guard g(mutex_);
    data_copy = shared_state_;
}  // Mutex released
CO_AWAIT remote_call(data_copy);  // Safe yield point

// BAD: Mutex held across yield
std::lock_guard g(mutex_);
CO_AWAIT remote_call();  // ❌ Deadlock risk!
```

---

## Race Condition Resolution Summary

| Scenario | Problem | Solution |
|----------|---------|----------|
| **Y-Topology** | On-demand proxy creation races | Routing proxies created upfront, held alive |
| **Concurrent add_ref/release** | Operations collide in bridge zone | Per-object mutex serializes operations |
| **Proxy destruction during call** | Use-after-free on active proxy | Operation guards block destructor |
| **Transport destruction race** | Both proxies destroyed simultaneously | Atomic `shutdown_initiated_` flag |
| **Mutex ordering deadlock** | Cycles in lock order graph | NO mutexes held during RPC |
| **Coroutine yield with lock** | Suspended while holding mutex | Copy data, release, then yield |

---

## Implementation Checklist

### Core Infrastructure
- [ ] Clarify: `destination_channel_zone` IS the transport identifier (no new type needed)
- [ ] Implement `transport` base class with state machine
- [ ] Implement `spsc_transport` (absorb `channel_manager` logic)
- [ ] Add `transport->register_service_proxy()` mechanism
- [ ] Add per-object operation serialization to transport
- [ ] Optionally rename `destination_channel_zone` → `destination_transport_zone` (for clarity)
- [ ] Optionally rename `caller_channel_zone` → `caller_transport_zone` (for clarity)

### Service Proxy Changes
- [ ] Add `std::shared_ptr<transport> transport_` member
- [ ] Add `bool is_routing_proxy_` flag
- [ ] Implement `operation_scope_guard` RAII helper
- [ ] Add `active_operations_` counter and destructor blocking
- [ ] Update all RPC methods to use operation guards
- [ ] Remove `lifetime_lock_` mechanism (replaced by transport ref counting)

### Service Changes
- [ ] Add `routing_proxies_` map (strong references)
- [ ] Add `transports_` map keyed by `zone` (remote zone, weak references)
- [ ] Implement `create_transport()` with bidirectional proxy creation
- [ ] Implement `create_routing_proxy()` for bridging scenarios
- [ ] Implement `on_transport_destroyed(zone remote_zone)` callback
- [ ] Remove on-demand proxy creation from `send()` (lines 239-250)
- [ ] Update all uses of `destination_channel_zone` to understand it's transport ID

### Testing
- [ ] Unit tests for transport state machine
- [ ] Unit tests for per-object operation serialization
- [ ] Integration test: Y-topology object passing
- [ ] Integration test: Concurrent add_ref/release through bridge
- [ ] Integration test: Service proxy destruction during active call
- [ ] Stress test: Many threads performing RPC calls
- [ ] Stress test: Rapid transport creation/destruction

### Documentation
- [ ] Update architecture diagrams
- [ ] Document routing proxy lifecycle
- [ ] Document mutex discipline rules
- [ ] Add coroutine safety guidelines
- [ ] Update migration guide for existing code

---

**End of Document**
