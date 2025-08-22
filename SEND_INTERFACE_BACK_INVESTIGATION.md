# Send Interface Back Test Investigation Report

## Problem Summary
The `send_interface_back` test is failing due to a service_proxy not being properly cleaned up, causing an assertion failure in the service destructor. The problematic service_proxy has zone_id=1, caller_zone_id=1, destination_zone_id=3, destination_channel_zone_id=2.

## Investigation Process

### Key Discovery: Different Pattern from Previous Issue
Unlike the `bounce_baz_between_two_interfaces` test where the problematic service_proxy had NO object_proxies, this service_proxy DOES have object_proxies created:
- `object_id = 1` 
- `object_id = 2`

### Evidence from Test Run

#### Service Proxy Creation and Usage
```
[INFO] inner_add_zone_proxy service zone: 1 destination_zone=3, caller_zone=1
[INFO] get_or_create_object_proxy service zone: 1 destination_zone=3, caller_zone=1, object_id = 1
[INFO] get_or_create_object_proxy service zone: 1 destination_zone=3, caller_zone=1, object_id = 2
```

#### Final Error Message
```
[INFO] service proxy zone_id 1, caller_zone_id 1, destination_zone_id 3, destination_channel_zone_id 2 has not been released in the service suspected unclean shutdown
```

#### Key Observation: Missing on_object_proxy_released Calls
**CRITICAL FINDING**: There are NO `on_object_proxy_released` calls for the problematic service_proxy (zone_id=1, destination_zone_id=3, caller_zone_id=1) in the entire test output.

We can see `on_object_proxy_released` calls for other service_proxies:
- `on_object_proxy_released service zone: 1 destination_zone=2, caller_zone=1, object_id = 1`
- `on_object_proxy_released service zone: 2 destination_zone=1, caller_zone=2, object_id = 1`

But NO calls for the problematic one despite having object_proxies created.

## Root Cause Analysis

### Problem Pattern Identified
1. **Service_proxy is created**: `inner_add_zone_proxy service zone: 1 destination_zone=3, caller_zone=1`
2. **Object_proxies are created**: Two object_proxies (object_id=1, object_id=2) are added to this service_proxy
3. **Object_proxies are destroyed**: The object_proxies get destroyed (likely during test teardown)
4. **Missing cleanup**: `on_object_proxy_released` is NOT called for this specific service_proxy
5. **Service destructor fails**: The service_proxy remains in the service's `other_zones` map

### Technical Analysis

#### Service Proxy Characteristics
- **Zone ID**: 1 (HOST zone)
- **Caller Zone ID**: 1 (Same as zone ID - this is significant)
- **Destination Zone ID**: 3 (EXAMPLE_ZONE)
- **Destination Channel Zone ID**: 2 (MAIN CHILD zone - routing through this zone)

#### Routing Pattern
This is a routing service_proxy where:
- HOST zone (1) needs to communicate with EXAMPLE_ZONE (3)
- Communication is routed through MAIN CHILD zone (2)
- The caller is also HOST zone (1) - same zone as the service_proxy location

### Comparison with Working Service Proxies

#### Working Example 1: zone=1, destination=2, caller=1
```
[INFO] on_object_proxy_released service zone: 1 destination_zone=2, caller_zone=1, object_id = 1
```
This service_proxy gets proper cleanup.

#### Working Example 2: zone=2, destination=1, caller=2  
```
[INFO] on_object_proxy_released service zone: 2 destination_zone=1, caller_zone=2, object_id = 1
```
This service_proxy also gets proper cleanup.

#### Broken Example: zone=1, destination=3, caller=1
- Object_proxies created but `on_object_proxy_released` NEVER called
- Service_proxy remains in `other_zones` map
- Causes service destructor assertion failure

## Hypothesis: Object Proxy Destruction Path Issue

### Likely Cause
The object_proxies associated with this service_proxy are being destroyed through a different code path that doesn't trigger `on_object_proxy_released`. This could be due to:

