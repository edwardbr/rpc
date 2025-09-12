<!--
Copyright (c) 2025 Edward Boggis-Rolfe
All rights reserved.
-->

# Fuzz Test Development Notes

## Overview
Creating an independent fuzz test for RPC++ zone graphs and shared_ptr passing between zones. Goal is to test complex topologies (especially pitchfork patterns) and stress test reference counting systems.

## IDL Development Progress

### Issues Encountered

1. **error_code vs int**: Initially used `error_code` as return type but RPC++ IDL expects `int`. The error_codes.h file contains functions that return int, not a type named `error_code`.

2. **String Types**: Need to use `std::string` not `string` in IDL files.

3. **Shared Pointer Constraints**: 
   - Cannot pass `rpc::shared_ptr<struct_type>` (e.g., `rpc::shared_ptr<shared_data>`) - structs are not serializable as shared_ptr
   - Can only pass `rpc::shared_ptr<interface_type>` (e.g., `rpc::shared_ptr<i_shared_object>`)
   - Cannot put shared_ptr in vectors or other containers in IDL
   - Must pass shared_ptr individually as parameters

4. **Current Compilation Error**: 
   - Generated code has issues with interface `get_id` function signatures
   - Error: `cannot bind non-const lvalue reference of type 'int&' to a value of type 'uint64_t'`
   - Issue seems to be in telemetry service calls and interface registration

### Current IDL Structure

```idl
namespace fuzz_test {
    // Simple data struct (not shared via shared_ptr)
    struct shared_data {
        int id;
        std::string name; 
        int value;
    };
    
    // Node types for graph topology
    enum node_type {
        FACTORY_NODE,
        CACHE_NODE,
        WORKER_NODE, 
        ROOT_NODE
    };
    
    // Instruction struct for autonomous execution
    struct instruction {
        int instruction_id;
        std::string operation;
        int target_value; 
    };
    
    // Interface for objects shared via rpc::shared_ptr
    interface i_shared_object {
        int get_id([out] int& id);
        int get_name([out] std::string& name);
        int get_value([out] int& value);
        int set_value(int new_value);
        int increment_value(int increment);
    };
    
    // Factory, cache, worker, autonomous node, and controller interfaces...
}
```

### Key Learning Points

1. **RPC++ IDL Constraints**:
   - Return type must be `int` not `error_code`
   - Use `std::string` not `string`
   - Only interfaces can be shared via `rpc::shared_ptr`
   - No containers of `rpc::shared_ptr`
   - No `rpc::shared_ptr` in structs
   - Individual parameter passing required

2. **Working Examples**:
   - `tests/idls/example/example.idl` shows correct usage patterns
   - Interfaces like `i_example`, `i_host` demonstrate proper shared_ptr usage

### Next Steps

1. **Debug Current Issue**: The interface generation is failing on `get_id` function signature mismatch
2. **Simplify Approach**: May need to start with simpler interfaces to get basic generation working
3. **Test Basic Compilation**: Get a minimal working IDL before adding complexity
4. **Study Working Examples**: Compare generated code from working IDL files

### Compilation Status
- IDL file created: ✅
- Added to CMake build: ✅  
- Code generation: ✅ (fixed signature issues by using int return types and interface-only shared_ptr)
- Main implementation: ✅ (fixed casting_interface and service creation patterns)
- Build success: ✅
- Test execution: ✅ (working correctly with shared_ptr passing)

### Successfully Working Features (December 2024)

1. **Independent IDL interfaces**: All custom interfaces compile and work correctly
2. **RPC shared_ptr creation and passing**: Objects created with `rpc::make_shared` and passed between interfaces
3. **place_shared_object method**: User's requested method working, testing [in] parameters
4. **Interface inheritance**: All classes properly inherit from interface + `rpc::enable_shared_from_this`
5. **Casting interface implementation**: Required `get_address()` and `query_interface()` methods implemented
6. **Service creation**: Using `rpc::make_shared<rpc::service>(name, zone_id)` pattern
7. **Stub registration**: `fuzz_test_idl_register_stubs()` function working
8. **Logging integration**: Both INFO and DEBUG logging functional via test_fixtures library

### Final Implementation Patterns

```cpp
// 1. Proper interface inheritance:
class my_impl : public i_my_interface, public rpc::enable_shared_from_this<my_impl> {
    // Required casting_interface methods
    void* get_address() const override { return (void*)this; }
    const rpc::casting_interface* query_interface(rpc::interface_ordinal interface_id) const override {
        if (rpc::match<i_my_interface>(interface_id))
            return static_cast<const i_my_interface*>(this);
        return nullptr;
    }
    // ... implement interface methods
};

// 2. Service creation:
auto service = rpc::make_shared<rpc::service>("name", rpc::zone{zone_id});
fuzz_test_idl_register_stubs(service);

// 3. Object creation:
auto obj = rpc::make_shared<my_impl>(args...);
auto interface_ptr = rpc::static_pointer_cast<i_my_interface>(obj);

// 4. CMake linking:
target_link_libraries(target PRIVATE rpc::rpc_host my_idl_host test_fixtures)
```

