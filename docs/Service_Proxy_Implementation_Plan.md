<!--
Copyright (c) 2025 Edward Boggis-Rolfe
All rights reserved.
-->

# Service Proxy and Transport Implementation Plan

**Version**: 2.0
**Date**: 2025-01-18
**Status**: Implementation Planning
**Related Documents**:
- Service_Proxy_Transport_Problem_Statement.md (Requirements and Problem Analysis)

---

## Executive Summary

This document provides the comprehensive implementation plan for refactoring RPC++ service proxy and transport architecture. It combines design proposals, technical specifications, migration strategy, and implementation phases.

The refactoring addresses critical issues identified in the problem statement:
1. **Y-Topology Race Condition** - Eliminate on-demand proxy creation
2. **Transport Abstraction** - Separate transport from routing logic
3. **Symmetrical Service Proxy Pairs** - Bidirectional creation and lifecycle
4. **Pass-Through Architecture** - Three alternative designs:
   - **Plan A**: Service Sink + Service Proxy (4-object circular dependency)
   - **Plan B**: Single Pass-Through object (simplest - 75% fewer objects)
   - **Plan C**: Unified Service Proxy with Polymorphic Handler (**RECOMMENDED**)
5. **Explicit Lifecycle Management** - Clear ownership and destruction order
6. **Fire-and-Forget Messaging** - Post method with optimistic ref cleanup
7. **Child Service Lifecycle** - Safe shutdown and DLL unloading

**Impact**: Eliminates race conditions, reduces code complexity by ~40-50%, provides foundation for future transport implementations, enables fire-and-forget messaging with optimistic reference cleanup, and ensures safe child service shutdown with DLL unloading safety.

**Plan C (Unified Service Proxy)** recommended for:
- **Reuses existing infrastructure**: No new service_sink class needed
- **Clean lifetime management**: No self-reference_ or is_routing_proxy_ flags
- **Polymorphic routing**: Handler can be service or pass_through
- **Transport flexibility**: Derived classes (SPSC, TCP, local, SGX) own their transport
- **Natural fit**: Works with existing service_proxy architecture

---

## Table of Contents

