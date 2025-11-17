<!--
Copyright (c) 2025 Edward Boggis-Rolfe
All rights reserved.
-->

# RPC++ Service Proxy and Transport Refactoring - Master Implementation Plan

**Version**: 2.0
**Date**: 2025-10-28
**Status**: In Progress - Milestones 1-3 Complete, Milestone 5 Next Priority

**Source Documents**:
- Problem_Statement_Critique_QA.md (15 Q&A requirements)
- Service_Proxy_Transport_Problem_Statement.md
- RPC++_User_Guide.md
- SPSC_Channel_Lifecycle.md
- qwen_implementation_proposal.md
- Implementation_Plan_Critique.md

---

## Table of Contents

1. [Implementation Status Report](#implementation-status-report) **NEW**
2. [Executive Summary](#executive-summary)
3. [Critical Architectural Principles](#critical-architectural-principles)
4. [Milestone-Based Implementation](#milestone-based-implementation)
5. [BDD/TDD Specification Framework](#bddtdd-specification-framework)
6. [Bi-Modal Testing Strategy](#bi-modal-testing-strategy)
7. [Success Criteria and Validation](#success-criteria-and-validation)

---

## Implementation Status Report

**Last Updated**: 2025-01-17

### Overview

This section tracks the actual implementation status against the planned milestones. The implementation has progressed beyond initial expectations in some areas while revealing architectural improvements over the original plan.

**Recent Completion (2025-01-17)**: Established correct ownership model for services, transports, and stubs, resolving zone lifecycle issues and enabling reliable multi-zone distributed systems. This foundational work is critical for pass-through implementation.

### Milestone Completion Status

| Milestone | Planned Status | Actual Status | Notes |
|-----------|---------------|---------------|-------|
| **Milestone 1**: Back-channel Support | ✅ COMPLETED | ✅ **VERIFIED COMPLETE** | All i_marshaller methods have back_channel parameters |
| **Milestone 2**: post() Fire-and-Forget | PARTIAL | ✅ **MOSTLY COMPLETE** | post() implemented, post_options defined, zone termination pending full integration |
| **Milestone 3**: Transport Base Class | PLANNED | ✅ **COMPLETED** | Fully implemented with enhancements beyond plan |
| **Milestone 4**: Transport Status Monitoring | PLANNED | 🟡 **PARTIAL** | Status management exists, needs testing |
| **Milestone 5**: Pass-Through Core | PLANNED | ❌ **NOT STARTED** | Critical missing component - next priority |
| **Milestone 6**: Both-or-Neither Guarantee | PLANNED | ❌ **BLOCKED** | Depends on Milestone 5 |
| **Milestone 7**: Zone Termination Broadcast | PLANNED | 🟡 **PARTIAL** | Infrastructure exists, needs pass-through integration |
| **Milestone 8**: Y-Topology Bidirectional | PLANNED | ❌ **BLOCKED** | Depends on Milestone 5 |
| **Milestone 9**: SPSC Integration | PLANNED | 🟡 **PARTIAL** | Basic structure exists, needs pass-through |
| **Milestone 10**: Full Integration | PLANNED | ❌ **PENDING** | Awaiting Milestones 5-9 |

### Key Architectural Deviations and Improvements

#### ✅ **IMPROVEMENT 1: Transport Implements i_marshaller**

**Original Plan** (lines 665-781): Transport base class with destination routing but NOT implementing i_marshaller

**Actual Implementation**:
```cpp
class transport : public i_marshaller {
    std::unordered_map<destination_zone, std::weak_ptr<i_marshaller>> destinations_;
    // Can route directly as a marshaller
};
```

**Why This Is Better**:
- Transport can be used directly as an i_marshaller
- Simplified routing through `inbound_send()`, `inbound_post()`, etc.
- Cleaner integration with service_proxy
- No need for manual delegation from service_proxy to transport

**Status**: ✅ **ACCEPT AS SUPERIOR DESIGN**

---

#### ✅ **IMPROVEMENT 2: Local Transport Architecture**

**Original Plan** (line 961-967): Oversimplified "always connected" local transport

**Actual Implementation**:
```cpp
namespace local {
    class parent_transport : public rpc::transport { /* child→parent */ }
    class child_transport : public rpc::transport { /* parent→child */ }
}
```

**Key Characteristics** (Verified 2025-10-28):
1. **Serialization**: Local transport DOES serialize/deserialize data
2. **Purpose**: Type safety, consistency, zone isolation, testability
3. **Shared Scheduler**: Parent and child zones share the same coroutine scheduler
4. **In-Process**: Serialized data passed via shared memory within same process
5. **Bidirectional**: Separate transports for parent→child and child→parent communication

**Why This Is Better**:
- More sophisticated than plan envisioned
- Proper zone isolation despite in-process communication
- Same serialization code path as remote transports (consistency)
- Easier to move child zone to separate process later (flexibility)
- Shared scheduler eliminates context switching overhead

**Status**: ✅ **ACCEPT AS SUPERIOR DESIGN**

---

#### ✅ **CLARIFICATION: Transport Classification**

All transports perform serialization - the difference is WHERE serialized data goes:

| Transport Type | Serialization | Process Boundary | Async/Sync | Scheduler |
|----------------|---------------|------------------|------------|-----------|
| **Local** (parent/child) | ✅ YES | In-process, separate zones | Both | Shared |
| **SPSC** | ✅ YES | Same or different process | Async only | Separate |
| **TCP** | ✅ YES | Network boundary | Async only | Separate |
| **SGX** | ✅ YES | Enclave boundary | Both | Separate |

**Key Insight**: "Local" doesn't mean "no serialization" - it means "in-process with shared scheduler"

---

#### ❌ **CRITICAL MISSING: Pass-Through Implementation**

**Plan Requirement** (Milestone 5, lines 1017-1426):
```cpp
class pass_through : public i_marshaller,
                     public std::enable_shared_from_this<pass_through> {
    std::shared_ptr<transport> forward_transport_;  // B→C
    std::shared_ptr<transport> reverse_transport_;  // B→A
    destination_zone forward_destination_;
    destination_zone reverse_destination_;

    // Dual-transport routing for A→B→C topology
    // Both-or-neither operational guarantee
    // Auto-deletion on zero counts
    // Transport status monitoring with zone_terminating propagation
};
```

**Actual Status**: ❌ **NOT IMPLEMENTED**

**Impact**:
- Cannot route through intermediary zones (A→B→C topology)
- Cannot enforce both-or-neither operational guarantee
- Y-topology race conditions remain unresolved
- Multi-hop distributed topologies not supported

**Blocking**:
- Milestone 6 (Both-or-Neither Guarantee)
- Milestone 8 (Y-Topology Bidirectional)
- Full distributed topology support

**Priority**: 🔴 **CRITICAL - IMPLEMENT NEXT**

---

#### ⚠️ **TODO: create_pass_through() Implementation**

**Current State** (service.cpp:1653):
```cpp
std::shared_ptr<i_marshaller> service::create_pass_through(...) {
    RPC_WARNING("create_pass_through called but not implemented - returning nullptr");
    return nullptr;  // TODO: Implement when pass_through class exists
}
```

**Status**: ⚠️ **Correctly marked as TODO, awaiting pass_through class**

---

### Milestone 3 Deep Dive: Transport Base Class ✅ COMPLETED

The transport base class implementation EXCEEDS plan requirements:

**Implemented Features**:
```cpp
// File: rpc/include/rpc/internal/transport.h
namespace rpc {
    enum class transport_status {
        CONNECTING, CONNECTED, RECONNECTING, DISCONNECTED
    };

    class transport : public i_marshaller {
    protected:
        std::string name_;
        zone zone_id_;
        zone adjacent_zone_id_;
        std::weak_ptr<rpc::service> service_;
        std::unordered_map<destination_zone, std::weak_ptr<i_marshaller>> destinations_;
        std::shared_mutex destinations_mutex_;
        std::atomic<transport_status> status_{transport_status::CONNECTING};

    public:
        // Destination management
        void add_destination(destination_zone dest, std::weak_ptr<i_marshaller> handler);
        void remove_destination(destination_zone dest);

        // Status management
        transport_status get_status() const;
        void set_status(transport_status new_status);
        void notify_all_destinations_of_disconnect();

        // Pure virtual for derived classes
        virtual CORO_TASK(int) connect(
            rpc::interface_descriptor input_descr,
            rpc::interface_descriptor& output_descr) = 0;

        // i_marshaller implementations for routing
        CORO_TASK(int) inbound_send(...);
        CORO_TASK(void) inbound_post(...);
        CORO_TASK(int) inbound_add_ref(...);
        CORO_TASK(int) inbound_release(...);
        CORO_TASK(int) inbound_try_cast(...);
    };
}
```

**Verification** (2025-10-28):
- ✅ Base class (not interface) - correct
- ✅ Implements i_marshaller - enhancement over plan
- ✅ Has transport_status enum with all 4 states
- ✅ Has destination routing map with weak_ptr
- ✅ Thread-safe with shared_mutex
- ✅ Pure virtual connect() for derived classes
- ✅ Status management methods present
- ✅ Notification mechanism for disconnection

**Derived Transport Classes Confirmed**:
- ✅ `local::parent_transport` - child→parent communication
- ✅ `local::child_transport` - parent→child communication
- 🟡 SPSC transport - exists but needs pass-through integration
- 🟡 TCP transport - exists but needs pass-through integration
- 🟡 SGX transport - exists but needs verification

---

#### ✅ **IMPROVEMENT 3: Service and Transport Ownership Model**

**Implementation Date**: 2025-01-17

**Problem**: Test `remote_type_test/0.create_new_zone` was failing with assertion `'!"error failed " "is_empty"'` in service destructor, indicating premature service destruction while references still existed.

**Root Cause Analysis**:
1. `object_stub` was using reference to service (`service&`) instead of strong ownership
2. `child_service` had no strong reference to parent transport
3. Parent zones were being destroyed while child zones still had active references
4. Reference counting was ineffective due to incorrect ownership chain

**Ownership Requirements Established**:
1. **Parent Transport Lifetime**: Must remain alive as long as there's a positive reference count between zones in either direction
2. **Single Parent Transport**: Only one parent transport per zone
3. **Child Service Ownership**: Must have strong reference to parent transport to keep parent zone alive
4. **Transport Lifetime**: All transports and service must keep parent transport alive
5. **Stub Ownership**: Stubs instantiated in service must keep service alive via `std::shared_ptr`

**Implementation Changes**:

**1. object_stub → service Ownership** (stub.h, stub.cpp):
```cpp
// BEFORE:
class object_stub {
    service& zone_;  // Reference - no ownership
    object_stub(object id, service& zone, void* target);
};

// AFTER:
class object_stub {
    std::shared_ptr<service> zone_;  // Strong ownership
    object_stub(object id, const std::shared_ptr<service>& zone, void* target);
    std::shared_ptr<service> get_zone() const { return zone_; }  // Returns shared_ptr
};
```

**2. child_service → parent_transport Ownership** (service.h):
```cpp
class child_service : public service {
    mutable std::mutex parent_protect;  // Made mutable for const getter
    std::shared_ptr<transport> parent_transport_;  // ADDED: Strong ownership
    destination_zone parent_zone_id_;

public:
    void set_parent_transport(const std::shared_ptr<transport>& parent_transport);
    std::shared_ptr<transport> get_parent_transport() const;
};
```

**3. Zone Creation Updates** (service.h:448-453):
```cpp
parent_transport->set_service(child_svc);

// CRITICAL: Child service must keep parent transport alive
child_svc->set_parent_transport(parent_transport);

auto parent_service_proxy = std::make_shared<rpc::service_proxy>(
    "parent", parent_transport, child_svc);
```

**4. Binding Function Signatures** (bindings.h):
```cpp
// Updated to accept shared_ptr instead of reference:
template<class T>
CORO_TASK(int) stub_bind_out_param(
    const std::shared_ptr<rpc::service>& zone,  // CHANGED: from service&
    uint64_t protocol_version,
    caller_channel_zone caller_channel_zone_id,
    caller_zone caller_zone_id,
    const shared_ptr<T>& iface,
    interface_descriptor& descriptor);

template<class T>
CORO_TASK(int) stub_bind_in_param(
    uint64_t protocol_version,
    const std::shared_ptr<rpc::service>& serv,  // CHANGED: from service&
    caller_channel_zone caller_channel_zone_id,
    caller_zone caller_zone_id,
    const rpc::interface_descriptor& encap,
    rpc::shared_ptr<T>& iface);
```

**5. Code Generator Updates** (synchronous_generator.cpp):
```cpp
// Line 473: Fixed rvalue binding error
stub("auto zone_ = target_stub_strong->get_zone();");  // CHANGED: from auto&

// Line 1397-1398: Updated service access
stub("auto service = get_object_stub().lock()->get_zone();");
stub("int __rpc_ret = service->create_interface_stub(...);");
```

**6. Friend Declarations** (service.h):
```cpp
// Added to service class protected section for template access:
template<class T>
friend CORO_TASK(int) stub_bind_out_param(const std::shared_ptr<rpc::service>&, ...);

template<class T>
friend CORO_TASK(int) stub_bind_in_param(uint64_t, const std::shared_ptr<rpc::service>&, ...);
```

**Compilation Fixes**:
1. **Mutex const-ness**: Made `parent_protect` mutable for const getter
2. **Rvalue binding**: Changed `auto&` to `auto` for value returns
3. **Private access**: Added friend declarations for binding templates
4. **Generated code**: Regenerated all IDL files with updated templates

**Test Verification**:
```bash
./build/output/debug/rpc_test --gtest_filter="remote_type_test/0.create_new_zone"
```

**Results**:
- ✅ Test PASSES (previously failed with assertion)
- ✅ Proper cleanup sequence with zero reference counts
- ✅ No premature service destruction
- ✅ Child zones properly keep parent zones alive

**Debug Output Confirms Correct Behavior**:
```
[DEBUG] Remote shared count = 0 for object 1
[DEBUG] object_proxy destructor: ... (current: shared=0, optimistic=0)
[       OK ] remote_type_test/0.create_new_zone (0 ms)
```

**Files Modified**:
- `/rpc/include/rpc/internal/stub.h`
- `/rpc/src/stub.cpp`
- `/rpc/include/rpc/internal/service.h`
- `/rpc/src/service.cpp`
- `/rpc/include/rpc/internal/bindings.h`
- `/generator/src/synchronous_generator.cpp`

**Status**: ✅ **VERIFIED COMPLETE**

**Impact**: Establishes correct ownership model for zone lifecycle management, enabling reliable multi-zone distributed systems. This is foundational work required before pass-through implementation.

---

### Current Implementation Capabilities

**What Works Now** (2025-01-17):
1. ✅ Back-channel data transmission across all RPC operations
2. ✅ Fire-and-forget post() messaging (needs full testing)
3. ✅ Local child zone creation with parent↔child transports
4. ✅ Transport base class with status management
5. ✅ Destination-based routing within transports
6. ✅ Service derives from i_marshaller (no i_service)
7. ✅ Bi-modal support (sync and async modes)
8. ✅ Serialization in all transport types including local
9. ✅ **Correct ownership model for services, stubs, and transports** (2025-01-17)

**What Doesn't Work Yet**:
1. ❌ Multi-hop routing through intermediaries (A→B→C)
2. ❌ Pass-through object creation and lifecycle
3. ❌ Both-or-neither operational guarantee enforcement
4. ❌ Automatic zone_terminating propagation through pass-through
5. ❌ Y-topology race condition elimination
6. ❌ Bidirectional proxy pair creation on connect

---

### Critical Path Forward

#### Phase 1: Milestone 5 - Pass-Through Core (NEXT PRIORITY)

**Prerequisites**: ✅ **Ownership model established (2025-01-17)** - Services, stubs, and transports now have correct lifecycle management

**Tasks**:
1. Implement `pass_through` class with dual transport routing
2. Implement reference counting (shared + optimistic)
3. Implement transport status monitoring
4. Implement zone_terminating propagation
5. Implement auto-deletion on zero counts
6. Add comprehensive tests

**Estimated Effort**: 2-3 weeks

**Blocking**: All remaining milestones

**Note**: Pass-through implementation can now proceed with confidence that the underlying ownership model is correct and tested.

---

#### Phase 2: Milestone 4 Completion - Transport Status Testing

**Tasks**:
1. Comprehensive testing of status transitions
2. clone_for_zone() refusal when DISCONNECTED
3. RECONNECTING state handling for TCP
4. Integration with pass_through

**Estimated Effort**: 1 week

**Depends On**: Milestone 5 (pass-through)

---

#### Phase 3: Milestones 6-8 - Operational Guarantees

**Tasks**:
1. Both-or-neither guarantee enforcement
2. Zone termination cascade testing
3. Y-topology bidirectional proxy pairs
4. Complete integration testing

**Estimated Effort**: 3-4 weeks

**Depends On**: Milestones 4 & 5

---

### Recommendations

1. **✅ ACCEPT** transport implements i_marshaller - superior design
2. **✅ ACCEPT** local transport architecture - more complete than planned
3. **🔴 PRIORITIZE** Milestone 5 (pass-through) - critical blocking issue
4. **📝 DOCUMENT** actual local transport behavior (serialization + shared scheduler)
5. **✅ MARK** Milestone 3 as COMPLETED in all documentation
6. **🧪 TEST** existing transport status management thoroughly
7. **📋 PLAN** pass-through implementation with detailed design review

---

## Executive Summary

This master plan distills all requirements, critiques, and proposals into a concrete, milestone-based implementation roadmap featuring an **elegant transport-centric architecture**. Each milestone includes:

- **BDD Feature Specifications**: Behavior-driven scenarios describing what the feature should do
- **TDD Test Specifications**: Test-driven unit tests defining implementation contracts
- **Bi-Modal Test Requirements**: Tests that pass in BOTH sync and async modes
- **Acceptance Criteria**: Clear definition of "done"
- **Implementation Guidance**: Concrete steps with code examples

### Key Architectural Innovation: Transport Base Class

This plan introduces a more elegant entity relationship model:

1. **Transport Base Class** (not interface) - All specialized transports (SPSC, TCP, Local, SGX) derive from `transport`
2. **Destination Routing** - Transport owns `unordered_map<destination_zone, weak_ptr<i_marshaller>>`
3. **Transport Status** - Enum: CONNECTING, CONNECTED, RECONNECTING, DISCONNECTED
4. **Service_Proxy Routing** - All traffic routes through transport, uses status to refuse traffic when DISCONNECTED
5. **Pass-Through with Dual Transports** - Holds two `shared_ptr<transport>`, auto-deletes on zero counts, monitors status for both-or-neither guarantee

**Timeline**: 20 weeks (5 months) with 10 major milestones
**Effort**: ~1 developer full-time or 2 developers part-time

---

## Critical Architectural Principles

### From Q&A and Critique (MUST FOLLOW)

1. **✅ Service Derives from i_marshaller**
   - NO `i_service` interface exists
   - Service class directly implements `i_marshaller`
   - Service manages local stubs and routes to service_proxies

2. **✅ Transport Base Class Architecture (IMPLEMENTED - Enhanced Design)**
   - **NO i_transport interface** - use concrete `transport` base class instead ✅
   - Transport is a base class for all derived transport types (SPSC, TCP, Local, SGX, etc.) ✅
   - **ENHANCEMENT**: Transport implements `i_marshaller` interface (allows direct routing) ✅
   - Transport owns an `unordered_map<destination_zone, weak_ptr<i_marshaller>>` for routing ✅
   - Public API (all implemented):
     - `add_destination(destination_zone, weak_ptr<i_marshaller>)` - register handler for destination ✅
     - `remove_destination(destination_zone)` - unregister handler ✅
     - `transport_status get_status()` - returns CONNECTING, CONNECTED, RECONNECTING, DISCONNECTED ✅
     - `virtual connect()` - pure virtual for derived classes to implement handshake ✅
   - Specialized transport functions move to derived classes ✅
   - Service_proxy contains `shared_ptr<transport>` for all communication ✅
   - **Local Transport Detail**: Performs serialization/deserialization for zone isolation, shares scheduler with parent zone ✅

3. **✅ Service_Proxy Routes ALL Traffic Through Transport**
   - Service_proxy holds `shared_ptr<transport>`
   - ALL traffic from transport to service goes through service_proxy
   - ALL traffic from service to transport goes through service_proxy
   - Service_proxy uses destination_zone to route incoming messages
   - If transport is DISCONNECTED, service_proxy refuses all further traffic

4. **✅ Pass-Through Implements i_marshaller**
   - Pass-through implements `i_marshaller` interface
   - Holds two `shared_ptr<transport>` objects (forward and reverse transports)
   - When called, forwards to appropriate transport based on destination_zone
   - **If either transport is DISCONNECTED**:
     - Sends `post()` with `zone_terminating` to the other transport
     - Transport receives notification and sets its own status to DISCONNECTED
     - Immediately deletes itself to prevent asymmetric state
   - **Auto-deletes when both mutual optimistic and shared counts reach zero**
   - Releases pointers to both transports and service on deletion

5. **✅ Both-or-Neither Operational Guarantee**
   - Pass-through guarantees BOTH transports operational or BOTH non-operational
   - Monitors transport status via `get_status()` method
   - No asymmetric states allowed
   - `clone_for_zone()` must refuse if transport NOT CONNECTED

6. **✅ Back-Channel Format**
   - Use `vector<rpc::back_channel_entry>` structure
   - Each entry: `uint64_t type_id` + `vector<uint8_t> payload`
   - Type IDs from IDL-generated fingerprints

7. **✅ Bi-Modal Support**
   - Local/SGX/DLL support BOTH sync and async
   - SPSC/TCP are async-only (require coroutines)
   - All solutions must work in both modes

8. **✅ Zone Termination Broadcast**
   - Transport detects failure and sets status to DISCONNECTED
   - Service_proxy detects DISCONNECTED and broadcasts `zone_terminating`
   - Notification sent to service AND pass-through
   - Cascading cleanup through topology

---

## Milestone-Based Implementation

### Milestone 1: i_marshaller Back-Channel Support (Week 1-2) ✅ COMPLETED

**Objective**: Add back-channel support to all i_marshaller methods

**Status**: ✅ **COMPLETED** - 2025-01-20
**Completion Notes**: All back-channel infrastructure implemented and tested. Build passes successfully with all changes integrated.

#### BDD Feature: Back-channel data transmission
```gherkin
Feature: Back-channel data transmission
  As an RPC developer
  I want to send metadata alongside RPC calls
  So that I can support distributed tracing, auth tokens, and certificates

  Scenario: Send back-channel data with RPC call
    Given a service proxy connected to remote zone
    And back-channel entry with type_id 12345 and payload "trace-123"
    When I invoke send() with back-channel data
    Then the remote service receives the back-channel entry
    And the response includes back-channel data from remote

  Scenario: Multiple back-channel entries
    Given a service proxy connected to remote zone
    And back-channel entries for OpenTelemetry and auth token
    When I invoke send() with multiple entries
    Then all entries are transmitted
    And response can include multiple back-channel entries

  Scenario: Back-channel with fire-and-forget post
    Given a service proxy connected to remote zone
    And back-channel entry with telemetry data
    When I invoke post() with back-channel data
    Then the message is sent without waiting for response
    And back-channel data is transmitted one-way
```

#### TDD Test Specifications

**Test 1.1**: `back_channel_entry` structure
```cpp
TEST_CASE("back_channel_entry basic structure") {
    // GIVEN
    rpc::back_channel_entry entry;
    entry.type_id = 0x12345678ABCDEF00;
    entry.payload = {0x01, 0x02, 0x03, 0x04};

    // WHEN - serialize and deserialize
    std::vector<char> buffer;
    to_yas_binary(entry, buffer);
    rpc::back_channel_entry deserialized;
    from_yas_binary(buffer, deserialized);

    // THEN
    REQUIRE(deserialized.type_id == entry.type_id);
    REQUIRE(deserialized.payload == entry.payload);
}
```

**Test 1.2**: i_marshaller send() with back-channel
```cpp
CORO_TYPED_TEST(service_proxy_test, "send with back_channel") {
    // GIVEN
    auto service_a = create_service("zone_a");
    auto service_b = create_service("zone_b");
    auto proxy_a_to_b = connect_zones(service_a, service_b);

    rpc::back_channel_entry trace_entry;
    trace_entry.type_id = TRACE_ID_FINGERPRINT;
    trace_entry.payload = serialize_trace_id("request-123");
    std::vector<rpc::back_channel_entry> in_back_channel = {trace_entry};
    std::vector<rpc::back_channel_entry> out_back_channel;

    // WHEN
    std::vector<char> out_buf;
    auto error = CO_AWAIT proxy_a_to_b->send(
        VERSION_3, encoding::yas_binary, tag++,
        caller_channel_zone{1}, caller_zone{1}, destination_zone{2},
        object{100}, interface_ordinal{1}, method{5},
        in_size, in_buf, out_buf,
        in_back_channel, out_back_channel);

    // THEN
    REQUIRE(error == rpc::error::OK());
    REQUIRE(!out_back_channel.empty());
    // Verify trace ID was received and response includes server trace data
}

// BI-MODAL REQUIREMENT: Test must pass in BOTH modes
#ifdef BUILD_COROUTINE
TEST_CASE("send with back_channel - async mode") { /* async variant */ }
#else
TEST_CASE("send with back_channel - sync mode") { /* sync variant */ }
#endif
```

**Test 1.3**: post() with back-channel (fire-and-forget)
```cpp
CORO_TYPED_TEST(service_proxy_test, "post with back_channel") {
    // GIVEN
    auto service_a = create_service("zone_a");
    auto service_b = create_service("zone_b");
    auto proxy_a_to_b = connect_zones(service_a, service_b);

    rpc::back_channel_entry metric_entry;
    metric_entry.type_id = METRIC_ID_FINGERPRINT;
    metric_entry.payload = serialize_metric("request_count", 1);
    std::vector<rpc::back_channel_entry> in_back_channel = {metric_entry};

    // WHEN
    CO_AWAIT proxy_a_to_b->post(
        VERSION_3, encoding::yas_binary, tag++,
        caller_channel_zone{1}, caller_zone{1}, destination_zone{2},
        object{100}, interface_ordinal{1}, method{10},
        post_options::normal,
        in_size, in_buf, in_back_channel);

    // THEN - fire-and-forget completes immediately
    // Verify via telemetry or service-side validation that metric was received
}
```

#### Implementation Tasks

**Task 1.1**: Define `back_channel_entry` structure (Header: `rpc/include/rpc/types.h`)
```cpp
struct back_channel_entry {
    uint64_t type_id;              // IDL-generated fingerprint
    std::vector<uint8_t> payload;  // Binary payload (application-defined)

    template<typename Ar> void serialize(Ar& ar) {
        ar & YAS_OBJECT_NVP("back_channel_entry",
            ("type_id", type_id),
            ("payload", payload));
    }
};
```

**Task 1.2**: Update `i_marshaller` interface (Header: `rpc/include/rpc/internal/marshaller.h`)
```cpp
class i_marshaller {
public:
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
        const std::vector<rpc::back_channel_entry>& in_back_channel,   // NEW
        std::vector<rpc::back_channel_entry>& out_back_channel          // NEW
    ) = 0;

    virtual CORO_TASK(void) post(
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
        const std::vector<rpc::back_channel_entry>& in_back_channel   // NEW
    ) = 0;

    virtual CORO_TASK(int) add_ref(
        uint64_t protocol_version,
        destination_channel_zone destination_channel_zone_id,
        destination_zone destination_zone_id,
        object object_id,
        caller_channel_zone caller_channel_zone_id,
        caller_zone caller_zone_id,
        known_direction_zone known_direction_zone_id,
        add_ref_options options,
        uint64_t& reference_count,
        const std::vector<rpc::back_channel_entry>& in_back_channel,   // NEW
        std::vector<rpc::back_channel_entry>& out_back_channel          // NEW
    ) = 0;

    virtual CORO_TASK(int) release(
        uint64_t protocol_version,
        destination_zone destination_zone_id,
        object object_id,
        caller_zone caller_zone_id,
        release_options options,
        uint64_t& reference_count,
        const std::vector<rpc::back_channel_entry>& in_back_channel,   // NEW
        std::vector<rpc::back_channel_entry>& out_back_channel          // NEW
    ) = 0;

    virtual CORO_TASK(int) try_cast(
        uint64_t protocol_version,
        destination_zone destination_zone_id,
        object object_id,
        interface_ordinal interface_id,
        const std::vector<rpc::back_channel_entry>& in_back_channel,   // NEW
        std::vector<rpc::back_channel_entry>& out_back_channel          // NEW
    ) = 0;
};
```

**Task 1.3**: Update service implementation
- File: `rpc/src/service.cpp`
- Update all i_marshaller method implementations to handle back-channel
- Pass back-channel data through routing logic

**Task 1.4**: Update service_proxy implementations
- Files: All service_proxy files (SPSC, Local, SGX, etc.)
- Update all i_marshaller method implementations
- Serialize/deserialize back-channel data in transport

**Task 1.5**: Update SPSC channel_manager
- File: `rpc/include/rpc/service_proxies/spsc/channel_manager.h`
- Add back-channel to envelope structure
- Update marshalling/unmarshalling logic

#### Acceptance Criteria

- ✅ `back_channel_entry` structure defined and serializable
- ✅ All i_marshaller methods updated with back-channel parameters
- ✅ Service implementation passes back-channel data
- ✅ Service_proxy implementations marshal back-channel correctly
- ✅ Tests pass in BOTH sync and async modes
- ✅ All existing tests still pass (backward compatibility)
- ✅ Telemetry shows back-channel data transmitted

#### Bi-Modal Testing Requirements

```cpp
// Test framework must run in BOTH modes
#ifdef BUILD_COROUTINE
  // Async mode: use CO_AWAIT
  auto result = CO_AWAIT proxy->send(..., in_back_channel, out_back_channel);
#else
  // Sync mode: blocking call
  auto result = proxy->send(..., in_back_channel, out_back_channel);
#endif
```

---

### Milestone 2: post() Fire-and-Forget Messaging (Week 3-4) - PARTIALLY COMPLETED

**Objective**: Implement fire-and-forget messaging with post() method

#### BDD Feature: Fire-and-forget messaging
```gherkin
Feature: Fire-and-forget messaging
  As an RPC developer
  I want to send one-way messages without waiting for response
  So that I can optimize performance for notifications and cleanup

  Scenario: Send post message without blocking
    Given a service proxy connected to remote zone
    And a post message with method "log_event"
    When I invoke post() with post_options::normal
    Then the method returns immediately
    And the message is delivered asynchronously
    And no response is expected

  Scenario: Zone termination via post
    Given a service proxy connected to remote zone
    And the remote zone is shutting down
    When the remote sends post() with post_options::zone_terminating
    Then the local service receives the termination notification
    And all proxies to that zone are marked non-operational
    And cleanup is triggered

  Scenario: Optimistic cleanup via post
    Given a pass-through routing between zones A and C
    And zone B acting as intermediary
    And optimistic_count reaches zero for an object
    When zone C sends post() with post_options::release_optimistic
    Then zone A receives the cleanup notification
    And optimistic references are released
    And no response is sent back
```

#### TDD Test Specifications

**Test 2.1**: post() completes immediately
```cpp
CORO_TYPED_TEST(post_messaging_test, "post completes without waiting") {
    // GIVEN
    auto service_a = create_service("zone_a");
    auto service_b = create_service("zone_b");
    auto proxy_a_to_b = connect_zones(service_a, service_b);

    // WHEN
    auto start = std::chrono::steady_clock::now();
    CO_AWAIT proxy_a_to_b->post(
        VERSION_3, encoding::yas_binary, tag++,
        caller_channel_zone{1}, caller_zone{1}, destination_zone{2},
        object{100}, interface_ordinal{1}, method{20},
        post_options::normal,
        in_size, in_buf, {});
    auto duration = std::chrono::steady_clock::now() - start;

    // THEN - should complete in microseconds (not wait for processing)
    REQUIRE(duration < std::chrono::milliseconds(10));
}
```

**Test 2.2**: zone_terminating notification
```cpp
CORO_TYPED_TEST(post_messaging_test, "zone_terminating broadcast") {
    // GIVEN
    auto service_a = create_service("zone_a");
    auto service_b = create_service("zone_b");
    auto service_c = create_service("zone_c");
    auto proxy_a_to_b = connect_zones(service_a, service_b);
    auto proxy_b_to_c = connect_zones(service_b, service_c);

    // WHEN - zone B terminates
    CO_AWAIT service_b->shutdown_and_broadcast_termination();

    // THEN - zones A and C receive termination notification
    REQUIRE(!proxy_a_to_b->is_operational());
    REQUIRE_EVENTUALLY(service_a->get_zone_status(zone{2}) == zone_status::terminated);
}
```

**Test 2.3**: Bi-modal post() behavior
```cpp
#ifdef BUILD_COROUTINE
CORO_TYPED_TEST(post_messaging_test, "post in async mode") {
    // Async mode - truly non-blocking
    CO_AWAIT proxy->post(..., post_options::normal, ...);
    // Returns immediately, message processed asynchronously
}
#else
TEST(post_messaging_test, "post in sync mode") {
    // Sync mode - still blocking but simpler
    proxy->post(..., post_options::normal, ...);
    // Blocks until message processed, but no response expected
}
#endif
```

#### Implementation Tasks

**Task 2.1**: Define `post_options` enum - ✅ **COMPLETED**
- Defined in `rpc/include/rpc/internal/marshaller.h`
- Values: normal, zone_terminating, release_optimistic
- Used throughout the system

**Task 2.2**: Implement service::handle_post() - ⏳ **PENDING** (requires remaining components)
- Service post method receives and processes messages with logging
- Implementation will be completed when remaining components are in place
- Will handle zone_terminating and release_optimistic options

**Task 2.3**: Update channel managers - ✅ **COMPLETED**
- ✅ SPSC: Added post message type to envelope and processing
- ✅ TCP: Added post message type to envelope and processing  
- ✅ Local: Direct post without serialization (inherited)

**Task 2.4**: Implement zone termination broadcast - ⏳ **PENDING** (requires remaining components)
- Implementation will be completed when remaining components are in place
- Will broadcast zone_terminating notifications when zones fail

#### Acceptance Criteria

- ✅ post() method implemented for all i_marshaller implementations
- ✅ post_options enum defined with required options
- ✅ post() completes without waiting for response
- ⏳ zone_terminating notification works (pending Task 2.2 and 2.4)
- ⏳ Optimistic cleanup via post() works (pending Task 2.2 and 2.4)
- ✅ Tests pass in BOTH sync and async modes
- ✅ Telemetry tracks post messages

---

### Milestone 3: Transport Base Class and Destination Routing (Week 5-6) ✅ COMPLETED

**Objective**: Implement transport base class with destination-based routing

**Status**: ✅ **COMPLETED** - 2025-10-28
**Implementation Notes**: Fully implemented with enhancements. Transport implements i_marshaller (improvement over plan). Local transport includes serialization and shared scheduler (more sophisticated than planned).

#### BDD Feature: Transport base class with destination routing
```gherkin
Feature: Transport base class with destination routing
  As a transport base class
  I want to route messages to different destinations
  So that service_proxy and pass_through can communicate through me

  Scenario: Register destination with service handler
    Given a transport instance (SPSC/TCP/Local)
    When I register destination zone{1} with service as handler
    Then incoming messages for zone{1} are routed to service
    And service dispatches to local stubs

  Scenario: Register destination with pass_through handler
    Given a transport instance
    When I register destination zone{3} with pass_through as handler
    Then incoming messages for zone{3} are routed to pass_through
    And pass_through forwards to appropriate transport

  Scenario: Multiple destinations on one transport
    Given a TCP transport connected to remote zone
    And destination zone{1} registered with service
    And destination zone{3} registered with pass_through
    When messages arrive for zone{1} and zone{3}
    Then transport routes zone{1} messages to service
    And transport routes zone{3} messages to pass_through

  Scenario: Transport status transitions
    Given a transport in CONNECTING state
    When connection establishes
    Then transport status becomes CONNECTED
    When connection fails
    Then transport status becomes DISCONNECTED
    And all registered destinations are notified
```

#### TDD Test Specifications

**Test 3.1**: Transport base class instantiation
```cpp
TEST_CASE("transport base class structure") {
    // GIVEN
    enum class transport_status { CONNECTING, CONNECTED, RECONNECTING, DISCONNECTED };

    class transport {
    protected:
        std::unordered_map<destination_zone, std::weak_ptr<i_marshaller>> destinations_;
        std::shared_mutex destinations_mutex_;
        std::atomic<transport_status> status_{transport_status::CONNECTING};

    public:
        void add_destination(destination_zone dest, std::weak_ptr<i_marshaller> handler);
        void remove_destination(destination_zone dest);
        transport_status get_status() const;
    };

    // WHEN - create derived transport
    class spsc_transport : public transport { /* ... */ };
    auto spsc = std::make_shared<spsc_transport>();

    // THEN - base class API available
    REQUIRE(spsc->get_status() == transport_status::CONNECTING);
}
```

**Test 3.2**: Add and remove destinations
```cpp
TEST_CASE("transport destination registration") {
    // GIVEN
    auto transport = create_spsc_transport();
    auto service = std::make_shared<rpc::service>("test_zone", zone{1});

    // WHEN - register destination
    transport->add_destination(destination_zone{1}, service);

    // THEN - destination is registered
    auto handler = transport->get_destination_handler(destination_zone{1});
    REQUIRE(handler.lock() == service);

    // WHEN - remove destination
    transport->remove_destination(destination_zone{1});

    // THEN - destination is unregistered
    auto removed_handler = transport->get_destination_handler(destination_zone{1});
    REQUIRE(removed_handler.lock() == nullptr);
}
```

**Test 3.3**: Message routing by destination
```cpp
CORO_TYPED_TEST(transport_routing_test, "route by destination zone") {
    // GIVEN
    auto transport = create_tcp_transport();
    auto service = create_service("zone_a", zone{1});
    auto pass_through = create_pass_through();

    transport->add_destination(destination_zone{1}, service);
    transport->add_destination(destination_zone{3}, pass_through);

    // WHEN - messages arrive for different destinations
    auto msg_for_service = create_message(dest=zone{1}, caller=zone{2});
    auto msg_for_passthrough = create_message(dest=zone{3}, caller=zone{2});

    transport->handle_incoming_message(msg_for_service);
    transport->handle_incoming_message(msg_for_passthrough);

    // THEN - transport routes to correct handlers
    REQUIRE_EVENTUALLY(service->has_processed_message());
    REQUIRE_EVENTUALLY(pass_through->has_routed_message());
}
```

**Test 3.4**: Transport status management
```cpp
CORO_TYPED_TEST(transport_status_test, "status transitions") {
    // GIVEN
    auto transport = create_tcp_transport("host", 8080);
    REQUIRE(transport->get_status() == transport_status::CONNECTING);

    // WHEN - connection establishes
    CO_AWAIT transport->connect();

    // THEN - status is CONNECTED
    REQUIRE(transport->get_status() == transport_status::CONNECTED);

    // WHEN - connection fails
    simulate_connection_failure(transport);

    // THEN - status is DISCONNECTED
    REQUIRE(transport->get_status() == transport_status::DISCONNECTED);
}
```

**Test 3.5**: Service_proxy refuses traffic when DISCONNECTED
```cpp
CORO_TYPED_TEST(transport_status_test, "refuse traffic when disconnected") {
    // GIVEN
    auto transport = create_connected_transport();
    auto proxy = create_service_proxy_with_transport(transport);

    // WHEN - transport becomes DISCONNECTED
    simulate_connection_failure(transport);
    REQUIRE(transport->get_status() == transport_status::DISCONNECTED);

    // Attempt to send message
    std::vector<char> out_buf;
    auto error = CO_AWAIT proxy->send(
        VERSION_3, encoding::yas_binary, tag++,
        caller_channel_zone{1}, caller_zone{1}, destination_zone{2},
        object{100}, interface_ordinal{1}, method{5},
        in_size, in_buf, out_buf, {}, {});

    // THEN - send refused with transport error
    REQUIRE(error == rpc::error::TRANSPORT_ERROR());
}
```

#### Implementation Tasks

**Task 3.1**: Define transport base class
```cpp
// Header: rpc/include/rpc/transport/transport.h
namespace rpc {

enum class transport_status {
    CONNECTING,      // Initial state, establishing connection
    CONNECTED,       // Fully operational
    RECONNECTING,    // Attempting to recover connection
    DISCONNECTED     // Terminal state, no further traffic allowed
};

class transport {
protected:
    // Map destination_zone to handler (service or pass_through)
    std::unordered_map<destination_zone, std::weak_ptr<i_marshaller>> destinations_;
    std::shared_mutex destinations_mutex_;
    std::atomic<transport_status> status_{transport_status::CONNECTING};

public:
    virtual ~transport() = default;

    // Destination management
    void add_destination(destination_zone dest, std::weak_ptr<i_marshaller> handler) {
        std::unique_lock lock(destinations_mutex_);
        destinations_[dest] = handler;
    }

    void remove_destination(destination_zone dest) {
        std::unique_lock lock(destinations_mutex_);
        destinations_.erase(dest);
    }

    // Status management
    transport_status get_status() const {
        return status_.load(std::memory_order_acquire);
    }

protected:
    // Helper for derived classes to route incoming messages
    std::shared_ptr<i_marshaller> get_destination_handler(destination_zone dest) const {
        std::shared_lock lock(destinations_mutex_);
        auto it = destinations_.find(dest);
        if (it != destinations_.end()) {
            return it->second.lock();
        }
        return nullptr;
    }

    void set_status(transport_status new_status) {
        status_.store(new_status, std::memory_order_release);
    }
};

} // namespace rpc
```

**Task 3.2**: Update SPSC transport to derive from transport base
```cpp
// Header: rpc/include/rpc/service_proxies/spsc/spsc_transport.h
class spsc_transport : public transport {
    // Existing SPSC channel_manager implementation
    // ...

    void incoming_message_handler(envelope_prefix prefix, envelope_payload payload) {
        // Extract destination_zone from message
        destination_zone dest = extract_destination_zone(payload);

        // Get handler for destination
        auto handler = get_destination_handler(dest);
        if (!handler) {
            RPC_ERROR("No handler registered for destination zone {}", dest.get_val());
            return;
        }

        // Route based on message type
        switch (prefix.message_type) {
            case message_type::send:
                handler->send(...);
                break;
            case message_type::post:
                handler->post(...);
                break;
            // ... other message types
        }
    }

    // Update status on connection state changes
    void on_peer_connected() {
        set_status(transport_status::CONNECTED);
    }

    void on_peer_disconnected() {
        set_status(transport_status::DISCONNECTED);
        notify_all_destinations_of_disconnect();
    }
};
```

**Task 3.3**: Update service_proxy to check transport status
```cpp
// File: rpc/include/rpc/internal/service_proxy.h
class service_proxy : public i_marshaller {
    std::shared_ptr<transport> transport_;

    CORO_TASK(int) send(...) override {
        // Check transport status before sending
        if (transport_->get_status() != transport_status::CONNECTED) {
            CO_RETURN rpc::error::TRANSPORT_ERROR();
        }

        // Proceed with send...
    }
};
```

**Task 3.4**: Create derived transport classes
```cpp
// TCP transport
class tcp_transport : public transport { /* TCP-specific implementation */ };

// Local transport (in-process)
class local_transport : public transport { /* Direct call implementation */ };

// SGX transport
class sgx_transport : public transport { /* SGX enclave implementation */ };
```

#### Acceptance Criteria

- ✅ Transport base class defined with destination routing
- ✅ transport_status enum with CONNECTING, CONNECTED, RECONNECTING, DISCONNECTED
- ✅ add_destination() and remove_destination() methods work
- ✅ get_status() returns current transport state
- ✅ Service_proxy refuses traffic when transport is DISCONNECTED
- ✅ Multiple destinations can be registered on one transport
- ✅ Message routing works by destination_zone
- ✅ All derived transport types (SPSC, TCP, Local, SGX) inherit from base
- ✅ Tests pass in BOTH sync and async modes

---

### Milestone 4: Transport Status Monitoring and Enforcement (Week 7-8)

**Objective**: Implement transport status monitoring and enforce operational guarantees

#### BDD Feature: Transport status monitoring
```gherkin
Feature: Transport status monitoring
  As a service_proxy or pass_through
  I want to monitor transport status
  So that I can enforce operational guarantees

  Scenario: Transport is CONNECTED
    Given an SPSC transport with active pump tasks
    And peer has not canceled
    When I check get_status()
    Then it returns CONNECTED
    And clone_for_zone() can create new proxies

  Scenario: Transport becomes DISCONNECTED - peer canceled
    Given an SPSC transport
    And peer sends zone_terminating
    When I check get_status()
    Then it returns DISCONNECTED
    And clone_for_zone() refuses to create new proxies

  Scenario: Transport status RECONNECTING
    Given a TCP transport that loses connection
    When automatic reconnection is attempted
    Then get_status() returns RECONNECTING
    And new operations are queued
    When connection re-establishes
    Then get_status() returns CONNECTED
    And queued operations are processed
```

#### TDD Test Specifications

**Test 4.1**: Status is CONNECTED when active
```cpp
TEST_CASE("transport status CONNECTED when active") {
    // GIVEN
    auto transport = create_active_spsc_transport();

    // WHEN/THEN
    REQUIRE(transport->get_status() == transport_status::CONNECTED);
}
```

**Test 4.2**: Status becomes DISCONNECTED after peer cancel
```cpp
CORO_TYPED_TEST(transport_status_test, "DISCONNECTED after peer cancel") {
    // GIVEN
    auto transport = create_active_spsc_transport();
    REQUIRE(transport->get_status() == transport_status::CONNECTED);

    // WHEN - peer sends zone_terminating
    CO_AWAIT simulate_peer_zone_termination(transport);

    // THEN
    REQUIRE(transport->get_status() == transport_status::DISCONNECTED);
}
```

**Test 4.3**: clone_for_zone() refuses when DISCONNECTED
```cpp
CORO_TYPED_TEST(transport_status_test, "clone refuses DISCONNECTED transport") {
    // GIVEN
    auto transport = create_active_spsc_transport();
    auto proxy = create_service_proxy_with_transport(transport);

    // WHEN - transport becomes DISCONNECTED
    CO_AWAIT transport->shutdown();
    REQUIRE(transport->get_status() == transport_status::DISCONNECTED);

    // Attempt to clone
    auto cloned_proxy = CO_AWAIT proxy->clone_for_zone(zone{5}, zone{1});

    // THEN - clone refused
    REQUIRE(cloned_proxy == nullptr);
}
```

**Test 4.4**: RECONNECTING state handling
```cpp
CORO_TYPED_TEST(transport_status_test, "RECONNECTING state") {
    // GIVEN
    auto transport = create_tcp_transport("host", 8080);
    CO_AWAIT transport->connect();
    REQUIRE(transport->get_status() == transport_status::CONNECTED);

    // WHEN - connection lost, auto-reconnect initiated
    simulate_transient_network_failure(transport);

    // THEN - status is RECONNECTING
    REQUIRE(transport->get_status() == transport_status::RECONNECTING);

    // WHEN - connection re-established
    CO_AWAIT wait_for_reconnection(transport);

    // THEN - status is CONNECTED again
    REQUIRE(transport->get_status() == transport_status::CONNECTED);
}
```

#### Implementation Tasks

**Task 4.1**: Implement status transitions for SPSC
```cpp
class spsc_transport : public transport {
    void on_connection_established() {
        set_status(transport_status::CONNECTED);
    }

    void on_peer_cancel_received() {
        set_status(transport_status::DISCONNECTED);
        notify_all_destinations_of_disconnect();
    }

    void on_local_shutdown_initiated() {
        set_status(transport_status::DISCONNECTED);
    }
};
```

**Task 4.2**: Implement status transitions for TCP
```cpp
class tcp_transport : public transport {
    void on_connection_established() {
        set_status(transport_status::CONNECTED);
    }

    void on_connection_lost() {
        if (auto_reconnect_enabled_) {
            set_status(transport_status::RECONNECTING);
            initiate_reconnection();
        } else {
            set_status(transport_status::DISCONNECTED);
        }
    }

    void on_reconnection_succeeded() {
        set_status(transport_status::CONNECTED);
        process_queued_operations();
    }

    void on_reconnection_failed() {
        set_status(transport_status::DISCONNECTED);
        notify_all_destinations_of_disconnect();
    }
};
```

**Task 4.3**: Implement status for Local transport
```cpp
class local_transport : public transport {
    local_transport() {
        // Local transport is always connected
        set_status(transport_status::CONNECTED);
    }
};
```

**Task 4.4**: Add transport status check to clone_for_zone()
```cpp
CORO_TASK(std::shared_ptr<service_proxy>) service_proxy::clone_for_zone(
    destination_zone dest, caller_zone caller) {

    // CRITICAL: Check transport status before cloning
    if (!transport_ || transport_->get_status() != transport_status::CONNECTED) {
        CO_RETURN nullptr; // Refuse to clone on non-CONNECTED transport
    }

    // Proceed with clone...
}
```

**Task 4.5**: Add disconnect notification to all destinations
```cpp
class transport {
protected:
    void notify_all_destinations_of_disconnect() {
        std::shared_lock lock(destinations_mutex_);
        for (auto& [dest_zone, handler_weak] : destinations_) {
            if (auto handler = handler_weak.lock()) {
                // Send zone_terminating post to each destination
                handler->post(
                    VERSION_3, encoding::yas_binary, 0,
                    caller_channel_zone{0}, caller_zone{0}, dest_zone,
                    object{0}, interface_ordinal{0}, method{0},
                    post_options::zone_terminating,
                    0, nullptr, {});
            }
        }
    }
};
```

#### Acceptance Criteria

- ✅ get_status() implemented for all transports
- ✅ Status transitions work: CONNECTING → CONNECTED → DISCONNECTED
- ✅ RECONNECTING status works for transports that support it
- ✅ Returns DISCONNECTED when peer terminates
- ✅ Returns DISCONNECTED when local shutdown initiated
- ✅ clone_for_zone() refuses when status is not CONNECTED
- ✅ All destinations notified when transport becomes DISCONNECTED
- ✅ Tests pass in BOTH sync and async modes

---

### Milestone 5: Pass-Through Core Implementation (Week 9-11)

**Objective**: Implement pass_through class with dual-transport routing

#### BDD Feature: Pass-through dual-transport routing
```gherkin
Feature: Pass-through dual-transport routing
  As a pass_through between zones
  I want to route messages between two transports
  So that zones can communicate through intermediaries

  Scenario: Forward direction routing
    Given zones A, B, C with B as intermediary
    And pass_through with forward_transport (B→C) and reverse_transport (B→A)
    And pass_through registered as destination on both transports
    When zone A sends message to zone C (arrives via reverse_transport)
    Then pass_through routes to forward_transport
    And zone C receives the message

  Scenario: Reverse direction routing
    Given zones A, B, C with B as intermediary
    And pass_through with forward_transport (B→C) and reverse_transport (B→A)
    When zone C sends message to zone A (arrives via forward_transport)
    Then pass_through routes to reverse_transport
    And zone A receives the message

  Scenario: Pass-through manages reference counting
    Given a pass_through with two transports
    When add_ref is called
    Then pass_through updates its internal reference count
    And maintains single count to service
    And auto-deletes when both shared and optimistic counts reach zero

  Scenario: Pass-through detects transport disconnection
    Given a pass_through with two transports
    When forward_transport becomes DISCONNECTED
    Then pass_through detects the status change
    And sends post(zone_terminating) to reverse_transport
    And reverse_transport receives notification and sets its own status to DISCONNECTED
    And pass_through immediately deletes itself to prevent asymmetric state
    And releases all pointers to transports and service
```

#### TDD Test Specifications

**Test 5.1**: Forward routing via transports
```cpp
CORO_TYPED_TEST(pass_through_test, "forward direction routing") {
    // GIVEN
    auto service_a = create_service("zone_a", zone{1});
    auto service_b = create_service("zone_b", zone{2});
    auto service_c = create_service("zone_c", zone{3});

    auto forward_transport = create_transport(service_b, service_c);  // B→C
    auto reverse_transport = create_transport(service_b, service_a);  // B→A

    auto pass_through = std::make_shared<rpc::pass_through>(
        forward_transport, reverse_transport, service_b, zone{3}, zone{1});

    // Register pass_through as destination on both transports
    forward_transport->add_destination(destination_zone{1}, pass_through);  // C→A messages
    reverse_transport->add_destination(destination_zone{3}, pass_through);  // A→C messages

    // WHEN - send from A to C (destination=3, arrives via reverse_transport)
    std::vector<char> out_buf;
    auto error = CO_AWAIT pass_through->send(
        VERSION_3, encoding::yas_binary, tag++,
        caller_channel_zone{2}, caller_zone{1}, destination_zone{3},
        object{100}, interface_ordinal{1}, method{5},
        in_size, in_buf, out_buf, {}, {});

    // THEN - routed to forward_transport
    REQUIRE(error == rpc::error::OK());
    REQUIRE(service_c->has_received_message());
}
```

**Test 5.2**: Reverse routing via transports
```cpp
CORO_TYPED_TEST(pass_through_test, "reverse direction routing") {
    // GIVEN
    auto pass_through = create_pass_through_A_to_C_via_B();

    // WHEN - send from C to A (destination=1, arrives via forward_transport)
    auto error = CO_AWAIT pass_through->send(
        VERSION_3, encoding::yas_binary, tag++,
        caller_channel_zone{2}, caller_zone{3}, destination_zone{1},
        object{200}, interface_ordinal{1}, method{10},
        in_size, in_buf, out_buf, {}, {});

    // THEN - routed to reverse_transport
    REQUIRE(error == rpc::error::OK());
    REQUIRE(service_a->has_received_message());
}
```

**Test 5.3**: Reference counting management
```cpp
CORO_TYPED_TEST(pass_through_test, "manages reference counting") {
    // GIVEN
    auto pass_through = create_pass_through_A_to_C_via_B();

    // WHEN - add_ref called
    uint64_t ref_count = 0;
    auto error = CO_AWAIT pass_through->add_ref(
        VERSION_3, destination_channel_zone{2}, destination_zone{3},
        object{100}, caller_channel_zone{2}, caller_zone{1},
        known_direction_zone{2}, add_ref_options::normal, ref_count, {}, {});

    // THEN - pass_through tracks count
    REQUIRE(error == rpc::error::OK());
    REQUIRE(ref_count == 1);
    REQUIRE(pass_through->get_shared_count() == 1);
}
```

**Test 5.4**: Auto-delete on zero counts
```cpp
CORO_TYPED_TEST(pass_through_test, "auto delete on zero counts") {
    // GIVEN
    auto pass_through = create_pass_through_A_to_C_via_B();
    std::weak_ptr<pass_through> weak_pt = pass_through;

    // Increment then decrement shared count to 1, then to 0
    uint64_t ref_count = 0;
    CO_AWAIT pass_through->add_ref(..., ref_count, ...);  // shared_count = 1
    CO_AWAIT pass_through->release(..., ref_count, ...);  // shared_count = 0

    // WHEN - both shared and optimistic counts are zero
    REQUIRE(pass_through->get_shared_count() == 0);
    REQUIRE(pass_through->get_optimistic_count() == 0);

    // Release our reference
    pass_through.reset();

    // THEN - pass_through auto-deleted
    REQUIRE(weak_pt.expired());
}
```

**Test 5.5**: Detect, send zone_terminating, and delete
```cpp
CORO_TYPED_TEST(pass_through_test, "detect send terminating and delete") {
    // GIVEN
    auto pass_through = create_pass_through_A_to_C_via_B();
    std::weak_ptr<pass_through> weak_pt = pass_through;

    auto forward_transport = pass_through->get_forward_transport();
    auto reverse_transport = pass_through->get_reverse_transport();

    // Setup telemetry to capture zone_terminating post
    auto post_monitor = setup_post_monitor(reverse_transport);

    REQUIRE(forward_transport->get_status() == transport_status::CONNECTED);
    REQUIRE(reverse_transport->get_status() == transport_status::CONNECTED);
    REQUIRE(!weak_pt.expired());

    // WHEN - forward_transport disconnects
    simulate_transport_failure(forward_transport);
    REQUIRE(forward_transport->get_status() == transport_status::DISCONNECTED);

    // Give pass_through time to detect and respond
    CO_AWAIT std::chrono::milliseconds(100);

    // THEN - pass_through sends post(zone_terminating) to reverse_transport
    REQUIRE(post_monitor->received_zone_terminating());

    // AND reverse_transport sets its own status to DISCONNECTED upon receiving the post
    REQUIRE(reverse_transport->get_status() == transport_status::DISCONNECTED);

    // AND pass_through deletes itself
    REQUIRE(weak_pt.expired());

    // Verify destinations removed from both transports
    REQUIRE(forward_transport->get_destination_handler(destination_zone{1}) == nullptr);
    REQUIRE(reverse_transport->get_destination_handler(destination_zone{3}) == nullptr);
}
```

#### Implementation Tasks

**Task 5.1**: Create pass_through class with dual transports
```cpp
// Header: rpc/include/rpc/internal/pass_through.h
class pass_through : public i_marshaller,
                     public std::enable_shared_from_this<pass_through> {
    destination_zone forward_destination_;  // Zone reached via forward_transport
    destination_zone reverse_destination_;  // Zone reached via reverse_transport

    std::atomic<uint64_t> shared_count_{0};
    std::atomic<uint64_t> optimistic_count_{0};

    std::shared_ptr<transport> forward_transport_;  // Transport to forward destination
    std::shared_ptr<transport> reverse_transport_;  // Transport to reverse destination
    std::weak_ptr<service> service_;

    std::atomic<bool> monitoring_active_{true};

public:
    pass_through(std::shared_ptr<transport> forward,
                std::shared_ptr<transport> reverse,
                std::shared_ptr<service> service,
                destination_zone forward_dest,
                destination_zone reverse_dest);

    ~pass_through();

    // i_marshaller implementations
    CORO_TASK(int) send(...) override;
    CORO_TASK(void) post(...) override;
    CORO_TASK(int) add_ref(...) override;
    CORO_TASK(int) release(...) override;
    CORO_TASK(int) try_cast(...) override;

    // Status monitoring
    uint64_t get_shared_count() const { return shared_count_.load(); }
    uint64_t get_optimistic_count() const { return optimistic_count_.load(); }

private:
    std::shared_ptr<transport> get_directional_transport(destination_zone dest);
    void monitor_transport_status();
    void trigger_self_destruction();
};
```

**Task 5.2**: Implement routing logic via transports
```cpp
CORO_TASK(int) pass_through::send(...) {
    // Determine target transport based on destination_zone
    auto target_transport = get_directional_transport(destination_zone_id);
    if (!target_transport) {
        CO_RETURN rpc::error::ZONE_NOT_FOUND();
    }

    // Check transport status before routing
    if (target_transport->get_status() != transport_status::CONNECTED) {
        CO_RETURN rpc::error::TRANSPORT_ERROR();
    }

    // Get handler from target transport and forward the call
    auto handler = target_transport->get_destination_handler(destination_zone_id);
    if (!handler) {
        CO_RETURN rpc::error::ZONE_NOT_FOUND();
    }

    CO_RETURN CO_AWAIT handler->send(...);
}

std::shared_ptr<transport> pass_through::get_directional_transport(
    destination_zone dest) {

    if (dest == forward_destination_) {
        return forward_transport_;
    } else if (dest == reverse_destination_) {
        return reverse_transport_;
    }
    return nullptr;
}
```

**Task 5.3**: Implement reference counting with auto-deletion
```cpp
CORO_TASK(int) pass_through::add_ref(...) {
    // Update pass_through reference count
    if (options == add_ref_options::normal) {
        shared_count_.fetch_add(1, std::memory_order_acq_rel);
    } else if (options == add_ref_options::optimistic) {
        optimistic_count_.fetch_add(1, std::memory_order_acq_rel);
    }

    // Route to target transport
    auto target = get_directional_transport(destination_zone_id);
    if (!target) {
        CO_RETURN rpc::error::ZONE_NOT_FOUND();
    }

    auto handler = target->get_destination_handler(destination_zone_id);
    CO_RETURN CO_AWAIT handler->add_ref(...);
}

CORO_TASK(int) pass_through::release(...) {
    // Update pass_through reference count
    bool should_delete = false;

    if (options == release_options::normal) {
        uint64_t prev = shared_count_.fetch_sub(1, std::memory_order_acq_rel);
        if (prev == 1 && optimistic_count_.load() == 0) {
            should_delete = true;
        }
    } else if (options == release_options::optimistic) {
        uint64_t prev = optimistic_count_.fetch_sub(1, std::memory_order_acq_rel);
        if (prev == 1 && shared_count_.load() == 0) {
            should_delete = true;
        }
    }

    // Route to target transport
    auto target = get_directional_transport(destination_zone_id);
    if (!target) {
        CO_RETURN rpc::error::ZONE_NOT_FOUND();
    }

    auto handler = target->get_destination_handler(destination_zone_id);
    auto result = CO_AWAIT handler->release(...);

    // Trigger self-destruction if counts are zero
    if (should_delete) {
        trigger_self_destruction();
    }

    CO_RETURN result;
}

void pass_through::trigger_self_destruction() {
    // Stop monitoring
    monitoring_active_.store(false);

    // Remove destinations from transports
    if (forward_transport_) {
        forward_transport_->remove_destination(reverse_destination_);
    }
    if (reverse_transport_) {
        reverse_transport_->remove_destination(forward_destination_);
    }

    // Release transport and service pointers
    forward_transport_.reset();
    reverse_transport_.reset();
    service_.reset();
}
```

**Task 5.4**: Implement transport status monitoring with zone_terminating post
```cpp
CORO_TASK(void) pass_through::monitor_transport_status() {
    while (monitoring_active_.load()) {
        CO_AWAIT std::chrono::milliseconds(100);

        bool forward_connected =
            forward_transport_ &&
            forward_transport_->get_status() == transport_status::CONNECTED;

        bool reverse_connected =
            reverse_transport_ &&
            reverse_transport_->get_status() == transport_status::CONNECTED;

        // If either transport is DISCONNECTED, send zone_terminating to the other
        if (!forward_connected || !reverse_connected) {
            // Step 1: Send zone_terminating post to the still-connected transport
            if (forward_connected && reverse_transport_) {
                // Forward is still connected but reverse is not
                // Send zone_terminating post to forward transport
                auto handler = forward_transport_->get_destination_handler(forward_destination_);
                if (handler) {
                    CO_AWAIT handler->post(
                        VERSION_3, encoding::yas_binary, 0,
                        caller_channel_zone{0}, caller_zone{reverse_destination_},
                        forward_destination_,
                        object{0}, interface_ordinal{0}, method{0},
                        post_options::zone_terminating,
                        0, nullptr, {});
                }
            }

            if (reverse_connected && forward_transport_) {
                // Reverse is still connected but forward is not
                // Send zone_terminating post to reverse transport
                auto handler = reverse_transport_->get_destination_handler(reverse_destination_);
                if (handler) {
                    CO_AWAIT handler->post(
                        VERSION_3, encoding::yas_binary, 0,
                        caller_channel_zone{0}, caller_zone{forward_destination_},
                        reverse_destination_,
                        object{0}, interface_ordinal{0}, method{0},
                        post_options::zone_terminating,
                        0, nullptr, {});
                }
            }

            // Step 2: Trigger immediate self-destruction to prevent asymmetric state
            trigger_self_destruction();
            break;
        }
    }
}

// Note: The transport receiving zone_terminating post will set its own status to DISCONNECTED
// trigger_self_destruction() removes destinations from transports, releases all pointers,
// and causes the pass_through shared_ptr to delete
```

#### Acceptance Criteria

- ✅ pass_through class implements i_marshaller
- ✅ Holds two `shared_ptr<transport>` objects (forward and reverse)
- ✅ Bidirectional routing works via transports (forward and reverse)
- ✅ Reference counting managed by pass_through
- ✅ Auto-deletes when both shared and optimistic counts reach zero
- ✅ Detects transport disconnection (monitors both transports)
- ✅ **Sends `post(zone_terminating)` to other transport** (not direct status setting)
- ✅ Transport receives zone_terminating and sets its own status to DISCONNECTED
- ✅ **Immediately deletes itself after sending zone_terminating post**
- ✅ Removes destinations from both transports during deletion
- ✅ Releases all pointers (transports and service) during deletion
- ✅ Prevents asymmetric states through immediate self-destruction
- ✅ Tests validate zone_terminating post was sent
- ✅ Tests validate pass_through deletion via weak_ptr
- ✅ Tests pass in BOTH sync and async modes

---

### Milestone 6: Both-or-Neither Guarantee (Week 12-13)

**Objective**: Implement operational guarantee preventing asymmetric states

#### BDD Feature: Both-or-neither operational guarantee
```gherkin
Feature: Both-or-neither operational guarantee
  As a pass_through
  I want to ensure both service_proxies are operational or both non-operational
  So that I prevent asymmetric communication failures

  Scenario: Both proxies operational
    Given a pass_through with two service_proxies
    And both transports are operational
    When I check operational status
    Then both_or_neither_operational() returns true
    And messages can flow in both directions

  Scenario: One proxy becomes non-operational
    Given a pass_through with two service_proxies
    And forward proxy transport fails
    When operational status is checked
    Then pass_through detects asymmetry
    And triggers shutdown of reverse proxy
    And both_or_neither_operational() returns true (both non-operational)

  Scenario: Refuse clone on asymmetric state
    Given a pass_through in asymmetric state (one proxy failing)
    When clone_for_zone() is called
    Then it refuses to create new proxy
    And returns nullptr
```

#### TDD Test Specifications

**Test 6.1**: Both operational
```cpp
TEST_CASE("both proxies operational") {
    // GIVEN
    auto forward = create_operational_proxy();
    auto reverse = create_operational_proxy();
    auto pass_through = create_pass_through(forward, reverse);

    // WHEN/THEN
    REQUIRE(pass_through->both_or_neither_operational());
    REQUIRE(forward->is_operational());
    REQUIRE(reverse->is_operational());
}
```

**Test 6.2**: Enforce symmetry on failure
```cpp
CORO_TYPED_TEST(both_or_neither_test, "enforce symmetry on forward failure") {
    // GIVEN
    auto forward = create_operational_proxy();
    auto reverse = create_operational_proxy();
    auto pass_through = create_pass_through(forward, reverse);

    // WHEN - forward proxy fails
    CO_AWAIT forward->get_transport()->shutdown();

    // Give pass_through time to detect and enforce
    CO_AWAIT std::chrono::milliseconds(100);

    // THEN - pass_through brings down reverse to maintain symmetry
    REQUIRE(!forward->is_operational());
    REQUIRE(!reverse->is_operational());
    REQUIRE(pass_through->both_or_neither_operational());
}
```

**Test 6.3**: Refuse clone on non-operational
```cpp
CORO_TYPED_TEST(both_or_neither_test, "refuse clone when not operational") {
    // GIVEN
    auto pass_through = create_pass_through();
    auto proxy = pass_through->get_forward_proxy();

    // WHEN - transport becomes non-operational
    CO_AWAIT proxy->get_transport()->shutdown();
    pass_through->enforce_both_or_neither_guarantee();

    // Attempt clone
    auto cloned = CO_AWAIT proxy->clone_for_zone(zone{10}, zone{1});

    // THEN - refused
    REQUIRE(cloned == nullptr);
}
```

#### Implementation Tasks

**Task 6.1**: Implement operational check
```cpp
bool pass_through::both_or_neither_operational() const {
    bool forward_ok = forward_proxy_ && forward_proxy_->is_operational();
    bool reverse_ok = reverse_proxy_ && reverse_proxy_->is_operational();

    // Both operational OR both non-operational
    return (forward_ok && reverse_ok) || (!forward_ok && !reverse_ok);
}
```

**Task 6.2**: Implement enforcement
```cpp
void pass_through::enforce_both_or_neither_guarantee() {
    bool forward_ok = forward_proxy_ && forward_proxy_->is_operational();
    bool reverse_ok = reverse_proxy_ && reverse_proxy_->is_operational();

    // Asymmetric state detected
    if (forward_ok != reverse_ok) {
        trigger_self_destruction();
    }
}

void pass_through::trigger_self_destruction() {
    // Bring down both proxies
    if (forward_proxy_) {
        forward_proxy_.reset();
    }
    if (reverse_proxy_) {
        reverse_proxy_.reset();
    }

    // Notify service
    if (service_) {
        service_->remove_pass_through(this);
    }

    // Release self-reference
    self_reference_.reset();
}
```

**Task 6.3**: Add periodic monitoring
```cpp
class pass_through {
    std::atomic<bool> monitoring_active_{true};

    CORO_TASK(void) monitor_operational_status() {
        while (monitoring_active_.load()) {
            CO_AWAIT std::chrono::milliseconds(100);
            enforce_both_or_neither_guarantee();
        }
    }
};
```

#### Acceptance Criteria

- ✅ both_or_neither_operational() check implemented
- ✅ Asymmetry detection works
- ✅ Automatic enforcement brings down both proxies
- ✅ clone_for_zone() checks operational status
- ✅ Tests pass in BOTH sync and async modes

---

### Milestone 7: Zone Termination Broadcast (Week 14-15)

**Objective**: Implement zone termination detection and cascading cleanup

#### BDD Feature: Zone termination broadcast
```gherkin
Feature: Zone termination broadcast
  As a transport
  I want to detect zone termination and broadcast notifications
  So that all connected zones can cleanup properly

  Scenario: Graceful zone shutdown
    Given zones A, B, C connected
    And zone B initiates graceful shutdown
    When zone B sends zone_terminating to A and C
    Then zones A and C receive notification
    And all proxies to zone B are marked non-operational
    And cleanup is triggered

  Scenario: Forced zone failure
    Given zones A, B, C connected
    And zone B crashes (transport detects failure)
    When transport detects connection failure
    Then transport broadcasts zone_terminating to service and pass_through
    And zones A and C are notified
    And cascading cleanup occurs

  Scenario: Cascading termination
    Given zone topology: Root → A → B → C
    And zone A terminates
    When zone A broadcasts termination
    Then zones B and C become unreachable via A
    And Root receives notifications for A, B, C
    And all proxies in subtree are cleaned up
```

#### TDD Test Specifications

**Test 7.1**: Graceful shutdown broadcast
```cpp
CORO_TYPED_TEST(zone_termination_test, "graceful shutdown broadcast") {
    // GIVEN
    auto service_a = create_service("zone_a", zone{1});
    auto service_b = create_service("zone_b", zone{2});
    auto service_c = create_service("zone_c", zone{3});

    connect_zones(service_a, service_b);
    connect_zones(service_b, service_c);

    // WHEN - zone B graceful shutdown
    CO_AWAIT service_b->shutdown_and_broadcast();

    // THEN - A and C notified
    REQUIRE_EVENTUALLY(service_a->is_zone_terminated(zone{2}));
    REQUIRE_EVENTUALLY(service_c->is_zone_terminated(zone{2}));
}
```

**Test 7.2**: Forced failure detection
```cpp
CORO_TYPED_TEST(zone_termination_test, "forced failure detection") {
    // GIVEN
    auto service_a = create_service("zone_a");
    auto service_b = create_service("zone_b");
    auto proxy_a_to_b = connect_zones_via_spsc(service_a, service_b);

    // WHEN - simulate connection failure (kill zone B process)
    simulate_connection_failure(proxy_a_to_b->get_transport());

    // THEN - transport detects and broadcasts
    REQUIRE_EVENTUALLY(service_a->is_zone_terminated(zone{2}));
}
```

**Test 7.3**: Cascading termination
```cpp
CORO_TYPED_TEST(zone_termination_test, "cascading termination") {
    // GIVEN topology: Root → A → B → C
    auto root = create_service("root", zone{1});
    auto a = create_service("a", zone{2});
    auto b = create_service("b", zone{3});
    auto c = create_service("c", zone{4});

    connect_zones(root, a);
    connect_zones(a, b);
    connect_zones(b, c);

    // WHEN - zone A terminates
    CO_AWAIT a->shutdown_and_broadcast();

    // THEN - root notified about A, B, C all unreachable
    REQUIRE_EVENTUALLY(root->is_zone_terminated(zone{2}));
    REQUIRE_EVENTUALLY(root->is_zone_terminated(zone{3}));
    REQUIRE_EVENTUALLY(root->is_zone_terminated(zone{4}));
}
```

#### Implementation Tasks

**Task 7.1**: Implement graceful broadcast
```cpp
CORO_TASK(void) service::shutdown_and_broadcast() {
    // Broadcast zone_terminating to all connected zones
    for (auto& [route, proxy_weak] : other_zones) {
        if (auto proxy = proxy_weak.lock()) {
            CO_AWAIT proxy->post(
                VERSION_3, encoding::yas_binary, tag++,
                caller_channel_zone{zone_id_}, caller_zone{zone_id_},
                destination_zone{route.destination},
                object{0}, interface_ordinal{0}, method{0},
                post_options::zone_terminating,
                0, nullptr, {});
        }
    }

    // Proceed with local shutdown
    CO_AWAIT shutdown();
}
```

**Task 7.2**: Implement transport failure detection
```cpp
CORO_TASK(void) channel_manager::monitor_connection() {
    while (is_operational()) {
        CO_AWAIT std::chrono::seconds(1);

        if (detect_connection_failure()) {
            // Broadcast to handler
            if (auto handler = handler_.lock()) {
                CO_AWAIT handler->post(
                    ..., post_options::zone_terminating, ...);
            }

            // Shutdown
            CO_AWAIT shutdown();
            break;
        }
    }
}
```

**Task 7.3**: Implement cascading cleanup
```cpp
CORO_TASK(void) service::handle_zone_termination(zone terminated_zone) {
    // Mark zone as terminated
    terminated_zones_.insert(terminated_zone);

    // Find all zones reachable ONLY via terminated zone
    auto unreachable = find_zones_via_only(terminated_zone);

    // Broadcast termination for unreachable zones
    for (auto unreachable_zone : unreachable) {
        CO_AWAIT broadcast_zone_termination(unreachable_zone);
    }

    // Cleanup proxies
    cleanup_proxies_to_zones(unreachable);
}
```

#### Acceptance Criteria

- ✅ Graceful shutdown broadcasts to all connected zones
- ✅ Transport failure detection works
- ✅ Cascading termination propagates correctly
- ✅ Service and pass_through receive notifications
- ✅ Tests pass in BOTH sync and async modes
- ✅ Telemetry tracks termination broadcasts

---

### Milestone 8: Y-Topology Bidirectional Proxies (Week 16-17)

**Objective**: Eliminate on-demand proxy creation, create bidirectional pairs upfront

#### BDD Feature: Bidirectional proxy pairs
```gherkin
Feature: Bidirectional proxy pairs
  As a service
  I want to create proxy pairs together when sharing objects
  So that I eliminate Y-topology race conditions

  Scenario: Create bidirectional pair on first connection
    Given zone Root connects to zone A
    When the connection is established
    Then both forward (Root→A) and reverse (A→Root) proxies are created
    And both proxies are registered immediately
    And no on-demand creation is needed

  Scenario: Y-topology object return
    Given topology: Root → A → B → C (7 levels deep)
    And zone C creates zone D autonomously
    And Root has no direct connection to zone D
    When zone C returns object from zone D to Root
    Then add_ref uses known_direction_zone to establish route
    And bidirectional proxy pair (Root↔D via C) is created
    And no race condition occurs

  Scenario: No reactive proxy creation in send()
    Given zones A and B connected
    When zone A sends message to zone B
    Then send() does NOT create reverse proxy
    And send() uses pre-created bidirectional pair
    And no "tmp is null" bug occurs
```

#### TDD Test Specifications

**Test 8.1**: Bidirectional creation on connect
```cpp
CORO_TYPED_TEST(bidirectional_test, "create pair on connect") {
    // GIVEN
    auto root = create_service("root", zone{1});
    auto a = create_service("a", zone{2});

    // WHEN - connect zones
    auto proxy_root_to_a = CO_AWAIT root->connect_to_zone<local_service_proxy>(
        "zone_a", zone{2}, ...);

    // THEN - both directions exist
    REQUIRE(proxy_root_to_a != nullptr);

    auto proxy_a_to_root = a->find_proxy(dest=zone{1}, caller=zone{2});
    REQUIRE(proxy_a_to_root != nullptr);
}
```

**Test 8.2**: Y-topology with known_direction_zone
```cpp
CORO_TYPED_TEST(y_topology_test, "object return via known_direction") {
    // GIVEN topology: Root → A → C (zone C creates zone D)
    auto root = create_service("root", zone{1});
    auto a = create_service("a", zone{2});
    auto c = create_service("c", zone{3});
    auto d = create_service("d", zone{7});

    connect_zones(root, a);
    connect_zones(a, c);
    connect_zones(c, d); // Root doesn't know about D

    // WHEN - C returns object from D to Root
    // add_ref from Root to zone D with known_direction=zone C
    uint64_t ref_count = 0;
    auto error = CO_AWAIT root->add_ref_to_remote_object(
        object{100}, zone{7}, known_direction_zone{3}, ref_count);

    // THEN - bidirectional pair created via C
    REQUIRE(error == rpc::error::OK());
    REQUIRE(ref_count == 1);

    // Verify reverse path exists
    auto reverse_proxy = root->find_proxy(dest=zone{1}, caller=zone{7});
    REQUIRE(reverse_proxy != nullptr);
}
```

**Test 8.3**: No reactive creation in send()
```cpp
CORO_TYPED_TEST(no_reactive_test, "send does not create reverse proxy") {
    // GIVEN
    auto a = create_service("a", zone{1});
    auto b = create_service("b", zone{2});
    auto proxy_a_to_b = connect_zones(a, b);

    // WHEN - send message
    auto before_count = a->get_proxy_count();

    std::vector<char> out_buf;
    CO_AWAIT proxy_a_to_b->send(...);

    auto after_count = a->get_proxy_count();

    // THEN - no new proxies created reactively
    REQUIRE(before_count == after_count);
}
```

#### Implementation Tasks

**Task 8.1**: Create bidirectional pair on connect
```cpp
template<typename ServiceProxyType>
CORO_TASK(rpc::shared_ptr<i_interface>) service::connect_to_zone(
    const char* zone_name,
    zone dest_zone,
    ...) {

    // Create forward proxy (this zone → dest zone)
    auto forward_proxy = std::make_shared<ServiceProxyType>(
        zone_id_, dest_zone, zone_id_, ...);

    // CRITICAL: Create reverse proxy (dest zone → this zone)
    auto reverse_proxy = std::make_shared<ServiceProxyType>(
        zone_id_, zone_id_, dest_zone, ...);

    // Register both immediately
    inner_add_zone_proxy(forward_proxy);
    inner_add_zone_proxy(reverse_proxy);

    // Create pass_through to manage both
    auto pass_through = std::make_shared<pass_through>(
        forward_proxy, reverse_proxy, shared_from_this(),
        dest_zone, zone_id_);

    pass_throughs_[{dest_zone, zone_id_}] = pass_through;

    CO_RETURN create_interface_proxy<i_interface>(forward_proxy);
}
```

**Task 8.2**: Implement add_ref with known_direction_zone
```cpp
CORO_TASK(int) service::add_ref(
    ...,
    destination_zone destination_zone_id,
    known_direction_zone known_direction_zone_id,
    ...) {

    // Try direct route first
    auto proxy = find_proxy(destination_zone_id, zone_id_);
    if (proxy) {
        CO_RETURN CO_AWAIT proxy->add_ref(...);
    }

    // Use known_direction_zone hint for Y-topology
    if (known_direction_zone_id != zone{0}) {
        auto hint_proxy = find_proxy(known_direction_zone_id, zone_id_);
        if (hint_proxy) {
            // Create bidirectional pair via known direction
            auto new_proxy = CO_AWAIT hint_proxy->clone_for_zone(
                destination_zone_id, zone_id_);

            if (new_proxy) {
                // Create reverse proxy too
                auto reverse = CO_AWAIT hint_proxy->clone_for_zone(
                    zone_id_, destination_zone_id);

                // Register both
                inner_add_zone_proxy(new_proxy);
                inner_add_zone_proxy(reverse);

                // Create pass_through
                auto pt = std::make_shared<pass_through>(
                    new_proxy, reverse, shared_from_this(),
                    destination_zone_id, zone_id_);
                pass_throughs_[{destination_zone_id, zone_id_}] = pt;

                CO_RETURN CO_AWAIT new_proxy->add_ref(...);
            }
        }
    }

    CO_RETURN rpc::error::ZONE_NOT_FOUND();
}
```

**Task 8.3**: Remove reactive creation from send()
```cpp
CORO_TASK(int) service::send(...) {
    // Find existing proxy - NO reactive creation
    auto proxy = find_proxy(destination_zone_id, caller_zone_id);
    if (!proxy) {
        CO_RETURN rpc::error::ZONE_NOT_FOUND();
    }

    // Use existing proxy
    CO_RETURN CO_AWAIT proxy->send(...);
}
```

#### Acceptance Criteria

- ✅ Bidirectional pairs created on connect
- ✅ Y-topology works with known_direction_zone
- ✅ No reactive proxy creation in send()
- ✅ "tmp is null" bug eliminated
- ✅ Y-topology tests pass (all 3 variants)
- ✅ Tests pass in BOTH sync and async modes

---

### Milestone 9: SPSC Channel Manager Integration (Week 18-19)

**Objective**: Update SPSC channel_manager to use new architecture

#### BDD Feature: SPSC with new architecture
```gherkin
Feature: SPSC channel manager with new architecture
  As an SPSC transport
  I want to integrate with handler registration and operational state
  So that I support pass_through and both-or-neither guarantee

  Scenario: Register pass_through as handler
    Given an SPSC channel_manager
    When I register a pass_through as handler
    Then incoming messages are routed to pass_through
    And pass_through routes to appropriate service_proxy

  Scenario: Operational state during shutdown
    Given an SPSC channel_manager with active pump tasks
    When peer sends zone_terminating
    Then is_operational() returns false
    And clone_for_zone() refuses to create proxies
    And pass_through is notified via post()

  Scenario: Phase out service_proxy_ref_count
    Given an SPSC channel_manager
    And pass_through manages service_proxy lifetime
    When service_proxies are destroyed
    Then pass_through triggers cleanup
    And channel_manager is notified
    And eventual removal of ref count mechanism
```

#### TDD Test Specifications

**Test 9.1**: Handler registration
```cpp
CORO_TYPED_TEST(spsc_integration_test, "register pass_through as handler") {
    // GIVEN
    auto channel_mgr = create_spsc_channel_manager();
    auto pass_through = create_pass_through();

    // WHEN
    channel_mgr->set_receive_handler(pass_through);

    // Send message
    send_via_spsc_queue(channel_mgr, create_send_message());

    // THEN - pass_through receives and routes
    REQUIRE_EVENTUALLY(pass_through->has_routed_message());
}
```

**Test 9.2**: Operational state check
```cpp
CORO_TYPED_TEST(spsc_integration_test, "operational state with pass_through") {
    // GIVEN
    auto channel_mgr = create_spsc_channel_manager();
    auto pass_through = create_pass_through();
    channel_mgr->set_receive_handler(pass_through);

    // WHEN - peer terminates
    CO_AWAIT simulate_peer_zone_termination(channel_mgr);

    // THEN
    REQUIRE(!channel_mgr->is_operational());

    // Pass-through notified via post
    REQUIRE(pass_through->received_zone_terminating());
}
```

**Test 9.3**: Lifecycle without ref count
```cpp
CORO_TYPED_TEST(spsc_integration_test, "lifecycle managed by pass_through") {
    // GIVEN
    auto channel_mgr = create_spsc_channel_manager();
    auto forward = create_service_proxy(channel_mgr);
    auto reverse = create_service_proxy(channel_mgr);
    auto pass_through = create_pass_through(forward, reverse);

    channel_mgr->set_receive_handler(pass_through);

    // WHEN - pass_through releases both proxies
    pass_through.reset();

    // THEN - channel_manager detects and shuts down
    REQUIRE_EVENTUALLY(!channel_mgr->is_operational());
}
```

#### Implementation Tasks

**Task 9.1**: Add handler to channel_manager
```cpp
class channel_manager {
    std::weak_ptr<i_marshaller> handler_;

    void set_receive_handler(std::weak_ptr<i_marshaller> handler) override {
        handler_ = handler;
    }

    void incoming_message_handler(envelope_prefix prefix, envelope_payload payload) {
        if (auto handler = handler_.lock()) {
            // Unpack and route based on message type
            switch (prefix.message_type) {
                case message_type::send:
                    handler->send(...);
                    break;
                case message_type::post:
                    handler->post(...);
                    break;
                // ... other types
            }
        }
    }
};
```

**Task 9.2**: Enhance shutdown for zone termination
```cpp
CORO_TASK(void) channel_manager::shutdown() {
    // If zone_terminating, notify handler
    if (zone_terminating_received_) {
        if (auto handler = handler_.lock()) {
            CO_AWAIT handler->post(
                VERSION_3, encoding::yas_binary, 0,
                caller_channel_zone{0}, caller_zone{0}, destination_zone{0},
                object{0}, interface_ordinal{0}, method{0},
                post_options::zone_terminating,
                0, nullptr, {});
        }
        // Skip waiting for peer
        CO_RETURN;
    }

    // Normal graceful shutdown
    // ... existing logic
}
```

**Task 9.3**: Document ref count phase-out plan
```cpp
// FUTURE: Remove service_proxy_ref_count_ when pass_through fully integrated
// CURRENT: Keep for backward compatibility
// std::atomic<int> service_proxy_ref_count_{0}; // DEPRECATED - to be removed
```

#### Acceptance Criteria

- ✅ SPSC channel_manager supports handler registration
- ✅ is_operational() works correctly
- ✅ Zone termination notification to handler works
- ✅ Tests pass in async mode (SPSC is async-only)
- ✅ Documentation updated for ref count phase-out

---

### Milestone 10: Full Integration and Validation (Week 20)

**Objective**: Complete integration, run all tests, validate bi-modal operation

#### BDD Feature: Full system integration
```gherkin
Feature: Full system integration
  As an RPC++ developer
  I want all components working together
  So that I have a robust, race-free RPC system

  Scenario: End-to-end message flow
    Given zones A, B, C connected with pass_throughs
    And back-channel data for distributed tracing
    When zone A sends message to zone C via B
    Then message is routed through pass_through in B
    And back-channel data is preserved
    And response flows back correctly

  Scenario: Cascading failure recovery
    Given zone topology with multiple levels
    And zone at intermediate level fails
    When transport detects failure
    Then zone_terminating broadcasts to all
    And cascading cleanup occurs
    And system remains stable

  Scenario: Bi-modal operation
    Given the same test suite
    When run in sync mode (BUILD_COROUTINE=OFF)
    Then all tests pass
    When run in async mode (BUILD_COROUTINE=ON)
    Then all tests pass
```

#### TDD Test Specifications

**Test 10.1**: End-to-end integration
```cpp
CORO_TYPED_TEST(full_integration_test, "end to end message flow") {
    // GIVEN
    auto a = create_service("a", zone{1});
    auto b = create_service("b", zone{2});
    auto c = create_service("c", zone{3});

    auto pt_a_c_via_b = create_pass_through_topology(a, b, c);

    rpc::back_channel_entry trace;
    trace.type_id = TRACE_FINGERPRINT;
    trace.payload = serialize_trace("req-123");

    // WHEN - A sends to C with back-channel
    std::vector<char> out_buf;
    std::vector<rpc::back_channel_entry> in_bc = {trace};
    std::vector<rpc::back_channel_entry> out_bc;

    auto error = CO_AWAIT a->send_to_zone(
        zone{3}, object{100}, interface_ordinal{1}, method{5},
        in_buf, out_buf, in_bc, out_bc);

    // THEN
    REQUIRE(error == rpc::error::OK());
    REQUIRE(!out_bc.empty()); // Response back-channel
    REQUIRE(c->received_trace_id("req-123"));
}
```

**Test 10.2**: Cascading failure
```cpp
CORO_TYPED_TEST(full_integration_test, "cascading failure recovery") {
    // GIVEN topology: Root → A → B → C → D
    auto root = create_service("root", zone{1});
    auto a = create_service("a", zone{2});
    auto b = create_service("b", zone{3});
    auto c = create_service("c", zone{4});
    auto d = create_service("d", zone{5});

    create_linear_topology({root, a, b, c, d});

    // WHEN - zone B fails
    CO_AWAIT simulate_hard_failure(b);

    // THEN - cascading cleanup
    REQUIRE_EVENTUALLY(root->is_zone_terminated(zone{3})); // B
    REQUIRE_EVENTUALLY(root->is_zone_terminated(zone{4})); // C (via B)
    REQUIRE_EVENTUALLY(root->is_zone_terminated(zone{5})); // D (via B)

    // Root and A still operational
    REQUIRE(root->is_operational());
    REQUIRE(a->is_operational());
}
```

**Test 10.3**: Bi-modal test suite
```cpp
// Framework runs ALL tests in both modes
int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);

#ifdef BUILD_COROUTINE
    std::cout << "Running in ASYNC mode" << std::endl;
#else
    std::cout << "Running in SYNC mode" << std::endl;
#endif

    return RUN_ALL_TESTS();
}
```

#### Implementation Tasks

**Task 10.1**: Run full test suite
```bash
# Sync mode
cmake --preset Debug -DBUILD_COROUTINE=OFF
cmake --build build --target all_tests
./build/all_tests

# Async mode
cmake --preset Debug -DBUILD_COROUTINE=ON
cmake --build build --target all_tests
./build/all_tests
```

**Task 10.2**: Performance benchmarking
```cpp
BENCHMARK_TEST(integration_benchmark, "message throughput") {
    auto a = create_service("a");
    auto b = create_service("b");
    auto proxy = connect_zones(a, b);

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 10000; ++i) {
        proxy->send(...);
    }

    auto duration = std::chrono::high_resolution_clock::now() - start;
    auto throughput = 10000.0 / duration.count();

    std::cout << "Throughput: " << throughput << " msgs/sec" << std::endl;
}
```

**Task 10.3**: Stress testing
```cpp
TEST(stress_test, "concurrent operations") {
    auto a = create_service("a");
    auto b = create_service("b");
    auto proxy = connect_zones(a, b);

    std::vector<std::thread> threads;
    for (int t = 0; t < 10; ++t) {
        threads.emplace_back([&]() {
            for (int i = 0; i < 1000; ++i) {
                proxy->send(...);
                proxy->add_ref(...);
                proxy->release(...);
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // Verify no crashes, leaks, or corruption
    REQUIRE(a->is_consistent());
    REQUIRE(b->is_consistent());
}
```

#### Acceptance Criteria

- ✅ All integration tests pass
- ✅ All tests pass in BOTH sync and async modes
- ✅ No memory leaks detected (valgrind clean)
- ✅ No race conditions detected (thread sanitizer clean)
- ✅ Performance benchmarks meet targets
- ✅ Stress tests pass (10K+ operations, 10+ threads)
- ✅ Documentation complete
- ✅ Migration guide written

---

## BDD/TDD Specification Framework

### BDD Structure

```gherkin
Feature: <High-level capability>
  As a <role>
  I want to <action>
  So that <benefit>

  Scenario: <Specific behavior>
    Given <precondition>
    And <additional precondition>
    When <action>
    Then <expected outcome>
    And <additional outcome>
```

### TDD Structure

```cpp
TEST_CASE("<test name>") {
    // GIVEN - setup
    auto service = create_test_service();

    // WHEN - action
    auto result = service->perform_action();

    // THEN - assertions
    REQUIRE(result == expected_value);
    REQUIRE(service->state() == expected_state);
}
```

### Bi-Modal Test Template

```cpp
#ifdef BUILD_COROUTINE
CORO_TYPED_TEST(test_suite, "test_name_async") {
    // GIVEN
    auto service = create_service();

    // WHEN
    auto result = CO_AWAIT service->async_operation();

    // THEN
    REQUIRE(result == expected);
}
#else
TEST(test_suite, "test_name_sync") {
    // GIVEN
    auto service = create_service();

    // WHEN
    auto result = service->sync_operation();

    // THEN
    REQUIRE(result == expected);
}
#endif
```

---

## Bi-Modal Testing Strategy

### Principle: Write Once, Test Twice

Every feature must pass tests in BOTH modes:
- **Sync Mode**: `BUILD_COROUTINE=OFF` - blocking calls, immediate cleanup
- **Async Mode**: `BUILD_COROUTINE=ON` - coroutine calls, eventual consistency

### Test Categorization

**Category 1: Bi-Modal Tests** (must run in both modes)
- Core RPC functionality (send, add_ref, release)
- Reference counting and lifecycle
- Object proxy management
- Local transport (supports both modes)
- SGX transport (supports both modes)

**Category 2: Async-Only Tests**
- SPSC transport (requires coroutines)
- TCP transport (requires async I/O)
- Fire-and-forget post() (sync mode is blocking)

### Test Execution Strategy

```bash
# Full bi-modal test run
./scripts/run_bimodal_tests.sh

# Which runs:
# 1. Build sync mode
cmake --preset Debug -DBUILD_COROUTINE=OFF
cmake --build build --target all_tests
./build/all_tests --gtest_filter="*" --gtest_output=xml:sync_results.xml

# 2. Build async mode
cmake --preset Debug -DBUILD_COROUTINE=ON
cmake --build build --target all_tests
./build/all_tests --gtest_filter="*" --gtest_output=xml:async_results.xml

# 3. Compare results
./scripts/compare_test_results.py sync_results.xml async_results.xml
```

### Bi-Modal Test Helpers

```cpp
// Helper to run test in both modes
#define BI_MODAL_TEST(suite, name, test_body) \
  CORO_TYPED_TEST(suite, name) { test_body } \
  TEST(suite, name ## _sync) { test_body }

// Usage
BI_MODAL_TEST(service_proxy_test, "send operation", {
    auto proxy = create_proxy();
    auto result = AWAIT_IF_ASYNC(proxy->send(...));
    REQUIRE(result == rpc::error::OK());
});
```

---

## Success Criteria and Validation

### Per-Milestone Success Criteria

Each milestone must meet:
- ✅ All BDD scenarios pass
- ✅ All TDD tests pass
- ✅ Bi-modal tests pass (where applicable)
- ✅ No regressions in existing tests
- ✅ Code review completed
- ✅ Documentation updated
- ✅ Telemetry/logging added

### Overall Project Success Criteria

1. **Functional Requirements**
   - ✅ Back-channel support implemented
   - ✅ Fire-and-forget post() working
   - ✅ Pass-through routing operational
   - ✅ Both-or-neither guarantee enforced
   - ✅ Zone termination broadcast working
   - ✅ Y-topology race condition eliminated
   - ✅ Bidirectional proxy pairs created upfront

2. **Quality Requirements**
   - ✅ All tests pass in sync mode
   - ✅ All tests pass in async mode
   - ✅ No memory leaks (valgrind clean)
   - ✅ No race conditions (thread sanitizer clean)
   - ✅ No deadlocks detected
   - ✅ Code coverage > 90%

3. **Performance Requirements**
   - ✅ Message latency < 100μs (local)
   - ✅ Throughput > 100K msgs/sec (local)
   - ✅ Memory overhead < 10% increase
   - ✅ CPU overhead < 5% increase

4. **Architecture Requirements (NEW - Elegant Transport Model)**
   - ✅ Transport base class (not interface) implemented
   - ✅ Destination-based routing via `unordered_map<destination_zone, weak_ptr<i_marshaller>>`
   - ✅ Transport status enum (CONNECTING, CONNECTED, RECONNECTING, DISCONNECTED)
   - ✅ Service_proxy routes ALL traffic through transport
   - ✅ Service_proxy refuses traffic when transport is DISCONNECTED
   - ✅ Pass-through holds two `shared_ptr<transport>` objects
   - ✅ Pass-through auto-deletes when counts reach zero
   - ✅ Pass-through monitors both transports for both-or-neither guarantee
   - ✅ Service derives from i_marshaller (no i_service)
   - ✅ Bi-modal support preserved

5. **Documentation Requirements**
   - ✅ Architecture guide updated
   - ✅ API documentation complete
   - ✅ Migration guide written
   - ✅ Examples updated
   - ✅ Troubleshooting guide created

---

## Risk Mitigation

### Technical Risks

**Risk 1: Breaking Changes**
- **Mitigation**: Maintain backward compatibility layer
- **Validation**: All existing tests must pass

**Risk 2: Performance Regression**
- **Mitigation**: Continuous benchmarking
- **Validation**: Performance tests in CI

**Risk 3: Race Conditions**
- **Mitigation**: Thread sanitizer in all tests
- **Validation**: Stress tests with 10+ threads

### Schedule Risks

**Risk 1: Scope Creep**
- **Mitigation**: Strict milestone acceptance criteria
- **Validation**: Weekly progress reviews

**Risk 2: Integration Delays**
- **Mitigation**: Continuous integration testing
- **Validation**: Daily build and test runs

---

## Conclusion

This master plan provides a concrete, testable roadmap for implementing the **elegant transport-centric architecture** for RPC++. Each milestone is:

- **Well-Defined**: Clear objectives and acceptance criteria
- **Testable**: BDD/TDD specifications with concrete tests
- **Bi-Modal**: Tests run in both sync and async modes
- **Incremental**: Each milestone builds on previous ones
- **Validated**: Success criteria ensure quality

### Key Architectural Benefits

The new transport base class architecture provides:

1. **Simplicity** - No `i_transport` interface, just a concrete base class
2. **Elegance** - Transport owns destination routing, status management in one place
3. **Flexibility** - All derived transports (SPSC, TCP, Local, SGX) inherit common functionality
4. **Robustness** - Transport status enum enforces operational guarantees
5. **Automatic Cleanup** - Pass-through auto-deletes when counts reach zero OR when transport disconnects
6. **Separation of Concerns** - Pass-through sends `post(zone_terminating)`, transport sets its own status
7. **Symmetry Enforcement** - Pass-through monitors both transports, sends zone_terminating to the other, then **immediately deletes itself**
8. **No Asymmetric States** - Impossible to have one transport operational while the other is disconnected

### Next Steps

**Implementation should proceed milestone-by-milestone, with each milestone fully tested and validated before moving to the next.**

The transport base class foundation (Milestone 3) is critical and must be solidly implemented before proceeding to pass-through (Milestone 5).

---

**End of Master Implementation Plan v2**

**Last Updated**: 2025-10-28 (with implementation status tracking and architectural improvements documented)