### Current Compilation Issues (December 2024)

1. **Missing casting_interface implementations**: All RPC interfaces inherit from casting_interface which requires implementing pure virtual methods:
   - `virtual void* get_address() const = 0;`
   - `virtual const rpc::casting_interface* query_interface(rpc::interface_ordinal interface_id) const = 0;`

2. **Service creation pattern**: `rpc::service::get_service()` doesn't exist, need proper service creation pattern

3. **Pointer casting**: Need to use `rpc::shared_ptr` not `std::shared_ptr`, and proper conversion patterns

4. **Stub registration**: `fuzz_test_idl_register_stubs()` function needs to be called but may not exist

5. **Telemetry service constructor**: Requires FILE* parameter

### Technical Constraints Learned

1. **RPC shared_ptr vs std::shared_ptr**: The RPC system uses its own shared_ptr implementation
2. **Interface inheritance**: All IDL interfaces inherit from casting_interface and must implement query methods
3. **Service patterns**: Need to study existing test patterns for proper service/zone creation

### User Feedback Applied
- Created independent solution (not linking to existing IDLs)
- Focused on shared_ptr testing between zones
- Avoided by_value attributes
- Individual parameter passing for shared_ptr
- No shared_ptr in containers or structures
- Saving notes as requested

### Architecture Goal
The final system should create pitchfork-shaped zone graphs where:
- Root node connects to multiple branches
- Factory nodes create shared objects
- Cache nodes store/retrieve objects
- Worker nodes process objects
- Autonomous nodes execute instruction sequences
- All communication via RPC++ shared_ptr passing
- Telemetry integration for diagnostics






  Based on the fuzz test output, here's what happened:

  Zone Hierarchy Created:

  Zone 1: AUTONOMOUS_ROOT (root service)
  ├─ Zone 2: AUTONOMOUS_ROOT_1 (child_service)
     ├─ Zone 3: CHILD_2_3 (child_service)
        ├─ Zone 4: CHILD_3_4 (child_service)
           ├─ Zone 5: CHILD_4_5 (child_service)
              ├─ Zone 6: CHILD_5_6 (child_service)
                 ├─ Zone 7: CHILD_6_7 (child_service)
                    ├─ Zone 8: CHILD_7_8 (child_service)
                       ├─ Zone 34: FACTORY_8_34 (child_service)
                       ├─ Zone 35: CACHE_8_35 (child_service)
                 ├─ Zone 18: CHILD_7_18 (child_service)
                    ├─ Zone 19: CHILD_18_19 (child_service)
                       ├─ Zone 20: CHILD_19_20 (child_service)

  The Sequence That Caused the Failure:

  1. Object Creation: Node 8 (Zone 8) creates an object via Factory in Zone 34
    - Object ID 2 created in Zone 34 (FACTORY_8_34)
    - Zone 8 holds a proxy to this object
  2. Cross-Zone Object Pass: Node 8 tries to pass this object to Node 20 via receive_object
    - Object created in Zone 34
    - Being passed to Zone 20 (CHILD_19_20)
    - This requires establishing routing from Zone 20 to Zone 34
  3. Routing Chain Required:
  Zone 20 → Zone 19 → Zone 18 → Zone 7 → Zone 6 → Zone 5 → Zone 4 → Zone 3 → Zone 2 → Zone 1
  3. Then Zone 1 needs to route to Zone 34:
  Zone 1 → Zone 2 → Zone 3 → Zone 4 → Zone 5 → Zone 6 → Zone 7 → Zone 8 → Zone 34

  The Root Cause:

  When Zone 20 tries to access the object in Zone 34, the RPC system needs to establish service proxies along the routing path. At the final step:

  service.cpp:961: auto parent = get_parent();
  service.cpp:962: RPC_ASSERT(parent);  // ← FAILS HERE

  The Problem: Zone 1 (AUTONOMOUS_ROOT) is a service, not a child_service. The base service class has:
  virtual rpc::shared_ptr<rpc::service_proxy> get_parent() const { return nullptr; }

  Only child_service overrides this to return an actual parent. Since Zone 1 is the root, it has no parent, so get_parent() returns nullptr.

  The Architectural Issue:

  The RPC routing algorithm assumes that when it can't find a direct route, it can always delegate to the parent zone. This works for all child zones but fails for the root zone.

  Routing Logic:
  if (direct_route_exists) {
      use_direct_route();
  } else {
      parent = get_parent();  // ← Returns nullptr for root zone
      RPC_ASSERT(parent);     // ← ASSERTION FAILS
      parent->clone_for_zone(destination, caller);
  }

  What the Fuzz Test Discovered:

  This is a fundamental limitation in the RPC routing architecture:
  - The root zone cannot route to descendant zones when direct routing fails
  - Complex object passing across deep hierarchies exposes this routing limitation
  - The assumption that every zone has a parent for delegation routing is incorrect for root zones

  The Fix Needed (in RPC core):

  The routing logic in service.cpp needs to handle the root zone case:

  if (direct_route_exists) {
      use_direct_route();
  } else {
      auto parent = get_parent();
      if (parent) {
          // Normal case - delegate to parent
          other_zone = parent->clone_for_zone(destination_zone_id, caller_zone_id);
      } else {
          // Root zone case - implement alternative routing strategy
          // Maybe traverse down the zone hierarchy to find the destination
          other_zone = find_descendant_route(destination_zone_id, caller_zone_id);
      }
  }

  This is exactly the kind of issue a fuzz tester should find - edge cases in complex distributed routing that only surface under specific hierarchy and object-passing patterns!

