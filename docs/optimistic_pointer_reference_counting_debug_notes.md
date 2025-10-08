# Optimistic Pointer Reference Counting Debug Notes

## Date: 2025-10-08

## Problem Statement
Test `type_test/2.optimistic_ptr_remote_shared_semantics_test` is failing. The test expects that after destroying a `rpc::shared_ptr` (leaving only an `rpc::optimistic_ptr`), calling through the optimistic_ptr should return `OBJECT_GONE` because the remote object has been deleted.

### Multi-Zone Reference Counting Semantics
**IMPORTANT**: Remote stub deletion is based on **aggregate shared reference count across ALL zones**:
- A stub is deleted when its `shared_count` reaches 0 (from ALL zones combined)
- Optimistic references do NOT keep stubs alive (weak semantics)
- If Zone A releases all its shared_ptrs but Zone B still holds shared_ptrs, the stub remains alive
- In the test scenario with only 2 zones (root + child), there should be no other zone holding references

## Architecture Understanding (Clarified)

### object_proxy
- **Purpose**: Aggregates ALL interface proxies pointing to the same remote object
- **Responsibility**: Pool multiple interface types and only send sp_add_ref/sp_release when aggregate counts transition 0→1 or 1→0
- `shared_count_` and `optimistic_count_` track TOTAL references across all interface types for this object
- Manages "fake multiple inheritance" so different interface proxies can share the same object_proxy
- `dynamic_pointer_cast` converts between multiple interface proxies on the same object_proxy

### control_block (one per rpc::shared_ptr/optimistic_ptr)
- **Purpose**: Manages references for a SPECIFIC interface type
- Multiple control blocks can exist for the same object_proxy (one per interface type)
- `shared_owners` and `optimistic_owners` track references for a specific interface
- Should call `object_proxy::add_ref()` / `object_proxy::release()` to aggregate counts

### Remote Service (Stub Zone)
- Maintains `object_stub` with TWO reference counts: `shared_count` and `optimistic_count`
- **Deletion Rule**: Stub is deleted when `shared_count` reaches 0, **regardless of `optimistic_count` value**
- Optimistic references don't keep stub alive (weak semantics)
- **Multi-Zone Aggregation**: `shared_count` represents the total across ALL zones that reference this stub
- **Implementation**: See `/rpc/src/service.cpp` line 1304: `if (!is_optimistic && !count)` triggers stub deletion

## Current Implementation Issues

### Issue 1: Control Block Constructor and Reference Synchronization
**Problem**: Control block constructor is synchronous but needs to establish remote reference

**Current Approach**:
```cpp
// In control_block_base constructor
if (!is_local) {
    auto obj_proxy = ptr->get_object_proxy();
    if (obj_proxy) {
        object_proxy_add_ref_shared(obj_proxy);  // Synchronous increment
    }
}
```

**Status**: Control block constructor calls `add_ref_shared()` to initialize counter to 1, matching the control block's initial state (shared_owners will be incremented to 1 immediately after construction).

### Issue 2: Factory Object Return Path
**Problem**: Objects returned from factories don't establish proper remote references

**Flow for `create_baz()` returning a new object**:
1. Remote factory creates object, stub has refcount=1
2. Interface descriptor returned to caller
3. `proxy_bind_out_param` called with `RELEASE_IF_NOT_NEW`
4. If object_proxy already exists: sends sp_release to balance factory's reference
5. If object_proxy is new: does NOT send sp_add_ref
6. Control block constructor increments `shared_count_` to 1 locally
7. **BUG**: No sp_add_ref sent to match the local increment
8. When shared_ptr destroyed: sp_release sent, but remote stub has no matching reference!

**Code Location**: `/rpc/include/rpc/internal/bindings.h` line 182:
```cpp
std::shared_ptr<rpc::object_proxy> op = CO_AWAIT service_proxy->get_or_create_object_proxy(
    encap.object_id, service_proxy::object_proxy_creation_rule::RELEASE_IF_NOT_NEW, false, {}, false);
```

### Issue 3: Double Release Prevention
**Status**: Fixed by removing notification from `release()` method

**Change Made**:
```cpp
// OLD: Called on_object_proxy_released() on 1→0 transitions
// NEW: Only call from destructor, control block handles sp_release via control_block_call_release()
```

### Issue 4: Destructor Inherited References
**Status**: Fixed by passing 0 for inherited counts

**Change Made**:
```cpp
// In object_proxy destructor
service_proxy->on_object_proxy_released(object_id_, 0, 0, true);
// Pass 0,0 because control block already released all references
```

## Test Results Summary

