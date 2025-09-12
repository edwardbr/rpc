<!--
Copyright (c) 2025 Edward Boggis-Rolfe
All rights reserved.
-->

# RPC++ Reference Counting Architecture Documentation

## Overview

The RPC++ library implements a sophisticated multi-level reference counting system to manage the lifetime of distributed objects across different execution zones (processes, threads, enclaves). The architecture consists of three main components with their own reference counting mechanisms:

1. **Services** - Root containers managing zones
2. **Service Proxies** - Communication channels between zones  
3. **Object Proxies** - Handles to remote objects

## Architecture Hierarchy

```
Service (Zone Container)
├── other_zones: Map<{destination_zone, caller_zone}, weak_ptr<service_proxy>>
│   ├── Service Proxy (Communication Channel)
│   │   ├── proxies_: Map<object_id, weak_ptr<object_proxy>>
│   │   ├── lifetime_lock_count_: External reference counter
│   │   └── Object Proxies (Remote Object Handles)
│   │       ├── object_id: Remote object identifier
│   │       ├── service_proxy_: Back-reference to parent
│   │       └── inherited_reference_count_: Race condition handling
│   └── [Additional Service Proxies...]
└── [Local stubs and wrapped objects...]
```

## Zone Types and Communication Patterns

### Zone Hierarchy
- **HOST (Zone 1)**: Root parent zone
- **MAIN CHILD (Zone 2)**: Intermediate zone, child of HOST
- **EXAMPLE_ZONE (Zone 3)**: Target zone, child of MAIN CHILD

### Service Proxy Types

#### 1. Direct Service Proxies
```cpp
destination_zone_id == zone_id.as_destination()
```
- **Purpose**: Direct communication to adjacent zones
- **Characteristics**: 
  - Host their own object_proxies locally
  - Manage actual objects in their destination zone
  - Straightforward reference counting

#### 2. Routing Service Proxies  
```cpp
destination_zone_id != zone_id.as_destination()
```
- **Purpose**: Route calls through intermediate zones
- **Characteristics**:
  - Should NOT host object_proxies locally (routing only)
  - Forward calls to destination zones
  - Complex reference counting due to multi-hop routing

**Example**: HOST → EXAMPLE_ZONE communication routes through MAIN CHILD:
```
HOST (zone=1) → [routing proxy: 1→3, caller=1] → MAIN CHILD (zone=2) → EXAMPLE_ZONE (zone=3)
```

## Reference Counting Mechanisms

### 1. Service Proxy External Reference Counting

#### Purpose
- Keeps service_proxy alive while external entities hold references
- Prevents premature destruction during multi-step operations
- Coordinates cleanup when service_proxy becomes unused

#### Key Methods
```cpp
class service_proxy {
    std::atomic<int> lifetime_lock_count_ = 0;
    
    void add_external_ref();           // Increment reference count
    int release_external_ref();        // Decrement reference count  
    bool is_unused() const;           // Check if count == 0
};
```

#### Reference Sources

**1. Service Proxy Creation** (`inner_add_zone_proxy`)
```cpp
// service.cpp:1293
service_proxy->add_external_ref();  // Initial reference when added to other_zones map
```

**2. Object Operations** (`add_ref` method)
```cpp
// service.cpp:914 and service.cpp:1053  
destination->add_external_ref();    // Reference during add_ref operations
other_zone->add_external_ref();     // Reference for routing operations
```

**3. Object Proxy Creation** (`get_or_create_object_proxy`)
```cpp
// proxy.h:742
add_external_ref();                 // Reference while object_proxy exists
```

**4. Temporary Operations** (`try_cast`, `prepare_remote_input_interface`)
```cpp
// service.cpp:738
other_zone->add_external_ref();     // Temporary reference during operation
other_zone->release_external_ref(); // Immediately balanced
```

### 2. Object Proxy Lifecycle Management

#### Creation Process
```cpp
rpc::shared_ptr<object_proxy> get_or_create_object_proxy(object_id, rule, new_proxy_added) {
    // 1. Check if object_proxy already exists
    auto item = proxies_.find(object_id);
    
    // 2. Create new object_proxy if needed
    if (!op) {
        op = rpc::shared_ptr<object_proxy>(new object_proxy(object_id, self_ref));
        proxies_[object_id] = op;
        if (!new_proxy_added) {
            add_external_ref();  // Keep service_proxy alive
        }
    }
    
    // 3. Handle creation rules
    if (rule == RELEASE_IF_NOT_NEW && !is_new) {
        release_external_ref();  // Balance reference for existing proxy
    }
}
```

#### Destruction Process
```cpp
object_proxy::~object_proxy() {
    // 1. Calculate inherited references from race conditions
    int inherited_count = inherited_reference_count_.load();
    
    // 2. Notify service_proxy of destruction
    service_proxy_->on_object_proxy_released(object_id_, inherited_count);
    
    // 3. Clean up back-reference
    service_proxy_ = nullptr;
}
```

#### Service Proxy Cleanup
```cpp
void on_object_proxy_released(object_id, inherited_reference_count) {
    // 1. Remove from proxies_ map
    {
        std::lock_guard proxy_lock(insert_control_);
        proxies_.erase(object_id);
    }
    
    // 2. Release external reference
    inner_release_external_ref();  // Balance reference from object_proxy creation
    
    // 3. Handle inherited references from race conditions
    for (int i = 0; i < inherited_reference_count; i++) {
        release(version_, destination_zone_id_, object_id, caller_zone_id_);
        inner_release_external_ref();
    }
}
```