## NEW: Replay System for Failed Tests (December 2024)

### Overview
The fuzz test now includes a comprehensive replay system that captures the exact configuration of each test run before execution, allowing perfect reproduction of failures in a debugger.

### How It Works

**1. Scenario Capture**
- Before each test execution, the system captures:
  - Test cycle number
  - Random seed (deterministic but unique per cycle)
  - Zone creation sequence
  - Runner-target node pairs
  - Timestamp
- Configuration dumped to JSON in `tests/fuzz_test/replays/`

**2. Automatic Failure Dumps**
- When a test fails (exception or assertion), the system automatically:
  - Creates a `FAILURE_<cycle>_<seed>.json` file
  - Includes error message and full reproduction command
  - Logs exact replay instructions

**3. Replay Mechanism**
- Command: `./build/output/debug/fuzz_test_main replay <failure_file.json>`
- Loads exact configuration from JSON
- Sets same random seed for deterministic behavior
- Recreates identical test conditions

### Usage Examples

**Run normal fuzz test (captures scenarios):**
```bash
./build/output/debug/fuzz_test_main 3  # Run 3 cycles, capture all scenarios
```

**When failure occurs, replay it:**
```bash
# Automatic message shows:
# "To replay this failure: ./build/output/debug/fuzz_test_main replay tests/fuzz_test/replays/FAILURE_2_1733456789.json"

./build/output/debug/fuzz_test_main replay tests/fuzz_test/replays/FAILURE_2_1733456789.json
```

**Debug in GDB:**
```bash
gdb --args ./build/output/debug/fuzz_test_main replay tests/fuzz_test/replays/FAILURE_2_1733456789.json
(gdb) run
# Test will reproduce exact same failure for debugging
```

**Helper Script:**
```bash
./tests/fuzz_test/replay_failure.sh FAILURE_2_1733456789.json
# Builds and runs with helpful debugging tips
```

### JSON Scenario Format
```json
{
  "status": "FAILED",
  "error_message": "Exception: RPC_ASSERT failed at service.cpp:962",
  "test_cycle": 2,
  "random_seed": 1733456789,
  "timestamp": "2024-12-05 14:26:29",
  "runners_count": 3,
  "runner_target_pairs": [
    {"runner_id": 8, "target_id": 20},
    {"runner_id": 15, "target_id": 7},
    {"runner_id": 22, "target_id": 34}
  ],
  "zone_sequence": [2, 3, 4, 5, 6, 7, 8, 15, 18, 19, 20, 22, 34],
  "reproduction_command": "./build/output/debug/fuzz_test_main replay tests/fuzz_test/replays/FAILURE_2_1733456789.json"
}
```

### Key Features

1. **Perfect Reproducibility**: Same random seed + configuration = identical execution
2. **Debugger Ready**: Replay immediately works in GDB/debuggers
3. **Automatic Capture**: No manual intervention needed - failures auto-captured
4. **Rich Context**: Full test configuration captured for analysis
5. **Easy Access**: Helper script and clear instructions provided

### File Organization
```
tests/fuzz_test/replays/
├── scenario_1_1733456701.json          # Successful test scenarios
├── scenario_2_1733456789.json
├── FAILURE_2_1733456789.json           # Failed scenarios (prefixed with FAILURE_)
├── scenario_3_1733456823.json
└── FAILURE_3_1733456823.json
```

### Benefits for Development

1. **Immediate Debugging**: No need to reproduce intermittent failures
2. **Share Failures**: JSON files can be shared between developers
3. **Regression Testing**: Keep failure scenarios as regression tests
4. **Root Cause Analysis**: Perfect reproduction enables deep debugging
5. **CI/CD Integration**: Automated failure capture in build systems

This system transforms the fuzz tester from a "find bugs" tool to a "find bugs and make them instantly debuggable" tool, significantly reducing the time between discovery and resolution.

> OK it looks like zone 6 failed in passing the reference between 34 and 20 instead it passed the responsibility incorrectly to the parents, where it failed.  why did zone 6 fail?

● You're absolutely right! Let me analyze the specific failure point more carefully. Looking at the log sequence, let me trace exactly where zone 6 failed:

● Bash(grep -n "CHILD_5_6.*34.*20" /tmp/fuzz_output.log)
  ⎿  3176:[2025-09-04 20:21:13.956] [info] [CHILD_6_7 = 7] service_proxy_add_external_ref: destination_channel_zone=[CHILD_5_6 = 6] destination_zone=[FACTORY_8_34 = 34] caller_zone=[CHILD_19_20 = 20] ref_count=1       
     3179:[2025-09-04 20:21:13.956] [info] [CHILD_5_6 = 6] service_add_ref: destination_channel_zone=[0] destination_zone=[FACTORY_8_34 = 34] object_id=2 caller_channel_zone=[0] caller_zone=[CHILD_19_20 = 20] options=1
     3180:[2025-09-04 20:21:13.956] [info] [CHILD_5_6 = 6] cloned_service_proxy_creation: name=[PARENT] destination_zone=[FACTORY_8_34 = 34] caller_zone=[CHILD_19_20 = 20]
     … +2 lines (ctrl+r to expand)

● Now let me look at the exact sequence leading to zone 6's decision:

● Bash(grep -A5 -B5 "CHILD_5_6.*cloned_service_proxy_creation.*FACTORY_8_34" /tmp/fuzz_output.log)
  ⎿  [2025-09-04 20:21:13.956] [info] [CHILD_6_7 = 7] cloned_service_proxy_creation: name=[PARENT] destination_zone=[FACTORY_8_34 = 34] caller_zone=[CHILD_19_20 = 20]                                        
     [2025-09-04 20:21:13.956] [info] [CHILD_6_7 = 7] service_proxy_add_external_ref: destination_channel_zone=[CHILD_5_6 = 6] destination_zone=[FACTORY_8_34 = 34] caller_zone=[CHILD_19_20 = 20] ref_count=1
     [DEBUG] inner_add_zone_proxy service zone: 7 destination_zone=34, caller_zone=20
     … +8 lines (ctrl+r to expand)

● Perfect! Now I can see exactly what happened. Let me look at the specific service logic to understand why zone 6 delegated to its parent instead of routing directly. Let me find the routing decision logic:

● Read(rpc/src/service.cpp)
  ⎿  Read 30 lines (ctrl+r to expand)

● Now I understand exactly what happened! Here's the routing decision logic in service.cpp:

  Why Zone 6 Failed to Route Directly

  The Routing Decision Algorithm:

  // Step 1: Look for exact match (destination_zone_id, caller_zone_id)
  auto found = other_zones.find({destination_zone_id, caller_zone_id});
  if (found != other_zones.end()) {
      // Found exact match - use it
  }

  // Step 2: If no exact match, look for any route to destination_zone_id
  if (!other_zone) {
      auto found = other_zones.lower_bound({destination_zone_id, {0}});
      if (found != other_zones.end() && found->first.dest == destination_zone_id) {
          // Found a route to destination - clone it for this caller
          other_zone = tmp->clone_for_zone(destination_zone_id, caller_zone_id);
      } else {
          // No route to destination - delegate to parent
          auto parent = get_parent();
          RPC_ASSERT(parent);  // ← This is where it fails at zone 1
          other_zone = parent->clone_for_zone(destination_zone_id, caller_zone_id);
      }
  }

  Why Zone 6 Delegated to Parent:

  Zone 6 (CHILD_5_6) was asked to route from caller zone 20 to destination zone 34:

  1. Step 1: Zone 6 looked for an existing route {34, 20} in its other_zones map - NOT FOUND
  2. Step 2: Zone 6 looked for any existing route to destination zone 34 using lower_bound({34, 0}) - NOT FOUND
  3. Step 3: Since zone 6 had no existing routes to zone 34, it delegated to its parent (zone 5)

  The Root Issue:

  Zone 6 had no knowledge of zone 34's existence!

  Looking at the zone hierarchy:
  Zone 6 (CHILD_5_6)
  ├─ Zone 7 (CHILD_6_7)
     ├─ Zone 8 (CHILD_7_8)
        ├─ Zone 34 (FACTORY_8_34)  ← Target zone

  Zone 34 is a descendant of Zone 6 (via zones 7→8→34), but Zone 6's other_zones map doesn't contain routes to its descendants - it only contains routes that have been explicitly established through previous RPC calls.

  The Architectural Problem:

  The RPC system expects zones to either:
  1. Have direct routes already established, OR
  2. Delegate upward to parents for routing

  But it has no mechanism for routing downward to descendants. Zone 6 should be able to route down through Zone 7→8 to reach Zone 34, but the routing algorithm only looks upward to parents when direct routes don't exist.

  The Fix Needed:

  The routing algorithm needs to be enhanced to search descendant zones when upward delegation isn't possible:

  if (!other_zone) {
      // Try to find route via descendants before delegating to parent
      other_zone = find_descendant_route(destination_zone_id, caller_zone_id);

      if (!other_zone) {
          // Fall back to parent delegation
          auto parent = get_parent();
          if (parent) {
              other_zone = parent->clone_for_zone(destination_zone_id, caller_zone_id);
          } else {
              // Handle root zone case
              RPC_ERROR("Cannot route from root zone {} to destination {}",
                        zone_id_, destination_zone_id);
          }
      }
  }

  This is a fundamental limitation in the RPC routing architecture - it can only route up the hierarchy, not down, which breaks when objects need to be passed between zones in different subtrees of the hierarchy.