1. **Reference Counting Issue**: The object_proxies might have incorrect reference counts preventing normal destruction
2. **Routing-Specific Bug**: Special handling for multi-hop routing (HOST->MAIN CHILD->EXAMPLE_ZONE) has a bug
3. **Same-Zone Caller Issue**: When caller_zone_id equals zone_id, there might be special logic that bypasses cleanup
4. **Race Condition**: Object_proxies destroyed during service shutdown before proper cleanup can occur

### Code Areas to Investigate

#### 1. Object Proxy Destructor Path
File: `/home/edward/projects/rpc2/rpc/src/proxy.cpp:18-38`
- Verify `on_object_proxy_released` is always called from `object_proxy::~object_proxy()`
- Check if there are early return paths that skip cleanup

#### 2. Service Proxy Routing Logic  
File: `/home/edward/projects/rpc2/rpc/include/rpc/proxy.h`
- Investigate routing service_proxy creation with same caller/zone IDs
- Check if multi-hop routing affects object_proxy lifecycle

#### 3. Zone Cleanup Ordering
File: `/home/edward/projects/rpc2/rpc/src/service.cpp`
- Verify service destruction order doesn't interfere with object_proxy cleanup
- Check if EXAMPLE_ZONE (3) destruction affects HOST zone (1) proxies

## Next Steps for Resolution

### 1. Immediate Investigation
- Add detailed logging to `object_proxy::~object_proxy()` to track destruction
- Verify all object_proxies for problematic service_proxy are actually destroyed
- Check if service_proxy gets prematurely destroyed before object_proxy cleanup

### 2. Routing-Specific Analysis
- Examine multi-hop routing logic (HOST->MAIN CHILD->EXAMPLE_ZONE)
- Investigate same-zone caller scenarios (caller_zone_id == zone_id)
- Review reference counting for routing service_proxies

### 3. Race Condition Testing
- Add timing logs to identify destruction order
- Test if service shutdown sequence affects object_proxy cleanup
- Verify thread safety in multi-zone scenarios

## Technical Context

### RPC++ Multi-Zone Architecture
- **HOST (1)**: Parent zone hosting main service
- **MAIN CHILD (2)**: Intermediate zone for routing
- **EXAMPLE_ZONE (3)**: Target zone with objects being accessed
- Routing pattern: HOST → MAIN CHILD → EXAMPLE_ZONE

### Service Proxy Types
1. **Direct Service Proxies**: Direct communication between zones
2. **Routing Service Proxies**: Multi-hop communication through intermediate zones
3. **Same-Zone Proxies**: Caller and service in same zone (special case)

### Expected Cleanup Flow
1. Object_proxy destructor called
2. `service_proxy::on_object_proxy_released()` triggered  
3. When all object_proxies released, service_proxy becomes eligible for cleanup
4. Service_proxy removed from `other_zones` map
5. Service destructor succeeds with empty `other_zones`

### Actual Broken Flow
1. Object_proxy destructor called (inferred)
2. **`service_proxy::on_object_proxy_released()` NEVER called** ← BUG HERE
3. Service_proxy remains in `other_zones` map
4. Service destructor fails assertion

## Comparison with Fixed Issue

### Previously Fixed: bounce_baz_between_two_zones
- **Problem**: Routing service_proxy with NO object_proxies never cleaned up
- **Root Cause**: Double external reference counting
- **Solution**: Fixed external reference logic in `service::add_ref()` and cleanup in `service::release()`

### Current Issue: send_interface_back  
- **Problem**: Service_proxy WITH object_proxies never triggers cleanup
- **Root Cause**: Object_proxy destruction doesn't call `on_object_proxy_released`
- **Solution**: Need to identify why object_proxy cleanup path is broken

This represents a different class of bug in the object_proxy lifecycle management rather than service_proxy reference counting.