### 3. Service Lifecycle Management

#### Service Proxy Storage
```cpp
class service {
    // Map: {destination_zone, caller_zone} -> weak_ptr<service_proxy>
    std::unordered_map<service_zone_pair, rpc::weak_ptr<service_proxy>> other_zones;
};
```

#### Cleanup Verification
```cpp
bool service::check_is_empty() const {
    // Verify all service_proxies are properly cleaned up
    for (auto item : other_zones) {
        auto svcproxy = item.second.lock();
        if (svcproxy) {
            // FAILURE: Service proxy still alive with references
            // Should have reference_count == 0 and proxies_.empty()
        }
    }
}
```

## Current Problem Analysis

### The Bug: Reference Count Imbalance

**Problematic Service Proxy**: `zone=1, destination_zone=3, caller_zone=1` (HOST → EXAMPLE_ZONE routing)

#### Reference Timeline
```
1. add_external_ref() count=1  ← inner_add_zone_proxy (service proxy creation)
2. add_external_ref() count=2  ← add_ref method (first call)  
3. add_external_ref() count=3  ← add_ref method (second call)
4. add_external_ref() count=4  ← add_ref method (third call)

5. release_external_ref() count=3  ← get_or_create_object_proxy (RELEASE_IF_NOT_NEW)
6. release_external_ref() count=2  ← on_object_proxy_released (object_id=1)  
7. release_external_ref() count=1  ← on_object_proxy_released (object_id=2)

FINAL STATE: reference_count=1, proxies_count=0
```

#### Root Cause
- **3 calls to `add_ref`** each call `add_external_ref()`
- **Only 2 corresponding `release`** operations occur
- **1 unmatched `add_ref`** never gets its balancing `release_external_ref()`
- Service proxy cannot be cleaned up due to remaining reference

### Reference Counting Invariants (Expected Behavior)

#### Service Proxy Invariants
1. **Creation Reference**: `inner_add_zone_proxy` → balanced by service proxy removal
2. **Operation References**: `add_ref` → balanced by corresponding `release` 
3. **Object References**: `get_or_create_object_proxy` → balanced by `on_object_proxy_released`
4. **Temporary References**: `try_cast`, etc. → immediately balanced

#### Object Proxy Invariants  
1. **Service Proxy Reference**: Each object_proxy holds service_proxy alive
2. **Clean Destruction**: `on_object_proxy_released` must be called
3. **Map Cleanup**: Object must be removed from `proxies_` map
4. **Race Condition Handling**: Inherited references properly cleaned up

#### Service Invariants
1. **Empty Check**: `other_zones` map should be empty at service destruction
2. **Weak Reference Cleanup**: Dead service_proxies should be cleaned from map
3. **Reference Balance**: All external references should be released

## Reference Counting Best Practices

### 1. Always Balance References
Every `add_external_ref()` must have a corresponding `release_external_ref()`:
```cpp
// CORRECT: Temporary reference pattern
other_zone->add_external_ref();
auto result = other_zone->some_operation();
other_zone->release_external_ref();
return result;
```

### 2. Use RAII for Reference Management
```cpp
class scoped_service_proxy_ref {
    rpc::shared_ptr<service_proxy> proxy_;
public:
    scoped_service_proxy_ref(rpc::shared_ptr<service_proxy> p) : proxy_(p) {
        if (proxy_) proxy_->add_external_ref();
    }
    ~scoped_service_proxy_ref() {
        if (proxy_) proxy_->release_external_ref();
    }
};
```

### 3. Verify Reference Counts in Debug Builds
```cpp
#ifdef DEBUG
void verify_reference_balance() {
    RPC_ASSERT(lifetime_lock_count_ >= 0);
    if (is_unused() && !proxies_.empty()) {
        RPC_WARNING("WARNING: Service proxy unused but has object proxies");
    }
}
#endif
```

## Future Documentation Improvements

### 1. Reference Counting Patterns
- Document expected reference lifecycles for each operation type
- Provide reference counting debugging tools
- Add assertions to verify reference balance

### 2. Zone Communication Architecture  
- Document routing patterns and service proxy creation rules
- Clarify when routing vs direct service proxies are used
- Provide topology visualization tools

### 3. Error Detection and Recovery
- Implement reference leak detection
- Add diagnostic logging for reference imbalances  
- Provide cleanup mechanisms for orphaned references

### 4. Thread Safety and Race Conditions
- Document locking strategies and race condition prevention
- Explain inherited reference counting mechanism
- Provide guidelines for safe multi-threaded usage

### 5. Performance Considerations
- Document reference counting overhead
- Provide optimization strategies for high-frequency operations
- Explain weak_ptr usage patterns for cycle prevention

## Debugging Reference Count Issues

### Diagnostic Tools
1. **Reference Count Logging**: Track all `add_external_ref`/`release_external_ref` calls
2. **Service Proxy State Dumps**: Show reference counts and object proxy counts
3. **Zone Topology Visualization**: Understand routing patterns
4. **Lifecycle Tracking**: Monitor object proxy creation/destruction

### Common Patterns of Reference Count Bugs
1. **Missing Release**: `add_ref` called but no corresponding `release`
2. **Double Release**: `release_external_ref` called more than expected
3. **Race Conditions**: References manipulated without proper synchronization
4. **Routing Proxy Misuse**: Object proxies created in routing service proxies

This architecture provides robust distributed object management but requires careful attention to reference counting balance to prevent resource leaks and ensure clean shutdown.