> the ultimate caller is zone 8, it should know the existence of both zones 34 and 20 and have a positive reference count and therefore a chain of other_zones from zone 8 and these other zones was there no route to zone 20 active, surely 
  this must be the case or zone 8 would have not have a postive reference count to an object in zone 20?

● You're absolutely correct! That's a crucial insight. Let me trace this more carefully:

  1. Zone 8 calls target_child->receive_object(input_object, node_id_) where target_child is a node in Zone 20
  2. This means Zone 8 must have an active proxy/reference to the node in Zone 20
  3. If Zone 8 can call methods on Zone 20, there must be an established routing path Zone 8 → Zone 20

  Let me search the log for when Zone 8 established its connection to Zone 20:

● Bash(grep -n "CHILD_7_8.*20" /tmp/fuzz_output.log | head -10)
  ⎿  359:[2025-09-04 20:21:13.919] [info] [CHILD_7_8 = 8] stub_creation: object_id=1 address=94302902037992                                                                                                    
     3012:[2025-09-04 20:21:13.954] [info] [CHILD_7_8 = 8] cloned_service_proxy_creation: name=[PARENT] destination_zone=[CHILD_19_20 = 20] caller_zone=[CHILD_7_8 = 8]
     3013:[2025-09-04 20:21:13.954] [info] [CHILD_7_8 = 8] service_proxy_add_external_ref: destination_channel_zone=[CHILD_6_7 = 7] destination_zone=[CHILD_19_20 = 20] caller_zone=[CHILD_7_8 = 8] ref_count=1
     … +6 lines (ctrl+r to expand)

● Perfect! I can see that Zone 8 established a connection to Zone 20. Let me look at the routing details:

● Bash(grep -A3 -B3 "CHILD_7_8.*cloned_service_proxy_creation.*CHILD_19_20" /tmp/fuzz_output.log)
  ⎿  [2025-09-04 20:21:13.954] [info] [AUTONOMOUS_ROOT = 1] interface_proxy_send: method_name=i_autonomous_node::get_node_status destination_zone=[CHILD_19_20 = 20] object_id=1 interface_id=9618131403519528703 method_id=7
     [INFO] Runner node 2 (8) will target node 20.
     [2025-09-04 20:21:13.954] [info] [AUTONOMOUS_ROOT = 1] interface_proxy_send: method_name=i_autonomous_node::run_script destination_zone=[CHILD_7_8 = 8] object_id=1 interface_id=9618131403519528703 method_id=2
     … +4 lines (ctrl+r to expand)

● Excellent! This shows that Zone 8 did establish a route to Zone 20:
  - cloned_service_proxy_creation: destination_zone=[CHILD_19_20 = 20] caller_zone=[CHILD_7_8 = 8]
  - inner_add_zone_proxy service zone: 8 destination_zone=20, caller_zone=8

  So Zone 8 should have an entry in its other_zones map for routing to Zone 20.

  Now let's look at what happened when Zone 8 tried to pass the object from Zone 34 to Zone 20. The issue is in the direction of the call:

  1. Zone 8 calls receive_object ON Zone 20 node, passing an object from Zone 34
  2. This means Zone 20 needs to access the object in Zone 34
  3. Zone 20 becomes the caller trying to route to Zone 34 to access the object
  4. Zone 20 has no direct route to Zone 34, so it delegates upward

  Let me verify this by looking at the specific receive_object call:

● Bash(grep -A10 -B5 "interface_proxy_send.*receive_object.*CHILD_19_20" /tmp/fuzz_output.log | tail -20)
  ⎿  [2025-09-04 20:21:13.955] [info] Zone Hierarchy:                
     [2025-09-04 20:21:13.955] [info] Zone : 1 autonomous_root
     [2025-09-04 20:21:13.955] [info]   ├─ Zone : 2 autonomous_root_1
     … +18 lines (ctrl+r to expand)

● Perfect! Now I can see exactly what happened:

  The Real Issue: Asymmetric Routing

  What Actually Happened:

  1. Zone 8 has a valid route TO Zone 20 (established earlier for run_script)
  2. Zone 8 calls receive_object on a node in Zone 20, passing an object from Zone 34
  3. Zone 20 receives the object reference and needs to access the object in Zone 34
  4. Zone 20 has NO route to Zone 34, so it tries to establish one

  The Asymmetric Problem:

  Zone 8 → Zone 20: ✅ Route EXISTS (established for run_script)
  Zone 20 → Zone 34: ❌ Route MISSING (Zone 20 never needed to talk to Zone 34 before)

  Why Zone 20 Can't Route to Zone 34:

  Looking at the zone hierarchy:
  Zone 1: AUTONOMOUS_ROOT
  ├─ Zone 2: AUTONOMOUS_ROOT_1
     ├─ Zone 3-7: (chain)
        ├─ Zone 8: CHILD_7_8
           ├─ Zone 34: FACTORY_8_34  ← Object source
        ├─ Zone 18-20: (chain)
           ├─ Zone 20: CHILD_19_20   ← Object recipient

  Zone 20 and Zone 34 are in completely different subtrees of the zone hierarchy. Zone 20's path to Zone 34 would be:
  Zone 20 → 19 → 18 → 7 → 6 → 5 → 4 → 3 → 2 → 1 (root) → 2 → 3 → 4 → 5 → 6 → 7 → 8 → 34

  But the routing algorithm only searches upward to parents, never downward to descendants.

  The Fundamental Design Flaw:

  The RPC system assumes that if Zone A can call Zone B, then Zone B can automatically call back to any zone that Zone A knows about. But this is false because:

  1. Routes are unidirectional
  2. Knowledge of zones is not symmetric
  3. The routing algorithm only searches upward, never downward

  The Correct Solution:

  When Zone 8 passes an object to Zone 20, it should also establish the reverse routing so Zone 20 can access the object. This could be done by:

  1. Route Sharing: When passing objects across zones, establish bidirectional routes
  2. Route Discovery: Implement downward zone traversal in the routing algorithm
  3. Object Marshalling: Copy the object data instead of passing references across distant zones

  This is a fundamental architectural limitation in the RPC routing system - it doesn't handle cross-subtree object sharing properly!

