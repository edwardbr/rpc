# Service Proxy and Transport Implementation Proposal

**Version**: 1.0  
**Date**: 2025-01-17  
**Status**: Proposed Solution

---

## Executive Summary

This document proposes a comprehensive redesign of the service proxy and transport architecture in RPC++ to address critical problems including race conditions, mixed responsibilities, complex lifecycle management, and lack of proper abstractions. The solution introduces clear separation of concerns through distinct transport, pass-through, and service sink abstractions while maintaining bi-modal (sync/async) compatibility.

## Table of Contents

1. [Problem Analysis Summary](#problem-analysis-summary)
2. [Proposed Architecture](#proposed-architecture)
3. [Core Abstraction Definitions](#core-abstraction-definitions)
4. [Implementation Strategy](#implementation-strategy)
5. [Race Condition Solutions](#race-condition-solutions)
6. [Performance and Thread Safety](#performance-and-thread-safety)
7. [Bi-Modal Support](#bi-modal-support)
8. [Backward Compatibility](#backward-compatibility)
9. [Implementation Timeline](#implementation-timeline)

---

## Problem Analysis Summary

### Critical Problems Identified

1. **Mixed Responsibilities**: Service proxy handles routing, transport, lifecycle, and protocol - should be separate concerns
2. **Y-Topology Race Condition**: On-demand proxy creation during critical path creates race windows
3. **Asymmetric Service Proxy Pairs**: Forward direction explicit, reverse on-demand - creates unpredictable lifecycle
4. **Transport Lifetime Mismatch**: Service proxy lifetime tied to individual object references, not bidirectional pairs
5. **Missing Transport Abstraction**: Only SPSC has emergent pattern, others ad-hoc
6. **No Fire-and-Forget Messaging**: Missing `post()` method for one-way operations
7. **Blurred Service/Pass-Through Responsibilities**: Service acts as universal receiver and router

### Root Causes

- **Lack of Abstraction Boundaries**: Transport, routing, and lifecycle concerns mixed
- **Reactive Proxy Creation**: On-demand creation violates predictable lifecycle
- **Insufficient Pass-Through Concept**: No explicit routing abstraction between zones
- **Inadequate Service Sink Abstraction**: No clear separation between transport and application logic

---

## Proposed Architecture

### New Component Hierarchy

```
┌─────────────────┐    ┌─────────────────┐    ┌─────────────────┐
│   Application   │    │   Transport     │    │   Application   │
│     (Client)    │    │    (Channel)    │    │    (Server)     │
├─────────────────┤    ├─────────────────┤    ├─────────────────┤
│  Generated      │    │  Transport      │    │  Generated      │
│  Proxy          │◄──►│  Abstraction    │◄──►│  Stub           │
├─────────────────┤    ├─────────────────┤    ├─────────────────┤
│  Service Proxy  │    │  Pass-Through   │    │  Service Proxy  │
│  (Routing/      │◄──►│  (Routing)      │◄──►│  (Routing/      │
│  Transport)     │    │                 │    │  Transport)     │
├─────────────────┤    ├─────────────────┤    ├─────────────────┤
│  Service Sink   │    │  Channel        │    │  Service Sink   │
│  (Handler)      │    │  Manager        │    │  (Handler)      │
└─────────────────┘    └─────────────────┘    └─────────────────┘
```

### Key Design Principles

1. **Separation of Concerns**: Each component handles one responsibility
2. **Predictable Lifecycle**: Explicit proxy pair creation, not reactive
3. **Inheritance-based Specialization**: Base service_proxy for general routing, derived classes only when transport needed
4. **Bi-Modal Compatibility**: Works in both sync and coroutine modes
5. **Thread Safe by Design**: Proper synchronization without deadlocks

### CRITICAL ARCHITECTURAL CLARIFICATIONS (From Implementation Plan Critique)

**CLARIFICATION 1: Service Architecture**
- **Service class** (NOT "i_service" interface) implements **i_marshaller** directly
- There is NO separate `i_service` interface - only the `service` class that derives from `i_marshaller`
- Service manages local object stubs and routes calls to appropriate service_proxy or local stub

**CLARIFICATION 2: Transport Ownership**
- **Transport is owned by specific service_proxy implementations** (spsc_service_proxy, tcp_service_proxy)
- Base `service_proxy` does NOT hold transport references
- **Transport sharing occurs at the transport level** - multiple service_proxy instances can reference the same transport object
- Each service_proxy manages its own transport lifecycle, but multiple service_proxies can share the same underlying transport resource

**CLARIFICATION 3: Service Sink is Transport-Internal**
- Service_sink (if it exists) is **NOT a core RPC++ class**
- Service_sink is **transport-internal implementation detail**, owned by service_proxy or transport
- Service does NOT manage or know about sinks
- Transport calls i_marshaller methods on either service OR pass_through

**CLARIFICATION 4: Back-Channel Format**
- Use `std::vector<back_channel_entry>` structure, NOT HTTP headers
- Each entry has `uint64_t type_id` (IDL-generated fingerprint) and `std::vector<uint8_t> payload`
- Type safety through IDL-generated type fingerprints

---

## Core Abstraction Definitions

### 1. Transport Interface (Pure Interface, No State)

**CRITICAL CORRECTION**: Following RPC++ naming convention, `i_transport` should be a pure interface with no member variables.

```cpp
// Pure transport interface - no member variables, only pure virtual methods
class i_transport {
public:
    virtual ~i_transport() = default;

    // Core transport operations (internal to service_proxy)
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
        const std::vector<back_channel_entry>& in_back_channel,
        std::vector<back_channel_entry>& out_back_channel
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
        const std::vector<back_channel_entry>& in_back_channel
    ) = 0;

    // Transport management (internal use)
    virtual bool is_operational() const = 0;
    virtual CORO_TASK(void) shutdown() = 0;
    virtual void set_receive_handler(std::weak_ptr<i_marshaller> handler) = 0;
    virtual std::shared_ptr<i_transport> clone_for_destination(destination_zone dest) = 0;
    
    // Transport-specific lifecycle management
    virtual void attach_service_proxy() = 0;
    virtual CORO_TASK(void) detach_service_proxy() = 0;
};

// Actual transport implementation with state - named without 'i_' prefix
class transport {
protected:
    // All member variables go here in the concrete implementation
    std::atomic<bool> operational_{true};
    std::atomic<int> service_proxy_count_{0};
    
    // Destination-to-handler map for routing inbound traffic to correct i_marshaller instance
    // Supports many-to-many connections between zones
    std::unordered_map<destination_zone, std::weak_ptr<i_marshaller>> destination_handlers_;
    mutable std::shared_mutex handlers_mutex_;

public:
    transport() = default;
    
    // Implement all i_transport interface methods
    CORO_TASK(int) send(...) override;
    CORO_TASK(void) post(...) override;
    
    // Additional implementation-specific methods
    void add_destination_handler(destination_zone dest, std::shared_ptr<i_marshaller> handler) {
        std::lock_guard g(handlers_mutex_);
        destination_handlers_[dest] = handler;
    }
    
    void remove_destination_handler(destination_zone dest) {
        std::lock_guard g(handlers_mutex_);
        destination_handlers_.erase(dest);
    }
    
    // CRITICAL: Handle incoming calls and route based on destination_zone
    CORO_TASK(void) handle_incoming_call(const marshalled_message& msg) {
        auto handler = get_handler_for_destination(msg.destination_zone_id);
        if (handler) {
            // Route the call to the appropriate handler (service or pass-through)
            // based on the destination in the message
            CO_AWAIT route_call_to_handler(handler, msg);
        } else {
            // No handler found for this destination - likely an error or zone shutdown
        }
    }
    
private:
    std::shared_ptr<i_marshaller> get_handler_for_destination(destination_zone dest) {
        std::shared_lock g(handlers_mutex_);
        auto it = destination_handlers_.find(dest);
        if (it != destination_handlers_.end()) {
            return it->second.lock();
        }
        return nullptr;
    }
    
    CORO_TASK(void) route_call_to_handler(std::shared_ptr<i_marshaller> handler, 
                                         const marshalled_message& msg);
};
```

**CORRECTED ARCHITECTURE**: Service proxies own their transport internally and implement i_marshaller by delegating to their transport:

```cpp
class service_proxy : public i_marshaller {
protected:
    std::shared_ptr<i_transport> transport_;  // Hidden implementation detail
    
public:
    // i_marshaller interface - implemented by delegating to transport
    CORO_TASK(int) send(
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
        const std::vector<back_channel_entry>& in_back_channel,
        std::vector<back_channel_entry>& out_back_channel
    ) override {
        return transport_ ? 
            transport_->send(protocol_version, encoding, tag,
                            caller_channel_zone_id, caller_zone_id, destination_zone_id,
                            object_id, interface_id, method_id,
                            in_size_, in_buf_, out_buf_,
                            in_back_channel, out_back_channel) :
            rpc::error::TRANSPORT_ERROR();  // For local service_proxy with no transport
    }

    CORO_TASK(void) post(
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
        const std::vector<back_channel_entry>& in_back_channel
    ) override {
        if (transport_) {
            CO_AWAIT transport_->post(protocol_version, encoding, tag,
                                     caller_channel_zone_id, caller_zone_id, destination_zone_id,
                                     object_id, interface_id, method_id, options,
                                     in_size_, in_buf_, in_back_channel);
        }
        // Local service_proxy: no transport, handles locally or routes differently
    }
    
    // Other i_marshaller methods implemented similarly...
};
```

**TRANSPORT SHARING PATTERN** (From Critique - Multiple service_proxies can share ONE transport):
```cpp
// Multiple service_proxy instances can share the same transport
// This allows efficient multiplexing over single physical connection
class shared_transport : public i_transport {
    // Reference counting for multiple service_proxy users
    std::atomic<int> user_count_{0};
    
    // Each service_proxy gets a unique identifier for disambiguation
    struct service_proxy_context {
        destination_zone dest_zone;
        caller_zone caller_zone;
        // Additional context for routing within shared transport
    };
    
    std::unordered_map<uint64_t, service_proxy_context> contexts_;
    
public:
    void add_user(destination_zone dest, caller_zone caller) {
        ++user_count_;
        // Register context for disambiguation
    }
    
    void remove_user() {
        if (--user_count_ == 0) {
            // Transport can be safely shut down when no users remain
        }
    }
};
```
```

### 2. Pass-Through Abstraction (Corrected Architecture)

**CRITICAL CORRECTION FROM CRITIQUE**: The pass-through should be responsible for reference counting and managing the lifecycle of its service_proxies, but the architecture should be corrected based on the critique findings. Pass-through implements i_marshaller and routes between service proxies, but transport remains hidden within the service_proxy implementations.

```cpp
class pass_through : public i_marshaller, public std::enable_shared_from_this<pass_through> {
    destination_zone destination_zone_id_;
    caller_zone caller_zone_id_; 
    
    // Reference counting responsibility (pass-through manages this)
    std::atomic<uint64_t> shared_count_{0};
    std::atomic<uint64_t> optimistic_count_{0};
    
    // Bidirectional service proxy management (direct ownership - CRITICAL IMPROVEMENT)
    std::shared_ptr<service_proxy> forward_proxy_;  // e.g., A→C direction
    std::shared_ptr<service_proxy> reverse_proxy_;  // e.g., C→A direction (opposite)
    
    // Service reference (managed by pass-through per Q&A requirement)
    // Use shared_ptr to ensure service lifecycle is managed properly
    // Service should not be destroyed while pass-through objects still exist
    std::shared_ptr<service> service_;
    
    // Self-reference to control pass_through lifecycle during shutdown
    // This is released during enforce_both_or_neither_guarantee to allow self-destruction
    std::shared_ptr<pass_through> self_reference_;
    
public:
    // Constructor creates bidirectional pair immediately
    pass_through(std::shared_ptr<service_proxy> forward_proxy,
                std::shared_ptr<service_proxy> reverse_proxy,
                std::shared_ptr<service> service,
                destination_zone dest_zone,
                caller_zone caller_zone)
        : forward_proxy_(std::move(forward_proxy))
        , reverse_proxy_(std::move(reverse_proxy))
        , service_(std::move(service))  // Move service to ensure proper ownership
        , destination_zone_id_(dest_zone)
        , caller_zone_id_(caller_zone)
    {
        // Initialize self-reference to control lifecycle
        self_reference_ = shared_from_this();
    }
    
    // i_marshaller implementations - route based on destination_zone
    CORO_TASK(int) send(
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
        const std::vector<back_channel_entry>& in_back_channel,
        std::vector<back_channel_entry>& out_back_channel
    ) override {
        // Route to appropriate service_proxy based on destination_zone
        std::shared_ptr<service_proxy> target_proxy = get_directional_proxy(destination_zone_id);
        if (target_proxy) {
            CO_RETURN CO_AWAIT target_proxy->send(protocol_version, encoding, tag,
                                                caller_channel_zone_id, caller_zone_id, destination_zone_id,
                                                object_id, interface_id, method_id,
                                                in_size_, in_buf_, out_buf_,
                                                in_back_channel, out_back_channel);
        }
        CO_RETURN rpc::error::ZONE_NOT_FOUND();
    }

    CORO_TASK(void) post(
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
        const std::vector<back_channel_entry>& in_back_channel
    ) override {
        std::shared_ptr<service_proxy> target_proxy = get_directional_proxy(destination_zone_id);
        if (target_proxy) {
            CO_AWAIT target_proxy->post(protocol_version, encoding, tag,
                                      caller_channel_zone_id, caller_zone_id, destination_zone_id,
                                      object_id, interface_id, method_id, options,
                                      in_size_, in_buf_, in_back_channel);
        }
        // No return needed for post
    }

    CORO_TASK(int) add_ref(
        uint64_t protocol_version,
        destination_channel_zone destination_channel_zone_id,
        destination_zone destination_zone_id,
        object object_id,
        caller_channel_zone caller_channel_zone_id,
        caller_zone caller_zone_id,
        known_direction_zone known_direction_zone_id,  // CRITICAL: For Y-topology routing
        add_ref_options options,
        uint64_t& reference_count,
        const std::vector<back_channel_entry>& in_back_channel,
        std::vector<back_channel_entry>& out_back_channel
    ) override {
        // Build operations (build_destination_route, build_caller_route) are complex and
        // should be handled by the service which has the global view of all connections
        if (options == add_ref_options::build_destination_route || 
            options == add_ref_options::build_caller_route) {
            // Pass responsibility to service which manages all zone connections
            if (service_) {
                CO_RETURN CO_AWAIT service_->handle_build_operation(
                    protocol_version, destination_channel_zone_id, destination_zone_id,
                    object_id, caller_channel_zone_id, caller_zone_id,
                    known_direction_zone_id, options, reference_count,
                    in_back_channel, out_back_channel);
            }
            // If service unavailable, return error
            CO_RETURN rpc::error::ZONE_NOT_FOUND();
        }
        
        // For normal add_ref operations, route to appropriate service_proxy
        std::shared_ptr<service_proxy> target_proxy = get_directional_proxy(destination_zone_id);
        if (target_proxy) {
            CO_RETURN CO_AWAIT target_proxy->add_ref(protocol_version, destination_channel_zone_id,
                                                   destination_zone_id, object_id,
                                                   caller_channel_zone_id, caller_zone_id,
                                                   known_direction_zone_id, options, reference_count,
                                                   in_back_channel, out_back_channel);
        }
        
        // If no route exists for normal add_ref operation, return error
        CO_RETURN rpc::error::ZONE_NOT_FOUND();
    }

    CORO_TASK(int) release(
        uint64_t protocol_version,
        destination_zone destination_zone_id,
        object object_id,
        caller_zone caller_zone_id,
        release_options options,
        uint64_t& reference_count,
        const std::vector<back_channel_entry>& in_back_channel,
        std::vector<back_channel_entry>& out_back_channel
    ) override {
        std::shared_ptr<service_proxy> target_proxy = get_directional_proxy(destination_zone_id);
        if (target_proxy) {
            CO_RETURN CO_AWAIT target_proxy->release(protocol_version, destination_zone_id,
                                                   object_id, caller_zone_id, options, reference_count,
                                                   in_back_channel, out_back_channel);
        }
        CO_RETURN rpc::error::ZONE_NOT_FOUND();
    }

    CORO_TASK(int) try_cast(
        uint64_t protocol_version,
        destination_zone destination_zone_id,
        object object_id,
        interface_ordinal interface_id,
        const std::vector<back_channel_entry>& in_back_channel,
        std::vector<back_channel_entry>& out_back_channel
    ) override {
        std::shared_ptr<service_proxy> target_proxy = get_directional_proxy(destination_zone_id);
        if (target_proxy) {
            CO_RETURN CO_AWAIT target_proxy->try_cast(protocol_version, destination_zone_id,
                                                    object_id, interface_id,
                                                    in_back_channel, out_back_channel);
        }
        CO_RETURN rpc::error::ZONE_NOT_FOUND();
    }

    // Pass-through specific methods
    std::shared_ptr<service_proxy> get_directional_proxy(destination_zone dest) {
        // Determine which proxy to use based on destination
        // Implementation depends on the specific routing logic needed
        if (dest == destination_zone_id_) {
            return forward_proxy_;
        } else {
            // For other destinations, return appropriate proxy based on routing logic
            // This could implement more complex routing decisions
            return reverse_proxy_; // Route via reverse direction
        }
    }
    
    // CRITICAL REQUIREMENT: Both-or-Neither operational guarantee
    // If one of the services ceases to be operational, then BOTH should be released
    // and the pass_through should self-destruct, bringing down the chain of 
    // service proxies to the opposite service
    void enforce_both_or_neither_guarantee() {
        // Check if either service has become non-operational
        bool forward_operational = forward_proxy_ && forward_proxy_->is_operational();
        bool reverse_operational = reverse_proxy_ && reverse_proxy_->is_operational();
        
        // If we have asymmetric operational state (one operational, one not)
        if (forward_operational != reverse_operational) {
            // Trigger self-destruction to enforce both-or-neither guarantee
            trigger_self_destruction();
        }
    }
    
    // Check current operational state
    bool both_or_neither_operational() const {
        // Both proxies must be operational or both non-operational
        bool forward_ok = forward_proxy_ && forward_proxy_->is_operational();
        bool reverse_ok = reverse_proxy_ && reverse_proxy_->is_operational();
        return (forward_ok && reverse_ok) || (!forward_ok && !reverse_ok);
    }
    
    // Trigger self-destruction when reference counts reach zero or operational asymmetry detected
    void trigger_self_destruction() {
        // Enforce both-or-neither: bring down both sides
        if (forward_proxy_) {
            // Release forward proxy and its connections
            forward_proxy_.reset();
        }
        if (reverse_proxy_) {
            // Release reverse proxy and its connections  
            reverse_proxy_.reset();
        }
        
        // Self-destruct: release self-reference to allow destruction
        // This will trigger cleanup of the pass_through itself
        if (service_) {
            // Notify service that this pass_through is shutting down
            service_->remove_pass_through(this);
        }
        
        // Release self-reference to allow pass_through to be destroyed
        // This is the key to controlled self-destruction
        self_reference_.reset();
        
        // AFTER THIS POINT, 'this' POINTER MAY BECOME INVALID
        // The pass_through will be destroyed when all external references are released
        // This cascades the shutdown to maintain both-or-neither guarantee
    }
    

    
private:
    // Check if any of our proxies is operational for the given destination
    bool is_proxy_for_destination_operational(destination_zone dest) const {
        // Check if the proxy for this destination is operational using polymorphic interface
        std::shared_ptr<service_proxy> proxy = get_directional_proxy(dest);
        return proxy && proxy->is_operational();
    }
    
    void update_reference_counts(uint64_t shared_delta, uint64_t optimistic_delta) {
        shared_count_ += shared_delta;
        optimistic_count_ += optimistic_delta;
        
        // Check if both reference counts have reached zero - triggers demise
        if (shared_count_ == 0 && optimistic_count_ == 0) {
            trigger_self_destruction();
            // NOTE: After trigger_self_destruction(), 'this' pointer may be invalid
            // Do not call any methods that access member variables after this point
            return;
        }
        
        // Update operational state based on reference counts
        // Only call if object is still valid
        update_operational_state();
    }
    
    // Monitor proxy operational status and enforce guarantee when needed
    void on_proxy_operational_status_changed() {
        // Called when a proxy's operational status may have changed
        update_operational_state();
    }
    
    // Trigger self-destruction when reference counts reach zero
    void trigger_self_destruction() {
        // Enforce both-or-neither: bring down both sides
        if (forward_proxy_) {
            // Release forward proxy and its connections
            forward_proxy_.reset();
        }
        if (reverse_proxy_) {
            // Release reverse proxy and its connections  
            reverse_proxy_.reset();
        }
        
        // Self-destruct: release self-reference to allow destruction
        // This will trigger cleanup of the pass_through itself
        if (service_) {
            // Notify service that this pass_through is shutting down
            service_->remove_pass_through(this);
        }
        
        // Release self-reference to allow pass_through to be destroyed
        // This is the key to controlled self-destruction
        self_reference_.reset();
        
        // The pass_through will be destroyed when all external references are released
        // This cascades the shutdown to maintain both-or-neither guarantee
    }
    
    // Accessors for bidirectional proxies
    std::shared_ptr<service_proxy> get_forward_proxy() const { return forward_proxy_; }
    std::shared_ptr<service_proxy> get_reverse_proxy() const { return reverse_proxy_; }
    
private:
    void update_operational_state() {
        // Implementation to maintain both-or-neither guarantee
        // If either proxy becomes non-operational, enforce both-or-neither
        enforce_both_or_neither_guarantee();
    }
    
    // Check if object is still valid before accessing member variables
    bool is_valid() const {
        // Simple validity check - in a real implementation might check magic numbers or flags
        return shared_count_ >= 0; // Basic check that object hasn't been corrupted
    }
    
    bool is_proxy_operational(const std::shared_ptr<service_proxy>& proxy) const {
        // Use polymorphic interface - no need to know specific proxy type
        return proxy->is_operational();
    }
    
    // Create new bidirectional proxies for a zone
    CORO_TASK(std::shared_ptr<service_proxy>) create_bidirectional_proxies_for_zone(
        destination_zone dest_zone, known_direction_zone known_direction);
};
```
```

### 3. Service Sink Abstraction (Transport-Internal)

```cpp
// Service sink remains transport-internal implementation detail
// Owned by service_proxy or pass_through as needed

class service_sink {
    std::shared_ptr<i_marshaller> handler_;  // service or pass_through
    std::shared_ptr<i_transport> transport_;
    
public:
    explicit service_sink(std::shared_ptr<i_marshaller> handler, 
                         std::shared_ptr<i_transport> transport);
    
    // Demarshalling and dispatch
    CORO_TASK(void) handle_incoming_message(const marshalled_message& msg);
    
    // Transport lifecycle integration
    void on_transport_shutdown();
};
```

### 4. Updated Service Proxy Hierarchy (Transport Optional)

```cpp
// Base service proxy for routing and marshalling
class service_proxy : public i_marshaller {
    zone zone_id_;
    destination_zone destination_zone_id_;
    destination_channel_zone destination_channel_zone_id_;
    caller_zone caller_zone_id_;
    
    // Minimal object proxy aggregation
    std::unordered_map<object, std::weak_ptr<object_proxy>> proxies_;
    
    // Backward compatibility for existing code
    mutable std::shared_mutex proxies_mutex_;
    
public:
    // Constructor - base class handles routing
    service_proxy(zone zone_id,
                 destination_zone dest_zone,
                 caller_zone caller_zone);
    
    // i_marshaller interface (delegates to internal transport or handles locally)
    CORO_TASK(int) send(...) override;
    CORO_TASK(void) post(...) override;
    CORO_TASK(int) add_ref(...) override;
    CORO_TASK(int) release(...) override;
    CORO_TASK(int) try_cast(...) override;
    
    // Object proxy management
    std::shared_ptr<object_proxy> get_or_create_object_proxy(object obj_id);
    
    // Lifecycle helpers
    bool has_active_objects() const;
    void cleanup_unused_proxies();
    
    // Polymorphic transport status - hides transport implementation details
    virtual bool is_operational() const = 0;
};

// SPSC service proxy - handles SPSC communication
class spsc_service_proxy : public service_proxy {
    std::shared_ptr<transport> transport_;  // Owns its concrete transport
    
public:
    spsc_service_proxy(zone zone_id,
                      destination_zone dest_zone,
                      caller_zone caller_zone,
                      queue_type* send_queue,
                      queue_type* receive_queue);
    // Internally creates spsc_transport and owns it
    
    // i_marshaller methods implemented by delegating to owned transport
    CORO_TASK(int) send(...) override;
    CORO_TASK(void) post(...) override;
    CORO_TASK(int) add_ref(...) override;
    CORO_TASK(int) release(...) override;
    CORO_TASK(int) try_cast(...) override;
    
    bool is_operational() const override { 
        return transport_ && transport_->is_operational(); 
    }
};

// TCP service proxy - handles TCP communication
class tcp_service_proxy : public service_proxy {
    std::shared_ptr<transport> transport_;  // Owns its concrete transport
    
public:
    tcp_service_proxy(zone zone_id,
                     destination_zone dest_zone,
                     caller_zone caller_zone,
                     const std::string& host,
                     uint16_t port);
    // Internally creates tcp_transport and owns it
    
    // i_marshaller methods implemented by delegating to owned transport
    CORO_TASK(int) send(...) override;
    CORO_TASK(void) post(...) override;
    CORO_TASK(int) add_ref(...) override;
    CORO_TASK(int) release(...) override;
    CORO_TASK(int) try_cast(...) override;
    
    bool is_operational() const override { 
        return transport_ && transport_->is_operational(); 
    }
};

// Local service proxy - no transport, direct in-process calls
class local_service_proxy : public service_proxy {
public:
    local_service_proxy(zone zone_id,
                       destination_zone dest_zone,
                       caller_zone caller_zone);
    // No transport needed, calls service directly
    
    // i_marshaller methods implemented by direct local calls
    CORO_TASK(int) send(...) override;
    CORO_TASK(void) post(...) override;
    CORO_TASK(int) add_ref(...) override;
    CORO_TASK(int) release(...) override;
    CORO_TASK(int) try_cast(...) override;
    
    bool is_operational() const override { 
        // Local in-process calls are always operational
        return true; 
    }
};

// TRANSPORT SHARING: Multiple service_proxy instances can share the same transport
// This happens when multiple service_proxy objects point to the same destination zone
// and are created with the same underlying transport connection
class transport {
    // Transport lifecycle managed by shared_ptr references from service_proxies
    // Each transport is owned by its respective service_proxy instance
    // Transport destruction occurs when all service_proxy references are released
    
    // CRITICAL: Destination-to-handler map for routing inbound traffic to correct i_marshaller
    // Supports many-to-many connections between zones - one transport can handle multiple destinations
    std::unordered_map<destination_zone, std::weak_ptr<i_marshaller>> destination_handlers_;
    mutable std::shared_mutex handlers_mutex_;

public:
    // Transport lifecycle managed by shared_ptr references from service_proxies
    // No explicit user management needed
    
    // CRITICAL: Add handler for specific destination (service or pass-through)
    void add_destination_handler(destination_zone dest, std::shared_ptr<i_marshaller> handler) {
        std::lock_guard g(handlers_mutex_);
        destination_handlers_[dest] = handler;
    }
    
    void remove_destination_handler(destination_zone dest) {
        std::lock_guard g(handlers_mutex_);
        destination_handlers_.erase(dest);
    }
    
    // Handle incoming demarshalled call and route based on destination_zone
    CORO_TASK(void) handle_incoming_call(const marshalled_message& msg) {
        auto handler = get_handler_for_destination(msg.destination_zone_id);
        if (handler) {
            // Route to appropriate destination based on message destination
            CO_AWAIT dispatch_to_handler(handler, msg);
        } else {
            // No handler found for this destination - may need to return error or handle specially
        }
    }
    
private:
    std::shared_ptr<i_marshaller> get_handler_for_destination(destination_zone dest) {
        std::shared_lock g(handlers_mutex_);
        auto it = destination_handlers_.find(dest);
        if (it != destination_handlers_.end()) {
            return it->second.lock();
        }
        return nullptr;
    }
    
    CORO_TASK(void) dispatch_to_handler(std::shared_ptr<i_marshaller> handler, 
                                       const marshalled_message& msg) {
        // Dispatch to the handler based on the message type
        switch (msg.message_type) {
            case message_type::send:
                CO_AWAIT handler->send(
                    msg.protocol_version, msg.encoding, msg.tag,
                    msg.caller_channel_zone_id, msg.caller_zone_id, msg.destination_zone_id,
                    msg.object_id, msg.interface_id, msg.method_id,
                    msg.in_size, msg.in_buf, msg.out_buf,
                    msg.in_back_channel, msg.out_back_channel
                );
                break;
            case message_type::post:
                CO_AWAIT handler->post(
                    msg.protocol_version, msg.encoding, msg.tag,
                    msg.caller_channel_zone_id, msg.caller_zone_id, msg.destination_zone_id,
                    msg.object_id, msg.interface_id, msg.method_id, msg.post_options,
                    msg.in_size, msg.in_buf, msg.in_back_channel
                );
                break;
            case message_type::add_ref:
                CO_AWAIT handler->add_ref(
                    msg.protocol_version, msg.encoding, msg.tag,
                    msg.caller_channel_zone_id, msg.caller_zone_id, msg.destination_zone_id,
                    msg.object_id, msg.add_ref_options, msg.out_param,
                    msg.in_back_channel, msg.out_back_channel
                );
                break;
            case message_type::release:
                CO_AWAIT handler->release(
                    msg.protocol_version, msg.encoding, msg.tag,
                    msg.caller_channel_zone_id, msg.caller_zone_id, msg.destination_zone_id,
                    msg.object_id, msg.release_options, msg.out_param,
                    msg.in_back_channel, msg.out_back_channel
                );
                break;
            case message_type::try_cast:
                CO_AWAIT handler->try_cast(
                    msg.protocol_version, msg.encoding, msg.tag,
                    msg.caller_channel_zone_id, msg.caller_zone_id, msg.destination_zone_id,
                    msg.object_id, msg.interface_id,
                    msg.in_back_channel, msg.out_back_channel
                );
                break;
        }
    }
};
```

---

## Implementation Strategy

### Phase 1: Transport Abstraction Layer

**Objective**: Create unified transport interface and migrate existing transports

**Tasks**:
1. Define `i_transport` interface with all required methods
2. **Migrate SPSC transport to use new pattern**:
   - Update `channel_manager` to implement `i_transport` interface
   - Replace `service_proxy_ref_count_` reference counting with pass-through managed lifetime
   - Add `set_receive_handler()` method to register i_marshaller handler (service or pass_through)
   - Add `is_operational()` method to check transport viability for clone_for_zone()
   - Implement zone_terminating message handling and broadcast to service/pass_through
   - Add transport-specific shutdown coordination to handle multiple service_proxies
3. Create TCP transport with channel manager pattern  
4. Update local and SGX transports to use transport abstraction
5. Implement transport registry and factory patterns

**Key Changes**:
- SPSC: `channel_manager` becomes the `i_transport` implementation with updated lifecycle management
- TCP: Introduce `tcp_channel_manager` similar to SPSC pattern
- Local: Minimal transport wrapper for direct calls
- SGX: Use SPSC pattern in coroutine mode, minimal wrapper in sync mode

**SPSC Channel Manager Specific Changes**:
1. **Handler Registration**: Add `std::weak_ptr<i_marshaller> handler_` to route incoming messages to service or pass_through
2. **Reference Counting**: Transition from `service_proxy_ref_count_` to pass-through managed lifecycle (will be phased out in future phases)
3. **Zone Termination**: Add handling for `post_options::zone_terminating` messages and broadcast to registered handlers
4. **Transport Viability**: Implement `is_operational()` method that checks:
   - `!peer_cancel_received_` (peer hasn't terminated)
   - `!cancel_sent_` (we haven't initiated shutdown) 
   - Pump tasks are still running
5. **Shutdown Coordination**: Enhanced shutdown logic to broadcast zone termination notifications to both service and pass_through handlers
6. **Message Routing**: Update `incoming_message_handler` to call registered i_marshaller handler after unpacking envelope

### Phase 2: Pass-Through Implementation

**Objective**: Implement routing abstraction and reference counting

**Tasks**:
1. Create `pass_through` class implementing `i_marshaller`
2. Implement both-or-neither operational guarantee
3. Add reference counting and lifecycle management
4. Create pass-through factory and registry
5. Update service to use pass-through for routing

**Key Features**:
- Bidirectional proxy pair management
- Reference counting responsibility
- Transport viability checks for `clone_for_zone()`
- Cascading cleanup for zone termination

**Transport-Specific Implementation Details**:

**SPSC Transport Integration**:
- Channel managers will register pass_through as their i_marshaller handler instead of services
- Multiple service_proxies can share one channel_manager (transport sharing pattern)
- Channel manager's `service_proxy_ref_count_` will be managed by pass_through (eventually phased out)
- Channel manager handles zone_terminating messages and forwards to pass_through for routing cleanup

**TCP Transport Integration**:
- TCP channel managers will follow same pattern as SPSC for handler registration
- TCP connections can be shared among multiple service_proxies using the same destination zone
- Zone termination handling includes TCP connection cleanup and notification broadcasting
- Connection reliability handling integrated with pass_through operational state

**Local Transport Integration**:
- For local_service_proxy, pass_through routing happens in-process without serialization
- Reference counting still applies for proper lifecycle management
- Zone termination is immediate since zones are in the same process
- Bypasses transport layer for direct local calls but still uses pass_through for routing decisions

**SGX Transport Integration**:
- In coroutine mode: SPSC channel after initial ecall setup, with pass_through managing both host_service_proxy and enclave_service_proxy
- In sync mode: Direct ecall/ocall with pass_through managing lifetimes
- Zone termination triggers both enclave and host cleanup coordination

### Phase 3: Service and Service Sink Refactoring

**Objective**: Separate service routing from transport handling

**Tasks**:
1. Refactor service to focus on local stub management
2. Remove routing logic from service (moved to pass-through)
3. Create service sink abstraction for transport-internal logic
4. Update service lifecycle and cleanup logic
5. Implement proper mutex discipline

### Phase 4: Back-Channel and Post Message Support

**Objective**: Add new messaging capabilities

**Tasks**:
1. Add back-channel parameters to all `i_marshaller` methods
2. Implement `post()` method for fire-and-forget messaging
3. Add `post_options` enum with all required options
4. Implement optimistic reference cleanup via `post()`
5. Add zone termination broadcast mechanism

### Phase 5: Y-Topology and Race Condition Resolution

**Objective**: Eliminate on-demand proxy creation

**Tasks**:
1. Implement explicit bidirectional proxy creation
2. Remove reactive proxy creation in `send()`
3. Add transport operational checks for `clone_for_zone()`
4. Implement proper zone topology validation
5. Update all affected tests

---

## Race Condition Solutions

### Solution 1: Y-Topology Race Condition

**Problem**: Dynamic proxy creation in hot path during `send()`

**Solution**: 
1. **Pre-create bidirectional proxy pairs** when first connection established
2. **Remove on-demand creation logic** from `send()` method
3. **Implement transport operational checks** before allowing `clone_for_zone()`

**Implementation**:
```cpp
class service {
    // Instead of creating reverse proxy on-demand
    void create_bidirectional_proxy_pair(destination_zone dest, caller_zone caller) {
        auto forward_proxy = create_service_proxy_for_destination(dest);
        auto reverse_proxy = create_service_proxy_for_destination(caller);
        
        // Register both immediately - no on-demand creation
        inner_add_zone_proxy(forward_proxy);
        inner_add_zone_proxy(reverse_proxy);
    }
};
```

### Solution 2: Service Proxy Destruction During Active Call

**Problem**: Use-after-free when proxy destroyed during active operation

**Solution**:
1. **Pass-through manages lifecycle** - controls both proxy lifetimes
2. **Transport holds strong references** during active operations
3. **Proper synchronization** with atomic reference counting

**Implementation**:
```cpp
class pass_through {
    // Atomic reference counting to prevent destruction during use
    std::atomic<uint32_t> active_operations_{0};
    
    CORO_TASK(int) send(...) {
        // Increment during operation
        ++active_operations_;
        
        // Hold strong reference to transport
        auto transport = transport_.lock();
        if (!transport) {
            --active_operations_;
            CO_RETURN rpc::error::SERVICE_PROXY_LOST_CONNECTION();
        }
        
        auto result = CO_AWAIT transport->send(...);
        
        // Decrement after operation
        --active_operations_;
        CO_RETURN result;
    }
};
```

### Solution 3: Transport Destruction with Proxy Pair

**Problem**: Multiple threads destroying paired proxies causing double-free

**Solution**:
1. **Pass-through manages both proxies** - single point of control
2. **Atomic state management** for operational guarantee
3. **Coordinated shutdown** with proper synchronization

**Implementation**:
```cpp
class pass_through {
    std::atomic<bool> shutdown_initiated_{false};
    std::mutex shutdown_mutex_;
    
    CORO_TASK(void) coordinated_shutdown() {
        if (shutdown_initiated_.exchange(true)) {
            // Already shutting down - another thread handles
            CO_RETURN;
        }
        
        // Acquire mutex to coordinate with reverse pass-through
        std::lock_guard g(shutdown_mutex_);
        
        // Ensure both-or-neither guarantee maintained
        if (auto reverse = reverse_pass_through_.lock()) {
            CO_AWAIT reverse->initiate_shutdown();
        }
        
        // Proceed with shutdown
        CO_AWAIT transport_->shutdown();
    }
};
```

### Solution 4: Mutex Discipline

**Problem**: Mutexes held across `CO_AWAIT` points causing deadlocks

**Solution**:
1. **Remove mutexes from hot paths** - use lock-free data structures where possible
2. **Implement RAII synchronization** with proper scoping
3. **Use atomic operations** for reference counting

**Implementation**:
```cpp
class service_proxy {
    // Use atomic reference counting instead of mutex
    std::atomic<uint64_t> object_count_{0};
    
    // Use concurrent data structures
    std::atomic<std::shared_ptr<object_proxy>>* get_object_proxy_atomic(object obj_id);
};
```

---

## Performance and Thread Safety

### Performance Optimizations

#### 1. Lock-Free Object Proxy Management
```cpp
// Replace mutex-guarded maps with lock-free alternatives
template<typename Key, typename Value>
class lock_free_map {
    // Implementation using atomic operations
    std::atomic<Value*> entries_[MAX_OBJECTS];
    
public:
    Value* get_or_create(Key key, std::function<Value*()> factory);
    bool remove(Key key);
};
```

#### 2. Object Pooling for Frequently Created Objects
```cpp
template<typename T>
class object_pool {
    std::vector<std::unique_ptr<T>> available_;
    std::mutex pool_mutex_;
    
public:
    std::unique_ptr<T> acquire();
    void release(std::unique_ptr<T> obj);
};
```

#### 3. Batched Reference Counting Updates
```cpp
class batched_reference_counter {
    // Batch multiple ref count operations to reduce synchronization overhead
    std::vector<std::pair<object, int64_t>> pending_updates_;
    
public:
    void add_update(object obj, int64_t delta);
    void commit_batch_updates();
};
```

### Thread Safety Patterns

#### 1. Actor Pattern for Service Operations
```cpp
class actor_service {
    moodycamel::blocking_concurrentqueue<std::function<void()>> operation_queue_;
    
    template<typename F>
    CORO_TASK(auto) send_message(F&& operation) {
        // Execute operation on dedicated thread
        // Ensures sequential consistency without mutexes
        co_return co_await schedule_on_actor_thread(std::forward<F>(operation));
    }
};
```

#### 2. Read-Write Lock Patterns
```cpp
class optimized_service {
    boost::shared_mutex access_mutex_;
    
    // Use shared locks for read-heavy operations
    auto get_stub_read_only(object id) {
        boost::shared_lock lock(access_mutex_);
        return stubs_.find(id);
    }
    
    // Use exclusive locks for modifications only
    void add_stub(object id, std::shared_ptr<object_stub> stub) {
        std::unique_lock lock(access_mutex_);
        stubs_[id] = stub;
    }
};
```

#### 3. Lock-Free Reference Counting
```cpp
class atomic_reference_counter {
    std::atomic<uint64_t> shared_count_{0};
    std::atomic<uint64_t> optimistic_count_{0};
    std::atomic<bool> being_destroyed_{false};
    
public:
    bool increment_shared() {
        uint64_t current = shared_count_.load(std::memory_order_acquire);
        while (true) {
            if (being_destroyed_.load(std::memory_order_acquire)) {
                return false; // Object being destroyed
            }
            if (shared_count_.compare_exchange_weak(current, current + 1)) {
                return true; // Successfully incremented
            }
        }
    }
    
    bool decrement_shared() {
        uint64_t current = shared_count_.fetch_sub(1, std::memory_order_acq_rel);
        return (current == 1); // Returns true if this was the last reference
    }
};
```

---

## Bi-Modal Support

### Synchronous Mode Adaptations

#### 1. Post Message Handling
```cpp
#ifdef BUILD_COROUTINE
// Coroutine mode: true fire-and-forget
CORO_TASK(void) handle_post_message(...) {
    // Execute asynchronously, no response expected
    CO_AWAIT dispatch_post_message_synchronously(...);
    CO_RETURN;
}
#else
// Synchronous mode: blocking fire-and-forget
void handle_post_message(...) {
    // Execute synchronously, return immediately
    dispatch_post_message_synchronously(...);
}
#endif
```

#### 2. Cleanup Timing Differences
```cpp
class service_proxy {
    CORO_TASK(void) cleanup_if_needed() {
#ifdef BUILD_COROUTINE
        // Async mode: schedule cleanup
        if (should_cleanup()) {
            io_scheduler_->schedule([this]() -> coro::task<void> {
                CO_AWAIT perform_cleanup();
                CO_RETURN;
            });
        }
#else
        // Sync mode: immediate cleanup
        if (should_cleanup()) {
            perform_cleanup();
        }
#endif
    }
};
```

#### 3. Transport-Specific Behavior
```cpp
// Local transport adapts to both modes
class local_transport : public i_transport {
    CORO_TASK(int) send(...) {
#ifdef BUILD_COROUTINE
        // In coroutine mode, still run synchronously but yield to scheduler
        auto result = direct_local_call(...);
        CO_AWAIT io_scheduler_->schedule(); // Yield to other coroutines
        CO_RETURN result;
#else
        // In sync mode, direct call with no scheduling
        CO_RETURN direct_local_call(...);
#endif
    }
};
```

### Transport Compatibility Matrix

| Transport Type | Sync Mode | Async Mode | Notes |
|----------------|-----------|------------|-------|
| Local | ✅ | ✅ | Bi-modal, direct calls |
| SGX (Sync) | ✅ | ✅ | Bi-modal, ecall/ocall |
| SGX (Async) | ❌ | ✅ | SPSC channel after setup |
| SPSC | ❌ | ✅ | Pump tasks required |
| TCP | ❌ | ✅ | Async I/O required |
| DLL | ✅ | ✅ | Bi-modal when implemented |

---

## Backward Compatibility

### API Compatibility

#### 1. Public Interface Preservation
- Keep existing `service_proxy` template parameters and method signatures
- Maintain `i_marshaller` interface (with additional back-channel parameters)
- Preserve object proxy lifecycle management
- Maintain service zone creation APIs (`connect_to_zone`, `attach_remote_zone`)

#### 2. Internal Changes Protected
- Hide transport abstraction behind existing interfaces
- Maintain existing error code behavior
- Preserve telemetry and logging interfaces
- Keep zone type system unchanged

### Migration Strategy

#### 1. Gradual Migration Path
```cpp
// Phase 1: Introduce new abstractions alongside existing code
// Phase 2: Update transports to use new abstractions
// Phase 3: Replace service routing with pass-through
// Phase 4: Remove old abstractions (deprecation cycle)
```

#### 2. Configuration Options
```cpp
// Compile-time option to use new vs old architecture
#ifdef RPC_USE_NEW_ARCHITECTURE
    #include "new_service_proxy.h"
    #include "transport_abstraction.h"
    #include "pass_through.h"
#else
    #include "legacy_service_proxy.h"  // Maintained for compatibility
#endif
```

### Testing Compatibility

#### 1. Maintain Existing Tests
- All current tests must pass with new implementation
- Add tests for new functionality
- Ensure race condition fixes don't break existing functionality

#### 2. Y-Topology Test Updates
```cpp
// Updated tests without on-demand proxy creation
TEST_CASE("test_y_topology_bidirectional_proxies") {
    // Create bidirectional proxies explicitly
    // Verify both directions work without race conditions
    REQUIRE(service_a_can_reach_service_c());
    REQUIRE(service_c_can_reach_service_a());  // Previously failed
}
```

---

## Implementation Timeline

### Phase 1: Foundation (Weeks 1-3)
- [ ] Define and implement `i_transport` interface
- [ ] Migrate SPSC to use new transport abstraction
- [ ] Create transport factory and registry patterns
- [ ] Unit tests for transport layer

### Phase 2: Routing Abstraction (Weeks 4-6)  
- [ ] Implement `pass_through` class with i_marshaller
- [ ] Add reference counting and lifecycle management
- [ ] Implement both-or-neither operational guarantee
- [ ] Unit tests for pass-through functionality

### Phase 3: Service Refactoring (Weeks 7-9)
- [ ] Separate service routing from transport handling
- [ ] Implement service sink abstractions
- [ ] Update service lifecycle management
- [ ] Integration tests for service layer

### Phase 4: New Features (Weeks 10-11)
- [ ] Add back-channel support to all i_marshaller methods
- [ ] Implement `post()` method for fire-and-forget
- [ ] Add zone termination broadcast mechanism
- [ ] Optimistic cleanup via post messages

### Phase 5: Race Condition Fixes (Weeks 12-13)
- [ ] Eliminate on-demand proxy creation
- [ ] Implement bidirectional pair creation
- [ ] Add transport operational checks
- [ ] Update all affected tests and benchmarks

### Phase 6: Optimization and Testing (Weeks 14-15)
- [ ] Performance optimization and profiling
- [ ] Stress testing and race condition verification
- [ ] Documentation and migration guide
- [ ] Final integration testing

---

## Critical Requirements and Guarantees

### Both-or-Neither Operational Guarantee

**CRITICAL REQUIREMENT FROM Q&A**: The pass-through must guarantee that BOTH or NEITHER of its service_proxies are operational.

```cpp
class pass_through {
    // Ensures bidirectional communication is always possible or completely unavailable
    std::atomic<bool> both_operational_{true};
    std::mutex operational_mutex_;
    
public:
    bool both_or_neither_operational() const {
        return both_operational_.load();
    }
    
    void ensure_both_or_neither_operational() {
        // If one service_proxy becomes non-operational, ensure both are marked as such
        // or coordinate to keep both operational
        std::lock_guard g(operational_mutex_);
        // Implementation ensures symmetric operational state
    }
    
    // Clone operations must check transport operational status
    bool can_create_new_proxy_for_transport(destination_zone dest) const {
        auto transport = get_transport_for_destination(dest);
        return transport && transport->is_operational();
    }
};
```

**IMPLEMENTATION STRATEGY**:
1. Pass-through monitors operational status of both service_proxies it manages
2. When one becomes non-operational, it either:
   - Takes down the other to maintain symmetry, OR
   - Attempts to restore the failed one
3. clone_for_zone() checks transport operational status before creating new proxies

### Transport Viability Checks

**CRITICAL REQUIREMENT**: clone_for_zone() must refuse to create new service_proxies if transport is not operational.

```cpp
class service_proxy {
public:
    static CORO_TASK(std::shared_ptr<service_proxy>) clone_for_zone(
        destination_zone dest_zone,
        caller_zone caller_zone,
        const service_proxy& template_proxy) {
        
        // CRITICAL: Check transport operational status first
        if (template_proxy.transport_ && !template_proxy.transport_->is_operational()) {
            CO_RETURN nullptr; // Refuse to create proxy on dead transport
        }
        
        // Proceed with clone operation
        auto cloned = std::make_shared<service_proxy>(
            template_proxy.zone_id_,
            dest_zone,
            caller_zone,
            template_proxy.transport_ ? 
                template_proxy.transport_->clone_for_destination(dest_zone) : 
                nullptr
        );
        
        CO_RETURN cloned;
    }
};
```

## Transport Implementation Specifics

### SPSC Channel Manager Updates

Based on the SPSC Channel Lifecycle document analysis, the following specific changes are required:

**1. Handler Registration System**:
```cpp
class channel_manager : public i_transport {
    // NEW: Weak reference to i_marshaller handler (service or pass_through)
    std::weak_ptr<i_marshaller> handler_;
    
    // Update incoming_message_handler to route calls to handler
    void incoming_message_handler(envelope_prefix prefix, envelope_payload payload) {
        if (auto handler = handler_.lock()) {
            // Route to appropriate handler after unpacking envelope
            if (prefix.message_type == message_type::send) {
                handler->send(/* params */);
            }
            else if (prefix.message_type == message_type::post) {
                handler->post(/* params */);
            }
            // ... other message types
        }
    }
};
```

**2. Lifecycle Management**:
- **Phase out** `service_proxy_ref_count_` in favor of pass_through managed lifecycle
- **Maintain** `tasks_completed_` for pump task coordination
- **Add** zone termination detection and broadcast mechanism

**3. Zone Termination Handling**:
```cpp
// In shutdown() method
CORO_TASK(void) shutdown() {
    // If zone_terminating received, skip peer acknowledgment
    if (zone_terminating_received_) {
        // Broadcast to service and pass-through for cleanup
        if (auto handler = handler_.lock()) {
            // Send zone_terminating to registered handler
            handler->post(/* zone_terminating options */);
        }
        // Skip waiting for peer acknowledgment
        CO_RETURN;
    }
    // ... normal graceful shutdown
}
```

**4. Transport Viability Checks**:
```cpp
bool is_operational() const override {
    return !peer_cancel_received_.load() && 
           !cancel_sent_ && 
           pump_tasks_running();
}
```

### TCP Transport Implementation

Following the SPSC pattern, TCP transport will implement similar features:

**1. Channel Manager Pattern**:
- `tcp_channel_manager` will implement `i_transport` interface
- Separate sender/receiver tasks for async I/O operations
- Handler registration system for routing to service or pass_through

**2. Connection Reliability**:
- Handle connection loss and reconnection
- Maintain operational state for transport viability checks
- Zone termination handling for graceful connection shutdown

**3. Reference Counting**:
- Multiple service_proxies can share one TCP connection (transport sharing)
- Pass_through will manage service_proxy lifetimes (replacing connection ref counting)

### Local Transport Simplification

**1. In-Process Communication**:
- No serialization/deserialization needed
- Direct function calls between zones in same process
- Handler registration still applies for routing through pass_through

**2. Lifecycle Management**:
- Immediate cleanup on zone termination (same process)
- No complex shutdown coordination needed
- Simple reference counting for service_proxy management

### SGX Transport Integration

**1. Dual Mode Support**:
- **Sync Mode**: Direct ecall/ocall with minimal transport abstraction
- **Coroutine Mode**: SPSC channel after initial setup, following same patterns as SPSC transport

**2. Enclave-Specific Handling**:
- Initial ecall to establish executor and SPSC channel
- Shared channel between host_service_proxy and enclave_service_proxy
- Coordinated cleanup between enclave and host on zone termination


## Expected Benefits

### Performance Improvements
- **Reduced Lock Contention**: Lock-free data structures in hot paths
- **Predictable Allocation**: No hot-path allocation for proxy creation
- **Efficient Reference Counting**: Atomic operations instead of mutexes
- **Better Concurrency**: Actor pattern for sequential operations

### Reliability Improvements  
- **Eliminated Race Conditions**: Predictable proxy lifecycle
- **Coordinated Shutdown**: Proper bidirectional cleanup
- **Transport Viability**: Prevent operations on dead transports
- **Deadlock Prevention**: Proper mutex discipline
- **Both-or-Neither Guarantee**: Symmetric service proxy operational states

### Maintainability Improvements
- **Separation of Concerns**: Clear component responsibilities
- **Testability**: Isolated unit testing for each concern
- **Extensibility**: Easy to add new transports
- **Debugging**: Clearer call stacks and component boundaries
- **Transport Hiding**: Implementation details hidden from service layer

### Feature Improvements
- **Fire-and-Forget Messaging**: New post() operations
- **Back-Channel Support**: Metadata with all messages
- **Better Zone Termination**: Cascading cleanup
- **Ad-hoc Messaging**: Service-level protocols
- **Transport Sharing**: Multiple service_proxies use single physical connection
- **Y-Topology Resolution**: Proper bidirectional proxy creation with known_direction_zone support

### Architectural Improvements
- **Corrected i_marshaller Hierarchy**: Service class (not interface) implements i_marshaller
- **Hidden Transport Abstraction**: Transport is implementation detail of service_proxy, not public interface
- **Service Sink as Transport-Internal**: No core service_sink class, transport-internal only
- **Proper Reference Counting**: Pass-through manages service_proxy lifecycles as required
- **Back-Channel Format Corrected**: vector<back_channel_entry> with type_id and payload, not HTTP headers
- **Bi-Modal Transport Support**: Proper handling of sync/async mode differences for all transport types

---

## Risks and Mitigation

### Technical Risks
- **Complexity**: New abstractions may add complexity - Mitigation: Clear interfaces and comprehensive documentation
- **Performance**: Additional indirections may impact performance - Mitigation: Profiling and optimization
- **Compatibility**: Breaking changes may affect users - Mitigation: Gradual migration path

### Implementation Risks
- **Testing**: Race conditions hard to test - Mitigation: Extensive stress testing and fuzzing
- **Integration**: Complex refactoring may introduce bugs - Mitigation: Comprehensive test coverage
- **Timeline**: Ambitious scope may extend timeline - Mitigation: Phased implementation with working subsets

---

## Conclusion

This proposal addresses the critical problems in the current service proxy and transport architecture through clean separation of concerns, proper abstractions, and race condition elimination. The solution maintains bi-modal compatibility while significantly improving performance, reliability, and maintainability. The phased implementation approach ensures steady progress with working subsets at each phase.

The key innovations are:
1. **Transport Abstraction** for clean separation of communication concerns
2. **Pass-Through Implementation** for dedicated routing and reference counting
3. **Predictable Lifecycle** eliminating reactive proxy creation
4. **Both-or-Neither Guarantee** for coordinated bidirectional operations
5. **Fire-and-Forget Messaging** for efficient cleanup and maintenance

This design provides a solid foundation for future growth while solving the current architecture's critical problems.