### Current State (After Fixes)
```
[DEBUG] make_optimistic: BEFORE - control_block(shared=1, optimistic=0), object_proxy(inherited_shared=1, inherited_optimistic=0) ✓
[DEBUG] make_optimistic: AFTER - control_block(shared=1, optimistic=1), object_proxy(inherited_shared=1, inherited_optimistic=1) ✓
[INFO] callback 42  ✓ First call succeeds
[DEBUG] object_proxy::release: shared reference ... (shared=0, optimistic=1) ✓
[INFO] callback 42  ✗ Second call succeeds but SHOULD return OBJECT_GONE
```

**Reference counts are correct, but remote object is NOT being deleted!**

## Root Cause Analysis

### Test Zone Topology
The failing test `optimistic_ptr_remote_shared_semantics_test` runs with `inproc_setup`:
- **Zone 1 (root_service)**: Host zone with `example` proxy pointing to Zone 2
- **Zone 2 (child_service)**: Example zone with `example` stub (local object)

### Object Creation Flow in Test
```cpp
auto example = lib.get_example();           // Zone 1's proxy → Zone 2 stub
await example->create_foo(f);                // Zone 2 creates foo IN ZONE 2, returns to Zone 1
await f->create_baz_interface(baz);          // Zone 2 creates baz IN ZONE 2, returns to Zone 1
f.reset();                                   // Zone 1 releases foo proxy → Zone 2 deletes foo stub
make_optimistic(baz, opt_baz);               // Creates optimistic_ptr to baz
baz.reset();                                 // Zone 1 releases baz proxy → Zone 2 SHOULD delete baz stub
await opt_baz->callback(42);                 // EXPECTED: OBJECT_GONE, ACTUAL: succeeds (BUG!)
```

### The Bug: Factory Object Return Path

The fundamental issue is with the **factory object return path**:

1. When `create_baz()` is called on the remote service (Zone 2):
   - Creates object with stub `shared_count=1` (factory's initial reference)
   - Returns interface_descriptor to caller (Zone 1)

2. When `proxy_bind_out_param` receives the descriptor in Zone 1:
   - Calls `get_or_create_object_proxy(RELEASE_IF_NOT_NEW)` (see `/rpc/include/rpc/internal/bindings.h` line 182)
   - For NEW object_proxy: **does nothing** (assumes factory reference is inherited)
   - For EXISTING object_proxy: sends sp_release (to balance factory's reference)

3. When control block is constructed in Zone 1:
   - Calls `object_proxy::add_ref_shared()` → `object_proxy.shared_count_=1` (local tracking)
   - **NO sp_add_ref sent to remote service!** (assumes factory reference covers this)

4. When `make_optimistic(baz, opt_baz)` is called in Zone 1:
   - Calls `control_block::try_increment_optimistic()` (see `/rpc/include/rpc/internal/remote_pointer.h` line 503)
   - Detects 0→1 transition for optimistic count
   - **Sends sp_add_ref(optimistic=true) to Zone 2**
   - Zone 2 stub now has: `shared_count=1, optimistic_count=1`

5. When `baz.reset()` is called in Zone 1:
   - Calls `control_block::decrement_shared_and_dispose_if_zero()`
   - Sends sp_release(optimistic=false) to Zone 2
   - Zone 2 stub now has: `shared_count=0, optimistic_count=1`
   - **Stub should be deleted** (see `/rpc/src/service.cpp` line 1304: `if (!is_optimistic && !count)`)

6. **BUT**: Zone 2 stub is NOT being deleted!

### Why the Stub Isn't Deleted

The stub `shared_count` never actually reaches 0 because:
- Factory creates stub with `shared_count=1`
- Zone 1 proxy assumes this reference is "inherited" (no sp_add_ref sent)
- Zone 1 proxy sends ONE sp_release when `baz.reset()` is called
- Zone 2 stub: `shared_count` goes from 1→0
- **Expected**: Stub deleted
- **Actual**: Stub remains alive (indicating `shared_count` didn't reach 0)

**This suggests the factory reference is NOT being properly balanced, OR there's an additional reference being added somewhere.**

## Why sp_add_ref is Not Being Sent

### Case 1: Objects Created via `create_baz()` Factory
- Uses `proxy_bind_out_param` with `RELEASE_IF_NOT_NEW`
- If object_proxy is NEW: no sp_add_ref sent
- Assumption: Factory already added reference, so local tracking starts at 1
- **But**: Control block's `add_ref_shared()` doesn't trigger sp_add_ref!

### Case 2: Objects Retrieved via `get_or_create_object_proxy(ADD_REF_IF_NEW)`
- When object_proxy is NEW: sp_add_ref IS sent
- **But**: We removed the local counter increment after sp_add_ref
- So remote has refcount=1 but local `shared_count_=0`
- Then control block constructor increments to `shared_count_=1`
- **Result**: Counts match, this case should work!

## Attempted Solutions

### Attempt 1: Increment in get_or_create_object_proxy
```cpp
// After sp_add_ref succeeds
if (is_optimistic) {
    op->add_ref_optimistic();
} else {
    op->add_ref_shared();
}
```
**Problem**: Control block also calls release(), causing double-release (shared=-1)

### Attempt 2: Don't call add_ref_shared in constructor
**Problem**: Counts don't match control block state, leads to negative counts

### Attempt 3: Current approach - constructor calls add_ref_shared
**Problem**: Works for normal case, but breaks for factory return path (no sp_add_ref sent)

## Potential Solutions

### Solution A: Make add_ref_shared Check for 0→1 Transition
Modify `object_proxy::add_ref_shared()` to detect when `prev_count == 0` and queue an async sp_add_ref:
```cpp
void object_proxy::add_ref_shared() {
    int prev = shared_count_.fetch_add(1, std::memory_order_relaxed);
    if (prev == 0) {
        // Queue async sp_add_ref to be sent
        // But how? Constructor can't wait...
    }
}
```
**Challenge**: Constructor is synchronous, can't wait for async operation

### Solution B: Lazy Reference Establishment
Don't send sp_add_ref in constructor, send it on first method call:
```cpp
// In interface_proxy::method_call()
if (!remote_ref_established_) {
    CO_AWAIT establish_remote_reference();
    remote_ref_established_ = true;
}
```
**Challenge**: Complex state management, affects all method calls

### Solution C: Fix proxy_bind_out_param Path
Change `RELEASE_IF_NOT_NEW` behavior for new object_proxy:
```cpp
if (!is_new && rule == object_proxy_creation_rule::RELEASE_IF_NOT_NEW) {
    // Only send sp_release if object_proxy already existed
    // For NEW object_proxy, the control block will manage the reference
}
```
**But**: Then for NEW object_proxy, need to send sp_add_ref somewhere...

### Solution D: Deferred sp_add_ref in Control Block
Have control block send sp_add_ref lazily on first increment:
```cpp
void increment_shared() {
    long prev = shared_owners.fetch_add(1, std::memory_order_relaxed);
    if (prev == 0 && !is_local) {
        // This is the 0→1 transition, need to send sp_add_ref
        // But this is SYNCHRONOUS!
    }
}
```
**Challenge**: Same async problem

### Solution E: Constructor Callback Pattern
```cpp
control_block_base(void* obj_ptr) {
    // ... existing code ...
    if (!is_local && obj_proxy) {
        obj_proxy->add_ref_shared();
        // Register callback to send sp_add_ref later
        obj_proxy->schedule_async_add_ref();
    }
}
```
**Challenge**: Race conditions, when does async add_ref actually execute?

## Recommended Next Steps

1. **Understand Factory Reference Semantics**:
   - When factory returns object, what's the reference count on the stub?
   - Should proxy_bind_out_param send sp_release for NEW object_proxy?
   - Need to trace through a factory call with full telemetry

2. **Consider Architectural Change**:
   - Perhaps control blocks should NOT manage remote references directly
   - Instead, object_proxy should track "how many control blocks exist"
   - Send sp_add_ref when first control block created
   - Send sp_release when last control block destroyed

3. **Alternative: Synchronous Initial Reference**:
   - For objects returned from factories, assume reference already exists
   - Don't send sp_release in proxy_bind_out_param
   - Let control block manage from there
   - May need flag to distinguish "factory-returned" vs "explicitly-fetched" objects

## Files Modified

### `/rpc/include/rpc/internal/remote_pointer.h`
- Line 318-341: Control block constructor calls `object_proxy_add_ref_shared()`
- Line 2258-2270: Fixed validation in make_optimistic (check counts match, not both zero/non-zero)
- Line 2300-2312: Fixed validation after make_optimistic

### `/rpc/include/rpc/internal/object_proxy.h`
- Line 35-38: Moved `shared_count_` and `optimistic_count_` to private
- Line 51-53: Added public getters for counts
- Line 66-69: Added `add_ref_shared()` and `add_ref_optimistic()` methods

### `/rpc/src/object_proxy.cpp`
- Line 117-126: Removed notification from release() on 1→0 transitions
- Line 140-187: Updated destructor to pass inherited_shared=0, inherited_optimistic=0
- Line 246-250: Added `object_proxy_add_ref_shared()` helper function

### `/rpc/src/service_proxy.cpp`
- Reverted: Removed increments after sp_add_ref (was causing double-release)
- Reverted: Removed decrements in RELEASE_IF_NOT_NEW (not needed)

## Key Insights

1. **Multiple Control Blocks Per object_proxy**: This is the intended design for supporting multiple interface types
2. **Aggregate Reference Counting**: object_proxy must aggregate across all control blocks before sending remote calls
3. **Async Constructor Problem**: Core challenge is that control block construction is synchronous but establishing remote reference is async
4. **Factory Path is Different**: Objects returned from factories follow different path than explicitly fetched objects

## Test Expectations vs Actual Behavior

The test `optimistic_ptr_remote_shared_semantics_test` validates optimistic pointer weak semantics:

**Test Sequence**:
1. Creates object via factory (`create_baz_interface()`)
2. Creates optimistic_ptr from shared_ptr (`make_optimistic(baz, opt_baz)`)
3. First call through optimistic_ptr → **Expected: SUCCESS** ✓ (object alive)
4. Destroys shared_ptr (`baz.reset()`)
5. Second call through optimistic_ptr → **Expected: OBJECT_GONE** ✗ (stub should be deleted)

**Expected Behavior**:
- When `baz.reset()` is called, Zone 1 releases its only shared reference
- Zone 2 stub `shared_count` should reach 0
- Stub should be deleted (even though `optimistic_count=1`)
- Second call returns `OBJECT_GONE` (weak semantics)

**Actual Behavior**:
- Zone 2 stub is NOT deleted after `baz.reset()`
- Second call succeeds (stub still alive)
- This indicates `shared_count` is not reaching 0 as expected

**Zone Confirmation**:
- Only 2 zones exist in test (root + child)
- No other zones could be holding references
- The `foo` object was already released (line 414 in test)
- Therefore, the issue is NOT multi-zone reference holding, but rather **incorrect reference count tracking**

## Investigation Summary (2025-10-08 Update)

### Key Finding: make_optimistic Adds Remote Reference
Analysis of `make_optimistic` implementation reveals it **DOES send sp_add_ref** to the remote stub:
- Location: `/rpc/include/rpc/internal/remote_pointer.h` line 2269
- When converting `shared_ptr → optimistic_ptr`, detects 0→1 transition for optimistic count
- Calls `CO_AWAIT cb->increment_optimistic()` which sends `sp_add_ref(optimistic=true)`
- This means Zone 2 stub has **both** shared and optimistic references after `make_optimistic`

### Reference Count Timeline (Revised Understanding)
```
1. create_baz_interface():  Zone 2 stub: shared=1, optimistic=0
2. Zone 1 gets proxy:       Zone 2 stub: shared=1, optimistic=0  (factory ref inherited)
3. make_optimistic():       Zone 2 stub: shared=1, optimistic=1  (optimistic ref added!)
4. baz.reset():             Zone 2 stub: shared=0, optimistic=1  (shared ref released)
5. EXPECTED: stub deleted   ACTUAL: stub still alive (WHY?)
```

### The Mystery
According to `/rpc/src/service.cpp` line 1304, stub should be deleted when `shared_count` reaches 0:
```cpp
if (!is_optimistic && !count) {
    stubs.erase(stub->get_id());  // Delete stub
}
```

**But the stub is NOT being deleted!** This means either:
1. The `shared_count` is not actually reaching 0 (additional reference somewhere)
2. The deletion code is not executing (code path issue)
3. The stub is being recreated after deletion (unlikely)

### Next Investigation Steps
1. **Add telemetry logging** to `object_stub::release()` to confirm `shared_count` value
2. **Add telemetry logging** to `service::release_local_stub()` to confirm deletion path
3. **Check if factory creates TWO references** instead of one (factory + return value)
4. **Verify sp_release is actually being sent** from Zone 1 when `baz.reset()` is called
5. **Check for hidden references** in control block or object_proxy initialization

## Conclusion

The reference counting synchronization between control blocks and object_proxy is now correct (no negative counts, proper aggregation). However, the **stub deletion is not occurring when shared_count reaches 0**.

The issue is NOT:
- Multi-zone reference holding (only 2 zones exist)
- Optimistic references preventing deletion (deletion code checks `!is_optimistic`)

The issue IS likely:
- **Factory reference counting mismatch**: Factory creates reference but proxy doesn't properly track/release it
- **Missing sp_release**: Zone 1 might not be sending sp_release when `baz.reset()` is called
- **Extra reference somewhere**: Control block constructor or make_optimistic might be adding extra reference

**Critical Next Step**: Add comprehensive telemetry to trace exact reference count values and sp_add_ref/sp_release calls through the entire lifecycle.
