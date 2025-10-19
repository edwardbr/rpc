<!--
Copyright (c) 2025 Edward Boggis-Rolfe
All rights reserved.
-->

# SPSC Channel Lifecycle Corrections Summary

**Date**: 2025-01-19
**Original Document**: SPSC_Channel_Lifecycle.md
**Corrected Document**: SPSC_Channel_Lifecycle_Corrected.md (merged into original)
**Source Documents**: Problem_Statement_Critique_QA.md, Service_Proxy_Transport_Problem_Statement.md, RPC++_User_Guide.md

---

## Overview

The SPSC_Channel_Lifecycle.md document was reviewed and corrected to align with requirements from the Q&A, Problem Statement, and User Guide. The document is now accurate and includes critical clarifications.

---

## Corrections Made

### ✅ CORRECTION 1: SPSC is Transport-Specific Implementation

**Added**: Header clarification stating SPSC channel manager is an **SPSC-specific implementation detail**, NOT a core RPC++ abstraction.

**Rationale**: Other transports (TCP, Local, SGX) may have completely different internal implementations. SPSC's channel_manager with pump tasks is unique to SPSC.

**Lines**: 6-10 (added header note)

---

### ✅ CORRECTION 2: Handler Polymorphism via i_marshaller

**Added**: Clarification that message handler calls **i_marshaller methods** on registered handler (service or pass_through).

**Original**: Document didn't explain what handler does with messages.

**Corrected**: Added explanation:
- Handler can be service OR pass_through (polymorphic via i_marshaller interface)
- Transport doesn't know which it is
- After unpacking envelope, transport calls: `handler_->send()`, `handler_->post()`, etc.

**Lines**: 33, 219-245 (added handler clarification and code example)

---

### ✅ CORRECTION 3: Reference Counting Mechanism Subject to Change

**Added**: **WARNING** that `service_proxy_ref_count_` mechanism is **subject to change** in future designs.

**Source**: Q&A Q2 explicitly states this mechanism may change.

**Impact**: Pass-through architecture may eliminate this reference counting pattern. Pass-through will control service_proxy lifetime instead.

**Lines**: 37-43, 55 (added warnings)

---

### ✅ CORRECTION 4: Transport Sharing Pattern

**Added**: Critical clarification that **multiple service_proxies can share ONE channel_manager**.

**Original**: Document didn't emphasize this pattern.

**Corrected**:
- Reference count can be 2, 3, or higher (not just bidirectional pair)
- Channel disambiguates messages by `(destination_zone, caller_zone)` tuple
- Confirms Q&A Q2 finding about transport sharing

**Lines**: 76-82 (added transport sharing section)

---

### ✅ CORRECTION 5: Zone Termination Support

**Added**: Extensive section on zone termination handling.

**Source**: Q&A Q9 requirements for zone termination broadcast.

**New Content**:
1. **Shutdown Process Enhancement** (Lines 117-136):
   - Graceful shutdown: existing behavior
   - Zone terminating: new behavior (detect failure, broadcast, skip waiting)

2. **Receive Task Zone Termination** (Lines 151-167):
   - Receive `zone_terminating` message
   - Notify service via `i_marshaller::post(post_options::zone_terminating)`
   - Notify pass-through (if exists) via same method
   - Mark zone as terminated
   - Clean up all service_proxies to that zone

3. **Key Invariant Added** (Lines 322-325):
   - Transport notifies service AND pass-through on zone termination
   - Required for proper cleanup of routing infrastructure

**Rationale**: Transport is responsible for detecting connection failure and broadcasting zone termination to handlers.

---

### ✅ CORRECTION 6: Operational State Check Requirement

**Added**: New section on `is_operational()` method requirement.

**Source**: Q&A Q3 both-or-neither guarantee and transport viability check.

**New Content** (Lines 458-461):
- SPSC channel_manager needs to expose `is_operational()` method
- Checks: `!peer_cancel_received_`, `!cancel_sent_`, tasks still running
- **Purpose**: Support both-or-neither guarantee
- **Usage**: `clone_for_zone()` must check before creating new proxies

**Rationale**: Prevents creating service_proxies on dying transport (asymmetric state prevention).

---

### ✅ CORRECTION 7: Handler Registration Member Variable

**Added**: New member variable documentation for handler registration.

**New Content** (Lines 354-358):
```cpp
### Handler Registration (NEW ADDITION)
- `std::weak_ptr<i_marshaller> handler_` - Service or pass_through that handles incoming messages
  - Polymorphic handler via i_marshaller interface
  - Transport does NOT know if it's service or pass_through
  - Transport calls handler methods after unpacking envelopes
```

**Rationale**: Documents how transport registers and calls handler polymorphically.

---

### ✅ CORRECTION 8: Synchronous Mode Clarification

**Added**: New section documenting that SPSC is **coroutine-only**.

**Source**: Q&A Q13 bi-modal support matrix.

**New Content** (Lines 410-427):
- SPSC requires `BUILD_COROUTINE=ON`
- Cannot be used in synchronous mode
- **Reason**: Requires async pump tasks to poll queues
- Without coroutines: no mechanism to pump messages
- **Impact**: This document only applies to async builds

**Rationale**: Clarifies SPSC limitations and transport mode requirements.

---

### ✅ CORRECTION 9: Future Architecture Changes Section

**Added**: New section on pass-through integration impacts.

**Source**: Q&A Q3 pass-through reference counting responsibility.

**New Content** (Lines 429-461):
1. **service_proxy_ref_count_ may be eliminated**
   - Pass-through will control service_proxy lifetime
   - Channel lifetime tied to pass-through, not individual proxies

2. **Handler registration will use pass_through**
   - Instead of service as handler, pass_through becomes handler
   - Pass-through implements i_marshaller for routing

