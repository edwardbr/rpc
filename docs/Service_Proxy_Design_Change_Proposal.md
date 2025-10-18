<!--
Copyright (c) 2025 Edward Boggis-Rolfe
All rights reserved.
-->

# Service Proxy Design Change Proposal

**Version**: 1.0
**Date**: 2025-01-18
**Status**: Proposed
**Related Documents**:
- Service_Proxy_Transport_Problem_Statement.md (Problem Analysis)
- Service_Proxy_Channel_Refactoring_Plan.md (Detailed Implementation Plan)

---

## Executive Summary

This document proposes a series of concrete design changes to address the critical issues identified in the RPC++ service proxy architecture. The proposal focuses on:

1. **Extracting Transport Abstraction** - Separate transport from routing logic
2. **Symmetrical Service Proxy Pairs** - Eliminate asymmetric creation and lifecycle
3. **Unified Channel Pattern** - Apply SPSC's emergent channel_manager pattern to all transports
4. **Explicit Lifecycle Management** - Clear ownership and destruction order
5. **Race Condition Elimination** - Remove Y-topology workaround and on-demand creation

**Impact**: These changes will eliminate the Y-topology race condition, simplify testing, reduce code complexity by ~40%, and provide a foundation for future transport implementations.

---

## Table of Contents

