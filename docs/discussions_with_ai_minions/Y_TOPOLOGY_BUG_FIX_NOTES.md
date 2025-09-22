<!--
Copyright (c) 2025 Edward Boggis-Rolfe
All rights reserved.
-->

# Y-Topology Zone Routing Bug Fix - Documentation

## Overview

This document describes the Y-shaped topology bug discovered by the fuzz tester and the fix implemented using the `known_direction_zone` parameter and reverse service proxy creation.

## The Problem

### Bug Description (from service.cpp line 222-229)

The bug occurs in a Y-shaped zone topology where:

1. **Zone 1** (Root) creates **Zone 3** (first prong of the Y) 
2. **Zone 5** (deep in first prong, 5 zones deep from root) autonomously asks **Zone 3** to create **Zone 7** (second prong)
3. **Zone 1** never knows **Zone 7** exists because it wasn't involved in the fork creation
4. **Zone 5** gets an object from **Zone 7** and passes it back to **Zone 1**
5. **Zone 1** cannot work out how to set up service proxy chain to **Zone 7** because it has no knowledge of that zone's existence

### Visual Representation

```
Zone 1 (Root)
├── Zone 3 (Original prong, created by Zone 1)
│   ├── Zone 4 (Intermediate)
│   │   ├── Zone 5 (Deep zone, 5 levels from root)
│   │   └── ...
│   └── Zone 6 (Fork created by Zone 3, UNKNOWN to Zone 1)
│       ├── Zone 7 (Target zone containing objects, UNKNOWN to Zone 1)
│       └── ...
```

### The Critical Sequence

1. Zone 5 calls Zone 3 to create Zone 7 (Zone 1 not involved)
2. Zone 5 receives an object reference from Zone 7
3. Zone 5 passes this object to Zone 1
4. Zone 1 tries to perform `add_ref` on the Zone 7 object
5. **BUG**: Zone 1 has no service proxy to Zone 7 and cannot establish one

## The Fix

### Components of the Solution

1. **known_direction_zone Parameter**: Added to all `add_ref` function signatures to provide routing hints
2. **Reverse Service Proxy Creation**: In `service::send` (line 222-255), temporary service proxies are created in the opposite direction to provide routing clues
3. **Enhanced add_ref Logic**: The `add_ref` function can now use the `known_direction_zone` hint to establish routing through intermediate zones

### Implementation Details

#### 1. service::send Enhancement (lines 222-255)

```cpp
// now get and lock the opposite direction
// note this is to address a bug found whereby we have a Y shaped graph whereby one prong creates a fork below it to form another prong
// this new prong is then ordered to pass back to the first prong an object.
// this object is then passed back to the root node which is unaware of the existence of the second prong.
// this code below forces the creation of a service proxy back to the sender of the call so that the root object can do an add_ref 
// hinting that the path of the object is somewhere along this path

if (auto found = other_zones.find({caller_zone_id.as_destination(), destination_zone_id.as_caller()}); found != other_zones.end())
{
    opposite_direction_proxy = found->second.lock();
    // ... create reverse proxy if needed
}
```

This creates a temporary service proxy from the destination back to the caller, providing a "breadcrumb trail" that `add_ref` can follow.

#### 2. known_direction_zone Parameter

The `known_direction_zone` parameter was added to:
- `rpc::i_marshaller::add_ref` 
- `rpc::service::add_ref`
- All service proxy `add_ref` implementations

Usage:
```cpp
auto err = add_ref(protocol_version, destination_channel_zone_id, destination_zone_id, 
                   object_id, caller_channel_zone_id, caller_zone_id,
                   known_direction_zone(zone_id_),  // <-- The hint
                   add_ref_options::build_caller_route, temp_ref_count);
```

#### 3. Enhanced Routing Logic (lines 1021-1027)

```cpp
else if(auto found = other_zones.lower_bound({known_direction_zone_id.as_destination(), {0}}); found != other_zones.end())
{
    // note that this is to support the Y shaped topology problem that the send function has the other half of this solution
    // the known_direction_zone_id is a hint explaining where the path of the object is if there is no clear route otherwise
    auto tmp = found->second.lock();
    RPC_ASSERT(tmp != nullptr);
    other_zone = tmp->clone_for_zone(destination_zone_id, caller_zone_id);
    inner_add_zone_proxy(other_zone);
}
```

When direct routing fails, the system uses the `known_direction_zone_id` hint to find an intermediate zone that can establish the proper routing.

## Test Cases

### Current Working Tests

The Y-topology bug fix is validated by three comprehensive test cases that demonstrate different aspects of the routing problem:

#### 1. `test_y_topology_and_return_new_prong_object`