3. **Both-or-Neither Guarantee Required**
   - Pass-through guarantees BOTH or NEITHER service_proxies operational
   - Channel manager must refuse operations if transport not operational
   - Prevents asymmetric states

4. **Operational State Check**
   - `clone_for_zone()` must check transport operational before creating proxies

**Rationale**: Prepares for future architectural changes without breaking current implementation.

---

## New Requirements Added to Q&A Document

### Q15: SPSC Channel Manager Requirements and Constraints

Added comprehensive Q15 to Problem_Statement_Critique_QA.md documenting:

1. **Handler Registration Pattern**
2. **Message Handler Architecture**
3. **Transport Sharing Pattern**
4. **Zone Termination Requirements**
5. **Operational State Check**
6. **Reference Counting Lifecycle (subject to change)**
7. **Task Lifecycle Guarantees**
8. **Shutdown Idempotency**
9. **No Data Loss During Shutdown**
10. **Reentrant Call Support**

**SPSC-Specific Constraints**:
- Coroutine-only transport
- Lock-free queue requirements
- Message chunking
- Reference cycle prevention

**Key Invariants**: 7 invariants documented

**New Requirements for Pass-Through Integration**: 3 requirements

**Impact on Core Architecture**: 4 architectural implications

---

## Files Modified

1. **SPSC_Channel_Lifecycle.md** (modified in place with corrections):
   - Added version 2.0 header
   - Added critical clarifications
   - Added zone termination support
   - Added operational state requirements
   - Added synchronous mode section
   - Added future architecture changes section

2. **Problem_Statement_Critique_QA.md**:
   - Added Q15 with comprehensive SPSC requirements
   - Updated summary to include Q15
   - Updated question count (14 → 15)

3. **SPSC_Channel_Lifecycle_Corrected.md** (created):
   - Full corrected version created
   - Merged back into original document

---

## Compliance Status

### ✅ Complies with Q&A Requirements

- ✅ Q2: Transport sharing (multiple proxies share ONE channel)
- ✅ Q3: Service_sink is transport-internal (not core class)
- ✅ Q3: Handler polymorphism (service or pass_through via i_marshaller)
- ✅ Q3: Both-or-neither guarantee (operational state check)
- ✅ Q9: Zone termination broadcast (transport responsibility)
- ✅ Q12: i_marshaller as universal interface
- ✅ Q13: SPSC is coroutine-only (async mode requirement)

### ✅ Complies with Problem Statement

- ✅ Lines 666-683: Transport responsibility for zone termination
- ✅ Back-channel support (documented in handler calls)
- ✅ Fire-and-forget messaging (`post_options::zone_terminating`)

### ✅ Complies with User Guide

- ✅ SPSC listed as coroutine-only transport
- ✅ Channel manager pattern documented
- ✅ Bidirectional queue architecture

---

## Key Insights

### 1. Transport-Specific Implementation Freedom

SPSC uses channel_manager with pump tasks, but this is NOT a requirement for other transports. TCP might use async I/O, Local might use direct calls. The only common interface is registering an `i_marshaller` handler.

### 2. Handler Polymorphism Pattern

The pattern of transports calling `handler_->send()`, `handler_->post()`, etc. without knowing if handler is service or pass_through is a key architectural pattern that should apply to ALL transports.

### 3. Operational State Visibility

The requirement for `is_operational()` method is critical for both-or-neither guarantee. This should be a requirement for ALL transports, not just SPSC.

### 4. Reference Counting Transition

The current `service_proxy_ref_count_` pattern is explicitly marked as temporary. Future architecture will use pass-through to control service_proxy lifetime.

### 5. Zone Termination Broadcast

Transport detecting connection failure and broadcasting `zone_terminating` to handlers is a critical requirement for graceful shutdown. This applies to ALL unreliable transports (TCP, network-based).

---

## Recommendations

### For Other Transport Implementations

1. **Implement Handler Registration**:
   - All transports should register `std::weak_ptr<i_marshaller> handler_`
   - Call handler methods after unpacking messages
   - Don't know if handler is service or pass_through

2. **Implement is_operational()**:
   - All transports should expose operational state
   - Support both-or-neither guarantee
   - Allow `clone_for_zone()` to check viability

3. **Implement Zone Termination Detection**:
   - Detect connection failures
   - Broadcast `handler_->post(post_options::zone_terminating)`
   - Support graceful multi-zone shutdown

4. **Reentrant Call Support**:
   - Schedule stub calls asynchronously (don't await inline)
   - Allow RPC methods to make reentrant calls back to caller
   - Critical for complex bidirectional protocols

### For Core Architecture

1. **Document Handler Polymorphism Pattern**:
   - This is a universal pattern, not SPSC-specific
   - Should be in core architecture documentation

2. **Standardize is_operational() Interface**:
   - All transports should implement this
   - Document as required transport capability

3. **Plan for Reference Counting Transition**:
   - Current `service_proxy_ref_count_` is temporary
   - Design pass-through to control service_proxy lifetime
   - Prepare for architectural change

---

## Summary

The SPSC_Channel_Lifecycle.md document is now **fully compliant** with Q&A, Problem Statement, and User Guide requirements. Critical clarifications added:

- SPSC is transport-specific (not universal pattern)
- Handler polymorphism via i_marshaller
- Transport sharing (multiple proxies per channel)
- Zone termination broadcast support
- Operational state visibility for both-or-neither guarantee
- Coroutine-only constraint
- Future pass-through integration impacts

New requirements discovered from SPSC analysis have been added to Q&A document (Q15), providing comprehensive documentation of SPSC-specific constraints and universal transport requirements.

---

**End of Document**
