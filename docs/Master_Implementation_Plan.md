<!--
Copyright (c) 2025 Edward Boggis-Rolfe
All rights reserved.
-->

# RPC++ Service Proxy and Transport Refactoring - Master Implementation Plan

**Version**: 1.0
**Date**: 2025-01-19
**Status**: Master Plan - Ready for Implementation

**Source Documents**:
- Problem_Statement_Critique_QA.md (15 Q&A requirements)
- Service_Proxy_Transport_Problem_Statement.md
- RPC++_User_Guide.md
- SPSC_Channel_Lifecycle.md
- qwen_implementation_proposal.md
- Implementation_Plan_Critique.md

---

## Table of Contents

1. [Executive Summary](#executive-summary)
2. [Critical Architectural Principles](#critical-architectural-principles)
3. [Milestone-Based Implementation](#milestone-based-implementation)
4. [BDD/TDD Specification Framework](#bddtdd-specification-framework)
5. [Bi-Modal Testing Strategy](#bi-modal-testing-strategy)
6. [Success Criteria and Validation](#success-criteria-and-validation)

---

## Executive Summary

This master plan distills all requirements, critiques, and proposals into a concrete, milestone-based implementation roadmap. Each milestone includes:

- **BDD Feature Specifications**: Behavior-driven scenarios describing what the feature should do
- **TDD Test Specifications**: Test-driven unit tests defining implementation contracts
- **Bi-Modal Test Requirements**: Tests that pass in BOTH sync and async modes
- **Acceptance Criteria**: Clear definition of "done"
- **Implementation Guidance**: Concrete steps with code examples

**Timeline**: 20 weeks (5 months) with 10 major milestones
**Effort**: ~1 developer full-time or 2 developers part-time

---

## Critical Architectural Principles

### From Q&A and Critique (MUST FOLLOW)

1. **✅ Service Derives from i_marshaller**
   - NO `i_service` interface exists
   - Service class directly implements `i_marshaller`
   - Service manages local stubs and routes to service_proxies

2. **✅ Transport is Hidden Implementation Detail**
   - NOT a public interface that services interact with
   - Owned internally by specific service_proxy implementations
   - Multiple service_proxies can share ONE transport (transport sharing pattern)

3. **✅ Service Sink is Transport-Internal**
   - NOT a core RPC++ class
   - If exists, owned by service_proxy or transport
   - Service does NOT know about sinks

4. **✅ Pass-Through Manages Reference Counting**
   - Pass-through controls lifecycle of both service_proxies it routes between
   - Maintains single reference count to service
   - service_proxy may not need lifetime_lock_ mechanism

5. **✅ Both-or-Neither Operational Guarantee**
   - Pass-through guarantees BOTH or NEITHER service_proxies operational
   - No asymmetric states allowed
   - `clone_for_zone()` must refuse if transport NOT operational

6. **✅ Back-Channel Format**
   - Use `vector<back_channel_entry>` structure
   - Each entry: `uint64_t type_id` + `vector<uint8_t> payload`
   - Type IDs from IDL-generated fingerprints

7. **✅ Bi-Modal Support**
   - Local/SGX/DLL support BOTH sync and async
   - SPSC/TCP are async-only (require coroutines)
   - All solutions must work in both modes

8. **✅ Zone Termination Broadcast**
   - Transport detects failure and broadcasts `zone_terminating`
   - Notification sent to service AND pass-through
   - Cascading cleanup through topology

---

## Milestone-Based Implementation

### Milestone 1: i_marshaller Back-Channel Support (Week 1-2)

**Objective**: Add back-channel support to all i_marshaller methods

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
        const std::vector<back_channel_entry>& in_back_channel,   // NEW
        std::vector<back_channel_entry>& out_back_channel          // NEW
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
        const std::vector<back_channel_entry>& in_back_channel   // NEW
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
        const std::vector<back_channel_entry>& in_back_channel,   // NEW
        std::vector<back_channel_entry>& out_back_channel          // NEW
    ) = 0;

    virtual CORO_TASK(int) release(
        uint64_t protocol_version,
        destination_zone destination_zone_id,
        object object_id,
        caller_zone caller_zone_id,
        release_options options,
        uint64_t& reference_count,
        const std::vector<back_channel_entry>& in_back_channel,   // NEW
        std::vector<back_channel_entry>& out_back_channel          // NEW
    ) = 0;

    virtual CORO_TASK(int) try_cast(
        uint64_t protocol_version,
        destination_zone destination_zone_id,
        object object_id,
        interface_ordinal interface_id,
        const std::vector<back_channel_entry>& in_back_channel,   // NEW
        std::vector<back_channel_entry>& out_back_channel          // NEW
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

### Milestone 2: post() Fire-and-Forget Messaging (Week 3-4)

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

**Task 2.1**: Define `post_options` enum
```cpp
enum class post_options : uint8_t {
    normal = 0,
    zone_terminating = 1,
    release_optimistic = 2
};
```

**Task 2.2**: Implement service::handle_post()
```cpp
CORO_TASK(void) service::handle_post(
    destination_zone destination_zone_id,
    object object_id,
    interface_ordinal interface_id,
    method method_id,
    post_options options,
    const std::vector<char>& in_buf,
    const std::vector<back_channel_entry>& in_back_channel) {

    if (options == post_options::zone_terminating) {
        // Handle zone termination
        CO_AWAIT handle_zone_termination(destination_zone_id);
        CO_RETURN;
    }

    if (options == post_options::release_optimistic) {
        // Handle optimistic cleanup
        CO_AWAIT handle_optimistic_release(object_id);
        CO_RETURN;
    }

    // Normal post - dispatch to stub
    CO_AWAIT dispatch_post_to_stub(object_id, interface_id, method_id, in_buf);
}
```

**Task 2.3**: Update channel managers
- SPSC: Add post message type to envelope
- TCP: Implement one-way message transmission
- Local: Direct post without serialization

**Task 2.4**: Implement zone termination broadcast
```cpp
CORO_TASK(void) service::broadcast_zone_termination(zone terminating_zone_id) {
    // Broadcast to all zones that have references to terminating zone
    for (auto& [route, proxy_weak] : other_zones) {
        if (auto proxy = proxy_weak.lock()) {
            if (involves_zone(route, terminating_zone_id)) {
                CO_AWAIT proxy->post(
                    ..., post_options::zone_terminating, ...);
            }
        }
    }
}
```

#### Acceptance Criteria

- ✅ post() method implemented for all i_marshaller implementations
- ✅ post_options enum defined with required options
- ✅ post() completes without waiting for response
- ✅ zone_terminating notification works
- ✅ Optimistic cleanup via post() works
- ✅ Tests pass in BOTH sync and async modes
- ✅ Telemetry tracks post messages

---

### Milestone 3: Transport Handler Registration (Week 5-6)

**Objective**: Implement polymorphic handler registration for transports

#### BDD Feature: Transport handler polymorphism
```gherkin
Feature: Transport handler polymorphism
  As a transport implementation
  I want to register an i_marshaller handler without knowing its type
  So that I can route messages to service or pass_through

  Scenario: Register service as handler
    Given an SPSC channel_manager transport
    When I register a service as the i_marshaller handler
    Then incoming messages are routed to service
    And service dispatches to local stubs

  Scenario: Register pass_through as handler
    Given an SPSC channel_manager transport
    When I register a pass_through as the i_marshaller handler
    Then incoming messages are routed to pass_through
    And pass_through routes to appropriate service_proxy

  Scenario: Multiple service_proxies share one transport
    Given a TCP transport connected to remote zone
    And service_proxy A with destination=remote, caller=zone1
    And service_proxy B with destination=remote, caller=zone2
    When both register with the same transport
    Then transport disambiguates by (destination, caller) tuple
    And routes messages to correct handler
```

#### TDD Test Specifications

**Test 3.1**: Handler registration
```cpp
TEST_CASE("channel_manager handler registration") {
    // GIVEN
    auto service = std::make_shared<rpc::service>("test_zone", zone{1});
    auto channel_manager = create_spsc_channel_manager();

    // WHEN
    channel_manager->set_receive_handler(service);

    // THEN
    auto handler = channel_manager->get_handler();
    REQUIRE(handler.lock() == service);
}
```

**Test 3.2**: Message routing to service
```cpp
CORO_TYPED_TEST(handler_routing_test, "route to service") {
    // GIVEN
    auto service = create_service("test_zone");
    auto channel_manager = create_channel_manager();
    channel_manager->set_receive_handler(service);

    // WHEN - send message via channel
    send_marshalled_message(channel_manager, create_test_send_message());

    // THEN - service receives and processes
    REQUIRE_EVENTUALLY(service->has_processed_message());
}
```

**Test 3.3**: Message routing to pass_through
```cpp
CORO_TYPED_TEST(handler_routing_test, "route to pass_through") {
    // GIVEN
    auto pass_through = create_pass_through();
    auto channel_manager = create_channel_manager();
    channel_manager->set_receive_handler(pass_through);

    // WHEN - send message via channel
    send_marshalled_message(channel_manager, create_test_send_message());

    // THEN - pass_through routes to destination service_proxy
    REQUIRE_EVENTUALLY(pass_through->has_routed_message());
}
```

**Test 3.4**: Transport sharing with disambiguation
```cpp
CORO_TYPED_TEST(transport_sharing_test, "multiple proxies share transport") {
    // GIVEN
    auto tcp_transport = create_tcp_transport("host", 8080);
    auto proxy_A = create_service_proxy(dest=zone{2}, caller=zone{1}, tcp_transport);
    auto proxy_B = create_service_proxy(dest=zone{2}, caller=zone{3}, tcp_transport);

    // Register both handlers with transport
    tcp_transport->add_destination_handler(zone{1}, proxy_A);
    tcp_transport->add_destination_handler(zone{3}, proxy_B);

    // WHEN - messages arrive for different callers
    auto msg_for_A = create_message(dest=zone{1}, caller=zone{2});
    auto msg_for_B = create_message(dest=zone{3}, caller=zone{2});

    tcp_transport->handle_incoming(msg_for_A);
    tcp_transport->handle_incoming(msg_for_B);

    // THEN - transport routes to correct proxy
    REQUIRE(proxy_A->received_message());
    REQUIRE(proxy_B->received_message());
}
```

#### Implementation Tasks

**Task 3.1**: Add handler registration to transport interface
```cpp
class i_transport {
public:
    virtual void set_receive_handler(std::weak_ptr<i_marshaller> handler) = 0;
};
```

**Task 3.2**: Update SPSC channel_manager
```cpp
class channel_manager : public i_transport {
    std::weak_ptr<i_marshaller> handler_;

    void set_receive_handler(std::weak_ptr<i_marshaller> handler) override {
        handler_ = handler;
    }

    void incoming_message_handler(envelope_prefix prefix, envelope_payload payload) {
        if (auto handler = handler_.lock()) {
            // Unpack and route to handler based on message type
            if (prefix.message_type == message_type::send) {
                handler->send(...);
            }
            else if (prefix.message_type == message_type::post) {
                handler->post(...);
            }
            // ... other message types
        }
    }
};
```

**Task 3.3**: Implement transport sharing pattern
```cpp
class shared_transport {
    // Map destination_zone to handler
    std::unordered_map<destination_zone, std::weak_ptr<i_marshaller>> handlers_;
    std::shared_mutex handlers_mutex_;

    void add_destination_handler(destination_zone dest, std::shared_ptr<i_marshaller> handler) {
        std::unique_lock lock(handlers_mutex_);
        handlers_[dest] = handler;
    }

    std::shared_ptr<i_marshaller> get_handler_for_destination(destination_zone dest) {
        std::shared_lock lock(handlers_mutex_);
        auto it = handlers_.find(dest);
        return (it != handlers_.end()) ? it->second.lock() : nullptr;
    }
};
```

#### Acceptance Criteria

- ✅ Transports can register i_marshaller handlers
- ✅ Handler can be service or pass_through (polymorphic)
- ✅ Message routing works for both handler types
- ✅ Transport sharing works with disambiguation
- ✅ Tests pass in BOTH sync and async modes

---

### Milestone 4: Transport Operational State (Week 7-8)

**Objective**: Implement is_operational() for both-or-neither guarantee

#### BDD Feature: Transport operational state
```gherkin
Feature: Transport operational state
  As a pass_through
  I want to check if transport is operational
  So that I can enforce both-or-neither guarantee

  Scenario: Transport is operational
    Given an SPSC channel_manager with active pump tasks
    And peer has not canceled
    When I check is_operational()
    Then it returns true
    And clone_for_zone() can create new proxies

  Scenario: Transport is non-operational - peer canceled
    Given an SPSC channel_manager
    And peer has sent zone_terminating
    When I check is_operational()
    Then it returns false
    And clone_for_zone() refuses to create new proxies

  Scenario: Transport is non-operational - local shutdown
    Given an SPSC channel_manager
    And local shutdown initiated
    When I check is_operational()
    Then it returns false
    And new operations return TRANSPORT_ERROR
```

#### TDD Test Specifications

**Test 4.1**: is_operational() when active
```cpp
TEST_CASE("channel_manager is_operational when active") {
    // GIVEN
    auto channel_mgr = create_active_channel_manager();

    // WHEN/THEN
    REQUIRE(channel_mgr->is_operational());
}
```

**Test 4.2**: is_operational() after peer cancel
```cpp
CORO_TYPED_TEST(operational_state_test, "not operational after peer cancel") {
    // GIVEN
    auto channel_mgr = create_active_channel_manager();

    // WHEN - peer sends zone_terminating
    CO_AWAIT simulate_peer_zone_termination(channel_mgr);

    // THEN
    REQUIRE(!channel_mgr->is_operational());
}
```

**Test 4.3**: clone_for_zone() refuses when not operational
```cpp
CORO_TYPED_TEST(operational_state_test, "clone refuses non-operational transport") {
    // GIVEN
    auto channel_mgr = create_active_channel_manager();
    auto proxy = create_service_proxy_with_transport(channel_mgr);

    // WHEN - transport becomes non-operational
    CO_AWAIT channel_mgr->shutdown();

    // Attempt to clone
    auto cloned_proxy = CO_AWAIT proxy->clone_for_zone(zone{5}, zone{1});

    // THEN - clone refused
    REQUIRE(cloned_proxy == nullptr);
}
```

#### Implementation Tasks

**Task 4.1**: Implement is_operational() for SPSC
```cpp
bool channel_manager::is_operational() const override {
    return !peer_cancel_received_.load(std::memory_order_acquire) &&
           !cancel_sent_ &&
           tasks_completed_.load(std::memory_order_acquire) < 2;
}
```

**Task 4.2**: Implement is_operational() for TCP
```cpp
bool tcp_channel_manager::is_operational() const override {
    return socket_connected_ &&
           !shutdown_initiated_ &&
           !connection_error_detected_;
}
```

**Task 4.3**: Implement is_operational() for Local
```cpp
bool local_transport::is_operational() const override {
    return true; // Always operational for in-process
}
```

**Task 4.4**: Add transport check to clone_for_zone()
```cpp
CORO_TASK(std::shared_ptr<service_proxy>) service_proxy::clone_for_zone(
    destination_zone dest, caller_zone caller) {

    // CRITICAL: Check transport operational before cloning
    if (transport_ && !transport_->is_operational()) {
        CO_RETURN nullptr; // Refuse to clone on dead transport
    }

    // Proceed with clone...
}
```

#### Acceptance Criteria

- ✅ is_operational() implemented for all transports
- ✅ Returns false when peer terminates
- ✅ Returns false when local shutdown initiated
- ✅ clone_for_zone() checks operational state
- ✅ Tests pass in BOTH sync and async modes

---

### Milestone 5: Pass-Through Core Implementation (Week 9-11)

**Objective**: Implement pass_through class with bidirectional routing

#### BDD Feature: Pass-through routing
```gherkin
Feature: Pass-through routing
  As a pass-through between zones
  I want to route messages bidirectionally
  So that zones can communicate through intermediaries

  Scenario: Forward direction routing
    Given zones A, B, C with B as intermediary
    And pass_through routing A ↔ C via B
    When zone A sends message to zone C
    Then pass_through routes to forward_proxy (B→C)
    And zone C receives the message

  Scenario: Reverse direction routing
    Given zones A, B, C with B as intermediary
    And pass_through routing A ↔ C via B
    When zone C sends message to zone A
    Then pass_through routes to reverse_proxy (C→A via B)
    And zone A receives the message

  Scenario: Pass-through manages reference counting
    Given a pass_through with two service_proxies
    When add_ref is called
    Then pass_through updates its internal reference count
    And maintains single count to service
    And service_proxy does not need lifetime_lock_
```

#### TDD Test Specifications

**Test 5.1**: Forward routing
```cpp
CORO_TYPED_TEST(pass_through_test, "forward direction routing") {
    // GIVEN
    auto service_a = create_service("zone_a", zone{1});
    auto service_b = create_service("zone_b", zone{2});
    auto service_c = create_service("zone_c", zone{3});

    auto forward_proxy = create_proxy(zone{2}, dest=zone{3}, caller=zone{1});
    auto reverse_proxy = create_proxy(zone{2}, dest=zone{1}, caller=zone{3});

    auto pass_through = std::make_shared<rpc::pass_through>(
        forward_proxy, reverse_proxy, service_b, zone{3}, zone{1});

    // WHEN - send from A to C (destination=3)
    std::vector<char> out_buf;
    auto error = CO_AWAIT pass_through->send(
        VERSION_3, encoding::yas_binary, tag++,
        caller_channel_zone{2}, caller_zone{1}, destination_zone{3},
        object{100}, interface_ordinal{1}, method{5},
        in_size, in_buf, out_buf, {}, {});

    // THEN - routed to forward_proxy
    REQUIRE(error == rpc::error::OK());
    REQUIRE(forward_proxy->was_called());
}
```

**Test 5.2**: Reverse routing
```cpp
CORO_TYPED_TEST(pass_through_test, "reverse direction routing") {
    // GIVEN - same setup as forward test
    auto pass_through = create_pass_through_A_to_C_via_B();

    // WHEN - send from C to A (destination=1)
    auto error = CO_AWAIT pass_through->send(
        VERSION_3, encoding::yas_binary, tag++,
        caller_channel_zone{2}, caller_zone{3}, destination_zone{1},
        object{200}, interface_ordinal{1}, method{10},
        in_size, in_buf, out_buf, {}, {});

    // THEN - routed to reverse_proxy
    REQUIRE(error == rpc::error::OK());
    REQUIRE(reverse_proxy->was_called());
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

**Test 5.4**: Bi-modal pass-through
```cpp
#ifdef BUILD_COROUTINE
CORO_TYPED_TEST(pass_through_test, "async mode routing") {
    auto error = CO_AWAIT pass_through->send(...);
    // Async routing with coroutines
}
#else
TEST(pass_through_test, "sync mode routing") {
    auto error = pass_through->send(...);
    // Sync routing with blocking calls
}
#endif
```

#### Implementation Tasks

**Task 5.1**: Create pass_through class
```cpp
class pass_through : public i_marshaller,
                     public std::enable_shared_from_this<pass_through> {
    destination_zone destination_zone_id_;
    caller_zone caller_zone_id_;

    std::atomic<uint64_t> shared_count_{0};
    std::atomic<uint64_t> optimistic_count_{0};

    std::shared_ptr<service_proxy> forward_proxy_;
    std::shared_ptr<service_proxy> reverse_proxy_;
    std::shared_ptr<service> service_;

public:
    pass_through(std::shared_ptr<service_proxy> forward,
                std::shared_ptr<service_proxy> reverse,
                std::shared_ptr<service> service,
                destination_zone dest,
                caller_zone caller);

    // i_marshaller implementations
    CORO_TASK(int) send(...) override;
    CORO_TASK(void) post(...) override;
    CORO_TASK(int) add_ref(...) override;
    CORO_TASK(int) release(...) override;
    CORO_TASK(int) try_cast(...) override;

private:
    std::shared_ptr<service_proxy> get_directional_proxy(destination_zone dest);
};
```

**Task 5.2**: Implement routing logic
```cpp
CORO_TASK(int) pass_through::send(...) {
    auto target_proxy = get_directional_proxy(destination_zone_id);
    if (!target_proxy) {
        CO_RETURN rpc::error::ZONE_NOT_FOUND();
    }

    CO_RETURN CO_AWAIT target_proxy->send(...);
}

std::shared_ptr<service_proxy> pass_through::get_directional_proxy(
    destination_zone dest) {

    if (dest == destination_zone_id_) {
        return forward_proxy_;
    } else {
        return reverse_proxy_;
    }
}
```

**Task 5.3**: Implement reference counting
```cpp
CORO_TASK(int) pass_through::add_ref(...) {
    // Update pass_through reference count
    if (options == add_ref_options::normal) {
        shared_count_.fetch_add(1, std::memory_order_acq_rel);
    }

    // Route to target proxy
    auto target = get_directional_proxy(destination_zone_id);
    CO_RETURN CO_AWAIT target->add_ref(...);
}

CORO_TASK(int) pass_through::release(...) {
    // Update pass_through reference count
    if (options == release_options::normal) {
        uint64_t prev = shared_count_.fetch_sub(1, std::memory_order_acq_rel);
        if (prev == 1 && optimistic_count_.load() == 0) {
            // Trigger self-destruction
            trigger_self_destruction();
        }
    }

    // Route to target proxy
    auto target = get_directional_proxy(destination_zone_id);
    CO_RETURN CO_AWAIT target->release(...);
}
```

#### Acceptance Criteria

- ✅ pass_through class implements i_marshaller
- ✅ Bidirectional routing works (forward and reverse)
- ✅ Reference counting managed by pass_through
- ✅ Single count to service maintained
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

4. **Architecture Requirements**
   - ✅ Transport is hidden implementation detail
   - ✅ Service derives from i_marshaller (no i_service)
   - ✅ Service_sink is transport-internal
   - ✅ Pass-through manages reference counting
   - ✅ Both-or-neither guarantee maintained
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

This master plan provides a concrete, testable roadmap for implementing the service proxy and transport refactoring. Each milestone is:

- **Well-Defined**: Clear objectives and acceptance criteria
- **Testable**: BDD/TDD specifications with concrete tests
- **Bi-Modal**: Tests run in both sync and async modes
- **Incremental**: Each milestone builds on previous ones
- **Validated**: Success criteria ensure quality

**Implementation should proceed milestone-by-milestone, with each milestone fully tested and validated before moving to the next.**

---

**End of Master Implementation Plan**
