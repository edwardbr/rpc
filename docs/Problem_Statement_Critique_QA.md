<!--
Copyright (c) 2025 Edward Boggis-Rolfe
All rights reserved.
-->

# Service Proxy Transport Problem Statement - Critique Q&A Session

**Date**: 2025-01-18
**Status**: In Progress - Requirements Gathering
**Purpose**: Document knowledge gaps and answers to refine problem statement

---

## Overview

This document captures a Q&A session to identify and fill knowledge gaps in the Service_Proxy_Transport_Problem_Statement.md. The goal is requirements gathering and understanding the current implementation, NOT solution design.

---

## Q1: Transport Creation - Parent/Child vs Peer Scenarios

**Question**: When is a transport created and who initiates it?

### Answer:

**Transport Creation Responsibility:**
- Transport is created by the **service proxy's specialized code** (hidden implementation)
- SPSC proxy creates SPSC transport, TCP proxy creates TCP transport, etc.
- Implementation details are **hidden from user** - user only provides high-level parameters (filename, port, IP)

**Parent/Child (`connect_to_zone`):**
- Parent creates child zone AND transport infrastructure (e.g., SPSC queues for child)
- Transport details passed privately through service proxy mechanism
- Child receives pre-configured transport
- Example: Program loading DLL - Zone 1 creates Zone 2 as child

**Peer/Peer (`attach_remote_zone`):**
- Used for peer connections (e.g., cluster nodes)
- Initial handshake may be asymmetric (initiator/responder)
- After connection established: **no asymmetry** - fully bidirectional
- Simultaneous reconnects handled by transport-specific code

**Key Insight**: `connect_to_zone` = hierarchical (parent→child), `attach_remote_zone` = peer-to-peer (symmetric)

---

## Q2: Transport Ownership and Service Proxy Pairing

**Question**: Is transport a single shared object, or does each zone have its own transport object?

### Answer:

**Depends on Transport Type:**

1. **Local transport (same address space):**
   - May not need transport object at all
   - Just virtual function calls - no serialization

2. **Cross-address-space (SPSC, TCP, SGX):**
   - **Each address space has its own transport object**
   - Transport objects on each side point to **shared underlying resources** (queues, sockets, shared memory)
   - Example: Zone A has transport_A pointing to queue pair, Zone B has transport_B pointing to same queue pair

**Reference Counting:**
- `shared_ptr` is **NEVER marshalled** across zones
- Reference counts maintained independently on each side
- Each side tracks its own shared_count/optimistic_count

**Transport Sharing:**
- **Multiple service proxies can share ONE transport**
- Example: Zone A might have service_proxy(A→B, caller=A) and service_proxy(A→B, caller=C) both using same TCP socket
- Sink on receiving side disambiguates which (destination, caller) pair the message is for

**SPSC Specifically:**
- Parent creates SPSC queues for child
- Queue addresses passed privately inside transport mechanism (hidden from user)
- Each zone has its own `channel_manager` object pointing to the shared queues
- `channel_manager.service_proxy_ref_count_` tracks how many service_proxies share this channel

---

## Q3: Service Sink - What Is It Exactly?

**Question**: Is "service_sink" a new abstraction that doesn't exist yet, or a renaming of something existing?

### Answer:

**Current State (The Mess):**
- All incoming calls from transport → fed directly into **service** after demarshalling
- Service acts as universal sink (receives everything) AND manages local objects AND manages outgoing proxies
- Designation is "blurred" - unclear separation of concerns

**Service Sink Concept (Vague/Undecided):**
- Represents component that:
  1. Deserializes call from transport (perhaps with channel manager)
  2. Routes to either:
     - **Service** (if destination is local object in this zone)
     - **Another service proxy** (if pass-through to different zone)

**Relationship Between Components:**