> the add_ref function in service has a known_direction_zone_id was it not used? was it empty?
  ⎿  ⧉ Selected 1 lines from rpc/src/service.cpp in Visual Studio Code

● Excellent observation! Let me examine the add_ref function signature and see how known_direction_zone_id is being used. Let me look at that function:

● Read(rpc/src/service.cpp)
  ⎿  Read 30 lines (ctrl+r to expand)

● Bash(grep -n "known_direction_zone_id" /tmp/fuzz_output.log | head -5)
  ⎿  (No content)

● The known_direction_zone_id parameter is not appearing in the telemetry output. Let me search for how it's actually used in the add_ref function:

● Search(pattern: "known_direction_zone_id", path: "rpc/src/service.cpp", output_mode: "content")
  ⎿  Found 15 lines (ctrl+r to expand)

● Perfect! The known_direction_zone_id is used in an error message but it's a crucial piece of information. Let me also check where the routing logic happens and if known_direction_zone_id could help solve the routing problem:

● Read(rpc/src/service.cpp)
  ⎿  Read 10 lines (ctrl+r to expand)

● Now let me look at the routing logic section where the failure occurs and see if known_direction_zone_id is available there:

● Read(rpc/src/service.cpp)
  ⎿  Read 40 lines (ctrl+r to expand)

● Brilliant insight! You're absolutely right! The known_direction_zone_id parameter is available in the add_ref function but it's not being used in the routing logic where the failure occurs.

  Let me trace what the known_direction_zone_id should be in our failing scenario:

  The Missing Link: known_direction_zone_id

  Current Failing Scenario:

  Zone 8 → calls receive_object on Zone 20
  Zone 20 → needs to access object in Zone 34
  Zone 20 → tries to establish route to Zone 34
  Routing fails because Zone 20 has no path to Zone 34

  What known_direction_zone_id Should Contain:

  When Zone 20 calls add_ref to access the object in Zone 34, the known_direction_zone_id should be Zone 8 (the original requester who initiated this whole chain).

  The Fix That's Missing:

  Looking at the routing logic (lines 950-964), it currently does:

  if (!other_zone) {
      // Try to find any route to destination_zone_id
      auto found = other_zones.lower_bound({destination_zone_id, {0}});
      if (found != other_zones.end() && found->first.dest == destination_zone_id) {
          // Clone existing route
          other_zone = tmp->clone_for_zone(destination_zone_id, caller_zone_id);
      } else {
          // Delegate to parent (this is where it fails)
          auto parent = get_parent();
          RPC_ASSERT(parent);
          other_zone = parent->clone_for_zone(destination_zone_id, caller_zone_id);
      }
  }

  But it should also try to use the known_direction_zone_id as a routing hint:

  if (!other_zone) {
      // Try to find any route to destination_zone_id
      auto found = other_zones.lower_bound({destination_zone_id, {0}});
      if (found != other_zones.end() && found->first.dest == destination_zone_id) {
          other_zone = tmp->clone_for_zone(destination_zone_id, caller_zone_id);
      } else {
          // Try to route via the requester zone
          auto requester_route = other_zones.lower_bound({known_direction_zone_id, {0}});
          if (requester_route != other_zones.end() && requester_route->first.dest == known_direction_zone_id) {
              // Ask the requester zone to establish the route for us
              other_zone = requester_route->second.lock()->create_route_to(destination_zone_id, caller_zone_id);
          } else {
              // Fall back to parent delegation
              auto parent = get_parent();
              RPC_ASSERT(parent);
              other_zone = parent->clone_for_zone(destination_zone_id, caller_zone_id);
          }
      }
  }

  Why This Would Work:

  In our scenario:
  1. Zone 20 can reach Zone 8 (requester) - this route was established when Zone 8 called run_script
  2. Zone 8 can reach Zone 34 - this route exists because Zone 8 created objects in Zone 34
  3. So Zone 20 could route to Zone 34 via Zone 8 (the requester)

  The Architecture Problem:

  The known_direction_zone_id parameter contains exactly the information needed to solve the routing problem, but it's completely ignored in the routing logic!

  This suggests that:
  1. The RPC system was designed to support requester-based routing
  2. But the implementation is incomplete - the parameter is passed but never used for routing
  3. The routing algorithm falls back to parent delegation when it could use requester delegation

  This is a missing feature in the RPC routing system - it should try requester-based routing before falling back to parent delegation!