1. [Design Principles](#design-principles)
2. [Core Architecture Changes](#core-architecture-changes)
3. [Proposed Design Changes](#proposed-design-changes)
4. [Critical Lifecycle Scenarios](#critical-lifecycle-scenarios)
5. [Implementation Phases](#implementation-phases)
6. [Migration Strategy](#migration-strategy)
7. [Risk Analysis](#risk-analysis)
8. [Success Criteria](#success-criteria)
9. [Appendices](#appendices)

---

## Design Principles

### Principle 1: Separation of Concerns

**Proposed Solution**:
```
service_proxy:  Routing logic + object proxy aggregation
transport:      Bidirectional communication + serialization
service_sink:   Receiving calls + dispatch/pass-through
service:        Lifecycle management + transport ownership
```

**Rationale**: Single Responsibility Principle - each component has one reason to change.

### Principle 2: Symmetrical Bidirectional Pairs

**Proposed Solution**:
```cpp
// Creating connection A↔B creates BOTH directions simultaneously
auto [proxy_a_to_b, proxy_b_to_a] = create_bidirectional_proxies(...);

// Both proxies share the same transport
// Both destroyed together when transport is destroyed
```

**Rationale**: Bidirectional transport implies bidirectional service proxies. Lifecycle symmetry eliminates race conditions.

### Principle 3: Explicit Transport Ownership

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

**Proposed Solution**:
```cpp
// Service proxies created proactively when transport is established
// No creation during critical path (send, add_ref, release)
// Routing proxies held alive by service, not reference counts
```

**Rationale**: Eliminates race windows and simplifies concurrency reasoning.

### Principle 5: Type-Safe Transport Identification

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

## Core Architecture Changes

### Separation of Concerns Architecture

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
│ - other_zones: map<zone_route, weak_ptr<service_proxy>>     │
│   zone_route = {destination_zone, caller_zone}               │
│                                                              │
│ Note: Routing proxies (zero objects) kept alive by          │
│       circular dependency when counts > 0 (Plan A)           │
└──────────────────────────────────────────────────────────────┘
       │ *
       │ owns (indirectly via service_proxy ownership)
       ↓
┌─────────────────────────────────────────────────────────────────────┐
│              BIDIRECTIONAL TRANSPORT PAIR IN ZONE B                  │
│                                                                      │
│  Transport to A ←────────────────┬────────────────→ Transport to C  │
│                                  │                                   │
│  SERVICE SINK (A→C)              │            SERVICE PROXY (A→C)    │
│  ┌─────────────────┐             │            ┌─────────────────┐   │
│  │ Receives from A │             │            │ Sends to C      │   │
│  │ - opposite_─────┼─────────────┼───────────►│ - transport: ───┼──►│
│  │   proxy: shared │  forwards   │            │   shared_ptr    │   │
│  │   (when cnt>0)  │             │            │   (to C)        │   │
│  └─────────────────┘             │            └─────────────────┘   │
│         ▲                        │                     │             │
│         │                        │                     │             │
│         │ shares transport       │                     │ forwards to │
│         │ (to A)                 │                     ↓             │
│  ┌─────────────────┐             │            ┌─────────────────┐   │
│  │ Sends to A      │             │            │ Receives from C │   │
│ ◄┼─── transport:   │             │            │ - opposite_     │   │
│  │      shared_ptr │             │            │   proxy: shared─┼───┤
│  │                 │◄────────────┼────────────┼─  (when cnt>0)  │   │
│  └─────────────────┘  forwards   │            └─────────────────┘   │
│  SERVICE PROXY (C→A)              │            SERVICE SINK (C→A)    │
│                                   │                                  │
│         Circular dependency when counts > 0:                         │
│  sink(A→C) → proxy(A→C) → sink(C→A) → proxy(C→A) → back to sink(A→C)│
│                                                                      │
│  Key Transport Sharing:                                              │
│  - sink(A→C) + proxy(C→A) share transport to Zone A                 │
│  - sink(C→A) + proxy(A→C) share transport to Zone C                 │
│  - Sink receives FROM a zone, Proxy sends TO a zone                 │
│  - They share the transport to/from that zone (opposite directions) │
└─────────────────────────────────────────────────────────────────────┘

Alternating Chain in Pass-Through Mode (Zone B between A and C):

  service_sink (A→C) [receives from Zone A via transport_A]
    │
    │ holds shared_ptr when counts > 0 (forwards to proxy A→C)
    ↓
  service_proxy (A→C) [sends to Zone C via transport_C]
    │
    │ owns shared_ptr to transport_C
    ↓
  service_sink (C→A) [receives from Zone C via transport_C]
    │
    │ holds shared_ptr when counts > 0 (forwards to proxy C→A)
    ↓
  service_proxy (C→A) [sends to Zone A via transport_A]
    │
    │ owns shared_ptr to transport_A
    ↓
  [back to service_sink (A→C)] ← completes the cycle

Transport Sharing (CRITICAL):
  - sink(A→C) receives from A via transport_A
  - proxy(C→A) sends to A via transport_A
  - sink(A→C) and proxy(C→A) share transport_A

  - sink(C→A) receives from C via transport_C
  - proxy(A→C) sends to C via transport_C
  - sink(C→A) and proxy(A→C) share transport_C

When counts == 0:
  - All sink→proxy links become weak_ptr
  - Cycle breaks, all 4 objects can be destroyed

When counts > 0:
  - Sink→proxy links are shared_ptr
  - Circular dependency keeps all 4 objects alive

Local Dispatch Mode (Non-Pass-Through):
  service_sink receives → dispatches to local service
  - opposite_proxy_ is nullptr (no forwarding)
  - Sink kept alive by transport (which holds shared_ptr or keeps it alive)
  - Proxy kept alive by object reference counts (or routing_proxies_ if zero objects)

                   ┌─────────────────────────┐
                   │  TRANSPORT (abstract)   │
                   ├─────────────────────────┤
                   │ - local_zone: zone      │
                   │ - remote_zone: zone     │
                   │ - service_sink: weak    │ ← Delivers to sink
                   │                         │
                   │ Virtual methods:        │
                   │ - send()                │
                   │ - post()                │
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

Key Relationships:
- Service only tracks service_proxies via other_zones map (weak_ptr)
- Transport is hidden implementation detail of service_proxy
- Links ALTERNATE: sink → proxy → sink → proxy (never sink→sink or proxy→proxy)
- Service_sink kept alive by transport (service doesn't know about sinks)
- Service_proxy kept alive by:
  - Normal proxy: object reference counts (shared + optimistic)
  - Routing proxy: circular dependency when counts > 0 (Plan A)
- Service_sink holds shared_ptr to opposite service_proxy ONLY when counts > 0
- Service_sink always holds weak_ptr for potential promotion

CRITICAL Transport Sharing (opposite directions):
- sink(A→C) receives FROM Zone A, proxy(C→A) sends TO Zone A
  → They share transport_A (bidirectional to Zone A)
- sink(C→A) receives FROM Zone C, proxy(A→C) sends TO Zone C
  → They share transport_C (bidirectional to Zone C)
- Sink and proxy that share a transport go in OPPOSITE directions
- Transport delivers received messages to its associated sink
- Sink either dispatches locally OR forwards to opposite proxy
```

---

## Proposed Design Changes

### Change 1: Extract Transport Interface

**Objective**: Create explicit transport abstraction separate from service_proxy.

#### Proposed State

```cpp
// New transport interface - inherits from i_marshaller
class i_transport : public i_marshaller {
public:
    virtual ~i_transport() = default;

    // Bidirectional communication (from i_marshaller)
    virtual CORO_TASK(int) send(
        uint64_t protocol_version,
        encoding encoding,
        uint64_t tag,
        caller_channel_zone caller_channel_zone_id,
        caller_zone caller_zone_id,
        destination_zone destination_zone_id,
        object object_id,
        interface_ordinal interface_id,
        method method_id,
        size_t in_size_,
        const char* in_buf_,
        std::vector<char>& out_buf_,
        // Back-channel support
        size_t in_back_channel_size_,
        const char* in_back_channel_buf_,
        std::vector<char>& out_back_channel_buf_
    ) = 0;

    // Fire-and-forget messaging (from i_marshaller)
    virtual CORO_TASK(int) post(
        uint64_t protocol_version,
        encoding encoding,
        uint64_t tag,
        caller_channel_zone caller_channel_zone_id,
        caller_zone caller_zone_id,
        destination_zone destination_zone_id,
        object object_id,
        interface_ordinal interface_id,
        method method_id,
        post_options options,
        size_t in_size_,
        const char* in_buf_,
        // Back-channel (send only, no receive)
        size_t in_back_channel_size_,
        const char* in_back_channel_buf_
    ) = 0;

    // Other i_marshaller methods with back-channel support
    virtual CORO_TASK(error_code) try_cast(
        uint64_t protocol_version,
        encoding encoding,
        uint64_t tag,
        caller_channel_zone caller_channel_zone_id,
        caller_zone caller_zone_id,
        destination_zone destination_zone_id,
        object object_id,
        interface_ordinal interface_id,
        size_t in_back_channel_size_,
        const char* in_back_channel_buf_,
        std::vector<char>& out_back_channel_buf_
    ) = 0;

    virtual CORO_TASK(error_code) add_ref(
        uint64_t protocol_version,
        encoding encoding,
        uint64_t tag,
        caller_channel_zone caller_channel_zone_id,
        caller_zone caller_zone_id,
        destination_zone destination_zone_id,
        object object_id,
        add_ref_options build_out_param_options,
        uint64_t& out_param,
        size_t in_back_channel_size_,
        const char* in_back_channel_buf_,
        std::vector<char>& out_back_channel_buf_
    ) = 0;

    virtual CORO_TASK(error_code) release(
        uint64_t protocol_version,
        encoding encoding,
        uint64_t tag,
        caller_channel_zone caller_channel_zone_id,
        caller_zone caller_zone_id,
        destination_zone destination_zone_id,
        object object_id,
        release_options options,
        uint64_t& out_param,
        size_t in_back_channel_size_,
        const char* in_back_channel_buf_,
        std::vector<char>& out_back_channel_buf_
    ) = 0;

    // Lifecycle
    virtual CORO_TASK(void) attach_service_proxy() = 0;
    virtual CORO_TASK(void) detach_service_proxy() = 0;
    virtual CORO_TASK(void) shutdown() = 0;

    // Protocol negotiation
    virtual uint64_t get_protocol_version() const = 0;
    virtual encoding get_encoding() const = 0;

    // Connection status
    virtual bool is_connected() const = 0;
    virtual const char* get_transport_type() const = 0;

protected:
    zone local_zone_;     // This zone (e.g., C)
    zone adjacent_zone_;  // The adjacent zone this transport connects to (e.g., B or D)
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

    CORO_TASK(int) post(...) override {
        // Determine if local or remote
        // If remote, delegate to transport
        CO_RETURN CO_AWAIT transport_->post(...);
    }
};
```

#### Transport Implementations

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

---

### Change 2: Implement Symmetrical Service Proxy Pairs

**Objective**: Eliminate asymmetric creation and ensure both directions exist simultaneously.

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

3. **Update `service::other_zones` map** (2 days)
   ```cpp
   // OLD: Map by {destination, caller}
   std::map<zone_route, std::weak_ptr<service_proxy>> other_zones;

   // NEW: Map by adjacent zone
   std::unordered_map<zone, transport_pair> transports_;
   ```

4. **Handle routing proxies** (3 days)
   - Service holds `shared_ptr` to routing proxies
   - Not destroyed by reference count = 0
   - Destroyed when service/transport destroyed
   ```cpp
   class service {
       // Routing proxies kept alive explicitly
       std::vector<std::shared_ptr<service_proxy>> routing_proxies_;
   };
   ```

---

### Change 3A: Service Sink Architecture (Plan A - Original Design)

**Note**: See Change 3B below for a simpler alternative design using a single `pass_through` object.

---

**Objective**: Introduce `service_sink` as a receiver-side counterpart to `service_proxy`, providing a unified architecture for both direct local calls and channel-based async communication.

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

#### Service Sink Design

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
        // Handle post messages specially
        if (msg.is_post_message()) {
            switch (msg.post_options) {
                case post_options::zone_terminating:
                    // Emergency shutdown notification received
                    mark_zone_as_terminated(msg.caller_zone_id);

                    // Clean up ALL service_proxies to that zone
                    cleanup_all_proxies_to_zone(msg.caller_zone_id);

                    // Clean up ALL services referencing that zone
                    cleanup_all_services_referencing_zone(msg.caller_zone_id);

                    // Ignore future release events from that zone
                    // No response sent
                    break;

                case post_options::release:
                    // Skip if zone already terminated
                    if (!is_zone_terminated(msg.caller_zone_id)) {
                        cleanup_optimistic_ref(msg.object_id);
                    }
                    // No response sent
                    break;

                case post_options::adhoc:
                    // Route to service-level ad-hoc message handler
                    CO_AWAIT service_->handle_adhoc_message(msg);
                    // No response sent
                    break;

                case post_options::fire_and_forget:
                    // Fire-and-forget call to object method
                    if (opposite_proxy_) {
                        // Pass-through mode: forward to opposite service proxy
                        CO_AWAIT opposite_proxy_->post(msg);
                    } else {
                        // Local mode: dispatch to local service's object stub
                        CO_AWAIT service_->dispatch_to_stub(msg);
                    }
                    // No response sent
                    break;
            }
        } else {
            // Regular RPC call (request/response)
            if (opposite_proxy_) {
                // Pass-through mode: forward to opposite service proxy
                CO_AWAIT opposite_proxy_->send(msg);
            } else {
                // Local mode: dispatch to local service's object stub
                // All channel traffic is delivered to the service for processing
                CO_AWAIT service_->dispatch_to_stub(msg);
            }
        }
    }
};
```

#### Service Lifecycle Management with Sinks

```cpp
class service {
    // ONLY collection needed - simple!
    std::map<zone_route, std::weak_ptr<service_proxy>> other_zones;

    // No separate routing_proxies_ collection
    // No transports_ collection
    // No service_sinks_ collection
};

class service_proxy {
private:
    // Transport ownership in derived classes (not shown here)

public:
    ~service_proxy() {
        // Clean destructor - no special checks needed
        // Destroyed when:
        // - Object proxies release it (normal case), OR
        // - Service sink circular chain releases it (routing case when counts > 0)
        // No assertions needed - lifetime naturally managed
    }
};
```

**Lifetime Management Summary**:

**Service**:
- ONE collection: `other_zones` map with `weak_ptr<service_proxy>`
- Minimal locking: only one mutex for the map
- Transport is hidden inside service_proxy
- Service sinks are hidden inside transport

**Service Sinks**:
- Kept alive by transport (service doesn't know they exist)
- In pass-through mode: also kept alive by circular dependency when counts > 0

**Service Proxies**:
- Normal proxies: Kept alive by object reference counts (shared + optimistic)
- Routing proxies: Kept alive by circular dependency when counts > 0
- In pass-through mode: circular shared_ptr chain forms (sink→proxy→sink→proxy)
- Found via `other_zones` map (weak_ptr lookup)

**Transports**:
- Hidden implementation detail inside service_proxy
- Owned by service_proxy via `shared_ptr`
- Keep sinks alive
- Shared by both proxy directions in a pair

#### Service Sink Operational Modes

**Mode 1: Local Dispatch (Non-Pass-Through)**
```cpp
// Sink delivers all channel traffic to the local service
service_sink sink(service_ptr);
// opposite_proxy_ is nullptr - not configured for pass-through

// When messages arrive:
sink.receive_call(msg);  // → dispatches to service_->dispatch_to_stub(msg)

// Lifecycle: Kept alive by transport and object reference counts
// - Sink: Transport holds reference to sink (service doesn't know about sinks)
// - Proxy: Kept alive by object reference counts
//   - If proxy has objects: aggregate ref count keeps it alive
//   - If routing proxy (zero objects): kept alive by circular dependency when counts > 0
// - Service only tracks proxies via other_zones map (weak_ptr)
// - Minimal locking: only one mutex for other_zones map
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

---

### Change 3B: Pass-Through Object (Plan B - Simplified Alternative)

**Objective**: Replace the 4-object circular dependency (2 sinks + 2 proxies) with a single `pass_through` object that handles bidirectional routing for a zone pair.

#### Motivation

The Plan A design (service_sink + service_proxy) creates complexity:
- 4 objects per zone pair in pass-through scenarios
- Circular dependency that forms/breaks based on reference counts
- Alternating chain: sink → proxy → sink → proxy
- Potential race conditions in count management across 4 objects

**Plan B simplifies to ONE object per zone pair.**

#### Pass-Through Design

```cpp
class pass_through : public i_marshaller {
private:
    // Keep service alive
    std::shared_ptr<service> service_;

    // Bidirectional transports (one to each zone)
    std::shared_ptr<transport> transport_to_zone_a_;  // Send to A, receive from A
    std::shared_ptr<transport> transport_to_zone_c_;  // Send to C, receive from C

    // Reference counts for BOTH directions
    std::atomic<uint64_t> shared_count_a_to_c_{0};      // A→C direction
    std::atomic<uint64_t> optimistic_count_a_to_c_{0};  // A→C direction
    std::atomic<uint64_t> shared_count_c_to_a_{0};      // C→A direction
    std::atomic<uint64_t> optimistic_count_c_to_a_{0};  // C→A direction

    zone zone_a_;
    zone zone_c_;

    // Self-reference for lifetime management
    std::shared_ptr<pass_through> self_reference_;

public:
    pass_through(std::shared_ptr<service> service,
                 std::shared_ptr<transport> transport_a,
                 std::shared_ptr<transport> transport_c,
                 zone zone_a,
                 zone zone_c)
        : service_(std::move(service))
        , transport_to_zone_a_(std::move(transport_a))
        , transport_to_zone_c_(std::move(transport_c))
        , zone_a_(zone_a)
        , zone_c_(zone_c)
    {
        // Hold self-reference initially
        self_reference_ = shared_from_this();

        // Register with transports to receive messages via i_marshaller interface
        // Transport will call send(), post(), add_ref(), release(), try_cast() directly
        transport_to_zone_a_->set_receive_handler(weak_from_this());
        transport_to_zone_c_->set_receive_handler(weak_from_this());
    }

    // i_marshaller implementation - destination_zone determines direction
    CORO_TASK(int) send(
        uint64_t protocol_version,
        encoding encoding,
        uint64_t tag,
        caller_channel_zone caller_channel_zone_id,
        caller_zone caller_zone_id,
        destination_zone destination_zone_id,  // ← KEY: Determines which transport!
        object object_id,
        interface_ordinal interface_id,
        method method_id,
        size_t in_size_,
        const char* in_buf_,
        std::vector<char>& out_buf_,
        size_t in_back_channel_size_,
        const char* in_back_channel_buf_,
        std::vector<char>& out_back_channel_buf_
    ) override {
        // Determine direction based on destination_zone
        if (destination_zone_id.get_val() == zone_c_.get_val()) {
            // A→C direction: use transport to Zone C
            CO_RETURN CO_AWAIT transport_to_zone_c_->send(
                protocol_version, encoding, tag,
                caller_channel_zone_id, caller_zone_id, destination_zone_id,
                object_id, interface_id, method_id,
                in_size_, in_buf_, out_buf_,
                in_back_channel_size_, in_back_channel_buf_, out_back_channel_buf_
            );
        } else if (destination_zone_id.get_val() == zone_a_.get_val()) {
            // C→A direction: use transport to Zone A
            CO_RETURN CO_AWAIT transport_to_zone_a_->send(
                protocol_version, encoding, tag,
                caller_channel_zone_id, caller_zone_id, destination_zone_id,
                object_id, interface_id, method_id,
                in_size_, in_buf_, out_buf_,
                in_back_channel_size_, in_back_channel_buf_, out_back_channel_buf_
            );
        } else {
            CO_RETURN error_codes::invalid_zone;
        }
    }

    // Similar implementation for post(), try_cast(), add_ref(), release()
    CORO_TASK(int) post(
        uint64_t protocol_version,
        encoding encoding,
        uint64_t tag,
        caller_channel_zone caller_channel_zone_id,
        caller_zone caller_zone_id,
        destination_zone destination_zone_id,  // ← Determines direction
        object object_id,
        interface_ordinal interface_id,
        method method_id,
        post_options options,
        size_t in_size_,
        const char* in_buf_,
        size_t in_back_channel_size_,
        const char* in_back_channel_buf_
    ) override {
        if (destination_zone_id.get_val() == zone_c_.get_val()) {
            CO_RETURN CO_AWAIT transport_to_zone_c_->post(...);
        } else if (destination_zone_id.get_val() == zone_a_.get_val()) {
            CO_RETURN CO_AWAIT transport_to_zone_a_->post(...);
        } else {
            CO_RETURN error_codes::invalid_zone;
        }
    }

    // Note: No special receive methods needed - transport calls i_marshaller methods directly
    // (send, post, try_cast, add_ref, release) based on incoming messages

    // add_ref may need special handling - TO BE INVESTIGATED
    // If special forking is required, may need to delegate to service instead of routing
    CORO_TASK(error_code) add_ref(
        uint64_t protocol_version,
        encoding encoding,
        uint64_t tag,
        caller_channel_zone caller_channel_zone_id,
        caller_zone caller_zone_id,
        destination_zone destination_zone_id,
        object object_id,
        add_ref_options build_out_param_options,
        uint64_t& out_param,
        size_t in_back_channel_size_,
        const char* in_back_channel_buf_,
        std::vector<char>& out_back_channel_buf_
    ) override {
        // TODO: Investigate if add_ref needs special forking logic
        // For now, route same as send()/post()
        if (destination_zone_id.get_val() == zone_c_.get_val()) {
            CO_RETURN CO_AWAIT transport_to_zone_c_->add_ref(
                protocol_version, encoding, tag,
                caller_channel_zone_id, caller_zone_id, destination_zone_id,
                object_id, build_out_param_options, out_param,
                in_back_channel_size_, in_back_channel_buf_, out_back_channel_buf_
            );
        } else if (destination_zone_id.get_val() == zone_a_.get_val()) {
            CO_RETURN CO_AWAIT transport_to_zone_a_->add_ref(
                protocol_version, encoding, tag,
                caller_channel_zone_id, caller_zone_id, destination_zone_id,
                object_id, build_out_param_options, out_param,
                in_back_channel_size_, in_back_channel_buf_, out_back_channel_buf_
            );
        } else {
            CO_RETURN error_codes::invalid_zone;
        }
    }

    // Update reference counts for one direction
    void update_counts(destination_zone dest, uint64_t shared_count, uint64_t optimistic_count) {
        if (dest.get_val() == zone_c_.get_val()) {
            // A→C direction
            shared_count_a_to_c_ = shared_count;
            optimistic_count_a_to_c_ = optimistic_count;
        } else if (dest.get_val() == zone_a_.get_val()) {
            // C→A direction
            shared_count_c_to_a_ = shared_count;
            optimistic_count_c_to_a_ = optimistic_count;
        }

        update_self_reference();
    }

    uint64_t get_total_ref_count() const {
        return shared_count_a_to_c_.load() + optimistic_count_a_to_c_.load() +
               shared_count_c_to_a_.load() + optimistic_count_c_to_a_.load();
    }

private:
    void update_self_reference() {
        if (get_total_ref_count() > 0) {
            // Keep alive
            if (!self_reference_) {
                self_reference_ = shared_from_this();
            }
        } else {
            // Allow destruction
            self_reference_.reset();
        }
    }
};
```

#### Service Integration

```cpp
class service {
    // Simple! Just one collection for pass-through objects
    std::map<zone_pair, std::weak_ptr<pass_through>> pass_throughs_;

    struct zone_pair {
        zone zone_a;
        zone zone_c;

        bool operator<(const zone_pair& other) const {
            // Normalize: always zone_a < zone_c
            auto [min_a, max_a] = std::minmax(zone_a.get_val(), zone_c.get_val());
            auto [min_b, max_b] = std::minmax(other.zone_a.get_val(), other.zone_c.get_val());
            return std::tie(min_a, max_a) < std::tie(min_b, max_b);
        }
    };

    void create_pass_through(zone zone_a, zone zone_c,
                            std::shared_ptr<transport> transport_a,
                            std::shared_ptr<transport> transport_c) {
        auto pt = std::make_shared<pass_through>(
            shared_from_this(),
            transport_a,
            transport_c,
            zone_a,
            zone_c
        );

        pass_throughs_[{zone_a, zone_c}] = pt;
    }
};
```

#### Benefits of Plan B

**Simplicity**:
- **1 object** instead of 4 (2 sinks + 2 proxies)
- **No circular dependency** to manage
- **No alternating chain** complexity
- **Single point of lifetime management**

**Performance**:
- **Fewer allocations**: 1 object vs 4
- **Atomic operations**: All counts in same object, easier to manage atomically
- **No race conditions**: Single object manages both directions
- **Less locking**: One object = simpler synchronization

**Correctness**:
- **Uses existing i_marshaller interface**: No new abstractions needed
- **destination_zone parameter**: Already present in all methods, naturally determines direction
- **Total ref count**: Destroyed when sum of ALL direction counts == 0
- **Generic stub+proxy**: Acts as both stub (receives) and proxy (sends) for both directions

**Comparison**:

| Aspect | Plan A (Sink+Proxy) | Plan B (Pass-Through) |
|--------|---------------------|----------------------|
| Objects per zone pair | 4 (2 sinks + 2 proxies) | 1 |
| Circular dependency | Yes (complex) | No |
| Reference count management | Across 4 objects | Single object |
| Direction determination | Object type (sink vs proxy) | destination_zone parameter |
| Race condition potential | Higher (4-object coordination) | Lower (atomic in 1 object) |
| Complexity | High | Low |
| LOC estimate | ~800 lines | ~400 lines |

#### When to Use Each Plan

**Plan A (Service Sink)**:
- When receive and send sides need different behavior
- When fine-grained control over each direction is needed
- When integrating with existing code expecting separate sink/proxy

**Plan B (Pass-Through)**:
- When simplicity and correctness are priorities (recommended)
- For new implementations
- When pass-through routing is the primary use case
- When minimizing race conditions is critical

**Recommendation**: Start with Plan B for new code. The simplicity and reduced race condition potential make it the better choice for most scenarios.

---

### Change 3C: Unified Service Proxy with Polymorphic Handler (Plan C - Recommended)

**Objective**: Unify service_proxy to act as both sink (receives from transport) and proxy (sends to handler), using polymorphic `i_marshaller` handler that can be either service or pass_through.

#### Motivation

Both Plan A and Plan B have complexity:
- **Plan A**: 4 objects with circular dependency
- **Plan B**: Still needs separate pass_through class

**Plan C insight**: Service proxy already exists and implements `i_marshaller`. Why not make it universal?

**Key principles**:
1. Service proxy has `weak_ptr<i_marshaller>` pointing to service OR pass_through
2. Uses `destination_zone` parameter to route: local (handler) vs remote (transport)
3. Transport ownership in derived classes (spsc_service_proxy, tcp_service_proxy, etc.)
4. No `self_reference_` needed - kept alive by object proxies or pass_through
5. No `is_routing_proxy_` flag needed - lifetime naturally managed

#### Base Service Proxy Design

```cpp
class service_proxy : public i_marshaller {
private:
    std::weak_ptr<i_marshaller> local_handler_;  // Service or pass_through
    zone this_zone_;  // The zone this proxy belongs to

public:
    service_proxy(std::weak_ptr<i_marshaller> handler, zone this_zone)
        : local_handler_(std::move(handler))
        , this_zone_(this_zone)
    {}

    // i_marshaller implementation - routes based on destination_zone
    CORO_TASK(int) send(
        uint64_t protocol_version,
        encoding encoding,
        uint64_t tag,
        caller_channel_zone caller_channel_zone_id,
        caller_zone caller_zone_id,
        destination_zone destination_zone_id,  // ← KEY: Routing decision
        object object_id,
        interface_ordinal interface_id,
        method method_id,
        size_t in_size_,
        const char* in_buf_,
        std::vector<char>& out_buf_,
        size_t in_back_channel_size_,
        const char* in_back_channel_buf_,
        std::vector<char>& out_back_channel_buf_
    ) override {
        // Check if destination is local to this zone
        if (destination_zone_id.get_val() == this_zone_.get_val()) {
            // LOCAL: Destination is in this zone
            // Delegate to service or pass_through
            if (auto handler = local_handler_.lock()) {
                CO_RETURN CO_AWAIT handler->send(
                    protocol_version, encoding, tag,
                    caller_channel_zone_id, caller_zone_id, destination_zone_id,
                    object_id, interface_id, method_id,
                    in_size_, in_buf_, out_buf_,
                    in_back_channel_size_, in_back_channel_buf_, out_back_channel_buf_
                );
            } else {
                // Handler (service/pass_through) is shutting down
                CO_RETURN error_codes::zone_shutting_down;
            }
        } else {
            // REMOTE: Destination is in another zone
            // Delegate to derived class (which owns transport)
            CO_RETURN CO_AWAIT send_remote(
                protocol_version, encoding, tag,
                caller_channel_zone_id, caller_zone_id, destination_zone_id,
                object_id, interface_id, method_id,
                in_size_, in_buf_, out_buf_,
                in_back_channel_size_, in_back_channel_buf_, out_back_channel_buf_
            );
        }
    }

    // Similar for post(), try_cast(), add_ref(), release()

    ~service_proxy() {
        // Clean destructor - no special checks needed
        // Destroyed when:
        // - Object proxies release it (normal case), OR
        // - Pass-through releases it (routing case)
    }

protected:
    // Derived classes implement remote sending via their transport
    virtual CORO_TASK(int) send_remote(
        uint64_t protocol_version,
        encoding encoding,
        uint64_t tag,
        caller_channel_zone caller_channel_zone_id,
        caller_zone caller_zone_id,
        destination_zone destination_zone_id,
        object object_id,
        interface_ordinal interface_id,
        method method_id,
        size_t in_size_,
        const char* in_buf_,
        std::vector<char>& out_buf_,
        size_t in_back_channel_size_,
        const char* in_back_channel_buf_,
        std::vector<char>& out_back_channel_buf_
    ) = 0;

    virtual CORO_TASK(int) post_remote(...) = 0;
    virtual CORO_TASK(error_code) try_cast_remote(...) = 0;
    virtual CORO_TASK(error_code) add_ref_remote(...) = 0;
    virtual CORO_TASK(error_code) release_remote(...) = 0;
};
```

#### Derived Classes Own Transport

```cpp
// SPSC service proxy
class spsc_service_proxy : public service_proxy {
private:
    std::shared_ptr<spsc_transport> transport_;
    zone remote_zone_;  // Zone this transport connects to

protected:
    CORO_TASK(int) send_remote(
        uint64_t protocol_version,
        encoding encoding,
        uint64_t tag,
        caller_channel_zone caller_channel_zone_id,
        caller_zone caller_zone_id,
        destination_zone destination_zone_id,
        object object_id,
        interface_ordinal interface_id,
        method method_id,
        size_t in_size_,
        const char* in_buf_,
        std::vector<char>& out_buf_,
        size_t in_back_channel_size_,
        const char* in_back_channel_buf_,
        std::vector<char>& out_back_channel_buf_
    ) override {
        // Validate destination matches our transport
        if (destination_zone_id.get_val() != remote_zone_.get_val()) {
            // ERROR: Message routed to wrong proxy!
            RPC_ERROR("Invalid routing: proxy to zone {} received message for zone {}",
                     remote_zone_.get_val(), destination_zone_id.get_val());
            CO_RETURN error_codes::invalid_routing;
        }

        // Send via SPSC transport
        CO_RETURN CO_AWAIT transport_->send(
            protocol_version, encoding, tag,
            caller_channel_zone_id, caller_zone_id, destination_zone_id,
            object_id, interface_id, method_id,
            in_size_, in_buf_, out_buf_,
            in_back_channel_size_, in_back_channel_buf_, out_back_channel_buf_
        );
    }

public:
    spsc_service_proxy(std::weak_ptr<i_marshaller> handler,
                      zone this_zone,
                      std::shared_ptr<spsc_transport> transport,
                      zone remote_zone)
        : service_proxy(handler, this_zone)
        , transport_(std::move(transport))
        , remote_zone_(remote_zone)
    {}
};

// Local service proxy (direct calls, no serialization)
class local_service_proxy : public service_proxy {
private:
    std::weak_ptr<service> remote_service_;
    zone remote_zone_;

protected:
    CORO_TASK(int) send_remote(..., destination_zone destination_zone_id, ...) override {
        // Validate destination
        if (destination_zone_id.get_val() != remote_zone_.get_val()) {
            RPC_ERROR("Invalid routing: proxy to zone {} received message for zone {}",
                     remote_zone_.get_val(), destination_zone_id.get_val());
            CO_RETURN error_codes::invalid_routing;
        }

        // Direct call to remote service (no serialization)
        if (auto svc = remote_service_.lock()) {
            CO_RETURN CO_AWAIT svc->send(...);
        } else {
            CO_RETURN error_codes::zone_shutting_down;
        }
    }

public:
    local_service_proxy(std::weak_ptr<i_marshaller> handler,
                       zone this_zone,
                       std::weak_ptr<service> remote_service,
                       zone remote_zone)
        : service_proxy(handler, this_zone)
        , remote_service_(std::move(remote_service))
        , remote_zone_(remote_zone)
    {}
};

// TCP, SGX service proxies follow same pattern
```

#### Service as i_marshaller Handler

```cpp
class service : public i_marshaller {
private:
    std::map<zone_route, std::weak_ptr<service_proxy>> other_zones;

public:
    // i_marshaller implementation - for local dispatch
    CORO_TASK(int) send(..., destination_zone destination_zone_id, ...) override {
        // Dispatch to local object stub
        CO_RETURN CO_AWAIT dispatch_to_stub(
            destination_zone_id, object_id, interface_id, method_id,
            in_buf_, out_buf_
        );
    }

    // Create service proxy pointing to this service
    std::shared_ptr<spsc_service_proxy> create_spsc_proxy(
        std::shared_ptr<spsc_transport> transport,
        zone remote_zone
    ) {
        return std::make_shared<spsc_service_proxy>(
            weak_from_this(),  // local_handler_ points to this service
            zone_id_,          // this_zone
            transport,
            remote_zone
        );
    }
};
```

#### Pass-Through as i_marshaller Handler

```cpp
class pass_through : public i_marshaller {
private:
    std::shared_ptr<service> service_;  // Keep service alive

    // Service proxies that use this pass_through as handler
    std::shared_ptr<spsc_service_proxy> proxy_from_a_;
    std::shared_ptr<spsc_service_proxy> proxy_from_c_;

    zone zone_a_;
    zone zone_c_;
    zone this_zone_;  // Zone B (where pass_through lives)

    // Reference counts for BOTH directions
    std::atomic<uint64_t> shared_count_a_to_c_{0};
    std::atomic<uint64_t> optimistic_count_a_to_c_{0};
    std::atomic<uint64_t> shared_count_c_to_a_{0};
    std::atomic<uint64_t> optimistic_count_c_to_a_{0};

public:
    pass_through(std::shared_ptr<service> service,
                 std::shared_ptr<spsc_transport> transport_to_a,
                 std::shared_ptr<spsc_transport> transport_to_c,
                 zone zone_a,
                 zone zone_c,
                 zone this_zone)
        : service_(std::move(service))
        , zone_a_(zone_a)
        , zone_c_(zone_c)
        , this_zone_(this_zone)
    {
        // Create service proxies that use THIS pass_through as handler
        proxy_from_a_ = std::make_shared<spsc_service_proxy>(
            weak_from_this(),  // local_handler_ points to this pass_through
            this_zone,
            transport_to_a,
            zone_a
        );

        proxy_from_c_ = std::make_shared<spsc_service_proxy>(
            weak_from_this(),  // local_handler_ points to this pass_through
            this_zone,
            transport_to_c,
            zone_c
        );

        // Register service proxies with transports to receive messages
        // Transport calls proxy's i_marshaller methods (send, post, add_ref, etc.)
        // Proxy then routes to local_handler_ (this pass_through) or remote transport
        transport_to_a->set_receive_handler(proxy_from_a_);
        transport_to_c->set_receive_handler(proxy_from_c_);
    }

    // i_marshaller implementation - routes based on destination
    CORO_TASK(int) send(..., destination_zone destination_zone_id, ...) override {
        // Route to appropriate proxy's transport
        if (destination_zone_id.get_val() == zone_a_.get_val()) {
            // Forward to Zone A via proxy_from_a_'s transport
            CO_RETURN CO_AWAIT proxy_from_a_->send(...);
        } else if (destination_zone_id.get_val() == zone_c_.get_val()) {
            // Forward to Zone C via proxy_from_c_'s transport
            CO_RETURN CO_AWAIT proxy_from_c_->send(...);
        } else if (destination_zone_id.get_val() == this_zone_.get_val()) {
            // ERROR: Pass-through doesn't host objects locally
            RPC_ERROR("Pass-through received call for local zone {} but has no objects",
                     this_zone_.get_val());
            CO_RETURN error_codes::invalid_routing;
        } else {
            // ERROR: Unknown destination
            CO_RETURN error_codes::invalid_zone;
        }
    }

    // add_ref may need special handling - TO BE INVESTIGATED
    // If special forking is required, may need to delegate to service instead of routing
    CORO_TASK(error_code) add_ref(..., destination_zone destination_zone_id, ...) override {
        // TODO: Investigate if add_ref needs special forking logic
        // For now, route same as send()
        if (destination_zone_id.get_val() == zone_a_.get_val()) {
            CO_RETURN CO_AWAIT proxy_from_a_->add_ref(...);
        } else if (destination_zone_id.get_val() == zone_c_.get_val()) {
            CO_RETURN CO_AWAIT proxy_from_c_->add_ref(...);
        } else {
            CO_RETURN error_codes::invalid_zone;
        }
    }

    // Update reference counts - determines lifetime
    void update_counts(destination_zone dest, uint64_t shared_count, uint64_t optimistic_count) {
        if (dest.get_val() == zone_c_.get_val()) {
            shared_count_a_to_c_ = shared_count;
            optimistic_count_a_to_c_ = optimistic_count;
        } else if (dest.get_val() == zone_a_.get_val()) {
            shared_count_c_to_a_ = shared_count;
            optimistic_count_c_to_a_ = optimistic_count;
        }

        // Pass-through destroyed when total ref count == 0
        // When destroyed, releases proxy_from_a_ and proxy_from_c_
    }

    uint64_t get_total_ref_count() const {
        return shared_count_a_to_c_.load() + optimistic_count_a_to_c_.load() +
               shared_count_c_to_a_.load() + optimistic_count_c_to_a_.load();
    }
};
```

#### Lifetime Management

**Service Proxy kept alive by**:
1. **Object proxies**: When aggregate ref count > 0
2. **Pass-through**: Holds `shared_ptr<service_proxy>` for routing
3. **NOT by service**: Service has `weak_ptr` only

**Scenarios**:
```cpp
// Normal proxy with objects:
object_proxy_1 holds shared_ptr<service_proxy>
object_proxy_2 holds shared_ptr<service_proxy>
// When all object proxies destroyed -> service_proxy destroyed ✓

// Routing proxy (no objects, used by pass_through):
pass_through holds shared_ptr<service_proxy> (proxy_from_a_, proxy_from_c_)
// When pass_through destroyed -> service_proxies destroyed ✓

// Service never holds strong reference:
service.other_zones has weak_ptr<service_proxy>
// Service can lookup but doesn't keep alive ✓
```

**No self_reference_ needed**: Object proxies or pass_through keep proxy alive

**No is_routing_proxy_ flag needed**: Lifetime naturally managed by whoever holds it

#### Benefits of Plan C

**Simplicity**:
- **Reuses existing service_proxy**: No new sink class
- **Polymorphic handler**: Service or pass_through via `i_marshaller`
- **Clean lifetime**: No self-reference tricks, no special flags
- **Natural routing**: `destination_zone` determines local vs remote

**Flexibility**:
- **Multiple transports**: Each derived class (SPSC, TCP, local, SGX)
- **Validation**: Derived class validates destination matches transport
- **Graceful shutdown**: `weak_ptr.lock()` returns nullptr when shutting down

**Performance**:
- **Fewer objects**: Service proxy already exists, just extend it
- **No circular dependency**: Clean ownership model
- **Service outlives proxies**: Service always valid (strong guarantee)

**Comparison Table**:

| Aspect | Plan A (Sink+Proxy) | Plan B (Pass-Through) | Plan C (Unified Proxy) |
|--------|---------------------|----------------------|------------------------|
| Objects per zone pair | 4 (2 sinks + 2 proxies) | 1 (pass_through) | 2 (2 proxies) + 1 pass_through |
| Circular dependency | Yes (complex) | No | No |
| New classes needed | service_sink | pass_through | None (extend existing) |
| Handler polymorphism | No | No | Yes (service or pass_through) |
| Transport ownership | Service proxy | Pass-through | Derived service_proxy |
| Lifetime management | Circular ref + counts | Self-reference | Object proxies or pass_through |
| Complexity | High | Medium | Low |
| Reuses existing code | Partial | No | Yes (service_proxy) |

#### When to Use Each Plan

**Plan A (Service Sink + Service Proxy)**:
- Legacy code expecting separate sink/proxy
- Fine-grained control over receive vs send sides

**Plan B (Single Pass-Through Object)**:
- Minimal object count priority
- Simple pass-through routing

**Plan C (Unified Service Proxy)** - **RECOMMENDED**:
- Reuse existing service_proxy infrastructure
- Polymorphic handler (service or pass_through)
- Clean lifetime without self-reference or flags
- Natural fit with existing codebase
- Derived classes own transport (SPSC, TCP, local, SGX)

---

### Change 4: Unified Transport Lifecycle Management

**Objective**: Clear ownership model and destruction order for transports.

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

---

### Change 5: Eliminate Y-Topology Race Condition

**Objective**: Remove on-demand proxy creation workaround in `service.cpp:239-250`.

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

---

### Change 6: Fire-and-Forget Messaging and Back-Channel Support

**Objective**: Implement `post()` method for one-way messages and add back-channel metadata support to all `i_marshaller` methods.

#### Post Options

```cpp
enum class post_options {
    fire_and_forget,      // One-way message to object method
    release,              // Optimistic reference cleanup notification
    zone_terminating,     // Emergency shutdown notification
    adhoc                 // Service-to-service maintenance messages
};
```

#### Use Cases

**1. Fire-and-Forget Messages**
- One-way notifications without waiting for response
- Performance optimization for async notifications
- Use with `post_options::fire_and_forget`

**2. Optimistic Reference Cleanup**
- Efficient cleanup without full RPC round-trip
- Only sent to service_proxy/sinks with `optimistic_count > 0`
- Use with `post_options::release`

**3. Zone Termination**
- Emergency shutdown notification to ALL service proxies/sinks
- Cascading cleanup with ignored release events from terminated zones
- Enables clean multi-zone shutdown without deadlocks
- Use with `post_options::zone_terminating`

**4. Ad-hoc Service Messages**
- Certificate management, keepalive pings, configuration updates
- Type controlled by `interface_id` parameter
- Not tied to specific objects
- Use with `post_options::adhoc`

#### Back-Channel Support

All `i_marshaller` methods (send, post, try_cast, add_ref, release) include back-channel parameters:

**Bidirectional (send, try_cast, add_ref, release)**:
```cpp
size_t in_back_channel_size_,
const char* in_back_channel_buf_,
std::vector<char>& out_back_channel_buf_
```

**Unidirectional (post only)**:
```cpp
size_t in_back_channel_size_,
const char* in_back_channel_buf_
// No out_back_channel_buf_ - fire-and-forget
```

**Back-Channel Data Format**:
- HTTP-header style key-value pairs
- Optional encryption per entry
- Use cases: certificate chains, OpenTelemetry IDs, auth tokens, custom metadata

**Benefits**:
1. **Performance**: Fire-and-forget messages don't wait for response
2. **Efficient Cleanup**: Optimistic reference cleanup without RPC overhead
3. **Graceful Shutdown**: Zone termination notification enables clean multi-zone shutdown
4. **Service Protocols**: Ad-hoc messages enable connection maintenance, security updates, monitoring
5. **Distributed Tracing**: Native OpenTelemetry support via back-channel
6. **Security**: Certificate chain transmission and authentication built into protocol

---

### Change 7: Child Service Lifecycle and DLL Unloading Safety

**Objective**: Ensure safe child service shutdown and DLL/shared object unloading.

#### Child Service Shutdown Conditions

A child service shuts down when ALL three conditions are met:

**1. No External Shared References**: All external shared references to the child == 0
   - Incoming optimistic references do NOT prevent shutdown
   - Only shared_count matters for incoming references

**2. No Internal References**: All internal references (shared + optimistic) from the child == 0
   - Outgoing optimistic references DO prevent shutdown
   - Child must release ALL references to other zones

**3. Not Routing Intermediary**: Not acting as routing intermediary between other zones

#### Critical Asymmetry

```cpp
// Incoming optimistic references (other zones → this service):
// - Do NOT prevent shutdown
// - Service may die even if others hold optimistic refs to it

// Outgoing optimistic references (this service → other zones):
// - DO prevent shutdown
// - Service cannot die if it holds optimistic refs to others
```

#### DLL/Shared Object Unloading Safety

**Critical Requirement**: Parent service MUST NOT unload DLL until:
1. Child service has completed garbage collection
2. All threads have returned from DLL code
3. All detach coroutines have completed

**Reference Implementation**: The `enclave_service_proxy` cleanup logic should be used as reference for DLL service proxies.

**Safety Pattern**:
```cpp
class parent_service_manager {
    std::shared_ptr<rpc::child_service> dll_child_;
    void* dll_handle_;  // DLL/SO handle

    ~parent_service_manager() {
        // 1. Release child service reference
        dll_child_.reset();

        // 2. Wait for child garbage collection
        // Must ensure all child destructors complete
        // All service_proxy::detach_service_proxy() coroutines finish

        // 3. Ensure no threads are executing in DLL code
        // All coroutines scheduled from DLL have completed

        // 4. NOW safe to unload DLL
        if (dll_handle_) {
            dlclose(dll_handle_);  // or FreeLibrary() on Windows
        }
    }
};
```

**Implementation Notes**:
- Create DLL service proxy type that adapts enclave cleanup patterns
- Parent must synchronize with child cleanup before unloading
- Use reference counting and completion events to track cleanup state

---

## Critical Lifecycle Scenarios

### Scenario 1: Routing/Bridging Service Proxies (No Object Proxies)

Service proxies may act as pure routing nodes without hosting any objects. These need to stay alive even with zero object references.

**Solution: Natural Lifetime Management**

```cpp
class service_proxy
{
private:
    std::shared_ptr<transport> transport_;  // Keeps transport alive (in derived classes)
    std::unordered_map<object, std::weak_ptr<object_proxy>> proxies_;

public:
    // Clean destructor - no special checks needed
    ~service_proxy()
    {
        // Destroyed when:
        // - Object proxies release it (normal case), OR
        // - Pass-through releases it (routing case)
        // No assertions needed - lifetime naturally managed
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

// Service has ONLY one collection - simple!
class service
{
private:
    // Single collection for ALL service proxies (routing and normal)
    std::map<zone_route, std::weak_ptr<service_proxy>> other_zones;

    // No separate routing_proxies_ collection needed!
    // Routing proxies kept alive by pass_through object

public:
    void register_proxy(destination_zone dest, caller_zone caller,
                       std::shared_ptr<service_proxy> proxy)
    {
        // Register in same collection (normal and routing proxies)
        other_zones[{dest, caller}] = proxy;
    }

    std::shared_ptr<service_proxy> lookup_proxy(destination_zone dest, caller_zone caller)
    {
        if (auto found = other_zones.find({dest, caller}); found != other_zones.end())
        {
            return found->second.lock();  // Returns nullptr if destroyed
        }
        return nullptr;
    }
};
```

**Lifetime Management**:

**Normal Proxy (has objects)**:
```cpp
object_proxy_1 holds shared_ptr<service_proxy>
object_proxy_2 holds shared_ptr<service_proxy>
// When all object proxies destroyed -> service_proxy destroyed ✓
```

**Routing Proxy (no objects, used by pass_through)**:
```cpp
pass_through holds shared_ptr<service_proxy> (proxy_from_a_, proxy_from_c_)
// When pass_through destroyed -> service_proxies destroyed ✓
```

**Service never holds strong reference**:
```cpp
service.other_zones has weak_ptr<service_proxy>
// Service can lookup but doesn't keep alive ✓
```

**Benefits**:
- **One collection**: Service only has `other_zones` map
- **One mutex**: Minimal locking overhead
- **Natural lifetime**: No self-reference or flags needed
- **Simple lookup**: Same weak_ptr pattern for all proxies
- **Clean code**: No special-case lifetime management

### Scenario 2: Concurrent add_ref/release Through Bridge

**Solution: Transport-Level Operation Serialization**

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

---

## Implementation Phases

### Phase 1: Transport Abstraction (Week 1-2)

**Deliverables:**
- [ ] Define `rpc::transport` abstract base class
- [ ] Update `service` to have `transports_` map (keyed by remote zone)
- [ ] Clarify that `destination_channel_zone` IS the transport identifier
- [ ] Implement transport registration/unregistration mechanism
- [ ] Create `spsc_transport` implementing `transport` interface
- [ ] Refactor `spsc::service_proxy` to delegate to transport

**Testing:**
- Unit tests for channel lifecycle
- Reference counting correctness tests
- All existing SPSC tests must pass

### Phase 2: Symmetrical Service Proxies (Week 3-4)

**Deliverables:**
- [ ] Modify `service::connect_to_zone()` to create channel + proxy pair
- [ ] Modify `service::attach_remote_zone()` to create channel + proxy pair
- [ ] Implement `service::register_channel()` method
- [ ] Update routing logic to use channel-based lookup
- [ ] Remove on-demand proxy creation from `service::send()`

**Testing:**
- Y-topology tests with new architecture
- Verify both directions exist immediately after connection
- Test proxy destruction only when channel destroyed

### Phase 3: Service Sink Implementation (Week 5-6)

**Deliverables:**
- [ ] Define `service_sink` class
- [ ] Implement reference counting logic
- [ ] Implement pass-through proxy management
- [ ] Integration with transports (SPSC, local, TCP, enclave)
- [ ] Service sink registration/tracking

**Testing:**
- Unit tests for sink lifecycle
- Pass-through routing tests
- Circular dependency management tests
- Y-topology tests

### Phase 4: Remaining Transports (Week 7-10)

**TCP Transport:**
- [ ] Create `tcp_transport` class
- [ ] Migrate TCP-specific connection logic
- [ ] Update tests

**Local Transport:**
- [ ] Create `local_transport` class
- [ ] Update tests

**SGX Transport:**
- [ ] Create `sgx_transport` class
- [ ] Handle enclave boundary semantics
- [ ] Update tests

### Phase 5: Cleanup and Optimization (Week 11-12)

**Deliverables:**
- [ ] Remove unused code (old proxy creation patterns)
- [ ] Remove Y-topology workaround (service.cpp:239)
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
   #define RPC_USE_SERVICE_SINK 0
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
       service_sink.h       (new)
       service_proxy_v3.h   (new)
   /rpc/src/v3/
       transport.cpp        (new)
       service_sink.cpp     (new)
       service_proxy_v3.cpp (new)
   ```

4. **Set up continuous integration** (2 days)
   - Run tests on every commit
   - Automated LOC tracking
   - Memory leak detection
   - Performance regression detection

### Validation Gates

**Phase 1 Gate**: Transport abstraction
- [ ] All SPSC tests pass with `spsc_transport`
- [ ] Local and TCP refactored
- [ ] No performance regression

**Phase 2 Gate**: Symmetrical proxies
- [ ] Can create proxy pairs successfully
- [ ] Both directions share transport
- [ ] All connection tests pass

**Phase 3 Gate**: Service Sink
- [ ] Local dispatch mode works
- [ ] Pass-through mode works
- [ ] Circular dependency management correct
- [ ] All Y-topology tests pass

**Phase 4 Gate**: Remaining transports
- [ ] All transports refactored
- [ ] All tests pass
- [ ] Performance acceptable

**Final Release Gate**:
- [ ] All functional requirements met
- [ ] All non-functional requirements met
- [ ] All phase gates passed
- [ ] Documentation complete
- [ ] Performance validated
- [ ] Security review complete

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

#### Risk 3: Service Sink Circular Dependencies

**Description**: 4-object circular dependency in pass-through mode may have subtle lifecycle bugs.

**Probability**: Medium
**Impact**: High
**Mitigation**:
- Extensive unit testing of lifecycle
- Reference counting validation
- Memory leak detection
- Stress testing

**Contingency**: Fallback to simpler non-sink architecture if needed.

### Medium Risk Items

#### Risk 4: Performance Regression from Increased Indirection

**Description**: Extra layers (transport, sink) may slow down hot paths.

**Probability**: Medium
**Impact**: Medium
**Mitigation**:
- Benchmark before/after
- Optimize common paths
- Consider inline hints
- Profile-guided optimization

**Contingency**: Accept small overhead for maintainability, or optimize critical paths.

---

## Success Criteria

### Functional Requirements

✅ **FR1**: Y-topology tests pass WITHOUT workaround at `service.cpp:239`

✅ **FR2**: All service proxies use transport abstraction (SPSC, Local, TCP, SGX)

✅ **FR3**: Service proxy pairs are created symmetrically and share transport

✅ **FR4**: Service sinks handle both local dispatch and pass-through modes

✅ **FR5**: Transport destroyed only when both directions detached

✅ **FR6**: Routing proxies have explicit lifecycle (not ref-counted to zero)

✅ **FR7**: No on-demand service proxy creation in hot paths (send, add_ref, release)

✅ **FR8**: Fire-and-forget messaging with `post()` method and all four post_options

✅ **FR9**: Back-channel support in all i_marshaller methods (send, post, try_cast, add_ref, release)

✅ **FR10**: Child services shut down safely when all three conditions met

✅ **FR11**: DLL unloading waits for child service cleanup to complete

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

---

## Appendices

### Appendix A: Code Examples

#### Example 1: Creating Symmetrical Proxy Pairs

**Before (Asymmetric)**:
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

**After (Symmetric)**:
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

#### Example 2: Service Sink Usage

**Local Dispatch**:
```cpp
// Sink delivers all channel traffic to the local service
auto sink = std::make_shared<service_sink>(service_ptr);
// opposite_proxy_ is nullptr - not configured for pass-through

// When messages arrive from transport:
CO_AWAIT sink->receive_call(msg);  // → dispatches to service_->dispatch_to_stub(msg)

// Service MUST hold shared_ptr to service_proxy
service->register_service_proxy(proxy);
```

**Pass-Through Routing**:
```cpp
// In Zone B (intermediary between A and C):

// Configure bidirectional pass-through:
sink_a_to_c->set_pass_through(proxy_a_to_c);  // Receives from A, forwards to C
sink_c_to_a->set_pass_through(proxy_c_to_a);  // Receives from C, forwards to A

// When counts > 0: 4-object circular dependency exists
// When counts == 0: chain breaks, all objects destroyed
```

### Appendix B: Migration Checklist

#### Pre-Migration
- [ ] Baseline metrics collected
- [ ] Feature flags implemented
- [ ] CI pipeline configured
- [ ] Parallel implementation structure created

#### Phase 1: Transport Abstraction
- [ ] `i_transport` interface defined
- [ ] `transport` base class implemented
- [ ] SPSC refactored to `spsc_transport`
- [ ] Local transport extracted
- [ ] TCP transport extracted
- [ ] SGX transport extracted
- [ ] All transport tests passing

#### Phase 2: Symmetrical Proxies
- [ ] `transport_pair` structure added
- [ ] `service::transports_` map implemented
- [ ] `connect_to_zone()` refactored
- [ ] Routing proxy management implemented
- [ ] All connection tests passing

#### Phase 3: Service Sink
- [ ] `service_sink` class implemented
- [ ] Local dispatch mode working
- [ ] Pass-through mode working
- [ ] Integration with all transports
- [ ] All Y-topology tests passing

#### Phase 4: Remaining Transports
- [ ] All transports migrated
- [ ] All tests passing

#### Phase 5: Cleanup
- [ ] Y-topology workaround removed
- [ ] service_proxy.cpp ≤450 LOC
- [ ] service.cpp ≤1500 LOC
- [ ] All tests passing

#### Final Validation
- [ ] All functional requirements met
- [ ] All non-functional requirements met
- [ ] Performance validated (≤5% overhead)
- [ ] No memory leaks
- [ ] Thread sanitizer clean
- [ ] Security review complete
- [ ] Code review complete

#### Release
- [ ] CHANGELOG updated
- [ ] Version bumped to 3.0
- [ ] Git tag created
- [ ] Release announced

### Appendix C: Glossary

| Term | Definition |
|------|------------|
| **Transport** | Bidirectional communication connection between two zones |
| **Service Proxy** | Lightweight routing object, holds `shared_ptr<transport>` |
| **Service Sink** | Receiver-side abstraction, handles local dispatch and pass-through |
| **Routing Proxy** | Service proxy with zero objects, used for multi-hop routing |
| **Symmetrical Proxies** | Bidirectional service proxy pairs created together |
| **Y-Topology** | Zone hierarchy where Root→A→B but Root has no direct connection to B |
| **Aggregate Ref Count** | Sum of (shared_count + optimistic_count) across all object proxies |
| **Transport Identification** | Transport identified by adjacent zone ID |
| **Pass-Through Mode** | Service sink forwards traffic to opposite service_proxy |
| **Local Dispatch Mode** | Service sink delivers traffic to local service |
| **Circular Dependency** | 4-object cycle: sink→proxy→sink→proxy when counts > 0 (Plan A only) |
| **Pass-Through Object** | Single object handling bidirectional routing for zone pair (Plan B) |
| **Fire-and-Forget** | One-way message sent via `post()` with no response expected |
| **post_options::fire_and_forget** | Send one-way message to object method |
| **post_options::release** | Notify service proxy chain to clean up optimistic reference |
| **post_options::zone_terminating** | Emergency shutdown notification - cleanup all proxies/services for terminating zone |
| **post_options::adhoc** | Service-to-service maintenance messages (certificates, pings, config) - type controlled by interface_id |
| **Back-Channel** | Bidirectional metadata channel for certificates, telemetry, auth tokens in all i_marshaller methods |
| **Cascading Cleanup** | Zone termination triggers cleanup that propagates across zone topology |
| **Ad-hoc Message** | Service-level protocol message not tied to specific objects (keepalive, certificates, etc.) |
| **Shared Reference** | Strong reference that keeps both object stub and proxy alive |
| **Optimistic Reference** | Weak-like reference that keeps proxy/transport alive but NOT stub |

---

**End of Document**