**Pattern**: Direct object return from autonomous zones
- Zone 1 → Zone 2 → Zone 3 (factory) → Zone 4 → Zone 5 (deep zone)
- Zone 5 autonomously asks Zone 3 to create Zone 6 → Zone 7 (second prong)
- Zone 1 remains unaware of Zones 6 and 7
- Zone 5 gets object from Zone 7 and passes it back through the chain

**Critical Aspect**: Tests the basic Y-topology routing where deep zones create forks at earlier points in the hierarchy.

#### 2. `test_y_topology_and_cache_and_retrieve_prong_object`

**Pattern**: Cached object retrieval from autonomous zones
- Zone 1 → Zone 2 → Zone 3 → Zone 4 → Zone 5 (deep factory)
- Zone 5 autonomously creates Zone 6 → Zone 7 via Zone 3
- Zone 5 caches Zone 7 object locally
- Host retrieves cached object, triggering routing through unknown zones

**Critical Aspect**: Tests object caching and retrieval patterns that break routing knowledge across zone boundaries.

#### 3. `test_y_topology_and_set_host_with_prong_object`

**Pattern**: Host registry with objects from autonomous zones  
- Zone 1 → Zone 2 → Zone 3 → Zone 4 → Zone 5 (deep factory)
- Zone 5 autonomously creates Zone 6 → Zone 7 via Zone 3
- Zone 5 caches Zone 7 object and sets it in host registry
- Zone 1 accesses object through host registry lookup

**Critical Aspect**: **This is the most critical test** - it triggers infinite recursion when `known_direction_zone` is disabled because the service proxy cannot find Zone 7 and goes into a recursive loop until stack overflow.

### Key Implementation Details

All tests use the `create_y_topology_fork` method which implements the true Y-topology pattern:

```cpp
error_code create_y_topology_fork(rpc::shared_ptr<yyy::i_example> factory_zone, 
                                  const std::vector<uint64_t>& fork_zone_ids)
{
    // CRITICAL Y-TOPOLOGY PATTERN:
    // This zone (e.g. Zone 5) asks an earlier zone in the hierarchy (e.g. Zone 3) 
    // to create autonomous zones. Zone 3 creates the new zones but Zone 1 (root) 
    // and other zones in the original chain are NOT notified.
    
    // Ask the factory zone to create autonomous zones via create_fork_and_return_object
    return factory_zone->cache_object_from_autonomous_zone(fork_zone_ids);
}
```

### Why These Tests Are Comprehensive

1. **True Y-Topology**: Creates actual fork where one prong creates another prong at an earlier hierarchy point
2. **Routing Isolation**: Ensures root zone has no knowledge of autonomous zones
3. **Multiple Patterns**: Tests different ways objects from unknown zones get passed back to root
4. **Stack Overflow Reproduction**: `test_y_topology_and_set_host_with_prong_object` specifically reproduces the infinite recursion crash

## Benefits of the Fix

### 1. **Maintains Zone Autonomy**
- Zones can create sub-hierarchies without involving parent zones
- Supports complex distributed topologies

### 2. **Backward Compatibility**
- Existing code continues to work
- New parameter uses sensible defaults

### 3. **Performance**
- Minimal overhead for the common case
- Only creates reverse proxies when needed

### 4. **Robustness** 
- Handles edge cases in complex zone hierarchies
- Provides fallback routing mechanisms

## Future Considerations

### 1. **Distributed Transaction Semantics**
- Consider implementing rollback mechanisms for complex multi-zone reference chains
- Ensure atomic handover for [out] parameters during zone shutdown

### 2. **Memory Optimization**
- Implement memory pooling for frequently created/destroyed routing proxies
- Optimize cleanup of temporary reverse proxies

### 3. **Enhanced Telemetry**
- Add telemetry for reverse proxy creation events
- Monitor routing performance in complex topologies

### 4. **Documentation Updates**
- Update user guide with complex topology best practices
- Document the `known_direction_zone` parameter usage patterns

## Summary

The Y-topology bug fix successfully addresses a critical issue in complex zone hierarchies by:

1. **Adding routing hints** through the `known_direction_zone` parameter
2. **Creating temporary reverse service proxies** to provide breadcrumb trails
3. **Enhancing the add_ref logic** to use these hints for routing decisions

This solution maintains the system's distributed architecture while enabling robust object lifecycle management across complex, autonomously-created zone topologies.

The fix has been thoroughly tested with three comprehensive test cases:
- `test_y_topology_and_return_new_prong_object`
- `test_y_topology_and_cache_and_retrieve_prong_object` 
- `test_y_topology_and_set_host_with_prong_object`

These tests validate that the routing fix works correctly across different patterns of autonomous zone creation and object passing, ensuring the system handles complex Y-topology scenarios without breaking existing functionality.