- **Transport**: Low-level communication (TCP sockets, SPSC queues) - should be abstracted away
- **Channel Manager**: Manages instance of communication layer (transport-specific, e.g., pairs of sender/receiver queues)
- **Service Sink** (potential): Demarshalls → routes to service OR service_proxy (doesn't exist yet - design TBD)
- **Service**: Represents the zone - manages object stubs (sinks for local objects) and service_proxies (for outgoing calls)

**Key Issue**:
- Within a single zone, a sink might need to call another sink to reach next destination
- Current code has service doing too many responsibilities
- Whether to introduce explicit "service_sink" abstraction is **still undecided** - part of design question

**Status**: Service_sink does **NOT exist today** - it's one potential design idea under evaluation

---

## Q4: Current Message Receiving Flow

**Question**: Walk through current call stack from "bytes arrive" to "method executes" or "message forwarded"

### Answer:

**Current Flow:**

```
1. Bytes arrive on transport
    ↓
2. receive_consumer_task (in channel_manager)
    ↓
3. incoming_message_handler (schedules based on message type...)
    ↓
4. stub_handle_send / stub_handle_add_ref / stub_handle_release / etc.
    ↓
5. service (ALWAYS receives - acts as universal sink)
    ↓
6. service unpacks message using from_yas_compressed_binary
    ↓
7. service checks destination_zone:
    - If destination == this zone → dispatch_to_stub (local object)
    - If destination != this zone → forward to service_proxy (pass-through)
```

**The Issue:**
- Service is doing BOTH local dispatch AND pass-through routing determination
- Maybe this responsibility should be split?
- If destination is not current zone, perhaps it should be forwarded to another service proxy WITHOUT service determining this?

**Note**: Don't go too deep into channel_manager implementation - it's potentially subject to change

---

## Q5: Pass-Through Routing Requirements

**Question**: What are the functional requirements for a zone acting as pass-through intermediary?

**Scenario:**
```
Zone A (caller_zone=1) → Zone B (zone=2) → Zone C (destination_zone=3)
```

### Answer:

**Pass-Through Requirements:**

1. **For regular calls (`send`):**
   - Currently has **Y-topology bug fix** creating symmetrical service proxies inefficiently
   - Zone B needs to relay: demarshall → remarshall → send to C

2. **For `add_ref` calls:**
   - **Complex logic** when remote object references are forwarded
   - More complicated than simple relay (details TBD - see next questions)

3. **Response Correlation:**
   - YES, Zone B **may need to track correlation** between requests and responses
   - Response path: C → B → A (goes back through Zone B)

4. **Failure Handling:**
   - **Same process**: If Zone B goes down → whole application crashes
   - **Cross-process**: Transport detects disconnect → rejects all subsequent calls to that zone → triggers application-level cleanup
   - **Cleanup broadcast**: Service proxy/transport should broadcast to all interested parties that zone has gone down
   - All stubs and proxies related to downed zone are cleaned up
   - Releases to downed zone allowed to **silently fail**

**Key Requirements:**
- NOT pure relay - Zone B needs state for correlation and reference counting
- Needs failure detection and broadcast mechanism
- Cleanup must handle zone termination gracefully

---

## Q6: add_ref "Forking Logic" - Current Behavior

**Question**: What is the functional requirement for add_ref when caller doesn't have direct service proxy to object's zone?

**Y-Topology Scenario:**
```
Root (Zone 1)
 ├─ Zone 3 (created by Root)
     ├─ Zone 4
         ├─ Zone 5 (deep zone, 5 levels from root)
         └─ Zone 7 (created by Zone 5 via Zone 3) ← Root UNAWARE of Zone 7
```

Timeline:
1. Zone 5 (deep in hierarchy) autonomously asks Zone 3 to create Zone 7
2. Zone 5 gets object from Zone 7 and passes it back to Root
3. Root now has `rpc::shared_ptr<object>` pointing to Zone 7
4. Root needs to call `add_ref` on object in Zone 7 (but has no service_proxy to Zone 7!)

### Answer:

**Current Workaround (service.cpp:239-266):**

1. **During `send()` operations**, service creates **reverse service proxy** to provide "breadcrumb trail"
   - If message goes from Zone A → Zone B, service.cpp:239 creates reverse proxy B→A
   - This provides routing hint for future `add_ref` operations

2. **`known_direction_zone` parameter** added to all `add_ref` signatures
   - Provides routing hint when direct route unavailable
   - Usage: `known_direction_zone(zone_id_)` indicates "path is through this zone"

3. **Enhanced `add_ref` routing logic (service.cpp:940-947)**:
   ```cpp
   else if (auto found = other_zones.lower_bound({known_direction_zone_id.as_destination(), {0}});
       found != other_zones.end())
   {
       // Y-topology fix: use hint to find intermediate zone
       tmp = found->second.lock();
       other_zone = tmp->clone_for_zone(destination_zone_id, caller_zone_id);
       inner_add_zone_proxy(other_zone);
   }
   ```

**What Makes add_ref Special:**

- **send() and add_ref are actually the SAME** (`call` and `send` are different names for same purpose)
- The "forking" happens when **object references are passed across zones**
- Root doesn't know about Zone 7, but needs to establish routing when object is returned

**Tests that Validate This (would fail without fix):**
- `test_y_topology_and_return_new_prong_object` - Direct object return
- `test_y_topology_and_cache_and_retrieve_prong_object` - Cached object retrieval
- `test_y_topology_and_set_host_with_prong_object` - **Most critical** - causes infinite recursion/stack overflow without fix

**The Problem:**
- Creating reverse proxies **reactively during send()** is inefficient
- On-demand proxy creation happens in critical path (race condition noted at line 239)
- Complex logic scattered between `send()` and `add_ref()`
- Comment at line 246 suggests: "perhaps the creation of two service proxies should be a feature of add ref and these pairs should support each others existence by perhaps some form of channel object"

**Key Insight**: The Y-topology bug fix reveals that **bidirectional service proxy pairs** should be created together, not asymmetrically on-demand.

**IMPORTANT CORRECTION:**
- **send() and add_ref() are NOT the same** - this was a misunderstanding
- **send/call** are the same (different names for RPC method invocation)
- **add_ref** has different purpose: increment shared/optimistic reference count
  - Mode 1: Add +1 to existing service proxy/sink for known object
  - Mode 2: Share object reference to WHOLE NEW ZONE (complex operation)
  - Mode 3: Three modes defined by `add_ref_options` (build_caller_route, etc.)

---

## Q7: Optimistic Reference Cleanup - Fire-and-Forget Requirement

**Question**: What are the functional requirements for optimistic reference cleanup messaging?

**Background from Problem Statement:**
- Problem 6 describes `post_options::release` for optimistic reference cleanup
- Fire-and-forget messaging (no response expected)
- Must only be sent to service_proxy/sinks with `optimistic_count > 0` for that object

### Answer:

**Purpose of post() Method:**
- `post()` is the **formalized interface function between zones** for fire-and-forget messages
- How it's handled INSIDE a service proxy is separate concern (not yet designed)
- New feature to handle optimistic count cleanup, among other things

**Design Status: TO BE DECIDED**

The following are **open design questions requiring proposals**:

1. **Routing/Filtering for Optimistic Cleanup:**
   - Perhaps there is a **tally made for pass-through objects**
   - Pass-through zones know when to clean up their optimistic counts
   - Service also maintains tally for objects it owns
   - **Specific mechanism: requires design proposal**

2. **Zone Termination Cleanup:**
   - If whole zone is going down: **one message per transport** would be more efficient
   - Transport sends zone termination message
   - Services and service_proxies handle cleanup on their side
   - Block any further calls to the dying zone
   - **More efficient than individual optimistic cleanup messages per object**

**Key Requirements Identified:**
- Fire-and-forget semantics (no response expected)
- Efficient broadcast on zone termination (one message per transport, not per object)
- Pass-through zones need to track optimistic counts for proper cleanup
- Must block further calls to terminated zones

**Status**: High-level requirements understood, detailed design TBD

---

## Q8: Child Service Shutdown - Optimistic Reference Semantics

**Question**: What are the concrete requirements for service shutdown when optimistic references are involved?

**From Problem Statement (Problem 7):**
> **Child Service Shutdown Condition:**
> - Incoming optimistic references (other zones → this service): Do NOT prevent shutdown
> - Outgoing optimistic references (this service → other zones): DO prevent shutdown

### Answer:

**Optimistic Pointer Semantics:**

**Purpose**: Optimistic pointers maintain the **channel open** without strong ownership of the pointed-to object.

**Incoming Optimistic Refs (other zones → this service):**
- Object being pointed to CAN be released if no more shared pointers to it
- If no more objects in zone AND no pass-throughs AND no shared pointers from outside → **service CAN die**
- Incoming optimistic pointers don't keep service alive

**Outgoing Optimistic Refs (this service → other zones):**
- Object with optimistic pointer is being kept alive by:
  - Another shared object, OR
  - Its own lock on existence
- Therefore **its service should NOT die** (the service holding the optimistic_ptr must stay alive)

**Shared Pointers (for comparison):**
- BOTH channel AND object should not die
- Owner of shared_ptr doesn't want either to go away (strong ownership)

**Clarification Needed:**

**Question about Problem Statement terminology:**
- What is meant by "child" in "child has outgoing optimistic refs"?
  - Is this the **service** (the zone itself)?
  - Or an **object IN that service's zone**?
- What is meant by "outgoing"?
  - Optimistic_ptr held BY objects in this zone pointing TO other zones?
  - Or optimistic_ptr held BY other zones pointing TO this zone?

**Parent/Child Module Lifetime (DLL Use Case):**

1. **Parent manages module lifetime** (e.g., DLL in which child service lives)
2. Parent must keep module alive **until after child service has ended**
3. Not an exact prerequisite in all cases
4. Child should **cease all further communication** with other zones once:
   - All inbound references released
   - All outbound references released
   - All pass-throughs cleared

**Key Requirements:**
- Service can die when: no objects + no pass-throughs + no external shared_ptrs
- Service must stay alive when: holding outgoing optimistic_ptrs (object needs service to be alive)
- Module unloading: wait until child service has fully ended
- Cleanup: child must release all references (in+out) and pass-throughs before termination

**Terminology Issue Identified:** Need clarification on "child has outgoing optimistic refs" - is this zone-level or object-level concern?

---

## Q9: Zone Termination Cascading Cleanup

**Question**: What is the algorithm for propagating zone termination through the topology?

**Example Topology:**
```
Root (1) → Branch A (2) → Leaf A1 (3)
                       → Leaf A2 (4)
         → Branch B (5) → Leaf B1 (6)
```

### Answer:

**Zone Knowledge and Discovery:**

1. **Root generates unique zone IDs** but doesn't necessarily know about child nodes' existence
2. **Discovery happens when object reference is passed** from new zone
3. **Location discovery via `known_direction_zone`:**
   - If passthrough exists to that zone → can work out location
   - Otherwise → consult route from where reference was passed using `known_direction_zone`

**Zone Termination Message (New Feature - Under Review):**

**Two Scenarios:**

**Scenario 1: No Optimistic Pointers**
- Existing termination through **ref-counted death** of service proxies works fine
- Service proxies naturally clean up as shared_ptr counts reach zero
- No special termination messages needed

**Scenario 2: With Optimistic Pointers**
- Death of optimistic reference **NOT controlled by shared_ptr**
- Polite thing: **notify all service proxies** (including passthrough channels) of zone's demise
- Adjacent zones should **pass this message to all adjacent zones** impacted by that zone ID going down
- **Only triggered on graceful shutdown**

**Forced Shutdown (Zone Crash):**
- ALL references (shared counts AND optimistic counts) and links through dying zone need notification
- **Not just that zone ID** - also ALL pass-through zone IDs going via dying zone
- Dying zone may **not be able to send message itself**
- **Transport stub logic responsibility** to generate these messages for each zone
- May cause **complete cascade of dying references** if zone is critical component

**Message Propagation Requirements:**

1. **Target audience**: Any zone with:
   - Shared reference to dying zone
   - Optimistic reference to dying zone
   - Pass-through references going VIA dying/dead zone

2. **Cascading impact**: If Zone 2 dies and was routing to Zones 3, 4:
   - All zones with references to 2, 3, 4 must be notified
   - Zones 3, 4 become unreachable even if they're still running

3. **Transport responsibility**:
   - Transport logic decides if connection is **dead vs just slow**
   - Configured by service proxy specialization initialization parameters
   - Transport detects hard failure and triggers cascade cleanup

**Key Requirements:**
- Graceful shutdown: zone sends `zone_terminating` to all adjacent zones
- Forced shutdown: transport detects failure and generates termination messages
- Propagation: adjacent zones relay to all impacted zones
- Cascading: dying zone takes down all pass-through routes
- Configuration: timeout/failure detection is transport-specific

**Design Status**: Zone termination messaging is **new feature under review**

---

## Q10: Back-Channel Data Format and Requirements

**Question**: What are the functional requirements for back-channel data?

**From Problem Statement (Problem 7):**
> Back-channel Support:
> - HTTP-header style key-value pairs
> - Optional encryption per entry
> - Native support for distributed tracing context propagation (OpenTelemetry)

### Answer:

**Back-Channel Data Structure:**

```cpp
struct back_channel_entry {
    uint64_t type_id;          // Fingerprint ID from IDL
    std::vector<uint8_t> payload;  // Arbitrary binary data
};

// Back-channel is vector of entries
std::vector<back_channel_entry> back_channel_data;
```

**Type ID Generation:**
- `type_id` is **fingerprint generated by IDL parser**
- Mechanism: Define unique structure name in its own namespace in IDL
- IDL parser generates unique fingerprint for that structure
- Ensures unique identification of back-channel data types

**Payload Contents (OUT OF SCOPE):**
- Payload is **arbitrary data** - RPC++ just passes it through
- Could be: serialized YAS binary, JSON, protobuf, certificate data, OpenTelemetry trace ID, etc.
- **What is in the data is out of scope** for this exercise
- Encryption: implementor decides if data is encrypted or not (out of scope)
- OpenTelemetry integration details: out of scope

**Key Requirements:**
1. **Pass-through semantics**: RPC++ transports back-channel data without interpreting it
2. **Type identification**: Each entry has IDL-generated fingerprint for type safety
3. **Binary payload**: No restrictions on payload contents (binary-safe)
4. **Vector of entries**: Multiple back-channel entries can be attached to single message

**Wire Format:**
- For now: **vector of back_channel_entry structs**
- Specific serialization format details not needed at requirements level

**In Scope:**
- Structure definition (type_id + payload)
- Mechanism for generating unique type IDs (IDL fingerprints)
- Pass-through transport of back-channel data

**Out of Scope:**
- Payload interpretation
- Encryption details
- OpenTelemetry-specific integration
- Wire format encoding specifics

---

## Q11: TCP Transport Requirements - Reliability and Reconnection

**Question**: What are the reliability and reconnection requirements for TCP transport?

**From Problem Statement (Dependencies section):**
> TCP: Must handle unreliable transport - connection loss, recovery, reconnection

### Answer:

**In-Flight Call Handling on Connection Loss:**

1. **Retry logic**: Up to **transport configuration** set by first service proxy to instantiate it
2. **When connection is lost**: Transport triggers `TRANSPORT_ERROR` to any awaiting calls
3. **No automatic retry** at RPC layer - application decides how to respond

**Reconnection Semantics:**

1. **All references are dropped** when connection is lost
2. Application can **no longer call those endpoints** - gets `TRANSPORT_ERROR`
3. **Application responsibility** to respond accordingly, including:
   - Requesting connection to be re-established
   - Cleaning up local references
   - Retry logic at application level

4. **Object references NOT preserved** across reconnection:
   - shared_ptr counts NOT automatically restored
   - New `add_ref()` calls needed if reconnecting

**Message Delivery Guarantees:**

- RPC++ guarantees **at-most-once** delivery
- Call may be lost on disconnect (not retried automatically)
- No duplicate delivery

**Reconnection vs Zone Termination:**

1. **Transport configuration decides** timeout thresholds
2. **No specific requirement** to notify application about reconnection events
3. **Optional notification mechanisms:**
   - Could use `i_marshaller::post()` events to application-chosen endpoint
   - If this feature is part of service proxy's design
   - Logging can report reconnect attempts

**Key Requirements:**
- Transport-specific retry configuration (set during service proxy instantiation)
- `TRANSPORT_ERROR` on connection loss (immediate failure of in-flight calls)
- At-most-once delivery guarantee
- Application-driven reconnection (RPC layer doesn't auto-reconnect)
- All references invalidated on disconnect
- Optional reconnection notifications via post() or logging

**Design Flexibility:**
- Timeout configuration is transport-specific
- Reconnection policy is transport-specific
- Notification mechanism is optional (not required)

---

## Q12: i_marshaller Interface - Current State and Required Changes

**Question**: What is the current i_marshaller interface and what needs to change?

**From Problem Statement:**
- Problem 5: "No Explicit Transport Abstraction Layer"
- Problem 7: Lists methods needed in `i_marshaller`

### Answer:

**Current i_marshaller Interface (from marshaller.h):**

```cpp
class i_marshaller {
public:
    virtual CORO_TASK(int) send(
        uint64_t protocol_version, encoding encoding, uint64_t tag,
        caller_channel_zone, caller_zone, destination_zone,
        object, interface_ordinal, method,
        size_t in_size_, const char* in_buf_,
        std::vector<char>& out_buf_) = 0;

    virtual CORO_TASK(void) post(
        uint64_t protocol_version, encoding encoding, uint64_t tag,
        caller_channel_zone, caller_zone, destination_zone,
        object, interface_ordinal, method,
        post_options options,
        size_t in_size_, const char* in_buf_) = 0;  // Fire-and-forget

    virtual CORO_TASK(int) try_cast(
        uint64_t protocol_version, destination_zone, object, interface_ordinal) = 0;

    virtual CORO_TASK(int) add_ref(
        uint64_t protocol_version,
        destination_channel_zone, destination_zone, object,
        caller_channel_zone, caller_zone,
        known_direction_zone,  // Y-topology routing hint
        add_ref_options,
        uint64_t& reference_count) = 0;

    virtual CORO_TASK(int) release(
        uint64_t protocol_version,
        destination_zone, object, caller_zone,
        release_options,
        uint64_t& reference_count) = 0;
};
```

**Current Implementers:**

1. **service_proxy**: Derives from `i_marshaller`
   - Represents remote service in another zone
   - Sends messages across transport

2. **i_service** (service class): Also derives from `i_marshaller`
   - Represents local zone
   - Handles local dispatch + pass-through routing currently

**Why Both Implement i_marshaller:**
- service_proxy is **remote representative** to service in another zone
- Both need to implement same interface for polymorphic routing
- Allows pass-through: zone can forward to either local service or remote service_proxy

**Required Changes - Back-Channel Support:**

**All methods need back-channel parameters added:**
- **send()**: Needs `in_back_channel` and `out_back_channel` parameters
- **add_ref()**: Needs `in_back_channel` and `out_back_channel` parameters
- **release()**: Needs `in_back_channel` and `out_back_channel` parameters
- **try_cast()**: Needs `in_back_channel` and `out_back_channel` parameters
- **post()**: Needs `in_back_channel` only (fire-and-forget, no response)

**Back-Channel Structure:**
```cpp
struct back_channel_entry {
    uint64_t type_id;          // IDL-generated fingerprint
    std::vector<uint8_t> payload;  // Arbitrary data
};
// Parameter: std::vector<back_channel_entry> back_channel
```

**Pass-Through Design Implication:**

1. **Current state**: Service does all pass-through routing (inefficient, handles graceful shutdown poorly)
2. **New design**: Probably create separate **pass_through object**
3. **pass_through would ALSO implement i_marshaller**
4. **New code required** - not refactoring existing

**Key Insight:**
- **i_marshaller is THE interface** through which ALL inter-zone and pass-through communication must rely
- Handler polymorphism: pass_through OR service (both implement i_marshaller)
- Allows routing decisions: local dispatch vs remote forward

**post() Method:**
- **Added but not committed** to marshaller.h
- Fire-and-forget (void return in coroutines, no response)
- Used for optimistic cleanup, zone termination notifications, etc.

**Design Status:**
- Current interface exists and works
- Need to add back-channel parameters (breaking change to all implementations)
- Need to create pass_through class implementing i_marshaller (new code)

---

## Q13: Bi-Modal Support (Synchronous vs Coroutine) - Critical Requirements

**Question**: What are the specific requirements for supporting both synchronous and coroutine execution modes?

**Critical Constraint**: "The main library needs to work just as happily in synchronous and coroutine modes."

### Answer:

**Synchronous Mode (BUILD_COROUTINE=OFF):**

1. **All i_marshaller methods become BLOCKING**
   - `CORO_TASK(int)` → `int`
   - `CORO_TASK(void)` → `void`
   - Fire-and-forget `post()` becomes **blocking call**
   - **This is why async is valuable** - posts are not scalable in synchronous mode

2. **No scheduling** in synchronous mode
   - All cleanup done **within the same call** as last shared_ptr is released
   - Everything is **consistent** (immediate cleanup)
   - No background tasks or delayed operations

3. **Post messaging may need to be curtailed**
   - Some post messaging logic may need different handling in sync mode
   - **To be determined** - design decision needed

**Coroutine Mode (BUILD_COROUTINE=ON):**

1. **Async execution**
   - True fire-and-forget for `post()`
   - Background tasks for cleanup
   - Scheduling-based operation

2. **"Eventually consistent"**
   - Cleanup happens asynchronously
   - Requires special handling in tests to account for timing differences

**Transport Compatibility:**

**Synchronous Mode Support:**
- ❌ **Local (in-process)**: Async only
- ❌ **SGX enclave**: Async only
- ❌ **DLL calls** (not implemented here): Async only
- ❌ **SPSC**: Async only
- ❌ **TCP**: Async only

**Key Insight**: ALL transports are **async only** - they do NOT work in synchronous mode.

**Bi-Modal Design Implications:**

**Question**: If all transports are async-only, what runs in synchronous mode?
- Is synchronous mode only for unit testing with mocked transports?
- Or is there a synchronous local transport not mentioned?
- **Clarification needed on synchronous mode use case**

**Test Handling:**
- Tests must handle different behavior between modes:
  - **Sync mode**: Immediate, consistent cleanup
  - **Async mode**: Eventually consistent, requires waiting for cleanup completion

**Design Constraints:**
- Solution must work in both modes
- Cleanup timing differs significantly between modes
- Post messaging may need mode-specific logic

**Status**: Critical constraint identified - all current transports are async-only, but library must support sync mode (use case TBD)

---

## Q13b: Synchronous Mode Clarification - Use Cases and Implementation

**Question**: What is the actual use case for synchronous mode if production transports require async?

### Answer:

**Current Implementation:**

1. **Global build flag**: `BUILD_COROUTINE` in `CMakePresets.json`
2. **Controls definitions in**: `rpc/include/rpc/internal/coroutine_support.h`
3. **Changes behavior** of all classes and interfaces throughout entire codebase
4. **Single namespace**: All code builds either sync OR async (not both simultaneously)

**Future Vision (Not Soon):**

1. **Dual namespace approach** in `rpc.h`:
   - `namespace rpc::async` - `#define BUILD_COROUTINE`, then include all headers
   - `namespace rpc::sync` - undefine, then include all headers again
2. **Result**: Two versions of all RPC logic with different calling conventions
3. **Allows**: Both sync and async in same binary
4. **Timeline**: Do not expect to implement this soon

**Transport Correction:**

- **Local service proxies CAN support both modes** (documentation error identified)
- Other transports (SGX, SPSC, TCP) are async-only

**Synchronous Mode Value Proposition:**

All of the following are **correct reasons** for maintaining sync mode:

1. ✅ **Legacy compatibility** - Existing code without coroutine support
2. ✅ **Simpler debugging** - No coroutine complexity in debugger
3. ✅ **Embedded systems** - Platforms without coroutine support
4. ✅ **Unit testing infrastructure** - Simpler test infrastructure without async coordination

**Key Architectural Constraint:**

- **Any design solution** must work in both sync and async modes
- Cleanup timing differs:
  - Sync: Immediate, consistent (within same call stack)
  - Async: Eventually consistent (scheduled cleanup)
- Post messaging may need curtailment in sync mode (TBD)

**Corrected Transport Matrix:**

| Transport | Sync Mode | Async Mode | Notes |
|-----------|-----------|------------|-------|
| Local (in-process) | ✅ Yes | ✅ Yes | Direct virtual calls |
| SGX enclave | ✅ Yes | ✅ Yes | Supports both modes |
| DLL calls (future) | ✅ Yes | ✅ Yes | Not implemented, supports both |
| SPSC | ❌ No | ✅ Yes | Requires pump tasks |
| TCP | ❌ No | ✅ Yes | Requires I/O scheduling |

---

## Q14: Routing Proxy Lifetime - Current Behavior

**Question**: What is the current lifecycle of routing proxies (pass-through service proxies)?

**Scenario:**
```
Zone A (caller_zone=1) ↔ Zone B (zone=2) ↔ Zone C (destination_zone=3)
```

Zone B has routing/pass-through proxy: `service_proxy(destination=3, caller=1)` with **zero objects**.

### Answer:

**Service Proxy Creation:**

1. **Initial service proxy** created by application using:
   - `connect_to_zone()` (parent creating child)
   - `attach_remote_zone()` (peer-to-peer connection)

2. **Additional service proxies** (including routing/pass-through proxies) created using:
   - `clone_for_zone()` to create other instances
   - Share the **same transport instance**

**Routing Proxy Definition Needed:**

**Question**: What exactly IS a "routing proxy" or "pass-through proxy"?
- Is it: service_proxy where `zone_id != caller_zone_id`?
- Or: service_proxy with zero objects but non-zero reference counts from other zones?
- **Terminology needs clarification**

**Service Proxy Lifetime - Current State:**

1. **Kept alive by**:
   - `shared_ptr` or `optimistic_ptr` references in the zone itself, OR
   - If pass-through: reference counts maintained by `shared_ptr`/`optimistic_ptr` in OTHER zones

2. **Self-preservation mechanisms**:
   - `lifetime_lock_` (service proxy keeps itself alive)
   - `shared_ptr` locks in `object_proxies`

3. **other_zones map**:
   - Holds **weak_ptr** to service proxies
   - Not strong ownership

4. **Destruction trigger**:
   - When there is **no need for that zone**, it automatically destroys itself
   - **Context needed**: What constitutes "no need"? (zero ref counts? no objects? no pass-throughs?)

**"tmp is null" Bug:**

- **New bug** detected when attempting to make coroutine and optimistic_ptr logic work
- Did NOT happen previously
- Needs investigation
- **May vanish with new design anyway**

**Y-Topology Workaround (service.cpp:239):**

1. **Creates `opposite_direction_proxy`** - is this a routing/pass-through proxy? **Yes**

2. **Current problem**: Added to fix immediate bug
   - Generates proxy **on-the-fly** during every `send()` call if not already present
   - Inefficient for rare Y-topology situations

3. **Better approach suggested**:
   - Pass-through should maintain **bidirectional/symmetrical link** between both endpoints
   - Create once and keep, rather than generate on-demand
   - Avoids overhead of checking/creating for every `send()` call

**Key Insights:**

1. **Routing proxies are pass-through proxies** (terminology equivalence)
2. **Current approach is reactive** - creates on-demand during send()
3. **Better approach would be proactive** - create bidirectional pair upfront
4. **Lifetime tied to reference counts** from other zones (not local objects)
5. **Definition of "routing/pass-through proxy" needs clarification** for precise requirements

**Open Questions:**
- Precise definition of routing/pass-through proxy
- What constitutes "no need for zone" destruction trigger
- Should bidirectional pairs be created eagerly or lazily

---

## Key Findings Summary

### Architectural Clarity Gained:

1. **Transport Ownership Model**:
   - Each address space has its own transport object pointing to shared resources (queues, sockets)
   - Multiple service proxies can share ONE transport (many logical connections over one physical channel)
   - Transport created by service proxy specialized code (hidden from user)
   - Parent creates transport for child in hierarchical scenarios

2. **Service Sink Does NOT Exist Yet**:
   - Current state: service acts as universal receiver (receives everything)
   - Service does BOTH local dispatch AND pass-through routing (blurred responsibilities)
   - Whether to introduce explicit "service_sink" abstraction is still undecided
   - Part of receiver-side design question

3. **i_marshaller as Universal Interface**:
   - THE interface for all inter-zone and pass-through communication
   - Currently implemented by: service_proxy (remote representative) and service (local zone)
   - Future: pass_through object would ALSO implement i_marshaller
   - All methods need back-channel parameters added (breaking change)

4. **Pass-Through Requirements**:
   - NOT pure relay - needs correlation tracking for request/response
   - Complex add_ref logic when forwarding remote object references
   - Y-topology workaround creates reverse proxies on-demand (inefficient)
   - Better approach: maintain bidirectional/symmetrical links upfront

5. **Reference Counting Semantics**:
   - Optimistic pointers: maintain channel open WITHOUT strong object ownership
   - Incoming optimistic refs: do NOT prevent service shutdown
   - Outgoing optimistic refs: DO prevent service shutdown (object needs service alive)
   - Zone can die when: no objects + no pass-throughs + no external shared_ptrs

### Critical Requirements Identified:

1. **Zone Termination Broadcast**:
   - Graceful: zone sends `zone_terminating` to adjacent zones
   - Forced: transport detects failure and generates termination messages
   - Cascading: dying zone takes down all pass-through routes
   - One message per transport (not per object) for efficiency

2. **Fire-and-Forget Messaging (post())**:
   - New feature for optimistic cleanup and zone termination
   - Already added to i_marshaller (not committed)
   - Routing/filtering mechanism TBD (requires design proposal)
   - May need curtailment in synchronous mode

3. **Back-Channel Data**:
   - Structure: `vector<{uint64_t type_id, vector<uint8_t> payload}>`
   - type_id from IDL-generated fingerprints
   - Pass-through semantics (RPC++ doesn't interpret payload)
   - All i_marshaller methods need back-channel parameters

4. **Bi-Modal Support (Sync/Async)**:
   - ALL design solutions must work in both modes
   - Sync: blocking calls, immediate cleanup, consistent state
   - Async: fire-and-forget, eventual consistency, scheduled cleanup
   - Local/SGX/DLL transports support both; SPSC/TCP async-only

5. **TCP Transport Reliability**:
   - At-most-once delivery guarantee
   - TRANSPORT_ERROR on connection loss
   - All references invalidated on disconnect
   - Application-driven reconnection (no auto-reconnect)

6. **add_ref Complexity**:
   - Three modes: normal, build_destination_route, build_caller_route
   - Also: optimistic flag for optimistic_ptr references
   - Y-topology: uses known_direction_zone hint for routing
   - Can create new service proxy when sharing object to whole new zone
   - Different from send/call (which invoke remote methods)

### Design Questions Requiring Proposals:

1. **Optimistic Cleanup Routing**:
   - How to track which zones have optimistic counts for objects
   - Tally mechanism for pass-through zones TBD
   - Selective notification vs broadcast TBD

2. **Pass-Through Architecture**:
   - Should there be explicit pass_through class implementing i_marshaller?
   - Or refactor service to separate concerns?
   - Or different approach entirely?

3. **Routing Proxy Definition**:
   - Need precise definition of "routing proxy" vs "pass-through proxy"
   - When created (eagerly vs lazily)?
   - How maintained (bidirectional pairs vs on-demand)?

4. **Post Messaging in Sync Mode**:
   - How should fire-and-forget work when calls are blocking?
   - May need curtailment or different implementation

### Known Issues to Address in New Design:

1. **"tmp is null" Bug**: New bug in coroutine/optimistic_ptr logic - may vanish with redesign
2. **Y-Topology Inefficiency**: Creating reverse proxies on every send() - should be bidirectional pairs
3. **Blurred Responsibilities**: Service doing both local dispatch and pass-through routing
4. **Missing Abstractions**: No explicit transport layer, no service_sink, no pass_through class

### Questions Answered (14 total):

- ✅ Q1-Q2: Transport creation and ownership model
- ✅ Q3-Q4: Service sink concept and current message flow
- ✅ Q5: Pass-through routing requirements
- ✅ Q6: add_ref Y-topology forking logic
- ✅ Q7: Optimistic reference cleanup (design TBD)
- ✅ Q8: Child service shutdown semantics
- ✅ Q9: Zone termination cascading
- ✅ Q10: Back-channel data format
- ✅ Q11: TCP transport reliability
- ✅ Q12: i_marshaller interface current state
- ✅ Q13a-b: Bi-modal sync/async support
- ✅ Q14: Routing proxy lifetime

---

## Next Steps

1. ✅ **Requirements Gathering Complete** - 14 questions answered
2. **Review Problem Statement** - Identify which problems are real vs based on misconceptions
3. **Update Problem Statement** - Incorporate findings and clarify requirements
4. **Identify Design Constraints** - Document must-haves vs nice-to-haves
5. **Prepare for Solution Design** - Once requirements are validated

---

## Summary

This Q&A session successfully gathered requirements for the Service Proxy Transport refactoring. Key accomplishments:

**Clarified Concepts:**
- Transport ownership and lifecycle
- i_marshaller as universal inter-zone interface
- Pass-through vs routing proxy terminology
- Optimistic pointer semantics
- Y-topology problem and current workaround

**Identified Requirements:**
- Back-channel support (type_id + payload)
- Fire-and-forget post() messaging
- Zone termination broadcast mechanism
- Bi-modal sync/async support
- At-most-once delivery guarantee

**Exposed Design Questions:**
- Service sink architecture (create new class vs refactor service)
- Pass-through implementation (explicit class vs integrated)
- Routing proxy lifecycle (eager vs lazy, bidirectional pairs)
- Optimistic cleanup routing mechanism

**Ready for Next Phase**: With requirements documented, can now critique Problem Statement document and identify gaps, misconceptions, and areas needing design proposals.

---

**End of Document** (Requirements Gathering Phase Complete)