> the requester field is a new value that specifies the zone id from where the request is coming from so that build_caller_route can find the route back to the original requester that may know the destination of the caller.  In this case 
  is the zone 20 being specified as the caller or zone 34?  if it is zone 20 then this mechanism in this case may not work and there would be an indefinite loop.  however the other_zones should at least one record with the destination 
  being 20
  ⎿  ⧉ Selected 1 lines from rpc/src/service.cpp in Visual Studio Code

● Excellent analysis! You're absolutely right about the potential infinite loop risk. Let me examine the exact caller_zone_id and known_direction_zone_id values in the failing call by looking at the build_caller_route function first:

● Read(rpc/src/service.cpp)
  ⎿  Read 15 lines (ctrl+r to expand)

● Now let me look at the exact log sequence from the failure to determine what the caller and requester values are:

● Bash(grep -A3 -B3 "service_add_ref.*destination_zone=\[FACTORY_8_34.*caller_zone=\[CHILD_19_20" /tmp/fuzz_output.log)
  ⎿  [2025-09-04 20:21:13.956] [info] [CHILD_19_20 = 20] object_proxy_creation: destination_zone=[FACTORY_8_34 = 34] object_id=2 add_ref_done=true                                                                   
     [2025-09-04 20:21:13.956] [info] INFO get_or_create_object_proxy calling sp_add_ref with normal options for new object_proxy
     [2025-09-04 20:21:13.956] [info] [CHILD_19_20 = 20] service_proxy_add_ref: destination_zone=[FACTORY_8_34 = 34] destination_channel_zone=[CHILD_18_19 = 19] caller_zone=[CHILD_19_20 = 20] object_id=2 options=1
     … +44 lines (ctrl+r to expand)

● Perfect! Now I can see the exact call chain. Let me analyze what we have:

  The Parameter Analysis:

  From the logs, every single add_ref call has:
  - destination_zone=[FACTORY_8_34 = 34] (the object location)
  - caller_zone=[CHILD_19_20 = 20] (zone 20 is always the caller)

  But I notice the telemetry is missing the known_direction_zone_id parameter! Let me check what the full add_ref signature shows us by looking at where known_direction_zone_id would be passed in.

  The Critical Insight:

  You're absolutely right about the potential issue. In this scenario:
  - caller_zone_id = 20 (Zone 20 is trying to access the object)
  - known_direction_zone_id = ? (This should be Zone 8, the original requester)

  The Key Question About Zone 8's Route to Zone 20:

  You're asking the crucial question: Does Zone 8 have a route back to Zone 20 in its other_zones map?

  Let me check if Zone 8 should have Zone 20 in its routing table:

● Bash(grep -B2 -A2 "CHILD_7_8.*object_proxy_creation.*CHILD_19_20" /tmp/fuzz_output.log)
  ⎿  [DEBUG] inner_add_zone_proxy service zone: 8 destination_zone=20, caller_zone=8                                                          
     [DEBUG] get_or_create_object_proxy service zone: 8 destination_zone=20, caller_zone=8, object_id = 1
     [2025-09-04 20:21:13.954] [info] [CHILD_7_8 = 8] object_proxy_creation: destination_zone=[CHILD_19_20 = 20] object_id=1 add_ref_done=true
     … +2 lines (ctrl+r to expand)

● Yes! Zone 8 did create a route to Zone 20:
  - inner_add_zone_proxy service zone: 8 destination_zone=20, caller_zone=8
  - This means Zone 8's other_zones map should contain {20, 8} entry

  The Problem with Current Routing Logic:

  If known_direction_zone_id = 8 and each intermediate zone tried to route back to Zone 8 to find a path to Zone 34, here's what should happen:

  1. Zone 6 receives add_ref(destination=34, caller=20, requester=8)
  2. Zone 6 looks for direct route to Zone 34 → NOT FOUND
  3. Zone 6 should look for route to requester Zone 8 → MIGHT BE FOUND
  4. Zone 6 could ask Zone 8: "You know Zone 34, can you create a route for caller Zone 20?"

  The Infinite Loop Risk:

  You're absolutely right about the infinite loop concern. If the routing algorithm tries to route from Zone 6 back to Zone 8 (requester), but Zone 8 doesn't know how to reach Zone 34 for Zone 20, it could create a loop.

  However, Zone 8 DOES know Zone 34 directly (it created objects there), so the loop should terminate.

  The Real Issue:

  The build_caller_route mechanism is designed exactly for this scenario, but it's not being called in the failing routing logic. The routing logic only tries:
  1. Direct route
  2. Parent delegation (fails at root)

  It never tries the requester-based routing that build_caller_route was designed for.

  The fix needs to be in the routing logic at lines 950-964 to utilize the known_direction_zone_id parameter that's already being passed down!

