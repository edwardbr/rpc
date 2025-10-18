<!--
Copyright (c) 2025 Edward Boggis-Rolfe
All rights reserved.
-->

# Service Proxy and Transport Problem Statement

**Version**: 1.0
**Date**: 2025-01-17
**Status**: Problem Analysis

---

## Executive Summary

This document analyzes the current architecture of service proxies, services, and transport mechanisms in RPC++. It identifies emergent design patterns, clarifies terminology, and documents critical problems that lead to race conditions and complexity. This is a **problem statement only** - no solutions are proposed here.

---

## Table of Contents

1. [Current Architecture Overview](#current-architecture-overview)
2. [Type System and Terminology](#type-system-and-terminology)
3. [Emergent Design Patterns](#emergent-design-patterns)
4. [Critical Problems](#critical-problems)
5. [Race Conditions Analysis](#race-conditions-analysis)
6. [Complexity Analysis](#complexity-analysis)

---

## Current Architecture Overview

### Component Responsibilities

#### Service
**Location**: `/rpc/src/service.cpp`, `/rpc/include/rpc/internal/service.h`

**Current Responsibilities**:
- Manages object stubs (local implementations)
- Registers and tracks service proxies
- Routes calls to appropriate service proxy or local stub
- Maintains maps of zones and their connections
- Handles reference counting for distributed objects

**Key Data Structures**:
```cpp
class service {
    // Object management
    std::unordered_map<object, std::weak_ptr<object_stub>> stubs;

    // Zone routing
    struct zone_route {
        destination_zone dest;
        caller_zone source;
    };
    std::map<zone_route, std::weak_ptr<service_proxy>> other_zones;
};
```

#### Service Proxy
**Location**: `/rpc/src/service_proxy.cpp`, `/rpc/include/rpc/internal/service_proxy.h`

**Current Responsibilities**:
1. **Routing**: Determines how to reach destination zone
2. **Transport**: Handles actual communication (serialization, network I/O)
3. **Lifecycle Management**: Reference counting and cleanup
4. **Protocol Negotiation**: Version and encoding fallback
5. **Object Proxy Aggregation**: Maintains map of object proxies

**Key Members**:
```cpp
class service_proxy {
    zone zone_id_;                      // This zone
    destination_zone destination_zone_id_;
    destination_channel_zone destination_channel_zone_;
    caller_zone caller_zone_id_;

    std::unordered_map<object, std::weak_ptr<object_proxy>> proxies_;

    std::atomic<int> lifetime_lock_count_;
    stdex::member_ptr<service_proxy> lifetime_lock_;
};
```

**Problem**: Service proxy has too many responsibilities mixed together.

#### Channel Manager (SPSC Only)
**Location**: `/tests/common/include/common/spsc/channel_manager.h`

The SPSC transport has an **emergent pattern**: a separate `channel_manager` class that encapsulates bidirectional communication.

**Key Features**:
- Manages bidirectional SPSC queues
- Handles send/receive pump tasks
- Tracks in-flight operations
- Reference counts service proxies using it
- Implements graceful shutdown

**This is the "transport" concept emerging naturally in SPSC code.**

---

## Type System and Terminology

### Zone Type Hierarchy

RPC++ uses strongly-typed zone identifiers defined in `/rpc/include/rpc/internal/types.h`:

#### Core Types

**`zone`**: Generic zone identifier (uint64_t wrapper)
- Represents any execution context (process, thread, enclave)
- Can convert to other zone types via `as_destination()`, `as_caller()`, etc.

**`destination_zone`**: The **ultimate** destination of a call
```
In A → B → C → D → E:
  destination_zone = E (final target)
```

**`caller_zone`**: The **original** caller of a call
```
In A → B → C → D → E:
  caller_zone = A (original source)
```

#### Channel/Transport Types

**`destination_channel_zone`**: The **immediate adjacent** zone in the destination direction
```
In A → B → C → D → E, when Zone C is processing:
  destination_channel_zone = D (next hop toward destination)
```

**`caller_channel_zone`**: The **immediate adjacent** zone in the caller direction
```
In A → B → C → D → E, when Zone C is processing:
  caller_channel_zone = B (previous hop from caller)
```

**Key Insight**: The "channel" types identify **point-to-point transports** between adjacent zones.

#### Supporting Types

**`known_direction_zone`**: Hint for routing when direct route is unavailable
- Used to solve Y-topology routing problems
- Indicates which intermediate zone knows the route

**`object`**: Unique identifier for an object within a zone

**`interface_ordinal`**: Identifier for interface type (for casting)

**`method`**: Identifier for method within an interface

### Multi-Hop Routing Example

```
Call Path: A → B → C → D → E

When Zone C is processing the call:

┌─────────────────────────┬───────┬──────────────────────────────────┐
│ Type                    │ Value │ Meaning                          │
├─────────────────────────┼───────┼──────────────────────────────────┤
│ caller_zone             │ A     │ Original caller                  │
│ destination_zone        │ E     │ Final destination                │
│ caller_channel_zone     │ B     │ Adjacent zone (previous hop)     │
│ destination_channel_zone│ D     │ Adjacent zone (next hop)         │
│ zone (current)          │ C     │ This zone processing the call    │
└─────────────────────────┴───────┴──────────────────────────────────┘

Zone C Transports:
  - Transport to B (adjacent_zone = B)
  - Transport to D (adjacent_zone = D)

Routing Decision:
  - Look up transport using destination_channel_zone (= D)
  - Forward call on transport to D
  - D will then forward to E
```

### Object Reference Counting

**Shared References** (`shared_count`): Strong reference counted pointers
- Increment on add_ref with `add_ref_options::normal`
- Decrement on release with `release_options::normal`
- **Object stub destroyed when shared_count reaches zero** (regardless of optimistic_count)

**Optimistic References** (`optimistic_count`): Weak-like references for transport keepalive
- Increment on add_ref with `add_ref_options::optimistic`
- Decrement on release with `release_options::optimistic`
- **Object proxy kept alive while optimistic_count > 0** (even if shared_count = 0)
- **Purpose**: Keeps transport connection alive without preventing object stub destruction

**Critical Lifecycle Distinction**:
```
Object Stub (remote object):
  - Destroyed when shared_count == 0
  - Optimistic references do NOT keep stub alive
  - Rationale: Remote object should be destroyed when no strong references remain

Object Proxy (local reference):
  - Kept alive while (shared_count > 0 OR optimistic_count > 0)
  - Rationale: Optimistic references keep transport alive for future use

Service Proxy:
  - Currently destroyed when aggregate ref count = 0 for all objects
  - Aggregate = sum of (shared_count + optimistic_count) across all object proxies
```

**Example Scenario**:
```
Zone A has object_proxy to remote object in Zone B
shared_count = 0, optimistic_count = 5

Result:
- Object stub in Zone B is DESTROYED (shared_count = 0)
- Object proxy in Zone A remains ALIVE (optimistic_count > 0)
- Service proxy A→B remains ALIVE (aggregate > 0)
- Transport connection A↔B stays open for future calls
```

---

## Emergent Design Patterns

### Pattern 1: Channel Manager (SPSC)

The SPSC transport has organically developed a separation of concerns:

**SPSC Service Proxy**: Routing and lifecycle
**SPSC Channel Manager**: Transport and communication

```cpp
class spsc_service_proxy : public service_proxy {
    std::shared_ptr<channel_manager> channel_manager_;

    CORO_TASK(int) send(...) override {
        // Delegates to channel_manager
        CO_RETURN CO_AWAIT channel_manager_->call_peer(...);
    }
};

class channel_manager {
    // Bidirectional communication
    queue_type* send_spsc_queue_;
    queue_type* receive_spsc_queue_;

    // Reference counting for service proxies
    std::atomic<int> service_proxy_ref_count_{0};
    std::atomic<int> tasks_completed_{0};

    // Lifecycle management
    std::shared_ptr<channel_manager> keep_alive_;

    // Operations
    void attach_service_proxy();
    CORO_TASK(void) detach_service_proxy();
    CORO_TASK(void) shutdown();
};
```

**This pattern shows the "transport" concept emerging naturally.**

### Pattern 2: Bidirectional Service Proxies

Service proxies are **implicitly bidirectional** but created asymmetrically:

```cpp
// Forward direction: Zone A → Zone B
auto forward_proxy = service_a->connect_to_zone<...>(zone_b, ...);

// Reverse direction: Zone B → Zone A (created on-demand!)
// No explicit creation - happens reactively during:
// - add_ref operations
// - Object passing back from B to A
```

**Problem**: Asymmetric creation leads to race conditions (see service.cpp:239).

### Pattern 3: Routing Proxies (Zero Object References)

In bridging scenarios, service proxies act as pure routers:

```
Zone A ← → Zone B ← → Zone C
          (Bridge)

Zone B has three service proxy pairs:
  1. B ← → A (may have objects)
  2. B ← → C (may have objects)
  3. A ← → C (routing only, NO objects!)
```

**The A↔C proxy in Zone B has zero object references but must stay alive for routing.**

**Current Problem**: Service proxies destroyed when object ref count = 0, but routing proxies need different lifecycle.

### Pattern 4: Transport Identification via Adjacent Zone

Transports are **implicitly** identified by their adjacent zone:

```cpp
// Zone C has these transports in its connections:
// - Transport to B: identified by adjacent_zone = B
// - Transport to D: identified by adjacent_zone = D

// When routing A→B→C→D→E:
// destination_channel_zone = D → Look up transport to D
```

**The `destination_channel_zone` and `caller_channel_zone` types ARE transport identifiers.**

---

## Critical Problems

### Problem 1: The Y-Topology Race Condition

**Location**: `service.cpp:239-250`

**Scenario**:
```
Root (Zone 1)
 ├─ Prong A (Zone 2)
 └─ Prong B (Zone 3) ← Created by Prong A

Timeline:
1. Prong A creates Prong B
2. Prong B creates object and passes to Prong A
3. Prong A returns object to Root
4. Root has no service proxy to Zone 3!
5. Root attempts add_ref on object in Zone 3
6. RACE: Service proxy must be created on-the-fly during critical path
```

**Current Workaround** (lines 239-250):
```cpp
// NOTE there is a race condition that will be addressed in the
// coroutine changes coming later
//
// note this is to address a bug found whereby we have a Y shaped graph
// whereby one prong creates a fork below it to form another prong this
// new prong is then ordered to pass back to the first prong an object.
// this object is then passed back to the root node which is unaware of
// the existence of the second prong.

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

**Why This Is A Problem**:
- Creates service proxies **reactively** during critical operations
- Race window between proxy creation and `add_external_ref()`
- Null pointer checks scatter throughout codebase
- Violates single responsibility (send() managing proxy lifecycle)
- Tests that fail without this workaround:
  - `test_y_topology_and_return_new_prong_object`
  - `test_y_topology_and_cache_and_retrieve_prong_object`
  - `test_y_topology_and_set_host_with_prong_object`

### Problem 2: Mixed Responsibilities in Service Proxy

**Current service_proxy Handles**:
1. **Routing Logic**: Determining path to destination
2. **Transport Logic**: Serialization, network I/O, queues
3. **Lifecycle Management**: Reference counting, cleanup
4. **Protocol Negotiation**: Version/encoding fallback
5. **Object Aggregation**: Maintaining object proxy map

**Impact**:
- Hard to test each concern in isolation
- Changes to transport affect routing logic
- Adding new transports requires duplicating routing logic
- ~800 LOC in service_proxy.cpp (should be ~300-400)

**Evidence of Problem**:
- SPSC has separate `channel_manager` (emergent pattern)
- TCP transport will need similar separation
- Local/SGX transports have no clear transport abstraction

### Problem 3: Asymmetric Service Proxy Creation

**Current Pattern**:
```cpp
// Forward direction: Explicit
auto sp_a_to_b = service_a->connect_to_zone<...>(zone_b, ...);

// Reverse direction: Implicit (created on-demand)
// Happens during:
// - First add_ref from B to A
// - First object passing from B to A
// - send() operation (see Problem 1)
```

**Problems**:
- Service proxy in reverse direction doesn't exist until first use
- Lifetime unpredictable (destroyed when ref count = 0)
- Cannot reason about bidirectional connectivity
- Race when both directions try to create simultaneously

**Impact on Routing Proxies**:
```
Zone A → Zone B → Zone C

// Zone B might have:
// - A→B proxy (has objects)
// - B→A proxy (created on-demand, may have been destroyed)
// - A→C routing proxy (NO objects, destroyed prematurely!)
```

### Problem 4: Service Proxy Lifetime vs Transport Lifetime

**Current Behavior**:
```cpp
service_proxy::~service_proxy() {
    // When aggregate ref count = 0, proxy destroyed
    // This may destroy transport connection prematurely!
}
```

**Scenario**:
```
Service Proxy A→B:
  - Object 1: shared_count = 0, optimistic_count = 3
  - Object 2: shared_count = 1, optimistic_count = 2
  - Aggregate: 6 (proxy alive)

Service Proxy B→A:
  - No objects
  - Aggregate: 0 (proxy destroyed)

Timeline:
T1: B→A service proxy destroyed (aggregate = 0)
T2: A→B still alive with aggregate = 6
T3: Zone B tries to call method on object in Zone A
T4: ERROR: No service proxy B→A exists to route the call!

Critical Issue:
- Object stubs in Zone A are alive (shared_count > 0 for Object 2)
- Object proxies in Zone B are alive (optimistic_count > 0 for both)
- Transport A↔B asymmetric: only A→B direction exists
- Zone B cannot send calls back to Zone A
```

**The Problem**: Transport lifetime tied to individual service proxy, not the bidirectional pair.

**Impact on Optimistic References**:
```
Optimistic references keep object proxies alive to maintain transport.
But if reverse service proxy is destroyed, transport is one-directional!

Example:
Zone A → Zone B (alive with optimistic refs)
Zone B → Zone A (destroyed, no reverse path!)

Optimistic references become useless - cannot send future calls.
```

### Problem 5: No Explicit Transport Abstraction

**Current State**:
- SPSC: Has `channel_manager` (emergent)
- TCP: Transport implicit in service proxy
- Local: Direct function calls, no abstraction
- SGX: Enclave boundary crossing, no abstraction

**Problem**: Cannot share transport across multiple service proxy pairs.

**Example**:
```cpp
// Want: Multiple object streams share one TCP connection
// Reality: Each service proxy potentially creates its own connection

auto sp1 = connect_to_zone<tcp::service_proxy>(zone_b, ...);  // Connection 1
auto sp2 = connect_to_zone<tcp::service_proxy>(zone_b, ...);  // Connection 2 (wasteful!)
```

### Problem 6: No Fire-and-Forget Messaging Support

**Current State**:
- All RPC calls are request/response (synchronous or async)
- No mechanism for fire-and-forget messages
- Optimistic reference cleanup requires full RPC round-trip

**Missing Feature**: `i_marshaller` needs a `post()` method for one-way messages AND all methods need back-channel support.

**Required Interface Changes**:
```cpp
class i_marshaller {
public:
    // UPDATED: send() method with back-channel support (request/response)
    virtual CORO_TASK(int) send(
        // ... existing parameters ...
        size_t in_size_,
        const char* in_buf_,
        std::vector<char>& out_buf_,

        // NEW: Back-channel parameters
        size_t in_back_channel_size_,
        const char* in_back_channel_buf_,
        std::vector<char>& out_back_channel_buf_
    ) = 0;

    // NEW: post() method (fire-and-forget, uni-directional - no out_back_channel_buf_)
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

        // NEW: Back-channel parameter (send only, no receive)
        size_t in_back_channel_size_,
        const char* in_back_channel_buf_
    ) = 0;

    // UPDATED: try_cast() with back-channel support
    virtual CORO_TASK(error_code) try_cast(
        // ... existing parameters ...

        // NEW: Back-channel parameters
        size_t in_back_channel_size_,
        const char* in_back_channel_buf_,
        std::vector<char>& out_back_channel_buf_
    ) = 0;

    // UPDATED: add_ref() with back-channel support
    virtual CORO_TASK(error_code) add_ref(
        // ... existing parameters ...

        // NEW: Back-channel parameters
        size_t in_back_channel_size_,
        const char* in_back_channel_buf_,
        std::vector<char>& out_back_channel_buf_
    ) = 0;

    // UPDATED: release() with back-channel support
    virtual CORO_TASK(error_code) release(
        // ... existing parameters ...

        // NEW: Back-channel parameters
        size_t in_back_channel_size_,
        const char* in_back_channel_buf_,
        std::vector<char>& out_back_channel_buf_
    ) = 0;
};

enum class post_options {
    fire_and_forget,      // One-way message to object method
    release,              // Optimistic reference cleanup notification
    zone_terminating,     // Emergency shutdown notification
    adhoc                 // Service-to-service maintenance messages
};
```

**Back-Channel Data Format**:

The back-channel data will be defined as a list of key-value pairs similar to HTTP headers

**Use Cases**:

**1. Fire-and-Forget Calls**:
```cpp
// Send notification without waiting for response
CO_AWAIT proxy->post(
    protocol_version,
    encoding,
    tag,
    caller_channel_zone_id,
    caller_zone_id,
    destination_zone_id,
    object_id,
    interface_id,
    method_id,
    post_options::fire_and_forget,
    in_size,
    in_buf
);
// Returns immediately, no response expected
```

**2. Optimistic Reference Cleanup**:
```cpp
// Notify service proxy chain that optimistic reference is being released
if (optimistic_count > 0) {
    // Only send to service_proxy/sinks with positive optimistic count
    CO_AWAIT proxy->post(
        protocol_version,
        encoding,
        tag,
        caller_channel_zone_id,
        caller_zone_id,
        destination_zone_id,
        object_id,
        interface_id,
        method_id,
        post_options::release,  // Special cleanup indicator
        0,
        nullptr
    );
}
```

**Critical Requirement for `post_options::release`**:
- Must ONLY be sent to service_proxy/sinks that have `optimistic_count > 0` for that object
- Allows service proxy chain to clean up optimistic references proactively
- Avoids keeping transport connections alive unnecessarily
- More efficient than full RPC round-trip for cleanup

**3. Zone Termination (Emergency Shutdown)**:
```cpp
// Service is shutting down (even in emergency) - notify ALL service proxies/sinks
for (auto& [zone_id, proxy] : all_service_proxies) {
    CO_AWAIT proxy->post(
        protocol_version,
        encoding,
        tag,
        caller_channel_zone_id,
        caller_zone_id,
        zone_id,
        object{0},  // Not specific to any object
        interface_ordinal{0},
        method{0},
        post_options::zone_terminating,  // Emergency shutdown
        0,
        nullptr
    );
}

// After sending zone_terminating:
// - NO MORE MESSAGES transmitted except cleanup
// - All service_sinks cleanup their service_proxies and services
```

**Critical Behavior for `post_options::zone_terminating`**:

**Shutdown Cascade**:
```
Service A shutting down:
1. Send zone_terminating to ALL connected service proxies/sinks
2. Recipients clean up their service_proxies and services
3. Cleanup may trigger cascade of release events in opposite direction
4. Release events destined for Service A are IGNORED (service already down)
5. Cleanup continues AS IF messages to Service A succeeded
```

**No-Message Mode After Termination**:
- **Before zone_terminating sent**: Normal RPC operations allowed
- **After zone_terminating sent**: Only cleanup messages allowed (no new RPCs)
- **When zone_terminating received**: Enter cleanup mode, ignore subsequent releases from that zone

**Cascading Cleanup Pattern**:
```
Zone A (terminating) → sends zone_terminating → Zone B
                                                  ↓
                                          Zone B cleans up proxies to A
                                                  ↓
                                          Triggers release events
                                                  ↓
                                          Releases destined for A → IGNORED
                                                  ↓
                                          Cleanup proceeds as if successful
```

**Service Sink Responsibility**:
```cpp
CORO_TASK(void) receive_call(const marshalled_message& msg) {
    if (msg.is_post_message()) {
        if (msg.post_options == post_options::zone_terminating) {
            // Emergency shutdown notification received
            mark_zone_as_terminated(msg.caller_zone_id);

            // Clean up ALL service_proxies to that zone
            cleanup_all_proxies_to_zone(msg.caller_zone_id);

            // Clean up ALL services referencing that zone
            cleanup_all_services_referencing_zone(msg.caller_zone_id);

            // Ignore future release events from that zone
            // No response sent
        } else if (msg.post_options == post_options::release) {
            // Skip if zone already terminated
            if (!is_zone_terminated(msg.caller_zone_id)) {
                cleanup_optimistic_ref(msg.object_id);
            }
            // No response sent
        } else {
            // Fire-and-forget call
            CO_AWAIT dispatch_to_local_method(msg);
            // No response sent
        }
    } else {
        // Regular RPC call - dispatch and send response
        auto result = CO_AWAIT dispatch_to_local_method(msg);
        CO_AWAIT send_response(result);
    }
}
```

**Guarantees**:
1. **All connected zones notified**: Even in emergency shutdown, zone_terminating sent to all
2. **Cleanup proceeds**: Even if remote zone is down, local cleanup continues
3. **No deadlocks**: Ignore release events from terminated zones
4. **Cascading cleanup**: Each zone cleans up its own resources, triggering further cleanup
5. **Idempotent**: Multiple zone_terminating messages handled gracefully
6. **Partial tree survival**: If a branch in zone tree dies, rest of tree continues to exist so long as references for remaining zones are correct

**Critical Design Principle - Isolated Branch Termination**:
```
Zone Tree:
         Root (Zone 1)
           ├─── Branch A (Zone 2)
           │      ├─── Leaf A1 (Zone 3)
           │      └─── Leaf A2 (Zone 4)
           └─── Branch B (Zone 5)
                  └─── Leaf B1 (Zone 6)

Scenario: Branch A (Zone 2) terminates
1. Zone 2 sends zone_terminating to: Root, Leaf A1, Leaf A2
2. Root cleans up proxies to Zone 2, 3, 4 (entire Branch A)
3. Branch B (Zone 5) and Leaf B1 (Zone 6) UNAFFECTED
4. Root ↔ Branch B connection remains active
5. Branch B continues normal operations

Result: Branch A subtree removed, Branch B subtree continues
```

**Reference Correctness Requirement**:
- **Before termination**: Root has references to Zones 2, 3, 4, 5, 6
- **After Zone 2 termination**: Root cleans up references to Zones 2, 3, 4
- **Remaining references**: Root retains valid references to Zones 5, 6
- **Tree still valid**: Root → Branch B → Leaf B1 continues functioning

**Why This Matters**:
- Zone termination is LOCALIZED to affected branch
- Unrelated zones remain operational
- References are cleaned up only for terminated zones
- No "poison pill" effect spreading across entire topology

**4. Ad-hoc Service-to-Service Messages**:
```cpp
// Send maintenance message between services
CO_AWAIT proxy->post(
    protocol_version,
    encoding,
    tag,
    caller_channel_zone_id,
    caller_zone_id,
    destination_zone_id,
    object{0},           // May be used if necessary
    interface_id,        // Controls type of ad-hoc message
    method{0},           // May be used if necessary
    post_options::adhoc,
    in_size,
    in_buf
);
```

**Use Cases for `post_options::adhoc`**:

**Network Certificate Management**:
```cpp
// Service A sends updated certificate to Service B
interface_ordinal cert_mgmt_interface{1000};  // Certificate management
CO_AWAIT proxy->post(
    protocol_version,
    encoding,
    tag,
    caller_channel_zone_id,
    caller_zone_id,
    destination_zone_id,
    object{0},
    cert_mgmt_interface,  // Identifies this as certificate message
    method{1},            // 1 = update_certificate
    post_options::adhoc,
    cert_size,
    cert_data
);
```

**Connection Keepalive/Ping**:
```cpp
// Periodic ping to keep connection alive
interface_ordinal keepalive_interface{1001};  // Keepalive management
CO_AWAIT proxy->post(
    protocol_version,
    encoding,
    tag,
    caller_channel_zone_id,
    caller_zone_id,
    destination_zone_id,
    object{0},
    keepalive_interface,  // Identifies this as ping message
    method{0},            // 0 = ping
    post_options::adhoc,
    0,
    nullptr
);
```

**Configuration Updates**:
```cpp
// Send configuration change to remote service
interface_ordinal config_interface{1002};  // Configuration management
CO_AWAIT proxy->post(
    protocol_version,
    encoding,
    tag,
    caller_channel_zone_id,
    caller_zone_id,
    destination_zone_id,
    object{0},
    config_interface,     // Identifies this as config message
    method{2},            // 2 = update_config
    post_options::adhoc,
    config_size,
    config_data
);
```

**Critical Design for `post_options::adhoc`**:
- **interface_id**: Identifies the type/category of ad-hoc message
- **object_id**: Optional - may be used if message relates to specific object
- **method_id**: Optional - may be used to specify subtype or action within interface category
- **Not tied to IDL**: Ad-hoc messages are service-level, not object-level
- **Extensible**: New message types can be added by defining new interface_ordinal values

**Service Sink Handling**:
```cpp
CORO_TASK(void) receive_call(const marshalled_message& msg) {
    if (msg.is_post_message()) {
        if (msg.post_options == post_options::adhoc) {
            // Route to service-level ad-hoc message handler
            switch (msg.interface_id.get_val()) {
                case 1000:  // Certificate management
                    CO_AWAIT handle_certificate_message(msg);
                    break;
                case 1001:  // Keepalive
                    CO_AWAIT handle_keepalive_message(msg);
                    break;
                case 1002:  // Configuration
                    CO_AWAIT handle_config_message(msg);
                    break;
                default:
                    // Unknown ad-hoc message type
                    RPC_WARNING("Unknown adhoc message type: {}", msg.interface_id.get_val());
            }
            // No response sent (fire-and-forget)
        } else if (msg.post_options == post_options::zone_terminating) {
            // ... zone termination handling ...
        }
        // ... other post options ...
    }
    // ... regular RPC handling ...
}
```

**Benefits of Ad-hoc Messages**:
1. **Connection Maintenance**: Keep TCP connections alive with periodic pings
2. **Security**: Update certificates and credentials without RPC overhead
3. **Configuration**: Propagate configuration changes across zones
4. **Monitoring**: Send health checks and status updates
5. **Extensibility**: Add new service-level protocols without modifying core RPC
6. **No Object Dependency**: Works even when no objects exist between services

**Benefits**:
1. **Performance**: Fire-and-forget messages don't wait for response
2. **Efficient Cleanup**: Optimistic reference cleanup without RPC overhead
3. **Resource Management**: Proactive transport connection cleanup
4. **Targeted Notifications**: Only notify service proxies that need to know
5. **Graceful Emergency Shutdown**: Zone termination notification enables clean multi-zone shutdown
6. **Deadlock Prevention**: Ignoring releases from terminated zones prevents cleanup deadlocks
7. **Cascading Cleanup**: Automatic propagation of cleanup across zone topology
8. **Service-Level Protocols**: Ad-hoc messages enable connection maintenance, security updates, and monitoring without object-level RPC overhead
9. **Back-Channel Support**: All i_marshaller methods support bidirectional metadata (certificates, telemetry, auth tokens)
10. **HTTP-Header Style Metadata**: Flexible key-value pairs with optional encryption for secure data transmission
11. **OpenTelemetry Integration**: Native support for distributed tracing context propagation
12. **Security Infrastructure**: Certificate chain transmission and authentication token support built into protocol

### Problem 7: No Receiver-Side Abstraction (Service Sink Missing)

**Current State**:
- Channel-based transports (SPSC, TCP) use sender/receiver design pattern suitable for async calls
- Receiver logic is embedded in service or service_proxy
- No unified abstraction for receiving and dispatching demarshalled calls
- Pass-through routing requires special-case code

**Problem**: No counterpart to service_proxy on the receiver side.

**Needed Architecture**:
```
Sender Side:     service_proxy (routes and sends)
Receiver Side:   service_sink (receives and dispatches)  ← MISSING!
```

**Impact on Local Calls**:
- Direct local calls may not need full sender/receiver pattern
- But having uniform architecture across all transports would simplify design
- Local transport could use sink for consistency even without channel object

**Impact on Pass-Through Routing**:
```
Zone A ←→ Zone B ←→ Zone C

Zone B needs to:
1. Receive from A → Either dispatch to local service OR forward to C
2. Receive from C → Either dispatch to local service OR forward to A

Current: Complex conditional logic in service::send()
Needed: service_sink that can:
  - Dispatch to local service when endpoint
  - Forward to opposite service_proxy when intermediary
```

**Lifecycle Problem**:
```cpp
// Non-pass-through case:
service_sink receives from channel → dispatches to service
Problem: No circular reference to keep service_proxy alive
Solution: Service MUST hold shared_ptr to service_proxy

// Pass-through case:
service_sink receives from channel → forwards to opposite_proxy
Problem: Creates 4-object circular dependency
Solution: Conditional shared_ptr based on reference counts
```

**Missing Features**:
1. **Unified receiver abstraction** for all transport types
2. **Dual-mode dispatch**: Local vs pass-through
3. **Automatic lifecycle management** based on reference counts
4. **Service tracking** of all sinks for cleanup
5. **Single concrete class** instead of per-transport receiver logic

**Current Workaround**: Receiver logic scattered across:
- `service::dispatch_to_stub()` for local dispatch
- Channel manager pump tasks for SPSC
- Service proxy for routing decisions
- No unified pattern

---

## Race Conditions Analysis

### Race 1: Concurrent add_ref/release Through Bridge

**Scenario**:
```
Topology: Zone A ← → Zone B ← → Zone C
          (Bridge)

Timeline:
T1: Zone A releases last reference to object in Zone C
    → send release message through Zone B
T2: Zone C creates new reference to same object
    → send add_ref message through Zone A (via B)
T3: Both messages arrive at Zone B simultaneously

Race Conditions:
- Zone B's routing proxy might be destroyed between T1 and T2
- Operations on same object not serialized
- Mutexes cannot span zones (deadlock risk)
- Coroutine yields allow interleaving
```

**Code Example**:
```cpp
// Zone A (release path)
CORO_TASK(void) zone_a_release_object() {
    uint64_t ref_count;
    auto error = CO_AWAIT proxy_to_c->release(..., ref_count);
    // Travels through Zone B's routing proxy
}

// Zone C (add_ref path)
CORO_TASK(void) zone_c_add_ref_object() {
    uint64_t ref_count;
    auto error = CO_AWAIT proxy_to_a->add_ref(..., ref_count);
    // Also travels through Zone B's routing proxy
}

// These can collide in Zone B!
```

**Current Mitigation**: None - race condition exists.

### Race 2: Service Proxy Destruction During Active Call

**Scenario**:
```
Timeline:
T1: Zone A initiates call to Zone C through Zone B
T2: Zone B's routing proxy begins processing call
T3: Different thread: Last object released in Zone B
T4: Zone B's routing proxy destructor runs
T5: Thread from T2 tries to use destroyed proxy

Result: Use-after-free bug!
```

**Current Mitigation**: `lifetime_lock_` mechanism (complex and fragile).

### Race 3: Transport Destruction with Service Proxy Pair

**Scenario** (Current SPSC):
```
Service Proxy A→B: 1 object (about to be released)
Service Proxy B→A: 0 objects (already destroyed)

Timeline:
T1: Thread 1 releases last object in A→B
T2: Thread 1 begins A→B proxy destructor
T3: Thread 2 is in B→A proxy destructor
T4: Both threads release shared_ptr<channel_manager>
T5: Channel manager destructor - which thread context?

Race: Must ensure shutdown only happens once
```

**Current Mitigation**: SPSC channel_manager has `atomic<bool> shutdown_initiated_`.

### Race 4: Mutex Ordering Deadlock

**Scenario**:
```
Zone A: mutex_a
Zone B: mutex_b
Zone C: mutex_c

Bad Pattern:
Thread 1: Lock mutex_a → RPC to B → Lock mutex_b → RPC to C → Lock mutex_c
Thread 2: Lock mutex_c → RPC to B → Lock mutex_b → RPC to A → Lock mutex_a

Result: DEADLOCK (cycle in lock order graph)
```

**Current Requirement**: Mutexes must be released before RPC calls (not always followed).

### Race 5: Coroutine Yield with Lock Held

**Scenario**:
```cpp
CORO_TASK(void) dangerous_pattern() {
    std::lock_guard g(mutex_);

    auto value = shared_state_;

    CO_AWAIT some_rpc_call();  // ❌ YIELD POINT with mutex held!

    shared_state_ = value + 1;
}

// Another coroutine can't acquire mutex while this one suspended!
// Deadlock if both coroutines need the same mutex.
```

**Current Requirement**: Must release mutex before `CO_AWAIT` (manual verification).

### Race 6: Weak Pointer Lock Failures

**Scenario**:
```cpp
// service.cpp common pattern:
auto found = other_zones.find({dest, caller});
auto proxy = found->second.lock();  // Returns nullptr!

// Service proxy destroyed between find and lock
```

**Current Handling**: Reactive null pointer checks throughout code.

**Problem**: Indicates service proxy lifetime management is unpredictable.

---

## Complexity Analysis

### Code Complexity Metrics

**Service Class** (`service.cpp`):
- **~1900 LOC** (should be ~1200)
- **4 mutexes**: `stub_control`, `service_events_control`, `zone_control`, (implicit in maps)
- **Complex operations**: send(), add_ref(), release() each 100+ LOC
- **Responsibility overload**: Manages stubs, proxies, routing, and reference counting

**Service Proxy Class** (`service_proxy.cpp`):
- **~800 LOC** (should be ~300-400)
- **3 mutexes**: `proxies_mutex_`, `insert_control_`, (internal to operations)
- **Mixed responsibilities**: Routing + Transport + Lifecycle + Protocol
- **Complex lifetime management**: `lifetime_lock_`, `lifetime_lock_count_`, reference counting

**SPSC Channel Manager**:
- **~500 LOC**
- Separate from service_proxy (good!)
- Shows what "transport" should look like
- Has own reference counting, lifecycle, shutdown

### Conceptual Complexity

**Zone Type System**:
- **5 different zone types** (zone, destination_zone, caller_zone, destination_channel_zone, caller_channel_zone)
- **Not obvious**: Which to use when
- **Poor naming**: "channel" types actually identify adjacent zones
- **Every signature**: Must carry 3-4 different zone parameters

**Service Proxy Lifecycle**:
```
Creation:
  - Explicit: connect_to_zone()
  - Implicit: On-demand during send/add_ref
  - Cloning: clone_for_zone() for routing

Lifetime:
  - lifetime_lock_ mechanism
  - lifetime_lock_count_ tracking
  - aggregate object ref counts
  - Destroyed when count = 0 (sometimes wrong for routing!)

Destruction:
  - Cleanup object proxies
  - Remove from service's other_zones map
  - Release lifetime_lock_
  - May trigger transport destruction
```

**Routing Logic**:
```cpp
// To send a call, must determine:
// 1. Is destination local? (destination_zone == zone_id)
// 2. Do we have direct route? (other_zones.find({dest, caller}))
// 3. Do we have route via channel? (other_zones.find({channel, caller}))
// 4. Must we create route? (clone_for_zone())
// 5. Is opposite direction needed? (Y-topology workaround)

// Current implementation: ~150 lines of nested ifs in service::send()
```

### Maintenance Burden

**Adding New Transport**:
Currently requires:
1. Create new `XXX_service_proxy` subclass
2. Implement all i_marshaller operations (send, try_cast, add_ref, release)
3. Handle protocol negotiation and fallback
4. Manage version and encoding
5. Duplicate routing logic
6. Handle reference counting
7. Implement cleanup

**Should require**:
1. Create transport implementation (send/receive only)
2. Done

**Testing Challenges**:
- Cannot unit test routing without transport
- Cannot unit test transport without routing
- Cannot test lifecycle without both
- Integration tests required for everything
- Race conditions hard to reproduce

---

## Current System Invariants

### Invariant 1: Zone Uniqueness
Each zone has unique `zone_id` generated by `service::zone_id_generator`.

### Invariant 2: Object ID Uniqueness
Objects have unique IDs within their zone (generated by `service::object_id_generator`).

### Invariant 3: Service Proxy Pair Symmetry (Violated!)
**Should be**: For every A→B proxy, there exists B→A proxy.
**Actually**: B→A created on-demand, may not exist or be destroyed.

### Invariant 4: Routing Consistency
A call from A to C through B must use:
- `caller_zone` = A
- `destination_zone` = C
- `caller_channel_zone` = B (if B is intermediate)
- `destination_channel_zone` = Next hop from current zone

### Invariant 5: Reference Count Accuracy

**Object Stub Lifetime**:
- Object stub destroyed when `shared_count == 0` (regardless of `optimistic_count`)
- Rationale: Optimistic references only keep transport alive, not the remote object

**Object Proxy Lifetime**:
- Object proxy kept alive while `(shared_count > 0 OR optimistic_count > 0)`
- Rationale: Optimistic references maintain transport connection for future use

**Service Proxy Lifetime**:
- Service proxy destroyed when aggregate ref count = 0 across all object proxies
- Aggregate = sum of `(shared_count + optimistic_count)` for all objects
- **Problem**: Service proxy destroyed even if it's a routing proxy with zero objects

### Invariant 6: Mutex Discipline (Not Enforced)
**Should be**: All mutexes released before RPC calls or `CO_AWAIT`.
**Actually**: Manual verification required, violations possible.

---

## Dependencies and External Factors

### Coroutine vs Non-Coroutine Builds

RPC++ supports two build modes:

**`#ifdef BUILD_COROUTINE`**:
- Uses `coro::task<T>` for async operations
- Operations can yield with `CO_AWAIT`
- Requires scheduler for task execution
- Mutexes can deadlock if held across `CO_AWAIT`

**Non-coroutine (blocking)**:
- Synchronous operations
- No yield points
- Simpler concurrency model
- But: Limited scalability

**Impact on Design**:
- All lifecycle management must work in both modes
- Destructors cannot use `CO_AWAIT` (must use `sync_wait` or schedule cleanup)
- Reference counting must be thread-safe

### Transport Types and Requirements

**Local Service Proxy** (`local_service_proxy` and `local_child_service_proxy`):
- **Transport**: Direct function calls within process
- **Serialization**: No serialization needed (native C++ objects)
- **Execution Mode**: Bi-modal - works both synchronously AND with coroutines
- **Wiring**: Simplest - minimal infrastructure needed
- **Use Case**: Parent-child service communication, in-process zones
- **Lifecycle**: `local_child_service_proxy` used from parent to child, `local_service_proxy` from child to parent
- **Key Feature**: No channel object required - direct function dispatch

**SPSC** (Single Producer Single Consumer):
- **Transport**: Lock-free queues between zones
- **Serialization**: Full serialization required
- **Execution Mode**: Coroutine-only (requires async pump tasks)
- **Wiring**: Requires channel object to manage sender/receiver pairs
- **Use Case**: In-process or inter-process with shared memory
- **Channel Manager**: Has `channel_manager` (emergent transport abstraction)
- **Reliability**: Must handle queue full/empty conditions
- **Key Feature**: Most mature transport implementation with explicit channel object

**TCP** (`tcp::service_proxy`):
- **Transport**: Network sockets
- **Serialization**: Full serialization required
- **Execution Mode**: Coroutine-only (requires async I/O)
- **Wiring**: Requires channel object to manage sender/receiver pairs
- **Use Case**: Network-based communication between machines
- **Reliability**: Must handle unreliable transport - connection loss, recovery, reconnection
- **Channel Manager**: Needs channel object similar to SPSC
- **Key Feature**: Contends with network failures and connection lifecycle

**SGX Enclave** (`enclave_service_proxy` and `host_service_proxy`):
- **Transport**: Intel Software Guard Extensions boundary crossing
- **Serialization**: Secure marshalling across enclave boundary
- **Execution Mode**: Hybrid - depends on coroutine mode
  - **Synchronous Mode**: Direct SGX ecall/ocall (inefficient, thread-limited)
  - **Coroutine Mode**: Initial ecall to create executor + SPSC channel, then use SPSC for all communication
- **Wiring**:
  - Sync mode: No channel object (direct ecalls)
  - Coroutine mode: Requires SPSC channel object after initial setup
- **Reliability**: Enclave thread pool limitations in sync mode
- **Key Feature**: Shares channel between both proxy directions (host_service_proxy shares enclave_service_proxy's channel)

**Key Distinctions Summary**:
```
┌─────────────────┬──────────────┬─────────────┬──────────────┬─────────────────┐
│ Transport Type  │ Coroutine    │ Channel     │ Serialization│ Reliability     │
│                 │ Required?    │ Object?     │ Required?    │ Concerns        │
├─────────────────┼──────────────┼─────────────┼──────────────┼─────────────────┤
│ Local           │ Optional     │ No          │ No           │ None (in-proc)  │
│ SPSC            │ Yes          │ Yes         │ Yes          │ Queue bounds    │
│ TCP             │ Yes          │ Yes         │ Yes          │ Network failure │
│ Enclave (Sync)  │ No           │ No          │ Yes          │ Thread limits   │
│ Enclave (Coro)  │ Yes          │ Yes (SPSC)  │ Yes          │ Initial setup   │
└─────────────────┴──────────────┴─────────────┴──────────────┴─────────────────┘
```

### Child Services and Parent-Child Relationships

**Location**: `rpc::child_service` class

**Architecture**:
- **Child services** are zones that are children of other services
- **Parent service proxy keepalive**: Child holds `shared_ptr<local_service_proxy>` to parent to keep parent alive
- **Lifecycle dependency**: Child's lifetime tied to parent's availability

**Shutdown Conditions**:

A child service shuts down when ALL three conditions are met:
1. **All external shared references to the child == 0** (no outside zones holding strong references - incoming optimistic references do NOT prevent shutdown)
2. **All internal shared AND optimistic references from the child == 0** (child released ALL references to other zones - outgoing optimistic references DO prevent shutdown)
3. **Not acting as routing intermediary** (not in the middle of any active routes between other zones)

**Critical Distinction**:
- **Incoming optimistic references** (other zones → this service): Do NOT prevent shutdown. The service may die even if other zones hold optimistic references to it.
- **Outgoing optimistic references** (this service → other zones): DO prevent shutdown. If the service has active optimistic pointers to remote objects, it may not shut down.

**Cleanup Sequence**:
```
When shutdown conditions met:
1. Complete all pending operations
2. Release all resources
3. Release parent_service_proxy_ (may trigger parent cleanup)
4. Destroy child service
```

**DLL/Shared Object Unloading Safety**:

**Critical Requirement**: If a child zone is in a DLL or shared object, the parent service MUST NOT unload the DLL until:
1. Child service has completed garbage collection
2. All threads have returned from DLL code
3. All detach coroutines have completed

**Reference Implementation**: The `enclave_service_proxy` already has cleanup logic for this scenario. This should be used as the reference implementation for DLL service proxies.

**Location**: SGX enclave boundary crossing code handles similar synchronization requirements:
- Waiting for all threads to exit enclave
- Ensuring all ecalls/ocalls complete
- Graceful cleanup before destroying enclave

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
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        // 3. Ensure no threads are executing in DLL code
        // All coroutines scheduled from DLL have completed

        // 4. NOW safe to unload DLL
        if (dll_handle_) {
            dlclose(dll_handle_);  // or FreeLibrary() on Windows
        }
    }
};
```

**Problem**: Current architecture doesn't have explicit DLL-specific service proxy cleanup. However, the `enclave_service_proxy` provides a reference implementation that should be adapted for DLL service proxies. Parent service must manually ensure child cleanup completes before unloading.

**Requirement**: Create DLL service proxy type that adapts enclave cleanup patterns for shared object lifecycle management.

### Telemetry Integration

**Location**: `/telemetry/src/console_telemetry_service.cpp`

Telemetry tracks:
- Service proxy creation/destruction
- add_ref/release operations
- Object proxy lifecycle
- Zone topology
- Child service lifecycle

**Impact**: Lifecycle hooks in service proxy and service for telemetry visibility.

---

## Testing Challenges

### Y-Topology Tests

**Location**: Tests mentioned in service.cpp:247-250

These tests **fail** without the opposite_direction_proxy workaround:
- `test_y_topology_and_return_new_prong_object`
- `test_y_topology_and_cache_and_retrieve_prong_object`
- `test_y_topology_and_set_host_with_prong_object`

**Why They're Important**: Exercise the on-demand proxy creation path.

### Fuzz Tests

**Location**: `/tests/fuzz_test/fuzz_test_main.cpp`

Tests hierarchical zone creation with 3+ levels:
- Property-based testing with randomized parameters
- Stress tests for race conditions
- Cross-zone `rpc::shared_ptr` marshalling

**Challenge**: Race conditions only appear under specific timing/load.

### Teardown Complexity

**Current Pattern** (`/tests/test_host/main.cpp`):
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
while (!shutdown_complete) {
    io_scheduler_->process_events(std::chrono::milliseconds(1));
}

// Continue processing to allow shutdown to finish
for (int i = 0; i < 100; ++i) {
    io_scheduler_->process_events(std::chrono::milliseconds(1));
}
```

**Why Complex**: Service proxy destruction is asynchronous, must wait for scheduled coroutines.

---

## Performance Considerations

### Memory Overhead

**Per Service Proxy**:
- Base object: ~200 bytes
- Object proxy map: O(n) where n = number of objects
- Lifetime lock: `shared_ptr` + atomic counter
- Various mutexes and atomics

**Scaling Issue**: Many routing proxies with zero objects waste memory.

### Lock Contention

**Hot Paths**:
```cpp
// service::send() - zone_control mutex
{
    std::lock_guard g(zone_control);
    // Lookup in other_zones map
}

// service_proxy::get_or_create_object_proxy() - proxies_mutex_
{
    std::lock_guard g(proxies_mutex_);
    // Lookup/insert in proxies_ map
}
```

**Contention**: Multiple threads making RPC calls compete for these locks.

### On-Demand Creation Overhead

Y-topology workaround (service.cpp:239):
- Dynamic proxy creation in hot path
- Memory allocation
- Map insertion
- Reference counting

**Better**: Pre-create proxies, avoid hot-path allocation.

---

## Documentation and Code Comments

### Notable Comments in Code

**service.cpp:239-250** (The Y-Topology Bug):
```cpp
// NOTE there is a race condition that will be addressed in the
// coroutine changes coming later
//
// note this is to address a bug found whereby we have a Y shaped graph
// whereby one prong creates a fork below it to form another prong this
// new prong is then ordered to pass back to the first prong an object.
// this object is then passed back to the root node which is unaware of
// the existence of the second prong. this code below forces the creation
// of a service proxy back to the sender of the call so that the root
// object can do an add_ref hinting that the path of the object is
// somewhere along this path
//
// this is a very unique situation, and indicates that perhaps the
// creation of two service proxies should be a feature of add ref and
// these pairs should support each others existence by perhaps some form
// of channel object.
```

**Key Phrase**: "perhaps some form of channel object" - acknowledging transport abstraction needed.

**types.h:126** (destination_channel_zone):
```cpp
// the zone that a service proxy was cloned from
struct destination_channel_zone {
    // ...
};
```

**Comment is misleading**: Actually identifies the adjacent zone for transport routing.

### Existing Documentation

**`SPSC_Channel_Lifecycle.md`**:
- Documents channel_manager lifecycle
- Reference counting for service proxies
- Shutdown coordination
- Shows emergent transport pattern

**`RPC++_User_Guide.md`**:
- 2250+ lines covering architecture
- Zone topology concepts
- Reference counting behavior
- Wire protocol details

**`CLAUDE.md`**:
- Project instructions
- Build system
- Recent changes (logging refactoring, JSON schema)
- Smart pointer migration notes

---

## Summary of Key Findings

### Emergent Patterns Identified

1. **Transport Abstraction** - SPSC `channel_manager` shows what transport should be
2. **Bidirectional Pairing** - Service proxies implicitly come in pairs (but created asymmetrically)
3. **Routing Proxies** - Zero-object proxies for bridging need special lifecycle
4. **Transport Identification** - `destination_channel_zone` and `caller_channel_zone` ARE transport identifiers

### Critical Problems Confirmed

1. **Y-Topology Race** (service.cpp:239) - On-demand proxy creation causes races
2. **Mixed Responsibilities** - Service proxy does routing + transport + lifecycle
3. **Asymmetric Creation** - Forward explicit, reverse on-demand (unpredictable)
4. **Lifetime Confusion** - Service proxy lifetime vs transport lifetime mismatch
5. **No Transport Abstraction** - Only SPSC has emergent pattern, others ad-hoc
6. **No Fire-and-Forget Messaging** - Missing `post()` method for one-way messages and optimistic reference cleanup
7. **No Receiver-Side Abstraction** - Missing service_sink counterpart to service_proxy
8. **Child Service Lifecycle** - No explicit synchronization for DLL unloading safety
9. **Transport Type Requirements** - Different transports have incompatible requirements (bi-modal vs coroutine-only, channel vs no channel)

### Race Conditions Documented

1. Concurrent add_ref/release through bridge
2. Service proxy destruction during active call
3. Transport destruction with proxy pair
4. Mutex ordering deadlock across zones
5. Coroutine yield with lock held
6. Weak pointer lock failures

### Complexity Drivers

- **Type System**: 5 zone types, 4 parameters per function signature
- **Lifecycle Management**: Multiple mechanisms (lifetime_lock, ref counting, aggregate counts)
- **Routing Logic**: 150+ lines of nested conditionals
- **On-Demand Creation**: Hot-path allocation and race conditions
- **Testing**: Integration tests required, race conditions hard to reproduce

---

## Appendix A: Key Code Locations

### Core Files

- `/rpc/include/rpc/internal/service.h` - Service class definition
- `/rpc/src/service.cpp` - Service implementation (lines 239-250: Y-topology bug)
- `/rpc/include/rpc/internal/service_proxy.h` - Service proxy definition
- `/rpc/src/service_proxy.cpp` - Service proxy implementation
- `/rpc/include/rpc/internal/types.h` - Zone type system

### Transport Implementations

- `/tests/common/include/common/spsc/channel_manager.h` - SPSC channel manager (emergent transport)
- `/tests/common/include/common/spsc/service_proxy.h` - SPSC service proxy
- `/tests/common/include/common/tcp/service_proxy.h` - TCP service proxy (no channel manager)

### Documentation

- `/docs/SPSC_Channel_Lifecycle.md` - Channel manager lifecycle (transport pattern)
- `/docs/RPC++_User_Guide.md` - Architecture and concepts
- `/docs/notes_for_v3.md` - Version 3 roadmap
- `/CLAUDE.md` - Project overview and recent changes

### Tests

- `/tests/test_host/main.cpp` - Main test infrastructure
- `/tests/fuzz_test/fuzz_test_main.cpp` - Hierarchical zone fuzz tests
- Y-topology tests referenced in service.cpp:247-250

---

## Appendix B: Terminology Reference

| Term | Definition |
|------|------------|
| **Zone** | Execution context (process, thread, enclave) with own service |
| **Service** | Manages object stubs and routes calls within a zone |
| **Service Proxy** | Routes calls to remote zone (currently also handles transport) |
| **Service Sink** | Receiver-side counterpart to service_proxy (missing in current architecture) |
| **Transport** | Bidirectional communication between adjacent zones (emerging concept) |
| **Channel Manager** | SPSC's transport implementation (pattern to replicate) |
| **Channel Object** | Manages sender/receiver pairs for async transports (SPSC, TCP, Enclave-coroutine) |
| **Object Stub** | Server-side wrapper around local implementation |
| **Object Proxy** | Client-side reference to remote object |
| **Routing Proxy** | Service proxy with zero objects, used for multi-hop routing |
| **Child Service** | Zone that is a child of another service, holds parent service proxy keepalive |
| **destination_zone** | Ultimate destination of a call |
| **caller_zone** | Original caller of a call |
| **destination_channel_zone** | Immediate adjacent zone toward destination (transport ID) |
| **caller_channel_zone** | Immediate adjacent zone toward caller (transport ID) |
| **Adjacent Zone** | Directly connected neighbor zone (one transport hop away) |
| **Aggregate Ref Count** | Sum of shared_count + optimistic_count for all objects in proxy |
| **Shared Reference** | Strong reference that keeps both object stub and proxy alive |
| **Optimistic Reference** | Weak-like reference that keeps proxy/transport alive but NOT stub |
| **Y-Topology** | Zone hierarchy where Root→A→B but Root has no direct route to B |
| **Fire-and-Forget** | One-way message sent via `post()` with no response expected |
| **post_options::fire_and_forget** | Send one-way message to object method |
| **post_options::release** | Notify service proxy chain to clean up optimistic reference |
| **post_options::zone_terminating** | Emergency shutdown notification - cleanup all proxies/services for terminating zone |
| **post_options::adhoc** | Service-to-service maintenance messages (certificates, pings, config) - type controlled by interface_id |
| **Cascading Cleanup** | Zone termination triggers cleanup that propagates across zone topology |
| **Ad-hoc Message** | Service-level protocol message not tied to specific objects (keepalive, certificates, etc.) |

---

**End of Document**
