<!--
Copyright (c) 2025 Edward Boggis-Rolfe
All rights reserved.
-->

# Notes for Version 3

## Tasks for RPC++ Version 3

### High Priority: Core Architecture Changes

#### EPIC-001: Coroutine Threading Architecture
**Priority**: High
**Complexity**: High
**Dependencies**: None
**Estimated Effort**: 3-4 weeks

- [ ] **TASK-001-1**: Implement sender-receiver design pattern between service proxies within a zone
  - **Description**: Design and implement sender-receiver communication pattern to resolve threading issues in service proxy communication
  - **Acceptance Criteria**: Service proxies can communicate safely across threads without data races
  - **Technical Notes**: Focus on zone-internal communication patterns

- [ ] **TASK-001-2**: Integrate fuzz testing for coroutine stress testing
  - **Description**: Enhance existing fuzz tester to stress test coroutine implementation under high load
  - **Acceptance Criteria**: Fuzz tester can run for extended periods without crashes or data corruption
  - **Dependencies**: TASK-001-1

- [ ] **TASK-001-3**: AI-assisted path analysis and documentation
  - **Description**: Use AI to analyze and document happy/unhappy code paths in coroutine implementation
  - **Acceptance Criteria**: Comprehensive documentation of all execution paths with failure scenarios
  - **Dependencies**: TASK-001-1, TASK-001-2

- [ ] **TASK-001-4**: Multi-threaded stress testing
  - **Description**: Create comprehensive multi-threaded test suite to validate coroutine robustness
  - **Acceptance Criteria**: System remains stable under heavy multi-threaded load
  - **Dependencies**: TASK-001-1

- [ ] **TASK-001-5**: Support for SGX Enclaves
  - **Description**: SGX out-of-the-box do not support std::exception pointer nor is the coroutine fully formed, patches may need to be applied
  - **Acceptance Criteria**: Be able to run zones in an enclave while running coroutines
  - **Dependencies**: TASK-001-1

#### EPIC-002: optimistic_ptr Redesign
**Priority**: High
**Complexity**: High
**Dependencies**: EPIC-001
**Estimated Effort**: 2-3 weeks

- [ ] **TASK-002-1**: Restrict shared_ptr to casting_interface remoteable classes only
  - **Description**: Implement type system restrictions to limit shared_ptr usage to remoteable classes
  - **Acceptance Criteria**: Compile-time enforcement prevents misuse of shared_ptr with non-remoteable types
  - **Technical Notes**: May require template metaprogramming and concepts

- [ ] **TASK-002-2**: Refactor shared_ptr/optimistic_ptr control block sharing
  - **Description**: Redesign control block architecture to allow sharing between shared_ptr and optimistic_ptr
  - **Acceptance Criteria**: Both pointer types use same control block while maintaining separate lifetime semantics
  - **Dependencies**: TASK-002-1

- [ ] **TASK-002-3**: Ban optimistic_ptr from local object usage
  - **Description**: Implement runtime/compile-time checks to prevent optimistic_ptr usage with local objects
  - **Acceptance Criteria**: Race conditions on control blocks are eliminated through usage restrictions
  - **Dependencies**: TASK-002-2

- [ ] **TASK-002-4**: Implement local_optimistic_ptr wrapper class
  - **Description**: Create wrapper class that makes optimistic_ptr safe for local object usage
  - **Acceptance Criteria**: Local objects can be safely used with optimistic_ptr semantics
  - **Dependencies**: TASK-002-3

### Medium Priority: Protocol and Interface Improvements

#### EPIC-003: Threading Infrastructure
**Priority**: High
**Complexity**: High
**Dependencies**: EPIC-001, EPIC-002
**Estimated Effort**: 1-2 weeks

- [ ] **TASK-003-1**: Fix identified threading issues
  - **Description**: Address known threading issues after coroutine/optimistic_ptr redesign is complete
  - **Acceptance Criteria**: All threading-related race conditions and deadlocks are resolved
  - **Dependencies**: EPIC-001, EPIC-002

#### EPIC-004: Protocol Version 3 Features
**Priority**: Medium
**Complexity**: Medium
**Dependencies**: None
**Estimated Effort**: 2-3 weeks

- [ ] **TASK-004-1**: Implement IDL attribute fingerprint separation
  - **Description**: Modify fingerprint generation to separate all IDL attributes with spaces
  - **Acceptance Criteria**: Fingerprint generation correctly handles spaced attribute separation
  - **Technical Notes**: May affect backward compatibility

- [ ] **TASK-004-2**: Add unencrypted key/value pairs to i_marshaller functions
  - **Description**: Extend all i_marshaller v3 functions to support unencrypted metadata
  - **Acceptance Criteria**: Routing certificates, OpenTelemetry IDs, and other metadata can be transmitted
  - **Use Cases**: Certificate routing, distributed tracing, debugging metadata

- [ ] **TASK-004-3**: Add RPC version parameter to casting_interface::query_interface
  - **Description**: Extend query_interface function signature to include RPC version parameter
  - **Acceptance Criteria**: Version-aware interface querying is supported
  - **Breaking Change**: Yes - API change

- [ ] **TASK-004-4**: Exclude deprecated elements from fingerprint generation
  - **Description**: Ensure deprecated IDL elements do not contribute to interface fingerprints
  - **Acceptance Criteria**: Fingerprints remain stable when elements are marked as deprecated
  - **Technical Notes**: Important for backward compatibility

### Low Priority: Development and Testing Infrastructure

#### EPIC-005: Development Tools
**Priority**: Low
**Complexity**: Medium
**Dependencies**: None
**Estimated Effort**: 1-2 weeks

- [x] **TASK-005-1**: Generate entity relationship graphs for test suite
  - **Description**: Create automated tooling to generate ER diagrams for test scenarios
  - **Acceptance Criteria**: Visual documentation of test entity relationships is available
  - **Benefits**: Improved test understanding and maintenance

- [ ] **TASK-005-2**: Implement interface ID specification for zone connections
  - **Description**: Modify zone connection API to specify input/output interface IDs
  - **Acceptance Criteria**: Zone connections explicitly declare their interface contracts
  - **Technical Notes**: Improves type safety and debugging

### Definition of Done
- [ ] All code changes include comprehensive unit tests
- [ ] Integration tests pass for all affected components
- [ ] Performance benchmarks show no regression
- [ ] Documentation is updated for all API changes
- [ ] Code review completed by senior team members
- [ ] Backward compatibility impact is documented
- [ ] Migration guide provided for breaking changes