1. [Design Principles](#design-principles)
2. [Proposed Changes](#proposed-changes)
   - [Change 1: Extract Transport Interface](#change-1-extract-transport-interface)
   - [Change 2: Implement Symmetrical Service Proxy Pairs](#change-2-implement-symmetrical-service-proxy-pairs)
   - [Change 3: Unified Transport Lifecycle Management](#change-3-unified-transport-lifecycle-management)
   - [Change 4: Eliminate Y-Topology Race Condition](#change-4-eliminate-y-topology-race-condition)
   - [Change 5: Simplify Service Proxy Responsibilities](#change-5-simplify-service-proxy-responsibilities)
   - [Change 6: Improve Type System Clarity](#change-6-improve-type-system-clarity)
   - [Change 7: Service Sink Architecture (Alternative/Complementary)](#change-7-service-sink-architecture-alternativecomplementary-approach)
3. [Migration Strategy](#migration-strategy)
4. [Implementation Phases](#implementation-phases)
5. [Risk Analysis](#risk-analysis)
6. [Success Criteria](#success-criteria)

---

## Design Principles

### Principle 1: Separation of Concerns

**Current Problem**: Service proxy handles routing + transport + lifecycle + protocol negotiation.

**Proposed Solution**:
```
service_proxy:  Routing logic + object proxy aggregation
transport:      Bidirectional communication + serialization
service:        Lifecycle management + transport ownership
```

**Rationale**: Single Responsibility Principle - each component has one reason to change.

### Principle 2: Symmetrical Bidirectional Pairs

**Current Problem**: Service proxies created asymmetrically - forward explicit, reverse on-demand.

**Proposed Solution**:
```cpp
// Creating connection A↔B creates BOTH directions simultaneously
auto [proxy_a_to_b, proxy_b_to_a] = create_bidirectional_proxies(...);

// Both proxies share the same transport
// Both destroyed together when transport is destroyed
```

**Rationale**: Bidirectional transport implies bidirectional service proxies. Lifecycle symmetry eliminates race conditions.

### Principle 3: Explicit Transport Ownership

**Current Problem**: Transport lifetime tied to individual service proxy, not the pair.

**Proposed Solution**:
```cpp
class transport {
    // Bidirectional communication
    // Reference counted by BOTH service proxy directions
    // Destroyed when both directions unregister
};

class service_proxy {
    std::shared_ptr<transport> transport_;  // Shared ownership
};
```

**Rationale**: Transport is inherently bidirectional. Both directions must exist for transport to function.

### Principle 4: Proactive Creation Over Reactive

**Current Problem**: Y-topology requires on-demand service proxy creation during send().

**Proposed Solution**:
```cpp
// Service proxies created proactively when transport is established
// No creation during critical path (send, add_ref, release)
// Routing proxies held alive by service, not reference counts
```

**Rationale**: Eliminates race windows and simplifies concurrency reasoning.

### Principle 5: Type-Safe Transport Identification

**Current Problem**: `destination_channel_zone` has misleading comment, conflates ultimate destination with adjacent zone.

**Proposed Solution**:
```cpp
// Rename for clarity
using transport_zone = destination_channel_zone;  // Adjacent zone identifying transport

// Or introduce new type
struct adjacent_zone {
    zone zone_id;
    // Identifies immediate neighbor for transport routing
};
```

**Rationale**: Clear naming prevents confusion between routing concepts.

---

## Proposed Changes

### Change 1: Extract Transport Interface

**Objective**: Create explicit transport abstraction separate from service_proxy.

#### Current State
```cpp
class service_proxy : public i_marshaller {
    // Mixed: routing + transport + lifecycle

    virtual CORO_TASK(int) send(...) = 0;  // Transport operation
    virtual CORO_TASK(error_code) try_cast(...) = 0;  // Routing operation
    // ... etc
};
```

#### Proposed State
```cpp
// New transport interface
class i_transport {
public:
    virtual ~i_transport() = default;

    // Bidirectional communication
    virtual CORO_TASK(int) send(destination_zone dest,
                                 caller_zone caller,
                                 const marshalled_message& msg) = 0;

    virtual CORO_TASK(int) receive(marshalled_message& msg) = 0;

    // Lifecycle
    virtual CORO_TASK(void) attach_service_proxy() = 0;
    virtual CORO_TASK(void) detach_service_proxy() = 0;
    virtual CORO_TASK(void) shutdown() = 0;

    // Protocol negotiation
    virtual uint64_t get_protocol_version() const = 0;
    virtual encoding get_encoding() const = 0;
};

// Service proxy focuses on routing
class service_proxy : public i_marshaller {
    std::shared_ptr<i_transport> transport_;  // Delegates to transport

    // Routing logic only
    CORO_TASK(int) send(...) override {
        // Determine if local or remote
        // If remote, delegate to transport
        CO_RETURN CO_AWAIT transport_->send(...);
    }
};
```

#### Implementation Steps

1. **Define `i_transport` interface** (1 day)
   - Extract transport operations from `i_marshaller`
   - Define lifecycle methods (attach, detach, shutdown)
   - Add protocol negotiation methods

2. **Create base `transport` class** (2 days)
   - Implement common reference counting for service proxies
   - Provide default lifecycle management
   - Handle bidirectional symmetry

3. **Refactor SPSC to use new interface** (3 days)
   - Rename `channel_manager` → `spsc_transport`
   - Implement `i_transport` interface
   - Update `spsc_service_proxy` to use new abstraction
   - **Validate**: All SPSC tests pass

4. **Refactor Local transport** (2 days)
   - Create `local_transport` implementing `i_transport`
   - Direct function call implementation
   - Update `local_service_proxy`
   - **Validate**: All local tests pass

5. **Refactor TCP transport** (5 days)
   - Extract socket management into `tcp_transport`
   - Implement bidirectional send/receive
   - Handle connection lifecycle
   - **Validate**: All TCP tests pass

6. **Refactor SGX transport** (5 days)
   - Create `sgx_transport` with enclave boundary crossing
   - Implement secure marshalling
   - **Validate**: All SGX tests pass

**Total Effort**: ~18 days

**Risk**: Medium - Requires careful refactoring of existing transports

**Mitigation**: Incremental approach, one transport at a time, full test validation at each step

---

### Change 2: Implement Symmetrical Service Proxy Pairs

**Objective**: Eliminate asymmetric creation and ensure both directions exist simultaneously.

#### Current State
```cpp
// Zone A
auto proxy_a_to_b = service_a->connect_to_zone<...>(zone_b, ...);
// Zone B's reverse proxy created on-demand (RACE!)

// Later, in service.cpp:239
if (!opposite_direction_proxy) {
    // Create on-the-fly during send() - WORKAROUND!
    opposite_direction_proxy = temp->clone_for_zone(...);
}
```

#### Proposed State
```cpp
// Zone A creates connection
auto connection = service_a->connect_to_zone<transport_type>(
    zone_b,
    connection_params
);

// Internally, service creates:
// 1. Transport (bidirectional)
// 2. Service proxy A→B (using transport)
// 3. Service proxy B→A (using same transport)
// 4. Registers both proxies immediately

// Both zones now have symmetric service proxies
// Both share the same transport
// Both destroyed together when transport is destroyed
```

#### Implementation Steps

1. **Add `transport_pair` structure** (1 day)
   ```cpp
   struct transport_pair {
       std::shared_ptr<i_transport> transport;
       std::shared_ptr<service_proxy> forward_proxy;   // A→B
       std::shared_ptr<service_proxy> reverse_proxy;   // B→A

       zone adjacent_zone;  // Identifies the transport
   };
   ```

2. **Refactor `service::connect_to_zone()`** (3 days)
   - Create transport first
   - Create both service proxy directions
   - Register both in `other_zones` map
   - Return forward_proxy to caller
   - **Validate**: Connection establishment works

3. **Update `service::other_zones` map** (2 days)
   ```cpp
   // OLD: Map by {destination, caller}
   std::map<zone_route, std::weak_ptr<service_proxy>> other_zones;

   // NEW: Map by adjacent zone
   std::unordered_map<zone, transport_pair> transports_;

   // Lookup becomes simpler:
   auto found = transports_.find(destination_channel_zone);
   ```

4. **Update routing logic in `service::send()`** (4 days)
   - Remove Y-topology workaround (lines 239-250)
   - Simplify to direct transport lookup
   - Remove on-demand proxy creation
   ```cpp
   CORO_TASK(int) service::send(...) {
       // Look up transport by adjacent zone
       auto found = transports_.find(destination_channel_zone);
       if (found == transports_.end()) {
           CO_RETURN error_codes::no_route_to_destination;
       }

       auto& pair = found->second;
       CO_RETURN CO_AWAIT pair.forward_proxy->send(...);
   }
   ```

5. **Handle routing proxies** (3 days)
   - Service holds `shared_ptr` to routing proxies
   - Not destroyed by reference count = 0
   - Destroyed when service/transport destroyed
   ```cpp
   class service {
       // Routing proxies kept alive explicitly
       std::vector<std::shared_ptr<service_proxy>> routing_proxies_;
   };
   ```

6. **Update all `connect_to_zone()` call sites** (5 days)
   - Review all test code
   - Update connection setup
   - Remove workarounds and null checks
   - **Validate**: All tests pass

**Total Effort**: ~18 days

**Risk**: High - Fundamental change to service proxy creation

**Mitigation**: Feature flag to toggle new behavior, extensive testing, incremental rollout

---

### Change 3: Unified Transport Lifecycle Management

**Objective**: Clear ownership model and destruction order for transports.

#### Current State
```cpp
// Service proxy destroyed when aggregate ref count = 0
service_proxy::~service_proxy() {
    // May destroy transport even if opposite direction still alive!
}
```

#### Proposed State
```cpp
class transport {
    std::atomic<int> service_proxy_ref_count_{0};

    void attach_service_proxy() {
        ++service_proxy_ref_count_;
    }

    CORO_TASK(void) detach_service_proxy() {
        if (--service_proxy_ref_count_ == 0) {
            // Last service proxy detached
            CO_AWAIT shutdown();
            // Transport destroyed after this coroutine completes
        }
    }
};

class service_proxy {
    std::shared_ptr<transport> transport_;

    ~service_proxy() {
        // Schedule detach coroutine
        if (transport_) {
            auto detach_task = transport_->detach_service_proxy();
            service_->schedule_cleanup(std::move(detach_task));
        }
    }
};
```

#### Lifecycle Rules

**Rule 1: Transport Ownership**
- Transport owned by `shared_ptr` in both service proxy directions
- Transport destroyed when last `shared_ptr` released (ref_count = 0)
- Shutdown initiated when last service proxy detaches

**Rule 2: Service Proxy Destruction**
```
Service Proxy Destroyed When:
  - Routing Proxy: When service destroyed OR explicitly removed
  - Object Proxy: When aggregate ref count = 0 for all objects

Aggregate = sum of (shared_count + optimistic_count) across all object proxies
```

**Rule 3: Transport Destruction**
```
Transport Destroyed When:
  - Both service proxy directions have detached
  - All in-flight operations completed
  - Shutdown coroutine completes
```

**Rule 4: Asymmetric Destruction Handling**
```
Scenario: Service Proxy A→B destroyed, B→A still alive

Timeline:
T1: A→B destructor runs → detach_service_proxy()
T2: transport->service_proxy_ref_count_ decrements (2→1)
T3: Transport remains alive (ref_count = 1)
T4: B→A still functional, can send/receive
T5: Later, B→A destroyed → detach_service_proxy()
T6: transport->service_proxy_ref_count_ decrements (1→0)
T7: Transport shutdown initiated
T8: Both shared_ptrs released → transport destroyed
```

#### Implementation Steps

1. **Add reference counting to transport base class** (1 day)
   ```cpp
   class transport {
       std::atomic<int> service_proxy_ref_count_{0};
       std::atomic<int> operations_in_flight_{0};
   };
   ```

2. **Implement attach/detach pattern** (2 days)
   - `attach_service_proxy()` increments count
   - `detach_service_proxy()` decrements and checks for shutdown
   - Handle coroutine vs non-coroutine builds

3. **Update service_proxy destructor** (2 days)
   - Schedule `detach_service_proxy()` coroutine
   - Use `sync_wait` in non-coroutine build
   - Ensure proper cleanup scheduling

4. **Implement graceful shutdown** (3 days)
   - Wait for in-flight operations
   - Close connections
   - Release resources
   - Pattern from SPSC `channel_manager`

5. **Handle destruction in both coroutine modes** (3 days)
   ```cpp
   #ifdef BUILD_COROUTINE
       // Schedule cleanup coroutine
       service_->get_scheduler()->schedule(transport_->detach_service_proxy());
   #else
       // Synchronous cleanup
       sync_wait(transport_->detach_service_proxy());
   #endif
   ```

6. **Update test teardown sequences** (4 days)
   - Ensure proper shutdown ordering
   - Wait for all detach coroutines
   - Validate no leaks
   - **Validate**: Clean shutdown in all tests

**Total Effort**: ~15 days

**Risk**: Medium - Async destruction requires careful coordination

**Mitigation**: Leverage SPSC pattern (already working), extensive teardown testing

---

### Change 4: Eliminate Y-Topology Race Condition

**Objective**: Remove on-demand proxy creation workaround in `service.cpp:239-250`.

#### Current Workaround (TO BE REMOVED)
```cpp
// service.cpp:239-250
// NOTE there is a race condition that will be addressed in the
// coroutine changes coming later
//
// Force creation of opposite_direction_proxy during send()
if (auto found = other_zones.find({caller_zone_id.as_destination(),
                                   destination_zone_id.as_caller()});
    found != other_zones.end())
{
    opposite_direction_proxy = found->second.lock();
    if (!opposite_direction_proxy)
    {
        // Service proxy was destroyed - must clone on-the-fly
        opposite_direction_proxy = temp->clone_for_zone(...);
        inner_add_zone_proxy(opposite_direction_proxy);
    }
}
```

#### Root Cause Analysis

**Y-Topology Scenario**:
```
Root (Zone 1)
 ├─ Prong A (Zone 2) [created by Root]
 └─ Prong B (Zone 3) [created by Prong A, unknown to Root]

Problem Timeline:
1. Prong A creates Prong B → creates service proxy 2→3
2. Prong B creates object, returns to Prong A
3. Prong A returns object to Root
4. Root has object from Zone 3 but NO service proxy 1→3!
5. Root tries add_ref on object in Zone 3
6. RACE: Must create service proxy 1→3 on-the-fly
```

**Why Current Workaround Fails**:
- Creates service proxy during `send()` (critical path)
- Race window between creation and `add_external_ref()`
- Null pointer checks scattered throughout
- Violates single responsibility

#### Proposed Solution: Proactive Proxy Notification

**Approach**: When intermediate zone creates new child zone, notify parent zones.

```cpp
// When Zone 2 (Prong A) creates Zone 3 (Prong B):
CORO_TASK(error_code) service::connect_to_zone(...) {
    // 1. Create transport and service proxy pair for 2↔3
    auto pair = create_transport_pair<transport_type>(zone_3, ...);

    // 2. Notify parent zones of new route
    CO_AWAIT notify_parent_of_new_route(zone_3, known_direction_zone{zone_2});

    CO_RETURN error_codes::ok;
}

// Root receives notification
CORO_TASK(void) service::handle_new_route_notification(
    destination_zone new_zone,
    known_direction_zone via_zone
) {
    // Create routing proxy 1→3 (via zone 2)
    auto routing_proxy = create_routing_proxy(new_zone, via_zone);

    // Hold routing proxy alive (not reference counted)
    routing_proxies_.push_back(routing_proxy);

    // Register for routing lookups
    register_route(new_zone, via_zone, routing_proxy);
}
```

**Benefits**:
- Proactive creation, no critical-path allocation
- No race conditions
- Routing proxies explicitly managed
- Clear lifecycle (destroyed with service)

#### Implementation Steps

1. **Define route notification protocol** (2 days)
   ```cpp
   // New RPC operation in base service interface
   CORO_TASK(void) notify_new_route(destination_zone new_zone,
                                     known_direction_zone via_zone);
   ```

2. **Implement notification propagation** (3 days)
   - When zone creates child, notify all ancestors
   - Propagate up the zone hierarchy
   - Handle multiple paths (graph, not tree)

3. **Create routing proxy on notification** (3 days)
   - Allocate routing service_proxy
   - Configure for multi-hop routing
   - Store in `routing_proxies_` (held alive by service)

4. **Update routing table** (2 days)
   - Register route in service's routing map
   - Handle route updates (new paths discovered)
   - Remove routes when zones destroyed

5. **Remove Y-topology workaround** (1 day)
   - Delete code at `service.cpp:239-250`
   - Remove null pointer checks
   - Simplify send() logic

6. **Validate Y-topology tests** (4 days)
   - `test_y_topology_and_return_new_prong_object`
   - `test_y_topology_and_cache_and_retrieve_prong_object`
   - `test_y_topology_and_set_host_with_prong_object`
   - **Validate**: All tests pass WITHOUT workaround

**Total Effort**: ~15 days

**Risk**: High - Complex distributed protocol

**Mitigation**: Incremental testing, keep workaround until validation complete, feature flag

---

### Change 5: Simplify Service Proxy Responsibilities

**Objective**: Reduce service_proxy.cpp from ~800 LOC to ~400 LOC by extracting transport.

#### Current Responsibilities (Too Many)
```cpp
class service_proxy {
    // 1. Routing logic
    // 2. Transport logic (serialization, I/O)
    // 3. Lifecycle management
    // 4. Protocol negotiation
    // 5. Object proxy aggregation
    // 6. Reference counting
};
```

#### Proposed Responsibilities (Focused)
```cpp
class service_proxy {
    // 1. Routing logic ONLY
    // 2. Object proxy aggregation
    // 3. Delegation to transport

    std::shared_ptr<i_transport> transport_;  // Handles 2, 4 from old list
    // Service owns lifecycle (3, 6 from old list)
};
```

#### Code Reduction Targets

**Before**:
- `service_proxy.cpp`: ~800 LOC
- `service.cpp`: ~1900 LOC

**After**:
- `service_proxy.cpp`: ~400 LOC (50% reduction)
- `transport.cpp`: ~300 LOC (new base class)
- `spsc_transport.cpp`: ~500 LOC (extracted from channel_manager)
- `local_transport.cpp`: ~200 LOC (extracted from local_service_proxy)
- `tcp_transport.cpp`: ~600 LOC (extracted from tcp_service_proxy)
- `service.cpp`: ~1400 LOC (25% reduction - simpler routing)

**Total LOC**: Similar, but better organized and testable

#### Implementation Steps

1. **Extract common transport operations** (3 days)
   - Serialization helpers
   - Protocol negotiation
   - Version/encoding fallback
   - Move to `transport` base class

2. **Move protocol negotiation to transport** (2 days)
   ```cpp
   class transport {
       uint64_t protocol_version_;
       encoding encoding_;

       CORO_TASK(error_code) negotiate_protocol();
   };
   ```

3. **Simplify service_proxy::send()** (2 days)
   ```cpp
   CORO_TASK(int) service_proxy::send(...) {
       // Just delegate to transport
       CO_RETURN CO_AWAIT transport_->send(...);
   }
   ```

4. **Move serialization to transport** (3 days)
   - Encoding/decoding logic
   - Buffer management
   - Compression (if applicable)

5. **Update all transport implementations** (5 days)
   - SPSC, Local, TCP, SGX
   - Move logic from service_proxy subclasses
   - **Validate**: All transports work

6. **Simplify service.cpp routing** (4 days)
   - Remove on-demand creation
   - Simplify lookup (adjacent zone only)
   - Remove zone_route complexity
   - **Validate**: Routing tests pass

**Total Effort**: ~19 days

**Risk**: Medium - Large refactoring

**Mitigation**: Incremental extraction, continuous testing

---

### Change 6: Improve Type System Clarity

**Objective**: Rename confusing types and clarify transport identification.

#### Current Issues

**Problem 1**: `destination_channel_zone` has misleading comment
```cpp
// types.h:126
// the zone that a service proxy was cloned from  ← MISLEADING!
struct destination_channel_zone {
    // Actually: adjacent zone toward destination
};
```

**Problem 2**: "channel" terminology conflicts with "transport"
- Some code uses "channel"
- Some documentation says "channel is the transport"
- Inconsistent naming

#### Proposed Changes

**Option A: Rename channel types** (Conservative)
```cpp
// Rename existing types for clarity
using transport_zone = destination_channel_zone;
using caller_transport_zone = caller_channel_zone;

// Update all usage sites
// Maintains binary compatibility
```

**Option B: Introduce adjacent_zone type** (Clean)
```cpp
// New type for transport identification
struct adjacent_zone {
    zone zone_id;

    // Clearly identifies immediate neighbor for transport
};

// Update function signatures
CORO_TASK(int) send(destination_zone dest,
                    caller_zone caller,
                    adjacent_zone next_hop,  // Clear intent
                    ...);
```

**Option C: Hybrid approach** (Recommended)
```cpp
// Keep existing types for compatibility
// Add clear aliases
using transport_id = destination_channel_zone;  // Adjacent zone identifying transport
using reverse_transport_id = caller_channel_zone;  // Adjacent zone on caller side

// Update comments
// the adjacent zone toward destination (identifies transport)
struct destination_channel_zone {
    zone val_;
};
```

#### Implementation Steps

1. **Update type comments** (1 day)
   - Fix misleading comment in `types.h:126`
   - Add clear explanation of adjacent vs ultimate destination
   - Document usage in multi-hop routing

2. **Add type aliases** (1 day)
   ```cpp
   // types.h
   using transport_id = destination_channel_zone;
   using reverse_transport_id = caller_channel_zone;
   ```

3. **Update documentation** (2 days)
   - User guide examples
   - Code comments
   - Design documents
   - Make "adjacent zone" concept explicit

4. **Consider gradual migration** (future work)
   - Could migrate to `adjacent_zone` type in v4
   - Maintain compatibility in v3

**Total Effort**: ~4 days

**Risk**: Low - Mostly documentation

**Mitigation**: Conservative approach, keep existing types

---

### Change 7: Service Sink Architecture (Alternative/Complementary Approach)

**Objective**: Introduce `service_sink` as a receiver-side counterpart to `service_proxy`, providing a unified architecture for both direct local calls and channel-based async communication.

#### Motivation

The current architecture uses a sender/receiver design pattern suitable for async channel-based calls (SPSC, TCP, Enclave-coroutine). However, this pattern may not always be necessary for direct local calls. The **service_sink** concept provides a unified receiver-side abstraction that can:

1. **Receive demarshalled calls from channels** and pass them to the local service
2. **Enable pass-through routing** by holding a service_proxy to another zone
3. **Simplify reference counting** for bidirectional pass-through connections
4. **Provide a single concrete class** rather than multiple derived types

#### Conceptual Architecture

**Naming Convention**:
- `service_proxy (X→Y)` = sends TO Zone Y
- `service_sink (X→Y)` = receives FROM Zone X

```
Zone A ←→ Zone C Pass-Through via Zone B:

In Zone B (the intermediary), the 4-object circular chain:

  service_sink (A→C) [receives from A]
    ↓ holds shared_ptr/weak_ptr
  service_proxy (A→C) [sends to C]
    ↓ owns shared_ptr
  service_sink (C→A) [receives from C]
    ↓ holds shared_ptr/weak_ptr
  service_proxy (C→A) [sends to A]
    ↓ owns shared_ptr
  service_sink (A→C) ← back to start (circular)

Each service_proxy has a channel/transport to the respective zone:
  - proxy (A→C) has channel to Zone C
  - proxy (C→A) has channel to Zone A

When counts > 0: sink→proxy links are shared_ptr (cycle complete)
When counts == 0: sink→proxy links are weak_ptr (cycle breaks)
```

#### Proposed Design

**service_sink Structure**:
```cpp
class service_sink {
private:
    // Always present: reference to local service
    std::shared_ptr<rpc::service> service_;

    // For pass-through routing: smart pointers to opposite service_proxy
    std::shared_ptr<service_proxy> opposite_proxy_;  // Strong ref when counts > 0
    std::weak_ptr<service_proxy> opposite_weak_ptr_; // Weak ref always held

    // Reference counting state
    std::atomic<uint64_t> shared_count_{0};
    std::atomic<uint64_t> optimistic_count_{0};

public:
    service_sink(std::shared_ptr<rpc::service> service)
        : service_(std::move(service))
    {}

    // Receive demarshalled call from channel/transport
    CORO_TASK(void) receive_call(const marshalled_message& msg);

    // Configure as pass-through to another zone
    void set_pass_through(std::shared_ptr<service_proxy> opposite_proxy) {
        opposite_weak_ptr_ = opposite_proxy;
        opposite_proxy_ = nullptr;  // Initially null
        update_opposite_proxy_state();
    }

    // Update reference counts (called when object refs change)
    void update_counts(uint64_t shared_count, uint64_t optimistic_count) {
        shared_count_ = shared_count;
        optimistic_count_ = optimistic_count;
        update_opposite_proxy_state();
    }

private:
    // Promote weak_ptr to shared_ptr when counts > 0
    void update_opposite_proxy_state() {
        uint64_t total_count = shared_count_.load() + optimistic_count_.load();

        if (total_count > 0) {
            // Active references exist - hold strong reference
            if (!opposite_proxy_) {
                opposite_proxy_ = opposite_weak_ptr_.lock();
            }
        } else {
            // No active references - release strong reference
            opposite_proxy_.reset();
        }
    }

    // Dispatch call to local service or pass through to opposite proxy
    CORO_TASK(void) dispatch_call(const marshalled_message& msg) {
        if (opposite_proxy_) {
            // Pass-through mode: forward to opposite service proxy
            CO_AWAIT opposite_proxy_->send(msg);
        } else {
            // Local mode: dispatch to local service's object stub
            // All channel traffic is delivered to the service for processing
            CO_AWAIT service_->dispatch_to_stub(msg);
        }
    }
};
```

**service_proxy Integration**:
```cpp
class service_proxy {
    // Transport for sending messages
    std::shared_ptr<i_transport> transport_;

    // Sink for receiving messages (shares same channel/transport)
    std::shared_ptr<service_sink> sink_;

public:
    service_proxy(std::shared_ptr<i_transport> transport,
                  std::shared_ptr<service_sink> sink)
        : transport_(std::move(transport))
        , sink_(std::move(sink))
    {}

    std::shared_ptr<service_sink> get_sink() const {
        return sink_;
    }
};
```

**Service Lifecycle Management**:
```cpp
class service {
    // Service must keep service_proxy alive for non-pass-through sinks
    // because there's no countervailing circular reference to keep the proxy alive
    std::vector<std::shared_ptr<service_proxy>> active_service_proxies_;

    void register_service_proxy(std::shared_ptr<service_proxy> proxy) {
        // Hold service_proxy alive
        // For pass-through: circular dependency keeps proxies alive
        // For local dispatch: service must hold shared_ptr
        active_service_proxies_.push_back(proxy);
    }

    void unregister_service_proxy(std::shared_ptr<service_proxy> proxy) {
        // Remove when connection is closed or zone destroyed
        auto it = std::find(active_service_proxies_.begin(),
                           active_service_proxies_.end(),
                           proxy);
        if (it != active_service_proxies_.end()) {
            active_service_proxies_.erase(it);
        }
    }
};
```

#### Service Sink Operational Modes

**Mode 1: Local Dispatch (Non-Pass-Through)**
```cpp
// Sink delivers all channel traffic to the local service
service_sink sink(service_ptr);
// opposite_proxy_ is nullptr - not configured for pass-through

// When messages arrive:
sink.receive_call(msg);  // → dispatches to service_->dispatch_to_stub(msg)

// Lifecycle: Service MUST hold shared_ptr to service_proxy
// - No circular dependency to keep proxy alive
// - Service keeps proxy in active_service_proxies_ vector
// - Proxy destroyed when service unregisters it or service destroyed
```

**Mode 2: Pass-Through Routing**
```cpp
// Sink forwards traffic to opposite service_proxy
service_sink sink(service_ptr);
sink.set_pass_through(opposite_proxy);  // Configure pass-through

// When messages arrive:
sink.receive_call(msg);  // → forwards to opposite_proxy->send(msg)

// Lifecycle: Circular dependency keeps proxies alive
// - When counts > 0: circular shared_ptr chain exists
// - When counts == 0: chain breaks, all objects can be destroyed
// - Service may still hold shared_ptr for routing purposes
```

#### Bidirectional Pass-Through Lifecycle

**Scenario**: Zone A ←→ Zone C connection passing through Zone B

**Initialization**:
```cpp
// In Zone B (intermediary):
// 1. service_proxy (A→C) [sends to C] owns service_sink (C→A) [receives from C]
auto proxy_a_to_c = ...;  // sends to Zone C
auto sink_c_to_a = proxy_a_to_c->get_sink();  // receives from Zone C

// 2. service_proxy (C→A) [sends to A] owns service_sink (A→C) [receives from A]
auto proxy_c_to_a = ...;  // sends to Zone A
auto sink_a_to_c = proxy_c_to_a->get_sink();  // receives from Zone A

// 3. Configure bidirectional pass-through:
// When receiving from C (sink C→A), forward by sending to A (proxy C→A)
sink_c_to_a->set_pass_through(proxy_c_to_a);

// When receiving from A (sink A→C), forward by sending to C (proxy A→C)
sink_a_to_c->set_pass_through(proxy_a_to_c);
```

**Reference Counting Behavior**:

**State 1: Active communication (shared_count > 0 or optimistic_count > 0 in either direction)**
```
service_sink (A→C) [receives from A]
  ↓ holds shared_ptr (count > 0)
service_proxy (A→C) [sends to C]
  ↓ owns shared_ptr
service_sink (C→A) [receives from C]
  ↓ holds shared_ptr (count > 0)
service_proxy (C→A) [sends to A]
  ↓ owns shared_ptr
service_sink (A→C) ← circular dependency! (4-object cycle)

Result: All 4 objects kept alive by circular shared_ptr chain
```

**State 2: Idle state (both shared_count == 0 AND optimistic_count == 0 in both directions)**
```
service_sink (A→C) [receives from A]
  ↓ holds weak_ptr only (counts = 0)
  ✗ no shared_ptr to service_proxy (A→C)

service_proxy (A→C) [sends to C]
  ↓ owns shared_ptr
service_sink (C→A) [receives from C]
  ↓ holds weak_ptr only (counts = 0)
  ✗ no shared_ptr to service_proxy (C→A)

service_proxy (C→A) [sends to A]
  ↓ owns shared_ptr
service_sink (A→C) ✗ cycle broken!

Result: No circular dependency, all shared_ptrs can be released
```

**Cleanup Sequence**:
```
1. Last object reference released in both directions
2. Both sinks detect counts == 0 and drop their shared_ptr to opposite service_proxy
   - sink (A→C) releases shared_ptr to proxy (A→C)
   - sink (C→A) releases shared_ptr to proxy (C→A)
3. 4-object circular dependency broken (both weak links in the chain)
4. service_proxy reference counts drop to zero (no external holders)
5. Both service_proxy destructors run
6. Both sinks destroyed (owned by their respective service_proxy)
7. Sinks notify service of their demise during destruction
8. Service cleans up routing entries
```

#### Integration with Transport Layer

**All service proxies use service_sinks**:
- **Local service proxy**: Sink receives direct function calls (no channel)
- **SPSC service proxy**: Sink receives from SPSC queue receiver
- **TCP service proxy**: Sink receives from socket receiver task
- **Enclave service proxy**:
  - Sync mode: Sink receives from ecall/ocall (no channel)
  - Coroutine mode: Sink receives from SPSC queue receiver

**Channel-based transports**:
```cpp
class spsc_transport : public i_transport {
    // Bidirectional SPSC queues
    spsc_queue* send_queue_;     // This zone → Remote zone
    spsc_queue* receive_queue_;  // Remote zone → This zone

    // Receiver pump task delivers to sink
    CORO_TASK(void) receive_pump_task() {
        while (running_) {
            marshalled_message msg;
            CO_AWAIT receive_queue_->dequeue(msg);

            // Deliver to sink for processing
            CO_AWAIT sink_->receive_call(msg);
        }
    }

    std::shared_ptr<service_sink> sink_;
};
```

**Local direct calls**:
```cpp
class local_service_proxy : public service_proxy {
    CORO_TASK(int) send(const marshalled_message& msg) override {
        // No channel - directly call opposite sink
        auto opposite_sink = get_opposite_sink();
        CO_AWAIT opposite_sink->receive_call(msg);
        CO_RETURN error_codes::ok;
    }
};
```

#### Advantages of Service Sink Architecture

**1. Uniform Design**:
- All service proxies use sinks (even local ones)
- Single concrete `service_sink` class, not multiple derived types
- Consistent architecture regardless of transport type

**2. Dual-Mode Operation**:
- **Local Dispatch Mode**: Sink delivers all channel traffic to the service for processing
- **Pass-Through Mode**: Sink forwards traffic to opposite service_proxy
- Single abstraction handles both use cases

**3. Simplified Pass-Through Routing**:
- Sink naturally handles both local dispatch and pass-through
- Reference counting automatically manages circular dependencies
- No special-case code for routing intermediaries

**4. Clear Lifecycle Ownership**:
- **Local Mode**: Service holds shared_ptr to keep service_proxy alive (no countervailing circular reference)
- **Pass-Through Mode**: Circular dependency keeps proxies alive when counts > 0, automatically releases when idle
- Service tracks all proxies for proper cleanup

**5. Automatic Lifecycle Management**:
- Circular dependency exists only when actively in use
- Automatically breaks when idle
- Service can track all sinks for cleanup

**6. Separation of Concerns**:
```
service_proxy:  Sending calls (uses transport)
service_sink:   Receiving calls (from transport or direct)
transport:      Bidirectional communication channel
service:        Manages sinks and overall lifecycle
```

**7. Cleaner Service Tracking**:
```cpp
class service {
    // Track all sinks that belong to this service
    std::vector<std::weak_ptr<service_sink>> sinks_;

    void register_sink(std::shared_ptr<service_sink> sink) {
        sinks_.push_back(sink);
    }

    // Interrogate sinks from related service_proxy
    std::shared_ptr<service_sink> find_sink_for_proxy(
        const std::shared_ptr<service_proxy>& proxy
    ) {
        return proxy->get_sink();
    }
};
```

#### Implementation Considerations

**Question 1**: Does service_sink position sit between channel receiver and object stub?
- **Answer**: Yes (Option 2). Channel receiver → service_sink → existing stub logic (or pass-through to another service_proxy). Sink handles routing/lifecycle, stub handles method dispatch.

**Question 2**: Pass-through topology confirmation?
- **Answer**: Yes. For A→B→C routing where B is the working service, the 4-object circular chain is:
  1. **sink (A→C)** [receives FROM A] holds shared_ptr/weak_ptr to...
  2. **proxy (A→C)** [sends TO C] owns shared_ptr to...
  3. **sink (C→A)** [receives FROM C] holds shared_ptr/weak_ptr to...
  4. **proxy (C→A)** [sends TO A] owns shared_ptr to...
  5. **sink (A→C)** ← back to start (4-object cycle)

  When counts > 0, the sink→proxy links become shared_ptr (cycle complete).
  When counts == 0, the sink→proxy links become weak_ptr only (cycle breaks).

**Question 3**: Count types for sink shared_ptr management?
- **Answer**: Yes (shared + optimistic counts). When both `shared_count` and `optimistic_count` reach zero on the pass-through service_proxy, the sink's `shared_ptr` to the opposite service_proxy is set to null.

**Question 4**: Do local calls use sinks?
- **Answer**: Always use sink. Even `local_service_proxy` has a `service_sink`, just without a channel in between. Provides uniform architecture.

#### Relationship to Other Proposed Changes

**Complementary to Transport Abstraction (Change 1)**:
- service_sink sits on receiver side
- `i_transport` provides bidirectional send/receive
- Sink is the consumer of transport's receive operations

**Enables Symmetrical Service Proxy Pairs (Change 2)**:
- Both proxy directions have corresponding sinks
- Sinks enable proper bidirectional lifecycle management
- Pass-through logic naturally implements routing proxies

**Simplifies Y-Topology Fix (Change 4)**:
- Pass-through sinks handle routing automatically
- No special notification protocol needed (alternative approach)
- Service can query sinks to understand topology

#### Implementation Effort Estimate

**Phase 1: Core service_sink class** (1 week)
- Define `service_sink` interface
- Implement reference counting logic
- Implement pass-through proxy management
- Unit tests for sink lifecycle

**Phase 2: Integration with transports** (2 weeks)
- Update SPSC to use sinks
- Update local service proxy to use sinks
- Update TCP and enclave proxies
- Validation tests

**Phase 3: Pass-through routing** (2 weeks)
- Implement bidirectional sink configuration
- Test circular dependency management
- Validate cleanup sequences
- Y-topology tests

**Phase 4: Service integration** (1 week)
- Service sink registration/tracking
- Service cleanup on sink destruction
- Documentation

**Total Effort**: ~6 weeks (30 days)

**Risk**: Medium
- New abstraction layer
- Circular dependency management requires careful testing
- Integration with existing transport layer

**Mitigation**:
- Incremental implementation
- Extensive unit testing of sink lifecycle
- Feature flag for gradual rollout
- Can be implemented alongside or instead of route notification protocol

---

## Migration Strategy

### Phase 0: Preparation (Week 1-2)

**Objective**: Set up infrastructure for safe migration.

#### Actions

1. **Create feature flags** (2 days)
   ```cpp
   // build_config.h
   #define RPC_USE_NEW_TRANSPORT_ABSTRACTION 0
   #define RPC_USE_SYMMETRICAL_PROXIES 0
   #define RPC_USE_PROACTIVE_ROUTING 0
   ```

2. **Establish baseline metrics** (3 days)
   - Run full test suite, record timings
   - Measure code coverage
   - Profile memory usage
   - Document current LOC metrics

3. **Create parallel implementation structure** (3 days)
   ```
   /rpc/include/rpc/internal/v3/
       transport.h          (new)
       service_proxy_v3.h   (new)
   /rpc/src/v3/
       transport.cpp        (new)
       service_proxy_v3.cpp (new)
   ```

4. **Set up continuous integration** (2 days)
   - Run tests on every commit
   - Automated LOC tracking
   - Memory leak detection
   - Performance regression detection

**Deliverables**:
- Feature flag framework
- Baseline metrics document
- Parallel implementation structure
- CI pipeline configured

---

### Phase 1: Transport Abstraction (Week 3-6)

**Objective**: Extract `i_transport` interface and refactor SPSC.

#### Week 3: Interface Definition

**Day 1-2**: Define `i_transport` interface
```cpp
class i_transport {
public:
    virtual ~i_transport() = default;

    virtual CORO_TASK(int) send(...) = 0;
    virtual CORO_TASK(int) receive(...) = 0;
    virtual CORO_TASK(void) attach_service_proxy() = 0;
    virtual CORO_TASK(void) detach_service_proxy() = 0;
    virtual CORO_TASK(void) shutdown() = 0;
    virtual uint64_t get_protocol_version() const = 0;
    virtual encoding get_encoding() const = 0;
};
```

**Day 3-4**: Create `transport` base class
```cpp
class transport : public i_transport {
    std::atomic<int> service_proxy_ref_count_{0};
    uint64_t protocol_version_;
    encoding encoding_;

    // Common implementation
};
```

**Day 5**: Unit tests for base class
- Reference counting
- Lifecycle management
- Protocol negotiation

#### Week 4: SPSC Refactoring

**Day 1-3**: Rename `channel_manager` → `spsc_transport`
- Implement `i_transport` interface
- Keep existing functionality
- Update includes and namespaces

**Day 4-5**: Update `spsc_service_proxy`
```cpp
class spsc_service_proxy : public service_proxy {
    std::shared_ptr<spsc_transport> transport_;

    CORO_TASK(int) send(...) override {
        CO_RETURN CO_AWAIT transport_->send(...);
    }
};
```

#### Week 5-6: Validation

**Day 1-5**: Run all SPSC tests
- Unit tests
- Integration tests
- Fuzz tests
- Measure performance

**Day 6-10**: Refactor Local and TCP transports
- Create `local_transport`
- Create `tcp_transport`
- Update service proxies
- Validate all tests

**Deliverables**:
- `i_transport` interface
- `transport` base class
- `spsc_transport` (refactored from channel_manager)
- `local_transport`, `tcp_transport`
- All tests passing

**Gate**: All existing tests pass with new transport abstraction before proceeding.

---

### Phase 2: Symmetrical Service Proxies (Week 7-10)

**Objective**: Implement bidirectional proxy pairs with shared transport.

#### Week 7: Data Structure Changes

**Day 1-2**: Add `transport_pair` structure
```cpp
struct transport_pair {
    std::shared_ptr<i_transport> transport;
    std::shared_ptr<service_proxy> forward_proxy;
    std::shared_ptr<service_proxy> reverse_proxy;
    zone adjacent_zone;
};
```

**Day 3-5**: Refactor `service::other_zones` map
```cpp
// OLD
std::map<zone_route, std::weak_ptr<service_proxy>> other_zones;

// NEW
std::unordered_map<zone, transport_pair> transports_;
```

#### Week 8: Connection Establishment

**Day 1-3**: Refactor `service::connect_to_zone()`
- Create transport first
- Create both service proxy directions
- Register both proxies
- Return forward proxy

**Day 4-5**: Handle routing proxies
- Service holds `shared_ptr` to routing proxies
- Not destroyed by ref count = 0
- Explicit lifecycle management

#### Week 9-10: Integration and Testing

**Day 1-5**: Update all connection sites
- Test code
- Example code
- Documentation

**Day 6-10**: Validation
- All tests pass
- No new race conditions
- Performance acceptable

**Deliverables**:
- `transport_pair` implementation
- Refactored `connect_to_zone()`
- Updated routing proxy management
- All tests passing

**Gate**: Can create symmetrical proxy pairs, all tests pass, before proceeding.

---

### Phase 3: Y-Topology Fix (Week 11-14)

**Objective**: Remove on-demand proxy creation workaround.

#### Week 11: Route Notification Protocol

**Day 1-2**: Define notification interface
```cpp
CORO_TASK(void) notify_new_route(destination_zone new_zone,
                                  known_direction_zone via_zone);
```

**Day 3-5**: Implement propagation logic
- Notify ancestors when child created
- Propagate up hierarchy
- Handle cycles/graphs

#### Week 12: Routing Proxy Creation

**Day 1-3**: Create routing proxies on notification
- Allocate proxy
- Configure for multi-hop
- Store in service

**Day 4-5**: Update routing table
- Register route
- Handle updates
- Remove on destroy

#### Week 13: Remove Workaround

**Day 1**: Delete `service.cpp:239-250`

**Day 2-5**: Simplify routing logic
- Remove null checks
- Simplify send()
- Clean up error paths

#### Week 14: Validation

**Day 1-5**: Y-topology tests
- `test_y_topology_and_return_new_prong_object`
- `test_y_topology_and_cache_and_retrieve_prong_object`
- `test_y_topology_and_set_host_with_prong_object`

**Day 6-10**: Full regression testing
- All test suites
- Performance benchmarks
- Memory leak checks

**Deliverables**:
- Route notification protocol
- Proactive routing proxy creation
- Y-topology workaround removed
- All Y-topology tests passing

**Gate**: All Y-topology tests pass WITHOUT workaround before proceeding.

---

### Phase 4: Code Simplification (Week 15-17)

**Objective**: Reduce complexity and LOC in service_proxy and service.

#### Week 15: Extract to Transport

**Day 1-2**: Move protocol negotiation to transport

**Day 3-5**: Move serialization to transport

#### Week 16: Simplify Service Proxy

**Day 1-3**: Reduce service_proxy.cpp to ~400 LOC

**Day 4-5**: Update all transport implementations

#### Week 17: Simplify Service

**Day 1-3**: Simplify routing logic in service.cpp

**Day 4-5**: Final validation and cleanup

**Deliverables**:
- service_proxy.cpp: ~400 LOC (from ~800)
- service.cpp: ~1400 LOC (from ~1900)
- Improved test coverage
- All tests passing

---

### Phase 5: Documentation and Rollout (Week 18-20)

**Objective**: Update documentation, finalize changes, prepare for release.

#### Week 18: Documentation

**Day 1-3**: Update user guide
- New transport abstraction
- Symmetrical proxy pairs
- Routing proxy management

**Day 4-5**: Update code documentation
- API reference
- Design documents
- Architecture diagrams

#### Week 19: Performance Validation

**Day 1-3**: Benchmark suite
- Compare before/after
- Identify regressions
- Optimize hot paths

**Day 4-5**: Memory profiling
- Check for leaks
- Measure overhead
- Validate scaling

#### Week 20: Final Validation and Release

**Day 1-2**: Code review
- Security review
- Thread safety review
- Performance review

**Day 3-4**: Integration testing
- Full test suite multiple times
- Stress testing
- Fuzz testing

**Day 5**: Release preparation
- Update CHANGELOG
- Version bump
- Tag release

**Deliverables**:
- Updated documentation
- Performance report
- Release notes
- Version 3.0 release

---

## Risk Analysis

### High Risk Items

#### Risk 1: Symmetrical Proxy Creation Breaks Existing Code

**Description**: Creating both proxy directions simultaneously may break assumptions in existing code.

**Probability**: Medium
**Impact**: High
**Mitigation**:
- Feature flag to toggle new behavior
- Incremental rollout
- Extensive testing before enabling
- Keep old code path available for rollback

**Contingency**: Revert to asymmetric creation if critical bugs found.

---

#### Risk 2: Route Notification Protocol Introduces New Race Conditions

**Description**: Distributed notification propagation may have edge cases with concurrent operations.

**Probability**: Medium
**Impact**: High
**Mitigation**:
- Formal protocol specification
- State machine design
- Extensive concurrent testing
- Fuzz testing with multiple zones

**Contingency**: Keep Y-topology workaround as fallback until validation complete.

---

#### Risk 3: Transport Lifecycle Deadlocks in Edge Cases

**Description**: Async destruction with coroutines may deadlock under specific conditions.

**Probability**: Low
**Impact**: High
**Mitigation**:
- Use proven SPSC pattern
- Extensive teardown testing
- Timeout mechanisms
- Deadlock detection in tests

**Contingency**: Add explicit shutdown ordering, timeout fallbacks.

---

### Medium Risk Items

#### Risk 4: Performance Regression from Increased Indirection

**Description**: Extra layer (transport) may slow down hot paths.

**Probability**: Medium
**Impact**: Medium
**Mitigation**:
- Benchmark before/after
- Optimize common paths
- Consider inline hints
- Profile-guided optimization

**Contingency**: Accept small overhead for maintainability, or optimize critical paths.

---

#### Risk 5: Test Suite Doesn't Catch All Race Conditions

**Description**: Race conditions are hard to reproduce, may slip through testing.

**Probability**: High
**Impact**: Medium
**Mitigation**:
- Extensive fuzz testing
- Thread sanitizer
- Stress testing
- Longer soak tests

**Contingency**: Incremental rollout, monitor production, fast rollback mechanism.

---

### Low Risk Items

#### Risk 6: Documentation Becomes Outdated

**Description**: Code changes faster than documentation updates.

**Probability**: High
**Impact**: Low
**Mitigation**:
- Documentation in code reviews
- Automated API doc generation
- Examples in tests

**Contingency**: Dedicated documentation sprint before release.

---

## Success Criteria

### Functional Requirements

✅ **FR1**: Y-topology tests pass WITHOUT workaround at `service.cpp:239`

✅ **FR2**: All service proxies use transport abstraction (SPSC, Local, TCP, SGX)

✅ **FR3**: Service proxy pairs are created symmetrically and share transport

✅ **FR4**: Transport destroyed only when both directions detached

✅ **FR5**: Routing proxies have explicit lifecycle (not ref-counted to zero)

✅ **FR6**: No on-demand service proxy creation in hot paths (send, add_ref, release)

### Non-Functional Requirements

✅ **NFR1**: Code complexity reduction
- `service_proxy.cpp`: ≤450 LOC (from ~800)
- `service.cpp`: ≤1500 LOC (from ~1900)

✅ **NFR2**: Performance
- No more than 5% overhead on send() operation
- No memory leaks (valgrind clean)
- Scale to 1000+ zones

✅ **NFR3**: Test coverage
- All existing tests pass
- Code coverage ≥85%
- No new race conditions detected by thread sanitizer

✅ **NFR4**: Documentation
- User guide updated with new patterns
- API documentation complete
- Migration guide for v2→v3

### Validation Criteria

**Phase 1 Gate**: Transport abstraction
- [ ] All SPSC tests pass with `spsc_transport`
- [ ] Local and TCP refactored
- [ ] No performance regression

**Phase 2 Gate**: Symmetrical proxies
- [ ] Can create proxy pairs successfully
- [ ] Both directions share transport
- [ ] All connection tests pass

**Phase 3 Gate**: Y-topology fix
- [ ] All Y-topology tests pass without workaround
- [ ] Route notification works correctly
- [ ] No new race conditions

**Phase 4 Gate**: Code simplification
- [ ] LOC targets met
- [ ] Test coverage maintained
- [ ] All tests pass

**Final Release Gate**:
- [ ] All functional requirements met
- [ ] All non-functional requirements met
- [ ] All phase gates passed
- [ ] Documentation complete
- [ ] Performance validated
- [ ] Security review complete

---

## Appendix A: Code Examples

### Example 1: Creating Symmetrical Proxy Pairs

#### Before (Asymmetric)
```cpp
// Zone A
auto service_a = std::make_shared<rpc::service>("host", rpc::zone{1});

// Create forward proxy A→B
auto proxy_a_to_b = service_a->connect_to_zone<local_service_proxy>(
    rpc::zone{2},
    service_b
);

// Reverse proxy B→A created on-demand later (RACE!)
```

#### After (Symmetric)
```cpp
// Zone A
auto service_a = std::make_shared<rpc::service>("host", rpc::zone{1});

// Create bidirectional connection A↔B
auto connection = service_a->connect_to_zone<local_transport>(
    rpc::zone{2},
    connection_params{service_b}
);

// Internally creates:
// - local_transport (bidirectional)
// - service_proxy A→B (using transport)
// - service_proxy B→A (using transport)
// Both proxies registered immediately in both services
```

### Example 2: Transport Lifecycle

#### Current (Problematic)
```cpp
// Service proxy A→B has objects
// Service proxy B→A has no objects → destroyed

// Later, Zone B tries to call Zone A
// ERROR: No service proxy B→A!
```

#### Proposed (Correct)
```cpp
// Service proxy A→B has objects (aggregate = 5)
service_proxy_a_to_b->~service_proxy() {
    // Will be called when aggregate ref count = 0
    transport_->detach_service_proxy();  // Decrement (2→1)
    // Transport stays alive
}

// Service proxy B→A has no objects (aggregate = 0)
service_proxy_b_to_a->~service_proxy() {
    // Called now
    transport_->detach_service_proxy();  // Decrement (1→0)
    // Last proxy detached → shutdown transport
}

// Both directions exist until transport destroyed
// Zone B can send calls until its proxy destroyed
```

### Example 3: Routing Proxy Management

#### Current (Incorrect)
```cpp
// Routing proxy with zero objects destroyed prematurely
auto routing_proxy = create_proxy_for_routing(...);
// Later: ref count = 0 → destroyed even though needed for routing!
```

#### Proposed (Correct)
```cpp
class service {
    // Hold routing proxies explicitly
    std::vector<std::shared_ptr<service_proxy>> routing_proxies_;

    void register_routing_proxy(std::shared_ptr<service_proxy> proxy) {
        routing_proxies_.push_back(proxy);
        // Kept alive by service, not by ref count
    }

    ~service() {
        // Routing proxies destroyed with service
        routing_proxies_.clear();
    }
};
```

---

## Appendix B: Migration Checklist

### Pre-Migration

- [ ] Baseline metrics collected
- [ ] Feature flags implemented
- [ ] CI pipeline configured
- [ ] Parallel implementation structure created

### Phase 1: Transport Abstraction

- [ ] `i_transport` interface defined
- [ ] `transport` base class implemented
- [ ] SPSC refactored to `spsc_transport`
- [ ] Local transport extracted
- [ ] TCP transport extracted
- [ ] SGX transport extracted
- [ ] All transport tests passing

### Phase 2: Symmetrical Proxies

- [ ] `transport_pair` structure added
- [ ] `service::transports_` map implemented
- [ ] `connect_to_zone()` refactored
- [ ] Routing proxy management implemented
- [ ] All connection tests passing

### Phase 3: Y-Topology Fix

- [ ] Route notification protocol defined
- [ ] Notification propagation implemented
- [ ] Routing proxy creation on notification
- [ ] Y-topology workaround removed
- [ ] All Y-topology tests passing

### Phase 4: Simplification

- [ ] Protocol negotiation moved to transport
- [ ] Serialization moved to transport
- [ ] service_proxy.cpp ≤450 LOC
- [ ] service.cpp ≤1500 LOC
- [ ] All tests passing

### Phase 5: Documentation

- [ ] User guide updated
- [ ] API documentation complete
- [ ] Migration guide written
- [ ] Performance report generated
- [ ] Release notes written

### Final Validation

- [ ] All functional requirements met
- [ ] All non-functional requirements met
- [ ] Performance validated (≤5% overhead)
- [ ] No memory leaks
- [ ] Thread sanitizer clean
- [ ] Security review complete
- [ ] Code review complete

### Release

- [ ] CHANGELOG updated
- [ ] Version bumped to 3.0
- [ ] Git tag created
- [ ] Release announced

---

**End of Document**
