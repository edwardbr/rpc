# Second Race Condition Analysis: "unable to find object in service proxy"

## Crash Details
- **Location**: `/home/edward/projects/rpc2/rpc/include/rpc/proxy.h:532`
- **Error Message**: "unable to find object in service proxy"  
- **Signal**: SIGABRT (assertion failure)
- **Function**: `rpc::service_proxy::on_object_proxy_released`

## Root Cause Analysis

### The Race Condition Scenario
The issue occurs in the `on_object_proxy_released` method:

```cpp
void on_object_proxy_released(object object_id)
{
    // Line 518-520: Lock and find object
    std::lock_guard l(insert_control_);
    auto item = proxies_.find(object_id);
    RPC_ASSERT(item != proxies_.end());  // ✓ PASSES - object found
    
    // Line 521: Try to lock weak_ptr
    if (item->second.lock() == nullptr)
    {
        // Expected path: weak_ptr is null, object is being destroyed
        proxies_.erase(item);
    }
    else
    {
        // ❌ PROBLEM PATH: weak_ptr still locks successfully
        // This should NOT happen if the object is being released
        std::string message("unable to find object in service proxy");
        printf("%s\n", message.c_str());
        RPC_ASSERT(false);  // ← CRASH HERE
    }
}
```

### Why This Race Condition Occurs

**Expected Behavior**:
1. `object_proxy` destructor calls `on_object_proxy_released`
2. The `weak_ptr` in `proxies_[object_id]` should be expired (nullptr when locked)
3. The entry gets erased from `proxies_`

**Actual Behavior (Race Condition)**:
1. Thread A: `object_proxy` destructor starts, calls `on_object_proxy_released`
2. Thread B: Another operation creates/accesses the same `object_proxy` (likely through `get_object_proxy`)
3. Thread A: Finds the object in `proxies_` but the `weak_ptr.lock()` succeeds because Thread B has a strong reference
4. Thread A: Hits the assertion failure because it expected the object to be dead

### Evidence from Crash Data
- **Lookup calls completed**: `lookup_enter_count: 15, lookup_exit_count: 15, Calls in progress: 0`
- **Timing**: Crash occurs during test cleanup phase
- **Call Stack**: Shows the crash in `on_object_proxy_released` (frame 8: offset +0x1901f2)

## Technical Analysis

### The `proxies_` Container
```cpp
std::unordered_map<object, rpc::weak_ptr<object_proxy>> proxies_;
```
- Stores `weak_ptr` to `object_proxy` instances
- Protected by `insert_control_` mutex
- Used for object lifetime management across zones

### Concurrent Access Patterns
1. **Creation Path** (`get_object_proxy`):
   ```cpp
   auto item = proxies_.find(object_id);
   if (item != proxies_.end())
       op = item->second.lock();  // Try to get existing
   if (op == nullptr) {
       op = rpc::shared_ptr<object_proxy>(new object_proxy(...));
       proxies_[object_id] = op;  // Store new weak_ptr
   }
   ```

2. **Destruction Path** (`on_object_proxy_released`):
   ```cpp
   auto item = proxies_.find(object_id);
   if (item->second.lock() == nullptr)  // Should be null during destruction
       proxies_.erase(item);
   else
       RPC_ASSERT(false);  // ← Race condition triggers here
   ```

### The Race Window
The race occurs between:
- **Destructor thread**: Calling `on_object_proxy_released`
- **Creation thread**: Calling `get_object_proxy` with the same `object_id`

Both operations are protected by the same mutex (`insert_control_`), but the timing allows:
1. Object starts destruction
2. Another thread recreates the same object before cleanup completes
3. Cleanup finds the "new" object instead of the expected null weak_ptr

## Reproduction Details

### Test Configuration
- **Test**: All `*multithreaded_two_zones*` tests
- **Command**: `./build/output/debug/rpc_test --gtest_filter=*multithreaded_two_zones* --gtest_repeat=20`
- **Success Rate**: Crash occurred on iteration 2 of 20
- **Pattern**: Inter-zone proxy creation/destruction during multithreaded operations

### Call Stack Analysis
```
Frame 8: ./build/output/debug/rpc_test(+0x1901f2) [address]  # on_object_proxy_released
Frame 9: ./build/output/debug/rpc_test(+0x18f6db) [address]  # object_proxy destructor
Frame 10: ./build/output/debug/rpc_test(+0x18f772) [address] # shared_ptr cleanup
```

## Implications

### Root Cause: Object ID Reuse Race
The fundamental issue is that object IDs can be reused while the previous object with the same ID is still in the destruction process. This creates a window where:

1. **Old object**: In destruction, expects to find null weak_ptr
2. **New object**: Created with same ID, creates valid weak_ptr
3. **Collision**: Old object's cleanup finds new object's valid weak_ptr

### Affected Scenarios
- Multi-zone RPC operations
- Rapid create/destroy cycles of proxy objects
- Concurrent proxy access during test teardown
- Object ID reuse patterns

## Recommended Fixes

### 1. Reference Counting Coordination
Ensure that `on_object_proxy_released` only runs after all references are truly gone:
```cpp
void on_object_proxy_released(object object_id)
{
    std::lock_guard l(insert_control_);
    auto item = proxies_.find(object_id);
    if (item != proxies_.end()) {
        auto locked = item->second.lock();
        if (!locked || locked.use_count() == 1) {
            // Safe to cleanup
            proxies_.erase(item);
        }
        // Else: Another thread has reference, defer cleanup
    }
}
```

### 2. Unique Object ID Generation
Prevent object ID reuse during overlapping lifetimes:
```cpp
// Use combination of zone_id + timestamp + counter
object generate_unique_object_id() {
    static std::atomic<uint64_t> counter{0};
    return {zone_id, current_time_ns(), counter.fetch_add(1)};
}
```

### 3. Deferred Cleanup Queue
Queue cleanup operations to avoid races:
```cpp
void on_object_proxy_released(object object_id)
{
    cleanup_queue_.push({object_id, std::chrono::steady_clock::now()});
    // Process queue periodically when safe
}
```

## Next Steps
1. Implement object ID uniqueness improvements
2. Add reference counting safeguards  
3. Test fix with aggressive multithreaded scenarios
4. Verify no regression in single-threaded performance