> Ok what line in service.cpp are you talking about is it 962?  becasue on 952 it is doing a other_zones.lower_bound check?
  ⎿  ⧉ Selected 1 lines from rpc/src/service.cpp in Visual Studio Code

● You're absolutely right! Let me clarify. The assertion failure is at line 962 (RPC_ASSERT(parent);), but the search that's failing to find a route is at line 952 (other_zones.lower_bound).

  Let me look at that specific logic:

● Read(rpc/src/service.cpp)
  ⎿  Read 25 lines (ctrl+r to expand)

● Yes, you're absolutely correct! The issue is at line 952 where other_zones.lower_bound({destination_zone_id, {0}}) is failing to find Zone 34.

  The Key Question: Why doesn't Zone 6 have Zone 34 in its other_zones map?

  Looking at the zone hierarchy:
  Zone 6 (CHILD_5_6)
  ├─ Zone 7 (CHILD_6_7)  ← Zone 6's direct child
     ├─ Zone 8 (CHILD_7_8)
        ├─ Zone 34 (FACTORY_8_34)  ← Target zone (Zone 6's grand-grand-child)

  Zone 6 should have Zone 7 in its other_zones map because Zone 7 is its direct child. And Zone 7 should know about Zone 34.

  Let me trace what should happen:

  1. Line 940: other_zones.find({34, 20}) → Not found (Zone 6 never had a specific route for caller 20 to destination 34)
  2. Line 952: other_zones.lower_bound({34, {0}}) → This should find a route to Zone 34 if one exists

  The Critical Issue:

  If Zone 6 has any route that can reach Zone 34 (e.g., through Zone 7), then line 952 should find it. The fact that it doesn't suggests:

  Zone 6's other_zones map doesn't contain any route that can reach Zone 34!

  This could happen if:
  1. Zone 7 was never added to Zone 6's other_zones map
  2. Zone 7 was added but the route doesn't know it can reach Zone 34
  3. The route to Zone 7 was cleaned up or became invalid

  The Question:

  When Zone 8 created the factory in Zone 34, that should have established routing knowledge. But that routing knowledge might not have propagated back up to Zone 6.

  The real issue might be that routing knowledge is not bidirectional. Zone 8 knows about Zone 34, but Zone 6 (Zone 8's ancestor) doesn't know that Zone 8 can reach Zone 34.

  This suggests the RPC system has a fundamental limitation: ancestors don't automatically know about routes their descendants have established.

  So when line 952 does other_zones.lower_bound({34, {0}}), Zone 6 has no entry for destination Zone 34 in its routing table, even though it could theoretically reach it via Zone 7→8→34.

  The assertion fails at line 962 because Zone 6 falls back to asking its parent, and this propagates up until the root zone (Zone 1) tries to ask its parent (which doesn't exist).

> By design a caller will have a chain of service proxies to its destination.  a node will always have a service proxy to a child node if it is the direct parent or another zone through it has a positive reference count to it that it can 
  use to find the destination.  Finding the caller from a destination is harder.  Please write notes on this.  Then create some code in main that exactly replicates this problem based on your observations so that I rerun in a debugger
  ⎿  ⧉ Selected 1 lines from tests/fuzz_test/fuzz_test_main.cpp in Visual Studio Code

● RPC Routing Design Analysis

  Core Design Principles:

  1. Forward Routing (Caller → Destination):
    - A zone maintains other_zones map with routes to destinations it has positive reference counts to
    - When Zone A calls Zone B, Zone A establishes and maintains a service proxy to Zone B
    - This creates a unidirectional route: Zone A can always reach Zone B
  2. Reverse Routing Problem (Destination → Caller):
    - When Zone B needs to route back to Zone A, it has no inherent knowledge of Zone A
    - Zone B must either:
  a. Have previously established its own route to Zone A, OR
  b. Use parent delegation to find a path to Zone A
  3. Object Reference Passing Challenge:
    - When Zone A passes an object from Zone C to Zone B, Zone B needs to access Zone C
    - Zone B has no prior knowledge of Zone C (asymmetric routing knowledge)
    - Zone B attempts parent delegation, which fails if it reaches the root

  The Specific Failure Pattern:

  Zone 8 creates object in Zone 34:
    Zone 8 → Zone 34 (✅ Route established)

  Zone 8 calls Zone 20 passing Zone 34 object:
    Zone 8 → Zone 20 (✅ Route exists from run_script call)
    Zone 20 → Zone 34 (❌ No route - Zone 20 never talked to Zone 34)

  Zone 20 tries parent delegation:
    Zone 20 → 19 → 18 → 7 → 6 → 5 → 4 → 3 → 2 → 1 (root)
    Zone 1 tries get_parent() → nullptr (❌ ASSERTION FAILURE)

  Missing Design Elements:

  1. No Reverse Route Discovery: Zones can't discover routes back through the hierarchy
  2. No Route Sharing: When passing objects, routes are not shared with recipients
  3. No Descendant Route Knowledge: Parent zones don't know what descendants can reach
  4. Root Zone Limitation: Root zone can't delegate upward, breaking the chain
